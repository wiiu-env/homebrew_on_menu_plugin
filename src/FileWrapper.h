#pragma once
#include <cstdint>

typedef struct FileHandleWrapper_t {
    uint32_t handle;
    bool inUse;
} FileHandleWrapper;

#define FILE_WRAPPER_SIZE 64
extern FileHandleWrapper gFileHandleWrapper[FILE_WRAPPER_SIZE];

int OpenFileForID(int id, const char *path, int32_t *handle);
bool FileHandleWrapper_FreeAll();