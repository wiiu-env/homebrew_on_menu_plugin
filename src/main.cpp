#include "main.h"
#include "FileInfos.h"
#include "SaveRedirection.h"
#include "bootLogoTex_tga.h"
#include "bootMovie_h264.h"
#include "fs/CFile.hpp"
#include "fs/FileReader.h"
#include "fs/FileReaderWUHB.h"
#include "globals.h"
#include "iconTex_tga.h"
#include "utils/StringTools.h"
#include "utils/ini.h"
#include <algorithm>
#include <content_redirection/redirection.h>
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/filesystem.h>
#include <coreinit/mcp.h>
#include <coreinit/memory.h>
#include <coreinit/systeminfo.h>
#include <coreinit/title.h>
#include <cstring>
#include <fnmatch.h>
#include <forward_list>
#include <fs/DirList.h>
#include <malloc.h>
#include <map>
#include <nn/acp.h>
#include <notifications/notifications.h>
#include <optional>
#include <rpxloader/rpxloader.h>
#include <sdutils/sdutils.h>
#include <sysapp/launch.h>
#include <sysapp/title.h>
#include <utils/logger.h>
#include <wuhb_utils/utils.h>
#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/storage.h>

typedef struct ACPMetaData {
    char bootmovie[80696];
    char bootlogo[28604];
} ACPMetaData;

WUPS_PLUGIN_NAME("Homebrew in Wii U menu");
WUPS_PLUGIN_DESCRIPTION("Allows the user to load homebrew from the Wii U menu");
WUPS_PLUGIN_VERSION(VERSION_FULL);
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
WUPS_USE_STORAGE("homebrew_on_menu"); // Use the storage API

#define HIDE_HOMEBREW_STRING                 "hideHomebrew"
#define PREFER_WUHB_OVER_RPX_STRING          "hideRPXIfWUHBExists"
#define HIDE_ALL_RPX_STRING                  "hideAllRPX"

#define HOMEBREW_APPS_DIRECTORY              "fs:/vol/external01/wiiu/apps"
#define IGNORE_FILE_PATH                     HOMEBREW_APPS_DIRECTORY "/.ignore"
#define HOMEBREW_LAUNCHER_FILENAME           "homebrew_launcher.wuhb"
#define HOMEBREW_LAUNCHER_OPTIONAL_DIRECTORY "homebrew_launcher"

#define HOMEBREW_LAUNCHER_PATH               HOMEBREW_APPS_DIRECTORY "/" HOMEBREW_LAUNCHER_FILENAME
#define HOMEBREW_LAUNCHER_PATH2              HOMEBREW_APPS_DIRECTORY "/" HOMEBREW_LAUNCHER_OPTIONAL_DIRECTORY "/" HOMEBREW_LAUNCHER_FILENAME

bool gHideHomebrew              = false;
bool gPreferWUHBOverRPX         = true;
bool gHideAllRPX                = false;
bool prevHideValue              = false;
bool prevPreferWUHBOverRPXValue = false;
bool prevHideAllRPX             = false;

bool gHomebrewLauncherExists = false;

std::vector<std::string> gIgnorePatterns;

INITIALIZE_PLUGIN() {
    memset((void *) &current_launched_title_info, 0, sizeof(current_launched_title_info));
    memset((void *) &gLaunchXML, 0, sizeof(gLaunchXML));
    gHomebrewLaunched = FALSE;

    gSerialId = {};
    if (!Utils::GetSerialId(gSerialId) || gSerialId.empty()) {
        DEBUG_FUNCTION_LINE_ERR("Homebrew on Menu Plugin: Failed to get the serial id");
        OSFatal("Homebrew on Menu Plugin: Failed to get the serial id");
    }

    // Use libwuhbutils.
    WUHBUtilsStatus error;
    if ((error = WUHBUtils_InitLibrary()) != WUHB_UTILS_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Homebrew on Menu Plugin: Failed to init WUHBUtils. Error %s [%d]", WUHBUtils_GetStatusStr(error), error);
        OSFatal("Homebrew on Menu Plugin: Failed to init WUHBUtils.");
    }

    // Use libcontentredirection.
    ContentRedirectionStatus error2;
    if ((error2 = ContentRedirection_InitLibrary()) != CONTENT_REDIRECTION_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Homebrew on Menu Plugin: Failed to init ContentRedirection. Error %s [%d]", ContentRedirection_GetStatusStr(error2), error2);
        OSFatal("Homebrew on Menu Plugin: Failed to init ContentRedirection.");
    }

    // Use librpxloader.
    RPXLoaderStatus error3;
    if ((error3 = RPXLoader_InitLibrary()) != RPX_LOADER_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Homebrew on Menu Plugin: Failed to init RPXLoader. Error %s [%d]", RPXLoader_GetStatusStr(error3), error3);
        OSFatal("Homebrew on Menu Plugin: Failed to init RPXLoader.");
    }

    // Use libnotifications.
    NotificationModuleStatus error4;
    if ((error4 = NotificationModule_InitLibrary()) != NOTIFICATION_MODULE_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Homebrew on Menu Plugin: Failed to init NotificationModule. Error %s [%d]", NotificationModule_GetStatusStr(error4), error4);
        OSFatal("Homebrew on Menu Plugin: Failed to init NotificationModule.");
    }

    // Open storage to read values
    WUPSStorageError storageRes = WUPS_OpenStorage();
    if (storageRes != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to open storage %s (%d)", WUPS_GetStorageStatusStr(storageRes), storageRes);
    } else {
        // Try to get value from storage
        if ((storageRes = WUPS_GetBool(nullptr, HIDE_HOMEBREW_STRING, &gHideHomebrew)) == WUPS_STORAGE_ERROR_NOT_FOUND) {
            // Add the value to the storage if it's missing.
            storageRes = WUPS_StoreBool(nullptr, HIDE_HOMEBREW_STRING, gHideHomebrew);
            if (storageRes != WUPS_STORAGE_ERROR_SUCCESS) {
                DEBUG_FUNCTION_LINE_ERR("Failed to store bool %s (%d)", WUPS_GetStorageStatusStr(storageRes), storageRes);
            }
        } else {
            if (storageRes != WUPS_STORAGE_ERROR_SUCCESS) {
                DEBUG_FUNCTION_LINE_ERR("Failed to get bool %s (%d)", WUPS_GetStorageStatusStr(storageRes), storageRes);
            }
        }

        if ((storageRes = WUPS_GetBool(nullptr, PREFER_WUHB_OVER_RPX_STRING, &gPreferWUHBOverRPX)) == WUPS_STORAGE_ERROR_NOT_FOUND) {
            // Add the value to the storage if it's missing.
            storageRes = WUPS_StoreBool(nullptr, PREFER_WUHB_OVER_RPX_STRING, gPreferWUHBOverRPX);
            if (storageRes != WUPS_STORAGE_ERROR_SUCCESS) {
                DEBUG_FUNCTION_LINE_ERR("Failed to store bool %s (%d)", WUPS_GetStorageStatusStr(storageRes), storageRes);
            }
        } else {
            if (storageRes != WUPS_STORAGE_ERROR_SUCCESS) {
                DEBUG_FUNCTION_LINE_ERR("Failed to get bool %s (%d)", WUPS_GetStorageStatusStr(storageRes), storageRes);
            }
        }

        if ((storageRes = WUPS_GetBool(nullptr, HIDE_ALL_RPX_STRING, &gHideAllRPX)) == WUPS_STORAGE_ERROR_NOT_FOUND) {
            // Add the value to the storage if it's missing.
            storageRes = WUPS_StoreBool(nullptr, HIDE_ALL_RPX_STRING, gHideAllRPX);
            if (storageRes != WUPS_STORAGE_ERROR_SUCCESS) {
                DEBUG_FUNCTION_LINE_ERR("Failed to store bool %s (%d)", WUPS_GetStorageStatusStr(storageRes), storageRes);
            }
        } else {
            if (storageRes != WUPS_STORAGE_ERROR_SUCCESS) {
                DEBUG_FUNCTION_LINE_ERR("Failed to get bool %s (%d)", WUPS_GetStorageStatusStr(storageRes), storageRes);
            }
        }

        prevHideValue              = gHideHomebrew;
        prevPreferWUHBOverRPXValue = gPreferWUHBOverRPX;
        prevHideAllRPX             = gHideAllRPX;

        // Close storage
        WUPS_CloseStorage();
    }
}

void hideHomebrewChanged(ConfigItemBoolean *item, bool newValue) {
    DEBUG_FUNCTION_LINE_VERBOSE("New value in gHideHomebrew: %d", newValue);
    gHideHomebrew = newValue;

    // If the value has changed, we store it in the storage.
    WUPSStorageError storageRes = WUPS_StoreBool(nullptr, HIDE_HOMEBREW_STRING, gHideHomebrew);
    if (storageRes != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to store bool: %s (%d)", WUPS_GetStorageStatusStr(storageRes), storageRes);
    }
}

void preferWUHBOverRPXChanged(ConfigItemBoolean *item, bool newValue) {
    DEBUG_FUNCTION_LINE_VERBOSE("New value in gPreferWUHBOverRPX: %d", newValue);
    gPreferWUHBOverRPX = newValue;

    // If the value has changed, we store it in the storage.
    WUPSStorageError storageRes = WUPS_StoreBool(nullptr, PREFER_WUHB_OVER_RPX_STRING, gPreferWUHBOverRPX);
    if (storageRes != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to store bool: %s (%d)", WUPS_GetStorageStatusStr(storageRes), storageRes);
    }
}

void hideAllRPXChanged(ConfigItemBoolean *item, bool newValue) {
    DEBUG_FUNCTION_LINE_VERBOSE("New value in gHideAllRPX: %d", newValue);
    gHideAllRPX = newValue;

    // If the value has changed, we store it in the storage.
    WUPSStorageError storageRes = WUPS_StoreBool(nullptr, HIDE_ALL_RPX_STRING, gHideAllRPX);
    if (storageRes != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to store bool: %s (%d)", WUPS_GetStorageStatusStr(storageRes), storageRes);
    }
}

WUPS_GET_CONFIG() {
    // We open the storage so we can persist the configuration the user did.
    WUPSStorageError storageRes;
    DEBUG_FUNCTION_LINE_ERR("In WUPS_GET_CONFIG");
    if ((storageRes = WUPS_OpenStorage()) != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to open storage %s (%d)", WUPS_GetStorageStatusStr(storageRes), storageRes);
        return 0;
    }

    WUPSConfigHandle config;
    WUPSConfig_CreateHandled(&config, "Homebrew on Menu");

    WUPSConfigCategoryHandle cat;
    WUPSConfig_AddCategoryByNameHandled(config, "Features", &cat);

    WUPSConfigItemBoolean_AddToCategoryHandled(config, cat, HIDE_HOMEBREW_STRING,
                                               gHomebrewLauncherExists ? "Hide all homebrew except Homebrew Launcher" : "Hide all homebrew",
                                               gHideHomebrew, &hideHomebrewChanged);

    WUPSConfigItemBoolean_AddToCategoryHandled(config, cat, PREFER_WUHB_OVER_RPX_STRING, "Prefer .wuhb over .rpx", gPreferWUHBOverRPX, &preferWUHBOverRPXChanged);
    WUPSConfigItemBoolean_AddToCategoryHandled(config, cat, HIDE_ALL_RPX_STRING, "Hide all .rpx", gHideAllRPX, &hideAllRPXChanged);

    return config;
}

bool sSDUtilsInitDone = false;
bool sSDIsMounted     = false;
bool sTitleRebooting  = false;

WUPS_CONFIG_CLOSED() {
    // Save all changes
    WUPSStorageError storageRes;
    if ((storageRes = WUPS_CloseStorage()) != WUPS_STORAGE_ERROR_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to close storage %s (%d)", WUPS_GetStorageStatusStr(storageRes), storageRes);
    }

    if (prevHideValue != gHideHomebrew || prevPreferWUHBOverRPXValue != gPreferWUHBOverRPX || prevHideAllRPX != gHideAllRPX) {
        if (!sTitleRebooting) {
            _SYSLaunchTitleWithStdArgsInNoSplash(OSGetTitleID(), nullptr);
            sTitleRebooting = true;
        }
    }
    prevHideValue              = gHideHomebrew;
    prevPreferWUHBOverRPXValue = gPreferWUHBOverRPX;
    prevHideAllRPX             = gHideAllRPX;
}

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
    if (OSGetForegroundBucket(nullptr, nullptr) && !sTitleRebooting) {
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

        CFile file(IGNORE_FILE_PATH, CFile::ReadOnly);
        if (file.isOpen()) {
            std::string strBuffer;
            strBuffer.resize(file.size());
            file.read((uint8_t *) &strBuffer[0], strBuffer.size());
            file.close();

            //! remove all windows crap signs
            size_t position;
            while (true) {
                position = strBuffer.find('\r');
                if (position == std::string::npos) {
                    break;
                }

                strBuffer.erase(position, 1);
            }

            gIgnorePatterns = StringTools::StringSplit(strBuffer, "\n");

            // Ignore all lines that start with '#'
            gIgnorePatterns.erase(std::remove_if(gIgnorePatterns.begin(), gIgnorePatterns.end(), [](auto &line) { return line.starts_with('#'); }), gIgnorePatterns.end());
        } else {
            DEBUG_FUNCTION_LINE_ERR("No ignore found");
        }

        gInWiiUMenu = true;

        struct stat st {};
        if (stat(HOMEBREW_LAUNCHER_PATH, &st) >= 0 || stat(HOMEBREW_LAUNCHER_PATH2, &st) >= 0) {
            gHomebrewLauncherExists = true;
        } else {
            gHomebrewLauncherExists = false;
        }

        if (SDUtils_InitLibrary() == SDUTILS_RESULT_SUCCESS) {
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
            DEBUG_FUNCTION_LINE_WARN("Failed to init SDUtils. Make sure to have the SDHotSwapModule loaded!");
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
        SDUtils_DeInitLibrary();
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

    std::vector<std::string> listOfExecutables;

    if (gHideHomebrew) {
        struct stat st {};
        if (stat(HOMEBREW_LAUNCHER_PATH, &st) >= 0) {
            listOfExecutables.emplace_back(HOMEBREW_LAUNCHER_PATH);
        } else if (stat(HOMEBREW_LAUNCHER_PATH2, &st) >= 0) {
            listOfExecutables.emplace_back(HOMEBREW_LAUNCHER_PATH2);
        }
    } else {
        // Reset current infos
        DirList dirList(HOMEBREW_APPS_DIRECTORY, ".rpx,.wuhb", DirList::Files | DirList::CheckSubfolders, 1);
        dirList.SortList();

        if (gHideAllRPX) {
            for (int i = 0; i < dirList.GetFilecount(); i++) {
                if (dirList.GetFilepath(i) != nullptr && !std::string_view(dirList.GetFilepath(i)).ends_with(".rpx")) {
                    listOfExecutables.emplace_back(dirList.GetFilepath(i));
                }
            }
        } else if (gPreferWUHBOverRPX) {
            // map<[path without extension], vector<[extension]>>
            std::map<std::string, std::vector<std::string>> pathWithoutExtensionMap;
            for (int i = 0; i < dirList.GetFilecount(); i++) {
                std::string pathNoExtension = StringTools::remove_extension(dirList.GetFilepath(i));
                if (pathWithoutExtensionMap.count(pathNoExtension) == 0) {
                    pathWithoutExtensionMap[pathNoExtension] = std::vector<std::string>();
                }
                pathWithoutExtensionMap[pathNoExtension].push_back(StringTools::get_extension(dirList.GetFilename(i)));
            }

            for (auto &l : pathWithoutExtensionMap) {
                if (l.second.size() == 1 && l.second.at(0) == ".rpx") {
                    listOfExecutables.push_back(l.first + ".rpx");
                } else {
                    listOfExecutables.push_back(l.first + ".wuhb");
                }
            }
        } else {
            for (int i = 0; i < dirList.GetFilecount(); i++) {
                listOfExecutables.emplace_back(dirList.GetFilepath(i));
            }
        }

        // Remove any executable that matches the ignore pattern.
        listOfExecutables.erase(std::remove_if(listOfExecutables.begin(), listOfExecutables.end(), [&](const auto &item) {
                                    auto path = item.substr(strlen(HOMEBREW_APPS_DIRECTORY) + 1);
                                    return std::ranges::any_of(gIgnorePatterns.begin(), gIgnorePatterns.end(),
                                                               [&](const auto &pattern) {
                                                                   if (fnmatch(pattern.c_str(), path.c_str(), FNM_CASEFOLD) == 0) {
                                                                       DEBUG_FUNCTION_LINE_INFO("Ignore \"%s\" because it matched pattern \"%s\"", path.c_str(), pattern.c_str());
                                                                       return true;
                                                                   }
                                                                   return false;
                                                               });
                                }),
                                listOfExecutables.end());
    }

    for (auto &filePath : listOfExecutables) {
        auto filename = StringTools::FullpathToFilename(filePath.c_str());

        //! skip wiiload temp files
        if (strcasecmp(filename, "temp.rpx") == 0) {
            continue;
        }

        //! skip wiiload temp files
        if (strcasecmp(filename, "temp.wuhb") == 0) {
            continue;
        }

        //! skip wiiload temp files
        if (strcasecmp(filename, "temp2.wuhb") == 0) {
            continue;
        }

        //! skip hidden linux and mac files
        if (filename[0] == '.' || filename[0] == '_') {
            continue;
        }

        auto repl  = "fs:/vol/external01/";
        auto input = filePath.c_str();
        const char *relativeFilepath;

        if (filePath.starts_with(repl)) {
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

        fileInfo->filename  = filename;
        fileInfo->longname  = filename;
        fileInfo->shortname = filename;
        fileInfo->author    = filename;

        // System apps don't have a splash screen.
        cur_title_info->appType = MCP_APP_TYPE_SYSTEM_APPS;

        DEBUG_FUNCTION_LINE_VERBOSE("Check %s", fileInfo->filename.c_str());

        // Check if the bootTvTex and bootDrcTex exists
        if (std::string_view(fileInfo->filename).ends_with(".wuhb")) {
            int result = 0;

#define TMP_BUNDLE_NAME "romfscheck"

            if (WUHBUtils_MountBundle(TMP_BUNDLE_NAME, filePath.c_str(), BundleSource_FileDescriptor, &result) == WUHB_UTILS_RESULT_SUCCESS && result >= 0) {
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
                DEBUG_FUNCTION_LINE_ERR("%s is not a valid .wuhb file: %d", filePath.c_str(), result);
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
    OSMemoryBarrier();
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

    {
        std::lock_guard<std::mutex> lock(fileInfosMutex);
        readCustomTitlesFromSD();

        for (auto &gFileInfo : fileInfos) {
            memcpy(&(titleList[titleCount]), &(gFileInfo->titleInfo), sizeof(MCPTitleListType));
            titleCount++;
        }
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
    {
        const std::lock_guard<std::mutex> lock(fileReaderListMutex);
        for (auto &reader : openFileReaders) {
            if ((uint32_t) reader.get() == (uint32_t) handle) {
                return (FSStatus) (reader->read(buffer, size * count) / size);
            }
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

WUPS_MUST_REPLACE_FOR_PROCESS(GetTitleVersionInfo__Q2_2nn4vctlFPQ3_2nn4vctl16TitleVersionInfoULQ3_2nn4Cafe9MediaType, WUPS_LOADER_LIBRARY_NN_VCTL, GetTitleVersionInfo__Q2_2nn4vctlFPQ3_2nn4vctl16TitleVersionInfoULQ3_2nn4Cafe9MediaType, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(GetUpdateInfo__Q2_2nn4vctlFPQ3_2nn4vctl10UpdateInfoULQ3_2nn4Cafe9MediaType, WUPS_LOADER_LIBRARY_NN_VCTL, GetUpdateInfo__Q2_2nn4vctlFPQ3_2nn4vctl10UpdateInfoULQ3_2nn4Cafe9MediaType, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(ACPGetApplicationBox, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetApplicationBox, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg, WUPS_LOADER_LIBRARY_DRMAPP, PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(MCP_RightCheckLaunchable, WUPS_LOADER_LIBRARY_COREINIT, MCP_RightCheckLaunchable, WUPS_FP_TARGET_PROCESS_WII_U_MENU);

WUPS_MUST_REPLACE_FOR_PROCESS(MCP_TitleList, WUPS_LOADER_LIBRARY_COREINIT, MCP_TitleList, WUPS_FP_TARGET_PROCESS_WII_U_MENU);

WUPS_MUST_REPLACE_FOR_PROCESS(ACPCheckTitleLaunchByTitleListTypeEx, WUPS_LOADER_LIBRARY_NN_ACP, ACPCheckTitleLaunchByTitleListTypeEx, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(ACPGetTitleMetaXmlByDevice, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetTitleMetaXmlByDevice, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(ACPGetLaunchMetaXml, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetLaunchMetaXml, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(ACPGetTitleMetaDirByDevice, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetTitleMetaDirByDevice, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(_SYSLaunchTitleByPathFromLauncher, WUPS_LOADER_LIBRARY_SYSAPP, _SYSLaunchTitleByPathFromLauncher, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(ACPGetLaunchMetaData, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetLaunchMetaData, WUPS_FP_TARGET_PROCESS_WII_U_MENU);

WUPS_MUST_REPLACE_FOR_PROCESS(FSReadFile, WUPS_LOADER_LIBRARY_COREINIT, FSReadFile, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(FSOpenFile, WUPS_LOADER_LIBRARY_COREINIT, FSOpenFile, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(FSCloseFile, WUPS_LOADER_LIBRARY_COREINIT, FSCloseFile, WUPS_FP_TARGET_PROCESS_WII_U_MENU);

WUPS_MUST_REPLACE_PHYSICAL_FOR_PROCESS(MCPGetTitleInternal, (0x3001C400 + 0x0205a590), (0x0205a590 - 0xFE3C00), WUPS_FP_TARGET_PROCESS_WII_U_MENU);
