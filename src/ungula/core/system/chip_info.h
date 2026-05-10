// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <cstdint>

/// @brief Platform-independent MCU information.
///
/// Populated at boot by a platform-specific implementation.
/// Portable struct — no SDK types in the interface.

namespace ungula::core::system
{

    constexpr uint8_t CHIP_MODEL_MAX_LEN = 24;
    constexpr uint8_t SDK_VERSION_MAX_LEN = 24;
    constexpr uint8_t CHIP_FEATURES_MAX_LEN = 80;

    struct ChipInfo {
        char model[CHIP_MODEL_MAX_LEN]; // e.g. "ESP32", "ESP32-S3", "STC32G"
        char sdkVersion[SDK_VERSION_MAX_LEN]; // e.g. "5.1.4", "N/A"
        char features[CHIP_FEATURES_MAX_LEN]; // e.g. "WiFi, BT, BLE, PSRAM"
        uint8_t cores; // number of CPU cores
        uint8_t revision; // silicon revision
        bool hasWifi;
        bool hasBluetooth;
        bool hasBle;
        bool hasPsram;
    };

    /// @brief Query current MCU info. Platform-specific implementation.
    ChipInfo queryChipInfo();

} // namespace ungula::core::system
