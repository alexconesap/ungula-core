// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stdint.h>

namespace ungula::core::time
{

/// @brief Pluggable source of absolute wall-clock time in milliseconds.
///
/// `ungula::core::time::now()` falls back to the local monotonic
/// `millis()` when no provider is installed. Install one via
/// `ungula::core::time::setTimeProvider()` to route `now()` through a
/// different clock — an NTP sync, an RTC chip, a mocked source for
/// tests, etc.
///
/// Implementations must be cheap (called on every `now()`) and must
/// report honestly via `isValid()` whether the returned value is usable.
/// When `isValid()` is false, `now()` falls back to the local clock.
class ITimeProvider {
    public:
        virtual ~ITimeProvider() = default;

        /// Current time, in milliseconds. Signed 64-bit:
        ///   - 64-bit so wall-clock epoch-ms fits without truncation.
        ///   - Signed so callers can subtract two values and detect
        ///     "in the future" (negative) cleanly. ESP-IDF and POSIX
        ///     `time_t` are both signed for the same reason.
        ///
        /// The meaning (UTC epoch? local epoch? uptime?) is the
        /// implementation's choice — by convention providers return
        /// UTC epoch-ms when they represent a wall clock, leaving
        /// timezone shifting to `nowInTz()`.
        virtual int64_t nowMs() const = 0;

        /// True when `nowMs()` is trustworthy. Returning false makes
        /// `now()` fall back to the local monotonic clock.
        virtual bool isValid() const = 0;
};

} // namespace ungula::core::time
