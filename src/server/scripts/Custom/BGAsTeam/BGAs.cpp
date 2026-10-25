#pragma execution_character_set("utf-8")
#include <Custom/_Helper/Helper.h>
#include "BGAs.h"
#include "BattlegroundMgr.h"
#include "Chat.h"
#include "Config.h"
#include "Containers.h"
#include "GroupMgr.h"
#include "Language.h"
#include "Log.h"
#include "Opcodes.h"
#include "ReputationMgr.h"
#include "ScriptMgr.h"
#include "GameTime.h"

#include "WorldPacket.h"
#include "WorldStatePackets.h"

#include <Battlegrounds/Zones/BattlegroundAB.h>
#include <Battlegrounds/Zones/BattlegroundWS.h>
#include <Battlegrounds/Zones/BattlegroundAV.h>
#include <Battlegrounds/Zones/BattlegroundEY.h>
#include <Battlegrounds/Zones/BattlegroundSA.h>
#include <Battlegrounds/Zones/BattlegroundIC.h>
#include "../../../scripts/Custom/AddonActive/DoAddon.h"

BGTempMap BGTempStore;

constexpr uint32 FactionFrostwolfClan = 729;
constexpr uint32 FactionStormpikeGuard = 730;
constexpr uint32 MapAlteracValley = 30;

BGAs* BGAs::Instance()
{
    static BGAs Instance;
    return &Instance;
}

void BGAs::LoadConfig()
{
    IsEnableSystem = sHelper->GetBoolean(3801);
    //进入战场后是否重置技能冷却
    IsEnableResetCooldowns = sHelper->GetBoolean(3802);
    //战场加入队列是否显示名字
    ShowPlayerName = sHelper->GetBoolean(3803);
}

TeamId BGAs::GetLowerTeamIdInBG(Battleground* bg, GroupQueueInfo* groupInfo)
{
    if (!bg)
        return TEAM_ALLIANCE;

    int32 plCountA = bg->GetPlayersCountByTeam(TEAM_ALLIANCE);
    int32 plCountH = bg->GetPlayersCountByTeam(TEAM_HORDE);
    uint32 diff = std::abs(plCountA - plCountH);

    if (diff)
    {
        return plCountA < plCountH ? TEAM_ALLIANCE : TEAM_HORDE;
    }

    return urand(0, 1) ? TEAM_ALLIANCE : TEAM_HORDE;
}

void BGAs::ValidatePlayerForBG(Battleground* bg, Player* player, TeamId teamId)
{
    if (player->GetTeamId(true) == teamId)
        return;

    BGData& bgdata = player->GetBGData();

    if (bgdata.bgTeamId != teamId)
        bgdata.bgTeamId = teamId;

    SetFakeRaceAndMorph(player);

    if (bg->GetMapId() == MapAlteracValley)
    {
        if (teamId == TEAM_HORDE)
        {
            player->GetReputationMgr().ApplyForceReaction(FactionFrostwolfClan, REP_FRIENDLY, true);
            player->GetReputationMgr().ApplyForceReaction(FactionStormpikeGuard, REP_HOSTILE, true);
        }
        else
        {
            player->GetReputationMgr().ApplyForceReaction(FactionFrostwolfClan, REP_HOSTILE, true);
            player->GetReputationMgr().ApplyForceReaction(FactionStormpikeGuard, REP_FRIENDLY, true);
        }

        player->GetReputationMgr().SendForceReactions();
    }
}

uint32 BGAs::GetAllPlayersCountInBG(Battleground* bg)
{
    return bg->GetPlayersSize();
}

uint8 BGAs::GetRandomRace(std::initializer_list<uint32> races)
{
    return Acore::Containers::SelectRandomContainerElement(races);
}

uint32 BGAs::GetMorphFromRace(uint8 race, uint8 gender)
{
    if (gender == GENDER_MALE)
    {
        switch (race)
        {
        case RACE_ORC:
            return FAKE_M_FEL_ORC;
        case RACE_DWARF:
            return FAKE_M_DWARF;
        case RACE_NIGHTELF:
            return FAKE_M_NIGHT_ELF;
        case RACE_DRAENEI:
            return FAKE_M_BROKEN_DRAENEI;
        case RACE_TROLL:
            return FAKE_M_TROLL;
        case RACE_HUMAN:
            return FAKE_M_HUMAN;
        case RACE_BLOODELF:
            return FAKE_M_BLOOD_ELF;
        case RACE_GNOME:
            return FAKE_M_GNOME;
        case RACE_TAUREN:
            return FAKE_M_TAUREN;
        default:
            return FAKE_M_BLOOD_ELF; // this should never happen, it's to fix a warning about return value
        }
    }
    else
    {
        switch (race)
        {
        case RACE_ORC:
            return FAKE_F_ORC;
        case RACE_DRAENEI:
            return FAKE_F_DRAENEI;
        case RACE_HUMAN:
            return FAKE_F_HUMAN;
        case RACE_BLOODELF:
            return FAKE_F_BLOOD_ELF;
        case RACE_GNOME:
            return FAKE_F_GNOME;
        case RACE_TAUREN:
            return FAKE_F_TAUREN;
        default:
            return FAKE_F_BLOOD_ELF; // this should never happen, it's to fix a warning about return value
        }
    }
}

void BGAs::RandomRaceMorph(uint8* race, uint32* morph, TeamId team, uint8 _class, uint8 gender)
{
    if (!race || !morph)
        return;

    // if alliance find a horde race
    if (team == TEAM_ALLIANCE)
    {
        // default race because UNDEAD morph is missing
        *race = RACE_BLOODELF;

        /*
        * TROLL FEMALE morph is missing
        * therefore MALE and FEMALE are handled in a different way
        *
        * UNDEAD is missing too but for both gender
        */
        if (gender == GENDER_MALE)
        {
            *morph = FAKE_M_BLOOD_ELF;

            switch (_class)
            {
            case CLASS_DRUID:
                *race = RACE_TAUREN;
                *morph = FAKE_M_TAUREN;
                break;
            case CLASS_SHAMAN:
            case CLASS_WARRIOR:
                // UNDEAD missing (only for WARRIOR)
                *race = GetRandomRace({ RACE_ORC, RACE_TAUREN, RACE_TROLL });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_PALADIN:
                // BLOOD ELF, so default race
                break;
            case CLASS_HUNTER:
            case CLASS_DEATH_KNIGHT:
                // UNDEAD missing (only for DEATH_KNIGHT)
                *race = GetRandomRace({ RACE_ORC, RACE_TAUREN, RACE_TROLL, RACE_BLOODELF });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_ROGUE:
                // UNDEAD missing
                *race = GetRandomRace({ RACE_ORC, RACE_TROLL, RACE_BLOODELF });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_MAGE:
            case CLASS_PRIEST:
                // UNDEAD missing
                *race = GetRandomRace({ RACE_TROLL, RACE_BLOODELF });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_WARLOCK:
                // UNDEAD missing
                *race = GetRandomRace({ RACE_ORC, RACE_BLOODELF });
                *morph = GetMorphFromRace(*race, gender);
                break;
            }
        }
        else
        {
            *morph = FAKE_F_BLOOD_ELF;

            switch (_class)
            {
            case CLASS_DRUID:
                *race = RACE_TAUREN;
                *morph = FAKE_F_TAUREN;
                break;
            case CLASS_SHAMAN:
            case CLASS_WARRIOR:
                // UNDEAD missing (only for WARRIOR)
                // TROLL FEMALE missing
                *race = GetRandomRace({ RACE_ORC, RACE_TAUREN });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_HUNTER:
            case CLASS_DEATH_KNIGHT:
                // TROLL FEMALE is missing
                // UNDEAD is missing (only for DEATH_KNIGHT)
                *race = GetRandomRace({ RACE_ORC, RACE_TAUREN, RACE_BLOODELF });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_ROGUE:
            case CLASS_WARLOCK:
                // UNDEAD is missing
                // TROLL FEMALE is missing (only for Rogue)
                *race = GetRandomRace({ RACE_ORC, RACE_BLOODELF });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_PALADIN:
                // BLOOD ELF, so default race
            case CLASS_MAGE:
            case CLASS_PRIEST:
                // UNDEAD and TROLL FEMALE morph are missing so use BLOOD ELF (default race)
                break;
            }
        }

    }
    else // otherwise find an alliance race
    {
        // default race
        *race = RACE_HUMAN;

        /*
        * FEMALE morphs DWARF and NIGHT ELF are missing
        * therefore MALE and FEMALE are handled in a different way
        *
        * removed RACE NIGHT_ELF to prevent client crash
        */
        if (gender == GENDER_MALE)
        {
            *morph = FAKE_M_HUMAN;

            switch (_class)
            {
            case CLASS_DRUID:
                *race = RACE_HUMAN; /* RACE_NIGHTELF; */
                *morph = FAKE_M_NIGHT_ELF;
                break;
            case CLASS_SHAMAN:
                *race = RACE_DRAENEI;
                *morph = FAKE_M_BROKEN_DRAENEI;
                break;
            case CLASS_WARRIOR:
            case CLASS_DEATH_KNIGHT:
                *race = GetRandomRace({ RACE_HUMAN, RACE_DWARF, RACE_GNOME, /* RACE_NIGHTELF, */ RACE_DRAENEI });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_PALADIN:
                *race = GetRandomRace({ RACE_HUMAN, RACE_DWARF, RACE_DRAENEI });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_HUNTER:
                *race = GetRandomRace({ RACE_DWARF, /* RACE_NIGHTELF, */ RACE_DRAENEI });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_ROGUE:
                *race = GetRandomRace({ RACE_HUMAN, RACE_DWARF, RACE_GNOME/* , RACE_NIGHTELF */ });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_PRIEST:
                *race = GetRandomRace({ RACE_HUMAN, RACE_DWARF, /* RACE_NIGHTELF,*/ RACE_DRAENEI });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_MAGE:
                *race = GetRandomRace({ RACE_HUMAN, RACE_GNOME, RACE_DRAENEI });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_WARLOCK:
                *race = GetRandomRace({ RACE_HUMAN, RACE_GNOME });
                *morph = GetMorphFromRace(*race, gender);
                break;
            }
        }
        else
        {
            *morph = FAKE_F_HUMAN;

            switch (_class)
            {
            case CLASS_DRUID:
                // FEMALE NIGHT ELF is missing
                break;
            case CLASS_SHAMAN:
            case CLASS_HUNTER:
                // FEMALE DWARF and NIGHT ELF are missing (only for HUNTER)
                *race = RACE_DRAENEI;
                *morph = FAKE_F_DRAENEI;
                break;
            case CLASS_WARRIOR:
            case CLASS_DEATH_KNIGHT:
            case CLASS_MAGE:
                // DWARF and NIGHT ELF are missing (only for WARRIOR and DEATH_KNIGHT)
                *race = GetRandomRace({ RACE_HUMAN, RACE_GNOME, RACE_DRAENEI });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_PALADIN:
            case CLASS_PRIEST:
                // DWARF is missing
                *race = GetRandomRace({ RACE_HUMAN, RACE_DRAENEI });
                *morph = GetMorphFromRace(*race, gender);
                break;
            case CLASS_ROGUE:
            case CLASS_WARLOCK:
                // DWARF and NIGHT ELF are missing (only for ROGUE)
                *race = GetRandomRace({ RACE_HUMAN, RACE_GNOME });
                *morph = GetMorphFromRace(*race, gender);
                break;
            }
        }
    }
}

void BGAs::SetFakeRaceAndMorph(Player* player)
{
    if (!player->InBattleground())
    {
        return;
    }

    if (player->GetTeamId(true) == player->GetBgTeamId())
    {
        return;
    }

    if (IsPlayerFake(player))
    {
        return;
    }

    uint8 FakeRace;
    uint32 FakeMorph;

    // generate random race and morph
    RandomRaceMorph(&FakeRace, &FakeMorph, player->GetTeamId(true), player->getClass(), player->getGender());

    FakePlayer fakePlayer;
    fakePlayer.FakeMorph = FakeMorph;
    fakePlayer.FakeRace = FakeRace;
    fakePlayer.FakeTeamID = player->TeamIdForRace(FakeRace);
    fakePlayer.RealMorph = player->GetDisplayId();
    fakePlayer.RealNativeMorph = player->GetNativeDisplayId();
    fakePlayer.RealRace = player->getRace(true);
    fakePlayer.RealTeamID = player->GetTeamId(true);

    _fakePlayerStore[player] = fakePlayer;

    player->setRace(FakeRace);
    SetFactionForRace(player, FakeRace);
    player->SetDisplayId(FakeMorph);
    player->SetNativeDisplayId(FakeMorph);
}

void BGAs::SetFactionForRace(Player* player, uint8 Race)
{
    if (!player)
        return;

    player->setTeamId(player->TeamIdForRace(Race));

    ChrRacesEntry const* DBCRace = sChrRacesStore.LookupEntry(Race);
    if (DBCRace)
        player->SetFaction(DBCRace->FactionID);
}

void BGAs::ClearFakePlayer(Player* player)
{
    if (!IsPlayerFake(player))
        return;

    player->setRace(_fakePlayerStore[player].RealRace);
    player->SetDisplayId(_fakePlayerStore[player].RealMorph);
    player->SetNativeDisplayId(_fakePlayerStore[player].RealNativeMorph);
    SetFactionForRace(player, _fakePlayerStore[player].RealRace);

    // Clear forced faction reactions. Rank doesn't matter here, not used when they are removed.
    player->GetReputationMgr().ApplyForceReaction(FactionFrostwolfClan, REP_FRIENDLY, false);
    player->GetReputationMgr().ApplyForceReaction(FactionStormpikeGuard, REP_FRIENDLY, false);

    _fakePlayerStore.erase(player);
}

bool BGAs::IsPlayerFake(Player* player)
{
    return player && _fakePlayerStore.find(player) != _fakePlayerStore.end();
}

void BGAs::FitPlayerInTeam(Player* player, Battleground* bg)
{
    // 预留功能，暂未实现
    if (!player || !bg)
        return;
}

void BGAs::DoForgetPlayersInBG(Player* player, Battleground* bg)
{
    if (!player || !bg)
        return;

    for (auto const& itr : bg->GetPlayers())
    {
        if (Player* _player = ObjectAccessor::FindPlayer(itr.first))
        {
            if (player->GetSession())
                player->GetSession()->SendNameQueryOpcode(_player->GetGUID());
            if (_player->GetSession())
                _player->GetSession()->SendNameQueryOpcode(player->GetGUID());
        }
    }
}

bool BGAs::IsPlayingNative(Player* player)
{
    return player->GetTeamId(true) == player->GetBGData().bgTeamId;
}

bool BGAs::CheckCrossFactionMatch(BattlegroundQueue* queue, BattlegroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers)
{
    uint32 freeA = maxPlayers;
    uint32 freeH = maxPlayers;

    queue->m_SelectionPools[TEAM_ALLIANCE].Init();
    queue->m_SelectionPools[TEAM_HORDE].Init();

    std::list<GroupQueueInfo*> groups = queue->m_QueuedGroups[bracket_id][BG_QUEUE_CFBG];
    groups.sort([](GroupQueueInfo* a, GroupQueueInfo* b) { return a->JoinTime < b->JoinTime; });

    bool startable = false;

    for (auto const& gInfo : groups)
    {
        if (gInfo->IsInvitedToBGInstanceGUID)
            continue;

        bool AddAsAlly = freeA == freeH ? (urand(0, 1) == 0) : freeA > freeH; // 空位相等时随机分配，提高匹配效率

        gInfo->teamId = AddAsAlly ? TEAM_ALLIANCE : TEAM_HORDE;

        if (queue->m_SelectionPools[AddAsAlly ? TEAM_ALLIANCE : TEAM_HORDE].AddGroup(gInfo, AddAsAlly ? maxPlayers : maxPlayers))
            AddAsAlly ? freeA -= gInfo->Players.size() : freeH -= gInfo->Players.size();
        else
            break;

        // Return when we're ready to start a BG, if we're in startup process
        if (queue->m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() >= minPlayers &&
            queue->m_SelectionPools[TEAM_HORDE].GetPlayerCount() >= minPlayers)
            startable = true;
    }

    if (startable)
        return true;

    // If we're in BG testing one player is enough
    if (sBattlegroundMgr->isTesting() && queue->m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() + queue->m_SelectionPools[TEAM_HORDE].GetPlayerCount() > 0)
        return true;

    // Return false when we didn't manage to fill the BattleGround in Filling "mode".
    // reset selectionpool for further attempts
    queue->m_SelectionPools[TEAM_ALLIANCE].Init();
    queue->m_SelectionPools[TEAM_HORDE].Init();
    return false;
}

bool BGAs::FillPlayersToCFBG(BattlegroundQueue* bgqueue, Battleground* bg, BattlegroundBracketId bracket_id)
{
    if (!IsEnableSystem || bg->isArena() || bg->isRated())
        return false;

    uint32 freeA = bg->GetFreeSlotsForTeam(TEAM_ALLIANCE);
    uint32 freeH = bg->GetFreeSlotsForTeam(TEAM_HORDE);

    uint32 maxA = freeA;
    uint32 maxH = freeH;

    std::list<GroupQueueInfo*> groups = bgqueue->m_QueuedGroups[bracket_id][BG_QUEUE_CFBG];
    groups.sort([](GroupQueueInfo* a, GroupQueueInfo* b) { return a->JoinTime < b->JoinTime; });

    for (auto const& gInfo : groups)
    {
        if (gInfo->IsInvitedToBGInstanceGUID)
            continue;

        TeamId targetTeam = GetLowerTeamIdInBG(bg, gInfo);
        gInfo->teamId = targetTeam;

        uint32& freeSlots = targetTeam == TEAM_ALLIANCE ? freeA : freeH;
        uint32 maxSlots = targetTeam == TEAM_ALLIANCE ? maxA : maxH;

        if (freeSlots >= gInfo->Players.size() &&
            bgqueue->m_SelectionPools[targetTeam].AddGroup(gInfo, maxSlots))
        {
            freeSlots -= gInfo->Players.size();
        }

        if (freeA == 0 && freeH == 0)
            break;
    }

    // If we're in BG testing one player is enough
    if (sBattlegroundMgr->isTesting() && bgqueue->m_SelectionPools[TEAM_ALLIANCE].GetPlayerCount() + bgqueue->m_SelectionPools[TEAM_HORDE].GetPlayerCount() > 0)
        return true;

    // 返回是否至少有一个玩家被添加到队列
    return (freeA < maxA || freeH < maxH);
}

std::unordered_map<ObjectGuid, Seconds> BGSpamProtectionCFBG;
void BGAs::SendMessageQueue(BattlegroundQueue* bgQueue, Battleground* bg, PvPDifficultyEntry const* bracketEntry, Player* leader)
{
    if (!bgQueue || !bg || !bracketEntry || !leader)
        return;

    // 清理过期的防刷屏数据，防止内存泄漏
    auto now = GameTime::GetGameTime();
    uint32 delay = sWorld->getIntConfig(CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_SPAM_DELAY);
    for (auto it = BGSpamProtectionCFBG.begin(); it != BGSpamProtectionCFBG.end();)
    {
        if (now - it->second > Seconds(delay * 2))
            it = BGSpamProtectionCFBG.erase(it);
        else
            ++it;
    }

    BattlegroundBracketId bracketId = bracketEntry->GetBracketId();

    auto bgName = bg->GetName();
    uint32 q_min_level = std::min(bracketEntry->minLevel, (uint32)80);
    uint32 q_max_level = std::min(bracketEntry->maxLevel, (uint32)80);
    uint32 MinPlayers = bg->GetMinPlayersPerTeam() * 2;
    uint32 qTotal = bgQueue->GetPlayersCountInGroupsQueue(bracketId, (BattlegroundQueueGroupTypes)BG_QUEUE_CFBG);

    if (sWorld->getBoolConfig(CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_PLAYERONLY))
    {
        leader->SendSysMsg("[战场] %s (战场等级: %u - %u) 已排队: %u - %u", bgName.c_str(), q_min_level, q_max_level, qTotal, MinPlayers);
    }
    else
    {
        if (sWorld->getBoolConfig(CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_TIMED))
        {
            if (bgQueue->GetQueueAnnouncementTimer(bracketEntry->bracketId) < 0)
            {
                bgQueue->SetQueueAnnouncementTimer(bracketEntry->bracketId, sWorld->getIntConfig(CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_TIMER));
            }
        }
        else
        {
            auto searchGUID = BGSpamProtectionCFBG.find(leader->GetGUID());

            if (searchGUID == BGSpamProtectionCFBG.end())
                BGSpamProtectionCFBG[leader->GetGUID()] = 0s;

            // Skip if spam time < 30 secs (default)
            if (GameTime::GetGameTime() - BGSpamProtectionCFBG[leader->GetGUID()] < Seconds(sWorld->getIntConfig(CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_SPAM_DELAY)))
            {
                return;
            }

            // When limited, it announces only if there are at least CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_LIMIT_MIN_PLAYERS in queue
            auto limitQueueMinLevel = sWorld->getIntConfig(CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_LIMIT_MIN_LEVEL);
            if (limitQueueMinLevel != 0 && q_min_level >= limitQueueMinLevel)
            {
                // limit only RBG for 80, WSG for lower levels
                auto bgTypeToLimit = q_min_level == 80 ? BATTLEGROUND_RB : BATTLEGROUND_WS;

                if (bg->GetBgTypeID() == bgTypeToLimit && qTotal < sWorld->getIntConfig(CONFIG_BATTLEGROUND_QUEUE_ANNOUNCER_LIMIT_MIN_PLAYERS))
                {
                    return;
                }
            }

            BGSpamProtectionCFBG[leader->GetGUID()] = GameTime::GetGameTime();

            if (ShowPlayerName)
            {
                sHelper->SendSerMessage("|cffff0000[战场公告]:|r <%s> [%u-%u] %s 加入队列 当前: %u人 需要: %u人|r", bgName.c_str(), q_min_level, q_max_level, sHelper->GetNameLinkColor(leader).c_str(), qTotal, MinPlayers);
            }
            else
            {
                sHelper->SendSerMessage("|cffff0000[战场公告]:|r <%s> [%u-%u] 当前: %u人 需要: %u人|r", bgName.c_str(), q_min_level, q_max_level, qTotal, MinPlayers);
            }
        }
    }
}

class BGTeamAs : public BGScript
{
public:
    BGTeamAs() : BGScript("BGTeamAs") {}

    void OnBattlegroundBeforeAddPlayer(Battleground* bg, Player* player) override
    {
        if (!sBGAs->IsEnableSystem || !bg || bg->isArena() || !player)
            return;

        uint32 WinKill = sBGAs->GetWinKillCount(bg->GetBgTypeID());
        AsInt64 WinDmg = sBGAs->GetCheckDmg(bg->GetBgTypeID());
        AsInt64 WinHe = sBGAs->GetCheckHe(bg->GetBgTypeID());

        if (WinKill)
            player->SendSpecialMsg("[战场]: 击杀敌方人数满足 |cFFFF0000%u|r 人或官方条件即可胜利", WinKill);
        if (WinDmg > 0)
            player->SendSpecialMsg("[战场]: 伤害量需要满足 |cFFFF0000%s|r", AsInt64ToString(WinDmg).c_str());
        if (WinHe > 0)
            player->SendSpecialMsg("[战场]: 治疗量需要满足 |cFFFF0000%s|r", AsInt64ToString(WinHe).c_str());

        player->SendSpecialMsg("[混排]: 你被分配到阵营 >> [%s]", sHelper->GetBGTeamStr(player->GetBgTeamId()).c_str());

        sBGAs->ValidatePlayerForBG(bg, player, player->GetBgTeamId());

        sBGAs->DoForgetPlayersInBG(player, bg);

        uint8 TeamIndex = uint8(player->GetBgTeamId());
        switch (bg->GetBgTypeID())
        {
        case BATTLEGROUND_AV:
        {
            if (BattlegroundAV* AV = dynamic_cast<BattlegroundAV*>(bg))
            {
                WorldPackets::WorldState::UpdateWorldState WState;
                WState.VariableID = CT_ON_AV + TeamIndex;
                WState.Value = AV->KillStore[TeamIndex];
                player->GetSession()->SendPacket(WState.Write());
            }
        }
        break;
        case BATTLEGROUND_WS:
        {
            if (BattlegroundWS* WS = dynamic_cast<BattlegroundWS*>(bg))
            {
                WorldPackets::WorldState::UpdateWorldState WState;
                WState.VariableID = CT_ON_WS + TeamIndex;
                WState.Value = WS->KillStore[TeamIndex];
                player->GetSession()->SendPacket(WState.Write());
            }
        }
        break;
        case BATTLEGROUND_AB:
        {
            if (BattlegroundAB* AB = dynamic_cast<BattlegroundAB*>(bg))
            {
                WorldPackets::WorldState::UpdateWorldState WState;
                WState.VariableID = CT_ON_AB + TeamIndex;
                WState.Value = AB->KillStore[TeamIndex];
                player->GetSession()->SendPacket(WState.Write());
            }
        }
        break;
        case BATTLEGROUND_EY:
        {
            if (BattlegroundEY* EY = dynamic_cast<BattlegroundEY*>(bg))
            {
                WorldPackets::WorldState::UpdateWorldState WState;
                WState.VariableID = CT_ON_EY + TeamIndex;
                WState.Value = EY->KillStore[TeamIndex];
                player->GetSession()->SendPacket(WState.Write());
            }
        }
        break;
        case BATTLEGROUND_SA:
        {
            if (BattlegroundSA* SA = dynamic_cast<BattlegroundSA*>(bg))
            {
                WorldPackets::WorldState::UpdateWorldState WState;
                WState.VariableID = CT_ON_SA + TeamIndex;
                WState.Value = SA->KillStore[TeamIndex];
                player->GetSession()->SendPacket(WState.Write());
            }
        }
        break;
        case BATTLEGROUND_IC:
        {
            if (BattlegroundIC* IC = dynamic_cast<BattlegroundIC*>(bg))
            {
                WorldPackets::WorldState::UpdateWorldState WState;
                WState.VariableID = CT_ON_IC + TeamIndex;
                WState.Value = IC->KillStore[TeamIndex];
                player->GetSession()->SendPacket(WState.Write());
            }
        }
        break;
        default:
        {

        }
        break;
        }
    }

    void OnBattlegroundAddPlayer(Battleground* bg, Player* player) override
    {
        if (!sBGAs->IsEnableSystem || bg->isArena())
        {
            return;
        }

        sBGAs->FitPlayerInTeam(player, bg);

        if (sBGAs->IsEnableResetCooldowns)
        {
            player->RemoveArenaSpellCooldowns(true);
        }
    }

    void OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId winnerTeamId) override
    {
        if (!bg || !player || bg->isArena())
            return;

        if (sBGAs->IsPlayerFake(player))
            sBGAs->ClearFakePlayer(player);

        auto Itr = BGTempStore.find(uint32(bg->GetBgTypeID()));
        if ((Itr == BGTempStore.end()))
            return;

        const auto Score = bg->GetPlayerScores();
        auto It = Score->find(player->GetGUIDLow());
        if ((It == Score->end()))
            return;

        if ((It->second->GetAsDamageDone() >= Itr->second.Damage || It->second->GetAsHealingDone() >= Itr->second.Healing))
        {
            if (player->GetBgTeamId() == winnerTeamId)
                sHelper->DoTheReward(player, Itr->second.WinTempId, "战场胜利奖励");
            else
                sHelper->DoTheReward(player, Itr->second.LosTempId, "战场失败奖励");
        }
        else
        {
            player->SendSpecialMsg(sHelper->GetGts(1063));
        }
    }

    void OnBattlegroundRemovePlayerAtLeave(Battleground* bg, Player* player) override
    {
        if (!sBGAs->IsEnableSystem || bg->isArena())
        {
            return;
        }

        sBGAs->FitPlayerInTeam(player, bg);

        if (sBGAs->IsPlayerFake(player))
        {
            sBGAs->ClearFakePlayer(player);
        }

        switch (bg->GetBgTypeID())
        {
        case BATTLEGROUND_AV:
        {
            player->SendUpdateWorldState(CT_ON_AV, 0);
            player->SendUpdateWorldState(CT_ON_AV + 1, 0);
        }
        break;
        case BATTLEGROUND_WS:
        {
            player->SendUpdateWorldState(CT_ON_WS, 0);
            player->SendUpdateWorldState(CT_ON_WS + 1, 0);
        }
        break;
        case BATTLEGROUND_AB:
        {
            player->SendUpdateWorldState(CT_ON_AB, 0);
            player->SendUpdateWorldState(CT_ON_AB + 1, 0);
        }
        break;
        case BATTLEGROUND_EY:
        {
            player->SendUpdateWorldState(CT_ON_EY, 0);
            player->SendUpdateWorldState(CT_ON_EY + 1, 0);
        }
        break;
        case BATTLEGROUND_SA:
        {
            player->SendUpdateWorldState(CT_ON_SA, 0);
            player->SendUpdateWorldState(CT_ON_SA + 1, 0);
        }
        break;
        case BATTLEGROUND_IC:
        {
            player->SendUpdateWorldState(CT_ON_IC, 0);
            player->SendUpdateWorldState(CT_ON_IC + 1, 0);
        }
        break;
        default:
        {

        }
        break;
        }
    }

    void OnAddGroup(BattlegroundQueue* queue, GroupQueueInfo* ginfo, uint32& index, Player* /*leader*/, Group* /*group*/, BattlegroundTypeId /* bgTypeId */, PvPDifficultyEntry const* /* bracketEntry */,
        uint8 /* arenaType */, bool /* isRated */, bool /* isPremade */, uint32 /* arenaRating */, uint32 /* matchmakerRating */, uint32 /* arenaTeamId */, uint32 /* opponentsArenaTeamId */) override
    {
        if (!queue)
        {
            return;
        }

        if (sBGAs->IsEnableSystem && !ginfo->ArenaType && !ginfo->IsRated)
        {
            index = BG_QUEUE_CFBG;
        }
    }

    bool CanFillPlayersToBG(BattlegroundQueue* queue, Battleground* bg, BattlegroundBracketId bracket_id) override
    {
        if (!sBGAs->IsEnableSystem || bg->isArena())
        {
            return true;
        }

        if (sBGAs->FillPlayersToCFBG(queue, bg, bracket_id))
        {
            return false;
        }

        return true;
    }

    bool IsCheckNormalMatch(BattlegroundQueue* queue, Battleground* bg, BattlegroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers) override
    {
        if (!sBGAs->IsEnableSystem || bg->isArena())
        {
            return false;
        }

        return sBGAs->CheckCrossFactionMatch(queue, bracket_id, minPlayers, maxPlayers);
    }

    bool CanSendMessageBGQueue(BattlegroundQueue* queue, Player* leader, Battleground* bg, PvPDifficultyEntry const* bracketEntry) override
    {
        if (bg->isArena() || !sBGAs->IsEnableSystem)
        {
            // if it's arena OR the CFBG is disabled, let the core handle the announcement
            return true;
        }

        // otherwise, let the CFBG module handle the announcement
        sBGAs->SendMessageQueue(queue, bg, bracketEntry, leader);
        return false;
    }

    void OnBattlegroundKillPlayer(Battleground* BG, Player* Killer, Player* Killed) override
    {
        uint8 TeamIndex = uint8(Killer->GetBgTeamId());

        switch (BG->GetBgTypeID())
        {
        case BATTLEGROUND_AV:
        {
            if (BattlegroundAV* AV = dynamic_cast<BattlegroundAV*>(BG))
            {
                AV->KillStore[TeamIndex]++;
                AV->UpdateWorldState(CT_ON_AV + TeamIndex, AV->KillStore[TeamIndex]);
            }
        }
        break;
        case BATTLEGROUND_WS:
        {
            if (BattlegroundWS* WS = dynamic_cast<BattlegroundWS*>(BG))
            {
                WS->KillStore[TeamIndex]++;
                WS->UpdateWorldState(CT_ON_WS + TeamIndex, WS->KillStore[TeamIndex]);
            }
        }
        break;
        case BATTLEGROUND_AB:
        {
            if (BattlegroundAB* AB = dynamic_cast<BattlegroundAB*>(BG))
            {
                AB->KillStore[TeamIndex]++;
                AB->UpdateWorldState(CT_ON_AB + TeamIndex, AB->KillStore[TeamIndex]);
            }
        }
        break;
        case BATTLEGROUND_EY:
        {
            if (BattlegroundEY* EY = dynamic_cast<BattlegroundEY*>(BG))
            {
                EY->KillStore[TeamIndex]++;
                EY->UpdateWorldState(CT_ON_EY + TeamIndex, EY->KillStore[TeamIndex]);
            }
        }
        break;
        case BATTLEGROUND_SA:
        {
            if (BattlegroundSA* SA = dynamic_cast<BattlegroundSA*>(BG))
            {
                SA->KillStore[TeamIndex]++;
                SA->UpdateWorldState(CT_ON_SA + TeamIndex, SA->KillStore[TeamIndex]);
            }
        }
        break;
        case BATTLEGROUND_IC:
        {
            if (BattlegroundIC* IC = dynamic_cast<BattlegroundIC*>(BG))
            {
                IC->KillStore[TeamIndex]++;
                IC->UpdateWorldState(CT_ON_IC + TeamIndex, IC->KillStore[TeamIndex]);
            }
        }
        break;
        default:
        {

        }
        break;
        }
    }

    void OnBattlegroundUpdate(Battleground* BG, uint32 Diff) override
    {
        if (BG->GetStatus() != STATUS_IN_PROGRESS)
            return;

        switch (BG->GetBgTypeID())
        {
        case BATTLEGROUND_AV:
        {
            if (BattlegroundAV* AV = dynamic_cast<BattlegroundAV*>(BG))
            {
                if (uint32 WinKillCount = sBGAs->GetWinKillCount(AV->GetBgTypeID()))
                {
                    if (AV->KillStore[TEAM_ALLIANCE] >= WinKillCount)
                        AV->EndBattleground(TEAM_ALLIANCE);
                    else if (AV->KillStore[TEAM_HORDE] >= WinKillCount)
                        AV->EndBattleground(TEAM_HORDE);
                }
            }
        }
        break;
        case BATTLEGROUND_WS:
        {
            if (BattlegroundWS* WS = dynamic_cast<BattlegroundWS*>(BG))
            {
                if (uint32 WinKillCount = sBGAs->GetWinKillCount(WS->GetBgTypeID()))
                {
                    if (WS->KillStore[TEAM_ALLIANCE] >= WinKillCount)
                        WS->EndBattleground(TEAM_ALLIANCE);
                    else if (WS->KillStore[TEAM_HORDE] >= WinKillCount)
                        WS->EndBattleground(TEAM_HORDE);
                }
            }
        }
        break;
        case BATTLEGROUND_AB:
        {
            if (BattlegroundAB* AB = dynamic_cast<BattlegroundAB*>(BG))
            {
                if (uint32 WinKillCount = sBGAs->GetWinKillCount(AB->GetBgTypeID()))
                {
                    if (AB->KillStore[TEAM_ALLIANCE] >= WinKillCount)
                        AB->EndBattleground(TEAM_ALLIANCE);
                    else if (AB->KillStore[TEAM_HORDE] >= WinKillCount)
                        AB->EndBattleground(TEAM_HORDE);
                }
            }
        }
        break;
        case BATTLEGROUND_EY:
        {
            if (BattlegroundEY* EY = dynamic_cast<BattlegroundEY*>(BG))
            {
                if (uint32 WinKillCount = sBGAs->GetWinKillCount(EY->GetBgTypeID()))
                {
                    if (EY->KillStore[TEAM_ALLIANCE] >= WinKillCount)
                        EY->EndBattleground(TEAM_ALLIANCE);
                    else if (EY->KillStore[TEAM_HORDE] >= WinKillCount)
                        EY->EndBattleground(TEAM_HORDE);
                }
            }
        }
        break;
        case BATTLEGROUND_SA:
        {
            if (BattlegroundSA* SA = dynamic_cast<BattlegroundSA*>(BG))
            {
                if (uint32 WinKillCount = sBGAs->GetWinKillCount(SA->GetBgTypeID()))
                {
                    if (SA->KillStore[TEAM_ALLIANCE] >= WinKillCount)
                        SA->EndBattleground(TEAM_ALLIANCE);
                    else if (SA->KillStore[TEAM_HORDE] >= WinKillCount)
                        SA->EndBattleground(TEAM_HORDE);
                }
            }
        }
        break;
        case BATTLEGROUND_IC:
        {
            if (BattlegroundIC* IC = dynamic_cast<BattlegroundIC*>(BG))
            {
                if (uint32 WinKillCount = sBGAs->GetWinKillCount(IC->GetBgTypeID()))
                {
                    if (IC->KillStore[TEAM_ALLIANCE] >= WinKillCount)
                        IC->EndBattleground(TEAM_ALLIANCE);
                    else if (IC->KillStore[TEAM_HORDE] >= WinKillCount)
                        IC->EndBattleground(TEAM_HORDE);
                }
            }
        }
        break;
        default:
        {

        }
        break;
        }
    }
};

class BGTeamAs_Player : public PlayerScript
{
public:
    BGTeamAs_Player() : PlayerScript("BGTeamAs_Player") { }

    void OnLogin(Player* player) override
    {
        SessionMap const& sessions = sWorld->GetAllSessions();
        for (SessionMap::const_iterator itr = sessions.begin(); itr != sessions.end(); ++itr)
        {
            if (!itr->second)
                continue;

            Player* _player = itr->second->GetPlayer();
            if (!_player || !_player->IsInWorld())
                continue;

            if (_player->m_HeahTcut >= 1.0f)
            {
                std::ostringstream ss;
                uint32 playerGuid = _player->GetGUID().GetCounter();
                ss << playerGuid << "#" << _player->m_HeahTcut << "#3";
                sDoAddon->SendPacketToClient(player, "ThroughLimitHp", ss.str().c_str());
            }
        }

        if (!sBGAs->IsEnableSystem)
        {
            return;
        }

        if (player->GetTeamId(true) != player->GetBgTeamId())
        {
            sBGAs->FitPlayerInTeam(player, player->GetBattleground());
        }
    }

    void OnLogout(Player* player) override
    {
        if (sBGAs->IsPlayerFake(player))
            sBGAs->ClearFakePlayer(player);
    }

    bool CanJoinInBattlegroundQueue(Player* player, ObjectGuid /*BattlemasterGuid*/, BattlegroundTypeId /*BGTypeID*/, uint8 joinAsGroup, GroupJoinBattlegroundResult& err) override
    {
        if (!sBGAs->IsEnableSystem)
            return true;

        if (joinAsGroup)
        {
            Group* group = player->GetGroup();
            if (!group)
                return true;

            if (group->isRaidGroup() || group->GetMembersCount() > 3)
                err = ERR_BATTLEGROUND_JOIN_FAILED;

            return false;
        }

        return true;
    }

    void OnBeforeSendChatMessage(Player* player, uint32& type, uint32& lang, std::string& /*msg*/) override
    {
        if (!player || !sBGAs->IsEnableSystem)
            return;

        Battleground* bg = player->GetBattleground();

        if (!bg || bg->isArena())
            return;

        // skip addon lang and universal
        if (lang == LANG_UNIVERSAL || lang == LANG_ADDON)
            return;

        // skip addon and system message
        if (type == CHAT_MSG_ADDON || type == CHAT_MSG_SYSTEM)
            return;

        // to gm lang
        lang = LANG_UNIVERSAL;
    }

    bool OnReputationChange(Player* player, uint32 factionID, int32& standing, bool /*incremental*/) override
    {
        uint32 repGain = player->GetReputation(factionID);
        TeamId teamId = player->GetTeamId(true);

        if ((factionID == FACTION_FROSTWOLF_CLAN && teamId == TEAM_ALLIANCE) ||
            (factionID == FACTION_STORMPIKE_GUARD && teamId == TEAM_HORDE))
        {
            uint32 diff = standing - repGain;
            player->GetReputationMgr().ModifyReputation(sFactionStore.LookupEntry(teamId == TEAM_ALLIANCE ? FACTION_STORMPIKE_GUARD : FACTION_FROSTWOLF_CLAN), diff);
            return false;
        }

        return true;
    }
};

class BGTeamAs_World : public WorldScript
{
public:
    BGTeamAs_World() : WorldScript("BGTeamAs_World") { }

    void OnAfterConfigLoad(bool /*Reload*/) override
    {
        sBGAs->LoadConfig();
    }
};

void AddSC_BGTeamScripts()
{
    new BGTeamAs();
    new BGTeamAs_Player();
    new BGTeamAs_World();
}

bool BGAs::IsCustomBG(BattlegroundTypeId Id)
{
    auto Itr = BGTempStore.find(uint32(Id));
    return (Itr != BGTempStore.end());
}

bool BGAs::IsCheckIpToBG(Battleground* BG, Player* player)
{
    uint32 Count = 0;
    auto const& BGPlayerMap = BG->GetPlayers();
    for (auto Itr = BGPlayerMap.begin(); Itr != BGPlayerMap.end(); ++Itr)
    {
        if (Player* Bplayer = ObjectAccessor::FindPlayer(Itr->first))
        {
            if (strcmp(Bplayer->GetSession()->GetRemoteAddress().c_str(), player->GetSession()->GetRemoteAddress().c_str()) == 0)
                Count++;
        }
    }

    return (sHelper->GetInt(1092) != 0 && Count > sHelper->GetInt(1092));
}

uint32 BGAs::GetWinKillCount(BattlegroundTypeId Id)
{
    auto Itr = BGTempStore.find(uint32(Id));
    return (Itr != BGTempStore.end() ? Itr->second.WinKillCount : 0);
}

AsInt64 BGAs::GetCheckDmg(BattlegroundTypeId Id)
{
    auto Itr = BGTempStore.find(uint32(Id));
    return Itr != BGTempStore.end() ? Itr->second.Damage : AsInt64Zero();
}

AsInt64 BGAs::GetCheckHe(BattlegroundTypeId Id)
{
    auto Itr = BGTempStore.find(uint32(Id));
    return Itr != BGTempStore.end() ? Itr->second.Healing : AsInt64Zero();
}

uint32 BGAs::GetResCount(BattlegroundTypeId Id)
{
    auto Itr = BGTempStore.find(uint32(Id));
    return Itr != BGTempStore.end() ? Itr->second.ResCount : 0;
}

/*战场控制加载*/
void BGAs::LoadBGConfig()
{
    LoadConfig();

    BGTempStore.clear();
    uint32 Ct = 0;
    QueryResult Result = WorldDatabase.Query("SELECT 战场编号, 需要伤害量, 需要治疗量, 胜利奖励模版, 失败奖励模版, 胜利人头数, 旗帜或资源数量 FROM _战场控制表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 BGType = Fields[0].Get<uint32>();
            BGTemp St;
            St.Damage = AsInt64FromString(Fields[1].Get<std::string>());
            St.Healing = AsInt64FromString(Fields[2].Get<std::string>());
            St.WinTempId = Fields[3].Get<uint32>();
            St.LosTempId = Fields[4].Get<uint32>();
            St.WinKillCount = Fields[5].Get<uint32>();
            St.ResCount = Fields[6].Get<uint32>();
            BGTempStore.insert(std::make_pair(BGType, St));
            ++Ct;
        } while (Result->NextRow());
    }
}
