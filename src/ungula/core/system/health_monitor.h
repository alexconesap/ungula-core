// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stdint.h>

// HealthMonitor — periodic sampler for free heap and related runtime stats.
//
// Embedded firmware should never grow its memory footprint over time. Static
// allocation, fixed-capacity buffers and zero-heap hot paths give that
// property by construction, but a single accidental `new` in a hot path or a
// leaked allocation inside a third-party component can break it silently and
// the device only fails after hours or days in the field.
//
// The cure is cheap: sample the free-heap counter once a minute, log it, and
// look at the trend. If `free_heap` oscillates around a fixed value the
// system is healthy. If it drifts down monotonically, something is leaking
// and you have minutes — not weeks — to find it.
//
// HealthMonitor only collects the numbers and gates the sampling rate. It
// does NOT log: by project convention libraries never call the logger
// directly. The caller decides what to do with the sample (log it, push it
// to a dashboard, raise an alarm, etc.).
//
// Usage:
//
//   static ungula::HealthMonitor health;
//
//   void loop() {
//     ungula::HealthSample s;
//     if (health.sample(60000U, s)) {
//       log_info_m("heap", "free=%u min=%u delta=%ld up=%u",
//                  s.free_heap, s.min_free_heap, (long)s.delta, s.uptime_ms);
//     }
//   }

namespace ungula::core::system
{

struct HealthSample {
        uint32_t free_heap; // current free bytes (default heap caps)
        uint32_t min_free_heap; // minimum free ever observed since boot
        int32_t delta; // free_heap - previous_free_heap (0 on first sample)
        uint32_t uptime_ms; // monotonic uptime in milliseconds
};

class HealthMonitor final {
    public:
        HealthMonitor() = default;

        // Take a sample if at least intervalMs have passed since the previous
        // one. Returns true and fills `out` when a new sample was taken,
        // false otherwise (in which case `out` is left untouched).
        //
        // The first call after construction always returns true with delta=0.
        // Subsequent calls compute delta against the previous sample.
        bool sample(uint32_t intervalMs, HealthSample &out);

        // Force-reset the internal state — useful after a deliberate large
        // allocation or free, so the next delta is measured against the new
        // baseline rather than the pre-event one.
        void reset();

    private:
        uint32_t last_sample_ms_ = 0;
        uint32_t last_free_ = 0;
        bool first_ = true;
};

} // namespace ungula::core::system
