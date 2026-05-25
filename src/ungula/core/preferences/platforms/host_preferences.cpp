// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.
//
// Host backend for `ungula::core::preferences`.
//
// Provides the platform-agnostic `initStorage()` free function so any
// non-MCU build (gtest, simulator, future host port) links cleanly. The
// implementation is a no-op returning true — there is nothing to bring
// up because host builds do not back any subsystem onto non-volatile
// flash. The function exists so portable composition-root code that
// calls `ungula::core::preferences::initStorage()` works identically on
// every platform.
//
// This file is compiled ONLY when no MCU platform macro is defined. The
// ESP32 build picks up the real implementation from esp32_preferences.cpp.

#if !defined(ESP_PLATFORM) && !defined(ARDUINO_ARCH_ESP32) && !defined(ESP32) && \
    !defined(STM32) && !defined(ARDUINO_ARCH_STM32)

#include "ungula/core/preferences/preferences.h"

namespace ungula::core::preferences
{

bool initStorage()
{
        // No persistent storage to initialise in a host build.
        return true;
}

} // namespace ungula::core::preferences

#endif
