#include "FileWrapper.h"
#include <cstdlib>
#include "fileinfos.h"
#include <cstring>
#include <cstdio>
#include <rpxloader.h>
#include "utils/logger.h"
#include <coreinit/cache.h>

FileHandleWrapper gFileHandleWrapper[FILE_WRAPPER_SIZE] __attribute__((section(".data")));

int FileHandleWrapper_GetSlot() {
    for (int i = 0; i < FILE_WRAPPER_SIZE; i++) {
        if (!gFileHandleWrapper[i].inUse) {
            gFileHandleWrapper[i].inUse = true;
            DCFlushRange(&gFileHandleWrapper[i], sizeof(FileHandleWrapper));
            return i;
        }
    }
    return -1;
}

int OpenFileForID(int id, const char *filepath, int *handle) {
    if (!mountRomfs(id)) {
        return -1;
    }
    char romName[10];
    snprintf(romName, 10, "%08X", id);

    char *dyn_path = (char *) malloc(strlen(filepath) + 1);
    char last = 0;
    int j = 0;
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
    dyn_path[j++] = 0;

    char completePath[256];
    snprintf(completePath, 256, "%s:/%s", romName, dyn_path);
    free(dyn_path);

    uint32_t out_handle = 0;
    if (RL_FileOpen(completePath, &out_handle) == 0) {
        int handle_wrapper_slot = FileHandleWrapper_GetSlot();

        if (handle_wrapper_slot < 0) {
            DEBUG_FUNCTION_LINE("No free slot");
            RL_FileClose(out_handle);
            return -2;
        }
        gFileHandleWrapper[handle_wrapper_slot].handle = out_handle;
        *handle = 0xFF000000 | (id << 12) | (handle_wrapper_slot & 0x00000FFF);
        gFileInfos[id].openedFiles++;
        return 0;
    } else {
        if (gFileInfos[id].openedFiles == 0) {
            DEBUG_FUNCTION_LINE("unmount");
            unmountRomfs(id);
        }
    }

    return -1;
}