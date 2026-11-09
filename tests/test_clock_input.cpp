#include "../src/clock_input.hpp"
#include <cassert>

int main()
{
  ClockInput input{};
  assert(parse_clock_input("29022024125959", input));
  assert(input.year == 2024 && input.month == 2 && input.day == 29);
  assert(!parse_clock_input("29022023125959", input));
  assert(!parse_clock_input("31112026125959", input));
  assert(!parse_clock_input("09112026240000", input));
  assert(!parse_clock_input("09112026126000", input));
  assert(!parse_clock_input("0911202612000x", input));
  assert(!parse_clock_input("0911202612000", input));
}
