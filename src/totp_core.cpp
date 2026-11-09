#include "totp_core.hpp"
#include <mbedtls/md.h>
#include <stdio.h>
#include <string.h>

static bool dynamic_code(const uint8_t *secret, size_t length, uint64_t counter,
                         TotpAlgorithm algorithm, uint32_t &binary) {
  if (!secret || !length) return false;
  mbedtls_md_type_t type = algorithm == TOTP_SHA256 ? MBEDTLS_MD_SHA256 :
                           algorithm == TOTP_SHA512 ? MBEDTLS_MD_SHA512 : MBEDTLS_MD_SHA1;
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(type);
  if (!info) return false;
  uint8_t message[8];
  for (int i = 7; i >= 0; --i) { message[i] = counter & 0xff; counter >>= 8; }
  uint8_t digest[64] = {};
  if (mbedtls_md_hmac(info, secret, length, message, sizeof(message), digest) != 0) return false;
  size_t digest_length = mbedtls_md_get_size(info);
  uint8_t offset = digest[digest_length - 1] & 0x0f;
  binary = ((digest[offset] & 0x7f) << 24) |
           (digest[offset + 1] << 16) | (digest[offset + 2] << 8) | digest[offset + 3];
  return true;
}

bool generate_hotp(const uint8_t *secret, size_t length, uint64_t counter,
                   uint8_t digits, TotpAlgorithm algorithm, char *output) {
  if (!output || (digits != 6 && digits != 8)) return false;
  uint32_t binary = 0;
  if (!dynamic_code(secret, length, counter, algorithm, binary)) return false;
  uint32_t modulus = digits == 8 ? 100000000U : 1000000U;
  snprintf(output, digits + 1, digits == 8 ? "%08u" : "%06u", binary % modulus);
  return true;
}

bool generate_totp(const uint8_t *secret, size_t length, uint64_t epoch,
                   uint8_t period, uint8_t digits, TotpAlgorithm algorithm,
                   char *output) {
  return period && generate_hotp(secret, length, epoch / period, digits, algorithm, output);
}

bool generate_steam(const uint8_t *secret, size_t length, uint64_t epoch,
                    char *output) {
  static const char alphabet[] = "23456789BCDFGHJKMNPQRTVWXY";
  uint32_t value = 0;
  if (!output || !dynamic_code(secret, length, epoch / 30, TOTP_SHA1, value)) return false;
  for (int i = 0; i < 5; ++i) { output[i] = alphabet[value % 26]; value /= 26; }
  output[5] = 0;
  return true;
}

bool totp_self_test() {
  static const uint8_t sha1[] = "12345678901234567890";
  static const uint8_t sha256[] = "12345678901234567890123456789012";
  static const uint8_t sha512[] = "1234567890123456789012345678901234567890123456789012345678901234";
  char code[9];
  char steam[6];
  return generate_hotp(sha1, sizeof(sha1) - 1, 0, 6, TOTP_SHA1, code) && !strcmp(code, "755224") &&
         generate_totp(sha1, sizeof(sha1) - 1, 59, 30, 8, TOTP_SHA1, code) && !strcmp(code, "94287082") &&
         generate_totp(sha256, sizeof(sha256) - 1, 59, 30, 8, TOTP_SHA256, code) && !strcmp(code, "46119246") &&
         generate_totp(sha512, sizeof(sha512) - 1, 59, 30, 8, TOTP_SHA512, code) && !strcmp(code, "90693936") &&
         generate_steam(sha1, sizeof(sha1) - 1, 59, steam) && !strcmp(steam, "PV9M4");
}
