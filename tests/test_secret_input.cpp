#include "../src/secret_input.hpp"
#include <cassert>
#include <cstring>

int main()
{
  char normalized[MAX_SECRET_TEXT + 1];
  assert(normalize_secret("jbswy3dp ehpk3pxp", normalized));
  assert(std::strcmp(normalized, "JBSWY3DPEHPK3PXP") == 0);
  assert(normalize_secret("JBSWY3DPEHPK3PXP====", normalized));
  assert(!normalize_secret("JBSWY3DP0HPK3PXP", normalized));
  assert(!normalize_secret("JBSWY3DP=EHPK3PXP", normalized));
  assert(!normalize_secret("ABCDEF2", normalized));
}
