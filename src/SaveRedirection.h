#pragma once
#include <stdbool.h>

extern bool gInWiiUMenu;
extern bool sSDIsMounted;

#define SAVE_REPLACEMENT_PATH "/vol/external01/wiiu/homebrew_on_menu_plugin"

void InitSaveData();