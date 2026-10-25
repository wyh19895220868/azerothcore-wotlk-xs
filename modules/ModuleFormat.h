#pragma once

#include "StringConvert.h"
#include "StringFormat.h"

#include <cstdint>
#include <cctype>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ModuleFormatCompat
{
    inline bool IsPrintfConversion(char value)
    {
        switch (value)
        {
            case 'a':
            case 'A':
            case 'c':
            case 'd':
            case 'e':
            case 'E':
            case 'f':
            case 'F':
            case 'g':
            case 'G':
            case 'i':
            case 'o':
            case 'p':
            case 's':
            case 'u':
            case 'x':
            case 'X':
                return true;
            default:
                return false;
        }
    }

    // Module sources still use printf-style placeholders while the core formatter uses fmt syntax.
    inline std::string ConvertPrintfFormat(std::string_view format)
    {
        std::string converted;
        converted.reserve(format.size());

        for (std::size_t index = 0; index < format.size(); ++index)
        {
            if (format[index] != '%')
            {
                converted.push_back(format[index]);
                continue;
            }

            if (index + 1 >= format.size())
            {
                converted.push_back('%');
                continue;
            }

            if (format[index + 1] == '%')
            {
                converted.push_back('%');
                ++index;
                continue;
            }

            std::size_t conversion = index + 1;
            while (conversion < format.size() &&
                (format[conversion] == '-' || format[conversion] == '+' || format[conversion] == ' ' ||
                    format[conversion] == '#' || format[conversion] == '0'))
                ++conversion;

            while (conversion < format.size() && std::isdigit(static_cast<unsigned char>(format[conversion])))
                ++conversion;

            if (conversion < format.size() && format[conversion] == '.')
            {
                ++conversion;
                while (conversion < format.size() && std::isdigit(static_cast<unsigned char>(format[conversion])))
                    ++conversion;
            }

            while (conversion < format.size() &&
                (format[conversion] == 'h' || format[conversion] == 'l' || format[conversion] == 'j' ||
                    format[conversion] == 'z' || format[conversion] == 't' || format[conversion] == 'L'))
                ++conversion;

            if (conversion < format.size() && IsPrintfConversion(format[conversion]))
            {
                converted += "{}";
                index = conversion;
                continue;
            }

            converted.push_back('%');
        }

        return converted;
    }

    template <typename T, typename = void>
    struct HasMemberToString : std::false_type { };

    template <typename T>
    struct HasMemberToString<T, std::void_t<decltype(std::declval<T const&>().ToString())>> : std::true_type { };

    inline std::string ToOldFormatString(std::string const& value)
    {
        return value;
    }

    inline std::string ToOldFormatString(std::string_view value)
    {
        return std::string(value);
    }

    inline std::string ToOldFormatString(char const* value)
    {
        return value ? std::string(value) : std::string();
    }

    inline std::string ToOldFormatString(char* value)
    {
        return value ? std::string(value) : std::string();
    }

    template <std::size_t N>
    inline std::string ToOldFormatString(char const (&value)[N])
    {
        return std::string(value);
    }

    inline std::string ToOldFormatString(char value)
    {
        return std::string(1, value);
    }

    inline std::string ToOldFormatString(bool value)
    {
        return value ? "1" : "0";
    }

    template <typename T>
    std::enable_if_t<std::is_enum_v<std::decay_t<T>>, std::string> ToOldFormatString(T value)
    {
        return Acore::ToString(static_cast<std::underlying_type_t<std::decay_t<T>>>(value));
    }

    template <typename T>
    std::enable_if_t<HasMemberToString<std::decay_t<T>>::value, std::string> ToOldFormatString(T const& value)
    {
        return value.ToString();
    }

    template <typename T>
    std::enable_if_t<std::is_pointer_v<std::decay_t<T>> && !std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::decay_t<T>>>, char>, std::string> ToOldFormatString(T value)
    {
        return Acore::ToString(static_cast<uint64>(reinterpret_cast<std::uintptr_t>(value)));
    }

    template <typename T>
    std::enable_if_t<!std::is_enum_v<std::decay_t<T>> && !std::is_pointer_v<std::decay_t<T>> && !HasMemberToString<std::decay_t<T>>::value, std::string> ToOldFormatString(T const& value)
    {
        return Acore::ToString(value);
    }

    template <typename... Args>
    std::string OldFormat(char const* fmt, Args&&... args)
    {
        return Acore::StringFormat(ConvertPrintfFormat(fmt), ToOldFormatString(std::forward<Args>(args)).c_str()...);
    }
}

template <typename... Args>
std::string ModuleOldFormat(char const* fmt, Args&&... args)
{
    return ModuleFormatCompat::OldFormat(fmt, std::forward<Args>(args)...);
}
