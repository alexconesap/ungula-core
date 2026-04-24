// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

/// @brief Host backend for TimeControl — std::chrono monotonic time,
/// std::this_thread for delays.
///
/// Used for desktop unit tests (no FreeRTOS available). Semantics match
/// the ESP backend close enough that host tests exercise real code paths
/// rather than a dedicated mock. Reached via the default `#else` branch
/// of `time_control.h`; no cross-platform guards inside this file.

#include <chrono>
#include <thread>

namespace ungula {

    namespace detail {

        // Fixed epoch captured on first call. Makes millis()/micros() start
        // near zero at program boot instead of std::chrono's arbitrary base.
        inline std::chrono::steady_clock::time_point hostEpoch() {
            static const auto epoch = std::chrono::steady_clock::now();
            return epoch;
        }

    }  // namespace detail

    inline TimeControl::ms_tick_t TimeControl::millis() {
        using namespace std::chrono;
        return static_cast<ms_tick_t>(
                duration_cast<milliseconds>(steady_clock::now() - detail::hostEpoch()).count());
    }

    inline TimeControl::us_tick_t TimeControl::micros() {
        using namespace std::chrono;
        return static_cast<us_tick_t>(
                duration_cast<microseconds>(steady_clock::now() - detail::hostEpoch()).count());
    }

    inline void TimeControl::delayMs(time_ms_t msv) {
        std::this_thread::sleep_for(std::chrono::milliseconds(msv));
    }

    inline void TimeControl::delayUs(time_us_t usv) {
        std::this_thread::sleep_for(std::chrono::microseconds(usv));
    }

    inline bool TimeControl::hasReachedMs(ms_tick_t now, ms_tick_t target) {
        return static_cast<int32_t>(now - target) >= 0;
    }

    inline bool TimeControl::hasReachedUs(us_tick_t now, us_tick_t target) {
        return static_cast<int32_t>(now - target) >= 0;
    }

    inline void TimeControl::delayUntilMs(ms_tick_t& reference, time_ms_t periodMs) {
        if (periodMs == 0U) {
            return;
        }
        // Advance 'reference' by exactly 'periodMs' (drift-free). Sleep
        // the remaining wall time only if we haven't crossed the boundary.
        const ms_tick_t target = reference + periodMs;
        const ms_tick_t now = millis();
        if (!hasReachedMs(now, target)) {
            delayMs(target - now);
        }
        reference = target;
    }

    inline void TimeControl::delayUntilUs(us_tick_t& reference, time_us_t periodUs) {
        if (periodUs == 0U) {
            return;
        }
        const us_tick_t target = reference + periodUs;
        const us_tick_t now = micros();
        if (!hasReachedUs(now, target)) {
            delayUs(target - now);
        }
        reference = target;
    }

}  // namespace ungula
