// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <stdint.h>

namespace ungula::core::system {

    class SystemControl final {
        public:
            SystemControl() = delete;
            ~SystemControl() = delete;

            static void reboot();
            static void rebootAfterMs(uint32_t waitTimeMs);
    };

}  // namespace ungula::core::system
