#pragma once

#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

class FileReader {

public:
    FileReader(uint8_t *buffer, uint32_t size);

    explicit FileReader(std::string &path);

    virtual ~FileReader();

    virtual int64_t read(uint8_t *buffer, uint32_t size);

    virtual bool isReady();

    virtual uint32_t getHandle() {
        return reinterpret_cast<uint32_t>(this);
    }

protected:
    FileReader() = default;

private:
    bool isReadFromBuffer = false;
    uint8_t *input_buffer = nullptr;
    uint32_t input_size   = 0;
    uint32_t input_pos    = 0;

    bool isReadFromFile = false;
    int file_fd         = 0;
};
