#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <zlib.h>

typedef struct {
    bool inUse;
    int fd;
    bool compressed;
    bool cInitDone;
    bool cCloseDone;
    z_stream strm;
    unsigned char in[0x1000];
} fileReadInformation;

#define FILE_READ_INFO_SIZE     32

extern fileReadInformation gFileReadInformation[FILE_READ_INFO_SIZE];

bool initFile(int slot);

int readFile(int slot, uint8_t *buffer, uint32_t size);

bool DeInitFile(int slot);

void DeInitAllFiles();

int32_t loadFileIntoBuffer(uint32_t id, char *filepath, char *buffer, int sizeToRead);

int32_t FSOpenFile_for_ID(uint32_t id, const char *filepath, int *handle);

int fileReadInformation_getSlot();

bool initCompressedFileReadInformation(fileReadInformation *info);