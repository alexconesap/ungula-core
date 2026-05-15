#include <gtest/gtest.h>
#include <ungula/core/util/crc32.h>
#include <cstring>

TEST(CRC32, EmptyDataReturnsZero)
{
        uint8_t data[1] = {};
        uint32_t crc = ungula::core::util::crc32(data, 0);
        // CRC32 of empty data: 0xFFFFFFFF ^ 0xFFFFFFFF = 0x00000000
        EXPECT_EQ(crc, 0x00000000u);
}

TEST(CRC32, KnownStringValue)
{
        // CRC32 of "123456789" is 0xCBF43926 (standard test vector)
        const char *str = "123456789";
        uint32_t crc = ungula::core::util::crc32(reinterpret_cast<const uint8_t *>(str), 9);
        EXPECT_EQ(crc, 0xCBF43926u);
}

TEST(CRC32, SingleByte)
{
        uint8_t data[1] = { 0x00 };
        uint32_t crc = ungula::core::util::crc32(data, 1);
        EXPECT_NE(crc, 0u);
}

TEST(CRC32, DifferentDataProducesDifferentCrc)
{
        uint8_t data1[4] = { 1, 2, 3, 4 };
        uint8_t data2[4] = { 1, 2, 3, 5 };
        EXPECT_NE(ungula::core::util::crc32(data1, 4), ungula::core::util::crc32(data2, 4));
}

TEST(CRC32, SameDataProducesSameCrc)
{
        uint8_t data[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
        EXPECT_EQ(ungula::core::util::crc32(data, 4), ungula::core::util::crc32(data, 4));
}

TEST(CRC32, ByteByByteMatchesBuffer)
{
        const char *str = "Hello";
        uint32_t bufferCrc = ungula::core::util::crc32(reinterpret_cast<const uint8_t *>(str), 5);

        // Compute byte by byte
        uint32_t byteCrc = 0xFFFFFFFFu;
        for (int i = 0; i < 5; i++) {
                byteCrc = ungula::core::util::crc32_byte(byteCrc, static_cast<uint8_t>(str[i]));
        }
        byteCrc ^= 0xFFFFFFFFu;

        EXPECT_EQ(byteCrc, bufferCrc);
}
