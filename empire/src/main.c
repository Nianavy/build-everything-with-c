#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h> 
#include <unistd.h>

#include "common.h"
#include "file.h"
#include "parse.h"

void print_usage(char *argv[]) {
    printf("Usage: %s -f <database file> [options]\n", argv[0]);
    printf("\t -n - create new database file\n");
    printf("\t -f - (required) path to database file\n");
    printf("\t -a <\"Name,Address,Hours\"> - add a new employee\n");
    printf("\t -l - list all employees\n");
    printf("\t -r - remove the last employee\n");
    return;
}

int main(int argc, char *argv[]) {
    char *filepath = NULL;
    char *addstring = NULL;
    bool newfile = false;
    int c;
    bool list_employees_flag = false;
    bool remove_employee_flag = false;

    int dbfd = -1;
    struct dbheader_t *dbhdr = NULL;
    struct employee_t *employees = NULL;

    while ((c = getopt(argc, argv, "nf:a:lr")) != -1) {
        switch (c) {
            case 'n':
                    newfile = true;
                    break;
            case 'f':
                    filepath = optarg;
                    break;
            case 'a':
                    addstring = optarg;
                    break;
            case 'l':
                    list_employees_flag = true;
                    break;
            case 'r':
                    remove_employee_flag = true;
                    break;
            case '?':
                    fprintf(stderr, "Error: Unknown option '-%c'\n", optopt);
                    print_usage(argv);
                    return STATUS_ERROR;
            default:
                    return STATUS_ERROR;
        }
    }

    if (filepath == NULL) {
        fprintf(stderr, "Error: Filepath is a required argument\n");
        print_usage(argv);
        return STATUS_ERROR;
    }

    if (newfile) {
        dbfd = create_db_file(filepath);
        if (dbfd == STATUS_ERROR) {
            fprintf(stderr, "Error: Unable to create database file '%s'\n", filepath);
            return STATUS_ERROR;
        }
        if (create_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
            fprintf(stderr, "Error: Failed to create database header for '%s'\n", filepath);
            close(dbfd); 
            return STATUS_ERROR;
        }
    }
    else {
        dbfd = open_db_file(filepath);
        if (dbfd == STATUS_ERROR) {
            fprintf(stderr, "Error: Unable to open database file '%s'\n", filepath);
            return STATUS_ERROR;
        }
        if (validate_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
            fprintf(stderr, "Error: Failed to validate database header for '%s'\n", filepath);
            close(dbfd);
            return STATUS_ERROR;
        }
    }

    if (read_employees(dbfd, dbhdr, &employees) != STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to read employees from '%s'\n", filepath);
        if (dbhdr) free(dbhdr);
        close(dbfd);
        return STATUS_ERROR;
    }

    if (addstring) {
        if (add_employee(dbhdr, &employees, addstring) != STATUS_SUCCESS) {
            fprintf(stderr, "Error: Failed to add employee.\n");
            if (dbhdr) free(dbhdr);
            if (employees) free(employees);
            close(dbfd);
            return STATUS_ERROR;
        }
    }

    if (remove_employee_flag) {
        if (remove_employee(dbhdr, &employees) != STATUS_SUCCESS) {
            fprintf(stderr, "Error: Failed to remove employee.\n");
            if (dbhdr) free(dbhdr);
            if (employees) free(employees);
            close(dbfd);
            return STATUS_ERROR;
        }
    }

    if (list_employees_flag) {
        if (list_employees(dbhdr, employees) != STATUS_SUCCESS) {
            fprintf(stderr, "Error: Failed to list employees.\n");
            if (dbhdr) free(dbhdr);
            if (employees) free(employees);
            close(dbfd);
            return STATUS_ERROR;
        }
    }

    if (output_file(dbfd, dbhdr, employees) != STATUS_SUCCESS) {
        fprintf(stderr, "Error: Failed to output file '%s' after operations.\n", filepath);
        if (dbhdr) free(dbhdr);
        if (employees) free(employees);
        close(dbfd);
        return STATUS_ERROR;
    }

    if (dbhdr) {
        free(dbhdr);
        dbhdr = NULL;
    }
    if (employees) {
        free(employees);
        employees = NULL;
    }
    if (dbfd != -1) {
        close(dbfd);
        dbfd = -1;
    }
    
    return STATUS_SUCCESS;
}