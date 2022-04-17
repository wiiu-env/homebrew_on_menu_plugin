#include "FileWrapper.h"
#include "fileinfos.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include <coreinit/cache.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <wuhb_utils/utils.h>

FileHandleWrapper gFileHandleWrapper[FILE_WRAPPER_SIZE] __attribute__((section(".data")));

std::mutex fileWrapperMutex;

int FileHandleWrapper_GetSlot() {
    std::lock_guard<std::mutex> lock(fileWrapperMutex);
    int res = -1;
    for (int i = 0; i < FILE_WRAPPER_SIZE; i++) {
        if (!gFileHandleWrapper[i].inUse) {
            gFileHandleWrapper[i].inUse = true;
            res                         = i;
            break;
        }
    }
    OSMemoryBarrier();
    return res;
}

bool FileHandleWrapper_FreeSlot(uint32_t slot) {
    if (slot >= FILE_WRAPPER_SIZE) {
        return false;
    }
    std::lock_guard<std::mutex> lock(fileWrapperMutex);
    gFileHandleWrapper[slot].handle = 0;
    gFileHandleWrapper[slot].inUse  = false;
    OSMemoryBarrier();
    return -1;
}

bool FileHandleWrapper_FreeAll() {
    std::lock_guard<std::mutex> lock(fileWrapperMutex);
    for (int i = 0; i < FILE_WRAPPER_SIZE; i++) {
        FileHandleWrapper_FreeSlot(i);
    }
    return -1;
}

int OpenFileForID(int id, const char *filepath, uint32_t *handle) {
    if (!mountRomfs(id)) {
        return -1;
    }
    char romName[10];
    snprintf(romName, 10, "%08X", id);

    char *dyn_path = (char *) malloc(strlen(filepath) + 1);
    char last      = 0;
    int j          = 0;
    for (int i = 0; filepath[i] != 0; i++) {
        if (filepath[i] == '/') {
            if (filepath[i] != last) {
                dyn_path[j++] = filepath[i];
            }
        } else {
            dyn_path[j++] = filepath[i];
        }
        last = filepath[i];
    }
    dyn_path[j] = 0;

    auto completePath = string_format("%s:/%s", romName, dyn_path);

    WUHBFileHandle fileHandle = 0;
    if (WUHBUtils_FileOpen(completePath.c_str(), &fileHandle) == WUHB_UTILS_RESULT_SUCCESS) {
        int handle_wrapper_slot = FileHandleWrapper_GetSlot();

        if (handle_wrapper_slot < 0) {
            DEBUG_FUNCTION_LINE_ERR("No free slot");
            if (WUHBUtils_FileClose(fileHandle) != WUHB_UTILS_RESULT_SUCCESS) {
                DEBUG_FUNCTION_LINE_ERR("Failed to close file %08X", fileHandle);
            }
            unmountRomfs(id);
            return -2;
        }
        gFileHandleWrapper[handle_wrapper_slot].handle = fileHandle;
        *handle                                        = 0xFF000000 | (id << 12) | (handle_wrapper_slot & 0x00000FFF);
        gFileInfos[id].openedFiles++;
        return 0;
    } else {
        DEBUG_FUNCTION_LINE_ERR("Failed to open file %s", filepath);
        if (gFileInfos[id].openedFiles == 0) {
            unmountRomfs(id);
        }
    }

    return -1;
}