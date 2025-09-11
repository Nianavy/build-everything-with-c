#include "../../include/file.h"  // 包含 file.h 声明

#include <errno.h>      // For errno, ENOENT
#include <fcntl.h>      // For open, O_RDWR, O_CREAT
#include <stdio.h>      // For perror, fprintf
#include <sys/stat.h>   // For open, stat
#include <sys/types.h>  // For open, stat
#include <unistd.h>     // For open, close

#include "../../include/common.h"  // 包含 STATUS_ERROR 等宏

/**
 * @brief 创建一个新的数据库文件。
 *        如果文件已存在，则关闭文件描述符并返回错误。
 * @param filename 要创建的文件名。
 * @return 成功时返回文件描述符，错误时返回 STATUS_ERROR。
 */
int create_db_file(char *filename) {
    // 尝试打开文件，检查是否已存在
    int fd = open(filename, O_RDWR);
    if (fd != -1) {
        close(fd);  // 文件存在，关闭并返回错误
        fprintf(stderr, "Error: File '%s' already exists.\n", filename);
        return STATUS_ERROR;
    }
    // 如果 open 失败是由于文件不存在 (errno == ENOENT)，则继续创建
    if (errno != ENOENT) {  // 如果错误不是文件不存在，说明 open 失败了
        perror("open check existing file");  // 打印其他 open 错误信息
        return STATUS_ERROR;
    }

    // 以读写模式创建新文件，权限为 0644 (rw-r--r--)
    fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        perror("open to create file");  // 打印创建文件时的错误
        return STATUS_ERROR;
    }

    return fd;
}

/**
 * @brief 打开一个现有的数据库文件。
 * @param filename 要打开的文件名。
 * @return 成功时返回文件描述符，错误时返回 STATUS_ERROR。
 */
int open_db_file(char *filename) {
    // 以读写模式打开现有文件，权限 0644
    int fd = open(filename, O_RDWR, 0644);
    if (fd == -1) {
        perror("open existing file");  // 打印打开文件时的错误
        return STATUS_ERROR;
    }

    return fd;
}