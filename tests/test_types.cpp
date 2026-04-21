#include <gtest/gtest.h>
#include <util/types.h>
#include <cmath>

// --- Temperature conversion ---

TEST(Temperature, CelsiusToFahrenheit) {
    EXPECT_DOUBLE_EQ(temperature::celsiusToFahrenheit(0.0), 32.0);
    EXPECT_DOUBLE_EQ(temperature::celsiusToFahrenheit(100.0), 212.0);
    EXPECT_NEAR(temperature::celsiusToFahrenheit(-40.0), -40.0, 0.001);
}

TEST(Temperature, FahrenheitToCelsius) {
    EXPECT_DOUBLE_EQ(temperature::fahrenheitToCelsius(32.0), 0.0);
    EXPECT_DOUBLE_EQ(temperature::fahrenheitToCelsius(212.0), 100.0);
    EXPECT_NEAR(temperature::fahrenheitToCelsius(-40.0), -40.0, 0.001);
}

TEST(Temperature, RoundTrip) {
    double c = 123.456;
    EXPECT_NEAR(temperature::fahrenheitToCelsius(temperature::celsiusToFahrenheit(c)), c, 1e-10);
}

TEST(Temperature, FloatVariants) {
    EXPECT_FLOAT_EQ(temperature::celsiusToFahrenheitF(0.0f), 32.0f);
    EXPECT_FLOAT_EQ(temperature::fahrenheitToCelsiusF(32.0f), 0.0f);
}

TEST(Temperature, IsValidTemperature) {
    EXPECT_TRUE(temperature::isValidTemperature(25.0));
    EXPECT_TRUE(temperature::isValidTemperature(-100.0));
    EXPECT_TRUE(temperature::isValidTemperature(1500.0));
    EXPECT_FALSE(temperature::isValidTemperature(-201.0));
    EXPECT_FALSE(temperature::isValidTemperature(1800.0));
    EXPECT_FALSE(temperature::isValidTemperature(NAN));
    EXPECT_FALSE(temperature::isValidTemperature(INFINITY));
}

// --- Math clamp ---

TEST(MathUtils, ClampDouble) {
    EXPECT_DOUBLE_EQ(math::clamp(5.0, 0.0, 10.0), 5.0);
    EXPECT_DOUBLE_EQ(math::clamp(-5.0, 0.0, 10.0), 0.0);
    EXPECT_DOUBLE_EQ(math::clamp(15.0, 0.0, 10.0), 10.0);
}

TEST(MathUtils, ClampFloat) {
    EXPECT_FLOAT_EQ(math::clampf(0.5f, 0.0f, 1.0f), 0.5f);
    EXPECT_FLOAT_EQ(math::clampf(-1.0f, 0.0f, 1.0f), 0.0f);
}

TEST(MathUtils, ClampInt) {
    EXPECT_EQ(math::clampi(5, 0, 10), 5);
    EXPECT_EQ(math::clampi(-1, 0, 10), 0);
    EXPECT_EQ(math::clampi(20, 0, 10), 10);
}

TEST(MathUtils, Clamp01) {
    EXPECT_DOUBLE_EQ(math::clamp01(0.5), 0.5);
    EXPECT_DOUBLE_EQ(math::clamp01(-0.1), 0.0);
    EXPECT_DOUBLE_EQ(math::clamp01(1.5), 1.0);
}

TEST(MathUtils, Lerp) {
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, 1.0), 10.0);
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, 0.5), 5.0);
}

TEST(MathUtils, LerpClampsT) {
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, -1.0), 0.0);
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, 2.0), 10.0);
}

// --- Enum conversion ---

enum class TestEnum : uint8_t { A = 0, B = 1, C = 42 };

TEST(EnumConversion, ToUint8) {
    EXPECT_EQ(toUint8(TestEnum::A), 0);
    EXPECT_EQ(toUint8(TestEnum::C), 42);
}

TEST(EnumConversion, FromUint8) {
    EXPECT_EQ(fromUint8<TestEnum>(0), TestEnum::A);
    EXPECT_EQ(fromUint8<TestEnum>(42), TestEnum::C);
}

// --- Platform string helpers ---

TEST(PlatformStrings, StringToInt) {
    EXPECT_EQ(stringToInt("42"), 42);
    EXPECT_EQ(stringToInt("invalid"), 0);
}

TEST(PlatformStrings, StringToDouble) {
    EXPECT_NEAR(stringToDouble("3.14"), 3.14, 0.001);
    EXPECT_DOUBLE_EQ(stringToDouble("invalid"), 0.0);
}

TEST(PlatformStrings, IntToString) {
    EXPECT_EQ(intToString(42), "42");
}

TEST(PlatformStrings, StringStartsWith) {
    EXPECT_TRUE(stringStartsWith("hello world", "hello"));
    EXPECT_FALSE(stringStartsWith("hello", "world"));
}

TEST(PlatformStrings, StringEndsWith) {
    EXPECT_TRUE(stringEndsWith("hello world", "world"));
    EXPECT_FALSE(stringEndsWith("hello", "world"));
}

TEST(PlatformStrings, StringTrim) {
    EXPECT_EQ(stringTrim("  hello  "), "hello");
    EXPECT_EQ(stringTrim(""), "");
}

TEST(PlatformStrings, StringCase) {
    EXPECT_EQ(stringToLower("HELLO"), "hello");
    EXPECT_EQ(stringToUpper("hello"), "HELLO");
}
