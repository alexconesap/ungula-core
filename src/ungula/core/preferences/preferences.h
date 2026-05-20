// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include "ungula/core/preferences/i_preferences.h"

// Platform-selected alias. Host code should always spell the type as
// `ungula::core::preferences::Preferences` - the concrete implementation
// is picked here at compile time. Adding a new platform = new header +
// new branch below; consumer code stays untouched.

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
#include "ungula/core/preferences/platforms/esp32_preferences.h"
namespace ungula::core::preferences
{
using Preferences = Esp32Preferences;
}
#elif defined(STM32) || defined(ARDUINO_ARCH_STM32)
#error "ungula::core::preferences: STM32 backend not implemented yet"
#else
#error "ungula::core::preferences: no implementation selected for this platform"
#endif
