// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#include <cstddef>
#include <cstdint>

namespace ungula {

    /// Compute CRC32 for a single byte (standard polynomial 0xEDB88320).
    inline uint32_t crc32_byte(uint32_t crc, uint8_t byte) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
        return crc;
    }

    /// Compute CRC32 over a data buffer.
    inline uint32_t crc32(const uint8_t* data, size_t len) {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc = crc32_byte(crc, data[i]);
        }
        return crc ^ 0xFFFFFFFFu;
    }

}  // namespace ungula
