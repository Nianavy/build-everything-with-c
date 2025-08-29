#ifndef FILE_H
#define FILE_H

/**
 * @brief 创建一个新的数据库文件。
 *        如果文件已存在则返回错误。
 * @param filename 要创建的文件名
 * @return 成功时返回文件描述符，错误时返回 STATUS_ERROR。
 */
int create_db_file(char *filename);

/**
 * @brief 打开一个现有的数据库文件。
 * @param filename 要打开的文件名
 * @return 成功时返回文件描述符，错误时返回 STATUS_ERROR。
 */
int open_db_file(char *filename);

#endif