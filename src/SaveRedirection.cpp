#include "SaveRedirection.h"
#include "globals.h"
#include <content_redirection/redirection.h>
#include <coreinit/mcp.h>
#include <coreinit/title.h>
#include <fs/FSUtils.h>
#include <functional>
#include <nn/act.h>
#include <nn/save.h>
#include <notifications/notifications.h>
#include <string>
#include <utils/StringTools.h>
#include <utils/logger.h>
#include <wups.h>

bool gInWiiUMenu __attribute__((section(".data")))        = false;
CRLayerHandle saveLayer __attribute__((section(".data"))) = 0;

static inline std::string getBaseSavePathLegacy() {
    return std::string(HOMEBREW_ON_MENU_PLUGIN_DATA_PATH) + "/save";
}

static inline std::string getBaseSavePathLegacyFS() {
    return std::string("fs:") + getBaseSavePathLegacy();
}

static inline std::string getBaseSavePath() {
    return string_format(HOMEBREW_ON_MENU_PLUGIN_DATA_PATH "/%s/save", gSerialId.c_str());
}

static inline std::string getBaseSavePathFS() {
    return "fs:" + getBaseSavePath();
}

void SaveRedirectionCleanUp() {
    if (saveLayer != 0) {
        DEBUG_FUNCTION_LINE("Remove save redirection: %s -> %s", "/vol/save", getBaseSavePathFS().c_str());
        auto res = ContentRedirection_RemoveFSLayer(saveLayer);
        if (res != CONTENT_REDIRECTION_RESULT_SUCCESS) {
            DEBUG_FUNCTION_LINE_ERR("Failed to remove save FSLayer");
        }
        saveLayer = 0;
    }
}

void CopyExistingFiles() {
    nn::act::Initialize();
    nn::act::PersistentId persistentId = nn::act::GetPersistentId();
    nn::act::Finalize();

    std::string common         = getBaseSavePathFS() + "/common";
    std::string commonLegacy   = getBaseSavePathLegacyFS() + "/common";
    std::string commonOriginal = "fs:/vol/save/common";
    std::string user           = string_format("%s/%08X", getBaseSavePathFS().c_str(), 0x80000000 | persistentId);
    std::string userLegacy     = string_format("%s/%08X", getBaseSavePathLegacyFS().c_str(), 0x80000000 | persistentId);
    std::string userOriginal   = string_format("fs:/vol/save/%08X", 0x80000000 | persistentId);

    FSUtils::CreateSubfolder(common.c_str());
    FSUtils::CreateSubfolder(user.c_str());

    auto BaristaAccountSaveFilePathNew      = user + "/BaristaAccountSaveFile.dat";
    auto BaristaAccountSaveFilePathOriginal = userOriginal + "/BaristaAccountSaveFile.dat";
    auto BaristaAccountSaveFilePathLegacy   = userLegacy + "/BaristaAccountSaveFile.dat";
    if (!FSUtils::CheckFile(BaristaAccountSaveFilePathNew.c_str())) {
        if (FSUtils::CheckFile(BaristaAccountSaveFilePathLegacy.c_str())) {
            DEBUG_FUNCTION_LINE("Copy %s to %s", BaristaAccountSaveFilePathLegacy.c_str(), BaristaAccountSaveFilePathNew.c_str());
            if (!FSUtils::copyFile(BaristaAccountSaveFilePathLegacy, BaristaAccountSaveFilePathNew)) {
                DEBUG_FUNCTION_LINE_ERR("Failed to copy file: %s -> %s", BaristaAccountSaveFilePathLegacy.c_str(), BaristaAccountSaveFilePathNew.c_str());
            } else {
                if (remove(BaristaAccountSaveFilePathLegacy.c_str()) < 0) {
                    DEBUG_FUNCTION_LINE_ERR("Failed to delete %s", BaristaAccountSaveFilePathLegacy.c_str());
                }
            }
        } else {
            DEBUG_FUNCTION_LINE("Copy %s to %s", BaristaAccountSaveFilePathOriginal.c_str(), BaristaAccountSaveFilePathNew.c_str());
            if (!FSUtils::copyFile(BaristaAccountSaveFilePathOriginal, BaristaAccountSaveFilePathNew)) {
                DEBUG_FUNCTION_LINE_ERR("Failed to copy file: %s -> %s", BaristaAccountSaveFilePathOriginal.c_str(), BaristaAccountSaveFilePathNew.c_str());
            }
        }
    }

    auto BaristaCommonSaveFile         = common + "/BaristaCommonSaveFile.dat";
    auto BaristaCommonSaveFileOriginal = commonOriginal + "/BaristaCommonSaveFile.dat";
    auto BaristaCommonSaveFileLegacy   = commonLegacy + "/BaristaCommonSaveFile.dat";
    if (!FSUtils::CheckFile(BaristaCommonSaveFile.c_str())) {
        if (FSUtils::CheckFile(BaristaCommonSaveFileLegacy.c_str())) {
            DEBUG_FUNCTION_LINE("Copy %s to %s", BaristaCommonSaveFileLegacy.c_str(), BaristaCommonSaveFile.c_str());
            if (!FSUtils::copyFile(BaristaCommonSaveFileLegacy, BaristaCommonSaveFile)) {
                DEBUG_FUNCTION_LINE_ERR("Failed to copy file: %s -> %s", BaristaCommonSaveFileLegacy.c_str(), BaristaCommonSaveFile.c_str());
            } else {
                if (remove(BaristaCommonSaveFileLegacy.c_str()) < 0) {
                    DEBUG_FUNCTION_LINE_ERR("Failed to delete %s", BaristaCommonSaveFileLegacy.c_str());
                }
            }
        } else {
            DEBUG_FUNCTION_LINE("Copy %s to %s", BaristaCommonSaveFileOriginal.c_str(), BaristaCommonSaveFile.c_str());
            if (!FSUtils::copyFile(BaristaCommonSaveFileOriginal, BaristaCommonSaveFile)) {
                DEBUG_FUNCTION_LINE_ERR("Failed to copy file: %s -> %s", BaristaCommonSaveFileOriginal.c_str(), BaristaCommonSaveFile.c_str());
            }
        }
    }

    auto BaristaIconDataBase         = common + "/BaristaIconDataBase.dat";
    auto BaristaIconDataBaseOriginal = commonOriginal + "/BaristaIconDataBase.dat";
    auto BaristaIconDataBaseLegacy   = commonLegacy + "/BaristaIconDataBase.dat";
    if (!FSUtils::CheckFile(BaristaIconDataBase.c_str())) {
        if (FSUtils::CheckFile(BaristaIconDataBaseLegacy.c_str())) {
            DEBUG_FUNCTION_LINE("Copy %s to %s", BaristaIconDataBaseLegacy.c_str(), BaristaIconDataBase.c_str());
            if (!FSUtils::copyFile(BaristaIconDataBaseLegacy, BaristaIconDataBase)) {
                DEBUG_FUNCTION_LINE_ERR("Failed to copy file: %s -> %s", BaristaIconDataBaseLegacy.c_str(), BaristaIconDataBase.c_str());
            } else {
                if (remove(BaristaIconDataBaseLegacy.c_str()) < 0) {
                    DEBUG_FUNCTION_LINE_ERR("Failed to delete %s", BaristaIconDataBaseLegacy.c_str());
                }
            }
        } else {
            DEBUG_FUNCTION_LINE("Copy %s to %s", BaristaIconDataBaseOriginal.c_str(), BaristaIconDataBase.c_str());
            if (!FSUtils::copyFile(BaristaIconDataBaseOriginal, BaristaIconDataBase)) {
                DEBUG_FUNCTION_LINE_ERR("Failed to copy file: %s -> %s", BaristaIconDataBaseOriginal.c_str(), BaristaIconDataBase.c_str());
            }
        }
    }
}

void initSaveData() {
    SaveRedirectionCleanUp();
    CopyExistingFiles();

    std::string replaceDir = getBaseSavePathFS();
    DEBUG_FUNCTION_LINE("Setup save redirection: %s -> %s", "/vol/save", replaceDir.c_str());
    auto res = ContentRedirection_AddFSLayer(&saveLayer, "homp_save_redirection", replaceDir.c_str(), FS_LAYER_TYPE_SAVE_REPLACE);
    if (res != CONTENT_REDIRECTION_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("Failed to add save FS Layer: %d", res);
    }
}

DECL_FUNCTION(int32_t, LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb, nn::act::SlotNo slot, nn::act::ACTLoadOption unk1, char const *unk2, bool unk3) {
    int32_t result = real_LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb(slot, unk1, unk2, unk3);
    if (result >= 0 && gInWiiUMenu) {
        DEBUG_FUNCTION_LINE("Changed account, we need to init the save data");
        // If the account has changed, we need to init save data for this account
        initSaveData();
    }

    return result;
}

DECL_FUNCTION(SAVEStatus, SAVEGetSharedSaveDataPath, uint64_t titleID, const char *path, char *buffer, uint32_t bufferSize) {
    if (titleID == 0x0005001010040000L || // Wii U Menu JPN
        titleID == 0x0005001010040100L || // Wii U Menu USA
        titleID == 0x0005001010040200L) { // Wii U Menu EUR
        if (buffer != nullptr) {
            std::string commonReplacement = getBaseSavePath() + "/common";
            auto BaristaCommonSaveFile    = "fs:" + commonReplacement + "/BaristaCommonSaveFile.dat";
            auto BaristaIconDataBase      = "fs:" + commonReplacement + "/BaristaIconDataBase.dat";
            if (FSUtils::CheckFile(BaristaCommonSaveFile.c_str()) &&
                FSUtils::CheckFile(BaristaIconDataBase.c_str())) {
                snprintf(buffer, bufferSize, "%s/%s", commonReplacement.c_str(), path);
                DEBUG_FUNCTION_LINE("Redirect Wii U Menu common path to %s", commonReplacement.c_str());
                return SAVE_STATUS_OK;
            }
        }
    }
    return real_SAVEGetSharedSaveDataPath(titleID, path, buffer, bufferSize);
}

extern bool gHideHomebrew;
extern bool sSDIsMounted;
DECL_FUNCTION(int32_t, SAVEInit) {
    auto res = real_SAVEInit();
    if (res >= 0) {
        if (!sSDIsMounted || gHideHomebrew) {
            DEBUG_FUNCTION_LINE_VERBOSE("Skip SD redirection, no SD Card is mounted");
            return res;
        }
        if (OSGetTitleID() == 0x0005001010040000L || // Wii U Menu JPN
            OSGetTitleID() == 0x0005001010040100L || // Wii U Menu USA
            OSGetTitleID() == 0x0005001010040200L) { // Wii U Menu EUR

            initSaveData();
            gInWiiUMenu = true;
        } else {
            gInWiiUMenu = false;
        }
    }

    return res;
}

DECL_FUNCTION(FSError, FSGetLastErrorCodeForViewer, FSClient *client) {
    auto res = real_FSGetLastErrorCodeForViewer(client);
    if (!gInWiiUMenu) {
        return res;
    }
    if ((uint32_t) res == 1503030) {
        // If we encounter error 1503030 when running the Wii U Menu we probably hit a Wii U Menu save related issue
        // Either the sd card is write locked or the save on the sd card it corrupted. Let the user now about this..

        std::string deleteHint  = string_format("If not write locked, delete \"sd:" HOMEBREW_ON_MENU_PLUGIN_DATA_PATH_BASE "/%s/save/common/BaristaIconDataBase.dat\".", gSerialId.c_str());
        std::string deleteHint2 = string_format("If deleting this file doesn't fix the error code, delete this directory: \"sd:" HOMEBREW_ON_MENU_PLUGIN_DATA_PATH_BASE "/%s\".", gSerialId.c_str());
        NotificationModuleHandle handle;

        NotificationModule_AddDynamicNotification("Caution: This resets the order of application on the Wii U Menu when using Aroma.", &handle);
        NotificationModule_AddDynamicNotification(deleteHint2.c_str(), &handle);
        NotificationModule_AddDynamicNotification("", &handle);
        NotificationModule_AddDynamicNotification(deleteHint.c_str(), &handle);
        NotificationModule_AddDynamicNotification("", &handle);
        NotificationModule_AddDynamicNotification("", &handle);
        NotificationModule_AddDynamicNotification("The SD card appears to be write-locked or the Wii U Menu save on the SD card is corrupted. Check the SD card write lock.", &handle);
    }
    return res;
}

WUPS_MUST_REPLACE_FOR_PROCESS(SAVEInit, WUPS_LOADER_LIBRARY_NN_SAVE, SAVEInit, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb, WUPS_LOADER_LIBRARY_NN_ACT, LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE_FOR_PROCESS(FSGetLastErrorCodeForViewer, WUPS_LOADER_LIBRARY_COREINIT, FSGetLastErrorCodeForViewer, WUPS_FP_TARGET_PROCESS_WII_U_MENU);
WUPS_MUST_REPLACE(SAVEGetSharedSaveDataPath, WUPS_LOADER_LIBRARY_NN_SAVE, SAVEGetSharedSaveDataPath);