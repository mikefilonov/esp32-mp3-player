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

#pragma once

#include <SD.h>

// Returns true if a matching MP3 file was found.
// On success, writes the full path into `out` (size `maxLen`).
// Pass currentFile="" to get the first file.
// Pass backwards=true to get the previous file.
bool sdFindNextFile(char* out, size_t maxLen, const char* currentFile, bool backwards = false);

void sdSetup();
bool sdCheckConnection();
