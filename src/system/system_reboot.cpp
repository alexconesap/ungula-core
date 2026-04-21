// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "system/system_reboot.h"

#include "time/time_control.h"

#if defined(ESP_PLATFORM)
#include "esp_system.h"
#define PLATFORM_SYSTEM_ESP_IDF 1
#else
#error "Unsupported platform"
#endif

namespace ungula {

    void SystemControl::reboot() {
#if defined(PLATFORM_SYSTEM_ESP_IDF)
        esp_restart();
#endif
    }

    void SystemControl::rebootAfterMs(uint32_t waitTimeMs) {
        if (waitTimeMs != 0U) {
            TimeControl::delayMs(waitTimeMs);
        }
        reboot();
    }

}  // namespace ungula