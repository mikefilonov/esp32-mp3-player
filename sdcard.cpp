#include "sdcard.h"
#include <Arduino.h>

#define SD_CS_PIN 5

static bool isMP3File(const char* name) {
  int len = strlen(name);
  if (len < 4) return false;
  const char* ext = name + len - 4;
  return (strcasecmp(ext, ".mp3") == 0);
}

void sdSetup() {
  while (!SD.begin(SD_CS_PIN)) {
    Serial.println("[sdcard] mount failed, retrying...");
    delay(3000);
  }
  Serial.println("[sdcard] SD OK");
}

bool sdFindNextFile(char* out, size_t maxLen, const char* currentFile, bool backwards) {
  bool fileFound = false;
  char prevFile[256] = {0};

  File root = SD.open("/");
  if (!root) {
    Serial.println("[sdcard] can't open root");
    return false;
  }

  while (true) {
    File entry = root.openNextFile();

    if (!entry) {
      Serial.println("[sdcard] no more files");
      break;
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
