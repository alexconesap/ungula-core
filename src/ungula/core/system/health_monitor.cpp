// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "ungula/core/system/health_monitor.h"

#include "ungula/core/time/time_control.h"

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#else
// Host build (unit tests). The heap counters are stubbed to zero so the
// caller still sees a valid sample with delta=0 forever.
#endif

namespace ungula::core::system {

    namespace {

        inline uint32_t free_heap_now() {
#if defined(ESP_PLATFORM)
            return static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
#else
            return 0U;
#endif
        }

        inline uint32_t min_free_heap_ever() {
#if defined(ESP_PLATFORM)
            return static_cast<uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
#else
            return 0U;
#endif
        }

    }  // namespace

    bool HealthMonitor::sample(uint32_t intervalMs, HealthSample& out) {
        const uint32_t now = ungula::core::time::millis();

        // First call always fires so the caller gets an immediate baseline.
        if (!first_ && (now - last_sample_ms_) < intervalMs) {
            return false;
        }

        const uint32_t free_now = free_heap_now();

        out.free_heap = free_now;
        out.min_free_heap = min_free_heap_ever();
        out.delta =
                first_ ? 0 : (static_cast<int32_t>(free_now) - static_cast<int32_t>(last_free_));
        out.uptime_ms = now;

        last_sample_ms_ = now;
        last_free_ = free_now;
        first_ = false;
        return true;
    }

    void HealthMonitor::reset() {
        first_ = true;
        last_sample_ms_ = 0;
        last_free_ = 0;
    }

}  // namespace ungula::core::system
