#pragma once

#include "navigation.h"
#include "player.h"
#include "sdcard.h"

// Actions that are safe to set from any task (BT callback, ISR, main loop).
// Heavy work (SD I/O, heap alloc) is deferred to loop() on the main task.
enum class ControllerAction : uint8_t {
  NONE,
  NEXT,
  PREV,
  PLAY_PAUSE,
  VOL_UP,
  VOL_DOWN,
  STOP,
};

class Mp3PlayerController :
    public NavigationController,
    public PlayerEvents {

  protected:
    char     currentFile[256] = {0};
    volatile ControllerAction pendingAction = ControllerAction::NONE;

    unsigned long lastSdCheck   = 0;
    bool          wasSdPresent  = false;
    bool          pairingModeActive = false;

    void doNext() {
      char nextFile[256] = {0};
      if (sdFindNextFile(nextFile, sizeof(nextFile), currentFile)) {
        strncpy(currentFile, nextFile, sizeof(currentFile) - 1);
        playerStartFile(currentFile);
        navigationSetLedColor(0, 255, 0); // Green when playing
      }
    }

    void doPrev() {
      char prevFile[256] = {0};
      if (sdFindNextFile(prevFile, sizeof(prevFile), currentFile, /*backwards=*/true)) {
        strncpy(currentFile, prevFile, sizeof(currentFile) - 1);
        playerStartFile(currentFile);
        navigationSetLedColor(0, 255, 0); // Green when playing
      }
    }

    void doPlayPause() {
      if (playerIsStopped()) {
        if (strnlen(currentFile, sizeof(currentFile)) == 0) {
          sdFindNextFile(currentFile, sizeof(currentFile), "");
        }
        if (strnlen(currentFile, sizeof(currentFile)) > 0) {
          playerStartFile(currentFile);
          navigationSetLedColor(0, 255, 0); // Green when playing
        }
      } else {
        bool nextPaused = !playerIsPaused();
        playerPause(nextPaused);
        if (nextPaused) {
          navigationSetLedColor(255, 128, 0); // Orange when paused
        } else {
          navigationSetLedColor(0, 255, 0);   // Green when playing
        }
      }
    }

  public:
    void setPairingModeActive(bool active) {
      pairingModeActive = active;
    }

    // ── Call from Arduino loop() — executes any pending action on the main task
    void loop() {
      // 1. Periodically check SD card status
      unsigned long now = millis();
      if (now - lastSdCheck >= 1000 || lastSdCheck == 0) {
        lastSdCheck = now;
        bool isSdPresent = sdCheckConnection();
        if (isSdPresent != wasSdPresent) {
          wasSdPresent = isSdPresent;
          if (isSdPresent) {
            Serial.println("[ctrl] SD card inserted!");
            // Auto start playback if Bluetooth is already connected
            if (playerIsConnected()) {
              pendingAction = ControllerAction::PLAY_PAUSE;
            } else {
              // Revert to Blue if BT is disconnected, UNLESS we are in pairing mode (Purple)
              if (pairingModeActive) {
                navigationSetLedColor(255, 0, 255); // Keep it Purple for pairing
              } else {
                navigationSetLedColor(0, 0, 255);   // Blue for normal reconnecting
              }
            }
          } else {
            Serial.println("[ctrl] SD card removed!");
            playerStop();
            memset(currentFile, 0, sizeof(currentFile)); // Clear old filename!
            navigationSetLedColor(255, 255, 0);          // Yellow indicating missing storage
          }
        }
      }

      // 2. Dispatch pending actions
      ControllerAction action = pendingAction;
      if (action == ControllerAction::NONE) return;
      pendingAction = ControllerAction::NONE;

      // Disable any media/playback/volume navigation until a connection is established
      if (action != ControllerAction::STOP && !playerIsConnected()) {
        Serial.println("[ctrl] Action ignored — waiting for Bluetooth connection...");
        return;
      }

      // Disable playback navigation if SD card is not present
      if (action != ControllerAction::STOP && !wasSdPresent) {
        Serial.println("[ctrl] Action ignored — SD card not present");
        return;
      }

      switch (action) {
        case ControllerAction::NEXT:       doNext();     break;
        case ControllerAction::PREV:       doPrev();     break;
        case ControllerAction::PLAY_PAUSE: doPlayPause(); break;
        case ControllerAction::VOL_UP: {
          uint8_t newVol = (uint8_t)min(127, (int)playerGetVolume() + 8);
          playerSetVolume(newVol);
          break;
        }
        case ControllerAction::VOL_DOWN: {
          uint8_t newVol = (uint8_t)max(0, (int)playerGetVolume() - 8);
          playerSetVolume(newVol);
          break;
        }
        case ControllerAction::STOP:
          playerStop();
          if (!playerIsConnected()) {
            navigationSetLedColor(0, 0, 255); // Keep it Blue when disconnected/reconnecting
          } else {
            navigationSetLedColor(255, 0, 0); // Red when stopped but connected
          }
          break;
        default: break;
      }
    }

    // ── NavigationController — safe to call from any task, just sets a flag ──

    void onSelectButtonPress() override {
      Serial.println("[ctrl] select");
      pendingAction = ControllerAction::PLAY_PAUSE;
    }

    void onLeftButtonPress() override {
      Serial.println("[ctrl] prev");
      pendingAction = ControllerAction::PREV;
    }

    void onRightButtonPress() override {
      Serial.println("[ctrl] next");
      pendingAction = ControllerAction::NEXT;
    }

    void onUpButtonPress() override {
      pendingAction = ControllerAction::VOL_UP;
    }

    void onDownButtonPress() override {
      pendingAction = ControllerAction::VOL_DOWN;
    }

    void onWheelSpin(int8_t delta) override {
      // TODO: implement seek
      (void)delta;
    }

    // ── PlayerEvents — called from player task, defer heavy work ─────────────

    void onTrackFinished() override {
      Serial.println("[ctrl] track finished — advancing");
      pendingAction = ControllerAction::NEXT;
    }

    void onBTConnected() override {
      Serial.println("[ctrl] BT connected — starting first track");
      pendingAction = ControllerAction::NEXT;
      pairingModeActive = false; // Successfully connected, clear pairing mode
    }

    void onBTDisconnected() override {
      Serial.println("[ctrl] BT disconnected");
      pendingAction = ControllerAction::STOP;
      navigationSetLedColor(0, 0, 255); // Blue when BT is disconnected
    }

};

