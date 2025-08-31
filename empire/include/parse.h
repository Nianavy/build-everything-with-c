#ifndef PARSE_H
#define PARSE_H

#include <stdint.h> // For uint_t types

/**
 * @brief 数据库文件头部的魔数，用于标识文件类型。
 */
#define HEADER_MAGIC 0x4c4c4144 // "LLAD" in ASCII

/**
 * @brief 数据库文件头部结构体。
 *        所有字段在文件读写时需要进行字节序转换。
 *        `__attribute__((__packed__))` 确保结构体没有填充字节，但可能导致对齐问题和性能下降。
 */
struct dbheader_t {
    uint32_t magic;    ///< 魔数，标识数据库文件
    uint16_t version;  ///< 数据库版本
    uint16_t count;    ///< 数据库中员工记录的数量
    uint32_t filesize; ///< 整个数据库文件的大小，包括头部和所有员工记录
} __attribute__((__packed__));

/**
 * @brief 员工信息结构体。
 *        `__attribute__((__packed__))` 确保结构体没有填充字节。
 */
struct employee_t {
    char name[256];    ///< 员工姓名，以 null 结尾
    char address[256]; ///< 员工地址，以 null 结尾
    uint32_t hours;    ///< 员工工作小时数
} __attribute__((__packed__));

/**
 * @brief 创建并初始化数据库文件头部。
 * @param fd 数据库文件的文件描述符。
 * @param headerOut 指向 dbheader_t 指针的指针，用于返回新创建的头部。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int create_db_header(struct dbheader_t **headerOut);

/**
 * @brief 验证数据库文件的头部信息。
 * @param fd 数据库文件的文件描述符。
 * @param headerOut 指向 dbheader_t 指针的指针，用于返回读取和验证后的头部。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int validate_db_header(int fd, struct dbheader_t **headerOut);

/**
 * @brief 将内存中的数据库头部和员工数据写入文件。
 *        此函数负责字节序转换和文件截断。
 * @param fd 数据库文件的文件描述符。
 * @param dbhdr 指向数据库头部的指针（只读）。
 * @param employees 指向员工数组的指针（只读）。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int output_file(int fd, const struct dbheader_t *dbhdr, const struct employee_t *employees);

/**
 * @brief 从数据库文件中读取所有员工记录到内存。
 * @param fd 数据库文件的文件描述符。
 * @param dbhdr 指向数据库头部的指针（只读），用于获取员工数量。
 * @param employeesOut 指向 struct employee_t 指针的指针，用于返回读取到的员工数组。
 *                     如果没有员工，则设为 NULL。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int read_employees(int fd, struct dbheader_t *dbhdr, struct employee_t **employeesOut);

/**
 * @brief 向内存中的员工数组添加一个新员工。
 *        此函数会重新分配内存以容纳新员工，并解析 `addstring`。
 * @param dbhdr 指向数据库头部的指针（会修改其 count 字段）。
 * @param employees 指向 struct employee_t 指针的指针（可能会改变内存地址）。
 * @param addstring 格式为 "Name-Address-Hours" 的员工信息字符串。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int add_employee(struct dbheader_t *dbhdr, struct employee_t **employees, const char *addstring);

/**
 * @brief 从内存中的员工数组中删除最后一个员工。
 *        此函数会重新分配内存以缩小数组。
 * @param dbhdr 指向数据库头部的指针（会修改其 count 字段）。
 * @param employees 指向 struct employee_t 指针的指针（可能会改变内存地址）。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int remove_employee(struct dbheader_t *dbhdr, struct employee_t **employees);

/**
 * @brief 列出内存中的所有员工信息到标准输出。
 * @param dbhdr 指向数据库头部的指针（只读）。
 * @param employees 指向员工数组的指针（只读）。
 * @return 成功时返回 STATUS_SUCCESS，错误时返回 STATUS_ERROR。
 */
int list_employees(const struct dbheader_t *dbhdr, const struct employee_t *employees);

#endif