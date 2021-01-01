#include "romfs_helper.h"
#include "utils/logger.h"
#include "utils/StringTools.h"
#include <stdio.h>
#include <sys/dir.h>

FileInfos gFileInfos[FILE_INFO_SIZE] __attribute__((section(".data")));

int32_t getIDByLowerTitleID(uint32_t lowerTitleID) {
    for (int i = 0; i < FILE_INFO_SIZE; i++) {
        if (strlen(gFileInfos[i].path) > 0 && gFileInfos[i].lowerTitleID == lowerTitleID) {
            return i;
        }
    }
    return -1;
}

void unmountRomfs(uint32_t id) {
    if (id >= FILE_INFO_SIZE) {
        return;
    }
    if (gFileInfos[id].romfsMounted) {
        char romName[10];
        snprintf(romName, 10, "%08X", id);
        DEBUG_FUNCTION_LINE("Unmounting %s", romName);
        int res = romfsUnmount(romName);
        DEBUG_FUNCTION_LINE("res: %d", res);
        gFileInfos[id].romfsMounted = false;
    }
}

void unmountAllRomfs() {
    for (int i = 0; i < FILE_INFO_SIZE; i++) {
        unmountRomfs(i);
    }
}

bool mountRomfs(uint32_t id) {
    if (id >= FILE_INFO_SIZE) {
        DEBUG_FUNCTION_LINE("HANDLE WAS TOO BIG %d", id);
        return false;
    }
    if (!gFileInfos[id].romfsMounted) {
        char buffer[256];
        snprintf(buffer, 256, "/vol/external01/%s", gFileInfos[id].path);
        char romName[10];
        snprintf(romName, 10, "%08X", id);
        DEBUG_FUNCTION_LINE("Mount %s as %s", buffer, romName);
        int32_t  res = 0;
        if ((res = romfsMount(romName, buffer, RomfsSource_FileDescriptor_CafeOS)) == 0) {
            DEBUG_FUNCTION_LINE("Mounted successfully ");
            gFileInfos[id].romfsMounted = true;
            return true;
        } else {
            DEBUG_FUNCTION_LINE("Mounting failed %d", res);
            return false;
        }
    }
    return true;
}


int32_t getRPXInfoForID(uint32_t id, romfs_fileInfo *info) {
    if (!mountRomfs(id)) {
        return -1;
    }
    DIR *dir;
    struct dirent *entry;
    char romName[10];
    snprintf(romName, 10, "%08X", id);

    char root[12];
    snprintf(root, 12, "%08X:/", id);

    if (!(dir = opendir(root))) {
        return -2;
    }
    bool found = false;
    int res = -3;
    while ((entry = readdir(dir)) != NULL) {
        if (StringTools::EndsWith(entry->d_name, ".rpx")) {
            if (romfsGetFileInfoPerPath(romName, entry->d_name, info) >= 0) {
                found = true;
                res = 0;
            }
            break;
        }
    }

    closedir(dir);

    if (!found) {
        return -4;
    }
    return res;
}
