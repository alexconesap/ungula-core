// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <cmath>
#include <cstdint>
#include <cctype>
#include <type_traits>

namespace ungula {

// ==================== TEMPERATURE CONVERSION ====================
// Shared utilities for temperature unit conversion

namespace temp {

    constexpr double FAHRENHEIT_FREEZING_POINT = 32.0;
    constexpr double FAHRENHEIT_TO_CELSIUS_SCALE = 5.0 / 9.0;
    constexpr double CELSIUS_TO_FAHRENHEIT_SCALE = 9.0 / 5.0;

    template <typename T>
    constexpr T celsiusToFahrenheit(T c) noexcept {
        static_assert(std::is_floating_point_v<T>, "T must be float or double");
        return (c * static_cast<T>(9.0 / 5.0)) + static_cast<T>(32.0);
    }

    template <typename T>
    constexpr T fahrenheitToCelsius(T f) noexcept {
        static_assert(std::is_floating_point_v<T>, "T must be float or double");
        return (f - static_cast<T>(32.0)) * static_cast<T>(5.0 / 9.0);
    }

    /// @brief Check if a Celsius temperature is within a reasonable range and is finite. Use to validate sensor readings or user input before processing.
    /// @param tempC Temperature in degrees Celsius to validate.
    /// @return True if the temperature is valid (finite and between -200.0 and 1800.0 °C), false otherwise.
    inline bool isValidTemperature(double tempC) noexcept {
        return std::isfinite(tempC) && tempC > -200.0 && tempC < 1800.0;
    }

    /// @brief Check if a Celsius temperature is within a reasonable range and is finite. Use to validate sensor readings or user input before processing.
    /// @param tempC Temperature in degrees Celsius to validate.
    /// @return True if the temperature is valid (finite and between -200.0 and 1800.0 °C), false otherwise.
    inline bool isValidTemperatureF(float tempC) noexcept {
        return std::isfinite(tempC) && tempC > -200.0f && tempC < 1800.0f;
    }

    /// @brief Pack a Celsius temperature for wire transmission.
    ///
    /// Encodes by multiplying by 10 and rounding to the nearest integer,
    /// giving 0.1 °C resolution. Receiver must call unpackCelsius() to decode.
    ///
    /// Encoding: value = round(celsius × 10)
    /// Range:    −3276.8 °C to 3276.7 °C (int16_t limits)
    ///
    /// @param celsius Temperature in degrees Celsius.
    /// @return Wire value (celsius × 10, rounded).
    inline int16_t packCelsius(float celsius) {
        return static_cast<int16_t>(celsius * 10.0f + (celsius >= 0.0f ? 0.5f : -0.5f));
    }

    /// @brief Unpack a wire-encoded Celsius value produced by packCelsius().
    /// @param packed Wire value (celsius × 10).
    /// @return Temperature in degrees Celsius with 0.1 °C resolution.
    inline float unpackCelsius(int16_t packed) {
        return static_cast<float>(packed) / 10.0f;
    }

}  // namespace temp

// ==================== MATH UTILITIES ====================
// Common clamping and interpolation functions

namespace math {

    /// @brief Clamp a value to a specified range, returning the clamped result. Use when you want a new value rather than modifying an existing variable.
    /// @tparam T The type of the value to clamp. Must support comparison operators.
    /// @param value The value to clamp.
    /// @param minVal The minimum allowed value. If value is less than minVal, minVal will be returned.
    /// @param maxVal The maximum allowed value. If value is greater than maxVal, maxVal will be returned.
    /// @return The clamped value, guaranteed to be between minVal and maxVal inclusive.
    template <typename T>
    constexpr T clamp(T value, T minVal, T maxVal) noexcept {
        return value < minVal ? minVal : (maxVal < value ? maxVal : value);
    }

    /// @brief Clamp a value to a specified range, modifying it in place. Use when you want to update an existing variable rather than returning a new value.
    /// @tparam T The type of the value to clamp. Must support comparison operators.
    /// @param value The value to clamp. This is passed by reference and will be modified in place to be within the specified range.
    /// @param minVal The minimum allowed value. If value is less than minVal, it will be set to minVal.
    /// @param maxVal The maximum allowed value. If value is greater than maxVal, it will be set to maxVal.
    template <typename T>
    constexpr void clamp_v(T& value, T minVal, T maxVal) noexcept {
        value = clamp(value, minVal, maxVal);
    }

    /// @brief Clamp a double or float number to the range [0.0, 1.0].
    /// @param value The double/float value to clamp.
    /// @return The clamped double/float value, guaranteed to be between 0.0 and 1.0 inclusive.
    template <typename T>
    constexpr T clamp01(T value) noexcept {
        return clamp<T>(value, static_cast<T>(0), static_cast<T>(1));
    }

    /// @brief Linearly interpolate between two values a and b by a factor t in the range [0.0, 1.0].
    /// @tparam T The type of the values to interpolate. Must support arithmetic operations and be compatible with the clamp01 function.
    /// @param a The start value corresponding to t=0.0.
    /// @param b The end value corresponding to t=1.0.
    /// @param t The interpolation factor, typically in the range [0.0, 1.0]. Values outside this range will be clamped to it.
    /// @return The interpolated value, calculated as a + (b - a) * clamp01(t).
    template <typename T>
    constexpr T lerp(T a, T b, T t) noexcept {
        return a + (b - a) * clamp01(t);
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
    constexpr uint8_t toUint8(EnumT val) noexcept {
        return static_cast<uint8_t>(val);
    }

    /// @brief Convert a uint8_t value back to its enum representation. Use for reading compact storage or deserialization.
    /// @tparam EnumT The enum type to convert to. Must be an enum with an underlying type that can fit in uint8_t.
    /// @param val The uint8_t value to convert to the enum type. The caller must ensure that the uint8_t value corresponds to a valid enum value of EnumT.
    /// @return The enum value corresponding to the given uint8_t. The caller must ensure that the uint8_t value can be safely cast to EnumT without data loss or undefined behavior.
    template <typename EnumT>
    constexpr EnumT fromUint8(uint8_t val) noexcept {
        return static_cast<EnumT>(val);
    }
} // namespace enums

}  // namespace ungula
