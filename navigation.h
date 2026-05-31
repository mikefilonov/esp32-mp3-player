#pragma once

#include <Arduino.h>

class NavigationController {
  public:
    virtual void onSelectButtonPress() = 0;
    virtual void onLeftButtonPress()   = 0;
    virtual void onRightButtonPress()  = 0;
    virtual void onUpButtonPress()     = 0;
    virtual void onDownButtonPress()   = 0;
    virtual void onWheelSpin(int8_t delta) = 0; // reserved for seek
};

void navigationSetup();
void navigationLoop(NavigationController* ctr);
void navigationSetLedColor(uint8_t r, uint8_t g, uint8_t b);
bool navigationIsButtonPressed();


