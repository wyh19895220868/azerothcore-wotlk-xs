#ifndef MOD_ITEM_SKILLS_EFFECTS_H
#define MOD_ITEM_SKILLS_EFFECTS_H

#include "Common.h"
#include "Player.h"
#include "Item.h"
#include "ItemSkillsManager.h"
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>

// 技能效果处理器类型定义
using SkillEffectHandler = std::function<void(Player*, Item*, ItemSkillTemplate const*, uint32)>;
using SkillEffectRemover = std::function<void(Player*, Item*, ItemSkillTemplate const*, uint32)>;
using SkillDescriptionGenerator = std::function<std::string(Item*, ItemSkillTemplate const*, uint32)>;

// 技能触发器
using SkillTriggerHandler = std::function<bool(Player*, Unit*, Item*, ItemSkillTemplate const*)>;

// 击中触发技能缓存信息
struct HitSkillInfo
{
    uint32 spellId;           // 技能ID
    uint32 skillTemplateId;   // 技能模板ID
    uint64 itemGuid;          // 物品GUID
    float triggerChance;      // 触发几率 (0-100)
    float ppmRate;            // 每分钟触发次数
    uint8 cooldownMode;       // 冷却模式
    int32 cooldownValue;      // 冷却值
    uint8 powerMode;          // 伤害强化模式
    int32 powerValue;         // 伤害强化值
    uint8 castMode;           // 施法模式
};

// 物品技能效果管理器
class ItemSkillsEffects
{
public:
    static ItemSkillsEffects* instance();

    // 初始化
    void Initialize();

    // 注册技能效果处理器
    void RegisterSkillEffectHandler(
        uint8 triggerType,
        SkillEffectHandler handler,
        SkillEffectRemover remover,
        SkillDescriptionGenerator descGenerator
    );

    // 应用物品技能效果
    void ApplyItemSkillEffects(Player* player, Item* item);

    // 移除物品技能效果
    void RemoveItemSkillEffects(Player* player, Item* item);

    // 根据GUID移除技能效果
    void RemoveItemSkillEffectsByGuid(Player* player, uint64 itemGuid);

    // 更新所有装备的技能效果
    void UpdateItemSkillEffects(Player* player);

    // 生成技能描述
    std::string GetSkillDescription(Item* item, uint32 skillTemplateId);

    // 触发器处理
    void OnPlayerDealDamage(Player* player, Unit* victim, AsInt64 const& damage, SpellInfo const* spellInfo);

    // 触发技能（供外部调用）
    void TriggerSkill(Player* player, Unit* target, Item* item, ItemSkillTemplate const* skillTemplate, uint64 itemGuid = 0);

    // 更新玩家的击中技能缓存（装备变化时调用）
    void UpdatePlayerHitSkillsCache(Player* player);

    // 清理玩家的击中技能缓存（下线时调用）
    void ClearPlayerHitSkillsCache(uint64 playerGuid);

private:
    // 触发限流配置（从配置文件读取）
    uint32 _maxTriggersPerDamage = 5;       // 每次伤害最多触发5个技能
    uint32 _globalTriggerCooldownMs = 500;  // 全局触发冷却500ms

    // 效果处理器映射 [触发类型] = 处理器
    std::map<uint8, SkillEffectHandler> _skillEffectHandlers;
    std::map<uint8, SkillEffectRemover> _skillEffectRemovers;
    std::map<uint8, SkillDescriptionGenerator> _skillDescriptionGenerators;

    // 递归防护：正在处理击中触发的玩家GUID集合
    std::set<uint64> _processingPlayers;

    // 玩家击中技能缓存 [playerGuid] = 击中触发技能列表
    std::map<uint64, std::vector<HitSkillInfo>> _playerHitSkillsCache;

    // 玩家上次触发时间 [playerGuid] = 上次触发时间(ms)
    std::map<uint64, uint32> _lastTriggerTime;

    // 检查技能冷却
    bool CheckSkillCooldown(Player* player, ItemSkillTemplate const* skillTemplate);

    // 设置技能冷却
    void SetSkillCooldown(Player* player, ItemSkillTemplate const* skillTemplate);

    // 减少技能使用次数
    void ConsumeSkillCharge(Player* player, Item* item, ItemSkillTemplate const* skillTemplate, uint64 itemGuid = 0);
};

#define sItemSkillsEffects ItemSkillsEffects::instance()

#endif // MOD_ITEM_SKILLS_EFFECTS_H
