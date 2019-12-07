#include <wups.h>

#include <zlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <exception>
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
#include "romfs_dev.h"
#include "filelist.h"



WUPS_PLUGIN_NAME("Homebrew SysLauncher");
WUPS_PLUGIN_DESCRIPTION("Allows the user to load homebrew from the System Menu");
WUPS_PLUGIN_VERSION("0.1");
WUPS_PLUGIN_AUTHOR("Maschell");
WUPS_PLUGIN_LICENSE("GPL");

typedef struct WUT_PACKED FileInfos_ {
    char path[256];
    char name[256];
    int32_t source;
    bool romfsMounted;
    int openedFiles;
} FileInfos;


typedef struct{
    bool inUse;
    int fd;
    bool compressed;
    bool cInitDone;
    bool cCloseDone;
    z_stream strm;
    unsigned char in[0x1000];
} fileReadInformation;

#define FILE_INFO_SIZE  300
FileInfos gFileInfos[FILE_INFO_SIZE] __attribute__((section(".data")));
fileReadInformation gFileReadInformation[32] __attribute__((section(".data")));
ACPMetaXml gLaunchXML __attribute__((section(".data")));
MCPTitleListType template_title __attribute__((section(".data")));
BOOL gHomebrewLaunched __attribute__((section(".data")));


WUPS_USE_WUT_CRT()

INITIALIZE_PLUGIN() {
    memset((void*) &template_title,0,sizeof(template_title));
    memset((void*) &gLaunchXML,0,sizeof(gLaunchXML));
    memset((void*) &gFileInfos,0,sizeof(gFileInfos));
    memset((void*) &gFileReadInformation,0,sizeof(gFileReadInformation));
    gHomebrewLaunched = FALSE;
}

int EndsWith(const char *str, const char *suffix);

void closeAllStreams() {
    for(int i = 0; i<32; i++) {
        if(gFileReadInformation[i].inUse != false) {

        }
    }
}

int fileReadInformation_getSlot() {
    for(int i = 0; i<32; i++) {
        if(gFileReadInformation[i].inUse == false) {
            gFileReadInformation[i].inUse = true;
            return i;
        }
    }
    return -1;
}

void unmountRomfs(uint32_t id);

int EndsWith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

bool mountRomfs(uint32_t id) {
    if(id >= FILE_INFO_SIZE){
        DEBUG_FUNCTION_LINE("HANDLE WAS TOO BIG\n");
        return false;
    }
    if(!gFileInfos[id].romfsMounted) {
        char buffer [256];
        snprintf(buffer,256,"fs:/vol/external01/%s", gFileInfos[id].path);
        char romName [10];
        snprintf(romName,10,"%08X", id);
        DEBUG_FUNCTION_LINE("Mount %s as %s\n", buffer, romName);
        if(romfsMount(romName,buffer) == 0) {
            DEBUG_FUNCTION_LINE("Mounted successfully \n");
            gFileInfos[id].romfsMounted = true;
            return true;
        } else {
            DEBUG_FUNCTION_LINE("Mounting failed\n");
            return false;
        }
    }
    return true;
}



void listdir(const char *name, int indent) {
    DEBUG_FUNCTION_LINE("Reading %s\n",name);
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(name)))
        return;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            char path[1024];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            snprintf(path, sizeof(path), "%s%s", name, entry->d_name);
            DEBUG_FUNCTION_LINE("%*s[%s]\n", indent, "", entry->d_name);
            listdir(path, indent + 2);
        } else {
            DEBUG_FUNCTION_LINE("%*s- %s\n", indent, "", entry->d_name);
        }
    }
    closedir(dir);
}

ON_APPLICATION_START(args) {
    socket_lib_init();
    log_init();
    DEBUG_FUNCTION_LINE("IN PLUGIN\n");

    if(_SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY) != OSGetTitleID()) {
        DEBUG_FUNCTION_LINE("gHomebrewLaunched to FALSE\n");
        gHomebrewLaunched = FALSE;
    }
}

void unmountAllRomfs();
ON_APPLICATION_END() {
    closeAllStreams();
    unmountAllRomfs();
}

void fillXmlForTitleID(uint32_t titleid_upper, uint32_t titleid_lower, ACPMetaXml* out_buf) {
    if(titleid_lower >= FILE_INFO_SIZE){
        return;
    }
    out_buf->title_id = ((uint64_t)titleid_upper * 0x100000000) + titleid_lower;
    strncpy(out_buf->longname_en,gFileInfos[titleid_lower].name,511);
    strncpy(out_buf->shortname_en,gFileInfos[titleid_lower].name,255);
    strncpy(out_buf->publisher_en,gFileInfos[titleid_lower].name,255);
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
    out_buf->reserved_flag0  = 0x00010001;
    out_buf->reserved_flag6  = 0x00000003;
    out_buf->pc_usk    = 128;
    strncpy(out_buf->product_code,"WUP-P-HBLD",strlen("WUP-P-HBLD"));
    strncpy(out_buf->content_platform,"WUP",strlen("WUP"));
    strncpy(out_buf->company_code,"0001",strlen("0001"));
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

DECL_FUNCTION(int32_t, MCP_TitleList, uint32_t handle, uint32_t* outTitleCount, MCPTitleListType* titleList, uint32_t size) {
    int32_t result = real_MCP_TitleList(handle, outTitleCount, titleList, size);
    uint32_t titlecount = *outTitleCount;

    DirList dirList("fs:/vol/external01/wiiu/apps", ".rpx,.wbf", DirList::Files | DirList::CheckSubfolders, 1);
    dirList.SortList();

    int j = 0;
    for(int i = 0; i < dirList.GetFilecount(); i++) {
        if(j >= FILE_INFO_SIZE){
            DEBUG_FUNCTION_LINE("TOO MANY TITLES\n");
            break;
        }
        //! skip our own application in the listing
        if(strcasecmp(dirList.GetFilename(i), "homebrew_launcher.rpx") == 0) {
            continue;
        }
        //! skip our own application in the listing
        if(strcasecmp(dirList.GetFilename(i), "temp.rpx") == 0) {
            continue;
        }

        //! skip hidden linux and mac files
        if(dirList.GetFilename(i)[0] == '.' || dirList.GetFilename(i)[0] == '_') {
            continue;
        }

        char buffer [25];
        snprintf(buffer,25,"/custom/%08X%08X", 0x0005000F, j);
        strcpy(template_title.path,buffer);

        char * repl = (char*)"fs:/vol/external01/";
        char * with = (char*)"";
        char * input = (char*) dirList.GetFilepath(i);

        char * path = str_replace(input,repl, with);
        if(path != NULL) {
            strncpy(gFileInfos[j].path,path, 255);
            free(path);
        }

        strncpy(gFileInfos[j].name, dirList.GetFilename(i),255);
        gFileInfos[j].source = 0; //SD Card;

        DEBUG_FUNCTION_LINE("[%d] %s\n",j, gFileInfos[j].path);

        const char * indexedDevice = "mlc";
        strcpy(template_title.indexedDevice,indexedDevice);
        if(EndsWith(gFileInfos[j].name, ".wbf")) {

            template_title.appType = MCP_APP_TYPE_GAME;
        } else {
            // System apps don't have a splash screen.
            template_title.appType = MCP_APP_TYPE_SYSTEM_APPS;
        }
        template_title.titleId = 0x0005000F00000000 + j;
        template_title.titleVersion = 1;
        template_title.groupId = 0x400;

        template_title.osVersion = OSGetOSID();
        template_title.sdkVersion = __OSGetProcessSDKVersion();
        template_title.unk0x60 = 0;

        memcpy(&(titleList[titlecount]), &template_title,sizeof(template_title));

        titlecount++;
        j++;
    }

    *outTitleCount = titlecount;

    return result;
}

DECL_FUNCTION(int32_t, MCP_GetTitleInfoByTitleAndDevice, uint32_t mcp_handle, uint32_t titleid_lower_1, uint32_t titleid_upper, uint32_t titleid_lower_2, uint32_t unknown, MCPTitleListType* title) {
    if(gHomebrewLaunched) {
        memcpy(title, &(template_title), sizeof(MCPTitleListType));
    } else if(titleid_upper ==  0x0005000F) {
        char buffer [25];
        snprintf(buffer,25,"/custom/%08X%08X", titleid_upper, titleid_lower_2);
        strcpy(template_title.path,buffer);
        template_title.titleId = 0x0005000F00000000 + titleid_lower_1;
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
}
LOAD_REQUEST;

int32_t getRPXInfoForID(uint32_t id, romfs_fileInfo * info);

DECL_FUNCTION(int32_t, ACPCheckTitleLaunchByTitleListTypeEx, MCPTitleListType* title, uint32_t u2) {
    if((title->titleId & 0x0005000F00000000) == 0x0005000F00000000 && (uint32_t)(title->titleId & 0xFFFFFFFF) < FILE_INFO_SIZE) {
        DEBUG_FUNCTION_LINE("Started homebrew\n");
        gHomebrewLaunched = TRUE;
        fillXmlForTitleID((title->titleId & 0xFFFFFFFF00000000) >> 32,(title->titleId & 0xFFFFFFFF), &gLaunchXML);

        LOAD_REQUEST request;
        memset(&request, 0, sizeof(request));

        request.command = 0xFC; // IPC_CUSTOM_LOAD_CUSTOM_RPX;
        request.target = 0;     // LOAD_FILE_TARGET_SD_CARD
        request.filesize = 0;   // unknown
        request.fileoffset = 0; //

        romfs_fileInfo info;
        int res = getRPXInfoForID((title->titleId & 0xFFFFFFFF),&info);
        if(res >= 0) {
            request.filesize = ((uint32_t*)&info.length)[1];
            request.fileoffset = ((uint32_t*)&info.offset)[1];
        }

        strncpy(request.path, gFileInfos[(uint32_t)(title->titleId & 0xFFFFFFFF)].path, 255);

        DEBUG_FUNCTION_LINE("Loading file %s size: %08X offset: %08X\n", request.path, request.filesize, request.fileoffset);

        DCFlushRange(&request, sizeof(LOAD_REQUEST));

        int mcpFd = IOS_Open("/dev/mcp", (IOSOpenMode)0);
        if(mcpFd >= 0) {
            int out = 0;
            IOS_Ioctl(mcpFd, 100, &request, sizeof(request), &out, sizeof(out));
            IOS_Close(mcpFd);
        }
        return 0;
    }

    int result = real_ACPCheckTitleLaunchByTitleListTypeEx(title, u2);
    return result;

}


void unmountRomfs(uint32_t id) {
    if(id >= FILE_INFO_SIZE){
        return;
    }
    if(gFileInfos[id].romfsMounted) {
        char romName [10];
        snprintf(romName,10,"%08X", id);
        DEBUG_FUNCTION_LINE("Unmounting %s\n", romName);
        int res = romfsUnmount(romName);
        DEBUG_FUNCTION_LINE("res: %d\n",res);
        gFileInfos[id].romfsMounted = false;
    }
}

void unmountAllRomfs() {
    for(int i = 0; i < FILE_INFO_SIZE; i++) {
        unmountRomfs(i);
    }
}


bool file_exist (char *filename) {
    struct stat   buffer;
    return (stat (filename, &buffer) == 0);
}


bool initCompressedFileReadInformation(fileReadInformation * info){
    if(info == NULL || !info->compressed){
        info->cInitDone = false;
        return false;
    }
    if(info->cInitDone){
        return true;
    }
    /* allocate inflate state */
    info->strm.zalloc = Z_NULL;
    info->strm.zfree = Z_NULL;
    info->strm.opaque = Z_NULL;
    info->strm.avail_in = 0;
    info->strm.next_in = Z_NULL;
    int ret = inflateInit2(&info->strm, MAX_WBITS | 16); //gzip
    if (ret != Z_OK){
        DEBUG_FUNCTION_LINE("ret != Z_OK\n");
        info->cInitDone = false;
        return false;
    }
    info->cInitDone = true;
    return true;
}

int32_t FSOpenFile_for_ID(uint32_t id, const char * filepath, int * handle) {
    if(!mountRomfs(id)) {
        return -1;
    }
    char romName [10];
    snprintf(romName,10,"%08X", id);

    char * test = (char *) malloc(strlen(filepath)+1);
    char last = 0;
    int j = 0;
    for(int i = 0; filepath[i] != 0; i++) {
        if(filepath[i] == '/' ) {
            if(filepath[i] != last) {
                test[j++] = filepath[i];
            }
        } else {
            test[j++] = filepath[i];
        }
        last = filepath[i];
    }
    test[j++] = 0;

    char buffer [256];
    snprintf(buffer,256,"%s:/%s.gz",romName, test);

    bool nonCompressed = false;
    if(!file_exist(buffer)) {
        snprintf(buffer,256,"%s:/%s",romName, test);
        if(!file_exist(buffer)) {
            return -3;
        }
        nonCompressed = true;
    }

    int fd = open(buffer,0);
    if(fd >= 0) {
        DEBUG_FUNCTION_LINE("Opened %s from %s \n",buffer, romName );
        int slot = fileReadInformation_getSlot();
        if(slot < 0){
            return -5;
        }
        fileReadInformation * info = &gFileReadInformation[slot];
        info->fd = fd;
        if(!nonCompressed){
            info->compressed = true;
            initCompressedFileReadInformation(info);
            DEBUG_FUNCTION_LINE("Init compressed, got slot %d\n", slot);
        }else{
            info->cInitDone = true;
        }
        *handle = 0xFF000000 + slot;
        return 0;
    }


    /*
    DEBUG_FUNCTION_LINE("OPENING %s\n", buffer);

    std::istream* is = new zstr::ifstream(buffer);
     DEBUG_FUNCTION_LINE("OPENED %s\n", buffer);
    if(is != NULL) {
        for(int i = 0; i<100; i++) {
            if(gFileHandleToStream[i] == NULL) {
                DEBUG_FUNCTION_LINE("Set stream to id %d\n",i);
                gFileHandleToStream[i] = is;
                *handle = 0xFF000000 + i;
                return 0;
            }
        }
    }
    DEBUG_FUNCTION_LINE("Failed to find handle \n");*/

    return -2;
}

int32_t getRPXInfoForID(uint32_t id, romfs_fileInfo * info) {
    if(!mountRomfs(id)) {
        return -1;
    }
    DIR *dir;
    struct dirent *entry;
    char romName [10];
    snprintf(romName,10,"%08X", id);

    char root[12];
    snprintf(root,12,"%08X:/", id);

    if (!(dir = opendir(root))) {
        return -2;
    }
    bool found = false;
    int res = -3;
    while ((entry = readdir(dir)) != NULL) {
        if(EndsWith(entry->d_name, ".rpx")) {
            if(romfs_GetFileInfoPerPath(romName, entry->d_name, info) >= 0) {
                found = true;
                res = 0;
            }
            break;
        }
    }

    closedir(dir);

    if(!found) {
        return -4;
    }
    return res;
}

DECL_FUNCTION(int, FSOpenFile, FSClient *client, FSCmdBlock *block, char *path, const char *mode, int *handle, int error) {
    char * start = "/vol/storage_mlc01/sys/title/0005000F";
    char * icon = ".tga";
    char * iconTex = "iconTex.tga";
    char * sound = ".btsnd";

    if(EndsWith(path,icon) || EndsWith(path,sound)) {
        if(strncmp(path,start,strlen(start)) == 0/* || (gHomebrewLaunched && EndsWith(path,iconTex))*/) {
            int res = FS_STATUS_NOT_FOUND;
            if(EndsWith(path,iconTex)) {
                // fallback to dummy icon if loaded homebrew is no .wbf
                //*handle = 0x1337;
                res = FS_STATUS_NOT_FOUND;
            }

            uint32_t val;
            char * id = path+1+strlen(start);
            id[8] = 0;
            char * ending = id+9;
            sscanf(id,"%08X", &val);
            if(FSOpenFile_for_ID(val, ending, handle) < 0) {
                return res;
            }
            return FS_STATUS_OK;
        }
    }

    int result = real_FSOpenFile(client, block, path, mode, handle, error);
    return result;
}

bool DeInitFile(int slot){
    if(gFileReadInformation[slot].compressed && gFileReadInformation[slot].cInitDone){
        /* clean up and return */
        (void)inflateEnd(&(gFileReadInformation[slot].strm));
    }

    close(gFileReadInformation[slot].fd);
    gFileReadInformation[slot].inUse = false;
    memset(&gFileReadInformation[slot],0, sizeof(fileReadInformation));
    return true;
}


DECL_FUNCTION(FSStatus, FSCloseFile, FSClient *client, FSCmdBlock *block, FSFileHandle handle, uint32_t flags) {
    if(handle == 0x1337) {
        //return FS_STATUS_OK;
    }
    if((handle & 0xFF000000) == 0xFF000000) {
        int32_t fd = (handle & 0x00FFFFFF);
        DEBUG_FUNCTION_LINE("Close %d\n", fd);
        DeInitFile(fd);
        unmountAllRomfs();
        return FS_STATUS_OK;
    }
    return real_FSCloseFile(client,block,handle,flags);
}


bool initFile(int slot){
    if(gFileReadInformation[slot].compressed && !gFileReadInformation[slot].cInitDone){
        initCompressedFileReadInformation(&gFileReadInformation[slot]);
        if(!gFileReadInformation[slot].cInitDone){
            DEBUG_FUNCTION_LINE("Failed to init\n");
            return false;
        }
    }
    return true;
}



int readFile(int slot, uint8_t * buffer, uint32_t size){
    fileReadInformation * info = &gFileReadInformation[slot];
    if(!info->compressed){
        //DEBUG_FUNCTION_LINE("non compressed\n");
        return read(info->fd, buffer, size);
    }else{
        int startValue = info->strm.total_out;
        int newSize = 0;
        int ret = 0;
        //DEBUG_FUNCTION_LINE("We want to read %d\n", size);
        //DEBUG_FUNCTION_LINE("startValue %d \n",startValue);
        do {
            int CHUNK = 0x1000;
            int nextOut = CHUNK;
            if(nextOut > size){
                nextOut = size;
            }
            //DEBUG_FUNCTION_LINE("nextOut = %d\n",nextOut);
            if(info->strm.avail_in == 0){
                //DEBUG_FUNCTION_LINE("Reading %d from compressed stream\n",CHUNK);
                info->strm.avail_in = read(info->fd, info->in, CHUNK);
                if (info->strm.avail_in == 0){
                    DEBUG_FUNCTION_LINE("strm.avail_in is 0\n");
                    break;
                }
                info->strm.next_in = info->in;
            }
            //DEBUG_FUNCTION_LINE("info->strm.avail_in = %d\n",info->strm.avail_in);
            //DEBUG_FUNCTION_LINE("info->strm.next_in = %d\n",info->strm.next_in);

            /* run inflate() on input until output buffer not full */
            do {
                //DEBUG_FUNCTION_LINE("newSize %d, size %d, info->strm.avail_out %d\n", newSize, size, info->strm.avail_out);

                if(nextOut > size - newSize){
                    nextOut = size - newSize;
                }

                info->strm.avail_out = nextOut;
                //DEBUG_FUNCTION_LINE("info->strm.avail_out = %d\n",info->strm.avail_out);
                info->strm.next_out = (buffer + newSize);
                //DEBUG_FUNCTION_LINE("info->strm.next_out = %08X\n",info->strm.next_out);
                ret = inflate(&info->strm, Z_NO_FLUSH);
                //DEBUG_FUNCTION_LINE("ret = %d\n",ret);
                if(ret == Z_STREAM_ERROR){
                    DEBUG_FUNCTION_LINE("Z_STREAM_ERROR\n");
                    return (FSStatus)0;
                }

                switch (ret) {
                case Z_NEED_DICT:
                    DEBUG_FUNCTION_LINE("Z_NEED_DICT\n");
                    ret = Z_DATA_ERROR;     /* and fall through */
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    DEBUG_FUNCTION_LINE("Z_MEM_ERROR or Z_DATA_ERROR\n");
                    (void)inflateEnd(&info->strm);
                    return (FSStatus) ret;
                }

                int canBeWritten = CHUNK - info->strm.avail_out;
                //DEBUG_FUNCTION_LINE("canBeWritten = %d\n",canBeWritten);

                newSize = info->strm.total_out - startValue;
                if(newSize == size){
                    //DEBUG_FUNCTION_LINE("newSize was as wanted %d\n", newSize);
                    break;
                }
                nextOut = CHUNK;
                if(newSize + nextOut >= (size)){
                    nextOut = (size) - newSize;
                }
                //DEBUG_FUNCTION_LINE("newSize = %d\n",newSize);
                //DEBUG_FUNCTION_LINE("nextOut = %d\n",nextOut);
            } while (info->strm.avail_out == 0 && newSize < (size));

            /* done when inflate() says it's done */
        } while (ret != Z_STREAM_END && newSize < size);

        return newSize;
    }
}


DECL_FUNCTION(FSStatus, FSReadFile, FSClient *client, FSCmdBlock *block, uint8_t *buffer, uint32_t size, uint32_t count, FSFileHandle handle,uint32_t unk1, uint32_t flags) {
    if(handle == 0x1337) {
        int cpySize = size*count;
        if(iconTex_tga_size < cpySize) {
            cpySize = iconTex_tga_size;
        }
        memcpy(buffer, iconTex_tga, cpySize);
        return (FSStatus)(cpySize/size);
    }
    if((handle & 0xFF000000) == 0xFF000000) {
        int32_t fd = (handle & 0x00FFFFFF);

        initFile(fd);
        int readSize = readFile(fd, buffer, (size*count));

        //DEBUG_FUNCTION_LINE("READ %d from %d result %d\n", size*count, fd, readSize);
        return (FSStatus)(readSize / size);
    }
    FSStatus result = real_FSReadFile(client, block, buffer, size, count, handle, unk1, flags);
    return result;
}

DECL_FUNCTION(int32_t, ACPGetTitleMetaXmlByDevice, uint32_t titleid_upper, uint32_t titleid_lower, ACPMetaXml* out_buf, uint32_t device, uint32_t u1) {
    int result = real_ACPGetTitleMetaXmlByDevice(titleid_upper, titleid_lower, out_buf, device,u1);
    if(titleid_upper ==  0x0005000F) {
        fillXmlForTitleID(titleid_upper,titleid_lower, out_buf);
        result = 0;
    }
    return result;
}

DECL_FUNCTION(int32_t, ACPGetTitleMetaDirByDevice, uint32_t titleid_upper, uint32_t titleid_lower, char* out_buf, uint32_t size, int device) {
    if(titleid_upper ==  0x0005000F) {
        snprintf(out_buf,53,"/vol/storage_mlc01/sys/title/%08X/%08X/meta", titleid_upper, titleid_lower);
        return 0;
    }
    int result = real_ACPGetTitleMetaDirByDevice(titleid_upper, titleid_lower, out_buf, size, device);
    return result;
}



DECL_FUNCTION(int32_t, _SYSLaunchTitleByPathFromLauncher, char* pathToLoad, uint32_t u2) {
    const char * start = "/custom/";
    if(strncmp(pathToLoad,start,strlen(start)) == 0) {
        strcpy(template_title.path,pathToLoad);
        uint64_t titleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);
        snprintf(pathToLoad,47,"/vol/storage_mlc01/sys/title/%08x/%08x", (uint32_t) (titleID >> 32), (uint32_t) (0x00000000FFFFFFFF & titleID));
    }

    int32_t result = real__SYSLaunchTitleByPathFromLauncher(pathToLoad, strlen(pathToLoad));
    return result;
}

DECL_FUNCTION(int32_t, ACPGetLaunchMetaXml, ACPMetaXml * metaxml) {
    int result = real_ACPGetLaunchMetaXml(metaxml);
    if(gHomebrewLaunched) {
        memcpy(metaxml, &gLaunchXML, sizeof(gLaunchXML));
    }
    return result;
}

DECL_FUNCTION(uint32_t, ACPGetApplicationBox,uint32_t * u1, uint32_t * u2, uint32_t u3, uint32_t u4) {
    if(u3 == 0x0005000F) {
        uint64_t titleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);
        u3 = (uint32_t) (titleID >> 32);
        u4 = (uint32_t) (0x00000000FFFFFFFF & titleID);
    }
    uint32_t result = real_ACPGetApplicationBox(u1,u2,u3,u4);
    return result;
}

DECL_FUNCTION(uint32_t, PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg, uint32_t * param ) {
    if(param[2] == 0x0005000F) {
        uint64_t titleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);
        param[2] = (uint32_t) (titleID >> 32);
        param[3] = (uint32_t) (0x00000000FFFFFFFF & titleID);
    }
    uint32_t result = real_PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg(param);
    return result;
}

DECL_FUNCTION(uint32_t, MCP_RightCheckLaunchable, uint32_t * u1, uint32_t * u2, uint32_t u3, uint32_t u4, uint32_t u5) {
    if(u3 == 0x0005000F) {
        uint64_t titleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_HEALTH_AND_SAFETY);
        u3 = (uint32_t) (titleID >> 32);
        u4 = (uint32_t) (0x00000000FFFFFFFF & titleID);
    }
    uint32_t result = real_MCP_RightCheckLaunchable(u1,u2,u3,u4,u5);
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

WUPS_MUST_REPLACE_PHYSICAL(HBM_NN_ACP_ACPGetTitleMetaXmlByDevice,   0x2E36CE44,                         0x0E36CE44);
WUPS_MUST_REPLACE(ACPGetApplicationBox,                             WUPS_LOADER_LIBRARY_NN_ACP,         ACPGetApplicationBox );
WUPS_MUST_REPLACE(PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg,     WUPS_LOADER_LIBRARY_DRMAPP,         PatchChkStart__3RplFRCQ3_2nn6drmapp8StartArg );
WUPS_MUST_REPLACE(MCP_RightCheckLaunchable,                         WUPS_LOADER_LIBRARY_COREINIT,       MCP_RightCheckLaunchable );

WUPS_MUST_REPLACE(FSReadFile,                                       WUPS_LOADER_LIBRARY_COREINIT,       FSReadFile);
WUPS_MUST_REPLACE(FSOpenFile,                                       WUPS_LOADER_LIBRARY_COREINIT,       FSOpenFile);
WUPS_MUST_REPLACE(FSCloseFile,                                      WUPS_LOADER_LIBRARY_COREINIT,       FSCloseFile);
WUPS_MUST_REPLACE(MCP_TitleList,                                    WUPS_LOADER_LIBRARY_COREINIT,       MCP_TitleList);
WUPS_MUST_REPLACE(MCP_GetTitleInfoByTitleAndDevice,                 WUPS_LOADER_LIBRARY_COREINIT,       MCP_GetTitleInfoByTitleAndDevice );

WUPS_MUST_REPLACE(ACPCheckTitleLaunchByTitleListTypeEx,             WUPS_LOADER_LIBRARY_NN_ACP,         ACPCheckTitleLaunchByTitleListTypeEx );
WUPS_MUST_REPLACE(ACPGetTitleMetaXmlByDevice,                       WUPS_LOADER_LIBRARY_NN_ACP,         ACPGetTitleMetaXmlByDevice );
WUPS_MUST_REPLACE(ACPGetLaunchMetaXml,                              WUPS_LOADER_LIBRARY_NN_ACP,         ACPGetLaunchMetaXml );
WUPS_MUST_REPLACE(ACPGetTitleMetaDirByDevice,                       WUPS_LOADER_LIBRARY_NN_ACP,         ACPGetTitleMetaDirByDevice );
WUPS_MUST_REPLACE(_SYSLaunchTitleByPathFromLauncher,                WUPS_LOADER_LIBRARY_SYSAPP,         _SYSLaunchTitleByPathFromLauncher);
