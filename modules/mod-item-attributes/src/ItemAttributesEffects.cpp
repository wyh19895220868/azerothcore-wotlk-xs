#include "AsInt256.h"
#include "StringConvert.h"
#include "ItemAttributesEffects.h"
#include "Logging/Log.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "ItemAttributesDBHelper.h"
#include "StringConvert.h"
#include "Util.h"
#include <sstream>
#include <chrono>
#include "ModuleFormat.h"

namespace
{
float ToStatFloat(AsInt64 const& value)
{
    return float(Acore::Number::ToInt32Saturated(value));
}

int32 ToLegacyInt32(AsInt64 const& value)
{
    return Acore::Number::ToInt32Saturated(value);
}

int32 ToRatingValue(AsInt64 const& value)
{
    return ToLegacyInt32(value);
}

uint32 ToSettingUInt32(AsInt64 const& value)
{
    if (value <= 0)
        return 0;

    return Acore::Number::ToUInt32Saturated(static_cast<uint64>(value));
}

bool TryGetWideItemMod(uint32 attributeType, ItemModType& mod)
{
    if (attributeType == 2 || (attributeType >= 8 && attributeType <= 11) || attributeType >= MAX_ITEM_MOD)
        return false;

    if (attributeType == 40)
        mod = ITEM_MOD_ATTACK_POWER;
    else if (attributeType == ITEM_MOD_SPELL_HEALING_DONE || attributeType == ITEM_MOD_SPELL_DAMAGE_DONE)
        mod = ITEM_MOD_SPELL_POWER;
    else
        mod = ItemModType(attributeType);

    return true;
}

void ApplyWideItemAttributeStore(Player* player, uint32 attributeType, AsInt64 const& value, bool apply)
{
    ItemModType mod;
    if (!player || value == 0 || !TryGetWideItemMod(attributeType, mod))
        return;

    int32 const legacyValue = Acore::Number::ToInt32Saturated(value);
    AsInt64 compatValue = legacyValue;
    switch (mod)
    {
        case ITEM_MOD_MANA:
        case ITEM_MOD_HEALTH:
        case ITEM_MOD_AGILITY:
        case ITEM_MOD_STRENGTH:
        case ITEM_MOD_INTELLECT:
        case ITEM_MOD_SPIRIT:
        case ITEM_MOD_STAMINA:
        case ITEM_MOD_ATTACK_POWER:
        case ITEM_MOD_RANGED_ATTACK_POWER:
        case ITEM_MOD_BLOCK_VALUE:
            compatValue = AsInt64(float(legacyValue));
            break;
        default:
            break;
    }

    player->ApplyAsStatBonus(mod, value, compatValue, apply);
}
}

// 【线程安全】Meyer's Singleton - C++11保证静态局部变量初始化的线程安全性
// 编译器会自动添加同步机制，确保多线程并发调用时只初始化一次
ItemAttributesEffects* ItemAttributesEffects::instance()
{
    static ItemAttributesEffects instance;
    return &instance;
}

// 【根本性修复】安全获取实例方法
ItemAttributesEffects* ItemAttributesEffects::SafeInstance()
{
    ItemAttributesEffects* inst = instance();
    return (inst && inst->IsInitialized()) ? inst : nullptr;
}

void ItemAttributesEffects::Initialize()
{
    // 防止重复初始化
    if (_isInitialized)
    {
        LOG_WARN("module.itemattributes", "ItemAttributesEffects::Initialize() 被重复调用，跳过初始化");
        return;
    }

    // 注册基础属性效果处理器

    auto registerRating = [this](uint32 attributeType, CombatRating rating, std::string name)
    {
        RegisterAttributeEffectHandler(attributeType,
            [rating](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value)
            {
                player->ApplyRatingMod(rating, ToRatingValue(value), true);
            },
            [rating](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value)
            {
                player->ApplyRatingMod(rating, ToRatingValue(value), false);
            },
            [name](Item*, ItemAttributeTemplate const*, AsInt64 value)
            {
                return name + " +" + Acore::ToString(value);
            });
    };

    // 护甲属性 (类型 2，自定义基础属性；官方 ItemMod 没有普通护甲枚举)
    RegisterAttributeEffectHandler(2,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "护甲 +" << value;
            return ss.str();
        }
    );

    // 力量属性 (类型 4)
    RegisterAttributeEffectHandler(4,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            // 【关键修复】安全获取物品GUID用于日志
            uint64 itemGuid = 0;
            if (item) {
                ObjectGuid guidObj = item->GetGUID();
                if (!guidObj.IsEmpty()) itemGuid = guidObj.GetCounter();
            }
            LOG_DEBUG("module.itemattributes.effects", ModuleOldFormat("【应用】力量 +%s (玩家:%s, 物品GUID:%s)", Acore::ToString(value), player->GetName(), itemGuid).c_str());
            player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            // 【关键修复】安全获取物品GUID用于日志
            uint64 itemGuid = 0;
            if (item) {
                ObjectGuid guidObj = item->GetGUID();
                if (!guidObj.IsEmpty()) itemGuid = guidObj.GetCounter();
            }
            LOG_DEBUG("module.itemattributes.effects", ModuleOldFormat("【移除】力量 -%s (玩家:%s, 物品GUID:%s)", Acore::ToString(value), player->GetName(), itemGuid).c_str());
            player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "力量 +" << value;
            return ss.str();
        }
    );

    // 敏捷属性 (类型 3)
    RegisterAttributeEffectHandler(3,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "敏捷 +" << value;
            return ss.str();
        }
    );

    // 耐力属性 (类型 7)
    RegisterAttributeEffectHandler(7,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "耐力 +" << value;
            return ss.str();
        }
    );

    // 智力属性 (类型 5)
    RegisterAttributeEffectHandler(5,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "智力 +" << value;
            return ss.str();
        }
    );

    // 精神属性 (类型 6)
    RegisterAttributeEffectHandler(6,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "精神 +" << value;
            return ss.str();
        }
    );

    // 生命值属性 (类型 1)
    RegisterAttributeEffectHandler(1,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "生命值 +" << value;
            return ss.str();
        }
    );

    // 法力值属性 (类型 0)
    RegisterAttributeEffectHandler(0,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_MANA, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_MANA, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "法力值 +" << value;
            return ss.str();
        }
    );

    // 攻击强度属性 (类型 38)

    registerRating(16, CR_HIT_MELEE, "近战命中等级");
    registerRating(17, CR_HIT_RANGED, "远程命中等级");
    registerRating(18, CR_HIT_SPELL, "法术命中等级");
    registerRating(19, CR_CRIT_MELEE, "近战暴击等级");
    registerRating(20, CR_CRIT_RANGED, "远程暴击等级");
    registerRating(21, CR_CRIT_SPELL, "法术暴击等级");
    registerRating(22, CR_HIT_TAKEN_MELEE, "近战命中回避等级");
    registerRating(23, CR_HIT_TAKEN_RANGED, "远程命中回避等级");
    registerRating(24, CR_HIT_TAKEN_SPELL, "法术命中回避等级");
    registerRating(25, CR_CRIT_TAKEN_MELEE, "近战致命一击回避等级");
    registerRating(26, CR_CRIT_TAKEN_RANGED, "远程致命一击回避等级");
    registerRating(27, CR_CRIT_TAKEN_SPELL, "法术致命一击回避等级");
    registerRating(28, CR_HASTE_MELEE, "近战急速等级");
    registerRating(29, CR_HASTE_RANGED, "远程急速等级");
    registerRating(30, CR_HASTE_SPELL, "法术急速等级");
    RegisterAttributeEffectHandler(33,
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { int32 v = ToRatingValue(value); player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, v, true); player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, v, true); player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, v, true); },
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { int32 v = ToRatingValue(value); player->ApplyRatingMod(CR_HIT_TAKEN_MELEE, v, false); player->ApplyRatingMod(CR_HIT_TAKEN_RANGED, v, false); player->ApplyRatingMod(CR_HIT_TAKEN_SPELL, v, false); },
        [](Item*, ItemAttributeTemplate const*, AsInt64 value) { return "命中回避等级 +" + Acore::ToString(value); });
    RegisterAttributeEffectHandler(34,
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { int32 v = ToRatingValue(value); player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, v, true); player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, v, true); player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, v, true); },
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { int32 v = ToRatingValue(value); player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, v, false); player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, v, false); player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, v, false); },
        [](Item*, ItemAttributeTemplate const*, AsInt64 value) { return "致命一击回避等级 +" + Acore::ToString(value); });

    RegisterAttributeEffectHandler(38,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "攻击强度 +" << value;
            return ss.str();
        }
    );

    // 法术强度属性 (类型 45)
    RegisterAttributeEffectHandler(45,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplySpellPowerBonus(ToLegacyInt32(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplySpellPowerBonus(ToLegacyInt32(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "法术强度 +" << value;
            return ss.str();
        }
    );

    // 暴击等级属性 (类型 32)
    RegisterAttributeEffectHandler(32,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_CRIT_MELEE, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_CRIT_RANGED, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_CRIT_SPELL, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_CRIT_MELEE, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_CRIT_RANGED, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_CRIT_SPELL, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "暴击等级 +" << value;
            return ss.str();
        }
    );

    // 命中等级属性 (类型 31)
    RegisterAttributeEffectHandler(31,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_HIT_MELEE, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_HIT_RANGED, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_HIT_SPELL, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_HIT_MELEE, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_HIT_RANGED, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_HIT_SPELL, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "命中等级 +" << value;
            return ss.str();
        }
    );

    // 急速等级属性 (类型 36)
    RegisterAttributeEffectHandler(36,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_HASTE_MELEE, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_HASTE_RANGED, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_HASTE_SPELL, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_HASTE_MELEE, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_HASTE_RANGED, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_HASTE_SPELL, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "急速等级 +" << value;
            return ss.str();
        }
    );

    // 护甲穿透等级属性 (类型 44)
    RegisterAttributeEffectHandler(44,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_ARMOR_PENETRATION, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_ARMOR_PENETRATION, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "护甲穿透等级 +" << value;
            return ss.str();
        }
    );

    // 法术穿透属性 (类型 47)
    RegisterAttributeEffectHandler(47,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplySpellPenetrationBonus(ToLegacyInt32(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplySpellPenetrationBonus(ToLegacyInt32(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "法术穿透 +" << value;
            return ss.str();
        }
    );

    // 防御等级属性 (类型 12)
    RegisterAttributeEffectHandler(12,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_DEFENSE_SKILL, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_DEFENSE_SKILL, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "防御等级 +" << value;
            return ss.str();
        }
    );

    // 躲闪等级属性 (类型 13)
    RegisterAttributeEffectHandler(13,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_DODGE, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_DODGE, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "躲闪等级 +" << value;
            return ss.str();
        }
    );

    // 招架等级属性 (类型 14)
    RegisterAttributeEffectHandler(14,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_PARRY, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_PARRY, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "招架等级 +" << value;
            return ss.str();
        }
    );

    // 格挡等级属性 (类型 15)
    RegisterAttributeEffectHandler(15,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_BLOCK, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_BLOCK, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "格挡等级 +" << value;
            return ss.str();
        }
    );

    // 韧性等级属性 (类型 35)
    RegisterAttributeEffectHandler(35,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, ToRatingValue(value), true);
            player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, ToRatingValue(value), false);
            player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "韧性等级 +" << value;
            return ss.str();
        }
    );

    // 精准等级属性 (类型 37)
    RegisterAttributeEffectHandler(37,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_EXPERTISE, ToRatingValue(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyRatingMod(CR_EXPERTISE, ToRatingValue(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "精准等级 +" << value;
            return ss.str();
        }
    );

    // 远程攻击强度属性 (类型 39)
    RegisterAttributeEffectHandler(39,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, ToStatFloat(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, ToStatFloat(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "远程攻击强度 +" << value;
            return ss.str();
        }
    );

    // 40在3.3.5中已废弃，作为兼容类型按近战攻击强度处理。
    RegisterAttributeEffectHandler(40,
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, ToStatFloat(value), true); },
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, ToStatFloat(value), false); },
        [](Item*, ItemAttributeTemplate const*, AsInt64 value) { return "形态攻击强度 +" + Acore::ToString(value); });

    // 41/42在3.3.5中已合并为法术强度，保留为兼容入口。
    RegisterAttributeEffectHandler(41,
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { player->ApplySpellPowerBonus(ToLegacyInt32(value), true); },
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { player->ApplySpellPowerBonus(ToLegacyInt32(value), false); },
        [](Item*, ItemAttributeTemplate const*, AsInt64 value) { return "法术治疗 +" + Acore::ToString(value); });
    RegisterAttributeEffectHandler(42,
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { player->ApplySpellPowerBonus(ToLegacyInt32(value), true); },
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { player->ApplySpellPowerBonus(ToLegacyInt32(value), false); },
        [](Item*, ItemAttributeTemplate const*, AsInt64 value) { return "法术伤害 +" + Acore::ToString(value); });

    // 法力恢复属性 (类型 43)
    RegisterAttributeEffectHandler(43,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyManaRegenBonus(ToLegacyInt32(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyManaRegenBonus(ToLegacyInt32(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "法力恢复 +" << value;
            return ss.str();
        }
    );

    // 生命恢复属性 (类型 46)
    RegisterAttributeEffectHandler(46,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyHealthRegenBonus(ToLegacyInt32(value), true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->ApplyHealthRegenBonus(ToLegacyInt32(value), false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "生命恢复 +" << value;
            return ss.str();
        }
    );

    // 格挡值属性 (类型 48)
    RegisterAttributeEffectHandler(48,
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, ToStatFloat(value), true);
        },
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, ToStatFloat(value), false);
        },
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "格挡值 +" << value;
            return ss.str();
        }
    );

    auto getWeaponUnitMod = [](Item* item) -> UnitMods
    {
        if (!item)
            return UNIT_MOD_END;

        switch (item->GetSlot())
        {
            case EQUIPMENT_SLOT_MAINHAND: return UNIT_MOD_DAMAGE_MAINHAND;
            case EQUIPMENT_SLOT_OFFHAND:  return UNIT_MOD_DAMAGE_OFFHAND;
            case EQUIPMENT_SLOT_RANGED:   return UNIT_MOD_DAMAGE_RANGED;
            default:                      return UNIT_MOD_END;
        }
    };

    auto registerWeaponDamage = [this, getWeaponUnitMod](uint32 attributeType, bool isDps, std::string name)
    {
        auto applyDamage = [getWeaponUnitMod, isDps](Player* player, Item* item, AsInt64 value, bool apply)
        {
            UnitMods unitMod = getWeaponUnitMod(item);
            if (unitMod == UNIT_MOD_END)
                return;

            float amount = ToStatFloat(value);
            if (isDps && item->GetTemplate()->Delay)
                amount *= float(item->GetTemplate()->Delay) / 1000.0f;
            player->HandleStatModifier(unitMod, TOTAL_VALUE, amount, apply);
        };

        RegisterAttributeEffectHandler(attributeType,
            [applyDamage](Player* player, Item* item, ItemAttributeTemplate const*, AsInt64 value) { applyDamage(player, item, value, true); },
            [applyDamage](Player* player, Item* item, ItemAttributeTemplate const*, AsInt64 value) { applyDamage(player, item, value, false); },
            [name](Item*, ItemAttributeTemplate const*, AsInt64 value) { return name + " +" + Acore::ToString(value); });
    };

    // 120: 武器速度，数值使用毫秒（2600 显示为 2.60 秒）。
    RegisterAttributeEffectHandler(120,
        [](Player* player, Item* item, ItemAttributeTemplate const*, AsInt64 value)
        {
            if (!item || value <= 0)
                return;
            uint32 delay = ToSettingUInt32(value);
            switch (item->GetSlot())
            {
                case EQUIPMENT_SLOT_MAINHAND: player->SetAttackTime(BASE_ATTACK, delay); break;
                case EQUIPMENT_SLOT_OFFHAND:  player->SetAttackTime(OFF_ATTACK, delay); break;
                case EQUIPMENT_SLOT_RANGED:   player->SetAttackTime(RANGED_ATTACK, delay); break;
                default: break;
            }
        },
        [](Player* player, Item* item, ItemAttributeTemplate const*, AsInt64)
        {
            if (!item)
                return;
            uint32 delay = item->GetTemplate()->Delay ? item->GetTemplate()->Delay : BASE_ATTACK_TIME;
            switch (item->GetSlot())
            {
                case EQUIPMENT_SLOT_MAINHAND: player->SetAttackTime(BASE_ATTACK, delay); break;
                case EQUIPMENT_SLOT_OFFHAND:  player->SetAttackTime(OFF_ATTACK, delay); break;
                case EQUIPMENT_SLOT_RANGED:   player->SetAttackTime(RANGED_ATTACK, delay); break;
                default: break;
            }
        },
        [](Item*, ItemAttributeTemplate const*, AsInt64 value)
        {
            return "武器速度 " + Acore::ToString(value) + " 毫秒";
        });

    registerWeaponDamage(121, false, "武器伤害");
    registerWeaponDamage(122, true, "每秒伤害");

    // 元素伤害以最小/最大成对配置。当前战斗核心的武器基础伤害为单一伤害区间，
    // 因此两端分别计入武器区间；学派名称用于配置和客户端展示。
    for (uint32 type = 123; type <= 134; ++type)
        registerWeaponDamage(type, false, "元素武器伤害");

    RegisterAttributeEffectHandler(135,
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, ToStatFloat(value), true); },
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, ToStatFloat(value), false); },
        [](Item*, ItemAttributeTemplate const*, AsInt64 value) { return "护甲 +" + Acore::ToString(value); });
    RegisterAttributeEffectHandler(136,
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, ToStatFloat(value), true); },
        [](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { player->HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, ToStatFloat(value), false); },
        [](Item*, ItemAttributeTemplate const*, AsInt64 value) { return "格挡值 +" + Acore::ToString(value); });
    RegisterAttributeEffectHandler(137,
        [](Player*, Item*, ItemAttributeTemplate const*, AsInt64) {},
        [](Player*, Item*, ItemAttributeTemplate const*, AsInt64) {},
        [](Item*, ItemAttributeTemplate const*, AsInt64 value) { return "物品等级 +" + Acore::ToString(value); });

    UnitMods const resistanceMods[] = {
        UNIT_MOD_RESISTANCE_HOLY, UNIT_MOD_RESISTANCE_FIRE, UNIT_MOD_RESISTANCE_NATURE,
        UNIT_MOD_RESISTANCE_FROST, UNIT_MOD_RESISTANCE_SHADOW, UNIT_MOD_RESISTANCE_ARCANE
    };
    char const* resistanceNames[] = { "神圣抗性", "火焰抗性", "自然抗性", "冰霜抗性", "暗影抗性", "奥术抗性" };
    for (uint32 i = 0; i < 6; ++i)
    {
        UnitMods unitMod = resistanceMods[i];
        std::string name = resistanceNames[i];
        RegisterAttributeEffectHandler(150 + i,
            [unitMod](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { player->HandleStatModifier(unitMod, TOTAL_VALUE, ToStatFloat(value), true); },
            [unitMod](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value) { player->HandleStatModifier(unitMod, TOTAL_VALUE, ToStatFloat(value), false); },
            [name](Item*, ItemAttributeTemplate const*, AsInt64 value) { return name + " +" + Acore::ToString(value); });
    }

    auto registerCustomCombatStat = [this](uint32 attributeType)
    {
        RegisterAttributeEffectHandler(attributeType,
            [attributeType](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value)
            {
                AsInt64 realValue = AsInt64FromString(Acore::ToString(value));
                player->ApplyAsStatBonus(static_cast<ItemModType>(attributeType), realValue, realValue, true);
            },
            [attributeType](Player* player, Item*, ItemAttributeTemplate const*, AsInt64 value)
            {
                AsInt64 realValue = AsInt64FromString(Acore::ToString(value));
                player->ApplyAsStatBonus(static_cast<ItemModType>(attributeType), realValue, realValue, false);
            },
            [attributeType](Item*, ItemAttributeTemplate const*, AsInt64 value)
            {
                return "属性 " + Acore::ToString(attributeType) + " +" + Acore::ToString(value);
            });
    };

    for (uint32 type = 200; type <= 230; ++type)
        registerCustomCombatStat(type);
    for (uint32 type = 300; type <= 330; ++type)
        registerCustomCombatStat(type);
    registerCustomCombatStat(402);
    registerCustomCombatStat(403);

    // ============================================================
    // 自定义属性 (400+)
    // ============================================================

    // 经验获取加成属性 (类型 400)
    RegisterAttributeEffectHandler(400,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            // 这里需要自定义实现经验获取加成逻辑
            // 示例：记录玩家的经验获取加成
            // 使用玩家设置系统存储经验加成
            player->UpdatePlayerSetting("item_attributes", 400, ToSettingUInt32(value));
            AsInt64 realValue = AsInt64FromString(Acore::ToString(value));
            player->ApplyAsStatBonus(static_cast<ItemModType>(400), realValue, realValue, true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            // 移除经验获取加成
            player->UpdatePlayerSetting("item_attributes", 400, 0);
            AsInt64 realValue = AsInt64FromString(Acore::ToString(value));
            player->ApplyAsStatBonus(static_cast<ItemModType>(400), realValue, realValue, false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "经验获取 +" << value << "%";
            return ss.str();
        }
    );

    // 掉落加成属性 (类型 401)
    RegisterAttributeEffectHandler(401,
        // 应用效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            // 这里需要自定义实现掉落加成逻辑
            // 示例：记录玩家的掉落加成
            // 使用玩家设置系统存储掉落加成
            player->UpdatePlayerSetting("item_attributes", 401, ToSettingUInt32(value));
            AsInt64 realValue = AsInt64FromString(Acore::ToString(value));
            player->ApplyAsStatBonus(static_cast<ItemModType>(401), realValue, realValue, true);
        },
        // 移除效果
        [](Player* player, Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            // 移除掉落加成
            player->UpdatePlayerSetting("item_attributes", 401, 0);
            AsInt64 realValue = AsInt64FromString(Acore::ToString(value));
            player->ApplyAsStatBonus(static_cast<ItemModType>(401), realValue, realValue, false);
        },
        // 生成描述
        [](Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 value) {
            std::ostringstream ss;
            ss << "掉落几率 +" << value << "%";
            return ss.str();
        }
    );

    // 【根本性修复】标记为已初始化
    _isInitialized = true;
}

void ItemAttributesEffects::RegisterAttributeEffectHandler(uint32 attributeType, AttributeEffectHandler handler, AttributeEffectRemover remover, AttributeDescriptionGenerator descGenerator)
{
    _attributeEffectHandlers[attributeType] = handler;
    _attributeEffectRemovers[attributeType] = remover;
    _attributeDescriptionGenerators[attributeType] = descGenerator;
}

// 【审计修复】按玩家跟踪批量更新状态
bool ItemAttributesEffects::IsBatchUpdateInProgress(uint64 playerGuid)
{
    std::lock_guard<std::mutex> lock(_batchUpdateMutex);
    return _batchUpdatePlayers.find(playerGuid) != _batchUpdatePlayers.end();
}

void ItemAttributesEffects::SetBatchUpdateInProgress(uint64 playerGuid, bool inProgress)
{
    std::lock_guard<std::mutex> lock(_batchUpdateMutex);
    if (inProgress)
        _batchUpdatePlayers.insert(playerGuid);
    else
        _batchUpdatePlayers.erase(playerGuid);
}

void ItemAttributesEffects::RequestDeferredStatsUpdate(Player* player)
{
    if (!player)
        return;

    // 登录加载阶段尽量不刷新，交给登录流程/其他系统统一刷新一次
    if (player->isBeingLoaded())
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();

    {
        std::lock_guard<std::mutex> lock(_deferredUpdateMutex);
        if (!_deferredUpdatePlayers.insert(playerGuid).second)
            return; // 已经安排过更新
    }

    player->m_Events.AddEventAtOffset([this, player, playerGuid]()
    {
        {
            std::lock_guard<std::mutex> lock(_deferredUpdateMutex);
            _deferredUpdatePlayers.erase(playerGuid);
        }

        if (!player || !player->IsInWorld() || player->isBeingLoaded())
            return;

        if (!player->CanModifyStats())
            return;

        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateSpellDamageAndHealingBonus();
    }, std::chrono::milliseconds(1));
}

void ItemAttributesEffects::ApplyItemAttributeEffects(Player* player, Item* item)
{
    if (!_isInitialized)
        return;

    if (!player || !item)
        return;

    std::vector<uint32> attributes;
    std::vector<AsInt64> values;

    ItemAttributesLoader* loader = sItemAttributesLoader;
    if (!loader)
        return;

    if (!loader->GetItemAttributesWithValues(item, attributes, values))
    {
        return;
    }

    // 确保属性和值的数量匹配
    if (attributes.size() != values.size())
    {
        // 【关键修复】安全获取物品GUID用于日志
        ObjectGuid itemGuidObj = item->GetGUID();
        uint64 itemGuid = itemGuidObj.IsEmpty() ? 0 : itemGuidObj.GetCounter();
        LOG_ERROR("module.itemattributes", ModuleOldFormat("【应用属性】错误 - 物品 GUID %s 的属性数量(%s)和值数量(%s)不匹配！", itemGuid, attributes.size(), values.size()).c_str());
        return;
    }

    if (attributes.empty())
        return;

    // 【审计修复】使用按玩家跟踪的批量更新标志，避免跨玩家并发问题
    uint64 playerGuid = player->GetGUID().GetCounter();
    bool batchInProgress = IsBatchUpdateInProgress(playerGuid);

    // 【性能优化】批量应用期间由上层统一刷新；普通情况下合并到一次延迟刷新
    bool canModifyBefore = player->CanModifyStats();
    bool needsDeferUpdate = !batchInProgress;
    if (needsDeferUpdate && canModifyBefore)
        player->SetCanModifyStats(false);

    // 应用每个属性效果
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        uint32 attributeType = attributes[i];
        AsInt64 value = values[i];

        auto handlerItr = _attributeEffectHandlers.find(attributeType);
        if (handlerItr != _attributeEffectHandlers.end())
        {
            if (handlerItr->second)
            {
                ApplyWideItemAttributeStore(player, attributeType, value, true);
                handlerItr->second(player, item, nullptr, value);
            }
            else
            {
                LOG_ERROR("module.itemattributes", ModuleOldFormat("属性类型 %s 的处理器为空指针！", attributeType).c_str());
            }
        }
        else
        {
            LOG_WARN("module.itemattributes", ModuleOldFormat("未找到属性类型 %s 的处理器", attributeType).c_str());
        }
    }

    if (needsDeferUpdate && canModifyBefore)
        player->SetCanModifyStats(true);

    if (needsDeferUpdate)
        RequestDeferredStatsUpdate(player);
}

void ItemAttributesEffects::RemoveItemAttributeEffects(Player* player, Item* item)
{
    if (!_isInitialized)
        return;

    if (!player || !item)
        return;

    std::vector<uint32> attributes;
    std::vector<AsInt64> values;

    ItemAttributesLoader* loader = sItemAttributesLoader;
    if (!loader)
        return;

    if (!loader->GetItemAttributesWithValues(item, attributes, values))
    {
        return;
    }

    // 确保属性和值的数量匹配
    if (attributes.size() != values.size())
    {
        // 【关键修复】安全获取物品GUID用于日志
        ObjectGuid itemGuidObj = item->GetGUID();
        uint64 itemGuid = itemGuidObj.IsEmpty() ? 0 : itemGuidObj.GetCounter();
        LOG_ERROR("module.itemattributes", ModuleOldFormat("【移除属性】错误 - 物品 GUID %s 的属性数量(%s)和值数量(%s)不匹配！", itemGuid, attributes.size(), values.size()).c_str());
        return;
    }

    if (attributes.empty())
        return;

    // 【审计修复】使用按玩家跟踪的批量更新标志，避免跨玩家并发问题
    uint64 playerGuid = player->GetGUID().GetCounter();
    bool batchInProgress = IsBatchUpdateInProgress(playerGuid);

    bool canModifyBefore = player->CanModifyStats();
    bool needsDeferUpdate = !batchInProgress;
    if (needsDeferUpdate && canModifyBefore)
        player->SetCanModifyStats(false);

    // 移除每个属性效果
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        uint32 attributeType = attributes[i];
        AsInt64 value = values[i];

        auto removerItr = _attributeEffectRemovers.find(attributeType);
        if (removerItr != _attributeEffectRemovers.end())
        {
            if (removerItr->second)
            {
                ApplyWideItemAttributeStore(player, attributeType, value, false);
                removerItr->second(player, item, nullptr, value);
            }
            else
            {
                LOG_ERROR("module.itemattributes", ModuleOldFormat("属性类型 %s 的移除器为空指针！", attributeType).c_str());
            }
        }
        else
        {
            LOG_WARN("module.itemattributes", ModuleOldFormat("未找到属性类型 %s 的移除器", attributeType).c_str());
        }
    }

    if (needsDeferUpdate && canModifyBefore)
        player->SetCanModifyStats(true);

    if (needsDeferUpdate)
        RequestDeferredStatsUpdate(player);
}

void ItemAttributesEffects::UpdateItemAttributeEffects(Player* player)
{
    if (!player)
        return;

    // 【审计修复】使用按玩家跟踪的批量更新标志，避免跨玩家并发问题
    uint64 playerGuid = player->GetGUID().GetCounter();

    // 【性能优化-批量更新】设置批量更新标志，防止每个装备都触发UpdateAllStats
    SetBatchUpdateInProgress(playerGuid, true);

    // 【性能优化-批量更新】禁用自动属性更新，处理完所有装备后统一更新一次
    player->SetCanModifyStats(false);

    // 移除所有物品属性效果
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            RemoveItemAttributeEffects(player, item);
        }
    }

    // 重新应用所有物品属性效果
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            ApplyItemAttributeEffects(player, item);
        }
    }

    // 【性能优化-批量更新】恢复批量更新标志
    SetBatchUpdateInProgress(playerGuid, false);

    // 【性能优化-批量更新】重新启用属性更新并统一计算一次
    // 这样所有装备的属性变化只触发一次UpdateStats
    player->SetCanModifyStats(true);
    player->UpdateAllStats();
}

void ItemAttributesEffects::RemoveItemAttributeEffectsByGuid(Player* player, uint64 itemGuid)
{
    // 【根本性修复】检查初始化状态
    if (!_isInitialized)
    {
        LOG_ERROR("module.itemattributes", "【致命错误】RemoveItemAttributeEffectsByGuid 被调用，但系统尚未初始化！");
        return;
    }

    if (!player || itemGuid == 0)
        return;

    // 使用DBHelper读取属性
    auto data = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);  // 【智能指针修复】自动管理内存

    if (!data)
        return;

    // 合并基础属性和追加属性
    std::vector<uint32> attributes;
    std::vector<AsInt64> values;

    attributes.insert(attributes.end(), data->baseAttributeIds.begin(), data->baseAttributeIds.end());
    attributes.insert(attributes.end(), data->additionalAttributeIds.begin(), data->additionalAttributeIds.end());

    values.insert(values.end(), data->baseAttributeValues.begin(), data->baseAttributeValues.end());
    values.insert(values.end(), data->additionalAttributeValues.begin(), data->additionalAttributeValues.end());

    // 【智能指针修复】移除手动 delete，unique_ptr 自动清理

    if (attributes.empty())
        return;

    // 确保属性和值的数量匹配
    if (attributes.size() != values.size())
    {
        LOG_ERROR("module.itemattributes", ModuleOldFormat("【根据GUID移除属性】错误 - 物品 GUID %s 的属性数量(%s)和值数量(%s)不匹配！", itemGuid, attributes.size(), values.size()).c_str());
        return;
    }

    bool canModifyBefore = player->CanModifyStats();
    if (canModifyBefore)
        player->SetCanModifyStats(false);

    // 移除每个属性效果
    // 【重要】数据库中保存的是属性类型，不是属性模板ID
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        uint32 attributeId = attributes[i];
        AsInt64 value = values[i];

        // 使用属性类型查询属性模板（因为数据库中保存的是属性类型）
        ItemAttributeTemplate const* attributeTemplate = sItemAttributesLoader->GetItemAttributeTemplateByType(attributeId);
        if (!attributeTemplate)
        {
            LOG_WARN("module.itemattributes", ModuleOldFormat("未找到属性类型对应的属性模板 类型ID: %s", attributeId).c_str());
            continue;
        }

        // 查找属性类型对应的效果移除器
        auto removerItr = _attributeEffectRemovers.find(attributeTemplate->attributeType);
        if (removerItr != _attributeEffectRemovers.end())
        {
            // 注意：这里我们没有Item对象，传nullptr
            // 移除器不应该依赖Item对象，只使用attributeTemplate和value
            ApplyWideItemAttributeStore(player, attributeTemplate->attributeType, value, false);
            removerItr->second(player, nullptr, attributeTemplate, value);
        }
        else
        {
            LOG_WARN("module.itemattributes", ModuleOldFormat("未找到属性类型 %s 的移除器", attributeTemplate->attributeType).c_str());
        }
    }

    if (canModifyBefore)
        player->SetCanModifyStats(true);

    // 根据GUID移除时同样请求一次合并刷新，避免客户端属性不同步
    RequestDeferredStatsUpdate(player);
}

std::string ItemAttributesEffects::GetAttributeDescription(Item* item, uint32 attributeId)
{
    // 【根本性修复】检查初始化状态
    if (!_isInitialized)
    {
        LOG_ERROR("module.itemattributes", "【致命错误】GetAttributeDescription 被调用，但系统尚未初始化！");
        return "【未初始化】";
    }

    if (!item)
        return "";

    // 【重要】attributeId实际上是属性类型，不是模板ID
    ItemAttributeTemplate const* attributeTemplate = sItemAttributesLoader->GetItemAttributeTemplateByType(attributeId);
    if (!attributeTemplate)
        return "";

    // 【审计修复】从数据库读取实际保存的值，而不是重新计算随机值
    AsInt64 value = 0;
    uint64 itemGuid = item->GetGUID().GetCounter();
    auto data = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);
    if (data)
    {
        // 在基础属性中查找
        for (size_t i = 0; i < data->baseAttributeIds.size(); ++i)
        {
            if (data->baseAttributeIds[i] == attributeId)
            {
                value = data->baseAttributeValues[i];
                break;
            }
        }
        // 如果没找到，在追加属性中查找
        if (value == 0)
        {
            for (size_t i = 0; i < data->additionalAttributeIds.size(); ++i)
            {
                if (data->additionalAttributeIds[i] == attributeId)
                {
                    value = data->additionalAttributeValues[i];
                    break;
                }
            }
        }
    }

    auto generatorItr = _attributeDescriptionGenerators.find(attributeTemplate->attributeType);
    if (generatorItr != _attributeDescriptionGenerators.end())
    {
        return generatorItr->second(item, attributeTemplate, value);
    }

    // 默认描述
    std::ostringstream ss;
    ss << attributeTemplate->clientDisplay << ": " << value;
    return ss.str();
}
