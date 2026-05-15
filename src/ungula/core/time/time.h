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

/// @brief Platform-abstracted time API as free functions.
///
/// Public API is platform-independent — every project includes this header
/// exactly the same way. The platform-specific implementation is selected
/// at the bottom of this file via `#ifdef` as we do in other Ungula's libraries:
///
///   - ESP32 (`ESP_PLATFORM`) → `platforms/time_control_esp32.h`
///   - anything else          → `platforms/time_control_host.h` (std::chrono
///                             backend — host tests, and a reasonable default
///                             until a real port lands)
///
/// Adding a new platform (STM32, etc.) means creating one more
/// `platforms/time_control_<name>.h` with inline definitions of the
/// platform-specific functions, and adding one `#elif` branch below.
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
///
/// ## Usage
///
/// ```cpp
///   #include <ungula/core/time/time.h>
///   namespace tc = ungula::core::time;
///   tc::delay(2000);
///   auto t = tc::millis();
/// ```
///
/// Or directly:
///
/// ```cpp
///   ungula::core::time::delay(2000);
/// ```

namespace ungula::core::time
{

using tick_ms_t = int64_t; /// monotonic ms since boot
using tick_us_t = int64_t; /// monotonic us since boot
using duration_ms_t = int64_t; /// delays, intervals, remaining time
using duration_us_t = int64_t;
using epoch_ms_t = int64_t; /// Unix epoch ms (signed for delta arithmetic)

namespace detail
{

        /// Sync offset between local and coordinator clocks.
        struct SyncState {
                int64_t offsetMs;
                int64_t offsetUs;
                bool active;
        };

        // C++17 inline statics — module-private state, single definition
        // across all TUs that include this header.
        inline ITimeProvider *provider_ = nullptr;
        inline SyncState sync_ = { 0, 0, false };
        inline int32_t timezoneOffsetSeconds_ = 0;

} // namespace detail

// ---- Local clock (platform-provided; defined in platform header) ----

/// Monotonic millisecond tick since boot. Effectively never wraps.
tick_ms_t millis();

/// Monotonic microsecond tick since boot. Effectively never wraps
/// (matches ESP-IDF's native `esp_timer_get_time()` width).
tick_us_t micros();

// ---- Delays (platform-provided; defined in platform header) ----

void delayMs(duration_ms_t msv);
void delayUs(duration_us_t usv);

/// Wait until the next periodic boundary, then advance 'reference' by
/// periodMs. Drift-free: 'reference' is bumped by the period, not
/// reset to now.
///
/// Example — run something every 50ms:
/// ```cpp
///   auto ref = ungula::core::time::millis();
///   while (running) {
///       readSensors();
///       sendStatus();
///       ungula::core::time::delayUntilMs(ref, 50);
///   }
/// ```
void delayUntilMs(tick_ms_t &reference, duration_ms_t periodMs);

/// Same idea in microseconds. Best-effort — relies on busy-wait.
void delayUntilUs(tick_us_t &reference, duration_us_t periodUs);

// ---- Time provider hook ----

inline void setTimeProvider(ITimeProvider *provider)
{
        detail::provider_ = provider;
}

inline void clearTimeProvider()
{
        detail::provider_ = nullptr;
}

/// Provider-aware current time, UTC. Routes through the installed
/// ITimeProvider when one is present and reports itself valid;
/// otherwise returns the local monotonic `millis()` (which is
/// monotonic-since-boot, NOT a real wall-clock value, until a
/// provider is installed).
///
/// Returns UTC by convention. Use `nowLocal()` for the configured
/// timezone, or `nowInTz()` for an arbitrary one.
inline epoch_ms_t now()
{
        if (detail::provider_ != nullptr && detail::provider_->isValid()) {
                return detail::provider_->nowMs();
        }
        return millis();
}

/// Explicit UTC alias. Same as `now()`.
inline epoch_ms_t nowUtc()
{
        return now();
}

/// Wall-clock in the timezone configured via
/// `setTimezoneOffsetSeconds()`. Equals `now()` until that setter
/// has been called.
inline epoch_ms_t nowLocal()
{
        return now() + (static_cast<int64_t>(detail::timezoneOffsetSeconds_) * 1000);
}

/// Wall-clock in an arbitrary timezone — caller supplies the offset
/// in seconds from UTC (e.g. -5 * 3600 for EST). Does NOT consult or
/// change the stored offset.
inline epoch_ms_t nowInTz(int32_t offsetSeconds)
{
        return now() + (static_cast<int64_t>(offsetSeconds) * 1000);
}

/// Set the offset used by `nowLocal()`. Default is 0 (UTC). No DST
/// awareness — for that, use the NTP client's strftime-based formatters.
inline void setTimezoneOffsetSeconds(int32_t offsetSeconds)
{
        detail::timezoneOffsetSeconds_ = offsetSeconds;
}

/// Convenience overload — pick a named zone instead of a raw offset.
/// Use the entries from `time/timezones.h` so the project never
/// hard-codes "what is the offset for Tokyo".
inline void setTimezone(tz::Timezone zone)
{
        detail::timezoneOffsetSeconds_ = tz::offsetSeconds(zone);
}

inline int32_t timezoneOffsetSeconds()
{
        return detail::timezoneOffsetSeconds_;
}

/// Alias for micros(). No provider hook — microsecond-grade external
/// sources are rare enough that it would pay zero users, and the hot
/// path matters for this getter.
inline tick_us_t nowUs()
{
        return micros();
}

// ---- Formatting ----
//
// Print the current time as a human-readable string. Returns 0 when
// no valid time provider is installed — formatting a monotonic-since-
// boot tick as a wall-clock date would just print "1970-01-01
// 00:00:NN", which is misleading.

/// Format current time as "YYYY-MM-DD HH:MM:SS" UTC. Buffer must be
/// at least 20 bytes.
inline size_t formatUtc(char *buf, size_t bufSize)
{
        if (detail::provider_ == nullptr || !detail::provider_->isValid()) {
                return 0;
        }
        return formatIso8601(buf, bufSize, static_cast<time_t>(detail::provider_->nowMs() / 1000),
                             0);
}

/// Format current time as "YYYY-MM-DD HH:MM:SS" in the configured
/// local zone (the offset set via `setTimezone()` or
/// `setTimezoneOffsetSeconds()`).
inline size_t formatLocal(char *buf, size_t bufSize)
{
        if (detail::provider_ == nullptr || !detail::provider_->isValid()) {
                return 0;
        }
        return formatIso8601(buf, bufSize, static_cast<time_t>(detail::provider_->nowMs() / 1000),
                             detail::timezoneOffsetSeconds_);
}

/// Format current time with a custom strftime spec, in the configured
/// local zone. For arbitrary epoch values + custom formats, call the
/// 5-arg `format()` directly.
inline size_t formatNow(char *buf, size_t bufSize, const char *strftimeFmt)
{
        if (detail::provider_ == nullptr || !detail::provider_->isValid()) {
                return 0;
        }
        return format(buf, bufSize, strftimeFmt,
                      static_cast<time_t>(detail::provider_->nowMs() / 1000),
                      detail::timezoneOffsetSeconds_);
}

// ---- Network-synced clock ----
// Stores offset between local clock and a remote coordinator's clock.
// setSyncTime() is called once per sync event (cheap: one subtraction).
// syncNow() is called on every read (cheap: one addition).

inline void setSyncTime(tick_ms_t remoteMs)
{
        detail::sync_.offsetMs = remoteMs - millis();
        detail::sync_.active = true;
}

inline void setSyncTimeUs(tick_us_t remoteUs)
{
        detail::sync_.offsetUs = remoteUs - micros();
        detail::sync_.active = true;
}

inline tick_ms_t syncNow()
{
        return millis() + detail::sync_.offsetMs;
}

inline tick_us_t syncNowUs()
{
        return micros() + detail::sync_.offsetUs;
}

inline int64_t syncOffset()
{
        return detail::sync_.offsetMs;
}

inline int64_t syncOffsetUs()
{
        return detail::sync_.offsetUs;
}

inline bool isSynced()
{
        return detail::sync_.active;
}

inline void clearSync()
{
        detail::sync_.offsetMs = 0;
        detail::sync_.offsetUs = 0;
        detail::sync_.active = false;
}

// ---- Convenience delays ----

/// Alias for delayMs(). Kept short so call sites read naturally:
///   `ungula::core::time::delay(2000);`
inline void delay(duration_ms_t msv)
{
        delayMs(msv);
}

/// Minimum-delay cooperative yield. Guaranteed to drop the current
/// task long enough that the lowest-priority task on the system gets
/// a chance to run — on FreeRTOS targets that's the IDLE task that
/// feeds the watchdog.
///
/// Why this exists: the obvious "yield" (`delay(0)`, or `vTaskDelay(0)`,
/// or `std::this_thread::yield()`) does NOT block. On ESP-IDF with the
/// stock 100 Hz tick, even `delay(1)` rounds down to zero ticks and
/// `vTaskDelay(0)` only triggers a `taskYIELD()` that can't reach IDLE
/// (IDLE is priority 0; the main task is priority 1). Call `yield()`
/// from a tight supervisory loop and you can be confident the watchdog
/// gets fed without knowing the tick rate of your target.
///
/// Cost: one scheduler tick on RTOS targets (10 ms on stock ESP-IDF,
/// 1 ms if `CONFIG_FREERTOS_HZ` is raised to 1000). On host, one
/// `std::this_thread::yield()` — does not block.
///
/// Declared here; defined in the platform header for the actual
/// minimum-blocking semantics per kernel.
void yield();

} // namespace ungula::core::time

// ---- Platform dispatch ----
// Each platform header contains inline definitions of the platform-
// specific functions (millis, micros, delay*, delayUntil*). No
// cross-platform #ifdefs inside those files — each is pure for its target.

#ifdef ESP_PLATFORM
#include "platforms/time_control_esp32.h"
#else
#include "platforms/time_control_host.h"
#endif
