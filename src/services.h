#pragma once
#include "constants.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { SERVICE_TOTP, SERVICE_HOTP, SERVICE_STEAM };

typedef struct {
  char name[MAX_SERVICE_NAME_LENGTH];
  struct { int length; uint8_t *value; } secret;
  uint8_t group, algorithm, digits, period, type;
  bool favorite;
  uint64_t counter;
  char totp[MAX_TOTP_LENGTH];
} Service;

bool upsert_service_by_name_and_group(const char *, uint8_t, int, uint8_t *,
  uint8_t, uint8_t, uint8_t, uint8_t, bool, uint64_t);
bool update_service_totp_in_active_services_group_by_name(const char *, char []);
void clear_all_services(void);
uint8_t get_active_group(void);
bool set_active_group(uint8_t);
Service *get_services(void);
bool change_active_group_left(void);
bool change_active_group_right(void);
bool ensure_group_exists(uint8_t);

#ifdef __cplusplus
}
#endif
