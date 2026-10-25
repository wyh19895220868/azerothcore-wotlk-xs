#include "Custom/_Helper/XsPlayerbotCompat.h"
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
#include "Player.h"
#include "Item.h"
#include "Bag.h"
#include "Pet.h"
#include <CharacterCache.h>
#include <algorithm>
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include "PlayerActivityMgr.h"
#pragma execution_character_set("utf-8")

ActivityTempVect ActivityStore;
RewActivityTempMap RewActivityStore;

void ActivityMgr::LoadActivityData()
{
    ActivityStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 序号, 活跃类型, 变量1, 变量2, 奖励活跃值, 提示信息 FROM _每日活跃度配置表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            ActivityTemp St;
            St.Num = Fields[0].Get<uint32>();
            std::string_view TypeStr = Fields[1].Get<std::string_view>();
            const char* Ctype = TypeStr.data();
            if (Ctype && *Ctype != '\0')
            {
                if (strcmp(Ctype, "击杀怪物") == 0)
                    St.AType = A_KILL_CRE;
                else if (strcmp(Ctype, "收集物品") == 0)
                    St.AType = A_KT_ITEM;
                else if (strcmp(Ctype, "完成任务") == 0)
                    St.AType = A_QUEST;
                else if (strcmp(Ctype, "击杀玩家") == 0)
                    St.AType = A_KILL_PLR;
                else if (strcmp(Ctype, "赢得战场") == 0)
                    St.AType = A_BG_WIN;
            }

            St.Param1 = Fields[2].Get<uint32>();
            St.Param2 = Fields[3].Get<uint32>();
            St.ActivityValue = Fields[4].Get<uint32>();
            St.Info = Fields[5].Get<std::string>();
            ActivityStore.push_back(St);

        } while (Result->NextRow());
    }

    uint32 Ct = 0;
    RewActivityStore.clear();
    Result = WorldDatabase.Query("SELECT 达到活跃值, 奖励模版 FROM _每日活跃度奖励表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 OnActivityValue = Fields[0].Get<uint32>();
            uint32 RTempId = Fields[1].Get<uint32>();
            Ct++;
            if (Ct > 24)
                continue;

            RewActivityStore.insert(std::make_pair(OnActivityValue, RTempId));

        } while (Result->NextRow());
    }
}

std::string ActivityMgr::GetActivityInfoByAType(ActivityTypes AType, uint32 Param)
{
    if (!Param)
        return "N";
    switch (AType)
    {
    case A_KILL_CRE: return  sHelper->GetCreatureNameByEntry(Param);
    case A_KT_ITEM: return  "I";
    case A_QUEST: return  sHelper->GetQuestName(Param);
    case A_KILL_PLR: return  "K";
    case A_BG_WIN: return  "B";
    default: return "N";
    }
    return "N";
}

uint32 ActivityMgr::GetAsActivity()
{
    uint32 Count = 0;
    for (auto Itr = ActivityStore.begin(); Itr != ActivityStore.end(); Itr++)
        Count += Itr->ActivityValue;

    return Count;
}

void ActivityMgr::SendPacketActivityData(Player* player)
{
    for (auto Itr = ActivityStore.begin(); Itr != ActivityStore.end(); Itr++)
    {
        std::string Link = GetActivityInfoByAType(Itr->AType, Itr->Param1);
        sDoAddon->SendPacketToClient(player, "DaySendActivityStoreData", std::to_string(Itr->Num) + "$" + std::to_string(uint32(Itr->AType)) + "$" + std::to_string(Itr->Param1) + "$" + std::to_string(Itr->Param2) + "$" + Link + "$" + std::to_string(Itr->ActivityValue) + "$" + Itr->Info + "$");
    }

    uint32 N = 0;
    for (auto Itr = RewActivityStore.begin(); Itr != RewActivityStore.end(); Itr++)
    {
        N++;
        std::string RGts = sHelper->GetRewardGts(Itr->second);
        sDoAddon->SendPacketToClient(player, "DaySendRewActivityStore", std::to_string(N) + "$" + std::to_string(Itr->first) + "$" + RGts + "$");
    }

    sDoAddon->SendPacketToClient(player, "DaySendActivityDataSize", std::to_string(ActivityStore.size()));
}

void ActivityMgr::DayResetSendPacketActivityData()
{
    LoadActivityData();

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        Player* player = Itr->second->GetPlayer();
        sDoAddon->SendPacketToClient(player, "CloseActivity");
        player->PActivityTemp1Vect.clear();
        player->PActivityTemp2Vect.clear();
        player->TempAtStore[uint32(AT4002)] = 0;
        SendPlayerActivityData(player);
    }

    CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_TEMP_AT);
    Rtmt->SetData(0, 0);
    Rtmt->SetData(1, uint32(AT4002));
    CharacterDatabase.Execute(Rtmt);

    CharacterDatabase.Execute(CharacterDatabase.GetPreparedStatement(CHAR_DEL_ACTIVITY));
    CharacterDatabase.Execute(CharacterDatabase.GetPreparedStatement(CHAR_DEL_ACTIVITY_POP));
}

void ActivityMgr::SendPlayerActivityData(Player* player)
{
    if (player->PActivityTemp1Vect.empty())
    {
        sDoAddon->SendPacketToClient(player, "AutoPlayerActivityData1", "Nullptr");
    }
    else
    {
        std::sort(player->PActivityTemp1Vect.begin(), player->PActivityTemp1Vect.end(), [](const PActivityTemp1& V1, const PActivityTemp1& V2) {return V1.Num < V2.Num; });
        for (auto Itr = player->PActivityTemp1Vect.begin(); Itr != player->PActivityTemp1Vect.end(); Itr++)
            sDoAddon->SendPacketToClient(player, "AutoPlayerActivityData1", std::to_string(Itr->Num) + "$" + std::to_string(Itr->AType) + "$" + std::to_string(Itr->Param1) + "$" + std::to_string(Itr->Param2) + "$");
    }

    if (player->PActivityTemp2Vect.empty())
    {
        sDoAddon->SendPacketToClient(player, "AutoPlayerActivityData2", "Nullptr");
    }
    else
    {
        uint32 N = 0;
        std::sort(player->PActivityTemp2Vect.begin(), player->PActivityTemp2Vect.end(), [](const PActivityTemp2& V1, const PActivityTemp2& V2) {return V1.ActivityPop < V2.ActivityPop; });
        for (auto Itr = player->PActivityTemp2Vect.begin(); Itr != player->PActivityTemp2Vect.end(); Itr++)
        {
            N++;
            sDoAddon->SendPacketToClient(player, "AutoPlayerActivityData2", std::to_string(N) + "$" + std::to_string(Itr->ActivityPop) + "$" + std::to_string(Itr->OnRew ? 1 : 0) + "$");
        }
    }

    sDoAddon->SendPacketToClient(player, "AutoAsNowPlayerActivity", AsInt64ToString(player->TempAtStore[AT4002].Value) + "#" + std::to_string(GetAsActivity()));
}

void ActivityMgr::UpdatePlayerOnePopActivity(Player* player)
{
    if (player->PActivityTemp2Vect.empty())
    {
        sDoAddon->SendPacketToClient(player, "AutoPlayerActivityData2", "Nullptr");
    }
    else
    {
        uint32 N = 0;
        std::sort(player->PActivityTemp2Vect.begin(), player->PActivityTemp2Vect.end(), [](const PActivityTemp2& V1, const PActivityTemp2& V2) {return V1.ActivityPop < V2.ActivityPop; });
        for (auto Itr = player->PActivityTemp2Vect.begin(); Itr != player->PActivityTemp2Vect.end(); Itr++)
        {
            N++;
            sDoAddon->SendPacketToClient(player, "AutoPlayerActivityData2", std::to_string(N) + "$" + std::to_string(Itr->ActivityPop) + "$" + std::to_string(Itr->OnRew ? 1 : 0) + "$");
        }
    }
}

bool ActivityMgr::IsOnRewPopValue(Player* player, uint32 PopValue)
{
    if (player->TempAtStore[AT4002].Value < PopValue)
        return false;
    bool On = true;
    for (auto Itr = player->PActivityTemp2Vect.begin(); Itr != player->PActivityTemp2Vect.end(); Itr++)
    {
        if (Itr->ActivityPop == PopValue && Itr->OnRew)
            On = false;
    }
    return On;
}

void ActivityMgr::SetOnRewPopValueAndUpdateDB(Player* player, uint32 PopValue)
{
    bool At = false;
    for (auto Itr = player->PActivityTemp2Vect.begin(); Itr != player->PActivityTemp2Vect.end(); Itr++)
    {
        if (Itr->ActivityPop == PopValue)
        {
            Itr->OnRew = true;
            At = true;
        }
    }

    if (!At)
    {
        PActivityTemp2 St;
        St.ActivityPop = PopValue;
        St.OnRew = true;
        player->PActivityTemp2Vect.push_back(St);
    }

    CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_ACTIVITY_POP);
    Utmt->SetData(0, player->GetGUIDLow());
    Utmt->SetData(1, PopValue);
    Utmt->SetData(2, true);
    CharacterDatabase.Execute(Utmt);
}

uint32 ActivityMgr::GetRewActivity(uint32 Num, uint32 AType, uint32 Param1)
{
    uint32 RewActivity = 0;
    for (auto Itr = ActivityStore.begin(); Itr != ActivityStore.end(); Itr++)
    {
        if (Itr->Num == Num && Itr->AType == ActivityTypes(AType) && Itr->Param1 == Param1)
        {
            RewActivity = Itr->ActivityValue;
        }
    }
    return RewActivity;
}

uint32 ActivityMgr::GetRewActivitySc(uint32 Num, uint32 AType)
{
    uint32 RewActivity = 0;
    for (auto Itr = ActivityStore.begin(); Itr != ActivityStore.end(); Itr++)
    {
        if (Itr->Num == Num && Itr->AType == ActivityTypes(AType))
        {
            RewActivity = Itr->ActivityValue;
        }
    }
    return RewActivity;
}

bool ActivityMgr::IsComOrMax(Player* player, uint32 Num, uint32 AType, uint32 Param1, uint32 DBCount)
{
    if (!Num)
        return true;
    bool ComOrMax = false;
    for (auto Itr = player->PActivityTemp1Vect.begin(); Itr != player->PActivityTemp1Vect.end(); Itr++)
    {
        if (Itr->Num == Num && Itr->AType == AType && Itr->Param1)
        {
            if (Itr->Param1 == Param1 && Itr->Param2 >= DBCount)
                ComOrMax = true;
        }
    }
    return ComOrMax;
}

bool ActivityMgr::IsComOrMaxSc(Player* player, uint32 Num, uint32 AType, uint32 DBCount)
{
    if (!Num)
        return true;
    bool ComOrMax = false;
    for (auto Itr = player->PActivityTemp1Vect.begin(); Itr != player->PActivityTemp1Vect.end(); Itr++)
    {
        if (Itr->Num == Num && Itr->AType == AType && Itr->Param2 >= DBCount)
            ComOrMax = true;
    }
    return ComOrMax;
}

std::pair<uint32, uint32> ActivityMgr::GetScNumDBCountByParamAType(uint32 AType)
{
    std::pair<uint32, uint32> Result;
    Result.first = 0;
    Result.second = 1;
    for (auto Itr = ActivityStore.begin(); Itr != ActivityStore.end(); Itr++)
    {
        if (Itr->AType == ActivityTypes(AType))
        {
            Result.first = Itr->Num;
            Result.second = Itr->Param2;
        }
    }
    return Result;
}

std::pair<uint32, uint32> ActivityMgr::GetNumDBCountByParamAType(uint32 AType, uint32 Param1)
{
    std::pair<uint32, uint32> Result;
    Result.first = 0;
    Result.second = 1;
    for (auto Itr = ActivityStore.begin(); Itr != ActivityStore.end(); Itr++)
    {
        if (Itr->AType == ActivityTypes(AType) && Itr->Param1 == Param1)
        {
            Result.first = Itr->Num;
            Result.second = Itr->Param2;
        }
    }
    return Result;
}

void ActivityMgr::UpdatePlayerActivityDataDB(Player* player, uint32 DBCount, uint32 Num, uint32 AType, uint32 Param1, uint32 Param2)
{
    uint32 OnCount;
    bool At = false;
    for (auto Itr = player->PActivityTemp1Vect.begin(); Itr != player->PActivityTemp1Vect.end(); Itr++)
    {
        if (Itr->Num == Num && AType == Itr->AType)
        {
            Itr->Param1 = Param1;
            Itr->Param2 += Param2;
            if (Itr->Param2 >= DBCount)
                Itr->Param2 = DBCount;

            OnCount = Itr->Param2;
            At = true;
        }
    }

    if (!At)
    {
        if (Param2 >= DBCount)
            Param2 = DBCount;

        OnCount = Param2;
        PActivityTemp1 St;
        St.Num = Num;
        St.AType = AType;
        St.Param1 = Param1;
        St.Param2 = Param2;
        player->PActivityTemp1Vect.push_back(St);
    }

    CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ACTIVITY_PARAM);
    Utmt->SetData(0, player->GetGUIDLow());
    Utmt->SetData(1, Num);
    Utmt->SetData(2, AType);
    Utmt->SetData(3, Param1);
    Utmt->SetData(4, OnCount);
    CharacterDatabase.Execute(Utmt);
}

void ActivityMgr::UpdatePlayerUIDataActivity(Player* player)
{
    if (player->PActivityTemp1Vect.empty())
    {
        sDoAddon->SendPacketToClient(player, "AutoPlayerActivityData1", "Nullptr");
    }
    else
    {
        std::sort(player->PActivityTemp1Vect.begin(), player->PActivityTemp1Vect.end(), [](const PActivityTemp1& V1, const PActivityTemp1& V2) {return V1.Num < V2.Num; });
        for (auto Itr = player->PActivityTemp1Vect.begin(); Itr != player->PActivityTemp1Vect.end(); Itr++)
            sDoAddon->SendPacketToClient(player, "AutoPlayerActivityData1", std::to_string(Itr->Num) + "$" + std::to_string(Itr->AType) + "$" + std::to_string(Itr->Param1) + "$" + std::to_string(Itr->Param2) + "$");
    }

    for (auto Itr = ActivityStore.begin(); Itr != ActivityStore.end(); Itr++)
    {
        std::string Link = GetActivityInfoByAType(Itr->AType, Itr->Param1);
        sDoAddon->SendPacketToClient(player, "DaySendActivityStoreData", std::to_string(Itr->Num) + "$" + std::to_string(uint32(Itr->AType)) + "$" + std::to_string(Itr->Param1) + "$" + std::to_string(Itr->Param2) + "$" + Link + "$" + std::to_string(Itr->ActivityValue) + "$" + Itr->Info + "$");
    }

    sDoAddon->SendPacketToClient(player, "AutoAsNowPlayerActivity", AsInt64ToString(player->TempAtStore[AT4002].Value) + "#" + std::to_string(GetAsActivity()));
}

void Player::OnLoginActivity()
{
    if (IsXsPlayerbot(this))
        return;

    PActivityTemp1Vect.clear();
    uint32 GuidLow = GetGUIDLow();
    CharacterDatabasePreparedStatement* CStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ACTIVITY_DATA);
    CStmt->SetData(0, GuidLow);
    PreparedQueryResult CResult = CharacterDatabase.Query(CStmt);
    if (CResult)
    {
        do
        {
            Field* Fields = CResult->Fetch();
            PActivityTemp1 St;
            St.Num = Fields[0].Get<uint32>();
            St.AType = Fields[1].Get<uint32>();
            St.Param1 = Fields[2].Get<uint32>();
            St.Param2 = Fields[3].Get<uint32>();
            PActivityTemp1Vect.push_back(St);
        } while (CResult->NextRow());
    }

    if (!PActivityTemp1Vect.empty())
        std::sort(PActivityTemp1Vect.begin(), PActivityTemp1Vect.end(), [](const PActivityTemp1& V1, const PActivityTemp1& V2) {return V1.Num < V2.Num; });

    PActivityTemp2Vect.clear();
    CStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ACTIVITY_POP);
    CStmt->SetData(0, GuidLow);
    CResult = CharacterDatabase.Query(CStmt);
    if (CResult)
    {
        do
        {
            Field* Fields = CResult->Fetch();
            PActivityTemp2 St;
            St.ActivityPop = Fields[0].Get<uint32>();
            St.OnRew = Fields[1].Get<bool>();
            PActivityTemp2Vect.push_back(St);
        } while (CResult->NextRow());
    }

    if (!PActivityTemp2Vect.empty())
        std::sort(PActivityTemp2Vect.begin(), PActivityTemp2Vect.end(), [](const PActivityTemp2& V1, const PActivityTemp2& V2) {return V1.ActivityPop < V2.ActivityPop; });
}

class OnActivityPlayer : public PlayerScript
{
public:
    OnActivityPlayer() : PlayerScript("OnActivityPlayer") {  }

    void OnCreatureKill(Player* player, Creature* Killed) override
    {
        uint32 AType = 1;
        uint32 Entry = Killed->GetEntry();
        auto [Num, DBCount] = sActivityMgr->GetNumDBCountByParamAType(AType, Entry);

        Group* Gp = Killed->GetLootRecipientGroup();
        if (Gp)
        {
            for (GroupReference* Itr = Gp->GetFirstMember(); Itr != nullptr; Itr = Itr->next())
            {
                Player* Member = Itr->GetSource();
                if (Member && Member->IsInWorld() && Killed->IsInMap(Member))
                {
                    if (sActivityMgr->IsComOrMax(Member, Num, AType, Entry, DBCount))
                        continue;
                    sActivityMgr->UpdatePlayerActivityDataDB(Member, DBCount, Num, AType, Entry, 1);
                    if (sActivityMgr->IsComOrMax(Member, Num, AType, Entry, DBCount))
                    {
                        uint32 Now = Member->GetTempAtUInt32(AT4002);
                        uint32 RewActivity = sActivityMgr->GetRewActivity(Num, AType, Entry);
                        Member->UpdateTempAt(AT4002, Now + RewActivity);
                        Member->SendSysMsg(sHelper->GetGts(6002), RewActivity);
                    }
                    sActivityMgr->UpdatePlayerUIDataActivity(Member);
                }
            }
        }
        else
        {
            if (sActivityMgr->IsComOrMax(player, Num, AType, Entry, DBCount))
                return;

            sActivityMgr->UpdatePlayerActivityDataDB(player, DBCount, Num, AType, Entry, 1);
            if (sActivityMgr->IsComOrMax(player, Num, AType, Entry, DBCount))
            {
                uint32 Now = player->GetTempAtUInt32(AT4002);
                uint32 RewActivity = sActivityMgr->GetRewActivity(Num, AType, Entry);
                player->UpdateTempAt(AT4002, Now + RewActivity);
                player->SendSysMsg(sHelper->GetGts(6002), RewActivity);
            }
            sActivityMgr->UpdatePlayerUIDataActivity(player);
        }
    }

    void OnStoreNewItem(Player* player, Item* pItem, uint32 Count) override
    {
        uint32 AType = 2;
        uint32 Entry = pItem->GetEntry();
        auto [Num, DBCount] = sActivityMgr->GetNumDBCountByParamAType(AType, Entry);
        if (sActivityMgr->IsComOrMax(player, Num, AType, Entry, DBCount))
            return;

        sActivityMgr->UpdatePlayerActivityDataDB(player, DBCount, Num, AType, Entry, Count);
        if (sActivityMgr->IsComOrMax(player, Num, AType, Entry, DBCount))
        {
            uint32 Now = player->GetTempAtUInt32(AT4002);
            uint32 RewActivity = sActivityMgr->GetRewActivity(Num, AType, Entry);
            player->UpdateTempAt(AT4002, Now + RewActivity);
            player->SendSysMsg(sHelper->GetGts(6003), RewActivity);
        }
        sActivityMgr->UpdatePlayerUIDataActivity(player);
    }

    void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
    {
        uint32 AType = 3;
        uint32 Entry = quest->GetQuestId();
        auto [Num, DBCount] = sActivityMgr->GetNumDBCountByParamAType(AType, Entry);
        if (sActivityMgr->IsComOrMax(player, Num, AType, Entry, DBCount))
            return;

        sActivityMgr->UpdatePlayerActivityDataDB(player, DBCount, Num, AType, Entry, 1);
        if (sActivityMgr->IsComOrMax(player, Num, AType, Entry, DBCount))
        {
            uint32 Now = player->GetTempAtUInt32(AT4002);
            uint32 RewActivity = sActivityMgr->GetRewActivity(Num, AType, Entry);
            player->UpdateTempAt(AT4002, Now + RewActivity);
            player->SendSysMsg(sHelper->GetGts(6004), RewActivity);
        }
        sActivityMgr->UpdatePlayerUIDataActivity(player);
    }

    void OnPVPKill(Player* Killer, Player* Killed) override
    {
        if (Killer->GetGUID() == Killed->GetGUID())
            return;

        if (strcmp(Killer->GetSession()->GetRemoteAddress().c_str(), Killed->GetSession()->GetRemoteAddress().c_str()) == 0)
            return;

        uint32 AType = 4;
        auto [Num, DBCount] = sActivityMgr->GetScNumDBCountByParamAType(AType);
        if (sActivityMgr->IsComOrMaxSc(Killer, Num, AType, DBCount))
            return;

        sActivityMgr->UpdatePlayerActivityDataDB(Killer, DBCount, Num, AType, 0, 1);
        if (sActivityMgr->IsComOrMaxSc(Killer, Num, AType, DBCount))
        {
            uint32 Now = Killer->GetTempAtUInt32(AT4002);
            uint32 RewActivity = sActivityMgr->GetRewActivitySc(Num, AType);
            Killer->UpdateTempAt(AT4002, Now + RewActivity);
            Killer->SendSysMsg(sHelper->GetGts(6003), RewActivity);
        }
        sActivityMgr->UpdatePlayerUIDataActivity(Killer);
    }
};

class BGWinTeamActivity : public BGScript
{
public:
    BGWinTeamActivity() : BGScript("BGWinTeamActivity") {}

    void OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId winnerTeamId) override
    {
        if (player->GetBgTeamId() != winnerTeamId)
            return;

        uint32 AType = 5;
        auto [Num, DBCount] = sActivityMgr->GetScNumDBCountByParamAType(AType);
        if (sActivityMgr->IsComOrMaxSc(player, Num, AType, DBCount))
            return;

        sActivityMgr->UpdatePlayerActivityDataDB(player, DBCount, Num, AType, 0, 1);
        if (sActivityMgr->IsComOrMaxSc(player, Num, AType, DBCount))
        {
            uint32 Now = player->GetTempAtUInt32(AT4002);
            uint32 RewActivity = sActivityMgr->GetRewActivitySc(Num, AType);
            player->UpdateTempAt(AT4002, Now + RewActivity);
            player->SendSysMsg(sHelper->GetGts(6003), RewActivity);
        }
        sActivityMgr->UpdatePlayerUIDataActivity(player);
    }
};

void AddSC_ActivityPlayerMgrScripts()
{
    new OnActivityPlayer();
    new BGWinTeamActivity();
}
