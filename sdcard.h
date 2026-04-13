#pragma once

#include <SD.h>

// Returns true if a matching MP3 file was found.
// On success, writes the full path into `out` (size `maxLen`).
// Pass currentFile="" to get the first file.
// Pass backwards=true to get the previous file.
bool sdFindNextFile(char* out, size_t maxLen, const char* currentFile, bool backwards = false);

void sdSetup();
