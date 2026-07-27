// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// ============================================================================
// Named persistent counters (lifetime tallies: runs, boots, cycles, faults).
//
// One counter = one `uint32_t` under its own key in a dedicated namespace, so a
// counter write never touches the config/program blobs. The backend is whatever
// IPreferences implementation the project injects (ESP32 NVS today, any other
// platform tomorrow) — this unit never names the storage technology.
//
// Usage:
//   ungula::core::preferences::counters::CounterStore counters(prefs);
//   counters.increment("runs");          // read + 1 + write, returns the new value
//   uint32_t runs = counters.read("runs");
//
// Wear: every write costs one storage entry. Meant for per-run / per-boot
// events (tens per day), NOT for per-loop or per-tick tallies — count those in
// RAM and persist the total once, when the run ends.
// ============================================================================

#include <cstdint>

#include <ungula/core/preferences/i_preferences.h>

namespace ungula::core::preferences::counters
{

/// Namespace used when the caller does not supply one.
inline constexpr const char *DEFAULT_NAMESPACE = "counters";

/// Longest counter name accepted. ESP32 NVS keys are capped at 15 chars + NUL;
/// a longer name is rejected instead of silently truncated (two names sharing
/// the first 15 chars would otherwise collide into one counter).
inline constexpr unsigned MAX_NAME_LEN = 15;

/// Counters saturate here instead of wrapping to 0. A lifetime tally that rolls
/// over is worse than one that sticks at the top: 2^32 runs is ~1.1 million
/// years at 10 runs/day, so hitting it means something is wrong.
inline constexpr uint32_t MAX_VALUE = UINT32_MAX;

/// Named counters persisted through an injected IPreferences backend.
///
/// Holds no state beyond the reference and the namespace, so it is cheap to
/// build at the call site — no need to keep one alive:
///
///     CounterStore(prefs).increment("runs");
///
/// Not thread-safe: the injected IPreferences owns a single backend handle, so
/// all counter calls must come from the same task as every other use of that
/// instance (in practice the app loop).
class CounterStore {
    public:
        explicit CounterStore(IPreferences &prefs, const char *ns = DEFAULT_NAMESPACE)
                : prefs_(prefs)
                , ns_(ns)
        {
        }

        /// Current value. Returns 0 when the counter was never created, when the
        /// name is invalid, or when the backend refuses to open.
        uint32_t read(const char *name) const
        {
                if (!isValidName(name) || !prefs_.begin(ns_)) {
                        return 0;
                }
                const uint32_t value = prefs_.getUInt32(name, 0);
                prefs_.end();
                return value;
        }

        /// Set the counter to `value`, creating it when it does not exist yet.
        /// Returns false on an invalid name or a backend failure.
        bool write(const char *name, uint32_t value) const
        {
                if (!isValidName(name) || !prefs_.begin(ns_)) {
                        return false;
                }
                const bool ok = prefs_.putUInt32(name, value);
                prefs_.end();
                return ok;
        }

        /// Read, add `step`, write back. Returns the new stored value, or 0 if
        /// the name is invalid or the write failed — a successful increment can
        /// never return 0 because the value saturates at MAX_VALUE.
        uint32_t increment(const char *name, uint32_t step = 1) const
        {
                if (!isValidName(name) || !prefs_.begin(ns_)) {
                        return 0;
                }
                const uint32_t current = prefs_.getUInt32(name, 0);
                const uint32_t next = (MAX_VALUE - current < step) ? MAX_VALUE : current + step;
                const bool ok = prefs_.putUInt32(name, next);
                prefs_.end();
                return ok ? next : 0;
        }

        /// Set the counter back to 0 (creating it if needed). Same contract as
        /// write(name, 0) — the key stays, so read() keeps returning a real 0.
        bool reset(const char *name) const
        {
                return write(name, 0);
        }

        /// True when the counter was created at some point. Only needed to tell
        /// "never counted" from "counted zero times" — read() gives 0 for both.
        bool exists(const char *name) const
        {
                if (!isValidName(name) || !prefs_.begin(ns_)) {
                        return false;
                }
                const bool found = prefs_.hasKey(name);
                prefs_.end();
                return found;
        }

        /// Name rule check: 1..MAX_NAME_LEN chars. Public so a project can
        /// validate operator-supplied names before calling.
        static bool isValidName(const char *name)
        {
                if (name == nullptr || name[0] == '\0') {
                        return false;
                }
                for (unsigned i = 0; i <= MAX_NAME_LEN; ++i) {
                        if (name[i] == '\0') {
                                return true;
                        }
                }
                return false; // no terminator within the limit — too long
        }

    private:
        IPreferences &prefs_;
        const char *ns_;
};

} // namespace ungula::core::preferences::counters
