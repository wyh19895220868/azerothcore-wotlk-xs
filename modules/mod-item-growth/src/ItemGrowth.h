#ifndef MODULE_ITEM_GROWTH_H
#define MODULE_ITEM_GROWTH_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "ItemGrowthMgr.h"
#include "World.h"
#include "GameTime.h"

// 世界脚本类，使用WorldScript的OnUpdate钩子函数实现延迟加载
class ItemGrowth : public WorldScript
{
public:
    ItemGrowth();

    // WorldScript
    void OnUpdate(uint32 diff) override;
    void OnAfterConfigLoad(bool reload) override;

private:
    bool _initialized;     // 是否已初始化
    uint32 _loadTimer;     // 计时器
    uint32 _delayTime;     // 延迟时间
    uint32 _expProcessTimer;  // 【性能优化】经验处理定时器

    // 加载模块
    uint32 LoadModule();
};

// 玩家脚本类，处理杀怪获取成长经验
class ItemGrowthPlayerScript : public PlayerScript
{
public:
    ItemGrowthPlayerScript();

    // PlayerScript
    void OnCreatureKill(Player* player, Creature* killed) override;
    void OnEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override;
    void OnAfterMoveItemFromInventory(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override;
    void OnLogin(Player* player) override;
    void OnLogout(Player* player) override;  // 【性能优化】玩家下线时保存累积经验
    void OnDelete(ObjectGuid guid, uint32 accountId) override;

    // 处理装备移除前的事件（用于处理装备替换）
    bool CanUnequipItem(Player* player, uint16 pos, bool swap) override;

private:
    // 获取生物成长经验
    uint32 GetCreatureGrowthExp(Item* item, Creature* killed);

    // 为玩家装备分配成长经验
    void DistributeGrowthExp(Player* player, Item* item, uint32 totalExp);
};

#endif // MODULE_ITEM_GROWTH_H
