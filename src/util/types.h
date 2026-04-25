// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
// Platform-agnostic types, string handling, and utilities shared across
// Ungula projects.

#include <cmath>
#include <cstdint>

#include "string_types.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

namespace ungula {

// Time functions
inline uint32_t platformMillis() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
            duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
inline uint32_t platformMicros() {
    using namespace std::chrono;
    return static_cast<uint32_t>(
            duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

// String helpers - Standard C++ implementation
inline int stringToInt(const string_t& s) {
    try {
        return std::stoi(s);
    } catch (...) {
        return 0;
    }
}
inline long stringToLong(const string_t& s) {
    try {
        return std::stol(s);
    } catch (...) {
        return 0;
    }
}
inline float stringToFloat(const string_t& s) {
    try {
        return std::stof(s);
    } catch (...) {
        return 0.0f;
    }
}
inline double stringToDouble(const string_t& s) {
    try {
        return std::stod(s);
    } catch (...) {
        return 0.0;
    }
}
inline int stringLength(const string_t& s) {
    return static_cast<int>(s.length());
}
inline int stringIndexOf(const string_t& s, char c, int start = 0) {
    size_t pos = s.find(c, start);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
}
inline int stringIndexOf(const string_t& s, const char* str, int start = 0) {
    size_t pos = s.find(str, start);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
}
inline string_t stringSubstring(const string_t& s, int start, int end) {
    return s.substr(start, end - start);
}
inline string_t stringSubstring(const string_t& s, int start) {
    return s.substr(start);
}
inline string_t stringToLower(const string_t& s) {
    string_t result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}
inline string_t stringToUpper(const string_t& s) {
    string_t result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}
inline string_t stringTrim(const string_t& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}
inline bool stringStartsWith(const string_t& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}
inline bool stringEndsWith(const string_t& s, const char* suffix) {
    size_t len = std::strlen(suffix);
    if (s.size() < len)
        return false;
    return s.compare(s.size() - len, len, suffix) == 0;
}
inline string_t intToString(int val) {
    return std::to_string(val);
}
inline string_t longToString(long val) {
    return std::to_string(val);
}
inline string_t floatToString(float val, int decimals = 2) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, static_cast<double>(val));
    return string_t(buf);
}
inline string_t doubleToString(double val, int decimals = 2) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, val);
    return string_t(buf);
}
inline const char* stringCStr(const string_t& s) {
    return s.c_str();
}

using PlatformString = std::string;

// ==================== TEMPERATURE CONVERSION ====================
// Shared utilities for temperature unit conversion

namespace temperature {

    constexpr double FAHRENHEIT_FREEZING_POINT = 32.0;
    constexpr double FAHRENHEIT_TO_CELSIUS_SCALE = 5.0 / 9.0;
    constexpr double CELSIUS_TO_FAHRENHEIT_SCALE = 9.0 / 5.0;

    constexpr double celsiusToFahrenheit(double c) {
        return c * CELSIUS_TO_FAHRENHEIT_SCALE + FAHRENHEIT_FREEZING_POINT;
    }

    constexpr double fahrenheitToCelsius(double f) {
        return (f - FAHRENHEIT_FREEZING_POINT) * FAHRENHEIT_TO_CELSIUS_SCALE;
    }

    constexpr float celsiusToFahrenheitF(float c) {
        return c * static_cast<float>(CELSIUS_TO_FAHRENHEIT_SCALE) +
               static_cast<float>(FAHRENHEIT_FREEZING_POINT);
    }

    constexpr float fahrenheitToCelsiusF(float f) {
        return (f - static_cast<float>(FAHRENHEIT_FREEZING_POINT)) *
               static_cast<float>(FAHRENHEIT_TO_CELSIUS_SCALE);
    }

    inline bool isValidTemperature(double tempC) {
        return std::isfinite(tempC) && tempC > -200.0 && tempC < 1800.0;
    }

    inline bool isValidTemperatureF(float tempC) {
        return std::isfinite(tempC) && tempC > -200.0f && tempC < 1800.0f;
    }

}  // namespace temperature

// ==================== MATH UTILITIES ====================
// Common clamping and interpolation functions

namespace math {

    inline double clamp(double value, double minVal, double maxVal) {
        return value < minVal ? minVal : (value > maxVal ? maxVal : value);
    }

    inline float clampf(float value, float minVal, float maxVal) {
        return value < minVal ? minVal : (value > maxVal ? maxVal : value);
    }

    inline int clampi(int value, int minVal, int maxVal) {
        return value < minVal ? minVal : (value > maxVal ? maxVal : value);
    }

    inline double clamp01(double value) {
        return clamp(value, 0.0, 1.0);
    }

    inline float clamp01f(float value) {
        return clampf(value, 0.0f, 1.0f);
    }

    inline double lerp(double a, double b, double t) {
        return a + (b - a) * clamp01(t);
    }

    inline float lerpf(float a, float b, float t) {
        return a + (b - a) * clamp01f(t);
    }

}  // namespace math

// ==================== ENUM CONVERSION UTILITIES ====================
// Generic functions for converting between enums and uint8_t.
// Works with any enum type from any project.

template <typename EnumT>
inline uint8_t toUint8(EnumT val) {
    return static_cast<uint8_t>(val);
}

template <typename EnumT>
inline EnumT fromUint8(uint8_t val) {
    return static_cast<EnumT>(val);
}

}  // namespace ungula
