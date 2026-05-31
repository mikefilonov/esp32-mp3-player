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

#include "player.h"
#include <Arduino.h>
#include <Preferences.h>


// ── Ring buffer ───────────────────────────────────────────────────────────────
#define RING_BUF_SAMPLES 4096
static int16_t  ring[RING_BUF_SAMPLES];
static volatile int ring_write = 0;
static volatile int ring_read  = 0;

static inline int ring_available() {
  return (ring_write - ring_read + RING_BUF_SAMPLES) % RING_BUF_SAMPLES;
}

// ── Custom AudioOutput that writes decoded samples into the ring buffer ───────
class RingBufferOutput : public AudioOutput {
public:
  bool begin() override { return true; }
  bool stop()  override { return true; }

  bool ConsumeSample(int16_t sample[2]) override {
    int next_w = (ring_write + 2) % RING_BUF_SAMPLES;
    if (next_w == ring_read) return false; // full
    ring[ring_write]                          = sample[0];
    ring[(ring_write + 1) % RING_BUF_SAMPLES] = sample[1];
    ring_write = next_w;
    return true;
  }
};

// ── Module state ──────────────────────────────────────────────────────────────
static BluetoothA2DPSource  a2dp_source;
static AudioFileSourceSD*   file    = nullptr;
static AudioGeneratorMP3*   mp3     = nullptr;
static RingBufferOutput*    ringOut = nullptr;

static PlayerEvents*    playerEvents  = nullptr;
static volatile bool    bt_connected  = false;
static bool             paused        = false;
static volatile uint8_t currentVolume = 64; // initialized to mid-range (50%) to prevent startup jumps
static Preferences preferences;
static bool volumeSyncedToSpeaker = false;

// ── A2DP data callback — runs on BT task, must not block ─────────────────────
static int32_t get_sound_data(Frame* frames, int32_t num_frames) {
  if (paused) {
    memset(frames, 0, num_frames * sizeof(Frame));
    return num_frames;
  }

  int available_frames = ring_available() / 2;
  int to_send = min((int)num_frames, available_frames);

  for (int i = 0; i < to_send; i++) {
    frames[i].channel1 = ring[ring_read];
    frames[i].channel2 = ring[(ring_read + 1) % RING_BUF_SAMPLES];
    ring_read = (ring_read + 2) % RING_BUF_SAMPLES;
  }
  // No PCM scaling — volume is handled entirely by the device's hardware DAC.

  for (int i = to_send; i < num_frames; i++) {
    frames[i].channel1 = 0;
    frames[i].channel2 = 0;
  }

  return num_frames;
}

static bool filter_devices(const char* ssid, esp_bd_addr_t address, int rssi) {
  Serial.printf("[player] Scanned compatible BT device: %s, RSSI: %d\n", ssid ? ssid : "(null)", rssi);
  if (ssid && strlen(ssid) > 0) {
    Serial.printf("[player] Connecting to discovered speaker: %s\n", ssid);
    return true; // Accept this device
  }
  return false;
}

// ── Public API ────────────────────────────────────────────────────────────────
bool playerInit(PlayerEvents* events, bool pairingModeRequested) {
  playerEvents = events;

  a2dp_source.set_data_callback_in_frames(get_sound_data);
  a2dp_source.set_ssid_callback(filter_devices);

  a2dp_source.set_auto_reconnect(true);
  a2dp_source.get_last_connection();

  // Restore volume from NVS preferences
  preferences.begin("mp3player", false);
  currentVolume = preferences.getUChar("volume", 64);
  preferences.end();
  Serial.printf("[player] NVS Restored volume: %d\n", currentVolume);

  bool noLastConnection = !a2dp_source.has_last_connection();
  bool activePairing = pairingModeRequested || noLastConnection;

  if (noLastConnection) {
    Serial.println("[player] No connection history found — entering pairing mode by default.");
  }

  if (pairingModeRequested) {
    Serial.println("[player] Pairing requested on boot — clearing last paired device to allow new pairing.");
    a2dp_source.clean_last_connection();
  }

  a2dp_source.set_on_connection_state_changed([](esp_a2d_connection_state_t state, void*) {
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
      Serial.println("[player] BT connected");
      bt_connected = true;
      if (playerEvents) playerEvents->onBTConnected();
    } else {
      Serial.println("[player] BT disconnected");
      bt_connected = false;
      volumeSyncedToSpeaker = false; // Reset sync flag on disconnect
      if (playerEvents) playerEvents->onBTDisconnected();
    }
  });

  return activePairing;
}

void playerStart() {
  a2dp_source.start();
  Serial.println("[player] A2DP started");
}

void playerLoop() {
  if (mp3 && mp3->isRunning()) {
    // Fill the ring buffer to maximum capacity in a single pass to absorb system latencies.
    // We keep decoding as long as there is space for more samples in the buffer.
    while (ring_available() < RING_BUF_SAMPLES - 512) {
      if (!mp3->loop()) {
        mp3->stop();
        Serial.println("[player] track finished");
        if (playerEvents) playerEvents->onTrackFinished();
        break;
      }
      yield(); // Yield to feed watchdog and allow other tasks to run
    }
  }
}

void playerStartFile(const char* path) {
  playerStop();

  Serial.printf("[player] opening %s\n", path);
  file = new AudioFileSourceSD(path);
  if (!file->isOpen()) {
    Serial.println("[player] failed to open file");
    delete file; file = nullptr;
    return;
  }

  ringOut = new RingBufferOutput();
  mp3     = new AudioGeneratorMP3();

  if (!mp3->begin(file, ringOut)) {
    Serial.println("[player] mp3->begin() failed");
    delete mp3;  mp3  = nullptr;
    delete file; file = nullptr;
    delete ringOut; ringOut = nullptr; // Fix memory leak
    return;
  }

  paused = false;
  Serial.printf("[player] playing %s\n", path);
}

void playerStop() {
  if (mp3) {
    if (mp3->isRunning()) mp3->stop();
    delete mp3;  mp3  = nullptr;
  }
  if (file) { delete file; file = nullptr; }
  if (ringOut) { delete ringOut; ringOut = nullptr; }
  ring_write = 0;
  ring_read  = 0;
  paused = false;
}

void playerPause(bool p) {
  paused = p;
  Serial.printf("[player] %s\n", p ? "paused" : "resumed");
}

bool playerIsStopped() {
  return mp3 == nullptr || !mp3->isRunning();
}

bool playerIsPaused() {
  return paused;
}

bool playerIsConnected() {
  return bt_connected;
}

static void saveVolumeToNVS(uint8_t vol) {
  preferences.begin("mp3player", false);
  preferences.putUChar("volume", vol);
  preferences.end();
  Serial.printf("[player] NVS Saved volume: %d\n", vol);
}

// Called by bt_navigation when the device reports a volume change.
// Just tracks the value so the controller can read it for Up/Down increments.
void playerUpdateVolume(uint8_t volume_0_127) {
  if (!volumeSyncedToSpeaker) {
    // Ignore initial speaker volume reports until we successfully push our NVS restored volume
    Serial.printf("[player] Ignored initial speaker volume report of %d to protect NVS restored state\n", volume_0_127);
    return;
  }
  if (currentVolume != volume_0_127) {
    currentVolume = volume_0_127;
    saveVolumeToNVS(volume_0_127);
  }
}

// Called by physical buttons via the controller.
// Sends an AVRC "set absolute volume" command to the device — the device's
// hardware DAC applies it. No PCM scaling on our side.
void playerSetVolume(uint8_t volume_0_127) {
  if (currentVolume != volume_0_127 || !volumeSyncedToSpeaker) {
    currentVolume = volume_0_127;
    saveVolumeToNVS(volume_0_127);
  }
  esp_avrc_ct_send_set_absolute_volume_cmd(1 /*TL_RN_VOLUME_CHANGE*/, volume_0_127);
  volumeSyncedToSpeaker = true; // We successfully synchronized our NVS volume to the speaker!
  Serial.printf("[player] set volume: %d\n", volume_0_127);
}

uint8_t playerGetVolume() {
  return currentVolume;
}

BluetoothA2DPSource* getA2DPSource() {
  return &a2dp_source;
}
