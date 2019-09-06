#include <wups.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <coreinit/systeminfo.h>
#include <coreinit/mcp.h>
#include <coreinit/filesystem.h>
#include <nsysnet/socket.h>
#include <coreinit/ios.h>
#include <vpad/input.h>
#include <utils/logger.h>
#include <map>
#include <utils/utils.h>
#include <fs/DirList.h>

#define TARGET_WIDTH (854)
#define TARGET_HEIGHT (480)


void printVPADButtons(VPADStatus * buffer);

WUPS_PLUGIN_NAME("Vpad input logger");
WUPS_PLUGIN_DESCRIPTION("Prints information about vpad inputs and sensors");
WUPS_PLUGIN_VERSION("v1.0");
WUPS_PLUGIN_AUTHOR("Maschell");
WUPS_PLUGIN_LICENSE("GPL");
IOSHandle handles[100];


struct WUT_PACKED _ACPMetaData{
   char bootmovie[80696];
   char bootlogo[28604];
   void * test;
};

struct WUT_PACKED _ACPMetaXml{
    uint64_t title_id; // 0x0C
    uint64_t boss_id; // 0x0E
    uint64_t os_version; // 0x0F
    uint64_t app_size; // 0x10
    uint64_t common_save_size; // 0x11
    uint64_t account_save_size; // 0x12
    uint64_t common_boss_size; // 0x13
    uint64_t account_boss_size; // 0x14
    uint64_t join_game_mode_mask; // 0x17
    uint32_t version; // 0x01
    char product_code[32]; // AAAAA
    char content_platform[32]; // BBBBB
    char company_code[8]; // CCCCC
    char mastering_date[32]; // DDDDD
    uint32_t logo_type; // 0x02
    uint32_t app_launch_type; // 0x03
    uint32_t invisible_flag; // 0x04
    uint32_t no_managed_flag; // 0x05
    uint32_t no_event_log; // 0x06
    uint32_t no_icon_database; // 0x07
    uint32_t launching_flag; // 0x08
    uint32_t install_flag; // 0x09
    uint32_t closing_msg; // 0x0A
    uint32_t title_version; // 0x0B
    uint32_t group_id; // 0x0D
    uint32_t save_no_rollback; // 0x15
    uint32_t bg_daemon_enable; //0x18    
    uint32_t join_game_id; // 0x16
    uint32_t olv_accesskey; // 0x19
    uint32_t wood_tin; // 0x1A
    uint32_t e_manual; // 0x1B
    uint32_t e_manual_version; // 0x1C
    uint32_t region; // 0x1D
    uint32_t pc_cero; // 0x1E
    uint32_t pc_esrb; // 0x1F
    uint32_t pc_bbfc; // 0x20
    uint32_t pc_usk; // 0x21
    uint32_t pc_pegi_gen; // 0x22
    uint32_t pc_pegi_fin; // 0x23
    uint32_t pc_pegi_prt; // 0x24
    uint32_t pc_pegi_bbfc; // 0x25
    uint32_t pc_cob; // 0x26
    uint32_t pc_grb; // 0x27
    uint32_t pc_cgsrr; // 0x28
    uint32_t pc_oflc; // 0x29
    uint32_t pc_reserved0; // 0x2A
    uint32_t pc_reserved1; // 0x2B
    uint32_t pc_reserved2; // 0x2C
    uint32_t pc_reserved3; // 0x2D
    uint32_t ext_dev_nunchaku; // 0x2E
    uint32_t ext_dev_classic; // 0x2F
    uint32_t ext_dev_urcc; // 0x30
    uint32_t ext_dev_board; // 0x31
    uint32_t ext_dev_usb_keyboard; // 0x32
    uint32_t ext_dev_etc; // 0x33
    char ext_dev_etc_name[512]; // EEEE
    uint32_t eula_version; // 0x34
    uint32_t drc_use; // 0x35
    uint32_t network_use; // 0x36
    uint32_t online_account_use; // 0x37
    uint32_t direct_boot; // 0x38
    uint32_t reserved_flag0; // 0x39
    uint32_t reserved_flag1; // 0x3A
    uint32_t reserved_flag2; // 0x3B
    uint32_t reserved_flag3; // 0x3C
    uint32_t reserved_flag4; // 0x3D
    uint32_t reserved_flag5; // 0x3E
    uint32_t reserved_flag6; // 0x3F
    uint32_t reserved_flag7; // 0x40
    char longname_ja[512]; // FF
    char longname_en[512]; // HH
    char longname_fr[512]; // II
    char longname_de[512]; // JJ
    char longname_it[512]; // KK
    char longname_es[512]; // L
    char longname_zhs[512]; // M
    char longname_ko[512]; // N
    char longname_nl[512]; // O
    char longname_pt[512]; // P
    char longname_ru[512]; // Q
    char longname_zht[512]; // R
    char shortname_ja[256]; // S
    char shortname_en[256]; //   T
    char shortname_fr[256]; // U
    char shortname_de[256]; //  V   
    char shortname_it[256]; // W
    char shortname_es[256]; // X
    char shortname_zhs[256]; // Y
    char shortname_ko[256]; // Z
    char shortname_nl[256]; // 11
    char shortname_pt[256]; // 22
    char shortname_ru[256]; // 33
    char shortname_zht[256]; // 44
    char publisher_ja[256]; // 55
    char publisher_en[256]; // 66
    char publisher_fr[256]; // 77
    char publisher_de[256]; // 88
    char publisher_it[256]; // 99
    char publisher_es[256]; // 1010
    char publisher_zhs[256]; // 1212
    char publisher_ko[256]; // 1313
    char publisher_nl[256]; // 1414
    char publisher_pt[256]; // 1515
    char publisher_ru[256]; // 1616
    char publisher_zht[256]; // 1717
    uint32_t add_on_unique_id0; // 0x41
    uint32_t add_on_unique_id1; // 0x42
    uint32_t add_on_unique_id2; // 0x43
    uint32_t add_on_unique_id3; // 0x44
    uint32_t add_on_unique_id4; // 0x45
    uint32_t add_on_unique_id5; // 0x46
    uint32_t add_on_unique_id6; // 0x47
    uint32_t add_on_unique_id7; // 0x48
    uint32_t add_on_unique_id8; // 0x49
    uint32_t add_on_unique_id9; // 0x4A
    uint32_t add_on_unique_id10; // 0x4B
    uint32_t add_on_unique_id11; // 0x4C
    uint32_t add_on_unique_id12; // 0x4D
    uint32_t add_on_unique_id13; // 0x4E
    uint32_t add_on_unique_id14; // 0x4F
    uint32_t add_on_unique_id15; // 0x50
    uint32_t add_on_unique_id16; // 0x51
    uint32_t add_on_unique_id17; // 0x52
    uint32_t add_on_unique_id18; // 0x53
    uint32_t add_on_unique_id19; // 0x54
    uint32_t add_on_unique_id20; // 0x55
    uint32_t add_on_unique_id21; // 0x56
    uint32_t add_on_unique_id22; // 0x57
    uint32_t add_on_unique_id23; // 0x58
    uint32_t add_on_unique_id24; // 0x59
    uint32_t add_on_unique_id25; // 0x5A    
    uint32_t add_on_unique_id26; // 0x5B
    uint32_t add_on_unique_id27; // 0x5C
    uint32_t add_on_unique_id28; // 0x5D
    uint32_t add_on_unique_id29; // 0x5E
    uint32_t add_on_unique_id30; // 0x5F
    uint32_t add_on_unique_id31; // 0x60   
};
WUT_CHECK_OFFSET(_ACPMetaXml, 0x00, title_id);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x08, boss_id);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x10, os_version);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x18, app_size);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x20, common_save_size);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x28, account_save_size);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x30, common_boss_size);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x38, account_boss_size);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x40, join_game_mode_mask);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x48, version);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x4C, product_code);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x6C, content_platform);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x8C, company_code);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x94, mastering_date);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xB4, logo_type);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xB8, app_launch_type);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xBC, invisible_flag);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xC0, no_managed_flag);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xC4, no_event_log);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xC8, no_icon_database);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xCC, launching_flag);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xD0, install_flag);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xD4, closing_msg);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xD8, title_version);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xDC, group_id);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xE0, save_no_rollback);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xE4, bg_daemon_enable);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xE8, join_game_id);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xEC, olv_accesskey);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xF0, wood_tin);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xF4, e_manual);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xF8, e_manual_version);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xFC, region);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x100, pc_cero);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x104, pc_esrb);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x108, pc_bbfc);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x10C, pc_usk);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x110, pc_pegi_gen);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x114, pc_pegi_fin);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x118, pc_pegi_prt);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x11C, pc_pegi_bbfc);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x120, pc_cob);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x124, pc_grb);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x128, pc_cgsrr);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x12C, pc_oflc);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x130, pc_reserved0);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x134, pc_reserved1);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x138, pc_reserved2);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x13C, pc_reserved3);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x140, ext_dev_nunchaku);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x144, ext_dev_classic);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x148, ext_dev_urcc);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x14C, ext_dev_board);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x150, ext_dev_usb_keyboard);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x154, ext_dev_etc);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x158, ext_dev_etc_name);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x358, eula_version);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x35C, drc_use);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x360, network_use);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x364, online_account_use);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x368, direct_boot);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x36C, reserved_flag0);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x370, reserved_flag1);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x374, reserved_flag2);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x378, reserved_flag3);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x37C, reserved_flag4);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x380, reserved_flag5);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x384, reserved_flag6);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x388, reserved_flag7);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x38C, longname_ja);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x58C, longname_en);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x78C, longname_fr);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x98C, longname_de);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xB8C, longname_it);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xD8C, longname_es);
WUT_CHECK_OFFSET(_ACPMetaXml, 0xF8C, longname_zhs);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x118C, longname_ko);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x138C, longname_nl);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x158C, longname_pt);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x178C, longname_ru);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x198C, longname_zht);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x1B8C, shortname_ja);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x1C8C, shortname_en);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x1D8C, shortname_fr);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x1E8C, shortname_de);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x1F8C, shortname_it);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x208C, shortname_es);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x218C, shortname_zhs);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x228C, shortname_ko);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x238C, shortname_nl);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x248C, shortname_pt);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x258C, shortname_ru);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x268C, shortname_zht);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x278C, publisher_ja);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x288C, publisher_en);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x298C, publisher_fr);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x2A8C, publisher_de);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x2B8C, publisher_it);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x2C8C, publisher_es);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x2D8C, publisher_zhs);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x2E8C, publisher_ko);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x2F8C, publisher_nl);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x308C, publisher_pt);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x318C, publisher_ru);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x328C, publisher_zht);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x338C, add_on_unique_id0);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x3394, add_on_unique_id2);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x3398, add_on_unique_id3);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x339C, add_on_unique_id4);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33A0, add_on_unique_id5);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33A4, add_on_unique_id6);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33A8, add_on_unique_id7);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33AC, add_on_unique_id8);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33B0, add_on_unique_id9);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33B4, add_on_unique_id10);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33B8, add_on_unique_id11);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33BC, add_on_unique_id12);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33C0, add_on_unique_id13);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33C4, add_on_unique_id14);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33C8, add_on_unique_id15);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33CC, add_on_unique_id16);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33D0, add_on_unique_id17);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33D4, add_on_unique_id18);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33D8, add_on_unique_id19);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33DC, add_on_unique_id20);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33E0, add_on_unique_id21);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33E4, add_on_unique_id22);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33E8, add_on_unique_id23);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33EC, add_on_unique_id24);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33F0, add_on_unique_id25);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33F4, add_on_unique_id26);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33F8, add_on_unique_id27);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x33FC, add_on_unique_id28);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x3400, add_on_unique_id29);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x3404, add_on_unique_id30);
WUT_CHECK_OFFSET(_ACPMetaXml, 0x3408, add_on_unique_id31);
WUT_CHECK_SIZE(_ACPMetaXml,0x340C);

extern "C" {
   extern void __init_wut_malloc();
   extern void __fini_wut_malloc();
}

WUPS_FS_ACCESS()

MCPTitleListType my[25] __attribute__((section(".data")));
MCPTitleListType template_title;

ON_APPLICATION_START(args) {
    __init_wut_malloc();
    socket_lib_init();
    log_init();
    DEBUG_FUNCTION_LINE("###############\n");
}

ON_APPLICATION_ENDING(){
    __fini_wut_malloc();
   DEBUG_FUNCTION_LINE("###############\n");
}

 
DECL_FUNCTION(int32_t, MCP_TitleList, uint32_t handle, uint32_t* outTitleCount, MCPTitleListType* titleList, uint32_t size) {
    int32_t result = real_MCP_TitleList(handle, outTitleCount, titleList, size);
    //DEBUG_FUNCTION_LINE("%08X %08X %08X %08X = %08X\n",handle,*outTitleCount,titleList,size,result);
    
    uint32_t titlecount = *outTitleCount;
       
    for(uint32_t i = 0;i<titlecount;i++){
        if(titleList[i].titleId == 0x000500101004e200){
            memcpy(&template_title, &(titleList[i]),sizeof(MCPTitleListType));
        }
    }
    
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
        
        DEBUG_FUNCTION_LINE("%s\n",dirList.GetFilename(i));

        char buffer [100];
        
        snprintf(buffer,100,"/custom/%08X%08X", 0x0005000F, j); 
       
        strcpy(template_title.path,buffer);    
        template_title.titleId = 0x0005000F00000000 + j;
        
        memcpy(&(titleList[titlecount]), &template_title ,sizeof(MCPTitleListType));
        memcpy(&(my[j]), &template_title ,sizeof(MCPTitleListType));
        titlecount++;
        j++;
    }
  
    for(uint32_t i = 0;i<titlecount;i++){        
        DEBUG_FUNCTION_LINE("%d %016llX %s \n",i, titleList[i].titleId, titleList[i].path);        
    }
    
    *outTitleCount = titlecount;
   
    return result;
}

DECL_FUNCTION(int32_t, MCP_TitleListByAppType,int32_t handle,
                       MCPAppType appType,
                       uint32_t *outTitleCount,
                       MCPTitleListType *titleList,
                       uint32_t titleListSizeBytes) {
                    
    int32_t result = real_MCP_TitleListByAppType(handle, appType, outTitleCount, titleList,titleListSizeBytes);
    //DEBUG_FUNCTION_LINE("%08X %08X %08X %08X %08X = %08X\n",handle,appType,*outTitleCount,titleList,titleListSizeBytes,result);
    
    for(uint32_t i = 0;i<*outTitleCount;i++){
         //DEBUG_FUNCTION_LINE("%016llX %s \n",titleList[i].titleId, titleList[i].path);
    }
   
    return result;
}

DECL_FUNCTION(int32_t, MCP_GetTitleInfoByTitleAndDevice, uint32_t mcp_handle, uint32_t titleid_lower_1, uint32_t titleid_upper, uint32_t titleid_lower_2, uint32_t u5, MCPTitleListType* u6) {
    DEBUG_FUNCTION_LINE("lower1: %08X ID: %08X%08X %08X %08X \n",titleid_lower_1 ,titleid_upper ,titleid_lower_2 ,u5 ,u6);

    if(titleid_upper ==  0x0005000F){
        memcpy(u6, &(my[titleid_lower_2]), sizeof(MCPTitleListType));
        return 0;
    }
   
    int result = real_MCP_GetTitleInfoByTitleAndDevice(mcp_handle, titleid_lower_1, titleid_upper, titleid_lower_2, u5, u6);
    DEBUG_FUNCTION_LINE("lower1: %08X ID: %08X%08X %08X %s = %08X \n",titleid_lower_1 ,titleid_upper ,titleid_lower_2 ,u5 ,u6->path ,result);
  
    return result;
}


DECL_FUNCTION(int32_t, ACPCheckTitleLaunchByTitleListType, uint32_t u1, uint32_t u2, uint32_t u3, uint32_t u4, uint32_t u5, uint32_t u6, uint32_t u7) {
    DEBUG_FUNCTION_LINE("\n");
    int result = real_ACPCheckTitleLaunchByTitleListType(u1, u2, u3, u4, u5, u6, u7);
    DEBUG_FUNCTION_LINE("%08X %08X %08X %08X %08X %08X %08X = %08X will force it to 0 \n",u1 ,u2 ,u3 ,u4 ,u5 ,u6 ,u7 ,result);
    return 0;
}

DECL_FUNCTION(int, FSOpenFile, FSClient *pClient, FSCmdBlock *pCmd, char *path, const char *mode, int *handle, int error) {  
    socket_lib_init();
    log_init();
    
    char * start = "/vol/storage_mlc01/sys/title/0005000F";
    if(strncmp(path,start,strlen(start)) == 0){
        strcpy(path,"/vol/storage_mlc01/usr/title/00050000/10172000/meta/iconTex.tga");
    }
          
    int result = real_FSOpenFile(pClient, pCmd, path, mode, handle, error);
    
    DEBUG_FUNCTION_LINE("%s! Result %d\n",path,result);
    return result;
}

DECL_FUNCTION(int32_t, ACPGetTitleMetaXmlByDevice, uint32_t titleid_upper, uint32_t titleid_lower, _ACPMetaXml* out_buf, uint32_t device, uint32_t u1) {
    
    int result = real_ACPGetTitleMetaXmlByDevice(titleid_upper, titleid_lower, out_buf, device,u1);
      
    if(titleid_upper ==  0x00050000 && titleid_lower == 0x13374842){     
        //dumpHex((void*)out_buf,0x3500);       
    }
    
    
    if(titleid_upper ==  0x0005000F){
        out_buf->title_id = 0x000500101004e200;
         
        char buffer [100];
        
        snprintf  (buffer,100,"/custom/%08X%08X", titleid_upper,titleid_lower) ;  
        
        
        strncpy(out_buf->longname_en,buffer,strlen(buffer));
        strncpy(out_buf->shortname_en,buffer,strlen(buffer));      
        
        result = 0;
    }
    
    DEBUG_FUNCTION_LINE("TitleID: %08X%08X res:%016llX device: %d %08X = %08X \n",titleid_upper ,titleid_lower ,out_buf->title_id , device ,u1,result);
    return result;    
}

DECL_FUNCTION(int32_t, ACPGetTitleMetaDirByDevice, uint32_t titleid_upper, uint32_t titleid_lower, char* out_buf, uint32_t size, int device) {
    int result = real_ACPGetTitleMetaDirByDevice(titleid_upper, titleid_lower, out_buf, size, device);
   
    
    if(titleid_upper ==  0x0005000F){
        return -1;
        /*DEBUG_FUNCTION_LINE("Replace\n");
        char *  newPath = "/vol/storage_mlc01/usr/title/0005000F/13374842/meta";
        
        strcpy(out_buf,newPath);
        
        result = 0;*/
    }
    
    DEBUG_FUNCTION_LINE("TitleID: %08X%08X path:%s (%d)device: %d = %08X \n",titleid_upper ,titleid_lower ,out_buf ,size, device ,result);
    
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
    if(strncmp(pathToLoad,start,strlen(start)) == 0){    
        /*
        std::map<std::string,std::string>::iterator it;
        it = rpxList.find(pathToLoad);
        if (it != rpxList.end()){
            std::string copy = it->second;
        
            char * repl = (char*)"fs:/vol/external01/";
            char * with = (char*)"/vol/storage_iosu_homebrew/";
            char * input = (char*) copy.c_str();

            char* extension = input + strlen(input) - 4;
            if (extension[0] == '.' &&
                extension[1] == 'r' &&
                extension[2] == 'p' &&
                extension[3] == 'x') {

                char rpxpath[280];
                char * path = str_replace(input,repl, with);
                if(path != NULL) {
                    log_printf("Loading file %s\n", path);

                    strncpy(rpxpath, path, sizeof(rpxpath) - 1);
                    rpxpath[sizeof(rpxpath) - 1] = '\0';

                    free(path);

                    int mcpFd = IOS_Open("/dev/mcp", (IOSOpenMode)0);
                    if(mcpFd >= 0) {
                        int out = 0;
                        IOS_Ioctl(mcpFd, 100, (void*)rpxpath, strlen(rpxpath), &out, sizeof(out));
                        IOS_Close(mcpFd);
                        if(out == 2) {
                            DEBUG_FUNCTION_LINE("sucess\n");
                        }else{
                            DEBUG_FUNCTION_LINE("failed\n");
                        }
                    }
                }
            }
        }else{
            DEBUG_FUNCTION_LINE("Mapping failed\n");
        }*/
        
        // always load H&S app
        strcpy(pathToLoad,"/vol/storage_mlc01/usr/title/00050000/10119b00");
    }    
    
    int32_t result = real__SYSLaunchTitleByPathFromLauncher(pathToLoad, strlen(pathToLoad));
    

    DEBUG_FUNCTION_LINE("%s %08X result %08X \n",pathToLoad,u2,result);    
    return result;
}

DECL_FUNCTION(int32_t, ACPGetLaunchMetaData, _ACPMetaData* metadata) {
    int result = real_ACPGetLaunchMetaData(metadata);  
    return result;
}

DECL_FUNCTION(int32_t, ACPGetLaunchMetaXml, _ACPMetaXml * metaxml) {
    int result = real_ACPGetLaunchMetaXml(metaxml);   
     
    char buffer [100];
    
    snprintf  (buffer,100,"Hello World!"); 
    strncpy(metaxml->longname_en,buffer,strlen(buffer));
    strncpy(metaxml->shortname_en,buffer,strlen(buffer));
    return result;
}

WUPS_MUST_REPLACE(FSOpenFile,                           WUPS_LOADER_LIBRARY_COREINIT,       FSOpenFile);
WUPS_MUST_REPLACE(MCP_TitleList,                        WUPS_LOADER_LIBRARY_COREINIT,       MCP_TitleList);
WUPS_MUST_REPLACE(MCP_GetTitleInfoByTitleAndDevice ,    WUPS_LOADER_LIBRARY_COREINIT,       MCP_GetTitleInfoByTitleAndDevice );
WUPS_MUST_REPLACE(ACPCheckTitleLaunchByTitleListType ,  WUPS_LOADER_LIBRARY_NN_ACP,         ACPCheckTitleLaunchByTitleListType );
WUPS_MUST_REPLACE(ACPGetTitleMetaXmlByDevice ,          WUPS_LOADER_LIBRARY_NN_ACP,         ACPGetTitleMetaXmlByDevice );
WUPS_MUST_REPLACE(ACPGetLaunchMetaData ,                WUPS_LOADER_LIBRARY_NN_ACP,         ACPGetLaunchMetaData );
WUPS_MUST_REPLACE(ACPGetLaunchMetaXml ,                 WUPS_LOADER_LIBRARY_NN_ACP,         ACPGetLaunchMetaXml );
WUPS_MUST_REPLACE(ACPGetTitleMetaDirByDevice ,          WUPS_LOADER_LIBRARY_NN_ACP,         ACPGetTitleMetaDirByDevice );
WUPS_MUST_REPLACE(_SYSLaunchTitleByPathFromLauncher,    WUPS_LOADER_LIBRARY_SYSAPP,         _SYSLaunchTitleByPathFromLauncher);
