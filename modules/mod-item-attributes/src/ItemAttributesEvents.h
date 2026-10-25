#ifndef ITEM_ATTRIBUTES_EVENTS_H
#define ITEM_ATTRIBUTES_EVENTS_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Item.h"
#include <mutex>  // 【根本性修复】添加线程安全支持
#include <set>

// 物品属性事件处理器
class ItemAttributesEvents : public PlayerScript, public ItemScript
{
public:
    ItemAttributesEvents();

    // PlayerScript 重写的虚函数
    void OnLogin(Player* player) override;
    void OnLogout(Player* player) override;
    void OnDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override;
    void OnGiveXP(Player* player, uint32& amount, Unit* victim) override;
    bool CanApplyItemTemplateStats(Player* player, Item* item, ItemTemplate const* proto, uint8 slot, bool apply) override;

    // 装备事件 - 重写 PlayerScript 的虚函数
    void OnEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override;
    void OnAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* item) override;

    // ItemScript 重写的虚函数
    bool OnQuestAccept(Player* player, Item* item, Quest const* quest) override;
    bool OnUse(Player* player, Item* item, SpellCastTargets const& targets) override;
    bool OnExpire(Player* player, ItemTemplate const* proto) override;
    bool OnRemove(Player* player, Item* item) override;
    void OnGossipSelect(Player* player, Item* item, uint32 sender, uint32 action) override;

private:
    // 存储每个玩家的装备槽位对应的物品GUID
    // Key: PlayerGUID, Value: map<slot, itemGUID>
    std::map<uint64, std::map<uint8, uint64>> _equippedItems;

    // 记录实际应用过 item_template Stat 槽的物品，保证移除时与应用时完全对称。
    std::map<uint64, std::set<uint64>> _templateStatsAppliedItems;

    // 【根本性修复】保护 _equippedItems 的互斥锁
    std::mutex _equippedItemsMutex;
};

#endif // ITEM_ATTRIBUTES_EVENTS_H
