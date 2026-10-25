#include "AsInt256.h"
#include "StringConvert.h"
#include "Custom/_Helper/AsInt64.h"
#ifndef RUNE_UTILS_H
#define RUNE_UTILS_H

#include "Common.h"
#include "StringConvert.h"
#include "Util.h"
#include <string>
#include <vector>
#include <sstream>
#include <limits>

// 符文工具类
class RuneUtils
{
public:
    // 解析逗号分隔的字符串为整数列表
    static std::vector<uint32> ParseCommaSeparatedUInt32(const std::string& str)
    {
        std::vector<uint32> result;
        if (str.empty())
            return result;

        std::stringstream ss(str);
        std::string token;

        while (std::getline(ss, token, ','))
        {
            // 移除空格
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);

            if (!token.empty())
            {
                try
                {
                    result.push_back(std::stoul(token));
                }
                catch (...)
                {
                    result.push_back(0);
                }
            }
        }

        return result;
    }

    // 将整数列表转换为逗号分隔的字符串
    static std::string JoinUInt32WithComma(const std::vector<uint32>& values)
    {
        if (values.empty())
            return "";

        std::stringstream ss;
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0)
                ss << ",";
            ss << values[i];
        }

        return ss.str();
    }

    // 解析效果值字符串 (格式: "属性ID 值,属性ID 值" 或 "99 值")
    static int32 ToInt32Saturated(AsInt64 const& value)
    {
        return Acore::Number::ToInt32Saturated(value);
    }

    static std::vector<std::pair<uint32, AsInt64>> ParseEffectValues(const std::string& effectValues)
    {
        std::vector<std::pair<uint32, AsInt64>> result;
        if (effectValues.empty())
            return result;

        std::stringstream ss(effectValues);
        std::string token;

        // 按逗号分割
        while (std::getline(ss, token, ','))
        {
            // 移除空格
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);

            if (token.empty())
                continue;

            // 解析 "属性ID 值" 格式
            std::stringstream tokenStream(token);
            uint32 attributeId = 0;
            std::string valueText;

            if (tokenStream >> attributeId >> valueText)
            {
                Optional<AsInt64> value = Acore::StringTo<AsInt64>(valueText);
                if (value)
                    result.push_back(std::make_pair(attributeId, *value));
            }
        }

        return result;
    }

    // 检查符文是否可以装备在指定物品上
    static bool CanEquipRuneOnItem(uint32 runeItemId, uint32 targetItemId)
    {
        // runeItemId = 0 表示通用符文，可以装备在任何物品上
        if (runeItemId == 0)
            return true;

        // 否则必须物品ID匹配
        return runeItemId == targetItemId;
    }

    // 检查符文槽位是否匹配
    static bool IsRuneSlotMatched(uint32 runeSlot, uint32 targetSlot)
    {
        return runeSlot == targetSlot;
    }

    // 获取插槽ID列表中指定索引的插槽ID
    static uint32 GetSlotIdAtIndex(const std::string& slotIds, size_t index)
    {
        auto slots = ParseCommaSeparatedUInt32(slotIds);
        if (index < slots.size())
            return slots[index];
        return 0;
    }

    // 获取符文ID列表中指定索引的符文ID
    static uint32 GetRuneIdAtIndex(const std::string& runeIds, size_t index)
    {
        auto runes = ParseCommaSeparatedUInt32(runeIds);
        if (index < runes.size())
            return runes[index];
        return 0;
    }

    // 设置符文ID列表中指定插槽的符文ID
    static std::string SetRuneIdAtSlot(const std::string& slotIds, const std::string& runeIds,
                                       uint32 targetSlot, uint32 newRuneId)
    {
        auto slots = ParseCommaSeparatedUInt32(slotIds);
        auto runes = ParseCommaSeparatedUInt32(runeIds);

        // 查找目标插槽的索引
        for (size_t i = 0; i < slots.size(); ++i)
        {
            if (slots[i] == targetSlot)
            {
                // 确保符文列表有足够的元素
                while (runes.size() <= i)
                    runes.push_back(0);

                runes[i] = newRuneId;
                return JoinUInt32WithComma(runes);
            }
        }

        return runeIds;
    }

    // 获取符文数量（非0的符文）
    static uint32 GetEquippedRuneCount(const std::string& runeIds)
    {
        auto runes = ParseCommaSeparatedUInt32(runeIds);
        uint32 count = 0;

        for (uint32 runeId : runes)
        {
            if (runeId != 0)
                count++;
        }

        return count;
    }

    // 获取插槽总数
    static uint32 GetTotalSlotCount(const std::string& slotIds)
    {
        auto slots = ParseCommaSeparatedUInt32(slotIds);
        return static_cast<uint32>(slots.size());
    }

    // 创建新的插槽列表 (1,2,3,...,count)
    static std::string CreateSlotIdList(uint32 count)
    {
        std::vector<uint32> slots;
        for (uint32 i = 1; i <= count && i <= 20; ++i)
        {
            slots.push_back(i);
        }
        return JoinUInt32WithComma(slots);
    }

    // 创建空的符文ID列表
    static std::string CreateEmptyRuneIdList(uint32 count)
    {
        std::vector<uint32> runes(count, 0);
        return JoinUInt32WithComma(runes);
    }
};

#endif // RUNE_UTILS_H
