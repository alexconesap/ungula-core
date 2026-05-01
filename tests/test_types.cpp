#include <gtest/gtest.h>
#include <util/types.h>
#include <cmath>

// Test code is allowed `using namespace`; library code is not.
using namespace ungula;

// --- Temperature conversion ---

TEST(Temperature, CelsiusToFahrenheit) {
    EXPECT_DOUBLE_EQ(temp::celsiusToFahrenheit(0.0), 32.0);
    EXPECT_DOUBLE_EQ(temp::celsiusToFahrenheit(100.0), 212.0);
    EXPECT_NEAR(temp::celsiusToFahrenheit(-40.0), -40.0, 0.001);
}

TEST(Temperature, FahrenheitToCelsius) {
    EXPECT_DOUBLE_EQ(temp::fahrenheitToCelsius(32.0), 0.0);
    EXPECT_DOUBLE_EQ(temp::fahrenheitToCelsius(212.0), 100.0);
    EXPECT_NEAR(temp::fahrenheitToCelsius(-40.0), -40.0, 0.001);
}

TEST(Temperature, RoundTrip) {
    double c = 123.456;
    EXPECT_NEAR(temp::fahrenheitToCelsius(temp::celsiusToFahrenheit(c)), c, 1e-10);
}

TEST(Temperature, FloatVariants) {
    EXPECT_FLOAT_EQ(temp::celsiusToFahrenheitF(0.0f), 32.0f);
    EXPECT_FLOAT_EQ(temp::fahrenheitToCelsiusF(32.0f), 0.0f);
}

TEST(Temperature, IsValidTemperature) {
    EXPECT_TRUE(temp::isValidTemperature(25.0));
    EXPECT_TRUE(temp::isValidTemperature(-100.0));
    EXPECT_TRUE(temp::isValidTemperature(1500.0));
    EXPECT_FALSE(temp::isValidTemperature(-201.0));
    EXPECT_FALSE(temp::isValidTemperature(1800.0));
    EXPECT_FALSE(temp::isValidTemperature(NAN));
    EXPECT_FALSE(temp::isValidTemperature(INFINITY));
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
    EXPECT_EQ(enums::toUint8(TestEnum::A), 0);
    EXPECT_EQ(enums::toUint8(TestEnum::C), 42);
}

TEST(EnumConversion, FromUint8) {
    EXPECT_EQ(enums::fromUint8<TestEnum>(0), TestEnum::A);
    EXPECT_EQ(enums::fromUint8<TestEnum>(42), TestEnum::C);
}

// --- Math clamp (by reference) ---

TEST(MathUtils, ClampDoubleByRef) {
    double v = 15.0;
    math::clamp_v(v, 0.0, 10.0);
    EXPECT_DOUBLE_EQ(v, 10.0);

    v = -5.0;
    math::clamp_v(v, 0.0, 10.0);
    EXPECT_DOUBLE_EQ(v, 0.0);

    v = 5.0;
    math::clamp_v(v, 0.0, 10.0);
    EXPECT_DOUBLE_EQ(v, 5.0);
}

TEST(MathUtils, ClampFloatByRef) {
    float v = -1.0f;
    math::clampf_v(v, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(v, 0.0f);

    v = 1.5f;
    math::clampf_v(v, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(v, 1.0f);
}

TEST(MathUtils, ClampIntByRef) {
    int v = 20;
    math::clampi_v(v, 0, 10);
    EXPECT_EQ(v, 10);

    v = -1;
    math::clampi_v(v, 0, 10);
    EXPECT_EQ(v, 0);

    v = 5;
    math::clampi_v(v, 0, 10);
    EXPECT_EQ(v, 5);
}
