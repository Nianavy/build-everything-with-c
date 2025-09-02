#include <stdio.h>
#include <string.h>
#include "../inc/parser.h"
#include "../inc/storage.h"
#include "../inc/engine.h"

int main() {
    Storage *store = storage_create();
    char input[512];
    KvCommand cmd;

    printf("tinykvweb > ");

    while (fgets(input, sizeof(input), stdin)) {
        if (parse_input(input, &cmd) == 0) engine_execute(store, &cmd);
        else printf("Invalied command\n");
        printf("tinykvweb > ");
    }

    storage_free(store);
    return 0;
}