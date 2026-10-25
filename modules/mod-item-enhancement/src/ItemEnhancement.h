#ifndef _ITEM_ENHANCEMENT_H_
#define _ITEM_ENHANCEMENT_H_

#include "Define.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "Configuration/Config.h"
#include "Chat.h"
#include "Item.h"
#include "Logging/Log.h"
#include "DatabaseEnv.h"
#include "WorldSession.h"
#include "GameTime.h"
#include "World.h"
#include <unordered_map>
#include <vector>
#include <map>
#include <string>

// 定义强化记录结构 - 与物品强化_记录表对应
struct EnhancementRecord
{
    uint32 itemGuid;                // guid
    uint32 ownerGuid;               // owner_guid
    uint32 itemTemplateId;          // 物品模板ID
    uint32 group;                   // 强化组号（用于选择对应的强化模板）
    uint32 level;                   // 强化等级
    uint32 enhancementExp;          // 强化经验
    std::string statValues;         // 属性值，格式：属性类型 值,属性类型 值
    uint32 qualityLevel;            // 品质等级
    uint32 qualityUpgradeCount;     // 品质进阶次数
    uint32 enhancementCount;        // 强化次数
    uint32 failureCount;            // 失败次数
    uint32 lastEnhanceTime;         // 最后强化时间
};

// 物品强化模板数据 - 与_模板_物品强化表对应
struct EnhancementTemplate
{
    uint32 id;                      // id
    std::string comment;            // 注释
    uint32 group;                   // 组
    uint32 level;                   // 等级
    float chance;                   // 几率
    uint32 itemStatGroup;           // 物品属性_模板_组
    uint32 distributionType;        // 平均随机分配
    AsInt64 statValue1;             // 属性值1
    AsInt64 statValue2;             // 属性值2
    AsInt64 statPercent1;           // 属性百分比1
    AsInt64 statPercent2;           // 属性百分比2
    AsInt64 bonusStat;              // 达到该强化等级后的奖励属性
    AsInt64 bonusStatPercent;       // 达到该强化等级后的奖励属性百分比
    std::string skillIds;           // 物品技能_模板_id_多个逗号隔开
    uint32 requirementId;           // 强化需求
    uint8 failureType;              // 失败处理
    uint32 failureDropLevels;       // 失败降级级数（失败处理=3）
    uint32 pityFailureCount;        // 连续失败达到该次数后必定成功
    float pityChancePerFailure;     // 每次连续失败增加的成功率
    uint32 luckItemId;              // 自动使用的幸运物品
    uint32 luckItemCount;
    float luckChanceBonus;
    uint32 protectionItemId;        // 降级/清零/摧毁时自动消耗的保护物品
    uint32 protectionItemCount;
};

// 【安全修复 2026-06-11】旧版 EnhanceItem 已删除（无需求消耗，存在绕过漏洞），统一使用 sItemEnhancementMgr->EnhanceItem
extern bool CanEnhanceItem(Item* item);
extern bool CanEnhanceItem(Player* player, Item* item);
extern Item* FindPlayerItem(Player* player, uint32 itemId);
extern Item* FindPlayerItemByGuid(Player* player, uint32 itemGuid);
extern void ShowEnhancementInfo(Player* player, Item* item);
extern void LoadItemEnhancementConfig();
extern void LoadEnhancementTemplates();
extern void LoadEnhancementRecords();
extern EnhancementRecord const* GetEnhancementRecord(uint32 itemGuid);
extern void SaveEnhancementRecord(const EnhancementRecord& record);
extern void StoreGuidInEnchantmentSlot(Item* item);

// 物品强化系统配置（默认值）
extern bool ItemEnhancementEnabled;
extern bool ItemEnhancementFailureProtectionEnabled;
extern bool ItemEnhancementShowSuccessRateEnabled;
extern bool ItemEnhancementAnnounceEnabled;
extern uint32 ItemEnhancementAnnounceLevel;
extern float ItemEnhancementGoldCostRate;
extern float ItemEnhancementMaterialCostRate;
extern float ItemEnhancementSuccessRateRate;
extern float ItemEnhancementAttributeRate;
extern uint32 ItemEnhancementCooldown;
extern uint32 ItemEnhancementMaxLevel;
extern bool ItemEnhancementDebugMode;

#endif // _ITEM_ENHANCEMENT_H_
