#include "inc/container_rootfs.h"
#include "inc/container_utils.h"

#include <sys/mount.h>  // mount, umount2
#include <sys/stat.h>   // mkdir
#include <unistd.h>     // chdir, rmdir
#include <sys/syscall.h> // For SYS_pivot_root
#include <errno.h>      // For errno
#include <string.h>     // For strerror

// 设置容器的根文件系统
int setup_rootfs(container_config_t *config) {
    printf("Container PID %d: Setting up rootfs '%s'\n", getpid(), config->rootfs_path);

    // 1. 确保 config->rootfs_path 是一个独立的挂载点。
    // 这对 pivot_root 至关重要。即使它在文件系统上只是一个普通目录，
    // 也需要 bind mount 自身来使其成为一个独立的挂载点。
    // MS_REC 表示递归绑定。
    if (mount(config->rootfs_path, config->rootfs_path, "bind", MS_BIND | MS_REC, NULL) == -1) {
        die("bind mount rootfs to itself failed");
    }
    printf("Container PID %d: Bind mounted rootfs to itself.\n", getpid());

    // 2. 将此挂载点设置为私有 (private)。
    // 这可以防止这个挂载点的事件 (如卸载) 传播到父命名空间或其他对等命名空间。
    // 确保在当前 mount 命名空间中的操作是隔离的。
    if (mount(NULL, config->rootfs_path, NULL, MS_PRIVATE, NULL) == -1) {
        // 如果失败，通常不是致命错误，但也值得注意。
        // 对于简单容器，可以忽略，但更严格的隔离需要这个。
        // 为了健壮性，这里我们把它视为致命错误。
        die("make rootfs private failed");
    }
    printf("Container PID %d: Rootfs mount point made private.\n", getpid());


    // 3. 切换到 rootfs_path 目录。
    // 这是为了让 pivot_root(., .old_root) 能正确工作，即当前工作目录就是新的根。
    if (chdir(config->rootfs_path) == -1) {
        die("chdir to rootfs_path failed");
    }
    printf("Container PID %d: Changed current working directory to '%s'\n", getpid(), config->rootfs_path);

    // 4. 在新的根目录下创建 old_root 挂载点。
    // pivot_root 会将旧的根文件系统挂载到这个点 (相对于当前目录 .old_root)。
    const char *old_root_path_in_new_root = ".old_root";
    if (mkdir(old_root_path_in_new_root, 0755) == -1 && errno != EEXIST) {
        die("mkdir for .old_root failed");
    }
    printf("Container PID %d: Created temporary old_root_path: %s\n", getpid(), old_root_path_in_new_root);

    // 5. 执行 pivot_root
    // new_root: current directory (".")
    // old_root: relative path to .old_root (must be under new_root)
    if (syscall(SYS_pivot_root, ".", old_root_path_in_new_root) == -1) {
        // 如果 pivot_root 失败，说明环境仍然不满足其严格要求。
        // 打印详细错误信息，并回退到 chroot 作为备用方案 (不推荐用于生产)。
        fprintf(stderr, "[!] Container PID %d: pivot_root failed: %s. Reverting to chroot (less secure).\n", getpid(), strerror(errno));

        // chroot 需要当前目录是目标rootfs，因为我们之前 chdir 过去了，所以 '.' 是正确的。
        if (chroot(".") == -1) {
            die("chroot failed as fallback");
        }
        // chroot 后，当前进程的根目录变为 '.' (即 config->rootfs_path)
        if (chdir("/") == -1) { // 再次 chdir 到新的逻辑根目录
            die("chdir to / after chroot failed");
        }
        printf("Container PID %d: Rootfs switched using chroot as fallback.\n", getpid());
        // 注意：chroot 后，旧的根文件系统仍然挂载在某个地方，不会被自动卸载。
        // 这种回退方式是临时措施，不是理想解决方案。
    } else {
        printf("Container PID %d: Rootfs switched using pivot_root.\n", getpid());

        // 6. pivot_root 成功后，旧的根目录被挂载到 /.old_root。
        // 切换到新的根目录 '/'
        if (chdir("/") == -1) {
            die("chdir to new root '/' after pivot_root failed");
        }
        printf("Container PID %d: Changed current working directory to new root '/'\n", getpid());

        // 7. 卸载旧的根目录
        // umount2 的路径是相对于新的根目录的。MNT_DETACH 允许延迟卸载。
        if (umount2("/.old_root", MNT_DETACH) == -1) {
            // 如果这里失败，通常意味着有进程还在 /.old_root 目录下活动，
            // 或者其他资源被占用。这需要更复杂的清理逻辑。
            die("umount2 /.old_root failed");
        }
        // 8. 移除旧根目录的挂载点
        if (rmdir("/.old_root") == -1) {
            fprintf(stderr, "[W] Container PID %d: rmdir /.old_root failed: %s (might still be in use by kernel/other resources)\n", getpid(), strerror(errno));
        }
        printf("Container PID %d: Old root unmounted and removed.\n", getpid());
    }

    // 9. 挂载 /proc 文件系统 (重要，否则很多工具无法正常工作)
    if (mkdir("/proc", 0555) == -1 && errno != EEXIST) {
        die("mkdir /proc failed");
    }
    if (mount("proc", "/proc", "proc", 0, NULL) == -1) {
        die("mount /proc failed");
    }
    printf("Container PID %d: /proc mounted.\n", getpid());

    // 10. 挂载 /sys 文件系统 (通常也需要)
    if (mkdir("/sys", 0555) == -1 && errno != EEXIST) {
        die("mkdir /sys failed");
    }
    if (mount("sysfs", "/sys", "sysfs", 0, NULL) == -1) {
        fprintf(stderr, "[W] Container PID %d: mount /sys failed: %s (non-fatal, but some tools may fail)\n", getpid(), strerror(errno));
    }
    printf("Container PID %d: /sys mounted (if successful).\n", getpid());

    // 11. 挂载 /dev 文件系统 (也经常需要)
    if (mkdir("/dev", 0555) == -1 && errno != EEXIST) {
        die("mkdir /dev failed");
    }
    if (mount("tmpfs", "/dev", "tmpfs", MS_NOSUID | MS_STRICTATIME, "mode=755") == -1) {
        die("mount /dev tmpfs failed");
    }
    // TODO: 创建基本的设备文件，例如 /dev/null, /dev/console, /dev/urandom 等。
    // 这通常通过 mknod(2) 或从宿主机复制/dev/目录下的关键文件来实现。
    // 简单的 busybox rootfs 可能已经有这些，或者在 mount devtmpfs 时自动创建。
    printf("Container PID %d: /dev mounted.\n", getpid());

    return 0; // 成功
}