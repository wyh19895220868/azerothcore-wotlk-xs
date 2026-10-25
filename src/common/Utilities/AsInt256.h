#ifndef ACORE_AS_INT256_H
#define ACORE_AS_INT256_H

#include "Define.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

// Compatibility names are retained for custom scripts. All values have 256 magnitude bits.
constexpr uint32 AS_LEGACY_FLOAT_INT32_MAX = 2147483520U;

inline AsInt64 AsInt64Zero()
{
    return 0;
}

inline AsInt64 ClampAsInt256(boost::multiprecision::int512_t const& value)
{
    if (value > std::numeric_limits<AsInt64>::max())
        return std::numeric_limits<AsInt64>::max();
    if (value < std::numeric_limits<AsInt64>::min())
        return std::numeric_limits<AsInt64>::min();
    return static_cast<AsInt64>(value);
}

inline bool TryParseAsInt256(std::string_view text, AsInt64& result)
{
    if (text.empty())
        return false;
    bool negative = text.front() == '-';
    size_t position = (negative || text.front() == '+') ? 1 : 0;
    if (position == text.size())
        return false;
    AsInt64 magnitude = 0;
    AsInt64 const limit = std::numeric_limits<AsInt64>::max();
    for (; position < text.size(); ++position)
    {
        unsigned char character = text[position];
        if (character < '0' || character > '9')
            return false;
        uint32 digit = character - '0';
        if (magnitude > (limit - digit) / 10)
            return false;
        magnitude = magnitude * 10 + digit;
    }
    result = negative ? -magnitude : magnitude;
    return true;
}

inline AsInt64 AsInt64FromString(std::string const& value)
{
    std::string text;
    for (unsigned char character : value)
        if (!std::isspace(character))
            text.push_back(static_cast<char>(character));
    if (text.empty())
        return 0;
    bool negative = text.front() == '-';
    size_t position = (negative || text.front() == '+') ? 1 : 0;
    AsInt64 magnitude = 0;
    AsInt64 const limit = std::numeric_limits<AsInt64>::max();
    for (; position < text.size(); ++position)
    {
        unsigned char character = text[position];
        if (character < '0' || character > '9')
            break;
        uint32 digit = character - '0';
        if (magnitude > (limit - digit) / 10)
            return negative ? -limit : limit;
        magnitude = magnitude * 10 + digit;
    }
    return negative ? -magnitude : magnitude;
}

inline std::string AsInt64ToString(AsInt64 const& value)
{
    return value.str();
}

inline bool AsInt64IsZero(AsInt64 const& value)
{
    return value == 0;
}

inline AsInt64 AsInt64Pow10(uint32 digits)
{
    AsInt64 result = 1;
    AsInt64 const limit = std::numeric_limits<AsInt64>::max();
    for (uint32 index = 0; index < digits; ++index)
    {
        if (result > limit / 10)
            return limit;
        result *= 10;
    }
    return result;
}

inline AsInt64 AsDecimalToScaledInt64(std::string const& value, uint32 scaleDigits = 4)
{
    std::string text;
    for (unsigned char character : value)
        if (!std::isspace(character))
            text.push_back(static_cast<char>(character));
    if (text.empty())
        return 0;
    bool negative = text.front() == '-';
    size_t position = (negative || text.front() == '+') ? 1 : 0;
    size_t start = position;
    while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])))
        ++position;
    boost::multiprecision::int512_t result = AsInt64FromString(text.substr(start, position - start));
    result *= AsInt64Pow10(scaleDigits);
    if (position < text.size() && text[position] == '.')
    {
        ++position;
        boost::multiprecision::int512_t fraction = 0;
        uint32 digits = 0;
        // A 256-bit integer can retain at most 78 decimal digits.
        scaleDigits = std::min<uint32>(scaleDigits, 78);
        while (digits < scaleDigits)
        {
            fraction *= 10;
            if (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])))
                fraction += text[position++] - '0';
            ++digits;
        }
        result += fraction;
    }
    if (negative)
        result = -result;
    return ClampAsInt256(result);
}

inline AsInt64 AsInt256FromFloating(long double value)
{
    if (std::isnan(value))
        return 0;
    if (value >= static_cast<long double>(std::numeric_limits<AsInt64>::max()))
        return std::numeric_limits<AsInt64>::max();
    if (value <= static_cast<long double>(std::numeric_limits<AsInt64>::min()))
        return std::numeric_limits<AsInt64>::min();
    return AsInt64(value);
}

inline AsInt64 AsInt256FromFloating(AsInt64 const& value)
{
    return value;
}

template <typename Integer, std::enable_if_t<std::is_integral_v<Integer>, int> = 0>
inline AsInt64 AsInt256FromFloating(Integer value)
{
    return AsInt64(value);
}

template <typename Generator>
inline AsInt64 RandomAsInt256InRange(AsInt64 const& minimum, AsInt64 const& maximum, Generator generate)
{
    if (maximum <= minimum)
        return minimum;
    boost::multiprecision::int512_t span = boost::multiprecision::int512_t(maximum) - minimum;
    unsigned bits = boost::multiprecision::msb(span) + 1;
    boost::multiprecision::int512_t mask = (boost::multiprecision::int512_t(1) << bits) - 1;
    boost::multiprecision::int512_t random;
    do
    {
        random = 0;
        for (unsigned index = 0; index < bits; index += 32)
            random = (random << 32) + generate();
        random &= mask;
    } while (random > span);
    return static_cast<AsInt64>(random + minimum);
}

inline AsInt64 MultiplyDivideAsInt256(AsInt64 const& value, AsInt64 const& multiplier,
    AsInt64 const& divisor)
{
    if (divisor == 0)
        return 0;
    boost::multiprecision::int512_t result = boost::multiprecision::int512_t(value) * multiplier;
    result /= divisor;
    return ClampAsInt256(result);
}

inline AsInt64 ScaleAsInt64ByScaled(AsInt64 const& value, AsInt64 const& scaled, uint32 scaleDigits = 4)
{
    if (value <= 0 || scaled <= 0)
        return 0;
    return MultiplyDivideAsInt256(value, scaled, AsInt64Pow10(scaleDigits));
}

inline AsInt64 ScaleAsInt64ByDouble(AsInt64 const& value, double modifier)
{
    if (value == 0 || !(modifier > 0.0))
        return 0;
    double scaled = modifier * 10000.0 + 0.5;
    if (!std::isfinite(scaled) || scaled >= static_cast<double>(std::numeric_limits<AsInt64>::max()))
        return std::numeric_limits<AsInt64>::max();
    return MultiplyDivideAsInt256(value, AsInt64(scaled), 10000);
}

inline int32 AsInt64ToInt32Clamp(AsInt64 const& value, int32 maxPositive = 2100000000)
{
    if (value > maxPositive)
        return maxPositive;
    if (value < std::numeric_limits<int32>::min())
        return std::numeric_limits<int32>::min();
    return static_cast<int32>(value);
}

inline uint32 AsInt64ToUInt32Clamp(AsInt64 const& value,
    uint32 maxPositive = std::numeric_limits<uint32>::max())
{
    if (value <= 0)
        return 0;
    if (value > maxPositive)
        return maxPositive;
    return static_cast<uint32>(value);
}

inline float AsInt64ToLegacyFloatClamp(AsInt64 const& value)
{
    return static_cast<float>(AsInt64ToUInt32Clamp(value, AS_LEGACY_FLOAT_INT32_MAX));
}

inline double AsInt64ToDoubleClamp(AsInt64 const& value, double maxPositive = 2100000000.0)
{
    if (value <= 0)
        return 0.0;
    return std::min(static_cast<double>(value), maxPositive);
}

#endif
