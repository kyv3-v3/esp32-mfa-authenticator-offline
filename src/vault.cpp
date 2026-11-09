#include "vault.hpp"
#include <SPIFFS.h>
#include <esp_system.h>
#include <mbedtls/base64.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

namespace {
constexpr const char *PRIMARY_PATH = "/vault.bin";
constexpr const char *PRIMARY_TEMP = "/vault.tmp";
constexpr const char *PRIMARY_BACKUP = "/vault.bak";
constexpr const char *DECOY_PATH = "/decoy.bin";
constexpr const char *DECOY_TEMP = "/decoy.tmp";
constexpr const char *DECOY_BACKUP = "/decoy.bak";
constexpr uint8_t VERSION = 1;
constexpr uint32_t PBKDF2_ROUNDS = 50000;

struct __attribute__((packed)) Header {
  char magic[4];
  uint8_t version;
  uint8_t salt[16];
  uint8_t nonce[12];
  uint32_t length;
};

uint8_t key[32] = {};
uint8_t active_salt[16] = {};
bool unlocked = false;
bool decoy_active = false;

const char *path(bool decoy) { return decoy ? DECOY_PATH : PRIMARY_PATH; }
const char *temp_path(bool decoy) { return decoy ? DECOY_TEMP : PRIMARY_TEMP; }
const char *backup_path(bool decoy) { return decoy ? DECOY_BACKUP : PRIMARY_BACKUP; }

void wipe(void *data, size_t length) {
  volatile uint8_t *p = static_cast<volatile uint8_t *>(data);
  while (length--) *p++ = 0;
}

bool valid_pin(const char *pin) {
  if (!pin) return false;
  size_t n = strlen(pin);
  if (n < 6 || n > 20) return false;
  for (size_t i = 0; i < n; ++i) if (pin[i] < '0' || pin[i] > '9') return false;
  return true;
}

bool derive(const char *pin, const uint8_t salt[16], uint8_t out[32]) {
  if (!valid_pin(pin)) return false;
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  bool ok = info && mbedtls_md_setup(&ctx, info, 1) == 0 &&
            mbedtls_pkcs5_pbkdf2_hmac(&ctx,
              reinterpret_cast<const unsigned char *>(pin), strlen(pin),
              salt, 16, PBKDF2_ROUNDS, 32, out) == 0;
  mbedtls_md_free(&ctx);
  return ok;
}

bool encrypt_to_file(const char *path, const String &plain,
                     const uint8_t use_key[32], const uint8_t salt[16]) {
  Header h{{'C','Y','D','V'}, VERSION, {}, {}, static_cast<uint32_t>(plain.length())};
  memcpy(h.salt, salt, sizeof(h.salt));
  esp_fill_random(h.nonce, sizeof(h.nonce));
  uint8_t *cipher = static_cast<uint8_t *>(malloc(h.length ? h.length : 1));
  if (!cipher) return false;
  uint8_t tag[16] = {};
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, use_key, 256);
  if (!rc) rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
      h.length, h.nonce, sizeof(h.nonce),
      reinterpret_cast<const uint8_t *>(&h), sizeof(h),
      reinterpret_cast<const uint8_t *>(plain.c_str()), cipher,
      sizeof(tag), tag);
  mbedtls_gcm_free(&gcm);
  if (rc) { wipe(cipher, h.length); free(cipher); return false; }

  File f = SPIFFS.open(path, FILE_WRITE);
  bool ok = f && f.write(reinterpret_cast<const uint8_t *>(&h), sizeof(h)) == sizeof(h) &&
            f.write(cipher, h.length) == h.length && f.write(tag, sizeof(tag)) == sizeof(tag);
  if (f) { f.flush(); f.close(); }
  wipe(cipher, h.length); free(cipher); wipe(tag, sizeof(tag));
  return ok;
}

bool decrypt_bytes(const uint8_t *raw, size_t raw_length, const char *pin,
                   String &plain, uint8_t derived[32]) {
  if (raw_length < sizeof(Header) + 16) return false;
  Header h;
  memcpy(&h, raw, sizeof(h));
  if (memcmp(h.magic, "CYDV", 4) || h.version != VERSION ||
      h.length > 65536 || raw_length != sizeof(Header) + h.length + 16) return false;
  if (!derive(pin, h.salt, derived)) return false;
  uint8_t *decoded = static_cast<uint8_t *>(malloc(h.length + 1));
  if (!decoded) { wipe(derived, 32); return false; }
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, derived, 256);
  if (!rc) rc = mbedtls_gcm_auth_decrypt(&gcm, h.length,
      h.nonce, sizeof(h.nonce), raw, sizeof(Header),
      raw + sizeof(Header) + h.length, 16,
      raw + sizeof(Header), decoded);
  mbedtls_gcm_free(&gcm);
  if (rc) { wipe(decoded, h.length + 1); free(decoded); wipe(derived, 32); return false; }
  decoded[h.length] = 0;
  plain = String(reinterpret_cast<char *>(decoded));
  wipe(decoded, h.length + 1); free(decoded);
  return true;
}

bool read_all(const char *path, uint8_t *&raw, size_t &length) {
  File f = SPIFFS.open(path, FILE_READ);
  if (!f) return false;
  length = f.size();
  raw = static_cast<uint8_t *>(malloc(length ? length : 1));
  bool ok = raw && f.read(raw, length) == length;
  f.close();
  if (!ok) { free(raw); raw = nullptr; }
  return ok;
}
}

bool vault_exists() { return SPIFFS.exists(PRIMARY_PATH) || SPIFFS.exists(PRIMARY_BACKUP); }
bool vault_decoy_exists() { return SPIFFS.exists(DECOY_PATH) || SPIFFS.exists(DECOY_BACKUP); }
bool vault_is_unlocked() { return unlocked; }
bool vault_is_decoy() { return unlocked && decoy_active; }

static bool create_vault(const char *pin, const String &plain, bool decoy, bool activate) {
  uint8_t salt[16], candidate[32];
  esp_fill_random(salt, sizeof(salt));
  const char *target = path(decoy), *temp = temp_path(decoy), *backup = backup_path(decoy);
  if (!derive(pin, salt, candidate) || !encrypt_to_file(temp, plain, candidate, salt)) {
    wipe(candidate, sizeof(candidate)); return false;
  }
  SPIFFS.remove(backup);
  if (SPIFFS.exists(target)) SPIFFS.rename(target, backup);
  if (!SPIFFS.rename(temp, target)) { SPIFFS.rename(backup, target); SPIFFS.remove(temp); wipe(candidate, 32); return false; }
  SPIFFS.remove(backup);
  if (activate) {
    memcpy(key, candidate, 32); unlocked = true; decoy_active = decoy;
    memcpy(active_salt, salt, 16);
  }
  wipe(candidate, 32);
  return true;
}

bool vault_create(const char *pin, const String &plain) { return create_vault(pin, plain, false, true); }
bool vault_create_decoy(const char *pin, const String &plain) { return create_vault(pin, plain, true, false); }

static bool unlock_vault(const char *pin, String &plain, bool decoy) {
  uint8_t *raw = nullptr; size_t length = 0; uint8_t candidate[32];
  const char *source = SPIFFS.exists(path(decoy)) ? path(decoy) : backup_path(decoy);
  if (!read_all(source, raw, length)) return false;
  if (length < sizeof(Header)) { wipe(raw, length); free(raw); return false; }
  Header h; memcpy(&h, raw, sizeof(h));
  bool ok = decrypt_bytes(raw, length, pin, plain, candidate);
  wipe(raw, length); free(raw);
  if (!ok) return false;
  memcpy(key, candidate, 32); wipe(candidate, 32); unlocked = true; decoy_active = decoy;
  memcpy(active_salt, h.salt, 16);
  return true;
}

bool vault_unlock(const char *pin, String &plain) { return unlock_vault(pin, plain, false); }
bool vault_unlock_decoy(const char *pin, String &plain) { return unlock_vault(pin, plain, true); }

bool vault_save(const String &plain) {
  if (!unlocked) return false;
  const char *target = path(decoy_active), *temp = temp_path(decoy_active), *backup = backup_path(decoy_active);
  if (!encrypt_to_file(temp, plain, key, active_salt)) return false;
  SPIFFS.remove(backup);
  if (SPIFFS.exists(target) && !SPIFFS.rename(target, backup)) { SPIFFS.remove(temp); return false; }
  if (!SPIFFS.rename(temp, target)) { SPIFFS.rename(backup, target); return false; }
  SPIFFS.remove(backup); return true;
}

bool vault_change_pin(const char *new_pin, const String &plain) {
  if (!unlocked) return false;
  uint8_t salt[16], candidate[32]; esp_fill_random(salt, sizeof(salt));
  const char *target = path(decoy_active), *temp = temp_path(decoy_active), *backup = backup_path(decoy_active);
  if (!derive(new_pin, salt, candidate) || !encrypt_to_file(temp, plain, candidate, salt)) {
    wipe(candidate, 32); return false;
  }
  SPIFFS.remove(backup); if (SPIFFS.exists(target)) SPIFFS.rename(target, backup);
  if (!SPIFFS.rename(temp, target)) { SPIFFS.rename(backup, target); wipe(candidate, 32); return false; }
  SPIFFS.remove(backup);
  memcpy(active_salt, salt, 16);
  memcpy(key, candidate, 32); wipe(candidate, 32); return true;
}

void vault_lock() { wipe(key, sizeof(key)); wipe(active_salt, sizeof(active_salt)); unlocked = false; decoy_active = false; }

bool vault_export_base64(String &out) {
  uint8_t *raw = nullptr; size_t length = 0;
  const char *target = path(decoy_active);
  if (!read_all(SPIFFS.exists(target) ? target : backup_path(decoy_active), raw, length)) return false;
  size_t needed = 0;
  mbedtls_base64_encode(nullptr, 0, &needed, raw, length);
  unsigned char *encoded = static_cast<unsigned char *>(malloc(needed + 1));
  bool ok = encoded && mbedtls_base64_encode(encoded, needed, &needed, raw, length) == 0;
  if (ok) { encoded[needed] = 0; out = String(reinterpret_cast<char *>(encoded)); }
  wipe(raw, length); free(raw); if (encoded) { wipe(encoded, needed); free(encoded); }
  return ok;
}

bool vault_restore_base64(const char *pin, const String &encoded, String &plain) {
  size_t raw_len = 0;
  mbedtls_base64_decode(nullptr, 0, &raw_len,
      reinterpret_cast<const unsigned char *>(encoded.c_str()), encoded.length());
  uint8_t *raw = static_cast<uint8_t *>(malloc(raw_len)); uint8_t candidate[32];
  if (!raw || mbedtls_base64_decode(raw, raw_len, &raw_len,
      reinterpret_cast<const unsigned char *>(encoded.c_str()), encoded.length()) != 0) {
    free(raw); return false;
  }
  bool ok = decrypt_bytes(raw, raw_len, pin, plain, candidate);
  if (ok) {
    const char *target = path(decoy_active), *temp = temp_path(decoy_active), *backup = backup_path(decoy_active);
    File f = SPIFFS.open(temp, FILE_WRITE);
    ok = f && f.write(raw, raw_len) == raw_len;
    if (f) { f.flush(); f.close(); }
    if (ok) { SPIFFS.remove(backup); if (SPIFFS.exists(target)) SPIFFS.rename(target, backup); ok = SPIFFS.rename(temp, target); if (!ok) SPIFFS.rename(backup, target); else SPIFFS.remove(backup); }
    if (ok) {
      Header h; memcpy(&h, raw, sizeof(h));
      memcpy(key, candidate, 32); memcpy(active_salt, h.salt, 16); unlocked = true;
    }
  }
  wipe(candidate, 32); wipe(raw, raw_len); free(raw); return ok;
}

bool vault_factory_reset() {
  vault_lock();
  SPIFFS.remove(PRIMARY_TEMP); SPIFFS.remove(PRIMARY_BACKUP);
  SPIFFS.remove(DECOY_TEMP); SPIFFS.remove(DECOY_BACKUP);
  bool primary = !SPIFFS.exists(PRIMARY_PATH) || SPIFFS.remove(PRIMARY_PATH);
  bool decoy = !SPIFFS.exists(DECOY_PATH) || SPIFFS.remove(DECOY_PATH);
  return primary && decoy;
}
