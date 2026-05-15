// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "ungula/core/system/system_reboot.h"

#include "ungula/core/time/time.h"

#if defined(ESP_PLATFORM)
#include "esp_system.h"
#define PLATFORM_SYSTEM_ESP_IDF 1
#else
#error "Unsupported platform"
#endif

namespace ungula::core::system
{

void SystemControl::reboot()
{
#if defined(PLATFORM_SYSTEM_ESP_IDF)
        esp_restart();
#endif
}

void SystemControl::rebootAfterMs(uint32_t waitTimeMs)
{
        if (waitTimeMs != 0U) {
                ungula::core::time::delayMs(waitTimeMs);
        }
        reboot();
}

} // namespace ungula::core::system
