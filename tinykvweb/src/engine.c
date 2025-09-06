#include <stdio.h>
#include <string.h>
#include "../inc/engine.h"

ExecutionResult engine_execute(Storage *storage, const KvCommand *cmd) {
    ExecutionResult result = {0};

    switch (cmd->type) {
        case CMD_SET:
            if (storage_set(storage, cmd->key, cmd->value) == 0) {
                snprintf(result.message, sizeof(result.message), "{\"status\":\"ok\"}");
                result.code = 0;
            }
            else {
                snprintf(result.message, sizeof(result.message), "{\"error\":\"set failed\"}");
                result.code = -1;
            }
            break;
        case CMD_GET: {
            const char *raw_val = storage_get(storage, cmd->key);
            if (raw_val) {
                size_t json_prefix_len = strlen("{\"value\":\"");
                size_t json_suffix_len = strlen("\"}");
                size_t val_len = strlen(raw_val);

                if (json_prefix_len + val_len + json_suffix_len < sizeof(result.message)) {
                    snprintf(result.message, sizeof(result.message), "{\"value\":\"%s\"}", raw_val);
                    result.code = 0;
                }
                else {
                    snprintf(result.message, sizeof(result.message), "{\"error\":\"value too large\"}");
                    result.code = -1;
                }
            }
            else {
                snprintf(result.message, sizeof(result.message), "{\"error\":\"not found\"}");
                result.code = -1;
            }
            break;
        }
        default:
            snprintf(result.message, sizeof(result.message), "{\"error\":\"unknown command\"}");
            result.code = -1;
    }
    return result;
}