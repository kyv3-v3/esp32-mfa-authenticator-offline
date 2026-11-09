#ifndef MFA_H
#define MFA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void update_totps();
bool mfa_vault_exists();
bool mfa_vault_unlocked();
bool mfa_create_vault(const char *pin);
bool mfa_unlock_vault(const char *pin);
uint32_t mfa_unlock_wait_seconds();
bool mfa_increment_hotp(size_t index);
void mfa_lock_vault();
bool mfa_factory_reset();
#ifdef __cplusplus
}

#include <Arduino.h>
#include "services.h"
bool mfa_change_pin(const char *pin);
bool mfa_set_duress_pin(const char *pin);
bool mfa_export_backup(String &out);
bool mfa_restore_backup(const char *pin, const String &data);
const char *add_service_from_touch(const char *name, const char *secret, uint8_t group);
const char *add_service_full(const char *name, const char *secret, uint8_t group, uint8_t algorithm, uint8_t digits, uint8_t period, uint8_t type, uint64_t counter, bool favorite);
const char *remove_service_from_touch(const char *name, uint8_t group);
const char *mfa_import_otpauth(const char *uri);
bool mfa_edit_service(size_t index, const char *name, uint8_t group, uint8_t algorithm, uint8_t digits, uint8_t period, uint8_t type, uint64_t counter, bool favorite);
bool mfa_move_service(size_t index, int direction);
bool mfa_set_favorite(size_t index, bool favorite);
size_t mfa_service_count();
const char *mfa_group_name(uint8_t group);
bool mfa_rename_group(uint8_t group, const char *name);
int mfa_create_group(const char *name);
bool mfa_delete_group(uint8_t group);
String mfa_public_json();
#endif
#endif
