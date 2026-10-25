#include "ItemEnhancement.h"
#include "ItemEnhancementMgr.h"
#include "Chat.h"
#include "Player.h"
#include "WorldPacket.h"
#include "GameTime.h"
#include "ScriptMgr.h"
#include "ObjectAccessor.h"
#include <unordered_map>
#include <chrono>
#include "ModuleFormat.h"

#define MODULE_ITEM_ATTRIBUTES

// 外部函数声明
extern void DeleteEnhancementRecord(uint32 itemGuid);

// ============================================================================
// 【架构重构】参考 mod-item-attributes 的稳定实现
// ============================================================================
//
// 问题根源：
// - ItemEnhancementMgr::PreloadPlayerEnhancementRecords 在登录时崩溃
// - CheckEquipmentChanges 周期性访问 Manager 崩溃
// - 崩溃地址始终在 0xC46XXX 区域，RCX=0, RDX=0 空指针
//
// 新架构（完全参考 ItemAttributesEvents）：
// 1. OnLogin 几乎不做任何操作（只发消息）
// 2. OnEquip 使用本地 map 跟踪，使用 isBeingLoaded() 检查
// 3. 完全移除 OnUpdate 周期性检查（事件驱动）
// 4. 不依赖复杂的 Manager 加载方法
//
// ============================================================================

// 装备跟踪数据（使用本地 map，不依赖全局数据结构）
static std::unordered_map<uint64, std::unordered_map<uint8, uint64>> _equippedEnhancementItems;

// 物品强化处理脚本
class ItemEnhancement_PlayerScript : public PlayerScript
{
public:
    ItemEnhancement_PlayerScript() : PlayerScript("ItemEnhancement_PlayerScript") { }

    void OnLogin(Player* player) override
    {
        // 【参考 ItemAttributesEvents::OnLogin】
        // 登录时不做任何加载操作，只发送可选消息

        if (!sItemEnhancementMgr->IsEnabled() || !player)
            return;

        // 可选：发送登录消息
        bool loginMessage = sConfigMgr->GetOption<bool>("ItemEnhancement.LoginMessage", false);
        if (loginMessage && player->GetSession())
        {
            std::string msg = "ITEMENHANCE\tLOGIN_COMPLETED";
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, LANG_UNIVERSAL, player, player, msg);
            player->GetSession()->SendPacket(&data);
        }

        LOG_DEBUG("module.itemenhancement", ModuleOldFormat("OnLogin: Player %s - Enhancement system ready (no preloading)", player->GetGUID().ToString()).c_str());
    }

    void OnLogout(Player* player) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player)
            return;

        // 清理玩家的装备跟踪数据
        uint64 playerGuid = player->GetGUID().GetCounter();
        _equippedEnhancementItems.erase(playerGuid);

        // 清理内存中的应用记录
        sItemEnhancementMgr->ClearPlayerAppliedEnhancements(player);

        LOG_DEBUG("module.itemenhancement", ModuleOldFormat("OnLogout: Player %s - Cleaned up tracking data", player->GetGUID().ToString()).c_str());
    }

    // 【完全移除 OnUpdate】
    // 不再使用周期性检查，完全事件驱动
};

// 装备事件处理脚本
class ItemEnhancement_EquipScript : public PlayerScript
{
public:
    ItemEnhancement_EquipScript() : PlayerScript("ItemEnhancement_EquipScript") { }

    void OnEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool /*update*/) override
    {
        // 【完全参考 ItemAttributesEvents::OnEquip 的实现】

        if (!sItemEnhancementMgr->IsEnabled() || !player || !item)
            return;

        // 只处理装备槽位
        if (bag != INVENTORY_SLOT_BAG_0 || slot >= EQUIPMENT_SLOT_END)
            return;

#ifndef MODULE_ITEM_ATTRIBUTES
        try
        {
            // 【安全检查】确保 Session 存在
            WorldSession* session = player->GetSession();
            if (!session)
            {
                LOG_WARN("module.itemenhancement", ModuleOldFormat("OnEquip: Player %s has no session", player->GetGUID().ToString()).c_str());
                return;
            }

            uint64 playerGuid = player->GetGUID().GetCounter();
            uint64 itemGuid = item->GetGUID().GetCounter();

            // 检查该槽位是否已有装备（需要先移除旧装备的强化）
            auto& playerEquipMap = _equippedEnhancementItems[playerGuid];
            auto it = playerEquipMap.find(slot);
            if (it != playerEquipMap.end())
            {
                uint64 oldItemGuid = it->second;

                // 移除旧装备的强化效果
                // 由于旧物品可能已经不在这个槽位了，我们直接清理该槽位的强化效果
                sItemEnhancementMgr->ClearSlotEnhancements(player, slot);

                LOG_DEBUG("module.itemenhancement", ModuleOldFormat("OnEquip: Cleared old item %s enhancement from slot %s for player %s", oldItemGuid, slot, player->GetGUID().ToString()).c_str());
            }

            // 记录新装备
            playerEquipMap[slot] = itemGuid;

            // 应用新装备的强化效果
            EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);
            if (record && record->level > 0)
            {
                // 设置可见槽位
                if (item->IsEquipped())
                    player->SetVisibleItemSlot(item->GetSlot(), item);

                sItemEnhancementMgr->ApplyOfficialItemEnhancement(player, item, record->level);

                LOG_DEBUG("module.itemenhancement", ModuleOldFormat("OnEquip: Applied enhancement level %s for item %s in slot %s for player %s", record->level, itemGuid, slot, player->GetGUID().ToString()).c_str());
            }

            // 【关键优化】登录加载阶段不做全量刷新
            // 参考 ItemAttributesEvents 的实现：
            // 使用 isBeingLoaded() 检查，避免登录时每件装备都刷新属性
            if (!player->isBeingLoaded())
            {
                player->UpdateAllStats();
                player->UpdateAttackPowerAndDamage();
                player->UpdateAttackPowerAndDamage(true);
                player->UpdateMaxHealth();
                player->UpdateMaxPower(POWER_MANA);
            }
        }
        catch (...)
        {
            LOG_ERROR("module.itemenhancement", ModuleOldFormat("OnEquip: Exception for player %s item %s slot %s", player->GetGUID().ToString(), item->GetGUID().GetCounter(), slot).c_str());
        }
#endif
    }

    void OnAfterMoveItemFromInventory(Player* player, Item* item, uint8 bag, uint8 slot, bool /*update*/) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player || !item)
            return;

#ifndef MODULE_ITEM_ATTRIBUTES
        try
        {
            // 如果物品从装备槽移出，移除强化效果
            if (bag == INVENTORY_SLOT_BAG_0 && slot < EQUIPMENT_SLOT_END)
            {
                // 【安全检查】
                if (!player->GetSession() || !player->IsInWorld())
                {
                    LOG_WARN("module.itemenhancement", ModuleOldFormat("OnAfterMoveItemFromInventory: Player %s not valid", player->GetGUID().ToString()).c_str());
                    return;
                }

                uint64 itemGuid = item->GetGUID().GetCounter();
                uint64 playerGuid = player->GetGUID().GetCounter();

                // 移除强化效果
                sItemEnhancementMgr->RemoveOfficialItemEnhancement(player, item);

                // 从跟踪map中移除
                auto& playerEquipMap = _equippedEnhancementItems[playerGuid];
                playerEquipMap.erase(slot);

                // 刷新玩家属性
                player->UpdateAllStats();
                player->UpdateAttackPowerAndDamage();
                player->UpdateAttackPowerAndDamage(true);
                player->UpdateMaxHealth();
                player->UpdateMaxPower(POWER_MANA);

                LOG_DEBUG("module.itemenhancement", ModuleOldFormat("OnAfterMoveItemFromInventory: Removed enhancement for item %s from slot %s", itemGuid, slot).c_str());
            }
        }
        catch (...)
        {
            LOG_ERROR("module.itemenhancement", ModuleOldFormat("OnAfterMoveItemFromInventory: Exception for player %s item %s", player->GetGUID().ToString(), item->GetGUID().GetCounter()).c_str());
        }
#endif
    }
};

// 物品移除事件处理脚本
class ItemEnhancement_AllItemScript : public AllItemScript
{
public:
    ItemEnhancement_AllItemScript() : AllItemScript("ItemEnhancement_AllItemScript") { }

    bool CanItemRemove(Player* player, Item* item) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player || !item)
            return true; // 允许移除

#ifdef MODULE_ITEM_ATTRIBUTES
        // 物品属性系统启用时，由物品属性模块统一处理属性移除
        return true;
#else
        try
        {
            // 【安全检查】
            if (!player->GetSession() || !player->IsInWorld())
                return true; // 玩家状态异常，允许移除但不处理强化

            // 检查物品是否从装备槽移除
            uint8 slot = item->GetSlot();
            if (slot >= EQUIPMENT_SLOT_END)
                return true; // 不是装备槽，允许移除

            // 【修复】CanItemRemove 不再移除强化效果
            // 强化效果的移除统一由 OnAfterMoveItemFromInventory 处理
            // 避免同一事件触发两次导致重复移除属性

            return true; // 允许移除物品
        }
        catch (...)
        {
            LOG_ERROR("module.itemenhancement", ModuleOldFormat("CanItemRemove: Exception for player %s item %s, allowing removal", player->GetGUID().ToString(), item->GetGUID().GetCounter()).c_str());
            return true; // 异常情况下仍然允许移除
        }
#endif
    }
};

// 物品脚本（处理物品删除）
class ItemEnhancement_ItemScript : public ItemScript
{
public:
    ItemEnhancement_ItemScript() : ItemScript("ItemEnhancement_ItemScript") { }

    bool OnRemove(Player* player, Item* item) override
    {
        if (!sItemEnhancementMgr->IsEnabled() || !player || !item)
            return false;

        // 【修复】只有在物品真正被销毁时才删除强化记录
        // OnRemove 在交易、邮寄、移动背包等场景也会触发，不应该删除记录
        // 检查物品是否真的要被销毁（通过检查物品状态）

        // 如果物品正在被交易或邮寄，不删除记录
        // 物品被销毁时，通常是通过 Player::DestroyItem 调用的
        // 此时物品的 owner 仍然是当前玩家

        uint32 itemGuid = item->GetGUID().GetCounter();
        EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);

        // 只有当物品有强化记录，且物品确实要被永久删除时才删除记录
        // 注意：这里我们保守处理，只记录日志，不主动删除
        // 强化记录的清理应该通过定期维护任务或GM命令来处理孤儿记录
        if (record)
        {
            LOG_DEBUG("module.itemenhancement", ModuleOldFormat("ItemEnhancement: OnRemove triggered for item GUID: %s, level: %s - record preserved for safety", itemGuid, record->level).c_str());
        }

        // 返回false表示不阻止物品移除
        return false;
    }
};

// 注册脚本
void AddSC_ItemEnhancementScript()
{
    new ItemEnhancement_PlayerScript();
    new ItemEnhancement_EquipScript();
    new ItemEnhancement_AllItemScript();
    new ItemEnhancement_ItemScript();
}
