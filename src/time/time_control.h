// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <ctime>
#include "i_time_provider.h"
#include "time_format.h"
#include "timezones.h"

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
///
/// ## Time types — int64_t throughout
///
/// All time values are signed 64-bit. Three reasons:
///   1. Monotonic ticks can never overflow in the lifetime of any
///      device (~292 million years vs. uint32_t ms's 49 days).
///   2. ESP-IDF's `esp_timer_get_time()` is already int64_t — anything
///      narrower drops bits silently. The previous uint32_t micros()
///      truncated past 71 minutes.
///   3. Signed math means deadlines work intuitively:
///        `duration_ms_t remaining = deadline - now;`
///      `remaining < 0` ↔ overdue. With unsigned, an overdue value
///      becomes a huge positive number and "wait until deadline" sleeps
///      for ~292M years.
///
/// Three separate aliases by intent — no more vague `time_ms_t`:
///   - `tick_ms_t` / `tick_us_t` — a moment captured from millis()/micros()
///   - `duration_ms_t` / `duration_us_t` — an interval (delay length, etc.)
///   - `epoch_ms_t` — a wall-clock instant since the Unix epoch
/// All are `int64_t`; the names exist to make intent visible at call sites.

namespace ungula {

    class TimeControl final {
        public:
            using tick_ms_t = int64_t;      /// monotonic ms since boot
            using tick_us_t = int64_t;      /// monotonic us since boot
            using duration_ms_t = int64_t;  /// delays, intervals, remaining time
            using duration_us_t = int64_t;
            using epoch_ms_t = int64_t;  /// Unix epoch ms (signed for delta arithmetic)

            TimeControl() = delete;
            ~TimeControl() = delete;
            TimeControl(const TimeControl&) = delete;
            TimeControl& operator=(const TimeControl&) = delete;

            // ---- Local clock (platform-provided) ----

            /// Monotonic millisecond tick since boot. Effectively never wraps.
            static tick_ms_t millis();

            /// Monotonic microsecond tick since boot. Effectively never wraps
            /// (matches ESP-IDF's native `esp_timer_get_time()` width).
            static tick_us_t micros();

            // ---- Time provider hook ----

            static void setTimeProvider(ITimeProvider* provider) {
                provider_ = provider;
            }
            static void clearTimeProvider() {
                provider_ = nullptr;
            }

            /// Provider-aware current time, UTC. Routes through the
            /// installed ITimeProvider when one is present and reports
            /// itself valid; otherwise returns the local monotonic
            /// `millis()` (which is monotonic-since-boot, NOT a real
            /// wall-clock value, until a provider is installed).
            ///
            /// Returns UTC by convention. Use `nowLocal()` for the
            /// configured timezone, or `nowInTz()` for an arbitrary one.
            static epoch_ms_t now() {
                if (provider_ != nullptr && provider_->isValid()) {
                    return provider_->nowMs();
                }
                return millis();
            }

            /// Explicit UTC alias. Same as `now()`.
            static epoch_ms_t nowUtc() {
                return now();
            }

            /// Wall-clock in the timezone configured via
            /// `setTimezoneOffsetSeconds()`. Equals `now()` until that
            /// setter has been called.
            static epoch_ms_t nowLocal() {
                return now() + (static_cast<int64_t>(timezoneOffsetSeconds_) * 1000);
            }

            /// Wall-clock in an arbitrary timezone — caller supplies the
            /// offset in seconds from UTC (e.g. -5 * 3600 for EST). Does
            /// NOT consult or change the stored offset.
            static epoch_ms_t nowInTz(int32_t offsetSeconds) {
                return now() + (static_cast<int64_t>(offsetSeconds) * 1000);
            }

            /// Set the offset used by `nowLocal()`. Default is 0 (UTC).
            /// No DST awareness — for that, use the NTP client's
            /// strftime-based formatters.
            static void setTimezoneOffsetSeconds(int32_t offsetSeconds) {
                timezoneOffsetSeconds_ = offsetSeconds;
            }

            /// Convenience overload — pick a named zone instead of a raw
            /// offset. Use the entries from `time/timezones.h` so the
            /// project never hard-codes "what is the offset for Tokyo".
            static void setTimezone(tz::Timezone zone) {
                timezoneOffsetSeconds_ = tz::offsetSeconds(zone);
            }

            static int32_t timezoneOffsetSeconds() {
                return timezoneOffsetSeconds_;
            }

            /// Alias for micros(). No provider hook — microsecond-grade
            /// external sources are rare enough that it would pay zero
            /// users, and the hot path matters for this getter.
            static tick_us_t nowUs() {
                return micros();
            }

            // ---- Formatting ----
            //
            // Print the current time as a human-readable string. Returns 0
            // when no valid time provider is installed — formatting a
            // monotonic-since-boot tick as a wall-clock date would just
            // print "1970-01-01 00:00:NN", which is misleading.

            /// Format current time as "YYYY-MM-DD HH:MM:SS" UTC.
            /// Buffer must be at least 20 bytes.
            static size_t formatUtc(char* buf, size_t bufSize) {
                if (provider_ == nullptr || !provider_->isValid()) {
                    return 0;
                }
                return time_format::formatIso8601(
                        buf, bufSize, static_cast<time_t>(provider_->nowMs() / 1000), 0);
            }

            /// Format current time as "YYYY-MM-DD HH:MM:SS" in the
            /// configured local zone (the offset set via `setTimezone()`
            /// or `setTimezoneOffsetSeconds()`).
            static size_t formatLocal(char* buf, size_t bufSize) {
                if (provider_ == nullptr || !provider_->isValid()) {
                    return 0;
                }
                return time_format::formatIso8601(buf, bufSize,
                                                  static_cast<time_t>(provider_->nowMs() / 1000),
                                                  timezoneOffsetSeconds_);
            }

            /// Format current time with a custom strftime spec, in the
            /// configured local zone. For arbitrary epoch values + custom
            /// formats, call `time_format::format()` directly.
            static size_t format(char* buf, size_t bufSize, const char* strftimeFmt) {
                if (provider_ == nullptr || !provider_->isValid()) {
                    return 0;
                }
                return time_format::format(buf, bufSize, strftimeFmt,
                                           static_cast<time_t>(provider_->nowMs() / 1000),
                                           timezoneOffsetSeconds_);
            }

            // ---- Network-synced clock ----
            // Stores offset between local clock and a remote coordinator's clock.
            // setSyncTime() is called once per sync event (cheap: one subtraction).
            // syncNow() is called on every read (cheap: one addition).

            static void setSyncTime(tick_ms_t remoteMs) {
                sync_.offsetMs = remoteMs - millis();
                sync_.active = true;
            }

            static void setSyncTimeUs(tick_us_t remoteUs) {
                sync_.offsetUs = remoteUs - micros();
                sync_.active = true;
            }

            static tick_ms_t syncNow() {
                return millis() + sync_.offsetMs;
            }

            static tick_us_t syncNowUs() {
                return micros() + sync_.offsetUs;
            }

            static int64_t syncOffset() {
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
            static void delay(duration_ms_t msv) {
                delayMs(msv);
            }

            static void delayMs(duration_ms_t msv);
            static void delayUs(duration_us_t usv);

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
            static void delayUntilMs(tick_ms_t& reference, duration_ms_t periodMs);

            /// Same idea in microseconds. Best-effort — relies on busy-wait.
            static void delayUntilUs(tick_us_t& reference, duration_us_t periodUs);

            /// Cooperative yield. Prefer this over `delayMs(0)` for intent.
            static void yield() {
                delayMs(0);
            }

        private:
            // Direct signed comparison — no clever unsigned-overflow trick
            // is needed once we're on int64_t. `now >= target` IS the test.
            static bool hasReachedMs(tick_ms_t now, tick_ms_t target) {
                return now >= target;
            }
            static bool hasReachedUs(tick_us_t now, tick_us_t target) {
                return now >= target;
            }

            /// Sync offset between local and coordinator clocks.
            struct SyncState {
                    int64_t offsetMs;
                    int64_t offsetUs;
                    bool active;
            };

            // C++17 inline static — storage lives here, no companion .cpp needed.
            inline static ITimeProvider* provider_ = nullptr;
            inline static SyncState sync_ = {0, 0, false};
            inline static int32_t timezoneOffsetSeconds_ = 0;
    };

}  // namespace ungula

// ---- Platform dispatch ----
// Each platform header contains inline definitions of the
// platform-specific methods (millis, micros, delay*, delayUntil*).
// No cross-platform #ifdefs inside those files — each is pure for
// its target.

#if defined(ESP_PLATFORM)
#include "platforms/time_control_esp32.h"
#else
#include "platforms/time_control_host.h"
#endif
