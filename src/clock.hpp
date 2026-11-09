#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int set_clock_utc(const char *);
int clock_is_set(void);
int get_second(void);
unsigned long get_epoch(void);
void set_clock_epoch(unsigned long);

#ifdef __cplusplus
}
#endif
