// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#include <cstddef>
#include <cstdint>

namespace ungula::core::preferences {

    /// Abstract interface for persistent key-value storage
    class IPreferences {
        public:
            virtual ~IPreferences() = default;

            /// Open a namespace for reading/writing
            /// @param ns  Namespace name (max 15 chars on ESP32)
            virtual bool begin(const char* ns) = 0;

            /// Close the namespace
            virtual void end() = 0;

            /// Store a string value
            virtual bool putString(const char* key, const char* value) = 0;

            /// Retrieve a string value
            /// @param buf      Output buffer
            /// @param bufSize  Buffer size
            /// @return Bytes read (0 if key not found)
            virtual size_t getString(const char* key, char* buf, size_t bufSize) const = 0;

            /// Store raw bytes
            virtual bool putBytes(const char* key, const uint8_t* data, size_t len) = 0;

            /// Retrieve raw bytes
            /// @return Bytes read (0 if key not found)
            virtual size_t getBytes(const char* key, uint8_t* buf, size_t bufSize) const = 0;

            /// Store a uint8_t value
            virtual bool putUInt8(const char* key, uint8_t value) = 0;

            /// Retrieve a uint8_t value
            virtual uint8_t getUInt8(const char* key, uint8_t defaultVal = 0) const = 0;

            /// Store a uint32_t value
            virtual bool putUInt32(const char* key, uint32_t value) = 0;

            /// Retrieve a uint32_t value
            virtual uint32_t getUInt32(const char* key, uint32_t defaultVal = 0) const = 0;

            /// Remove a key
            virtual bool remove(const char* key) = 0;

            /// Clear all keys in the current namespace
            virtual bool clear() = 0;

            /// Check if a key exists
            virtual bool hasKey(const char* key) const = 0;
    };

}  // namespace ungula::core::preferences
