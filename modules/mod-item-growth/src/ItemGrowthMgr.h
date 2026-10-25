#include "AsInt256.h"
#include "StringConvert.h"
#ifndef MODULE_ITEM_GROWTH_MGR_H
#define MODULE_ITEM_GROWTH_MGR_H

#include "Player.h"
#include "Config.h"
#include "Define.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

// 成长装备GUID偏移量常量 - 已弃用
// 不再向randomPropertyId字段中添加GROWTH_GUID_OFFSET偏移量
// 现在直接存储物品的真实GUID
// constexpr int32 GROWTH_GUID_OFFSET = 1000000;

// 定义枚举类型
enum ItemGrowthAttributeType
{
    ITEM_GROWTH_ATTR_STRENGTH = 1,
    ITEM_GROWTH_ATTR_AGILITY = 2,
    ITEM_GROWTH_ATTR_INTELLECT = 3,
    ITEM_GROWTH_ATTR_STAMINA = 4,
    ITEM_GROWTH_ATTR_ALL = 0
};

enum ItemGrowthConditionType
{
    ITEM_GROWTH_CONDITION_NONE = 0,
    ITEM_GROWTH_CONDITION_KILL_CREATURE = 1,
    ITEM_GROWTH_CONDITION_COMPLETE_QUEST = 2,
    ITEM_GROWTH_CONDITION_VISIT_AREA = 3,
    ITEM_GROWTH_CONDITION_PVP_KILL = 4
};

enum ItemGrowthEnvironmentType
{
    ITEM_GROWTH_ENVIRONMENT_NONE = 0,
    ITEM_GROWTH_ENVIRONMENT_MAP = 1,
    ITEM_GROWTH_ENVIRONMENT_ZONE = 2,
    ITEM_GROWTH_ENVIRONMENT_DUNGEON = 3,
    ITEM_GROWTH_ENVIRONMENT_BATTLEGROUND = 4
};



// 物品成长模板结构
struct ItemGrowthTemplate
{
    ItemGrowthTemplate() = default;

    uint32 id;
    uint32 requirementTemplateId;       // 需求模板ID，0表示无需求限制，大于0时需要满足对应需求模板才能随机到该组
    uint32 group;
    uint32 level;
    uint32 probability;                 // 该组的随机几率权重（只有等级1的记录使用此字段）
    uint32 requiredExp;
    uint32 itemPropertyGroup;           // 物品属性组ID，关联mod-item-attributes模块
                                        // 0 = 不关联任何属性组
                                        // >0 = 关联对应ID的属性组，升级时自动应用该组的随机属性
    uint32 attributeDistributionType;   // 属性分配方式
                                        // 0 = 平均分配：升级时固定获得levelRewardAttribute和levelRewardAttributePercent的值
                                        // 1 = 随机分配：升级时在minAttributeValue-maxAttributeValue和minAttributePercent-maxAttributePercent范围内随机获得属性
    AsInt64 minAttributeValue;
    AsInt64 maxAttributeValue;
    AsInt64 minAttributePercent;
    AsInt64 maxAttributePercent;
    AsInt64 levelRewardAttribute;
    AsInt64 levelRewardAttributePercent;
    std::string skillIds;
};

// 玩家物品成长记录结构
struct ItemGrowthPlayerRecord
{
    ItemGrowthPlayerRecord() = default;

    uint32 id;
    uint32 playerGuid;
    uint32 itemGuid;
    uint32 itemEntry;
    uint32 currentLevel;
    uint32 currentExp;
    uint32 requiredExp;                 // 升级所需经验
    uint32 itemPropertyGroup;           // 物品属性模板组
    uint32 growthCount;
    uint32 failCount;
    AsInt64 currentAttributeValue;
    AsInt64 currentAttributePercent;
    std::string growthAttributes;       // 成长属性（格式：属性类型 值,属性类型 值）
    std::string percentAttributes;      // 百分比属性（格式：属性类型 值,属性类型 值）

    // 【性能优化】新增字段：用于异步批量处理
    uint32 pendingExp;                  // 待处理的累积经验
    bool isDirty;                       // 是否需要保存到数据库
};

// 【性能优化】玩家经验累积缓存结构
struct PlayerExpAccumulator
{
    std::unordered_map<uint32, uint32> itemExpMap;  // 物品GUID -> 累积经验
    uint32 lastProcessTime;                         // 上次处理时间
    bool needsUpdate;                               // 是否需要更新属性
};

class ItemGrowthMgr
{
public:
    static ItemGrowthMgr* instance();

    void Initialize();
    void LoadFromDB();
    void LoadItemGrowthTemplates();
    bool IsEnabled() const { return _enabled; }

    // 获取加载的数据条数
    uint32 GetTemplateCount() const { return _itemGrowthTemplates.size(); }
    uint32 GetPlayerRecordCount() const { return _playerItemRecords.size(); }

    // 获取物品成长模板
    ItemGrowthTemplate const* GetItemGrowthTemplate(uint32 id) const;
    ItemGrowthTemplate const* GetItemGrowthTemplateByLevel(uint32 level, uint32 group = 0) const;
    std::map<uint32, ItemGrowthTemplate> const& GetItemGrowthTemplates() const { return _itemGrowthTemplates; }

    // 获取玩家物品成长记录
    ItemGrowthPlayerRecord* GetPlayerItemRecord(uint32 itemGuid);
    ItemGrowthPlayerRecord const* GetPlayerItemRecord(uint32 itemGuid) const;
    void SavePlayerItemRecord(ItemGrowthPlayerRecord* record);
    void CreatePlayerItemRecord(Player* player, Item* item, uint32 propertyGroup = 1);
    void DeletePlayerRecords(uint32 playerGuid);  // 删除指定玩家的所有成长记录
    std::map<uint32, ItemGrowthPlayerRecord>& GetPlayerItemRecords() { return _playerItemRecords; }

    // 物品成长相关功能
    bool CanItemGrow(Item* item) const;
    void SetItemCanGrow(Player* player, Item* item, uint32 propertyGroup);
    // 为鉴定系统设置成长（使用鉴定表中的数量/数值配置进行一次性约束）
    void SetItemCanGrowForIdentification(Player* player, Item* item, uint32 propertyGroup,
        uint32 minAttrCount, uint32 maxAttrCount, AsInt64 minAttrValue, AsInt64 maxAttrValue);
    bool AddItemExp(Player* player, Item* item, uint32 exp);
    // AddExp 是 AddItemExp 的别名
    bool AddExp(Player* player, Item* item, uint32 exp) { return AddItemExp(player, item, exp); }
    bool UpgradeItem(Player* player, Item* item);

    // 【性能优化】异步批量经验处理
    void AccumulateItemExp(Player* player, Item* item, uint32 exp);  // 累积经验（不立即处理）
    void ProcessAccumulatedExp(Player* player);                      // 处理累积的经验
    void ProcessAllPendingExp();                                     // 处理所有玩家的待处理经验（定时调用）
    void FlushPlayerExp(Player* player);                             // 玩家下线时强制处理
    void SaveDirtyRecords();                                         // 批量保存脏数据
    void SaveDirtyRecordsAsync();                                    // 异步保存脏数据

    // 属性应用
    void ApplyItemAttributes(Player* player, Item* item);
    void ForceApplyItemAttributes(Player* player, Item* item);
    void ApplyOfficialItemGrowth(Player* player, Item* item, ItemGrowthPlayerRecord* record, const ItemGrowthTemplate* currentTemplate);

    // 成长属性处理
    void ParseGrowthAttributes(const std::string& attributeString, std::vector<std::pair<uint32, AsInt64>>& attributes);
    void ApplyGrowthAttributes(Player* player, Item* item, const std::string& attributeString);
    std::string FormatGrowthAttributes(std::vector<std::pair<uint32, AsInt64>> const& attributes);

    // 升级时的属性生成和应用
    void GenerateLevelUpAttributes(ItemGrowthPlayerRecord* record, const ItemGrowthTemplate* levelTemplate);
    void GenerateLevelUpAttributesBasedOnItem(Player* player, Item* item, ItemGrowthPlayerRecord* record, const ItemGrowthTemplate* levelTemplate);

    void RefreshAndApplyGrowthAttributes(Player* player, Item* item, ItemGrowthPlayerRecord* record);
    void ApplyAttributeToPlayer(Player* player, uint32 attrType, AsInt64 attrValue, bool isAdd);
    void ApplyPercentAttributeToPlayer(Player* player, uint32 attrType, AsInt64 percentValue, bool isAdd);
    void VerifyAttributesApplied(Player* player, const std::string& attributeString);

    // 新的Aura系统方法
    void ApplyGrowthAura(Player* player, uint32 itemGuid);
    void RemoveGrowthAura(Player* player, uint32 itemGuid);
    void RemoveGrowthAttributesDirect(Player* player, const std::string& attributeString);

    // GUID存储方法（参考强化系统）
    void StoreGuidInRandomProperty(Item* item);

    // 确保物品具备唯一seed（解决聊天链接和客户端识别问题）
    void EnsureItemUniqueSeed(Player* player, Item* item);

    // 修复0级记录的专用函数
    bool FixZeroLevelRecord(Player* player, Item* item, uint32 itemGuid);

    // 升级经验计算
    uint32 CalculateRequiredExp(uint32 currentLevel, uint32 propertyGroup);
    void UpdateRequiredExp(ItemGrowthPlayerRecord* record);

    // 模块关联检查
    bool IsItemAttributesModuleAvailable() const;

    // 配置获取
    uint32 GetMaxLevel() const { return _maxLevel; }
    float GetBaseExpMultiplier() const { return _baseExpMultiplier; }
    uint32 GetUpgradeSuccessRate() const { return _upgradeSuccessRate; }
    bool GetRequireSoulbound() const { return _requireSoulbound; }
    bool GetAutoApplyAttributes() const { return _autoApplyAttributes; }
    bool GetKillExpEnabled() const { return _killExpEnabled; }
    float GetKillExpMultiplier() const { return _killExpMultiplier; }
    bool GetExpNotificationEnabled() const { return _expNotificationEnabled; }

private:
    ItemGrowthMgr();
    ~ItemGrowthMgr() = default;

    bool _enabled;
    bool _identificationWrite = false;
    uint32 _maxLevel;
    float _baseExpMultiplier;
    uint32 _upgradeSuccessRate;
    bool _requireSoulbound;
    bool _autoApplyAttributes;
    bool _killExpEnabled;          // 是否启用杀怪获取成长经验
    float _killExpMultiplier;      // 杀怪成长经验倍率
    bool _expNotificationEnabled;  // 是否启用经验获取通知

    std::map<uint32, ItemGrowthTemplate> _itemGrowthTemplates;
    std::map<uint32, ItemGrowthPlayerRecord> _playerItemRecords;

    // 跟踪已应用的成长属性，避免重复应用
    // Key: 玩家GUID << 32 | 物品GUID, Value: 已应用的属性字符串
    std::map<uint64, std::string> _appliedGrowthAttributes;

    // 【性能优化】异步批量处理相关
    mutable std::mutex _expAccumulatorMutex;                              // 经验累积器锁
    std::unordered_map<uint32, PlayerExpAccumulator> _playerExpAccumulators;  // 玩家GUID -> 经验累积器
    mutable std::mutex _dirtyRecordsMutex;                                // 脏记录锁
    std::unordered_set<uint32> _dirtyRecordGuids;                         // 需要保存的物品GUID集合
    uint32 _lastBatchSaveTime;                                            // 上次批量保存时间
    uint32 _batchSaveInterval;                                            // 批量保存间隔（毫秒）
    uint32 _expProcessInterval;                                           // 经验处理间隔（毫秒）
};

#define sItemGrowthMgr ItemGrowthMgr::instance()

#endif // MODULE_ITEM_GROWTH_MGR_H
