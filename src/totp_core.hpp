#pragma once
#include <stdint.h>
#include <stddef.h>

enum TotpAlgorithm : uint8_t { TOTP_SHA1 = 1, TOTP_SHA256 = 2, TOTP_SHA512 = 3 };
bool generate_totp(const uint8_t *secret, size_t length, uint64_t epoch,
                   uint8_t period, uint8_t digits, TotpAlgorithm algorithm,
                   char *output);
bool generate_hotp(const uint8_t *secret, size_t length, uint64_t counter,
                   uint8_t digits, TotpAlgorithm algorithm, char *output);
bool generate_steam(const uint8_t *secret, size_t length, uint64_t epoch,
                    char *output);
bool totp_self_test();
