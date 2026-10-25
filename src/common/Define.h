/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ACORE_DEFINE_H
#define ACORE_DEFINE_H

#include "CompilerDefs.h"
#include <boost/multiprecision/cpp_int.hpp>
#include <cinttypes>
#include <climits>
#include <cmath>
#include <limits>
#include <type_traits>

#define ACORE_LITTLEENDIAN 0
#define ACORE_BIGENDIAN    1

#if !defined(ACORE_ENDIAN)
#  if defined (BOOST_BIG_ENDIAN)
#    define ACORE_ENDIAN ACORE_BIGENDIAN
#  else
#    define ACORE_ENDIAN ACORE_LITTLEENDIAN
#  endif
#endif

#if AC_PLATFORM == AC_PLATFORM_WINDOWS
#  define ACORE_PATH_MAX MAX_PATH
#  define _USE_MATH_DEFINES
#else //AC_PLATFORM != AC_PLATFORM_WINDOWS
#  define ACORE_PATH_MAX PATH_MAX
#endif //AC_PLATFORM

#if !defined(COREDEBUG)
#  define ACORE_INLINE inline
#else //COREDEBUG
#  if !defined(ACORE_DEBUG)
#    define ACORE_DEBUG
#  endif //ACORE_DEBUG
#  define ACORE_INLINE
#endif //!COREDEBUG

#if AC_COMPILER == AC_COMPILER_GNU
#  define ATTR_PRINTF(F, V) __attribute__ ((format (printf, F, V)))
#else //AC_COMPILER != AC_COMPILER_GNU
#  define ATTR_PRINTF(F, V)
#endif //AC_COMPILER == AC_COMPILER_GNU

#ifdef ACORE_API_USE_DYNAMIC_LINKING
#  if AC_COMPILER == AC_COMPILER_MICROSOFT
#    define AC_API_EXPORT __declspec(dllexport)
#    define AC_API_IMPORT __declspec(dllimport)
#  elif AC_COMPILER == AC_COMPILER_GNU
#    define AC_API_EXPORT __attribute__((visibility("default")))
#    define AC_API_IMPORT
#  else
#    error compiler not supported!
#  endif
#else
#  define AC_API_EXPORT
#  define AC_API_IMPORT
#endif

#ifdef ACORE_API_EXPORT_COMMON
#  define AC_COMMON_API AC_API_EXPORT
#else
#  define AC_COMMON_API AC_API_IMPORT
#endif

#ifdef ACORE_API_EXPORT_DATABASE
#  define AC_DATABASE_API AC_API_EXPORT
#else
#  define AC_DATABASE_API AC_API_IMPORT
#endif

#ifdef ACORE_API_EXPORT_SHARED
#  define AC_SHARED_API AC_API_EXPORT
#else
#  define AC_SHARED_API AC_API_IMPORT
#endif

#ifdef ACORE_API_EXPORT_GAME
#  define AC_GAME_API AC_API_EXPORT
#else
#  define AC_GAME_API AC_API_IMPORT
#endif

#define UI64LIT(N) UINT64_C(N)
#define SI64LIT(N) INT64_C(N)

#define STRING_VIEW_FMT_ARG(str) static_cast<int>((str).length()), (str).data()

typedef std::int64_t int64;
typedef std::int32_t int32;
typedef std::int16_t int16;
typedef std::int8_t int8;
typedef std::uint64_t uint64;

// Preserve existing custom APIs while restoring the original 256-bit magnitude backend.
// Signed values use Boost sign-magnitude; stock WoW fields remain explicitly clamped projections.
using AsInt256 = boost::multiprecision::int256_t;
using AsInt64 = AsInt256;
typedef std::uint32_t uint32;
typedef std::uint16_t uint16;
typedef std::uint8_t uint8;

namespace Acore::Number
{
template <typename T>
inline long double ToLongDouble(T value)
{
    return static_cast<long double>(value);
}

template <typename T>
inline float ToFloat(T value)
{
    long double const converted = ToLongDouble(value);
    if (converted >= static_cast<long double>(std::numeric_limits<float>::max()))
        return std::numeric_limits<float>::max();
    if (converted <= static_cast<long double>(-std::numeric_limits<float>::max()))
        return -std::numeric_limits<float>::max();
    return static_cast<float>(converted);
}

template <typename T>
inline int32 ToInt32Saturated(T value)
{
    long double const converted = ToLongDouble(value);
    if (converted >= static_cast<long double>(std::numeric_limits<int32>::max()))
        return std::numeric_limits<int32>::max();
    if (converted <= static_cast<long double>(std::numeric_limits<int32>::min()))
        return std::numeric_limits<int32>::min();
    return static_cast<int32>(value);
}

template <typename T>
inline uint32 ToUInt32Saturated(T value)
{
    long double const converted = ToLongDouble(value);
    if (converted <= 0.0L)
        return 0;
    if (converted >= static_cast<long double>(std::numeric_limits<uint32>::max()))
        return std::numeric_limits<uint32>::max();
    return static_cast<uint32>(value);
}

template <typename T>
inline uint64 ToUInt64Saturated(T value)
{
    long double const converted = ToLongDouble(value);
    if (converted <= 0.0L)
        return 0;
    if (converted >= static_cast<long double>(std::numeric_limits<uint64>::max()))
        return std::numeric_limits<uint64>::max();
    return static_cast<uint64>(value);
}

template <typename T>
inline int64 ToInt64Saturated(T value)
{
    long double const converted = ToLongDouble(value);
    if (converted <= static_cast<long double>(std::numeric_limits<int64>::min()))
        return std::numeric_limits<int64>::min();
    if (converted >= static_cast<long double>(std::numeric_limits<int64>::max()))
        return std::numeric_limits<int64>::max();
    return static_cast<int64>(value);
}

template <typename Value, typename Percent>
inline auto CalculatePct(Value value, Percent percent)
{
    if constexpr (std::is_same_v<Value, AsInt256> || std::is_same_v<Percent, AsInt256>)
    {
        boost::multiprecision::int512_t result = boost::multiprecision::int512_t(value) * percent / 100;
        if (result > std::numeric_limits<AsInt256>::max())
            return std::numeric_limits<AsInt256>::max();
        if (result < std::numeric_limits<AsInt256>::min())
            return std::numeric_limits<AsInt256>::min();
        return static_cast<AsInt256>(result);
    }
    else
        return (value * percent) / 100;
}
}
#endif //ACORE_DEFINE_H
