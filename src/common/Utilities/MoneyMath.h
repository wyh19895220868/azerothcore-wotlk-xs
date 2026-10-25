#ifndef ACORE_MONEY_MATH_H
#define ACORE_MONEY_MATH_H

#include "Define.h"
#include <cmath>
#include <limits>

namespace MoneyMath
{
inline uint64 Add(uint64 value, uint64 amount)
{
    return amount > UINT64_MAX - value ? UINT64_MAX : value + amount;
}

inline uint64 Multiply(uint64 value, uint64 count)
{
    return count && value > UINT64_MAX / count ? UINT64_MAX : value * count;
}

// Prices are integral copper. Quantize configured rates to millionths and keep balances integral.
inline uint64 Scale(uint64 value, double rate)
{
    if (!std::isfinite(rate) || rate <= 0.0)
        return 0;
    if (rate > 1000000.0)
        return UINT64_MAX;
    uint64 factor = uint64(std::round(rate * 1000000.0));
    uint64 whole = Multiply(value / 1000000, factor);
    uint64 fraction = (value % 1000000) * factor / 1000000;
    return fraction > UINT64_MAX - whole ? UINT64_MAX : whole + fraction;
}
}

#endif
