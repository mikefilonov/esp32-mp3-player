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
void playerStart(std::vector<const char *> names); // call after all BT callbacks are registered
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
