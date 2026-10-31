
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
#include <cmath>
#include <limits>
#include <random>
#include "ItemRandomAtMgr.h"
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include <Mails/Mail.h>
#include <Loot/LootMgr.h>
#include <Custom\ItemFuncMgr\ItemFuncMgr.h>
#include <Custom\PlayerModMgr\PlayerModMgr.h>
#include <Custom/CreatureMod/AsCreature.h>
#pragma execution_character_set("utf-8")

GenerateItemAtMap GenerateItemAtStore;
GTempItemListMap GTempItemAtStore;
StatItemGroupMap StatItemAtStore;
SpellItemGroupMap SpellItemAtStore;
UItemAtTempMap UItemAtTempStore;
EnchItemGroupMap EnchItemAtStore;
SuperUpSellTempVect SuperUpSellStore;
OnUpRankAtVect OnUpRankAtStore;
ResetResAtVect ResetResAtStore;

//总缓存
ItemAtCacheMap ItemAtCacheStore;

namespace
{
AsInt64 RandomAsInt64(AsInt64 minValue, AsInt64 maxValue)
{
    if (minValue > maxValue)
        std::swap(minValue, maxValue);

    AsInt64 range = maxValue - minValue;
    if (range <= 0)
        return minValue;

    AsInt64 randomValue = 0;
    for (uint8 i = 0; i < 7; ++i)
    {
        randomValue <<= 32;
        randomValue += urand(0, std::numeric_limits<uint32>::max());
    }

    return minValue + (randomValue % (range + 1));
}

AsInt64 NormalizeRandomAttributeMultiplier(AsInt64 value)
{
    return value > 0 ? value : AsInt64(1);
}

AsInt64 ApplyFixedPointMod(AsInt64 value, float mod)
{
    if (mod == 0.0f || AsInt64IsZero(value))
        return value;

    int64 scale = 10000;
    int64 modScaled = int64(std::llround(double(mod) * double(scale)));
    return value + ((value * modScaled) / scale);
}

uint32 AsWeaponDamageCompat(AsInt64 const& damage)
{
    return AsInt64ToUInt32Clamp(damage, AS_LEGACY_FLOAT_INT32_MAX);
}

AsInt64 GetTemplateWeaponMinDamage(ItemTemplate const* temp)
{
    if (!temp)
        return AsInt64Zero();

    return temp->AsDamageMin[0] > 0 ? temp->AsDamageMin[0] : AsInt64(uint32(temp->Damage[0].DamageMin));
}

AsInt64 GetTemplateWeaponMaxDamage(ItemTemplate const* temp)
{
    if (!temp)
        return AsInt64Zero();

    return temp->AsDamageMax[0] > 0 ? temp->AsDamageMax[0] : AsInt64(uint32(temp->Damage[0].DamageMax));
}

void SetRandomItemWeaponDamage(Item* item, AsInt64 minDamage, AsInt64 maxDamage)
{
    if (!item)
        return;

    item->AsMinWDmg = minDamage > 0 ? minDamage : AsInt64Zero();
    item->AsMaxWDmg = maxDamage > 0 ? maxDamage : AsInt64Zero();
    item->MinWDmg = AsWeaponDamageCompat(item->AsMinWDmg);
    item->MaxWDmg = AsWeaponDamageCompat(item->AsMaxWDmg);
}

AsInt64 RollConfiguredWeaponDamage(AsInt64 minDamage, AsInt64 maxDamage)
{
    if (minDamage <= 0 && maxDamage <= 0)
        return AsInt64Zero();

    return RandomAsInt64(minDamage, maxDamage);
}

AsInt64 GetRandomItemStatValue(Item* item, Player* player, uint8 slot, AsInt64 value, bool includeEqSlotMod)
{
    value = ApplyFixedPointMod(value, sItemRandomAtMgr->GetModForRank(item->GetEntry(), item->RmAtLevel));

    if (includeEqSlotMod)
        value = ApplyFixedPointMod(value, sPlayerModMgr->GetModPlayerEqSlot(player, slot));

    float soulMod = sItemFuncMgr->GetSoulItemPtData(player, item->GetEntry());
    int64 scale = 10000;
    value = (value * int64(std::llround(double(soulMod) * double(scale)))) / scale;

    return value;
}

void ApplyRandomItemStat(Player* player, Item* item, uint8 slot, bool apply, bool includeEqSlotMod)
{
    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
    {
        int32 statType = item->RmAtStatStore[i].first;
        AsInt64 value = item->RmAtStatStore[i].second;
        if (statType < 0 || statType >= MAX_ITEM_MOD || AsInt64IsZero(value))
            continue;

        value = GetRandomItemStatValue(item, player, slot, value, includeEqSlotMod);
        if (!AsInt64IsZero(value))
            player->DoApplyStat(ItemModType(statType), value, apply);
    }
}

void ApplyRandomAttributeMultiplier(Player* player, Item* item, int32 statType, AsInt64 const& multiplier, bool apply)
{
    if (multiplier <= 1)
        return;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return;

    AsInt64 baseValue = 0;
    for (uint8 i = 0; i < proto->StatsCount; ++i)
    {
        if (proto->ItemStat[i].ItemStatType == uint32(statType))
        {
            baseValue = proto->AsItemStatValue[i];
            if (AsInt64IsZero(baseValue))
                baseValue = proto->ItemStat[i].ItemStatValue;
            break;
        }
    }

    if (AsInt64IsZero(baseValue))
        return;

    AsInt64 delta = baseValue * (multiplier - 1);
    if (!AsInt64IsZero(delta))
        player->DoApplyStat(ItemModType(statType), delta, apply);
}

}

//------------   随机属性倍率   ------------
struct _Attribute
{
    uint32 Attri_a;
    uint32 Attri_b;
    AsInt64 Attri_c;
    AsInt64 Attri_d;
    float Attri_e;
};
typedef std::multimap<uint32, _Attribute> sx_Attribute;
sx_Attribute m_Attribute;
//------------   随机属性倍率   ------------

void ItemRandomAtMgr::LoadItemRandomAt()
{
    GenerateItemAtStore.clear();
    m_Attribute.clear();
    GTempItemAtStore.clear();
    StatItemAtStore.clear();
    SpellItemAtStore.clear();
    EnchItemAtStore.clear();
    UItemAtTempStore.clear();
    SuperUpSellStore.clear();
    OnUpRankAtStore.clear();
    ResetResAtStore.clear();

    // 22011=1: use template item systems and disable the legacy super-random equipment system.
    if (sHelper->GetBoolean(22011))
        return;
}

bool ItemRandomAtMgr::IsGenerateItemAt(uint32 ItemId)
{
    if (sHelper->GetBoolean(22011))
        return false;

    auto Itr = GenerateItemAtStore.find(ItemId);
    return (Itr != GenerateItemAtStore.end());
}

bool ItemRandomAtMgr::InGTempItemAtStore(uint32 GroupTempId)
{
    if (sHelper->GetBoolean(22011))
        return false;

    return (GTempItemAtStore.find(GroupTempId) != GTempItemAtStore.end());
}

float ItemRandomAtMgr::GetModForRank(uint32 ItemId, uint32 Level)
{
    float Mod = 0.0f;
    if (sHelper->GetBoolean(22011))
        return Mod;

    if (!Level)
        return Mod;

    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return Mod;

    for (auto It = Iter->second.begin(); It != Iter->second.end(); ++It)
    {
        if (It->ULevel > 0 && It->ULevel <= Level)
            Mod += It->Mod;
    }

    return Mod;
}

uint32 ItemRandomAtMgr::AutoGroupNum(uint32 GroupTempId)
{
    auto Iter = GTempItemAtStore.find(GroupTempId);
    if (Iter == GTempItemAtStore.end())
        return 0;

    uint32 Num = 0;
    float RIdx = 0.0f;
    std::map<uint32, float> VectSt;

    for (auto Itr = Iter->second.begin(); Itr != Iter->second.end(); Itr++)
        RIdx += Itr->Chance;

    for (auto Itr = Iter->second.begin(); Itr != Iter->second.end(); Itr++)
    {
        if (frand(0, RIdx) <= Itr->Chance)
        {
            Num = Itr->Num;
            break;
        }
        else
            RIdx -= Itr->Chance;
    }

    return Num;
}

RmAtStatVect ItemRandomAtMgr::DoGenerateGroupStatTypeValue(StatItemGroupList& DoVect, uint32 Count)
{
    RmAtStatVect TempVect;
    if (!Count)
        return TempVect;

    Count = Count > DoVect.size() ? DoVect.size() : Count;

    StatItemGroupList TempStatVect = DoVect;
    if (!sHelper->GetBoolean(14001))
    {
        Acore::Containers::AtRandomResize(TempStatVect, Count);
        for (auto Itr = TempStatVect.begin(); Itr != TempStatVect.end(); Itr++)
        {
            int32 SType = Itr->SType;
            AsInt64 Value = RandomAsInt64(Itr->MinValue, Itr->MaxValue);
            TempVect.push_back(std::pair(SType, Value));
        }

        return TempVect;
    }

    for (uint32 i = 0; i < Count; i++)
    {
        if (!TempStatVect.empty())
        {
            float RIdx = 0.0f;
            for (auto Itr = TempStatVect.begin(); Itr != TempStatVect.end(); Itr++)
                RIdx += Itr->Chance;

            for (auto Itr = TempStatVect.begin(); Itr != TempStatVect.end(); Itr++)
            {
                if (frand(0, RIdx) <= Itr->Chance)
                {
                    int32 SType = Itr->SType;
                    AsInt64 Value = RandomAsInt64(Itr->MinValue, Itr->MaxValue);
                    TempVect.push_back(std::pair(SType, Value));

                    break;
                }
                else
                    RIdx -= Itr->Chance;
            }
        }
    }

    return TempVect;
}

std::vector<uint32> ItemRandomAtMgr::DoGenerateGroupSpellId(SpellItemGroupList& DoVect, uint32 Count)
{
    std::vector<uint32> TempVect;
    if (!Count)
        return TempVect;

    Count = Count > DoVect.size() ? DoVect.size() : Count;

    SpellItemGroupList TempSpellMap = DoVect;
    if (!sHelper->GetBoolean(14002))
    {
        Acore::Containers::AtRandomResize(TempSpellMap, Count);
        for (auto Itr = TempSpellMap.begin(); Itr != TempSpellMap.end(); Itr++)
            TempVect.push_back(Itr->SpellId);

        return TempVect;
    }

    for (uint32 i = 0; i < Count; i++)
    {
        if (!TempSpellMap.empty())
        {
            float RIdx = 0.0f;
            for (auto Itr = TempSpellMap.begin(); Itr != TempSpellMap.end(); Itr++)
                RIdx += Itr->Chance;

            for (auto Itr = TempSpellMap.begin(); Itr != TempSpellMap.end(); Itr++)
            {
                if (frand(0, RIdx) <= Itr->Chance)
                {
                    TempVect.push_back(Itr->SpellId);
                    break;
                }
                else
                    RIdx -= Itr->Chance;
            }
        }
    }

    return TempVect;
}

std::vector<uint32> ItemRandomAtMgr::DoGenerateGroupEnchId(EnchItemGroupList& DoVect, uint32 Count)
{
    std::vector<uint32> TempVect;
    if (!Count)
        return TempVect;

    Count = Count > DoVect.size() ? DoVect.size() : Count;

    EnchItemGroupList TempEnchMap = DoVect;
    Acore::Containers::AtRandomResize(TempEnchMap, Count);
    for (auto Itr = TempEnchMap.begin(); Itr != TempEnchMap.end(); Itr++)
        TempVect.push_back(Itr->EnchId);

    return TempVect;
}

void ItemRandomAtMgr::DoGenerateItemAt(Item* DoItem, uint32 CreateLevel, bool Reset)
{
    if (sHelper->GetBoolean(22011))
        return;

    if (!DoItem)
        return;

    Player* player = DoItem->GetOwner();
    if (!player)
        return;

    ItemTemplate const* Temp = DoItem->GetTemplate();
    if (!Temp)
        return;

    auto Itr = GenerateItemAtStore.find(DoItem->GetEntry());
    if (Itr == GenerateItemAtStore.end())
        return;

    //------------   随机属性倍率   ------------
    if (uint32 Gain_sx = Itr->second._Attris)
    {
        auto Its = m_Attribute.find(Gain_sx);
        if (Its != m_Attribute.end())
        {
            for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
                DoItem->_Attribute[i] = AsInt64(1);

            for (int i = 0; i < m_Attribute.count(Gain_sx); i++, Its++)
            {
                if (roll_chance_f(Its->second.Attri_e))
                {
                    AsInt64 _sj = RandomAsInt64(Its->second.Attri_c, Its->second.Attri_d);

                    if (Its->second.Attri_b == 3)
                        DoItem->_Attribute[0] = _sj;
                    if (Its->second.Attri_b == 4)
                        DoItem->_Attribute[1] = _sj;
                    if (Its->second.Attri_b == 5)
                        DoItem->_Attribute[2] = _sj;
                    if (Its->second.Attri_b == 6)
                        DoItem->_Attribute[3] = _sj;
                    if (Its->second.Attri_b == 7)
                        DoItem->_Attribute[4] = _sj;
                    if (Its->second.Attri_b == 38)
                        DoItem->_Attribute[5] = _sj;
                    if (Its->second.Attri_b == 45)
                        DoItem->_Attribute[6] = _sj;

                }
            }
        }
    }
    //------------   随机属性倍率   ------------

    //原始追加属性
    uint32 GTempId = Itr->second.GroupTempId;
    if (!GTempId)
        return;

    auto Iterg = GTempItemAtStore.find(GTempId);
    if (Iterg == GTempItemAtStore.end())
        return;

    if (!CreateLevel)
    {
        if (!roll_chance_f(Itr->second.Chance))
            return;

        if (!sHelper->CheckNeed(player, Itr->second.NTempId, "超级生成消耗"))
            return;
    }

    uint32 Num = AutoGroupNum(GTempId);

    DoItem->RmAtLevel = CreateLevel;
    std::string LevelStr = GetLevelStr(DoItem->GetEntry(), DoItem->RmAtLevel);
    DoItem->LevelStr = LevelStr.empty() ? "nullptr" : LevelStr;
    SetRandomItemWeaponDamage(DoItem, GetTemplateWeaponMinDamage(Temp), GetTemplateWeaponMaxDamage(Temp));

    for (auto It = Iterg->second.begin(); It != Iterg->second.end(); It++)
    {
        if (It->Num == Num)
        {
            DoItem->Prefix = It->Prefix.empty() ? "nullptr" : It->Prefix;
            DoItem->Suffix = It->Suffix.empty() ? "nullptr" : It->Suffix;

            AsInt64 Min = RollConfiguredWeaponDamage(It->MinWpDmg1, It->MinWpDmg2);
            AsInt64 Max = RollConfiguredWeaponDamage(It->MaxWpDmg1, It->MaxWpDmg2);
            SetRandomItemWeaponDamage(DoItem,
                Min <= 0 ? GetTemplateWeaponMinDamage(Temp) : Min,
                Max <= 0 ? GetTemplateWeaponMaxDamage(Temp) : Max);

            auto TItr = StatItemAtStore.find(It->StatGroupId);
            if (TItr != StatItemAtStore.end())
            {
                auto StatTempVect = DoGenerateGroupStatTypeValue(TItr->second, It->StatCount);
                for (uint8 i = 0; i < StatTempVect.size(); i++)
                {
                    if (StatTempVect[i].first > -1 && !AsInt64IsZero(StatTempVect[i].second))
                    {
                        DoItem->RmAtStatStore[i].first = StatTempVect[i].first;
                        DoItem->RmAtStatStore[i].second = StatTempVect[i].second;
                    }
                }
            }

            auto SItr = SpellItemAtStore.find(It->SpellGroupId);
            if (SItr != SpellItemAtStore.end())
            {
                auto SpellTempVect = DoGenerateGroupSpellId(SItr->second, It->SpellCount);
                for (uint8 i = 0; i < SpellTempVect.size(); i++)
                {
                    if (SpellTempVect[i])
                        DoItem->RmAtSpell[i] = SpellTempVect[i];
                }
            }

            if (Reset)
            {
                for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
                    DoItem->ClearEnchantment(EnchantmentSlot(ESlot));

                auto EItr = EnchItemAtStore.find(It->EnchGroupId);
                if (EItr != EnchItemAtStore.end())
                {
                    auto EnchTempVect = DoGenerateGroupEnchId(EItr->second, It->EnchCount);
                    for (uint8 i = 0; i < EnchTempVect.size(); i++)
                    {
                        if (EnchTempVect[i])
                            DoItem->SetEnchantment(EnchantmentSlot(i + PROP_ENCHANTMENT_SLOT_0), EnchTempVect[i], 0, 0);
                    }
                }

                EnchantTemp Temp;
                Temp.EnchantId1 = DoItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0));
                Temp.EnchantId2 = DoItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_1));
                Temp.EnchantId3 = DoItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_2));
                Temp.EnchantId4 = DoItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_3));
                Temp.EnchantId5 = DoItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_4));
                EnchantAtStore[DoItem->GetGUIDLow()] = Temp;

                std::string SendMsg;
                for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
                {
                    uint32 EnchantId = DoItem->GetEnchantmentId(EnchantmentSlot(ESlot));
                    SendMsg.append(std::to_string(EnchantId) + "#");
                }

                if (!SendMsg.empty())
                {
                    sDoAddon->SendPacketToClient(player, "AutoEnchant", std::to_string(DoItem->GetGUIDLow()) + "$" + SendMsg);
                }
            }
        }
    }

    if (CreateLevel)
    {
        auto Iters = UItemAtTempStore.find(DoItem->GetEntry());
        if (Iters != UItemAtTempStore.end())
        {
            for (auto Itert = Iters->second.begin(); Itert != Iters->second.end(); Itert++)
            {
                if (Itert->ULevel == CreateLevel)
                {
                    if (!Itert->OnAtUpSave)
                    {
                        for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
                            DoItem->RmAtSpell[i] = 0;

                        auto SItr = SpellItemAtStore.find(Itert->SpellGroupId);
                        if (SItr != SpellItemAtStore.end())
                        {
                            auto SpellTempVect = DoGenerateGroupSpellId(SItr->second, Itert->SpellCount);
                            for (uint8 i = 0; i < SpellTempVect.size(); i++)
                            {
                                if (SpellTempVect[i])
                                    DoItem->RmAtSpell[i] = SpellTempVect[i];
                            }
                        }
                    }
                }
            }
        }
    }

    DoSaveDBItemAtAndUpdate(DoItem);
}

void ItemRandomAtMgr::DoGenerateItemAtForDevelop(Item* DoItem, uint32 GroupTempId)
{
    if (sHelper->GetBoolean(22011))
        return;

    if (!GroupTempId)
        return;

    if (!DoItem)
        return;

    ItemTemplate const* Temp = DoItem->GetTemplate();
    if (!Temp)
        return;

    Player* player = DoItem->GetOwner();
    if (!player)
        return;

    uint32 GTempId = GroupTempId;
    uint32 Num = AutoGroupNum(GTempId);

    auto Iterg = GTempItemAtStore.find(GTempId);
    if (Iterg == GTempItemAtStore.end())
        return;

    DoItem->RmAtLevel = 0;

    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
    {
        DoItem->RmAtStatStore[i].first = -1;
        DoItem->RmAtStatStore[i].second = 0;
    }

    for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
        DoItem->RmAtSpell[i] = 0;

    std::string LevelStr = GetLevelStr(DoItem->GetEntry(), DoItem->RmAtLevel);
    DoItem->LevelStr = LevelStr.empty() ? "nullptr" : LevelStr;
    SetRandomItemWeaponDamage(DoItem, GetTemplateWeaponMinDamage(Temp), GetTemplateWeaponMaxDamage(Temp));

    for (auto It = Iterg->second.begin(); It != Iterg->second.end(); It++)
    {
        if (It->Num == Num)
        {
            DoItem->Prefix = It->Prefix.empty() ? "nullptr" : It->Prefix;
            DoItem->Suffix = It->Suffix.empty() ? "nullptr" : It->Suffix;

            AsInt64 Min = RollConfiguredWeaponDamage(It->MinWpDmg1, It->MinWpDmg2);
            AsInt64 Max = RollConfiguredWeaponDamage(It->MaxWpDmg1, It->MaxWpDmg2);
            SetRandomItemWeaponDamage(DoItem,
                Min <= 0 ? GetTemplateWeaponMinDamage(Temp) : Min,
                Max <= 0 ? GetTemplateWeaponMaxDamage(Temp) : Max);

            auto TItr = StatItemAtStore.find(It->StatGroupId);
            if (TItr != StatItemAtStore.end())
            {
                auto StatTempVect = DoGenerateGroupStatTypeValue(TItr->second, It->StatCount);
                for (uint8 i = 0; i < StatTempVect.size(); i++)
                {
                    if (StatTempVect[i].first > -1 && !AsInt64IsZero(StatTempVect[i].second))
                    {
                        DoItem->RmAtStatStore[i].first = StatTempVect[i].first;
                        DoItem->RmAtStatStore[i].second = StatTempVect[i].second;
                    }
                }
            }

            auto SItr = SpellItemAtStore.find(It->SpellGroupId);
            if (SItr != SpellItemAtStore.end())
            {
                auto SpellTempVect = DoGenerateGroupSpellId(SItr->second, It->SpellCount);
                for (uint8 i = 0; i < SpellTempVect.size(); i++)
                {
                    if (SpellTempVect[i])
                        DoItem->RmAtSpell[i] = SpellTempVect[i];
                }
            }
        }
    }

    DoSaveDBItemAtAndUpdate(DoItem);
}

void ItemRandomAtMgr::DoGenerateItemAtLoot(Item* DoItem, uint32 CreateLevel, int32 TempGroupId)
{
    if (sHelper->GetBoolean(22011))
        return;

    if (!DoItem)
        return;

    Player* player = DoItem->GetOwner();
    if (!player)
        return;

    ItemTemplate const* Temp = DoItem->GetTemplate();
    if (!Temp)
        return;

    auto Itr = GenerateItemAtStore.find(DoItem->GetEntry());
    if (Itr == GenerateItemAtStore.end())
        return;

    if (!TempGroupId)
        return;

    //------------   随机属性倍率   ------------
    if (uint32 Gain_sx = Itr->second._Attris)
    {
        auto Its = m_Attribute.find(Gain_sx);
        if (Its != m_Attribute.end())
        {
            for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
                DoItem->_Attribute[i] = AsInt64(1);

            for (int i = 0; i < m_Attribute.count(Gain_sx); i++, Its++)
            {
                if (roll_chance_f(Its->second.Attri_e))
                {
                    AsInt64 _sj = RandomAsInt64(Its->second.Attri_c, Its->second.Attri_d);

                    if (Its->second.Attri_b == 3)
                        DoItem->_Attribute[0] = _sj;
                    if (Its->second.Attri_b == 4)
                        DoItem->_Attribute[1] = _sj;
                    if (Its->second.Attri_b == 5)
                        DoItem->_Attribute[2] = _sj;
                    if (Its->second.Attri_b == 6)
                        DoItem->_Attribute[3] = _sj;
                    if (Its->second.Attri_b == 7)
                        DoItem->_Attribute[4] = _sj;
                    if (Its->second.Attri_b == 38)
                        DoItem->_Attribute[5] = _sj;
                    if (Its->second.Attri_b == 45)
                        DoItem->_Attribute[6] = _sj;

                }
            }
        }
    }
    //------------   随机属性倍率   ------------

    auto Iterg = GTempItemAtStore.find(TempGroupId);
    if (Iterg == GTempItemAtStore.end())
        return;

    if (!CreateLevel)
    {
        if (!roll_chance_f(Itr->second.Chance))
            return;

        if (!sHelper->CheckNeed(player, Itr->second.NTempId, "超级生成消耗"))
            return;
    }

    uint32 GTempId = TempGroupId;
    uint32 Num = AutoGroupNum(GTempId);

    DoItem->RmAtLevel = CreateLevel;
    std::string LevelStr = GetLevelStr(DoItem->GetEntry(), DoItem->RmAtLevel);
    DoItem->LevelStr = LevelStr.empty() ? "nullptr" : LevelStr;
    SetRandomItemWeaponDamage(DoItem, GetTemplateWeaponMinDamage(Temp), GetTemplateWeaponMaxDamage(Temp));

    for (auto It = Iterg->second.begin(); It != Iterg->second.end(); It++)
    {
        if (It->Num == Num)
        {
            DoItem->Prefix = It->Prefix.empty() ? "nullptr" : It->Prefix;
            DoItem->Suffix = It->Suffix.empty() ? "nullptr" : It->Suffix;

            AsInt64 Min = RollConfiguredWeaponDamage(It->MinWpDmg1, It->MinWpDmg2);
            AsInt64 Max = RollConfiguredWeaponDamage(It->MaxWpDmg1, It->MaxWpDmg2);
            SetRandomItemWeaponDamage(DoItem,
                Min <= 0 ? GetTemplateWeaponMinDamage(Temp) : Min,
                Max <= 0 ? GetTemplateWeaponMaxDamage(Temp) : Max);

            auto TItr = StatItemAtStore.find(It->StatGroupId);
            if (TItr != StatItemAtStore.end())
            {
                auto StatTempVect = DoGenerateGroupStatTypeValue(TItr->second, It->StatCount);
                for (uint8 i = 0; i < StatTempVect.size(); i++)
                {
                    if (StatTempVect[i].first > -1 && !AsInt64IsZero(StatTempVect[i].second))
                    {
                        DoItem->RmAtStatStore[i].first = StatTempVect[i].first;
                        DoItem->RmAtStatStore[i].second = StatTempVect[i].second;
                    }
                }
            }

            auto SItr = SpellItemAtStore.find(It->SpellGroupId);
            if (SItr != SpellItemAtStore.end())
            {
                auto SpellTempVect = DoGenerateGroupSpellId(SItr->second, It->SpellCount);
                for (uint8 i = 0; i < SpellTempVect.size(); i++)
                {
                    if (SpellTempVect[i])
                        DoItem->RmAtSpell[i] = SpellTempVect[i];
                }
            }
        }
    }

    if (CreateLevel)
    {
        auto Iters = UItemAtTempStore.find(DoItem->GetEntry());
        if (Iters != UItemAtTempStore.end())
        {
            for (auto Itert = Iters->second.begin(); Itert != Iters->second.end(); Itert++)
            {
                if (Itert->ULevel == CreateLevel)
                {
                    if (!Itert->OnAtUpSave)
                    {
                        for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
                            DoItem->RmAtSpell[i] = 0;

                        auto SItr = SpellItemAtStore.find(Itert->SpellGroupId);
                        if (SItr != SpellItemAtStore.end())
                        {
                            auto SpellTempVect = DoGenerateGroupSpellId(SItr->second, Itert->SpellCount);
                            for (uint8 i = 0; i < SpellTempVect.size(); i++)
                            {
                                if (SpellTempVect[i])
                                    DoItem->RmAtSpell[i] = SpellTempVect[i];
                            }
                        }
                    }
                }
            }
        }
    }

    DoSaveDBItemAtAndUpdate(DoItem);
}

void ItemRandomAtMgr::DoUpPowerItemAt(Item* DoItem, uint32 SetLevel, bool Reset)
{
    if (sHelper->GetBoolean(22011))
        return;

    if (!DoItem)
        return;

    ItemTemplate const* Temp = DoItem->GetTemplate();
    if (!Temp)
        return;

    auto Itr = GenerateItemAtStore.find(DoItem->GetEntry());
    if (Itr == GenerateItemAtStore.end())
        return;

    Player* player = DoItem->GetOwner();
    if (!player)
        return;

    auto Iters = UItemAtTempStore.find(DoItem->GetEntry());
    if (Iters == UItemAtTempStore.end())
        return;

    if (!SetLevel)
    {
        DoGenerateItemAt(DoItem, Reset);
        return;
    }

    DoItem->RmAtLevel = SetLevel;
    std::string LevelStr = GetLevelStr(DoItem->GetEntry(), DoItem->RmAtLevel);
    DoItem->LevelStr = LevelStr.empty() ? "nullptr" : LevelStr;

    for (auto It = Iters->second.begin(); It != Iters->second.end(); It++)
    {
        if (It->ULevel == SetLevel)
        {
            if (!It->OnAtUpSave)
            {
                for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
                    DoItem->RmAtSpell[i] = 0;

                auto SItr = SpellItemAtStore.find(It->SpellGroupId);
                if (SItr != SpellItemAtStore.end())
                {
                    auto SpellTempVect = DoGenerateGroupSpellId(SItr->second, It->SpellCount);
                    for (uint8 i = 0; i < SpellTempVect.size(); i++)
                    {
                        if (SpellTempVect[i])
                            DoItem->RmAtSpell[i] = SpellTempVect[i];
                    }
                }
            }
        }
    }

    if (Reset)
    {
        for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
            DoItem->ClearEnchantment(EnchantmentSlot(ESlot));
    }

    DoSaveDBItemAtAndUpdate(DoItem);
}

bool ItemRandomAtMgr::DoResetItemAt(Item* DoItem, uint32 NTempId, uint32 GTempId, bool ResetEnch, bool ResetAtUp)
{
    if (!DoItem)
        return false;

    if (!GTempId)
        return false;

    ItemTemplate const* Temp = DoItem->GetTemplate();
    if (!Temp)
        return false;

    auto Itr = GenerateItemAtStore.find(DoItem->GetEntry());
    if (Itr == GenerateItemAtStore.end())
        return false;

    auto Iterg = GTempItemAtStore.find(GTempId);
    if (Iterg == GTempItemAtStore.end())
        return false;

    Player* player = DoItem->GetOwner();
    if (!player)
        return false;

    if (!sHelper->CheckNeed(player, NTempId, "重置超级强化消耗"))
        return false;

    uint32 Num = AutoGroupNum(GTempId);

    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
    {
        DoItem->RmAtStatStore[i].first = -1;
        DoItem->RmAtStatStore[i].second = 0;
    }

    if (ResetAtUp)
    {
        for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
            DoItem->RmAtSpell[i] = 0;
    }

    std::string LevelStr = GetLevelStr(DoItem->GetEntry(), DoItem->RmAtLevel);
    DoItem->LevelStr = LevelStr.empty() ? "nullptr" : LevelStr;
    SetRandomItemWeaponDamage(DoItem, GetTemplateWeaponMinDamage(Temp), GetTemplateWeaponMaxDamage(Temp));

    for (auto It = Iterg->second.begin(); It != Iterg->second.end(); It++)
    {
        if (It->Num == Num)
        {
            DoItem->Prefix = It->Prefix.empty() ? "nullptr" : It->Prefix;
            DoItem->Suffix = It->Suffix.empty() ? "nullptr" : It->Suffix;

            AsInt64 Min = RollConfiguredWeaponDamage(It->MinWpDmg1, It->MinWpDmg2);
            AsInt64 Max = RollConfiguredWeaponDamage(It->MaxWpDmg1, It->MaxWpDmg2);
            SetRandomItemWeaponDamage(DoItem,
                Min <= 0 ? GetTemplateWeaponMinDamage(Temp) : Min,
                Max <= 0 ? GetTemplateWeaponMaxDamage(Temp) : Max);

            auto TItr = StatItemAtStore.find(It->StatGroupId);
            if (TItr != StatItemAtStore.end())
            {
                auto StatTempVect = DoGenerateGroupStatTypeValue(TItr->second, It->StatCount);
                for (uint8 i = 0; i < StatTempVect.size(); i++)
                {
                    if (StatTempVect[i].first > -1 && !AsInt64IsZero(StatTempVect[i].second))
                    {
                        DoItem->RmAtStatStore[i].first = StatTempVect[i].first;
                        DoItem->RmAtStatStore[i].second = StatTempVect[i].second;
                    }
                }
            }

            if (ResetAtUp)
            {
                auto SItr = SpellItemAtStore.find(It->SpellGroupId);
                if (SItr != SpellItemAtStore.end())
                {
                    auto SpellTempVect = DoGenerateGroupSpellId(SItr->second, It->SpellCount);
                    for (uint8 i = 0; i < SpellTempVect.size(); i++)
                    {
                        if (SpellTempVect[i])
                            DoItem->RmAtSpell[i] = SpellTempVect[i];
                    }
                }
            }

            if (ResetEnch)
            {
                for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
                    DoItem->ClearEnchantment(EnchantmentSlot(ESlot));

                auto EItr = EnchItemAtStore.find(It->EnchGroupId);
                if (EItr != EnchItemAtStore.end())
                {
                    auto EnchTempVect = DoGenerateGroupEnchId(EItr->second, It->EnchCount);
                    for (uint8 i = 0; i < EnchTempVect.size(); i++)
                    {
                        if (EnchTempVect[i])
                            DoItem->SetEnchantment(EnchantmentSlot(i + PROP_ENCHANTMENT_SLOT_0), EnchTempVect[i], 0, 0);
                    }
                }

                EnchantTemp Temp;
                Temp.EnchantId1 = DoItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0));
                Temp.EnchantId2 = DoItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_1));
                Temp.EnchantId3 = DoItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_2));
                Temp.EnchantId4 = DoItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_3));
                Temp.EnchantId5 = DoItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_4));
                EnchantAtStore[DoItem->GetGUIDLow()] = Temp;

                std::string SendMsg;
                for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
                {
                    uint32 EnchantId = DoItem->GetEnchantmentId(EnchantmentSlot(ESlot));
                    SendMsg.append(std::to_string(EnchantId) + "#");
                }

                if (!SendMsg.empty())
                    sDoAddon->SendPacketToClient(player, "AutoEnchant", std::to_string(DoItem->GetGUIDLow()) + "$" + SendMsg);
            }
        }
    }

    DoSaveDBItemAtAndUpdate(DoItem);
    return true;
}

void ItemRandomAtMgr::DoSaveDBItemAtAndUpdate(Item* DoItem)
{
    if (!DoItem || sHelper->GetBoolean(22011))
        return;

    float Mod = GetModForRank(DoItem->GetEntry(), DoItem->RmAtLevel);

    std::string SendMsg;
    std::string DPS = sHelper->GetWpDPSString(DoItem->AsMinWDmg, DoItem->AsMaxWDmg, DoItem->GetEntry());
    SendMsg.append(std::to_string(DoItem->GetGUIDLow()) + "$" + std::to_string(DoItem->RmAtLevel) + "$" + DoItem->Prefix + "$" + DoItem->Suffix + "$" + DoItem->LevelStr + "$");
    SendMsg.append(AsInt64ToString(DoItem->AsMinWDmg) + "$" + AsInt64ToString(DoItem->AsMaxWDmg) + "$" + DPS + "$");

    std::sort(DoItem->RmAtStatStore.begin(), DoItem->RmAtStatStore.end(), [](auto const& X, auto const& Y) { return (X.first != -1 && Y.first != -1 && X.first < Y.first); });

    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
        SendMsg.append(std::to_string(DoItem->RmAtStatStore[i].first) + "#");

    SendMsg.append("$");

    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
        SendMsg.append(AsInt64ToString(DoItem->RmAtStatStore[i].second) + "#");

    SendMsg.append("$");

    for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
        SendMsg.append(std::to_string(DoItem->RmAtSpell[i]) + "#");

    SendMsg.append("$");

    SendMsg.append(std::to_string(Mod) + "$");

    //------------   随机属性倍率   ------------
    std::string AttrisStr;
    for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
    {
        AsInt64 multiplier = NormalizeRandomAttributeMultiplier(DoItem->_Attribute[i]);
        SendMsg.append(AsInt64ToString(multiplier) + "#");
        AttrisStr.append(AsInt64ToString(multiplier) + "#");
    }
    //------------   随机属性倍率   ------------

    SendMsg.append("$");
    SendMsg.append(AttrisStr);
    SendMsg.append("$");
    SendMsg.append(IsGenerateItemAt(DoItem->GetEntry()) ? "1" : "0");
    SendMsg.append("$");
    SendMsg.append(std::to_string(DoItem->GetEntry()));

    // Publish the changed random-item state before the addon packet is sent;
    // FrozenCore computes the new cache ID while that packet is dispatched.
    ItemAtCacheTemp Temp;
    Temp.ItemId = DoItem->GetEntry();
    Temp.ULevel = DoItem->RmAtLevel;
    Temp.Prefix = DoItem->Prefix;
    Temp.Suffix = DoItem->Suffix;
    Temp.LevelStr = DoItem->LevelStr;
    Temp.MinWpDmg = DoItem->AsMinWDmg;
    Temp.MaxWpDmg = DoItem->AsMaxWDmg;
    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
    {
        Temp.AtStatStore[i].first = DoItem->RmAtStatStore[i].first;
        Temp.AtStatStore[i].second = DoItem->RmAtStatStore[i].second;
    }
    for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
        Temp.AtSpell[i] = DoItem->RmAtSpell[i];
    for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
        Temp._AtSpell[i] = NormalizeRandomAttributeMultiplier(DoItem->_Attribute[i]);
    Temp.Mod = Mod;
    ItemAtCacheStore[DoItem->GetGUIDLow()] = Temp;

    if (!SendMsg.empty() && DoItem->GetOwner())
    {
        sDoAddon->SendPacketToClient(DoItem->GetOwner(), "ClientAtGuidData", SendMsg);
    }

    SendMsg.clear();

    CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_RM_AT);
    Utmt->SetData(0, DoItem->GetGUIDLow());
    Utmt->SetData(1, DoItem->GetEntry());
    Utmt->SetData(2, DoItem->RmAtLevel);
    Utmt->SetData(3, DoItem->Prefix);
    Utmt->SetData(4, DoItem->Suffix);
    Utmt->SetData(5, DoItem->LevelStr);
    Utmt->SetData(6, AsInt64ToString(DoItem->AsMinWDmg));
    Utmt->SetData(7, AsInt64ToString(DoItem->AsMaxWDmg));

    std::sort(DoItem->RmAtStatStore.begin(), DoItem->RmAtStatStore.end(), [](auto const& X, auto const& Y) { return (X.first != -1 && Y.first != -1 && X.first < Y.first); });

    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
    {
        Utmt->SetData(i * 2 + 8, DoItem->RmAtStatStore[i].first);
        Utmt->SetData(i * 2 + 9, AsInt64ToString(DoItem->RmAtStatStore[i].second));
    }

    for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
        Utmt->SetData(i + 40, DoItem->RmAtSpell[i]);

    //------------   随机属性倍率   ------------
    for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
        Utmt->SetData(i + 48, AsInt64ToString(NormalizeRandomAttributeMultiplier(DoItem->_Attribute[i])));
    //------------   随机属性倍率   ------------

    CharacterDatabase.Execute(Utmt);

}

void ItemRandomAtMgr::DoItemAtLoadForDB(Item* DoItem)
{
    if (!DoItem || sHelper->GetBoolean(22011))
        return;

    ItemTemplate const* Temp = DoItem->GetTemplate();
    if (!Temp)
        return;

    auto itr = ItemAtCacheStore.find(DoItem->GetGUIDLow());
    if (itr != ItemAtCacheStore.end())
    {
        ItemAtCacheTemp const& cacheData = itr->second;
        DoItem->RmAtLevel = cacheData.ULevel;
        DoItem->Prefix = cacheData.Prefix;
        DoItem->Suffix = cacheData.Suffix;
        DoItem->LevelStr = cacheData.LevelStr;
        SetRandomItemWeaponDamage(DoItem, cacheData.MinWpDmg, cacheData.MaxWpDmg);
        for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
        {
            DoItem->RmAtStatStore[i].first = cacheData.AtStatStore[i].first;
            DoItem->RmAtStatStore[i].second = cacheData.AtStatStore[i].second;
        }
        for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
            DoItem->RmAtSpell[i] = cacheData.AtSpell[i];

        //------------   随机属性倍率   ------------
        for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
            DoItem->_Attribute[i] = NormalizeRandomAttributeMultiplier(cacheData._AtSpell[i]);
        //------------   随机属性倍率   ------------
    }
}

uint32 ItemRandomAtMgr::GetSuperUpRTempId(uint32 ItemId, uint32 Level)
{
    if (!IsGenerateItemAt(ItemId))
        return 0;

    uint32 RTempId = 0;
    for (auto Itr = SuperUpSellStore.begin(); Itr != SuperUpSellStore.end(); ++Itr)
    {
        if (Itr->ItemId > 0)
        {
            if (Itr->ItemId == ItemId && Itr->Level == Level)
            {
                RTempId = Itr->RTempId;
                break;
            }
        }
        else
        {
            if (Itr->Level == Level)
                RTempId = Itr->RTempId;
        }
    }

    return RTempId;
}

bool ItemRandomAtMgr::DoSuperUpSellItem(Player* player, uint32 ItemId, uint32 Level, uint32 Count)
{
    uint32 RTempId = GetSuperUpRTempId(ItemId, Level);
    if (!RTempId)
        return false;

    sHelper->DoTheReward(player, RTempId, "出售超级随机装备奖励", Count);
    return true;
}

bool ItemRandomAtMgr::IsInItemSuperSellNow(uint32 ItemId)
{
    if (!IsGenerateItemAt(ItemId))
        return false;

    bool In = false;
    for (auto Itr = SuperUpSellStore.begin(); Itr != SuperUpSellStore.end(); ++Itr)
    {
        if (Itr->ItemId == 0 || Itr->ItemId == ItemId)
            In = true;
    }

    return In;
}

bool ItemRandomAtMgr::IsInItemSuperUp(uint32 ItemId)
{
    return IsGenerateItemAt(ItemId);
}

uint32 ItemRandomAtMgr::GetNowULevel(uint32 GuidLow)
{
    auto Itr = ItemAtCacheStore.find(GuidLow);
    return Itr != ItemAtCacheStore.end() ? Itr->second.ULevel : 0;
}

uint32 ItemRandomAtMgr::GetNextULevel(uint32 GuidLow, uint32 ItemId)
{
    uint32 NextULevel = GetNowULevel(GuidLow) + 1;
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0;

    for (auto Itr = Iter->second.begin(); Itr != Iter->second.end(); Itr++)
    {
        if (Itr->ULevel == NextULevel)
            return NextULevel;
    }

    return 0;
}

uint32 ItemRandomAtMgr::GetMaxULevel(uint32 ItemId)
{
    uint32 Max = 0;
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return Max;

    return uint32(Iter->second.size());
}

float ItemRandomAtMgr::GetUMod(uint32 ItemId, uint32 ULevel)
{
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0.0f;

    for (auto Itr = Iter->second.begin(); Itr != Iter->second.end(); Itr++)
    {
        if (Itr->ULevel == ULevel)
            return Itr->Mod;
    }
    return 0.0f;
}

uint32 ItemRandomAtMgr::GetUNTempId(uint32 ItemId, uint32 ULevel)
{
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0;

    for (auto Itr = Iter->second.begin(); Itr != Iter->second.end(); Itr++)
    {
        if (Itr->ULevel == ULevel)
            return Itr->NTempId;
    }
    return 0;
}

bool ItemRandomAtMgr::GetUOnSave(uint32 ItemId, uint32 ULevel)
{
    if (!ULevel)
        return true;
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0;

    for (auto Itr = Iter->second.begin(); Itr != Iter->second.end(); Itr++)
    {
        if (Itr->ULevel == ULevel)
            return (Itr->OnSave > 0);
    }
    return false;
}

float ItemRandomAtMgr::GetUChance(uint32 ItemId, uint32 ULevel)
{
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0.0f;

    for (auto Itr = Iter->second.begin(); Itr != Iter->second.end(); Itr++)
    {
        if (Itr->ULevel == ULevel)
            return Itr->UChance;
    }
    return 0.0f;
}

float ItemRandomAtMgr::GetDChance(uint32 ItemId, uint32 ULevel)
{
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0.0f;

    for (auto Itr = Iter->second.begin(); Itr != Iter->second.end(); Itr++)
    {
        if (Itr->ULevel == ULevel)
            return Itr->DChance;
    }
    return 0.0f;
}

uint32 ItemRandomAtMgr::GetDLevel(uint32 ItemId, uint32 ULevel)
{
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0;

    for (auto Itr = Iter->second.begin(); Itr != Iter->second.end(); Itr++)
    {
        if (Itr->ULevel == ULevel)
            return Itr->DLevel;
    }
    return 0;
}

uint32 ItemRandomAtMgr::GetNewAddToLevel(uint32 ItemId)
{
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0;

    std::vector<float> ListKv;
    std::map<uint32, float> Rs;

    for (auto Itr = Iter->second.begin(); Itr != Iter->second.end(); Itr++)
    {
        if (Itr->NewAddChance > 0.0f)
        {
            ListKv.push_back(Itr->NewAddChance);
            Rs.insert(std::make_pair(Itr->ULevel, Itr->NewAddChance));
        }
    }

    if (Rs.empty())
        return 0;

    uint32 NewLevel = 0;
    float RIdx = 0.0f;
    for (auto Itr = Rs.begin(); Itr != Rs.end(); Itr++)
        RIdx += Itr->second;

    for (auto It = Rs.begin(); It != Rs.end(); It++)
    {
        if (frand(0, RIdx) <= It->second)
        {
            NewLevel = It->first;
            break;
        }
        else
            RIdx -= It->second;
    }

    return NewLevel;
}

uint32 ItemRandomAtMgr::GetNewId(uint32 ItemId, uint32 ULevel)
{
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0;

    for (auto Itr = Iter->second.begin(); Itr != Iter->second.end(); Itr++)
    {
        if (Itr->ULevel == ULevel)
            return Itr->NewId;
    }
    return 0;
}

void ItemRandomAtMgr::OnApplyItemAtStoreStat(Player* player, Item* KItem, uint8 Slot, bool Apply)
{
    uint32 ItemId = KItem->GetEntry();
    if (!IsGenerateItemAt(ItemId))
        return;

    ItemTemplate const* Proto = KItem->GetTemplate();
    if (Slot >= INVENTORY_SLOT_BAG_END || !Proto)
        return;

    ApplyRandomItemStat(player, KItem, Slot, Apply, true);

    //------------   随机属性倍率   ------------
    for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
    {
        int8 StatType = 0;
        if (i == 0)
            StatType = 3;
        if (i == 1)
            StatType = 4;
        if (i == 2)
            StatType = 5;
        if (i == 3)
            StatType = 6;
        if (i == 4)
            StatType = 7;
        if (i == 5)
            StatType = 38;
        if (i == 6)
            StatType = 45;

        AsInt64 Val = NormalizeRandomAttributeMultiplier(KItem->_Attribute[i]);
        if (Val <= 1)
            continue;

        ApplyRandomAttributeMultiplier(player, KItem, StatType, Val, Apply);
    }
    //------------   随机属性倍率   ------------
}


void ItemRandomAtMgr::OnApplyItemAtStoreSpell(Player* player, Item* KItem, bool Apply)
{
    uint32 ItemId = KItem->GetEntry();
    if (!IsGenerateItemAt(ItemId))
        return;

    for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
    {
        uint32 SpellId = KItem->RmAtSpell[i];
        if (SpellId == 0)
            continue;

        SpellInfo const* SpellTemp = sSpellMgr->GetSpellInfo(SpellId);
        if (SpellTemp)
            player->ApplyEquipSpell(SpellTemp, KItem, Apply);
    }
}

void ItemRandomAtMgr::OnApplyItemAtStoreStatToHF(Player* player, Item* KItem, bool Apply)
{
    uint32 ItemId = KItem->GetEntry();
    if (!IsGenerateItemAt(ItemId))
        return;

    ItemTemplate const* Proto = KItem->GetTemplate();
    if (!Proto)
        return;

    ApplyRandomItemStat(player, KItem, EQUIPMENT_SLOT_START, Apply, false);
}


void ItemRandomAtMgr::OnApplyItemAtStoreSpellToHF(Player* player, Item* KItem, bool Apply)
{
    uint32 ItemId = KItem->GetEntry();
    if (!IsGenerateItemAt(ItemId))
        return;

    for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
    {
        uint32 SpellId = KItem->RmAtSpell[i];
        if (SpellId == 0)
            continue;

        SpellInfo const* SpellTemp = sSpellMgr->GetSpellInfo(SpellId);
        if (SpellTemp)
            player->ApplyEquipSpell(SpellTemp, KItem, Apply);
    }
}

float ItemRandomAtMgr::GetOChance(uint32 ItemId, uint32 TargetItemId)
{
    float Chance = 0.0f;
    for (auto It = OnUpRankAtStore.begin(); It != OnUpRankAtStore.end(); ++It)
    {
        if (It->UseItemId == ItemId)
        {
            if (!It->TarItemId)
            {
                Chance = It->OChance;
                break;
            }
            else if (It->TarItemId == TargetItemId)
            {
                Chance = It->OChance;
                break;
            }
        }
    }

    return Chance;
}

bool ItemRandomAtMgr::InUpDo(uint32 ItemId, uint32 TargetItemId)
{
    float In = false;
    for (auto It = OnUpRankAtStore.begin(); It != OnUpRankAtStore.end(); ++It)
    {
        if (It->UseItemId == ItemId)
        {
            if (!It->TarItemId)
            {
                In = true;
                break;
            }
            else if (It->TarItemId == TargetItemId)
            {
                In = true;
                break;
            }
        }
    }

    return In;
}

uint32 ItemRandomAtMgr::GetInUpRank(uint32 ItemId, uint32 TargetItemId)
{
    uint32 Rank = 0;
    for (auto It = OnUpRankAtStore.begin(); It != OnUpRankAtStore.end(); ++It)
    {
        if (It->UseItemId == ItemId)
        {
            if (!It->TarItemId)
            {
                Rank = It->ORank;
                break;
            }
            else if (It->TarItemId == TargetItemId)
            {
                Rank = It->ORank;
                break;
            }
        }
    }

    return Rank;
}

bool ItemRandomAtMgr::IsWpItem(uint8 Slot, ItemTemplate const* Proto)
{
    if (!Proto)
        return false;
    bool Wp = (Slot == EQUIPMENT_SLOT_MAINHAND || Slot == EQUIPMENT_SLOT_OFFHAND || Slot == EQUIPMENT_SLOT_RANGED);
    return (Wp && Proto->Class == ITEM_CLASS_WEAPON);
}

float ItemRandomAtMgr::AutoWpMinDamage(Player* player, uint8 Slot, ItemTemplate const* Proto, Item* KItem)
{
    if (!Proto)
        return 0.0f;
    if (!KItem)
        return 0.0f;
    if (!IsWpItem(Slot, Proto))
        return 0.0f;

    AsInt64 Dmg = AsInt64Zero();

    if (IsGenerateItemAt(Proto->ItemId))
    {
        Dmg = KItem->AsMinWDmg > 0 ? KItem->AsMinWDmg : AsInt64(KItem->MinWDmg);
        float RmMod = GetModForRank(Proto->ItemId, KItem->RmAtLevel);
        Dmg = ApplyFixedPointMod(Dmg, RmMod);
    }
    else
    {
        Dmg = Proto->AsDamageMin[0] > 0 ? Proto->AsDamageMin[0] : AsInt64(uint32(Proto->Damage[0].DamageMin));
    }

    float EqMod = sPlayerModMgr->GetModPlayerEqSlot(player, Slot);
    Dmg = ApplyFixedPointMod(Dmg, EqMod);
    return float(AsWeaponDamageCompat(Dmg));
}

float ItemRandomAtMgr::AutoWpMaxDamage(Player* player, uint8 Slot, ItemTemplate const* Proto, Item* KItem)
{
    if (!Proto)
        return 0.0f;
    if (!KItem)
        return 0.0f;
    if (!IsWpItem(Slot, Proto))
        return 0.0f;

    AsInt64 Dmg = AsInt64Zero();

    if (IsGenerateItemAt(Proto->ItemId))
    {
        Dmg = KItem->AsMaxWDmg > 0 ? KItem->AsMaxWDmg : AsInt64(KItem->MaxWDmg);
        float RmMod = GetModForRank(Proto->ItemId, KItem->RmAtLevel);
        Dmg = ApplyFixedPointMod(Dmg, RmMod);
    }
    else
    {
        Dmg = Proto->AsDamageMax[0] > 0 ? Proto->AsDamageMax[0] : AsInt64(uint32(Proto->Damage[0].DamageMax));
    }

    float EqMod = sPlayerModMgr->GetModPlayerEqSlot(player, Slot);
    Dmg = ApplyFixedPointMod(Dmg, EqMod);
    return float(AsWeaponDamageCompat(Dmg));
}

uint32 ItemRandomAtMgr::GetResetNTempId(uint32 ItemId, uint32 TargetItemId)
{
    uint32 TempId = 0;
    for (auto It = ResetResAtStore.begin(); It != ResetResAtStore.end(); ++It)
    {
        if (It->UseItemId == ItemId)
        {
            if (!It->TarItemId)
            {
                TempId = It->NTempId;
                break;
            }
            else if (It->TarItemId == TargetItemId)
            {
                TempId = It->NTempId;
                break;
            }
        }
    }

    return TempId;
}

float ItemRandomAtMgr::GetResetOChance(uint32 ItemId, uint32 TargetItemId)
{
    float Chance = 0.0f;
    for (auto It = ResetResAtStore.begin(); It != ResetResAtStore.end(); ++It)
    {
        if (It->UseItemId == ItemId)
        {
            if (!It->TarItemId)
            {
                Chance = It->UChance;
                break;
            }
            else if (It->TarItemId == TargetItemId)
            {
                Chance = It->UChance;
                break;
            }
        }
    }

    return Chance;
}

bool ItemRandomAtMgr::InResetOnSave(uint32 ItemId, uint32 TargetItemId)
{
    bool Save = true;
    for (auto It = ResetResAtStore.begin(); It != ResetResAtStore.end(); ++It)
    {
        if (It->UseItemId == ItemId)
        {
            if (!It->TarItemId)
            {
                Save = It->OnSave;
                break;
            }
            else if (It->TarItemId == TargetItemId)
            {
                Save = It->OnSave;
                break;
            }
        }
    }

    return Save;
}

bool ItemRandomAtMgr::InResetOnAtUpSave(uint32 ItemId, uint32 TargetItemId)
{
    bool Save = true;
    for (auto It = ResetResAtStore.begin(); It != ResetResAtStore.end(); ++It)
    {
        if (It->UseItemId == ItemId)
        {
            if (!It->TarItemId)
            {
                Save = It->OnAtUpSave;
                break;
            }
            else if (It->TarItemId == TargetItemId)
            {
                Save = It->OnAtUpSave;
                break;
            }
        }
    }

    return Save;
}

bool ItemRandomAtMgr::IntResetDo(uint32 ItemId, uint32 TargetItemId)
{
    float In = false;
    for (auto It = ResetResAtStore.begin(); It != ResetResAtStore.end(); ++It)
    {
        if (It->UseItemId == ItemId)
        {
            if (!It->TarItemId)
            {
                In = true;
                break;
            }
            else if (It->TarItemId == TargetItemId)
            {
                In = true;
                break;
            }
        }
    }

    return In;
}

uint32 ItemRandomAtMgr::GetResetSpGroupId(uint32 ItemId, uint32 TargetItemId)
{
    uint32 Id = 0;
    for (auto It = ResetResAtStore.begin(); It != ResetResAtStore.end(); ++It)
    {
        if (It->UseItemId == ItemId)
        {
            if (!It->TarItemId)
            {
                Id = It->GroupTempId;
                break;
            }
            else if (It->TarItemId == TargetItemId)
            {
                Id = It->GroupTempId;
                break;
            }
        }
    }

    return Id;
}

std::string ItemRandomAtMgr::GetLevelStr(uint32 ItemId, uint32 Level)
{
    std::string LevelStr = "";
    if (!Level)
        return LevelStr;

    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return LevelStr;

    for (auto It = Iter->second.begin(); It != Iter->second.end(); It++)
    {
        if (It->ULevel == Level)
            LevelStr = It->LevelStr;
    }

    return LevelStr;
}

uint32 ItemRandomAtMgr::GetStTempId(Creature* Me)
{
    if (Me && Me->StTempId > 0)
        return Me->StTempId;

    uint32 MapId = Me->GetMapId();
    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank)
            return Me->StTempId;
    }

    if (!sAsCreature->UseLegacyMapCreatureAttributes())
        return 0;

    if (sAsCreature->IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == sAsCreature->IsCustomBoss(Me) && Itr->CreTempId)
                return Itr->CreTempId;
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == sAsCreature->IsCustomBoss(Me) && Itr->CreTempId)
                return Itr->CreTempId;
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == sAsCreature->IsCustomBoss(Me) && Itr->CreTempId)
                return Itr->CreTempId;
        }
    }

    return 0;
}

uint32 ItemRandomAtMgr::GetSuperXyItem(uint32 ItemId, uint32 Level)
{
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0;

    for (auto It = Iter->second.begin(); It != Iter->second.end(); It++)
    {
        if (It->ULevel == Level)
            return It->XyItem;
    }

    return 0;
}

float ItemRandomAtMgr::GetSuperXyOdds(uint32 ItemId, uint32 Level)
{
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0.0f;

    for (auto It = Iter->second.begin(); It != Iter->second.end(); It++)
    {
        if (It->ULevel == Level)
            return It->XyOdds;
    }

    return 0.0f;
}

uint32 ItemRandomAtMgr::GetSuperXyCount(uint32 ItemId, uint32 Level)
{
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0;

    for (auto It = Iter->second.begin(); It != Iter->second.end(); It++)
    {
        if (It->ULevel == Level)
            return It->XyCount;
    }

    return 0;
}

uint32 ItemRandomAtMgr::GetSuperFBItem(uint32 ItemId, uint32 Level)
{
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return 0;

    for (auto It = Iter->second.begin(); It != Iter->second.end(); It++)
    {
        if (It->ULevel == Level)
            return It->FBItem;
    }

    return 0;
}

bool ItemRandomAtMgr::GetSuperFBFlag(uint32 ItemId, uint32 Level)
{
    auto Iter = UItemAtTempStore.find(ItemId);
    if (Iter == UItemAtTempStore.end())
        return false;

    for (auto It = Iter->second.begin(); It != Iter->second.end(); It++)
    {
        if (It->ULevel == Level)
            return It->Flag;
    }

    return false;
}
