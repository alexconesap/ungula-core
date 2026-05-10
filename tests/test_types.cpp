#include <gtest/gtest.h>
#include <ungula/core/util/types.h>
#include <cmath>

// Test code is allowed `using namespace`; library code is not.
using namespace ungula::core::util;

// --- Temperature conversion ---

TEST(Temperature, CelsiusToFahrenheit)
{
    EXPECT_DOUBLE_EQ(temp::celsiusToFahrenheit(0.0), 32.0);
    EXPECT_DOUBLE_EQ(temp::celsiusToFahrenheit(100.0), 212.0);
    EXPECT_NEAR(temp::celsiusToFahrenheit(-40.0), -40.0, 0.001);
}

TEST(Temperature, FahrenheitToCelsius)
{
    EXPECT_DOUBLE_EQ(temp::fahrenheitToCelsius(32.0), 0.0);
    EXPECT_DOUBLE_EQ(temp::fahrenheitToCelsius(212.0), 100.0);
    EXPECT_NEAR(temp::fahrenheitToCelsius(-40.0), -40.0, 0.001);
}

TEST(Temperature, RoundTrip)
{
    double c = 123.456;
    EXPECT_NEAR(temp::fahrenheitToCelsius(temp::celsiusToFahrenheit(c)), c, 1e-10);
}

TEST(Temperature, FloatVariants)
{
    EXPECT_FLOAT_EQ(temp::celsiusToFahrenheit(0.0f), 32.0F);
    EXPECT_FLOAT_EQ(temp::fahrenheitToCelsius(32.0f), 0.0F);
}

TEST(Temperature, IsValidTemperature)
{
    EXPECT_TRUE(temp::isValidTemperature(25.0));
    EXPECT_TRUE(temp::isValidTemperature(-100.0));
    EXPECT_TRUE(temp::isValidTemperature(1500.0));
    EXPECT_TRUE(temp::isValidTemperature(1500.0F));
    EXPECT_FALSE(temp::isValidTemperature(-201.0));
    EXPECT_FALSE(temp::isValidTemperature(1800.0));
    EXPECT_FALSE(temp::isValidTemperature(1800.0F));
    EXPECT_FALSE(temp::isValidTemperature(NAN));
    EXPECT_FALSE(temp::isValidTemperature(INFINITY));
}

TEST(Temperature, PackCelsius)
{
    EXPECT_EQ(temp::packCelsius(0.0f), 0);
    EXPECT_EQ(temp::packCelsius(25.5f), 255);
    EXPECT_EQ(temp::packCelsius(-10.0f), -100);
    EXPECT_EQ(temp::packCelsius(100.0f), 1000);
    EXPECT_EQ(temp::packCelsius(-0.1f), -1);
    EXPECT_EQ(temp::packCelsius(0.04f), 0);
    EXPECT_EQ(temp::packCelsius(0.05f), 1);
}

TEST(Temperature, UnpackCelsius)
{
    EXPECT_FLOAT_EQ(temp::unpackCelsius(0), 0.0f);
    EXPECT_FLOAT_EQ(temp::unpackCelsius(255), 25.5f);
    EXPECT_FLOAT_EQ(temp::unpackCelsius(-100), -10.0f);
    EXPECT_FLOAT_EQ(temp::unpackCelsius(1000), 100.0f);
    EXPECT_FLOAT_EQ(temp::unpackCelsius(-1), -0.1f);
}

TEST(Temperature, PackUnpackRoundTrip)
{
    const float inputs[] = { 0.0f, 25.0F, -40.0F, 100.0F, -10.5f, 0.05F };
    for (float c : inputs) {
        int16_t packed = temp::packCelsius(c);
        float unpacked = temp::unpackCelsius(packed);
        EXPECT_NEAR(unpacked, c, 0.1F) << "round-trip failed for " << c;
    }
}

// --- Math clamp ---

TEST(MathUtils, ClampDouble)
{
    EXPECT_DOUBLE_EQ(math::clamp(5.0, 0.0, 10.0), 5.0);
    EXPECT_DOUBLE_EQ(math::clamp(-5.0, 0.0, 10.0), 0.0);
    EXPECT_DOUBLE_EQ(math::clamp(15.0, 0.0, 10.0), 10.0);
}

TEST(MathUtils, ClampFloat)
{
    EXPECT_FLOAT_EQ(math::clamp(0.5f, 0.0f, 1.0f), 0.5F);
    EXPECT_FLOAT_EQ(math::clamp(-1.0f, 0.0f, 1.0f), 0.0F);
}

TEST(MathUtils, ClampInt)
{
    EXPECT_EQ(math::clamp(5, 0, 10), 5);
    EXPECT_EQ(math::clamp(-1, 0, 10), 0);
    EXPECT_EQ(math::clamp(20, 0, 10), 10);
}

TEST(MathUtils, Clamp01)
{
    EXPECT_DOUBLE_EQ(math::clamp01(0.1), 0.1);
    EXPECT_DOUBLE_EQ(math::clamp01(0.5), 0.5);
    EXPECT_DOUBLE_EQ(math::clamp01(-0.1), 0.0);
    EXPECT_DOUBLE_EQ(math::clamp01(0.9), 0.9);
    EXPECT_DOUBLE_EQ(math::clamp01(1.0), 1.0);
    EXPECT_DOUBLE_EQ(math::clamp01(1.5), 1.0);
}

TEST(MathUtils, Lerp)
{
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, 1.0), 10.0);
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, 0.5), 5.0);
}

TEST(MathUtils, LerpClampsT)
{
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, -1.0), 0.0);
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, 2.0), 10.0);
    EXPECT_DOUBLE_EQ(math::lerp(0.0, 10.0, 1.5), 10.0);
}

// --- Enum conversion ---

enum class TestEnum : uint8_t { A = 0, B = 1, C = 42 };

TEST(EnumConversion, ToUint8)
{
    EXPECT_EQ(enums::toUint8(TestEnum::A), 0);
    EXPECT_EQ(enums::toUint8(TestEnum::C), 42);
}

TEST(EnumConversion, FromUint8)
{
    EXPECT_EQ(enums::fromUint8<TestEnum>(0), TestEnum::A);
    EXPECT_EQ(enums::fromUint8<TestEnum>(42), TestEnum::C);
}

// --- Math clamp (by reference) ---

TEST(MathUtils, ClampDoubleByRef)
{
    double var = 15.0;
    math::clamp_v(var, 0.0, 10.0);
    EXPECT_DOUBLE_EQ(var, 10.0);

    var = -5.0;
    math::clamp_v(var, 0.0, 10.0);
    EXPECT_DOUBLE_EQ(var, 0.0);

    var = 5.0;
    math::clamp_v(var, 0.0, 10.0);
    EXPECT_DOUBLE_EQ(var, 5.0);
}

TEST(MathUtils, ClampFloatByRef)
{
    float var = -1.0F;
    math::clamp_v(var, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(var, 0.0F);

    var = 1.5f;
    math::clamp_v(var, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(var, 1.0F);
}

TEST(MathUtils, ClampIntByRef)
{
    int var = 20;
    math::clamp_v(var, 0, 10);
    EXPECT_EQ(var, 10);

    var = -1;
    math::clamp_v(var, 0, 10);
    EXPECT_EQ(var, 0);

    var = 5;
    math::clamp_v(var, 0, 10);
    EXPECT_EQ(var, 5);
}
