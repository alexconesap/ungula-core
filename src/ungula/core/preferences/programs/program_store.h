// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// ============================================================================
// Generic program/recipe store with NVS persistence and CRC integrity.
//
// The ProgramT template parameter is the project-specific struct that holds
// the recipe data. It must be a POD type with:
//   - char name[N]   (program display name)
//   - bool valid      (true if the slot is in use)
//
// Usage:
//   struct MyProgram { char name[64]; int speed; bool valid; ... };
//   ungula::ProgramStore<MyProgram, 10> store(prefs);
//   store.init("DEFAULT");
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <ungula/core/preferences/core/i_preferences.h>

namespace ungula::core::preferences::programs
{

    template <typename ProgramT, int MaxPrograms> class ProgramStore {
    public:
        explicit ProgramStore(IPreferences &prefs)
                : prefs_(prefs)
        {
            for (auto &prg : programs_) {
                memset(&prg, 0, sizeof(prg));
                prg.valid = false;
            }
        }

        /// Load all programs from NVS. Creates a default if none exist.
        /// defaultName: name for the auto-created default program.
        /// createDefault: callback that returns a default ProgramT with the given name.
        void init(const char *defaultName, ProgramT (*createDefault)(const char *))
        {
            loadAllFromNvs();
            loadMetaFromNvs();

            if (countValid() == 0 && createDefault) {
                programs_[0] = createDefault(defaultName);
                saveProgramToNvs(0);
                activeIndex_ = 0;
                lastUsedIndex_ = 0;
                saveMetaToNvs();
            }

            // Validate active index
            if (activeIndex_ < 0 || activeIndex_ >= MaxPrograms || !programs_[activeIndex_].valid) {
                for (int idx = 0; idx < MaxPrograms; idx++) {
                    if (programs_[idx].valid) {
                        activeIndex_ = idx;
                        saveMetaToNvs();
                        break;
                    }
                }
            }
        }

        const ProgramT &getProgram(int index) const
        {
            if (index < 0 || index >= MaxPrograms) {
                return programs_[0];
            }
            return programs_[index];
        }

        bool saveProgram(int index, const ProgramT &program)
        {
            if (index < 0 || index >= MaxPrograms) {
                return false;
            }
            programs_[index] = program;
            programs_[index].valid = true;
            saveProgramToNvs(index);
            return true;
        }

        bool deleteProgram(int index)
        {
            if (index < 0 || index >= MaxPrograms) {
                return false;
            }
            if (!programs_[index].valid) {
                return false;
            }
            if (countValid() <= 1) {
                return false;
            }

            programs_[index].valid = false;
            memset(programs_[index].name, 0, sizeof(programs_[index].name));
            saveProgramToNvs(index);

            if (activeIndex_ == index) {
                for (int idx = 0; idx < MaxPrograms; idx++) {
                    if (programs_[idx].valid) {
                        activeIndex_ = idx;
                        break;
                    }
                }
                saveMetaToNvs();
            }
            return true;
        }

        int getActiveIndex() const
        {
            return activeIndex_;
        }

        void setActiveIndex(int index)
        {
            if (index < 0 || index >= MaxPrograms) {
                return;
            }
            if (!programs_[index].valid) {
                return;
            }
            activeIndex_ = index;
            lastUsedIndex_ = index;
            saveMetaToNvs();
        }

        int getLastUsedIndex() const
        {
            return lastUsedIndex_;
        }

        void setLastUsedIndex(int index)
        {
            if (index < 0 || index >= MaxPrograms) {
                return;
            }
            lastUsedIndex_ = index;
            saveMetaToNvs();
        }

        int countValid() const
        {
            int count = 0;
            for (const auto &prg : programs_) {
                if (prg.valid) {
                    count++;
                }
            }
            return count;
        }

        static constexpr int maxPrograms()
        {
            return MaxPrograms;
        }

    private:
        IPreferences &prefs_;
        ProgramT programs_[MaxPrograms];
        int activeIndex_ = 0;
        int lastUsedIndex_ = -1;

        static constexpr const char *NVS_NS = "programs";

        static void programKey(int index, char *buf, size_t bufSize)
        {
            snprintf(buf, bufSize, "p%d", index);
        }

        struct ProgramBlob {
            ProgramT program;
            uint32_t crc;
        };

        static uint32_t computeCrc(const ProgramT &prog)
        {
            // Simple CRC32 over the program bytes
            const auto *data = reinterpret_cast<const uint8_t *>(&prog);
            uint32_t crc_val = 0xFFFFFFFF;
            for (size_t idx = 0; idx < sizeof(ProgramT); idx++) {
                crc_val ^= data[idx];
                for (int bit = 0; bit < 8; bit++) {
                    crc_val = (crc_val >> 1) ^ (0xEDB88320 & -(crc_val & 1));
                }
            }
            return ~crc_val;
        }

        void loadAllFromNvs()
        {
            if (!prefs_.begin(NVS_NS)) {
                return;
            }

            for (int idx = 0; idx < MaxPrograms; idx++) {
                char key[4];
                programKey(idx, key, sizeof(key));

                if (!prefs_.hasKey(key)) {
                    programs_[idx].valid = false;
                    continue;
                }

                ProgramBlob blob;
                size_t read = prefs_.getBytes(key, reinterpret_cast<uint8_t *>(&blob), sizeof(blob));
                if (read != sizeof(blob)) {
                    programs_[idx].valid = false;
                    continue;
                }

                if (blob.crc != computeCrc(blob.program)) {
                    programs_[idx].valid = false;
                    continue;
                }

                programs_[idx] = blob.program;
            }
            prefs_.end();
        }

        void saveProgramToNvs(int index)
        {
            if (index < 0 || index >= MaxPrograms) {
                return;
            }

            ProgramBlob blob;
            blob.program = programs_[index];
            blob.crc = computeCrc(blob.program);

            char key[4];
            programKey(index, key, sizeof(key));

            if (!prefs_.begin(NVS_NS)) {
                return;
            }
            prefs_.putBytes(key, reinterpret_cast<const uint8_t *>(&blob), sizeof(blob));
            prefs_.end();
        }

        void loadMetaFromNvs()
        {
            if (!prefs_.begin(NVS_NS)) {
                return;
            }
            activeIndex_ = static_cast<int>(prefs_.getUInt8("active", 0));
            lastUsedIndex_ = static_cast<int>(prefs_.getUInt8("last", 255));
            if (lastUsedIndex_ == 255) {
                lastUsedIndex_ = -1;
            }
            prefs_.end();
        }

        void saveMetaToNvs()
        {
            if (!prefs_.begin(NVS_NS)) {
                return;
            }
            prefs_.putUInt8("active", static_cast<uint8_t>(activeIndex_));
            prefs_.putUInt8("last", lastUsedIndex_ >= 0 ? static_cast<uint8_t>(lastUsedIndex_) : 255);
            prefs_.end();
        }
    };

} // namespace ungula::core::preferences::programs
