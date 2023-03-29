#include "SaveRedirection.h"
#include <content_redirection/redirection.h>
#include <coreinit/title.h>
#include <fs/FSUtils.h>
#include <functional>
#include <nn/act.h>
#include <nn/save.h>
#include <string>
#include <utils/StringTools.h>
#include <utils/logger.h>
#include <wups.h>

bool gInWiiUMenu __attribute__((section(".data")))        = false;
CRLayerHandle saveLayer __attribute__((section(".data"))) = 0;

void SaveRedirectionCleanUp() {
    if (saveLayer != 0) {
        DEBUG_FUNCTION_LINE("Remove save redirection: %s -> %s", "/vol/save", "fs:" SAVE_REPLACEMENT_PATH "/save/");
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

    std::string common         = "fs:" SAVE_REPLACEMENT_PATH "/save/common";
    std::string commonOriginal = "fs:/vol/save/common";
    std::string user           = string_format("fs:" SAVE_REPLACEMENT_PATH "/save/%08X", 0x80000000 | persistentId);
    std::string userOriginal   = string_format("fs:/vol/save/%08X", 0x80000000 | persistentId);

    FSUtils::CreateSubfolder(common.c_str());
    FSUtils::CreateSubfolder(user.c_str());

    auto BaristaAccountSaveFilePathNew      = user + "/BaristaAccountSaveFile.dat";
    auto BaristaAccountSaveFilePathOriginal = userOriginal + "/BaristaAccountSaveFile.dat";
    if (!FSUtils::CheckFile(BaristaAccountSaveFilePathNew.c_str())) {
        DEBUG_FUNCTION_LINE("Copy %s to %s", BaristaAccountSaveFilePathOriginal.c_str(), BaristaAccountSaveFilePathNew.c_str());
        if (!FSUtils::copyFile(BaristaAccountSaveFilePathOriginal, BaristaAccountSaveFilePathNew)) {
            DEBUG_FUNCTION_LINE_ERR("Failed to copy file: %s -> %s", BaristaAccountSaveFilePathOriginal.c_str(), BaristaAccountSaveFilePathNew.c_str());
        }
    }

    auto BaristaCommonSaveFile         = common + "/BaristaCommonSaveFile.dat";
    auto BaristaCommonSaveFileOriginal = commonOriginal + "/BaristaCommonSaveFile.dat";
    if (!FSUtils::CheckFile(BaristaCommonSaveFile.c_str())) {
        DEBUG_FUNCTION_LINE("Copy %s to %s", BaristaCommonSaveFileOriginal.c_str(), BaristaCommonSaveFile.c_str());
        if (!FSUtils::copyFile(BaristaCommonSaveFileOriginal, BaristaCommonSaveFile)) {
            DEBUG_FUNCTION_LINE_ERR("Failed to copy file: %s -> %s", BaristaCommonSaveFileOriginal.c_str(), BaristaCommonSaveFile.c_str());
        }
    }

    auto BaristaIconDataBase         = common + "/BaristaIconDataBase.dat";
    auto BaristaIconDataBaseOriginal = commonOriginal + "/BaristaIconDataBase.dat";
    if (!FSUtils::CheckFile(BaristaIconDataBase.c_str())) {
        DEBUG_FUNCTION_LINE("Copy %s to %s", BaristaIconDataBaseOriginal.c_str(), BaristaIconDataBase.c_str());
        if (!FSUtils::copyFile(BaristaIconDataBaseOriginal, BaristaIconDataBase)) {
            DEBUG_FUNCTION_LINE_ERR("Failed to copy file: %s -> %s", BaristaIconDataBaseOriginal.c_str(), BaristaIconDataBase.c_str());
        }
    }
}

void initSaveData() {
    SaveRedirectionCleanUp();
    CopyExistingFiles();

    DEBUG_FUNCTION_LINE("Setup save redirection: %s -> %s", "/vol/save", "fs:" SAVE_REPLACEMENT_PATH "/save/");
    auto res = ContentRedirection_AddFSLayer(&saveLayer, "homp_save_redirection", "fs:" SAVE_REPLACEMENT_PATH "/save/", FS_LAYER_TYPE_SAVE_REPLACE);
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
            std::string commonReplacement = SAVE_REPLACEMENT_PATH "/save/common";
            auto BaristaCommonSaveFile    = "fs:" + commonReplacement + "/BaristaCommonSaveFile.dat";
            auto BaristaIconDataBase      = "fs:" + commonReplacement + "/BaristaIconDataBase.dat";
            if (FSUtils::CheckFile(BaristaCommonSaveFile.c_str()) &&
                FSUtils::CheckFile(BaristaIconDataBase.c_str())) {
                snprintf(buffer, bufferSize, "%s/%s", commonReplacement.c_str(), path);
                DEBUG_FUNCTION_LINE("Redirect Wii U Menu common path with %s", buffer);
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

WUPS_MUST_REPLACE(SAVEInit, WUPS_LOADER_LIBRARY_NN_SAVE, SAVEInit);
WUPS_MUST_REPLACE(LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb, WUPS_LOADER_LIBRARY_NN_ACT, LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb);
WUPS_MUST_REPLACE(SAVEGetSharedSaveDataPath, WUPS_LOADER_LIBRARY_NN_SAVE, SAVEGetSharedSaveDataPath);