// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <type_traits>

#include <ungula/core/preferences/preferences.h>
#include <ungula/core/preferences/platforms/esp32_preferences.h>

static_assert(std::is_same_v<ungula::core::preferences::Preferences,
                             ungula::core::preferences::Esp32Preferences>,
              "Preferences alias must resolve to Esp32Preferences on ESP32 builds");
