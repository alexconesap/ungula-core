// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <ungula/core/preferences/i_preferences.h>
#include <ungula/core/preferences/tools/counters/counter_store.h>

namespace
{

using ungula::core::preferences::IPreferences;
using ungula::core::preferences::counters::CounterStore;
using ungula::core::preferences::counters::MAX_VALUE;

/// In-memory IPreferences with per-namespace uint32 storage. Tracks how many
/// times a value was written so the tests can assert the flash-write cost of
/// each operation (one write per increment, none for a read).
class FakePreferences final : public IPreferences {
    public:
        explicit FakePreferences(const char *failingNs = nullptr)
                : failingNs_(failingNs != nullptr ? failingNs : "")
        {
        }

        bool begin(const char *ns) override
        {
                if (ns == nullptr || failingNs_ == ns) {
                        return false;
                }
                ns_ = ns;
                opened_ = true;
                return true;
        }

        void end() override
        {
                opened_ = false;
        }

        bool putUInt32(const char *key, uint32_t value) override
        {
                if (!opened_ || key == nullptr) {
                        return false;
                }
                if (failWrites_) {
                        return false;
                }
                u32_[ns_ + "/" + key] = value;
                ++writeCount_;
                return true;
        }

        uint32_t getUInt32(const char *key, uint32_t defaultVal = 0) const override
        {
                if (!opened_ || key == nullptr) {
                        return defaultVal;
                }
                auto it = u32_.find(ns_ + "/" + key);
                return (it == u32_.end()) ? defaultVal : it->second;
        }

        bool hasKey(const char *key) const override
        {
                if (!opened_ || key == nullptr) {
                        return false;
                }
                return u32_.find(ns_ + "/" + key) != u32_.end();
        }

        bool remove(const char *key) override
        {
                if (!opened_ || key == nullptr) {
                        return false;
                }
                u32_.erase(ns_ + "/" + key);
                return true;
        }

        bool clear() override
        {
                if (!opened_) {
                        return false;
                }
                u32_.clear();
                return true;
        }

        // Unused by counters — minimal conforming stubs.
        bool putString(const char *, const char *) override
        {
                return opened_;
        }
        size_t getString(const char *, char *buf, size_t bufSize) const override
        {
                if (buf != nullptr && bufSize > 0) {
                        buf[0] = '\0';
                }
                return 0;
        }
        bool putBytes(const char *, const uint8_t *, size_t) override
        {
                return opened_;
        }
        size_t getBytes(const char *, uint8_t *, size_t) const override
        {
                return 0;
        }
        bool putUInt8(const char *, uint8_t) override
        {
                return opened_;
        }
        uint8_t getUInt8(const char *, uint8_t defaultVal = 0) const override
        {
                return defaultVal;
        }

        // --- test helpers ---
        void failWrites(bool fail)
        {
                failWrites_ = fail;
        }
        unsigned writeCount() const
        {
                return writeCount_;
        }
        bool isOpen() const
        {
                return opened_;
        }
        void seed(const char *ns, const char *key, uint32_t value)
        {
                u32_[std::string(ns) + "/" + key] = value;
        }

    private:
        bool opened_ = false;
        bool failWrites_ = false;
        unsigned writeCount_ = 0;
        std::string ns_;
        std::string failingNs_;
        std::map<std::string, uint32_t> u32_;
};

TEST(CounterStore, ReadReturnsZeroWhenCounterDoesNotExist)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        EXPECT_EQ(counters.read("runs"), 0u);
        EXPECT_FALSE(counters.exists("runs"));
}

TEST(CounterStore, WriteCreatesTheCounter)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        EXPECT_TRUE(counters.write("runs", 7));
        EXPECT_TRUE(counters.exists("runs"));
        EXPECT_EQ(counters.read("runs"), 7u);
}

TEST(CounterStore, WriteOverwritesPreviousValue)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        EXPECT_TRUE(counters.write("runs", 7));
        EXPECT_TRUE(counters.write("runs", 3));
        EXPECT_EQ(counters.read("runs"), 3u);
}

TEST(CounterStore, IncrementCreatesTheCounterAtOne)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        EXPECT_EQ(counters.increment("runs"), 1u);
        EXPECT_EQ(counters.read("runs"), 1u);
}

TEST(CounterStore, IncrementReturnsTheNewValue)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        counters.write("runs", 41);

        EXPECT_EQ(counters.increment("runs"), 42u);
        EXPECT_EQ(counters.read("runs"), 42u);
}

TEST(CounterStore, IncrementAcceptsAStep)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        EXPECT_EQ(counters.increment("meters", 10), 10u);
        EXPECT_EQ(counters.increment("meters", 5), 15u);
}

TEST(CounterStore, IncrementSaturatesInsteadOfWrapping)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        counters.write("runs", MAX_VALUE - 1);

        EXPECT_EQ(counters.increment("runs"), MAX_VALUE);
        EXPECT_EQ(counters.increment("runs"), MAX_VALUE);
        EXPECT_EQ(counters.increment("runs", 1000), MAX_VALUE);
}

TEST(CounterStore, ResetSetsZeroAndKeepsTheKey)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        counters.write("runs", 12);

        EXPECT_TRUE(counters.reset("runs"));
        EXPECT_EQ(counters.read("runs"), 0u);
        EXPECT_TRUE(counters.exists("runs"));
}

TEST(CounterStore, ResetCreatesAMissingCounter)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        EXPECT_TRUE(counters.reset("fresh"));
        EXPECT_TRUE(counters.exists("fresh"));
        EXPECT_EQ(counters.read("fresh"), 0u);
}

TEST(CounterStore, CountersAreIndependent)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        counters.increment("runs");
        counters.increment("runs");
        counters.increment("boots");

        EXPECT_EQ(counters.read("runs"), 2u);
        EXPECT_EQ(counters.read("boots"), 1u);
}

TEST(CounterStore, NamespacesAreIsolated)
{
        FakePreferences prefs;
        CounterStore mainCounters(prefs, "cnt_main");
        CounterStore nodeCounters(prefs, "cnt_node");

        mainCounters.write("runs", 5);
        nodeCounters.write("runs", 9);

        EXPECT_EQ(mainCounters.read("runs"), 5u);
        EXPECT_EQ(nodeCounters.read("runs"), 9u);
}

TEST(CounterStore, ReadDoesNotWrite)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        counters.write("runs", 3);
        const unsigned before = prefs.writeCount();

        counters.read("runs");
        counters.read("runs");
        counters.exists("runs");

        EXPECT_EQ(prefs.writeCount(), before);
}

TEST(CounterStore, IncrementCostsExactlyOneWrite)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        counters.increment("runs");

        EXPECT_EQ(prefs.writeCount(), 1u);
}

TEST(CounterStore, EveryCallClosesTheNamespace)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        counters.write("runs", 1);
        EXPECT_FALSE(prefs.isOpen());
        counters.read("runs");
        EXPECT_FALSE(prefs.isOpen());
        counters.increment("runs");
        EXPECT_FALSE(prefs.isOpen());
        counters.exists("runs");
        EXPECT_FALSE(prefs.isOpen());
}

TEST(CounterStore, InvalidNamesAreRejected)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        const char *tooLong = "0123456789abcdef"; // 16 chars, limit is 15

        EXPECT_FALSE(counters.write(nullptr, 1));
        EXPECT_FALSE(counters.write("", 1));
        EXPECT_FALSE(counters.write(tooLong, 1));
        EXPECT_EQ(counters.increment(tooLong), 0u);
        EXPECT_EQ(counters.read(tooLong), 0u);
        EXPECT_FALSE(counters.reset(tooLong));
        EXPECT_FALSE(counters.exists(tooLong));
        EXPECT_EQ(prefs.writeCount(), 0u);
}

TEST(CounterStore, NameAtTheLengthLimitIsAccepted)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        const char *atLimit = "0123456789abcde"; // exactly 15 chars

        EXPECT_TRUE(CounterStore::isValidName(atLimit));
        EXPECT_EQ(counters.increment(atLimit), 1u);
}

TEST(CounterStore, BackendFailureIsReported)
{
        FakePreferences prefs("locked");
        CounterStore counters(prefs, "locked");

        EXPECT_FALSE(counters.write("runs", 1));
        EXPECT_EQ(counters.read("runs"), 0u);
        EXPECT_EQ(counters.increment("runs"), 0u);
        EXPECT_FALSE(counters.reset("runs"));
        EXPECT_FALSE(counters.exists("runs"));
}

TEST(CounterStore, IncrementReturnsZeroWhenTheWriteFails)
{
        FakePreferences prefs;
        CounterStore counters(prefs);

        counters.write("runs", 4);
        prefs.failWrites(true);

        EXPECT_EQ(counters.increment("runs"), 0u);

        prefs.failWrites(false);
        EXPECT_EQ(counters.read("runs"), 4u); // unchanged on failure
}

TEST(CounterStore, ReadsAValuePersistedByAnotherInstance)
{
        FakePreferences prefs;
        prefs.seed("counters", "runs", 99);

        EXPECT_EQ(CounterStore(prefs).read("runs"), 99u);
}

} // namespace
