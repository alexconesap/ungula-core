// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "chip_info.h"

#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <soc/rtc.h>
#include <cstring>

namespace ungula::core::system
{

static const char *chipModelName(esp_chip_model_t model)
{
        switch (model) {
        case CHIP_ESP32:
                return "ESP32";
        case CHIP_ESP32S2:
                return "ESP32-S2";
        case CHIP_ESP32S3:
                return "ESP32-S3";
        case CHIP_ESP32C3:
                return "ESP32-C3";
        case CHIP_ESP32H2:
                return "ESP32-H2";
        default:
                return "Unknown";
        }
}

ChipInfo queryChipInfo()
{
        ChipInfo info = {};

        esp_chip_info_t raw;
        esp_chip_info(&raw);

        // Model name
        strncpy(info.model, chipModelName(raw.model), CHIP_MODEL_MAX_LEN - 1);

        // (SDK)ESP-IDF version
        strncpy(info.sdkVersion, esp_get_idf_version(), SDK_VERSION_MAX_LEN - 1);

        // Core count and revision
        info.cores = static_cast<uint8_t>(raw.cores);
        info.revision = static_cast<uint8_t>(raw.revision);

        // Feature flags
        info.hasWifi = (raw.features & CHIP_FEATURE_WIFI_BGN) != 0;
        info.hasBluetooth = (raw.features & CHIP_FEATURE_BT) != 0;
        info.hasBle = (raw.features & CHIP_FEATURE_BLE) != 0;
        info.hasPsram = (raw.features & CHIP_FEATURE_EMB_PSRAM) != 0;

        // Build human-readable feature string
        info.features[0] = '\0';
        if (info.hasWifi) {
                strncat(info.features, "WiFi", CHIP_FEATURES_MAX_LEN - strlen(info.features) - 1);
        }
        if (info.hasBluetooth) {
                strncat(info.features, ", BT", CHIP_FEATURES_MAX_LEN - strlen(info.features) - 1);
        }
        if (info.hasBle) {
                strncat(info.features, ", BLE", CHIP_FEATURES_MAX_LEN - strlen(info.features) - 1);
        }
        if (info.hasPsram) {
                strncat(info.features, ", PSRAM",
                        CHIP_FEATURES_MAX_LEN - strlen(info.features) - 1);
        }
        if ((raw.features & CHIP_FEATURE_EMB_FLASH) != 0) {
                strncat(info.features, ", EmbFlash",
                        CHIP_FEATURES_MAX_LEN - strlen(info.features) - 1);
        }

        return info;
}

} // namespace ungula::core::system
