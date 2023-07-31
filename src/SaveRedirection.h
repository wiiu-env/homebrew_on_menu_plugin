#pragma once

extern bool gInWiiUMenu;

#define HOMEBREW_ON_MENU_PLUGIN_DATA_PATH_BASE "/wiiu/homebrew_on_menu_plugin"
#define HOMEBREW_ON_MENU_PLUGIN_DATA_PATH      "/vol/external01" HOMEBREW_ON_MENU_PLUGIN_DATA_PATH_BASE

void SaveRedirectionCleanUp();