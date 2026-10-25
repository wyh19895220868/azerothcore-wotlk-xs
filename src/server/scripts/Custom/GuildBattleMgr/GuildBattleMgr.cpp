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
#include "CharacterCache.h"
#include "DisableMgr.h"
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "PoolMgr.h"
#include "Config.h"
#include "WorldSession.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"
#include "Pet.h"
#include <CharacterCache.h>
#include <algorithm>
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include "GuildBattleMgr.h"
#include <Custom/EventMgr/EventMgr.h>
#include <Maps/MapMgr.h>
#include <sstream>
#pragma execution_character_set("utf-8")

GuildBattleMap GuildBattleStore;

namespace
{
AsInt64 GetAsConfigValue(std::string const& value)
{
    return value.empty() ? AsInt64Zero() : AsInt64FromString(value);
}

std::string CleanGuildBattleUIField(std::string value)
{
    std::replace_if(value.begin(), value.end(), [](char ch)
    {
        return ch == '|' || ch == '\t' || ch == '\r' || ch == '\n';
    }, ' ');
    return value;
}
}

void GuildBattleMgr::ResetGuildBattleData()
{
    DefendId = 0;
    AttackId = 0;
    InStart = false;
    GuildBattleAtData.clear();
    DefendResCount = 0;
    AttackResCount = 0;
    AtGroupMap.clear();
    WinCount = 0;
    NowWinId = 0;
    GuildBattlePlayers.clear();
}

void GuildBattleMgr::LoadGuildBattle()
{
    GuildBattleStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 序号, 配置值 FROM __公会战配置表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            GuildBattleStore.insert(std::make_pair(Fields[0].Get<uint32>(), Fields[1].Get<std::string>()));

        } while (Result->NextRow());
    }

    AtGuildLoadFromDB();
}

std::string GuildBattleMgr::GetStrConfig(uint32 Index)
{
    auto Itr = GuildBattleStore.find(Index);
    return Itr != GuildBattleStore.end() ? Itr->second : "";
}

uint32 GuildBattleMgr::GetUInt(uint32 Index)
{
    auto Itr = GuildBattleStore.find(Index);
    if (Itr != GuildBattleStore.end())
        return uint32(atoi(Itr->second.c_str()));

    return 0;
}

float GuildBattleMgr::GetFloat(uint32 Index)
{
    auto Itr = GuildBattleStore.find(Index);
    if (Itr != GuildBattleStore.end())
        return float(atof(Itr->second.c_str()));

    return 0.0f;
}

void GuildBattleMgr::AtGuildLoadFromDB()
{
    WinCount = 0;
    QueryResult Result = CharacterDatabase.Query("SELECT 公会编号, 当前胜利次数 FROM _公会战存档 LIMIT 1;");
    if (Result)
    {
        DefendId = Result->Fetch()[0].Get<uint32>();
        WinCount = Result->Fetch()[1].Get<uint32>();
        AttackId = sGuildMgr->GetAttackGuildId(DefendId);
    }
}

void GuildBattleMgr::InitGuildBattleDataWorldState(bool Start)
{
    if (Start)
        AtGuildLoadFromDB();

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second)
            continue;

        Player* player = Itr->second->GetPlayer();
        if (!player || !player->IsInWorld())
            continue;

        RemoveAurasStop(player);

        Guild* DoGuild = player->GetGuild();
        if (!DoGuild)
            continue;

        if (Start)
        {
            if (DefendId)
            {
                if (DoGuild->GetId() == DefendId)
                    player->SendSysMsg(sHelper->GetGts(9001));
                else
                    player->SendSysMsg(sHelper->GetGts(9002));
            }
            else
            {
                player->SendSysMsg(sHelper->GetGts(9003));
            }
        }
        else
        {
            player->UninviteFromGroup();
            if (player->GetGroup())
                player->RemoveFromGroup();
        }
    }

    Guild* WinGuild = sGuildMgr->GetGuildById(DefendId);
    if (!sGuildBattleMgr->NowWinId && !Start && (AttackResCount < sGuildBattleMgr->GetUInt(1007)) && WinGuild && AttackId && DefendId)
    {
        sHelper->SendMailToGuid(WinGuild->GetLeaderGUID(), "公会争霸战", "会长奖励", sGuildBattleMgr->GetUInt(1004), 1);
        sHelper->SendSerMessage(sHelper->GetGts(9004));
        CharacterDatabase.DirectExecute("DELETE FROM _公会战存档 LIMIT 1;");
        WinCount++;
        CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_GUILD_BAT);
        Utmt->SetData(0, DefendId);
        Utmt->SetData(1, WinCount);
        CharacterDatabase.Execute(Utmt);

        sGuildBattleMgr->RewPlayerEnd(DefendId);
    }
}

GameObject* GuildBattleMgr::GetGuildBattleGO()
{
    uint32 GOGuidLow = GetUInt(1012);
    if (!GOGuidLow)
        return nullptr;

    auto Itr = PosMap.find(GetUInt(1013));
    if (Itr == PosMap.end())
        return nullptr;

    Map* TarMap = sMapMgr->FindMap(Itr->second.MapId, 0);
    if (!TarMap)
        return nullptr;

    GameObjectData const* GOData = sObjectMgr->GetGOData(GOGuidLow);
    if (!GOData)
        return nullptr;

    auto GoBounds = TarMap->GetGameObjectBySpawnIdStore().equal_range(GOGuidLow);
    if (GoBounds.first != GoBounds.second)
    {
        GameObject* GO = GoBounds.first->second;
        return GO;
    }

    return nullptr;
}

uint32 GuildBattleMgr::InRangeGuildPlayerResUpdate(Map* map)
{
    if (!map || !AttackId || !DefendId)
        return 0;

    auto Itr = PosMap.find(GetUInt(1013));
    if (Itr == PosMap.end() || Itr->second.MapId != map->GetId())
        return 0;

    float Radius = GetFloat(1014);
    if (Radius <= 0.0f)
        return 0;

    bool HasAttackPlayer = false;
    bool HasDefendPlayer = false;
    for (auto const& PlayerRef : map->GetPlayers())
    {
        Player* player = PlayerRef.GetSource();
        if (!player || !player->IsAlive() || !player->IsInWorld())
            continue;

        if (!player->IsWithinDist3d(Itr->second.X, Itr->second.Y, Itr->second.Z, Radius))
            continue;

        uint32 GuildId = player->GetGuildId();
        if (GuildId == AttackId)
            HasAttackPlayer = true;
        else if (GuildId == DefendId)
            HasDefendPlayer = true;

        if (HasAttackPlayer && HasDefendPlayer)
            return 0;
    }

    return HasAttackPlayer ? AttackId : (HasDefendPlayer ? DefendId : 0);
}

void GuildBattleMgr::UpdateWorldState(uint32 /*Index*/, uint32 /*ResCount*/)
{
    SendGuildBattleUIToAll();
}

void GuildBattleMgr::SendGuildBattleUI(Player* player)
{
    if (!player || !InStart || !AttackId || !DefendId)
    {
        if (player)
            sDoAddon->SendPacketToClient(player, "GuildBattleUI", "CLEAR");
        return;
    }

    uint32 playerGuildId = player->GetGuildId();
    uint32 role = playerGuildId == AttackId ? 1 : (playerGuildId == DefendId ? 2 : 0);
    if (!role)
    {
        sDoAddon->SendPacketToClient(player, "GuildBattleUI", "CLEAR");
        return;
    }

    Guild* attackGuild = sGuildMgr->GetGuildById(AttackId);
    Guild* defendGuild = sGuildMgr->GetGuildById(DefendId);
    if (!attackGuild || !defendGuild)
    {
        sDoAddon->SendPacketToClient(player, "GuildBattleUI", "CLEAR");
        return;
    }

    uint32 attackTeam = sCharacterCache->GetCharacterTeamByGuid(attackGuild->GetLeaderGUID());
    uint32 defendTeam = sCharacterCache->GetCharacterTeamByGuid(defendGuild->GetLeaderGUID());
    std::ostringstream payload;
    payload << "DATA|"
        << AttackResCount << '|'
        << DefendResCount << '|'
        << GetUInt(1007) << '|'
        << GetUInt(1008) << '|'
        << attackTeam << '|'
        << defendTeam << '|'
        << role << '|'
        << CleanGuildBattleUIField(attackGuild->GetName()) << '|'
        << CleanGuildBattleUIField(defendGuild->GetName());
    sDoAddon->SendPacketToClient(player, "GuildBattleUI", payload.str());
}

void GuildBattleMgr::SendGuildBattleUIToAll()
{
    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second)
            continue;

        Player* player = Itr->second->GetPlayer();
        if (!player || !player->IsInWorld())
            continue;

        if (!InStart || player->GetGuildId() == AttackId || player->GetGuildId() == DefendId)
            SendGuildBattleUI(player);
    }
}

void GuildBattleMgr::GuildUpdateEventDamage(Player* Att, Player* Tar, AsInt64 const& Damage)
{
    GameObject* GO = GetGuildBattleGO();
    if (!GO)
        return;

    if (!Att || !Tar || Damage <= 0 || Att->GetGUID() == Tar->GetGUID() || Tar->GetTypeId() != TYPEID_PLAYER || Att->GetZoneId() != GO->GetZoneId())
        return;

    GuildBattleAtData[Att->GetGUID()].Damage += Damage;
}

void GuildBattleMgr::GuildUpdateEventHeal(Player* Cast, Player* Tar, AsInt64 const& Heal)
{
    GameObject* GO = GetGuildBattleGO();
    if (!GO)
        return;

    if (!Cast || !Tar || Heal <= 0 || Cast->GetGUID() == Tar->GetGUID() || Tar->GetTypeId() != TYPEID_PLAYER || Cast->GetZoneId() != GO->GetZoneId())
        return;

    GuildBattleAtData[Cast->GetGUID()].Heal+= Heal;
}

void GuildBattleMgr::OnDoAuraPlayerGuild(Player* player, uint32 ZoneId)
{
    Guild* DoGuild = player->GetGuild();
    if (!DoGuild)
        return;

    GameObject* GO = sGuildBattleMgr->GetGuildBattleGO();
    if (!GO)
        return;

    if (GO->GetZoneId() != ZoneId)
        return;

    if (DefendId && AttackId)
    {
        std::string StrAuras = sGuildBattleMgr->GetStrConfig(DoGuild->GetId() == DefendId ? 1009 : 1010);
        Tokenizer AuraData(StrAuras, '#');
        for (auto const& Temp : AuraData)
        {
            uint32 AuraId = uint32(atoi(Temp));
            if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(AuraId))
            {
                if (!player->HasAura(AuraId))
                    player->AddAura(AuraId, player);
            }
        }

        if (WinCount && DoGuild->GetId() == AttackId)
        {
            uint32 KAuraId = sGuildBattleMgr->GetUInt(1011);
            if (!player->HasAura(KAuraId))
            {
                if (Aura* DoAura = player->AddAura(KAuraId, player))
                    DoAura->SetStackAmount(WinCount);
            }
        }
    }
}

void GuildBattleMgr::RemoveAurasStop(Player* player)
{
    std::string StrAuras1 = sGuildBattleMgr->GetStrConfig(1009);
    Tokenizer AuraData1(StrAuras1, '#');
    for (auto const& Temp : AuraData1)
    {
        uint32 AuraId = uint32(atoi(Temp));
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(AuraId))
        {
            if (player->HasAura(AuraId))
                player->RemoveAurasDueToSpell(AuraId);
        }
    }

    std::string StrAuras2 = sGuildBattleMgr->GetStrConfig(1010);
    Tokenizer AuraData2(StrAuras2, '#');
    for (auto const& Temp : AuraData2)
    {
        uint32 AuraId = uint32(atoi(Temp));
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(AuraId))
        {
            if (player->HasAura(AuraId))
                player->RemoveAurasDueToSpell(AuraId);
        }
    }

    uint32 KAuraId = sGuildBattleMgr->GetUInt(1011);
    if (player->HasAura(KAuraId))
        player->RemoveAurasDueToSpell(KAuraId);
}

void GuildBattleMgr::RewPlayerEnd(uint32 WinGuildId)
{
    AsInt64 NeedHeal = GetAsConfigValue(GetStrConfig(1017));
    AsInt64 NeedDmg = GetAsConfigValue(GetStrConfig(1018));

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second)
            continue;
        Player* player = Itr->second->GetPlayer();
        if (!player || !player->IsInWorld())
            continue;
        if (player->GetGuildId() == WinGuildId)
        {
            if (GuildBattleAtData.find(player->GetGUID()) != GuildBattleAtData.end() && (GuildBattleAtData[player->GetGUID()].Damage >= NeedDmg || GuildBattleAtData[player->GetGUID()].Heal >= NeedHeal))
                sHelper->DoTheReward(player, GetUInt(1005), "公会争霸战胜利的奖励");
        }
        else
        {
            if (GuildBattleAtData.find(player->GetGUID()) != GuildBattleAtData.end() && (GuildBattleAtData[player->GetGUID()].Damage >= NeedDmg || GuildBattleAtData[player->GetGUID()].Heal >= NeedHeal))
                sHelper->DoTheReward(player, GetUInt(1006), "公会争霸战失败的奖励");
        }
    }
}

class NpcGuildBattle : public CreatureScript
{
public:
    NpcGuildBattle() : CreatureScript("NpcGuildBattle") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(9005), 0, 1);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(9006), 0, 2);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        if (!Sender && !Action)
        {
            OnGossipHello(player, creature);
            return true;
        }

        uint32 EventId = sGuildBattleMgr->GetUInt(1001);
        if (!sGameEventMgr->IsActiveEvent(EventId))
        {
            player->SendSysMsg("该活动尚未激活");
            CloseGossipMenuFor(player);
            return true;
        }

        if (Action == 1)
        {
            if (!sHelper->CheckNeed(player, sGuildBattleMgr->GetUInt(1002), "公会战申请消耗"))
            {
                CloseGossipMenuFor(player);
                return true;
            }

            if (sGuildBattleMgr->InStart)
            {
                CloseGossipMenuFor(player);
                player->SendSysMsg(sHelper->GetGts(9007));
                return true;
            }

            Guild* DoGuild = player->GetGuild();
            if (!DoGuild)
            {
                player->SendSysMsg(sHelper->GetGts(9008));
                CloseGossipMenuFor(player);
                return true;
            }

            if (DoGuild->GetId() == sGuildBattleMgr->DefendId)
            {
                player->SendSysMsg(sHelper->GetGts(9009));
                CloseGossipMenuFor(player);
                return true;
            }

            if (!sGuildBattleMgr->DefendId && !sGuildBattleMgr->AttackId)
            {
                player->SendSysMsg(sHelper->GetGts(9010));
                sGuildBattleMgr->DefendId = DoGuild->GetId();
                sGuildBattleMgr->DefendResCount = 0;

                CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_GUILD_BAT);
                Utmt->SetData(0, DoGuild->GetId());
                Utmt->SetData(1, 0);
                CharacterDatabase.Execute(Utmt);

                CloseGossipMenuFor(player);
                return true;
            }

            CloseGossipMenuFor(player);
            sGuildBattleMgr->AttackId = DoGuild->GetId();
            sGuildBattleMgr->InStart = true;
            sGuildBattleMgr->AttackResCount = 0;

            SessionMap const& Sessions = sWorld->GetAllSessions();
            for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
            {
                if (!Itr->second)
                    continue;

                Player* Gplayer = Itr->second->GetPlayer();
                if (!Gplayer || !Gplayer->IsInWorld())
                    continue;

                sGuildBattleMgr->OnDoAuraPlayerGuild(Gplayer, Gplayer->GetZoneId());

                Guild* AtDoGuild = Gplayer->GetGuild();
                if (!AtDoGuild)
                    continue;

                if (AtDoGuild->GetId() == sGuildBattleMgr->DefendId)
                    sHelper->PopToPlayerDialog(Gplayer, Acore::StringFormat(sGuildBattleMgr->GetStrConfig(1021), "防守方"), 0, sGuildBattleMgr->GetUInt(1016), GuildBattle);
                else
                    sHelper->PopToPlayerDialog(Gplayer, Acore::StringFormat(sGuildBattleMgr->GetStrConfig(1021), "进攻方"), 0, sGuildBattleMgr->GetUInt(1015), GuildBattle);
            }

            sGuildBattleMgr->SendGuildBattleUIToAll();

            return true;
        }
        else if(Action == 2)
        {
            Guild* DoGuild = player->GetGuild();
            if (!DoGuild)
            {
                CloseGossipMenuFor(player);
                return true;
            }

            if (!sGuildBattleMgr->DefendId && !sGuildBattleMgr->AttackId)
            {
                player->SendSysMsg(sHelper->GetGts(9011));
                CloseGossipMenuFor(player);
                return true;
            }

            if (!sGuildBattleMgr->InStart)
            {
                CloseGossipMenuFor(player);
                return true;
            }

            CloseGossipMenuFor(player);

            player->UninviteFromGroup();
            if (player->GetGroup())
                player->RemoveFromGroup();

            bool GExist = false;
            for (auto Itr = sGuildBattleMgr->AtGroupMap.begin(); Itr != sGuildBattleMgr->AtGroupMap.end(); Itr++)
            {
                if (Group* Gp = sGroupMgr->GetGroupByGUID(Itr->first))
                {
                    if (!Gp->IsFull() && Itr->second == DoGuild->GetId())
                    {
                        Gp->AddMember(player);
                        GExist = true;
                        break;
                    }
                }
            }

            if (!GExist)
            {
                Group* Gp = new Group();
                Gp->Create(player);
                Gp->ConvertToRaid();
                sGroupMgr->AddGroup(Gp);
                sGuildBattleMgr->AtGroupMap[Gp->GetGUID().ToGuidLow()] = DoGuild->GetId();
            }

            player->TeleToPos(sGuildBattleMgr->GetUInt(DoGuild->GetId() == sGuildBattleMgr->DefendId ? 1016 : 1015));
            return true;
        }

        return true;
    }
};

class GuildBattleEventScript : public GameEventScript
{
public:
    GuildBattleEventScript() : GameEventScript("GuildBattleEventScript") { }

    void OnStart(uint16 DoEventId) override
    {
        if (DoEventId > MINEVENTSO)
            return;

        uint32 EventId = sGuildBattleMgr->GetUInt(1001);
        if (!EventId || DoEventId != EventId)
            return;

        sGuildBattleMgr->ResetGuildBattleData();
        sGuildBattleMgr->InitGuildBattleDataWorldState(true);
    }

    void OnStop(uint16 DoEventId) override
    {
        if (DoEventId > MINEVENTSO)
            return;

        uint32 EventId = sGuildBattleMgr->GetUInt(1001);
        if (!EventId || DoEventId != EventId)
            return;

        sGuildBattleMgr->InitGuildBattleDataWorldState(false);
        sGuildBattleMgr->ResetGuildBattleData();
        sGuildBattleMgr->SendGuildBattleUIToAll();
    }
};

class GuildBattleAllMapScript : public AllMapScript
{
public:
    GuildBattleAllMapScript() : AllMapScript("GuildBattleAllMapScript") { DoTime = 0; }

    void OnMapUpdate(Map* NowMap, uint32 Diff) override
    {
        if (!NowMap)
            return;

        uint32 EventId = sGuildBattleMgr->GetUInt(1001);
        if (!EventId)
            return;

        auto Itr = PosMap.find(sGuildBattleMgr->GetUInt(1013));
        if (Itr == PosMap.end())
            return;

        if (Itr->second.MapId != NowMap->GetId())
            return;

        if (!sGuildBattleMgr->InStart)
            return;

        DoTime += Diff;
        if (DoTime >= IN_MILLISECONDS)
        {
            DoTime = 0;

            uint32 GuildId = sGuildBattleMgr->InRangeGuildPlayerResUpdate(NowMap);
            if (!GuildId)
                return;
            Guild* DoGuild = sGuildMgr->GetGuildById(GuildId);
            if (!DoGuild)
                return;

            uint32 WinGuildId = 0;
            if (DoGuild->GetId() == sGuildBattleMgr->DefendId)
            {
                sGuildBattleMgr->DefendResCount += sGuildBattleMgr->GetUInt(1008);
                sGuildBattleMgr->UpdateWorldState(sGuildBattleMgr->GetUInt(1020), sGuildBattleMgr->DefendResCount);
                if (sGuildBattleMgr->DefendResCount >= sGuildBattleMgr->GetUInt(1007))
                    WinGuildId = DoGuild->GetId();
            }
            else
            {
                sGuildBattleMgr->AttackResCount += sGuildBattleMgr->GetUInt(1008);
                sGuildBattleMgr->UpdateWorldState(sGuildBattleMgr->GetUInt(1019), sGuildBattleMgr->AttackResCount);
                if (sGuildBattleMgr->AttackResCount >= sGuildBattleMgr->GetUInt(1007))
                    WinGuildId = DoGuild->GetId();
            }

            if (WinGuildId)
            {
                Guild* WinGuild = sGuildMgr->GetGuildById(WinGuildId);
                if (!WinGuild)
                    return;

                sGuildBattleMgr->NowWinId = WinGuild->GetId();
                if (Player* player = ObjectAccessor::FindPlayer(WinGuild->GetLeaderGUID()))
                    sHelper->SendMailToGuid(WinGuild->GetLeaderGUID(), "公会争霸战", "会长奖励", sGuildBattleMgr->GetUInt(1004), 1);

                sHelper->SendSerMessage(sHelper->GetGts(9012), WinGuild->GetName().c_str());

                CharacterDatabase.DirectExecute("DELETE FROM _公会战存档 LIMIT 1;");

                uint32 WinCount = WinGuild->GetId() == sGuildBattleMgr->DefendId ? (sGuildBattleMgr->WinCount + 1) : 1;
                CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_GUILD_BAT);
                Utmt->SetData(0, GuildId);
                Utmt->SetData(1, WinCount);
                CharacterDatabase.Execute(Utmt);

                sGuildBattleMgr->RewPlayerEnd(GuildId);
                sGameEventMgr->StopEvent(sGuildBattleMgr->GetUInt(1001), false, true);

                WinGuildId = 0;
            }
        }
    }

private:
    uint32 DoTime;
};

class GuildBattleEventPlayerScript : PlayerScript
{
public:
    GuildBattleEventPlayerScript() : PlayerScript("GuildBattleEventPlayerScript") {}

    void OnLogin(Player* player) override
    {
        sGuildBattleMgr->RemoveAurasStop(player);
        sGuildBattleMgr->SendGuildBattleUI(player);
    }

    void OnUpdateZone(Player* player, uint32 newZone, uint32 newArea) override
    {
        sGuildBattleMgr->OnDoAuraPlayerGuild(player, newZone);
    }
};

void AddSC_GuildBattleScript()
{
    new NpcGuildBattle();
    new GuildBattleEventScript();
    new GuildBattleAllMapScript();
    new GuildBattleEventPlayerScript();
}

/***************************************************************************************************************************************************************/

AsPlayerBattleMap AsPlayerBattleStore;

void AsPlayerBattleMgr::LoadAsPlayerBattle()
{
    AsPlayerBattleStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 序号, 配置值 FROM __全服对战配置表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            AsPlayerBattleStore.insert(std::make_pair(Fields[0].Get<uint32>(), Fields[1].Get<std::string>()));

        } while (Result->NextRow());
    }
}

std::string AsPlayerBattleMgr::GetStrConfig(uint32 Index)
{
    auto Itr = AsPlayerBattleStore.find(Index);
    return Itr != AsPlayerBattleStore.end() ? Itr->second : "";
}

uint32 AsPlayerBattleMgr::GetUInt(uint32 Index)
{
    auto Itr = AsPlayerBattleStore.find(Index);
    if (Itr != AsPlayerBattleStore.end())
        return uint32(atoi(Itr->second.c_str()));

    return 0;
}

float AsPlayerBattleMgr::GetFloat(uint32 Index)
{
    auto Itr = AsPlayerBattleStore.find(Index);
    if (Itr != AsPlayerBattleStore.end())
        return float(atof(Itr->second.c_str()));

    return 0.0f;
}

GameObject* AsPlayerBattleMgr::GetAsPlayerBattleGO()
{
    uint32 GOGuidLow = GetUInt(5011);
    if (!GOGuidLow)
        return nullptr;

    uint32 PosId = GetUInt(5003);
    if (!PosId)
        PosId = GetUInt(5004);

    auto Itr = PosMap.find(PosId);
    if (Itr == PosMap.end())
        return nullptr;

    Map* TarMap = sMapMgr->FindMap(Itr->second.MapId, 0);
    if (!TarMap)
        return nullptr;

    GameObjectData const* GOData = sObjectMgr->GetGOData(GOGuidLow);
    if (!GOData)
        return nullptr;

    auto GoBounds = TarMap->GetGameObjectBySpawnIdStore().equal_range(GOGuidLow);
    if (GoBounds.first != GoBounds.second)
    {
        GameObject* GO = GoBounds.first->second;
        return GO;
    }

    return nullptr;
}

void AsPlayerBattleMgr::InitFacGroupBattle()
{
    Started = true;

    NowWinFac = GROUP_TEAM_NULL;
    AResCount = 0;
    HResCount = 0;

    AsPlayersGroupMap.clear();
    AsPlayersFacMap.clear();
}

void AsPlayerBattleMgr::EndFacGroupBattle()
{
    Started = false;

    SendAsPlayerBattleUIToAll();

    for (auto Itr = AsPlayersGroupMap.begin(); Itr != AsPlayersGroupMap.end(); Itr++)
        if (Group* Gp = sGroupMgr->GetGroupByGUID(Itr->first))
            Gp->Disband();

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second)
            continue;

        Player* player = Itr->second->GetPlayer();
        if (!player || !player->IsInWorld())
            continue;

        AddOrRemoveAuras(player, GROUP_TEAM_NULL, false);

        ObjectGuid Guid = player->GetGUID();
        if (AsPlayersFacMap.find(Guid) != AsPlayersFacMap.end())
            ResetTempFaction(player);
    }

    if (NowWinFac == GROUP_TEAM_NULL)
    {
        if (AResCount == HResCount)
        {
            uint32 NotWin = GetUInt(5055);
            if (NotWin == 0)
            {
                sHelper->SendSerMessage(sHelper->GetGts(10109));
            }
            else if (NotWin == 1)
            {
                RewAsPlayerEnd(GROUP_TEAM_A);
                sHelper->SendSerMessage(sHelper->GetGts(10108), GetFacStr(GROUP_TEAM_A).c_str());
            }
            else if(NotWin == 2)
            {
                RewAsPlayerEnd(GROUP_TEAM_H);
                sHelper->SendSerMessage(sHelper->GetGts(10108), GetFacStr(GROUP_TEAM_H).c_str());
            }
        }
        else if (AResCount > HResCount)
        {
            RewAsPlayerEnd(GROUP_TEAM_A);
            sHelper->SendSerMessage(sHelper->GetGts(10108), GetFacStr(GROUP_TEAM_A).c_str());
        }
        else if (AResCount < HResCount)
        {
            RewAsPlayerEnd(GROUP_TEAM_H);
            sHelper->SendSerMessage(sHelper->GetGts(10108), GetFacStr(GROUP_TEAM_H).c_str());
        }
    }

    NowWinFac = GROUP_TEAM_NULL;
    AResCount = 0;
    HResCount = 0;

    AsPlayersGroupMap.clear();
    AsPlayersFacMap.clear();
}

std::string AsPlayerBattleMgr::GetFacStr(FacGroupType Fac)
{
    std::string StrName = (Fac == GROUP_TEAM_A) ? "联盟" : "部落";
    return StrName;
}

bool AsPlayerBattleMgr::CheckPlayerInBattle(Player* player)
{
    auto Itr = AsPlayersFacMap.find(player->GetGUID());
    return Itr != AsPlayersFacMap.end();
}

void AsPlayerBattleMgr::ReStartInGossipBattle(Player* player)
{
    auto Iter = AsPlayersFacMap.find(player->GetGUID());
    if (Iter != AsPlayersFacMap.end())
    {
        AllocationGroupAndTele(player, Iter->second.Fac);
    }
    else
    {
        FacGroupType FGt = AutoFacGroup(player);
        AllocationGroupAndTele(player, FGt);
    }
}

FacGroupType AsPlayerBattleMgr::AutoFacGroup(Player* /*player*/)
{
    uint32 ACount = GetFactionCounts(GROUP_TEAM_A);
    uint32 HCount = GetFactionCounts(GROUP_TEAM_H);
    if (ACount == HCount)
        return urand(0, 1) ? GROUP_TEAM_A : GROUP_TEAM_H;

    return ACount < HCount ? GROUP_TEAM_A : GROUP_TEAM_H;
}

void AsPlayerBattleMgr::SetTempFaction(Player* player, FacGroupType Fac)
{
    auto Itr = AsPlayersFacMap.find(player->GetGUID());
    if (Itr == AsPlayersFacMap.end())
        return;
    player->SetFaction(uint32(Fac));
}

void AsPlayerBattleMgr::ResetTempFaction(Player* player)
{
    auto Itr = AsPlayersFacMap.find(player->GetGUID());
    if (Itr == AsPlayersFacMap.end())
        return;

    player->SetFactionForRace(player->GetRace(true));
}

void AsPlayerBattleMgr::ResetLoginTempFaction(Player* player)
{
    auto Itr = AsPlayersFacMap.find(player->GetGUID());
    if (Itr == AsPlayersFacMap.end())
        return;

    player->SetFaction(uint32(Itr->second.Fac));
}

uint32 AsPlayerBattleMgr::GetFactionCounts(FacGroupType Fac)
{
    uint32 Count = 0;
    for (auto Itr = AsPlayersFacMap.begin(); Itr != AsPlayersFacMap.end(); Itr++)
        if (Fac == Itr->second.Fac)
            Count++;

    return Count;
}

void AsPlayerBattleMgr::AllocationGroupAndTele(Player* player, FacGroupType Fac)
{
    auto Itr = AsPlayersFacMap.find(player->GetGUID());
    if (Itr == AsPlayersFacMap.end())
    {
        AsPlayerBattleEventData Data;
        Data.Fac = Fac;
        Data.Damage = 0;
        Data.Heal = 0;
        AsPlayersFacMap.insert(std::make_pair(player->GetGUID(), Data));
    }

    AddOrRemoveAuras(player, Fac, true);
    player->SendSysMsg(sAsPlayerBattleMgr->GetStrConfig(5007).c_str(), sAsPlayerBattleMgr->GetFacStr(Fac).c_str());

    bool GExist = false;
    for (auto Itr = AsPlayersGroupMap.begin(); Itr != AsPlayersGroupMap.end(); Itr++)
    {
        if (Group* Gp = sGroupMgr->GetGroupByGUID(Itr->first))
        {
            if (!Gp->IsFull() && Itr->second == Fac)
            {
                Gp->AddMember(player);
                GExist = true;
                break;
            }
        }
    }

    if (!GExist)
    {
        Group* Gp = new Group();
        Gp->Create(player);
        Gp->ConvertToRaid();
        sGroupMgr->AddGroup(Gp);
        AsPlayersGroupMap[Gp->GetGUID().ToGuidLow()] = Fac;
    }

    SetTempFaction(player, Fac);
    uint32 PosId = GetTelePosId(Fac);
    player->TeleToPos(PosId);
    SendAsPlayerBattleUI(player);
}

uint32 AsPlayerBattleMgr::GetTelePosId(FacGroupType Fac)
{
    if (Fac == GROUP_TEAM_NULL)
        return 0;

    uint32 Index = (Fac == GROUP_TEAM_A) ? 5003 : 5004;
    return sAsPlayerBattleMgr->GetUInt(Index);
}

void AsPlayerBattleMgr::CheckHandlerPlayers(uint32 ZoneId)
{
    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second)
            continue;

        Player* player = Itr->second->GetPlayer();
        if (!player || !player->IsInWorld() || player->GetZoneId() != ZoneId)
            continue;

        ObjectGuid Guid = player->GetGUID();
        if (AsPlayersFacMap.find(Guid) == AsPlayersFacMap.end())
            player->SendSysMsg(sHelper->GetGts(10081));
    }
}

FacGroupType AsPlayerBattleMgr::InRangeFacPlayerResUpdate()
{
    GameObject* GO = GetAsPlayerBattleGO();
    if (!GO)
        return GROUP_TEAM_NULL;

    float Radius = GetFloat(5012);
    std::list<Player*> RPlayers;
    Acore::AnyPlayerInObjectRangeCheck Checker(GO, Radius);
    Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> Searcher(GO, RPlayers, Checker);
    Cell::VisitWorldObjects(GO, Searcher, Radius);

    if (RPlayers.empty())
        return GROUP_TEAM_NULL;

    std::vector<FacGroupType> AtFGts;
    for (auto Itr = RPlayers.begin(); Itr != RPlayers.end(); ++Itr)
    {
        Player* player = *Itr;
        if (player->IsAlive())
        {
            auto Iter = AsPlayersFacMap.find(player->GetGUID());
            if (Iter != AsPlayersFacMap.end())
                AtFGts.push_back(Iter->second.Fac);
        }
    }

    if (AtFGts.empty())
        return GROUP_TEAM_NULL;

    FacGroupType FGtId = AtFGts[0];
    bool DoOne = std::all_of(AtFGts.begin(), AtFGts.end(), [FGtId](FacGroupType Check) { return Check == FGtId; });
    if (!DoOne)
        return GROUP_TEAM_NULL;

    return FGtId;
}

void AsPlayerBattleMgr::UpdateAsWorldState(uint32 /*Index*/, uint32 /*ResCount*/)
{
    SendAsPlayerBattleUIToAll();
}

void AsPlayerBattleMgr::SendAsPlayerBattleUI(Player* player)
{
    if (!player || !Started)
    {
        if (player)
            sDoAddon->SendPacketToClient(player, "AsPlayerBattleUI", "CLEAR");
        return;
    }

    auto Itr = AsPlayersFacMap.find(player->GetGUID());
    if (Itr == AsPlayersFacMap.end())
    {
        sDoAddon->SendPacketToClient(player, "AsPlayerBattleUI", "CLEAR");
        return;
    }

    std::ostringstream Payload;
    Payload << "DATA|"
        << AResCount << '|'
        << HResCount << '|'
        << GetUInt(5008) << '|'
        << GetUInt(5009) << '|'
        << uint32(Itr->second.Fac);
    sDoAddon->SendPacketToClient(player, "AsPlayerBattleUI", Payload.str());
}

void AsPlayerBattleMgr::SendAsPlayerBattleUIToAll()
{
    for (auto Itr = AsPlayersFacMap.begin(); Itr != AsPlayersFacMap.end(); ++Itr)
        if (Player* player = ObjectAccessor::FindPlayer(Itr->first))
            SendAsPlayerBattleUI(player);
}

void AsPlayerBattleMgr::RewAsPlayerEnd(FacGroupType WinFac)
{
    AsInt64 NeedHeal = GetAsConfigValue(GetStrConfig(5041));
    AsInt64 NeedDmg = GetAsConfigValue(GetStrConfig(5042));
    for (auto Itr = AsPlayersFacMap.begin(); Itr != AsPlayersFacMap.end(); ++Itr)
    {
        if (Itr->second.Heal >= NeedHeal || Itr->second.Damage >= NeedDmg)
        {
            if (Itr->second.Fac == WinFac)
            {
                if (Player* player = ObjectAccessor::FindPlayer(Itr->first))
                    sHelper->DoTheReward(player, GetUInt(5043), "全服争霸战胜利的奖励");
            }
            else
            {
                if (Player* player = ObjectAccessor::FindPlayer(Itr->first))
                    sHelper->DoTheReward(player, GetUInt(5044), "全服争霸战失败的奖励");
            }
        }
    }
}

void AsPlayerBattleMgr::AsPlayerUpdateEventDamage(Player* Att, Player* Tar, AsInt64 const& Damage)
{
    GameObject* GO = GetAsPlayerBattleGO();
    if (!GO)
        return;

    if (!Att || !Tar || Damage <= 0 || Att->GetGUID() == Tar->GetGUID() || Tar->GetTypeId() != TYPEID_PLAYER || Att->GetZoneId() != GO->GetZoneId())
        return;

    auto Iter = AsPlayersFacMap.find(Att->GetGUID());
    if (Iter == AsPlayersFacMap.end())
        return;

    Iter->second.Damage += Damage;
}

void AsPlayerBattleMgr::AsPlayerUpdateEventHeal(Player* Cast, Player* Tar, AsInt64 const& Heal)
{
    GameObject* GO = GetAsPlayerBattleGO();
    if (!GO)
        return;

    if (!Cast || !Tar || Heal <= 0 || Cast->GetGUID() == Tar->GetGUID() || Tar->GetTypeId() != TYPEID_PLAYER || Cast->GetZoneId() != GO->GetZoneId())
        return;

    auto Iter = AsPlayersFacMap.find(Cast->GetGUID());
    if (Iter == AsPlayersFacMap.end())
        return;

    Iter->second.Heal += Heal;
}

void AsPlayerBattleMgr::LoginAddAuras(Player* player)
{
    auto Itr = AsPlayersFacMap.find(player->GetGUID());
    if (Itr != AsPlayersFacMap.end())
    {
        AddOrRemoveAuras(player, Itr->second.Fac, true);
    }
}

void AsPlayerBattleMgr::AddOrRemoveAuras(Player* player, FacGroupType Fac, bool Add)
{
    if (Fac == GROUP_TEAM_NULL)
    {
        std::string StrAuras1 = sAsPlayerBattleMgr->GetStrConfig(5005);
        Tokenizer AuraData1(StrAuras1, '#');
        for (auto const& Temp : AuraData1)
        {
            uint32 AuraId = uint32(atoi(Temp));
            if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(AuraId))
            {
                if (player->HasAura(AuraId))
                    player->RemoveAurasDueToSpell(AuraId);
            }
        }

        std::string StrAuras2 = sAsPlayerBattleMgr->GetStrConfig(5006);
        Tokenizer AuraData2(StrAuras2, '#');
        for (auto const& Temp : AuraData2)
        {
            uint32 AuraId = uint32(atoi(Temp));
            if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(AuraId))
            {
                if (player->HasAura(AuraId))
                    player->RemoveAurasDueToSpell(AuraId);
            }
        }
        return;
    }

    std::string StrAuras = sAsPlayerBattleMgr->GetStrConfig(Fac == GROUP_TEAM_A ? 5005 : 5006);
    Tokenizer AuraData(StrAuras, '#');
    for (auto const& Temp : AuraData)
    {
        uint32 AuraId = uint32(atoi(Temp));
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(AuraId))
        {
            if (Add)
            {
                if (!player->HasAura(AuraId))
                    player->AddAura(AuraId, player);
            }
            else
            {
                if (player->HasAura(AuraId))
                    player->RemoveAurasDueToSpell(AuraId);
            }
        }
    }
}

void AsPlayerBattleMgr::RemoveAsPlayersGroupMap(uint32 GpGuidLow)
{
    auto Itr = AsPlayersGroupMap.find(GpGuidLow);
    if (Itr != AsPlayersGroupMap.end())
    {
        AsPlayersGroupMap.erase(GpGuidLow);
    }
}

class NpcAsPlayerBattle : public CreatureScript
{
public:
    NpcAsPlayerBattle() : CreatureScript("NpcAsPlayerBattle") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(10091), 0, 1);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(10091), 0, 2);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        if (!sGameEventMgr->IsActiveEvent(sAsPlayerBattleMgr->GetUInt(5001)))
        {
            CloseGossipMenuFor(player);
            player->SendSysMsg(sHelper->GetGts(10101));
            return true;
        }

        CloseGossipMenuFor(player);

        player->UninviteFromGroup();
        if (player->GetGroup())
            player->RemoveFromGroup();

        sAsPlayerBattleMgr->ReStartInGossipBattle(player);

        return true;
    }
};

class AsPlayerBattleEventScript : public GameEventScript
{
public:
    AsPlayerBattleEventScript() : GameEventScript("AsPlayerBattleEventScript") { }

    void OnStart(uint16 DoEventId) override
    {
        if (DoEventId > MINEVENTSO)
            return;

        uint32 EventId = sAsPlayerBattleMgr->GetUInt(5001);
        if (!EventId || EventId != DoEventId)
            return;

        sAsPlayerBattleMgr->InitFacGroupBattle();

        SessionMap const& Sessions = sWorld->GetAllSessions();
        for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
        {
            if (!Itr->second)
                continue;

            Player* player = Itr->second->GetPlayer();
            if (!player || !player->IsInWorld())
                continue;

            sHelper->PopToPlayerDialog(player, sAsPlayerBattleMgr->GetStrConfig(5002), 0, 0, AsPlrBgEvent);
        }
    }

    void OnStop(uint16 DoEventId) override
    {
        if (DoEventId > MINEVENTSO)
            return;

        uint32 EventId = sAsPlayerBattleMgr->GetUInt(5001);
        if (!EventId || EventId != DoEventId)
            return;

        sAsPlayerBattleMgr->EndFacGroupBattle();
    }
};

class AsPlayerBattleAllMapScript : public AllMapScript
{
public:
    AsPlayerBattleAllMapScript() : AllMapScript("AsPlayerBattleAllMapScript") { DoCheckTime = 0; DoResTime = 0; }

    void ResetTime()
    {
        DoCheckTime = 0;
        DoResTime = 0;
    }

    void OnMapUpdate(Map* NowMap, uint32 Diff) override
    {
        if (!NowMap)
        {
            ResetTime();
            return;
        }

        auto Itr = PosMap.find(sAsPlayerBattleMgr->GetUInt(5003));
        if (Itr == PosMap.end())
            return;

        if (Itr->second.MapId != NowMap->GetId())
            return;

        GameObject* GO = sAsPlayerBattleMgr->GetAsPlayerBattleGO();
        if (!GO)
        {
            ResetTime();
            return;
        }

        if (!sAsPlayerBattleMgr->Started)
        {
            ResetTime();
            return;
        }

        DoCheckTime += Diff;
        DoResTime += Diff;
        if (DoCheckTime >= 3 * IN_MILLISECONDS)
        {
            DoCheckTime = 0;
            sAsPlayerBattleMgr->CheckHandlerPlayers(sAsPlayerBattleMgr->GetUInt(5010));
        }

        if (DoResTime >= IN_MILLISECONDS)
        {
            DoResTime = 0;
            FacGroupType Fac = sAsPlayerBattleMgr->InRangeFacPlayerResUpdate();
            if (Fac == GROUP_TEAM_NULL)
                return;

            uint32 AddRes = sAsPlayerBattleMgr->GetUInt(5009);
            if (Fac == GROUP_TEAM_A)
            {
                sAsPlayerBattleMgr->AResCount += AddRes;
                sAsPlayerBattleMgr->UpdateAsWorldState(sAsPlayerBattleMgr->GetUInt(5031), sAsPlayerBattleMgr->AResCount);
                if (sAsPlayerBattleMgr->AResCount >= sAsPlayerBattleMgr->GetUInt(5008))
                    sAsPlayerBattleMgr->NowWinFac = GROUP_TEAM_A;
            }
            else
            {
                sAsPlayerBattleMgr->HResCount += AddRes;
                sAsPlayerBattleMgr->UpdateAsWorldState(sAsPlayerBattleMgr->GetUInt(5032), sAsPlayerBattleMgr->HResCount);
                if (sAsPlayerBattleMgr->HResCount >= sAsPlayerBattleMgr->GetUInt(5008))
                    sAsPlayerBattleMgr->NowWinFac = GROUP_TEAM_H;
            }

            if (sAsPlayerBattleMgr->NowWinFac != GROUP_TEAM_NULL)
            {
                sAsPlayerBattleMgr->RewAsPlayerEnd(sAsPlayerBattleMgr->NowWinFac);
                sGameEventMgr->StopEvent(sAsPlayerBattleMgr->GetUInt(5001), false, true);
                sHelper->SendSerMessage(sHelper->GetGts(10108), sAsPlayerBattleMgr->GetFacStr(sAsPlayerBattleMgr->NowWinFac).c_str());
            }
        }
    }

private:
    uint32 DoCheckTime;
    uint32 DoResTime;
};

class AsPlayerBattleEventPlayerScript : PlayerScript
{
public:
    AsPlayerBattleEventPlayerScript() : PlayerScript("AsPlayerBattleEventPlayerScript") {}

    void OnUpdateZone(Player* player, uint32 newZone, uint32 newArea) override
    {
        if (!sAsPlayerBattleMgr->Started)
        {
            sAsPlayerBattleMgr->ResetTempFaction(player);
            sAsPlayerBattleMgr->AddOrRemoveAuras(player, GROUP_TEAM_NULL, false);
            return;
        }

        uint32 ZoneId = player->GetZoneId();
        if (ZoneId == sAsPlayerBattleMgr->GetUInt(5010))
        {
            sAsPlayerBattleMgr->LoginAddAuras(player);
            sAsPlayerBattleMgr->ResetLoginTempFaction(player);
        }
        else
        {
            sAsPlayerBattleMgr->AddOrRemoveAuras(player, GROUP_TEAM_NULL, false);
            sAsPlayerBattleMgr->ResetTempFaction(player);
        }
    }
};

class AsPlayerBattleOnGroupDisbandScript : public GroupScript
{
public:
    AsPlayerBattleOnGroupDisbandScript() : GroupScript("AsPlayerBattleOnGroupDisbandScript") { }

    void OnDisband(Group* Gp) override
    {
        sAsPlayerBattleMgr->RemoveAsPlayersGroupMap(Gp->GetGUID().ToGuidLow());
    }
};

void AddSC_AsPlayerBattleScript()
{
    new NpcAsPlayerBattle();
    new AsPlayerBattleEventScript();
    new AsPlayerBattleAllMapScript();
    new AsPlayerBattleEventPlayerScript();
    new AsPlayerBattleOnGroupDisbandScript();
}
