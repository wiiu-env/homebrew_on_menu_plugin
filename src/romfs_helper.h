#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <wut_romfs_dev.h>



typedef struct WUT_PACKED FileInfos_ {
    char path[256];
    char name[256];
    int32_t source;
    bool romfsMounted;
    int openedFiles;
} FileInfos;

#define FILE_INFO_SIZE          300
extern FileInfos gFileInfos[FILE_INFO_SIZE];



void unmountAllRomfs();
void unmountRomfs(uint32_t id);
bool mountRomfs(uint32_t id);

int32_t getRPXInfoForID(uint32_t id, romfs_fileInfo * info);
