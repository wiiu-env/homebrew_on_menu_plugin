#include "FileWrapper.h"
#include "fileinfos.h"
#include "filelist.h"
#include "fs/FSUtils.h"
#include "utils/StringTools.h"
#include "utils/ini.h"
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/filesystem.h>
#include <coreinit/mcp.h>
#include <coreinit/mutex.h>
#include <coreinit/systeminfo.h>
#include <coreinit/title.h>
#include <cstring>
#include <fs/DirList.h>
#include <nn/acp.h>
#include <rpxloader.h>
#include <sysapp/title.h>
#include <utils/logger.h>
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

void readCustomTitlesFromSD();

extern "C" void _SYSLaunchTitleWithStdArgsInNoSplash(uint64_t, uint32_t);

WUPS_USE_WUT_DEVOPTAB();

OSMutex fileWrapperMutex;
OSMutex fileinfoMutex;

INITIALIZE_PLUGIN() {
    memset((void *) &current_launched_title_info, 0, sizeof(current_launched_title_info));
    memset((void *) &gLaunchXML, 0, sizeof(gLaunchXML));
    memset((void *) &gFileInfos, 0, sizeof(gFileInfos));
    gHomebrewLaunched = FALSE;
    OSInitMutex(&fileWrapperMutex);
    OSInitMutex(&fileinfoMutex);
}

ON_APPLICATION_START() {
    initLogging();

    if (OSGetTitleID() == 0x0005001010040000L || // Wii U Menu JPN
        OSGetTitleID() == 0x0005001010040100L || // Wii U Menu USA
        OSGetTitleID() == 0x0005001010040200L) { // Wii U Menu ERU
        readCustomTitlesFromSD();
    }

    if (_SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY) != OSGetTitleID()) {
        DEBUG_FUNCTION_LINE("gHomebrewLaunched to FALSE");
        gHomebrewLaunched = FALSE;
    }
}

ON_APPLICATION_ENDS() {
    deinitLogging();
}

void fillXmlForTitleID(uint32_t titleid_upper, uint32_t titleid_lower, ACPMetaXml *out_buf) {
    int32_t id = getIDByLowerTitleID(titleid_lower);
    if (id < 0) {
        DEBUG_FUNCTION_LINE("Failed to get id by titleid");
        return;
    }
    if (id >= FILE_INFO_SIZE) {
        return;
    }
    out_buf->title_id = ((uint64_t) titleid_upper * 0x100000000) + titleid_lower;
    strncpy(out_buf->longname_en, gFileInfos[id].longname, 64);
    strncpy(out_buf->shortname_en, gFileInfos[id].shortname, 64);
    strncpy(out_buf->publisher_en, gFileInfos[id].author, 64);
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
    strncpy(out_buf->product_code, "WUP-P-HBLD", strlen("WUP-P-HBLD") + 1);
    strncpy(out_buf->content_platform, "WUP", strlen("WUP") + 1);
    strncpy(out_buf->company_code, "0001", strlen("0001") + 1);
}

/* hash: compute hash value of string */
unsigned int hash(char *str) {
    unsigned int h;
    unsigned char *p;

    h = 0;
    for (p = (unsigned char *) str; *p != '\0'; p++) {
        h = 37 * h + *p;
    }
    return h; // or, h % ARRAY_SIZE;
}

static int handler(void *user, const char *section, const char *name,
                   const char *value) {
    auto *fInfo = (FileInfos *) user;
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    DEBUG_FUNCTION_LINE("%s %s %s", section, name, value);
    if (MATCH("menu", "longname")) {
        strncpy(fInfo->longname, value, 64 - 1);
    } else if (MATCH("menu", "shortname")) {
        strncpy(fInfo->shortname, value, 64 - 1);
    } else if (MATCH("menu", "author")) {
        strncpy(fInfo->author, value, 64 - 1);
    } else {
        return 0; /* unknown section/name, error */
    }

    return 1;
}

void readCustomTitlesFromSD() {
    // Reset current infos
    unmountAllRomfs();
    memset((void *) &gFileInfos, 0, sizeof(gFileInfos));

    DirList dirList("fs:/vol/external01/wiiu/apps", ".rpx,.wuhb", DirList::Files | DirList::CheckSubfolders, 1);
    dirList.SortList();

    int j = 0;
    for (int i = 0; i < dirList.GetFilecount(); i++) {
        if (j >= FILE_INFO_SIZE) {
            DEBUG_FUNCTION_LINE("TOO MANY TITLES");
            break;
        }

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

        char *repl  = (char *) "fs:/vol/external01/";
        char *with  = (char *) "";
        char *input = (char *) dirList.GetFilepath(i);

        char *path = StringTools::str_replace(input, repl, with);
        if (path != nullptr) {
            strncpy(gFileInfos[j].path, path, 255);
            free(path);
        }

        gFileInfos[j].lowerTitleID = hash(gFileInfos[j].path);

        MCPTitleListType *cur_title_info = &(gFileInfos[j].titleInfo);

        char buffer[25];
        snprintf(buffer, 25, "/custom/%08X%08X", UPPER_TITLE_ID_HOMEBREW, gFileInfos[j].lowerTitleID);
        strcpy(cur_title_info->path, buffer);

        strncpy(gFileInfos[j].filename, dirList.GetFilename(i), 255);
        strncpy(gFileInfos[j].longname, dirList.GetFilename(i), 64);
        strncpy(gFileInfos[j].shortname, dirList.GetFilename(i), 64);
        strncpy(gFileInfos[j].author, dirList.GetFilename(i), 64);
        gFileInfos[j].source = 0; //SD Card;

        const char *indexedDevice = "mlc";
        strcpy(cur_title_info->indexedDevice, indexedDevice);

        // System apps don't have a splash screen.
        cur_title_info->appType = MCP_APP_TYPE_SYSTEM_APPS;

        // Check if the have bootTvTex and bootDrcTex that could be shown.
        if (StringTools::EndsWith(gFileInfos[j].filename, ".wuhb")) {
            int result = 0;
            if ((result = RL_MountBundle("romfscheck", dirList.GetFilepath(i), BundleSource_FileDescriptor)) == 0) {
                uint32_t file_handle = 0;
                if (RL_FileOpen("romfscheck:/meta/meta.ini", &file_handle) == 0) {
                    // this buffer should be big enough for our .ini
                    char ini_buffer[0x1000];
                    memset(ini_buffer, 0, sizeof(ini_buffer));
                    uint32_t offset = 0;
                    uint32_t toRead = sizeof(ini_buffer);
                    do {
                        int res = RL_FileRead(file_handle, reinterpret_cast<uint8_t *>(&ini_buffer[offset]), toRead);
                        if (res <= 0) {
                            break;
                        }
                        offset += res;
                        toRead -= res;
                    } while (offset < sizeof(ini_buffer));

                    if (ini_parse_string(ini_buffer, handler, &gFileInfos[j]) < 0) {
                        DEBUG_FUNCTION_LINE("Failed to parse ini");
                    }

                    RL_FileClose(file_handle);
                }

                bool foundSplashScreens = true;
                if (!RL_FileExists("romfscheck:/meta/bootTvTex.tga")) {
                    foundSplashScreens = false;
                }
                if (!RL_FileExists("romfscheck:/meta/bootDrcTex.tga")) {
                    foundSplashScreens = false;
                }
                if (foundSplashScreens) {
                    // Show splash screens
                    cur_title_info->appType = MCP_APP_TYPE_GAME;
                }
                RL_UnmountBundle("romfscheck");
            } else {
                DEBUG_FUNCTION_LINE("%s is not a valid .wuhb file: %d", dirList.GetFilepath(i), result);
                continue;
            }
        }

        cur_title_info->titleId      = TITLE_ID_HOMEBREW_MASK | gFileInfos[j].lowerTitleID;
        cur_title_info->titleVersion = 1;
        cur_title_info->groupId      = 0x400;

        cur_title_info->osVersion  = OSGetOSID();
        cur_title_info->sdkVersion = __OSGetProcessSDKVersion();
        cur_title_info->unk0x60    = 0;

        j++;
    }
}

DECL_FUNCTION(int32_t, MCP_TitleList, uint32_t handle, uint32_t *outTitleCount, MCPTitleListType *titleList, uint32_t size) {
    int32_t result      = real_MCP_TitleList(handle, outTitleCount, titleList, size);
    uint32_t titlecount = *outTitleCount;

    OSLockMutex(&fileinfoMutex);
    for (auto &gFileInfo : gFileInfos) {
        if (gFileInfo.lowerTitleID == 0) {
            break;
        }
        memcpy(&(titleList[titlecount]), &(gFileInfo.titleInfo), sizeof(gFileInfo.titleInfo));
        titlecount++;
    }
    OSUnlockMutex(&fileinfoMutex);

    *outTitleCount = titlecount;

    return result;
}

DECL_FUNCTION(int32_t, ACPCheckTitleLaunchByTitleListTypeEx, MCPTitleListType *title, uint32_t u2) {
    if ((title->titleId & TITLE_ID_HOMEBREW_MASK) == TITLE_ID_HOMEBREW_MASK) {
        int32_t id = getIDByLowerTitleID(title->titleId & 0xFFFFFFFF);
        if (id >= 0) {
            DEBUG_FUNCTION_LINE("Starting a homebrew title");

            fillXmlForTitleID((title->titleId & 0xFFFFFFFF00000000) >> 32, (title->titleId & 0xFFFFFFFF), &gLaunchXML);

            std::string bundleFilePath = std::string("/vol/external01/") + gFileInfos[id].path;

            gHomebrewLaunched = TRUE;

            RL_LoadFromSDOnNextLaunch(gFileInfos[id].path);
            return 0;
        } else {
            DEBUG_FUNCTION_LINE("Failed to get the id for titleID %016llX", title->titleId);
        }
    }

    int result = real_ACPCheckTitleLaunchByTitleListTypeEx(title, u2);
    return result;
}

DECL_FUNCTION(int, FSOpenFile, FSClient *client, FSCmdBlock *block, char *path, const char *mode, int *handle, int error) {
    const char *start   = "/vol/storage_mlc01/sys/title/0005000F";
    const char *icon    = ".tga";
    const char *iconTex = "iconTex.tga";
    const char *sound   = ".btsnd";

    if (StringTools::EndsWith(path, icon) || StringTools::EndsWith(path, sound)) {
        if (strncmp(path, start, strlen(start)) == 0) {
            int res = FS_STATUS_NOT_FOUND;

            if (StringTools::EndsWith(path, iconTex)) {
                // fallback to dummy icon if loaded homebrew is no .wbf
                *handle = 0x13371338;
                res     = FS_STATUS_OK;
            }

            uint32_t lowerTitleID;
            char *id     = path + 1 + strlen(start);
            id[8]        = 0;
            char *ending = id + 9;
            sscanf(id, "%08X", &lowerTitleID);
            int32_t idVal = getIDByLowerTitleID(lowerTitleID);
            if (idVal < 0) {
                DEBUG_FUNCTION_LINE("Failed to find id for titleID %08X", lowerTitleID);
            } else {
                if (OpenFileForID(idVal, ending, handle) < 0) {
                    return res;
                }
            }
            return FS_STATUS_OK;
        }
    }

    int result = real_FSOpenFile(client, block, path, mode, handle, error);
    return result;
}

DECL_FUNCTION(FSStatus, FSCloseFile, FSClient *client, FSCmdBlock *block, FSFileHandle handle, uint32_t flags) {
    if (handle == 0x13371338) {
        return FS_STATUS_OK;
    } else if ((handle & 0xFF000000) == 0xFF000000) {
        int32_t fd    = (handle & 0x00000FFF);
        int32_t romid = (handle & 0x00FFF000) >> 12;
        OSLockMutex(&fileinfoMutex);
        uint32_t rl_handle = gFileHandleWrapper[fd].handle;
        RL_FileClose(rl_handle);
        if (gFileInfos[romid].openedFiles--) {
            DCFlushRange(&gFileInfos[romid].openedFiles, 4);
            if (gFileInfos[romid].openedFiles <= 0) {
                DEBUG_FUNCTION_LINE("unmount romfs no more handles");
                unmountRomfs(romid);
            }
        }
        OSUnlockMutex(&fileinfoMutex);
        return FS_STATUS_OK;
    }
    return real_FSCloseFile(client, block, handle, flags);
}

DECL_FUNCTION(FSStatus, FSReadFile, FSClient *client, FSCmdBlock *block, uint8_t *buffer, uint32_t size, uint32_t count, FSFileHandle handle, uint32_t unk1, uint32_t flags) {
    if (handle == 0x13371338) {
        uint32_t cpySize = size * count;
        if (iconTex_tga_size < cpySize) {
            cpySize = iconTex_tga_size;
        }
        memcpy(buffer, iconTex_tga, cpySize);
        return (FSStatus) (cpySize / size);
    } else if ((handle & 0xFF000000) == 0xFF000000) {
        int32_t fd    = (handle & 0x00000FFF);
        int32_t romid = (handle & 0x00FFF000) >> 12;

        uint32_t rl_handle = gFileHandleWrapper[fd].handle;

        int readSize = RL_FileRead(rl_handle, buffer, (size * count));

        return (FSStatus) (readSize / size);
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
            for (auto &gFileInfo : gFileInfos) {
                if (gFileInfo.lowerTitleID == inputPtrAsU32[1]) {
                    memcpy(&titles[0], &(gFileInfo.titleInfo), sizeof(MCPTitleListType));
                    return 1;
                }
            }
            DEBUG_FUNCTION_LINE("Failed to find lower TID %08X", inputPtrAsU32[1]);
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
