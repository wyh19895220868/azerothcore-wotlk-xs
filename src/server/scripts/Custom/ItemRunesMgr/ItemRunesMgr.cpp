
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
#include "Bag.h"
#include "Pet.h"
#include <CharacterCache.h>
#include <algorithm>
#include "ItemRunesMgr.h"
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include <Mails/Mail.h>
#include <Loot/LootMgr.h>
#include <Custom\ItemFuncMgr\ItemFuncMgr.h>
#pragma execution_character_set("utf-8")

//_装备符文配置表
AtRunesTempMap AtRunesTempStore;
//_装备符文宝石表
ItemRunesTempMap ItemRunesTempStore;
//_装备符文技能组合表
SpGroupTempVect SpGroupTempStore;
//_装备符文开槽表
ItemASlotRunesTempMap ItemASlotRunesTempStore;

//符文缓存器
ItemRunesCacheMap ItemRunesCacheStore;

static void BindItemRuneArchive(CharacterDatabasePreparedStatement* stmt, Item* item)
{
    stmt->SetData(0, item->GetGUIDLow());
    stmt->SetData(1, item->GetEntry());
    stmt->SetData(2, item->NowCount);
    stmt->SetData(3, item->RunesCount);
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        stmt->SetData(4 + i, item->ResItems[i]);

    uint8 spellBase = static_cast<uint8>(4 + MAX_RUNES_COUNT);
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        stmt->SetData(spellBase + i, item->OSpells[i]);

    uint8 nameIndex = static_cast<uint8>(spellBase + MAX_RUNES_COUNT);
    stmt->SetData(nameIndex, item->GSpellName);
    stmt->SetData(nameIndex + 1, item->GSpellListStr);
}

static void UpdateFrozenCoreRuneCache(Item* DoItem)
{
    if (!DoItem)
        return;

    ItemRunesCacheTemp Cache;
    Cache.ItemId = DoItem->GetEntry();
    Cache.NowCount = DoItem->NowCount;
    Cache.RunesCount = DoItem->RunesCount;
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
    {
        Cache.ResItems[i] = DoItem->ResItems[i];
        Cache.OSpells[i] = DoItem->OSpells[i];
    }
    Cache.GSpellName = DoItem->GSpellName;
    Cache.GSpellListStr = DoItem->GSpellListStr;
    ItemRunesCacheStore[DoItem->GetGUIDLow()] = Cache;
}

ItemRunesMgr::ItemRunesMgr() { }
ItemRunesMgr::~ItemRunesMgr() { }

void ItemRunesMgr::LoadItemRunes()
{
    AtRunesTempStore.clear();
    uint32 Ct = 0;
    QueryResult Result = WorldDatabase.Query("SELECT 装备编号, 符文插槽最少数量, 符文插槽最多数量, 符文组合编号, 最多可开槽 FROM _装备符文配置表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            AtRunesTemp St;
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            St.MinRCount = Fields[1].Get<uint8>();
            St.MaxRCount = Fields[2].Get<uint8>();
            St.GroupId = Fields[3].Get<uint32>();
            St.MaxOpen = Fields[4].Get<uint32>();
            if (St.MaxRCount > MAX_RUNES_COUNT)
                St.MaxRCount = MAX_RUNES_COUNT;
            if (St.MinRCount > St.MaxRCount)
                St.MinRCount = St.MaxRCount;

            if (ItemTemplate* TarInfo = const_cast<ItemTemplate*>(sObjectMgr->GetItemTemplate(ItemId)))
            {
                if (!TarInfo->RequiredLevel || TarInfo->RequiredLevel == 1)
                    TarInfo->RequiredLevel = 2;
            }

            AtRunesTempStore.insert(std::make_pair(ItemId, St));
            ++Ct;
        } while (Result->NextRow());
    }

    Ct = 0;
    ItemRunesTempStore.clear();
    Result = WorldDatabase.Query("SELECT 宝石编号, 自带技能, 主动或被动, 插入该宝石需求模板, 拆除该宝石需求模板, 插入后是否绑定 FROM _装备符文宝石表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            ItemRunesTemp St;
            Field* Fields = Result->Fetch();
            uint32 KItemId = Fields[0].Get<uint32>();
            St.MeSpellId = Fields[1].Get<uint32>();
            St.Casted = Fields[2].Get<bool>();
            St.NTempId1 = Fields[3].Get<uint32>();
            St.NTempId2 = Fields[4].Get<uint32>();
            St.Bind = Fields[5].Get<bool>();

            ItemRunesTempStore.insert(std::make_pair(KItemId, St));
            ++Ct;

        } while (Result->NextRow());
    }

    Ct = 0;
    SpGroupTempStore.clear();
    Result = WorldDatabase.Query("SELECT 组合编号, 序号, 组合效果名称, 符文宝石1, 符文宝石2, 符文宝石3, 符文宝石4, 符文宝石5, 符文宝石6, 符文宝石7, 符文宝石8, 技能列表 FROM _装备符文技能组合表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            SpGroupTemp St;
            Field* Fields = Result->Fetch();
            St.GroupId = Fields[0].Get<uint32>();
            St.Num = Fields[1].Get<uint32>();
            St.GName = Fields[2].Get<std::string>();
            St.ResItem1 = Fields[3].Get<uint32>();
            St.ResItem2 = Fields[4].Get<uint32>();
            St.ResItem3 = Fields[5].Get<uint32>();
            St.ResItem4 = Fields[6].Get<uint32>();
            St.ResItem5 = Fields[7].Get<uint32>();
            St.ResItem6 = Fields[8].Get<uint32>();
            St.ResItem7 = Fields[9].Get<uint32>();
            St.ResItem8 = Fields[10].Get<uint32>();

            if (!St.ResItem1 && !St.ResItem2 && !St.ResItem3 && !St.ResItem4 && !St.ResItem5 && !St.ResItem6 && !St.ResItem7 && !St.ResItem8)
                continue;

            St.SpellListStr = Fields[11].Get<std::string>();
            if (St.SpellListStr.empty())
                continue;

            SpGroupTempStore.emplace_back(St);
            ++Ct;

        } while (Result->NextRow());
    }

    Ct = 0;
    ItemASlotRunesTempStore.clear();
    Result = WorldDatabase.Query("SELECT 装备编号, 需求模板, 开槽成功率 FROM _装备符文开槽表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            ItemASlotRunesTemp St;
            Field* Fields = Result->Fetch();
            uint32 KItemId = Fields[0].Get<uint32>();
            St.NTempId = Fields[1].Get<uint32>();
            St.Chance = Fields[2].Get<float>();

            ItemASlotRunesTempStore.insert(std::make_pair(KItemId, St));
            ++Ct;

        } while (Result->NextRow());
    }
}

bool ItemRunesMgr::IsRunesItem(uint32 ItemId)
{
    auto Itr = ItemRunesTempStore.find(ItemId);
    return (Itr != ItemRunesTempStore.end());
}

bool ItemRunesMgr::IsAtRunesItem(uint32 RunesItem)
{
    auto Itr = AtRunesTempStore.find(RunesItem);
    return (Itr != AtRunesTempStore.end());
}

void ItemRunesMgr::AutoGenerateRandomRunesCount(Item* DoItem)
{
    if (!DoItem->GetOwner())
        return;

    uint32 TarItemId = DoItem->GetEntry();
    auto Itr = AtRunesTempStore.find(TarItemId);
    if (Itr == AtRunesTempStore.end())
        return;

    DoItem->RunesCount = urand(Itr->second.MinRCount, Itr->second.MaxRCount);

    std::string SendMsg;
    SendMsg.clear();

    SendMsg.append(std::to_string(DoItem->GetGUIDLow()) + "$" + std::to_string(DoItem->NowCount) + "$" + std::to_string(DoItem->RunesCount) + "$");

    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        SendMsg.append(std::to_string(DoItem->ResItems[i]) + "#");

    SendMsg.append("$");

    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        SendMsg.append(std::to_string(DoItem->OSpells[i]) + "#");

    SendMsg.append("$nullptr$nullptr$");
    SendMsg.append(std::to_string(DoItem->GetEntry()));

    // Publish the in-memory rune state before the addon packet is sent. The
    // FrozenCore protocol computes the new cache ID at packet time.
    ItemRunesCacheTemp PendingCache;
    PendingCache.ItemId = DoItem->GetEntry();
    PendingCache.NowCount = DoItem->NowCount;
    PendingCache.RunesCount = DoItem->RunesCount;
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
    {
        PendingCache.ResItems[i] = DoItem->ResItems[i];
        PendingCache.OSpells[i] = DoItem->OSpells[i];
    }
    PendingCache.GSpellName = DoItem->GSpellName;
    PendingCache.GSpellListStr = DoItem->GSpellListStr;
    ItemRunesCacheStore[DoItem->GetGUIDLow()] = PendingCache;

    if (DoItem->GetOwner())
        sDoAddon->SendPacketToClient(DoItem->GetOwner(), "ClientAtRunesData", BuildRunesDataPayload(DoItem->GetGUIDLow(), PendingCache));

    SendMsg.clear();

    CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_RUNES_AT);
    BindItemRuneArchive(Utmt, DoItem);
    CharacterDatabase.Execute(Utmt);

    ItemRunesCacheTemp Temp;
    Temp.ItemId = DoItem->GetEntry();
    Temp.NowCount = DoItem->NowCount;
    Temp.RunesCount = DoItem->RunesCount;
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
    {
        Temp.ResItems[i] = DoItem->ResItems[i];
        Temp.OSpells[i] = DoItem->OSpells[i];
    }
    Temp.GSpellName = DoItem->GSpellName;
    Temp.GSpellListStr = DoItem->GSpellListStr;

    ItemRunesCacheStore[DoItem->GetGUIDLow()] = Temp;
}

uint32 ItemRunesMgr::GetAddNtempId(uint32 RunesItem)
{
    auto Itr = ItemRunesTempStore.find(RunesItem);
    return (Itr != ItemRunesTempStore.end()) ? Itr->second.NTempId1 : 0;
}

uint32 ItemRunesMgr::GetDelNtempId(uint32 RunesItem)
{
    auto Itr = ItemRunesTempStore.find(RunesItem);
    return (Itr != ItemRunesTempStore.end()) ? Itr->second.NTempId2 : 0;
}

uint32 ItemRunesMgr::GetOSpellId(uint32 RunesItem)
{
    auto Itr = ItemRunesTempStore.find(RunesItem);
    return (Itr != ItemRunesTempStore.end()) ? Itr->second.MeSpellId : 0;
}

std::vector<uint32> ItemRunesMgr::DeleteZero(std::vector<uint32> Items)
{
    for (auto Itr = Items.begin(); Itr != Items.end();)
    {
        if (*Itr == 0)
            Itr = Items.erase(Itr);
        else
            Itr++;
    }

    return Items;
}

bool ItemRunesMgr::IsFillVectEqual(std::vector<uint32>& A, std::vector<uint32>& B)
{
    if (A.empty())
        return false;
    if (B.empty())
        return false;
    if (A.size() < B.size())
        return false;

    std::sort(A.begin(), A.end(), [](const uint32& X, const uint32& Y) { return X < Y; });
    std::sort(B.begin(), B.end(), [](const uint32& X, const uint32& Y) { return X < Y; });

    return std::includes(A.begin(), A.end(), B.begin(), B.end());
}

std::pair<std::string, std::string> ItemRunesMgr::PullGSpellStore(Item* DoItem, uint32 GroupId)
{
    std::pair<std::string, std::string> Result;
    Result.first = "";
    Result.second = "";

    std::map<uint32, std::vector<uint32>> DoList;
    DoList.clear();
    for (auto It = SpGroupTempStore.begin(); It != SpGroupTempStore.end(); It++)
    {
        if (It->GroupId == GroupId)
        {
            std::vector<uint32> Store = { It->ResItem1, It->ResItem2, It->ResItem3, It->ResItem4, It->ResItem5, It->ResItem6, It->ResItem7, It->ResItem8 };
            DoList[It->Num] = DeleteZero(Store);
        }
    }

    if (DoList.empty())
        return Result;

    std::vector<uint32> OList;
    OList.clear();
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
    {
        if (!DoItem->ResItems[i])
            continue;

        OList.push_back(DoItem->ResItems[i]);
    }

    int32 Num = -1;
    for (auto Itr = DoList.begin(); Itr != DoList.end(); Itr++)
    {
        auto& Items = Itr->second;
        bool In = IsFillVectEqual(OList, Items);
        if (In)
            Num = int32(Itr->first);
    }

    if (Num < 0)
        return Result;

    for (auto It = SpGroupTempStore.begin(); It != SpGroupTempStore.end(); It++)
    {
        if (It->GroupId == GroupId && It->Num == Num)
        {
            Result.first = It->GName;
            Result.second = It->SpellListStr;
        }
    }

    DoList.clear();
    OList.clear();
    return Result;
}

void ItemRunesMgr::HandlersItemRunesOnNow(Item* DoItem, uint32 GroupId)
{
    auto [SName, SList] = PullGSpellStore(DoItem, GroupId);
    if (!SName.empty() && !SList.empty())
    {
        DoItem->GSpellName = SName;
        DoItem->GSpellListStr = SList;
    }
}

bool ItemRunesMgr::DoAddRunes(Item* DoItem, uint32 RunesItem, uint8 Slot, Player* player)
{
    uint32 TarItemId = DoItem->GetEntry();
    uint32 TarGuidLow = DoItem->GetGUIDLow();
    auto Itr = AtRunesTempStore.find(TarItemId);
    if (Itr == AtRunesTempStore.end())
        return false;

    auto Iter = ItemRunesTempStore.find(RunesItem);
    if (Iter == ItemRunesTempStore.end())
        return false;

    if (Slot > (MAX_RUNES_COUNT - 1))
        return false;

    if (DoItem->NowCount >= DoItem->RunesCount || DoItem->NowCount >= MAX_RUNES_COUNT)
        return false;

    uint32 AddNtempId = GetAddNtempId(RunesItem);
    if (!sHelper->CheckNeed(player, AddNtempId, "符文镶嵌功能消耗"))
        return false;

    DoItem->NowCount++;
    DoItem->ResItems[Slot] = RunesItem;
    uint32 OSpellId = GetOSpellId(RunesItem);
    DoItem->OSpells[Slot] = OSpellId;

    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        HandlersItemRunesOnNow(DoItem, Itr->second.GroupId);

    std::string SendMsg;
    SendMsg.clear();

    SendMsg.append(std::to_string(DoItem->GetGUIDLow()) + "$" + std::to_string(DoItem->NowCount) + "$" + std::to_string(DoItem->RunesCount) + "$");

    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        SendMsg.append(std::to_string(DoItem->ResItems[i]) + "#");

    SendMsg.append("$");

    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        SendMsg.append(std::to_string(DoItem->OSpells[i]) + "#");

    SendMsg.append("$");

    SendMsg.append((DoItem->GSpellName.empty()) ? "nullptr$" : (DoItem->GSpellName + "$"));
    SendMsg.append((DoItem->GSpellListStr.empty()) ? "nullptr$" : (DoItem->GSpellListStr + "$"));
    SendMsg.append(std::to_string(DoItem->GetEntry()));

    UpdateFrozenCoreRuneCache(DoItem);
    if (player)
        sDoAddon->SendPacketToClient(player, "ClientAtRunesData", BuildRunesDataPayload(DoItem->GetGUIDLow(), ItemRunesCacheStore[DoItem->GetGUIDLow()]));

    SendMsg.clear();

    CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_RUNES_AT);
    BindItemRuneArchive(Utmt, DoItem);
    CharacterDatabase.Execute(Utmt);

    ItemRunesCacheTemp Temp;
    Temp.ItemId = DoItem->GetEntry();
    Temp.NowCount = DoItem->NowCount;
    Temp.RunesCount = DoItem->RunesCount;
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
    {
        Temp.ResItems[i] = DoItem->ResItems[i];
        Temp.OSpells[i] = DoItem->OSpells[i];
    }
    Temp.GSpellName = DoItem->GSpellName;
    Temp.GSpellListStr = DoItem->GSpellListStr;

    ItemRunesCacheStore[DoItem->GetGUIDLow()] = Temp;

    return true;
}

bool ItemRunesMgr::DoDeleteRunes(Item* DoItem, uint32 RunesItem, uint8 Slot, Player* player)
{
    uint32 TarItemId = DoItem->GetEntry();
    uint32 TarGuidLow = DoItem->GetGUIDLow();
    auto Itr = AtRunesTempStore.find(TarItemId);
    if (Itr == AtRunesTempStore.end())
        return false;

    auto Iter = ItemRunesTempStore.find(RunesItem);
    if (Iter == ItemRunesTempStore.end())
        return false;

    if (Slot > (MAX_RUNES_COUNT - 1))
        return false;

    if (!DoItem->NowCount)
        return false;

    uint32 DelNtempId = GetDelNtempId(RunesItem);
    if (!sHelper->CheckNeed(player, DelNtempId, "符文拆除功能消耗"))
        return false;

    DoItem->NowCount--;
    DoItem->ResItems[Slot] = 0;
    DoItem->OSpells[Slot] = 0;

    DoItem->GSpellName = "";
    DoItem->GSpellListStr = "";

    if (DoItem->NowCount)
    {
        for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
            HandlersItemRunesOnNow(DoItem, Itr->second.GroupId);
    }

    std::string SendMsg;
    SendMsg.clear();

    SendMsg.append(std::to_string(DoItem->GetGUIDLow()) + "$" + std::to_string(DoItem->NowCount) + "$" + std::to_string(DoItem->RunesCount) + "$");

    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        SendMsg.append(std::to_string(DoItem->ResItems[i]) + "#");

    SendMsg.append("$");

    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        SendMsg.append(std::to_string(DoItem->OSpells[i]) + "#");

    SendMsg.append("$");

    SendMsg.append((DoItem->GSpellName.empty()) ? "nullptr$" : (DoItem->GSpellName + "$"));
    SendMsg.append((DoItem->GSpellListStr.empty()) ? "nullptr$" : (DoItem->GSpellListStr + "$"));
    SendMsg.append(std::to_string(DoItem->GetEntry()));

    UpdateFrozenCoreRuneCache(DoItem);
    if (player)
        sDoAddon->SendPacketToClient(player, "ClientAtRunesData", BuildRunesDataPayload(DoItem->GetGUIDLow(), ItemRunesCacheStore[DoItem->GetGUIDLow()]));

    SendMsg.clear();

    CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_RUNES_AT);
    BindItemRuneArchive(Utmt, DoItem);
    CharacterDatabase.Execute(Utmt);

    ItemRunesCacheTemp Temp;
    Temp.ItemId = DoItem->GetEntry();
    Temp.NowCount = DoItem->NowCount;
    Temp.RunesCount = DoItem->RunesCount;
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
    {
        Temp.ResItems[i] = DoItem->ResItems[i];
        Temp.OSpells[i] = DoItem->OSpells[i];
    }
    Temp.GSpellName = DoItem->GSpellName;
    Temp.GSpellListStr = DoItem->GSpellListStr;

    ItemRunesCacheStore[DoItem->GetGUIDLow()] = Temp;

    return true;
}

bool ItemRunesMgr::DoDeleteAsRunes(Item* DoItem, Player* player)
{
    uint32 TarItemId = DoItem->GetEntry();
    uint32 TarGuidLow = DoItem->GetGUIDLow();
    auto Itr = AtRunesTempStore.find(TarItemId);
    if (Itr == AtRunesTempStore.end())
        return false;

    if (!DoItem->NowCount)
        return false;

    DoItem->GSpellName = "";
    DoItem->GSpellListStr = "";
    DoItem->NowCount = 0;
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
    {
        if (DoItem->ResItems[i] != 0)
        {
            player->AddItem(DoItem->ResItems[i], 1, "符文拆除获得");
            DoItem->ResItems[i] = 0;
        }
        DoItem->OSpells[i] = 0;
    }

    std::string SendMsg;
    SendMsg.clear();

    SendMsg.append(std::to_string(DoItem->GetGUIDLow()) + "$" + std::to_string(DoItem->NowCount) + "$" + std::to_string(DoItem->RunesCount) + "$");

    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        SendMsg.append(std::to_string(DoItem->ResItems[i]) + "#");

    SendMsg.append("$");

    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        SendMsg.append(std::to_string(DoItem->OSpells[i]) + "#");

    SendMsg.append("$");

    SendMsg.append((DoItem->GSpellName.empty()) ? "nullptr$" : (DoItem->GSpellName + "$"));
    SendMsg.append((DoItem->GSpellListStr.empty()) ? "nullptr$" : (DoItem->GSpellListStr + "$"));
    SendMsg.append(std::to_string(DoItem->GetEntry()));

    UpdateFrozenCoreRuneCache(DoItem);
    if (player)
        sDoAddon->SendPacketToClient(player, "ClientAtRunesData", BuildRunesDataPayload(DoItem->GetGUIDLow(), ItemRunesCacheStore[DoItem->GetGUIDLow()]));

    SendMsg.clear();

    CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_RUNES_AT);
    BindItemRuneArchive(Utmt, DoItem);
    CharacterDatabase.Execute(Utmt);

    ItemRunesCacheTemp Temp;
    Temp.ItemId = DoItem->GetEntry();
    Temp.NowCount = DoItem->NowCount;
    Temp.RunesCount = DoItem->RunesCount;
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
    {
        Temp.ResItems[i] = DoItem->ResItems[i];
        Temp.OSpells[i] = DoItem->OSpells[i];
    }
    Temp.GSpellName = DoItem->GSpellName;
    Temp.GSpellListStr = DoItem->GSpellListStr;

    ItemRunesCacheStore[DoItem->GetGUIDLow()] = Temp;

    return true;
}

bool ItemRunesMgr::DoAddSlotRunes(Item* DoItem, Player* player)
{
    uint32 TarItemId = DoItem->GetEntry();
    uint32 TarGuidLow = DoItem->GetGUIDLow();
    auto Itr = AtRunesTempStore.find(TarItemId);
    if (Itr == AtRunesTempStore.end())
    {
        player->SendSysMsg(sHelper->GetGts(3902));
        return false;
    }

    auto Iter = ItemRunesCacheStore.find(TarGuidLow);
    if (Iter == ItemRunesCacheStore.end())
    {
        player->SendSysMsg(sHelper->GetGts(3902));
        return false;
    }

    auto Itar = ItemASlotRunesTempStore.find(TarItemId);
    if (Itar == ItemASlotRunesTempStore.end())
    {
        player->SendSysMsg(sHelper->GetGts(3902));
        return false;
    }

    if (DoItem->RunesCount >= Itr->second.MaxOpen)
    {
        player->SendSysMsg(sHelper->GetGts(7001), Itr->second.MaxOpen);
        return false;
    }

    if (DoItem->RunesCount >= MAX_RUNES_COUNT)
        return false;

    uint32 DelNtempId = Itar->second.NTempId;
    if (!sHelper->CheckNeed(player, DelNtempId, "符文开槽消耗"))
        return false;

    if (!roll_chance_f(Itar->second.Chance))
        return false;

    DoItem->RunesCount++;

    CharacterDatabasePreparedStatement* UDtmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_RUNES_SLOT);
    UDtmt->SetData(0, DoItem->RunesCount);
    UDtmt->SetData(1, TarGuidLow);
    CharacterDatabase.Execute(UDtmt);

    std::string SendMsg;
    SendMsg.clear();

    SendMsg.append(std::to_string(DoItem->GetGUIDLow()) + "$" + std::to_string(DoItem->RunesCount) + "$");

    if (!SendMsg.empty())
    {
        sDoAddon->SendPacketToClient(player, "ClientRunesAddSlot", SendMsg);
    }

    SendMsg.clear();

    ItemRunesCacheStore[DoItem->GetGUIDLow()].RunesCount = DoItem->RunesCount;

    return true;
}

bool ItemRunesMgr::IsTaskBind(uint32 RunesItem)
{
    auto Itr = ItemRunesTempStore.find(RunesItem);
    return Itr != ItemRunesTempStore.end() ? Itr->second.Bind : false;
}

void ItemRunesMgr::LoadForDBRunes(Item* DoItem)
{
    auto Itr = ItemRunesCacheStore.find(DoItem->GetGUIDLow());
    if (Itr == ItemRunesCacheStore.end())
        return;

    ItemRunesCacheTemp const& Cache = Itr->second;
    DoItem->NowCount = Cache.NowCount;
    DoItem->RunesCount = Cache.RunesCount;
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        DoItem->ResItems[i] = Cache.ResItems[i];
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        DoItem->OSpells[i] = Cache.OSpells[i];
    DoItem->GSpellName = Cache.GSpellName;
    DoItem->GSpellListStr = Cache.GSpellListStr;
}

void ItemRunesMgr::OnApplyItemRunesSpell(Player* player, Item* KItem, bool Apply)
{
    uint32 ItemId = KItem->GetEntry();
    auto Itr = AtRunesTempStore.find(ItemId);
    if (Itr == AtRunesTempStore.end())
        return;

    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
    {
        uint32 SpellId = KItem->OSpells[i];
        if (SpellId == 0)
            continue;

        uint32 ResItemId = KItem->ResItems[i];
        if (ResItemId == 0)
            continue;

        if (SpellInfo const* STemp = sSpellMgr->GetSpellInfo(SpellId))
        {
            if (IsRunesCasted(ResItemId))
            {
                if (Apply)
                    player->learnSpell(SpellId);
                else
                    player->removeSpell(SpellId, SPEC_MASK_ALL, false);
            }
            else
            {
                player->ApplyEquipSpell(STemp, KItem, Apply);
            }
        }
    }

    Tokenizer Data(KItem->GSpellListStr, '#');
    for (auto const& Temp : Data)
    {
        uint32 SpellId = atoi(Temp);
        if (SpellId == 0)
            continue;

        if (SpellInfo const* SInfo = sSpellMgr->GetSpellInfo(SpellId))
            player->ApplyEquipSpell(SInfo, KItem, Apply);
    }
}

bool ItemRunesMgr::HasPlayerRunesItemIdAlls(Player* player, uint32 ItemId)
{
    std::vector<Item*> ItemList;
    ItemList.clear();

    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            ItemList.push_back(pItem);

    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            ItemList.push_back(pItem);

    //额外四包
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
        if (Bag* pBag = player->GetBagByPos(i))
            for (uint32 j = 0; j < pBag->GetBagSize(); j++)
                if (Item* pItem = player->GetItemByPos(i, j))
                    ItemList.push_back(pItem);

    //银行
    for (uint8 i = BANK_SLOT_ITEM_START; i < BANK_SLOT_BAG_END; ++i)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            ItemList.push_back(pItem);

    //银行包
    for (uint8 i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        if (Bag* pBag = player->GetBagByPos(i))
            for (uint32 j = 0; j < pBag->GetBagSize(); j++)
                if (Item* pItem = player->GetItemByPos(i, j))
                    ItemList.push_back(pItem);

    bool Has = false;
    if (ItemList.empty())
        return true;

    for (auto Itr = ItemList.begin(); Itr != ItemList.end(); Itr++)
    {
        if (Item* RItem = *Itr)
        {
            for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
            {
                if (RItem->ResItems[i] == ItemId)
                    Has = true;
            }
        }
    }

    return Has;
}

bool ItemRunesMgr::IsRunesCasted(uint32 ResItemId)
{
    auto Itr = ItemRunesTempStore.find(ResItemId);
    return (Itr != ItemRunesTempStore.end()) ? Itr->second.Casted : false;
}
