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
#include "EnterFBMgr.h"
#include <Custom/EventMgr/EventMgr.h>
#pragma execution_character_set("utf-8")

EnterFBMap EnterFBStore;
ResetFBMap ResetFBStore;

void EnterFBMgr::LoadEnterFBMgr()
{
    EnterFBStore.clear();
    uint32 Ct = 0;
    QueryResult Result = WorldDatabase.Query("SELECT 副本地图编号, 难度, 通行证, 需求模版, 需要装等, 排除类型 FROM _进入副本控制表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 MapId = Fields[0].Get<uint32>();
            EnterFBInfo St;
            St.Diff = Fields[1].Get<int32>();
            Tokenizer Tokens(Fields[2].Get<std::string>(), '#');
            for (Tokenizer::const_iterator Itr = Tokens.begin(); Itr != Tokens.end(); ++Itr)
                St.CItems.push_back(uint32(atol(*Itr)));

            St.NTempId = Fields[3].Get<uint32>();
            St.AsItemLevel = Fields[4].Get<uint32>();
            St.NotType = Fields[5].Get<std::string>();
            EnterFBStore.insert(std::make_pair(MapId, St));
            ++Ct;
        } while (Result->NextRow());
    }

    ResetFBStore.clear();
    Ct = 0;
    Result = WorldDatabase.Query("SELECT 序号, 菜单号, 菜单文本, 副本地图编号, 副本难度, 需求模版, 重置次数 FROM _副本重置;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            ResetFBInfo St;
            St.Num = Fields[0].Get<int32>();
            St.Menu = Fields[1].Get<int32>();
            St.Gossip = Fields[2].Get<std::string>();
            St.MapId = Fields[3].Get<int32>();
            St.Diff = Fields[4].Get<int32>();
            St.NTempId = Fields[5].Get<uint32>();
            St.Count = Fields[6].Get<uint32>();
            ResetFBStore.insert(std::make_pair(St.Num, St));
            ++Ct;
        } while (Result->NextRow());
    }
}

void EnterFBMgr::PullPlayerItemLevel(Player* player, uint32 MapId, bool IsRaid)
{
    auto Itr = EnterFBStore.find(MapId);
    if (Itr == EnterFBStore.end())
        return;

    int32 Difficulty = int32(player->GetDifficulty(IsRaid));
    uint32 AsLevel = 0;

    player->SendSysMsg(sHelper->GetGts(7501), sHelper->GetMapName(MapId));

    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            ItemTemplate const* Template = pItem->GetTemplate();
            if (!Template)
                continue;
            if (i == EQUIPMENT_SLOT_OFFHAND)
            {
                player->SendSysMsg(sHelper->GetGts(7502), sHelper->GetItemSlotName(i).c_str());
                continue;
            }
            if (InNotType(i, Itr->second.NotType))
            {
                player->SendSysMsg(sHelper->GetGts(7502), sHelper->GetItemSlotName(i).c_str());
                continue;
            }

            AsLevel += Template->ItemLevel;
            player->SendSysMsg(sHelper->GetGts(7503), sHelper->GetItemSlotName(i).c_str(), Template->ItemLevel);
        }
    }

    player->SendSysMsg(sHelper->GetGts(7504), AsLevel, GetAsLevel(MapId, Difficulty));
}

uint32 EnterFBMgr::GetAsLevel(uint32 MapId, int32 Diff)
{
    auto Itr = EnterFBStore.find(MapId);
    if (Itr == EnterFBStore.end())
        return 0;

    for (int i = 0; i < EnterFBStore.count(MapId); i++, Itr++)
    {
        if (Itr->second.Diff < 0 || Itr->second.Diff == Diff)
            return Itr->second.AsItemLevel;
    }

    return 0;
}

bool EnterFBMgr::InNotType(uint32 InventoryType, std::string NotType)
{
    bool InNot = false;
    if (!NotType.empty())
    {
        Tokenizer Store(NotType, '#');
        if (!Store.IsEmpty())
        {
            for (auto It = Store.begin(); It != Store.end(); ++It)
            {
                uint32 DoType = uint32(atoi(*It));
                if (DoType == InventoryType)
                    InNot = true;
            }
        }
    }
    return InNot;
}

bool EnterFBMgr::IsDoCheckItem(Player* player, uint32 MapId, bool IsRaid)
{
    auto Itr = EnterFBStore.find(MapId);
    if (Itr == EnterFBStore.end())
        return true;

    int32 Difficulty = int32(player->GetDifficulty(IsRaid));

    for (int i = 0; i < EnterFBStore.count(MapId); i++, Itr++)
    {
        if (Itr->second.Diff < 0 || Itr->second.Diff == Difficulty)
        {
            for (auto It = Itr->second.CItems.begin(); It != Itr->second.CItems.end(); It++)
            {
                if (*It > 0 && !player->HasItemCount(*It))
                {
                    player->SendAsMsg(sHelper->GetGts(6746), sHelper->GetItemLink(*It).c_str());
                    return false;
                }
            }
        }
    }

    return true;
}

bool EnterFBMgr::IsDoItemLevel(Player* player, uint32 MapId, bool IsRaid)
{
    auto Itr = EnterFBStore.find(MapId);
    if (Itr == EnterFBStore.end())
        return true;

    uint32 Difficulty = uint32(player->GetDifficulty(IsRaid));
    uint32 AsLevel = 0;
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            ItemTemplate const* Template = pItem->GetTemplate();
            if (!Template)
                continue;
            if (i == EQUIPMENT_SLOT_OFFHAND)
                continue;
            if (InNotType(i, Itr->second.NotType))
                continue;
            AsLevel += Template->ItemLevel;
        }
    }

    for (int i = 0; i < EnterFBStore.count(MapId); i++, Itr++)
    {
        if (Itr->second.Diff < 0 || Itr->second.Diff == Difficulty)
            return (AsLevel >= Itr->second.AsItemLevel);
    }

    return true;
}

uint32 EnterFBMgr::GetTempId(uint32 MapId, int32 Diff)
{
    auto Itr = EnterFBStore.find(MapId);
    if (Itr == EnterFBStore.end())
        return 0;

    for (int i = 0; i < EnterFBStore.count(MapId); i++, Itr++)
    {
        if (Itr->second.Diff < 0 || Itr->second.Diff == Diff)
            return Itr->second.NTempId;
    }

    return 0;
}

uint32 EnterFBMgr::GetCount(uint32 MapId, uint32 Diff)
{
    uint32 Count = 0;
    for (auto It = ResetFBStore.begin(); It != ResetFBStore.end(); ++It)
    {
        if (It->second.MapId == MapId && It->second.Diff == Diff)
            Count = It->second.Count;
    }

    return Count;
}
