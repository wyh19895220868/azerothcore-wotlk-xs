#include "AsInt256.h"
#include "StringConvert.h"
#ifndef HUANJING_SYSTEM_H
#define HUANJING_SYSTEM_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Creature.h"
#include "Item.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "WorldSession.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Util.h"
#include <vector>
#include <utility>
#include <unordered_set>
#include <array>
#include <map>
#include <mutex>
#include <optional>

// 血条系统集成到幻境系统中

// 预声明
class Unit;
class Spell;
struct SpellInfo;

// 血条数据结构
struct HealthBarData
{
    uint32 totalBars;           // 总血条数量
    uint32 currentBar;          // 当前剩余血条数（0 表示多倍血条已清空）
    AsInt64 healthPerBar;       // 每个血条的血量
    AsInt64 totalHealth;        // 总血量（诊断显示用）
    AsInt64 currentHealth;      // 当前血条剩余血量，避免多条相加后超过 64 位
    AsInt64 visibleHealth;      // 官方目标框显示用血量，跨血条时只降不回升
    bool enabled;               // 是否启用血条系统
    uint32 lastSwitchTime;      // 上次血条切换的时间戳（用于保护期）
    uint32 lastBroadcastTime;   // 上次向客户端广播血条信息的时间戳（ms）
    uint32 lastBroadcastBars;   // 上次广播时的剩余血条数

    HealthBarData() : totalBars(0), currentBar(1), healthPerBar(0), totalHealth(0),
                     currentHealth(0), visibleHealth(0), enabled(false), lastSwitchTime(0),
                     lastBroadcastTime(0), lastBroadcastBars(0) {}
};

struct HuanJingExternalHealthBarDamageResult
{
    bool handled = false;
    bool fatal = false;
    AsInt64 absorbedDamage = 0;
    AsInt64 coreDamage = 0;
    AsInt64 appliedDamage = 0;
    AsInt64 beforeHealth = 0;
    AsInt64 afterHealth = 0;
};

// 幻境等级配置结构
struct HuanJingLevelConfig
{
    uint32 level;
    std::string name;
    uint32 requiredExp;
    uint32 needTemplateId;
    std::string buff;
    std::string comment;
};

// 地图生物属性控制配置结构
struct HuanJingCreatureConfig
{
    uint32 configId = 0;
    int32 huanJingLevel = 0;
    int32 creatureId = -1;
    int32 creatureType = -1;
    int32 mapId = -1;
    int32 areaId = -1;
    int32 difficulty = -1;
    std::string mythicDifficulty;
    std::vector<uint32> mythicDifficulties;
    bool allMythicDifficulties = true;
    int32 minLevel = -1;
    int32 maxLevel = -1;

    // 地图表只负责路由、随机组和刷新策略。
    uint32 creatureGroupId = 0;
    uint32 attributeTemplateId = 0;
    uint32 noticeMode = 0;
    std::string respawnMessage;
    uint32 respawnTime = 0;

    // 从控制组解析出的外观，只在该生物首次解析或重生时应用。
    uint32 displayId = 0;
    float displayScale = 0.0f;
    uint32 weapon1 = 0;
    uint32 weapon2 = 0;
    uint32 weapon3 = 0;

    // 倍率字段
    float healthMultiplier = 1.0f;
    float manaMultiplier = 1.0f;
    float minDamageMultiplier = 1.0f;
    float maxDamageMultiplier = 1.0f;
    float armorMultiplier = 1.0f;
    float fireResistMultiplier = 1.0f;
    float natureResistMultiplier = 1.0f;
    float frostResistMultiplier = 1.0f;
    float shadowResistMultiplier = 1.0f;
    float arcaneResistMultiplier = 1.0f;
    float attackPowerMultiplier = 1.0f;
    float spellPowerMultiplier = 1.0f;
    float moveSpeedMultiplier = 1.0f;
    float attackSpeedMultiplier = 1.0f;
    float castSpeedMultiplier = 1.0f;
    float equipAttrMultiplier = 1.0f;
    char equipAttrMode = 'x';

    // 血条系统配置
    uint32 healthBarCount = 0;           // 血条数量

    // 命中和穿透属性
    float hitChanceBonus = 0.0f;        // 命中率加成百分比
    uint32 armorPenetration = 0;     // 护甲穿透数值
    float armorPenetrationPct = 0.0f;   // 护甲穿透百分比
    uint32 blockPenetration = 0;     // 格挡穿透数值
    float blockPenetrationPct = 0.0f;   // 格挡穿透百分比
    AsInt64 damageReductionPct = 0;    // 伤害减免百分比：1-99=减伤，100=免疫，>100=增伤

    // 固定值字段
    AsInt64 healthValue = 0;
    AsInt64 manaValue = 0;
    AsInt64 minDamageValue = 0;
    AsInt64 maxDamageValue = 0;
    AsInt64 damageTakenValue = 0;  // 受到的伤害固定值，来自旧属性模板
    AsInt64 trueDamageValue = 0;     // 真实伤害值
    AsInt64 armorValue = 0;
    AsInt64 holyResistValue = 0;
    AsInt64 fireResistValue = 0;
    AsInt64 natureResistValue = 0;
    AsInt64 frostResistValue = 0;
    AsInt64 shadowResistValue = 0;
    AsInt64 arcaneResistValue = 0;
    AsInt64 attackPowerValue = 0;
    AsInt64 spellPowerValue = 0;
    uint32 moveSpeedValue = 0;
    uint32 attackSpeedValue = 0;
    uint32 castSpeedValue = 0;
    uint32 equipAttrValue = 0;

    // 官方掉落配置
    uint32 officialDrop = 1;

    // 物品鉴定系统集成
    uint32 identificationGroup = 0;  // 鉴定组ID（0=不自动鉴定，>0=使用指定鉴定组）
    uint8 identificationQuality = 0; // 鉴定最低品质要求（0=灰色,1=白色,2=绿色,3=蓝色,4=紫色,5=橙色,6=红色,7=金色）
    uint32 identificationItemLevel = 0; // 鉴定最低物品等级要求（0=无限制，>0=物品等级必须>=该值才会被鉴定）

    // 动态难度控制
    bool dynamicDifficultyEnabled = false;
    AsInt64 combatPowerThreshold = 0;
    float difficultyMultiplier = 1.0f;

};

// 旧幻境等级/经验数据结构（兼容空实现保留）
struct PlayerHuanJingData
{
    uint32 playerGuid;
    uint32 huanJingLevel;
    uint32 huanJingExp;
    std::string huanJingBuff;
};





// 装备属性增强数据结构
struct ItemAttributeEnhancement
{
    uint32 recordId;
    uint32 playerGuid;
    uint32 itemGuid;
    uint32 itemId;
    int multiplier;
    char multiplierMode;
    std::string attributeData; // 格式：属性类型 原值 增强值,属性类型 原值 增强值
    uint32 identificationGroupId; // 鉴定组ID（0表示未鉴定）
};



struct CreatureDeathIdentificationContext
{
    uint32 identificationGroupId = 0;
    char multiplierMode = 'x';
    uint8 identificationQuality = 0;
    uint32 identificationItemLevel = 0;
    uint32 equipAttrValue = 0;
    float equipAttrMultiplier = 1.0f;
    bool fromMapControl = false;
};

class HuanJingSystem
{
public:
    static HuanJingSystem* instance();

    // 初始化系统
    void Initialize();

    // 加载配置数据
    void LoadHuanJingLevelConfigs();
    void LoadHuanJingCreatureConfigs();
    void LoadCreatureAttributeOverrideEntries();
    // Reload map creature configuration and reapply it to nearby loaded creatures.
    uint32 ReloadMapCreatureConfigsAndRefresh(Player* player, float range = 250.0f);

    // 玩家数据管理
    void LoadPlayerData(Player* player);
    void SavePlayerData(Player* player);
    void SaveAllOnlinePlayersData();           // 保存所有在线玩家数据（用于服务器关闭时）
    // 【线程安全修复】返回值拷贝而非裸指针，避免锁释放后悬空指针风险
    std::optional<PlayerHuanJingData> GetPlayerData(uint32 playerGuid);
    std::optional<PlayerHuanJingData> GetPlayerData(uint32 playerGuid) const;

    // 幻境等级管理
    uint32 GetPlayerHuanJingLevel(Player* player) const;
    uint32 GetPlayerHuanJingExp(Player* player);
    void SetPlayerHuanJingLevel(Player* player, uint32 level);
    void SetPlayerHuanJingExp(Player* player, uint32 exp);
    void AddPlayerHuanJingExp(Player* player, uint32 exp);
    void SetPlayerHuanJingBuff(Player* player, const std::string& buff);

    // 配置管理
    HuanJingLevelConfig* GetLevelConfig(uint32 level);
    const std::unordered_map<uint32, HuanJingLevelConfig>& GetAllLevelConfigs() const;
    uint32 GetCreatureConfigCount() const;
    const HuanJingCreatureConfig* GetMapCreatureConfig(Creature* creature, Player* player = nullptr) const;
    const HuanJingCreatureConfig* GetCreatureConfig(Creature* creature, Player* player = nullptr) const;
    void ForgetResolvedCreatureConfig(Creature const* creature);
    bool HasCreatureAttributeOverride(uint32 creatureId) const;
    bool HasCreatureAttributeOverride(Creature const* creature) const;

    // 动态等级配置生成
    bool EnsureLevelConfigExists(uint32 level);  // 确保等级配置存在，不存在则动态创建
    HuanJingLevelConfig GenerateDefaultLevelConfig(uint32 level);  // 生成默认等级配置

    // 动态生物属性配置生成
    bool EnsureCreatureConfigExists(uint32 huanJingLevel);  // 确保生物属性配置存在，不存在则动态创建
    HuanJingCreatureConfig GenerateDefaultCreatureConfig(uint32 huanJingLevel);  // 生成默认生物属性配置

    // 法术强度管理
    void SetCreatureSpellPower(ObjectGuid guid, AsInt64 spellPower);
    AsInt64 GetCreatureSpellPower(ObjectGuid guid) const;
    void ClearCreatureSpellPower(Creature* creature);
    void SetCreatureVirtualMana(ObjectGuid guid, AsInt64 maxMana);
    bool GetCreatureVirtualMana(Creature* creature, AsInt64& currentMana, AsInt64& maxMana) const;
    void ClearCreatureVirtualMana(ObjectGuid guid);

    // 血条系统配置获取
    uint32 GetCreatureHealthBarCount(Creature* creature, Player* player) const;
    bool EnsureCreatureHealthBarsFromConfig(Creature* creature, Player* player = nullptr, bool sendUpdate = false);
    AsInt64 GetCreatureTrueDamageValue(Creature* creature, Player* player) const;

    // 血条系统管理方法
    void InitializeCreatureHealthBars(Creature* creature, uint32 barCount, AsInt64 healthPerBar = 0, AsInt64 totalHealth = 0);
    const HealthBarData* GetHealthBarData(Creature const* creature) const;
    HealthBarData* GetMutableHealthBarData(Creature const* creature);
    bool BuildCustomTargetHealthPacket(Creature const* creature, std::string& payload) const;
    void RemoveCreatureHealthBars(Creature const* creature);
    bool IsHealthBarEnabled(Creature const* creature) const;
    bool GetCreatureVirtualHealth(ObjectGuid guid, AsInt64& currentHealth, AsInt64& maxHealth) const;
    std::string GetHealthBarDisplay(Creature const* creature) const;
    void SendHealthBarUpdate(Creature* creature);
    bool ResetCreatureHealthBarsToFull(Creature* creature, bool sendUpdate = true);
    void ClearHealthBarData();
    void UpdateCreatureHealth(Creature* creature, AsInt64 newHealth);
    void ProcessHealthBarRestoreQueue();
    void AddToHealthBarRestoreQueue(Creature const* creature, AsInt64 health);
    bool ProcessCreatureHealthRestore(Creature* creature);
    // 按节流规则广播血条更新：同一生物  intervalMs 内只发送一次，且血条数变化时优先发送
    void BroadcastHealthBarUpdateThrottled(Creature* creature, uint32 nowMs, uint32 intervalMs);
    // 外部特效伤害入口：先扣幻境虚拟血条，返回剩余需要交给核心本体结算的伤害
    HuanJingExternalHealthBarDamageResult ApplyExternalCreatureHealthBarDamage(Unit* attacker, Creature* creature, AsInt64 const& damage);
    // 核心致死逻辑接管：在 Unit::Kill 中统一调用，返回 true 表示由血条系统接管本次致死（不再真正 Kill）
    bool HandleUnitKill(Unit* killer, Unit* victim, SpellInfo const* spellProto);

    // 生物属性调整
    // 只有配置了具体生物、地图或区域的行才改属性。全 -1 通配行和没有配置的生物保持当前血量。
    bool CreatureUsesDedicatedStats(Creature* creature, Player* player) const;
    void AdjustCreatureStats(Creature* creature, Player* player, bool allowInCombat = false);
    // 按指定玩家的幻境等级单体调整生物属性：
    // - 如果该生物尚未调整，则按此玩家等级调整；
    // - 如果已按更高等级调整，则保持不变；
    // - 如果已按更低等级调整，则提升到此玩家等级。
    void AdjustCreatureStatsForPlayer(Creature* creature, Player* player, bool allowInitialCombatAdjust = false, Unit* combatVictim = nullptr);
    void AdjustCreatureStatsWithHighestLevel(Creature* creature, float range = 200.0f);
    uint32 GetHighestHuanJingLevelNearCreature(Creature* creature, float range = 200.0f);
    void RestoreCreatureToOriginalStats(Creature* creature);
    // keepDeathRecords：死亡路径传 true，保留刚写入的死亡鉴定组记录供拾取使用；
    // 活体还原路径用默认 false 全清，防止鉴定ID残留
    void CleanupCreatureAdjustmentData(Creature* creature, bool keepDeathRecords = false);
    // 玩家离开时重新调整附近生物：根据剩余玩家的最高幻境等级调整，如果没有其他玩家则还原到原始状态
    void ReAdjustNearbyCreaturesOnPlayerLeave(Player* player, float range = 200.0f);
    // 副本重置时清理指定地图/实例的所有生物调整数据
    void CleanupInstanceCreatureAdjustments(uint32 mapId, uint32 instanceId = 0);
    bool IsCreatureMarked(Creature* creature);
    bool IsCreatureBeingAdjusted(Creature* creature);

    // 重生相关方法
    void SaveCreatureAdjustmentForRespawn(Creature* creature);
    void RestoreCreatureAdjustmentAfterRespawn(Creature* creature);
    HuanJingCreatureConfig* GetBestMatchConfig(uint32 huanJingLevel, uint32 creatureId, uint32 mapId, uint32 areaId, uint32 difficulty, uint32 mythicDifficulty, uint32 creatureLevel, uint32 creatureType);

    // 掉落系统
    void AdjustItemStats(Item* item, Player* player);
    void AdjustItemStatsWithLevel(Item* item, Player* player, uint32 huanJingLevel);  // 使用指定幻境等级调整物品属性

    // 生物死亡幻境等级记录（防止拾取漏洞）
    void RecordCreatureDeathHuanJingLevel(ObjectGuid creatureGuid, uint32 huanJingLevel);
    uint32 GetCreatureDeathHuanJingLevel(ObjectGuid creatureGuid) const;
    void ClearCreatureDeathHuanJingLevel(ObjectGuid creatureGuid);

    // 装备属性倍率系统
    void ApplyItemAttributeMultiplier(Item* item, float multiplier, char multiplierMode = 'x');
    float GetItemAttributeMultiplier(Item* item) const;
    char GetItemAttributeMode(Item* item) const;
    void RemoveItemAttributeMultiplier(Item* item);
    void RemoveItemAttributeMultiplierTemporary(Item* item); // 临时移除，不删除数据库记录
    void CleanupUnequippedItemEnhancements(Player* player);
    bool IsItemTemporarilyDisabled(uint32 itemGuid) const;

    // 装备属性增强数据库管理
    void SaveItemAttributeEnhancement(uint32 playerGuid, uint32 itemGuid, uint32 itemId, int multiplier, char multiplierMode, const std::string& attributeData, uint32 identificationGroupId = 0);
    ItemAttributeEnhancement* GetItemAttributeEnhancement(uint32 itemGuid);
    void LoadItemAttributeEnhancements(uint32 playerGuid);
    void DeleteItemAttributeEnhancement(uint32 itemGuid);
    std::string BuildAttributeDataString(Item* item, int multiplier, char multiplierMode = 'x');
    void UpdateItemIdentificationGroup(uint32 itemGuid, uint32 identificationGroupId);  // 更新物品鉴定组ID

    // 属性应用和移除功能（参考强化系统）
    void ApplyHuanJingEnhancement(Player* player, Item* item, bool updateStats = true);
    void RemoveHuanJingEnhancement(Player* player, Item* item);
    void RemoveAllPlayerHuanJingEnhancements(Player* player, bool updateStats = true);
    void ApplyStatModifier(Player* player, uint32 statType, AsInt64 const& value, bool apply, bool skipUpdate = false);

    // 跟踪已应用的幻境效果（参考强化系统）
    void TrackAppliedHuanJingEnhancement(Player* player, Item* item, const std::string& statValues);
    void UntrackAppliedHuanJingEnhancement(Player* player, Item* item);
    std::string GetAppliedHuanJingEnhancement(Player* player, Item* item);
    void ClearAppliedHuanJingEnhancement(Player* player, Item* item); // 清除单个物品的跟踪数据（不移除属性）
    void ClearPlayerAppliedHuanJingEnhancements(Player* player);
    void ClearSlotHuanJingEnhancements(Player* player, uint8 slot);
    void ClearPlayerMemoryCache(uint32 playerGuid); // 【内存泄漏修复】清理玩家内存缓存

    // 工具函数
    uint32 GetCreatureTypeByEntry(uint32 entry) const;
    uint32 GetCreatureMapDifficulty(Creature const* creature) const;
    uint32 GetCreatureMythicDifficulty(Creature const* creature) const;
    bool MatchesCondition(int32 configValue, uint32 actualValue);
    float CalculateItemMultiplierByLevel(uint32 huanJingLevel, Item* item);
    long double CalculateCreatureContentDifficultyScale(Creature* creature) const;

    // 数据库清理功能
    void CleanupInvalidItems();
    void ScheduleItemCleanup();

    // 配置获取方法
    uint32 GetMaxLevel() const { return _maxLevel; }  // 最大幻境等级
    // 【P2修复】Enable/Debug 配置获取方法
    bool IsEnabled() const { return _enabled; }       // 系统是否启用
    bool IsDebugEnabled() const { return _debug; }    // 调试模式是否启用

    // 属性上限配置获取方法
    AsInt64 GetMaxHealthValue() const { return _maxHealthValue; }
    long double GetMaxDamageValue() const { return _maxDamageValue; }
    AsInt64 GetMaxArmorValue() const { return _maxArmorValue; }
    AsInt64 GetMaxAttackPowerValue() const { return _maxAttackPowerValue; }
    AsInt64 GetMaxResistanceValue() const { return _maxResistanceValue; }
    AsInt64 GetMaxManaValue() const { return _maxManaValue; }
    AsInt64 GetMaxSpellPowerValue() const { return _maxSpellPowerValue; }
    float GetMaxCritChance() const { return _maxCritChance; }
    float GetMaxHitChance() const { return _maxHitChance; }
    uint32 GetMaxHaste() const { return _maxHaste; }
    uint32 GetMaxExpertise() const { return _maxExpertise; }
    float GetMaxDodgeChance() const { return _maxDodgeChance; }
    float GetMaxParryChance() const { return _maxParryChance; }
    float GetMaxBlockChance() const { return _maxBlockChance; }
    float GetMaxMovementSpeed() const { return _maxMovementSpeed; }

    // 对外提供的安全血量检查接口：内部会调用 ClampCreatureHealthToMax
    void EnsureCreatureHealthSafe(Creature* creature);

    // NPC商店购买需求检查功能
    uint32 GetPurchaseNeedTemplateId(Player* player);
    bool CheckVendorItemRequirements(Player* player);
    std::string GetVendorItemRequirementMessage(Player* player);

    // 公有成员，供命令访问
    std::unordered_map<uint64, uint32> _creatureAdjustedLevels; // 生物唯一键 -> 当前调整的幻境等级
    std::unordered_map<uint64, uint32> _creatureRespawnAdjustments; // 生物唯一键 -> 重生前的调整等级
    std::unordered_map<ObjectGuid, CreatureDeathIdentificationContext> _creatureDeathIdentificationGroup; // 生物GUID -> 死亡时的鉴定上下文

    // 【性能优化】将这些数据容器移到public，以便在延迟事件回调中直接访问
    std::unordered_map<uint32, PlayerHuanJingData> _playerData;
    std::unordered_map<uint32, ItemAttributeEnhancement> _itemEnhancements; // 装备GUID -> 属性增强数据

private:
    HuanJingSystem() = default;

    HuanJingCreatureConfig const* ResolveCreatureConfig(Creature* creature, HuanJingCreatureConfig const* routeConfig);
    void ApplyResolvedCreatureAppearance(Creature* creature, HuanJingCreatureConfig const& config, bool applyAuras);

    std::unordered_map<uint32, HuanJingLevelConfig> _levelConfigs;
    std::vector<HuanJingCreatureConfig> _creatureConfigs;
    std::unordered_map<uint64, HuanJingCreatureConfig> _resolvedCreatureConfigs;

    // GetBestMatchConfig 的记忆化缓存：查询参数 -> 最佳匹配（含未命中 nullptr）。
    // 该函数在伤害热路径被高频调用且每次 O(N) 扫描全配置表；
    // 配置只在加载/重载时变化，重载时必须清空缓存（指针指向 _creatureConfigs 元素）。
    mutable std::mutex _configMatchCacheMutex;
    mutable std::map<std::array<uint32, 6>, HuanJingCreatureConfig*> _configMatchCache;

    std::unordered_set<uint32> _creatureAttributeOverrideEntries;
    std::unordered_map<uint32, float> _itemMultipliers; // 装备GUID -> 属性倍率（保留用于兼容）
    std::unordered_set<uint32> _temporarilyDisabledItems; // 临时禁用的装备GUID集合

    // 跟踪玩家已应用的幻境增强效果 <玩家GUID, <物品GUID, 属性值字符串>>
    std::unordered_map<uint32, std::unordered_map<uint32, std::string>> _appliedHuanJingEnhancements;

    // 递归保护
    std::unordered_set<uint64> _adjustingCreatures; // 正在调整的生物唯一键集合，防止递归调用

    // 生物死亡时记录击杀者的幻境等级，用于修复拾取漏洞
    // 防止玩家用低倍率击杀后切换高倍率拾取
    std::unordered_map<ObjectGuid, uint32> _creatureDeathHuanJingLevel; // 生物GUID -> 击杀时的幻境等级

    // 清理相关
    uint32 _lastCleanupTime;
    uint32 _cleanupInterval; // 清理间隔（秒）

    // 【P2修复】Enable/Debug 配置参数
    bool _enabled = true;           // 系统是否启用
    bool _debug = false;            // 调试模式是否启用

    // 配置参数
    uint32 _maxLevel = 2000;        // 最大幻境等级

    // 属性上限配置参数（防止超过字段承载范围）
    AsInt64 _maxHealthValue;        // 血量上限
    long double _maxDamageValue;    // 伤害上限
    AsInt64 _maxArmorValue;         // 护甲上限
    AsInt64 _maxAttackPowerValue;   // 攻击强度上限
    AsInt64 _maxResistanceValue;     // 抗性上限
    AsInt64 _maxManaValue;          // 法力值上限
    AsInt64 _maxSpellPowerValue;    // 法术强度上限
    float _maxCritChance;           // 暴击几率上限
    float _maxHitChance;            // 命中几率上限
    uint32 _maxHaste;               // 急速等级上限
    uint32 _maxExpertise;           // 精准等级上限
    float _maxDodgeChance;          // 闪避几率上限
    float _maxParryChance;          // 招架几率上限
    float _maxBlockChance;          // 格挡几率上限
    float _maxMovementSpeed;        // 移动速度上限

    // 法术强度存储
    std::unordered_map<ObjectGuid, AsInt64> _creatureSpellPower;
    std::unordered_map<ObjectGuid, AsInt64> _creatureVirtualMaxMana;

public:
    // 线程安全保护：互斥锁，保护所有共享容器免受多线程并发访问问题
    // 设置为 public 允许外部脚本访问，但仅用于正确的锁保护
    mutable std::mutex _creatureMutex;      // 保护生物相关容器
    mutable std::mutex _playerMutex;        // 保护玩家数据容器
    mutable std::mutex _itemMutex;          // 保护物品增强容器
    mutable std::mutex _healthBarMutex;     // 保护血条系统容器

    // 血条数据容器（需要public以便外部脚本在锁内访问）
    std::unordered_map<uint64, HealthBarData> _healthBarData;
    std::unordered_map<uint64, AsInt64> _healthBarRestoreQueue; // 血条恢复队列

private:

    // 血条系统私有方法
    void CalculateCurrentBar(HealthBarData& data);

    // 安全保护：将生物当前生命值限制在更新字段可承载范围之内，超出部分交给虚拟血条
    void ClampCreatureHealthToMax(Creature* creature);
};

#define sHuanJingSystem HuanJingSystem::instance()

// 脚本钩子类
class HuanJingSystemPlayerScript : public PlayerScript
{
public:
    HuanJingSystemPlayerScript() : PlayerScript("HuanJingSystemPlayerScript")
    {
        LOG_INFO("server.loading", ">> [幻境系统] HuanJingSystemPlayerScript 构造函数被调用");
    }

    void OnPlayerLogin(Player* player) override;
    void OnPlayerLogout(Player* player) override;
    void OnPlayerCreatureKill(Player* killer, Creature* killed) override;
    void OnPlayerKilledByCreature(Creature* killer, Player* killed) override;
    void OnPlayerPVPKill(Player* killer, Player* killed) override;
    void OnPlayerReleasedGhost(Player* player) override;
    void OnPlayerResurrect(Player* player, float restore_percent, bool& applySickness) override;
    void OnPlayerUpdate(Player* player, uint32 diff) override;
    void OnPlayerMapChanged(Player* player) override;
    void OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override;
    bool OnPlayerCanUnequipItem(Player* player, uint16 pos, bool swap) override;
    void OnPlayerAfterMoveItemFromInventory(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override;
    void OnPlayerLootItem(Player* player, Item* item, uint32 count, ObjectGuid lootguid) override;
    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 count) override;
    void OnPlayerBeforeBuyItemFromVendor(Player* player, ObjectGuid vendorguid, uint32 vendorslot, uint32& item, uint8 count, uint8 bag, uint8 slot) override;
    void OnPlayerEnterCombat(Player* player, Unit* enemy) override;

private:
    std::unordered_map<uint32, ObjectGuid> _lastTargetGuids;
};


class HuanJingSystemCreatureScript : public AllCreatureScript
{
public:
    HuanJingSystemCreatureScript() : AllCreatureScript("HuanJingSystemCreatureScript") { }

    void OnCreatureAddWorld(Creature* creature) override;
    void OnCreatureRemoveWorld(Creature* creature) override;
    void OnAllCreatureUpdate(Creature* creature, uint32 diff) override;
};

// 新增 UnitScript 来处理伤害和死亡钩子
class HuanJingSystemUnitScript : public UnitScript
{
public:
    HuanJingSystemUnitScript() : UnitScript("HuanJingSystemUnitScript") { }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override;
    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override;
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage, SpellInfo const* spellInfo) override;
    void OnBeforeRollMeleeOutcomeAgainst(Unit const* attacker, Unit const* victim, WeaponAttackType attType,
        int32& attackerMaxSkillValueForLevel, int32& victimMaxSkillValueForLevel, int32& attackerWeaponSkill,
        int32& victimDefenseSkill, int32& critChance, int32& missChance, int32& dodgeChance,
        int32& parryChance, int32& blockChance) override;
    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override;
    void OnDamageMeleeNow(Unit* attacker, Unit* victim, uint32& damage) override;
    void OnDamageSpellEnd(Unit* attacker, Unit* victim, uint32 castId, uint32& damage) override;
    void OnCreKillToCreOrDie(Unit* killer, Unit* killed) override;
    void OnUnitDeath(Unit* unit, Unit* killer) override;
    void OnUnitEnterEvadeMode(Unit* unit, uint8 evadeReason) override;
};

class HuanJingSystemItemScript : public ItemScript
{
public:
    HuanJingSystemItemScript() : ItemScript("HuanJingSystemItemScript") { }

    bool OnItemCreate(Item* item, ItemTemplate const* itemProto, Player const* owner);
    bool OnRemove(Player* player, Item* item) override;
};



// 脚本注册函数声明
void Addmod_huanjinxitongScripts();

// 全局函数：获取生物的幻境系统法术强度
AsInt64 GetHuanJingCreatureSpellPower(ObjectGuid guid);
bool HuanJingPrepareExternalCreatureForPlayerDamage(Unit* attacker, Creature* creature);
HuanJingExternalHealthBarDamageResult HuanJingApplyExternalCreatureHealthBarDamage(Unit* attacker, Creature* creature, AsInt64 const& damage);
bool HuanJingIsExternalCreatureHealthBarEnabled(Creature* creature);
void HuanJingEnterExternalPlayerDamageNoInitialAdjust();
void HuanJingLeaveExternalPlayerDamageNoInitialAdjust();

#endif // HUANJING_SYSTEM_H
