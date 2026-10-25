#include "AsInt256.h"
#include "StringConvert.h"
#ifndef ITEM_ENHANCEMENT_MGR_H
#define ITEM_ENHANCEMENT_MGR_H
#include "Common.h"
#include "Player.h"
#include "Item.h"
#include "Configuration/Config.h"
#include "DatabaseEnv.h"
#include "ScriptMgr.h"
#include "ItemEnhancement.h"
#include <map>
#include <unordered_map>
#include <vector>
#include <memory>

enum ElementType
{
    ELEMENT_NONE = 0,
    ELEMENT_FIRE = 1,
    ELEMENT_WATER = 2,
    ELEMENT_WIND = 3,
    ELEMENT_EARTH = 4,
    ELEMENT_LIGHT = 5,
    ELEMENT_DARK = 6,
    MAX_ELEMENT_TYPE
};

enum SpecializationType
{
    SPEC_NONE = 0,
    SPEC_ATTACK = 1,
    SPEC_DEFENSE = 2,
    SPEC_HEALTH = 3,
    SPEC_SPELL = 4,
    MAX_SPEC_TYPE
};

enum EnhancementFailureType
{
    FAILURE_NONE = 0,        // 不变
    FAILURE_RESET_LEVEL = 1, // 等级清0
    FAILURE_DESTROY = 2,     // 摧毁
    FAILURE_DOWNGRADE = 3    // 降低指定等级
};

// 前向声明，实际定义在ItemEnhancement.h中
struct EnhancementTemplate;
struct ResonanceTemplate;
struct ElementTemplate;
struct SpecializationTemplate;
struct EnhancementRecord;

class ItemEnhancementMgr
{
public:
    static ItemEnhancementMgr* instance();

    void LoadConfig();
    void LoadEnhancementTemplates();

    bool IsEnabled() const { return _enabled; }
    bool IsFailureProtectionEnabled() const { return _failureProtectionEnabled; }
    bool IsShowSuccessRateEnabled() const { return _showSuccessRateEnabled; }
    bool IsAnnounceEnabled() const { return _announceEnabled; }
    uint32 GetAnnounceLevel() const { return _announceLevel; }
    float GetGoldCostRate() const { return _goldCostRate; }
    float GetMaterialCostRate() const { return _materialCostRate; }
    float GetSuccessRateRate() const { return _successRateRate; }
    float GetAttributeRate() const { return _attributeRate; }
    uint32 GetCooldown() const { return _cooldown; }
    uint32 GetMaxLevel() const { return _maxLevel; }
    bool IsDebugMode() const { return _debugMode; }

    EnhancementTemplate const* GetEnhancementTemplate(uint32 id) const;
    EnhancementTemplate const* GetEnhancementTemplateByGroupAndLevel(uint32 group, uint32 level) const;

    // 调试函数
    uint32 GetTemplateCount() const { return _enhancementTemplates->size(); }
    EnhancementRecord const* GetEnhancementRecord(uint32 itemGuid) const;

    void SaveEnhancementRecord(EnhancementRecord const& record);
    void DeleteEnhancementRecord(uint32 itemGuid);
    void DeleteAllEnhancementRecordsByPlayer(uint32 playerGuid);
    void ClearPlayerRecordCache(uint32 playerGuid);  // 清理玩家的记录缓存

    bool CanEnhanceItem(Player* player, Item* item) const;
    bool EnhanceItem(Player* player, Item* item, uint32 group = 0);
    // 为鉴定系统初始化强化（直接创建等级1，不依赖随机判定，并按鉴定模板约束条目数量与数值范围）
    bool InitializeEnhancementForIdentification(Player* player, Item* item, uint32 group,
        uint32 minAttrCount, uint32 maxAttrCount, AsInt64 minAttrValue, AsInt64 maxAttrValue);
    void ApplyEnhancementEffects(Player* player, Item* item);
    void RemoveEnhancementEffects(Player* player, Item* item);
    void SendEnhancementInfo(Player* player, Item* item);
    void PreloadPlayerEnhancementRecords(Player* player);

    std::string GetEnhancementVisual(uint32 level);

    // 官方装备相关
    bool IsOfficialItem(uint32 itemId) const;
    void ApplyOfficialItemEnhancement(Player* player, Item* item, uint32 level);
    void RemoveOfficialItemEnhancement(Player* player, Item* item);

    // 强化附魔相关函数
    uint32 CreateEnhancementEnchantment(Item* item, uint32 level);
    bool IsEnhancementEnchantment(uint32 enchantId) const;
    uint32 ExtractLevelFromEnchantment(uint32 enchantId) const;
    void ApplyStatModifier(Player* player, uint32 statType, AsInt64 const& value, bool apply);
    void ApplyStatModifierBatch(Player* player, uint32 statType, AsInt64 const& value, bool apply);  // 批量版本，不立即更新
    void RemovePreviousEnhancementStats(Player* player, const std::string& statValues);
    std::string GetStatTypeName(uint32 statType) const;

    // 装备穿戴/脱下事件处理
    void OnEquipItem(Player* player, Item* item);
    void OnPlayerUnequipItem(Player* player, Item* item);
    void ClearSlotEnhancements(Player* player, uint8 slot);

    // Tooltip显示相关
    std::string GetEnhancementTooltip(Item* item) const;

    // 客户端通信相关
    void SendEnhancementDataToClient(Player* player, Item* item, const EnhancementRecord& record);
    void SendAddonMessage(Player* player, const std::string& message);

    // 已应用强化效果管理
    void TrackAppliedEnhancement(Player* player, Item* item, const std::string& statValues);
    void UntrackAppliedEnhancement(Player* player, Item* item);
    std::string GetAppliedEnhancement(Player* player, Item* item);
    void ClearPlayerAppliedEnhancements(Player* player);

    // 属性值生成和应用
    std::string GenerateEnhancementStats(Item* item, uint32 level);
    // 根据指定强化组和等级生成本次强化的增量属性
    std::string GenerateIncrementalEnhancementStats(Item* item, uint32 level, uint32 group);
    void ApplyStoredEnhancementStats(Player* player, Item* item);
    void RemoveStoredEnhancementStats(Player* player, Item* item);
    std::map<uint32, AsInt64> ParseStatValues(const std::string& statValues) const;

    // 物品基础属性信息
    std::string GetItemBaseStatsInfo(Item* item) const;

private:
    ItemEnhancementMgr();
    ~ItemEnhancementMgr();

    bool _enabled;
    bool _failureProtectionEnabled;
    bool _showSuccessRateEnabled;
    bool _announceEnabled;
    uint32 _announceLevel;
    float _goldCostRate;
    float _materialCostRate;
    float _successRateRate;
    float _attributeRate;
    uint32 _cooldown;
    uint32 _maxLevel;
    bool _debugMode;

    std::unordered_map<uint32, EnhancementTemplate>* _enhancementTemplates;
    std::map<std::pair<uint32, uint32>, uint32>* _enhancementTemplatesByGroupLevel;

public:
    // 跟踪玩家已应用的强化效果 <玩家GUID, <物品GUID, 属性值字符串>>
    std::unordered_map<uint32, std::unordered_map<uint32, std::string>> _appliedEnhancements;

private:
    struct EnhancementRecordCacheEntry
    {
        std::unique_ptr<EnhancementRecord> record;
        uint32 lastUpdateTime = 0;
    };

    uint32 _recordCacheLifetime;
    mutable std::unordered_map<uint32, EnhancementRecordCacheEntry> _recordCache;

    std::unique_ptr<EnhancementRecord> DeserializeRecord(Field* fields) const;

    // 在启用物品属性系统时，将强化属性同步到物品属性数据表，交由物品属性模块统一应用
    void SyncEnhancementAttributesToItemAttributes(Player* player, Item* item,
        const std::string& newStatValues, const std::string& oldStatValues);
};

#define sItemEnhancementMgr ItemEnhancementMgr::instance()

#endif // ITEM_ENHANCEMENT_MGR_H
