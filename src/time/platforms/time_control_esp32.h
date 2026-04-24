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

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <type_traits>
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

namespace ungula {

    namespace detail {

        template <typename TickT>
        inline bool hasReachedTick(TickT now, TickT target) {
            static_assert(std::is_unsigned_v<TickT>, "Tick type must be unsigned");
            using SignedT = std::make_signed_t<TickT>;
            return static_cast<SignedT>(now - target) >= 0;
        }

        // Hybrid delayUntilUs thresholds: coarse sleep for large gaps,
        // tighter spin near the target. Guards shave off the worst-case
        // overshoot from the sleep helper.
        inline constexpr uint32_t COARSE_THRESHOLD_US = 1000;
        inline constexpr uint32_t COARSE_GUARD_US = 200;
        inline constexpr uint32_t FINE_THRESHOLD_US = 50;
        inline constexpr uint32_t FINE_GUARD_US = 10;

    }  // namespace detail

    inline TimeControl::ms_tick_t TimeControl::millis() {
        return static_cast<ms_tick_t>(esp_timer_get_time() / 1000ULL);
    }

    inline TimeControl::us_tick_t TimeControl::micros() {
        return static_cast<us_tick_t>(esp_timer_get_time());
    }

    inline void TimeControl::delayMs(time_ms_t msv) {
        vTaskDelay(pdMS_TO_TICKS(msv));
    }

    inline void TimeControl::delayUs(time_us_t usv) {
        esp_rom_delay_us(static_cast<uint32_t>(usv));
    }

    inline bool TimeControl::hasReachedMs(ms_tick_t now, ms_tick_t target) {
        return detail::hasReachedTick(now, target);
    }

    inline bool TimeControl::hasReachedUs(us_tick_t now, us_tick_t target) {
        return detail::hasReachedTick(now, target);
    }

    inline void TimeControl::delayUntilMs(ms_tick_t& reference, time_ms_t periodMs) {
        if (periodMs == 0U) {
            return;
        }
        // Let FreeRTOS handle the scheduling. Translate back and forth via
        // ticks so the public ms_tick_t stays platform-neutral.
        TickType_t refTicks = pdMS_TO_TICKS(reference);
        const TickType_t periodTicks = pdMS_TO_TICKS(periodMs);
        // If periodMs rounds to zero ticks, force at least one tick so we
        // still advance (and don't block forever).
        const TickType_t safePeriodTicks = (periodTicks == 0U) ? 1U : periodTicks;
        vTaskDelayUntil(&refTicks, safePeriodTicks);
        reference = static_cast<ms_tick_t>(refTicks * portTICK_PERIOD_MS);
    }

    inline void TimeControl::delayUntilUs(us_tick_t& reference, time_us_t periodUs) {
        if (periodUs == 0U) {
            return;
        }
        const us_tick_t target = reference + periodUs;
        for (;;) {
            const us_tick_t now = micros();
            if (hasReachedUs(now, target)) {
                break;
            }
            const us_tick_t remaining = target - now;
            if (remaining > detail::COARSE_THRESHOLD_US) {
                delayUs(remaining - detail::COARSE_GUARD_US);
            } else if (remaining > detail::FINE_THRESHOLD_US) {
                delayUs(remaining - detail::FINE_GUARD_US);
            }
            // Under the spin threshold: tight loop until target.
        }
        reference = target;
    }

}  // namespace ungula
