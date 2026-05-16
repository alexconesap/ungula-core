// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#if defined(ESP_PLATFORM)

#include "ungula/core/preferences/i_preferences.h"
#include "nvs.h"

namespace ungula::core::preferences
{

/// Pure ESP-IDF NVS implementation of IPreferences.
/// Uses nvs_flash API directly — no Arduino dependency.
class Esp32Preferences : public IPreferences {
    public:
        Esp32Preferences();
        ~Esp32Preferences() override;

        bool begin(const char *name) override;
        void end() override;
        bool putString(const char *key, const char *value) override;
        size_t getString(const char *key, char *buf, size_t bufSize) const override;
        bool putBytes(const char *key, const uint8_t *data, size_t len) override;
        size_t getBytes(const char *key, uint8_t *buf, size_t bufSize) const override;
        bool putUInt8(const char *key, uint8_t value) override;
        uint8_t getUInt8(const char *key, uint8_t defaultVal) const override;
        bool putUInt32(const char *key, uint32_t value) override;
        uint32_t getUInt32(const char *key, uint32_t defaultVal) const override;
        bool remove(const char *key) override;
        bool clear() override;
        bool hasKey(const char *key) const override;

    private:
        nvs_handle_t handle_;
        bool opened_;
};

} // namespace ungula::core::preferences
#endif // ESP_PLATFORM
