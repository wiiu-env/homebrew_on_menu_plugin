#pragma once
#include "../FileInfos.h"
#include "FileReader.h"
#include <wuhb_utils/utils.h>


class FileReaderWUHB : public FileReader {
    bool initDone = false;
    std::shared_ptr<FileInfos> info;
    WUHBFileHandle fileHandle = 0;
    bool autoUnmount          = false;

public:
    explicit FileReaderWUHB(const std::shared_ptr<FileInfos> &info, const std::string &relativeFilepath, bool autoUnmount);
    ~FileReaderWUHB() override;
    int64_t read(uint8_t *buffer, uint32_t size) override;
    bool isReady() override;
    void UnmountBundle();
};
