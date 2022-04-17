#include "fileinfos.h"
#include "utils/logger.h"
#include <coreinit/cache.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <wuhb_utils/utils.h>

FileInfos gFileInfos[FILE_INFO_SIZE] __attribute__((section(".data")));
std::mutex fileinfoMutex;

int32_t getIDByLowerTitleID(uint32_t lowerTitleID) {
    std::lock_guard<std::mutex> lock(fileinfoMutex);
    int res = -1;
    for (int i = 0; i < FILE_INFO_SIZE; i++) {
        if (strlen(gFileInfos[i].path) > 0 && gFileInfos[i].lowerTitleID == lowerTitleID) {
            res = i;
            break;
        }
    }
    OSMemoryBarrier();
    return res;
}

void unmountRomfs(uint32_t id) {
    if (id >= FILE_INFO_SIZE) {
        return;
    }
    std::lock_guard<std::mutex> lock(fileinfoMutex);
    if (gFileInfos[id].romfsMounted) {
        char romName[10];
        snprintf(romName, 10, "%08X", id);
        int32_t outRes;
        if (WUHBUtils_UnmountBundle(romName, &outRes) || outRes != 0) {
            DEBUG_FUNCTION_LINE_ERR("Failed to unmount \"%s\"", romName);
        }
        gFileInfos[id].romfsMounted = false;
    }
    OSMemoryBarrier();
}

void unmountAllRomfs() {
    for (int i = 0; i < FILE_INFO_SIZE; i++) {
        unmountRomfs(i);
    }
}

bool mountRomfs(uint32_t id) {
    if (id >= FILE_INFO_SIZE) {
        DEBUG_FUNCTION_LINE_ERR("HANDLE WAS TOO BIG %d", id);
        return false;
    }
    std::lock_guard<std::mutex> lock(fileinfoMutex);
    bool result = false;
    if (!gFileInfos[id].romfsMounted) {
        char buffer[256];
        snprintf(buffer, 256, "/vol/external01/%s", gFileInfos[id].path);
        char romName[10];
        snprintf(romName, 10, "%08X", id);
        DEBUG_FUNCTION_LINE("Mount %s as %s", buffer, romName);
        int32_t res = 0;
        if (WUHBUtils_MountBundle(romName, buffer, BundleSource_FileDescriptor_CafeOS, &res) == WUHB_UTILS_RESULT_SUCCESS && res == 0) {
            DEBUG_FUNCTION_LINE("Mounted successfully ");
            gFileInfos[id].romfsMounted = true;
            result                      = true;
        } else {
            DEBUG_FUNCTION_LINE_ERR("Mounting failed %d", res);
            result = false;
        }
    }

    OSMemoryBarrier();
    return result;
}