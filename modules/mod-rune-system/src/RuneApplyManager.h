#ifndef RUNE_APPLY_MANAGER_H
#define RUNE_APPLY_MANAGER_H

#include "Common.h"
#include "RuneTypes.h"
#include "RuneSystem.h"
#include "RuneEffectHandler.h"
#include "RuneEquipHandler.h"
#include "Player.h"
#include <vector>
#include <map>
#include "ModuleFormat.h"

// 符文应用管理器
class RuneApplyManager
{
public:
    // 为玩家应用所有装备上的符文效果
    // 前置条件：调用方已通过 LoadRuneStorageFromDB(characterGuid) 一次性加载该角色全部符文数据
    // （该函数会同时填充按物品索引的缓存），这里不再逐件查库。
    static void ApplyAllPlayerRunes(Player* player)
    {
        if (!player)
            return;

        // 只应用已装备物品上的符文效果
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            // 应用该物品的符文效果
            ApplyItemRunes(player, item->GetGUID().GetCounter());
        }
    }

    // 移除玩家所有符文效果
    static void RemoveAllPlayerRunes(Player* player)
    {
        if (!player)
            return;

        // 只移除已装备物品上的符文效果
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            uint32 itemGuid = item->GetGUID().GetCounter();

            // 移除该物品的符文效果
            RemoveItemRunes(player, itemGuid);
        }
    }

    // 应用单个物品的符文效果
    static void ApplyItemRunes(Player* player, uint32 itemGuid)
    {
        if (!player)
            return;

        // 获取物品的符文存储
        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);
        if (!storage)
        {
            return;
        }

        // 解析符文ID列表（注意：这里存储的是符文物品ID，不是符文记录ID）
        auto runeItemIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

        for (size_t i = 0; i < runeItemIds.size(); ++i)
        {
            uint32 runeItemId = runeItemIds[i];

            if (runeItemId == 0)
            {
                continue;
            }

            // 通过物品ID获取符文信息
            const CustomRuneInfo* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeItemId);
            if (!runeInfo)
            {
                continue;
            }

            RuneEffectHandler::ApplyRuneEffects(player, *runeInfo);
        }
    }

    // 移除单个物品的符文效果
    static void RemoveItemRunes(Player* player, uint32 itemGuid)
    {
        if (!player)
            return;

        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);
        if (!storage)
        {
            return;
        }

        // 解析符文ID列表（注意：这里存储的是符文物品ID，不是符文记录ID）
        auto runeItemIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

        for (size_t i = 0; i < runeItemIds.size(); ++i)
        {
            uint32 runeItemId = runeItemIds[i];

            if (runeItemId == 0)
            {
                continue;
            }

            // 通过物品ID获取符文信息
            const CustomRuneInfo* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeItemId);
            if (!runeInfo)
            {
                continue;
            }

            RuneEffectHandler::RemoveRuneEffects(player, *runeInfo);
        }
    }

    // 通过符文物品ID装备符文（最简化方案）
    static bool EquipRuneByItemId(Player* player, uint32 itemGuid, uint32 slotId, uint32 runeItemId)
    {
        if (!player)
            return false;

        uint32 characterGuid = player->GetGUID().GetCounter();

        // 通过物品ID查找符文记录ID（使用正确的列名 'id' 而不是 '符文ID'）
        QueryResult result = WorldDatabase.Query(ModuleOldFormat("SELECT id FROM _模板_物品符文 WHERE 物品ID = %s", runeItemId).c_str());

        if (!result)
        {
            LOG_ERROR("rune.apply", ModuleOldFormat("符文物品ID %s 未配置", runeItemId).c_str());
            return false;
        }

        Field* fields = result->Fetch();
        uint32 runeId = fields[0].Get<uint32>();

        return EquipRune(player, itemGuid, slotId, runeId);
    }

    // 装备符文时调用（使用符文记录ID）
    static bool EquipRune(Player* player, uint32 itemGuid, uint32 slotId, uint32 runeId)
    {
        if (!player)
            return false;

        uint32 characterGuid = player->GetGUID().GetCounter();

        // 获取符文信息
        const CustomRuneInfo* runeInfo = sRuneSystem->GetRuneInfo(runeId);
        if (!runeInfo)
        {
            LOG_ERROR("rune.apply", ModuleOldFormat("符文ID %s 不存在", runeId).c_str());
            return false;
        }

        // 获取物品信息
        Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, itemGuid));
        if (!item)
        {
            LOG_ERROR("rune.apply", ModuleOldFormat("物品GUID %s 不存在", itemGuid).c_str());
            return false;
        }

        // 检查符文是否可以装备到该物品
        uint32 itemId = item->GetEntry();
        if (!RuneEquipHandler::CanEquipRuneToSlot(runeInfo->runeSlot, slotId, runeInfo->itemId, itemId))
        {
            LOG_ERROR("rune.apply", ModuleOldFormat("符文 %s 不能装备到物品 %s 的插槽 %s", runeId, itemId, slotId).c_str());
            return false;
        }

        // 装备符文到插槽（传入符文物品ID，即runeInfo->itemId）
        uint32 runeItemId = runeInfo->itemId;
        if (runeItemId == 0)
        {
            LOG_ERROR("rune.apply", ModuleOldFormat("符文 %s 的物品ID为0", runeId).c_str());
            return false;
        }

        if (!RuneEquipHandler::EquipRuneToSlot(itemGuid, slotId, runeItemId, characterGuid))
        {
            return false;
        }

        // 如果物品已装备，立即应用效果
        if (item->IsEquipped())
        {
            RuneEffectHandler::ApplyRuneEffects(player, *runeInfo);
        }

        // 重新加载符文数据
        sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);
        sRuneSystem->LoadRuneStorageFromDB(characterGuid);

        LOG_INFO("rune.apply", ModuleOldFormat("玩家 %s 在物品 %s 的插槽 %s 装备了符文物品 %s", characterGuid, itemGuid, slotId, runeItemId).c_str());

        return true;
    }

    // 卸载符文时调用
    static bool UnequipRune(Player* player, uint32 itemGuid, uint32 slotId)
    {
        if (!player)
            return false;

        uint32 characterGuid = player->GetGUID().GetCounter();

        // 获取当前装备的符文ID（注意：这里获取的是符文物品ID，不是符文记录ID）
        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);
        if (!storage)
            return false;

        uint32 currentRuneItemId = RuneEquipHandler::GetEquippedRuneId(storage->slotIds, storage->runeIds, slotId);
        if (currentRuneItemId == 0)
        {
            return false; // 插槽是空的
        }

        // 通过物品ID获取符文信息（修复：使用GetRuneInfoByItemId而不是GetRuneInfo）
        const CustomRuneInfo* runeInfo = sRuneSystem->GetRuneInfoByItemId(currentRuneItemId);

        // 移除符文
        if (!RuneEquipHandler::RemoveRuneFromSlot(itemGuid, slotId))
        {
            return false;
        }

        // 如果物品已装备且符文信息存在，移除效果
        Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, itemGuid));
        if (item && item->IsEquipped() && runeInfo)
        {
            RuneEffectHandler::RemoveRuneEffects(player, *runeInfo);
        }

        // 重新加载符文数据
        sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);
        sRuneSystem->LoadRuneStorageFromDB(characterGuid);

        LOG_INFO("rune.apply", ModuleOldFormat("玩家 %s 从物品 %s 的插槽 %s 卸载了符文物品 %s", characterGuid, itemGuid, slotId, currentRuneItemId).c_str());

        return true;
    }
};

#endif // RUNE_APPLY_MANAGER_H
