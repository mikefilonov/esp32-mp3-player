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
#include "navigation.h"
#include "player.h"

// Register callbacks for AVRC passthrough commands (play/pause/next/prev/vol)
// and absolute volume notifications.
// Call before playerStart() so callbacks are in place when BT starts.
void btNavigationSetup(BluetoothA2DPSource* source, NavigationController* ctr);
