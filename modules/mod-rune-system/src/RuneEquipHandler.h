#ifndef RUNE_EQUIP_HANDLER_H
#define RUNE_EQUIP_HANDLER_H

#include "Common.h"
#include "RuneTypes.h"
#include "RuneUtils.h"
#include "Item.h"
#include <string>

// 符文装备处理类
class RuneEquipHandler
{
public:
    // 装备符文到指定插槽
    static bool EquipRuneToSlot(uint32 itemGuid, uint32 slotId, uint32 runeItemId, uint32 characterGuid)
    {
        // 查询当前物品的符文记录
        std::string query = "SELECT ID, 插槽ID, 符文物品ID FROM 符文系统_数据 WHERE 物品GUID = " + std::to_string(itemGuid);
        QueryResult result = CharacterDatabase.Query(query.c_str());

        if (!result)
        {
            return false; // 物品没有符文插槽
        }

        Field* fields = result->Fetch();
        uint32 recordId = fields[0].Get<uint32>();
        std::string slotIds = fields[1].Get<std::string>();
        std::string runeItemIds = fields[2].Get<std::string>();

        // 使用RuneUtils设置符文物品ID
        std::string newRuneItemIds = RuneUtils::SetRuneIdAtSlot(slotIds, runeItemIds, slotId, runeItemId);

        if (newRuneItemIds == runeItemIds)
        {
            return false; // 没有改变，可能是插槽不存在
        }

        // 更新数据库
        // 【数据丢失修复】改同步写：调用方（RuneApplyManager）随后会同步重读本表刷新内存缓存，
        // 异步写未落库时缓存被旧数据回滚 → 槽位显示为空可二次镶嵌，符文丢失
        std::string updateQuery = "UPDATE 符文系统_数据 SET 符文物品ID = '" + newRuneItemIds + "' WHERE ID = " + std::to_string(recordId);
        CharacterDatabase.DirectExecute(updateQuery.c_str());

        return true;
    }

    // 从指定插槽移除符文
    static bool RemoveRuneFromSlot(uint32 itemGuid, uint32 slotId)
    {
        return EquipRuneToSlot(itemGuid, slotId, 0, 0); // 设置为0表示移除
    }

    // 检查插槽是否已装备符文
    static bool IsSlotEquipped(const std::string& slotIds, const std::string& runeIds, uint32 targetSlot)
    {
        auto slots = RuneUtils::ParseCommaSeparatedUInt32(slotIds);
        auto runes = RuneUtils::ParseCommaSeparatedUInt32(runeIds);

        for (size_t i = 0; i < slots.size(); ++i)
        {
            if (slots[i] == targetSlot)
            {
                if (i < runes.size() && runes[i] != 0)
                {
                    return true;
                }
                return false;
            }
        }

        return false;
    }

    // 获取指定插槽装备的符文ID
    static uint32 GetEquippedRuneId(const std::string& slotIds, const std::string& runeIds, uint32 targetSlot)
    {
        auto slots = RuneUtils::ParseCommaSeparatedUInt32(slotIds);
        auto runes = RuneUtils::ParseCommaSeparatedUInt32(runeIds);

        for (size_t i = 0; i < slots.size(); ++i)
        {
            if (slots[i] == targetSlot)
            {
                if (i < runes.size())
                {
                    return runes[i];
                }
                return 0;
            }
        }

        return 0;
    }

    // 获取装备上所有符文的ID列表
    static std::vector<uint32> GetAllEquippedRuneIds(const std::string& runeIds)
    {
        std::vector<uint32> result;
        auto runes = RuneUtils::ParseCommaSeparatedUInt32(runeIds);

        for (uint32 runeId : runes)
        {
            if (runeId != 0)
            {
                result.push_back(runeId);
            }
        }

        return result;
    }

    // 检查符文是否可以装备到指定插槽
    static bool CanEquipRuneToSlot(uint32 runeSlot, uint32 targetSlot, uint32 runeItemId, uint32 targetItemId)
    {
        // 检查槽位是否匹配 (runeSlot=0 表示任意槽位)
        if (runeSlot != 0 && runeSlot != targetSlot)
        {
            return false;
        }

        // 检查物品ID是否匹配
        if (!RuneUtils::CanEquipRuneOnItem(runeItemId, targetItemId))
        {
            return false;
        }

        return true;
    }
};

#endif // RUNE_EQUIP_HANDLER_H
