#ifndef RUNE_SYSTEM_H
#define RUNE_SYSTEM_H

#include "Common.h"
#include "Player.h"
#include "Item.h"
#include "ScriptMgr.h"
#include "Config.h"
#include "Chat.h"
#include "Logging/Log.h"
#include "RuneTypes.h"
#include <unordered_map>

// 战斗热路径的 per-player 法术加成缓存（定义在 RuneCombatScripts.cpp）。
// 符文镶嵌/卸载/升级/转移后调用，保证加成立即生效；装备穿脱由 TTL 周期刷新。
namespace RuneCombat
{
    void InvalidateSpellBonusCache(uint32 playerGuid);
}

class RuneSystem
{
public:
    static RuneSystem* instance();

    void Initialize();
    void LoadRunesFromDB();
    void LoadRuneStorageFromDB(uint32 characterGuid);
    void LoadItemRuneStorageFromDB(uint32 itemGuid);

    // 符文操作方法
    // 注意：runeId 参数实际上是符文的物品ID (itemId)，而不是符文的记录ID
    bool AddRuneToItem(Player* player, Item* item, uint32 runeId, uint32 slotId);
    bool RemoveRuneFromItem(Player* player, Item* item, uint32 slotId);
    bool UpgradeRune(Player* player, Item* item, uint32 slotId);
    bool TransferRune(Player* player, Item* sourceItem, uint32 sourceSlot, Item* targetItem, uint32 targetSlot);

    // 获取符文信息
    CustomRuneInfo const* GetRuneInfo(uint32 runeId) const;
    CustomRuneInfo const* GetRuneInfoByItemId(uint32 itemId) const;
    std::vector<CustomRuneStorage>& GetCharacterRuneStore(uint32 characterGuid) { return _characterRuneStore[characterGuid]; }
    std::vector<CustomRuneStorage> GetCharacterRunes(uint32 characterGuid) const;
    CustomRuneStorage const* GetItemRuneStorage(uint32 itemGuid) const;

    // 获取触发效果
    std::vector<std::tuple<uint32, uint32, uint32>>& GetProcEffects(uint32 playerGuid) { return _procEffects[playerGuid]; }

    // 应用符文效果
    void ApplyRuneEffects(Player* player);
    void RemoveRuneEffects(Player* player);

    // 保存符文数据
    void SaveRuneStorage(const CustomRuneStorage& storage);
    void DeleteRuneStorage(uint32 itemGuid, uint32 slotId);

    // 【性能优化】全局符文效果缓存
    void LoadRuneEffectCache();
    void SendRuneEffectsToPlayer(Player* player);
    std::vector<std::string> const& GetCachedRuneEffectMessages();
    void ClearRuneEffectCache() { _effectCacheInitialized = false; }

private:
    RuneSystem() {}
    ~RuneSystem() {}

    static RuneSystem* _instance;

    std::map<uint32, CustomRuneInfo> _runeInfoStore;
    std::unordered_map<uint32, uint32> _runeItemIdIndex; // 物品ID -> 符文ID（热路径 O(1) 反查）
    std::map<uint32, std::vector<CustomRuneStorage>> _characterRuneStore;
    std::map<uint32, CustomRuneStorage> _itemRuneSlotStore; // 每个物品只有一条记录

    // 存储已应用的效果，用于移除效果时使用
    // 键：玩家GUID，值：<符文ID, 效果参数>对的列表
    std::map<uint32, std::vector<std::pair<uint32, uint32>>> _appliedEffects;

    // 存储触发效果，用于处理攻击事件时使用
    // 键：玩家GUID，值：<符文ID, 触发几率, 触发法术ID>的元组列表
    std::map<uint32, std::vector<std::tuple<uint32, uint32, uint32>>> _procEffects;

    // 【性能优化】全局符文效果缓存
    std::map<uint32, std::string> _runeEffectCache; // itemId -> effectDesc
    std::vector<std::string> _cachedRuneEffectMessages; // 预构建的分批消息
    bool _effectCacheInitialized = false;
};

#define sRuneSystem RuneSystem::instance()

#endif // RUNE_SYSTEM_H
