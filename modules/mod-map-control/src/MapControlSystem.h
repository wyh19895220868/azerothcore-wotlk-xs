#ifndef MODULE_MAP_CONTROL_SYSTEM_H
#define MODULE_MAP_CONTROL_SYSTEM_H

#include "Define.h"
#include <array>
#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class Creature;
class ItemTemplate;
class Player;

struct MapControlTemplate
{
    std::string comment;
    uint32 id = 0;
    std::string instanceDifficulty;
    uint32 mythicDifficulty = 0;
    uint32 groupSize = 0;
    int32 mapId = -1;
    int32 zoneId = -1;
    int32 areaId = -1;
    int32 requirementId = 0;
    std::string areaType;
    uint32 healOnEnter = 0;
    uint32 disallowGroup = 0;
    uint32 disallowFlight = 0;
    std::string disabledSpells;
    std::string disabledItems;
    std::string grantAuras;
    std::string allowedMounts;
    float visibilityDistance = 0.0f;
    std::string allianceDeathRespawnCoords;
    std::string hordeDeathRespawnCoords;
    std::string progressTitle;
    uint32 deathLimit = 0;
    uint32 challengeTimeSeconds = 0;
    std::string progressValues;
    std::string progressRewards;
    std::string progressRewardTargets;
    uint32 progressRewardAnnounce = 0;
    uint32 progressLoop = 0;
    uint32 enterMapAnnounce = 0;

    std::set<uint32> disabledSpellSet;
    std::set<uint32> disabledItemSet;
    std::set<uint32> grantAuraSet;
    std::set<uint32> allowedMountSet;
    std::vector<std::pair<uint32, uint32>> progressValuePairs;
    std::vector<std::pair<uint32, uint32>> progressRewardPairs;
    std::vector<std::pair<uint32, uint32>> progressRewardTargetPairs;
    std::map<uint32, uint32> progressRewardTargetMap;
};

struct MapControlPlayerState
{
    uint32 activeTemplateId = 0;
    uint32 updateTimer = 0;
    uint32 deathCount = 0;
    uint32 progressValue = 0;
    uint32 startTime = 0;
    uint32 lastAnnounceId = 0;
    uint32 activeMapId = 0;
    uint32 activeInstanceId = 0;
    bool locationInitialized = false;
    std::set<uint32> rewardedProgress;
    std::set<uint32> appliedAuras;
    std::string lastProgressPayload;
    uint32 offlineSince = 0;
};

class MapControlSystem
{
public:
    static MapControlSystem* instance();

    void LoadTemplates();
    void Reload();

    MapControlTemplate const* GetTemplate(uint32 id) const;
    size_t GetTemplateCount() const;
    MapControlTemplate const* FindTemplateForPlayer(Player* player) const;

    void ApplyToPlayer(Player* player, bool mapChanged);
    void UpdatePlayer(Player* player, uint32 diff);
    void ClearPlayerState(Player* player);
    void SendProgressAddonData(Player* player);
    void CleanupExpiredPlayerStates();
    void HandlePlayerDeath(Player* player);
    bool HandlePlayerRelease(Player* player);
    void HandleCreatureKill(Player* player, Creature* killed);
    bool CanUseItem(Player* player, ItemTemplate const* proto) const;
    bool CanGroup(Player* player) const;

private:
    MapControlSystem() = default;

    static constexpr std::size_t PlayerStateShardCount = 64;

    struct PlayerStateShard
    {
        std::recursive_mutex mutex;
        std::unordered_map<uint32, MapControlPlayerState> playerStates;
        std::unordered_map<std::string, uint32> deathCountsByRun;
    };

    PlayerStateShard& GetPlayerStateShard(uint32 playerGuid)
    {
        return _playerStateShards[playerGuid % PlayerStateShardCount];
    }

    std::map<uint32, MapControlTemplate> _templates;
    std::array<PlayerStateShard, PlayerStateShardCount> _playerStateShards;
    std::atomic<uint32> _lastStateCleanupTime{0};
};

#define sMapControlSystem MapControlSystem::instance()

void AddMapControlSystemScripts();

#endif // MODULE_MAP_CONTROL_SYSTEM_H
