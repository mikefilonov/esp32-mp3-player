/*
 * ESP32 Bluetooth MP3 Player (A2DP Source)
 * Copyright (C) 2026  Mike Filonov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "sdcard.h"
#include <Arduino.h>

#define SD_CS_PIN 5

static bool isMP3File(const char* name) {
  int len = strlen(name);
  if (len < 4) return false;
  const char* ext = name + len - 4;
  return (strcasecmp(ext, ".mp3") == 0);
}

static bool sd_ok = false;

void sdSetup() {
  Serial.println("[sdcard] Initializing SD Card...");
  sd_ok = SD.begin(SD_CS_PIN);
  if (sd_ok) {
    Serial.println("[sdcard] SD Card detected and mounted successfully.");
  } else {
    Serial.println("[sdcard] SD Card not present at boot.");
  }
}

bool sdCheckConnection() {
  if (sd_ok) {
    // Perform a quick read check to verify card hasn't been removed
    File root = SD.open("/");
    if (root) {
      root.close();
      return true;
    }
    // If opening root fails, the card was pulled out
    Serial.println("[sdcard] SD Card disconnected!");
    SD.end();
    sd_ok = false;
  }

  // Attempt to re-initialize SD card
  sd_ok = SD.begin(SD_CS_PIN);
  if (sd_ok) {
    Serial.println("[sdcard] SD Card reconnected!");
  }
  return sd_ok;
}

bool sdFindNextFile(char* out, size_t maxLen, const char* currentFile, bool backwards) {
  if (!sdCheckConnection()) {
    return false;
  }

  bool fileFound = false;
  bool rewindDone = false;
  char prevFile[256] = {0};

  File root = SD.open("/");
  if (!root) {
    Serial.println("[sdcard] can't open root");
    return false;
  }

  while (true) {
    File entry = root.openNextFile();

    if (!entry) {
      if (rewindDone) { 
        Serial.println("[sdcard] no more files");
        break;
      }

      root.rewindDirectory();
      currentFile = ""; // start on first file
      rewindDone = true; // don't allow infinite loop
      continue; // restart search
    }

    if (entry.isDirectory()) { entry.close(); continue; }

    const char* name = entry.name();

    if (name[0] == '.') { entry.close(); continue; }
    if (!isMP3File(name)) { entry.close(); continue; }

    int len = strlen(name);
    if (len >= 250) { entry.close(); continue; }

    // Build full path
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "/%s", name);

    if (!backwards) {
      // Forward: pick the entry after currentFile (or first if currentFile is empty)
      bool noneSelected = (strnlen(currentFile, maxLen) == 0);
      bool prevWasCurrent = (strnlen(prevFile, 256) > 0 &&
                             strncmp(prevFile, currentFile, 256) == 0);
      if (noneSelected || prevWasCurrent) {
        strncpy(out, fullPath, maxLen - 1);
        out[maxLen - 1] = '\0';
        entry.close();
        fileFound = true;
        break;
      }
    } else {
      // Backward: pick the entry before currentFile
      if (strnlen(currentFile, maxLen) == 0) { entry.close(); break; }

      // On the first file with no previous: stay on first (no-op)
      if (strnlen(prevFile, 256) == 0 && strncmp(fullPath, currentFile, 256) == 0) {
        strncpy(out, fullPath, maxLen - 1);
        out[maxLen - 1] = '\0';
        entry.close();
        fileFound = true;
        break;
      }
      // Found current: promote prev
      if (strncmp(fullPath, currentFile, 256) == 0) {
        strncpy(out, prevFile, maxLen - 1);
        out[maxLen - 1] = '\0';
        entry.close();
        fileFound = true;
        break;
      }
    }

    strncpy(prevFile, fullPath, sizeof(prevFile) - 1);
    entry.close();
  }

  root.close();
  return fileFound;
}
