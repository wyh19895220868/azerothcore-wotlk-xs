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
#include "ItemShopMgr.h"
#pragma execution_character_set("utf-8")

ItemShopMap ItemShopStore;

void ItemShopMgr::LoadItemShop()
{
    ItemShopStore.clear();
    uint32 Ct = 0;
    QueryResult Result = WorldDatabase.Query("SELECT 物品编号, 归属分类, 需求模版 FROM _在线商城出售表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            ItemShopTemp St;
            St.MenuTab = Fields[1].Get<uint32>();
            St.NTempId = Fields[2].Get<uint32>();
            ItemShopStore.insert(std::make_pair(ItemId, St));
            ++Ct;
        } while (Result->NextRow());
    }
}

void ItemShopMgr::SendTabNames(Player* player)
{
    std::string SendMsg;
    SendMsg.clear();
    for (uint32 i = 1071; i < 1077; ++i)
    {
        std::string Name = std::string(sHelper->GetConfig(i));
        Name = (Name == "" ? "nullptr" : Name);
        SendMsg.append(Name + "$");
    }
    sDoAddon->SendPacketToClient(player, "ShowTabName", SendMsg);
    SendMsg.clear();
}

uint32 ItemShopMgr::GetCountForTab(uint32 Tab)
{
    uint32 Count = 0;
    for (auto Itr = ItemShopStore.begin(); Itr != ItemShopStore.end(); ++Itr)
    {
        if (Itr->second.MenuTab == Tab)
            Count++;
    }

    return Count;
}

uint32 ItemShopMgr::GetPage(uint32 ItemListSize)
{
    uint32 PageCount = uint32(ItemListSize / MAX_LIST_COUNT);
    uint32 X = (ItemListSize % MAX_LIST_COUNT);
    if (X != 0)
    {
        if (ItemListSize > PageCount * MAX_LIST_COUNT)
            PageCount++;
    }

    return PageCount;
}

void ItemShopMgr::SendDataForPage(Player* player, uint32 Tab, uint32 Page)
{
    Page = Page - 1;
    Page = (Page > 0) ? Page : 0;
    std::vector<uint32> ItemList;
    ItemList.clear();
    for (auto Itr = ItemShopStore.begin(); Itr != ItemShopStore.end(); ++Itr)
    {
        if (Itr->second.MenuTab == Tab)
            ItemList.push_back(Itr->first);
    }

    uint32 Count = ItemList.size();
    if (!Count)
        return;

    uint32 N = Page * MAX_LIST_COUNT;
    uint32 M = (Page + 1) * MAX_LIST_COUNT;
    uint32 X = (Count % MAX_LIST_COUNT);

    if (X != 0 && M > Count)
        M = N + X;

    std::string SendMsg;
    SendMsg.clear();

    for (uint32 i = N; i < M; ++i)
    {
        uint32 ItemId = ItemList[i];
        SendMsg.append(sHelper->GetItemLink(ItemId) + "#");
    }

    uint32 PageCount = GetPage(Count);
    sDoAddon->SendPacketToClient(player, "ShowShopTabPage", std::to_string(PageCount) + "$" + SendMsg);

    ItemList.clear();
    SendMsg.clear();
}

void ItemShopMgr::SendDataForSearch(Player* player, std::string IdOrName)
{
    //判断是否数字
    bool Num = sHelper->IsAsNum(IdOrName);

    std::string ItemName = IdOrName;
    uint32 ItemId = atoi(IdOrName.c_str());
    std::vector<uint32> ItemList;
    ItemList.clear();
    if (Num)
    {
        for (auto Itr = ItemShopStore.begin(); Itr != ItemShopStore.end(); Itr++)
        {
            if (ItemId == Itr->first)
                ItemList.push_back(Itr->first);
        }
    }
    else
    {
        for (auto Itr = ItemShopStore.begin(); Itr != ItemShopStore.end(); Itr++)
        {
            ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->first);
            if (!Proto)
                continue;

            std::string IName = Proto->Name1;
            if (IName.find(ItemName) != std::string::npos)
                ItemList.push_back(Itr->first);
        }
    }

    uint32 Count = ItemList.size() > MAX_LIST_COUNT ? MAX_LIST_COUNT : ItemList.size();
    if (!Count)
        return;

    std::string SendMsg;
    SendMsg.clear();

    for (uint32 i = 0; i < Count; ++i)
    {
        uint32 ItemId = ItemList[i];
        SendMsg.append(sHelper->GetItemLink(ItemId) + "#");
    }
    sDoAddon->SendPacketToClient(player, "ShowSearchShop", SendMsg);

    ItemList.clear();
    SendMsg.clear();
}

uint32 ItemShopMgr::GetNTempId(uint32 ItemId)
{
    auto Itr = ItemShopStore.find(ItemId);
    return (Itr != ItemShopStore.end()) ? Itr->second.NTempId : 0;
}

void ItemShopMgr::SendNTempGtsForPage(Player* player, uint32 ItemId)
{
    uint32 NTempId = GetNTempId(ItemId);
    if (NTempId)
    {
        std::string Gts = sHelper->GetNeedGts(NTempId);
        sDoAddon->SendPacketToClient(player, "ShowBuyNeedInfo", sHelper->GetItemLink(ItemId) + "$" + Gts);
    }
    else
    {
        if (!sItemShopMgr->IsBuyItem(player, ItemId))
            return;
        player->AddItem(ItemId, 1, "在线商城购买获得");
    }
}

bool ItemShopMgr::IsBuyItem(Player* player, uint32 ItemId)
{
    ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(ItemId);
    if (!pProto)
        return false;

    auto Itr = ItemShopStore.find(ItemId);
    if (Itr == ItemShopStore.end())
        return false;

    if (pProto->MaxCount > 0)
    {
        if (player->HasItemCount(pProto->ItemId, pProto->MaxCount, true))
        {
            player->GetSession()->SendNotification(sHelper->GetGts(1080));
            player->SendSysMsg(sHelper->GetGts(1080));
            return false;
        }
    }
    return true;
}
