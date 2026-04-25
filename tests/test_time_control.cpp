// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <time/time_control.h>
#include <time/i_time_provider.h>
#include <time/timezones.h>

#include <cstdint>
#include <cstring>
#include <set>
#include <string>

namespace {

    using ungula::ITimeProvider;
    using ungula::TimeControl;
    namespace tz = ungula::tz;

    // Scripted provider that lets the test control both the reported time
    // and whether the provider reports itself valid. Used to exercise every
    // branch of TimeControl::now()'s provider routing.
    class ScriptedProvider final : public ITimeProvider {
        public:
            int64_t nowMs() const override {
                return scripted_now_;
            }
            bool isValid() const override {
                return scripted_valid_;
            }

            void set(int64_t now, bool valid) {
                scripted_now_ = now;
                scripted_valid_ = valid;
            }

        private:
            int64_t scripted_now_ = 0;
            bool scripted_valid_ = false;
    };

    // Every test clears provider + sync state up-front and at teardown so
    // TimeControl's global singletons can't leak across cases.
    class TimeControlTest : public ::testing::Test {
        protected:
            void SetUp() override {
                TimeControl::clearTimeProvider();
                TimeControl::clearSync();
                TimeControl::setTimezoneOffsetSeconds(0);  // back to UTC
            }
            void TearDown() override {
                TimeControl::clearTimeProvider();
                TimeControl::clearSync();
                TimeControl::setTimezoneOffsetSeconds(0);
            }
    };

    // ---- Monotonic local clock ----

    TEST_F(TimeControlTest, MillisIsMonotonic) {
        const auto a = TimeControl::millis();
        TimeControl::delayMs(2);
        const auto b = TimeControl::millis();
        EXPECT_GE(b, a);
    }

    TEST_F(TimeControlTest, MicrosIsMonotonic) {
        const auto a = TimeControl::micros();
        TimeControl::delayUs(50);
        const auto b = TimeControl::micros();
        EXPECT_GE(b, a);
    }

    TEST_F(TimeControlTest, NowAliasesMillisWhenNoProviderInstalled) {
        // Without a provider, now() must match millis() behaviour — any
        // drift here means the provider hook is misrouted.
        const auto m = TimeControl::millis();
        const auto n = TimeControl::now();
        EXPECT_NEAR(static_cast<double>(n), static_cast<double>(m), 5.0);
    }

    TEST_F(TimeControlTest, NowUsAliasesMicros) {
        const auto m = TimeControl::micros();
        const auto n = TimeControl::nowUs();
        EXPECT_NEAR(static_cast<double>(n), static_cast<double>(m), 500.0);
    }

    // ---- ITimeProvider routing ----

    TEST_F(TimeControlTest, ProviderReturningValidIsUsed) {
        ScriptedProvider p;
        // Real epoch-ms value — well beyond uint32_t. Verifies the wider
        // 64-bit path actually carries the full value.
        constexpr int64_t kEpochMs = 1'763'808'000'000LL;
        p.set(kEpochMs, /*valid=*/true);
        TimeControl::setTimeProvider(&p);

        EXPECT_EQ(TimeControl::now(), kEpochMs);
        EXPECT_GT(TimeControl::now(), 0xFFFFFFFFLL);  // proves no truncation
    }

    TEST_F(TimeControlTest, ProviderReportedInvalidFallsBackToLocal) {
        ScriptedProvider p;
        p.set(1'763'808'000'000LL, /*valid=*/false);
        TimeControl::setTimeProvider(&p);

        // Provider says "don't trust me" → now() must fall back to the
        // local monotonic clock, which is tiny compared to the epoch value
        // the provider reports.
        EXPECT_LT(TimeControl::now(), 1'000'000'000LL);
    }

    TEST_F(TimeControlTest, ClearTimeProviderRestoresLocalClock) {
        ScriptedProvider p;
        p.set(555'000LL, /*valid=*/true);
        TimeControl::setTimeProvider(&p);
        EXPECT_EQ(TimeControl::now(), 555'000LL);

        TimeControl::clearTimeProvider();
        EXPECT_LT(TimeControl::now(), 555'000LL);
    }

    TEST_F(TimeControlTest, ProviderValidityIsCheckedOnEveryCall) {
        // A provider that toggles between valid/invalid mid-test — now()
        // must re-check isValid() on every call, not cache it.
        ScriptedProvider p;
        p.set(100LL, true);
        TimeControl::setTimeProvider(&p);
        EXPECT_EQ(TimeControl::now(), 100LL);

        p.set(200LL, false);
        EXPECT_LT(TimeControl::now(), 200LL);  // fell back to local.

        p.set(300LL, true);
        EXPECT_EQ(TimeControl::now(), 300LL);
    }

    // ---- Timezone ----

    TEST_F(TimeControlTest, DefaultTimezoneOffsetIsUtc) {
        EXPECT_EQ(TimeControl::timezoneOffsetSeconds(), 0);
        // No provider, no offset → nowLocal() == now() exactly.
        EXPECT_EQ(TimeControl::nowLocal(), TimeControl::now());
    }

    TEST_F(TimeControlTest, NowLocalAppliesStoredOffset) {
        ScriptedProvider p;
        constexpr int64_t kEpochMs = 1'700'000'000'000LL;
        p.set(kEpochMs, true);
        TimeControl::setTimeProvider(&p);

        // EST: UTC-5h. nowLocal() = UTC + offset.
        constexpr int32_t kEstOffsetS = -5 * 3600;
        TimeControl::setTimezoneOffsetSeconds(kEstOffsetS);

        EXPECT_EQ(TimeControl::nowUtc(), kEpochMs);
        EXPECT_EQ(TimeControl::nowLocal(), kEpochMs + (kEstOffsetS * 1000LL));
        EXPECT_EQ(TimeControl::timezoneOffsetSeconds(), kEstOffsetS);
    }

    TEST_F(TimeControlTest, NowInTzIsIndependentOfStoredOffset) {
        ScriptedProvider p;
        constexpr int64_t kEpochMs = 1'700'000'000'000LL;
        p.set(kEpochMs, true);
        TimeControl::setTimeProvider(&p);

        TimeControl::setTimezoneOffsetSeconds(-5 * 3600);  // EST stored

        // Asking for JST (+9h) must NOT consult the stored EST offset.
        constexpr int32_t kJstOffsetS = 9 * 3600;
        EXPECT_EQ(TimeControl::nowInTz(kJstOffsetS),
                  kEpochMs + (kJstOffsetS * 1000LL));

        // Stored offset still EST after the nowInTz() call.
        EXPECT_EQ(TimeControl::timezoneOffsetSeconds(), -5 * 3600);
    }

    // ---- Named-timezone helpers ----

    TEST_F(TimeControlTest, SetTimezoneNamedAppliesCorrectOffset) {
        TimeControl::setTimezone(tz::Timezone::JST);
        EXPECT_EQ(TimeControl::timezoneOffsetSeconds(), 9 * 3600);

        TimeControl::setTimezone(tz::Timezone::EST);
        EXPECT_EQ(TimeControl::timezoneOffsetSeconds(), -5 * 3600);

        TimeControl::setTimezone(tz::Timezone::UTC);
        EXPECT_EQ(TimeControl::timezoneOffsetSeconds(), 0);
    }

    TEST_F(TimeControlTest, SetTimezoneNamedFlowsThroughNowLocal) {
        ScriptedProvider p;
        constexpr int64_t kEpochMs = 1'700'000'000'000LL;
        p.set(kEpochMs, true);
        TimeControl::setTimeProvider(&p);

        TimeControl::setTimezone(tz::Timezone::IST_IN);  // +5:30
        const int64_t expectedOffsetMs = ((5 * 3600) + (30 * 60)) * 1000LL;
        EXPECT_EQ(TimeControl::nowLocal(), kEpochMs + expectedOffsetMs);
    }

    TEST(TimezoneTableTest, OffsetsMatchExpected) {
        EXPECT_EQ(tz::offsetSeconds(tz::Timezone::UTC), 0);
        EXPECT_EQ(tz::offsetSeconds(tz::Timezone::JST), 9 * 3600);
        EXPECT_EQ(tz::offsetSeconds(tz::Timezone::IST_IN), (5 * 3600) + (30 * 60));
        EXPECT_EQ(tz::offsetSeconds(tz::Timezone::NST), -((3 * 3600) + (30 * 60)));
        EXPECT_EQ(tz::offsetSeconds(tz::Timezone::HST), -10 * 3600);
        EXPECT_EQ(tz::offsetSeconds(tz::Timezone::NZDT), 13 * 3600);
    }

    TEST(TimezoneTableTest, AbbreviationsLookUpCorrectly) {
        EXPECT_STREQ(tz::abbreviation(tz::Timezone::UTC), "UTC");
        EXPECT_STREQ(tz::abbreviation(tz::Timezone::JST), "JST");
        EXPECT_STREQ(tz::abbreviation(tz::Timezone::IST_IN), "IST");
        // Region-suffixed enums collapse to the short name in display.
        EXPECT_STREQ(tz::abbreviation(tz::Timezone::CST_CN), "CST");
        EXPECT_STREQ(tz::abbreviation(tz::Timezone::CST_NA), "CST");
    }

    // Canonical inventory — every Timezone enum value listed exactly once.
    // Updating the enum must update this list and (separately) the
    // TIMEZONES table; the next two tests fail loud if any of the three
    // ever drift apart.
    constexpr tz::Timezone kAllZones[] = {
            // 0:00 family
            tz::Timezone::UTC, tz::Timezone::GMT, tz::Timezone::WET,
            // positive offsets
            tz::Timezone::BST_UK, tz::Timezone::CET, tz::Timezone::WEST,
            tz::Timezone::CEST, tz::Timezone::EET, tz::Timezone::EEST,
            tz::Timezone::MSK, tz::Timezone::GST, tz::Timezone::PKT,
            tz::Timezone::IST_IN, tz::Timezone::BDT, tz::Timezone::ICT,
            tz::Timezone::CST_CN, tz::Timezone::SGT, tz::Timezone::AWST,
            tz::Timezone::JST, tz::Timezone::KST, tz::Timezone::ACST,
            tz::Timezone::AEST, tz::Timezone::AEDT, tz::Timezone::NZST,
            tz::Timezone::NZDT,
            // negative offsets
            tz::Timezone::BRT, tz::Timezone::ART, tz::Timezone::NST,
            tz::Timezone::AST_ATL, tz::Timezone::EST, tz::Timezone::EDT,
            tz::Timezone::CST_NA, tz::Timezone::CDT_NA, tz::Timezone::MST_NA,
            tz::Timezone::MDT_NA, tz::Timezone::PST_NA, tz::Timezone::PDT_NA,
            tz::Timezone::AKST, tz::Timezone::AKDT, tz::Timezone::HST,
    };

    constexpr size_t kAllZonesCount = sizeof(kAllZones) / sizeof(kAllZones[0]);
    constexpr size_t kTableCount = sizeof(tz::TIMEZONES) / sizeof(tz::TIMEZONES[0]);

    TEST(TimezoneTableTest, NoDuplicateEnumValuesInTable) {
        // The lookup helpers do a linear scan and stop at the first match.
        // A duplicate entry would silently route to whichever row appears
        // first — this catches the slip if anyone copy-pastes a row
        // without changing the enum.
        std::set<int> seen;
        for (const auto& entry : tz::TIMEZONES) {
            const int key = static_cast<int>(entry.tz);
            EXPECT_EQ(seen.count(key), 0U) << "duplicate enum in TIMEZONES table";
            seen.insert(key);
        }
        EXPECT_EQ(seen.size(), kTableCount);
    }

    TEST(TimezoneTableTest, EveryEnumValueHasATableRow) {
        // Catches "added an enum value but forgot to add a row to
        // TIMEZONES[]" — without this the lookup would silently return 0
        // and the bug would only surface in production.
        for (const tz::Timezone zone : kAllZones) {
            bool found = false;
            for (const auto& entry : tz::TIMEZONES) {
                if (entry.tz == zone) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << "Timezone enum value "
                               << static_cast<int>(zone)
                               << " has no row in TIMEZONES[]";
        }
    }

    TEST(TimezoneTableTest, EnumInventoryAndTableHaveSameSize) {
        // Catches "added a table row but forgot to add the enum to the
        // inventory list above" (or vice versa). Either side drifting
        // shows up here.
        EXPECT_EQ(kAllZonesCount, kTableCount);
    }

    TEST(TimezoneTableTest, OffsetsAreConstexpr) {
        // The whole table is constexpr — verify a few values resolve at
        // compile time. Failure here would surface at link, not runtime.
        constexpr int32_t jst = tz::offsetSeconds(tz::Timezone::JST);
        constexpr int32_t pst = tz::offsetSeconds(tz::Timezone::PST_NA);
        static_assert(jst == 9 * 3600, "JST table value drifted");
        static_assert(pst == -8 * 3600, "PST_NA table value drifted");
        SUCCEED();
    }

    // ---- Real-world TZ scenarios (Los Angeles + Barcelona) ----
    //
    // These exercise the two zones the project documents in its README,
    // covering both negative (Americas) and positive (Europe) offset
    // arithmetic, plus the standard/daylight pair for each.

    TEST(TimezoneTableTest, LosAngelesAndBarcelonaOffsets) {
        // Los Angeles — Pacific. PST in winter, PDT in summer (DST).
        EXPECT_EQ(tz::offsetSeconds(tz::Timezone::PST_NA), -8 * 3600);
        EXPECT_EQ(tz::offsetSeconds(tz::Timezone::PDT_NA), -7 * 3600);
        // Barcelona — Spain follows Central Europe. CET winter, CEST summer.
        EXPECT_EQ(tz::offsetSeconds(tz::Timezone::CET), 1 * 3600);
        EXPECT_EQ(tz::offsetSeconds(tz::Timezone::CEST), 2 * 3600);
    }

    TEST_F(TimeControlTest, NowLocalForLosAngeles) {
        // Anchor a known UTC instant, install as the time source, and verify
        // that nowLocal() returns the expected wall time for both halves of
        // the year (PST and PDT).
        ScriptedProvider clock;
        constexpr int64_t kUtcMs = 1'700'000'000'000LL;  // 2023-11-14 22:13:20 UTC
        clock.set(kUtcMs, true);
        TimeControl::setTimeProvider(&clock);

        // Winter — PST = UTC-8. LA wall clock = 14:13:20.
        TimeControl::setTimezone(tz::Timezone::PST_NA);
        EXPECT_EQ(TimeControl::nowLocal(), kUtcMs - (8LL * 3600 * 1000));
        EXPECT_EQ(TimeControl::nowUtc(), kUtcMs);  // UTC view unchanged

        // Summer — PDT = UTC-7. Same UTC instant, different local clock.
        TimeControl::setTimezone(tz::Timezone::PDT_NA);
        EXPECT_EQ(TimeControl::nowLocal(), kUtcMs - (7LL * 3600 * 1000));
    }

    TEST_F(TimeControlTest, NowLocalForBarcelona) {
        ScriptedProvider clock;
        constexpr int64_t kUtcMs = 1'700'000'000'000LL;
        clock.set(kUtcMs, true);
        TimeControl::setTimeProvider(&clock);

        // Winter — CET = UTC+1.
        TimeControl::setTimezone(tz::Timezone::CET);
        EXPECT_EQ(TimeControl::nowLocal(), kUtcMs + (1LL * 3600 * 1000));

        // Summer — CEST = UTC+2.
        TimeControl::setTimezone(tz::Timezone::CEST);
        EXPECT_EQ(TimeControl::nowLocal(), kUtcMs + (2LL * 3600 * 1000));
    }

    TEST_F(TimeControlTest, RuntimeTimezoneSwitchAcrossHemispheres) {
        // Switching between LA and Barcelona at runtime must take effect on
        // the very next nowLocal() call — no caching, no init-only state.
        ScriptedProvider clock;
        constexpr int64_t kUtcMs = 1'700'000'000'000LL;
        clock.set(kUtcMs, true);
        TimeControl::setTimeProvider(&clock);

        TimeControl::setTimezone(tz::Timezone::PST_NA);  // -8:00
        const int64_t la = TimeControl::nowLocal();

        TimeControl::setTimezone(tz::Timezone::CET);     // +1:00
        const int64_t bcn = TimeControl::nowLocal();

        // 9-hour delta between LA and Barcelona in winter.
        EXPECT_EQ(bcn - la, 9LL * 3600 * 1000);
    }

    TEST_F(TimeControlTest, NowInTzIgnoresStoredZoneInLaToBcnFlip) {
        // A caller temporarily formatting in another zone via nowInTz()
        // must NOT disturb the configured local zone. Locks the contract
        // for code that does e.g. "render Barcelona time in a UI tooltip
        // while the device is configured for Los Angeles".
        ScriptedProvider clock;
        constexpr int64_t kUtcMs = 1'700'000'000'000LL;
        clock.set(kUtcMs, true);
        TimeControl::setTimeProvider(&clock);

        TimeControl::setTimezone(tz::Timezone::PST_NA);

        const int32_t bcnOffset = tz::offsetSeconds(tz::Timezone::CET);
        EXPECT_EQ(TimeControl::nowInTz(bcnOffset),
                  kUtcMs + (static_cast<int64_t>(bcnOffset) * 1000));

        // Stored offset still LA after the nowInTz() call.
        EXPECT_EQ(TimeControl::timezoneOffsetSeconds(),
                  tz::offsetSeconds(tz::Timezone::PST_NA));
    }

    // ---- TimeControl::format* (delegates to time_format) ----

    TEST_F(TimeControlTest, FormatReturnsZeroWithoutValidProvider) {
        // Without a provider, "now" is monotonic-since-boot. Formatting
        // it would print 1970-01-01 — misleading. Contract is: return 0.
        char buf[32];
        std::memset(buf, 0xAA, sizeof(buf));
        EXPECT_EQ(TimeControl::formatUtc(buf, sizeof(buf)), 0U);
        EXPECT_EQ(TimeControl::formatLocal(buf, sizeof(buf)), 0U);
    }

    TEST_F(TimeControlTest, FormatUtcUsesProviderEpoch) {
        ScriptedProvider clock;
        // 1700000000 sec = 2023-11-14 22:13:20 UTC. As ms: ×1000.
        clock.set(1'700'000'000'000LL, true);
        TimeControl::setTimeProvider(&clock);

        char buf[32] = {};
        const size_t n = TimeControl::formatUtc(buf, sizeof(buf));
        EXPECT_EQ(n, 19U);
        EXPECT_STREQ(buf, "2023-11-14 22:13:20");
    }

    TEST_F(TimeControlTest, FormatLocalAppliesStoredOffset) {
        ScriptedProvider clock;
        clock.set(1'700'000'000'000LL, true);
        TimeControl::setTimeProvider(&clock);

        TimeControl::setTimezone(tz::Timezone::PST_NA);  // -8:00
        char buf[32] = {};
        TimeControl::formatLocal(buf, sizeof(buf));
        EXPECT_STREQ(buf, "2023-11-14 14:13:20");

        TimeControl::setTimezone(tz::Timezone::CET);  // +1:00
        TimeControl::formatLocal(buf, sizeof(buf));
        EXPECT_STREQ(buf, "2023-11-14 23:13:20");
    }

    TEST_F(TimeControlTest, FormatCustomSpecHonoursStoredOffset) {
        ScriptedProvider clock;
        clock.set(1'700'000'000'000LL, true);
        TimeControl::setTimeProvider(&clock);
        TimeControl::setTimezone(tz::Timezone::JST);  // +9:00

        char buf[16] = {};
        TimeControl::format(buf, sizeof(buf), "%H:%M");
        EXPECT_STREQ(buf, "07:13");  // anchor + 9h, with date carry already applied
    }

    TEST_F(TimeControlTest, NowUtcAndNowAreEquivalent) {
        ScriptedProvider p;
        p.set(1'234'567'890LL, true);
        TimeControl::setTimeProvider(&p);
        TimeControl::setTimezoneOffsetSeconds(7 * 3600);  // would shift if used

        // now() / nowUtc() must IGNORE the stored offset.
        EXPECT_EQ(TimeControl::now(), 1'234'567'890LL);
        EXPECT_EQ(TimeControl::nowUtc(), TimeControl::now());
    }

    // ---- Sync clock (independent from provider path) ----

    TEST_F(TimeControlTest, SyncNowWithoutSyncMatchesLocalMillis) {
        EXPECT_FALSE(TimeControl::isSynced());
        const auto m = TimeControl::millis();
        const auto s = TimeControl::syncNow();
        EXPECT_NEAR(static_cast<double>(s), static_cast<double>(m), 5.0);
        EXPECT_EQ(TimeControl::syncOffset(), 0);
    }

    TEST_F(TimeControlTest, SetSyncTimeComputesCorrectOffset) {
        // Inject a coordinator's ms. syncNow() should land near that value
        // for a short window after injection (before too much local time
        // has elapsed).
        const int64_t coordinatorMs = 42'000'000LL;
        TimeControl::setSyncTime(coordinatorMs);
        EXPECT_TRUE(TimeControl::isSynced());

        const auto s = TimeControl::syncNow();
        // Allow a generous 50 ms window for scheduler jitter on the host.
        EXPECT_NEAR(static_cast<double>(s), static_cast<double>(coordinatorMs), 50.0);
    }

    TEST_F(TimeControlTest, ClearSyncResetsOffsetAndFlag) {
        TimeControl::setSyncTime(123'456U);
        EXPECT_TRUE(TimeControl::isSynced());

        TimeControl::clearSync();
        EXPECT_FALSE(TimeControl::isSynced());
        EXPECT_EQ(TimeControl::syncOffset(), 0);
    }

    // ---- delayUntilMs drift-free scheduling ----

    TEST_F(TimeControlTest, DelayUntilMsAdvancesReferenceByPeriod) {
        auto ref = TimeControl::millis();
        const auto refBefore = ref;
        TimeControl::delayUntilMs(ref, 5);
        // delayUntilMs must bump 'reference' by exactly the period, not
        // by however long the delay actually took. Drift-free by design.
        EXPECT_EQ(ref - refBefore, 5LL);
    }

    TEST_F(TimeControlTest, DelayUntilMsZeroPeriodIsNoOp) {
        auto ref = TimeControl::millis();
        const auto refBefore = ref;
        TimeControl::delayUntilMs(ref, 0);
        EXPECT_EQ(ref, refBefore);
    }

}  // namespace
