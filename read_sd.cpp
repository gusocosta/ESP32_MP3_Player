#pragma once
#include <Arduino.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "bitmaps.h"
#include "read_sd.h"
#include <vector>
#include <algorithm>

struct FileEntry {
  String name;
  int physicalIndex;
};

String gen_folder_signature(const char* folderPath) {
  File folder = SD.open(folderPath);

  if (!folder || !folder.isDirectory()) {
    if (folder) folder.close();
    return "ERROR";
  }

  folder.rewindDirectory();

  std::vector<FileEntry> foldersList;
  std::vector<FileEntry> filesList;
  foldersList.reserve(64);
  filesList.reserve(128);

  int physicalIndex = 0;
  unsigned long totalSize = 0;

  while (true) {
    File entry = folder.openNextFile();
    if (!entry) break;

    String itemName = String(entry.name());

    if (itemName.equalsIgnoreCase("System Volume Information") || itemName.equalsIgnoreCase(".Trashes") || itemName.equalsIgnoreCase(".file_sign.txt") || itemName.startsWith(".")) {
      entry.close();
      physicalIndex++;
      continue;
    }

    if (entry.isDirectory()) {
      if (foldersList.size() < 256) {
        foldersList.push_back({ itemName, physicalIndex });
      }
    } else {
      totalSize += entry.size();
      if (filesList.size() < 256) {
        filesList.push_back({ itemName, physicalIndex });
      }
    }

    entry.close();
    physicalIndex++;
  }

  std::sort(foldersList.begin(), foldersList.end(), [](const FileEntry& a, const FileEntry& b) {
    return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
  });

  if (String(folderPath) != "/") {
    foldersList.insert(foldersList.begin(), { "..", -1 });
  }

  std::sort(filesList.begin(), filesList.end(), [](const FileEntry& a, const FileEntry& b) {
    return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
  });

  int totalItemCount = foldersList.size() + filesList.size();
  String signature = String(totalItemCount) + "_" + String(totalSize);

  extern int total_items;
  total_items = totalItemCount;

  String cacheFilePath = String(folderPath);
  if (!cacheFilePath.endsWith("/")) cacheFilePath += "/";
  cacheFilePath += ".file_sign.txt";

  if (SD.exists(cacheFilePath.c_str())) {
    File checkFile = SD.open(cacheFilePath.c_str(), FILE_READ);
    if (checkFile) {
      String firstLine = checkFile.readStringUntil('\n');
      checkFile.close();
      firstLine.trim();

      int separatorPos = firstLine.indexOf("$$");
      if (separatorPos != -1) {
        String savedSignature = firstLine.substring(separatorPos + 2);

        if (signature == savedSignature) {
          Serial.println("[CACHE] Pasta idêntica. Pulando escrita no SD!");
          folder.close();
          return signature;
        }
      }
    }
  }

  Serial.println("[CACHE] Pasta modificada ou nova. Atualizando arquivo txt...");

  if (SD.exists(cacheFilePath.c_str())) {
    SD.remove(cacheFilePath.c_str());
  }

  File cacheFile = SD.open(cacheFilePath.c_str(), FILE_WRITE);
  if (!cacheFile) {
    folder.close();
    return "ERROR_CACHE_WRITE";
  }

  cacheFile.print(folderPath);
  cacheFile.print("$$");
  cacheFile.println(signature);

  for (const auto& f : foldersList) {
    cacheFile.print(f.name);
    cacheFile.print("$$");
    cacheFile.println(f.physicalIndex);
  }

  for (const auto& f : filesList) {
    cacheFile.print(f.name);
    cacheFile.print("&&");
    cacheFile.println(f.physicalIndex);
  }

  cacheFile.close();
  folder.close();

  update_draw = true;
  return signature;
}

void displayFolderMenu(const char* folderPath, int startItem, int maxLines) {
  String cachePath = String(folderPath);
  if (!cachePath.endsWith("/")) cachePath += "/";
  cachePath += ".file_sign.txt";

  File cacheFile = SD.open(cachePath.c_str(), FILE_READ);
  if (!cacheFile) return;

  if (cacheFile.available()) {
    cacheFile.readStringUntil('\n');  
  }

  int currentItemIndex = 0;
  int lines_printed = 0;
  extern int sel_index;

  oled.rect(0, 0, 127, 7, 1);
  oled.invertText(true);
  oled.setCursor(52, 0);
  oled.print("Menu");
  oled.invertText(false);

  while (cacheFile.available() && lines_printed < maxLines) {
    String line = cacheFile.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) continue;

    if (currentItemIndex >= startItem) {
      int separatorPos = line.indexOf("$$");
      bool isFolder = true;
      if (separatorPos == -1) {
        separatorPos = line.indexOf("&&");
        isFolder = false;
      }

      if (sel_index == lines_printed) {
        oled.drawBitmap(1, ((lines_printed + 1) * 8), spr_sel, 8, 8);
      }

      if (separatorPos != -1) {
        String itemName = line.substring(0, separatorPos);

        if (isFolder) {
          oled.drawBitmap(9, ((lines_printed + 1) * 8), spr_file, 8, 8);
          oled.setCursor(18, lines_printed + 1);
          oled.print(itemName);
        } else {
          oled.setCursor(8, lines_printed + 1);
          oled.print(itemName);
        }
        lines_printed++;
      }
    }
    currentItemIndex++;
  }
  cacheFile.close();
}

void handleMenuSelection(const char* folderPath, int targetLineIndex) {
  String cachePath = String(folderPath);
  if (!cachePath.endsWith("/")) cachePath += "/";
  cachePath += ".file_sign.txt";

  File cacheFile = SD.open(cachePath.c_str(), FILE_READ);
  if (!cacheFile) {
    can_check_btn = true;
    return;
  }

  if (cacheFile.available()) cacheFile.readStringUntil('\n');

  int currentLine = 0;
  String selectedLine = "";

  while (cacheFile.available()) {
    String line = cacheFile.readStringUntil('\n');
    if (currentLine == targetLineIndex) {
      selectedLine = line;
      break;
    }
    currentLine++;
  }
  cacheFile.close();

  selectedLine.trim();
  if (selectedLine.length() == 0) {
    can_check_btn = true;
    return;
  }

  int separatorPos = selectedLine.indexOf("$$");
  bool isFolder = true;

  if (separatorPos == -1) {
    separatorPos = selectedLine.indexOf("&&");
    isFolder = false;
  }

  if (separatorPos != -1) {
    String itemName = selectedLine.substring(0, separatorPos);
    String fatIndexStr = selectedLine.substring(separatorPos + 2);
    int physicalFATIndex = fatIndexStr.toInt();

    extern String currentFolderPath;
    extern int menu_scroll;
    extern int sel_index;
    extern bool update_draw;

    if (itemName == "..") {
      int lastSlash = currentFolderPath.lastIndexOf('/');
      if (lastSlash > 0) {
        currentFolderPath = currentFolderPath.substring(0, lastSlash);
      } else {
        currentFolderPath = "/";
      }
    
      gen_folder_signature(currentFolderPath.c_str());
      menu_scroll = 0;
      sel_index = 0;
      update_draw = true;
      can_check_btn = true;
      return;
    }

    if (isFolder) {
      if (currentFolderPath == "/") {
        currentFolderPath += itemName;
      } else {
        currentFolderPath += "/" + itemName;
      }

      gen_folder_signature(currentFolderPath.c_str());
      menu_scroll = 0;
      sel_index = 0;
      update_draw = true;
    } else {
      player_start(currentFolderPath.c_str(), itemName);
      extern state_struct state; 
      state = state_player;
      
      update_draw = true;
    }
  }
  can_check_btn = true;
}

void skip_track(int direction) {
  String cachePath = currentFolderPath;
  if (!cachePath.endsWith("/")) cachePath += "/";
  cachePath += ".file_sign.txt";

  File cacheFile = SD.open(cachePath.c_str(), FILE_READ);
  if (!cacheFile) return;

  if (cacheFile.available()) cacheFile.readStringUntil('\n'); 

  int current_line = 0;
  int target_line = clickedItemIndex + direction; 
  bool found_valid = false;

  while (cacheFile.available()) {
    String line = cacheFile.readStringUntil('\n');
    if (current_line == target_line) {
      if (line.indexOf("&&") != -1) { 
        found_valid = true;
      }
      break;
    }
    current_line++;
  }
  cacheFile.close();
  if (found_valid) {
    clickedItemIndex = target_line; 
    handleMenuSelection(currentFolderPath.c_str(), clickedItemIndex); 
  } else {
    is_paused = true;
    update_draw = true;
  }
}
