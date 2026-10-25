#include "Custom/_Helper/XsPlayerbotCompat.h"
#include "CharPvpTop.h"
#include "CharacterCache.h"
#include "GameEventMgr.h"
#include <MapMgr.h>
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "World.h"
#include "WorldSession.h"
#include <Custom/_Helper/Helper.h>
#include "GameTime.h"
#include <AI/ScriptedAI/ScriptedGossip.h>
#pragma execution_character_set("utf-8")

CharPvpTop::CharPvpTop()
{
    TopEvent = false;
    PvpCount = 0;
    TeleStarted = false;
    SoloNextTime = 0;
}

CharPvpTop::~CharPvpTop() { }

void CharPvpTop::LoadTopSys()
{
    SoloPvpConfigStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 序号, 配置值 FROM _比武大会配置;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Id = Fields[0].Get<uint32>();
            std::string Value = Fields[1].Get<std::string>();
            SoloPvpConfigStore.insert(std::make_pair(Id, Value));
        } while (Result->NextRow());
    }

    VCTopSys.clear();
    AsSoloAuras.clear();

    Result = WorldDatabase.Query("SELECT 名次, 礼包物品, 附加光环 FROM _比武大会奖励 ORDER BY 名次;");
    if (Result)
    {
        do
        {
            Field * fields = Result->Fetch();
            SoloTopSys St;

            St.Num = fields[0].Get<uint32>();
            St.ItemId = fields[1].Get<uint32>();
            St.AuraId = fields[2].Get<uint32>();
            AsSoloAuras.push_back(St.AuraId);
            VCTopSys.insert(std::make_pair(St.Num, St));

        } while (Result->NextRow());
    }
}

uint32 CharPvpTop::GetRewItemId(uint32 Num)
{
    const SoloTopSys* TopSys = FindTopSys(Num);
    return TopSys ? TopSys->ItemId : 0;
}

uint32 CharPvpTop::GetRewAuraId(uint32 Num)
{
    const SoloTopSys* TopSys = FindTopSys(Num);
    return TopSys ? TopSys->ItemId : 0;
}

void CharPvpTop::SendAsUpdateWorldState(uint32 AtValue)
{
    uint32 Index = GetSoloPvpConfig(106);
    if (!Index)
        return;

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        Itr->second->GetPlayer()->SendUpdateWorldState(Index, AtValue);
    }
}

uint32 CharPvpTop::GetSoloPvpConfig(uint32 Index)
{
    auto Itr = SoloPvpConfigStore.find(Index);
    if (Itr != SoloPvpConfigStore.end())
        return uint32(atoi(Itr->second.c_str()));

    return 0;
}


void CharPvpTop::RollTopTeamNum()
{
    if (!TopEvent)
        return;

    UpdateCharTopData();

    uint32 TopMax = GetTopMax();
    if (!PvpCount)
    {
        if (TopMax < GetSoloPvpConfig(105))
        {
            sHelper->SendSerMessage(sHelper->GetGts(9601), GetSoloPvpConfig(105));
            sGameEventMgr->StopEvent(GetSoloPvpConfig(100), false, true);
            return;
        }
    }
    else
    {
        if (TopMax < GetSoloPvpConfig(105))
        {
            if (TopMax == 1)
            {
                ObjectGuid AGuld = SoloTopMaxVect.at(0);
                Player* Winner = ObjectAccessor::FindPlayer(AGuld);
                if (!Winner)
                {
                    sGameEventMgr->StopEvent(GetSoloPvpConfig(100), false, true);
                    return;
                }

                Winner->TopMc = TopMax;
                Winner->LastTopMc = Winner->TopMc;
                SoloPlayerTop[Winner->LastTopMc] = Winner->GetGUID();

                CharacterDatabasePreparedStatement* UStmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_SOLO_PVP_DATA);
                UStmt->SetData(0, TopMax);
                UStmt->SetData(1, Winner->LastTopMc);
                UStmt->SetData(2, Winner->GetGUIDLow());
                CharacterDatabase.Execute(UStmt);

                sHelper->SendSerMessage(sHelper->GetGts(9605), sHelper->GetNameLinkColor(Winner).c_str());

                if (!SoloPlayerTop.empty())
                {
                    std::ostringstream Oss;
                    Oss << "=====比武大会排名=====\n";
                    for (auto Itr = SoloPlayerTop.begin(); Itr != SoloPlayerTop.end(); Itr++)
                    {
                        if (!Itr->first)
                            continue;

                        CharacterCacheEntry const* CacheEntry = sCharacterCache->GetCharacterCacheByGuid(Itr->second);
                        if (CacheEntry)
                        {
                            Oss << "第" << Itr->first << "名: " << CacheEntry->Name << " 奖励: " << sHelper->GetItemLink(GetRewItemId(Itr->first)) << "\n";
                        }
                    }
                    sHelper->SendSerMessage(Oss.str().c_str());
                }

                sGameEventMgr->StopEvent(GetSoloPvpConfig(100), false, true);
                return;
            }
            else if(TopMax == 0)
            {
                if (!SoloPlayerTop.empty())
                {
                    std::ostringstream Oss;
                    Oss << "=====比武大会排名=====\n";
                    for (auto Itr = SoloPlayerTop.begin(); Itr != SoloPlayerTop.end(); Itr++)
                    {
                        if (!Itr->first)
                            continue;

                        CharacterCacheEntry const* CacheEntry = sCharacterCache->GetCharacterCacheByGuid(Itr->second);
                        if (CacheEntry)
                        {
                            Oss << "第" << Itr->first << "名: " << CacheEntry->Name << " 奖励: " << sHelper->GetItemLink(GetRewItemId(Itr->first)) << "\n";
                        }
                    }
                    sHelper->SendSerMessage(Oss.str().c_str());
                }
                sGameEventMgr->StopEvent(GetSoloPvpConfig(100), false, true);
                return;
            }
        }
    }

    PvpCount++;
    uint32 MaxCount = SoloTopDataVect.size() / 2;
    for (uint32 Num = 1; Num < MaxCount + 1; ++Num)
    {
        ObjectGuid AGuid, BGuid;
        uint32 R1 = urand(0, SoloTopDataVect.size() - 1);
        uint32 M1 = 0;
        for (auto Itr1 = SoloTopDataVect.begin(); Itr1 != SoloTopDataVect.end(); Itr1++)
        {
            if (M1 == R1)
            {
                AGuid = *Itr1;
                SoloTopDataVect.erase(Itr1);
                break;
            }
            M1++;
        }

        uint32 R2 = urand(0, SoloTopDataVect.size() - 1);
        uint32 M2 = 0;
        for (auto Itr2 = SoloTopDataVect.begin(); Itr2 != SoloTopDataVect.end(); Itr2++)
        {
            if (M2 == R2)
            {
                BGuid = *Itr2;
                SoloTopDataVect.erase(Itr2);
                break;
            }
            M2++;
        }

        AddTopTeam(Num, AGuid, BGuid);
        SengPVPgo(Num);
    }
}

void CharPvpTop::UpdateCharTopData()
{
    SoloTopDataVect.clear();
    SoloTopMaxVect.clear();

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        if (Itr->second->GetPlayer()->GetAreaId() != GetSoloPvpConfig(101) && Itr->second->GetPlayer()->TopMc == CtTopMc)
        {
            //无名次
            Itr->second->GetPlayer()->TopMc = 0;
            Itr->second->GetPlayer()->LastTopMc = 0;
            continue;
        }

        //没有决斗出名次
        if (Itr->second->GetPlayer()->TopMc != CtTopMc)
            continue;

        SoloTopDataVect.push_back(Itr->second->GetPlayer()->GetGUID());
        SoloTopMaxVect.push_back(Itr->second->GetPlayer()->GetGUID());
    }
}

void CharPvpTop::SengPVPgo(uint32 Num)
{
    Player* Aplayer = ObjectAccessor::FindPlayer(VCtopCustom[Num].AGuld);
    Player* Bplayer = ObjectAccessor::FindPlayer(VCtopCustom[Num].BGuld);

    if (!Aplayer || !Bplayer)
        return;

    if (Aplayer->duel || Bplayer->duel)
        return;

    Aplayer->TopTeamNum = Bplayer->TopTeamNum = Num;

    ObjectGuid GoGuid = ObjectGuid(HighGuid::GameObject, GetSoloPvpConfig(102));
    if (GoGuid == ObjectGuid::Empty)
        return;

    sHelper->SendSerMessage(sHelper->GetGts(9602), PvpCount, sHelper->GetNameLinkColor(Aplayer).c_str(), sHelper->GetNameLinkColor(Bplayer).c_str());
    Aplayer->SendSysMsg(sHelper->GetGts(9607), sHelper->GetNameLinkColor(Bplayer).c_str());
    Bplayer->SendSysMsg(sHelper->GetGts(9607), sHelper->GetNameLinkColor(Aplayer).c_str());

    Aplayer->duel = std::make_unique<DuelInfo>(Bplayer, Aplayer, false);
    Bplayer->duel = std::make_unique<DuelInfo>(Aplayer, Bplayer, false);

    Aplayer->SetGuidValue(PLAYER_DUEL_ARBITER, GoGuid);
    Bplayer->SetGuidValue(PLAYER_DUEL_ARBITER, GoGuid);

    time_t Now = GameTime::GetGameTime().count();
    Aplayer->duel->StartTime = Now + BeginTime;
    Bplayer->duel->StartTime = Now + BeginTime;

    Aplayer->duel->State = DUEL_STATE_COUNTDOWN;
    Bplayer->duel->State = DUEL_STATE_COUNTDOWN;

    Aplayer->SetPvpTime = true;
    Bplayer->SetPvpTime = true;
    Aplayer->SendDuelCountdown(BeginTime * IN_MILLISECONDS);
    Bplayer->SendDuelCountdown(BeginTime * IN_MILLISECONDS);
}

void CharPvpTop::CheckSoloDuel()
{
    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second)
            continue;

        Player* player = Itr->second->GetPlayer();
        if (!player || !player->IsInWorld() || player->GetAreaId() != GetSoloPvpConfig(101))
            continue;

        if (player->duel && player->duel->Opponent && player->duel->Opponent->IsInWorld())
        {
            if (player->GetHealthPct() < player->duel->Opponent->GetHealthPct())
            {
                player->DuelComplete(DUEL_FLED);
            }
            else if (player->GetHealthPct() > player->duel->Opponent->GetHealthPct())
            {
                player->duel->Opponent->DuelComplete(DUEL_FLED);
            }
            else if (player->GetHealthPct() == player->duel->Opponent->GetHealthPct())
            {
                if (player->GetHealth() < player->duel->Opponent->GetHealth())
                {
                    player->DuelComplete(DUEL_FLED);
                }
                else if (player->GetHealth() > player->duel->Opponent->GetHealth())
                {
                    player->duel->Opponent->DuelComplete(DUEL_FLED);
                }
                else
                {
                    if (urand(1, 2) == 1)
                        player->DuelComplete(DUEL_FLED);
                    else
                        player->duel->Opponent->DuelComplete(DUEL_FLED);
                }
            }
        }
    }
}

void CharPvpTop::PVPupdate()
{
    if (CustomEventSetStore.find(GetSoloPvpConfig(100)) == CustomEventSetStore.end())
        return;

    if (!sCharPvpTop->TeleStarted)
    {
        bool IsActive = sGameEventMgr->IsActiveEvent(GetSoloPvpConfig(100));
        if (!IsActive)
        {
            uint32 NTime = sGameEventMgr->NextCheck(GetSoloPvpConfig(100));
            if (NTime <= 32)
            {
                sCharPvpTop->TeleStarted = true;
                SessionMap const& Sessions = sWorld->GetAllSessions();
                for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
                {
                    if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
                        continue;

                    Itr->second->GetPlayer()->SendSysMsg("[比武大会]: 30秒后开始 现在提前传送报名的玩家到指定位置 请保持在区域内");
                    if (Itr->second->GetPlayer()->GetAreaId() != GetSoloPvpConfig(101) && Itr->second->GetPlayer()->TopMc == CtTopMc)
                    {
                        Itr->second->GetPlayer()->TeleToPos(GetSoloPvpConfig(104));
                    }
                }
            }
        }
    }

    if (TopEvent)
    {
        uint32 NextTime = sCharPvpTop->GetSoloPvpConfig(103);
        SoloNextTime++;
        if (SoloNextTime >= NextTime)
        {
            SoloNextTime = 0;
            CheckSoloDuel();

            sCharPvpTop->RollTopTeamNum();
        }
        else
        {
            SendAsUpdateWorldState(NextTime - SoloNextTime);
        }
    }
}

void CharPvpTop::AddTopAuraToAllPlayer()
{
    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        SendAndClearTopAura(Itr->second->GetPlayer());
    }
}

void CharPvpTop::SendAndClearTopAura(Player * player)
{
    uint32 AuraId = 0;
    const SoloTopSys* TopSys = FindTopSys(player->LastTopMc);
    if (TopSys && TopSys->AuraId)
        AuraId = TopSys->AuraId;

    if (!AuraId)
    {
        for (auto Itr = AsSoloAuras.begin(); Itr != AsSoloAuras.end(); Itr++)
        {
            if (player->HasAura(*Itr))
                player->RemoveAurasDueToSpell(*Itr);
        }
    }
    else
    {
        for (auto Itr = AsSoloAuras.begin(); Itr != AsSoloAuras.end(); Itr++)
        {
            if (AuraId == *Itr && !player->HasAura(*Itr))
                player->AddAura(AuraId, player);
            else if (AuraId != *Itr && player->HasAura(*Itr))
                player->RemoveAurasDueToSpell(*Itr);
        }
    }
}


class NpcSoloPvpTopScript : public CreatureScript
{
public:
    NpcSoloPvpTopScript() : CreatureScript("NpcSoloPvpTopScript")
    {
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "上一界比武大会排行榜[仅显示前10]", 1, 4);

        if (player->TopMc == CtTopMc)
        {
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "你已报名", 1, 1);
        }
        else if (player->TopMc > 0 && player->TopMc < CtTopMc)
        {
            if (sGameEventMgr->IsActiveEvent(sCharPvpTop->GetSoloPvpConfig(100)))
            {
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "比武大会已经开始", 1, 1);
            }
            else
            {
                const SoloTopSys* TopSys = sCharPvpTop->FindTopSys(player->TopMc);
                if (TopSys && TopSys->ItemId)
                {
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, "领取比武大会奖励", 1, 2);
                }
                else
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, "比武大会报名", 1, 3);
            }

        }
        else if (!player->TopMc)
        {
            if (!sGameEventMgr->IsActiveEvent(sCharPvpTop->GetSoloPvpConfig(100)))
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "比武大会报名", 1, 3);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "比武大会已经开始", 1, 1);
        }


        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        if (action == 1)
            OnGossipHello(player, creature);
        else if (action == 2)
        {
            const SoloTopSys* TopSys = sCharPvpTop->FindTopSys(player->TopMc);
            if (TopSys && TopSys->ItemId)
                player->AddItem(TopSys->ItemId, 1, "比武大会奖励");

            player->TopMc = 0;

            CharacterDatabasePreparedStatement* UStmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_SOLO_PVP_DATA1);
            UStmt->SetData(0, player->TopMc);
            UStmt->SetData(1, player->GetGUIDLow());
            CharacterDatabase.Execute(UStmt);

            player->SendSysMsg(sHelper->GetGts(9603));
            OnGossipHello(player, creature);
            return false;
        }
        else if (action == 3)
        {
            player->TopMc = CtTopMc;

            CharacterDatabasePreparedStatement* UStmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_SOLO_PVP_DATA1);
            UStmt->SetData(0, player->TopMc);
            UStmt->SetData(1, player->GetGUIDLow());
            CharacterDatabase.Execute(UStmt);

            player->SendSysMsg(sHelper->GetGts(9604));
            OnGossipHello(player, creature);
            return false;
        }
        else if (action == 4)
        {
            ClearGossipMenuFor(player);

            if (!sCharPvpTop->SoloPlayerTop.empty())
            {
                for (auto Itr = sCharPvpTop->SoloPlayerTop.begin(); Itr != sCharPvpTop->SoloPlayerTop.end(); Itr++)
                {
                    if (!Itr->first || Itr->first > 10)
                        continue;

                    CharacterCacheEntry const* CacheEntry = sCharacterCache->GetCharacterCacheByGuid(Itr->second);
                    if (CacheEntry)
                    {
                        std::ostringstream Oss;
                        Oss << "第" << Itr->first << "名: " << CacheEntry->Name << " 奖励: " << sHelper->GetItemLink(sCharPvpTop->GetRewItemId(Itr->first));
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, Oss.str().c_str(), 1, 5);
                    }
                }
            }
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "暂无排行榜", 1, 5);

            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        }
        else if (action == 5)
        {
            OnGossipHello(player, creature);
            return false;
        }
        return true;
    }
};

class OnSoloPvpPlayerMod : public PlayerScript
{
public:
    OnSoloPvpPlayerMod() : PlayerScript("OnSoloPvpPlayerMod") {  }

    void OnLogin(Player* player) override
    {
        if (IsXsPlayerbot(player))
            return;

        uint32 GuidLow = player->GetGUIDLow();
        ObjectGuid Guid = player->GetGUID();

        CharacterDatabasePreparedStatement* CStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_SOLO_PVP_DATA);
        CStmt->SetData(0, GuidLow);
        PreparedQueryResult CResult = CharacterDatabase.Query(CStmt);
        if (CResult)
        {
            Field* Fields = CResult->Fetch();
            player->TopMc = Fields[0].Get<uint32>();
            player->LastTopMc = Fields[1].Get<uint32>();

            sCharPvpTop->SoloPlayerTop[player->LastTopMc] = Guid;
            if (player->TopMc == CtTopMc && sCharPvpTop->TopEvent)
                player->TopMc = 0;

            sCharPvpTop->SendAndClearTopAura(player);
        }
        else
        {
            CStmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_SOLO_PVP_DATA);
            CStmt->SetData(0, GuidLow);
            CharacterDatabase.Execute(CStmt);
        }
    }

    void OnUpdateArea(Player* player, uint32 oldArea, uint32 newArea) override
    {
        if (player->duel)
            if (oldArea == sCharPvpTop->GetSoloPvpConfig(101))
                player->DuelComplete(DUEL_FLED);
    }

    void OnDuelStart(Player* player1, Player* player2) override
    {
        if (sCharPvpTop->TopEvent && player1->GetAreaId() == sCharPvpTop->GetSoloPvpConfig(101) && player2->GetAreaId() == sCharPvpTop->GetSoloPvpConfig(101))
        {
            if (player1->SetPvpTime)
            {
                player1->SetPvpTime = false;
                player1->UpdateObjectVisibility();
                player2->UpdateObjectVisibility();
            }
        }
    }

    void OnCustomSoloDuelEnd(Player* Winner, Player* Loser) override
    {
        if (!sCharPvpTop->TopEvent)
            return;

        if (Winner && Loser && Winner->GetAreaId() == sCharPvpTop->GetSoloPvpConfig(101))
        {
            Winner->TopTeamNum = Loser->TopTeamNum = 0;
            Loser->UpdateObjectVisibility();
            Winner->UpdateObjectVisibility();

            Winner->CastSpell(Winner, 47292, false);
            sHelper->SendSerMessage(sHelper->GetGts(9606), sCharPvpTop->PvpCount, sHelper->GetNameLinkColor(Winner).c_str());

            Loser->TopMc = sCharPvpTop->GetTopMax();
            Loser->LastTopMc = Loser->TopMc;
            CharacterDatabasePreparedStatement* UStmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_SOLO_PVP_DATA);
            UStmt->SetData(0, Loser->TopMc);
            UStmt->SetData(1, Loser->LastTopMc);
            UStmt->SetData(2, Loser->GetGUIDLow());
            CharacterDatabase.Execute(UStmt);

            sCharPvpTop->SoloPlayerTop[Loser->LastTopMc] = Loser->GetGUID();
            sCharPvpTop->DeleteTopData(Loser->GetGUID());

            if (sCharPvpTop->GetTopMax() == 1)
            {
                Winner->TopMc = sCharPvpTop->GetTopMax();
                Winner->LastTopMc = Winner->TopMc;
                sCharPvpTop->SoloPlayerTop[Winner->LastTopMc] = Winner->GetGUID();

                CharacterDatabasePreparedStatement* UStmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_SOLO_PVP_DATA);
                UStmt->SetData(0, sCharPvpTop->GetTopMax());
                UStmt->SetData(1, Winner->LastTopMc);
                UStmt->SetData(2, Winner->GetGUIDLow());
                CharacterDatabase.Execute(UStmt);

                sHelper->SendSerMessage(sHelper->GetGts(9605), sHelper->GetNameLinkColor(Winner).c_str());

                if (!sCharPvpTop->SoloPlayerTop.empty())
                {
                    std::ostringstream Oss;
                    Oss << "=====比武大会排名=====\n";
                    for (auto Itr = sCharPvpTop->SoloPlayerTop.begin(); Itr != sCharPvpTop->SoloPlayerTop.end(); Itr++)
                    {
                        if (!Itr->first)
                            continue;

                        CharacterCacheEntry const* CacheEntry = sCharacterCache->GetCharacterCacheByGuid(Itr->second);
                        if (CacheEntry)
                        {
                            Oss << "第" << Itr->first << "名: " << CacheEntry->Name << " 奖励: " << sHelper->GetItemLink(sCharPvpTop->GetRewItemId(Itr->first)) << "\n";
                        }
                    }
                    sHelper->SendSerMessage(Oss.str().c_str());
                }

                sGameEventMgr->StopEvent(sCharPvpTop->GetSoloPvpConfig(100), false, true);
            }

            Winner->SetHealth(Winner->GetMaxHealth());
            Winner->RemoveArenaSpellCooldowns();
            Winner->SetPower(POWER_MANA, Winner->GetMaxPower(POWER_MANA));
            Winner->SetPower(POWER_ENERGY, Winner->GetMaxPower(POWER_ENERGY));

            Loser->SetHealth(Loser->GetMaxHealth());
            Loser->RemoveArenaSpellCooldowns();
            Loser->SetPower(POWER_MANA, Loser->GetMaxPower(POWER_MANA));
            Loser->SetPower(POWER_ENERGY, Loser->GetMaxPower(POWER_ENERGY));
        }
    }
};

class OnSoloPvPEventScript : public GameEventScript
{
public:
    OnSoloPvPEventScript() : GameEventScript("OnSoloPvPEventScript") { }

    void OnStart(uint16 DoEventId) override
    {
        if (DoEventId == sCharPvpTop->GetSoloPvpConfig(100))
        {
            sCharPvpTop->SendAsUpdateWorldState(sCharPvpTop->GetSoloPvpConfig(103));
            sCharPvpTop->SoloNextTime = 0;

            if (sHelper->GetBoolean(18088))
            {
                for (auto Iter = sCharPvpTop->AsSoloAuras.begin(); Iter != sCharPvpTop->AsSoloAuras.end(); Iter++)
                    CharacterDatabase.Execute("DELETE FROM character_aura WHERE spell = '{}';", (*Iter));

                SessionMap const& Sessions = sWorld->GetAllSessions();
                for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
                {
                    if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
                        continue;

                    for (auto It = sCharPvpTop->AsSoloAuras.begin(); It != sCharPvpTop->AsSoloAuras.end(); It++)
                    {
                        if (Itr->second->GetPlayer()->HasAura(*It))
                            Itr->second->GetPlayer()->RemoveAurasDueToSpell(*It);
                    }
                }
            }

            CharacterDatabasePreparedStatement* UStmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_SOLO_PVP_DATA2);
            UStmt->SetData(0, 0);
            CharacterDatabase.Execute(UStmt);

            sCharPvpTop->TeleStarted = false;
            sCharPvpTop->PvpCount = 0;
            sCharPvpTop->SoloPlayerTop.clear();
            sCharPvpTop->TopEvent = true;

            sHelper->SendSerMessage("[比武大会]: 活动正式开始 每一轮同时进行多个队伍决斗");
            sCharPvpTop->RollTopTeamNum();
        }
    }

    void OnStop(uint16 DoEventId) override
    {
        if (DoEventId == sCharPvpTop->GetSoloPvpConfig(100))
        {
            sCharPvpTop->SendAsUpdateWorldState(0);
            sCharPvpTop->SoloNextTime = 0;
            sCharPvpTop->TeleStarted = false;
            sCharPvpTop->TopEvent = false;
            sCharPvpTop->AddTopAuraToAllPlayer();
        }
    }
};

class SoloEventWorldScript : public WorldScript
{
public:
    SoloEventWorldScript() : WorldScript("SoloEventWorldScript") { }

    void OnUpdate(uint32 Diff) override
    {
        if (!sCharPvpTop->TopEvent)
            return;

        if (!sCharPvpTop->GetSoloPvpConfig(106))
            return;
    }
};

void AddSC_CharPvpTop()
{
    new NpcSoloPvpTopScript();
    new OnSoloPvpPlayerMod();
    new OnSoloPvPEventScript();
    new SoloEventWorldScript();
}
