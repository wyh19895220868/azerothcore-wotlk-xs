#include "Common.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ArenaTeamMgr.h"
#include "GuildMgr.h"
#include "GroupMgr.h"
#include "SpellMgr.h"
#include "UpdateMask.h"
#include "World.h"
#include "ArenaTeam.h"
#include "Transport.h"
#include "Language.h"
#include "GameEventMgr.h"
#include "Spell.h"
#include "Chat.h"
#include "AccountMgr.h"
#include "InstanceSaveMgr.h"
#include "SpellAuras.h"
#include "Util.h"
#include "GossipDef.h"
#include "Vehicle.h"
#include "AchievementMgr.h"
#include "DisableMgr.h"
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "PoolMgr.h"
#include "Config.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"
#include "Pet.h"
#include <CharacterCache.h>
#include <algorithm>
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include "BattlegroundQueue.h"
#include <Time\GameTime.h>
#include "ArenaSoloMgr.h"
#include <Battlegrounds\BattlegroundMgr.h>
#pragma execution_character_set("utf-8")

ArenaSoloConfigMap ArenaSoloConfigStore;
ArenaSoloInFtMap ArenaSoloInFtStore;

const uint32 CUSTOM_ARENA_TEAM_1v1 = 1; //1V1竞技场
const uint32 GOSSIP_1 = 1;       //创建队伍
const uint32 GOSSIP_2 = 2;       //排队积分赛
const uint32 GOSSIP_3 = 3;       //离开队列
const uint32 GOSSIP_4 = 4;       //查询统计信息
const uint32 GOSSIP_5 = 5;       //解散清除队伍
const uint32 GOSSIP_666 = 666;       //显示正在进行的队伍
const uint32 GOSSIP_888 = 888;       //观战

void ArenaSoloMgr::LoadArenaSoloConfig()
{
    ArenaSoloConfigStore.clear();

    QueryResult Result = WorldDatabase.Query("SELECT 序号, 配置值 FROM _1V1竞技场配置表;");
    if (!Result)
    {
        return;
    }

    do
    {
        Field* Fields = Result->Fetch();
        uint32 Id = Fields[0].Get<uint32>();
        std::string Value = Fields[1].Get<std::string>();

        ArenaSoloConfigStore.insert(std::make_pair(Id, Value));

    } while (Result->NextRow());
}

const char* ArenaSoloMgr::GetConfig(uint32 Index)
{
    auto Itr = ArenaSoloConfigStore.find(Index);
    return Itr != ArenaSoloConfigStore.end() ? Itr->second.c_str() : "";
}

std::string ArenaSoloMgr::GetStrConfig(uint32 Index)
{
    auto Itr = ArenaSoloConfigStore.find(Index);
    return Itr != ArenaSoloConfigStore.end() ? Itr->second : "";
}

int64 ArenaSoloMgr::GetInt(uint32 Index)
{
    auto Itr = ArenaSoloConfigStore.find(Index);
    if (Itr != ArenaSoloConfigStore.end())
        return _atoi64(Itr->second.c_str());

    return -1;
}

uint64 ArenaSoloMgr::GetUInt(uint32 Index)
{
    auto Itr = ArenaSoloConfigStore.find(Index);
    if (Itr != ArenaSoloConfigStore.end())
        return _atoi64(Itr->second.c_str());

    return 0;
}

float ArenaSoloMgr::GetFloat(uint32 Index)
{
    auto Itr = ArenaSoloConfigStore.find(Index);
    if (Itr != ArenaSoloConfigStore.end())
        return atof(Itr->second.c_str());

    return 0.0f;
}

bool ArenaSoloMgr::GetBoolean(uint32 Index)
{
    auto Itr = ArenaSoloConfigStore.find(Index);
    if (Itr != ArenaSoloConfigStore.end())
    {
        std::transform(Itr->second.begin(), Itr->second.end(), Itr->second.begin(), ::toupper);
        return (Itr->second == "1" || Itr->second == "YES" || Itr->second == "TRUE" || Itr->second == "ON" || Itr->second == "是");
    }

    return false;
}

bool ArenaSoloMgr::ArenaSoloCheckTalents(Player* player)
{
    if (!sArenaSoloMgr->GetBoolean(20))
        return true;

    if (sArenaSoloMgr->GetStrConfig(21).empty() && sArenaSoloMgr->GetStrConfig(23).empty())
        return true;

    std::vector<uint32> TBanTabList;
    Tokenizer AStore(sArenaSoloMgr->GetStrConfig(21), '#');
    for (uint8 i = 0; i < AStore.size(); i++)
        TBanTabList.push_back(atoi(AStore[i]));;

    std::vector<uint32> SBanTabList;
    Tokenizer BStore(sArenaSoloMgr->GetStrConfig(23), '#');
    for (uint8 i = 0; i < BStore.size(); i++)
        SBanTabList.push_back(atoi(BStore[i]));;

    if (TBanTabList.empty() && SBanTabList.empty())
        return true;

    bool Yes = true;
    if (!SBanTabList.empty())
    {
        for (uint8 i = 0; SBanTabList[i] != 0; i++)
        {
            if (player->HasSpell(SBanTabList[i]) || player->HasAura(SBanTabList[i]))
            {
                player->SendSysMsg(sHelper->GetGts(6677), sHelper->GetSpellName(SBanTabList[i]));
                Yes = false;
            }
        }
    }

    uint32 CheckCount = sArenaSoloMgr->GetUInt(22);

    uint32 Count = 0;
    if (!TBanTabList.empty())
    {
        for (uint32 TalentId = 0; TalentId < sTalentStore.GetNumRows(); ++TalentId)
        {
            TalentEntry const* TalentInfo = sTalentStore.LookupEntry(TalentId);
            if (!TalentInfo)
                continue;

            for (int8 Rank = MAX_TALENT_RANK - 1; Rank >= 0; --Rank)
            {
                if (TalentInfo->RankID[Rank] == 0)
                    continue;

                if (player->HasTalent(TalentInfo->RankID[Rank], player->GetActiveSpec()))
                {
                    for (uint8 i = 0; TBanTabList[i] != 0; i++)
                        if (TBanTabList[i] == TalentInfo->TalentTab)
                            Count += Rank + 1;
                }
            }
        }
    }

    if (!CheckCount && Count >= CheckCount)
    {
        player->SendSysMsg(sHelper->GetGts(6678), CheckCount);
        Yes = false;
    }

    return Yes;
}

void ArenaSoloMgr::AutoInitPlayerInSoloCount()
{
    WorldSessionMgr::SessionMap const& Sessions = sWorldSessionMgr->GetAllSessions();
    for (WorldSessionMgr::SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        Player* player = Itr->second->GetPlayer();
        player->TempAtStore[uint32(AT4003)] = 0;
    }
    CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_TEMP_AT);
    Rtmt->SetData(0, 0);
    Rtmt->SetData(1, uint32(AT4003));
    CharacterDatabase.Execute(Rtmt);
}

class NpcSoloArena : public CreatureScript
{
public:
    NpcSoloArena() : CreatureScript("NpcSoloArena") { }

    void OnShowSoloInList(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (ArenaSoloInFtStore.empty())
        {
            player->SendSysMsg(sHelper->GetGts(6679));
            OnGossipHello(player, creature);
            return;
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6680), 0, GOSSIP_666);
        struct AtTemp { uint32 AGuidLow = 0; std::string AName = ""; std::string BName = ""; };
        std::vector<AtTemp> DoVect;
        for (auto Itr = ArenaSoloInFtStore.begin(); Itr != ArenaSoloInFtStore.end(); Itr++)
        {
            if (!Itr->first)
                continue;
            Player* Aplayer = ObjectAccessor::FindPlayer(Itr->second.AGuid);
            Player* Bplayer = ObjectAccessor::FindPlayer(Itr->second.BGuid);
            if (Aplayer && Bplayer)
            {
                AtTemp St;
                St.AGuidLow = Itr->second.AGuid.GetCounter();
                St.AName = Aplayer->GetName();
                St.BName = Bplayer->GetName();
                DoVect.push_back(St);
            }
        }

        Acore::Containers::RandomResize(DoVect, 30);
        for (auto Itr = DoVect.begin(); Itr != DoVect.end(); Itr++)
        {
            std::stringstream Oss;
            Oss << "进行中的比赛 [|cFF0000FF" << Itr->AName << "|r] |cFFFF0000Vs|r [|cFF0000FF" << Itr->BName << "|r]";
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Oss.str(), Itr->AGuidLow, GOSSIP_888, "\n观战期间无法聊天/攻击/施法\n并且会处于隐匿状态\n小退重新上线或比赛结束即可结束观战\n", 0, false);
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature);
    }

    void OnStartWatchBG(Player* player, Creature* creature, uint32 Sender, uint32 Action)
    {
        ClearGossipMenuFor(player);

        if (player->IsInCombat())
        {
            player->SendSysMsg(sHelper->GetGts(1001));
            OnGossipHello(player, creature);
            return;
        }

        if (player->InBattleground())
        {
            player->SendSysMsg(sHelper->GetGts(6683));
            OnGossipHello(player, creature);
            return;
        }

        if (player->InArenaWatchSolo)
        {
            player->SendSysMsg(sHelper->GetGts(6684));
            OnGossipHello(player, creature);
            return;
        }

        Player* TargetPlayer = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, Sender));
        if (!TargetPlayer)
        {
            player->SendSysMsg(sHelper->GetGts(6685));
            OnGossipHello(player, creature);
            return;
        }

        Battleground* BG = TargetPlayer->GetBattleground();
        if (!BG)
        {
            player->SendSysMsg(sHelper->GetGts(6686));
            OnGossipHello(player, creature);
            return;
        }

        player->SetEntryPoint();
        if (player->IsVisible())
            player->SetVisible(false);
        player->EntryNowPhase = player->GetPhaseMask();
        player->SetBattlegroundId(TargetPlayer->GetBattlegroundId(), TargetPlayer->GetBattlegroundTypeId(), PLAYER_MAX_BATTLEGROUND_QUEUES, false, false, TEAM_NEUTRAL);
        player->SetCanTeleport(true);
        if (player->TeleportTo(TargetPlayer->GetMapId(), TargetPlayer->GetPositionX(), TargetPlayer->GetPositionY(), TargetPlayer->GetPositionZ() + 0.25f, player->GetOrientation(), TELE_TO_GM_MODE, TargetPlayer))
        {
            BG->WatchArenaList.insert(player);
            player->InArenaWatchSolo = true;
            player->RemovePet(nullptr, PET_SAVE_NOT_IN_SLOT, true);
            player->SetPhaseMask(TargetPlayer->GetPhaseMask() | 1, false);
        }
        CloseGossipMenuFor(player);
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player || !creature)
            return true;

        if (!sArenaSoloMgr->GetBoolean(11))
        {
            player->SendSysMsg(sHelper->GetGts(6691));
            return true;
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6692), 0, 0);

        if (player->InBattlegroundQueueForBattlegroundQueueType(BattlegroundQueueTypeId(11)))
        {
            AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6693), 0, GOSSIP_3);
        }

        uint32 ArenaTeamId = player->GetArenaTeamId(ArenaTeam::GetSlotByType(CUSTOM_ARENA_TEAM_1v1));
        if (!ArenaTeamId)
        {
            if (sArenaSoloMgr->GetUInt(24) > 0)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6694), 0, GOSSIP_1, sHelper->GetNeedGts(sArenaSoloMgr->GetUInt(24)), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6695), 0, GOSSIP_1);
        }
        else
        {
            if (!player->InBattlegroundQueueForBattlegroundQueueType(BattlegroundQueueTypeId(11)))
            {
                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6696), 0, GOSSIP_2);
                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6697), 0, GOSSIP_5, sHelper->GetGts(6698), 0, false);
            }

            AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6699), 0, GOSSIP_4);
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6700), 0, GOSSIP_666);

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        if (!player || !creature)
            return true;

        ClearGossipMenuFor(player);

        if (!Sender && !Action)
        {
            OnGossipHello(player, creature);
            return true;
        }

        if (Action == GOSSIP_1 || Action == GOSSIP_2 || Action == GOSSIP_666 || Action == GOSSIP_888)
        {
            if (sArenaSoloMgr->GetUInt(12) > 0 && !sGameEventMgr->IsActiveEvent(sArenaSoloMgr->GetUInt(12)))
            {
                player->SendSysMsg(sHelper->GetGts(6701));
                return true;
            }
        }

        switch (Action)
        {
        case GOSSIP_1:
        {
            if (sArenaSoloMgr->GetUInt(13) > 0 && player->getLevel() < sArenaSoloMgr->GetUInt(13))
            {
                player->SendSysMsg(sHelper->GetGts(6702), sArenaSoloMgr->GetUInt(13));
                OnGossipHello(player, creature);
                return true;
            }

            if (!CreateArenateam(player))
            {
                OnGossipHello(player, creature);
                return true;
            }
        }
        break;

        case GOSSIP_2:
        {
            if (!JoinQueueArena(player, true))
            {

            }

            CloseGossipMenuFor(player);
            return true;
        }
        break;

        case GOSSIP_3:
        {
            if (!player->InBattlegroundQueueForBattlegroundQueueType(BattlegroundQueueTypeId(11)))
                return true;

            WorldPacket Packet;
            Packet << (uint8)CUSTOM_ARENA_TEAM_1v1 << (uint8)0x0 << (uint32)BATTLEGROUND_AA << (uint16)0x0 << (uint8)0x0;
            player->GetSession()->HandleBattleFieldPortOpcode(Packet);
            OnGossipHello(player, creature);
            return true;
        }
        break;

        case GOSSIP_4:
        {
            ArenaTeam* ATeam = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(ArenaTeam::GetSlotByType(CUSTOM_ARENA_TEAM_1v1)));
            if (ATeam)
            {
                std::stringstream Oss;
                Oss << "--------------------------------------------------";
                Oss << "\n当前竞技场积分: " << ATeam->GetStats().Rating;
                Oss << "\n竞技场机制级别: " << ATeam->GetStats().Rank << " (按匹配策略级别匹配比赛)";
                Oss << "\n赛季总计场次: " << ATeam->GetStats().SeasonGames;
                Oss << "\n赛季胜利场次: " << ATeam->GetStats().SeasonWins;
                Oss << "\n星期总计场次: " << ATeam->GetStats().WeekGames;
                Oss << "\n星期胜利场次: " << ATeam->GetStats().WeekWins;
                Oss << "\n--------------------------------------------------";
                player->SendSysMsg(Oss.str().c_str());
            }
        }
        break;

        case GOSSIP_5:
        {
            WorldPacket Packet;
            Packet << player->GetArenaTeamId(ArenaTeam::GetSlotByType(CUSTOM_ARENA_TEAM_1v1));
            player->GetSession()->HandleArenaTeamLeaveOpcode(Packet);
            player->SendSysMsg(sHelper->GetGts(6703));
            OnGossipHello(player, creature);
            return true;
        }
        break;
        case GOSSIP_666:
        {
            OnShowSoloInList(player, creature);
            return true;
        }
        case GOSSIP_888:
        {
            if (!sArenaSoloMgr->GetBoolean(14))
            {
                player->SendSysMsg(sHelper->GetGts(6704));
                OnGossipHello(player, creature);
                return true;
            }
            OnStartWatchBG(player, creature, Sender, Action);
            return true;
        }
        break;
        default:
            OnGossipHello(player, creature);
            return true;
            break;
        }

        OnGossipHello(player, creature);
        return true;
    }

private:
    bool JoinQueueArena(Player* player, bool IsRated)
    {
        if (!sArenaSoloMgr->GetBoolean(11))
        {
            player->SendSysMsg(sHelper->GetGts(6705));
            return false;
        }

        if (sArenaSoloMgr->GetUInt(12) > 0 && !sGameEventMgr->IsActiveEvent(sArenaSoloMgr->GetUInt(12)))
        {
            player->SendSysMsg(sHelper->GetGts(6706));
            return false;
        }

        if (sArenaSoloMgr->GetUInt(13) > 0 && player->getLevel() < sArenaSoloMgr->GetUInt(13))
        {
            player->SendSysMsg(sHelper->GetGts(6707), sArenaSoloMgr->GetUInt(13));
            return false;
        }

        if (!sArenaSoloMgr->ArenaSoloCheckTalents(player))
        {
            return false;
        }

        uint32 Now = player->GetTempAtUInt32(AT4003);
        uint32 CheckCount = sArenaSoloMgr->GetUInt(25);
        if (CheckCount > 0 && Now >= CheckCount)
        {
            player->SendSysMsg(sHelper->GetGts(6708), CheckCount);
            return false;
        }

        uint8 Arenaslot = ArenaTeam::GetSlotByType(CUSTOM_ARENA_TEAM_1v1);
        uint32 ArenaRating = 0;
        uint32 MatchmakerRating = 0;
        uint32 AteamId = 0;

        if (player->InBattleground())
        {
            player->SendSysMsg(sHelper->GetGts(6709));
            return false;
        }

        Battleground* BG = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);
        if (!BG)
        {
            player->SendSysMsg(sHelper->GetGts(6710));
            return false;
        }

        BattlegroundTypeId BGTypeId = BG->GetBgTypeID();
        BattlegroundQueueTypeId BGQueueTypeId = BattlegroundMgr::BGQueueTypeId(BGTypeId, CUSTOM_ARENA_TEAM_1v1);

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, BATTLEGROUND_AA, nullptr))
        {
            ChatHandler(player->GetSession()).PSendSysMessage(LANG_ARENA_DISABLED);
            return false;
        }

        PvPDifficultyEntry const* BracketEntry = GetBattlegroundBracketByLevel(BG->GetMapId(), player->getLevel());
        if (!BracketEntry)
        {
            player->SendSysMsg(sHelper->GetGts(6711));
            return false;
        }

        if (player->GetBattlegroundQueueIndex(BGQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        {
            player->SendSysMsg(sHelper->GetGts(6712));
            return false;
        }

        if (!player->HasFreeBattlegroundQueueId())
        {
            player->SendSysMsg(sHelper->GetGts(6713));
            return false;
        }

        if (IsRated)
        {
            AteamId = player->GetArenaTeamId(Arenaslot);
            ArenaTeam* ATeam = sArenaTeamMgr->GetArenaTeamById(AteamId);
            if (!ATeam)
            {
                player->GetSession()->SendNotInArenaTeamPacket(CUSTOM_ARENA_TEAM_1v1);
                player->SendSysMsg(sHelper->GetGts(6714));
                return false;
            }

            ArenaRating = ATeam->GetRating();
            MatchmakerRating = ArenaRating;

            if (ArenaRating <= 0)
                ArenaRating = 1;
        }

        BattlegroundQueue& BGQueue = sBattlegroundMgr->GetBattlegroundQueue(BGQueueTypeId);
        BG->SetRated(IsRated);
        BG->SetMaxPlayersPerTeam(1);
        GroupQueueInfo* GInfo = BGQueue.AddGroup(player, nullptr, BATTLEGROUND_AA, BracketEntry, CUSTOM_ARENA_TEAM_1v1, IsRated, false, ArenaRating, MatchmakerRating, AteamId);
        uint32 AvgTime = BGQueue.GetAverageQueueWaitTime(GInfo);
        uint32 QueueSlot = player->AddBattlegroundQueueId(BGQueueTypeId);
        WorldPacket Packet;
        sBattlegroundMgr->BuildBattlegroundStatusPacket(&Packet, BG, QueueSlot, STATUS_WAIT_QUEUE, AvgTime, 0, CUSTOM_ARENA_TEAM_1v1, TEAM_NEUTRAL, IsRated);
        player->GetSession()->SendPacket(&Packet);
        sBattlegroundMgr->ScheduleQueueUpdate(MatchmakerRating, CUSTOM_ARENA_TEAM_1v1, BGQueueTypeId, BATTLEGROUND_AA, BracketEntry->GetBracketId());
        return true;
    }

    bool CreateArenateam(Player* player)
    {
        if (!sArenaSoloMgr->GetBoolean(11))
        {
            player->SendSysMsg(sHelper->GetGts(6715));
            return false;
        }

        uint8 Slot = ArenaTeam::GetSlotByType(CUSTOM_ARENA_TEAM_1v1);
        if (Slot == 0)
        {
            player->SendSysMsg(sHelper->GetGts(6716));
            return false;
        }

        if (player->GetArenaTeamId(Slot))
        {
            player->SendSysMsg(sHelper->GetGts(6717));
            return false;
        }

        if (sArenaSoloMgr->GetUInt(24) > 0 && !sHelper->CheckNeed(player, sArenaSoloMgr->GetUInt(24), "1V1竞技场创建队伍消耗"))
            return false;

        int N = 1;
        std::stringstream TeamName;
        TeamName << player->GetName();
        do
        {
            if (sArenaTeamMgr->GetArenaTeamByName(TeamName.str()) != nullptr)
            {
                TeamName.str(std::string());
                TeamName << player->GetName() << (N++);
            }
            else
                break;

        } while (N < 100);

        ArenaTeam* ATeam = new ArenaTeam();
        if (!ATeam->Create(player->GetGUID(), CUSTOM_ARENA_TEAM_1v1, TeamName.str(), 4283124816, 45, 4294242303, 5, 4294705149))
        {
            delete ATeam;
            player->SendSysMsg(sHelper->GetGts(6718));
            return false;
        }

        player->SendSysMsg(sHelper->GetGts(6719), TeamName.str().c_str());
        sArenaTeamMgr->AddArenaTeam(ATeam);
        return true;
    }
};

class ConfigLoadSoloArena : public WorldScript
{
public:
    ConfigLoadSoloArena() : WorldScript("ConfigLoadSoloArena") {}

    void OnAfterConfigLoad(bool Reload) override
    {
        if (!Reload)
        {
            ArenaTeam::ArenaSlotByType.insert({ 1, 3 });
            ArenaTeam::ArenaReqPlayersForType.insert({ 1, 2 });
            BattlegroundMgr::queueToBg.insert({ 11,   BATTLEGROUND_AA });
            BattlegroundMgr::QueueToArenaType.emplace(11, ArenaType(CUSTOM_ARENA_TEAM_1v1));
            BattlegroundMgr::ArenaTypeToQueue.emplace(1, BattlegroundQueueTypeId(11));
        }
    }
};

class TeamSoloArena : public ArenaTeamScript
{
public:
    TeamSoloArena() : ArenaTeamScript("TeamSoloArena") {}

    void OnGetSlotByType(const uint32 Typ, uint8& Slot) override
    {
        if (Typ == CUSTOM_ARENA_TEAM_1v1)
            Slot = 3;
    }

    void OnTypeIDToQueueID(const BattlegroundTypeId, const uint8 ArenaType, uint32& BGQueueTypeId) override
    {
        if (ArenaType == CUSTOM_ARENA_TEAM_1v1)
            BGQueueTypeId = BattlegroundQueueTypeId(11);
    }

    void OnQueueIdToArenaType(const BattlegroundQueueTypeId BGQueueTypeId, uint8& ArenaType) override
    {
        if (BGQueueTypeId == BattlegroundQueueTypeId(11))
            ArenaType = CUSTOM_ARENA_TEAM_1v1;
    }

    void OnGetArenaPoints(ArenaTeam* ATeam, float& Points) override
    {
        if (ATeam->GetType() == CUSTOM_ARENA_TEAM_1v1)
            Points *= 0.64f;
    }


    void OnSetArenaMaxPlayersPerTeam(const uint8 Typ, uint32& MaxPlayersPerTeam) override
    {
        if (Typ == CUSTOM_ARENA_TEAM_1v1)
            MaxPlayersPerTeam = 1;
    }
};

class BGMessageArenaSoloQueue : public BGScript
{
public:
    BGMessageArenaSoloQueue() : BGScript("BGMessageArenaQueue") { }

    bool OnBeforeSendJoinMessageArenaQueue(BattlegroundQueue* Queue, Player* Leader, GroupQueueInfo* GInfo, PvPDifficultyEntry const* BracketEntry, bool IsRated) override
    {
        if (!IsRated)
            return true;

        if (!sArenaSoloMgr->GetBoolean(11))
            return true;

        Battleground* BG = sBattlegroundMgr->GetBattlegroundTemplate(GInfo->BgTypeId);
        if (!BG)
            return true;

        if (!BG->isArena())
            return true;

        ArenaTeam* AtTeam = sArenaTeamMgr->GetArenaTeamById(GInfo->ArenaTeamId);
        if (!AtTeam || !GInfo->IsRated)
            return true;

        if (!sArenaSoloMgr->GetBoolean(15))
            return false;

        sHelper->SendSerMessage(sHelper->GetGts(6720), sHelper->GetNameLinkColor(Leader).c_str(), AtTeam->GetStats().Rating, AtTeam->GetStats().Rank);
        return false;
    }

    void OnBattlegroundEndReward(Battleground* BG, Player* player, TeamId winnerTeamId) override
    {
        if (!sArenaSoloMgr->GetBoolean(11))
            return;
        if (!player)
            return;
        if (!BG)
            return;
        if (!BG->isArena())
            return;
        if (BG->GetArenaType() != CUSTOM_ARENA_TEAM_1v1)
            return;

        if (player->GetBgTeamId() == winnerTeamId)
            sHelper->DoTheReward(player, sArenaSoloMgr->GetUInt(18), "1V1竞技场胜利奖励");
        else
            sHelper->DoTheReward(player, sArenaSoloMgr->GetUInt(19), "1V1竞技场失败奖励");
    }

    void OnBattlegroundStart(Battleground* BG) override
    {
        if (!sArenaSoloMgr->GetBoolean(11))
            return;
        if (!BG)
            return;
        if (!BG->isArena())
            return;
        if (BG->GetArenaType() != CUSTOM_ARENA_TEAM_1v1)
            return;

        auto Itr = ArenaSoloInFtStore.find(BG);
        if (Itr == ArenaSoloInFtStore.end())
        {
            std::vector<ObjectGuid> DoList;
            for (auto const& [NowGuid, player] : BG->GetPlayers())
            {
                if (player)
                    DoList.push_back(NowGuid);
            }

            if (DoList.size() == 2)
            {
                SoloGuidTemp Temp;
                Temp.AGuid = DoList[0];
                Temp.BGuid = DoList[1];
                ArenaSoloInFtStore.insert(std::make_pair(BG, Temp));
            }
            DoList.clear();
        }
    }

    void OnBattlegroundEnd(Battleground* BG, TeamId winnerTeam) override
    {
        if (!BG)
            return;
        if (!BG->isArena())
            return;
        if (BG->GetArenaType() != CUSTOM_ARENA_TEAM_1v1)
            return;

        auto Itr = ArenaSoloInFtStore.find(BG);
        if (Itr != ArenaSoloInFtStore.end())
            ArenaSoloInFtStore.erase(Itr);

        for (auto Itr = BG->WatchArenaList.begin(); Itr != BG->WatchArenaList.end(); Itr++)
        {
            Player* player = *Itr;
            if (player)
            {
                if (player->InArenaWatchSolo)
                {
                    player->InArenaWatchSolo = false;
                    if (!player->IsVisible())
                        player->SetVisible(true);
                    player->SetPhaseMask(player->EntryNowPhase, true);
                }
                player->SetCanTeleport(true);
                player->TeleportToEntryPoint();
            }
        }
        BG->WatchArenaList.clear();
    }

    void OnBattlegroundAddPlayer(Battleground* BG, Player* player) override
    {
        if (!BG)
            return;
        if (!BG->isArena())
            return;
        if (BG->GetArenaType() != CUSTOM_ARENA_TEAM_1v1)
            return;

        uint32 Now = player->GetTempAtUInt32(AT4003);
        player->UpdateTempAt(AT4003, Now + 1);
    }
};

class ArenaSoloPlayerScript : public PlayerScript
{
public:
    ArenaSoloPlayerScript() : PlayerScript("ArenaPlayerScript") {}

    bool NotSetArenaTeamInfoField(Player* player, uint8 slot, ArenaTeamInfoType type, uint32 value) override
    {
        if (slot == 3)
            return false;

        return true;
    }

    void GetCustomGetArenaTeamId(Player const* player, uint8 Slot, uint32& ArenaTeamId) const override
    {
        if (Slot == 3)
        {
            ArenaTeam* ATeam = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), CUSTOM_ARENA_TEAM_1v1);
            if (ATeam)
                ArenaTeamId = ATeam->GetId();
            else
                ArenaTeamId = 0;
        }
    }

    void GetCustomArenaPersonalRating(const Player* player, uint8 Slot, uint32& Rating) const override
    {
        if (Slot == 3)
        {
            ArenaTeam* ATeam = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), CUSTOM_ARENA_TEAM_1v1);
            if (ATeam)
                Rating = ATeam->GetRating();
        }
    }

    void OnGetMaxPersonalArenaRatingRequirement(const Player* player, uint32 Minslot, uint32& MaxArenaRating) const override
    {
        if (Minslot < 3)
        {
            ArenaTeam* ATeam = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), CUSTOM_ARENA_TEAM_1v1);
            if (ATeam)
                MaxArenaRating = std::max(ATeam->GetRating(), MaxArenaRating);
        }
    }

    bool CanBattleFieldPort(Player* player, uint8 arenaType, BattlegroundTypeId BGTypeID, uint8 action) override
    {
        uint32 Now = player->GetTempAtUInt32(AT4003);
        uint32 CheckCount = sArenaSoloMgr->GetUInt(25);
        if (arenaType == CUSTOM_ARENA_TEAM_1v1 && CheckCount > 0 && Now >= CheckCount)
        {
            return false;
        }

        return true;
    }
};

class OnSoloArenaGameEventScript : public GameEventScript
{
public:
    OnSoloArenaGameEventScript() : GameEventScript("OnSoloArenaGameEventScript") { }

    void OnStart(uint16 DoEventId) override
    {
        if (DoEventId == sArenaSoloMgr->GetUInt(12))
        {
            sArenaSoloMgr->AutoInitPlayerInSoloCount();
        }
    }

    void OnStop(uint16 DoEventId) override
    {
        if (DoEventId == sArenaSoloMgr->GetUInt(12))
        {
            sArenaSoloMgr->AutoInitPlayerInSoloCount();
        }
    }
};


void AddSC_SoloArenaScripts()
{
    new ConfigLoadSoloArena();
    new NpcSoloArena();
    new TeamSoloArena();
    new BGMessageArenaSoloQueue();
    new ArenaSoloPlayerScript();
    new OnSoloArenaGameEventScript();
}
