#include <stdio.h>    // For perror, fprintf
#include <unistd.h>   // For lseek, read, write, ftruncate
#include <stdlib.h>   // For calloc, free, strtol, strdup
#include <arpa/inet.h> // For htonl, ntohl (网络字节序转换)
#include <sys/types.h> // For stat, ssize_t
#include <sys/stat.h> // For stat, fstat
#include <string.h>   // For memset, strncpy, strtok, strlen
#include <errno.h>    // For errno, ERANGE
#include <limits.h>   // For LONG_MAX, LONG_MIN, UINT_MAX

#include "../../include/common.h" // 包含 STATUS_SUCCESS 等宏
#include "../../include/parse.h"  // 包含 dbheader_t, employee_t 结构体

/**
 * @brief 创建并初始化数据库文件头部。
 *        分配内存并设置魔数、版本号、计数和文件大小。
 * @param fd 数据库文件的文件描述符（未使用，但保留原始签名）。
 * @param headerOut 指向 dbheader_t 指针的指针，用于返回新创建的头部。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int create_db_header(struct dbheader_t **headerOut) {
    // 分配并清零数据库头部内存，使用 cleanup 宏确保自动释放
    struct dbheader_t *header __attribute__((cleanup(_cleanup_ptr_))) = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
        perror("calloc for db header"); // 使用 perror 报告内存分配错误
        return STATUS_ERROR;
    }

    // 初始化头部字段
    header->version = PROTO_VER;    // 使用协议版本宏
    header->count = 0;              // 初始员工数量为 0
    header->magic = HEADER_MAGIC;   // 设置魔数
    header->filesize = sizeof(struct dbheader_t); // 初始文件大小仅为头部大小

    *headerOut = header; // 将新创建的头部指针返回
    header = NULL;       // 清除 cleanup 宏对 header 的作用，因为 ownership 已转移给调用者
    return STATUS_SUCCESS;
}

/**
 * @brief 验证数据库文件的头部信息。
 *        读取头部，进行字节序转换，并检查魔数、版本和文件大小是否一致。
 * @param fd 数据库文件的文件描述符。
 * @param headerOut 指向 dbheader_t 指针的指针，用于返回读取和验证后的头部。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int validate_db_header(int fd, struct dbheader_t **headerOut) {
    if (fd < 0) {
        fprintf(stderr, "Error: Got a bad FD (%d) from the user in validate_db_header\n", fd);
        return STATUS_ERROR;
    }

    // 分配并清零头部内存用于读取，使用 cleanup 宏确保自动释放
    struct dbheader_t *header __attribute__((cleanup(_cleanup_ptr_))) = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
        perror("calloc for db header validation");
        return STATUS_ERROR;
    }

    lseek(fd, 0, SEEK_SET); // 定位到文件开头

    // 读取数据库头部
    ssize_t bytes_read = read(fd, header, sizeof(struct dbheader_t));
    if (bytes_read != sizeof(struct dbheader_t)) {
        if (bytes_read == -1) {
            perror("Error reading database header");
        } else {
            fprintf(stderr, "Error: Incomplete database header read. Expected %zu bytes, got %zd.\n",
                    sizeof(struct dbheader_t), bytes_read);
        }
        return STATUS_ERROR;
    }

    // 将头部字段从网络字节序转换回主机字节序
    header->magic = ntohl(header->magic);
    header->version = ntohs(header->version);
    header->count = ntohs(header->count);
    header->filesize = ntohl(header->filesize);

    // 验证魔数
    if (header->magic != HEADER_MAGIC) {
        fprintf(stderr, "Error: Improper header magic. Expected 0x%X, got 0x%X\n", HEADER_MAGIC, header->magic);
        return STATUS_ERROR;
    }

    // 验证协议版本
    if (header->version != PROTO_VER) {
        fprintf(stderr, "Error: Improper header version. Expected %d, got %hu\n", PROTO_VER, header->version);
        return STATUS_ERROR;
    }

    // 获取实际文件大小，并与头部中的 filesize 字段进行比较
    struct stat dbstat = {0};
    if (fstat(fd, &dbstat) == -1) {
        perror("Error getting file stats");
        return STATUS_ERROR;
    }
    // 注意：dbstat.st_size 是 long int，可能与 header->filesize (uint32_t) 类型不匹配，进行显式转换
    if (header->filesize != (uint32_t)dbstat.st_size) {
        fprintf(stderr, "Error: Corrupted database. Header filesize %u mismatch with actual file size %ld\n", header->filesize, dbstat.st_size);
        return STATUS_ERROR;
    }

    *headerOut = header; // 将验证后的头部指针返回
    header = NULL;       // 清除 cleanup 宏的作用
    return STATUS_SUCCESS;
}

/**
 * @brief 将内存中的数据库头部和员工数据写入文件。
 *        此函数负责字节序转换和文件截断，确保文件内容与内存状态一致。
 * @param fd 数据库文件的文件描述符。
 * @param dbhdr 指向数据库头部的指针（只读）。
 * @param employees 指向员工数组的指针（只读）。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int output_file(int fd, const struct dbheader_t *dbhdr, const struct employee_t *employees) {
    if (fd < 0) {
        fprintf(stderr, "Error: Got a bad FD (%d) from the user in output_file\n", fd);
        return STATUS_ERROR;
    }
    if (dbhdr == NULL) {
        fprintf(stderr, "Error: dbhdr is NULL in output_file\n");
        return STATUS_ERROR;
    }

    // 创建一个临时的 dbheader_t 副本，用于字节序转换，不修改原始 dbhdr
    struct dbheader_t temp_dbhdr = *dbhdr;
    // 计算文件应该有的总大小
    uint32_t calculated_filesize = sizeof(struct dbheader_t) + temp_dbhdr.count * sizeof(struct employee_t);

    // 将头部字段转换为主机字节序
    temp_dbhdr.version = htons(temp_dbhdr.version);
    temp_dbhdr.count = htons(temp_dbhdr.count);
    temp_dbhdr.magic = htonl(temp_dbhdr.magic);
    temp_dbhdr.filesize = htonl(calculated_filesize);

    lseek(fd, 0, SEEK_SET); // 定位到文件开头

    // 写入转换后的数据库头部
    if (write(fd, &temp_dbhdr, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
        perror("Error writing database header");
        return STATUS_ERROR;
    }

    // 检查在有员工记录时 employees 指针是否为 NULL
    if (dbhdr->count > 0 && employees == NULL) {
        fprintf(stderr, "Error: dbhdr->count > 0 but employees is NULL in output_file\n");
        return STATUS_ERROR;
    }

    // 循环写入每个员工记录
    for (int i = 0; i < dbhdr->count; ++i) {
        struct employee_t temp_employee = employees[i]; // 创建副本进行转换
        temp_employee.hours = htonl(temp_employee.hours); // 转换 hours 字段为网络字节序

        if (write(fd, &temp_employee, sizeof(struct employee_t)) != sizeof(struct employee_t)) {
            perror("Error writing employee record");
            return STATUS_ERROR;
        }
    }

    // 截断文件到计算出的实际大小，删除多余数据
    if (ftruncate(fd, calculated_filesize) == -1) {
        perror("Error truncating file");
        return STATUS_ERROR;
    }

    return STATUS_SUCCESS;
}

/**
 * @brief 从数据库文件中读取所有员工记录到内存。
 * @param fd 数据库文件的文件描述符。
 * @param dbhdr 指向数据库头部的指针（只读），用于获取员工数量。
 * @param employeesOut 指向 struct employee_t 指针的指针，用于返回读取到的员工数组。
 *                     如果没有员工，则设为 NULL。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int read_employees(int fd, struct dbheader_t *dbhdr, struct employee_t **employeesOut) {
    if (fd < 0) {
        fprintf(stderr, "Error: Got a bad FD (%d) from the user in read_employees\n", fd);
        return STATUS_ERROR;
    }
    if (dbhdr == NULL) {
        fprintf(stderr, "Error: dbhdr is NULL in read_employees\n");
        return STATUS_ERROR;
    }

    lseek(fd, sizeof(struct dbheader_t), SEEK_SET); // 定位到文件头部之后，员工数据开始处

    int count = dbhdr->count; // 获取员工数量

    if (count == 0) {
        *employeesOut = NULL; // 没有员工，返回 NULL
        return STATUS_SUCCESS;
    }

    // 分配内存来存储员工数组，使用 cleanup 宏确保自动释放
    struct employee_t *employees __attribute__((cleanup(_cleanup_ptr_))) = calloc(count, sizeof(struct employee_t));
    if (employees == NULL) {
        perror("calloc for employees"); // 报告内存分配错误
        return STATUS_ERROR;
    }

    // 读取所有员工记录
    ssize_t bytes_read = read(fd, employees, count * sizeof(struct employee_t));
    if (bytes_read != (ssize_t)(count * sizeof(struct employee_t))) {
        if (bytes_read == -1) {
            perror("Error reading employee records");
        } else {
            fprintf(stderr, "Error: Incomplete employee records read. Expected %zu bytes, got %zd.\n",
                    (size_t)count * sizeof(struct employee_t), bytes_read);
        }
        return STATUS_ERROR;
    }

    // 转换员工数据中的字段（例如 hours）回主机字节序
    for (int i = 0; i < count; ++i) {
        employees[i].hours = ntohl(employees[i].hours);
    }
    
    *employeesOut = employees; // 将读取到的员工数组指针返回
    employees = NULL;          // 清除 cleanup 宏的作用
    return STATUS_SUCCESS;
}

/**
 * @brief 向内存中的员工数组添加一个新员工。
 *        此函数会重新分配内存以容纳新员工，并解析 `addstring`。
 * @param dbhdr 指向数据库头部的指针（会修改其 count 字段）。
 * @param employees_ptr 指向 struct employee_t 指针的指针（可能会改变内存地址）。
 * @param addstring 格式为 "Name-Address-Hours" 的员工信息字符串。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int add_employee(struct dbheader_t *dbhdr, struct employee_t **employees_ptr, const char *addstring) {
    if (dbhdr == NULL || employees_ptr == NULL || addstring == NULL) {
        fprintf(stderr, "Error: Invalid arguments to add_employee (NULL pointer).\n");
        return STATUS_ERROR;
    }

    // 复制 addstring，因为 strtok 会修改它。使用 cleanup 宏确保自动释放
    char *addstring_copy __attribute__((cleanup(_cleanup_ptr_))) = strdup(addstring);
    if (addstring_copy == NULL) {
        perror("strdup addstring"); // 报告内存分配错误
        return STATUS_ERROR;
    }

    // 使用 strtok 分割字符串：姓名-地址-小时数
    char *name = strtok(addstring_copy, "-");
    char *address = strtok(NULL, "-");
    char *hours_str = strtok(NULL, "-");
    char *extra_token = strtok(NULL, "-"); // 检查是否有额外的分隔符

    // 验证字符串格式
    if (!name || !address || !hours_str || extra_token) {
        fprintf(stderr, "Error: Invalid addstring format. Expected 'Name-Address-Hours', got: '%s'\n", addstring);
        return STATUS_ERROR;
    }

    // 尝试重新分配内存以容纳新员工
    // realloc 可能返回 NULL，此时 *employees_ptr 不会改变
    struct employee_t *temp_employees = realloc(*employees_ptr, (dbhdr->count + 1) * sizeof(struct employee_t));
    if (temp_employees == NULL) {
        perror("realloc for add_employee"); // 报告内存分配错误
        return STATUS_ERROR;
    }
    *employees_ptr = temp_employees; // 更新 employees_ptr 指向新内存

    // 获取新员工的索引
    int new_employee_idx = dbhdr->count;

    // 拷贝 name，并确保空终止
    strncpy((*employees_ptr)[new_employee_idx].name, name, sizeof((*employees_ptr)[new_employee_idx].name) - 1);
    (*employees_ptr)[new_employee_idx].name[sizeof((*employees_ptr)[new_employee_idx].name) - 1] = '\0';
 
    // 拷贝 address，并确保空终止
    strncpy((*employees_ptr)[new_employee_idx].address, address, sizeof((*employees_ptr)[new_employee_idx].address) - 1);
    (*employees_ptr)[new_employee_idx].address[sizeof((*employees_ptr)[new_employee_idx].address) - 1] = '\0';

    // 转换 hours 字符串为数字
    char *end_ptr;
    errno = 0; // 重置 errno 以检测 strtol 错误
    long hours_long = strtol(hours_str, &end_ptr, 10);

    // 检查 strtol 转换结果：是否有未解析的字符，是否为空字符串，是否超出 long 范围
    if (*end_ptr != '\0' || hours_str == end_ptr || (errno == ERANGE && (hours_long == LONG_MAX || hours_long == LONG_MIN))) {
        fprintf(stderr, "Error: Invalid hours input or value out of range: '%s'.\n", hours_str);
        return STATUS_ERROR; 
    }

    // 检查 hours 的值是否在 unsigned int 范围内（非负且不超过最大值）
    if (hours_long < 0 || (unsigned long)hours_long > UINT_MAX) {
        fprintf(stderr, "Error: Hours value %ld is out of valid unsigned int range [0, %u].\n", hours_long, UINT_MAX);
        return STATUS_ERROR;
    }
    (*employees_ptr)[new_employee_idx].hours = (uint32_t)hours_long; // 存储为 uint32_t

    dbhdr->count++; // 只有当所有数据都验证并准备好后，才增加员工计数

    return STATUS_SUCCESS;
}

/**
 * @brief 从内存中的员工数组中删除最后一个员工。
 *        此函数会重新分配内存以缩小数组。
 * @param dbhdr 指向数据库头部的指针（会修改其 count 字段）。
 * @param employees_ptr 指向 struct employee_t 指针的指针（可能会改变内存地址）。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR (例如没有员工可删除)。
 */
int remove_employee(struct dbheader_t *dbhdr, struct employee_t **employees_ptr) {
    if (dbhdr == NULL || employees_ptr == NULL) {
        fprintf(stderr, "Error: Invalid arguments to remove_employee (NULL pointer).\n");
        return STATUS_ERROR;
    }

    // 检查是否有员工可删除
    if (dbhdr->count <= 0) {
        fprintf(stderr, "Error: No employees to remove.\n");
        return STATUS_ERROR;
    }

    dbhdr->count--; // 递减员工计数
    printf("Removed last employee. New count: %hu\n", dbhdr->count);

    if (dbhdr->count == 0) {
        // 如果删除后没有员工，则释放整个员工数组内存并清空指针
        if (*employees_ptr != NULL) {
            free(*employees_ptr);
            *employees_ptr = NULL;
        }
    } else {
        // 尝试缩小内存。如果失败，旧内存仍然有效，这是警告而非致命错误
        struct employee_t *temp_employees = realloc(*employees_ptr, dbhdr->count * sizeof(struct employee_t));
        if (temp_employees == NULL) {
            fprintf(stderr, "Warning: Failed to reallocate memory to shrink employee array. "
                            "Memory might be underutilized, but existing data is safe.\n");
            // 如果 realloc 失败，*employees_ptr 仍然指向旧的有效内存，不会丢失数据。
            // 此时不更新 *employees_ptr。
        } else {
            *employees_ptr = temp_employees; // 更新 employees_ptr 指向新内存
        }
    }

    return STATUS_SUCCESS;
}

/**
 * @brief 列出内存中的所有员工信息到标准输出。
 * @param dbhdr 指向数据库头部的指针（只读）。
 * @param employees 指向员工数组的指针（只读）。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR (例如数据不一致)。
 */
int list_employees(const struct dbheader_t *dbhdr, const struct employee_t *employees) {
    if (dbhdr == NULL) {
        fprintf(stderr, "Error: dbhdr is NULL in list_employees\n");
        return STATUS_ERROR;
    }
    // 检查员工数量与指针一致性
    if (dbhdr->count > 0 && employees == NULL) {
        fprintf(stderr, "Error: dbhdr->count > 0 but employees is NULL in list_employees\n");
        return STATUS_ERROR;
    }

    if (dbhdr->count == 0) {
        printf("No employees to list.\n");
        return STATUS_SUCCESS;
    }

    printf("\n--- Employee List (%hu records) ---\n", dbhdr->count);
    for (int i = 0; i < dbhdr->count; ++i) {
        printf("Employee #%d:\n", i + 1);
        printf("\tName: %s\n", employees[i].name);
        printf("\tAddress: %s\n", employees[i].address);
        printf("\tHours: %u\n", employees[i].hours);
    }
    printf("----------------------------------\n\n");
    return STATUS_SUCCESS;
}