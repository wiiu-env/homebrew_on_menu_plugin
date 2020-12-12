#include <wups.h>
#include <cstdio>
#include <cstring>
#include <coreinit/title.h>
#include <coreinit/cache.h>
#include <coreinit/systeminfo.h>
#include <coreinit/mcp.h>
#include <coreinit/filesystem.h>
#include <sysapp/title.h>
#include <nn/acp.h>
#include <coreinit/ios.h>
#include <utils/logger.h>
#include "utils/StringTools.h"
#include <fs/DirList.h>
#include <wut_romfs_dev.h>
#include "readFileWrapper.h"
#include <whb/log_udp.h>
#include "fs/FSUtils.h"
#include "romfs_helper.h"
#include "filelist.h"

struct _ACPMetaData {
    char bootmovie[80696];
    char bootlogo[28604];
} _ACPMetaData;

WUPS_PLUGIN_NAME("Homebrew in Wii U menu");
WUPS_PLUGIN_DESCRIPTION("Allows the user to load homebrew from the Wii U menu");
WUPS_PLUGIN_VERSION("0.1");
WUPS_PLUGIN_AUTHOR("Maschell");
WUPS_PLUGIN_LICENSE("GPL");

#define UPPER_TITLE_ID_HOMEBREW 0x0005000F
#define TITLE_ID_HOMEBREW_MASK (((uint64_t) UPPER_TITLE_ID_HOMEBREW) << 32)

char gIconCache[65580] __attribute__((section(".data")));
ACPMetaXml gLaunchXML __attribute__((section(".data")));
MCPTitleListType template_title __attribute__((section(".data")));
BOOL gHomebrewLaunched __attribute__((section(".data")));

WUPS_USE_WUT_CRT()

INITIALIZE_PLUGIN() {
    memset((void *) &template_title, 0, sizeof(template_title));
    memset((void *) &gLaunchXML, 0, sizeof(gLaunchXML));
    memset((void *) &gFileInfos, 0, sizeof(gFileInfos));
    memset((void *) &gFileReadInformation, 0, sizeof(gFileReadInformation));
    memset((void *) &gIconCache, 0, sizeof(gIconCache));
    gHomebrewLaunched = FALSE;
}

ON_APPLICATION_START(args) {
    WHBLogUdpInit();
    DEBUG_FUNCTION_LINE("IN PLUGIN");

    if (_SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY) != OSGetTitleID()) {
        DEBUG_FUNCTION_LINE("gHomebrewLaunched to FALSE");
        gHomebrewLaunched = FALSE;
    }
}

ON_APPLICATION_END() {
    DeInitAllFiles();
    unmountAllRomfs();
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
    strncpy(out_buf->longname_en, gFileInfos[id].name, 511);
    strncpy(out_buf->shortname_en, gFileInfos[id].name, 255);
    strncpy(out_buf->publisher_en, gFileInfos[id].name, 255);
    out_buf->e_manual = 1;
    out_buf->e_manual_version = 0;
    out_buf->title_version = 1;
    out_buf->network_use = 1;
    out_buf->launching_flag = 4;
    out_buf->online_account_use = 1;
    out_buf->os_version = 0x000500101000400A;
    out_buf->region = 0xFFFFFFFF;
    out_buf->common_save_size = 0x0000000001790000;
    out_buf->group_id = 0x400;
    out_buf->drc_use = 1;
    out_buf->version = 1;
    out_buf->reserved_flag0 = 0x00010001;
    out_buf->reserved_flag6 = 0x00000003;
    out_buf->pc_usk = 128;
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

DECL_FUNCTION(int32_t, MCP_TitleList, uint32_t handle, uint32_t *outTitleCount, MCPTitleListType *titleList, uint32_t size) {
    int32_t result = real_MCP_TitleList(handle, outTitleCount, titleList, size);
    uint32_t titlecount = *outTitleCount;

    DirList dirList("fs:/vol/external01/wiiu/apps", ".rpx,.wbf", DirList::Files | DirList::CheckSubfolders, 1);
    dirList.SortList();

    int j = 0;
    for (int i = 0; i < dirList.GetFilecount(); i++) {
        if (j >= FILE_INFO_SIZE) {
            DEBUG_FUNCTION_LINE("TOO MANY TITLES");
            break;
        }
        //! skip our own application in the listing
        if (strcasecmp(dirList.GetFilename(i), "homebrew_launcher.rpx") == 0) {
            continue;
        }
        //! skip our own application in the listing
        if (strcasecmp(dirList.GetFilename(i), "temp.rpx") == 0) {
            continue;
        }

        //! skip hidden linux and mac files
        if (dirList.GetFilename(i)[0] == '.' || dirList.GetFilename(i)[0] == '_') {
            continue;
        }

        char *repl = (char *) "fs:/vol/external01/";
        char *with = (char *) "";
        char *input = (char *) dirList.GetFilepath(i);

        char *path = StringTools::str_replace(input, repl, with);
        if (path != NULL) {
            strncpy(gFileInfos[j].path, path, 255);
            free(path);
        }

        gFileInfos[j].lowerTitleID = hash(gFileInfos[j].path);

        char buffer[25];
        snprintf(buffer, 25, "/custom/%08X%08X", UPPER_TITLE_ID_HOMEBREW, gFileInfos[j].lowerTitleID);
        strcpy(template_title.path, buffer);

        strncpy(gFileInfos[j].name, dirList.GetFilename(i), 255);
        gFileInfos[j].source = 0; //SD Card;

        const char *indexedDevice = "mlc";
        strcpy(template_title.indexedDevice, indexedDevice);


        // System apps don't have a splash screen.
        template_title.appType = MCP_APP_TYPE_SYSTEM_APPS;

        // Check if the have bootTvTex and bootDrcTex that could be shown.
        if (StringTools::EndsWith(gFileInfos[j].name, ".wbf")) {
            if (romfsMount("romfscheck", dirList.GetFilepath(i)) == 0) {
                bool foundSplashScreens = true;
                if (!FSUtils::CheckFile("romfscheck:/meta/bootTvTex.tga") && !FSUtils::CheckFile("romfscheck:/meta/bootTvTex.tga.gz")) {
                    foundSplashScreens = false;
                }
                if (!FSUtils::CheckFile("romfscheck:/meta/bootDrcTex.tga") && !FSUtils::CheckFile("romfscheck:/meta/bootDrcTex.tga.gz")) {
                    foundSplashScreens = false;
                }
                if (foundSplashScreens) {
                    DEBUG_FUNCTION_LINE("Show splash screens");
                    // Show splash screens
                    template_title.appType = MCP_APP_TYPE_GAME;
                }
                romfsUnmount("romfscheck");
            } else {
                //DEBUG_FUNCTION_LINE("Mounting %s failed", dirList.GetFilepath(i));
            }
        }

        template_title.titleId = TITLE_ID_HOMEBREW_MASK | gFileInfos[j].lowerTitleID;
        template_title.titleVersion = 1;
        template_title.groupId = 0x400;

        template_title.osVersion = OSGetOSID();
        template_title.sdkVersion = __OSGetProcessSDKVersion();
        template_title.unk0x60 = 0;

        DEBUG_FUNCTION_LINE("[%d] %s [%016llX]", j, gFileInfos[j].path, template_title.titleId);

        memcpy(&(titleList[titlecount]), &template_title, sizeof(template_title));

        titlecount++;
        j++;
    }

    *outTitleCount = titlecount;

    return result;
}

DECL_FUNCTION(int32_t, MCP_GetTitleInfoByTitleAndDevice, uint32_t mcp_handle, uint32_t titleid_lower_1, uint32_t titleid_upper, uint32_t titleid_lower_2, uint32_t unknown, MCPTitleListType *title) {
    if (gHomebrewLaunched) {
        memcpy(title, &(template_title), sizeof(MCPTitleListType));
    } else if (titleid_upper == UPPER_TITLE_ID_HOMEBREW) {
        char buffer[25];
        snprintf(buffer, 25, "/custom/%08X%08X", titleid_upper, titleid_lower_2);
        strcpy(template_title.path, buffer);
        template_title.titleId = TITLE_ID_HOMEBREW_MASK | titleid_lower_1;
        memcpy(title, &(template_title), sizeof(MCPTitleListType));
        return 0;
    }
    int result = real_MCP_GetTitleInfoByTitleAndDevice(mcp_handle, titleid_lower_1, titleid_upper, titleid_lower_2, unknown, title);

    return result;
}

typedef struct __attribute((packed)) {
    uint32_t command;
    uint32_t target;
    uint32_t filesize;
    uint32_t fileoffset;
    char path[256];
} LOAD_REQUEST;

int32_t getRPXInfoForID(uint32_t id, romfs_fileInfo *info);

DECL_FUNCTION(int32_t, ACPCheckTitleLaunchByTitleListTypeEx, MCPTitleListType *title, uint32_t u2) {
    if ((title->titleId & TITLE_ID_HOMEBREW_MASK) == TITLE_ID_HOMEBREW_MASK) {
        int32_t id = getIDByLowerTitleID(title->titleId & 0xFFFFFFFF);
        if (id >= 0) {
            DEBUG_FUNCTION_LINE("Started homebrew");
            gHomebrewLaunched = TRUE;
            fillXmlForTitleID((title->titleId & 0xFFFFFFFF00000000) >> 32, (title->titleId & 0xFFFFFFFF), &gLaunchXML);

            LOAD_REQUEST request;
            memset(&request, 0, sizeof(request));

            request.command = 0xFC; // IPC_CUSTOM_LOAD_CUSTOM_RPX;
            request.target = 0;     // LOAD_FILE_TARGET_SD_CARD
            request.filesize = 0;   // unknown
            request.fileoffset = 0; //

            romfs_fileInfo info;
            int res = getRPXInfoForID(id, &info);
            if (res >= 0) {
                request.filesize = ((uint32_t *) &info.length)[1];
                request.fileoffset = ((uint32_t *) &info.offset)[1];
                loadFileIntoBuffer((title->titleId & 0xFFFFFFFF), "meta/iconTex.tga", gIconCache, sizeof(gIconCache));
            }

            strncpy(request.path, gFileInfos[id].path, 255);


            DEBUG_FUNCTION_LINE("Loading file %s size: %08X offset: %08X", request.path, request.filesize, request.fileoffset);

            DCFlushRange(&request, sizeof(LOAD_REQUEST));

            int mcpFd = IOS_Open("/dev/mcp", (IOSOpenMode) 0);
            if (mcpFd >= 0) {
                int out = 0;
                IOS_Ioctl(mcpFd, 100, &request, sizeof(request), &out, sizeof(out));
                IOS_Close(mcpFd);
            }
            return 0;
        } else {
            DEBUG_FUNCTION_LINE("Failed to get the id for titleID %016llX", title->titleId);
        }
    }

    int result = real_ACPCheckTitleLaunchByTitleListTypeEx(title, u2);
    return result;

}

DECL_FUNCTION(int, FSOpenFile, FSClient *client, FSCmdBlock *block, char *path, const char *mode, int *handle, int error) {
    const char *start = "/vol/storage_mlc01/sys/title/0005000F";
    const char *icon = ".tga";
    const char *iconTex = "iconTex.tga";
    const char *sound = ".btsnd";

    if (StringTools::EndsWith(path, icon) || StringTools::EndsWith(path, sound)) {
        if (strncmp(path, start, strlen(start)) == 0) {
            int res = FS_STATUS_NOT_FOUND;
            if (StringTools::EndsWith(path, iconTex)) {
                // fallback to dummy icon if loaded homebrew is no .wbf
                *handle = 0x13371338;
                res = FS_STATUS_OK;
            }

            uint32_t lowerTitleID;
            char *id = path + 1 + strlen(start);
            id[8] = 0;
            char *ending = id + 9;
            sscanf(id, "%08X", &lowerTitleID);
            int32_t idVal = getIDByLowerTitleID(lowerTitleID);
            if (idVal < 0) {
                DEBUG_FUNCTION_LINE("Failed to find id for titleID %08X", lowerTitleID);
            } else {
                if (FSOpenFile_for_ID(idVal, ending, handle) < 0) {
                    return res;
                }
            }
            return FS_STATUS_OK;
        } else if (gHomebrewLaunched) {
            if (StringTools::EndsWith(path, iconTex)) {
                *handle = 0x13371337;
                return FS_STATUS_OK;
            } else {
                DEBUG_FUNCTION_LINE("%s", path);
            }

        }
    }

    int result = real_FSOpenFile(client, block, path, mode, handle, error);
    return result;
}

DECL_FUNCTION(FSStatus, FSCloseFile, FSClient *client, FSCmdBlock *block, FSFileHandle handle, uint32_t flags) {
    if (handle == 0x13371337 || handle == 0x13371338) {
        return FS_STATUS_OK;
    }
    if ((handle & 0xFF000000) == 0xFF000000) {
        int32_t fd = (handle & 0x00000FFF);
        int32_t romid = (handle & 0x00FFF000) >> 12;
        DEBUG_FUNCTION_LINE("Close %d %d", fd, romid);
        DeInitFile(fd);
        if (gFileInfos[romid].openedFiles--) {
            if (gFileInfos[romid].openedFiles <= 0) {
                DEBUG_FUNCTION_LINE("unmount romfs no more handles");
                unmountRomfs(romid);
            }
        }
        //unmountAllRomfs();
        return FS_STATUS_OK;
    }
    return real_FSCloseFile(client, block, handle, flags);
}

DECL_FUNCTION(FSStatus, FSReadFile, FSClient *client, FSCmdBlock *block, uint8_t *buffer, uint32_t size, uint32_t count, FSFileHandle handle, uint32_t unk1, uint32_t flags) {
    if (handle == 0x13371337) {
        uint32_t cpySize = size * count;
        if (sizeof(gIconCache) < cpySize) {
            cpySize = sizeof(gIconCache);
        }
        memcpy(buffer, gIconCache, cpySize);
        return (FSStatus) (cpySize / size);
    } else if (handle == 0x13371338) {
        uint32_t cpySize = size * count;
        if (iconTex_tga_size < cpySize) {
            cpySize = iconTex_tga_size;
        }
        memcpy(buffer, iconTex_tga, cpySize);
        DEBUG_FUNCTION_LINE("DUMMY");
        return (FSStatus) (cpySize / size);
    }
    if ((handle & 0xFF000000) == 0xFF000000) {
        int32_t fd = (handle & 0x00000FFF);
        int32_t romid = (handle & 0x00FFF000) >> 12;

        DEBUG_FUNCTION_LINE("READ %d from %d rom: %d", size * count, fd, romid);

        int readSize = readFile(fd, buffer, (size * count));

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

DECL_FUNCTION(int32_t, _SYSLaunchTitleByPathFromLauncher, char *pathToLoad, uint32_t u2) {
    const char *start = "/custom/";
    if (strncmp(pathToLoad, start, strlen(start)) == 0) {
        strcpy(template_title.path, pathToLoad);
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
        u3 = (uint32_t) (titleID >> 32);
        u4 = (uint32_t) (0x00000000FFFFFFFF & titleID);
    }
    uint32_t result = real_ACPGetApplicationBox(u1, u2, u3, u4);
    return result;
}

DECL_FUNCTION(uint32_t, PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg, uint32_t *param) {
    if (param[2] == UPPER_TITLE_ID_HOMEBREW) {
        uint64_t titleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);
        param[2] = (uint32_t) (titleID >> 32);
        param[3] = (uint32_t) (0x00000000FFFFFFFF & titleID);
    }
    uint32_t result = real_PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg(param);
    return result;
}

DECL_FUNCTION(uint32_t, MCP_RightCheckLaunchable, uint32_t *u1, uint32_t *u2, uint32_t u3, uint32_t u4, uint32_t u5) {
    if (u3 == UPPER_TITLE_ID_HOMEBREW) {
        uint64_t titleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);
        u3 = (uint32_t) (titleID >> 32);
        u4 = (uint32_t) (0x00000000FFFFFFFF & titleID);
    }
    uint32_t result = real_MCP_RightCheckLaunchable(u1, u2, u3, u4, u5);
    return result;
}

DECL_FUNCTION(int32_t, HBM_NN_ACP_ACPGetTitleMetaXmlByDevice, uint32_t titleid_upper, uint32_t titleid_lower, ACPMetaXml *metaxml, uint32_t device) {
    if (gHomebrewLaunched) {
        memcpy(metaxml, &gLaunchXML, sizeof(gLaunchXML));
        return 0;
    }
    int result = real_HBM_NN_ACP_ACPGetTitleMetaXmlByDevice(titleid_upper, titleid_lower, metaxml, device);
    return result;
}


DECL_FUNCTION(uint32_t, ACPGetLaunchMetaData, struct _ACPMetaData *metadata) {
    uint32_t result = real_ACPGetLaunchMetaData(metadata);

    if (gHomebrewLaunched) {
        memcpy(metadata->bootmovie, bootMovie_h264, bootMovie_h264_size);
        memcpy(metadata->bootlogo, bootLogoTex_tga, bootLogoTex_tga_size);
    }

    return result;
}

WUPS_MUST_REPLACE_PHYSICAL_FOR_PROCESS(HBM_NN_ACP_ACPGetTitleMetaXmlByDevice, 0x2E36CE44, 0x0E36CE44, WUPS_FP_TARGET_PROCESS_HOME_MENU);
WUPS_MUST_REPLACE(ACPGetApplicationBox, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetApplicationBox);
WUPS_MUST_REPLACE(PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg, WUPS_LOADER_LIBRARY_DRMAPP, PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg);
WUPS_MUST_REPLACE(MCP_RightCheckLaunchable, WUPS_LOADER_LIBRARY_COREINIT, MCP_RightCheckLaunchable);

WUPS_MUST_REPLACE(FSReadFile, WUPS_LOADER_LIBRARY_COREINIT, FSReadFile);
WUPS_MUST_REPLACE(FSOpenFile, WUPS_LOADER_LIBRARY_COREINIT, FSOpenFile);
WUPS_MUST_REPLACE(FSCloseFile, WUPS_LOADER_LIBRARY_COREINIT, FSCloseFile);
WUPS_MUST_REPLACE(MCP_TitleList, WUPS_LOADER_LIBRARY_COREINIT, MCP_TitleList);
WUPS_MUST_REPLACE(MCP_GetTitleInfoByTitleAndDevice, WUPS_LOADER_LIBRARY_COREINIT, MCP_GetTitleInfoByTitleAndDevice);

WUPS_MUST_REPLACE(ACPCheckTitleLaunchByTitleListTypeEx, WUPS_LOADER_LIBRARY_NN_ACP, ACPCheckTitleLaunchByTitleListTypeEx);
WUPS_MUST_REPLACE(ACPGetTitleMetaXmlByDevice, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetTitleMetaXmlByDevice);
WUPS_MUST_REPLACE(ACPGetLaunchMetaXml, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetLaunchMetaXml);
WUPS_MUST_REPLACE(ACPGetTitleMetaDirByDevice, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetTitleMetaDirByDevice);
WUPS_MUST_REPLACE(_SYSLaunchTitleByPathFromLauncher, WUPS_LOADER_LIBRARY_SYSAPP, _SYSLaunchTitleByPathFromLauncher);
WUPS_MUST_REPLACE(ACPGetLaunchMetaData, WUPS_LOADER_LIBRARY_NN_ACP, ACPGetLaunchMetaData);
