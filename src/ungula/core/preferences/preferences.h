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
// No MCU platform detected — typically a host gtest build. The `Preferences`
// alias is intentionally not defined here: any attempt to instantiate it
// host-side will produce a clear "undefined identifier" diagnostic at the
// call site, which is what we want. The platform-agnostic `initStorage()`
// declaration below still applies (host implementation lives in
// platforms/host_preferences.cpp).
#endif

namespace ungula::core::preferences
{

/// Initialise the underlying non-volatile storage.
///
/// MUST be called once at boot, BEFORE any subsystem that uses NVS:
/// `Preferences`, `WiFi`, `ESP-NOW`, `BLE`, etc. On a fresh chip or after
/// an SDK version bump the on-flash format may be stale; the platform
/// implementation transparently erases and retries once before giving up.
///
/// Backend-agnostic free function — each platform supplies its own
/// implementation in the corresponding `platforms/*.cpp`. Host code calls
/// `ungula::core::preferences::initStorage()` and never touches the
/// platform-specific NVS API directly.
///
/// Arduino-ESP32 ran this implicitly at startup so Arduino sketches never
/// saw the requirement; pure ESP-IDF / STM32 / any-platform projects do.
///
/// Returns true on success, false if storage could not be brought up even
/// after the erase-and-retry attempt.
bool initStorage();

} // namespace ungula::core::preferences
