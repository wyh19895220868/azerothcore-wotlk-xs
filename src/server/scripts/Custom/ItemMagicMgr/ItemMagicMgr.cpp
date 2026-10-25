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
#include "ItemMagicMgr.h"
#include <Time\GameTime.h>
#pragma execution_character_set("utf-8")

ItemMagicMap ItemMagicStore;
TypeMagicMap TypeMagicStore;
ZodiacMap ZodiacStore;
ZodiacCenterMap ZodiacCenterStore;

void ItemMagicMgr::LoadItemMagicData()
{
    ItemMagicStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 物品编号, 法宝类型, 技能编号, 是否主动被动, 是否绑定, 全局限制数量 FROM _玩家法宝技能表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            ItemMagicStruct St;
            uint32 ItemId = Fields[0].Get<uint32>();

            std::string_view TypeStr = Fields[1].Get<std::string_view>();
            const char* Te = TypeStr.data();
            if (Te && *Te != '\0')
            {
                if (strcmp(Te, "金") == 0)
                    St.OType = M_1;
                else if (strcmp(Te, "木") == 0)
                    St.OType = M_2;
                else if (strcmp(Te, "水") == 0)
                    St.OType = M_3;
                else if (strcmp(Te, "火") == 0)
                    St.OType = M_4;
                else if (strcmp(Te, "土") == 0)
                    St.OType = M_5;
                else if (strcmp(Te, "天") == 0)
                    St.OType = M_6;
                else if (strcmp(Te, "地") == 0)
                    St.OType = M_7;
                else if (strcmp(Te, "玄") == 0)
                    St.OType = M_8;
            }

            St.SpellId = Fields[2].Get<uint32>();
            St.IsSpell = Fields[3].Get<bool>();
            St.ToBind = Fields[4].Get<bool>();
            St.MaxCount = Fields[5].Get<uint32>();

            ItemMagicStore.insert(std::make_pair(ItemId, St));
        } while (Result->NextRow());
    }

    TypeMagicStore.clear();
    Result = WorldDatabase.Query("SELECT 法宝类型, 激活光环, 是否主动被动 FROM _玩家法宝类型激活表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();

            MagicTypes OType = M_1;
            std::string_view TypeStr = Fields[0].Get<std::string_view>();
            const char* Te = TypeStr.data();
            if (Te && *Te != '\0')
            {
                if (strcmp(Te, "金") == 0)
                    OType = M_1;
                else if (strcmp(Te, "木") == 0)
                    OType = M_2;
                else if (strcmp(Te, "水") == 0)
                    OType = M_3;
                else if (strcmp(Te, "火") == 0)
                    OType = M_4;
                else if (strcmp(Te, "土") == 0)
                    OType = M_5;
                else if (strcmp(Te, "天") == 0)
                    OType = M_6;
                else if (strcmp(Te, "地") == 0)
                    OType = M_7;
                else if (strcmp(Te, "玄") == 0)
                    OType = M_8;
            }

            TypeMagicTemp St;
            St.FAuraId = Fields[1].Get<uint32>();
            St.IsSpell = Fields[2].Get<bool>();

            TypeMagicStore.insert(std::make_pair(OType, St));
        } while (Result->NextRow());
    }
}

bool ItemMagicMgr::GetToBind(uint32 ItemId)
{
    auto Itr = ItemMagicStore.find(ItemId);
    return Itr != ItemMagicStore.end() ? Itr->second.ToBind : false;
}

uint32 ItemMagicMgr::GetMagicAuraId(uint32 ItemId)
{
    auto Itr = ItemMagicStore.find(ItemId);
    return Itr != ItemMagicStore.end() ? Itr->second.SpellId : 0;
}

std::string ItemMagicMgr::GetTStr(uint32 ItemId)
{
    auto Itr = ItemMagicStore.find(ItemId);
    if (Itr == ItemMagicStore.end())
        return "";

    switch (Itr->second.OType)
    {
    case M_1: return "金";
    case M_2: return "木";
    case M_3: return "水";
    case M_4: return "火";
    case M_5: return "土";
    case M_6: return "天";
    case M_7: return "地";
    case M_8: return "玄";
    }

    return "";
}

uint32 ItemMagicMgr::GetMagicItemMax(uint32 ItemId)
{
    auto Itr = ItemMagicStore.find(ItemId);
    return Itr != ItemMagicStore.end() ? Itr->second.MaxCount : 0;
}

MagicTypes ItemMagicMgr::GetOType(uint32 ItemId)
{
    auto Itr = ItemMagicStore.find(ItemId);
    return Itr != ItemMagicStore.end() ? Itr->second.OType : M_NULL;
}

MagicTypes ItemMagicMgr::AutoForClientGroupId(uint32 GroupId, uint32 Num)
{
    if (GroupId > 6)
    {
        switch (Num)
        {
        case 1: return M_6;
        case 2: return M_8;
        case 3: return M_7;
        }
    }
    else
    {
        switch (GroupId)
        {
        case 1: return M_1;
        case 2: return M_2;
        case 3: return M_4;
        case 4: return M_5;
        case 5: return M_3;
        }
    }

    return M_NULL;
}

void ItemMagicMgr::UpdateMagicOnPlayerHandle(Player* player, MagicTypes Typ, uint32 AtGroup, uint32 ItemId, uint32 Num, bool Add)
{
    uint32 AuraId = GetMagicAuraId(ItemId);
    uint32 Index = Num - 1;
    if (Add)
    {
        if (Typ == M_1)
        {
            uint32 OldItem = player->MagicGroup1[Index];
            if (OldItem)
            {
                player->SendSysMsg(sHelper->GetGts(6103));
                return;
            }

            player->DestroyItemCount(ItemId, 1, true);
            if (IsSpellForItem(ItemId))
            {
                if (!player->HasSpell(AuraId))
                    player->learnSpell(AuraId);
            }
            else
            {
                if (!player->HasAura(AuraId))
                    player->AddAura(AuraId, player);
            }

            player->MagicGroup1[Index] = ItemId;
            sDoAddon->SendPacketToClient(player, "NowYeslItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num) + "#" + std::to_string(AuraId) + "#" + std::to_string(ItemId));
            RepOrDeleteItemMagic(player->GetGUIDLow(), ItemId, Num, AtGroup, Typ, true);

            bool As = true;
            for (uint8 i = 0; i < 5; i++)
            {
                uint32 ItemId = player->MagicGroup1[i];
                if (!ItemId)
                    As = false;
            }

            if (As)
            {
                uint32 FAuraId = GetMagicFitAuraId(Typ);
                if (IsSpellForOType(Typ))
                {
                    if (!player->HasSpell(FAuraId))
                        player->learnSpell(FAuraId);
                }
                else
                {
                    if (!player->HasAura(FAuraId))
                        player->AddAura(FAuraId, player);
                }

                player->MagicActivate[0] = FAuraId;
                sDoAddon->SendPacketToClient(player, "NowYeslItemMagic", std::to_string(6) + "#" + std::to_string(1) + "#" + std::to_string(FAuraId));
                RepOrDeleteItemMagic(player->GetGUIDLow(), FAuraId, 1, 6, Typ, true);
                player->SendSysMsg(sHelper->GetGts(6106), sHelper->GetSpellName(FAuraId));
            }
        }
        else if(Typ == M_2)
        {
            uint32 OldItem = player->MagicGroup2[Index];
            if (OldItem)
            {
                player->SendSysMsg(sHelper->GetGts(6103));
                return;
            }

            player->DestroyItemCount(ItemId, 1, true);
            if (IsSpellForItem(ItemId))
            {
                if (!player->HasSpell(AuraId))
                    player->learnSpell(AuraId);
            }
            else
            {
                if (!player->HasAura(AuraId))
                    player->AddAura(AuraId, player);
            }

            player->MagicGroup2[Index] = ItemId;
            sDoAddon->SendPacketToClient(player, "NowYeslItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num) + "#" + std::to_string(AuraId) + "#" + std::to_string(ItemId));
            RepOrDeleteItemMagic(player->GetGUIDLow(), ItemId, Num, AtGroup, Typ, true);

            bool As = true;
            for (uint8 i = 0; i < 5; i++)
            {
                uint32 ItemId = player->MagicGroup2[i];
                if (!ItemId)
                    As = false;
            }

            if (As)
            {
                uint32 FAuraId = GetMagicFitAuraId(Typ);
                if (IsSpellForOType(Typ))
                {
                    if (!player->HasSpell(FAuraId))
                        player->learnSpell(FAuraId);
                }
                else
                {
                    if (!player->HasAura(FAuraId))
                        player->AddAura(FAuraId, player);
                }

                player->MagicActivate[1] = FAuraId;
                sDoAddon->SendPacketToClient(player, "NowYeslItemMagic", std::to_string(6) + "#" + std::to_string(2) + "#" + std::to_string(FAuraId));
                RepOrDeleteItemMagic(player->GetGUIDLow(), FAuraId, 2, 6, Typ, true);
                player->SendSysMsg(sHelper->GetGts(6106), sHelper->GetSpellName(FAuraId));
            }
        }
        else if(Typ == M_3)
        {
            uint32 OldItem = player->MagicGroup5[Index];
            if (OldItem)
            {
                player->SendSysMsg(sHelper->GetGts(6103));
                return;
            }

            player->DestroyItemCount(ItemId, 1, true);
            if (IsSpellForItem(ItemId))
            {
                if (!player->HasSpell(AuraId))
                    player->learnSpell(AuraId);
            }
            else
            {
                if (!player->HasAura(AuraId))
                    player->AddAura(AuraId, player);
            }

            player->MagicGroup5[Index] = ItemId;
            sDoAddon->SendPacketToClient(player, "NowYeslItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num) + "#" + std::to_string(AuraId) + "#" + std::to_string(ItemId));
            RepOrDeleteItemMagic(player->GetGUIDLow(), ItemId, Num, AtGroup, Typ, true);

            bool As = true;
            for (uint8 i = 0; i < 5; i++)
            {
                uint32 ItemId = player->MagicGroup5[i];
                if (!ItemId)
                    As = false;
            }

            if (As)
            {
                uint32 FAuraId = GetMagicFitAuraId(Typ);
                if (IsSpellForOType(Typ))
                {
                    if (!player->HasSpell(FAuraId))
                        player->learnSpell(FAuraId);
                }
                else
                {
                    if (!player->HasAura(FAuraId))
                        player->AddAura(FAuraId, player);
                }

                player->MagicActivate[2] = FAuraId;
                sDoAddon->SendPacketToClient(player, "NowYeslItemMagic", std::to_string(6) + "#" + std::to_string(5) + "#" + std::to_string(FAuraId));
                RepOrDeleteItemMagic(player->GetGUIDLow(), FAuraId, 5, 6, Typ, true);
                player->SendSysMsg(sHelper->GetGts(6106), sHelper->GetSpellName(FAuraId));
            }
        }
        else if(Typ == M_4)
        {
            uint32 OldItem = player->MagicGroup3[Index];
            if (OldItem)
            {
                player->SendSysMsg(sHelper->GetGts(6103));
                return;
            }

            player->DestroyItemCount(ItemId, 1, true);
            if (IsSpellForItem(ItemId))
            {
                if (!player->HasSpell(AuraId))
                    player->learnSpell(AuraId);
            }
            else
            {
                if (!player->HasAura(AuraId))
                    player->AddAura(AuraId, player);
            }

            player->MagicGroup3[Index] = ItemId;
            sDoAddon->SendPacketToClient(player, "NowYeslItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num) + "#" + std::to_string(AuraId) + "#" + std::to_string(ItemId));
            RepOrDeleteItemMagic(player->GetGUIDLow(), ItemId, Num, AtGroup, Typ, true);

            bool As = true;
            for (uint8 i = 0; i < 5; i++)
            {
                uint32 ItemId = player->MagicGroup3[i];
                if (!ItemId)
                    As = false;
            }

            if (As)
            {
                uint32 FAuraId = GetMagicFitAuraId(Typ);
                if (IsSpellForOType(Typ))
                {
                    if (!player->HasSpell(FAuraId))
                        player->learnSpell(FAuraId);
                }
                else
                {
                    if (!player->HasAura(FAuraId))
                        player->AddAura(FAuraId, player);
                }

                player->MagicActivate[3] = FAuraId;
                sDoAddon->SendPacketToClient(player, "NowYeslItemMagic", std::to_string(6) + "#" + std::to_string(3) + "#" + std::to_string(FAuraId));
                RepOrDeleteItemMagic(player->GetGUIDLow(), FAuraId, 3, 6, Typ, true);
                player->SendSysMsg(sHelper->GetGts(6106), sHelper->GetSpellName(FAuraId));
            }
        }
        else if(Typ == M_5)
        {
            uint32 OldItem = player->MagicGroup4[Index];
            if (OldItem)
            {
                player->SendSysMsg(sHelper->GetGts(6103));
                return;
            }

            player->DestroyItemCount(ItemId, 1, true);
            if (IsSpellForItem(ItemId))
            {
                if (!player->HasSpell(AuraId))
                    player->learnSpell(AuraId);
            }
            else
            {
                if (!player->HasAura(AuraId))
                    player->AddAura(AuraId, player);
            }

            player->MagicGroup4[Index] = ItemId;
            sDoAddon->SendPacketToClient(player, "NowYeslItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num) + "#" + std::to_string(AuraId) + "#" + std::to_string(ItemId));
            RepOrDeleteItemMagic(player->GetGUIDLow(), ItemId, Num, AtGroup, Typ, true);

            bool As = true;
            for (uint8 i = 0; i < 5; i++)
            {
                uint32 ItemId = player->MagicGroup4[i];
                if (!ItemId)
                    As = false;
            }

            if (As)
            {
                uint32 FAuraId = GetMagicFitAuraId(Typ);
                if (IsSpellForOType(Typ))
                {
                    if (!player->HasSpell(FAuraId))
                        player->learnSpell(FAuraId);
                }
                else
                {
                    if (!player->HasAura(FAuraId))
                        player->AddAura(FAuraId, player);
                }

                player->MagicActivate[4] = FAuraId;
                sDoAddon->SendPacketToClient(player, "NowYeslItemMagic", std::to_string(6) + "#" + std::to_string(4) + "#" + std::to_string(FAuraId));
                RepOrDeleteItemMagic(player->GetGUIDLow(), FAuraId, 4, 6, Typ, true);
                player->SendSysMsg(sHelper->GetGts(6106), sHelper->GetSpellName(FAuraId));
            }
        }
        else if (Typ == M_6 || Typ == M_7 || Typ == M_8)
        {
            uint32 OldItem = player->MagicCentre[Index];
            if (OldItem)
            {
                player->SendSysMsg(sHelper->GetGts(6103));
                return;
            }

            bool As = true;
            for (uint8 i = 0; i < 5; i++)
            {
                uint32 FAuraId = player->MagicActivate[i];
                if (!FAuraId)
                    As = false;
            }

            if (!As)
            {
                player->SendSysMsg(sHelper->GetGts(6108));
                return;
            }

            player->DestroyItemCount(ItemId, 1, true);
            if (IsSpellForItem(ItemId))
            {
                if (!player->HasSpell(AuraId))
                    player->learnSpell(AuraId);
            }
            else
            {
                if (!player->HasAura(AuraId))
                    player->AddAura(AuraId, player);
            }

            player->MagicCentre[Index] = ItemId;
            sDoAddon->SendPacketToClient(player, "NowYeslItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num) + "#" + std::to_string(AuraId) + "#" + std::to_string(ItemId));
            RepOrDeleteItemMagic(player->GetGUIDLow(), ItemId, Num, AtGroup, Typ, true);
        }
    }
    else
    {
        if (Typ == M_1)
        {
            uint32 OldItem = player->MagicGroup1[Index];
            if (!OldItem)
                return;

            Item* ItemOld = player->AddItemByIdOne(OldItem, "法宝拆除获得");
            if (ItemOld && GetToBind(OldItem))
            {
                ItemOld->UnBinded = false;
                ItemOld->SetBinding(true);
                ItemOld->UpdateTempKt(KT03, 1);
            }

            uint32 OldAuraId = GetMagicAuraId(OldItem);
            if (OldAuraId)
            {
                if (IsSpellForItem(OldItem))
                {
                    if (player->HasSpell(OldAuraId))
                        player->removeSpell(OldAuraId, SPEC_MASK_ALL, false);
                }
                else
                {
                    if (player->HasAura(OldAuraId))
                        player->RemoveAurasDueToSpell(OldAuraId);
                }
            }

            MagicTypes OldOType = GetOType(OldItem);
            player->MagicGroup1[Index] = 0;
            sDoAddon->SendPacketToClient(player, "YesTaskFuncItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num));
            RepOrDeleteItemMagic(player->GetGUIDLow(), OldItem, Num, AtGroup, OldOType, false);

            uint32 FAuraId = GetMagicFitAuraId(OldOType);
            if (FAuraId)
            {
                if (IsSpellForOType(OldOType))
                {
                    if (player->HasSpell(FAuraId))
                        player->removeSpell(FAuraId, SPEC_MASK_ALL, false);
                }
                else
                {
                    if (player->HasAura(FAuraId))
                        player->RemoveAurasDueToSpell(FAuraId);
                }

                player->MagicActivate[0] = 0;
                sDoAddon->SendPacketToClient(player, "YesTaskFuncItemMagic", std::to_string(6) + "#" + std::to_string(1));
                RepOrDeleteItemMagic(player->GetGUIDLow(), FAuraId, 1, 6, OldOType, false);
            }
        }
        else if (Typ == M_2)
        {
            uint32 OldItem = player->MagicGroup2[Index];
            if (!OldItem)
                return;

            Item* ItemOld = player->AddItemByIdOne(OldItem, "法宝拆除获得");
            if (ItemOld && GetToBind(OldItem))
            {
                ItemOld->UnBinded = false;
                ItemOld->SetBinding(true);
                ItemOld->UpdateTempKt(KT03, 1);
            }

            uint32 OldAuraId = GetMagicAuraId(OldItem);
            if (OldAuraId)
            {
                if (IsSpellForItem(OldItem))
                {
                    if (player->HasSpell(OldAuraId))
                        player->removeSpell(OldAuraId, SPEC_MASK_ALL, false);
                }
                else
                {
                    if (player->HasAura(OldAuraId))
                        player->RemoveAurasDueToSpell(OldAuraId);
                }
            }

            MagicTypes OldOType = GetOType(OldItem);
            player->MagicGroup2[Index] = 0;
            sDoAddon->SendPacketToClient(player, "YesTaskFuncItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num));
            RepOrDeleteItemMagic(player->GetGUIDLow(), OldItem, Num, AtGroup, OldOType, false);

            uint32 FAuraId = GetMagicFitAuraId(OldOType);
            if (FAuraId)
            {
                if (IsSpellForOType(OldOType))
                {
                    if (player->HasSpell(FAuraId))
                        player->removeSpell(FAuraId, SPEC_MASK_ALL, false);
                }
                else
                {
                    if (player->HasAura(FAuraId))
                        player->RemoveAurasDueToSpell(FAuraId);
                }

                player->MagicActivate[1] = 0;
                sDoAddon->SendPacketToClient(player, "YesTaskFuncItemMagic", std::to_string(6) + "#" + std::to_string(2));
                RepOrDeleteItemMagic(player->GetGUIDLow(), FAuraId, 2, 6, OldOType, false);
            }
        }
        else if (Typ == M_3)
        {
            uint32 OldItem = player->MagicGroup5[Index];
            if (!OldItem)
                return;

            Item* ItemOld = player->AddItemByIdOne(OldItem, "法宝拆除获得");
            if (ItemOld && GetToBind(OldItem))
            {
                ItemOld->UnBinded = false;
                ItemOld->SetBinding(true);
                ItemOld->UpdateTempKt(KT03, 1);
            }

            uint32 OldAuraId = GetMagicAuraId(OldItem);
            if (OldAuraId)
            {
                if (IsSpellForItem(OldItem))
                {
                    if (player->HasSpell(OldAuraId))
                        player->removeSpell(OldAuraId, SPEC_MASK_ALL, false);
                }
                else
                {
                    if (player->HasAura(OldAuraId))
                        player->RemoveAurasDueToSpell(OldAuraId);
                }
            }

            MagicTypes OldOType = GetOType(OldItem);
            player->MagicGroup5[Index] = 0;
            sDoAddon->SendPacketToClient(player, "YesTaskFuncItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num));
            RepOrDeleteItemMagic(player->GetGUIDLow(), OldItem, Num, AtGroup, OldOType, false);

            uint32 FAuraId = GetMagicFitAuraId(OldOType);
            if (FAuraId)
            {
                if (IsSpellForOType(OldOType))
                {
                    if (player->HasSpell(FAuraId))
                        player->removeSpell(FAuraId, SPEC_MASK_ALL, false);
                }
                else
                {
                    if (player->HasAura(FAuraId))
                        player->RemoveAurasDueToSpell(FAuraId);
                }

                player->MagicActivate[2] = 0;
                sDoAddon->SendPacketToClient(player, "YesTaskFuncItemMagic", std::to_string(6) + "#" + std::to_string(5));
                RepOrDeleteItemMagic(player->GetGUIDLow(), FAuraId, 5, 6, OldOType, false);
            }
        }
        else if (Typ == M_4)
        {
            uint32 OldItem = player->MagicGroup3[Index];
            if (!OldItem)
                return;

            Item* ItemOld = player->AddItemByIdOne(OldItem, "法宝拆除获得");
            if (ItemOld && GetToBind(OldItem))
            {
                ItemOld->UnBinded = false;
                ItemOld->SetBinding(true);
                ItemOld->UpdateTempKt(KT03, 1);
            }

            uint32 OldAuraId = GetMagicAuraId(OldItem);
            if (OldAuraId)
            {
                if (IsSpellForItem(OldItem))
                {
                    if (player->HasSpell(OldAuraId))
                        player->removeSpell(OldAuraId, SPEC_MASK_ALL, false);
                }
                else
                {
                    if (player->HasAura(OldAuraId))
                        player->RemoveAurasDueToSpell(OldAuraId);
                }
            }

            MagicTypes OldOType = GetOType(OldItem);
            player->MagicGroup3[Index] = 0;
            sDoAddon->SendPacketToClient(player, "YesTaskFuncItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num));
            RepOrDeleteItemMagic(player->GetGUIDLow(), OldItem, Num, AtGroup, OldOType, false);

            uint32 FAuraId = GetMagicFitAuraId(OldOType);
            if (FAuraId)
            {
                if (IsSpellForOType(OldOType))
                {
                    if (player->HasSpell(FAuraId))
                        player->removeSpell(FAuraId, SPEC_MASK_ALL, false);
                }
                else
                {
                    if (player->HasAura(FAuraId))
                        player->RemoveAurasDueToSpell(FAuraId);
                }

                player->MagicActivate[3] = 0;
                sDoAddon->SendPacketToClient(player, "YesTaskFuncItemMagic", std::to_string(6) + "#" + std::to_string(3));
                RepOrDeleteItemMagic(player->GetGUIDLow(), FAuraId, 3, 6, OldOType, false);
            }
        }
        else if (Typ == M_5)
        {
            uint32 OldItem = player->MagicGroup4[Index];
            if (!OldItem)
                return;

            Item* ItemOld = player->AddItemByIdOne(OldItem, "法宝拆除获得");
            if (ItemOld && GetToBind(OldItem))
            {
                ItemOld->UnBinded = false;
                ItemOld->SetBinding(true);
                ItemOld->UpdateTempKt(KT03, 1);
            }

            uint32 OldAuraId = GetMagicAuraId(OldItem);
            if (OldAuraId)
            {
                if (IsSpellForItem(OldItem))
                {
                    if (player->HasSpell(OldAuraId))
                        player->removeSpell(OldAuraId, SPEC_MASK_ALL, false);
                }
                else
                {
                    if (player->HasAura(OldAuraId))
                        player->RemoveAurasDueToSpell(OldAuraId);
                }
            }

            MagicTypes OldOType = GetOType(OldItem);
            player->MagicGroup4[Index] = 0;
            sDoAddon->SendPacketToClient(player, "YesTaskFuncItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num));
            RepOrDeleteItemMagic(player->GetGUIDLow(), OldItem, Num, AtGroup, OldOType, false);

            uint32 FAuraId = GetMagicFitAuraId(OldOType);
            if (FAuraId)
            {
                if (IsSpellForOType(OldOType))
                {
                    if (player->HasSpell(FAuraId))
                        player->removeSpell(FAuraId, SPEC_MASK_ALL, false);
                }
                else
                {
                    if (player->HasAura(FAuraId))
                        player->RemoveAurasDueToSpell(FAuraId);
                }

                player->MagicActivate[4] = 0;
                sDoAddon->SendPacketToClient(player, "YesTaskFuncItemMagic", std::to_string(6) + "#" + std::to_string(4));
                RepOrDeleteItemMagic(player->GetGUIDLow(), FAuraId, 4, 6, OldOType, false);
            }
        }
        else if (Typ == M_6 || Typ == M_7 || Typ == M_8)
        {
            uint32 OldItem = player->MagicCentre[Index];
            if (!OldItem)
                return;

            Item* ItemOld = player->AddItemByIdOne(OldItem, "法宝拆除获得");
            if (ItemOld && GetToBind(OldItem))
            {
                ItemOld->UnBinded = false;
                ItemOld->SetBinding(true);
                ItemOld->UpdateTempKt(KT03, 1);
            }

            uint32 OldAuraId = GetMagicAuraId(OldItem);
            if (OldAuraId)
            {
                if (IsSpellForItem(OldItem))
                {
                    if (player->HasSpell(OldAuraId))
                        player->removeSpell(OldAuraId, SPEC_MASK_ALL, false);
                }
                else
                {
                    if (player->HasAura(OldAuraId))
                        player->RemoveAurasDueToSpell(OldAuraId);
                }
            }

            MagicTypes OldOType = GetOType(OldItem);
            player->MagicCentre[Index] = 0;
            sDoAddon->SendPacketToClient(player, "YesTaskFuncItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num));
            RepOrDeleteItemMagic(player->GetGUIDLow(), OldItem, Num, AtGroup, OldOType, false);
        }
    }
}

bool ItemMagicMgr::IsSpellForItem(uint32 ItemId)
{
    auto Itr = ItemMagicStore.find(ItemId);
    return Itr != ItemMagicStore.end() ? Itr->second.IsSpell : false;
}

bool ItemMagicMgr::IsSpellForOType(MagicTypes OType)
{
    auto Itr = TypeMagicStore.find(OType);
    return Itr != TypeMagicStore.end() ? Itr->second.IsSpell : false;
}

uint32 ItemMagicMgr::GetMagicFitAuraId(MagicTypes OType)
{
    auto Itr = TypeMagicStore.find(OType);
    return Itr != TypeMagicStore.end() ? Itr->second.FAuraId : 0;
}

void ItemMagicMgr::RepOrDeleteItemMagic(uint32 PlayerGuidLow, uint32 ItemIdOrAuraId, uint32 Num, uint32 AtGroup, MagicTypes OType, bool Ins)
{
    CharacterDatabaseStatements Statement = Ins ? CHAR_REP_ITEMMAGIC : CHAR_DEL_ITEMMAGIC;
    CharacterDatabasePreparedStatement* Ktmt = CharacterDatabase.GetPreparedStatement(Statement);
    Ktmt->SetData(0, PlayerGuidLow);
    Ktmt->SetData(1, ItemIdOrAuraId);
    Ktmt->SetData(2, uint32(OType));
    Ktmt->SetData(3, Num);
    Ktmt->SetData(4, AtGroup);
    CharacterDatabase.Execute(Ktmt);
}

uint32 ItemMagicMgr::GetPlayerActCountForOType(Player* player, uint32 ItemId)
{
    uint32 Count = 0;

    for (uint8 i = 0; i < 5; i++)
    {
        uint32 ItemId1 = player->MagicGroup1[i];
        if (ItemId1 == ItemId)
            Count++;

        uint32 ItemId2 = player->MagicGroup2[i];
        if (ItemId2 == ItemId)
            Count++;

        uint32 ItemId3 = player->MagicGroup3[i];
        if (ItemId3 == ItemId)
            Count++;

        uint32 ItemId4 = player->MagicGroup4[i];
        if (ItemId4 == ItemId)
            Count++;

        uint32 ItemId5 = player->MagicGroup5[i];
        if (ItemId5 == ItemId)
            Count++;
    }

    for (uint8 i = 0; i < 3; i++)
    {
        uint32 ItemId7 = player->MagicCentre[i];
        if (ItemId7 == ItemId)
            Count++;
    }

    return Count;
}

void ItemMagicMgr::PlayerLoginLoad(Player* player)
{
    if (IsXsPlayerbot(player))
        return;

    CharacterDatabasePreparedStatement* Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEMMAGIC);
    Stmt->SetData(0, player->GetGUIDLow());
    Stmt->SetData(1, 1);
    PreparedQueryResult Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            uint32 Num = Fields[2].Get<uint32>();
            player->MagicGroup1[Num - 1] = ItemId;
            auto Itr = ItemMagicStore.find(ItemId);
            if (Itr != ItemMagicStore.end())
            {
                if (Itr->second.SpellId)
                {
                    if (Itr->second.IsSpell)
                    {
                        if (!player->HasSpell(Itr->second.SpellId))
                            player->learnSpell(Itr->second.SpellId);
                    }
                    else
                    {
                        if (!player->HasAura(Itr->second.SpellId))
                            player->AddAura(Itr->second.SpellId, player);
                    }
                }
            }

        } while (Result->NextRow());
    }

    Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEMMAGIC);
    Stmt->SetData(0, player->GetGUIDLow());
    Stmt->SetData(1, 2);
    Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            uint32 Num = Fields[2].Get<uint32>();
            player->MagicGroup2[Num - 1] = ItemId;
            auto Itr = ItemMagicStore.find(ItemId);
            if (Itr != ItemMagicStore.end())
            {
                if (Itr->second.SpellId)
                {
                    if (Itr->second.IsSpell)
                    {
                        if (!player->HasSpell(Itr->second.SpellId))
                            player->learnSpell(Itr->second.SpellId);
                    }
                    else
                    {
                        if (!player->HasAura(Itr->second.SpellId))
                            player->AddAura(Itr->second.SpellId, player);
                    }
                }
            }

        } while (Result->NextRow());
    }

    Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEMMAGIC);
    Stmt->SetData(0, player->GetGUIDLow());
    Stmt->SetData(1, 3);
    Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            uint32 Num = Fields[2].Get<uint32>();
            player->MagicGroup3[Num - 1] = ItemId;
            auto Itr = ItemMagicStore.find(ItemId);
            if (Itr != ItemMagicStore.end())
            {
                if (Itr->second.SpellId)
                {
                    if (Itr->second.IsSpell)
                    {
                        if (!player->HasSpell(Itr->second.SpellId))
                            player->learnSpell(Itr->second.SpellId);
                    }
                    else
                    {
                        if (!player->HasAura(Itr->second.SpellId))
                            player->AddAura(Itr->second.SpellId, player);
                    }
                }
            }

        } while (Result->NextRow());
    }

    Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEMMAGIC);
    Stmt->SetData(0, player->GetGUIDLow());
    Stmt->SetData(1, 4);
    Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            uint32 Num = Fields[2].Get<uint32>();
            player->MagicGroup4[Num - 1] = ItemId;
            auto Itr = ItemMagicStore.find(ItemId);
            if (Itr != ItemMagicStore.end())
            {
                if (Itr->second.SpellId)
                {
                    if (Itr->second.IsSpell)
                    {
                        if (!player->HasSpell(Itr->second.SpellId))
                            player->learnSpell(Itr->second.SpellId);
                    }
                    else
                    {
                        if (!player->HasAura(Itr->second.SpellId))
                            player->AddAura(Itr->second.SpellId, player);
                    }
                }
            }

        } while (Result->NextRow());
    }

    Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEMMAGIC);
    Stmt->SetData(0, player->GetGUIDLow());
    Stmt->SetData(1, 5);
    Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            uint32 Num = Fields[2].Get<uint32>();
            player->MagicGroup5[Num - 1] = ItemId;
            auto Itr = ItemMagicStore.find(ItemId);
            if (Itr != ItemMagicStore.end())
            {
                if (Itr->second.SpellId)
                {
                    if (Itr->second.IsSpell)
                    {
                        if (!player->HasSpell(Itr->second.SpellId))
                            player->learnSpell(Itr->second.SpellId);
                    }
                    else
                    {
                        if (!player->HasAura(Itr->second.SpellId))
                            player->AddAura(Itr->second.SpellId, player);
                    }
                }
            }

        } while (Result->NextRow());
    }

    Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEMMAGIC);
    Stmt->SetData(0, player->GetGUIDLow());
    Stmt->SetData(1, 6);
    Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 SpellId = Fields[0].Get<uint32>();
            MagicTypes OType = MagicTypes(Fields[1].Get<uint32>());
            uint32 Num = Fields[2].Get<uint32>();
            player->MagicActivate[Num - 1] = SpellId;
            auto Itr = TypeMagicStore.find(OType);
            if (Itr != TypeMagicStore.end())
            {
                if (Itr->second.FAuraId)
                {
                    if (Itr->second.IsSpell)
                    {
                        if (!player->HasSpell(Itr->second.FAuraId))
                            player->learnSpell(Itr->second.FAuraId);
                    }
                    else
                    {
                        if (!player->HasAura(Itr->second.FAuraId))
                            player->AddAura(Itr->second.FAuraId, player);
                    }
                }
            }

        } while (Result->NextRow());
    }

    Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEMMAGIC);
    Stmt->SetData(0, player->GetGUIDLow());
    Stmt->SetData(1, 7);
    Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            uint32 Num = Fields[2].Get<uint32>();
            player->MagicCentre[Num - 1] = ItemId;
            auto Itr = ItemMagicStore.find(ItemId);
            if (Itr != ItemMagicStore.end())
            {
                if (Itr->second.SpellId)
                {
                    if (Itr->second.IsSpell)
                    {
                        if (!player->HasSpell(Itr->second.SpellId))
                            player->learnSpell(Itr->second.SpellId);
                    }
                    else
                    {
                        if (!player->HasAura(Itr->second.SpellId))
                            player->AddAura(Itr->second.SpellId, player);
                    }
                }
            }

        } while (Result->NextRow());
    }
}

void ItemMagicMgr::LoadItemZodiacData()
{
    ZodiacStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 生肖物品编号, 自带光环, 归属位置, 放入需求模版, 取出需求模版 FROM _传奇十二生肖配置表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 UseItemId = Fields[0].Get<uint32>();
            ZodiacTemp St;
            St.ItemAura = Fields[1].Get<uint32>();
            St.Slot = Fields[2].Get<uint32>();
            St.InTempId = Fields[3].Get<uint32>();
            St.OutTempId = Fields[4].Get<uint32>();

            ZodiacStore.insert(std::make_pair(UseItemId, St));
        } while (Result->NextRow());
    }

    ZodiacCenterStore.clear();
    Result = WorldDatabase.Query("SELECT 阶级, 生肖物品1, 生肖物品2, 生肖物品3, 生肖物品4, 生肖物品5, 生肖物品6, 生肖物品7, 生肖物品8, 生肖物品9, 生肖物品10, 生肖物品11, 生肖物品12, 激活光环 FROM _传奇十二生肖激活表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Index = Fields[0].Get<uint32>();
            ZodiacCenterTemp St;
            St.Item1 = Fields[1].Get<uint32>();
            St.Item2 = Fields[2].Get<uint32>();
            St.Item3 = Fields[3].Get<uint32>();
            St.Item4 = Fields[4].Get<uint32>();
            St.Item5 = Fields[5].Get<uint32>();
            St.Item6 = Fields[6].Get<uint32>();
            St.Item7 = Fields[7].Get<uint32>();
            St.Item8 = Fields[8].Get<uint32>();
            St.Item9 = Fields[9].Get<uint32>();
            St.Item10 = Fields[10].Get<uint32>();
            St.Item11 = Fields[11].Get<uint32>();
            St.Item12 = Fields[12].Get<uint32>();
            St.CenterAuraId = Fields[13].Get<uint32>();

            ZodiacCenterStore.insert(std::make_pair(Index, St));
        } while (Result->NextRow());
    }
}

bool ItemMagicMgr::InZodiacStoreData(uint32 ItemId)
{
    auto Itr = ZodiacStore.find(ItemId);
    return Itr != ZodiacStore.end();
}

uint32 ItemMagicMgr::GetZodiacAuraId(uint32 ItemId)
{
    auto Itr = ZodiacStore.find(ItemId);
    return Itr != ZodiacStore.end() ? Itr->second.ItemAura : 0;
}

uint32 ItemMagicMgr::GetZodiacSlot(uint32 ItemId)
{
    auto Itr = ZodiacStore.find(ItemId);
    return Itr != ZodiacStore.end() ? Itr->second.Slot : 0;
}

uint32 ItemMagicMgr::GetZodiacItemId(uint32 AuraId)
{
    uint32 ItemId = 0;
    for (auto Itr = ZodiacStore.begin(); Itr != ZodiacStore.end(); Itr++)
    {
        if (Itr->second.ItemAura == AuraId)
        {
            ItemId = Itr->first;
            break;
        }
    }

    return ItemId;
}

uint32 ItemMagicMgr::GetZodiacTempId(uint32 ItemId, bool In)
{
    auto Itr = ZodiacStore.find(ItemId);
    if (Itr != ZodiacStore.end())
        return In ? Itr->second.InTempId : Itr->second.OutTempId;

    return 0;
}

void ItemMagicMgr::CheckUpdateCenterAsZodiacPlayer(Player* player, bool In)
{
    uint32 CenterAuraId = 0;
    for (auto Itr = ZodiacCenterStore.begin(); Itr != ZodiacCenterStore.end(); Itr++)
    {
        if (GetZodiacAuraId(Itr->second.Item1) == player->GetTempAtUInt32(AT5001)
            && GetZodiacAuraId(Itr->second.Item2) == player->GetTempAtUInt32(AT5002)
            && GetZodiacAuraId(Itr->second.Item3) == player->GetTempAtUInt32(AT5003)
            && GetZodiacAuraId(Itr->second.Item4) == player->GetTempAtUInt32(AT5004)
            && GetZodiacAuraId(Itr->second.Item5) == player->GetTempAtUInt32(AT5005)
            && GetZodiacAuraId(Itr->second.Item6) == player->GetTempAtUInt32(AT5006)
            && GetZodiacAuraId(Itr->second.Item7) == player->GetTempAtUInt32(AT5007)
            && GetZodiacAuraId(Itr->second.Item8) == player->GetTempAtUInt32(AT5008)
            && GetZodiacAuraId(Itr->second.Item9) == player->GetTempAtUInt32(AT5009)
            && GetZodiacAuraId(Itr->second.Item10) == player->GetTempAtUInt32(AT5010)
            && GetZodiacAuraId(Itr->second.Item11) == player->GetTempAtUInt32(AT5011)
            && GetZodiacAuraId(Itr->second.Item12) == player->GetTempAtUInt32(AT5012)
        )
        {
            CenterAuraId = Itr->second.CenterAuraId;
        }
    }

    if (CenterAuraId)
    {
        if (In)
        {
            if (!player->HasAura(CenterAuraId))
                player->AddAura(CenterAuraId, player);

            sDoAddon->SendPacketToClient(player, "UpdateCenterZodiacItemData", std::to_string(CenterAuraId));
            player->UpdateTempAt(AT5013, CenterAuraId);
            player->SendSysMsg(sHelper->GetGts(8003), sHelper->GetSpellName(CenterAuraId));
        }
        else
        {
            if (player->HasAura(CenterAuraId))
                player->RemoveAurasDueToSpell(CenterAuraId);

            sDoAddon->SendPacketToClient(player, "UpdateCenterZodiacItemData", "0");
            player->UpdateTempAt(AT5013, 0);
        }
    }
}

//生肖
void ItemMagicMgr::PlayerLoginLoadZodiac(Player* player)
{
    if (IsXsPlayerbot(player))
        return;

    for (uint32 i = AT5001; i < ATZCMAX; i++)
    {
        if (uint32 AuraId = player->TempAtStore[i])
        {
            if (!player->HasAura(AuraId))
                player->AddAura(AuraId, player);
        }
    }
}

class OnItemMagicPlayer : public PlayerScript
{
public:
    OnItemMagicPlayer() : PlayerScript("OnItemMagicPlayer") {  }

    void OnLogin(Player* player) override
    {
        sItemMagicMgr->PlayerLoginLoad(player);
        sItemMagicMgr->PlayerLoginLoadZodiac(player);
    }
};

void AddSC_ItemMagicPlayerScript()
{
    new OnItemMagicPlayer();
}
