#ifndef MOD_ITEM_SKILLS_EVENTS_H
#define MOD_ITEM_SKILLS_EVENTS_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Item.h"
#include <map>

// 前向声明
class SpellCastTargets;

// 前向声明辅助函数（用于登录时）
void ForceClientRefreshItemTemplate(Player* player, uint32 itemId, Item* item);

// 物品技能事件处理类
class ItemSkillsEvents : public PlayerScript
{
public:
    ItemSkillsEvents() : PlayerScript("ItemSkillsEvents") {}

    // 玩家登录时加载技能配置
    void OnLogin(Player* player) override;

    // 玩家登出时清理内存数据
    void OnLogout(Player* player) override;

    // 装备物品时
    void OnEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override;

    // 物品从背包移走时（包括卸下装备）
    void OnAfterMoveItemFromInventory(Player* player, Item* it, uint8 bag, uint8 slot, bool update) override;

private:
    // 初始化物品技能
    void InitializeItemSkills(Player* player, Item* item);

    // 应用装备效果
    void ApplyEquipEffects(Player* player, Item* item);

    // 移除装备效果
    void RemoveEquipEffects(Player* player, Item* item);

    // 装备跟踪映射 [玩家GUID][装备槽位] = 物品GUID
    static std::map<uint64, std::map<uint8, uint64>> _equippedItems;
};

// 物品技能使用触发处理类（使用AllItemScript全局拦截）
class ItemSkillsItemScript : public AllItemScript
{
public:
    ItemSkillsItemScript() : AllItemScript("ItemSkillsItemScript") {}

    // 使用物品时触发（全局拦截）
    bool CanItemUse(Player* player, Item* item, SpellCastTargets const& targets) override;
};

// 物品技能单位事件处理类
class ItemSkillsUnitEvents : public UnitScript
{
public:
    ItemSkillsUnitEvents() : UnitScript("ItemSkillsUnitEvents") {}

    // 近战攻击时触发（只有玩家真正的近战攻击才会触发，不会被技能伤害、DoT等触发）
    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override;

    // 法术伤害时触发（远程法术、魔法攻击等）
    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override;
};

#endif // MOD_ITEM_SKILLS_EVENTS_H
