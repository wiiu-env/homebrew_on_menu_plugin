#pragma once
#include <cstdint>
#include <coreinit/mcp.h>

#include <romfs_dev.h>

typedef struct WUT_PACKED FileInfos_ {
    char path[256];
    char name[256];
    int32_t source;
    uint32_t lowerTitleID;
    bool romfsMounted;
    int openedFiles;
    MCPTitleListType titleInfo;
} FileInfos;

#define FILE_INFO_SIZE          300
extern FileInfos gFileInfos[FILE_INFO_SIZE];

int32_t getIDByLowerTitleID(uint32_t lowerTitleID);

void unmountAllRomfs();

void unmountRomfs(uint32_t id);

bool mountRomfs(uint32_t id);

int32_t getRPXInfoForID(uint32_t id, romfs_fileInfo *info);
