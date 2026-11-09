#pragma once
#include <Arduino.h>
#include <stdint.h>
struct DeviceSettings {
  uint8_t brightness;
  bool light_theme;
  uint16_t display_timeout;
  uint16_t auto_lock;
  bool large_text;
  bool high_contrast;
  bool reduced_motion;
};
extern DeviceSettings device_settings;
void settings_load();
bool settings_save();
String settings_to_json();
bool settings_from_json(const String &json);
