#include <wups.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <coreinit/title.h>
#include <coreinit/cache.h>
#include <coreinit/systeminfo.h>
#include <coreinit/mcp.h>
#include <coreinit/filesystem.h>
#include <coreinit/systeminfo.h>
#include <coreinit/memorymap.h>
#include <coreinit/dynload.h>
#include <sysapp/title.h>
#include <nn/acp.h>
#include <nsysnet/socket.h>
#include <coreinit/ios.h>
#include <vpad/input.h>
#include <utils/logger.h>
#include <map>
#include <utils/utils.h>
#include <fs/DirList.h>

#define TARGET_WIDTH (854)
#define TARGET_HEIGHT (480)


WUPS_ALLOW_KERNEL()

void printVPADButtons(VPADStatus * buffer);

WUPS_PLUGIN_NAME("Vpad input logger");
WUPS_PLUGIN_DESCRIPTION("Prints information about vpad inputs and sensors");
WUPS_PLUGIN_VERSION("v1.0");
WUPS_PLUGIN_AUTHOR("Maschell");
WUPS_PLUGIN_LICENSE("GPL");
IOSHandle handles[100];


struct WUT_PACKED ACPMetaData {
    char bootmovie[80696];
    char bootlogo[28604];
};


extern "C" {
    extern void __init_wut_malloc();
    extern void __fini_wut_malloc();
}

WUPS_FS_ACCESS()

ACPMetaXml gLaunchXML __attribute__((section(".data")));
MCPTitleListType template_title __attribute__((section(".data")));
BOOL gHomebrewLaunched __attribute__((section(".data")));

INITIALIZE_PLUGIN() {
    memset((void*) &template_title,0,sizeof(template_title));
    memset((void*) &gLaunchXML,0,sizeof(gLaunchXML));
    gHomebrewLaunched = FALSE;
}

ON_APPLICATION_START(args) {
    __init_wut_malloc();
    socket_lib_init();
    log_init();

    if(_SYSGetSystemApplicationTitleId(APP_ID_Updater) == OSGetTitleID()) {
        gHomebrewLaunched = FALSE;
    }

}

ON_APPLICATION_ENDING() {
    __fini_wut_malloc();
}

void fillXmlForTitleID(uint32_t titleid_upper, uint32_t titleid_lower, ACPMetaXml* out_buf) {
    out_buf->title_id = ((uint64_t)titleid_upper * 0x100000000) + titleid_lower;
    // TODO
    char buffer [25];
    snprintf(buffer,25,"/custom/%08X%08X", titleid_upper,titleid_lower);
    DEBUG_FUNCTION_LINE("%s\n",buffer);
    strncpy(out_buf->longname_en,buffer,strlen(buffer));
    strncpy(out_buf->shortname_en,buffer,strlen(buffer));
    out_buf->e_manual = 1;
    out_buf->e_manual_version = 1;
    out_buf->region = 0xFFFFFFFF;
}

DECL_FUNCTION(int32_t, MCP_TitleList, uint32_t handle, uint32_t* outTitleCount, MCPTitleListType* titleList, uint32_t size) {
    int32_t result = real_MCP_TitleList(handle, outTitleCount, titleList, size);
    uint32_t titlecount = *outTitleCount;

    DirList dirList("sd:/wiiu/apps", ".rpx", DirList::Files | DirList::CheckSubfolders, 1);
    dirList.SortList();
    int j = 0;
    for(int i = 0; i < dirList.GetFilecount(); i++) {
        //! skip our own application in the listing
        if(strcasecmp(dirList.GetFilename(i), "homebrew_launcher.elf") == 0)
            continue;
        //! skip our own application in the listing
        if(strcasecmp(dirList.GetFilename(i), "homebrew_launcher.rpx") == 0)
            continue;

        //! skip hidden linux and mac files
        if(dirList.GetFilename(i)[0] == '.' || dirList.GetFilename(i)[0] == '_')
            continue;

        char buffer [25];
        snprintf(buffer,25,"/custom/%08X%08X", 0x0005000F, j);
        strcpy(template_title.path,buffer);

        const char * indexedDevice = "mlc";
        strcpy(template_title.indexedDevice,indexedDevice);
        template_title.appType = MCP_APP_TYPE_SystemApps; // for some reason we need to act like an system app.
        template_title.titleId = 0x0005000F00000000 + j;
        template_title.titleVersion = 1;
        template_title.groupId = 0x400;
        template_title.osVersion = OSGetOSID();
        template_title.sdkVersion = __OSGetProcessSDKVersion();
        template_title.unk0x60 = 1;

        memcpy(&(titleList[titlecount]), &template_title,sizeof(template_title));
        titlecount++;
        j++;
    }

    *outTitleCount = titlecount;

    for(int i = 0; i< *outTitleCount; i++) {
        DEBUG_FUNCTION_LINE("%s %08X %016llX %08X %08X %s \n",titleList[i].path, titleList[i].appType, titleList[i].osVersion, titleList[i].sdkVersion,titleList[i].groupId,titleList[i].indexedDevice);
        //dumpHex(&titleList[i],sizeof(MCPTitleListType));
    }

    return result;
}

DECL_FUNCTION(int32_t, MCP_GetTitleInfoByTitleAndDevice, uint32_t mcp_handle, uint32_t titleid_lower_1, uint32_t titleid_upper, uint32_t titleid_lower_2, uint32_t unknown, MCPTitleListType* title) {

    if(gHomebrewLaunched) {
        memcpy(title, &(template_title), sizeof(MCPTitleListType));
    } else if(titleid_upper ==  0x0005000F) {
        char buffer [25];
        snprintf(buffer,25,"/custom/%08X%08X", titleid_lower_2, titleid_upper);
        strcpy(template_title.path,buffer);
        template_title.titleId = 0x0005000F00000000 + titleid_lower_1;
        memcpy(title, &(template_title), sizeof(MCPTitleListType));
        DEBUG_FUNCTION_LINE("%s %08X %016llX %08X \n",title->path, title->appType, title->osVersion, title->sdkVersion);
        return 0;
    }
    int result = real_MCP_GetTitleInfoByTitleAndDevice(mcp_handle, titleid_lower_1, titleid_upper, titleid_lower_2, unknown, title);

    return result;
}


DECL_FUNCTION(int32_t, ACPCheckTitleLaunchByTitleListTypeEx, MCPTitleListType* title, uint32_t u2) {
    if(title->titleId & 0x0005000F00000000) {
        DEBUG_FUNCTION_LINE("Started homebrew\n");
        gHomebrewLaunched = TRUE;
        fillXmlForTitleID((title->titleId & 0xFFFFFFFF00000000) >> 32,(title->titleId & 0xFFFFFFFF), &gLaunchXML);
        return 0;
    }

    int result = real_ACPCheckTitleLaunchByTitleListTypeEx(title, u2);
    return result;

}

int EndsWith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

DECL_FUNCTION(void, FSInit) {
    socket_lib_init();
    log_init();
    OSDynLoad_Module sModuleHandle = NULL;
    OSDynLoad_Acquire("nn_acp.rpl", &sModuleHandle);
    void *functionptr = NULL;
    OSDynLoad_FindExport(sModuleHandle, FALSE, "ACPGetTitleMetaDir", &functionptr);
    DEBUG_FUNCTION_LINE("%08X %08X\n",functionptr, OSEffectiveToPhysical((uint32_t)functionptr));
    return real_FSInit();
}

DECL_FUNCTION(int, FSOpenFile, FSClient *pClient, FSCmdBlock *pCmd, char *path, const char *mode, int *handle, int error) {
    char * start = "/vol/storage_mlc01/sys/title/0005000F";
    char * icon = "iconTex.tga";

    if(EndsWith(path,icon)) {
        if(gHomebrewLaunched || strncmp(path,start,strlen(start)) == 0) {
            strcpy(path,"/vol/storage_mlc01/usr/title/00050000/10172000/meta/iconTex.tga");
        }
    }

    int result = real_FSOpenFile(pClient, pCmd, path, mode, handle, error);
    DEBUG_FUNCTION_LINE("%s! Result %08X %d\n",path,*handle, result);
    return result;
}

DECL_FUNCTION(FSStatus, FSReadFile, FSClient *client, FSCmdBlock *block, uint8_t *buffer, uint32_t size, uint32_t count, FSFileHandle handle,uint32_t unk1, uint32_t flags) {
    FSStatus result = real_FSReadFile(client, block, buffer, size, count, handle, unk1, flags);
    DEBUG_FUNCTION_LINE("%08X %d\n", handle, result);
    return result;
}

DECL_FUNCTION(FSStatus,FSBindMount,FSClient *client,
              FSCmdBlock *cmd,
              const char *source,
              const char *target,
              uint32_t flags) {
    FSStatus result = real_FSBindMount(client, cmd, source, target, flags);
    DEBUG_FUNCTION_LINE("%s %s %d\n", source,target, result);
    return result;
}

DECL_FUNCTION(FSStatus, FSReadDir, FSClient *client, FSCmdBlock *block,  FSDirectoryHandle handle, FSDirectoryEntry *entry, uint32_t flags) {
    FSStatus result = real_FSReadDir(client, block, handle, entry, flags);
    DEBUG_FUNCTION_LINE("%s %d\n", entry->name, result);
    return result;
}

DECL_FUNCTION(int32_t, ACPGetTitleMetaXmlByDevice, uint32_t titleid_upper, uint32_t titleid_lower, ACPMetaXml* out_buf, uint32_t device, uint32_t u1) {
    DEBUG_FUNCTION_LINE("\n");
    int result = real_ACPGetTitleMetaXmlByDevice(titleid_upper, titleid_lower, out_buf, device,u1);

    if(titleid_upper ==  0x00050000 && titleid_lower == 0x13374842) {
        //dumpHex((void*)out_buf,0x3500);
    }

    if(titleid_upper ==  0x0005000F) {
        fillXmlForTitleID(titleid_upper,titleid_lower, out_buf);
        result = 0;
    }

    DEBUG_FUNCTION_LINE("TitleID: %08X%08X res:%016llX device: %d %08X = %08X \n",titleid_upper,titleid_lower,out_buf->title_id, device,u1,result);
    return result;
}

DECL_FUNCTION(int32_t, ACPGetTitleMetaDirByDevice, uint32_t titleid_upper, uint32_t titleid_lower, char* out_buf, uint32_t size, int device) {
    DEBUG_FUNCTION_LINE("\n");
    int result = real_ACPGetTitleMetaDirByDevice(titleid_upper, titleid_lower, out_buf, size, device);

    if(titleid_upper ==  0x0005000F) {
        strcpy(out_buf,"/vol/storage_mlc01/usr/title/00050000/10119b00");
        return -1;
    }

    DEBUG_FUNCTION_LINE("TitleID: %08X%08X path:%s (%d)device: %d = %08X \n",titleid_upper,titleid_lower,out_buf,size, device,result);

    return result;
}


// You must free the result if result is non-NULL.
char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = (char*)malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

DECL_FUNCTION(int32_t, _SYSLaunchTitleByPathFromLauncher, char* pathToLoad, uint32_t u2) {
    DEBUG_FUNCTION_LINE("\n");
    const char * start = "/custom/";
    if(strncmp(pathToLoad,start,strlen(start)) == 0) {
        strcpy(template_title.path,pathToLoad);

        // always load H&S app
        strcpy(pathToLoad,"/vol/storage_mlc01/usr/title/00050000/10119b00");
    }

    int32_t result = real__SYSLaunchTitleByPathFromLauncher(pathToLoad, strlen(pathToLoad));

    DEBUG_FUNCTION_LINE("%s %08X result %08X \n",pathToLoad,u2,result);
    return result;
}

DECL_FUNCTION(int32_t, ACPGetLaunchMetaXml, ACPMetaXml * metaxml) {
    DEBUG_FUNCTION_LINE("\n");

    int result = real_ACPGetLaunchMetaXml(metaxml);
    if(gHomebrewLaunched) {
        memcpy(metaxml, &gLaunchXML, sizeof(gLaunchXML));
    }
    return result;
}

DECL_FUNCTION(int32_t, HBM_NN_ACP_ACPGetTitleMetaXmlByDevice, uint32_t titleid_upper, uint32_t titleid_lower, ACPMetaXml* metaxml, uint32_t device) {
    if(gHomebrewLaunched) {
        memcpy(metaxml, &gLaunchXML, sizeof(gLaunchXML));
        return 0;
    }
    int result = real_HBM_NN_ACP_ACPGetTitleMetaXmlByDevice(titleid_upper, titleid_lower, metaxml, device);

    return result;
}

DECL_FUNCTION(int32_t, EMANUAL_NN_ACP_ACPGetTitleMetaXml, uint32_t titleid_upper, uint32_t titleid_lower, ACPMetaXml* metaxml, uint32_t device) {
    DEBUG_FUNCTION_LINE("%08X %08X %08X %08X\n",titleid_upper,titleid_lower,metaxml,device);

    if(gHomebrewLaunched) {
        memcpy(metaxml, &gLaunchXML, sizeof(gLaunchXML));
        return 0;
    }
    int result = real_EMANUAL_NN_ACP_ACPGetTitleMetaXml(titleid_upper, titleid_lower, metaxml, device);

    return result;
}

DECL_FUNCTION(int32_t, EMANUAL_NN_ACP_ACPGetTitleMetaDir, uint32_t titleid_upper, uint32_t titleid_lower, ACPMetaXml* metaxml, uint32_t device) {
    DEBUG_FUNCTION_LINE("%08X %08X %08X %08X\n",titleid_upper,titleid_lower,metaxml,device);

    if(gHomebrewLaunched) {
        //memcpy(metaxml, &gLaunchXML, sizeof(gLaunchXML));
        //return 0;
    }
    int result = real_EMANUAL_NN_ACP_ACPGetTitleMetaDir(titleid_upper, titleid_lower, metaxml, device);

    return result;
}

WUPS_MUST_REPLACE_PHYSICAL(EMANUAL_NN_ACP_ACPGetTitleMetaDir,      0x4FA0A6F4, 0x0FA0A6F4);
WUPS_MUST_REPLACE_PHYSICAL(EMANUAL_NN_ACP_ACPGetTitleMetaXml,      0x4FA0C280, 0x0FA0C280);
WUPS_MUST_REPLACE_PHYSICAL(HBM_NN_ACP_ACPGetTitleMetaXmlByDevice,  0x2E36CE44, 0x0E36CE44);
WUPS_MUST_REPLACE(FSInit,                            WUPS_LOADER_LIBRARY_COREINIT,       FSInit);
WUPS_MUST_REPLACE(FSBindMount,                            WUPS_LOADER_LIBRARY_COREINIT,       FSBindMount);
WUPS_MUST_REPLACE(FSReadDir,                            WUPS_LOADER_LIBRARY_COREINIT,       FSReadDir);
WUPS_MUST_REPLACE(FSReadFile,                           WUPS_LOADER_LIBRARY_COREINIT,       FSReadFile);
WUPS_MUST_REPLACE(FSOpenFile,                           WUPS_LOADER_LIBRARY_COREINIT,       FSOpenFile);
WUPS_MUST_REPLACE(MCP_TitleList,                        WUPS_LOADER_LIBRARY_COREINIT,       MCP_TitleList);
WUPS_MUST_REPLACE(MCP_GetTitleInfoByTitleAndDevice,     WUPS_LOADER_LIBRARY_COREINIT,       MCP_GetTitleInfoByTitleAndDevice );
WUPS_MUST_REPLACE(ACPCheckTitleLaunchByTitleListTypeEx,   WUPS_LOADER_LIBRARY_NN_ACP,         ACPCheckTitleLaunchByTitleListTypeEx );
WUPS_MUST_REPLACE(ACPGetTitleMetaXmlByDevice,           WUPS_LOADER_LIBRARY_NN_ACP,         ACPGetTitleMetaXmlByDevice );
WUPS_MUST_REPLACE(ACPGetLaunchMetaXml,                  WUPS_LOADER_LIBRARY_NN_ACP,         ACPGetLaunchMetaXml );
WUPS_MUST_REPLACE(ACPGetTitleMetaDirByDevice,           WUPS_LOADER_LIBRARY_NN_ACP,         ACPGetTitleMetaDirByDevice );
WUPS_MUST_REPLACE(_SYSLaunchTitleByPathFromLauncher,    WUPS_LOADER_LIBRARY_SYSAPP,         _SYSLaunchTitleByPathFromLauncher);
