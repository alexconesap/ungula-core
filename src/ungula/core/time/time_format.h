// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <ctime>

/// @brief Pure date/time formatter — `strftime` over an explicit
/// (epoch seconds, offset seconds) pair.
///
/// Knows nothing about NTP, RTC, FreeRTOS, or the time API's provider
/// hook. Callers supply the value; the formatter prints it. This is the
/// building block that `formatLocal()` / `formatUtc()` and the NTP-side
/// wrapper (when one exists) delegate to.
///
/// All functions use `gmtime_r` internally — the offset is applied
/// arithmetically. The system's TZ environment is never consulted, so
/// behaviour is identical across hosts and ESP32 builds.

namespace ungula::core::time
{

/// Format an arbitrary epoch instant with a custom strftime spec.
///
/// @param buf Destination buffer.
/// @param bufSize Capacity of `buf` in bytes (must be > 0).
/// @param strftimeFmt Format string, e.g. "%Y-%m-%d %H:%M:%S".
/// @param epochSeconds UTC epoch seconds (use `time_t(0)` to
///   signal "no valid time" — the function returns 0 then).
/// @param offsetSeconds Offset in seconds to add before formatting
///   (positive east of UTC). Default 0 = UTC output.
/// @return Number of characters written, or 0 on failure
///   (small buffer, null inputs, or epoch == 0).
inline size_t format(char *buf, size_t bufSize, const char *strftimeFmt, time_t epochSeconds,
                     int32_t offsetSeconds = 0)
{
        if (buf == nullptr || bufSize == 0 || strftimeFmt == nullptr) {
                return 0;
        }
        if (epochSeconds <= 0) {
                buf[0] = '\0';
                return 0;
        }
        const time_t shifted = epochSeconds + offsetSeconds;
        struct tm timeinfo{};
        gmtime_r(&shifted, &timeinfo);
        return strftime(buf, bufSize, strftimeFmt, &timeinfo);
}

/// Convenience for the common log-line shape "YYYY-MM-DD HH:MM:SS".
/// Buffer must be at least 20 bytes (19 chars + null terminator).
inline size_t formatIso8601(char *buf, size_t bufSize, time_t epochSeconds,
                            int32_t offsetSeconds = 0)
{
        return format(buf, bufSize, "%Y-%m-%d %H:%M:%S", epochSeconds, offsetSeconds);
}

} // namespace ungula::core::time
