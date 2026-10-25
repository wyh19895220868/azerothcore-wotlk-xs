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
#include "NoticeMgr.h"
#include <Time\GameTime.h>
#pragma execution_character_set("utf-8")

KillCreNoticeMap KillCreNoticeStore;
KillPlayerNoticeMap KillPlayerNoticeStore;
LootItemNoticeMap LootItemNoticeStore;
ItemNewNoticeMap ItemNewNoticeStore;

void NoticeMgr::LoadNoticeMgr()
{
    KillCreNoticeStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 生物编号, 通告内容 FROM _通告_击杀生物表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 CreEntry = Fields[0].Get<uint32>();
            std::string Notice = Fields[1].Get<std::string>();
            KillCreNoticeStore.insert(std::make_pair(CreEntry, Notice));
        } while (Result->NextRow());
    }

    KillPlayerNoticeStore.clear();
    Result = WorldDatabase.Query("SELECT 地图编号, 通告内容 FROM _通告_击杀玩家表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            int32 MapId = Fields[0].Get<int32>();
            std::string Notice = Fields[1].Get<std::string>();
            KillPlayerNoticeStore.insert(std::make_pair(MapId, Notice));
        } while (Result->NextRow());
    }

    LootItemNoticeStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 通告内容 FROM _通告_物品掉落表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            std::string Notice = Fields[1].Get<std::string>();
            LootItemNoticeStore.insert(std::make_pair(ItemId, Notice));
        } while (Result->NextRow());
    }

    ItemNewNoticeStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 通告内容 FROM _通告_物品获得表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            std::string Notice = Fields[1].Get<std::string>();
            ItemNewNoticeStore.insert(std::make_pair(ItemId, Notice));
        } while (Result->NextRow());
    }
}

void NoticeMgr::OnKillCreNotice(Player* player, Creature* Killed)
{
    if (!sHelper->GetBoolean(1015))
        return;

    if (sHelper->GetBoolean(3002) && player->GetSession()->GetSecurity() > SEC_PLAYER)
        return;

    uint32 CreEntry = Killed->GetEntry();
    auto Itr = KillCreNoticeStore.find(CreEntry);
    if (Itr != KillCreNoticeStore.end())
    {
        time_t Now = GameTime::GetGameTime().count();
        Killed->CombatTime = (Killed->CombatTime == 0) ? Now : Killed->CombatTime;
        time_t NeedTime = Now - Killed->CombatTime;
        NeedTime = (NeedTime <= 0 ? 3 : NeedTime);

        std::string Fpp = player->GetGroup() ? " <团队> " : " <个人> ";
        std::string Msg = Itr->second + Fpp + " 用时: " + sHelper->SecTimeString(NeedTime);

        if (sHelper->GetBoolean(13801))
            sHelper->SendGMMessage(Msg.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetCreatureNameByEntry(CreEntry).c_str());
        else
            sHelper->SendSerMessage(Msg.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetCreatureNameByEntry(CreEntry).c_str());

        Killed->CombatTime = 0;
    }
}

void NoticeMgr::OnKillPlayerNotice(Player* Killer, Player* Killed)
{
    if (!sHelper->GetBoolean(1016))
        return;

    if (Killer->GetGUID() == Killed->GetGUID())
        return;

    int32 MapId = Killer->GetMapId();
    for (auto Itr = KillPlayerNoticeStore.begin(); Itr != KillPlayerNoticeStore.end(); ++Itr)
    {
        if (Itr->first < 0)
        {
            sHelper->SendSerMessage(Itr->second.c_str(), sHelper->GetNameLinkColor(Killer).c_str(), sHelper->GetNameLinkColor(Killed).c_str());
        }
        else if (Itr->first == MapId)
        {
            sHelper->SendSerMessage(Itr->second.c_str(), sHelper->GetNameLinkColor(Killer).c_str(), sHelper->GetNameLinkColor(Killed).c_str());
        }
    }
}

void NoticeMgr::OnLootItemNotice(Player* player, Item* OnItem, uint32 Count, ObjectGuid LootGuid)
{
    if (!OnItem)
        return;

    if (!sHelper->GetBoolean(1017))
        return;

    if (sHelper->GetBoolean(3002) && player->GetSession()->GetSecurity() > SEC_PLAYER)
        return;

    uint32 ItemId = OnItem->GetEntry();
    auto Itr = LootItemNoticeStore.find(ItemId);
    if (Itr != LootItemNoticeStore.end())
    {
        sHelper->SendSerMessage(Itr->second.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetItemLink(ItemId).c_str(), Count);
    }
}

void NoticeMgr::OnItemNewNotice(Player* player, Item* OnItem, uint32 Count)
{
    if (!OnItem)
        return;

    if (!sHelper->GetBoolean(13101))
        return;

    if (sHelper->GetBoolean(13102) && player->GetSession()->GetSecurity() > SEC_PLAYER)
        return;

    uint32 ItemId = OnItem->GetEntry();
    auto Itr = ItemNewNoticeStore.find(ItemId);
    if (Itr != ItemNewNoticeStore.end())
    {
        sHelper->SendSerMessage(Itr->second.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetItemLink(ItemId).c_str(), Count);
    }
}

class OnNoticePlayer : public PlayerScript
{
public:
    OnNoticePlayer() : PlayerScript("OnNoticePlayer") {  }

    void OnCreatureKill(Player* player, Creature* Killed) override
    {
        sNoticeMgr->OnKillCreNotice(player, Killed);
    }

    void OnPVPKill(Player* Killer, Player* Killed) override
    {
        if (Killer->GetGUID() == Killed->GetGUID())
            return;

        if (strcmp(Killer->GetSession()->GetRemoteAddress().c_str(), Killed->GetSession()->GetRemoteAddress().c_str()) == 0)
            return;

        sNoticeMgr->OnKillPlayerNotice(Killer, Killed);
    }

    void OnLootItem(Player* player, Item* OnItem, uint32 Count, ObjectGuid LootGuid) override
    {
        sNoticeMgr->OnLootItemNotice(player, OnItem, Count, LootGuid);
    }

    void OnStoreNewItem(Player* player, Item* pItem, uint32 Count) override
    {
        sNoticeMgr->OnItemNewNotice(player, pItem, Count);
    }
};

void AddSC_NoticePlayerScripts()
{
    new OnNoticePlayer();
}
