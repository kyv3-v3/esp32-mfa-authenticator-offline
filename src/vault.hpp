#pragma once

#include <Arduino.h>

bool vault_exists();
bool vault_decoy_exists();
bool vault_is_unlocked();
bool vault_is_decoy();
bool vault_create(const char *pin, const String &plain);
bool vault_create_decoy(const char *pin, const String &plain);
bool vault_unlock(const char *pin, String &plain);
bool vault_unlock_decoy(const char *pin, String &plain);
bool vault_save(const String &plain);
bool vault_change_pin(const char *new_pin, const String &plain);
void vault_lock();
bool vault_export_base64(String &out);
bool vault_restore_base64(const char *pin, const String &encoded, String &plain);
bool vault_factory_reset();
