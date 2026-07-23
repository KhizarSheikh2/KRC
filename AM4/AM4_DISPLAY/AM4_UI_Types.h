#pragma once

#include <Arduino.h>

enum AM4ScreenPage : uint8_t {
  AM4_PAGE_MAIN = 0,
  AM4_PAGE_TEMPERATURES = 1
};

enum AM4CardIcon : uint8_t {
  AM4_ICON_CLOCK,
  AM4_ICON_CALENDAR,
  AM4_ICON_SYSTEM_STATUS,
  AM4_ICON_SUCTION_PRESSURE,
  AM4_ICON_DISCHARGE_PRESSURE
};

struct AM4DashboardCard {
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
  const char* title;
  uint16_t accentColor;
  AM4CardIcon icon;
};
