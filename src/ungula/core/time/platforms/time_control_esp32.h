// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

/// @brief ESP32 / ESP-IDF backend for TimeControl.
///
/// Pure ESP-IDF — FreeRTOS for coarse delays, esp_timer for monotonic
/// time, esp_rom_delay_us for busy-wait microsecond delays. No guards
/// on anything other platform might lack; this file is only reached
/// via the `#if defined(ESP_PLATFORM)` branch of `time_control.h`.
///
/// `esp_timer_get_time()` returns int64_t microseconds — feeding it
/// straight through (no narrowing) is the whole point of the int64_t
/// time-types refactor.

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp_timer.h"

#if __has_include("esp_rom_sys.h")
#include "esp_rom_sys.h"
#elif __has_include("esp_rom/esp_rom_sys.h")
#include "esp_rom/esp_rom_sys.h"
#elif __has_include("rom/ets_sys.h")
#include "rom/ets_sys.h"
#else
#error "No ESP ROM delay header found"
#endif

namespace ungula::core::time::detail {

    // Hybrid delayUntilUs thresholds: coarse sleep for large gaps,
    // tighter spin near the target. Guards shave off the worst-case
    // overshoot from the sleep helper.
    inline constexpr int64_t COARSE_THRESHOLD_US = 1000;
    inline constexpr int64_t COARSE_GUARD_US = 200;
    inline constexpr int64_t FINE_THRESHOLD_US = 50;
    inline constexpr int64_t FINE_GUARD_US = 10;

}  // namespace ungula::core::time::detail

namespace ungula::core::time {
    inline TimeControl::tick_ms_t TimeControl::millis() {
        return esp_timer_get_time() / 1000;
    }

    inline TimeControl::tick_us_t TimeControl::micros() {
        return esp_timer_get_time();
    }

    inline void TimeControl::delayMs(duration_ms_t msv) {
        if (msv <= 0) {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(static_cast<uint32_t>(msv)));
    }

    inline void TimeControl::delayUs(duration_us_t usv) {
        if (usv <= 0) {
            return;
        }
        esp_rom_delay_us(static_cast<uint32_t>(usv));
    }

    inline void TimeControl::delayUntilMs(tick_ms_t& reference, duration_ms_t periodMs) {
        if (periodMs <= 0) {
            return;
        }
        // Translate the reference to RTOS ticks, let FreeRTOS handle the
        // scheduling, then translate back so the public type stays
        // platform-neutral. Cast guard: pdMS_TO_TICKS / TickType_t are
        // 32-bit — anything that doesn't fit at this scale (~49 days of
        // ms) wouldn't survive the FreeRTOS API anyway.
        TickType_t refTicks = pdMS_TO_TICKS(static_cast<uint32_t>(reference));
        const TickType_t periodTicks = pdMS_TO_TICKS(static_cast<uint32_t>(periodMs));
        // If periodMs rounds to zero ticks, force at least one tick so we
        // still advance (and don't block forever).
        const TickType_t safePeriodTicks = (periodTicks == 0U) ? 1U : periodTicks;
        vTaskDelayUntil(&refTicks, safePeriodTicks);
        reference = static_cast<tick_ms_t>(refTicks * portTICK_PERIOD_MS);
    }

    inline void TimeControl::delayUntilUs(tick_us_t& reference, duration_us_t periodUs) {
        if (periodUs <= 0) {
            return;
        }
        const tick_us_t target = reference + periodUs;
        for (;;) {
            const tick_us_t now = micros();
            if (now >= target) {
                break;
            }
            const int64_t remaining = target - now;
            if (remaining > detail::COARSE_THRESHOLD_US) {
                delayUs(remaining - detail::COARSE_GUARD_US);
            } else if (remaining > detail::FINE_THRESHOLD_US) {
                delayUs(remaining - detail::FINE_GUARD_US);
            }
            // Under the spin threshold: tight loop until target.
        }
        reference = target;
    }

}  // namespace ungula::core::time
