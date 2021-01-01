#include "readFileWrapper.h"
#include "fs/FSUtils.h"
#include "utils/logger.h"

#include <cstdio>
#include <fcntl.h>
#include "romfs_helper.h"
#include <cstdlib>

fileReadInformation gFileReadInformation[FILE_READ_INFO_SIZE] __attribute__((section(".data")));

int readFile(int slot, uint8_t *buffer, uint32_t size) {
    fileReadInformation *info = &gFileReadInformation[slot];
    if (!info->compressed) {
        //DEBUG_FUNCTION_LINE("non compressed");
        return read(info->fd, buffer, size);
    } else {
        int startValue = info->strm.total_out;
        uint32_t newSize = 0;
        int ret = 0;
        //DEBUG_FUNCTION_LINE("We want to read %d", size);
        //DEBUG_FUNCTION_LINE("startValue %d ",startValue);
        do {
            int CHUNK = 0x1000;
            uint32_t nextOut = CHUNK;
            if (nextOut > size) {
                nextOut = size;
            }
            //DEBUG_FUNCTION_LINE("nextOut = %d",nextOut);
            if (info->strm.avail_in == 0) {
                //DEBUG_FUNCTION_LINE("Reading %d from compressed stream",CHUNK);
                info->strm.avail_in = read(info->fd, info->in, CHUNK);
                if (info->strm.avail_in == 0) {
                    DEBUG_FUNCTION_LINE("strm.avail_in is 0");
                    break;
                }
                info->strm.next_in = info->in;
            }
            //DEBUG_FUNCTION_LINE("info->strm.avail_in = %d",info->strm.avail_in);
            //DEBUG_FUNCTION_LINE("info->strm.next_in = %d",info->strm.next_in);

            /* run inflate() on input until output buffer not full */
            do {
                //DEBUG_FUNCTION_LINE("newSize %d, size %d, info->strm.avail_out %d", newSize, size, info->strm.avail_out);

                if (nextOut > size - newSize) {
                    nextOut = size - newSize;
                }

                info->strm.avail_out = nextOut;
                //DEBUG_FUNCTION_LINE("info->strm.avail_out = %d",info->strm.avail_out);
                info->strm.next_out = (buffer + newSize);
                //DEBUG_FUNCTION_LINE("info->strm.next_out = %08X",info->strm.next_out);
                ret = inflate(&info->strm, Z_NO_FLUSH);
                //DEBUG_FUNCTION_LINE("ret = %d",ret);
                if (ret == Z_STREAM_ERROR) {
                    DEBUG_FUNCTION_LINE("Z_STREAM_ERROR");
                    return 0;
                }

                switch (ret) {
                    case Z_NEED_DICT:
                        DEBUG_FUNCTION_LINE("Z_NEED_DICT");
                        ret = Z_DATA_ERROR;     /* and fall through */
                    case Z_DATA_ERROR:
                    case Z_MEM_ERROR:
                        DEBUG_FUNCTION_LINE("Z_MEM_ERROR or Z_DATA_ERROR");
                        (void) inflateEnd(&info->strm);
                        return ret;
                }

                //int canBeWritten = CHUNK - info->strm.avail_out;
                //DEBUG_FUNCTION_LINE("canBeWritten = %d",canBeWritten);

                newSize = info->strm.total_out - startValue;
                if (newSize == size) {
                    //DEBUG_FUNCTION_LINE("newSize was as wanted %d", newSize);
                    break;
                }
                nextOut = CHUNK;
                if (newSize + nextOut >= (size)) {
                    nextOut = (size) - newSize;
                }
                //DEBUG_FUNCTION_LINE("newSize = %d",newSize);
                //DEBUG_FUNCTION_LINE("nextOut = %d",nextOut);
            } while (info->strm.avail_out == 0 && newSize < (size));

            /* done when inflate() says it's done */
        } while (ret != Z_STREAM_END && newSize < size);

        return newSize;
    }
}


bool DeInitFile(int slot) {
    if (gFileReadInformation[slot].compressed && gFileReadInformation[slot].cInitDone) {
        /* clean up and return */
        (void) inflateEnd(&(gFileReadInformation[slot].strm));
    }

    close(gFileReadInformation[slot].fd);
    gFileReadInformation[slot].inUse = false;
    memset(&gFileReadInformation[slot], 0, sizeof(fileReadInformation));
    return true;
}


void DeInitAllFiles() {
    for (int i = 0; i < FILE_READ_INFO_SIZE; i++) {
        fileReadInformation *info = &gFileReadInformation[i];
        if (info->inUse) {
            DeInitFile(i);
        }
    }
}

int fileReadInformation_getSlot() {
    for (int i = 0; i < 32; i++) {
        if (!gFileReadInformation[i].inUse) {
            gFileReadInformation[i].inUse = true;
            return i;
        }
    }
    return -1;
}


bool initCompressedFileReadInformation(fileReadInformation *info) {
    if (info == nullptr || !info->compressed) {
        info->cInitDone = false;
        return false;
    }
    if (info->cInitDone) {
        return true;
    }
    /* allocate inflate state */
    info->strm.zalloc = Z_NULL;
    info->strm.zfree = Z_NULL;
    info->strm.opaque = Z_NULL;
    info->strm.avail_in = 0;
    info->strm.next_in = Z_NULL;
    int ret = inflateInit2(&info->strm, MAX_WBITS | 16); //gzip
    if (ret != Z_OK) {
        DEBUG_FUNCTION_LINE("ret != Z_OK");
        info->cInitDone = false;
        return false;
    }
    info->cInitDone = true;
    return true;
}


int32_t loadFileIntoBuffer(uint32_t lowerTitleID, const char *filepath, char *buffer, int sizeToRead) {
    int32_t id = getIDByLowerTitleID(lowerTitleID);
    if (id < 0) {
        DEBUG_FUNCTION_LINE("Failed to get id by titleid");
        return -3;
    }
    if (!mountRomfs(id)) {
        return -1;
    }
    int handle = 0;
    if (FSOpenFile_for_ID(id, filepath, &handle) != 0) {
        return -2;
    }

    int32_t fd = (handle & 0x00000FFF);
    int32_t romid = (handle & 0x00FFF000) >> 12;

    DEBUG_FUNCTION_LINE("READ %d from %d rom: %d", sizeToRead, fd, romid);

    int readSize = readFile(fd, (uint8_t *) buffer, (sizeToRead));

    DEBUG_FUNCTION_LINE("Close %d %d", fd, romid);
    DeInitFile(fd);
    if (gFileInfos[romid].openedFiles--) {
        if (gFileInfos[romid].openedFiles <= 0) {
            DEBUG_FUNCTION_LINE("unmount romfs no more handles");
            unmountRomfs(romid);
        }
    }

    return readSize;
}


int32_t FSOpenFile_for_ID(uint32_t id, const char *filepath, int *handle) {
    if (!mountRomfs(id)) {
        return -1;
    }
    char romName[10];
    snprintf(romName, 10, "%08X", id);

    char *test = (char *) malloc(strlen(filepath) + 1);
    char last = 0;
    int j = 0;
    for (int i = 0; filepath[i] != 0; i++) {
        if (filepath[i] == '/') {
            if (filepath[i] != last) {
                test[j++] = filepath[i];
            }
        } else {
            test[j++] = filepath[i];
        }
        last = filepath[i];
    }
    test[j++] = 0;

    char buffer[256];
    snprintf(buffer, 256, "%s:/%s.gz", romName, test);

    bool nonCompressed = false;
    if (!FSUtils::CheckFile(buffer)) {
        snprintf(buffer, 256, "%s:/%s", romName, test);
        free(test);
        if (!FSUtils::CheckFile(buffer)) {
            return -3;
        }
        nonCompressed = true;
    }
    free(test);

    int fd = open(buffer, 0);
    if (fd >= 0) {
        DEBUG_FUNCTION_LINE("Opened %s from %s ", buffer, romName);
        int slot = fileReadInformation_getSlot();
        if (slot < 0) {
            DEBUG_FUNCTION_LINE("Failed to get a slot");
            close(fd);
            return -5;
        }
        fileReadInformation *info = &gFileReadInformation[slot];
        info->fd = fd;
        if (!nonCompressed) {
            info->compressed = true;
            initCompressedFileReadInformation(info);
            DEBUG_FUNCTION_LINE("Init compressed, got slot %d", slot);
        } else {
            info->cInitDone = true;
        }
        *handle = 0xFF000000 | (id << 12) | (slot & 0x00000FFF);
        gFileInfos[id].openedFiles++;
        return 0;
    }
    return -2;
}
