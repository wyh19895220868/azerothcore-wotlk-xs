#include "ItemAttributesEvents.h"
#include "ItemAttributesLoader.h"
#include "ItemAttributesEffects.h"
#include "ItemAttributesDisplay.h"
#include "ItemAttributesGenerator.h"
#include "ItemAttributesDBHelper.h"
#include "Configuration/Config.h"
#include "Logging/Log.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include <chrono>
#include <limits>
#include "ModuleFormat.h"

ItemAttributesEvents::ItemAttributesEvents() :
    PlayerScript("ItemAttributesEvents"),
    ItemScript("ItemAttributesEvents")
{
}

void ItemAttributesEvents::OnGiveXP(Player* player, uint32& amount, Unit*)
{
    if (!player || !amount || !sHelper->GetBoolean(22011))
        return;

    AsInt64 bonus = player->GetAsStatBonus(static_cast<ItemModType>(400));
    if (bonus <= 0)
        return;

    AsInt64 result = AsInt64(amount) + AsInt64(amount) * bonus / 100;
    amount = AsInt64ToUInt32Clamp(result, std::numeric_limits<uint32>::max());
}

bool ItemAttributesEvents::CanApplyItemTemplateStats(Player* player, Item* item, ItemTemplate const*, uint8, bool apply)
{
    if (!player || !item || item->GetGUID().IsEmpty())
        return true;

    uint64 playerGuid = player->GetGUID().GetCounter();
    uint64 itemGuid = item->GetGUID().GetCounter();

    if (!apply)
    {
        std::lock_guard<std::mutex> lock(_equippedItemsMutex);
        auto playerItr = _templateStatsAppliedItems.find(playerGuid);
        if (playerItr == _templateStatsAppliedItems.end() || playerItr->second.erase(itemGuid) == 0)
            return false;

        if (playerItr->second.empty())
            _templateStatsAppliedItems.erase(playerItr);
        return true;
    }

    bool replaceTemplateStats = false;
    if (sHelper->GetBoolean(22011) && sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
    {
        if (auto data = ItemAttributesDBHelper::LoadItemAttributes(itemGuid))
            replaceTemplateStats = !data->baseAttributeIds.empty();
    }

    std::lock_guard<std::mutex> lock(_equippedItemsMutex);
    if (replaceTemplateStats)
    {
        auto playerItr = _templateStatsAppliedItems.find(playerGuid);
        if (playerItr != _templateStatsAppliedItems.end())
        {
            playerItr->second.erase(itemGuid);
            if (playerItr->second.empty())
                _templateStatsAppliedItems.erase(playerItr);
        }
        return false;
    }

    _templateStatsAppliedItems[playerGuid].insert(itemGuid);
    return true;
}

void ItemAttributesEvents::OnLogin(Player* player)
{
    auto loginStart = std::chrono::high_resolution_clock::now();

    if (!player || !sHelper->GetBoolean(22011) || !sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
        return;

    // 【性能优化】登录时不再刷新玩家属性
    // 原因：
    // 1. OnEquip 已经为每个装备应用了属性（已优化为跳过Update）
    // 2. 幻境系统的 OnLogin 会在最后统一刷新所有属性
    // 3. 避免多个系统重复调用 UpdateAllStats，节省约 39ms
    //
    // 如果需要单独刷新，可以在配置中启用 ItemAttributes.ForceRefreshOnLogin

    auto step1Start = std::chrono::high_resolution_clock::now();
    bool forceRefresh = sConfigMgr->GetOption<bool>("ItemAttributes.ForceRefreshOnLogin", false);
    if (forceRefresh)
    {
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateSpellDamageAndHealingBonus();
    }
    auto step1End = std::chrono::high_resolution_clock::now();
    auto step1Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step1End - step1Start).count();

    // 可以在这里添加登录消息
    if (sConfigMgr->GetOption<bool>("ItemAttributes.LoginMessage", false))
    {
        player->GetSession()->SendAreaTriggerMessage("物品追加属性系统已加载");
    }

    auto loginEnd = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loginEnd - loginStart).count();

    // 【性能诊断】记录登录处理耗时
    if (totalDuration > 100)
    {
        LOG_WARN("module.itemattributes.perf", ModuleOldFormat("OnLogin 耗时过长: %sms (玩家=%s, 强制刷新=%s)", totalDuration, player->GetName(), forceRefresh).c_str());
    }
    else if (sConfigMgr->GetOption<bool>("ItemAttributes.Debug.Performance", false))
    {
        LOG_DEBUG("module.itemattributes.perf", ModuleOldFormat("OnLogin 完成: %sms (强制刷新=%sms)", totalDuration, step1Duration).c_str());
    }
}

void ItemAttributesEvents::OnLogout(Player* player)
{
    if (!player)
        return;

    // 【根本性修复】线程安全地清理该玩家的装备跟踪数据
    uint64 playerGuid = player->GetGUID().GetCounter();
    {
        std::lock_guard<std::mutex> lock(_equippedItemsMutex);
        _equippedItems.erase(playerGuid);
        _templateStatsAppliedItems.erase(playerGuid);
    }

    // 不在登出流程做全表孤立属性清理。
    // 登出时核心正在保存 item_instance，清理线程读取/删除同表会与保存事务竞争，
    // 造成 [1213] Deadlock。孤立数据清理保留给 GM 手动命令执行。
}

void ItemAttributesEvents::OnDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid)
{
    if (!sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
        return;

    // 当角色被删除时，清理该角色所有物品的属性数据
    // 【审计修复】使用传入的事务来确保数据一致性
    if (trans)
    {
        trans->Append(ModuleOldFormat("DELETE FROM `物品属性_数据` WHERE `物品GUID` IN "
            "(SELECT `guid` FROM `item_instance` WHERE `owner_guid` = %s)", guid));
    }
    else
    {
        // 如果没有事务，使用异步执行
        CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `物品属性_数据` WHERE `物品GUID` IN "
            "(SELECT `guid` FROM `item_instance` WHERE `owner_guid` = %s)", guid).c_str());
    }
    ItemAttributesDBHelper::FlushCache();
}

void ItemAttributesEvents::OnEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update)
{
    if (!player || !item || !sHelper->GetBoolean(22011) || !sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
        return;

    // 只处理真正的装备槽位（背包装备、银行等忽略），避免不必要的属性应用
    // 【性能优化】提前检查，避免不必要的GUID获取
    if (bag != INVENTORY_SLOT_BAG_0 || slot >= EQUIPMENT_SLOT_END)
        return;

    // 【性能诊断】记录开始时间
    using namespace std::chrono;
    auto perfStart = high_resolution_clock::now();

    // 【关键修复】检查物品是否已完全初始化
    // 在玩家登录时加载装备的过程中，Item对象的m_uint32Values可能还未初始化
    // 此时调用GetGUID()会触发"GetGuidValue called on uninitialized object"错误
    ObjectGuid itemGuidObj = item->GetGUID();
    if (itemGuidObj.IsEmpty())
    {
        // 物品尚未初始化，跳过此次处理
        // 注意：这不是错误，只是初始化顺序问题，后续会再次触发OnEquip
        return;
    }

    uint64 playerGuid = player->GetGUID().GetCounter();
    uint32 itemEntry = item->GetEntry();
    uint64 itemGuid = itemGuidObj.GetCounter();

    // 【性能诊断】是否在加载状态
    bool isLoading = player->isBeingLoaded();
    if (sConfigMgr->GetOption<bool>("ItemAttributes.Debug.Performance", false))
    {
        LOG_DEBUG("module.itemattributes.perf", ModuleOldFormat("OnEquip: 玩家=%s 物品=%s 槽位=%s 加载中=%s", player->GetName(), itemEntry, slot, isLoading).c_str());
    }

    // 【根本性修复】线程安全地检查和更新装备映射
    uint64 oldItemGuid = 0;
    bool hasOldItem = false;
    {
        std::lock_guard<std::mutex> lock(_equippedItemsMutex);
        auto& playerEquipMap = _equippedItems[playerGuid];
        auto it = playerEquipMap.find(slot);
        if (it != playerEquipMap.end())
        {
            oldItemGuid = it->second;
            hasOldItem = true;
        }
        // 记录新装备
        playerEquipMap[slot] = itemGuid;
    }

    // 如果有旧装备，移除其属性效果
    if (hasOldItem)
    {
        // 通过 GUID 查找旧物品
        // 注意：此时旧物品可能已经不在装备槽了，可能在背包中
        // 我们需要遍历玩家的所有物品来查找
        Item* oldItem = nullptr;
        // 先检查背包
        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            if (Item* bagItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                // 【关键修复】检查物品是否已完全初始化
                ObjectGuid bagItemGuid = bagItem->GetGUID();
                if (!bagItemGuid.IsEmpty() && bagItemGuid.GetCounter() == oldItemGuid)
                {
                    oldItem = bagItem;
                    break;
                }
            }
        }

        if (sItemAttributesEffects)
        {
            if (oldItem)
                sItemAttributesEffects->RemoveItemAttributeEffects(player, oldItem);
            else
                sItemAttributesEffects->RemoveItemAttributeEffectsByGuid(player, oldItemGuid);
        }
    }

    // 【修复】移除重复代码：playerEquipMap[slot] = itemGuid 已在上面的lock作用域中执行

    // 【关键修复】添加空指针检查和安全性验证
    // 应用新装备的属性
    if (sItemAttributesEffects)
    {
        sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
    }

    // 刷新玩家属性交由 ItemAttributesEffects 内部统一合并处理，
    // 避免每次换装都对全身装备做“移除→UpdateAllStats→重加”的 O(装备数) 操作。

    auto perfEnd = high_resolution_clock::now();
    auto perfMs = duration_cast<milliseconds>(perfEnd - perfStart).count();

    // 【性能诊断】如果耗时超过阈值，记录警告
    if (perfMs > 50)  // 超过50ms记录警告
    {
        LOG_WARN("module.itemattributes.perf", ModuleOldFormat("OnEquip 耗时过长: %sms (玩家=%s, 物品=%s, 槽位=%s, 加载中=%s)", perfMs, player->GetName(), itemEntry, slot, isLoading).c_str());
    }
    else if (sConfigMgr->GetOption<bool>("ItemAttributes.Debug.Performance", false))
    {
        LOG_DEBUG("module.itemattributes.perf", ModuleOldFormat("OnEquip 完成: %sms", perfMs).c_str());
    }
}

void ItemAttributesEvents::OnAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* item)
{
    if (!player || !sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
        return;

    // 这个钩子在装备槽的可见物品改变时触发
    // 穿戴装备时：item != nullptr
    // 卸下装备时：item == nullptr

    if (item == nullptr)
    {
        uint64 playerGuid = player->GetGUID().GetCounter();
        uint64 oldItemGuid = 0;
        bool found = false;

        // 【根本性修复】线程安全地从映射表中获取该槽位之前的物品GUID
        {
            std::lock_guard<std::mutex> lock(_equippedItemsMutex);
            auto playerIt = _equippedItems.find(playerGuid);
            if (playerIt != _equippedItems.end())
            {
                auto& playerEquipMap = playerIt->second;
                auto slotIt = playerEquipMap.find(slot);
                if (slotIt != playerEquipMap.end())
                {
                    oldItemGuid = slotIt->second;
                    found = true;
                    // 从映射表中移除
                    playerEquipMap.erase(slotIt);
                }
            }
        }

        if (found)
        {
            // 【关键修复】添加空指针检查
            // 直接根据GUID移除属性，不需要查找Item对象
            if (sItemAttributesEffects)
            {
                sItemAttributesEffects->RemoveItemAttributeEffectsByGuid(player, oldItemGuid);
            }
        }
    }
}

// OnDummyEffect 不是 ItemScript 的虚函数，已移除

bool ItemAttributesEvents::OnQuestAccept(Player* player, Item* item, Quest const* quest)
{
    // 这里可以处理物品接受任务时的逻辑
    return false; // 返回true表示处理了事件，返回false表示继续处理
}

bool ItemAttributesEvents::OnUse(Player* player, Item* item, SpellCastTargets const& targets)
{
    // 这里可以处理物品使用时的逻辑
    return false; // 返回true表示处理了事件，返回false表示继续处理
}

bool ItemAttributesEvents::OnExpire(Player* player, ItemTemplate const* proto)
{
    // 这里可以处理物品过期时的逻辑
    return false; // 返回true表示处理了事件，返回false表示继续处理
}

bool ItemAttributesEvents::OnRemove(Player* player, Item* item)
{
    // 注意：这个事件不会在物品删除时触发
    // 物品删除时的清理由 ItemAttributesGlobalScript::OnItemDelFromDB 处理
    return false;
}

void ItemAttributesEvents::OnGossipSelect(Player* player, Item* item, uint32 sender, uint32 action)
{
    // 这里可以处理物品对话选择时的逻辑
}
