#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "common.h"
#include "parse.h"

int create_db_header(int fd, struct dbheader_t **headerOut) {
    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
        printf("Error: Malloc failed to create db header\n");
        return STATUS_ERROR;
    }

    header->version = 0x1;
    header->count = 0;
    header->magic = HEADER_MAGIC;
    header->filesize = sizeof(struct dbheader_t);

    *headerOut = header;

    return STATUS_SUCCESS;
}

int validate_db_header(int fd, struct dbheader_t **headerOut) {
    if (fd < 0) {
        printf("Error: Got a bad FD from the user in validate_db_header\n");
        return STATUS_ERROR;
    }

    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
        printf("Error: Malloc failed to create a db header for validation\n");
        return STATUS_ERROR;
    }

    lseek(fd, 0, SEEK_SET);

    ssize_t bytes_read = read(fd, header, sizeof(struct dbheader_t));
    if (bytes_read != sizeof(struct dbheader_t)) {
        perror("Error reading database header");
        free(header);
        return STATUS_ERROR;
    }

    header->magic = ntohl(header->magic);
    header->version = ntohs(header->version);
    header->count = ntohs(header->count);
    header->filesize = ntohl(header->filesize);

    if (header->magic != HEADER_MAGIC) {
        printf("Error: Improper header magic. Expected 0x%X, got 0x%X\n", HEADER_MAGIC, header->magic);
        free(header);
        return STATUS_ERROR;
    }

    if (header->version != 1) {
        printf("Error: Improper header version. Expected 1, got %hu\n", header->version);
        free(header);
        return STATUS_ERROR;
    }

    struct stat dbstat = {0};
    if (fstat(fd, &dbstat) == -1) {
        perror("Error getting file stats");
        free(header);
        return STATUS_ERROR;
    }
    if (header->filesize != dbstat.st_size) {
        printf("Error: Corrupted database. Header filesize %u mismatch with actual file size %ld\n", header->filesize, dbstat.st_size);
        free(header);
        return STATUS_ERROR;
    }

    *headerOut = header;

    return STATUS_SUCCESS;
}

int output_file(int fd, struct dbheader_t *dbhdr, struct employee_t *employees) {
    if (fd < 0) {
        printf("Error: Got a bad FD from the user in output_file\n");
        return STATUS_ERROR;
    }

    struct dbheader_t temp_dbhdr = *dbhdr;
    unsigned int calculated_filesize = sizeof(struct dbheader_t) + temp_dbhdr.count * sizeof(struct employee_t);

    temp_dbhdr.version = htons(temp_dbhdr.version);
    temp_dbhdr.count = htons(temp_dbhdr.count);
    temp_dbhdr.magic = htonl(temp_dbhdr.magic);
    temp_dbhdr.filesize = htonl(calculated_filesize);

    lseek(fd, 0, SEEK_SET);

    if (write(fd, &temp_dbhdr, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
        perror("Error writing database header");
        return STATUS_ERROR;
    }

    for (int i = 0; i < dbhdr->count; ++i) {
        struct employee_t temp_employee = employees[i];
        temp_employee.hours = htonl(temp_employee.hours);

        if (write(fd, &temp_employee, sizeof(struct employee_t)) != sizeof(struct employee_t)) {
            perror("Error writing employee record");
            return STATUS_ERROR;
        }
    }

    if (ftruncate(fd, calculated_filesize) == -1) {
        perror("Error truncating file");
        return STATUS_ERROR;
    }

    return STATUS_SUCCESS;
}

int read_employees(int fd, struct dbheader_t *dbhdr, struct employee_t **employeesOut) {
    if (fd < 0) {
        printf("Error: Got a bad FD from the user in read_employees\n");
        return STATUS_ERROR;
    }

    lseek(fd, sizeof(struct dbheader_t), SEEK_SET);

    int count = dbhdr->count;

    if (count == 0) {
        *employeesOut = NULL;
        return STATUS_SUCCESS;
    }

    struct employee_t *employees = calloc(count, sizeof(struct employee_t));
    if (employees == NULL) {
        printf("Error: Malloc failed to allocate memory for employees\n");
        return STATUS_ERROR;
    }

    ssize_t bytes_read = read(fd, employees, count * sizeof(struct employee_t));
    if (bytes_read != (ssize_t)(count * sizeof(struct employee_t))) {
        perror("Error reading employee records");
        free(employees);
        return STATUS_ERROR;
    }

    for (int i = 0; i < count; ++i) {
        employees[i].hours = ntohl(employees[i].hours);
    }
    
    *employeesOut = employees;

    return STATUS_SUCCESS;
}

int add_employee(struct dbheader_t *dbhdr, struct employee_t **employees, char *addstring) {

    dbhdr->count++;

    struct employee_t *temp = realloc(*employees, dbhdr->count * sizeof(struct employee_t));
    if (temp == NULL) {
        printf("Error: Failed to reallocate memory for employees during add\n");
        dbhdr->count--;
        return STATUS_ERROR;
    }
    *employees = temp;

    char *addstring_copy = strdup(addstring);
    if (addstring_copy == NULL) {
        printf("Error: Failed to duplicate addstring for parsing\n");
        dbhdr->count--;
        return STATUS_ERROR;
    }

    char *name = strtok(addstring_copy, "-");
    char *address = strtok(NULL, "-");
    char *hours = strtok(NULL, "-");

    if (!name || !address || !hours) {
        printf("Error: Invalid addstring format. Expected 'name,address,hours'. Got: '%s'\n", addstring);
        free(addstring_copy);
        dbhdr->count--;
        return STATUS_ERROR;
    }

    int new_employee_idx = dbhdr->count - 1;

    strncpy((*employees)[new_employee_idx].name, name, sizeof((*employees)[new_employee_idx].name) - 1);
    (*employees)[new_employee_idx].name[sizeof((*employees)[new_employee_idx].name) - 1] = '\0';
 
    strncpy((*employees)[new_employee_idx].address, address, sizeof((*employees)[new_employee_idx].address) - 1);
    (*employees)[new_employee_idx].address[sizeof((*employees)[new_employee_idx].address) - 1] = '\0';

    char *end_ptr;
    errno = 0;
    long hours_long = strtol(hours, &end_ptr, 10);

    if (*end_ptr != '\0' || hours == end_ptr || (errno == ERANGE && (hours_long == LONG_MAX || hours_long == LONG_MIN))) {
        printf("Error: Invalid hours input or value out of range: '%s'\n", hours);
        free(addstring_copy);
        dbhdr->count--;
        return STATUS_ERROR;
    }

    if (hours_long < 0 || (unsigned long)hours_long > UINT_MAX) {
        printf("Error: Hours value %ld is out of valid unsigned int range [0, %u]\n", hours_long, UINT_MAX);
        free(addstring_copy);
        dbhdr->count--;
        return STATUS_ERROR;
    }
    (*employees)[new_employee_idx].hours = (unsigned int)hours_long;

    free(addstring_copy);
    return STATUS_SUCCESS;
}

int remove_employee(struct dbheader_t *dbhdr, struct employee_t **employees) {
    if (dbhdr->count <= 0) {
        printf("Error: No employees to remove.\n");
        return STATUS_ERROR;
    }

    dbhdr->count--; 
    printf("Removed last employee. New count: %hu\n", dbhdr->count);

    if (dbhdr->count == 0) {
        if (*employees != NULL) {
            free(*employees);
            *employees = NULL;
        }
    } else {
        struct employee_t *temp = realloc(*employees, dbhdr->count * sizeof(struct employee_t));
        if (temp == NULL) {
            printf("Warning: Failed to reallocate memory to shrink employee array. Data not lost, but memory might be underutilized.\n");
        } else {
            *employees = temp;
        }
    }

    return STATUS_SUCCESS;
}

int list_employees(struct dbheader_t *dbhdr, struct employee_t *employees) {
    if (dbhdr->count == 0) {
        printf("No employees to list.\n");
        return STATUS_SUCCESS;
    }

    for (int i = 0; i < dbhdr->count; ++i) {
        printf("Employee %d\n", i);
        printf("\tName: %s\n", employees[i].name);
        printf("\tAddress: %s\n", employees[i].address);
        printf("\tHours: %u\n", employees[i].hours);
    }
    return STATUS_SUCCESS;
}