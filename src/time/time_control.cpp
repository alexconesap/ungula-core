// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "time/time_control.h"

#if defined(ESP_PLATFORM)
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

#else
#error "Unsupported platform: TimeControl requires ESP_PLATFORM"
#endif

namespace ungula {

    TimeControl::SyncState TimeControl::sync_ = {0, 0, false};

    namespace {

        template <typename TickT>
        inline bool hasReached(TickT now, TickT target) {
            static_assert(std::is_unsigned_v<TickT>, "Tick type must be unsigned");
            using SignedT = std::make_signed_t<TickT>;
            return static_cast<SignedT>(now - target) >= 0;
        }

        constexpr uint32_t COARSE_THRESHOLD_US = 1000;  // above this -> coarse delay
        constexpr uint32_t COARSE_GUARD_US = 200;       // leave margin to avoid overshoot

        constexpr uint32_t FINE_THRESHOLD_US = 50;  // medium precision zone
        constexpr uint32_t FINE_GUARD_US = 10;      // tighter margin

        constexpr uint32_t SPIN_THRESHOLD_US = 50;  // below this -> pure spin

    }  // namespace

    void TimeControl::delayMs(time_ms_t msv) {
        vTaskDelay(pdMS_TO_TICKS(msv));
    }

    void TimeControl::delayUs(time_us_t usv) {
        esp_rom_delay_us(static_cast<uint32_t>(usv));
    }

    TimeControl::ms_tick_t TimeControl::millis() {
        return static_cast<ms_tick_t>(esp_timer_get_time() / 1000ULL);
    }

    TimeControl::us_tick_t TimeControl::micros() {
        return static_cast<us_tick_t>(esp_timer_get_time());
    }

    void TimeControl::delayUntilMs(ms_tick_t& reference, time_ms_t periodMs) {
        if (periodMs == 0U) {
            return;
        }

        // Convert ms reference to RTOS ticks, let FreeRTOS handle the scheduling
        TickType_t refTicks = pdMS_TO_TICKS(reference);
        const TickType_t periodTicks = pdMS_TO_TICKS(periodMs);

        // If periodMs is non-zero but rounds to zero ticks, force at least 1 tick
        const TickType_t safePeriodTicks = (periodTicks == 0U) ? 1U : periodTicks;

        vTaskDelayUntil(&refTicks, safePeriodTicks);

        // Convert back to ms so the public type stays platform-neutral
        reference = static_cast<ms_tick_t>(refTicks * portTICK_PERIOD_MS);
    }

    void TimeControl::delayUntilUs(us_tick_t& reference, time_us_t periodUs) {
        if (periodUs == 0U) {
            return;
        }

        const us_tick_t target = reference + periodUs;

        for (;;) {
            const us_tick_t now = micros();
            if (hasReachedUs(now, target))
                break;

            const us_tick_t remaining = target - now;

            // Hybrid wait: coarse sleep for large gaps, tighter spin near the end
            if (remaining > COARSE_THRESHOLD_US) {
                delayUs(remaining - COARSE_GUARD_US);
            } else if (remaining > FINE_THRESHOLD_US) {
                delayUs(remaining - FINE_GUARD_US);
            }
        }

        reference = target;
    }

    bool TimeControl::hasReachedMs(ms_tick_t now, ms_tick_t target) {
        return hasReached(now, target);
    }

    bool TimeControl::hasReachedUs(us_tick_t now, us_tick_t target) {
        return hasReached(now, target);
    }

}  // namespace ungula
