// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <type_traits>

#include <ungula/core/preferences/i_preferences.h>
#include <ungula/core/preferences/nvs_config_store.h>

namespace
{

using ungula::core::preferences::ConfigLoadStatus;
using ungula::core::preferences::IPreferences;
using ungula::core::preferences::NvsConfigStore;

struct TestConfig {
        float kp = 1.0f;
        float ki = 0.1f;
        float kd = 0.05f;
        uint32_t flags = 0;

        bool operator==(const TestConfig& other) const
        {
                return kp == other.kp && ki == other.ki && kd == other.kd
                       && flags == other.flags;
        }
};

class SimpleMemoryPreferences final : public IPreferences {
    public:
        explicit SimpleMemoryPreferences(const char* failingNs = nullptr)
                : failingNs_(failingNs ? failingNs : "")
        {
        }

        bool begin(const char* ns) override
        {
                if (failingNs_ == ns) {
                        return false;
                }
                opened_ = true;
                return true;
        }

        void end() override
        {
                opened_ = false;
        }

        bool putString(const char* key, const char* value) override
        {
                (void)key;
                (void)value;
                return opened_;
        }

        size_t getString(const char* key, char* buf, size_t bufSize) const override
        {
                (void)key;
                if (buf == nullptr || bufSize == 0) {
                        return 0;
                }
                buf[0] = '\0';
                return 0;
        }

        bool putBytes(const char* key, const uint8_t* data, size_t len) override
        {
                if (!opened_ || key == nullptr || data == nullptr || len == 0) {
                        return false;
                }
                auto& entry = storage_[key];
                entry.assign(data, data + len);
                return true;
        }

        size_t getBytes(const char* key, uint8_t* buf, size_t bufSize) const override
        {
                if (!opened_ || key == nullptr || buf == nullptr || bufSize == 0) {
                        return 0;
                }
                auto it = storage_.find(key);
                if (it == storage_.end()) {
                        return 0;
                }
                size_t n = (it->second.size() <= bufSize) ? it->second.size()
                                                           : bufSize;
                std::memcpy(buf, it->second.data(), n);
                return n;
        }

        bool putUInt8(const char* key, uint8_t value) override
        {
                (void)key;
                (void)value;
                return opened_;
        }

        uint8_t getUInt8(const char* key, uint8_t defaultVal = 0) const override
        {
                (void)key;
                return defaultVal;
        }

        bool putUInt32(const char* key, uint32_t value) override
        {
                (void)key;
                (void)value;
                return opened_;
        }

        uint32_t getUInt32(const char* key, uint32_t defaultVal = 0) const override
        {
                (void)key;
                return defaultVal;
        }

        bool remove(const char* key) override
        {
                if (!opened_ || key == nullptr) {
                        return false;
                }
                storage_.erase(key);
                return true;
        }

        bool clear() override
        {
                if (!opened_) {
                        return false;
                }
                storage_.clear();
                return true;
        }

        bool hasKey(const char* key) const override
        {
                if (!opened_ || key == nullptr) {
                        return false;
                }
                return storage_.find(key) != storage_.end();
        }

    private:
        bool opened_ = false;
        std::string failingNs_;
        std::map<std::string, std::vector<uint8_t>> storage_;
};

TEST(NvsConfigStore, ConfigIsTriviallyCopyable)
{
        static_assert(std::is_trivially_copyable<TestConfig>::value,
                      "NvsConfigStore requires trivially copyable ConfigT");
        SUCCEED();
}

TEST(NvsConfigStore, LoadReturnsDefaultsWhenNothingStored)
{
        SimpleMemoryPreferences prefs;
        NvsConfigStore<TestConfig> store(prefs, "ns", "key");

        TestConfig defaults{ 1.0f, 0.2f, 0.1f, 42 };
        ConfigLoadStatus status = ConfigLoadStatus::Loaded;

        TestConfig result = store.load(defaults, &status);

        EXPECT_EQ(status, ConfigLoadStatus::Defaulted);
        EXPECT_EQ(result.kp, 1.0f);
        EXPECT_EQ(result.ki, 0.2f);
        EXPECT_EQ(result.kd, 0.1f);
        EXPECT_EQ(result.flags, 42u);
}

TEST(NvsConfigStore, SaveAndLoadRoundtrip)
{
        SimpleMemoryPreferences prefs;
        NvsConfigStore<TestConfig> store(prefs, "ns", "key");

        TestConfig toSave{ 2.5f, 0.5f, 0.25f, 123 };
        EXPECT_TRUE(store.save(toSave));

        ConfigLoadStatus status = ConfigLoadStatus::Defaulted;
        TestConfig loaded = store.load({ .kp = 0.f }, &status);

        EXPECT_EQ(status, ConfigLoadStatus::Loaded);
        EXPECT_EQ(loaded.kp, 2.5f);
        EXPECT_EQ(loaded.ki, 0.5f);
        EXPECT_EQ(loaded.kd, 0.25f);
        EXPECT_EQ(loaded.flags, 123u);
}

TEST(NvsConfigStore, LoadReturnsDefaultsWhenSizeMismatch)
{
        SimpleMemoryPreferences prefs;
        NvsConfigStore<TestConfig> store(prefs, "ns", "key");

        prefs.begin("ns");
        uint8_t shortData[3] = { 1, 2, 3 };
        prefs.putBytes("key", shortData, 3);
        prefs.end();

        ConfigLoadStatus status = ConfigLoadStatus::Loaded;
        TestConfig defaults{ 9.0f, 8.0f, 7.0f, 6 };
        TestConfig result = store.load(defaults, &status);

        EXPECT_EQ(status, ConfigLoadStatus::Defaulted);
        EXPECT_EQ(result.kp, 9.0f);
}

TEST(NvsConfigStore, CorruptedDataTriggersRecoveryAndRewrite)
{
        SimpleMemoryPreferences prefs;
        NvsConfigStore<TestConfig> store(prefs, "ns", "key");

        TestConfig defaults{ 1.0f, 0.2f, 0.1f, 0 };
        store.save(defaults);

        {
                prefs.begin("ns");
                uint8_t blob[sizeof(TestConfig) + sizeof(uint32_t)];
                std::memset(blob, 0xAA, sizeof(blob));
                prefs.putBytes("key", blob, sizeof(blob));
                prefs.end();
        }

        ConfigLoadStatus status = ConfigLoadStatus::Loaded;
        TestConfig result = store.load(defaults, &status);

        EXPECT_EQ(status, ConfigLoadStatus::Recovered);
        EXPECT_EQ(result.kp, 1.0f);

        {
                ConfigLoadStatus checkStatus = ConfigLoadStatus::Defaulted;
                TestConfig rewritten = store.load(defaults, &checkStatus);
                EXPECT_EQ(checkStatus, ConfigLoadStatus::Loaded);
                EXPECT_EQ(rewritten.kp, 1.0f);
        }
}

TEST(NvsConfigStore, SaveReturnsFalseWhenBeginFails)
{
        SimpleMemoryPreferences prefs("bad_ns");
        NvsConfigStore<TestConfig> store(prefs, "bad_ns", "key");

        TestConfig cfg{ 1.0f, 0.1f, 0.05f, 0 };

        EXPECT_FALSE(store.save(cfg));
}

TEST(NvsConfigStore, LoadReturnsDefaultsWhenBeginFails)
{
        SimpleMemoryPreferences prefs("bad_ns");
        NvsConfigStore<TestConfig> store(prefs, "bad_ns", "key");

        TestConfig defaults{ 3.14f, 2.71f, 1.41f, 99 };
        ConfigLoadStatus status = ConfigLoadStatus::Loaded;

        TestConfig result = store.load(defaults, &status);

        EXPECT_EQ(status, ConfigLoadStatus::Defaulted);
        EXPECT_EQ(result.kp, 3.14f);
}

TEST(NvsConfigStore, StatusPointerCanBeNull)
{
        SimpleMemoryPreferences prefs;
        NvsConfigStore<TestConfig> store(prefs, "ns", "key");

        TestConfig defaults{ 1.0f, 0.1f, 0.05f, 0 };
        TestConfig result = store.load(defaults, nullptr);

        EXPECT_EQ(result.kp, 1.0f);
}

TEST(NvsConfigStore, MultipleSaveOverwritesPrevious)
{
        SimpleMemoryPreferences prefs;
        NvsConfigStore<TestConfig> store(prefs, "ns", "key");

        TestConfig first{ 1.0f, 0.1f, 0.05f, 1 };
        TestConfig second{ 2.0f, 0.2f, 0.1f, 2 };

        EXPECT_TRUE(store.save(first));
        EXPECT_TRUE(store.save(second));

        ConfigLoadStatus status = ConfigLoadStatus::Defaulted;
        TestConfig loaded = store.load({ 0.f, 0.f, 0.f, 0 }, &status);

        EXPECT_EQ(status, ConfigLoadStatus::Loaded);
        EXPECT_EQ(loaded.kp, 2.0f);
        EXPECT_EQ(loaded.flags, 2u);
}

} // namespace