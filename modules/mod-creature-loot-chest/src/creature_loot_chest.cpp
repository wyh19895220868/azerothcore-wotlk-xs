/*
 * 生物击杀参与宝箱模块
 * Creature Loot Chest Module for AzerothCore
 */

#include "creature_loot_chest.h"
#include "GameTime.h"
#include "G3D/Quat.h"
#include "LootMgr.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"

using namespace Acore::ChatCommands;

// ============================================================
// 单例
// ============================================================
CreatureLootChestMgr* CreatureLootChestMgr::instance()
{
    static CreatureLootChestMgr inst;
    return &inst;
}

// ============================================================
// 加载配置
// ============================================================
void CreatureLootChestMgr::Load()
{
    RemoveAllChests();
    _configs.clear();
    {
        std::lock_guard<std::mutex> lock(_pendingMutex);
        _pendingContributions.clear();
        _pendingChestSpawns.clear();
        _updateSerial = 0;
    }
    _maxActiveChests = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("CreatureLootChest.MaxActiveChests", 2000));
    _maxPlayersPerChest = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("CreatureLootChest.MaxPlayersPerChest", 500));
    _maxPendingCreatures = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("CreatureLootChest.MaxPendingCreatures", 10000));
    _maxContributorsPerCreature = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("CreatureLootChest.MaxContributorsPerCreature", 500));

    QueryResult result = WorldDatabase.Query(
        "SELECT 生物ID, 宝箱ID, 宝箱掉落ID, 宝箱存在时长, 最低伤害要求, IP数量限制, IP限制白名单账号 "
        "FROM 生物击杀参与宝箱 ORDER BY 生物ID");

    if (!result)
    {
        LOG_WARN("module", "生物击杀宝箱：表为空，未加载配置");
        return;
    }

    uint32 count = 0;
    uint32 skipped = 0;
    do
    {
        Field* fields = result->Fetch();
        CreatureLootChestConfig cfg;
        cfg.creatureEntry = fields[0].Get<uint32>();
        cfg.chestEntry    = fields[1].Get<uint32>();
        cfg.lootId        = fields[2].Get<uint32>();
        cfg.existDuration = fields[3].Get<uint32>();
        cfg.minDamage     = fields[4].Get<float>();
        cfg.ipLimit       = fields[5].Get<uint8>();
        cfg.ipLimitWhitelist = fields[6].Get<std::string>();

        if (cfg.creatureEntry == 0 || cfg.chestEntry == 0 || cfg.lootId == 0)
        {
            skipped++;
            continue;
        }

        if (!LootTemplates_Gameobject.HaveLootFor(cfg.lootId))
        {
            LOG_WARN("module", "生物击杀宝箱：掉落ID {} 在 gameobject_loot_template 中不存在，跳过生物 {}",
                cfg.lootId, cfg.creatureEntry);
            skipped++;
            continue;
        }

        _configs[cfg.creatureEntry] = cfg;
        count++;
    } while (result->NextRow());

    LOG_INFO("module", "生物击杀宝箱：加载成功 {} 条，跳过 {} 条", count, skipped);
}

// ============================================================
// 每5秒更新：清理过期宝箱
// ============================================================
void CreatureLootChestMgr::Update(uint32 diff)
{
    std::vector<PendingChestSpawn> readySpawns;
    {
        std::lock_guard<std::mutex> lock(_pendingMutex);
        ++_updateSerial;

        for (auto it = _pendingChestSpawns.begin(); it != _pendingChestSpawns.end();)
        {
            if (it->readyAtUpdate > _updateSerial)
            {
                ++it;
                continue;
            }

            readySpawns.emplace_back(std::move(*it));
            it = _pendingChestSpawns.erase(it);
        }
    }

    for (PendingChestSpawn& pending : readySpawns)
        SpawnChest(std::move(pending));

    _cleanupTimerMs += diff;
    if (_cleanupTimerMs >= 5000)
    {
        _cleanupTimerMs = 0;
        CleanupExpiredChests();
    }
}

void CreatureLootChestMgr::RemoveAllChests()
{
    std::vector<std::pair<Map*, ObjectGuid>> chests;
    chests.reserve(_chestStates.size());
    for (auto const& pair : _chestStates)
        chests.emplace_back(pair.second.map, pair.first);

    _chestStates.clear();
    for (auto const& pair : chests)
    {
        if (pair.first)
        {
            if (GameObject* chest = pair.first->GetGameObject(pair.second))
                chest->Delete();
        }
    }
}

// ============================================================
// 清理过期宝箱
// ============================================================
void CreatureLootChestMgr::CleanupExpiredChests()
{
    uint32 now = GameTime::GetGameTime().count();
    std::vector<ObjectGuid> expired;

    for (auto& pair : _chestStates)
    {
        if (pair.second.expireTime <= now)
            expired.push_back(pair.first);
    }

    for (ObjectGuid chestGuid : expired)
    {
        auto it = _chestStates.find(chestGuid);
        if (it == _chestStates.end())
            continue;

        ChestState* state = &it->second;

        if (state->map)
        {
            if (GameObject* chest = state->map->GetGameObject(chestGuid))
            {
                chest->Delete();
            }
        }
    }
}

// ============================================================
// 获取配置
// ============================================================
CreatureLootChestConfig const* CreatureLootChestMgr::GetConfig(uint32 creatureEntry) const
{
    auto it = _configs.find(creatureEntry);
    if (it != _configs.end())
        return &it->second;
    return nullptr;
}

// ============================================================
// 获取宝箱状态
// ============================================================
ChestState* CreatureLootChestMgr::GetChestStateByChestGuid(ObjectGuid chestGuid)
{
    auto it = _chestStates.find(chestGuid);
    if (it != _chestStates.end())
        return &it->second;
    return nullptr;
}

// ============================================================
// 移除宝箱状态（由 OnGameObjectRemoveWorld 调用）
// ============================================================
void CreatureLootChestMgr::RemoveChestStateByChestGuid(ObjectGuid chestGuid)
{
    _chestStates.erase(chestGuid);
}

void CreatureLootChestMgr::ClearPendingContributions(ObjectGuid creatureGuid)
{
    std::lock_guard<std::mutex> lock(_pendingMutex);
    _pendingContributions.erase(creatureGuid);
}

// ============================================================
// 获取玩家IP
// ============================================================
std::string CreatureLootChestMgr::GetPlayerIP(Player* player)
{
    if (!player || !player->GetSession())
        return "";
    return player->GetSession()->GetRemoteAddress();
}

// ============================================================
// 检查IP限制
// ============================================================
bool CreatureLootChestMgr::CheckIPLimit(ChestState* state, std::string const& ip, uint32 accountId, std::string const& whitelist)
{
    if (state->ipLimit == 0)
        return true;

    if (!whitelist.empty() && accountId != 0)
    {
        std::string wl = whitelist;
        size_t pos = 0;
        while ((pos = wl.find('#')) != std::string::npos)
        {
            std::string token = wl.substr(0, pos);
            if (!token.empty())
            {
                try
                {
                    if (std::stoul(token) == accountId)
                        return true;
                }
                catch (...)
                {
                    LOG_WARN("module", "生物击杀宝箱：忽略无效白名单账号 '{}'", token);
                }
            }
            wl.erase(0, pos + 1);
        }
        if (!wl.empty())
        {
            try
            {
                if (std::stoul(wl) == accountId)
                    return true;
            }
            catch (...)
            {
                LOG_WARN("module", "生物击杀宝箱：忽略无效白名单账号 '{}'", wl);
            }
        }
    }

    uint32 currentIPCount = 0;
    for (ObjectGuid const& guid : state->lootedPlayers)
    {
        if (Player* p = ObjectAccessor::FindPlayer(guid))
        {
            if (GetPlayerIP(p) == ip)
                ++currentIPCount;
        }
    }

    return currentIPCount < state->ipLimit;
}

// ============================================================
// 生成宝箱（不写入DB，纯内存）
// ============================================================
GameObject* CreatureLootChestMgr::SpawnChest(PendingChestSpawn&& pending)
{
    Map* map = sMapMgr->FindMap(pending.mapId, pending.instanceId);
    if (!map)
        return nullptr;

    if (_chestStates.size() >= _maxActiveChests)
    {
        CleanupExpiredChests();
        if (_chestStates.size() >= _maxActiveChests)
        {
            LOG_ERROR("module", "生物击杀宝箱：活动宝箱达到上限 {}，拒绝继续生成", _maxActiveChests);
            return nullptr;
        }
    }

    CreatureLootChestConfig const* cfg = &pending.config;
    Position const& position = pending.position;

    GameObject* chest = new GameObject();
    uint32 lowGuid = sObjectMgr->GenerateGameObjectSpawnId();

    if (!chest->Create(lowGuid, cfg->chestEntry, map, pending.phaseMask,
                       position.GetPositionX(), position.GetPositionY(), position.GetPositionZ(), position.GetOrientation(),
                       G3D::Quat(0.0f, 0.0f, 0.0f, 1.0f), 0, GO_STATE_READY))
    {
        LOG_ERROR("module", "生物击杀宝箱：创建宝箱失败 entry={}", cfg->chestEntry);
        delete chest;
        return nullptr;
    }

    // 不保存到DB，纯内存存在
    if (!map->AddToMap(chest))
    {
        LOG_ERROR("module", "Creature loot chest: failed to create chest entry={}", cfg->chestEntry);
        delete chest;
        return nullptr;
    }

    // 创建宝箱状态
    ChestState state;
    state.chestGuid            = chest->GetGUID();
    state.chestEntry           = cfg->chestEntry;
    state.creatureEntry        = cfg->creatureEntry;
    state.lootId               = cfg->lootId;
    state.ipLimit              = cfg->ipLimit;
    state.ipLimitWhitelist     = cfg->ipLimitWhitelist;
    state.map                  = map;
    state.expireTime           = GameTime::GetGameTime().count() + cfg->existDuration;

    // 将生物死亡前的伤害贡献迁移到宝箱
    state.contributors         = std::move(pending.contributors);

    // 以 chestGuid 为 key 存储状态
    _chestStates[state.chestGuid] = state;

    return chest;
}

// ============================================================
// 生物死亡：生成宝箱
// ============================================================
void CreatureLootChestMgr::OnCreatureDeath(Creature* killed, Unit* /*killer*/)
{
    if (!killed)
        return;

    CreatureLootChestConfig const* cfg = GetConfig(killed->GetEntry());
    if (!cfg)
        return;

    // Unit::Kill is still finishing when this callback runs. Snapshot the
    // required data and add the game object during the next world update.
    PendingChestSpawn pending;
    pending.config = *cfg;
    pending.mapId = killed->GetMapId();
    pending.instanceId = killed->GetInstanceId();
    pending.position = killed->GetPosition();
    pending.position.m_positionZ += 5.0f;
    pending.phaseMask = killed->GetPhaseMask();

    {
        std::lock_guard<std::mutex> lock(_pendingMutex);
        if (_pendingChestSpawns.size() >= _maxPendingCreatures)
        {
            LOG_ERROR("module", "Creature loot chest: pending spawn limit {} reached for creature {}", _maxPendingCreatures, killed->GetEntry());
            _pendingContributions.erase(killed->GetGUID());
            return;
        }

        pending.readyAtUpdate = _updateSerial + 1;

        auto contributionIt = _pendingContributions.find(killed->GetGUID());
        if (contributionIt != _pendingContributions.end())
        {
            pending.contributors = std::move(contributionIt->second);
            _pendingContributions.erase(contributionIt);
        }

        _pendingChestSpawns.emplace_back(std::move(pending));
    }
}

// ============================================================
// 玩家造成伤害（累计），在宝箱生成前就记录
// ============================================================
void CreatureLootChestMgr::OnPlayerDamage(Creature* victim, Player* attacker, float damage)
{
    if (!victim || !attacker || damage == 0)
        return;

    CreatureLootChestConfig const* cfg = GetConfig(victim->GetEntry());
    if (!cfg)
        return;

    std::lock_guard<std::mutex> lock(_pendingMutex);

    auto pendingItr = _pendingContributions.find(victim->GetGUID());
    if (pendingItr == _pendingContributions.end())
    {
        if (_pendingContributions.size() >= _maxPendingCreatures)
            return;
        pendingItr = _pendingContributions.emplace(victim->GetGUID(), std::unordered_map<ObjectGuid, DamageContribution>()).first;
    }

    auto& contributors = pendingItr->second;
    if (contributors.find(attacker->GetGUID()) == contributors.end() && contributors.size() >= _maxContributorsPerCreature)
        return;
    contributors[attacker->GetGUID()].playerGuid = attacker->GetGUID();
    contributors[attacker->GetGUID()].totalDamage += damage;
    contributors[attacker->GetGUID()].ipAddress = GetPlayerIP(attacker);
}

// ============================================================
// 玩家是否可以拾取宝箱（仅返回bool）
// ============================================================
bool CreatureLootChestMgr::CanPlayerLootChest(Player* player, GameObject* chest)
{
    return CanPlayerLootChestWithReason(player, chest) == ChestLootDenyReason::None;
}

// ============================================================
// 玩家是否可以拾取宝箱（返回具体失败原因）
// ============================================================
ChestLootDenyReason CreatureLootChestMgr::CanPlayerLootChestWithReason(Player* player, GameObject* chest, float* outCurrentDamage, float* outMinDamage)
{
    if (!player || !chest)
        return ChestLootDenyReason::Unknown;

    if (!player->IsAlive())
        return ChestLootDenyReason::Dead;

    ChestState* state = GetChestStateByChestGuid(chest->GetGUID());
    if (!state)
        return ChestLootDenyReason::Unknown;

    uint32 now = GameTime::GetGameTime().count();
    if (state->expireTime <= now)
        return ChestLootDenyReason::Expired;

    // 已打开过的玩家，不受IP和伤害限制，可重复开启
    if (state->lootedPlayers.find(player->GetGUID()) != state->lootedPlayers.end())
        return ChestLootDenyReason::None;

    std::string ip = GetPlayerIP(player);
    if (!CheckIPLimit(state, ip, player->GetSession()->GetAccountId(), state->ipLimitWhitelist))
        return ChestLootDenyReason::IPLimit;

    CreatureLootChestConfig const* cfg = GetConfig(state->creatureEntry);
    float minDamage = cfg ? cfg->minDamage : 0.0f;
    if (outMinDamage)
        *outMinDamage = minDamage;

    auto it = state->contributors.find(player->GetGUID());
    if (it == state->contributors.end())
    {
        if (outCurrentDamage)
            *outCurrentDamage = 0.0f;
        return minDamage > 0.0f ? ChestLootDenyReason::DamageTooLow : ChestLootDenyReason::DamageTooLow;
    }

    if (outCurrentDamage)
        *outCurrentDamage = it->second.totalDamage;

    if (minDamage > 0.0f && it->second.totalDamage < minDamage)
        return ChestLootDenyReason::DamageTooLow;

    return ChestLootDenyReason::None;
}

// ============================================================
// 玩家完成拾取
// ============================================================
void CreatureLootChestMgr::OnPlayerLootedChest(Player* player, GameObject* chest)
{
    if (!player || !chest)
        return;

    ChestState* state = GetChestStateByChestGuid(chest->GetGUID());
    if (!state)
        return;

    state->lootedPlayers.insert(player->GetGUID());
}

// ============================================================
// 获取宝箱的掉落ID
// ============================================================
uint32 CreatureLootChestMgr::GetChestLootId(GameObject* chest)
{
    if (!chest)
        return 0;

    ChestState* state = GetChestStateByChestGuid(chest->GetGUID());
    if (!state)
        return 0;

    return state->lootId;
}

// ============================================================
// 获取玩家独立的掉落对象（不存在则自动生成）
// ============================================================
Loot* CreatureLootChestMgr::GetPlayerChestLoot(Player* player, GameObject* chest)
{
    if (!player || !chest)
        return nullptr;

    ChestState* state = GetChestStateByChestGuid(chest->GetGUID());
    if (!state)
        return nullptr;

    ObjectGuid playerGuid = player->GetGUID();

    auto it = state->playerLoots.find(playerGuid);
    if (it == state->playerLoots.end())
    {
        if (state->playerLoots.size() >= _maxPlayersPerChest)
            return nullptr;

        auto loot = std::make_shared<Loot>();
        loot->sourceGameObject = chest;
        loot->FillLoot(state->lootId, LootTemplates_Gameobject, player, true, false, LOOT_MODE_DEFAULT, chest);
        state->playerLoots[playerGuid] = std::move(loot);
        it = state->playerLoots.find(playerGuid);
    }

    return it->second.get();
}

// ============================================================
// PlayerScript：捕获玩家击杀生物（用于生成宝箱）
// ============================================================
class CreatureLootChestPlayerScript : public PlayerScript
{
public:
    CreatureLootChestPlayerScript() : PlayerScript("CreatureLootChestPlayerScript") {}

    void OnCreatureKill(Player* /*killer*/, Creature* creature) override
    {
        if (!creature)
            return;
        sCreatureLootChestMgr->OnCreatureDeath(creature, nullptr);
    }
};

// ============================================================
// UnitScript：捕获玩家造成伤害（累计）
// ============================================================
class CreatureLootChestUnitScript : public UnitScript
{
public:
    CreatureLootChestUnitScript() : UnitScript("CreatureLootChestUnitScript") {}

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!attacker || !victim || damage == 0)
            return;

        Player* player = attacker->ToPlayer();
        if (!player)
            return;

        Creature* creature = victim->ToCreature();
        if (!creature)
            return;

        sCreatureLootChestMgr->OnPlayerDamage(creature, player, (float)damage);
    }
};

// ============================================================
// AllGameObjectScript：捕获宝箱状态变化和删除
// ============================================================
class CreatureLootChestAllGOScript : public AllGameObjectScript
{
public:
    CreatureLootChestAllGOScript() : AllGameObjectScript("CreatureLootChestAllGOScript") {}

    void OnGameObjectLootStateChanged(GameObject* go, uint32 state, Unit* /*unit*/) override
    {
        if (!go || state != GO_JUST_DEACTIVATED)
            return;
        if (go->GetGoType() != GAMEOBJECT_TYPE_CHEST)
            return;
        if (sCreatureLootChestMgr->GetChestLootId(go) == 0)
            return;

        go->SetLootState(GO_READY);
    }

    void OnGameObjectRemoveWorld(GameObject* go) override
    {
        if (!go)
            return;
        sCreatureLootChestMgr->RemoveChestStateByChestGuid(go->GetGUID());
    }
};

class CreatureLootChestAllCreatureScript : public AllCreatureScript
{
public:
    CreatureLootChestAllCreatureScript() : AllCreatureScript("CreatureLootChestAllCreatureScript") { }

    void OnCreatureRemoveWorld(Creature* creature) override
    {
        if (creature)
            sCreatureLootChestMgr->ClearPendingContributions(creature->GetGUID());
    }
};

// ============================================================
// WorldScript：加载配置 + 定期更新
// ============================================================
class CreatureLootChestWorldScript : public WorldScript
{
public:
    CreatureLootChestWorldScript() : WorldScript("CreatureLootChestWorldScript") {}

    void OnStartup() override
    {
        sCreatureLootChestMgr->Load();
    }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload)
            sCreatureLootChestMgr->Load();
    }

    void OnUpdate(uint32 diff) override
    {
        sCreatureLootChestMgr->Update(diff);
    }
};

// ============================================================
// GM命令：重载配置
// ============================================================
class CreatureLootChestCommandScript : public CommandScript
{
public:
    CreatureLootChestCommandScript() : CommandScript("CreatureLootChestCommandScript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable reloadTable =
        {
            { "baoxiang", HandleReloadCommand, SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "reload", reloadTable },
        };

        return commandTable;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        sCreatureLootChestMgr->Load();
        handler->SendSysMessage(">> 生物击杀宝箱配置已重新加载");
        return true;
    }
};

// ============================================================
// 模块注册
// ============================================================
void Addmod_creature_loot_chestScripts()
{
    new CreatureLootChestPlayerScript();
    new CreatureLootChestUnitScript();
    new CreatureLootChestAllGOScript();
    new CreatureLootChestAllCreatureScript();
    new CreatureLootChestWorldScript();
    new CreatureLootChestCommandScript();
}
