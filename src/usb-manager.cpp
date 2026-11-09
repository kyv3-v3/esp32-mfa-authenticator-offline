#include "usb-manager.hpp"
#include "mfa.h"
#include "clock.hpp"
#include "settings.hpp"
#include "display.hpp"
#include "ui/ui.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <Update.h>
#include <mbedtls/base64.h>

static String line;
static bool ota_active = false;
static size_t ota_expected = 0, ota_written = 0;
static void refresh_ui() { if (mfa_vault_unlocked()) { ui_totp_screen_render_totp_components(); update_totps(); ui_totp_screen_update_totp_labels(); ui_totp_screen_update_totp_countdowns(); } }
static void reply(bool ok, const char *message, const String &data = "") {
  JsonDocument out; out["ok"] = ok; out["message"] = message; if (data.length()) out["data"] = data;
  serializeJson(out, Serial); Serial.println();
}
static void execute(const String &input) {
  JsonDocument doc; if (deserializeJson(doc, input)) { reply(false, "Invalid JSON"); return; }
  const char *cmd = doc["cmd"] | "";
  if (!strcmp(cmd, "status")) { JsonDocument out; out["vault"] = mfa_vault_exists(); out["unlocked"] = mfa_vault_unlocked(); out["clock"] = clock_is_set(); out["epoch"] = get_epoch(); out["services"] = mfa_service_count(); out["wait"] = mfa_unlock_wait_seconds(); String data; serializeJson(out, data); reply(true, "CYD ready", data); }
  else if (!strcmp(cmd, "time")) { unsigned long epoch = doc["epoch"] | 0UL; if (epoch < 1577836800UL) reply(false, "Invalid epoch"); else { set_clock_epoch(epoch); reply(true, "Time synchronized"); } }
  else if (!strcmp(cmd, "list")) { if (!mfa_vault_unlocked()) reply(false, "Unlock the CYD"); else reply(true, "List", mfa_public_json()); }
  else if (!strcmp(cmd, "import_uri")) { const char *error = mfa_import_otpauth(doc["uri"] | ""); if (!error) refresh_ui(); reply(!error, error ? error : "TOTP imported"); }
  else if (!strcmp(cmd, "backup")) { String data; bool ok = mfa_vault_unlocked() && mfa_export_backup(data); reply(ok, ok ? "Encrypted backup" : "Unlock the CYD", data); }
  else if (!strcmp(cmd, "restore")) { bool ok = mfa_restore_backup(doc["pin"] | "", doc["data"] | ""); if (ok) refresh_ui(); reply(ok, ok ? "Backup restored" : "Invalid backup or PIN"); }
  else if (!strcmp(cmd, "change_pin")) { bool ok = mfa_change_pin(doc["pin"] | ""); reply(ok, ok ? "PIN updated" : "Invalid PIN"); }
  else if (!strcmp(cmd, "edit")) { bool ok = mfa_edit_service(doc["index"] | 999, doc["name"] | "", doc["group"] | 0, doc["algorithm"] | 1, doc["digits"] | 6, doc["period"] | 30, doc["type"] | SERVICE_TOTP, doc["counter"] | 0ULL, doc["favorite"] | false); if (ok) refresh_ui(); reply(ok, ok ? "Service updated" : "Update failed"); }
  else if (!strcmp(cmd, "favorite")) { bool ok = mfa_set_favorite(doc["index"] | 999, doc["favorite"] | true); if (ok) refresh_ui(); reply(ok, ok ? "Favorite updated" : "Update failed"); }
  else if (!strcmp(cmd, "move")) { bool ok = mfa_move_service(doc["index"] | 999, doc["direction"] | 0); if (ok) refresh_ui(); reply(ok, ok ? "Order updated" : "Move failed"); }
  else if (!strcmp(cmd, "group_create")) { int id = mfa_create_group(doc["name"] | "New group"); reply(id >= 0, id >= 0 ? "Group created" : "Creation failed", id >= 0 ? String(id) : ""); }
  else if (!strcmp(cmd, "group_rename")) { bool ok = mfa_rename_group(doc["group"] | 0, doc["name"] | ""); reply(ok, ok ? "Group renamed" : "Update failed"); }
  else if (!strcmp(cmd, "group_delete")) { bool ok = mfa_delete_group(doc["group"] | 0); reply(ok, ok ? "Group deleted" : "Deletion failed"); }
  else if (!strcmp(cmd, "settings")) { device_settings.brightness = constrain(doc["brightness"] | device_settings.brightness, 10, 100); device_settings.light_theme = doc["light_theme"] | device_settings.light_theme; device_settings.display_timeout = constrain(doc["display_timeout"] | device_settings.display_timeout, 0, 600); device_settings.auto_lock = constrain(doc["auto_lock"] | device_settings.auto_lock, 30, 3600); device_settings.large_text = doc["large_text"] | device_settings.large_text; device_settings.high_contrast = doc["high_contrast"] | device_settings.high_contrast; device_settings.reduced_motion = doc["reduced_motion"] | device_settings.reduced_motion; settings_save(); display_apply_settings(); refresh_ui(); reply(true, "Settings saved"); }
  else if (!strcmp(cmd, "duress_pin")) { bool ok = mfa_set_duress_pin(doc["pin"] | ""); reply(ok, ok ? "Duress PIN configured" : "PIN is invalid or matches the primary PIN"); }
  else if (!strcmp(cmd, "diagnostics")) { JsonDocument out; out["firmware"] = "2.0-offline"; out["built"] = __DATE__ " " __TIME__; out["uptime_seconds"] = millis() / 1000UL; out["free_heap"] = ESP.getFreeHeap(); out["min_free_heap"] = ESP.getMinFreeHeap(); out["flash_size"] = ESP.getFlashChipSize(); out["sketch_size"] = ESP.getSketchSize(); out["free_sketch"] = ESP.getFreeSketchSpace(); out["spiffs_total"] = SPIFFS.totalBytes(); out["spiffs_used"] = SPIFFS.usedBytes(); out["services"] = mfa_service_count(); out["clock"] = clock_is_set(); String data; serializeJson(out, data); reply(true, "Diagnostics ready", data); }
  else if (!strcmp(cmd, "emergency_export")) { String backup; bool ok = mfa_vault_unlocked() && mfa_export_backup(backup); if (!ok) reply(false, "Unlock the CYD"); else { JsonDocument pkg; pkg["format"] = "CYD-EMERGENCY"; pkg["version"] = 1; pkg["backup"] = backup; pkg["settings"] = settings_to_json(); String data; serializeJson(pkg, data); reply(true, "Emergency package ready", data); } }
  else if (!strcmp(cmd, "emergency_restore")) { JsonObject pkg = doc["package"].as<JsonObject>(); String plain; bool ok = !strcmp(pkg["format"] | "", "CYD-EMERGENCY") && mfa_restore_backup(doc["pin"] | "", pkg["backup"] | ""); if (ok && pkg["settings"].is<const char *>()) settings_from_json(pkg["settings"].as<String>()); if (ok) { display_apply_settings(); refresh_ui(); } reply(ok, ok ? "Package restored" : "Invalid package or PIN"); }
  else if (!strcmp(cmd, "fw_begin")) { ota_expected = doc["size"] | 0; ota_written = 0; ota_active = ota_expected > 0 && Update.begin(ota_expected, U_FLASH); reply(ota_active, ota_active ? "Update started" : "Not enough OTA space"); }
  else if (!strcmp(cmd, "fw_chunk")) { String encoded = doc["data"] | ""; size_t needed = 0; mbedtls_base64_decode(nullptr, 0, &needed, (const unsigned char *)encoded.c_str(), encoded.length()); uint8_t *raw = (uint8_t *)malloc(needed ? needed : 1); size_t length = 0; bool ok = ota_active && raw && mbedtls_base64_decode(raw, needed, &length, (const unsigned char *)encoded.c_str(), encoded.length()) == 0 && Update.write(raw, length) == length; if (ok) ota_written += length; free(raw); if (!ok) { Update.abort(); ota_active = false; } reply(ok, ok ? "Firmware chunk received" : "Firmware write failed", String(ota_written)); }
  else if (!strcmp(cmd, "fw_end")) { bool ok = ota_active && ota_written == ota_expected && Update.end(true); ota_active = false; reply(ok, ok ? "Firmware updated: restarting" : "Firmware is incomplete or invalid"); if (ok) { Serial.flush(); delay(300); ESP.restart(); } }
  else if (!strcmp(cmd, "lock")) { request_device_lock(); reply(true, "Vault locked"); }
  else if (!strcmp(cmd, "factory_reset") && !strcmp(doc["confirm"] | "", "ERASE")) { bool ok = mfa_factory_reset(); reply(ok, ok ? "Vault erased" : "Reset failed"); ESP.restart(); }
  else reply(false, "Unknown command");
}
void usb_manager_begin() { Serial.begin(115200); line.reserve(8192); }
void usb_manager_poll() { while (Serial.available()) { char c = Serial.read(); if (c == '\n') { if (line.length()) execute(line); line = ""; } else if (c != '\r' && line.length() < 100000) line += c; } }
