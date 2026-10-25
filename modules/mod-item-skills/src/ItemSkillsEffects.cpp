#include "ItemSkillsEffects.h"
#include "ItemSkillsDBHelper.h"
#include "Logging/Log.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Spell.h"
#include "SpellDefines.h"
#include "Unit.h"
#include "Timer.h"
#include "Config.h"
#include "Chat.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include <sstream>
#include <ctime>
#include <random>
#include "ModuleFormat.h"

namespace
{
    bool ShouldSuppressItemSkillClientFeedbackForDebug(Player const* player, ItemSkillTemplate const* skillTemplate, char const* source)
    {
        if (!sConfigMgr->GetOption<bool>("Debug.SuppressPlayerProcClientFeedback", false, false))
            return false;

        static thread_local uint32 suppressedFeedback = 0;
        static thread_local uint32 windowStartMs = 0;

        uint32 const now = getMSTime();
        if (!windowStartMs)
            windowStartMs = now;

        ++suppressedFeedback;

        uint32 const logIntervalMs = std::max<uint32>(sConfigMgr->GetOption<uint32>("Debug.SuppressPlayerProcClientFeedback.LogIntervalMs", 1000, false), 100);
        uint32 const elapsedMs = getMSTimeDiff(windowStartMs, now);
        if (elapsedMs >= logIntervalMs)
        {
            LOG_INFO("server.loading", ModuleOldFormat("[ProcClientFeedbackTest] Suppressed %s item-skill client feedback events in %s ms. last player=%s spell=%s source=%s", suppressedFeedback, elapsedMs, player ? player->GetGUID().ToString() : "none", skillTemplate ? skillTemplate->spellId : 0, source ? source : "unknown").c_str());
            suppressedFeedback = 0;
            windowStartMs = now;
        }

        return true;
    }
}

ItemSkillsEffects* ItemSkillsEffects::instance()
{
    static ItemSkillsEffects instance;
    return &instance;
}

void ItemSkillsEffects::Initialize()
{
    // 读取配置
    _maxTriggersPerDamage = sConfigMgr->GetOption<uint32>("ItemSkills.MaxTriggersPerDamage", 5);
    _globalTriggerCooldownMs = sConfigMgr->GetOption<uint32>("ItemSkills.GlobalTriggerCooldown", 500);

    // 注册装备触发技能处理器
    RegisterSkillEffectHandler(ITEM_SKILL_TRIGGER_ON_EQUIP,
        // 应用效果 - 装备时触发技能
        [](Player* player, Item* item, ItemSkillTemplate const* skillTemplate, uint32 level) {
            if (!player || !skillTemplate)
                return;

            // 装备时立即施放技能
            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(skillTemplate->spellId))
            {
                // 根据冷却模式设置触发标志
                TriggerCastFlags flags = TRIGGERED_FULL_MASK;
                if (skillTemplate->cooldownMode == 1)
                {
                    // 无冷却：忽略冷却
                    flags = TriggerCastFlags(flags | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD);
                }
                else if (skillTemplate->cooldownMode == 2)
                {
                    // 自定义冷却：不设置冷却（装备触发不使用自定义冷却）
                    flags = TriggerCastFlags(flags | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD);
                }

                player->CastSpell(player, skillTemplate->spellId, flags);
            }
        },
        // 移除效果 - 卸下时移除技能效果
        [](Player* player, Item* item, ItemSkillTemplate const* skillTemplate, uint32 level) {
            if (!player || !skillTemplate)
                return;

            // 移除技能光环效果
            player->RemoveAurasDueToSpell(skillTemplate->spellId);
        },
        // 生成描述
        [](Item* item, ItemSkillTemplate const* skillTemplate, uint32 level) {
            if (!skillTemplate)
                return std::string("");

            std::ostringstream ss;
            ss << "装备: " << skillTemplate->clientDisplay;
            if (level > 1)
                ss << " (等级 " << level << ")";
            return ss.str();
        }
    );

    // 注册击中触发技能处理器
    RegisterSkillEffectHandler(ITEM_SKILL_TRIGGER_ON_HIT,
        // 应用效果 - 记录装备了击中触发技能
        [](Player* player, Item* item, ItemSkillTemplate const* skillTemplate, uint32 level) {
            // 击中触发不需要立即应用效果，在OnPlayerDealDamage中处理
            LOG_DEBUG("module.itemskills", ModuleOldFormat("玩家 %s 装备击中触发技能: %s", player->GetName(), skillTemplate->comment).c_str());
        },
        // 移除效果
        [](Player* player, Item* item, ItemSkillTemplate const* skillTemplate, uint32 level) {
            // 击中触发不需要特别移除
            LOG_DEBUG("module.itemskills", ModuleOldFormat("玩家 %s 移除击中触发技能: %s", player->GetName(), skillTemplate->comment).c_str());
        },
        // 生成描述
        [](Item* item, ItemSkillTemplate const* skillTemplate, uint32 level) {
            if (!skillTemplate)
                return std::string("");

            std::ostringstream ss;
            ss << "击中时触发: " << skillTemplate->clientDisplay;
            if (skillTemplate->ppmRate > 0)
                ss << " (每分钟 " << skillTemplate->ppmRate << " 次)";
            if (level > 1)
                ss << " (等级 " << level << ")";
            return ss.str();
        }
    );

    // 注册使用触发技能处理器
    RegisterSkillEffectHandler(ITEM_SKILL_TRIGGER_ON_USE,
        // 应用效果 - 使用时触发
        [](Player* player, Item* item, ItemSkillTemplate const* skillTemplate, uint32 level) {
            // 使用触发不需要在装备时处理
            LOG_DEBUG("module.itemskills", ModuleOldFormat("玩家 %s 装备使用触发技能: %s", player->GetName(), skillTemplate->comment).c_str());
        },
        // 移除效果
        [](Player* player, Item* item, ItemSkillTemplate const* skillTemplate, uint32 level) {
            LOG_DEBUG("module.itemskills", ModuleOldFormat("玩家 %s 移除使用触发技能: %s", player->GetName(), skillTemplate->comment).c_str());
        },
        // 生成描述
        [](Item* item, ItemSkillTemplate const* skillTemplate, uint32 level) {
            if (!skillTemplate)
                return std::string("");

            std::ostringstream ss;
            ss << "使用: " << skillTemplate->clientDisplay;
            if (skillTemplate->charges != 0)
                ss << " (" << abs(skillTemplate->charges) << " 次使用)";
            if (level > 1)
                ss << " (等级 " << level << ")";
            return ss.str();
        }
    );

    // 注册学习技能处理器
    RegisterSkillEffectHandler(ITEM_SKILL_TRIGGER_LEARN_SPELL,
        // 应用效果 - 学习技能
        [](Player* player, Item* item, ItemSkillTemplate const* skillTemplate, uint32 level) {
            if (!player || !skillTemplate)
                return;

            // 让玩家学习技能（添加双重检查和异常处理）
            if (!player->HasSpell(skillTemplate->spellId))
            {
                try
                {
                    player->learnSpell(skillTemplate->spellId);
                }
                catch (...)
                {
                    // 忽略重复学习的错误（玩家可能已经通过其他途径学会了该技能）
                }
            }
        },
        // 移除效果 - 遗忘技能（检查是否有其他装备提供相同技能）
        [](Player* player, Item* item, ItemSkillTemplate const* skillTemplate, uint32 level) {
            if (!player || !skillTemplate)
                return;

            uint32 spellId = skillTemplate->spellId;
            uint64 currentItemGuid = item ? item->GetGUID().GetCounter() : 0;
            uint64 playerGuid = player->GetGUID().GetCounter();
            bool otherItemHasSpell = sItemSkillsManager->HasOtherItemProvidingSpell(
                playerGuid, currentItemGuid, spellId, ITEM_SKILL_TRIGGER_LEARN_SPELL);

            // 只有当没有其他装备提供相同技能时才移除
            if (!otherItemHasSpell && player->HasSpell(spellId))
            {
                player->removeSpell(spellId, SPEC_MASK_ALL, false);
                LOG_DEBUG("module.itemskills", ModuleOldFormat("玩家 %s 遗忘了技能 %s", player->GetName(), spellId).c_str());
            }
        },
        // 生成描述
        [](Item* item, ItemSkillTemplate const* skillTemplate, uint32 level) {
            if (!skillTemplate)
                return std::string("");

            std::ostringstream ss;
            ss << "装备后学习: " << skillTemplate->clientDisplay;
            if (level > 1)
                ss << " (等级 " << level << ")";
            return ss.str();
        }
    );
}

void ItemSkillsEffects::RegisterSkillEffectHandler(uint8 triggerType, SkillEffectHandler handler, SkillEffectRemover remover, SkillDescriptionGenerator descGenerator)
{
    _skillEffectHandlers[triggerType] = handler;
    _skillEffectRemovers[triggerType] = remover;
    _skillDescriptionGenerators[triggerType] = descGenerator;
}

void ItemSkillsEffects::ApplyItemSkillEffects(Player* player, Item* item)
{
    if (!player || !item || !sHelper->GetBoolean(22011))
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();
    uint64 itemGuid = item->GetGUID().GetCounter();

    // 优先从内存读取技能列表
    std::vector<ItemSkillData> skills = sItemSkillsManager->GetItemSkillsFromMemory(playerGuid, itemGuid);

    // 内存中没有则从数据库缓存读取
    if (skills.empty())
    {
        skills = sItemSkillsDBHelper->GetItemSkillsData(item);
    }

    if (skills.empty())
        return;

    // 应用每个技能效果
    for (const auto& skillData : skills)
    {
        ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillData.skillTemplateId);
        if (!skillTemplate)
        {
            LOG_WARN("module.itemskills", ModuleOldFormat("未找到技能模板 ID: %s", skillData.skillTemplateId).c_str());
            continue;
        }

        // 检查技能模板是否启用
        if (!skillTemplate->enabled)
        {
            LOG_DEBUG("module.itemskills", ModuleOldFormat("技能模板 %s 已禁用，跳过", skillData.skillTemplateId).c_str());
            continue;
        }

        // 查找触发类型对应的效果处理器
        auto handlerItr = _skillEffectHandlers.find(skillTemplate->triggerType);
        if (handlerItr != _skillEffectHandlers.end())
        {
            handlerItr->second(player, item, skillTemplate, 1);
        }
        else
        {
            LOG_WARN("module.itemskills", ModuleOldFormat("未找到触发类型 %s 的处理器", skillTemplate->triggerType).c_str());
        }
    }
}

void ItemSkillsEffects::RemoveItemSkillEffects(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();
    uint64 itemGuid = item->GetGUID().GetCounter();

    // 优先从内存读取技能列表
    std::vector<ItemSkillData> skills = sItemSkillsManager->GetItemSkillsFromMemory(playerGuid, itemGuid);

    // 内存中没有则从数据库缓存读取
    if (skills.empty())
    {
        skills = sItemSkillsDBHelper->GetItemSkillsData(item);
    }

    if (skills.empty())
        return;

    // 移除每个技能效果
    for (const auto& skillData : skills)
    {
        ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillData.skillTemplateId);
        if (!skillTemplate)
            continue;

        // 查找触发类型对应的效果移除器
        auto removerItr = _skillEffectRemovers.find(skillTemplate->triggerType);
        if (removerItr != _skillEffectRemovers.end())
        {
            removerItr->second(player, item, skillTemplate, 1);
        }
    }
}

void ItemSkillsEffects::RemoveItemSkillEffectsByGuid(Player* player, uint64 itemGuid)
{
    if (!player || itemGuid == 0)
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();

    // 优先从内存读取技能数据（避免数据库查询）
    std::vector<ItemSkillData> skills = sItemSkillsManager->GetItemSkillsFromMemory(playerGuid, itemGuid);

    // 如果内存中没有，尝试从数据库缓存读取
    if (skills.empty())
    {
        skills = sItemSkillsDBHelper->GetItemSkillsData(itemGuid);
    }

    if (skills.empty())
        return;

    // 移除每个技能效果
    for (const auto& skillData : skills)
    {
        ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillData.skillTemplateId);
        if (!skillTemplate)
            continue;

        // 特殊处理学习技能（触发类型=100）
        if (skillData.triggerType == 100 || skillTemplate->triggerType == 100)
        {
            if (player->HasSpell(skillData.skillId))
            {
                player->removeSpell(skillData.skillId, SPEC_MASK_ALL, false);
            }
            continue;
        }

        // 其他类型的技能，使用注册的移除器
        auto removerItr = _skillEffectRemovers.find(skillTemplate->triggerType);
        if (removerItr != _skillEffectRemovers.end())
        {
            removerItr->second(player, nullptr, skillTemplate, 1);
        }
    }
}

void ItemSkillsEffects::UpdateItemSkillEffects(Player* player)
{
    if (!player)
        return;

    // 移除所有物品技能效果
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            RemoveItemSkillEffects(player, item);
        }
    }

    // 重新应用所有物品技能效果
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            ApplyItemSkillEffects(player, item);
        }
    }
}

std::string ItemSkillsEffects::GetSkillDescription(Item* item, uint32 skillTemplateId)
{
    if (!item)
        return "";

    ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillTemplateId);
    if (!skillTemplate)
        return "";

    auto generatorItr = _skillDescriptionGenerators.find(skillTemplate->triggerType);
    if (generatorItr != _skillDescriptionGenerators.end())
    {
        return generatorItr->second(item, skillTemplate, skillTemplate->level);
    }

    // 默认描述
    return skillTemplate->clientDisplay;
}

void ItemSkillsEffects::TriggerSkill(Player* player, Unit* target, Item* item, ItemSkillTemplate const* skillTemplate, uint64 itemGuid)
{
    if (!player || !skillTemplate)
        return;

    uint64 effectiveItemGuid = item ? item->GetGUID().GetCounter() : itemGuid;

    // 玩家死亡时不触发技能
    if (!player->IsAlive())
        return;

    // 如果目标存在但已死亡，也不触发（除非是自身buff类技能）
    if (target && target != player && !target->IsAlive())
        return;

    // 【触发率平方修复】此前这里对 triggerChance 再掷一次骰，而唯一调用方
    // HandleHitTrigger 在筛选阶段已经按同一几率掷过——实际触发率变成配置值的平方
    //（配置 30% 实际 9%）。几率判定统一由调用方负责，此处不再重复掷骰。

    // 检查冷却
    if (!CheckSkillCooldown(player, skillTemplate))
    {
        return;
    }

    // 【修复】检查使用次数（在触发前检查）
    if (skillTemplate->charges != 0 && effectiveItemGuid != 0)
    {
        uint64 playerGuid = player->GetGUID().GetCounter();

        if (!sItemSkillsManager->HasSkillCharge(playerGuid, effectiveItemGuid, skillTemplate->spellId))
        {
            // 次数已用完，不触发
            LOG_DEBUG("module.itemskills", ModuleOldFormat("[TriggerSkill] 技能 %s 使用次数已耗尽", skillTemplate->spellId).c_str());
            return;
        }
    }

    // 获取技能信息
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(skillTemplate->spellId);
    if (!spellInfo)
    {
        LOG_ERROR("module.itemskills", ModuleOldFormat("[Hit] 技能ID %s 不存在", skillTemplate->spellId).c_str());
        return;
    }

    // 根据 castMode 决定施法模式
    // castMode = 0: 官方施法时间 + 官方动画（但击中触发需要忽略施法时间）
    // castMode = 1: 无施法时间（瞬发）+ 有动画
    // castMode = 2: 无施法时间 + 无动画（静默模式）

    bool useSilentMode = (skillTemplate->castMode == 2);

    // 如果不使用静默模式，使用 CastSpell
    if (!useSilentMode)
    {
        Unit* effectTarget = target ? target : player;

        // 根据 castMode 设置触发标志
        // 注意：击中触发始终需要是瞬发的，否则会打断玩家当前动作
        TriggerCastFlags triggerFlags = TRIGGERED_NONE;

        // 击中触发必须忽略施法时间（否则每次攻击都会打断玩家）
        triggerFlags = TriggerCastFlags(triggerFlags | TRIGGERED_CAST_DIRECTLY);
        // 忽略GCD
        triggerFlags = TriggerCastFlags(triggerFlags | TRIGGERED_IGNORE_GCD);
        // 忽略消耗（击中触发不应消耗法力）
        triggerFlags = TriggerCastFlags(triggerFlags | TRIGGERED_IGNORE_POWER_AND_REAGENT_COST);

        if (skillTemplate->castMode == 1)
        {
            // 瞬发模式：额外确保瞬发
            triggerFlags = TriggerCastFlags(triggerFlags | TRIGGERED_CAST_DIRECTLY);
        }
        // castMode == 0 时使用 TRIGGERED_FULL_MASK（包含所有忽略标志）
        ShouldSuppressItemSkillClientFeedbackForDebug(player, skillTemplate, "item-skills-castspell");

        // 应用伤害强化（如果配置了）
        if (skillTemplate->powerMode != 0 && skillTemplate->powerValue != 0)
        {
            // 创建自定义技能值对象来修改伤害
            CustomSpellValues values;

            // 遍历技能的所有效果，应用伤害强化
            for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
            {
                if (!spellInfo->Effects[i].IsEffect())
                    continue;

                uint32 effect = spellInfo->Effects[i].Effect;
                int32 basePoints = spellInfo->Effects[i].BasePoints;

                // 只对伤害和治疗效果应用强化
                if (effect == SPELL_EFFECT_SCHOOL_DAMAGE ||           // 2 - 直接伤害
                    effect == SPELL_EFFECT_WEAPON_DAMAGE ||           // 121 - 武器伤害
                    effect == SPELL_EFFECT_WEAPON_PERCENT_DAMAGE ||   // 122 - 武器百分比伤害
                    effect == SPELL_EFFECT_NORMALIZED_WEAPON_DMG ||   // 140 - 标准化武器伤害
                    effect == SPELL_EFFECT_HEAL)                      // 10 - 治疗
                {
                    int32 modifiedValue = basePoints;

                    if (skillTemplate->powerMode == 1)
                    {
                        // 固定值增幅
                        modifiedValue += skillTemplate->powerValue;
                    }
                    else if (skillTemplate->powerMode == 2)
                    {
                        // 百分比增幅
                        modifiedValue = static_cast<int32>(modifiedValue * (1.0f + skillTemplate->powerValue / 100.0f));
                    }

                    // 设置自定义伤害值
                    values.AddSpellMod(SpellValueMod(SPELLVALUE_BASE_POINT0 + i), modifiedValue);
                }
            }

            // 使用自定义值施放技能
            SpellCastTargets targets;
            targets.SetUnitTarget(effectTarget);
            player->CastSpell(targets, spellInfo, &values, triggerFlags);
        }
        else
        {
            // 没有伤害强化，直接施放
            player->CastSpell(effectTarget, skillTemplate->spellId, triggerFlags);
        }

        // 设置冷却
        SetSkillCooldown(player, skillTemplate);

        // 消耗使用次数
        ConsumeSkillCharge(player, item, skillTemplate, effectiveItemGuid);

        return;
    }

    // 静默模式：直接应用技能效果，不播放施法动画
    // 遍历技能的所有效果
    bool hasEffect = false;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (!spellInfo->Effects[i].IsEffect())
            continue;

        uint32 effect = spellInfo->Effects[i].Effect;
        int32 basePoints = spellInfo->Effects[i].BasePoints;

        // 应用伤害强化
        if (skillTemplate->powerMode == 1)
        {
            basePoints += skillTemplate->powerValue;
        }
        else if (skillTemplate->powerMode == 2)
        {
            basePoints = static_cast<int32>(basePoints * (1.0f + skillTemplate->powerValue / 100.0f));
        }

        Unit* effectTarget = target ? target : player;

        switch (effect)
        {
            case SPELL_EFFECT_SCHOOL_DAMAGE:           // 2 - 直接伤害
            case SPELL_EFFECT_WEAPON_DAMAGE:           // 121 - 武器伤害
            case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:   // 122 - 武器百分比伤害
            case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:   // 140 - 标准化武器伤害
            {
                if (effectTarget && effectTarget != player)
                {
                    uint32 damage = basePoints > 0 ? basePoints : 100; // 默认100点伤害

                    // 造成伤害 - 修正参数顺序
                    Unit::DealDamage(player, effectTarget, damage, nullptr, SPELL_DIRECT_DAMAGE,
                        SpellSchoolMask(spellInfo->SchoolMask), spellInfo, false);


                    // 发送伤害日志（让客户端显示伤害数字）
                    if (!ShouldSuppressItemSkillClientFeedbackForDebug(player, skillTemplate, "item-skills-silent-damage-log"))
                    {
                        player->SendSpellNonMeleeDamageLog(effectTarget, spellInfo, damage,
                            SpellSchoolMask(spellInfo->SchoolMask), 0, 0, false, 0, false);
                    }

                    hasEffect = true;
                }
                break;
            }

            case SPELL_EFFECT_HEAL:                    // 10 - 治疗
            {
                if (effectTarget)
                {
                    uint32 heal = basePoints > 0 ? Acore::Number::ToUInt32Saturated(basePoints) : 100;

                    // 创建HealInfo对象
                    HealInfo healInfo(player, effectTarget, heal, spellInfo, SpellSchoolMask(spellInfo->SchoolMask));
                    player->HealBySpell(healInfo);

                    hasEffect = true;
                }
                break;
            }

            case SPELL_EFFECT_APPLY_AURA:              // 6 - 应用光环
            case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:   // 65 - 队伍光环
            {
                // 对于光环效果，还是需要使用CastSpell，但使用完全触发模式
                if (!hasEffect) // 只施放一次
                {
                    player->CastSpell(effectTarget, skillTemplate->spellId, TRIGGERED_FULL_MASK);
                    hasEffect = true;
                }
                break;
            }

            default:
                break;
        }
    }

    if (!hasEffect)
    {
        LOG_DEBUG("module.itemskills", ModuleOldFormat("技能 %s 没有可处理的静默效果，跳过施放", skillTemplate->spellId).c_str());
        return;
    }

    // 设置冷却（在施放后）
    SetSkillCooldown(player, skillTemplate);

    // 消耗使用次数
    ConsumeSkillCharge(player, item, skillTemplate, effectiveItemGuid);
}

bool ItemSkillsEffects::CheckSkillCooldown(Player* player, ItemSkillTemplate const* skillTemplate)
{
    if (!player || !skillTemplate)
        return false;

    // 无冷却模式直接返回true（不检查冷却）
    if (skillTemplate->cooldownMode == 1)
        return true;

    // 检查技能是否在冷却中
    return !player->HasSpellCooldown(skillTemplate->spellId);
}

void ItemSkillsEffects::SetSkillCooldown(Player* player, ItemSkillTemplate const* skillTemplate)
{
    if (!player || !skillTemplate)
        return;

    // 因为使用了 TRIGGERED_FULL_MASK，所有冷却都被忽略了
    // 需要根据冷却模式手动添加冷却

    // 获取冷却时间修正系数
    float cooldownModifier = sItemSkillsManager->GetCooldownModifier();

    if (skillTemplate->cooldownMode == 1)
    {
        // 无冷却：不设置
        return;
    }
    else if (skillTemplate->cooldownMode == 2 && skillTemplate->cooldownValue > 0)
    {
        // 自定义冷却：应用修正系数后手动添加
        uint32 effectiveCooldown = static_cast<uint32>(skillTemplate->cooldownValue * cooldownModifier);
        player->AddSpellCooldown(skillTemplate->spellId, 0xFFFFFFFF, effectiveCooldown, true);
    }
    else if (skillTemplate->cooldownMode == 0)
    {
        // 官方冷却：因为使用了TRIGGERED_FULL_MASK，需要手动获取并添加官方冷却
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(skillTemplate->spellId);
        if (spellInfo && spellInfo->RecoveryTime > 0)
        {
            uint32 effectiveCooldown = static_cast<uint32>(spellInfo->RecoveryTime * cooldownModifier);
            player->AddSpellCooldown(skillTemplate->spellId, 0, effectiveCooldown, true);
        }
        else if (spellInfo && spellInfo->CategoryRecoveryTime > 0)
        {
            uint32 effectiveCooldown = static_cast<uint32>(spellInfo->CategoryRecoveryTime * cooldownModifier);
            player->AddSpellCooldown(skillTemplate->spellId, 0, effectiveCooldown, true);
        }
    }
}

void ItemSkillsEffects::ConsumeSkillCharge(Player* player, Item* item, ItemSkillTemplate const* skillTemplate, uint64 itemGuid)
{
    if (!player || !skillTemplate)
        return;

    if (skillTemplate->charges == 0)
        return; // 无限使用

    uint64 effectiveItemGuid = item ? item->GetGUID().GetCounter() : itemGuid;
    if (effectiveItemGuid == 0)
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();

    // 消耗使用次数
    sItemSkillsManager->ConsumeSkillCharge(playerGuid, effectiveItemGuid, skillTemplate->spellId);

    // 【修复】检查是否需要移除技能（charges < 0 表示用完后消失）
    if (skillTemplate->charges < 0)
    {
        // 检查剩余次数
        if (!sItemSkillsManager->HasSkillCharge(playerGuid, effectiveItemGuid, skillTemplate->spellId))
        {
            if (!item)
            {
                LOG_INFO("module.itemskills", ModuleOldFormat("[ConsumeCharge] 外部虚拟技能 %s 使用次数耗尽，当前不会自动移除技能模板", skillTemplate->id).c_str());
                return;
            }

            // 次数用完，从物品移除该技能
            sItemSkillsDBHelper->RemoveSkillFromItem(item, skillTemplate->id);

            // 刷新内存缓存
            sItemSkillsManager->RefreshItemSkillsInMemory(playerGuid, item);

            // 更新击中技能缓存
            sItemSkillsEffects->UpdatePlayerHitSkillsCache(player);

            // 通知玩家
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("|cFFFF0000物品技能 [%s] 使用次数已耗尽，技能已消失！|r", skillTemplate->clientDisplay).c_str());

            LOG_INFO("module.itemskills", ModuleOldFormat("[ConsumeCharge] 玩家 %s 物品 %s 技能 %s 使用次数耗尽，已移除", player->GetName(), item->GetEntry(), skillTemplate->id).c_str());
        }
    }
}

void ItemSkillsEffects::OnPlayerDealDamage(Player* player, Unit* victim, AsInt64 const& damage, SpellInfo const* spellInfo)
{
    if (!player || !victim || !sHelper->GetBoolean(22011))
        return;

    // 双重检查：玩家或目标死亡时不触发
    if (!player->IsAlive() || !victim->IsAlive())
        return;

    // 检查玩家是否在世界中
    if (!player->IsInWorld())
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();

    // 1. 递归防护
    if (_processingPlayers.count(playerGuid))
        return;

    // 2. 全局冷却检查（核心优化点）
    uint32 currentTime = getMSTime();
    auto lastTimeIt = _lastTriggerTime.find(playerGuid);
    if (lastTimeIt != _lastTriggerTime.end())
    {
        uint32 timeDiff = currentTime - lastTimeIt->second;
        if (timeDiff < _globalTriggerCooldownMs)
        {
            return;  // 全局冷却中，直接跳过
        }
    }

    // 3. 从缓存获取击中技能（不再每次遍历装备）
    auto cacheIt = _playerHitSkillsCache.find(playerGuid);
    if (cacheIt == _playerHitSkillsCache.end() || cacheIt->second.empty())
    {
        return;  // 没有击中触发技能
    }

    _processingPlayers.insert(playerGuid);

    // 4. 筛选可触发的技能（通过几率和冷却检查）
    std::vector<const HitSkillInfo*> availableSkills;
    float chanceModifier = sItemSkillsManager->GetChanceModifier();
    float ppmModifier = sItemSkillsManager->GetPPMModifier();

    for (const auto& hitSkill : cacheIt->second)
    {
        // 冷却检查
        if (player->HasSpellCooldown(hitSkill.spellId))
            continue;

        // 触发几率检查（应用修正系数）
        float effectiveChance = hitSkill.triggerChance * chanceModifier;
        if (effectiveChance < 100.0f && frand(0.0f, 100.0f) > effectiveChance)
            continue;

        // PPM检查（应用修正系数）
        if (hitSkill.ppmRate > 0)
        {
            float effectivePPM = hitSkill.ppmRate * ppmModifier;
            float chance = effectivePPM / 60.0f;
            if (frand(0.0f, 1.0f) > chance)
                continue;
        }

        availableSkills.push_back(&hitSkill);
    }

    // 5. 随机打乱顺序（保证公平性）
    if (availableSkills.size() > 1)
    {
        static std::random_device rd;
        static std::mt19937 g(rd());
        std::shuffle(availableSkills.begin(), availableSkills.end(), g);
    }

    // 6. 取前 MAX_TRIGGERS_PER_DAMAGE 个触发
    uint32 triggeredCount = 0;
    for (const auto* hitSkill : availableSkills)
    {
        if (triggeredCount >= _maxTriggersPerDamage)
            break;

        // 获取技能模板以获取完整配置
        ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(hitSkill->skillTemplateId);
        if (!skillTemplate)
            continue;

        // 获取物品用于设置冷却
        Item* item = nullptr;
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* checkItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (checkItem && checkItem->GetGUID().GetCounter() == hitSkill->itemGuid)
            {
                item = checkItem;
                break;
            }
        }

        // 触发技能
        TriggerSkill(player, victim, item, skillTemplate, hitSkill->itemGuid);
        triggeredCount++;
    }

    // 7. 更新全局触发时间
    if (triggeredCount > 0)
    {
        _lastTriggerTime[playerGuid] = currentTime;
    }

    _processingPlayers.erase(playerGuid);
}

// 更新玩家的击中技能缓存（装备变化时调用）
void ItemSkillsEffects::UpdatePlayerHitSkillsCache(Player* player)
{
    if (!player)
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();
    _playerHitSkillsCache[playerGuid].clear();

    if (!sHelper->GetBoolean(22011))
        return;

    auto appendHitSkills = [this, playerGuid](uint64 itemGuid, std::vector<ItemSkillData> const& skills)
    {
        for (const auto& skill : skills)
        {
            if (skill.triggerType != ITEM_SKILL_TRIGGER_ON_HIT)
                continue;

            ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skill.skillTemplateId);
            if (!skillTemplate || !skillTemplate->enabled)
                continue;

            HitSkillInfo info;
            info.spellId = skill.skillId;
            info.skillTemplateId = skill.skillTemplateId;
            info.itemGuid = itemGuid;
            info.triggerChance = skill.triggerChance > 0 ? skill.triggerChance : skillTemplate->triggerChance;
            info.ppmRate = skillTemplate->ppmRate;
            info.cooldownMode = skillTemplate->cooldownMode;
            info.cooldownValue = skillTemplate->cooldownValue;
            info.powerMode = skillTemplate->powerMode;
            info.powerValue = skillTemplate->powerValue;
            info.castMode = skillTemplate->castMode;

            _playerHitSkillsCache[playerGuid].push_back(info);
        }
    };

    // 只检查可能有击中触发技能的槽位（武器、饰品、戒指等）
    static const uint8 hitSlots[] = {
        EQUIPMENT_SLOT_HEAD,
        EQUIPMENT_SLOT_NECK,
        EQUIPMENT_SLOT_SHOULDERS,
        EQUIPMENT_SLOT_CHEST,
        EQUIPMENT_SLOT_WAIST,
        EQUIPMENT_SLOT_LEGS,
        EQUIPMENT_SLOT_FEET,
        EQUIPMENT_SLOT_WRISTS,
        EQUIPMENT_SLOT_HANDS,
        EQUIPMENT_SLOT_FINGER1,
        EQUIPMENT_SLOT_FINGER2,
        EQUIPMENT_SLOT_TRINKET1,
        EQUIPMENT_SLOT_TRINKET2,
        EQUIPMENT_SLOT_BACK,
        EQUIPMENT_SLOT_MAINHAND,
        EQUIPMENT_SLOT_OFFHAND,
        EQUIPMENT_SLOT_RANGED
    };

    for (uint8 slot : hitSlots)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        uint64 itemGuid = item->GetGUID().GetCounter();
        std::vector<ItemSkillData> skills = sItemSkillsManager->GetItemSkillsFromMemory(playerGuid, itemGuid);
        appendHitSkills(itemGuid, skills);
    }

    if (auto externalSkills = sItemSkillsManager->GetExternalItemSkills(playerGuid))
    {
        for (auto const& pair : *externalSkills)
            appendHitSkills(pair.first, pair.second);
    }

    LOG_DEBUG("module.itemskills", ModuleOldFormat("[HitCache] 玩家 %s 击中技能缓存已更新，共 %s 个技能", player->GetName(), _playerHitSkillsCache[playerGuid].size()).c_str());
}

// 清理玩家的击中技能缓存（下线时调用）
void ItemSkillsEffects::ClearPlayerHitSkillsCache(uint64 playerGuid)
{
    _playerHitSkillsCache.erase(playerGuid);
    _lastTriggerTime.erase(playerGuid);
}
