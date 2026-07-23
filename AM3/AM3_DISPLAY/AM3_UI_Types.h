#pragma once

#include <Arduino.h>

enum AM3ScreenPage : uint8_t {
  AM3_PAGE_MAIN = 0,
  AM3_PAGE_TEMPERATURES = 1
};

enum AM3CardIcon : uint8_t {
  AM3_ICON_CLOCK,
  AM3_ICON_CALENDAR,
  AM3_ICON_SYSTEM_STATUS,
  AM3_ICON_SUCTION_PRESSURE,
  AM3_ICON_DISCHARGE_PRESSURE
};

struct AM3DashboardCard {
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
  const char* title;
  uint16_t accentColor;
  AM3CardIcon icon;
};
