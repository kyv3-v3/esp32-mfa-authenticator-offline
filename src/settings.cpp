#include "settings.hpp"
#include <ArduinoJson.h>
#include <SPIFFS.h>

DeviceSettings device_settings = {50, false, 30, 120, false, false, false};

static void encode(JsonDocument &doc) {
  doc["brightness"] = device_settings.brightness;
  doc["light_theme"] = device_settings.light_theme;
  doc["display_timeout"] = device_settings.display_timeout;
  doc["auto_lock"] = device_settings.auto_lock;
  doc["large_text"] = device_settings.large_text;
  doc["high_contrast"] = device_settings.high_contrast;
  doc["reduced_motion"] = device_settings.reduced_motion;
}

static void decode(JsonDocument &doc) {
  device_settings.brightness = constrain(doc["brightness"] | device_settings.brightness, 10, 100);
  device_settings.light_theme = doc["light_theme"] | device_settings.light_theme;
  device_settings.display_timeout = constrain(doc["display_timeout"] | device_settings.display_timeout, 0, 600);
  device_settings.auto_lock = constrain(doc["auto_lock"] | device_settings.auto_lock, 30, 3600);
  device_settings.large_text = doc["large_text"] | device_settings.large_text;
  device_settings.high_contrast = doc["high_contrast"] | device_settings.high_contrast;
  device_settings.reduced_motion = doc["reduced_motion"] | device_settings.reduced_motion;
}

void settings_load() {
  File file = SPIFFS.open("/settings.json", FILE_READ);
  JsonDocument doc;
  if (file && !deserializeJson(doc, file)) decode(doc);
}

bool settings_save() {
  JsonDocument doc;
  encode(doc);
  File file = SPIFFS.open("/settings.json", FILE_WRITE);
  if (!file) return false;
  size_t expected = measureJson(doc), written = serializeJson(doc, file);
  file.flush();
  return written == expected;
}

String settings_to_json() {
  JsonDocument doc;
  encode(doc);
  String json;
  serializeJson(doc, json);
  return json;
}

bool settings_from_json(const String &json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  decode(doc);
  return settings_save();
}

extern "C" bool ui_reduced_motion_enabled() { return device_settings.reduced_motion; }
extern "C" bool ui_large_text_enabled() { return device_settings.large_text; }
extern "C" bool ui_high_contrast_enabled() { return device_settings.high_contrast; }
