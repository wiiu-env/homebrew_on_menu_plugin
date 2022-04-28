#include "FileReaderWUHB.h"
#include "utils/StringTools.h"
#include "utils/logger.h"
#include <coreinit/cache.h>
#include <wuhb_utils/utils.h>

FileReaderWUHB::FileReaderWUHB(const std::shared_ptr<FileInfos> &info, const std::string &relativeFilepath, bool autoUnmount) {
    if (!info) {
        DEBUG_FUNCTION_LINE_ERR("Info was NULL");
        return;
    }
    if (!info->isBundle) {
        DEBUG_FUNCTION_LINE("Failed to init file reader for %s, is not a bundle.", info->relativeFilepath.c_str());
        return;
    }
    this->autoUnmount = autoUnmount;
    this->info        = info;
    std::lock_guard<std::mutex> lock(info->accessLock);

    auto romfsName = string_format("%08X", info->lowerTitleID);

    if (!info->MountBundle(romfsName)) {
        return;
    }

    auto filepath = romfsName.append(":/").append(relativeFilepath);

    WUHBUtilsStatus status;
    if ((status = WUHBUtils_FileOpen(filepath.c_str(), &this->fileHandle)) != WUHB_UTILS_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE("Failed to open file in bundle: %s error: %d", filepath.c_str(), status);

        UnmountBundle();

        return;
    }
    this->info->fileCount++;

    this->initDone = true;
    OSMemoryBarrier();
}

FileReaderWUHB::~FileReaderWUHB() {
    if (!this->initDone) {
        return;
    }

    std::lock_guard<std::mutex> lock(info->accessLock);

    if (this->fileHandle != 0) {
        if (WUHBUtils_FileClose(this->fileHandle) != WUHB_UTILS_RESULT_SUCCESS) {
            DEBUG_FUNCTION_LINE_ERR("WUHBUtils_FileClose failed for %08X", this->fileHandle);
        }
        this->fileHandle = 0;
        info->fileCount--;
    }

    UnmountBundle();
    OSMemoryBarrier();
}

int64_t FileReaderWUHB::read(uint8_t *buffer, uint32_t size) {
    if (!this->initDone) {
        DEBUG_FUNCTION_LINE_ERR("read file but init was not successful");
        return -1;
    }
    int32_t outRes = -1;
    if (WUHBUtils_FileRead(this->fileHandle, buffer, size, &outRes) == WUHB_UTILS_RESULT_SUCCESS) {
        return outRes;
    }
    DEBUG_FUNCTION_LINE_ERR("WUHBUtils_FileRead failed");
    return -1;
}

bool FileReaderWUHB::isReady() {
    return this->initDone;
}
void FileReaderWUHB::UnmountBundle() {
    if (autoUnmount && info->fileCount <= 0) {
        if (!info->UnmountBundle()) {
            DEBUG_FUNCTION_LINE_ERR("Failed to unmount");
        }
    } else {
        DEBUG_FUNCTION_LINE_VERBOSE("Filecount is %d, we don't want to unmount yet", info->fileCount);
    }
}
