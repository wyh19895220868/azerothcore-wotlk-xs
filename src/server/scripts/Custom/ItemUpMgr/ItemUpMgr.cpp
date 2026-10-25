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
#include "ItemUpMgr.h"
#include <Custom/EventMgr/EventMgr.h>
#include <Custom/PlayerModMgr/PlayerModMgr.h>
#include <Custom\ItemRandomAtMgr\ItemRandomAtMgr.h>
#pragma execution_character_set("utf-8")

MUpMap MUpStore;

void ItemUpMgr::LoadItemUpData()
{
    MUpStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 原物品编号, 新物品编号, 合成模式, 需求模版, 是否保留附魔宝石, 是否保留超级武器伤害, 是否保留超级属性, 是否保留超级技能, 成功率, 幸运宝石, 幸运几率, 幸运数量, 防爆宝石, 是否碎裂 FROM _物品普通升级合成表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            MUpTemp St;
            St.NewId = Fields[1].Get<uint32>();
            St.MType = Fields[2].Get<uint32>();
            St.TempId = Fields[3].Get<uint32>();
            St.OnSave = Fields[4].Get<bool>();
            St.OnWp = Fields[5].Get<bool>();
            St.OnSt = Fields[6].Get<bool>();
            St.OnSp = Fields[7].Get<bool>();
            St.Chance = Fields[8].Get<float>();
            St.XyItem = Fields[9].Get<uint32>();
            St.XyOdds = Fields[10].Get<float>();
            St.XyCount = Fields[11].Get<uint32>();
            St.FBItem = Fields[12].Get<uint32>();
            St.Flag = Fields[13].Get<bool>();
            MUpStore.insert(std::make_pair(ItemId, St));
        } while (Result->NextRow());
    }
}

bool ItemUpMgr::InItemUpData(uint32 ItemId)
{
    auto Itr = MUpStore.find(ItemId);
    return Itr != MUpStore.end();
}

class ItemMUp : public ItemScript
{
public:
    ItemMUp() : ItemScript("ItemMUp") {}

    void ShowItemUpNowGossipHello(Player* player, Item* OnItem, uint32 TarItemGuid)
    {
        ClearGossipMenuFor(player);
        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, TarItemGuid)))
        {
            uint32 TarItemId = TargetItem->GetEntry();
            auto Itr = MUpStore.find(TarItemId);
            if (Itr == MUpStore.end())
            {
                player->SendSysMsg(sHelper->GetGts(6201));
                return;
            }

            if (TargetItem->IsEquipped())
            {
                player->SendSysMsg(sHelper->GetGts(1085));
                return;
            }

            if (TargetItem->GetCount() > 1)
            {
                player->SendSysMsg(sHelper->GetGts(1060));
                return;
            }

            if (TargetItem->IsBag() && TargetItem->ToBag() && !TargetItem->ToBag()->IsEmpty())
            {
                player->SendSysMsg("不能升级非空的背包......");
                return;
            }

            std::string TItemIconLink = sHelper->GetItemIcon(TarItemId, 16, 16, 0, 0) + sHelper->GetItemLink(TarItemId);
            std::string NItemIconLink = sHelper->GetItemIcon(Itr->second.NewId, 16, 16, 0, 0) + sHelper->GetItemLink(Itr->second.NewId);
            std::string UChance = sHelper->ToExString(Itr->second.Chance);
            std::string VipChance = sHelper->ToExString(sPlayerModMgr->GetVipUpOdds(player->GetVipRank()));
            uint32 XyCount = player->GetItemCount(Itr->second.XyItem);
            XyCount = XyCount > Itr->second.XyCount ? Itr->second.XyCount : XyCount;
            std::string XyOdds = sHelper->ToExString(Itr->second.XyOdds * XyCount);

            AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(7041), TItemIconLink), 0, TarItemGuid);
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(7042), NItemIconLink), 0, TarItemGuid);
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(7043), UChance, XyOdds, VipChance), 0, TarItemGuid);
            if (Itr->second.OnSave)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(7044)), 0, TarItemGuid);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(7045)), 0, TarItemGuid);

            if (Itr->second.OnWp)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(7149)), 0, TarItemGuid);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(7148)), 0, TarItemGuid);

            if (Itr->second.OnSt)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(7145)), 0, TarItemGuid);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(7144)), 0, TarItemGuid);

            if (Itr->second.OnSp)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(7147)), 0, TarItemGuid);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(7146)), 0, TarItemGuid);

            std::ostringstream MtStr;
            if (Itr->second.MType == 0)
                MtStr << Acore::StringFormat(sHelper->GetGts(7046));
            else
                MtStr << Acore::StringFormat(sHelper->GetGts(7047), Itr->second.MType, TItemIconLink, Itr->second.MType);

            AddGossipItemFor(player, GOSSIP_ICON_TALK, MtStr.str(), 0, TarItemGuid);
            if (Itr->second.TempId)
            {
                std::string Gts = sHelper->GetNeedItemUpGts(Itr->second.TempId);
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Gts, 0, TarItemGuid);
            }
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(7048)), TarItemId, TarItemGuid);
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, OnItem->GetGUID());
        }
    }

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        uint32 TarItemId = TargetItem->GetEntry();
        uint32 TarItemGuid = TargetItem->GetGUIDLow();

        auto Itr = MUpStore.find(TarItemId);
        if (Itr == MUpStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(6201));
            return false;
        }

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        if (TargetItem->GetCount() > 1)
        {
            player->SendSysMsg(sHelper->GetGts(1060));
            return false;
        }

        if (TargetItem->IsBag() && TargetItem->ToBag() && !TargetItem->ToBag()->IsEmpty())
        {
            player->SendSysMsg("不能升级非空的背包......");
            return false;
        }

        ShowItemUpNowGossipHello(player, CastItem, TarItemGuid);
        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        if (!Sender)
        {
            ShowItemUpNowGossipHello(player, OnItem, Action);
            return;
        }

        auto Itr = MUpStore.find(Sender);
        if (Itr != MUpStore.end())
        {
            if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, Action)))
            {
                if (!sHelper->IsStoreNewBag(player, Itr->second.NewId))
                {
                    CloseGossipMenuFor(player);
                    return;
                }

                uint32 TarItemId = TargetItem->GetEntry();
                if (Itr->second.MType > 0 && !player->HasItemCount(TarItemId, Itr->second.MType))
                {
                    player->SendSysMsg(sHelper->GetGts(6205), sHelper->GetItemLink(TarItemId).c_str(), Itr->second.MType);
                    CloseGossipMenuFor(player);
                    return;
                }

                if (sHelper->CheckNeed(player, Itr->second.TempId, "物品升级合成消耗"))
                {
                    uint32 XyCount = player->GetItemCount(Itr->second.XyItem);
                    XyCount = XyCount > Itr->second.XyCount ? Itr->second.XyCount : XyCount;

                    if (Itr->second.XyItem > 0)
                        player->DestroyItemCount(Itr->second.XyItem, XyCount, true);

                    if (roll_chance_f(Itr->second.Chance + Itr->second.XyOdds * XyCount + sPlayerModMgr->GetVipUpOdds(player->GetVipRank())))
                    {
                        if (Itr->second.OnSave)
                        {
                            Item* NewItem = player->AddNewSuperItemSaveRandomSock(TargetItem, Itr->second.NewId);
                            if (NewItem && sItemRandomAtMgr->IsGenerateItemAt(NewItem->GetEntry()))
                            {
                                bool In = false;

                                if (Itr->second.OnWp)
                                {
                                    NewItem->MinWDmg = TargetItem->MinWDmg;
                                    NewItem->MaxWDmg = TargetItem->MaxWDmg;
                                    NewItem->AsMinWDmg = TargetItem->AsMinWDmg;
                                    NewItem->AsMaxWDmg = TargetItem->AsMaxWDmg;

                                    In = true;
                                }

                                if (Itr->second.OnSt)
                                {
                                    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
                                    {
                                        NewItem->RmAtStatStore[i].first = TargetItem->RmAtStatStore[i].first;
                                        NewItem->RmAtStatStore[i].second = TargetItem->RmAtStatStore[i].second;
                                    }
                                    In = true;
                                }

                                if (Itr->second.OnSp)
                                {
                                    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
                                        NewItem->RmAtSpell[i] = TargetItem->RmAtSpell[i];

                                    In = true;
                                }

                                if (In)
                                {
                                    NewItem->RmAtLevel = TargetItem->RmAtLevel;
                                    NewItem->Prefix = TargetItem->Prefix;
                                    NewItem->Suffix = TargetItem->Suffix;
                                    NewItem->LevelStr = TargetItem->LevelStr;

                                    sItemRandomAtMgr->DoSaveDBItemAtAndUpdate(NewItem);
                                }
                            }
                        }
                        else
                        {
                            Item* NewItem = player->AddItemByIdOne(Itr->second.NewId, "物品升级合成功能获得");
                            if (NewItem && sItemRandomAtMgr->IsGenerateItemAt(NewItem->GetEntry()))
                            {
                                bool In = false;

                                if (Itr->second.OnWp)
                                {
                                    NewItem->MinWDmg = TargetItem->MinWDmg;
                                    NewItem->MaxWDmg = TargetItem->MaxWDmg;
                                    NewItem->AsMinWDmg = TargetItem->AsMinWDmg;
                                    NewItem->AsMaxWDmg = TargetItem->AsMaxWDmg;

                                    In = true;
                                }

                                if (Itr->second.OnSt)
                                {
                                    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
                                    {
                                        NewItem->RmAtStatStore[i].first = TargetItem->RmAtStatStore[i].first;
                                        NewItem->RmAtStatStore[i].second = TargetItem->RmAtStatStore[i].second;
                                    }
                                    In = true;
                                }

                                if (Itr->second.OnSp)
                                {
                                    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
                                        NewItem->RmAtSpell[i] = TargetItem->RmAtSpell[i];

                                    In = true;
                                }

                                if (In)
                                {
                                    NewItem->RmAtLevel = TargetItem->RmAtLevel;
                                    NewItem->Prefix = TargetItem->Prefix;
                                    NewItem->Suffix = TargetItem->Suffix;
                                    NewItem->LevelStr = TargetItem->LevelStr;

                                    sItemRandomAtMgr->DoSaveDBItemAtAndUpdate(NewItem);
                                }
                            }
                        }

                        player->DestroyItem(TargetItem->GetBagSlot(), TargetItem->GetSlot(), true);
                        player->SendSysMsg(sHelper->GetGts(6202), sHelper->GetItemLink(Itr->second.NewId).c_str());
                    }
                    else
                    {
                        if (Itr->second.Flag)
                        {
                            if (Itr->second.FBItem > 0)
                            {
                                if (!player->HasItemCount(Itr->second.FBItem))
                                {
                                    player->DestroyItem(TargetItem->GetBagSlot(), TargetItem->GetSlot(), true);

                                    player->SendSysMsg(sHelper->GetGts(6203));
                                }
                                else
                                {
                                    player->SendSysMsg(sHelper->GetGts(6204));
                                }

                                player->DestroyItemCount(Itr->second.FBItem, 1, true);
                            }
                            else
                            {
                                player->SendSysMsg(sHelper->GetGts(6215));
                            }
                        }
                        else
                        {
                            player->SendSysMsg(sHelper->GetGts(6215));
                        }
                    }

                    if (Itr->second.MType > 1)
                        player->DestroyItemCount(TarItemId, Itr->second.MType - 1, true);
                }
            }
        }

        CloseGossipMenuFor(player);
    }
};

void AddSC_CtItemUpScripts()
{
    new ItemMUp();
}
