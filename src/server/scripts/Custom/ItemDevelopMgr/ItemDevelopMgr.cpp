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
#include "ItemDevelopMgr.h"
#include <Custom/EventMgr/EventMgr.h>
#include <Custom/PlayerModMgr/PlayerModMgr.h>
#include <Custom/CreatureMod/AsCreature.h>
#include <Custom/ItemRandomAtMgr/ItemRandomAtMgr.h>
#pragma execution_character_set("utf-8")

DevelopConfigVect DevelopConfigStore;
UseXpGroupVect UseXpGroupStore;
ItemDevelopCacheMap ItemDevelopCacheStore;

void ItemDevelopMgr::LoadItemDevelopData()
{
    DevelopConfigStore.clear();
    UseXpGroupStore.clear();
}

bool ItemDevelopMgr::IsInItemDevelop(uint32 ItemId)
{
    bool In = false;
    for (auto Itr = DevelopConfigStore.begin(); Itr != DevelopConfigStore.end(); ++Itr)
        if (Itr->ItemId == ItemId)
            In = true;

    return In;
}

uint32 ItemDevelopMgr::GetDevelopNextRankXp(uint32 ItemId, uint32 NextRank)
{
    uint32 NextRankXp = 0;
    for (auto Itr = DevelopConfigStore.begin(); Itr != DevelopConfigStore.end(); ++Itr)
        if (Itr->ItemId == ItemId && Itr->Rank == NextRank)
            NextRankXp = Itr->NeedXp;

    return NextRankXp;
}

std::set<Item*> ItemDevelopMgr::AutoPlayerAsInDevelop(Player* player)
{
    std::set<Item*> ItemList;

    //身上和主包
    for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; i++)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (IsInItemDevelop(pItem->GetEntry()))
                ItemList.insert(pItem);

    //额外四包
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
        if (Bag* pBag = player->GetBagByPos(i))
            for (uint32 j = 0; j < pBag->GetBagSize(); j++)
                if (Item* pItem = player->GetItemByPos(i, j))
                    if (IsInItemDevelop(pItem->GetEntry()))
                        ItemList.insert(pItem);

    return ItemList;
}

uint32 ItemDevelopMgr::GetUseItemDevelopRankXp(uint32 UseItemId, uint32 TarItemId)
{
    uint32 RankXp = 0;
    for (auto Itr = UseXpGroupStore.begin(); Itr != UseXpGroupStore.end(); ++Itr)
    {
        if (Itr->UseItemId == UseItemId)
        {
            if (!Itr->TarItemId)
            {
                RankXp = urand(Itr->RewXpMin, Itr->RewXpMax);
                break;
            }
            else if (Itr->TarItemId == TarItemId)
            {
                RankXp = urand(Itr->RewXpMin, Itr->RewXpMax);
                break;
            }
        }
    }

    return RankXp;
}

std::pair<uint32, uint32> ItemDevelopMgr::GetItemDevelopDataCt(uint32 ItemId, uint32 NextRank)
{
    std::pair<uint32, uint32> Result;
    Result.first = 0;
    Result.second = 0;

    for (auto Itr = DevelopConfigStore.begin(); Itr != DevelopConfigStore.end(); ++Itr)
    {
        if (Itr->ItemId == ItemId && Itr->Rank == NextRank)
        {
            Result.first = Itr->NewIdType;
            Result.second = Itr->STempId;
        }
    }
    return Result;
}

void ItemDevelopMgr::CreateSendDataDevelop(Item* DoItem)
{
    if (!DoItem->GetOwner())
        return;

    if (DevelopConfigStore.empty())
        return;

    if (!IsInItemDevelop(DoItem->GetEntry()))
        return;
    uint32 NextRankXp = sItemDevelopMgr->GetDevelopNextRankXp(DoItem->GetEntry(), DoItem->DeveLopRank + 1);

    ItemDevelopCacheTemp St;
    St.ItemId = DoItem->GetEntry();
    St.Rank = DoItem->DeveLopRank;
    St.NowXp = DoItem->DeveLopXp;
    ItemDevelopCacheStore[DoItem->GetGUIDLow()] = St;

    CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_DEVELOP_AT);
    Rtmt->SetData(0, DoItem->GetGUIDLow());
    Rtmt->SetData(1, DoItem->GetEntry());
    Rtmt->SetData(2, DoItem->DeveLopRank);
    Rtmt->SetData(3, DoItem->DeveLopXp);
    CharacterDatabase.Execute(Rtmt);

    std::string SendMsg;
    SendMsg.append(std::to_string(DoItem->GetGUIDLow()) + "#" + std::to_string(DoItem->DeveLopRank) + "#" + std::to_string(DoItem->DeveLopXp) + "#" + std::to_string(NextRankXp));
    sDoAddon->SendPacketToClient(DoItem->GetOwner(), "ClientDevelopData", SendMsg);
}

void ItemDevelopMgr::LoadFromDBDevelop(Item* DoItem)
{
    if (DevelopConfigStore.empty())
        return;

    if (!DoItem)
        return;

    ItemTemplate const* Temp = DoItem->GetTemplate();
    if (!Temp)
        return;

    auto Itr = ItemDevelopCacheStore.find(DoItem->GetGUIDLow());
    if (Itr == ItemDevelopCacheStore.end())
        return;

    DoItem->DeveLopRank = Itr->second.Rank;
    DoItem->DeveLopXp = Itr->second.NowXp;
}

class ItemUseDevelop : public ItemScript
{
public:
    ItemUseDevelop() : ItemScript("ItemUseDevelop") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (DevelopConfigStore.empty())
            return false;

        Item* DoItem = Targets.GetItemTarget();
        if (!CastItem || !DoItem)
            return false;

        if (DoItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        uint32 UseItemId = CastItem->GetEntry();
        uint32 GuidLow = DoItem->GetGUIDLow();
        uint32 ItemId = DoItem->GetEntry();
        uint32 NowXp = DoItem->DeveLopXp;
        uint32 NextRank = DoItem->DeveLopRank + 1;

        uint32 NextRankXp = sItemDevelopMgr->GetDevelopNextRankXp(ItemId, NextRank);
        if (!NextRankXp)
        {
            player->SendSysMsg(sHelper->GetGts(8101));
            return false;
        }

        uint32 RewRankXp = sItemDevelopMgr->GetUseItemDevelopRankXp(UseItemId, ItemId);
        if (!RewRankXp)
            return false;

        if (RewRankXp)
        {
            RewRankXp += DoItem->DeveLopXp;
            while (RewRankXp >= NextRankXp && NextRankXp)
            {
                RewRankXp -= NextRankXp;
                DoItem->DeveLopRank += 1;
                NextRankXp = sItemDevelopMgr->GetDevelopNextRankXp(ItemId, DoItem->DeveLopRank + 1);
            }

            uint32 NewDeveLopRank = DoItem->DeveLopRank;
            uint32 NewDeveLopXp = RewRankXp;
            DoItem->DeveLopXp = NewDeveLopXp;

            if (NewDeveLopRank >= NextRank)
            {
                auto [NewIdType, STempId] = sItemDevelopMgr->GetItemDevelopDataCt(ItemId, NextRank);
                if (NewIdType)
                {
                    player->DestroyItem(DoItem->GetBagSlot(), DoItem->GetSlot(), true);
                    Item* NewItem = player->AddItemByIdOne(NewIdType, "装备超级特殊成长获得", true);
                    if (NewItem)
                    {
                        GuidLow = NewItem->GetGUIDLow();
                        ItemId = NewItem->GetEntry();
                        NewItem->DeveLopRank = 0;
                        NewItem->DeveLopXp = 0;

                        NewDeveLopRank = NewItem->DeveLopRank;
                        NewDeveLopXp = NewItem->DeveLopXp;
                    }
                }
                else
                {
                    sItemRandomAtMgr->DoGenerateItemAtForDevelop(DoItem, STempId);
                }
            }

            ItemDevelopCacheTemp St;
            St.ItemId = ItemId;
            St.Rank = NewDeveLopRank;
            St.NowXp = NewDeveLopXp;
            ItemDevelopCacheStore[GuidLow] = St;

            CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_DEVELOP_AT);
            Rtmt->SetData(0, GuidLow);
            Rtmt->SetData(1, ItemId);
            Rtmt->SetData(2, NewDeveLopRank);
            Rtmt->SetData(3, NewDeveLopXp);
            CharacterDatabase.Execute(Rtmt);

            std::string SendMsg;
            SendMsg.append(std::to_string(GuidLow) + "#" + std::to_string(NewDeveLopRank) + "#" + std::to_string(NewDeveLopXp) + "#" + std::to_string(NextRankXp));
            sDoAddon->SendPacketToClient(player, "ClientDevelopData", SendMsg);
        }

        player->DestroyItemCount(UseItemId, 1, true);
        player->SendSysMsg(sHelper->GetGts(7300));

        return true;
    }
};

class PlayerKillDevelop : public PlayerScript
{
public:
    PlayerKillDevelop() : PlayerScript("PlayerKillDevelop") {}

    void OnCreatureKill(Player* player, Creature* Killed) override
    {
        if (DevelopConfigStore.empty())
            return;

        uint32 CreEntry = Killed->GetEntry();

        std::set<Item*> ItemDevelopList = sItemDevelopMgr->AutoPlayerAsInDevelop(player);
        if (ItemDevelopList.empty())
            return;

        bool On = false;
        for (auto Itr = ItemDevelopList.begin(); Itr != ItemDevelopList.end(); ++Itr)
        {
            Item* DoItem = *Itr;
            if (!DoItem)
                continue;

            uint32 GuidLow = DoItem->GetGUIDLow();
            uint32 ItemId = DoItem->GetEntry();
            uint32 NowXp = DoItem->DeveLopXp;
            uint32 NextRank = DoItem->DeveLopRank + 1;
            uint32 NextRankXp = sItemDevelopMgr->GetDevelopNextRankXp(ItemId, NextRank);
            if (!NextRankXp)
                continue;

            for (auto It = DevelopConfigStore.begin(); It != DevelopConfigStore.end(); ++It)
            {
                if (It->ItemId == ItemId && NextRank == It->Rank)
                {
                    uint32 NewXP = 0;
                    switch (It->UpRankType)
                    {
                    case 1:
                    {
                        if (!DoItem->IsEquipped())
                        {
                            if (It->MapType < 0)
                            {
                                if (sAsCreature->IsCustomBoss(Killed))
                                    NewXP += urand(It->KillXpMin2, It->KillXpMax2);
                                else
                                    NewXP += urand(It->KillXpMin1, It->KillXpMax1);
                            }
                            else
                            {
                                switch (It->MapType)
                                {
                                case 1:
                                {
                                    if (It->MapEntry == Killed->GetMapId())
                                    {
                                        if (sAsCreature->IsCustomBoss(Killed))
                                            NewXP += urand(It->KillXpMin2, It->KillXpMax2);
                                        else
                                            NewXP += urand(It->KillXpMin1, It->KillXpMax1);
                                    }
                                }
                                break;
                                case 2:
                                {
                                    if (It->MapEntry == Killed->GetZoneId())
                                    {
                                        if (sAsCreature->IsCustomBoss(Killed))
                                            NewXP += urand(It->KillXpMin2, It->KillXpMax2);
                                        else
                                            NewXP += urand(It->KillXpMin1, It->KillXpMax1);
                                    }
                                }
                                break;
                                case 3:
                                {
                                    if (It->MapEntry == Killed->GetAreaId())
                                    {
                                        if (sAsCreature->IsCustomBoss(Killed))
                                            NewXP += urand(It->KillXpMin2, It->KillXpMax2);
                                        else
                                            NewXP += urand(It->KillXpMin1, It->KillXpMax1);
                                    }
                                }
                                break;
                                }
                            }
                        }
                    }
                    break;
                    case 2:
                    {
                        if (DoItem->IsEquipped())
                        {
                            if (It->MapType < 0)
                            {
                                if (sAsCreature->IsCustomBoss(Killed))
                                    NewXP += urand(It->KillXpMin2, It->KillXpMax2);
                                else
                                    NewXP += urand(It->KillXpMin1, It->KillXpMax1);
                            }
                            else
                            {
                                switch (It->MapType)
                                {
                                case 1:
                                {
                                    if (It->MapEntry == Killed->GetMapId())
                                    {
                                        if (sAsCreature->IsCustomBoss(Killed))
                                            NewXP += urand(It->KillXpMin2, It->KillXpMax2);
                                        else
                                            NewXP += urand(It->KillXpMin1, It->KillXpMax1);
                                    }
                                }
                                break;
                                case 2:
                                {
                                    if (It->MapEntry == Killed->GetZoneId())
                                    {
                                        if (sAsCreature->IsCustomBoss(Killed))
                                            NewXP += urand(It->KillXpMin2, It->KillXpMax2);
                                        else
                                            NewXP += urand(It->KillXpMin1, It->KillXpMax1);
                                    }
                                }
                                break;
                                case 3:
                                {
                                    if (It->MapEntry == Killed->GetAreaId())
                                    {
                                        if (sAsCreature->IsCustomBoss(Killed))
                                            NewXP += urand(It->KillXpMin2, It->KillXpMax2);
                                        else
                                            NewXP += urand(It->KillXpMin1, It->KillXpMax1);
                                    }
                                }
                                break;
                                }
                            }
                        }
                    }
                    break;
                    case 3:
                    {
                        if (It->MapType < 0)
                        {
                            if (sAsCreature->IsCustomBoss(Killed))
                                NewXP += urand(It->KillXpMin2, It->KillXpMax2);
                            else
                                NewXP += urand(It->KillXpMin1, It->KillXpMax1);
                        }
                        else
                        {
                            switch (It->MapType)
                            {
                            case 1:
                            {
                                if (It->MapEntry == Killed->GetMapId())
                                {
                                    if (sAsCreature->IsCustomBoss(Killed))
                                        NewXP += urand(It->KillXpMin2, It->KillXpMax2);
                                    else
                                        NewXP += urand(It->KillXpMin1, It->KillXpMax1);
                                }
                            }
                            break;
                            case 2:
                            {
                                if (It->MapEntry == Killed->GetZoneId())
                                {
                                    if (sAsCreature->IsCustomBoss(Killed))
                                        NewXP += urand(It->KillXpMin2, It->KillXpMax2);
                                    else
                                        NewXP += urand(It->KillXpMin1, It->KillXpMax1);
                                }
                            }
                            break;
                            case 3:
                            {
                                if (It->MapEntry == Killed->GetAreaId())
                                {
                                    if (sAsCreature->IsCustomBoss(Killed))
                                        NewXP += urand(It->KillXpMin2, It->KillXpMax2);
                                    else
                                        NewXP += urand(It->KillXpMin1, It->KillXpMax1);
                                }
                            }
                            break;
                            }
                        }
                    }
                    break;
                    }

                    if (It->ToEntrySets.find(CreEntry) != It->ToEntrySets.end())
                        NewXP += urand(It->KillXpMin3, It->KillXpMax3);

                    if (NewXP)
                    {
                        On = true;
                        NewXP += DoItem->DeveLopXp;
                        while (NewXP >= NextRankXp && NextRankXp)
                        {
                            NewXP -= NextRankXp;
                            DoItem->DeveLopRank += 1;
                            NextRankXp = sItemDevelopMgr->GetDevelopNextRankXp(ItemId, DoItem->DeveLopRank + 1);
                        }

                        uint32 NewDeveLopRank = DoItem->DeveLopRank;
                        uint32 NewDeveLopXp = NewXP;
                        DoItem->DeveLopXp = NewDeveLopXp;

                        if (NewDeveLopRank >= NextRank)
                        {
                            if (It->NewIdType)
                            {
                                player->DestroyItem(DoItem->GetBagSlot(), DoItem->GetSlot(), true);
                                Item* NewItem = player->AddItemByIdOne(It->NewIdType, "装备超级特殊成长获得", true);
                                if (NewItem)
                                {
                                    GuidLow = NewItem->GetGUIDLow();
                                    ItemId = NewItem->GetEntry();
                                    NewItem->DeveLopRank = 0;
                                    NewItem->DeveLopXp = 0;

                                    NewDeveLopRank = NewItem->DeveLopRank;
                                    NewDeveLopXp = NewItem->DeveLopXp;
                                }
                            }
                            else
                            {
                                sItemRandomAtMgr->DoGenerateItemAtForDevelop(DoItem, It->STempId);
                                if (DoItem->IsEquipped())
                                {
                                    player->_ApplyItemMods(DoItem, DoItem->GetSlot(), false);
                                    player->_ApplyItemMods(DoItem, DoItem->GetSlot(), true);
                                }
                            }
                        }

                        ItemDevelopCacheTemp St;
                        St.ItemId = ItemId;
                        St.Rank = NewDeveLopRank;
                        St.NowXp = NewDeveLopXp;
                        ItemDevelopCacheStore[GuidLow] = St;

                        CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_DEVELOP_AT);
                        Rtmt->SetData(0, GuidLow);
                        Rtmt->SetData(1, ItemId);
                        Rtmt->SetData(2, NewDeveLopRank);
                        Rtmt->SetData(3, NewDeveLopXp);
                        CharacterDatabase.Execute(Rtmt);

                        std::string SendMsg;
                        SendMsg.append(std::to_string(GuidLow) + "#" + std::to_string(NewDeveLopRank) + "#" + std::to_string(NewDeveLopXp) + "#" + std::to_string(NextRankXp));
                        sDoAddon->SendPacketToClient(player, "ClientDevelopData", SendMsg);
                    }
                }
            }
        }

        if (On)
            player->SendSysMsg(sHelper->GetGts(7300));
    }
};

void AddSC_DevelopScripts()
{
    new ItemUseDevelop();
    new PlayerKillDevelop();
}
