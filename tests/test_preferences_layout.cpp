// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <type_traits>

#include <ungula/core/preferences/i_preferences.h>
#include <ungula/core/preferences/preferences.h>
#include <ungula/core/preferences/tools/programs/program_store.h>

namespace
{

using ungula::core::preferences::IPreferences;
using ungula::core::preferences::initStorage;
using ungula::core::preferences::programs::ProgramStore;

struct DemoProgram {
        char name[32];
        uint16_t speed;
        bool valid;
};

class MemoryPreferences final : public IPreferences {
    public:
        bool begin(const char *ns) override
        {
                (void)ns;
                opened_ = true;
                return true;
        }

        void end() override
        {
                opened_ = false;
        }

        bool putString(const char *key, const char *value) override
        {
                (void)key;
                (void)value;
                return opened_;
        }

        size_t getString(const char *key, char *buf, size_t bufSize) const override
        {
                (void)key;
                if (buf == nullptr || bufSize == 0) {
                        return 0;
                }
                buf[0] = '\0';
                return 0;
        }

        bool putBytes(const char *key, const uint8_t *data, size_t len) override
        {
                if (!opened_ || key == nullptr || data == nullptr || len == 0) {
                        return false;
                }
                if (std::strcmp(key, "active") == 0 || std::strcmp(key, "last") == 0) {
                        return false;
                }
                Value slot{};
                slot.size = (len <= sizeof(slot.bytes)) ? len : sizeof(slot.bytes);
                std::memcpy(slot.bytes, data, slot.size);
                values_[indexForKey(key)] = slot;
                present_[indexForKey(key)] = true;
                return true;
        }

        size_t getBytes(const char *key, uint8_t *buf, size_t bufSize) const override
        {
                if (!opened_ || key == nullptr || buf == nullptr || bufSize == 0) {
                        return 0;
                }
                const int idx = indexForKey(key);
                if (!present_[idx]) {
                        return 0;
                }
                const size_t n = (values_[idx].size <= bufSize) ? values_[idx].size : bufSize;
                std::memcpy(buf, values_[idx].bytes, n);
                return n;
        }

        bool putUInt8(const char *key, uint8_t value) override
        {
                if (!opened_ || key == nullptr) {
                        return false;
                }
                if (std::strcmp(key, "active") == 0) {
                        active_ = value;
                        hasActive_ = true;
                        return true;
                }
                if (std::strcmp(key, "last") == 0) {
                        last_ = value;
                        hasLast_ = true;
                        return true;
                }
                return false;
        }

        uint8_t getUInt8(const char *key, uint8_t defaultVal = 0) const override
        {
                if (!opened_ || key == nullptr) {
                        return defaultVal;
                }
                if (std::strcmp(key, "active") == 0) {
                        return hasActive_ ? active_ : defaultVal;
                }
                if (std::strcmp(key, "last") == 0) {
                        return hasLast_ ? last_ : defaultVal;
                }
                return defaultVal;
        }

        bool putUInt32(const char *key, uint32_t value) override
        {
                (void)key;
                (void)value;
                return false;
        }

        uint32_t getUInt32(const char *key, uint32_t defaultVal = 0) const override
        {
                (void)key;
                return defaultVal;
        }

        bool remove(const char *key) override
        {
                if (!opened_ || key == nullptr) {
                        return false;
                }
                present_[indexForKey(key)] = false;
                return true;
        }

        bool clear() override
        {
                if (!opened_) {
                        return false;
                }
                for (bool &slot : present_) {
                        slot = false;
                }
                hasActive_ = false;
                hasLast_ = false;
                return true;
        }

        bool hasKey(const char *key) const override
        {
                if (!opened_ || key == nullptr) {
                        return false;
                }
                if (std::strcmp(key, "active") == 0) {
                        return hasActive_;
                }
                if (std::strcmp(key, "last") == 0) {
                        return hasLast_;
                }
                return present_[indexForKey(key)];
        }

    private:
        struct Value {
                uint8_t bytes[64]{};
                size_t size = 0;
        };

        static int indexForKey(const char *key)
        {
                if (key == nullptr || key[0] != 'p') {
                        return 0;
                }
                const int d = key[1] - '0';
                if (d < 0 || d > 9) {
                        return 0;
                }
                return d;
        }

        bool opened_ = false;
        Value values_[10]{};
        bool present_[10]{};
        bool hasActive_ = false;
        bool hasLast_ = false;
        uint8_t active_ = 0;
        uint8_t last_ = 255;
};

DemoProgram makeDefault(const char *name)
{
        DemoProgram prg{};
        std::snprintf(prg.name, sizeof(prg.name), "%s", name);
        prg.speed = 100;
        prg.valid = true;
        return prg;
}

TEST(PreferencesLayout, ProgramStoreCompilesAtNewToolsPath)
{
        MemoryPreferences prefs;
        ProgramStore<DemoProgram, 3> store(prefs);
        store.init("DEFAULT", &makeDefault);

        EXPECT_EQ(store.countValid(), 1);
        EXPECT_EQ(store.getActiveIndex(), 0);

        DemoProgram p = store.getProgram(0);
        p.speed = 250;
        EXPECT_TRUE(store.saveProgram(0, p));
        EXPECT_EQ(store.getProgram(0).speed, 250);
}

TEST(PreferencesLayout, InterfaceRemainsAbstract)
{
        static_assert(std::is_abstract<IPreferences>::value,
                      "IPreferences must remain an abstract interface");
        SUCCEED();
}

TEST(PreferencesLayout, InitStorageSucceedsOnHost)
{
        // Host backend (platforms/host_preferences.cpp) is a no-op returning
        // true. Calling twice must remain idempotent so composition-root
        // code does not have to guard the call.
        EXPECT_TRUE(initStorage());
        EXPECT_TRUE(initStorage());
}

} // namespace
