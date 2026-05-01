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

// ==================== TEMPERATURE CONVERSION ====================
// Shared utilities for temperature unit conversion

namespace temp {

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
        return (c * static_cast<float>(CELSIUS_TO_FAHRENHEIT_SCALE)) +
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

}  // namespace temp

// ==================== MATH UTILITIES ====================
// Common clamping and interpolation functions

namespace math {

    /// @brief Clamp a double value to a specified range [minVal, maxVal]. Returns the clamped value without modifying the input.
    /// @param value The double value to clamp.
    /// @param minVal The minimum allowed value for the input. If `value` is less than `minVal`, the return value will be `minVal`.
    /// @param maxVal The maximum allowed value for the input. If `value` is greater than `maxVal`, the return value will be `maxVal`.
    /// @return The clamped double value, guaranteed to be between `minVal` and `maxVal` inclusive. If `value` is within the range, it is returned unchanged.
    inline double clamp(double value, double minVal, double maxVal) {
        return value < minVal ? minVal : (value > maxVal ? maxVal : value);
    }

    /// @brief Clamp a double value to a specified range [minVal, maxVal]. Modifies the input value in place if it is outside the range.
    /// @param value The double value to clamp. This parameter is passed by reference and will be modified to fit within the specified range.
    /// @param minVal The minimum allowed value for the input. If `value` is less than `minVal`, it will be set to `minVal`.
    /// @param maxVal The maximum allowed value for the input. If `value` is greater than `maxVal`, it will be set to `maxVal`.
    inline void clamp_v(double& value, double minVal, double maxVal) {
        if (value < minVal) {
            value = minVal;
        } else if (value > maxVal) {
            value = maxVal;
        }
    }

    /// @brief Clamp a float value to a specified range [minVal, maxVal]. Returns the clamped value without modifying the input.
    /// @param value The float value to clamp.
    /// @param minVal The minimum allowed value for the input. If `value` is less than `minVal`, the return value will be `minVal`.
    /// @param maxVal The maximum allowed value for the input. If `value` is greater than `maxVal`, the return value will be `maxVal`.
    /// @return The clamped float value, guaranteed to be between `minVal` and `maxVal` inclusive. If `value` is within the range, it is returned unchanged.
    inline float clampf(float value, float minVal, float maxVal) {
        return value < minVal ? minVal : (value > maxVal ? maxVal : value);
    }

    /// @brief Clamp a float value to a specified range [minVal, maxVal]. Modifies the input value in place if it is outside the range.
    /// @param value The float value to clamp. This parameter is passed by reference and will be modified to fit within the specified range.
    /// @param minVal The minimum allowed value for the input. If `value` is less than `minVal`, it will be set to `minVal`.
    /// @param maxVal The maximum allowed value for the input. If `value` is greater than `maxVal`, it will be set to `maxVal`.
    inline void clampf_v(float& value, float minVal, float maxVal) {
        if (value < minVal) {
            value = minVal;
        } else if (value > maxVal) {
            value = maxVal;
        }
    }

    /// @brief Clamp an integer value to a specified range [minVal, maxVal]. Returns the clamped value without modifying the input.
    /// @param value The integer value to clamp.
    /// @param minVal The minimum allowed value for the input. If `value` is less than `minVal`, the return value will be `minVal`.
    /// @param maxVal The maximum allowed value for the input. If `value` is greater than `maxVal`, the return value will be `maxVal`.
    /// @return The clamped integer value, guaranteed to be between `minVal` and `maxVal` inclusive. If `value` is within the range, it is returned unchanged.
    inline int clampi(int value, int minVal, int maxVal) {
        return value < minVal ? minVal : (value > maxVal ? maxVal : value);
    }

    /// @brief Clamp an integer value to a specified range [minVal, maxVal]. Modifies the input value in place in required.
    /// @param value The integer value to clamp. This parameter is passed by reference and will be modified to fit within the specified range.
    /// @param minVal The minimum allowed value for the input. If `value` is less than `minVal`, it will be set to `minVal`.
    /// @param maxVal The maximum allowed value for the input. If `value` is greater than `maxVal`, it will be set to `maxVal`.
    inline void clampi_v(int& value, int minVal, int maxVal) {
        if (value < minVal) {
            value = minVal;
        } else if (value > maxVal) {
            value = maxVal;
        }
    }

    /// @brief Clamp a double to the range [0, 1].
    /// @param value The double value to clamp.
    /// @return The clamped double value, guaranteed to be between 0.0 and 1.0 inclusive.
    inline double clamp01(double value) {
        return clamp(value, 0.0, 1.0);
    }

    /// @brief Clamp a float to the range [0, 1].
    /// @param value The float value to clamp.
    /// @return The clamped float value, guaranteed to be between 0.0f and 1.0f inclusive.
    inline float clamp01f(float value) {
        return clampf(value, 0.0f, 1.0f);
    }

    /// @brief Linear interpolation for doubles. Clamps t to [0, 1] to prevent extrapolation.
    /// @param a Starting value corresponding to t=0.
    /// @param b Ending value corresponding to t=1.
    /// @param t Interpolation factor. If t is outside [0, 1], the result is clamped to a or b respectively. 
    /// @return Interpolated value between a and b. If t is outside [0, 1], returns a or b respectively.
    inline double lerp(double a, double b, double t) {
        return a + (b - a) * clamp01(t);
    }

    /// @brief Linear interpolation for floats. Clamps t to [0, 1] to prevent extrapolation.
    /// @param a Starting value corresponding to t=0.
    /// @param b Ending value corresponding to t=1.
    /// @param t Interpolation factor. If t is outside [0, 1], the result is clamped to a or b respectively.
    /// @return Interpolated value between a and b. If t is outside [0, 1], returns a or b respectively.
    inline float lerpf(float a, float b, float t) {
        return a + (b - a) * clamp01f(t);
    }

}  // namespace math

// ==================== ENUM CONVERSION UTILITIES ====================
// Generic functions for converting between enums and uint8_t.
// Works with any enum type from any project.

namespace enums {
    /// @brief Convert an enum value to its underlying uint8_t representation. Use for compact storage or serialization.
    /// @tparam EnumT The enum type to convert. Must be an enum with an underlying type that can fit in uint8_t.
    /// @param val The enum value to convert to uint8_t.
    /// @return The uint8_t representation of the enum value. The caller must ensure that the enum value can be safely cast to uint8_t without data loss.
    template <typename EnumT>
    inline uint8_t toUint8(EnumT val) {
        return static_cast<uint8_t>(val);
    }

    /// @brief Convert a uint8_t value back to its enum representation. Use for reading compact storage or deserialization.
    /// @tparam EnumT The enum type to convert to. Must be an enum with an underlying type that can fit in uint8_t.
    /// @param val The uint8_t value to convert to the enum type. The caller must ensure that the uint8_t value corresponds to a valid enum value of EnumT.
    /// @return The enum value corresponding to the given uint8_t. The caller must ensure that the uint8_t value can be safely cast to EnumT without data loss or undefined behavior.
    template <typename EnumT>
    inline EnumT fromUint8(uint8_t val) {
        return static_cast<EnumT>(val);
    }
} // namespace enums

}  // namespace ungula
