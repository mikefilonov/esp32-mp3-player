#pragma once

#include "BluetoothA2DPSource.h"
#include "navigation.h"
#include "player.h"

// Register callbacks for AVRC passthrough commands (play/pause/next/prev/vol)
// and absolute volume notifications.
// Call before playerStart() so callbacks are in place when BT starts.
void btNavigationSetup(BluetoothA2DPSource* source, NavigationController* ctr);
