#include "clock.hpp"
#include "clock_input.hpp"
#include <ESP32Time.h>
#include <cstdlib>

ESP32Time rtc;
RTC_DATA_ATTR static uint32_t clock_marker = 0;
static constexpr uint32_t CLOCK_MARKER = 0x43594d46;

int set_clock_utc(const char *digits)
{
  ClockInput input;
  if (!parse_clock_input(digits, input))
    return 0;

  setenv("TZ", "UTC0", 1);
  tzset();
  rtc.setTime(input.second, input.minute, input.hour,
              input.day, input.month, input.year);
  clock_marker = CLOCK_MARKER;
  return 1;
}

int clock_is_set()
{
  return clock_marker == CLOCK_MARKER && time(nullptr) >= 1577836800;
}

int get_second()
{
  return rtc.getSecond();
}

unsigned long get_epoch() { return rtc.getEpoch(); }

void set_clock_epoch(unsigned long epoch)
{
  rtc.setTime(epoch);
  clock_marker = CLOCK_MARKER;
}
