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

#include "BluetoothA2DPSource.h"
#include "AudioGeneratorMP3.h"
#include "AudioFileSourceSD.h"

class PlayerEvents {
  public:
    virtual void onTrackFinished() = 0;
    virtual void onBTConnected() = 0;
    virtual void onBTDisconnected() = 0;
};

bool playerInit(PlayerEvents* events, bool pairingModeRequested);
void playerStart(); // call after all BT callbacks are registered
void playerLoop();

void playerStartFile(const char* path);
void playerStop();
void playerPause(bool paused);
bool playerIsStopped();
bool playerIsPaused();
bool playerIsConnected();
void playerSetVolume(uint8_t volume_0_127);   // physical buttons — goes via a2dp_source
void playerUpdateVolume(uint8_t volume_0_127); // bt_navigation only — skips a2dp_source
uint8_t playerGetVolume();

BluetoothA2DPSource* getA2DPSource();
