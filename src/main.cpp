#include "FileInfos.h"
#include "SaveRedirection.h"
#include "filelist.h"
#include "fs/FSUtils.h"
#include "fs/FileReader.h"
#include "fs/FileReaderWUHB.h"
#include "utils/StringTools.h"
#include "utils/ini.h"
#include <content_redirection/redirection.h>
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/filesystem.h>
#include <coreinit/mcp.h>
#include <coreinit/mutex.h>
#include <coreinit/systeminfo.h>
#include <coreinit/title.h>
#include <cstring>
#include <forward_list>
#include <fs/DirList.h>
#include <malloc.h>
#include <mutex>
#include <nn/acp.h>
#include <optional>
#include <rpxloader/rpxloader.h>
#include <sdutils/sdutils.h>
#include <sysapp/launch.h>
#include <sysapp/title.h>
#include <utils/logger.h>
#include <wuhb_utils/utils.h>
#include <wups.h>

typedef struct ACPMetaData {
    char bootmovie[80696];
    char bootlogo[28604];
} ACPMetaData;

WUPS_PLUGIN_NAME("Homebrew in Wii U menu");
WUPS_PLUGIN_DESCRIPTION("Allows the user to load homebrew from the Wii U menu");
WUPS_PLUGIN_VERSION("0.1");
WUPS_PLUGIN_AUTHOR("Maschell");
WUPS_PLUGIN_LICENSE("GPL");

#define UPPER_TITLE_ID_HOMEBREW 0x0005000F
#define TITLE_ID_HOMEBREW_MASK  (((uint64_t) UPPER_TITLE_ID_HOMEBREW) << 32)

ACPMetaXml gLaunchXML __attribute__((section(".data")));
MCPTitleListType current_launched_title_info __attribute__((section(".data")));
BOOL gHomebrewLaunched __attribute__((section(".data")));

std::mutex fileInfosMutex;
std::forward_list<std::shared_ptr<FileInfos>> fileInfos;

std::mutex fileReaderListMutex;
std::forward_list<std::unique_ptr<FileReader>> openFileReaders;

void readCustomTitlesFromSD();

WUPS_USE_WUT_DEVOPTAB();

INITIALIZE_PLUGIN() {
    memset((void *) &current_launched_title_info, 0, sizeof(current_launched_title_info));
    memset((void *) &gLaunchXML, 0, sizeof(gLaunchXML));
    gHomebrewLaunched = FALSE;

    // Use libwuhbutils.
    WUHBUtilsStatus error;
    if ((error = WUHBUtils_Init()) != WUHB_UTILS_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Homebrew on Menu Plugin: Failed to init WUHBUtils. Error %d", error);
        OSFatal("Homebrew on Menu Plugin: Failed to init WUHBUtils.");
    }

    // Use libcontentredirection.
    ContentRedirectionStatus error2;
    if ((error2 = ContentRedirection_Init()) != CONTENT_REDIRECTION_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Homebrew on Menu Plugin: Failed to init ContentRedirection. Error %d", error2);
        OSFatal("Homebrew on Menu Plugin: Failed to init ContentRedirection.");
    }

    // Use librpxloader.
    RPXLoaderStatus error3;
    if ((error3 = RPXLoader_InitLibrary()) != RPX_LOADER_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Homebrew on Menu Plugin: Failed to init RPXLoader. Error %d", error3);
        OSFatal("Homebrew on Menu Plugin: Failed to init RPXLoader.");
    }
}

bool sSDUtilsInitDone = false;
bool sSDIsMounted     = false;
bool sTitleRebooting  = false;

void Cleanup() {
    {
        const std::lock_guard<std::mutex> lock1(fileReaderListMutex);
        openFileReaders.clear();
    }
    {
        const std::lock_guard<std::mutex> lock(fileInfosMutex);
        fileInfos.clear();
    }
}

void SDCleanUpHandlesHandler() {
    Cleanup();
}

void SDAttachedHandler([[maybe_unused]] SDUtilsAttachStatus status) {
    if (!sTitleRebooting) {
        _SYSLaunchTitleWithStdArgsInNoSplash(OSGetTitleID(), nullptr);
        sTitleRebooting = true;
    }
}

ON_APPLICATION_START() {
    Cleanup();
    initLogging();
    sSDIsMounted = false;

    if (OSGetTitleID() == 0x0005001010040000L || // Wii U Menu JPN
        OSGetTitleID() == 0x0005001010040100L || // Wii U Menu USA
        OSGetTitleID() == 0x0005001010040200L) { // Wii U Menu EUR
        gInWiiUMenu = true;

        if (SDUtils_Init() >= 0) {
            DEBUG_FUNCTION_LINE("SDUtils_Init done");
            sSDUtilsInitDone = true;
            sTitleRebooting  = false;
            if (SDUtils_AddAttachHandler(SDAttachedHandler) != SDUTILS_RESULT_SUCCESS) {
                DEBUG_FUNCTION_LINE_ERR("Failed to add AttachedHandler");
            }
            if (SDUtils_AddCleanUpHandlesHandler(SDCleanUpHandlesHandler) != SDUTILS_RESULT_SUCCESS) {
                DEBUG_FUNCTION_LINE_ERR("Failed to add CleanUpHandlesHandler");
            }
            if (SDUtils_IsSdCardMounted(&sSDIsMounted) != SDUTILS_RESULT_SUCCESS) {
                DEBUG_FUNCTION_LINE_ERR("IsSdCardMounted failed");
            }
        } else {
            DEBUG_FUNCTION_LINE_ERR("Failed to init SDUtils. Make sure to have the SDHotSwapModule loaded!");
        }
    } else {
        gInWiiUMenu = false;
    }

    if (_SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY) != OSGetTitleID()) {
        gHomebrewLaunched = FALSE;
    }
}

ON_APPLICATION_ENDS() {
    Cleanup();
    SaveRedirectionCleanUp();
    deinitLogging();
    gInWiiUMenu = false;
    if (sSDUtilsInitDone) {
        SDUtils_RemoveAttachHandler(SDAttachedHandler);
        SDUtils_RemoveCleanUpHandlesHandler(SDCleanUpHandlesHandler);
        SDUtils_DeInit();
        sSDUtilsInitDone = false;
    }
    sSDIsMounted = false;
}

std::optional<std::shared_ptr<FileInfos>> getIDByLowerTitleID(uint32_t titleid_lower) {
    std::lock_guard<std::mutex> lock(fileInfosMutex);
    for (auto &cur : fileInfos) {
        if (cur->lowerTitleID == titleid_lower) {
            return cur;
        }
    }
    return {};
}

void fillXmlForTitleID(uint32_t titleid_upper, uint32_t titleid_lower, ACPMetaXml *out_buf) {
    auto titleIdInfoOpt = getIDByLowerTitleID(titleid_lower);
    if (!titleIdInfoOpt.has_value()) {
        DEBUG_FUNCTION_LINE_ERR("Failed to get info by titleid");
        return;
    }
    auto &titleInfo = titleIdInfoOpt.value();

    out_buf->title_id = (((uint64_t) titleid_upper) << 32) + titleid_lower;
    strncpy(out_buf->longname_en, titleInfo->longname.c_str(), sizeof(out_buf->longname_en) - 1);
    strncpy(out_buf->shortname_en, titleInfo->shortname.c_str(), sizeof(out_buf->shortname_en) - 1);
    strncpy(out_buf->publisher_en, titleInfo->author.c_str(), sizeof(out_buf->publisher_en) - 1);
    out_buf->e_manual           = 1;
    out_buf->e_manual_version   = 0;
    out_buf->title_version      = 1;
    out_buf->network_use        = 1;
    out_buf->launching_flag     = 4;
    out_buf->online_account_use = 1;
    out_buf->os_version         = 0x000500101000400A;
    out_buf->region             = 0xFFFFFFFF;
    out_buf->common_save_size   = 0x0000000001790000;
    out_buf->group_id           = 0x400;
    out_buf->drc_use            = 1;
    out_buf->version            = 1;
    out_buf->reserved_flag0     = 0x00010001;
    out_buf->reserved_flag6     = 0x00000003;
    out_buf->pc_usk             = 128;
    strncpy(out_buf->product_code, "WUP-P-HBLD", sizeof(out_buf->product_code) - 1);
    strncpy(out_buf->content_platform, "WUP", sizeof(out_buf->content_platform) - 1);
    strncpy(out_buf->company_code, "0001", sizeof(out_buf->company_code) - 1);
}

static int handler(void *user, const char *section, const char *name,
                   const char *value) {
    auto *fInfo = (std::shared_ptr<FileInfos> *) user;
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("menu", "longname")) {
        fInfo->operator->()->longname = value;
    } else if (MATCH("menu", "shortname")) {
        fInfo->operator->()->shortname = value;
    } else if (MATCH("menu", "author")) {
        fInfo->operator->()->author = value;
    } else {
        return 0; /* unknown section/name, error */
    }

    return 1;
}

bool CheckFileExistsHelper(const char *path);
void readCustomTitlesFromSD() {
    std::lock_guard<std::mutex> lock(fileInfosMutex);
    if (!fileInfos.empty()) {
        DEBUG_FUNCTION_LINE_VERBOSE("Using cached value");
        return;
    }
    // Reset current infos
    DirList dirList("fs:/vol/external01/wiiu/apps", ".rpx,.wuhb", DirList::Files | DirList::CheckSubfolders, 1);
    dirList.SortList();

    for (int i = 0; i < dirList.GetFilecount(); i++) {
        //! skip wiiload temp files
        if (strcasecmp(dirList.GetFilename(i), "temp.rpx") == 0) {
            continue;
        }

        //! skip wiiload temp files
        if (strcasecmp(dirList.GetFilename(i), "temp.wuhb") == 0) {
            continue;
        }

        //! skip wiiload temp files
        if (strcasecmp(dirList.GetFilename(i), "temp2.wuhb") == 0) {
            continue;
        }

        //! skip hidden linux and mac files
        if (dirList.GetFilename(i)[0] == '.' || dirList.GetFilename(i)[0] == '_') {
            continue;
        }


        auto repl  = "fs:/vol/external01/";
        auto input = dirList.GetFilepath(i);
        const char *relativeFilepath;

        if (std::string_view(input).starts_with(repl)) {
            relativeFilepath = &input[strlen(repl)];
        } else {
            DEBUG_FUNCTION_LINE_ERR("Skip %s, Path doesn't start with %s (This should never happen", input, repl);
            continue;
        }

        auto fileInfo = make_shared_nothrow<FileInfos>(relativeFilepath);
        if (!fileInfo) {
            DEBUG_FUNCTION_LINE_ERR("No more memory");
            break;
        }

        std::lock_guard<std::mutex> infoLock(fileInfo->accessLock);

        auto *cur_title_info = &(fileInfo->titleInfo);

        snprintf(cur_title_info->path, sizeof(cur_title_info->path), "/custom/%08X%08X", UPPER_TITLE_ID_HOMEBREW, fileInfo->lowerTitleID);

        const char *indexedDevice = "mlc";
        strncpy(cur_title_info->indexedDevice, indexedDevice, sizeof(cur_title_info->indexedDevice) - 1);

        fileInfo->filename  = dirList.GetFilename(i);
        fileInfo->longname  = dirList.GetFilename(i);
        fileInfo->shortname = dirList.GetFilename(i);
        fileInfo->author    = dirList.GetFilename(i);

        // System apps don't have a splash screen.
        cur_title_info->appType = MCP_APP_TYPE_SYSTEM_APPS;

        DEBUG_FUNCTION_LINE_VERBOSE("Check %s", fileInfo->filename.c_str());

        // Check if the bootTvTex and bootDrcTex exists
        if (std::string_view(fileInfo->filename).ends_with(".wuhb")) {
            int result = 0;

#define TMP_BUNDLE_NAME "romfscheck"

            if (WUHBUtils_MountBundle(TMP_BUNDLE_NAME, dirList.GetFilepath(i), BundleSource_FileDescriptor, &result) == WUHB_UTILS_RESULT_SUCCESS && result >= 0) {
                fileInfo->isBundle = true;
                uint8_t *buffer;
                uint32_t bufferSize;

                auto readRes = WUHBUtils_ReadWholeFile(TMP_BUNDLE_NAME ":/meta/meta.ini", &buffer, &bufferSize);
                if (readRes == WUHB_UTILS_RESULT_SUCCESS) {
                    buffer[bufferSize - 1] = '\0';
                    if (ini_parse_string((const char *) buffer, handler, &fileInfo) < 0) {
                        DEBUG_FUNCTION_LINE_ERR("Failed to parse meta.ini");
                    }
                    free(buffer);
                    buffer = nullptr;
                } else {
                    DEBUG_FUNCTION_LINE_ERR("Failed to open or read meta.ini: %d", readRes);
                }

                auto bootTvTexPath  = TMP_BUNDLE_NAME ":/meta/bootTvTex.tga";
                auto bootDrcTexPath = TMP_BUNDLE_NAME ":/meta/bootDrcTex.tga";
                if (CheckFileExistsHelper(bootTvTexPath) && CheckFileExistsHelper(bootDrcTexPath)) {
                    // Show splash screens
                    cur_title_info->appType = MCP_APP_TYPE_GAME;
                    DEBUG_FUNCTION_LINE_VERBOSE("Title has splashscreen");
                }

                int32_t unmountRes;
                if (WUHBUtils_UnmountBundle(TMP_BUNDLE_NAME, &unmountRes) == WUHB_UTILS_RESULT_SUCCESS) {
                    if (unmountRes != 0) {
                        DEBUG_FUNCTION_LINE_ERR("Unmount result was \"%s\"", TMP_BUNDLE_NAME);
                    }
                } else {
                    DEBUG_FUNCTION_LINE_ERR("Failed to unmount \"%s\"", TMP_BUNDLE_NAME);
                }
            } else {
                DEBUG_FUNCTION_LINE_ERR("%s is not a valid .wuhb file: %d", dirList.GetFilepath(i), result);
                continue;
            }
        }

        cur_title_info->titleId      = TITLE_ID_HOMEBREW_MASK | fileInfo->lowerTitleID;
        cur_title_info->titleVersion = 1;
        cur_title_info->groupId      = 0x400;

        cur_title_info->osVersion  = OSGetOSID();
        cur_title_info->sdkVersion = __OSGetProcessSDKVersion();
        cur_title_info->unk0x60    = 0;

        fileInfos.push_front(fileInfo);
    }
}

bool CheckFileExistsHelper(const char *path) {
    int32_t exists;
    int32_t res;
    if ((res = WUHBUtils_FileExists(path, &exists)) == WUHB_UTILS_RESULT_SUCCESS) {
        if (!exists) {
            DEBUG_FUNCTION_LINE_VERBOSE("## WARN ##: Missing %s", path);
            return false;
        }
        return true;
    }
    DEBUG_FUNCTION_LINE_ERR("Failed to check if %s exists: %d", path, res);

    return false;
}

DECL_FUNCTION(int32_t, MCP_TitleList, uint32_t handle, uint32_t *outTitleCount, MCPTitleListType *titleList, uint32_t size) {
    int32_t result = real_MCP_TitleList(handle, outTitleCount, titleList, size);

    if (!gInWiiUMenu) {
        DEBUG_FUNCTION_LINE_VERBOSE("Not in Wii U Menu");
        return result;
    }

    uint32_t titleCount = *outTitleCount;

    std::lock_guard<std::mutex> lock(fileInfosMutex);
    readCustomTitlesFromSD();

    for (auto &gFileInfo : fileInfos) {
        memcpy(&(titleList[titleCount]), &(gFileInfo->titleInfo), sizeof(MCPTitleListType));
        titleCount++;
    }

    *outTitleCount = titleCount;

    return result;
}

DECL_FUNCTION(int32_t, ACPCheckTitleLaunchByTitleListTypeEx, MCPTitleListType *title, uint32_t u2) {
    if ((title->titleId & TITLE_ID_HOMEBREW_MASK) == TITLE_ID_HOMEBREW_MASK) {
        std::lock_guard<std::mutex> lock(fileInfosMutex);
        auto fileInfo = getIDByLowerTitleID(title->titleId & 0xFFFFFFFF);
        if (fileInfo.has_value()) {
            DEBUG_FUNCTION_LINE("Starting a homebrew title");

            fillXmlForTitleID((title->titleId & 0xFFFFFFFF00000000) >> 32, (title->titleId & 0xFFFFFFFF), &gLaunchXML);

            gHomebrewLaunched = TRUE;

            if (RPXLoader_PrepareLaunchFromSD(fileInfo.value()->relativeFilepath.c_str()) == RPX_LOADER_RESULT_SUCCESS) {
                return 0;
            }

            DEBUG_FUNCTION_LINE_ERR("Failed to prepare launch for %s", fileInfo.value()->relativeFilepath.c_str());
        } else {
            DEBUG_FUNCTION_LINE_ERR("Failed to get info for titleID %016llX", title->titleId);
        }
    }

    return real_ACPCheckTitleLaunchByTitleListTypeEx(title, u2);
}


DECL_FUNCTION(int, FSOpenFile, FSClient *client, FSCmdBlock *block, char *path, const char *mode, uint32_t *handle, int error) {
    const char *start   = "/vol/storage_mlc01/sys/title/0005000F";
    const char *tga     = ".tga";
    const char *iconTex = "iconTex.tga";
    const char *sound   = ".btsnd";

    std::string_view pathStr = path;

    if (pathStr.starts_with(start)) {
        std::unique_ptr<FileReader> reader;
        if (pathStr.ends_with(tga) || pathStr.ends_with(sound)) {
            char *id           = path + 1 + strlen(start);
            id[8]              = 0;
            char *relativePath = id + 9;
            auto lowerTitleID  = strtoul(id, 0, 16);
            auto fileInfo      = getIDByLowerTitleID(lowerTitleID);
            if (fileInfo.has_value()) {
                reader = make_unique_nothrow<FileReaderWUHB>(fileInfo.value(), relativePath, !gHomebrewLaunched);
                if (reader && !reader->isReady()) {
                    reader.reset();
                }
            }
        }
        // If the icon is requested and loading it from a bundle failed, we fall back to a default one.
        if (reader == nullptr && pathStr.ends_with(iconTex)) {
            reader = make_unique_nothrow<FileReader>((uint8_t *) iconTex_tga, iconTex_tga_size);
            if (reader && !reader->isReady()) {
                reader.reset();
            }
        }
        if (reader) {
            std::lock_guard<std::mutex> lock(fileReaderListMutex);
            *handle = reader->getHandle();
            openFileReaders.push_front(std::move(reader));
            return FS_STATUS_OK;
        }
    }

    int result = real_FSOpenFile(client, block, path, mode, handle, error);
    return result;
}

DECL_FUNCTION(FSStatus, FSCloseFile, FSClient *client, FSCmdBlock *block, FSFileHandle handle, uint32_t flags) {
    if (remove_locked_first_if(fileReaderListMutex, openFileReaders, [handle](auto &cur) { return cur->getHandle() == handle; })) {
        return FS_STATUS_OK;
    }

    return real_FSCloseFile(client, block, handle, flags);
}

DECL_FUNCTION(FSStatus, FSReadFile, FSClient *client, FSCmdBlock *block, uint8_t *buffer, uint32_t size, uint32_t count, FSFileHandle handle, uint32_t unk1, uint32_t flags) {
    const std::lock_guard<std::mutex> lock(fileReaderListMutex);
    for (auto &reader : openFileReaders) {
        if ((uint32_t) reader.get() == (uint32_t) handle) {
            return (FSStatus) (reader->read(buffer, size * count) / size);
        }
    }
    FSStatus result = real_FSReadFile(client, block, buffer, size, count, handle, unk1, flags);
    return result;
}

DECL_FUNCTION(int32_t, ACPGetTitleMetaXmlByDevice, uint32_t titleid_upper, uint32_t titleid_lower, ACPMetaXml *out_buf, uint32_t device, uint32_t u1) {
    int result = real_ACPGetTitleMetaXmlByDevice(titleid_upper, titleid_lower, out_buf, device, u1);
    if (titleid_upper == UPPER_TITLE_ID_HOMEBREW) {
        fillXmlForTitleID(titleid_upper, titleid_lower, out_buf);
        result = 0;
    }
    return result;
}

DECL_FUNCTION(int32_t, ACPGetTitleMetaDirByDevice, uint32_t titleid_upper, uint32_t titleid_lower, char *out_buf, uint32_t size, int device) {
    if (titleid_upper == UPPER_TITLE_ID_HOMEBREW) {
        snprintf(out_buf, 53, "/vol/storage_mlc01/sys/title/%08X/%08X/meta", titleid_upper, titleid_lower);
        return 0;
    }
    int result = real_ACPGetTitleMetaDirByDevice(titleid_upper, titleid_lower, out_buf, size, device);
    return result;
}

/*
 * Load the H&S app instead
 */
DECL_FUNCTION(int32_t, _SYSLaunchTitleByPathFromLauncher, char *pathToLoad, uint32_t u2) {
    const char *start = "/custom/";
    if (strncmp(pathToLoad, start, strlen(start)) == 0) {
        strcpy(current_launched_title_info.path, pathToLoad);
        uint64_t titleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);
        snprintf(pathToLoad, 47, "/vol/storage_mlc01/sys/title/%08x/%08x", (uint32_t) (titleID >> 32), (uint32_t) (0x00000000FFFFFFFF & titleID));
    }

    int32_t result = real__SYSLaunchTitleByPathFromLauncher(pathToLoad, strlen(pathToLoad));
    return result;
}

DECL_FUNCTION(int32_t, ACPGetLaunchMetaXml, ACPMetaXml *metaxml) {
    int result = real_ACPGetLaunchMetaXml(metaxml);
    if (gHomebrewLaunched) {
        memcpy(metaxml, &gLaunchXML, sizeof(gLaunchXML));
    }
    return result;
}

DECL_FUNCTION(uint32_t, ACPGetApplicationBox, uint32_t *u1, uint32_t *u2, uint32_t u3, uint32_t u4) {
    if (u3 == UPPER_TITLE_ID_HOMEBREW) {
        uint64_t titleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);
        u3               = (uint32_t) (titleID >> 32);
        u4               = (uint32_t) (0x00000000FFFFFFFF & titleID);
    }
    uint32_t result = real_ACPGetApplicationBox(u1, u2, u3, u4);
    return result;
}

/*
 * Redirect the launchable check to H&S
 */
DECL_FUNCTION(uint32_t, PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg, uint32_t *param) {
    if (param[2] == UPPER_TITLE_ID_HOMEBREW) {
        uint64_t titleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);
        param[2]         = (uint32_t) (titleID >> 32);
        param[3]         = (uint32_t) (0x00000000FFFFFFFF & titleID);
    }
    uint32_t result = real_PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg(param);
    return result;
}

/*
 * Redirect the launchable check to H&S
 */
DECL_FUNCTION(uint32_t, MCP_RightCheckLaunchable, uint32_t *u1, uint32_t *u2, uint32_t u3, uint32_t u4, uint32_t u5) {
    if (u3 == UPPER_TITLE_ID_HOMEBREW) {
        uint64_t titleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);
        u3               = (uint32_t) (titleID >> 32);
        u4               = (uint32_t) (0x00000000FFFFFFFF & titleID);
    }
    uint32_t result = real_MCP_RightCheckLaunchable(u1, u2, u3, u4, u5);
    return result;
}

/*
 * Patch the boot movie and and boot logo
 */
DECL_FUNCTION(uint32_t, ACPGetLaunchMetaData, struct ACPMetaData *metadata) {
    uint32_t result = real_ACPGetLaunchMetaData(metadata);

    if (gHomebrewLaunched) {
        memcpy(metadata->bootmovie, bootMovie_h264, bootMovie_h264_size);
        memcpy(metadata->bootlogo, bootLogoTex_tga, bootLogoTex_tga_size);
        DCFlushRange(metadata->bootmovie, bootMovie_h264_size);
        DCFlushRange(metadata->bootlogo, bootMovie_h264_size);
    }

    return result;
}

typedef struct TitleVersionInfo TitleVersionInfo;

struct WUT_PACKED TitleVersionInfo {
    uint16_t currentVersion;
    uint16_t neededVersion;
    uint8_t needsUpdate;
};
WUT_CHECK_OFFSET(TitleVersionInfo, 0x00, currentVersion);
WUT_CHECK_OFFSET(TitleVersionInfo, 0x02, neededVersion);
WUT_CHECK_OFFSET(TitleVersionInfo, 0x04, needsUpdate);
WUT_CHECK_SIZE(TitleVersionInfo, 0x05);


DECL_FUNCTION(uint32_t, GetTitleVersionInfo__Q2_2nn4vctlFPQ3_2nn4vctl16TitleVersionInfoULQ3_2nn4Cafe9MediaType, TitleVersionInfo *titleVersionInfo, uint32_t u2, uint32_t u3, uint32_t u4, uint32_t u5) {
    int32_t result = real_GetTitleVersionInfo__Q2_2nn4vctlFPQ3_2nn4vctl16TitleVersionInfoULQ3_2nn4Cafe9MediaType(titleVersionInfo, u2, u3, u4, u5);

    if (result < 0) {
        // Fake result if it's H&S
        uint64_t titleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);
        auto expected_u3 = (uint32_t) (titleID >> 32);
        auto expected_u4 = (uint32_t) (0x00000000FFFFFFFF & titleID);

        if (expected_u3 == u3 && expected_u4 == u4) {
            if (titleVersionInfo != nullptr) {
                titleVersionInfo->currentVersion = 129;
                titleVersionInfo->neededVersion  = 129;
                titleVersionInfo->needsUpdate    = 0;
            }
            return 0;
        }
    }

    return result;
}

DECL_FUNCTION(uint32_t, GetUpdateInfo__Q2_2nn4vctlFPQ3_2nn4vctl10UpdateInfoULQ3_2nn4Cafe9MediaType, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t u4, uint32_t u5, uint32_t u6) {
    uint32_t result = real_GetUpdateInfo__Q2_2nn4vctlFPQ3_2nn4vctl10UpdateInfoULQ3_2nn4Cafe9MediaType(u1, u2, u3, u4, u5, u6);

    uint64_t titleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);
    auto expected_u3 = (uint32_t) (titleID >> 32);
    auto expected_u4 = (uint32_t) (0x00000000FFFFFFFF & titleID);

    if (expected_u3 == u3 && expected_u4 == u4) {
        return 0xa121f480;
    }

    return result;
}

DECL_FUNCTION(uint32_t, MCPGetTitleInternal, uint32_t mcp_handle, void *input, uint32_t type, MCPTitleListType *titles, uint32_t out_cnt) {
    if (input != nullptr) {
        auto *inputPtrAsU32 = (uint32_t *) input;
        if (inputPtrAsU32[0] == UPPER_TITLE_ID_HOMEBREW && out_cnt >= 1) {
            for (auto &gFileInfo : fileInfos) {
                if (gFileInfo->lowerTitleID == inputPtrAsU32[1]) {
                    memcpy(&titles[0], &(gFileInfo->titleInfo), sizeof(MCPTitleListType));
                    return 1;
                }
            }
            DEBUG_FUNCTION_LINE_ERR("Failed to find lower TID %08X", inputPtrAsU32[1]);
        }
    }

    uint32_t result = real_MCPGetTitleInternal(mcp_handle, input, type, titles, out_cnt);

    return result;
}

WUPS_MUST_REPLACE(GetTitleVersionInfo__Q2_2nn4vctlFPQ3_2nn4vctl16TitleVersionInfoULQ3_2nn4Cafe9MediaType, WUPS_LOADER_LIBRARY_NN_VCTL, GetTitleVersionInfo__Q2_2nn4vctlFPQ3_2nn4vctl16TitleVersionInfoULQ3_2nn4Cafe9MediaType);
WUPS_MUST_REPLACE(GetUpdateInfo__Q2_2nn4vctlFPQ3_2nn4vctl10UpdateInfoULQ3_2nn4Cafe9MediaType, WUPS_LOADER_LIBRARY_NN_VCTL, GetUpdateInfo__Q2_2nn4vctlFPQ3_2nn4vctl10UpdateInfoULQ3_2nn4Cafe9MediaType);
WUPS_MUST_REPLACE(ACPGetApplicationBox, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetApplicationBox);
WUPS_MUST_REPLACE(PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg, WUPS_LOADER_LIBRARY_DRMAPP, PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg);
WUPS_MUST_REPLACE(MCP_RightCheckLaunchable, WUPS_LOADER_LIBRARY_COREINIT, MCP_RightCheckLaunchable);

WUPS_MUST_REPLACE(MCP_TitleList, WUPS_LOADER_LIBRARY_COREINIT, MCP_TitleList);

WUPS_MUST_REPLACE(ACPCheckTitleLaunchByTitleListTypeEx, WUPS_LOADER_LIBRARY_NN_ACP, ACPCheckTitleLaunchByTitleListTypeEx);
WUPS_MUST_REPLACE(ACPGetTitleMetaXmlByDevice, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetTitleMetaXmlByDevice);
WUPS_MUST_REPLACE(ACPGetLaunchMetaXml, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetLaunchMetaXml);
WUPS_MUST_REPLACE(ACPGetTitleMetaDirByDevice, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetTitleMetaDirByDevice);
WUPS_MUST_REPLACE(_SYSLaunchTitleByPathFromLauncher, WUPS_LOADER_LIBRARY_SYSAPP, _SYSLaunchTitleByPathFromLauncher);
WUPS_MUST_REPLACE(ACPGetLaunchMetaData, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetLaunchMetaData);

WUPS_MUST_REPLACE(FSReadFile, WUPS_LOADER_LIBRARY_COREINIT, FSReadFile);
WUPS_MUST_REPLACE(FSOpenFile, WUPS_LOADER_LIBRARY_COREINIT, FSOpenFile);
WUPS_MUST_REPLACE(FSCloseFile, WUPS_LOADER_LIBRARY_COREINIT, FSCloseFile);

WUPS_MUST_REPLACE_PHYSICAL(MCPGetTitleInternal, (0x3001C400 + 0x0205a590), (0x0205a590 - 0xFE3C00));
