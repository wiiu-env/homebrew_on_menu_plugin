#include "utils.h"
#include "logger.h"
#include <coreinit/mcp.h>
#include <string>

/* hash: compute hash value of string */
unsigned int hash_string(const char *str) {
    unsigned int h;
    unsigned char *p;

    h = 0;
    for (p = (unsigned char *) str; *p != '\0'; p++) {
        h = 37 * h + *p;
    }
    return h; // or, h % ARRAY_SIZE;
}

bool Utils::GetSerialId(std::string &serialID) {
    bool result = false;
    alignas(0x40) MCPSysProdSettings settings{};
    auto handle = MCP_Open();
    if (handle >= 0) {
        if (MCP_GetSysProdSettings(handle, &settings) == 0) {
            serialID = std::string(settings.code_id) + settings.serial_id;
            result   = true;
        } else {
            DEBUG_FUNCTION_LINE_ERR("Failed to get SerialId");
        }
        MCP_Close(handle);
    } else {
        DEBUG_FUNCTION_LINE_ERR("MCP_Open failed");
    }
    return result;
}