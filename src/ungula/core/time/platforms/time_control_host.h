// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

/// @brief Host backend for the time API — std::chrono monotonic time,
/// std::this_thread for delays.
///
/// Used for desktop unit tests (no FreeRTOS available). Semantics match
/// the ESP backend close enough that host tests exercise real code paths
/// rather than a dedicated mock. Reached via the default `#else` branch
/// of `time_control.h`; no cross-platform guards inside this file.
///
/// All time values are int64_t end-to-end — see the comment at the top
/// of `time_control.h` for the rationale.

#include <chrono>
#include <thread>

namespace ungula::core::time::detail
{

    // Fixed epoch captured on first call. Makes millis()/micros() start
    // near zero at program boot instead of std::chrono's arbitrary base.
    inline std::chrono::steady_clock::time_point hostEpoch()
    {
        static const auto epoch = std::chrono::steady_clock::now();
        return epoch;
    }

} // namespace ungula::core::time::detail

namespace ungula::core::time
{

    inline tick_ms_t millis()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now() - detail::hostEpoch()).count();
    }

    inline tick_us_t micros()
    {
        using namespace std::chrono;
        return duration_cast<microseconds>(steady_clock::now() - detail::hostEpoch()).count();
    }

    inline void delayMs(duration_ms_t msv)
    {
        if (msv <= 0) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(msv));
    }

    inline void delayUs(duration_us_t usv)
    {
        if (usv <= 0) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(usv));
    }

    inline void delayUntilMs(tick_ms_t &reference, duration_ms_t periodMs)
    {
        if (periodMs <= 0) {
            return;
        }
        // Advance 'reference' by exactly 'periodMs' (drift-free). Sleep
        // the remaining wall time only if we haven't crossed the boundary.
        const tick_ms_t target = reference + periodMs;
        const tick_ms_t now = millis();
        if (now < target) {
            delayMs(target - now);
        }
        reference = target;
    }

    inline void delayUntilUs(tick_us_t &reference, duration_us_t periodUs)
    {
        if (periodUs <= 0) {
            return;
        }
        const tick_us_t target = reference + periodUs;
        const tick_us_t now = micros();
        if (now < target) {
            delayUs(target - now);
        }
        reference = target;
    }

} // namespace ungula::core::time
