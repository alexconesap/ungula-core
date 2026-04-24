// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stdint.h>
#include "i_time_provider.h"

/// @brief Platform-abstracted time source.
///
/// Public API is platform-independent — every project includes this header
/// exactly the same way. The platform-specific implementation is selected
/// at the bottom of this file via `#ifdef`, mirroring the pattern used by
/// `hal/gpio/gpio_access.h`:
///
///   - ESP32 (`ESP_PLATFORM`) → `platforms/time_control_esp32.h`
///   - anything else          → `platforms/time_control_host.h` (std::chrono
///                             backend — host tests, and a reasonable default
///                             until a real port lands)
///
/// Adding a new platform (STM32, etc.) means creating one more
/// `platforms/time_control_<name>.h` with inline definitions of the
/// platform-specific methods, and adding one `#elif` branch below.
/// No file in this repo needs `#ifdef` branching on platform — each
/// platform header is pure.

namespace ungula {

    class TimeControl final {
        public:
            using ms_tick_t = uint32_t;
            using us_tick_t = uint32_t;
            using time_ms_t = uint32_t;
            using time_us_t = uint32_t;

            TimeControl() = delete;
            ~TimeControl() = delete;
            TimeControl(const TimeControl&) = delete;
            TimeControl& operator=(const TimeControl&) = delete;

            // ---- Local clock (platform-provided) ----

            /// Monotonic millisecond tick. Wraps around every ~49 days.
            static ms_tick_t millis();

            /// Monotonic microsecond tick. Wraps around every ~71 minutes.
            static us_tick_t micros();

            // ---- Time provider hook ----

            static void setTimeProvider(ITimeProvider* provider) {
                provider_ = provider;
            }
            static void clearTimeProvider() {
                provider_ = nullptr;
            }

            /// Provider-aware current time. Routes through the installed
            /// ITimeProvider when one is present and reports itself valid;
            /// otherwise returns the local monotonic `millis()`.
            static ms_tick_t now() {
                if (provider_ != nullptr && provider_->isValid()) {
                    return provider_->nowMs();
                }
                return millis();
            }

            /// Alias for micros(). No provider hook — microsecond-grade
            /// external sources are rare enough that it would pay zero
            /// users, and the hot path matters for this getter.
            static us_tick_t nowUs() {
                return micros();
            }

            // ---- Network-synced clock ----
            // Stores offset between local clock and a remote coordinator's clock.
            // setSyncTime() is called once per sync event (cheap: one subtraction).
            // syncNow() is called on every read (cheap: one addition).

            static void setSyncTime(ms_tick_t remoteMs) {
                sync_.offsetMs = static_cast<int32_t>(remoteMs - millis());
                sync_.active = true;
            }

            static void setSyncTimeUs(us_tick_t remoteUs) {
                sync_.offsetUs = remoteUs - micros();
                sync_.active = true;
            }

            static ms_tick_t syncNow() {
                return millis() + static_cast<ms_tick_t>(sync_.offsetMs);
            }

            static us_tick_t syncNowUs() {
                return micros() + sync_.offsetUs;
            }

            static int32_t syncOffset() {
                return sync_.offsetMs;
            }

            static int64_t syncOffsetUs() {
                return sync_.offsetUs;
            }

            static bool isSynced() {
                return sync_.active;
            }

            static void clearSync() {
                sync_.offsetMs = 0;
                sync_.offsetUs = 0;
                sync_.active = false;
            }

            // ---- Delays (platform-provided) ----

            /// Alias for delayMs(). Kept for source-compat with callers
            /// that used `delay()` originally.
            static void delay(time_ms_t msv) {
                delayMs(msv);
            }

            static void delayMs(time_ms_t msv);
            static void delayUs(time_us_t usv);

            /// Wait until the next periodic boundary, then advance
            /// 'reference' by periodMs. Drift-free: 'reference' is bumped
            /// by the period, not reset to now.
            ///
            /// Example — run something every 50ms:
            /// ```cpp
            ///   auto ref = TimeControl::millis();
            ///   while (running) {
            ///       readSensors();
            ///       sendStatus();
            ///       TimeControl::delayUntilMs(ref, 50);
            ///   }
            /// ```
            static void delayUntilMs(ms_tick_t& reference, time_ms_t periodMs);

            /// Same idea in microseconds. Best-effort — relies on busy-wait.
            static void delayUntilUs(us_tick_t& reference, time_us_t periodUs);

            /// Cooperative yield. Prefer this over `delayMs(0)` for intent.
            static void yield() {
                delayMs(0);
            }

        private:
            static bool hasReachedMs(ms_tick_t now, ms_tick_t target);
            static bool hasReachedUs(us_tick_t now, us_tick_t target);

            /// Sync offset between local and coordinator clocks.
            struct SyncState {
                    int32_t offsetMs;
                    int64_t offsetUs;
                    bool active;
            };

            // C++17 inline static — storage lives here, no companion .cpp needed.
            inline static ITimeProvider* provider_ = nullptr;
            inline static SyncState sync_ = {0, 0, false};
    };

}  // namespace ungula

// ---- Platform dispatch ----
// Each platform header contains inline definitions of the
// platform-specific methods (millis, micros, delay*, delayUntil*,
// hasReached*). No cross-platform #ifdefs inside those files — each
// is pure for its target.

#if defined(ESP_PLATFORM)
#include "platforms/time_control_esp32.h"
#else
#include "platforms/time_control_host.h"
#endif
