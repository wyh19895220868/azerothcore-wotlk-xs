#ifndef ITEM_SETS_H
#define ITEM_SETS_H

#include "Common.h"
#include "Player.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ScriptMgr.h"
#include "ChatCommand.h"
#include "Chat.h"
#include "Configuration/Config.h"
#include <map>
#include <vector>
#include <string>
#include <set>

// 套装效果类型
enum ItemSetEffectType
{
    ITEM_SET_EFFECT_STAT = 1,   // 属性加成
    ITEM_SET_EFFECT_SKILL = 2,  // 技能效果
    ITEM_SET_EFFECT_AURA = 3    // 光环效果
};

// 套装数据结构
struct ItemSetData
{
    uint32 SetId;               // 套装ID
    uint32 Group;               // 套装组别
    std::string SetName;        // 套装名称
    std::string SetDesc;        // 套装描述
    uint32 RequirementId;       // 旧核心 __模版需求总表 ID
    uint8 SetQuality;           // 套装品质
    uint8 ItemsCount;           // 激活效果需要的物品数量
    uint8 EffectType;           // 效果类型
    std::string SetAttributes;  // 套装属性数据
    uint32 EffectId;            // 效果ID
    float EffectValue;          // 效果数值
    std::string EffectDesc;     // 效果描述
};

// 简化的玩家套装状态结构
struct PlayerSetStatus
{
    uint32 PlayerGuid;                              // 玩家GUID
    std::map<uint32, uint32> ItemSetMap;            // 物品GUID -> 套装ID 的映射
    std::map<uint32, std::string> ActiveAttributes; // 套装ID -> 激活属性字符串
};

// 装备缓存结构 - 用于增量更新优化
struct EquipmentCache
{
    std::map<uint8, uint32> slotToItemGuid;  // 槽位 -> 物品GUID
    std::map<uint8, uint32> slotToSetId;     // 槽位 -> 套装ID
    std::map<uint32, uint8> setIdToCount;    // 套装ID -> 装备件数

    void Clear()
    {
        slotToItemGuid.clear();
        slotToSetId.clear();
        setIdToCount.clear();
    }
};

// 套装组信息结构
struct SetGroupInfo
{
    uint32 GroupId;             // 组ID
    std::vector<uint32> SetIds; // 该组包含的套装ID列表
    std::vector<float> Probabilities; // 对应的几率列表
};

// 技能增强效果结构
struct SpellEnhancement
{
    uint32 spellId;      // 增强的技能ID
    float enhancePercent; // 增强百分比
};

// 配置管理类
class ItemSetsConfig
{
public:
    static ItemSetsConfig* instance();

    bool LoadConfig();

    // 配置选项
    bool IsModuleEnabled() const { return _enabled; }
    bool IsDebugMode() const { return _debugMode; }
    bool AllowPartialBonus() const { return _allowPartialBonus; }
    int GetMaxSetsPerPlayer() const { return _maxSetsPerPlayer; }
    bool ShowBonusNotification() const { return _showBonusNotification; }
    bool ShowSetIconMark() const { return _showSetIconMark; }

private:
    ItemSetsConfig() = default;
    ~ItemSetsConfig() = default;

    bool _enabled{true};
    bool _debugMode{false};
    bool _allowPartialBonus{true};
    int _maxSetsPerPlayer{10};
    bool _showBonusNotification{true};
    bool _showSetIconMark{true};
};

// 套装管理器
class ItemSetsManager
{
public:
    static ItemSetsManager* instance();

    bool Initialize();
    void LoadItemSets();
    void LoadPlayerSetRecords();

    // 套装数据查询
    const std::vector<ItemSetData>& GetItemSetData(uint32 setId) const;
    std::string GetSetName(uint32 setId) const;
    std::vector<uint32> GetAllSetIds() const;

    // 核心功能
    bool AssignRandomSetToItem(Player* player, uint32 itemId, uint8 slot = 0); // slot参数已废弃，保留为兼容性
    void RefreshPlayerSetEffects(Player* player);

    // 增量更新优化函数
    void RefreshPlayerSetEffectsOptimized(Player* player, uint8 changedSlot = 0xFF);
    void RebuildEquipmentCache(Player* player);
    void ApplySingleSetEffect(Player* player, uint32 setId, uint8 currentCount);

    bool DeactivateItemSet(Player* player, uint32 setId);

    // 数据管理
    void LoadPlayerSetStatus(Player* player);
    void SavePlayerSetStatus(Player* player);
    PlayerSetStatus* GetPlayerSetStatus(uint32 playerGuid);

    // 工具函数
    uint32 GetRandomSetForGroup(uint32 groupId);
    uint32 GetRandomSetForGroup(uint32 groupId, Player* player); // 考虑玩家需求的版本
    uint32 GetItemSetId(Player* player, uint32 itemGuid) const;
    bool CheckSetRequirements(Player* player, uint32 setId) const;
    bool ConsumeSetRequirements(Player* player, uint32 setId) const;
    void ShowGroupRequirements(Player* player, uint32 groupId); // 显示组内套装的需求条件
    void LoadSetGroups();
    void CheckAndUpdatePlayerSets(Player* player);
    void ClearPlayerSetStatus(uint32 playerGuid);
    void ClearPlayerMemoryCache(uint32 playerGuid); // 【内存泄漏修复】只清理内存缓存

    // GUID存储函数
    void StoreGuidInEnchantmentSlot(Item* item);

    // 物品查找函数
    Item* FindItemByGuid(Player* player, uint32 itemGuid);

    // 客户端通信函数
    void SendItemSetDataToClient(Player* player);
    void SendSetEffectDescriptionToClient(Player* player, const std::string& effectDesc);
    void SendAllSetEffectDescriptions(Player* player);

    // 套装效果处理
    void CalculateAndApplySetEffects(Player* player);

    // 技能增强效果管理
    void AddSpellEnhancement(Player* player, uint32 spellId, float enhancePercent);
    void RemoveSpellEnhancement(Player* player, uint32 spellId);
    void ClearAllSpellEnhancements(Player* player);
    std::vector<SpellEnhancement> GetPlayerSpellEnhancements(uint32 playerGuid) const;

    // 套装效果清除
    void ClearAllSetEffects(Player* player, bool skipUpdate = false);

    // 属性效果应用（需要被外部调用）
    void ApplyStatEffects(Player* player, const std::string& attributes, bool apply);

private:
    ItemSetsManager() = default;
    ~ItemSetsManager() = default;

    std::map<uint32, std::vector<ItemSetData>> _itemSets;        // 套装ID -> 套装数据列表
    std::map<uint32, SetGroupInfo> _setGroups;                   // 组ID -> 组信息
    std::map<uint32, PlayerSetStatus> _playerSetStatus;          // 玩家GUID -> 套装状态
    std::map<uint32, std::vector<SpellEnhancement>> _playerSpellEnhancements; // 玩家GUID -> 技能增强列表
    std::map<uint32, std::string> _lastSentEquipState;           // 玩家GUID -> 上次发送的装备状态哈希

    // 装备缓存 - 用于增量更新优化
    std::map<uint32, EquipmentCache> _playerEquipmentCache;      // 玩家GUID -> 装备缓存

    // 效果应用函数
    void ApplySkillEffects(Player* player, const std::string& skills, bool apply);
    void ApplyAuraEffects(Player* player, const std::string& auras, bool apply);

    // 新的统一效果处理函数
    void ApplyEnhancedEffect(Player* player, uint32 effectId, float effectValue, bool apply);
    void CastEnhancedSpell(Player* player, uint32 spellId, float enhancePercent);
    void ApplyEnhancedAura(Player* player, uint32 auraId, float enhancePercent, bool apply);
    bool IsSpellEffect(uint32 spellId) const;
    bool IsAuraEffect(uint32 spellId) const;

    // 套装效果处理
    void ApplySetEffects(Player* player, uint32 setId, const std::string& attributes, bool apply);

    // 辅助函数
    std::string ParseAttributesToDescription(const std::string& attributes);
    std::string GetAttributeName(uint32 attributeType) const;

    // 统一更新玩家属性的辅助函数
    void UpdatePlayerStats(Player* player);

    // 属性解析函数
    std::vector<std::pair<uint32, int64>> ParseAttributes(const std::string& attributeStr) const;
    std::vector<uint32> ParseSkillsOrAuras(const std::string& dataStr) const;
};

// 命令处理类
class ItemSetsCommands
{
public:
    // 命令处理函数
    static bool HandleItemSetsList(ChatHandler* handler, const char* args);
    static bool HandleItemSetsInfo(ChatHandler* handler, const char* args);
    static bool HandleItemSetsPlayer(ChatHandler* handler, const char* args);
    static bool HandleItemSetsAssign(ChatHandler* handler, const char* args);
    static bool HandleItemSetsClear(ChatHandler* handler, const char* args);
    static bool HandleItemSetsReload(ChatHandler* handler, const char* args);
    static bool HandleItemSetsCheck(ChatHandler* handler, const char* args);
    static bool HandleItemSetsRefresh(ChatHandler* handler, const char* args);
    static bool HandleItemSetsDebug(ChatHandler* handler, const char* args);

    // 客户端通信函数
    static void SendItemSetDataToClient(Player* player, PlayerSetStatus* status);

    // 特定物品检查相关函数
    static bool HandleSpecificItemCheck(ChatHandler* handler, const char* args, Player* player);
    static Item* FindItemByIdAndGuid(Player* player, uint32 itemId, uint32 itemGuid);
    static void SendSpecificItemDataToClient(Player* player, Item* item, uint32 setId, const std::string& setName, const std::vector<ItemSetData>& setDataList);
};

// 命令脚本类
class ItemSetsCommandScript : public CommandScript
{
public:
    ItemSetsCommandScript();

    std::vector<Acore::ChatCommands::ChatCommandBuilder> GetCommands() const override;
};

// 物品脚本类
class ItemSetsItemScript : public ItemScript
{
public:
    ItemSetsItemScript();

    // 物品工具提示（自定义方法）
    bool OnTooltipDisplayed(Player* player, Item* item, ItemTemplate const* proto, uint32& count);
    bool OnUse(Player* player, Item* item, SpellCastTargets const& targets) override;

private:
    void ShowItemSetInfo(Player* player, uint32 setId);
    std::string GetSlotName(uint8 slot);
};

// 玩家脚本类
class ItemSetsPlayerScript : public PlayerScript
{
public:
    ItemSetsPlayerScript();

    // 玩家事件
    void OnLogin(Player* player) override;
    void OnPlayerLoadFromDB(Player* player) override;
    void OnLogout(Player* player) override;
    void OnPlayerDelete(ObjectGuid guid, uint32 accountId) override;
    void OnEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override;
    bool CanUnequipItem(Player* player, uint16 pos, bool swap);
    void OnAfterMoveItemFromInventory(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override;
    void OnUpdate(Player* player, uint32 diff) override;
    void OnSpellCast(Player* player, Spell* spell, bool skipCheck) override;

private:
    // 用于检查装备变化的缓存 - 已废弃，使用EquipmentCache替代
    // std::map<uint32, std::set<uint32>> _lastEquippedItems;

    // 延迟刷新计时器
    std::map<uint32, uint32> _delayedRefreshTimers; // 玩家GUID -> 剩余延迟时间(毫秒)

    // 【修复】光环检查计时器 - 为每个玩家单独维护
    std::map<uint32, uint32> _auraCheckTimers; // 玩家GUID -> 计时器(毫秒)
    std::set<uint32> _initializedPlayers;

    // 辅助函数
    void CheckAndRestoreSetAuras(Player* player);
    void CheckAndRemoveKnownInvalidDebuffs(Player* player);
    bool ShouldBeSpellEnhancement(uint32 spellId);
    bool IsActiveSpellByAnalysis(SpellInfo const* spellInfo);
    bool HasSpellEnhancement(Player* player, uint32 spellId);
};

// 世界脚本类
class ItemSets_WorldScript : public WorldScript
{
public:
    ItemSets_WorldScript();

    void OnAfterConfigLoad(bool /*reload*/) override;
    void OnUpdate(uint32 diff) override;

private:
    uint32 _loadTimer;
    uint32 _loadDelay;
    bool _isLoaded;

    uint32 LoadModule();
};

// 全局单例定义
#define sItemSetsConfig ItemSetsConfig::instance()
#define sItemSetsManager ItemSetsManager::instance()

// 脚本加载函数声明
void AddSC_ItemSets();
void LoadItemSetsScripts();
void AddItemSetsScripts();

#endif // ITEM_SETS_H
