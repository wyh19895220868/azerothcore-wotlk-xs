#include "AsInt256.h"
#include "gtest/gtest.h"

TEST(AsInt256Test, PreservesDecimalValuesAboveNativeIntegerRange)
{
    std::string const decimal = "12345678901234567890123456789012345678901234567890123456789012345";
    AsInt256 value;
    ASSERT_TRUE(TryParseAsInt256(decimal, value));
    EXPECT_EQ(AsInt64ToString(value), decimal);
    ASSERT_TRUE(TryParseAsInt256("-" + decimal, value));
    EXPECT_EQ(AsInt64ToString(value), "-" + decimal);
}

TEST(AsInt256Test, RejectsInvalidAndOverflowingInput)
{
    AsInt256 value;
    std::string const maximum = std::numeric_limits<AsInt256>::max().str();
    ASSERT_TRUE(TryParseAsInt256(maximum, value));
    EXPECT_EQ(value, std::numeric_limits<AsInt256>::max());
    EXPECT_FALSE(TryParseAsInt256(maximum + "0", value));
    EXPECT_FALSE(TryParseAsInt256("17junk", value));
    EXPECT_FALSE(TryParseAsInt256("-", value));
    EXPECT_FALSE(TryParseAsInt256("", value));
}

TEST(AsInt256Test, UsesWideIntermediateValuesBeforeClamping)
{
    AsInt256 const maximum = std::numeric_limits<AsInt256>::max();
    EXPECT_EQ(MultiplyDivideAsInt256(maximum, maximum, maximum), maximum);
    EXPECT_EQ(MultiplyDivideAsInt256(maximum, 2, 2), maximum);
    EXPECT_EQ(MultiplyDivideAsInt256(maximum, 2, 1), maximum);
    EXPECT_EQ(MultiplyDivideAsInt256(-maximum, 2, 1), -maximum);
    EXPECT_EQ(AsInt64ToUInt32Clamp(AsInt256(1) << 200), std::numeric_limits<uint32>::max());
}

TEST(AsInt256Test, GeneratesValuesAcrossWideSignedRanges)
{
    uint32 seed = 7;
    auto generate = [&seed]
    {
        seed = seed * 1664525U + 1013904223U;
        return seed;
    };
    AsInt256 const minimum = -(AsInt256(1) << 200);
    AsInt256 const maximum = AsInt256(1) << 220;
    for (uint32 index = 0; index < 1000; ++index)
    {
        AsInt256 const value = RandomAsInt256InRange(minimum, maximum, generate);
        EXPECT_GE(value, minimum);
        EXPECT_LE(value, maximum);
    }
    EXPECT_EQ(RandomAsInt256InRange(maximum, maximum, generate), maximum);
}
