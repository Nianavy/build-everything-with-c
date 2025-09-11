#include "../inc/storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ITEMS 1024
#define KEY_SIZE 128
#define VALUE_SIZE 256

typedef struct {
    char key[KEY_SIZE];
    char value[VALUE_SIZE];
} KvPair;

struct Storage {
    KvPair items[MAX_ITEMS];
    int count;
};

Storage *storage_create() {
    Storage *s = malloc(sizeof(Storage));
    if (s) s->count = 0;
    return s;
}

void storage_free(Storage *storage) {
    free(storage);
}

int storage_set(Storage *storage, const char *key, const char *value) {
    for (int i = 0; i < storage->count; ++i) {
        if (strcmp(storage->items[i].key, key) == 0) {
            strncpy(storage->items[i].value, value, VALUE_SIZE);
            return 0;
        }
    }
    if (storage->count >= MAX_ITEMS) return -1;
    strncpy(storage->items[storage->count].key, key, KEY_SIZE);
    strncpy(storage->items[storage->count].value, value, VALUE_SIZE);
    storage->count++;
    return 0;
}

const char *storage_get(Storage *storage, const char *key) {
    for (int i = 0; i < storage->count; ++i) {
        if (strcmp(storage->items[i].key, key) == 0)
            return storage->items[i].value;
    }
    return NULL;
}