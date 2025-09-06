/*
demo for file io.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    // open file
    int fd = open("test.txt", O_RDWR | O_CREAT, S_IRWXU);
    if (fd == -1) {
        perror("open file error");
        return -1;
    }

    // write file
    char content[] = "hello world!";
    int wt_size = write(fd, content, sizeof(content));
    if (wt_size == -1) {
        perror("write file error");
        close(fd);
        return -1;
    }

    // lseek file
    lseek(fd, 0, SEEK_SET);

    // read file
    char buf[256] = {0};
    int rd_size = read(fd, buf, sizeof(buf));
    if (rd_size == -1) {
        perror("read file error");
        close(fd);
        return -1;
    }
    printf("Read size = %d\ncontent: %s\n", rd_size, buf);

    close(fd);
    return 0;
}