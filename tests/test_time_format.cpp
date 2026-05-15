// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <ungula/core/time/time_format.h>

#include <cstring>

namespace
{

namespace fmt = ungula::core::time;

// Anchor instant used across the suite. Pure arithmetic via gmtime_r,
// so the expected strings are stable across hosts and ESP32 builds.
constexpr time_t kAnchorEpoch = 1700000000; // 2023-11-14 22:13:20 UTC
constexpr const char *kAnchorUtcStr = "2023-11-14 22:13:20";

// ---- Pure formatter ----

TEST(TimeFormatTest, FormatIso8601Utc)
{
        char buf[32] = {};
        const size_t n = fmt::formatIso8601(buf, sizeof(buf), kAnchorEpoch);
        EXPECT_EQ(n, std::strlen(kAnchorUtcStr));
        EXPECT_STREQ(buf, kAnchorUtcStr);
}

TEST(TimeFormatTest, OffsetEastShiftsForward)
{
        // JST = UTC + 9h. Anchor 22:13:20 UTC → 07:13:20 next day in Tokyo.
        char buf[32] = {};
        fmt::formatIso8601(buf, sizeof(buf), kAnchorEpoch, 9 * 3600);
        EXPECT_STREQ(buf, "2023-11-15 07:13:20");
}

TEST(TimeFormatTest, OffsetWestShiftsBackward)
{
        // PST = UTC - 8h. 22:13:20 UTC → 14:13:20 in Los Angeles.
        char buf[32] = {};
        fmt::formatIso8601(buf, sizeof(buf), kAnchorEpoch, -8 * 3600);
        EXPECT_STREQ(buf, "2023-11-14 14:13:20");

        // CET = UTC + 1h. → 23:13:20 in Barcelona (winter).
        fmt::formatIso8601(buf, sizeof(buf), kAnchorEpoch, 1 * 3600);
        EXPECT_STREQ(buf, "2023-11-14 23:13:20");
}

TEST(TimeFormatTest, CustomStrftimeSpec)
{
        char buf[32] = {};
        fmt::format(buf, sizeof(buf), "%H:%M", kAnchorEpoch, 0);
        EXPECT_STREQ(buf, "22:13");

        fmt::format(buf, sizeof(buf), "%Y/%m/%d", kAnchorEpoch, 0);
        EXPECT_STREQ(buf, "2023/11/14");
}

TEST(TimeFormatTest, ZeroEpochReturnsZero)
{
        // Epoch 0 means "no valid time yet" — formatting it would print
        // the Unix epoch start, which is misleading in a log.
        char buf[32];
        std::memset(buf, 0xAA, sizeof(buf));
        EXPECT_EQ(fmt::formatIso8601(buf, sizeof(buf), 0), 0U);
        EXPECT_EQ(buf[0], '\0'); // buffer was cleared.
}

TEST(TimeFormatTest, NegativeEpochReturnsZero)
{
        char buf[32] = {};
        EXPECT_EQ(fmt::formatIso8601(buf, sizeof(buf), -1), 0U);
}

TEST(TimeFormatTest, NullInputsReturnZero)
{
        char buf[32] = {};
        EXPECT_EQ(fmt::formatIso8601(nullptr, sizeof(buf), kAnchorEpoch), 0U);
        EXPECT_EQ(fmt::format(buf, sizeof(buf), nullptr, kAnchorEpoch), 0U);
        EXPECT_EQ(fmt::formatIso8601(buf, 0, kAnchorEpoch), 0U);
}

TEST(TimeFormatTest, BufferTooSmallReturnsZero)
{
        // ISO-8601 needs 19 chars + null. 10 bytes is short.
        char buf[10] = {};
        EXPECT_EQ(fmt::formatIso8601(buf, sizeof(buf), kAnchorEpoch), 0U);
}

TEST(TimeFormatTest, OffsetCarriesAcrossDateBoundary)
{
        // Anchor + 9h crosses midnight UTC → next day. Lock the carry.
        char buf[32] = {};
        fmt::format(buf, sizeof(buf), "%d", kAnchorEpoch, 9 * 3600);
        EXPECT_STREQ(buf, "15");
}

} // namespace
