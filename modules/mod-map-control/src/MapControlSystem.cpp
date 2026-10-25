#include "MapControlSystem.h"
#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "Group.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "GameTime.h"
#include "Language.h"
#include "Log.h"
#include "Map.h"
#include "ModuleFormat.h"
#include "ObjectDefines.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "World.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <sstream>

namespace
{
    constexpr char MAP_PROGRESS_ADDON_PREFIX[] = "MPRG";

    std::string CleanAddonField(std::string text)
    {
        for (char& ch : text)
        {
            if (ch == '|' || ch == '~' || ch == '^' || ch == '\t' || ch == '\r' || ch == '\n')
                ch = ' ';
        }

        return text;
    }

    void SendMapProgressUIMessage(Player* player, std::string const& payload)
    {
        if (!player)
            return;

        std::string fullMessage = std::string(MAP_PROGRESS_ADDON_PREFIX) + "\t" + payload;
        WorldPacket packet;
        ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&packet);
    }

    std::set<uint32> ParseNumberSet(std::string const& text)
    {
        std::set<uint32> values;
        std::stringstream ss(text);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            char* end = nullptr;
            unsigned long value = std::strtoul(token.c_str(), &end, 10);
            if (end != token.c_str() && value > 0 && value <= UINT32_MAX)
                values.insert(static_cast<uint32>(value));
        }

        return values;
    }

    std::vector<uint32> ParseNumberList(std::string const& text)
    {
        std::vector<uint32> values;
        std::stringstream ss(text);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            char* end = nullptr;
            unsigned long value = std::strtoul(token.c_str(), &end, 10);
            if (end != token.c_str() && value > 0 && value <= UINT32_MAX)
                values.push_back(static_cast<uint32>(value));
        }

        return values;
    }

    std::vector<std::pair<uint32, uint32>> ParsePairList(std::string const& text)
    {
        std::vector<std::pair<uint32, uint32>> values;
        std::stringstream ss(text);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            std::stringstream pairStream(token);
            uint32 left = 0;
            uint32 right = 0;
            pairStream >> left >> right;
            if (left || right)
                values.push_back(std::make_pair(left, right));
        }

        return values;
    }

    bool IsPositiveText(std::string const& text)
    {
        return !text.empty() && text != "0";
    }

    uint32 GetPlayerMapRank(Player* player)
    {
        if (!player)
            return 0;

        uint32 mapRank = player->GetMapRank();
        if (mapRank > 0)
            return mapRank;

        Map* map = player->GetMap();
        return map ? map->GetMapRank() : 0;
    }

    std::string BuildDeathRunKey(Player* player, MapControlTemplate const& tmpl)
    {
        if (!player)
            return "";

        uint32 mapId = player->GetMapId();
        uint32 instanceId = player->GetInstanceId();
        if (Map* map = player->GetMap())
        {
            mapId = map->GetId();
            instanceId = map->GetInstanceId();
        }

        std::ostringstream key;
        key << player->GetGUID().GetCounter() << ":"
            << tmpl.id << ":"
            << mapId << ":"
            << instanceId;
        return key.str();
    }

    bool DifficultyMatches(MapControlTemplate const& tmpl, Player* player)
    {
        if (!player || tmpl.instanceDifficulty == "全部")
            return true;

        Map* map = player->GetMap();
        if (!map || !map->IsDungeon())
            return true;

        uint32 difficulty = map->GetDifficulty();
        if (tmpl.instanceDifficulty == "普通5或10人")
            return map->IsRaid() ? difficulty == RAID_DIFFICULTY_10MAN_NORMAL : difficulty == DUNGEON_DIFFICULTY_NORMAL;
        if (tmpl.instanceDifficulty == "普通25人")
            return map->IsRaid() && difficulty == RAID_DIFFICULTY_25MAN_NORMAL;
        if (tmpl.instanceDifficulty == "英雄5或10人")
            return map->IsRaid() ? difficulty == RAID_DIFFICULTY_10MAN_HEROIC : difficulty == DUNGEON_DIFFICULTY_HEROIC;
        if (tmpl.instanceDifficulty == "英雄25人")
            return map->IsRaid() && difficulty == RAID_DIFFICULTY_25MAN_HEROIC;

        return true;
    }

    bool MythicDifficultyMatches(MapControlTemplate const& tmpl, Player* player)
    {
        if (!tmpl.mythicDifficulty)
            return true;

        return GetPlayerMapRank(player) == tmpl.mythicDifficulty;
    }

    bool GroupSizeMatches(MapControlTemplate const& tmpl, Player* player)
    {
        if (!tmpl.groupSize || !player)
            return true;

        Group* group = player->GetGroup();
        uint32 members = group ? group->GetMembersCount() : 1;
        return members <= tmpl.groupSize;
    }

    bool TemplateExtraConditionsMatch(MapControlTemplate const& tmpl, Player* player)
    {
        return DifficultyMatches(tmpl, player) && MythicDifficultyMatches(tmpl, player) && GroupSizeMatches(tmpl, player);
    }

    uint32 GetTemplateMatchPriority(MapControlTemplate const& tmpl, Player* player)
    {
        if (!player)
            return 0;

        uint32 priority = 0;
        if (tmpl.mapId >= 0 && static_cast<uint32>(tmpl.mapId) == player->GetMapId())
            priority = 300;
        else if (tmpl.zoneId >= 0 && static_cast<uint32>(tmpl.zoneId) == player->GetZoneId())
            priority = 200;
        else if (tmpl.areaId >= 0 && static_cast<uint32>(tmpl.areaId) == player->GetAreaId())
            priority = 100;

        if (!priority)
            return 0;

        if (tmpl.mythicDifficulty > 0)
            priority += 20;
        if (tmpl.instanceDifficulty != "全部")
            priority += 10;
        if (tmpl.groupSize > 0)
            priority += 1;

        return priority;
    }

    void SendMapControlMessage(Player* player, char const* text)
    {
        if (player && text && *text)
            player->GetSession()->SendAreaTriggerMessage("%s", text);
    }

    void TeleportPlayerOut(Player* player)
    {
        if (!player)
            return;

        player->SetCanTeleport(true);
        player->TeleportToEntryPoint();
    }

    void FillPlayerPower(Player* player)
    {
        if (!player || !player->IsAlive())
            return;

        player->SetHealth(player->GetMaxHealth());
        player->SetPower(POWER_MANA, player->GetMaxPower(POWER_MANA));
        player->SetPower(POWER_RAGE, player->GetMaxPower(POWER_RAGE));
        player->SetPower(POWER_ENERGY, player->GetMaxPower(POWER_ENERGY));
        player->SetPower(POWER_RUNIC_POWER, player->GetMaxPower(POWER_RUNIC_POWER));
    }

    void ApplyAreaType(Player* player, std::string const& areaType)
    {
        if (!player)
            return;

        if (areaType == "安全区" || areaType == "绝对安全区")
        {
            player->pvpInfo.IsInFFAPvPArea = false;
            player->UpdateFFAPvPState(false);
            player->SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
            player->pvpInfo.IsInNoPvPArea = true;
            player->CombatStopWithPets();
            return;
        }

        if (areaType == "自由PVP" || areaType == "公会PVP")
        {
            player->pvpInfo.IsInFFAPvPArea = true;
            player->UpdateFFAPvPState(false);
            player->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
            player->pvpInfo.IsInNoPvPArea = false;
            return;
        }

        if (areaType == "自定义阵营")
        {
            player->pvpInfo.IsInFFAPvPArea = false;
            player->UpdateFFAPvPState(false);
            player->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
            player->pvpInfo.IsInNoPvPArea = false;
            player->UpdatePvPState();
        }
    }

    void ClearAreaType(Player* player)
    {
        if (!player)
            return;

        player->pvpInfo.IsInFFAPvPArea = false;
        player->UpdateFFAPvPState(false);
        player->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
        player->pvpInfo.IsInNoPvPArea = false;
        player->UpdatePvPState();
    }

    bool IsAllowedFlightAura(Player* player, MapControlTemplate const& tmpl)
    {
        if (!player || tmpl.allowedMountSet.empty())
            return false;

        for (uint32 spellId : tmpl.allowedMountSet)
            if (player->HasAura(spellId))
                return true;

        return false;
    }

    void EnforceFlightRule(Player* player, MapControlTemplate const& tmpl)
    {
        if (!player || tmpl.disallowFlight == 0 || IsAllowedFlightAura(player, tmpl))
            return;

        if (player->HasAuraType(SPELL_AURA_FLY) || player->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) || player->CanFly())
        {
            player->Dismount();
            player->RemoveAurasByType(SPELL_AURA_FLY);
            player->RemoveAurasByType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED);
            player->RemoveAurasByType(SPELL_AURA_MOD_INCREASE_FLIGHT_SPEED);
            player->SetCanFly(false);
            SendMapControlMessage(player, "当前地图禁止飞行");
        }
    }

    void ApplyVisibilityDistance(Player* player, MapControlTemplate const& tmpl)
    {
        if (!player)
            return;

        if (tmpl.visibilityDistance <= 0.0f)
        {
            player->ResetVisibilityDistanceOverride();
            return;
        }

        player->SetVisibilityDistanceOverride(tmpl.visibilityDistance);
    }

    bool TeleportToRandomCoord(Player* player, std::string const& coordList)
    {
        if (!player)
            return false;

        std::vector<uint32> coords = ParseNumberList(coordList);
        if (coords.empty())
            return false;

        uint32 coordId = coords[urand(0, static_cast<uint32>(coords.size() - 1))];
        auto itr = PosMap.find(coordId);
        if (itr == PosMap.end())
            return false;

        player->SpawnCorpseBones();
        player->TeleportTo(itr->second.MapId, itr->second.X, itr->second.Y, itr->second.Z, itr->second.O);
        return true;
    }

    bool IsItemBlockedByRule(ItemTemplate const* proto, MapControlTemplate const& tmpl)
    {
        if (!proto)
            return false;

        if (tmpl.disabledItemSet.find(proto->ItemId) != tmpl.disabledItemSet.end())
            return true;

        return false;
    }

    bool IsFlightItemBlockedByRule(ItemTemplate const* proto, MapControlTemplate const& tmpl)
    {
        if (!proto || tmpl.disallowFlight == 0)
            return false;

        if (tmpl.allowedMountSet.find(proto->ItemId) != tmpl.allowedMountSet.end())
            return false;

        for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            int32 rawSpellId = proto->Spells[i].SpellId;
            if (rawSpellId <= 0)
                continue;

            uint32 spellId = static_cast<uint32>(rawSpellId);
            if (tmpl.allowedMountSet.find(spellId) != tmpl.allowedMountSet.end())
                return false;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (spellInfo && (spellInfo->HasAura(SPELL_AURA_FLY) || spellInfo->HasAura(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED)))
                return true;
        }

        return false;
    }
}

MapControlSystem* MapControlSystem::instance()
{
    static MapControlSystem instance;
    return &instance;
}

void MapControlSystem::LoadTemplates()
{
    _templates.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT "
        "`注释`, `id`, `副本难度`, `秘境难度`, `队伍人数`, `map`, `zone`, `area`, `条件需求`, `区域属性`, "
        "`进入自动满血`, `是否允许组队`, `是否允许飞行`, "
        "`禁用技能_多个逗号隔开`, `禁用物品_多个逗号隔开`, `获得光环_多个逗号隔开`, `允许坐骑id_多个逗号隔开`, `可视距离`, "
        "`__采集_坐标_id_联盟死亡复活点_多个随机逗号隔开`, `__采集_坐标_id_部落死亡复活点_多个随机逗号隔开`, "
        "`地图进度标题`, `地图死亡次数限制x`, `地图挑战时间秒x`, `地图进度值x`, `地图进度值奖励x`, "
        "`地图进度值奖励对象x`, `地图完成进度公告x`, `地图进度是否循环`, `进入地图公告` "
        "FROM `_模板_地图控制` ORDER BY `id`");

    if (!result)
    {
        LOG_WARN("module.map-control", ">> _模板_地图控制 表为空或不存在");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        MapControlTemplate tmpl;
        tmpl.comment = fields[0].Get<std::string>();
        tmpl.id = fields[1].Get<uint32>();
        tmpl.instanceDifficulty = fields[2].Get<std::string>();
        tmpl.mythicDifficulty = fields[3].Get<uint32>();
        tmpl.groupSize = fields[4].Get<uint32>();
        tmpl.mapId = fields[5].Get<int32>();
        tmpl.zoneId = fields[6].Get<int32>();
        tmpl.areaId = fields[7].Get<int32>();
        tmpl.requirementId = fields[8].Get<int32>();
        tmpl.areaType = fields[9].Get<std::string>();
        tmpl.healOnEnter = fields[10].Get<uint32>();
        tmpl.disallowGroup = fields[11].Get<uint32>();
        tmpl.disallowFlight = fields[12].Get<uint32>();
        tmpl.disabledSpells = fields[13].Get<std::string>();
        tmpl.disabledItems = fields[14].Get<std::string>();
        tmpl.grantAuras = fields[15].Get<std::string>();
        tmpl.allowedMounts = fields[16].Get<std::string>();
        tmpl.visibilityDistance = fields[17].Get<float>();
        tmpl.allianceDeathRespawnCoords = fields[18].Get<std::string>();
        tmpl.hordeDeathRespawnCoords = fields[19].Get<std::string>();
        tmpl.progressTitle = fields[20].Get<std::string>();
        tmpl.deathLimit = fields[21].Get<uint32>();
        tmpl.challengeTimeSeconds = fields[22].Get<uint32>();
        tmpl.progressValues = fields[23].Get<std::string>();
        tmpl.progressRewards = fields[24].Get<std::string>();
        tmpl.progressRewardTargets = fields[25].Get<std::string>();
        tmpl.progressRewardAnnounce = fields[26].Get<uint32>();
        tmpl.progressLoop = fields[27].Get<uint32>();
        tmpl.enterMapAnnounce = fields[28].Get<uint32>();
        tmpl.disabledSpellSet = ParseNumberSet(tmpl.disabledSpells);
        tmpl.disabledItemSet = ParseNumberSet(tmpl.disabledItems);
        tmpl.grantAuraSet = ParseNumberSet(tmpl.grantAuras);
        tmpl.allowedMountSet = ParseNumberSet(tmpl.allowedMounts);
        tmpl.progressValuePairs = ParsePairList(tmpl.progressValues);
        tmpl.progressRewardPairs = ParsePairList(tmpl.progressRewards);
        tmpl.progressRewardTargetPairs = ParsePairList(tmpl.progressRewardTargets);
        for (auto const& target : tmpl.progressRewardTargetPairs)
            tmpl.progressRewardTargetMap[target.first] = target.second;
        std::sort(tmpl.progressRewardPairs.begin(), tmpl.progressRewardPairs.end(),
            [](std::pair<uint32, uint32> const& left, std::pair<uint32, uint32> const& right)
            {
                return left.first < right.first;
            });

        if (tmpl.id == 0)
        {
            LOG_WARN("module.map-control", "跳过 id=0 的 _模板_地图控制 配置");
            continue;
        }

        _templates[tmpl.id] = tmpl;
    } while (result->NextRow());

    LOG_INFO("server.loading", "→_模板_地图控制 加载完成√");
}

void MapControlSystem::Reload()
{
    LoadTemplates();

    for (PlayerStateShard& shard : _playerStateShards)
    {
        std::lock_guard<std::recursive_mutex> lock(shard.mutex);
        shard.playerStates.clear();
        shard.deathCountsByRun.clear();
    }
}

MapControlTemplate const* MapControlSystem::GetTemplate(uint32 id) const
{
    auto itr = _templates.find(id);
    if (itr == _templates.end())
        return nullptr;

    return &itr->second;
}

size_t MapControlSystem::GetTemplateCount() const
{
    return _templates.size();
}

MapControlTemplate const* MapControlSystem::FindTemplateForPlayer(Player* player) const
{
    MapControlTemplate const* best = nullptr;
    uint32 bestPriority = 0;

    for (auto const& pair : _templates)
    {
        MapControlTemplate const& tmpl = pair.second;
        uint32 priority = GetTemplateMatchPriority(tmpl, player);
        if (!priority || priority < bestPriority)
            continue;

        if (!TemplateExtraConditionsMatch(tmpl, player))
            continue;

        if (!best || priority > bestPriority || tmpl.id < best->id)
        {
            best = &tmpl;
            bestPriority = priority;
        }
    }

    return best;
}

void MapControlSystem::ApplyToPlayer(Player* player, bool mapChanged)
{
    if (!player || !player->IsInWorld() || player->IsGameMaster())
        return;

    uint32 guid = player->GetGUID().GetCounter();
    PlayerStateShard& shard = GetPlayerStateShard(guid);
    std::lock_guard<std::recursive_mutex> lock(shard.mutex);
    MapControlPlayerState& state = shard.playerStates[guid];
    state.offlineSince = 0;
    MapControlTemplate const* tmpl = FindTemplateForPlayer(player);

    if (!tmpl)
    {
        if (state.activeTemplateId)
        {
            for (uint32 auraId : state.appliedAuras)
                player->RemoveAura(auraId);

            ClearAreaType(player);
            player->ResetVisibilityDistanceOverride();
            SendMapProgressUIMessage(player, "CLEAR|离开地图控制区域");
        }

        shard.playerStates.erase(guid);
        return;
    }

    uint32 currentMapId = player->GetMapId();
    uint32 currentInstanceId = player->GetInstanceId();
    bool firstApply = state.activeTemplateId != tmpl->id;
    bool enteredMap = firstApply || (mapChanged && (!state.locationInitialized ||
        state.activeMapId != currentMapId || state.activeInstanceId != currentInstanceId));
    if (firstApply)
    {
        for (uint32 auraId : state.appliedAuras)
            player->RemoveAura(auraId);

        state = MapControlPlayerState();
        state.activeTemplateId = tmpl->id;
        state.startTime = static_cast<uint32>(GameTime::GetGameTime().count());
    }

    state.activeMapId = currentMapId;
    state.activeInstanceId = currentInstanceId;
    state.locationInitialized = true;

    if (tmpl->deathLimit)
    {
        std::string deathKey = BuildDeathRunKey(player, *tmpl);
        auto deathItr = shard.deathCountsByRun.find(deathKey);
        uint32 storedDeathCount = deathItr != shard.deathCountsByRun.end() ? deathItr->second : 0;
        if (firstApply || storedDeathCount > state.deathCount)
            state.deathCount = storedDeathCount;

        if (state.deathCount >= tmpl->deathLimit)
        {
            SendProgressAddonData(player);
            SendMapControlMessage(player, "当前副本死亡次数已用完，无法进入");
            TeleportPlayerOut(player);
            return;
        }
    }

    if (tmpl->requirementId > 0 && !sHelper->NotDeleteCheckNeed(player, static_cast<uint32>(tmpl->requirementId)))
    {
        SendMapControlMessage(player, "不满足当前地图进入条件，已传出地图");
        TeleportPlayerOut(player);
        return;
    }

    if (tmpl->disallowGroup && player->GetGroup())
    {
        player->RemoveFromGroup();
        SendMapControlMessage(player, "当前地图禁止组队");
    }

    if (tmpl->healOnEnter && enteredMap)
        FillPlayerPower(player);

    ApplyAreaType(player, tmpl->areaType);
    ApplyVisibilityDistance(player, *tmpl);
    EnforceFlightRule(player, *tmpl);

    for (uint32 auraId : tmpl->grantAuraSet)
    {
        if (!player->HasAura(auraId))
            player->AddAura(auraId, player);

        state.appliedAuras.insert(auraId);
    }

    if (enteredMap)
    {
        if (tmpl->enterMapAnnounce)
            sHelper->SendSerMessage("[地图控制]: %s 进入地图控制区域 [%s]", sHelper->GetNameLinkColor(player).c_str(), tmpl->comment.empty() ? "未命名" : tmpl->comment.c_str());
    }

    SendProgressAddonData(player);
}

void MapControlSystem::UpdatePlayer(Player* player, uint32 diff)
{
    if (!player || !player->IsInWorld())
        return;

    CleanupExpiredPlayerStates();

    uint32 guid = player->GetGUID().GetCounter();
    PlayerStateShard& shard = GetPlayerStateShard(guid);
    std::lock_guard<std::recursive_mutex> lock(shard.mutex);
    auto stateItr = shard.playerStates.find(guid);
    if (stateItr == shard.playerStates.end())
    {
        ApplyToPlayer(player, false);
        return;
    }

    if (stateItr->second.updateTimer > diff)
    {
        stateItr->second.updateTimer -= diff;
        return;
    }

    stateItr->second.updateTimer = 1000;
    ApplyToPlayer(player, false);

    stateItr = shard.playerStates.find(guid);
    if (stateItr == shard.playerStates.end())
        return;

    MapControlTemplate const* tmpl = GetTemplate(stateItr->second.activeTemplateId);
    if (!tmpl || !tmpl->challengeTimeSeconds)
        return;

    uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
    if (stateItr->second.startTime && now > stateItr->second.startTime && now - stateItr->second.startTime >= tmpl->challengeTimeSeconds)
    {
        SendMapControlMessage(player, "地图挑战时间已结束，已传回进入点");
        TeleportPlayerOut(player);
    }
}

void MapControlSystem::ClearPlayerState(Player* player)
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    PlayerStateShard& shard = GetPlayerStateShard(guid);
    std::lock_guard<std::recursive_mutex> lock(shard.mutex);
    auto itr = shard.playerStates.find(guid);
    if (itr != shard.playerStates.end())
    {
        for (uint32 auraId : itr->second.appliedAuras)
            player->RemoveAura(auraId);

        itr->second.offlineSince = static_cast<uint32>(GameTime::GetGameTime().count());
        itr->second.lastProgressPayload.clear();
    }

    // 小退期间保留进度、奖励领取状态和挑战计时，上线后继续当前挑战。
    // 光环会在 ApplyToPlayer 中重新补上，因此这里只移除离线角色身上的效果。
    SendMapProgressUIMessage(player, "CLEAR|玩家离线");
}

void MapControlSystem::SendProgressAddonData(Player* player)
{
    if (!player || !player->IsInWorld())
        return;

    uint32 guid = player->GetGUID().GetCounter();
    PlayerStateShard& shard = GetPlayerStateShard(guid);
    std::lock_guard<std::recursive_mutex> lock(shard.mutex);
    auto stateItr = shard.playerStates.find(guid);
    if (stateItr == shard.playerStates.end())
    {
        SendMapProgressUIMessage(player, "CLEAR|当前地图没有进度");
        return;
    }

    MapControlPlayerState& state = stateItr->second;
    MapControlTemplate const* tmpl = GetTemplate(state.activeTemplateId);
    if (!tmpl)
    {
        SendMapProgressUIMessage(player, "CLEAR|地图模板不存在");
        return;
    }

    uint32 nextProgress = 0;
    uint32 nextReward = 0;
    std::ostringstream rewardList;
    bool firstReward = true;
    for (auto const& reward : tmpl->progressRewardPairs)
    {
        if (!firstReward)
            rewardList << "~";

        firstReward = false;
        bool rewarded = state.rewardedProgress.find(reward.first) != state.rewardedProgress.end();
        rewardList << reward.first << "^" << reward.second << "^" << (rewarded ? 1 : 0);

        if (!nextProgress && reward.first > state.progressValue && !rewarded)
        {
            nextProgress = reward.first;
            nextReward = reward.second;
        }
    }

    std::ostringstream progressList;
    bool firstProgress = true;
    for (auto const& progress : tmpl->progressValuePairs)
    {
        if (!firstProgress)
            progressList << "~";

        firstProgress = false;
        progressList << progress.first << "^" << progress.second;
    }

    uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
    uint32 elapsed = (state.startTime && now >= state.startTime) ? (now - state.startTime) : 0;
    if (!tmpl->challengeTimeSeconds)
        elapsed = 0;
    uint32 remaining = 0;
    if (tmpl->challengeTimeSeconds)
        remaining = elapsed >= tmpl->challengeTimeSeconds ? 0 : (tmpl->challengeTimeSeconds - elapsed);

    std::ostringstream payload;
    payload << "DATA|"
            << tmpl->id << "|"
            << CleanAddonField(tmpl->progressTitle.empty() ? tmpl->comment : tmpl->progressTitle) << "|"
            << state.progressValue << "|"
            << nextProgress << "|"
            << nextReward << "|"
            << state.deathCount << "|"
            << tmpl->deathLimit << "|"
            << remaining << "|"
            << tmpl->challengeTimeSeconds << "|"
            << elapsed << "|"
            << tmpl->progressLoop << "|"
            << CleanAddonField(tmpl->comment) << "|"
            << rewardList.str() << "|"
            << progressList.str();

    std::string payloadText = payload.str();
    if (payloadText == state.lastProgressPayload)
        return;

    state.lastProgressPayload = payloadText;
    SendMapProgressUIMessage(player, payloadText);
}

void MapControlSystem::CleanupExpiredPlayerStates()
{
    uint32 const now = static_cast<uint32>(GameTime::GetGameTime().count());
    uint32 lastCleanupTime = _lastStateCleanupTime.load(std::memory_order_relaxed);
    if (lastCleanupTime && now - lastCleanupTime < 60)
        return;

    if (!_lastStateCleanupTime.compare_exchange_strong(lastCleanupTime, now, std::memory_order_relaxed))
        return;

    for (PlayerStateShard& shard : _playerStateShards)
    {
        std::lock_guard<std::recursive_mutex> lock(shard.mutex);
        std::vector<uint32> expiredPlayers;
        for (auto const& pair : shard.playerStates)
        {
            if (pair.second.offlineSince && now - pair.second.offlineSince >= 3600)
                expiredPlayers.push_back(pair.first);
        }

        for (uint32 playerGuid : expiredPlayers)
        {
            shard.playerStates.erase(playerGuid);
            std::string const prefix = std::to_string(playerGuid) + ":";
            for (auto itr = shard.deathCountsByRun.begin(); itr != shard.deathCountsByRun.end();)
            {
                if (itr->first.compare(0, prefix.size(), prefix) == 0)
                    itr = shard.deathCountsByRun.erase(itr);
                else
                    ++itr;
            }
        }
    }
}

void MapControlSystem::HandlePlayerDeath(Player* player)
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    PlayerStateShard& shard = GetPlayerStateShard(guid);
    std::lock_guard<std::recursive_mutex> lock(shard.mutex);
    auto stateItr = shard.playerStates.find(guid);
    if (stateItr == shard.playerStates.end())
        return;

    MapControlTemplate const* tmpl = GetTemplate(stateItr->second.activeTemplateId);
    if (!tmpl || !tmpl->deathLimit)
        return;

    stateItr->second.deathCount++;
    shard.deathCountsByRun[BuildDeathRunKey(player, *tmpl)] = stateItr->second.deathCount;
    SendProgressAddonData(player);
    if (stateItr->second.deathCount >= tmpl->deathLimit)
    {
        SendMapControlMessage(player, "地图死亡次数已达到上限，已传回进入点");
        TeleportPlayerOut(player);
    }
}

bool MapControlSystem::HandlePlayerRelease(Player* player)
{
    if (!player)
        return true;

    MapControlTemplate const* tmpl = FindTemplateForPlayer(player);
    if (!tmpl)
        return true;

    if (player->GetTeamId() == TEAM_ALLIANCE)
        return !TeleportToRandomCoord(player, tmpl->allianceDeathRespawnCoords);

    if (player->GetTeamId() == TEAM_HORDE)
        return !TeleportToRandomCoord(player, tmpl->hordeDeathRespawnCoords);

    return true;
}

void MapControlSystem::HandleCreatureKill(Player* player, Creature* killed)
{
    if (!player || !killed)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    PlayerStateShard& shard = GetPlayerStateShard(guid);
    std::lock_guard<std::recursive_mutex> lock(shard.mutex);
    auto stateItr = shard.playerStates.find(guid);
    if (stateItr == shard.playerStates.end())
        return;

    MapControlTemplate const* tmpl = GetTemplate(stateItr->second.activeTemplateId);
    if (!tmpl)
        return;

    if (tmpl->progressValuePairs.empty())
        return;

    uint32 rank = killed->GetCreatureTemplate() ? killed->GetCreatureTemplate()->rank : 0;
    uint32 addValue = 0;
    for (auto const& pair : tmpl->progressValuePairs)
    {
        if (pair.first == rank)
        {
            addValue = pair.second;
            break;
        }
    }

    if (!addValue)
        return;

    stateItr->second.progressValue += addValue;
    SendProgressAddonData(player);
    if (!tmpl->progressTitle.empty())
        player->SendSysMsg("%s: %u", tmpl->progressTitle.c_str(), stateItr->second.progressValue);

    for (auto const& reward : tmpl->progressRewardPairs)
    {
        uint32 requiredProgress = reward.first;
        uint32 rewardTemplate = reward.second;
        if (!requiredProgress || !rewardTemplate || stateItr->second.progressValue < requiredProgress)
            continue;

        if (stateItr->second.rewardedProgress.find(requiredProgress) != stateItr->second.rewardedProgress.end())
            continue;

        uint32 targetMode = 0;
        auto targetItr = tmpl->progressRewardTargetMap.find(requiredProgress);
        if (targetItr != tmpl->progressRewardTargetMap.end())
            targetMode = targetItr->second;

        std::vector<Player*> rewardTargets;
        Group* group = player->GetGroup();
        if (targetMode == 0 && group)
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (member && member->IsInWorld() && player->IsInMap(member))
                    rewardTargets.push_back(member);
            }
        }
        else if (targetMode == 1 && group)
        {
            std::vector<Player*> candidates;
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (member && member->IsInWorld() && player->IsInMap(member))
                    candidates.push_back(member);
            }

            if (!candidates.empty())
                rewardTargets.push_back(candidates[urand(0, static_cast<uint32>(candidates.size() - 1))]);
        }
        else
        {
            rewardTargets.push_back(player);
        }

        for (Player* target : rewardTargets)
            sHelper->DoTheReward(target, rewardTemplate, "地图进度奖励");

        if (tmpl->progressRewardAnnounce)
            sHelper->SendSerMessage("[地图控制]: %s 达成地图进度 %u，奖励模板 %u 已发放", sHelper->GetNameLinkColor(player).c_str(), requiredProgress, rewardTemplate);

        stateItr->second.rewardedProgress.insert(requiredProgress);
        SendProgressAddonData(player);
    }

    if (tmpl->progressLoop && !tmpl->progressRewardPairs.empty())
    {
        uint32 maxProgress = 0;
        for (auto const& reward : tmpl->progressRewardPairs)
            maxProgress = std::max(maxProgress, reward.first);

        if (maxProgress && stateItr->second.progressValue >= maxProgress)
        {
            stateItr->second.progressValue = 0;
            stateItr->second.rewardedProgress.clear();
            SendProgressAddonData(player);
        }
    }
}

bool MapControlSystem::CanUseItem(Player* player, ItemTemplate const* proto) const
{
    MapControlTemplate const* tmpl = FindTemplateForPlayer(player);
    if (!tmpl)
        return true;

    return !IsItemBlockedByRule(proto, *tmpl) && !IsFlightItemBlockedByRule(proto, *tmpl);
}

bool MapControlSystem::CanGroup(Player* player) const
{
    MapControlTemplate const* tmpl = FindTemplateForPlayer(player);
    return !tmpl || tmpl->disallowGroup == 0;
}

class MapControlWorldScript : public WorldScript
{
public:
    MapControlWorldScript() :
        WorldScript("MapControlWorldScript"),
        _initialized(false),
        _loadTimer(0),
        _delayTime(3 * IN_MILLISECONDS)
    {
    }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload)
        {
            sMapControlSystem->Reload();
            _initialized = true;
        }
    }

    void OnUpdate(uint32 diff) override
    {
        if (_initialized)
            return;

        _loadTimer += diff;
        if (_loadTimer < _delayTime)
            return;

        sMapControlSystem->LoadTemplates();
        _initialized = true;
    }

private:
    bool _initialized;
    uint32 _loadTimer;
    uint32 _delayTime;
};

class MapControlPlayerScript : public PlayerScript
{
public:
    MapControlPlayerScript() : PlayerScript("MapControlPlayerScript") { }

    void OnLogin(Player* player) override
    {
        sMapControlSystem->ApplyToPlayer(player, true);
    }

    void OnLogout(Player* player) override
    {
        sMapControlSystem->ClearPlayerState(player);
    }

    void OnMapChanged(Player* player) override
    {
        sMapControlSystem->ApplyToPlayer(player, true);
    }

    void OnUpdateZone(Player* player, uint32 /*newZone*/, uint32 /*newArea*/) override
    {
        sMapControlSystem->ApplyToPlayer(player, true);
    }

    void OnUpdateArea(Player* player, uint32 /*oldArea*/, uint32 /*newArea*/) override
    {
        sMapControlSystem->ApplyToPlayer(player, true);
    }

    void OnUpdate(Player* player, uint32 diff) override
    {
        sMapControlSystem->UpdatePlayer(player, diff);
    }

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        if (msg.substr(0, tabPos) != MAP_PROGRESS_ADDON_PREFIX)
            return;

        std::string command = msg.substr(tabPos + 1);
        if (command == "QUERY")
        {
            sMapControlSystem->ApplyToPlayer(player, false);
        }
    }

    void OnPlayerKilledByCreature(Creature* /*killer*/, Player* killed) override
    {
        sMapControlSystem->HandlePlayerDeath(killed);
    }

    void OnPVPKill(Player* /*killer*/, Player* killed) override
    {
        sMapControlSystem->HandlePlayerDeath(killed);
    }

    bool CanRepopAtGraveyard(Player* player) override
    {
        return sMapControlSystem->HandlePlayerRelease(player);
    }

    void OnCreatureKill(Player* killer, Creature* killed) override
    {
        sMapControlSystem->HandleCreatureKill(killer, killed);
    }

    void OnPlayerCreatureKilledByPet(Player* petOwner, Creature* killed) override
    {
        sMapControlSystem->HandleCreatureKill(petOwner, killed);
    }

    bool CanUseItem(Player* player, ItemTemplate const* proto, InventoryResult& result) override
    {
        if (!sMapControlSystem->CanUseItem(player, proto))
        {
            result = EQUIP_ERR_CANT_DO_RIGHT_NOW;
            player->SendSysMsg("当前地图禁止使用该物品");
            return false;
        }

        return true;
    }

    bool OnPlayerCanCastItemUseSpell(Player* player, Item* item, SpellCastTargets const& /*targets*/, uint8 /*cast_count*/, uint32 /*glyphIndex*/) override
    {
        if (item && !sMapControlSystem->CanUseItem(player, item->GetTemplate()))
        {
            player->SendSysMsg("当前地图禁止使用该物品");
            return false;
        }

        return true;
    }

    void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!player || !spell || !spell->GetSpellInfo())
            return;

        MapControlTemplate const* tmpl = sMapControlSystem->FindTemplateForPlayer(player);
        if (!tmpl)
            return;

        uint32 spellId = spell->GetSpellInfo()->Id;
        if (tmpl->disabledSpellSet.find(spellId) != tmpl->disabledSpellSet.end())
        {
            player->InterruptSpell(CURRENT_GENERIC_SPELL);
            player->RemoveAura(spellId);
            player->SendSysMsg("当前地图禁止使用该技能");
        }
    }

    bool OnPlayerCanGroupInvite(Player* player, std::string& /*membername*/) override
    {
        if (!sMapControlSystem->CanGroup(player))
        {
            player->SendSysMsg("当前地图禁止组队");
            return false;
        }

        return true;
    }

    bool OnPlayerCanGroupAccept(Player* player, Group* /*group*/) override
    {
        if (!sMapControlSystem->CanGroup(player))
        {
            player->SendSysMsg("当前地图禁止组队");
            return false;
        }

        return true;
    }

    void OnPlayerIsFFAPvP(Player* player, bool& result) override
    {
        MapControlTemplate const* tmpl = sMapControlSystem->FindTemplateForPlayer(player);
        if (tmpl && (tmpl->areaType == "自由PVP" || tmpl->areaType == "公会PVP"))
            result = true;
    }

    void OnPlayerIsPvP(Player* player, bool& result) override
    {
        MapControlTemplate const* tmpl = sMapControlSystem->FindTemplateForPlayer(player);
        if (tmpl && tmpl->areaType == "自定义阵营")
            result = true;
    }
};

class MapControlCommandScript : public CommandScript
{
public:
    MapControlCommandScript() : CommandScript("MapControlCommandScript") { }

    std::vector<Acore::ChatCommands::ChatCommandBuilder> GetCommands() const override
    {
        using namespace Acore::ChatCommands;

        static ChatCommandTable mapControlSubCommands =
        {
            { "重载", HandleReloadCommand, SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "地图控制", mapControlSubCommands },
        };

        return commandTable;
    }

private:
    static bool HandleReloadCommand(ChatHandler* handler, const char* /*args*/)
    {
        sMapControlSystem->Reload();
        handler->PSendSysMessage("%s", ModuleOldFormat("地图控制模板已重载，当前数量:%s", static_cast<uint32>(sMapControlSystem->GetTemplateCount())).c_str());
        return true;
    }
};

void AddMapControlSystemScripts()
{
    new MapControlWorldScript();
    new MapControlPlayerScript();
    new MapControlCommandScript();
}
