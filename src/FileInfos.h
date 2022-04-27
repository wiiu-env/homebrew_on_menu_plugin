#pragma once
#include "utils/logger.h"
#include "utils/utils.h"
#include <coreinit/mcp.h>
#include <mutex>
#include <string>
#include <wuhb_utils/utils.h>

class FileInfos {
public:
    explicit FileInfos(const std::string &relativePath) : relativeFilepath(relativePath) {
        this->lowerTitleID = hash_string(relativePath.c_str());
    }
    ~FileInfos() {
        std::lock_guard<std::mutex> lock(mountLock);
        if (isMounted) {
            UnmountBundle();
        }
    }
    bool MountBundle(const std::string &romfsName) {
        if (isMounted) {
            if (mountPath != romfsName) {
                DEBUG_FUNCTION_LINE_ERR("Can't mount as %s because it's already mounted with a different name (%s)", romfsName.c_str(), mountPath.c_str());
                return false;
            }
            return true;
        }
        if (!isBundle) {
            DEBUG_FUNCTION_LINE_VERBOSE("Mounting not possible, is not a bundle");
            return false;
        }
        auto fullMountPath = std::string("/vol/external01/").append(this->relativeFilepath);
        int32_t outRes     = -1;
        if (WUHBUtils_MountBundle(romfsName.c_str(), fullMountPath.c_str(), BundleSource_FileDescriptor_CafeOS, &outRes) != WUHB_UTILS_RESULT_SUCCESS || outRes < 0) {
            DEBUG_FUNCTION_LINE_ERR("Failed to mount bundle: %s", romfsName.c_str());
            return false;
        }
        DEBUG_FUNCTION_LINE_VERBOSE("Succesfully mounted %s", romfsName.c_str());
        this->isMounted = true;
        mountPath       = romfsName;
        return true;
    }

    bool UnmountBundle() {
        if (!isBundle) {
            DEBUG_FUNCTION_LINE_VERBOSE("Skip unmounting, is not a bundle");
            return true;
        }
        if (!isMounted) {
            DEBUG_FUNCTION_LINE_VERBOSE("Skip unmounting, is not mounted");
            return true;
        }
        int32_t outRes = -1;
        if (WUHBUtils_UnmountBundle(mountPath.c_str(), &outRes) != WUHB_UTILS_RESULT_SUCCESS || outRes < 0) {
            DEBUG_FUNCTION_LINE_ERR("Failed to unmount bundle: %s", mountPath.c_str());
            return false;
        } else {
            DEBUG_FUNCTION_LINE_VERBOSE("Successfully unmounted bundle %s", this->mountPath.c_str());
        }
        this->isMounted = false;
        this->mountPath.clear();

        return true;
    }

    std::string relativeFilepath;
    std::string filename;

    std::string longname;
    std::string shortname;
    std::string author;

    uint32_t lowerTitleID;
    MCPTitleListType titleInfo{};

    int32_t fileCount = 0;

    bool isBundle = false;

    std::mutex accessLock;

private:
    std::mutex mountLock;
    std::string mountPath;
    bool isMounted = false;
};