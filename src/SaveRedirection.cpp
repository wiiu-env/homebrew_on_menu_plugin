#include "SaveRedirection.h"
#include <coreinit/filesystem.h>
#include <coreinit/title.h>
#include <fs/FSUtils.h>
#include <functional>
#include <nn/act.h>
#include <nn/save.h>
#include <string>
#include <utils/StringTools.h>
#include <utils/logger.h>
#include <wups.h>

bool gInWiiUMenu __attribute__((section(".data")));

static FSStatus CallWithNewPath(const char *oldPath, const std::function<FSStatus(const char *)> &callFunctionWithPath) {
    if (!gInWiiUMenu || strncmp(oldPath, "/vol/save/", 10) != 0) {
        return callFunctionWithPath(oldPath);
    }

    char *newPath = (char *) malloc(0x280);
    if (!newPath) {
        return FS_STATUS_FATAL_ERROR;
    }

    snprintf(newPath, 0x27F, SAVE_REPLACEMENT_PATH "/save/%s", &oldPath[10]);

    auto res = callFunctionWithPath(newPath);
    free(newPath);
    return res;
}

void initSaveData() {
    nn::act::Initialize();
    nn::act::PersistentId persistentId = nn::act::GetPersistentId();
    nn::act::Finalize();

    std::string common         = "fs:" SAVE_REPLACEMENT_PATH "/save/common";
    std::string commonOriginal = "fs:/vol/save/common";
    std::string user           = StringTools::strfmt("fs:" SAVE_REPLACEMENT_PATH "/save/%08X", 0x80000000 | persistentId);
    std::string userOriginal   = StringTools::strfmt("fs:/vol/save/%08X", 0x80000000 | persistentId);

    FSUtils::CreateSubfolder(common.c_str());
    FSUtils::CreateSubfolder(user.c_str());

    auto BaristaAccountSaveFilePathNew      = user + "/BaristaAccountSaveFile.dat";
    auto BaristaAccountSaveFilePathOriginal = userOriginal + "/BaristaAccountSaveFile.dat";
    if (!FSUtils::CheckFile(BaristaAccountSaveFilePathNew.c_str())) {
        DEBUG_FUNCTION_LINE("Copy %s to %s", BaristaAccountSaveFilePathOriginal.c_str(), BaristaAccountSaveFilePathNew.c_str());
        FSUtils::copyFile(BaristaAccountSaveFilePathOriginal, BaristaAccountSaveFilePathNew);
    }

    auto BaristaCommonSaveFile         = common + "/BaristaCommonSaveFile.dat";
    auto BaristaCommonSaveFileOriginal = commonOriginal + "/BaristaCommonSaveFile.dat";
    if (!FSUtils::CheckFile(BaristaCommonSaveFile.c_str())) {
        DEBUG_FUNCTION_LINE("Copy %s to %s", BaristaCommonSaveFileOriginal.c_str(), BaristaCommonSaveFile.c_str());
        FSUtils::copyFile(BaristaCommonSaveFileOriginal, BaristaCommonSaveFile);
    }

    auto BaristaIconDataBase         = common + "/BaristaIconDataBase.dat";
    auto BaristaIconDataBaseOriginal = commonOriginal + "/BaristaIconDataBase.dat";
    if (!FSUtils::CheckFile(BaristaIconDataBase.c_str())) {
        DEBUG_FUNCTION_LINE("Copy %s to %s", BaristaIconDataBaseOriginal.c_str(), BaristaIconDataBase.c_str());
        FSUtils::copyFile(BaristaIconDataBaseOriginal, BaristaIconDataBase);
    }
}

DECL_FUNCTION(FSStatus, FSOpenFileAsync, FSClient *client, FSCmdBlock *block, const char *path, const char *mode, FSFileHandle *outHandle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    return CallWithNewPath(path, [client, block, mode, outHandle, errorMask, asyncData](const char *_path) -> FSStatus {
        DEBUG_FUNCTION_LINE("Call FSOpenFileAsync %s", _path);
        return real_FSOpenFileAsync(client, block, _path, mode, outHandle, errorMask, asyncData);
    });
}

DECL_FUNCTION(FSStatus, FSGetStatAsync, FSClient *client, FSCmdBlock *block, const char *path, FSStat *stat, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    return CallWithNewPath(path, [client, block, stat, errorMask, asyncData](const char *_path) -> FSStatus {
        DEBUG_FUNCTION_LINE("Call FSGetStatAsync %s", _path);
        return real_FSGetStatAsync(client, block, _path, stat, errorMask, asyncData);
    });
}

DECL_FUNCTION(FSStatus, FSRemoveAsync, FSClient *client, FSCmdBlock *block, const char *path, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    return CallWithNewPath(path, [client, block, errorMask, asyncData](const char *_path) -> FSStatus {
        DEBUG_FUNCTION_LINE("Call FSRemoveAsync %s", _path);
        return real_FSRemoveAsync(client, block, _path, errorMask, asyncData);
    });
}

DECL_FUNCTION(FSStatus, FSOpenDirAsync, FSClient *client, FSCmdBlock *block, const char *path, FSDirectoryHandle *handle, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    return CallWithNewPath(path, [client, block, handle, errorMask, asyncData](const char *_path) -> FSStatus {
        DEBUG_FUNCTION_LINE("Call FSOpenDirAsync %s", _path);
        return real_FSOpenDirAsync(client, block, _path, handle, errorMask, asyncData);
    });
}

DECL_FUNCTION(FSStatus, FSMakeDirAsync, FSClient *client, FSCmdBlock *block, const char *path, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    return CallWithNewPath(path, [client, block, errorMask, asyncData](const char *_path) -> FSStatus {
        DEBUG_FUNCTION_LINE("Call FSMakeDirAsync %s", _path);
        return real_FSMakeDirAsync(client, block, _path, errorMask, asyncData);
    });
}

DECL_FUNCTION(FSStatus, FSChangeModeAsync, FSClient *client, FSCmdBlock *block, const char *path, FSMode mode, FSMode modeMask, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    return CallWithNewPath(path, [client, block, mode, modeMask, errorMask, asyncData](const char *_path) -> FSStatus {
        DEBUG_FUNCTION_LINE("Call FSChangeModeAsync %s", _path);
        return real_FSChangeModeAsync(client, block, _path, mode, modeMask, errorMask, asyncData);
    });
}

DECL_FUNCTION(FSStatus, FSChangeDirAsync, FSClient *client, FSCmdBlock *block, const char *path, FSErrorFlag errorMask, FSAsyncData *asyncData) {
    return CallWithNewPath(path, [client, block, errorMask, asyncData](const char *_path) -> FSStatus {
        DEBUG_FUNCTION_LINE("Call FSChangeDirAsync %s", _path);
        return real_FSChangeDirAsync(client, block, _path, errorMask, asyncData);
    });
}

DECL_FUNCTION(int32_t, LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb, nn::act::SlotNo slot, nn::act::ACTLoadOption unk1, char const *unk2, bool unk3) {
    int32_t result = real_LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb(slot, unk1, unk2, unk3);
    if (result >= 0 && gInWiiUMenu) {
        DEBUG_FUNCTION_LINE("changed account, init save data");
        // If the account has changed, we need to init save data for this account
        // Calls our function replacement.
        SAVEInit();
    }

    return result;
}

DECL_FUNCTION(int32_t, SAVEInit) {
    auto res = real_SAVEInit();
    if (res >= 0) {
        if (OSGetTitleID() == 0x0005001010040000L || // Wii U Menu JPN
            OSGetTitleID() == 0x0005001010040100L || // Wii U Menu USA
            OSGetTitleID() == 0x0005001010040200L) { // Wii U Menu EUR
            DEBUG_FUNCTION_LINE("Init Save redirection");

            initSaveData();
            gInWiiUMenu = true;
        } else {
            gInWiiUMenu = false;
        }
    }

    return res;
}


WUPS_MUST_REPLACE(SAVEInit, WUPS_LOADER_LIBRARY_NN_SAVE, SAVEInit);
WUPS_MUST_REPLACE(FSOpenFileAsync, WUPS_LOADER_LIBRARY_COREINIT, FSOpenFileAsync);
WUPS_MUST_REPLACE(FSOpenDirAsync, WUPS_LOADER_LIBRARY_COREINIT, FSOpenDirAsync);
WUPS_MUST_REPLACE(FSRemoveAsync, WUPS_LOADER_LIBRARY_COREINIT, FSRemoveAsync);
WUPS_MUST_REPLACE(FSMakeDirAsync, WUPS_LOADER_LIBRARY_COREINIT, FSMakeDirAsync);
// WUPS_MUST_REPLACE(FSRenameAsync, WUPS_LOADER_LIBRARY_COREINIT, FSRenameAsync);
WUPS_MUST_REPLACE(FSChangeDirAsync, WUPS_LOADER_LIBRARY_COREINIT, FSChangeDirAsync);
WUPS_MUST_REPLACE(FSChangeModeAsync, WUPS_LOADER_LIBRARY_COREINIT, FSChangeModeAsync);
WUPS_MUST_REPLACE(FSGetStatAsync, WUPS_LOADER_LIBRARY_COREINIT, FSGetStatAsync);
WUPS_MUST_REPLACE(LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb, WUPS_LOADER_LIBRARY_NN_ACT, LoadConsoleAccount__Q2_2nn3actFUc13ACTLoadOptionPCcb);