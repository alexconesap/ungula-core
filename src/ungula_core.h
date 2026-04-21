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
#include "util/queue.h"
#include "util/string_types.h"
#include "util/string_utils.h"
#include "util/types.h"

// Preferences
#include "preferences/core/i_preferences.h"

// Time / Delay control
#include "time/time_control.h"

// System Control (Reboot, runtime health)
#include "system/health_monitor.h"
#include "system/system_reboot.h"
