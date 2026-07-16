#ifndef READ_SD_H
#define READ_SD_H

#include <Arduino.h>
#include <SD.h>
#include <GyverOLED.h>

enum state_struct {state_load, state_dir, state_player};

extern GyverOLED<SSD1306_128x64> oled;
extern bool update_draw;
extern bool is_paused;
extern int sel_index;
extern int menu_scroll;
extern int total_items;
extern bool can_check_btn;
extern void player_start(const char* folderPath, const String& fileName);
extern int clickedItemIndex;
extern String currentFolderPath;


String gen_folder_signature(const char* folderPath);
void check_folder_cache(String folderName);
void scan_all_folders(const char* currentPath);
void displayFolderMenu(const char* folderPath, int startItem, int maxLines);
void handleMenuSelection(const char* folderPath, int targetLineIndex);
void skip_track(int direction);

#endif