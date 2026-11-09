#ifndef CLOCK_INPUT_HPP
#define CLOCK_INPUT_HPP

#include <cstring>

struct ClockInput
{
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

// GGMMAAAAHHMMSS, in UTC.
inline bool parse_clock_input(const char *digits, ClockInput &out)
{
  if (!digits || std::strlen(digits) != 14)
    return false;

  for (int i = 0; i < 14; ++i)
    if (digits[i] < '0' || digits[i] > '9')
      return false;

  auto number = [digits](int start, int length) {
    int value = 0;
    for (int i = 0; i < length; ++i)
      value = value * 10 + digits[start + i] - '0';
    return value;
  };

  out.day = number(0, 2);
  out.month = number(2, 2);
  out.year = number(4, 4);
  out.hour = number(8, 2);
  out.minute = number(10, 2);
  out.second = number(12, 2);

  if (out.year < 2020 || out.year > 2099 || out.month < 1 || out.month > 12 ||
      out.hour > 23 || out.minute > 59 || out.second > 59)
    return false;

  static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int max_day = days_in_month[out.month - 1];
  if (out.month == 2 && (out.year % 4 == 0) &&
      (out.year % 100 != 0 || out.year % 400 == 0))
    ++max_day;

  return out.day >= 1 && out.day <= max_day;
}

#endif
