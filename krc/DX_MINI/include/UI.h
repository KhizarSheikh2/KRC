#pragma once

#include <Arduino.h>

enum UiAction : uint8_t {
  UI_ACTION_NONE = 0,
  UI_ACTION_TOGGLE_POWER,
  UI_ACTION_TOGGLE_MODE,
  UI_ACTION_DISCHARGE_MINUS,
  UI_ACTION_DISCHARGE_PLUS,
  UI_ACTION_RETURN_MINUS,
  UI_ACTION_RETURN_PLUS,
  UI_ACTION_TOGGLE_EVAP,
  UI_ACTION_TOGGLE_WASH,
  UI_ACTION_TOGGLE_UNIT
};

void uiInit();
void uiLoop();
UiAction uiTakeAction();
void updateDisplay();

void showStartupScreen();
void uiSetBootProgress(int value, const char *status);
void uiShowMain();
void showCenteredText(const char *text, int seconds = 1);
