#ifndef NAZ_AS_INT128_H
#define NAZ_AS_INT128_H

#include "AsInt64.h"

// Historical name, shared 256-bit implementation.
using AsInt128 = AsInt256;
inline AsInt128 AsInt128Zero() { return AsInt64Zero(); }
inline AsInt128 AsInt128FromString(std::string const& value) { return AsInt64FromString(value); }
inline std::string AsInt128ToString(AsInt128 const& value) { return AsInt64ToString(value); }
inline bool AsInt128IsZero(AsInt128 const& value) { return AsInt64IsZero(value); }
inline AsInt128 AsInt128Pow10(uint32 digits) { return AsInt64Pow10(digits); }
inline AsInt128 AsDecimalToScaledInt128(std::string const& value, uint32 digits = 4)
{
    return AsDecimalToScaledInt64(value, digits);
}
inline AsInt128 ScaleAsInt128ByScaled(AsInt128 const& value, AsInt128 const& scaled, uint32 digits = 4)
{
    return ScaleAsInt64ByScaled(value, scaled, digits);
}
inline AsInt128 ScaleAsInt128ByDouble(AsInt128 const& value, double modifier)
{
    return ScaleAsInt64ByDouble(value, modifier);
}
inline int32 AsInt128ToInt32Clamp(AsInt128 const& value, int32 limit = 2100000000)
{
    return AsInt64ToInt32Clamp(value, limit);
}
inline uint32 AsInt128ToUInt32Clamp(AsInt128 const& value, uint32 limit = std::numeric_limits<uint32>::max())
{
    return AsInt64ToUInt32Clamp(value, limit);
}
inline float AsInt128ToLegacyFloatClamp(AsInt128 const& value) { return AsInt64ToLegacyFloatClamp(value); }
inline double AsInt128ToDoubleClamp(AsInt128 const& value, double limit = 2100000000.0)
{
    return AsInt64ToDoubleClamp(value, limit);
}

#endif
