#ifndef RUNE_TYPES_H
#define RUNE_TYPES_H

#include "Common.h"
#include <string>

// 符文类型枚举
enum CustomRuneType
{
    CUSTOM_RUNE_TYPE_NONE      = 0,
    CUSTOM_RUNE_TYPE_ATTACK    = 1,    // 攻击符文
    CUSTOM_RUNE_TYPE_DEFENSE   = 2,    // 防御符文
    CUSTOM_RUNE_TYPE_UTILITY   = 3,    // 功能符文
    CUSTOM_RUNE_TYPE_SPECIAL   = 4     // 特殊符文
};

// 符文品质枚举
enum CustomRuneQuality
{
    CUSTOM_RUNE_QUALITY_COMMON     = 0,    // 普通
    CUSTOM_RUNE_QUALITY_UNCOMMON   = 1,    // 优秀
    CUSTOM_RUNE_QUALITY_RARE       = 2,    // 精良
    CUSTOM_RUNE_QUALITY_EPIC       = 3,    // 史诗
    CUSTOM_RUNE_QUALITY_LEGENDARY  = 4     // 传说
};

// 符文效果类型枚举
enum CustomRuneEffectType
{
    CUSTOM_RUNE_EFFECT_ATTRIBUTE_BONUS     = 0,    // 属性加成
    CUSTOM_RUNE_EFFECT_SPELL_TRIGGER       = 1,    // 触发法术
    CUSTOM_RUNE_EFFECT_PROC_CHANCE         = 2,    // 触发几率
    CUSTOM_RUNE_EFFECT_SPECIAL_EFFECT      = 3     // 特殊效果
};

// 符文信息结构
struct CustomRuneInfo
{
    uint32 runeId;                  // 符文ID
    uint32 itemId;                  // 物品ID (0=不绑定, >0=绑定物品ID)
    std::string name;               // 符文名称
    uint32 runeSlot;                // 符文槽位 (1-28)
    uint32 runeQuality;             // 符文品质
    uint32 spellId;                 // 关联法术ID
    uint32 cooldown;                // 冷却时间
    uint32 procChance;              // 触发几率
    uint32 effectType;              // 效果类型 (0=无加成, 1=属性加成, 2=技能加成, 3=百分比属性, 4=百分比技能)
    std::string effectValues;       // 效果值 (格式: 属性类型 值,属性类型 值)
    std::string effectDescription;  // 效果描述

    // 默认构造函数
    CustomRuneInfo() = default;

    // 复制构造函数
    CustomRuneInfo(const CustomRuneInfo&) = default;

    // 移动构造函数
    CustomRuneInfo(CustomRuneInfo&& other) noexcept
        : runeId(other.runeId), itemId(other.itemId), name(std::move(other.name)),
          runeSlot(other.runeSlot), runeQuality(other.runeQuality), spellId(other.spellId), cooldown(other.cooldown),
          procChance(other.procChance), effectType(other.effectType), effectValues(std::move(other.effectValues)),
          effectDescription(std::move(other.effectDescription))
    {
    }

    // 复制赋值运算符
    CustomRuneInfo& operator=(const CustomRuneInfo&) = default;

    // 移动赋值运算符
    CustomRuneInfo& operator=(CustomRuneInfo&& other) noexcept
    {
        if (this != &other)
        {
            runeId = other.runeId;
            itemId = other.itemId;
            name = std::move(other.name);
            runeSlot = other.runeSlot;
            runeQuality = other.runeQuality;
            spellId = other.spellId;
            cooldown = other.cooldown;
            procChance = other.procChance;
            effectType = other.effectType;
            effectValues = std::move(other.effectValues);
            effectDescription = std::move(other.effectDescription);
        }
        return *this;
    }
};

// 符文存储结构
// 插槽ID和符文ID使用逗号分隔的字符串存储
struct CustomRuneStorage
{
    uint32 recordId;                // 记录ID
    uint32 characterGuid;           // 角色GUID
    uint32 itemId;                  // 物品ID
    uint32 itemGuid;                // 物品GUID
    std::string slotIds;            // 插槽ID列表 (逗号分隔, 如: "1,2,3,4,5")
    std::string runeIds;            // 符文ID列表 (逗号分隔, 如: "1001,1002,0,0,0", 0表示空插槽)

    // 默认构造函数
    CustomRuneStorage() = default;

    // 复制构造函数
    CustomRuneStorage(const CustomRuneStorage&) = default;

    // 移动构造函数
    CustomRuneStorage(CustomRuneStorage&& other) noexcept
        : recordId(other.recordId), characterGuid(other.characterGuid), itemId(other.itemId),
          itemGuid(other.itemGuid), slotIds(std::move(other.slotIds)), runeIds(std::move(other.runeIds))
    {
    }

    // 复制赋值运算符
    CustomRuneStorage& operator=(const CustomRuneStorage&) = default;

    // 移动赋值运算符
    CustomRuneStorage& operator=(CustomRuneStorage&& other) noexcept
    {
        if (this != &other)
        {
            recordId = other.recordId;
            characterGuid = other.characterGuid;
            itemId = other.itemId;
            itemGuid = other.itemGuid;
            slotIds = std::move(other.slotIds);
            runeIds = std::move(other.runeIds);
        }
        return *this;
    }
};

#endif // RUNE_TYPES_H
