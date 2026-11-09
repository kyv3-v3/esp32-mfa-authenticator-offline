#include "mfa.h"
#include "secret_input.hpp"
#include "totp_core.hpp"
#include "vault.hpp"
#include "clock.hpp"
#include <ArduinoJson.h>
#include <Base32.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <stdlib.h>
#include <string.h>

static const char *STORE_PATH = "/totps.json";
static const char *BACKUP_PATH = "/totps.bak";
struct StoredService {
  char name[MAX_SERVICE_NAME_LENGTH];
  char secret[MAX_SECRET_TEXT + 1];
  uint8_t group, algorithm, digits, period, type;
  bool favorite;
  uint64_t counter;
};
static StoredService stored[MAX_NUMBER_OF_SERVICES];
static size_t stored_count = 0;
static char group_names[MAX_NUMBER_OF_GROUPS][21] = {};
static bool security_loaded = false;
static uint8_t failed_attempts = 0;
static uint32_t lock_until = 0;

struct Secret { int length; uint8_t *value; };
static Secret decode_secret(const char *encoded) {
  uint8_t *value = nullptr;
  int length = Base32().fromBase32((uint8_t *)encoded, strlen(encoded), value);
  return {length, value};
}

static uint32_t failure_delay(uint8_t failures) {
  if (!failures) return 0;
  uint8_t shift = failures > 8 ? 8 : failures - 1;
  uint32_t seconds = 2UL << shift;
  return seconds > 300 ? 300 : seconds;
}
static void load_security() {
  if (security_loaded) return;
  Preferences prefs; prefs.begin("mfa-sec", true); failed_attempts = prefs.getUChar("fails", 0); prefs.end();
  lock_until = millis() + failure_delay(failed_attempts) * 1000UL;
  security_loaded = true;
}
static void save_failures() {
  Preferences prefs; prefs.begin("mfa-sec", false); prefs.putUChar("fails", failed_attempts); prefs.end();
}
static void register_failure() {
  load_security(); if (failed_attempts < 12) ++failed_attempts; save_failures();
  lock_until = millis() + failure_delay(failed_attempts) * 1000UL;
}
static void clear_failures() { load_security(); failed_attempts = 0; lock_until = 0; save_failures(); }

static void reset_groups() { memset(group_names, 0, sizeof(group_names)); strcpy(group_names[0], "Main"); }
static bool put_record(const char *name, const char *secret, int group, uint8_t algorithm = 1, uint8_t digits = 6, uint8_t period = 30, uint8_t type = SERVICE_TOTP, uint64_t counter = 0, bool favorite = false) {
  if (!name || !name[0] || strlen(name) >= MAX_SERVICE_NAME_LENGTH || group < 0 || group >= MAX_NUMBER_OF_GROUPS || algorithm < 1 || algorithm > 3 || (digits != 6 && digits != 8) || period < 15 || period > 120 || type > SERVICE_STEAM) return false;
  char normalized[MAX_SECRET_TEXT + 1]; if (!normalize_secret(secret, normalized)) return false;
  if (!group_names[group][0]) snprintf(group_names[group], 21, "Group %d", group);
  for (size_t i = 0; i < stored_count; ++i) if (stored[i].group == group && !strcmp(stored[i].name, name)) {
    strcpy(stored[i].secret, normalized); stored[i].algorithm = algorithm; stored[i].digits = digits; stored[i].period = period; stored[i].type = type; stored[i].counter = counter; stored[i].favorite = favorite; return true;
  }
  if (stored_count >= MAX_NUMBER_OF_SERVICES) return false;
  StoredService &item = stored[stored_count++]; strcpy(item.name, name); strcpy(item.secret, normalized); item.group = group; item.algorithm = algorithm; item.digits = digits; item.period = period; item.type = type; item.counter = counter; item.favorite = favorite; return true;
}
static void sort_favorites() {
  for (size_t i = 1; i < stored_count; ++i) {
    StoredService item = stored[i]; size_t j = i;
    while (j && item.favorite && !stored[j - 1].favorite) { stored[j] = stored[j - 1]; --j; }
    stored[j] = item;
  }
}
static bool read_document(JsonDocument &doc) {
  if (!doc["services"].is<JsonArray>()) return false;
  stored_count = 0;
  reset_groups();
  int group = 0;
  if (doc["groups"].is<JsonArray>()) for (JsonVariant value : doc["groups"].as<JsonArray>()) {
    if (group >= MAX_NUMBER_OF_GROUPS) break;
    const char *name = value.as<const char *>();
    if (name) { strncpy(group_names[group], name, 20); group_names[group][20] = 0; }
    ++group;
  }
  for (JsonObject item : doc["services"].as<JsonArray>()) if (!put_record(item["name"] | "", item["secret"] | "", item["group"] | 0, item["algorithm"] | 1, item["digits"] | 6, item["period"] | 30, item["type"] | SERVICE_TOTP, item["counter"] | 0ULL, item["favorite"] | false)) return false;
  sort_favorites();
  return true;
}
static bool parse_store(Stream &stream) { JsonDocument doc; return !deserializeJson(doc, stream) && read_document(doc); }
static bool parse_store(const String &payload) { JsonDocument doc; return !deserializeJson(doc, payload) && read_document(doc); }
static String serialize_store() {
  JsonDocument doc; JsonArray groups = doc["groups"].to<JsonArray>(); for (int i = 0; i < MAX_NUMBER_OF_GROUPS; ++i) groups.add(group_names[i]);
  JsonArray list = doc["services"].to<JsonArray>(); for (size_t i = 0; i < stored_count; ++i) { JsonObject item = list.add<JsonObject>(); item["name"] = stored[i].name; item["secret"] = stored[i].secret; item["group"] = stored[i].group; item["algorithm"] = stored[i].algorithm; item["digits"] = stored[i].digits; item["period"] = stored[i].period; item["type"] = stored[i].type; item["counter"] = stored[i].counter; item["favorite"] = stored[i].favorite; }
  String out; serializeJson(doc, out); return out;
}
static bool import_legacy(const char *path) { File f = SPIFFS.open(path, FILE_READ); if (!f) return false; bool ok = parse_store(f); f.close(); return ok; }
static bool persist() { return vault_is_unlocked() && vault_save(serialize_store()); }
static bool rebuild_services() {
  uint8_t previous = get_active_group(); clear_all_services();
  for (uint8_t group = 0; group < MAX_NUMBER_OF_GROUPS; ++group) if (group_names[group][0]) ensure_group_exists(group);
  for (size_t i = 0; i < stored_count; ++i) { Secret decoded = decode_secret(stored[i].secret); if (decoded.length <= 0 || !decoded.value || !upsert_service_by_name_and_group(stored[i].name, stored[i].group, decoded.length, decoded.value, stored[i].algorithm, stored[i].digits, stored[i].period, stored[i].type, stored[i].favorite, stored[i].counter)) { free(decoded.value); return false; } }
  if (!set_active_group(previous)) set_active_group(0);
  return true;
}
bool mfa_vault_exists() { return vault_exists(); }
bool mfa_create_vault(const char *pin) {
  stored_count = 0; reset_groups(); bool loaded = (SPIFFS.exists(STORE_PATH) && import_legacy(STORE_PATH)) || (SPIFFS.exists(BACKUP_PATH) && import_legacy(BACKUP_PATH));
  if (!loaded) { stored_count = 0; reset_groups(); }
  if (!vault_create(pin, serialize_store()) || !rebuild_services()) return false;
  SPIFFS.remove(STORE_PATH);
  SPIFFS.remove(BACKUP_PATH);
  return true;
}
uint32_t mfa_unlock_wait_seconds() { load_security(); int32_t remaining = static_cast<int32_t>(lock_until - millis()); return remaining > 0 ? (remaining + 999) / 1000 : 0; }
bool mfa_unlock_vault(const char *pin) {
  if (mfa_unlock_wait_seconds()) return false;
  String plain; bool ok = vault_unlock(pin, plain);
  if (!ok && vault_decoy_exists()) ok = vault_unlock_decoy(pin, plain);
  if (!ok || !parse_store(plain)) { vault_lock(); register_failure(); return false; }
  plain = ""; clear_failures(); return rebuild_services();
}
void mfa_lock_vault() { clear_all_services(); memset(stored, 0, sizeof(stored)); stored_count = 0; vault_lock(); }
bool mfa_vault_unlocked() { return vault_is_unlocked(); }
bool mfa_change_pin(const char *pin) { return vault_change_pin(pin, serialize_store()); }
bool mfa_set_duress_pin(const char *pin) {
  if (!vault_is_unlocked() || vault_is_decoy()) return false;
  String probe; if (vault_unlock(pin, probe)) return false;
  return vault_create_decoy(pin, "{\"services\":[]}");
}
bool mfa_export_backup(String &out) { return vault_export_base64(out); }
bool mfa_restore_backup(const char *pin, const String &data) { String plain; return vault_restore_base64(pin, data, plain) && parse_store(plain) && rebuild_services(); }
bool mfa_factory_reset() { mfa_lock_vault(); reset_groups(); clear_failures(); return vault_factory_reset(); }
const char *add_service_full(const char *name, const char *secret, uint8_t group, uint8_t algorithm, uint8_t digits, uint8_t period, uint8_t type, uint64_t counter, bool favorite) {
  if (!vault_is_unlocked()) return "Vault locked";
  StoredService old[MAX_NUMBER_OF_SERVICES];
  memcpy(old, stored, sizeof(stored));
  size_t previous = stored_count;
  if (!put_record(name, secret, group, algorithm, digits, period, type, counter, favorite)) return "Invalid authenticator data";
  sort_favorites();
  if (!persist()) { memcpy(stored, old, sizeof(stored)); stored_count = previous; return "Save failed"; } return rebuild_services() ? nullptr : "Not enough memory";
}
const char *add_service_from_touch(const char *name, const char *secret, uint8_t group) { return add_service_full(name, secret, group, 1, 6, 30, SERVICE_TOTP, 0, false); }
const char *remove_service_from_touch(const char *name, uint8_t group) {
  for (size_t i = 0; i < stored_count; ++i) if (stored[i].group == group && !strcmp(stored[i].name, name)) {
    StoredService old = stored[i]; memmove(&stored[i], &stored[i + 1], (stored_count - i - 1) * sizeof(StoredService)); --stored_count;
    if (!persist()) { memmove(&stored[i + 1], &stored[i], (stored_count - i) * sizeof(StoredService)); stored[i] = old; ++stored_count; return "Save failed"; } rebuild_services(); return nullptr; }
  return "Service not found";
}
bool mfa_edit_service(size_t index, const char *name, uint8_t group, uint8_t algorithm, uint8_t digits, uint8_t period, uint8_t type, uint64_t counter, bool favorite) {
  if (index >= stored_count || !name || !name[0] || strlen(name) >= MAX_SERVICE_NAME_LENGTH || group >= MAX_NUMBER_OF_GROUPS || algorithm < 1 || algorithm > 3 || (digits != 6 && digits != 8) || period < 15 || period > 120 || type > SERVICE_STEAM) return false;
  StoredService old[MAX_NUMBER_OF_SERVICES]; memcpy(old, stored, sizeof(stored)); strncpy(stored[index].name, name, MAX_SERVICE_NAME_LENGTH - 1); stored[index].name[MAX_SERVICE_NAME_LENGTH - 1] = 0; stored[index].group = group; stored[index].algorithm = algorithm; stored[index].digits = digits; stored[index].period = period; stored[index].type = type; stored[index].counter = counter; stored[index].favorite = favorite;
  if (!group_names[group][0]) snprintf(group_names[group], 21, "Group %u", group);
  sort_favorites();
  if (!persist()) { memcpy(stored, old, sizeof(stored)); return false; }
  return rebuild_services();
}
bool mfa_move_service(size_t index, int direction) {
  int target = static_cast<int>(index) + (direction < 0 ? -1 : 1); if (index >= stored_count || !direction || target < 0 || target >= static_cast<int>(stored_count)) return false;
  StoredService temp = stored[index]; stored[index] = stored[target]; stored[target] = temp; if (!persist()) { temp = stored[index]; stored[index] = stored[target]; stored[target] = temp; return false; } return rebuild_services();
}
bool mfa_set_favorite(size_t index, bool favorite) {
  if (index >= stored_count) return false;
  StoredService old[MAX_NUMBER_OF_SERVICES];
  memcpy(old, stored, sizeof(stored));
  stored[index].favorite = favorite; sort_favorites();
  if (!persist()) { memcpy(stored, old, sizeof(stored)); return false; }
  return rebuild_services();
}
bool mfa_increment_hotp(size_t index) {
  if (index >= stored_count || stored[index].type != SERVICE_HOTP) return false;
  ++stored[index].counter; if (!persist()) { --stored[index].counter; return false; }
  return rebuild_services();
}
size_t mfa_service_count() { return stored_count; }
const char *mfa_group_name(uint8_t group) { return group < MAX_NUMBER_OF_GROUPS ? group_names[group] : ""; }
bool mfa_rename_group(uint8_t group, const char *name) { if (group >= MAX_NUMBER_OF_GROUPS || !name || !name[0] || strlen(name) > 20) return false; char old[21]; strcpy(old, group_names[group]); strcpy(group_names[group], name); if (!persist()) { strcpy(group_names[group], old); return false; } return true; }
int mfa_create_group(const char *name) { for (int i = 0; i < MAX_NUMBER_OF_GROUPS; ++i) if (!group_names[i][0]) { strncpy(group_names[i], name && name[0] ? name : "New group", 20); group_names[i][20] = 0; if (persist()) return i; group_names[i][0] = 0; return -1; } return -1; }
bool mfa_delete_group(uint8_t group) { if (!group || group >= MAX_NUMBER_OF_GROUPS || !group_names[group][0]) return false; for (size_t i = 0; i < stored_count; ++i) if (stored[i].group == group) stored[i].group = 0; group_names[group][0] = 0; return persist() && rebuild_services(); }

static String decode_url(const String &value) { String out; out.reserve(value.length()); for (size_t i = 0; i < value.length(); ++i) { if (value[i] == '%' && i + 2 < value.length()) { char hex[3] = {value[i + 1], value[i + 2], 0}; out += static_cast<char>(strtol(hex, nullptr, 16)); i += 2; } else out += value[i] == '+' ? ' ' : value[i]; } return out; }
static String query_value(const String &query, const char *key) { String prefix = String(key) + "="; int start = 0; while (start < static_cast<int>(query.length())) { int end = query.indexOf('&', start); if (end < 0) end = query.length(); String part = query.substring(start, end); if (part.startsWith(prefix)) return decode_url(part.substring(prefix.length())); start = end + 1; } return ""; }
const char *mfa_import_otpauth(const char *uri) {
  if (!uri) return "Missing URI";
  String value(uri);
  bool hotp = value.startsWith("otpauth://hotp/");
  if (!hotp && !value.startsWith("otpauth://totp/")) return "Invalid otpauth URI";
  int q = value.indexOf('?');
  if (q < 0) return "URI has no parameters";
  String label = decode_url(value.substring(15, q)), query = value.substring(q + 1), secret = query_value(query, "secret"), issuer = query_value(query, "issuer"), alg = query_value(query, "algorithm"), digit_text = query_value(query, "digits"), period_text = query_value(query, "period"), counter_text = query_value(query, "counter"), encoder = query_value(query, "encoder");
  if (issuer.length() && label.startsWith(issuer + ":")) label = label.substring(issuer.length() + 1);
  uint8_t algorithm = alg.equalsIgnoreCase("SHA256") ? 2 : alg.equalsIgnoreCase("SHA512") ? 3 : 1;
  uint8_t digits = digit_text.toInt() == 8 ? 8 : 6;
  int p = period_text.length() ? period_text.toInt() : 30;
  bool steam = encoder.equalsIgnoreCase("steam") || issuer.equalsIgnoreCase("steam") || digit_text.toInt() == 5;
  uint8_t type = hotp ? SERVICE_HOTP : (steam ? SERVICE_STEAM : SERVICE_TOTP);
  return add_service_full(label.c_str(), secret.c_str(), get_active_group(), algorithm, digits, p, type, strtoull(counter_text.c_str(), nullptr, 10), false);
}
String mfa_public_json() { JsonDocument doc; JsonArray list = doc["services"].to<JsonArray>(); for (size_t i = 0; i < stored_count; ++i) { JsonObject item = list.add<JsonObject>(); item["index"] = i; item["name"] = stored[i].name; item["group"] = stored[i].group; item["algorithm"] = stored[i].algorithm; item["digits"] = stored[i].digits; item["period"] = stored[i].period; item["type"] = stored[i].type; item["counter"] = stored[i].counter; item["favorite"] = stored[i].favorite; } JsonArray groups = doc["groups"].to<JsonArray>(); for (int i = 0; i < MAX_NUMBER_OF_GROUPS; ++i) groups.add(group_names[i]); String out; serializeJson(doc, out); return out; }
void update_totps() {
  uint64_t epoch = get_epoch(); Service *services = get_services(); uint8_t group = get_active_group();
  for (int i = 0; i < MAX_NUMBER_OF_SERVICES; ++i) {
    Service *s = &services[i]; if (!s->name[0] || s->group != group || !s->secret.value) continue; char code[MAX_TOTP_LENGTH] = {}; bool ok = false;
    if (s->type == SERVICE_HOTP) ok = generate_hotp(s->secret.value, s->secret.length, s->counter, s->digits, static_cast<TotpAlgorithm>(s->algorithm), code);
    else if (s->type == SERVICE_STEAM) ok = generate_steam(s->secret.value, s->secret.length, epoch, code);
    else ok = generate_totp(s->secret.value, s->secret.length, epoch, s->period, s->digits, static_cast<TotpAlgorithm>(s->algorithm), code);
    if (ok) update_service_totp_in_active_services_group_by_name(s->name, code);
  }
}
