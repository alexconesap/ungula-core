// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// -------------------------------------------------------------------
// Persist a POD config struct as ONE CRC-protected NVS blob, with a defaults
// fallback. Layout: { ConfigT config; uint32_t crc; } under a single key; the
// CRC32 spans the config bytes only. On a missing/short read the defaults are
// returned; on a CRC mismatch the defaults are returned AND rewritten (so a
// corrupted blob self-heals).
//
// The project owns ConfigT, its defaults, and the namespace/key (every machine's
// device config differs); this owns only the begin/getBytes/CRC/putBytes
// mechanics. ConfigT must be trivially copyable — it is stored as raw bytes.
// Logging is the caller's job (the lib never prints): load() reports where the
// returned config came from via ConfigLoadStatus.
// -------------------------------------------------------------------

#include <cstddef>
#include <cstdint>

#include <ungula/core/preferences/i_preferences.h>
#include <ungula/core/util/crc32.h>

namespace ungula::core::preferences
{

/// Where the config returned by load() came from.
enum class ConfigLoadStatus : uint8_t {
        Loaded, // a valid CRC-checked blob was read from NVS
        Defaulted, // nothing stored (or wrong size) — defaults returned as-is
        Recovered, // stored blob failed CRC — defaults returned AND rewritten
};

template <typename ConfigT>
class NvsConfigStore
{
    public:
        NvsConfigStore(IPreferences &prefs, const char *ns, const char *key)
                : prefs_(prefs)
                , ns_(ns)
                , key_(key)
        {
        }

        /// Load the config. Returns `defaults` on a miss/short-read (Defaulted) or
        /// a CRC mismatch (Recovered, defaults also rewritten); otherwise the
        /// stored blob (Loaded). Pass `status` to learn which happened.
        ConfigT load(const ConfigT &defaults, ConfigLoadStatus *status = nullptr) const
        {
                if (!prefs_.begin(ns_)) {
                        setStatus(status, ConfigLoadStatus::Defaulted);
                        return defaults;
                }
                Blob blob;
                const size_t read = prefs_.getBytes(key_, reinterpret_cast<uint8_t *>(&blob), sizeof(blob));
                prefs_.end();

                if (read != sizeof(blob)) {
                        setStatus(status, ConfigLoadStatus::Defaulted);
                        return defaults;
                }
                if (blob.crc != crcOf(blob.config)) {
                        save(defaults);
                        setStatus(status, ConfigLoadStatus::Recovered);
                        return defaults;
                }
                setStatus(status, ConfigLoadStatus::Loaded);
                return blob.config;
        }

        /// Write the config (config + CRC) under the key. Returns false if the
        /// namespace could not be opened or the write failed.
        bool save(const ConfigT &config) const
        {
                Blob blob;
                blob.config = config;
                blob.crc = crcOf(config);
                if (!prefs_.begin(ns_)) {
                        return false;
                }
                const bool ok = prefs_.putBytes(key_, reinterpret_cast<const uint8_t *>(&blob), sizeof(blob));
                prefs_.end();
                return ok;
        }

    private:
        struct Blob {
                ConfigT config;
                uint32_t crc;
        };

        static uint32_t crcOf(const ConfigT &c)
        {
                return ungula::core::util::crc32(reinterpret_cast<const uint8_t *>(&c), sizeof(ConfigT));
        }
        static void setStatus(ConfigLoadStatus *status, ConfigLoadStatus value)
        {
                if (status != nullptr) {
                        *status = value;
                }
        }

        IPreferences &prefs_;
        const char *ns_;
        const char *key_;
};

} // namespace ungula::core::preferences
