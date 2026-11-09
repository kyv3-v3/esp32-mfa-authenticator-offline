#ifndef SECRET_INPUT_HPP
#define SECRET_INPUT_HPP

#include <ctype.h>
#include <stddef.h>

static constexpr size_t MAX_SECRET_TEXT = 128;

inline bool normalize_secret(const char *source, char *out)
{
  if (!source)
    return false;

  size_t length = 0;
  bool padding = false;
  for (; *source; ++source)
  {
    unsigned char ch = static_cast<unsigned char>(*source);
    if (ch == ' ' || ch == '-' || ch == '\t')
      continue;
    if (ch == '=')
    {
      padding = true;
      continue;
    }
    if (padding || length == MAX_SECRET_TEXT)
      return false;
    ch = static_cast<unsigned char>(toupper(ch));
    if (!((ch >= 'A' && ch <= 'Z') || (ch >= '2' && ch <= '7')))
      return false;
    out[length++] = static_cast<char>(ch);
  }
  out[length] = '\0';
  return length >= 8 && length % 8 != 1 && length % 8 != 3 && length % 8 != 6;
}

#endif
