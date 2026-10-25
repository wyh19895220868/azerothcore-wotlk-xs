#include "ItemGrowth.h"
#include "ItemGrowthMgr.h"
#include "ScriptMgr.h"
#include "Logging/Log.h"
#include "Config.h"
#include "Chat.h"
#include "Player.h"
#include "GameTime.h"
#include "Spell.h"
#include "Configuration/Config.h"
#include "World.h"
#include "Random.h"
#include "../../../src/server/scripts/Custom/CreatureMod/AsCreature.h"
#include "../../../src/server/scripts/Custom/ItemDevelopMgr/ItemDevelopMgr.h"
#include <algorithm>
#include <chrono>
#include "ModuleFormat.h"

ItemGrowth::ItemGrowth() :
    WorldScript("ItemGrowth"),
    _initialized(false),
    _loadTimer(0),
    _delayTime(1 * IN_MILLISECONDS), // 1秒延迟
    _expProcessTimer(0)              // 【性能优化】经验处理定时器
{
    // 注册脚本
}

void ItemGrowth::OnAfterConfigLoad(bool reload)
{
    // 只加载配置，不初始化模块
    // 注意：不在这里初始化，而是在OnUpdate中延迟加载
}

void ItemGrowth::OnUpdate(uint32 diff)
{
    // 如果已经初始化，处理定时任务
    if (_initialized)
    {
        // 【性能优化】定时处理累积的经验
        _expProcessTimer += diff;
        if (_expProcessTimer >= 2000)  // 每2秒处理一次
        {
            _expProcessTimer = 0;
            sItemGrowthMgr->ProcessAllPendingExp();
        }
        return;
    }

    // 累加时间
    _loadTimer += diff;

    // 当达到延迟时间时，加载模块
    if (_loadTimer >= _delayTime)
    {
        // 在服务器完全启动后初始化物品成长系统


        uint32 count = LoadModule();

        if (sItemGrowthMgr->IsEnabled())
        {
            LOG_INFO("server.loading", "→_模板_物品成长 加载完成√");
        }
        else
        {
        }


        _initialized = true;
    }
}

uint32 ItemGrowth::LoadModule()
{
    // 初始化模块
    sItemGrowthMgr->Initialize();

    if (!sItemGrowthMgr->IsEnabled())
        return 0;

    // 获取加载的数据条数
    uint32 templateCount = sItemGrowthMgr->GetTemplateCount();
    uint32 recordCount = sItemGrowthMgr->GetPlayerRecordCount();

    LOG_DEBUG("server.loading", ModuleOldFormat("ItemGrowth::LoadModule - 物品成长系统已加载完成，加载了 %s 条模板和 %s 条玩家记录", templateCount, recordCount).c_str());

    return templateCount + recordCount;
}

namespace
{
bool IsDevelopMapMatched(DevelopTemp const& config, Creature* killed)
{
    if (config.MapType < 0)
        return true;

    switch (config.MapType)
    {
        case 1:
            return config.MapEntry == killed->GetMapId();
        case 2:
            return config.MapEntry == killed->GetZoneId();
        case 3:
            return config.MapEntry == killed->GetAreaId();
        default:
            return false;
    }
}

bool IsDevelopEquipStateMatched(DevelopTemp const& config, Item* item)
{
    switch (config.UpRankType)
    {
        case 1:
            return !item->IsEquipped();
        case 2:
            return item->IsEquipped();
        case 3:
            return true;
        default:
            return false;
    }
}

uint32 RandomDevelopExp(uint32 minValue, uint32 maxValue)
{
    if (!minValue && !maxValue)
        return 0;

    return urand(std::min(minValue, maxValue), std::max(minValue, maxValue));
}
}

ItemGrowthPlayerScript::ItemGrowthPlayerScript() : PlayerScript("ItemGrowthPlayerScript")
{
}

void ItemGrowthPlayerScript::OnCreatureKill(Player* player, Creature* killed)
{
    if (!player || !killed)
        return;

    // 检查物品成长系统是否启用
    if (!sItemGrowthMgr->IsEnabled())
        return;

    if (DevelopConfigStore.empty())
        return;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item || !sItemGrowthMgr->CanItemGrow(item))
            continue;

        uint32 growthExp = GetCreatureGrowthExp(item, killed);
        if (growthExp > 0)
            DistributeGrowthExp(player, item, growthExp);
    }
}

// 处理装备变化事件
void ItemGrowthPlayerScript::OnEquip(Player* player, Item* item, uint8 /*bag*/, uint8 slot, bool /*update*/)
{
    if (!player || !item)
        return;

    // 检查物品成长系统是否启用
    if (!sItemGrowthMgr->IsEnabled())
        return;

    using namespace std::chrono;
    auto perfStart = high_resolution_clock::now();

    uint32 itemGuid = item->GetGUID().GetCounter();

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnEquip: 玩家 %s 装备物品 %s (GUID:%s) 到槽位 %s", player->GetName(), item->GetTemplate()->Name1, itemGuid, slot).c_str());

    // 不再向随机附魔字段写入/同步GUID，避免破坏随机附魔与链接校验

    // 检查物品是否可以成长
    bool canGrow = sItemGrowthMgr->CanItemGrow(item);
    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnEquip: 物品 %s 可成长状态: %s", item->GetTemplate()->Name1, canGrow ? "是" : "否").c_str());

    if (!canGrow)
        return;

    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

    if (record)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnEquip: 找到物品GUID %s 的成长记录，等级:%s 属性:'%s'", itemGuid, record->currentLevel, record->growthAttributes).c_str());

        // 关键修复：确保成长装备有唯一的seed
        // 这解决了聊天链接验证失败和客户端识别问题
        sItemGrowthMgr->EnsureItemUniqueSeed(player, item);

        // 修复：检查并修复0级记录或空属性记录
        bool wasFixed = sItemGrowthMgr->FixZeroLevelRecord(player, item, itemGuid);
        if (wasFixed)
        {
            LOG_INFO("module.itemgrowth", "OnEquip: 已修复0级记录，准备应用属性");
        }

        // 关键修改：成长系统不再应用属性，完全由幻境系统统一处理
        // 幻境系统会读取成长属性并应用完整的倍率值
        // 这样可以避免BASE_VALUE的不稳定性，确保倍率正确应用
        // sItemGrowthMgr->ApplyGrowthAura(player, itemGuid);  // 已禁用

        LOG_DEBUG("module.itemgrowth", "OnEquip: 成长属性将由幻境系统统一应用（完整倍率值）");
    }
    else
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnEquip: 物品GUID %s 没有成长记录", itemGuid).c_str());
    }

    auto perfEnd = high_resolution_clock::now();
    auto perfMs = duration_cast<milliseconds>(perfEnd - perfStart).count();
}

// 处理卸下装备事件（当物品从装备槽移动到背包时）
void ItemGrowthPlayerScript::OnAfterMoveItemFromInventory(Player* player, Item* item, uint8 bag, uint8 slot, bool /*update*/)
{
    if (!player || !item)
        return;

    // 检查物品成长系统是否启用
    if (!sItemGrowthMgr->IsEnabled())
        return;

    // 检查物品是否可以成长
    if (!sItemGrowthMgr->CanItemGrow(item))
        return;

    // 只处理从装备槽移出的情况
    if (bag != INVENTORY_SLOT_BAG_0 || slot < EQUIPMENT_SLOT_START || slot >= EQUIPMENT_SLOT_END)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnAfterMoveItemFromInventory: 物品不是从装备槽移除 (bag:%s, slot:%s)", bag, slot).c_str());
        return;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();

    // 检查物品现在是否还在装备槽
    // 如果物品现在在另一个装备槽（武器互换），不要移除属性
    bool stillEquipped = false;
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (equippedItem && equippedItem->GetGUID().GetCounter() == itemGuid)
        {
            stillEquipped = true;
            break;
        }
    }

    if (stillEquipped)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnAfterMoveItemFromInventory: 物品 %s 仍在装备槽（武器互换），跳过成长属性移除", itemGuid).c_str());
        return;
    }

    // 物品已离开装备槽，移除成长属性
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

    if (record)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnAfterMoveItemFromInventory: 物品 %s 已离开装备槽，移除成长属性", itemGuid).c_str());
        sItemGrowthMgr->RemoveGrowthAura(player, itemGuid);
    }
    else
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnAfterMoveItemFromInventory: 物品GUID %s 没有成长记录", itemGuid).c_str());
    }
}

// 处理玩家登录事件
void ItemGrowthPlayerScript::OnLogin(Player* player)
{
    auto loginStart = std::chrono::high_resolution_clock::now();

    if (!player)
        return;

    // 检查物品成长系统是否启用
    if (!sItemGrowthMgr->IsEnabled())
        return;

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnLogin: 玩家 %s 登录，开始应用所有装备的成长属性", player->GetName()).c_str());

    auto step1Start = std::chrono::high_resolution_clock::now();
    uint32 appliedCount = 0;
    uint32 totalEquipment = 0;

    // 遍历玩家所有装备槽位
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
        {
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnLogin: 槽位 %s 为空", slot).c_str());
            continue;
        }

        totalEquipment++;
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnLogin: 检查槽位 %s 的装备 %s (GUID:%s)", slot, item->GetTemplate()->Name1, item->GetGUID().GetCounter()).c_str());

        // 检查物品是否可以成长
        bool canGrow = sItemGrowthMgr->CanItemGrow(item);
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnLogin: 装备 %s 可成长状态: %s", item->GetTemplate()->Name1, canGrow ? "是" : "否").c_str());

        if (!canGrow)
            continue;

        uint32 itemGuid = item->GetGUID().GetCounter();

        // 不再向随机附魔字段写入/同步GUID，避免破坏随机附魔与链接校验

        ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

        if (record)
        {
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnLogin: 找到物品GUID %s 的成长记录，等级:%s 属性:'%s'", itemGuid, record->currentLevel, record->growthAttributes).c_str());

            // 关键修复：确保成长装备有唯一的seed
            // 这解决了聊天链接验证失败和客户端识别问题
            sItemGrowthMgr->EnsureItemUniqueSeed(player, item);

            // 修复：检查并修复0级记录或空属性记录
            bool wasFixed = sItemGrowthMgr->FixZeroLevelRecord(player, item, itemGuid);
            if (wasFixed)
            {
                LOG_INFO("module.itemgrowth", "OnLogin: 已修复0级记录，准备应用属性");
            }

            // 关键修改：登录时也不应用属性，完全由幻境系统统一处理
            // 幻境系统的OnLogin会应用完整的倍率值
            // sItemGrowthMgr->ApplyGrowthAura(player, itemGuid);  // 已禁用

            LOG_DEBUG("module.itemgrowth", "OnLogin: 成长属性将由幻境系统统一应用（完整倍率值）");
            appliedCount++;
        }
        else
        {
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnLogin: 物品GUID %s 没有成长记录", itemGuid).c_str());
        }
    }
    auto step1End = std::chrono::high_resolution_clock::now();
    auto step1Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step1End - step1Start).count();

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnLogin: 玩家 %s 登录完成，总装备 %s 件，应用成长属性 %s 件", player->GetName(), totalEquipment, appliedCount).c_str());
}

// 处理装备移除前的事件（用于处理装备替换）
// 【审计修复】此回调不再移除属性，统一由 OnAfterMoveItemFromInventory 处理
// 原因：两个回调都调用 RemoveGrowthAura 会导致属性双重移除
bool ItemGrowthPlayerScript::CanUnequipItem(Player* player, uint16 pos, bool swap)
{
    if (!player)
        return true;

    // 检查物品成长系统是否启用
    if (!sItemGrowthMgr->IsEnabled())
        return true;

    // 只处理装备槽位
    if (!player->IsEquipmentPos(pos))
        return true;

    uint8 slot = pos & 255;

    // 【审计修复】所有属性移除逻辑统一在 OnAfterMoveItemFromInventory 中处理
    // 这样可以：
    // 1. 避免重复移除属性（CanUnequipItem 和 OnAfterMoveItemFromInventory 都调用）
    // 2. 正确处理武器互换场景（物品仍在装备槽时不移除属性）
    // 3. 确保只有物品真正离开装备槽时才移除属性
    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CanUnequipItem: 槽位=%s swap=%s - 跳过，属性移除将在 OnAfterMoveItemFromInventory 中统一处理", slot, swap ? "true" : "false").c_str());

    return true; // 允许卸装备
}

// 【性能优化】处理玩家下线事件
void ItemGrowthPlayerScript::OnLogout(Player* player)
{
    if (!player || !sItemGrowthMgr->IsEnabled())
        return;

    // 玩家下线时，强制处理并保存所有累积的经验
    sItemGrowthMgr->FlushPlayerExp(player);

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("OnLogout: 玩家 %s 下线，已处理累积经验", player->GetName()).c_str());
}

// 处理角色删除事件
void ItemGrowthPlayerScript::OnDelete(ObjectGuid guid, uint32 /*accountId*/)
{
    if (!sItemGrowthMgr->IsEnabled())
        return;

    // 玩家删除角色时，删除该角色的所有成长记录
    uint32 playerGuid = guid.GetCounter();
    sItemGrowthMgr->DeletePlayerRecords(playerGuid);
}

uint32 ItemGrowthPlayerScript::GetCreatureGrowthExp(Item* item, Creature* killed)
{
    if (!item || !killed)
        return 0;

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);
    if (!record)
        return 0;

    uint32 itemId = item->GetEntry();
    uint32 nextRank = record->currentLevel + 1;
    uint32 creatureEntry = killed->GetEntry();
    bool isBoss = sAsCreature->IsCustomBoss(killed);
    uint32 totalExp = 0;

    for (DevelopTemp const& config : DevelopConfigStore)
    {
        if (config.ItemId != itemId || config.Rank != nextRank)
            continue;

        if (!IsDevelopEquipStateMatched(config, item))
            continue;

        if (IsDevelopMapMatched(config, killed))
        {
            totalExp += isBoss
                ? RandomDevelopExp(config.KillXpMin2, config.KillXpMax2)
                : RandomDevelopExp(config.KillXpMin1, config.KillXpMax1);
        }

        if (config.ToEntrySets.find(creatureEntry) != config.ToEntrySets.end())
            totalExp += RandomDevelopExp(config.KillXpMin3, config.KillXpMax3);
    }

    return totalExp;
}

void ItemGrowthPlayerScript::DistributeGrowthExp(Player* player, Item* item, uint32 totalExp)
{
    if (!player || !item || totalExp == 0)
        return;

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("DistributeGrowthExp: 玩家 %s 装备 %s 获得 %s 点成长经验", player->GetName(), item->GetEntry(), totalExp).c_str());

    sItemGrowthMgr->AccumulateItemExp(player, item, totalExp);
}

class ItemGrowthDevelopScrollScript : public AllItemScript
{
public:
    ItemGrowthDevelopScrollScript() : AllItemScript("ItemGrowthDevelopScrollScript") { }

    bool CanItemUse(Player* player, Item* castItem, SpellCastTargets const& targets) override
    {
        if (!player || !castItem || !sItemGrowthMgr->IsEnabled())
            return false;

        if (UseXpGroupStore.empty())
            return false;

        Item* targetItem = targets.GetItemTarget();
        if (!targetItem)
            return false;

        if (targetItem->IsEquipped())
        {
            ChatHandler(player->GetSession()).SendSysMessage("请把目标装备放入背包后再使用成长经验卷轴。");
            return false;
        }

        uint32 rewardExp = sItemDevelopMgr->GetUseItemDevelopRankXp(castItem->GetEntry(), targetItem->GetEntry());
        if (!rewardExp)
            return false;

        if (!sItemGrowthMgr->AddItemExp(player, targetItem, rewardExp))
            return false;

        player->DestroyItemCount(castItem->GetEntry(), 1, true);
        ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("装备成长获得 %s 点经验。", rewardExp).c_str());
        return true;
    }
};

// 添加脚本
void AddSC_ItemGrowth()
{
    new ItemGrowth();
    new ItemGrowthPlayerScript();
    new ItemGrowthDevelopScrollScript();
}
