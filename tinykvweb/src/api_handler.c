#include "../inc/api_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../inc/engine.h"
#include "../inc/parser.h"

static void handle_query(Storage *store, const char *body, char *response,
                         int max_len) {
    KvCommand cmd;
    if (parse_input(body, &cmd) != 0) {
        snprintf(response, max_len, "{\"error\":\"Invalid query syntax\"}\n");
        return;
    }

    ExecutionResult result = engine_execute(store, &cmd);
    snprintf(response, max_len, "%s\n", result.message);
}

static void handle_health(Storage *store, const char *body, char *response,
                          int max_len) {
    (void)store;
    (void)body;
    snprintf(response, max_len, "{\"status\":\"ok\"}\n");
}

static void handle_index(Storage *store, const char *body, char *response,
                         int max_len) {
    (void)store;
    (void)body;

    FILE *file = fopen("web/index.html", "r");
    if (!file) {
        snprintf(response, max_len, "{\"error\":\"index.html not found\"}\n");
        return;
    }

    char file_content[8192];
    size_t bytes_read = fread(file_content, 1, sizeof(file_content) - 1, file);
    file_content[bytes_read] = '\0';
    fclose(file);

    snprintf(response, max_len,
             "HTTP/1.0 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s",
             bytes_read, file_content);
}

static void handle_static(Storage *store, const char *body, char *response,
                          int max_len, const char *path) {
    (void)store;
    (void)body;

    // 防止路径穿越
    if (strstr(path, "..") || strstr(path, "//")) {
        snprintf(response, max_len,
                 "HTTP/1.0 403 Forbidden\r\n\r\nAccess denied");
        return;
    }

    char filepath[512] = {0};
    if (snprintf(filepath, sizeof(filepath), "../web%s", path) >=
        sizeof(filepath)) {
        snprintf(response, max_len, "HTTP/1.0 414 URI Too Long\r\n\r\n");
        return;
    }

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        snprintf(response, max_len,
                 "HTTP/1.0 404 Not Found\r\nContent-Type: "
                 "text/plain\r\n\r\nFile not found");
        return;
    }

    struct stat st;
    if (fstat(fileno(file), &st) != 0) {
        fclose(file);
        snprintf(response, max_len,
                 "HTTP/1.0 500 Internal Server Error\r\n\r\n");
        return;
    }
    size_t file_size = st.st_size;

    // 内容类型判断
    const char *ext = strrchr(path, '.');
    const char *content_type = "application/octet-stream";
    if (ext) {
        if (strcmp(ext, ".css") == 0)
            content_type = "text/css";
        else if (strcmp(ext, ".js") == 0)
            content_type = "application/javascript";
        else if (strcmp(ext, ".html") == 0)
            content_type = "text/html";
        else if (strcmp(ext, ".png") == 0)
            content_type = "image/png";
        else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
            content_type = "image/jpeg";
        else if (strcmp(ext, ".svg") == 0)
            content_type = "image/svg+xml";
    }

    // 检查响应缓冲区是否足够
    int header_len = snprintf(response, max_len,
                              "HTTP/1.0 200 OK\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "\r\n",
                              content_type, file_size);

    if (header_len < 0 || header_len >= max_len ||
        header_len + file_size >= max_len) {
        fclose(file);
        snprintf(response, max_len,
                 "HTTP/1.0 507 Insufficient Storage\r\n\r\nFile too large");
        return;
    }

    // 读取文件
    char *file_content = malloc(file_size);
    if (!file_content) {
        fclose(file);
        snprintf(response, max_len,
                 "HTTP/1.0 500 Internal Server Error\r\n\r\nOut of memory");
        return;
    }

    size_t bytes_read = fread(file_content, 1, file_size, file);
    fclose(file);

    if (bytes_read != file_size) {
        free(file_content);
        snprintf(response, max_len,
                 "HTTP/1.0 500 Internal Server Error\r\n\r\nRead error");
        return;
    }

    // 拷贝内容
    memcpy(response + header_len, file_content, file_size);
    response[header_len + file_size] = '\0';
    free(file_content);
}

typedef struct {
    const char *path;
    ApiHandlerFn handler;
} ApiRoute;

static ApiRoute routes[] = {
    {"/api/query", handle_query},
    {"/api/health", handle_health},
    {"/", handle_index},
};

#define NUM_ROUTES (sizeof(routes) / sizeof(routes[0]))

void handle_api_request(Storage *store, const char *path, const char *body,
                        char *response, int max_len) {
    for (size_t i = 0; i < NUM_ROUTES; ++i) {
        if (strcmp(path, routes[i].path) == 0) {
            routes[i].handler(store, body, response, max_len);
            return;
        }
    }
    handle_static(store, body, response, max_len, path);
}