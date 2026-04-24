// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stdint.h>

/// @brief Named timezones with fixed UTC offsets.
///
/// Lets the application say `Timezone::JST` instead of computing
/// `9 * 3600` by hand. The mapping lives in a `constexpr` table so
/// the compiler folds it into a single load — no runtime cost.
///
/// ## What this is and isn't
///
/// - **Is**: a static name → UTC-offset table for ~30 commonly used
///   abbreviations.
/// - **Is not**: a tzdata implementation. There is no DST awareness
///   here. Zones that observe DST appear twice (e.g. `EST` for winter
///   and `EDT` for summer) and the application chooses which one is
///   currently in effect. For real DST handling, format via the NTP
///   client's strftime path (`ntp_format_local`) which uses the C
///   library's `tm` machinery.
/// - **Naming collisions**: a few abbreviations clash across regions
///   (e.g. CST is both US Central -6:00 and China Standard +8:00).
///   Disambiguated with a region suffix: `CST_NA` vs `CST_CN`,
///   `IST_IN` (India), `AST_ATL` (Atlantic).

namespace ungula {
    namespace tz {

        /// @brief Named timezones. Stable across versions — entries can
        /// be added at the end, but existing values must not be reordered
        /// since downstream code may be persisting them as integers.
        enum class Timezone : uint8_t {
                // ---- 0:00 family ----
                UTC = 0,
                GMT,            // +0:00
                WET,            // +0:00 — Western European
                // ---- positive offsets ----
                BST_UK,         // +1:00 — British Summer Time
                CET,            // +1:00 — Central European
                WEST,           // +1:00 — Western European Summer
                CEST,           // +2:00 — Central European Summer
                EET,            // +2:00 — Eastern European
                EEST,           // +3:00 — Eastern European Summer
                MSK,            // +3:00 — Moscow
                GST,            // +4:00 — Gulf (UAE)
                PKT,            // +5:00 — Pakistan
                IST_IN,         // +5:30 — India
                BDT,            // +6:00 — Bangladesh
                ICT,            // +7:00 — Indochina (TH/VN/KH/LA)
                CST_CN,         // +8:00 — China Standard
                SGT,            // +8:00 — Singapore
                AWST,           // +8:00 — Australian Western
                JST,            // +9:00 — Japan
                KST,            // +9:00 — Korea
                ACST,           // +9:30 — Australian Central
                AEST,           // +10:00 — Australian Eastern
                AEDT,           // +11:00 — Australian Eastern Daylight
                NZST,           // +12:00 — New Zealand
                NZDT,           // +13:00 — New Zealand Daylight
                // ---- negative offsets ----
                BRT,            // -3:00 — Brazil (Brasília)
                ART,            // -3:00 — Argentina
                NST,            // -3:30 — Newfoundland Standard
                AST_ATL,        // -4:00 — Atlantic Standard (Halifax, Puerto Rico, etc.)
                EST,            // -5:00 — Eastern Standard (US/Canada)
                EDT,            // -4:00 — Eastern Daylight
                CST_NA,         // -6:00 — Central Standard (US/Canada)
                CDT_NA,         // -5:00 — Central Daylight
                MST_NA,         // -7:00 — Mountain Standard
                MDT_NA,         // -6:00 — Mountain Daylight
                PST_NA,         // -8:00 — Pacific Standard
                PDT_NA,         // -7:00 — Pacific Daylight
                AKST,           // -9:00 — Alaska Standard
                AKDT,           // -8:00 — Alaska Daylight
                HST,            // -10:00 — Hawaii Standard (no DST)
        };

        struct Entry {
                Timezone tz;
                int32_t offsetSeconds;
                const char* abbreviation;
        };

        /// Single source of truth for the (enum → offset, name) mapping.
        /// `constexpr` so callers can use it at compile time too.
        inline constexpr Entry TIMEZONES[] = {
                {Timezone::UTC, 0, "UTC"},
                {Timezone::GMT, 0, "GMT"},
                {Timezone::WET, 0, "WET"},

                {Timezone::BST_UK, 1 * 3600, "BST"},
                {Timezone::CET, 1 * 3600, "CET"},
                {Timezone::WEST, 1 * 3600, "WEST"},
                {Timezone::CEST, 2 * 3600, "CEST"},
                {Timezone::EET, 2 * 3600, "EET"},
                {Timezone::EEST, 3 * 3600, "EEST"},
                {Timezone::MSK, 3 * 3600, "MSK"},
                {Timezone::GST, 4 * 3600, "GST"},
                {Timezone::PKT, 5 * 3600, "PKT"},
                {Timezone::IST_IN, (5 * 3600) + (30 * 60), "IST"},
                {Timezone::BDT, 6 * 3600, "BDT"},
                {Timezone::ICT, 7 * 3600, "ICT"},
                {Timezone::CST_CN, 8 * 3600, "CST"},
                {Timezone::SGT, 8 * 3600, "SGT"},
                {Timezone::AWST, 8 * 3600, "AWST"},
                {Timezone::JST, 9 * 3600, "JST"},
                {Timezone::KST, 9 * 3600, "KST"},
                {Timezone::ACST, (9 * 3600) + (30 * 60), "ACST"},
                {Timezone::AEST, 10 * 3600, "AEST"},
                {Timezone::AEDT, 11 * 3600, "AEDT"},
                {Timezone::NZST, 12 * 3600, "NZST"},
                {Timezone::NZDT, 13 * 3600, "NZDT"},

                {Timezone::BRT, -3 * 3600, "BRT"},
                {Timezone::ART, -3 * 3600, "ART"},
                {Timezone::NST, -((3 * 3600) + (30 * 60)), "NST"},
                {Timezone::AST_ATL, -4 * 3600, "AST"},
                {Timezone::EST, -5 * 3600, "EST"},
                {Timezone::EDT, -4 * 3600, "EDT"},
                {Timezone::CST_NA, -6 * 3600, "CST"},
                {Timezone::CDT_NA, -5 * 3600, "CDT"},
                {Timezone::MST_NA, -7 * 3600, "MST"},
                {Timezone::MDT_NA, -6 * 3600, "MDT"},
                {Timezone::PST_NA, -8 * 3600, "PST"},
                {Timezone::PDT_NA, -7 * 3600, "PDT"},
                {Timezone::AKST, -9 * 3600, "AKST"},
                {Timezone::AKDT, -8 * 3600, "AKDT"},
                {Timezone::HST, -10 * 3600, "HST"},
        };

        /// Resolve a Timezone to its UTC offset in seconds.
        /// Linear scan, but the table is small and `constexpr` so the
        /// compiler folds the whole thing for compile-time arguments.
        constexpr int32_t offsetSeconds(Timezone tz) {
            for (const auto& entry : TIMEZONES) {
                if (entry.tz == tz) {
                    return entry.offsetSeconds;
                }
            }
            return 0;
        }

        /// Resolve a Timezone to its short string ("UTC", "JST", "EST", ...).
        constexpr const char* abbreviation(Timezone tz) {
            for (const auto& entry : TIMEZONES) {
                if (entry.tz == tz) {
                    return entry.abbreviation;
                }
            }
            return "UTC";
        }

    }  // namespace tz
}  // namespace ungula
