/*
 * 生物击杀参与宝箱模块
 * Creature Loot Chest Module for AzerothCore
 *
 * 功能描述：
 * - 当生物死亡时，在死亡坐标处生成一个宝箱
 * - 只有对生物累计伤害达到最低要求的玩家才能开启宝箱
 * - 每个玩家独立拾取（首次开启后不能再开）
 * - 宝箱不写入DB，重启后消失，存在时长到达后自动删除
 */

#ifndef MOD_CREATURE_LOOT_CHEST_H
#define MOD_CREATURE_LOOT_CHEST_H

#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "GameObject.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct Loot;

enum class ChestLootDenyReason
{
    None,
    Dead,
    Expired,
    AlreadyLooted,
    IPLimit,
    NoDamageContribution,
    DamageTooLow,
    Unknown
};

struct CreatureLootChestConfig
{
    uint32 creatureEntry = 0;
    uint32 chestEntry = 0;
    uint32 lootId = 0;
    uint32 existDuration = 300;
    float minDamage = 0.0f;
    uint8 ipLimit = 0;
    std::string ipLimitWhitelist;
};

struct DamageContribution
{
    ObjectGuid playerGuid;
    float totalDamage = 0.0f;
    std::string ipAddress;
};

struct ChestState
{
    ObjectGuid chestGuid;
    uint32 chestEntry = 0;
    uint32 creatureEntry = 0;
    uint32 lootId = 0;
    uint32 expireTime = 0;
    uint8 ipLimit = 0;
    std::string ipLimitWhitelist;
    Map* map = nullptr;
    std::unordered_map<ObjectGuid, DamageContribution> contributors;
    std::unordered_set<ObjectGuid> lootedPlayers;
    std::unordered_map<ObjectGuid, std::shared_ptr<Loot>> playerLoots;
};

struct PendingChestSpawn
{
    CreatureLootChestConfig config;
    uint32 mapId = 0;
    uint32 instanceId = 0;
    Position position;
    uint32 phaseMask = 1;
    uint64 readyAtUpdate = 0;
    std::unordered_map<ObjectGuid, DamageContribution> contributors;
};

class CreatureLootChestMgr
{
public:
    static CreatureLootChestMgr* instance();

    void Load();
    void Update(uint32 diff);
    void CleanupExpiredChests();

    void OnCreatureDeath(Creature* killed, Unit* killer);
    void OnPlayerDamage(Creature* victim, Player* attacker, float damage);

    bool CanPlayerLootChest(Player* player, GameObject* chest);
    ChestLootDenyReason CanPlayerLootChestWithReason(Player* player, GameObject* chest, float* outCurrentDamage = nullptr, float* outMinDamage = nullptr);
    void OnPlayerLootedChest(Player* player, GameObject* chest);
    uint32 GetChestLootId(GameObject* chest);
    Loot* GetPlayerChestLoot(Player* player, GameObject* chest);

    void RemoveChestStateByChestGuid(ObjectGuid chestGuid);
    void ClearPendingContributions(ObjectGuid creatureGuid);

private:
    CreatureLootChestMgr() = default;
    CreatureLootChestMgr(CreatureLootChestMgr const&) = delete;
    CreatureLootChestMgr& operator=(CreatureLootChestMgr const&) = delete;

    CreatureLootChestConfig const* GetConfig(uint32 creatureEntry) const;
    ChestState* GetChestStateByChestGuid(ObjectGuid chestGuid);
    bool CheckIPLimit(ChestState* state, std::string const& ip, uint32 accountId, std::string const& whitelist);
    std::string GetPlayerIP(Player* player);
    GameObject* SpawnChest(PendingChestSpawn&& pending);
    void RemoveAllChests();

    std::unordered_map<uint32, CreatureLootChestConfig> _configs;
    std::unordered_map<ObjectGuid, ChestState> _chestStates;
    std::unordered_map<ObjectGuid, std::unordered_map<ObjectGuid, DamageContribution>> _pendingContributions;
    std::vector<PendingChestSpawn> _pendingChestSpawns;
    std::mutex _pendingMutex;
    uint32 _cleanupTimerMs = 0;
    uint64 _updateSerial = 0;
    uint32 _maxActiveChests = 2000;
    uint32 _maxPlayersPerChest = 500;
    uint32 _maxPendingCreatures = 10000;
    uint32 _maxContributorsPerCreature = 500;
};

#define sCreatureLootChestMgr CreatureLootChestMgr::instance()

#endif
