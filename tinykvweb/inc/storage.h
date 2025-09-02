#ifndef STORAGE_H
#define STORAGE_H

typedef struct Storage Storage;

Storage *storage_create();

void storage_free(Storage *storage);

int storage_set(Storage *storage, const char *key, const char *value);

const char *storage_get(Storage *storage, const char *key);

#endif