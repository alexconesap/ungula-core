// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#ifndef __cplusplus
#error UngulaCore requires a C++ compiler
#endif

// Ungula Generic Library - reusable utilities for any embedded project
// Include this header to activate the library in Arduino

// Utilities
#include "ungula/core/util/queue.h"
#include "ungula/core/util/string_types.h"
#include "ungula/core/util/string_utils.h"
#include "ungula/core/util/types.h"

// Preferences
#include "ungula/core/preferences/core/i_preferences.h"

// Time / Delay control
#include "ungula/core/time/time.h"

// System Control (Reboot, runtime health)
#include "ungula/core/system/health_monitor.h"
#include "ungula/core/system/system_reboot.h"
