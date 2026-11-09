#include "services.h"
#include <stdlib.h>
#include <string.h>

static Service services[MAX_NUMBER_OF_SERVICES];
static uint8_t service_count, active_group, groups[MAX_NUMBER_OF_GROUPS], group_count;

static int find_service(const char *name, uint8_t group) {
  for (int i = 0; i < service_count; ++i)
    if (services[i].group == group && !strcmp(services[i].name, name)) return i;
  return -1;
}

static bool has_group(uint8_t group) {
  for (int i = 0; i < group_count; ++i) if (groups[i] == group) return true;
  return false;
}

bool ensure_group_exists(uint8_t group) {
  if (has_group(group)) return true;
  if (group_count >= MAX_NUMBER_OF_GROUPS) return false;
  int i = group_count++;
  while (i && groups[i - 1] > group) { groups[i] = groups[i - 1]; --i; }
  groups[i] = group;
  if (group_count == 1) active_group = group;
  return true;
}

bool upsert_service_by_name_and_group(const char *name, uint8_t group,
    int secret_length, uint8_t *secret, uint8_t algorithm, uint8_t digits,
    uint8_t period, uint8_t type, bool favorite, uint64_t counter) {
  if (!name || !*name || strlen(name) >= MAX_SERVICE_NAME_LENGTH ||
      secret_length <= 0 || !secret || !ensure_group_exists(group)) return false;
  int index = find_service(name, group);
  if (index < 0) {
    if (service_count >= MAX_NUMBER_OF_SERVICES) return false;
    index = service_count++;
  } else {
    free(services[index].secret.value);
  }
  Service *item = &services[index];
  strcpy(item->name, name);
  item->group = group;
  item->secret.length = secret_length;
  item->secret.value = secret;
  item->algorithm = algorithm;
  item->digits = digits;
  item->period = period;
  item->type = type;
  item->favorite = favorite;
  item->counter = counter;
  return true;
}

bool update_service_totp_in_active_services_group_by_name(const char *name, char code[]) {
  int index = find_service(name, active_group);
  if (index < 0 || !code) return false;
  strcpy(services[index].totp, code);
  return true;
}

void clear_all_services(void) {
  for (int i = 0; i < service_count; ++i) free(services[i].secret.value);
  memset(services, 0, sizeof(services));
  memset(groups, 0, sizeof(groups));
  service_count = group_count = active_group = 0;
}

uint8_t get_active_group(void) { return active_group; }
Service *get_services(void) { return services; }

bool set_active_group(uint8_t group) {
  if (!has_group(group)) return false;
  active_group = group;
  return true;
}

static bool move_group(int direction) {
  for (int i = 0; i < group_count; ++i) {
    if (groups[i] != active_group) continue;
    int next = i + direction;
    if (next < 0 || next >= group_count) return false;
    active_group = groups[next];
    return true;
  }
  return false;
}

bool change_active_group_left(void) { return move_group(-1); }
bool change_active_group_right(void) { return move_group(1); }
