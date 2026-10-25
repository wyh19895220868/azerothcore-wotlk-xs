#include "AsInt256.h"
#include "StringConvert.h"
#include "Custom/_Helper/AsInt64.h"
#ifndef RUNE_EFFECT_HANDLER_H
#define RUNE_EFFECT_HANDLER_H

#include "Common.h"
#include "RuneTypes.h"
#include "RuneUtils.h"
#include "Player.h"
#include "SpellMgr.h"
#include <limits>
#include <string>
#include <vector>
#include "ModuleFormat.h"

// 符文效果处理类
class RuneEffectHandler
{
public:
    // 应用符文效果到玩家
    static void ApplyRuneEffects(Player* player, const CustomRuneInfo& rune)
    {
        if (!player)
            return;

        switch (rune.effectType)
        {
            case 0: // 无加成
                break;

            case 1: // 属性加成
                ApplyAttributeBonus(player, rune.effectValues);
                break;

            case 2: // 技能加成
                ApplySkillBonus(player, rune);
                break;

            case 3: // 百分比属性加成
                ApplyPercentAttributeBonus(player, rune.effectValues);
                break;

            case 4: // 百分比技能加成
                ApplyPercentSkillBonus(player, rune);
                break;

            default:
                break;
        }
    }

    // 移除符文效果
    static void RemoveRuneEffects(Player* player, const CustomRuneInfo& rune)
    {
        if (!player)
            return;

        switch (rune.effectType)
        {
            case 1: // 属性加成
                RemoveAttributeBonus(player, rune.effectValues);
                break;

            case 2: // 技能加成
                // 技能加成在触发时计算，不需要移除
                break;

            case 3: // 百分比属性加成
                RemovePercentAttributeBonus(player, rune.effectValues);
                break;

            case 4: // 百分比技能加成
                // 百分比技能加成在触发时计算，不需要移除
                break;

            default:
                break;
        }
    }

private:
    static float ToRuneFloat(AsInt64 const& value)
    {
        return Acore::Number::ToFloat(value);
    }

    static int32 ToRuneInt32(AsInt64 const& value)
    {
        return Acore::Number::ToInt32Saturated(value);
    }

    static AsInt64 NegateRuneValue(AsInt64 const& value)
    {
        if (value == std::numeric_limits<AsInt64>::min())
            return std::numeric_limits<AsInt64>::max();

        return -value;
    }

    // 应用属性加成
    static void ApplyAttributeBonus(Player* player, const std::string& effectValues)
    {
        auto attributes = RuneUtils::ParseEffectValues(effectValues);

        for (const auto& attr : attributes)
        {
            uint32 attributeId = attr.first;
            AsInt64 value = attr.second;
            std::string const valueText = Acore::ToString(value);
            float const floatValue = ToRuneFloat(value);
            int32 const ratingValue = ToRuneInt32(value);

            // 根据属性ID应用不同的加成
            switch (attributeId)
            {
                // 基础资源
                case 0: // 法力值
                    player->HandleStatModifier(UNIT_MOD_MANA, TOTAL_VALUE, floatValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用法力值: +%s", valueText).c_str());
                    break;

                case 1: // 护甲值
                    player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, floatValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用护甲值: +%s", valueText).c_str());
                    break;

                case 2: // 生命值
                    player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, floatValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用生命值: +%s", valueText).c_str());
                    break;

                // 基础属性
                // UNIT_MOD_STAT_START 对应的枚举顺序：
                // UNIT_MOD_STAT_STRENGTH = 0 (力量)
                // UNIT_MOD_STAT_AGILITY = 1 (敏捷)
                // UNIT_MOD_STAT_STAMINA = 2 (耐力)
                // UNIT_MOD_STAT_INTELLECT = 3 (智力)
                // UNIT_MOD_STAT_SPIRIT = 4 (精神)
                case 3: // 敏捷
                    player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, floatValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用敏捷: +%s", valueText).c_str());
                    break;
                case 4: // 力量
                    player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, floatValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用力量: +%s", valueText).c_str());
                    break;
                case 5: // 智力
                    player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, floatValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用智力: +%s", valueText).c_str());
                    break;
                case 6: // 精神
                    player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, floatValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用精神: +%s", valueText).c_str());
                    break;
                case 7: // 耐力
                    player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, floatValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用耐力: +%s", valueText).c_str());
                    break;

                // 防御等级
                case 12: // 防御等级
                    player->ApplyRatingMod(CR_DEFENSE_SKILL, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用防御等级: +%s", valueText).c_str());
                    break;
                case 13: // 躲闪等级
                    player->ApplyRatingMod(CR_DODGE, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用躲闪等级: +%s", valueText).c_str());
                    break;
                case 14: // 招架等级
                    player->ApplyRatingMod(CR_PARRY, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用招架等级: +%s", valueText).c_str());
                    break;
                case 15: // 格挡等级
                    player->ApplyRatingMod(CR_BLOCK, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用格挡等级: +%s", valueText).c_str());
                    break;

                // 命中等级
                case 16: // 近战命中等级
                    player->ApplyRatingMod(CR_HIT_MELEE, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用近战命中等级: +%s", valueText).c_str());
                    break;
                case 17: // 远程命中等级
                    player->ApplyRatingMod(CR_HIT_RANGED, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用远程命中等级: +%s", valueText).c_str());
                    break;
                case 18: // 法术命中等级
                    player->ApplyRatingMod(CR_HIT_SPELL, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用法术命中等级: +%s", valueText).c_str());
                    break;
                case 31: // 命中等级(通用)
                    player->ApplyRatingMod(CR_HIT_MELEE, ratingValue, true);
                    player->ApplyRatingMod(CR_HIT_RANGED, ratingValue, true);
                    player->ApplyRatingMod(CR_HIT_SPELL, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用命中等级(通用): +%s", valueText).c_str());
                    break;

                // 暴击等级
                case 19: // 近战暴击等级
                    player->ApplyRatingMod(CR_CRIT_MELEE, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用近战暴击等级: +%s", valueText).c_str());
                    break;
                case 20: // 远程暴击等级
                    player->ApplyRatingMod(CR_CRIT_RANGED, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用远程暴击等级: +%s", valueText).c_str());
                    break;
                case 21: // 法术暴击等级
                    player->ApplyRatingMod(CR_CRIT_SPELL, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用法术暴击等级: +%s", valueText).c_str());
                    break;
                case 32: // 暴击等级(通用)
                    player->ApplyRatingMod(CR_CRIT_MELEE, ratingValue, true);
                    player->ApplyRatingMod(CR_CRIT_RANGED, ratingValue, true);
                    player->ApplyRatingMod(CR_CRIT_SPELL, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用暴击等级(通用): +%s", valueText).c_str());
                    break;

                // 被命中/被暴击等级
                case 22: // 近战被命中等级
                    player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用近战被命中等级: +%s", valueText).c_str());
                    break;
                case 23: // 远程被命中等级
                    player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用远程被命中等级: +%s", valueText).c_str());
                    break;
                case 24: // 法术被命中等级
                    player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用法术被命中等级: +%s", valueText).c_str());
                    break;
                case 25: // 近战被暴击等级
                    player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用近战被暴击等级: +%s", valueText).c_str());
                    break;
                case 26: // 远程被暴击等级
                    player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用远程被暴击等级: +%s", valueText).c_str());
                    break;
                case 27: // 法术被暴击等级
                    player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用法术被暴击等级: +%s", valueText).c_str());
                    break;

                // 急速等级
                case 28: // 近战急速等级
                    player->ApplyRatingMod(CR_HASTE_MELEE, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用近战急速等级: +%s", valueText).c_str());
                    break;
                case 29: // 远程急速等级
                    player->ApplyRatingMod(CR_HASTE_RANGED, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用远程急速等级: +%s", valueText).c_str());
                    break;
                case 30: // 法术急速等级
                    player->ApplyRatingMod(CR_HASTE_SPELL, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用法术急速等级: +%s", valueText).c_str());
                    break;
                case 36: // 急速等级(通用)
                    player->ApplyRatingMod(CR_HASTE_MELEE, ratingValue, true);
                    player->ApplyRatingMod(CR_HASTE_RANGED, ratingValue, true);
                    player->ApplyRatingMod(CR_HASTE_SPELL, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用急速等级(通用): +%s", valueText).c_str());
                    break;

                // 其他等级
                case 35: // 韧性等级
                    player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, ratingValue, true);
                    player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, ratingValue, true);
                    player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用韧性等级: +%s", valueText).c_str());
                    break;
                case 37: // 精准等级
                    player->ApplyRatingMod(CR_EXPERTISE, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用精准等级: +%s", valueText).c_str());
                    break;
                case 46: // 护甲穿透等级
                    player->ApplyRatingMod(CR_ARMOR_PENETRATION, ratingValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用护甲穿透等级: +%s", valueText).c_str());
                    break;
                case 47: // 精通等级 (WotLK不支持，预留)
                    LOG_DEBUG("rune.effect", "精通等级在WotLK中不可用");
                    break;

                // 攻击强度
                case 38: // 攻击强度
                    player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, floatValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用攻击强度: +%s", valueText).c_str());
                    break;
                case 39: // 远程攻击强度
                    player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, floatValue, true);
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用远程攻击强度: +%s", valueText).c_str());
                    break;

                // 法术属性
                case 40: // 法术治疗
                case 41: // 法术伤害
                case 42: // 法术治疗和伤害
                case 45: // 法术强度
                    // 这些属性需要特殊处理，可能需要Aura或其他机制
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用法术属性: 属性ID=%s, 值=%s", attributeId, valueText).c_str());
                    break;

                case 43: // 法术穿透
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用法术穿透: +%s", valueText).c_str());
                    break;

                case 44: // 生命值回复
                    LOG_DEBUG("rune.effect", ModuleOldFormat("应用生命值回复: +%s", valueText).c_str());
                    break;

                default:
                    LOG_WARN("rune.effect", ModuleOldFormat("未知属性ID: %s", attributeId).c_str());
                    break;
            }
        }

        // 更新玩家属性
        player->UpdateAllStats();
    }

    // 移除属性加成（与应用相反的操作）
    static void RemoveAttributeBonus(Player* player, const std::string& effectValues)
    {
        LOG_ERROR("rune.effect", ModuleOldFormat("[移除属性] 开始移除属性加成，effectValues=%s", effectValues).c_str());

        auto attributes = RuneUtils::ParseEffectValues(effectValues);

        LOG_ERROR("rune.effect", ModuleOldFormat("[移除属性] 解析到 %s 个属性", attributes.size()).c_str());

        for (const auto& attr : attributes)
        {
            uint32 attributeId = attr.first;
            AsInt64 value = attr.second;
            std::string const valueText = Acore::ToString(value);
            AsInt64 const negativeValue = NegateRuneValue(value);
            float const negativeFloatValue = ToRuneFloat(negativeValue);
            int32 const ratingValue = ToRuneInt32(value);

            LOG_ERROR("rune.effect", ModuleOldFormat("[移除属性] 移除属性ID=%s, 值=%s", attributeId, valueText).c_str());

            // 根据属性ID移除不同的加成（使用负值）
            switch (attributeId)
            {
                case 0: // 法力值
                    player->HandleStatModifier(UNIT_MOD_MANA, TOTAL_VALUE, negativeFloatValue, true);
                    break;
                case 1: // 护甲值
                    player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, negativeFloatValue, true);
                    break;

                case 2: // 生命值
                    player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, negativeFloatValue, true);
                    break;
                case 3: // 敏捷
                    player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, negativeFloatValue, true);
                    break;
                case 4: // 力量
                    player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, negativeFloatValue, true);
                    break;
                case 5: // 智力
                    player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, negativeFloatValue, true);
                    break;
                case 6: // 精神
                    player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, negativeFloatValue, true);
                    break;
                case 7: // 耐力
                    LOG_ERROR("rune.effect", ModuleOldFormat("[移除属性] 移除耐力: -%s, 调用HandleStatModifier(负值)", valueText).c_str());
                    player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, negativeFloatValue, true);
                    LOG_ERROR("rune.effect", "[移除属性] 耐力移除完成");
                    break;
                case 12: player->ApplyRatingMod(CR_DEFENSE_SKILL, ratingValue, false); break;
                case 13: player->ApplyRatingMod(CR_DODGE, ratingValue, false); break;
                case 14: player->ApplyRatingMod(CR_PARRY, ratingValue, false); break;
                case 15: player->ApplyRatingMod(CR_BLOCK, ratingValue, false); break;
                case 16: player->ApplyRatingMod(CR_HIT_MELEE, ratingValue, false); break;
                case 17: player->ApplyRatingMod(CR_HIT_RANGED, ratingValue, false); break;
                case 18: player->ApplyRatingMod(CR_HIT_SPELL, ratingValue, false); break;
                case 19: player->ApplyRatingMod(CR_CRIT_MELEE, ratingValue, false); break;
                case 20: player->ApplyRatingMod(CR_CRIT_RANGED, ratingValue, false); break;
                case 21: player->ApplyRatingMod(CR_CRIT_SPELL, ratingValue, false); break;
                case 22: player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, ratingValue, false); break;
                case 23: player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, ratingValue, false); break;
                case 24: player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, ratingValue, false); break;
                case 25: player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, ratingValue, false); break;
                case 26: player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, ratingValue, false); break;
                case 27: player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, ratingValue, false); break;
                case 28: player->ApplyRatingMod(CR_HASTE_MELEE, ratingValue, false); break;
                case 29: player->ApplyRatingMod(CR_HASTE_RANGED, ratingValue, false); break;
                case 30: player->ApplyRatingMod(CR_HASTE_SPELL, ratingValue, false); break;
                case 31: // 命中等级(通用)
                    player->ApplyRatingMod(CR_HIT_MELEE, ratingValue, false);
                    player->ApplyRatingMod(CR_HIT_RANGED, ratingValue, false);
                    player->ApplyRatingMod(CR_HIT_SPELL, ratingValue, false);
                    break;
                case 32: // 暴击等级(通用)
                    player->ApplyRatingMod(CR_CRIT_MELEE, ratingValue, false);
                    player->ApplyRatingMod(CR_CRIT_RANGED, ratingValue, false);
                    player->ApplyRatingMod(CR_CRIT_SPELL, ratingValue, false);
                    break;
                case 35: // 韧性等级
                    player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, ratingValue, false);
                    player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, ratingValue, false);
                    player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, ratingValue, false);
                    break;
                case 36: // 急速等级(通用)
                    player->ApplyRatingMod(CR_HASTE_MELEE, ratingValue, false);
                    player->ApplyRatingMod(CR_HASTE_RANGED, ratingValue, false);
                    player->ApplyRatingMod(CR_HASTE_SPELL, ratingValue, false);
                    break;
                case 37: player->ApplyRatingMod(CR_EXPERTISE, ratingValue, false); break;
                case 38: player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, negativeFloatValue, true); break;
                case 39: player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, negativeFloatValue, true); break;
                case 46: player->ApplyRatingMod(CR_ARMOR_PENETRATION, ratingValue, false); break;
                case 47: break; // 精通等级在WotLK中不可用
                default: break;
            }
        }

        LOG_ERROR("rune.effect", "[移除属性] 所有属性移除完成，调用UpdateAllStats");
        player->UpdateAllStats();
        LOG_ERROR("rune.effect", "[移除属性] UpdateAllStats完成");
    }

    // 应用技能加成
    static void ApplySkillBonus(Player* player, const CustomRuneInfo& rune)
    {
        // 技能加成格式: "99 数值"
        auto values = RuneUtils::ParseEffectValues(rune.effectValues);

        if (values.empty() || values[0].first != 99)
            return;

        AsInt64 bonusValue = values[0].second;

        // 技能加成通常在触发时动态计算
        // 这里可以记录到玩家的自定义数据中
        LOG_DEBUG("rune.effect", ModuleOldFormat("符文 %s 技能加成: %s", rune.runeId, Acore::ToString(bonusValue)).c_str());
    }

    static bool HandlePercentAttribute(Player* player, uint32 attributeId, AsInt64 const& percentValue, bool apply)
    {
        float value = ToRuneFloat(percentValue);
        switch (attributeId)
        {
            case 0: // 法力值
                return player->HandleStatModifier(UNIT_MOD_MANA, TOTAL_PCT, value, apply);
            case 1: // 护甲值（保持符文现有属性编号兼容）
                return player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_PCT, value, apply);
            case 2: // 生命值
                return player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_PCT, value, apply);
            case 3: // 敏捷
                return player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_PCT, value, apply);
            case 4: // 力量
                return player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_PCT, value, apply);
            case 5: // 智力
                return player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_PCT, value, apply);
            case 6: // 精神
                return player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_PCT, value, apply);
            case 7: // 耐力
                return player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_PCT, value, apply);
            case 38: // 攻击强度
                return player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, value, apply);
            case 39: // 远程攻击强度
                return player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_PCT, value, apply);
            default:
                return false;
        }
    }

    // 应用百分比属性加成
    static void ApplyPercentAttributeBonus(Player* player, const std::string& effectValues)
    {
        auto attributes = RuneUtils::ParseEffectValues(effectValues);

        for (const auto& attr : attributes)
        {
            uint32 attributeId = attr.first;
            AsInt64 percentValue = attr.second;

            if (HandlePercentAttribute(player, attributeId, percentValue, true))
                LOG_DEBUG("rune.effect", ModuleOldFormat("应用百分比属性加成: 属性ID=%s, 百分比=%s%%", attributeId, Acore::ToString(percentValue)).c_str());
            else
                LOG_WARN("rune.effect", ModuleOldFormat("百分比属性ID %s 暂不支持，已跳过", attributeId).c_str());
        }

        player->UpdateAllStats();
    }

    // 移除百分比属性加成
    static void RemovePercentAttributeBonus(Player* player, const std::string& effectValues)
    {
        auto attributes = RuneUtils::ParseEffectValues(effectValues);

        for (const auto& attr : attributes)
        {
            uint32 attributeId = attr.first;
            AsInt64 percentValue = attr.second;

            if (HandlePercentAttribute(player, attributeId, percentValue, false))
                LOG_DEBUG("rune.effect", ModuleOldFormat("移除百分比属性加成: 属性ID=%s, 百分比=%s%%", attributeId, Acore::ToString(percentValue)).c_str());
            else
                LOG_WARN("rune.effect", ModuleOldFormat("百分比属性ID %s 暂不支持，移除时已跳过", attributeId).c_str());
        }

        player->UpdateAllStats();
    }

    // 应用百分比技能加成
    static void ApplyPercentSkillBonus(Player* player, const CustomRuneInfo& rune)
    {
        auto values = RuneUtils::ParseEffectValues(rune.effectValues);

        if (values.empty() || values[0].first != 99)
            return;

        AsInt64 percentBonus = values[0].second;

        LOG_DEBUG("rune.effect", ModuleOldFormat("符文 %s 百分比技能加成: %s%%", rune.runeId, Acore::ToString(percentBonus)).c_str());
    }

public:
    // 计算符文的技能伤害/治疗加成
    static int32 CalculateSkillBonus(const CustomRuneInfo& rune)
    {
        if (rune.effectType != 2 && rune.effectType != 4)
            return 0;

        auto values = RuneUtils::ParseEffectValues(rune.effectValues);

        if (values.empty() || values[0].first != 99)
            return 0;

        return RuneUtils::ToInt32Saturated(values[0].second);
    }

    // 获取符文的触发法术ID
    static uint32 GetTriggerSpellId(const CustomRuneInfo& rune)
    {
        return rune.spellId;
    }

    // 获取符文的触发几率
    static uint32 GetProcChance(const CustomRuneInfo& rune)
    {
        return rune.procChance;
    }

    // 获取符文的冷却时间
    static uint32 GetCooldown(const CustomRuneInfo& rune)
    {
        return rune.cooldown;
    }

    // 检查符文是否可以触发
    static bool CanProcRune(const CustomRuneInfo& rune, uint32 currentTime, uint32 lastProcTime)
    {
        // 检查冷却
        if (rune.cooldown > 0)
        {
            uint32 timeSinceLastProc = currentTime - lastProcTime;
            if (timeSinceLastProc < rune.cooldown)
            {
                return false;
            }
        }

        // 检查触发几率
        if (rune.procChance > 0 && rune.procChance < 100)
        {
            uint32 roll = urand(1, 100);
            if (roll > rune.procChance)
            {
                return false;
            }
        }

        return true;
    }
};

#endif // RUNE_EFFECT_HANDLER_H
