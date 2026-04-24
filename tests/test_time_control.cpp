// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <time/time_control.h>
#include <time/i_time_provider.h>

#include <cstdint>

namespace {

    using ungula::ITimeProvider;
    using ungula::TimeControl;

    // Scripted provider that lets the test control both the reported time
    // and whether the provider reports itself valid. Used to exercise every
    // branch of TimeControl::now()'s provider routing.
    class ScriptedProvider final : public ITimeProvider {
        public:
            uint32_t nowMs() const override {
                return scripted_now_;
            }
            bool isValid() const override {
                return scripted_valid_;
            }

            void set(uint32_t now, bool valid) {
                scripted_now_ = now;
                scripted_valid_ = valid;
            }

        private:
            uint32_t scripted_now_ = 0;
            bool scripted_valid_ = false;
    };

    // Every test clears provider + sync state up-front and at teardown so
    // TimeControl's global singletons can't leak across cases.
    class TimeControlTest : public ::testing::Test {
        protected:
            void SetUp() override {
                TimeControl::clearTimeProvider();
                TimeControl::clearSync();
            }
            void TearDown() override {
                TimeControl::clearTimeProvider();
                TimeControl::clearSync();
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
        p.set(1'700'000'000U, /*valid=*/true);
        TimeControl::setTimeProvider(&p);

        EXPECT_EQ(TimeControl::now(), 1'700'000'000U);
    }

    TEST_F(TimeControlTest, ProviderReportedInvalidFallsBackToLocal) {
        ScriptedProvider p;
        p.set(1'700'000'000U, /*valid=*/false);
        TimeControl::setTimeProvider(&p);

        // Provider says "don't trust me" → now() must fall back to the
        // local monotonic clock, which is tiny compared to the epoch value
        // the provider reports.
        EXPECT_LT(TimeControl::now(), 1'000'000'000U);
    }

    TEST_F(TimeControlTest, ClearTimeProviderRestoresLocalClock) {
        ScriptedProvider p;
        p.set(555'000U, /*valid=*/true);
        TimeControl::setTimeProvider(&p);
        EXPECT_EQ(TimeControl::now(), 555'000U);

        TimeControl::clearTimeProvider();
        EXPECT_LT(TimeControl::now(), 555'000U);
    }

    TEST_F(TimeControlTest, ProviderValidityIsCheckedOnEveryCall) {
        // A provider that toggles between valid/invalid mid-test — now()
        // must re-check isValid() on every call, not cache it.
        ScriptedProvider p;
        p.set(100U, true);
        TimeControl::setTimeProvider(&p);
        EXPECT_EQ(TimeControl::now(), 100U);

        p.set(200U, false);
        EXPECT_LT(TimeControl::now(), 200U);  // fell back to local.

        p.set(300U, true);
        EXPECT_EQ(TimeControl::now(), 300U);
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
        const uint32_t coordinatorMs = 42'000'000U;
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
        EXPECT_EQ(ref - refBefore, 5U);
    }

    TEST_F(TimeControlTest, DelayUntilMsZeroPeriodIsNoOp) {
        auto ref = TimeControl::millis();
        const auto refBefore = ref;
        TimeControl::delayUntilMs(ref, 0);
        EXPECT_EQ(ref, refBefore);
    }

}  // namespace
