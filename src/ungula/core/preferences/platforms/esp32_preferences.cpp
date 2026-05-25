// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32) || defined(ESP32)

#include "ungula/core/preferences/platforms/esp32_preferences.h"
#include "ungula/core/preferences/preferences.h"  // declares initStorage()

#include "nvs_flash.h"

namespace ungula::core::preferences
{

bool initStorage()
{
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                // Partition format mismatch (fresh chip or IDF version bump).
                // Wipe and retry once.
                (void)nvs_flash_erase();
                err = nvs_flash_init();
        }
        return err == ESP_OK;
}

Esp32Preferences::Esp32Preferences()
        : handle_(0)
        , opened_(false)
{
}

Esp32Preferences::~Esp32Preferences()
{
        if (opened_) {
                nvs_close(handle_);
        }
}

bool Esp32Preferences::begin(const char *name)
{
        if (opened_) {
                nvs_close(handle_);
                opened_ = false;
        }
        esp_err_t err = nvs_open(name, NVS_READWRITE, &handle_);
        if (err != ESP_OK) {
                return false;
        }
        opened_ = true;
        return true;
}

void Esp32Preferences::end()
{
        if (opened_) {
                nvs_close(handle_);
                handle_ = 0;
                opened_ = false;
        }
}

bool Esp32Preferences::putString(const char *key, const char *value)
{
        if (!opened_) {
                return false;
        }
        if (nvs_set_str(handle_, key, value) != ESP_OK) {
                return false;
        }
        return nvs_commit(handle_) == ESP_OK;
}

size_t Esp32Preferences::getString(const char *key, char *buf, size_t bufSize) const
{
        if (!opened_ || bufSize == 0) {
                return 0;
        }
        // Query the stored length first (includes null terminator).
        size_t required = 0;
        if (nvs_get_str(handle_, key, nullptr, &required) != ESP_OK) {
                buf[0] = '\0';
                return 0;
        }
        // nvs_get_str refuses to read if the buffer is too small (returns
        // ESP_ERR_NVS_INVALID_LENGTH). Read the full value only when it fits.
        if (required > bufSize) {
                buf[0] = '\0';
                return 0;
        }
        size_t len = bufSize;
        if (nvs_get_str(handle_, key, buf, &len) != ESP_OK) {
                buf[0] = '\0';
                return 0;
        }
        return len > 0 ? len - 1 : 0; // Exclude null terminator from count
}

bool Esp32Preferences::putBytes(const char *key, const uint8_t *data, size_t len)
{
        if (!opened_) {
                return false;
        }
        if (nvs_set_blob(handle_, key, data, len) != ESP_OK) {
                return false;
        }
        return nvs_commit(handle_) == ESP_OK;
}

size_t Esp32Preferences::getBytes(const char *key, uint8_t *buf, size_t bufSize) const
{
        if (!opened_ || bufSize == 0) {
                return 0;
        }
        size_t len = bufSize;
        if (nvs_get_blob(handle_, key, buf, &len) != ESP_OK) {
                return 0;
        }
        return len;
}

bool Esp32Preferences::putUInt8(const char *key, uint8_t value)
{
        if (!opened_) {
                return false;
        }
        if (nvs_set_u8(handle_, key, value) != ESP_OK) {
                return false;
        }
        return nvs_commit(handle_) == ESP_OK;
}

uint8_t Esp32Preferences::getUInt8(const char *key, uint8_t defaultVal) const
{
        if (!opened_) {
                return defaultVal;
        }
        uint8_t val = 0;
        if (nvs_get_u8(handle_, key, &val) != ESP_OK) {
                return defaultVal;
        }
        return val;
}

bool Esp32Preferences::putUInt32(const char *key, uint32_t value)
{
        if (!opened_) {
                return false;
        }
        if (nvs_set_u32(handle_, key, value) != ESP_OK) {
                return false;
        }
        return nvs_commit(handle_) == ESP_OK;
}

uint32_t Esp32Preferences::getUInt32(const char *key, uint32_t defaultVal) const
{
        if (!opened_) {
                return defaultVal;
        }
        uint32_t val = 0;
        if (nvs_get_u32(handle_, key, &val) != ESP_OK) {
                return defaultVal;
        }
        return val;
}

bool Esp32Preferences::remove(const char *key)
{
        if (!opened_) {
                return false;
        }
        if (nvs_erase_key(handle_, key) != ESP_OK) {
                return false;
        }
        return nvs_commit(handle_) == ESP_OK;
}

bool Esp32Preferences::clear()
{
        if (!opened_) {
                return false;
        }
        if (nvs_erase_all(handle_) != ESP_OK) {
                return false;
        }
        return nvs_commit(handle_) == ESP_OK;
}

bool Esp32Preferences::hasKey(const char *key) const
{
        if (!opened_) {
                return false;
        }
        // NVS has no type-agnostic "key exists" in v5.1.  Probe each type we use.
        // NOT_FOUND means the key is absent; TYPE_MISMATCH means it exists but as
        // a different type; OK means it exists and matches.
        size_t len = 0;
        esp_err_t err = nvs_get_blob(handle_, key, nullptr, &len);
        if (err != ESP_ERR_NVS_NOT_FOUND) {
                return (err == ESP_OK || err == ESP_ERR_NVS_TYPE_MISMATCH);
        }
        err = nvs_get_str(handle_, key, nullptr, &len);
        if (err != ESP_ERR_NVS_NOT_FOUND) {
                return (err == ESP_OK || err == ESP_ERR_NVS_TYPE_MISMATCH);
        }
        uint8_t u8v = 0;
        err = nvs_get_u8(handle_, key, &u8v);
        if (err != ESP_ERR_NVS_NOT_FOUND) {
                return (err == ESP_OK || err == ESP_ERR_NVS_TYPE_MISMATCH);
        }
        uint32_t u32v = 0;
        err = nvs_get_u32(handle_, key, &u32v);
        return (err == ESP_OK);
}

} // namespace ungula::core::preferences
#endif // ESP_PLATFORM || ARDUINO_ARCH_ESP32 || ESP32
