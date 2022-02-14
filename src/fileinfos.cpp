#include "fileinfos.h"
#include "utils/logger.h"
#include <coreinit/cache.h>
#include <coreinit/mutex.h>
#include <cstdio>
#include <cstring>
#include <rpxloader.h>

FileInfos gFileInfos[FILE_INFO_SIZE] __attribute__((section(".data")));
extern OSMutex fileinfoMutex;

int32_t getIDByLowerTitleID(uint32_t lowerTitleID) {
    OSLockMutex(&fileinfoMutex);
    int res = -1;
    for (int i = 0; i < FILE_INFO_SIZE; i++) {
        if (strlen(gFileInfos[i].path) > 0 && gFileInfos[i].lowerTitleID == lowerTitleID) {
            res = i;
            break;
        }
    }
    OSMemoryBarrier();
    OSUnlockMutex(&fileinfoMutex);
    return res;
}

void unmountRomfs(uint32_t id) {
    if (id >= FILE_INFO_SIZE) {
        return;
    }
    OSLockMutex(&fileinfoMutex);
    if (gFileInfos[id].romfsMounted) {
        char romName[10];
        snprintf(romName, 10, "%08X", id);
        DEBUG_FUNCTION_LINE("Unmounting %s", romName);
        int res = RL_UnmountBundle(romName);
        DEBUG_FUNCTION_LINE("res: %d", res);
        gFileInfos[id].romfsMounted = false;
    }
    OSMemoryBarrier();
    OSUnlockMutex(&fileinfoMutex);
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
    OSLockMutex(&fileinfoMutex);
    bool result = false;
    if (!gFileInfos[id].romfsMounted) {
        char buffer[256];
        snprintf(buffer, 256, "/vol/external01/%s", gFileInfos[id].path);
        char romName[10];
        snprintf(romName, 10, "%08X", id);
        DEBUG_FUNCTION_LINE("Mount %s as %s", buffer, romName);
        int32_t res = 0;
        if ((res = RL_MountBundle(romName, buffer, BundleSource_FileDescriptor_CafeOS)) == 0) {
            DEBUG_FUNCTION_LINE("Mounted successfully ");
            gFileInfos[id].romfsMounted = true;
            result                      = true;
        } else {
            DEBUG_FUNCTION_LINE("Mounting failed %d", res);
            result = false;
        }
    }

    OSMemoryBarrier();
    OSUnlockMutex(&fileinfoMutex);
    return result;
}