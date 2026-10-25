
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

//_装备超级随机生成总表
GenerateItemAtMap GenerateItemAtStore;
//_装备超级随机模板总表
GTempItemListMap GTempItemAtStore;
//_装备超级随机属性组表
StatItemGroupMap StatItemAtStore;
//_装备超级随机技能组表
SpellItemGroupMap SpellItemAtStore;
//_装备超级随机强化总表
UItemAtTempMap UItemAtTempStore;
//_装备超级随机附魔组表
EnchItemGroupMap EnchItemAtStore;
//_装备超级强化售卖奖励表
SuperUpSellTempVect SuperUpSellStore;
//_装备超级随机强化秒升表
OnUpRankAtVect OnUpRankAtStore;
//_装备超级随机重置总表
ResetResAtVect ResetResAtStore;
//__生物超级掉落组表
RAtLootGroupVect RAtLootGroupStore;

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
    RAtLootGroupStore.clear();

    // 22011=1: use template item systems and disable the legacy super-random equipment system.
    if (sHelper->GetBoolean(22011))
        return;
    QueryResult Result = WorldDatabase.Query("SELECT 装备编号, 超级模板组号, 需求模板, 是否覆盖, 概率, 属性倍率组 FROM _装备超级随机生成总表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            GenerateItemAt St;
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            St.GroupTempId = Fields[1].Get<uint32>();
            St.NTempId = Fields[2].Get<uint32>();
            St.OnCover = Fields[3].Get<uint32>();
            St.Chance = Fields[4].Get<float>();
            St._Attris = Fields[5].Get<uint32>();

            if (ItemTemplate* TarInfo = const_cast<ItemTemplate*>(sObjectMgr->GetItemTemplate(ItemId)))
            {
                if (!TarInfo->RequiredLevel || TarInfo->RequiredLevel == 1)
                    TarInfo->RequiredLevel = 2;

                if (St.OnCover)
                {
                    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
                    {
                        TarInfo->ItemStat[i].ItemStatType = 0;
                        TarInfo->ItemStat[i].ItemStatValue = 0;
                        TarInfo->AsItemStatValue[i] = AsInt64Zero();
                    }

                    if (TarInfo->Class == ITEM_CLASS_WEAPON)
                    {
                        TarInfo->Damage[0].DamageMin = 1.0f;
                        TarInfo->Damage[0].DamageMax = 2.0f;
                        TarInfo->AsDamageMin[0] = AsInt64(1);
                        TarInfo->AsDamageMax[0] = AsInt64(2);
                        TarInfo->Damage[0].DamageType = 0;
                        TarInfo->Damage[1].DamageMin = 0;
                        TarInfo->Damage[1].DamageMax = 0;
                        TarInfo->AsDamageMin[1] = AsInt64Zero();
                        TarInfo->AsDamageMax[1] = AsInt64Zero();
                        TarInfo->Damage[1].DamageType = 0;
                    }

                    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
                    {
                        TarInfo->Spells[i].SpellId = 0;
                        TarInfo->Spells[i].SpellTrigger = 0;
                        TarInfo->Spells[i].SpellCharges = 0;
                        TarInfo->Spells[i].SpellPPMRate = 0;
                        TarInfo->Spells[i].SpellCooldown = -1;
                        TarInfo->Spells[i].SpellCategory = 0;
                        TarInfo->Spells[i].SpellCategoryCooldown = -1;
                    }
                }
            }

            GenerateItemAtStore.insert(std::make_pair(ItemId, St));
        } while (Result->NextRow());
    }

    //------------   随机属性倍率   ------------
    m_Attribute.clear();
    Result = WorldDatabase.Query("SELECT 属性倍率组, 属性类型, 最小倍率, 最大倍率, 概率 FROM _装备超级随机属性倍率;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            _Attribute st;
            Field* fields = Result->Fetch();

            st.Attri_a = fields[0].Get<uint32>();
            st.Attri_b = fields[1].Get<uint32>();
            st.Attri_c = NormalizeRandomAttributeMultiplier(AsInt64FromString(fields[2].Get<std::string>()));
            st.Attri_d = NormalizeRandomAttributeMultiplier(AsInt64FromString(fields[3].Get<std::string>()));
            st.Attri_e = fields[4].Get<float>();

            m_Attribute.insert(sx_Attribute::value_type(st.Attri_a, st));
        } while (Result->NextRow());
    }
    //------------   随机属性倍率   ------------

    GTempItemAtStore.clear();
    Result = WorldDatabase.Query("SELECT 模板组号, 序号, 几率, 前缀名称, 后缀名称, 武器最小伤害Min, 武器最小伤害Max, 武器最大伤害Min, 武器最大伤害Max, 属性组, 属性条数, 技能组, 技能条数, 随机附魔组, 随机附魔条数 FROM _装备超级随机模板总表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            GTempItemAt St;
            St.GroupTempId = Fields[0].Get<uint32>();
            St.Num = Fields[1].Get<uint32>();
            St.Chance = Fields[2].Get<float>();
            St.Prefix = Fields[3].Get<std::string>();
            St.Suffix = Fields[4].Get<std::string>();
            St.MinWpDmg1 = AsInt64FromString(Fields[5].Get<std::string>());
            St.MinWpDmg2 = AsInt64FromString(Fields[6].Get<std::string>());
            St.MaxWpDmg1 = AsInt64FromString(Fields[7].Get<std::string>());
            St.MaxWpDmg2 = AsInt64FromString(Fields[8].Get<std::string>());
            St.StatGroupId = Fields[9].Get<uint32>();
            St.StatCount = Fields[10].Get<uint32>();
            St.SpellGroupId = Fields[11].Get<uint32>();
            St.SpellCount = Fields[12].Get<uint32>();
            St.EnchGroupId = Fields[13].Get<uint32>();
            St.EnchCount = Fields[14].Get<uint32>();
            if (St.StatCount > MAX_AT_A_COUNT)
                St.StatCount = MAX_AT_A_COUNT;
            if (St.SpellCount > MAX_AT_B_COUNT)
                St.SpellCount = MAX_AT_B_COUNT;
            if (St.EnchCount > MAX_EN_COUNT)
                St.EnchCount = MAX_EN_COUNT;

            GTempItemAtStore[St.GroupTempId].push_back(St);

        } while (Result->NextRow());
    }

    StatItemAtStore.clear();
    Result = WorldDatabase.Query("SELECT 组号, 属性类型, 最小值, 最大值, 生效几率 FROM _装备超级随机属性组表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            StatItemAt St;
            St.GroupId = Fields[0].Get<uint32>();
            St.SType = Fields[1].Get<int32>();
            St.MinValue = AsInt64FromString(Fields[2].Get<std::string>());
            St.MaxValue = AsInt64FromString(Fields[3].Get<std::string>());
            St.Chance = Fields[4].Get<float>();
            if (St.MinValue > St.MaxValue)
                St.MinValue = St.MaxValue;

            StatItemAtStore[St.GroupId].push_back(St);

        } while (Result->NextRow());
    }

    SpellItemAtStore.clear();
    Result = WorldDatabase.Query("SELECT 组号, 技能编号, 生效几率 FROM _装备超级随机技能组表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            SpellItemAt St;
            St.GroupId = Fields[0].Get<uint32>();
            St.SpellId = Fields[1].Get<uint32>();
            St.Chance = Fields[2].Get<float>();

            SpellItemAtStore[St.GroupId].push_back(St);

        } while (Result->NextRow());
    }

    EnchItemAtStore.clear();
    Result = WorldDatabase.Query("SELECT 组号, 附魔编号, 生效几率 FROM _装备超级随机附魔组表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            EnchItemAt St;
            St.GroupId = Fields[0].Get<uint32>();
            St.EnchId = Fields[1].Get<uint32>();
            St.Chance = Fields[2].Get<float>();

            EnchItemAtStore[St.GroupId].push_back(St);

        } while (Result->NextRow());
    }

    UItemAtTempStore.clear();
    Result = WorldDatabase.Query("SELECT 装备编号, 等级, 直接强化几率, 强化百分比, 技能组, 技能条数, 需求模版, 是否保留随机附魔, 是否保留超级技能, 成功率, 失败后降级的几率, 被降低的等级, 强化后新的编号, 强化名称, 幸运宝石, 幸运几率, 幸运数量, 防爆宝石, 是否碎裂 FROM _装备超级随机强化总表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            UItemAtTemp St;
            St.ItemId = Fields[0].Get<uint32>();
            St.ULevel = Fields[1].Get<uint32>();
            St.NewAddChance = Fields[2].Get<float>();
            St.Mod = Fields[3].Get<float>();
            St.SpellGroupId = Fields[4].Get<uint32>();
            St.SpellCount = Fields[5].Get<uint32>();
            St.NTempId = Fields[6].Get<uint32>();
            St.OnSave = Fields[7].Get<uint32>();
            St.OnAtUpSave = Fields[8].Get<uint32>();
            St.UChance = Fields[9].Get<float>();
            St.DChance = Fields[10].Get<float>();
            St.DLevel = Fields[11].Get<uint32>();
            St.NewId = Fields[12].Get<uint32>();
            St.LevelStr = Fields[13].Get<std::string>();
            St.XyItem = Fields[14].Get<uint32>();
            St.XyOdds = Fields[15].Get<float>();
            St.XyCount = Fields[16].Get<uint32>();
            St.FBItem = Fields[17].Get<uint32>();
            St.Flag = Fields[18].Get<bool>();

            UItemAtTempStore[St.ItemId].push_back(St);

        } while (Result->NextRow());
    }

    SuperUpSellStore.clear();
    Result = WorldDatabase.Query("SELECT 装备编号, 等级, 奖励模版 FROM _装备超级强化售卖奖励表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            SuperUpSellTemp St;
            St.ItemId = Fields[0].Get<uint32>();
            St.Level = Fields[1].Get<uint32>();
            St.RTempId = Fields[2].Get<uint32>();

            SuperUpSellStore.push_back(St);

        } while (Result->NextRow());
    }

    OnUpRankAtStore.clear();
    Result = WorldDatabase.Query("SELECT 使用物品编号, 目标装备编号, 秒升等级, 几率 FROM _装备超级随机强化秒升表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            OnUpRankAt St;
            St.UseItemId = Fields[0].Get<uint32>();
            St.TarItemId = Fields[1].Get<uint32>();
            St.ORank = Fields[2].Get<uint32>();
            St.OChance = Fields[3].Get<float>();
            OnUpRankAtStore.push_back(St);

        } while (Result->NextRow());
    }

    ResetResAtStore.clear();
    Result = WorldDatabase.Query("SELECT 使用物品编号, 目标装备编号, 超级模板组号, 需求模板, 是否保留随机附魔, 是否保留超级技能, 概率 FROM _装备超级随机重置总表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            ResetResAt St;
            St.UseItemId = Fields[0].Get<uint32>();
            St.TarItemId = Fields[1].Get<uint32>();
            St.GroupTempId = Fields[2].Get<uint32>();
            St.NTempId = Fields[3].Get<uint32>();
            St.OnSave = Fields[4].Get<uint32>();
            St.OnAtUpSave = Fields[5].Get<uint32>();
            St.UChance = Fields[6].Get<float>();
            ResetResAtStore.push_back(St);

        } while (Result->NextRow());
    }

    RAtLootGroupStore.clear();
    Result = WorldDatabase.Query("SELECT 组号, 物品编号, 几率, 超级模版组号 FROM __生物超级掉落组表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            RAtLootGroupTemp St;
            St.RAtLootGroup = Fields[0].Get<uint32>();
            St.ItemId = Fields[1].Get<uint32>();
            St.Chance = Fields[2].Get<float>();
            St.SpGTempId = Fields[3].Get<uint32>();
            RAtLootGroupStore.push_back(St);

        } while (Result->NextRow());
    }
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

void ItemRandomAtMgr::DeleteForDoVect(std::vector<DoTemp>& DoVect, int32 ItemId)
{
    for (auto Itr = DoVect.begin(); Itr != DoVect.end();)
    {
        if (Itr->ItemId == ItemId)
            Itr = DoVect.erase(Itr);
        else
            Itr++;
    }
}

std::vector<std::pair<int32, int32>> ItemRandomAtMgr::DoGenerateGroupLootItemAndStTempId(std::vector<DoTemp> DoVect, uint32 Count)
{
    std::vector<std::pair<int32, int32>> TempVect;
    if (!Count)
        return TempVect;

    for (uint32 i = 0; i < Count; i++)
    {
        std::vector<DoTemp> ChanceList;
        ChanceList.clear();

        for (auto It = DoVect.begin(); It != DoVect.end(); It++)
        {
            DoTemp St;
            St.ItemId = It->ItemId;
            St.Chance = It->Chance;
            St.SpGTempId = int32(It->SpGTempId);
            ChanceList.push_back(St);
        }

        if (!ChanceList.empty())
        {
            float RIdx = 0.0f;
            for (auto Itr = ChanceList.begin(); Itr != ChanceList.end(); Itr++)
                RIdx += Itr->Chance;

            for (auto It = ChanceList.begin(); It != ChanceList.end(); It++)
            {
                if (frand(0, RIdx) <= It->Chance)
                {
                    int32 ItemId = int32(It->ItemId);
                    int32 SpGTempId = It->SpGTempId;
                    TempVect.push_back(std::pair(ItemId, SpGTempId));
                    DeleteForDoVect(DoVect, int32(It->ItemId));
                    break;
                }
                else
                    RIdx -= It->Chance;
            }

            ChanceList.clear();
        }
    }

    return TempVect;
}

void ItemRandomAtMgr::AsRandomAtCustomFillLoot(Creature* Me, Loot* OnLoot, Player* Looter)
{
    if (sHelper->GetBoolean(22011))
        return;

    if (sAsCreature->IsNotFor(Me))
        return;

    uint32 StTempId = GetStTempId(Me);
    auto Itr = AtCreatureStatTempStore.find(StTempId);
    if (Itr == AtCreatureStatTempStore.end())
        return;

    if (!Itr->second.RAtLootGroupId || Itr->second.RAtLootChance <= 0.0f)
        return;

    if (!roll_chance_f(Itr->second.RAtLootChance))
        return;

    uint32 GroupId = Itr->second.RAtLootGroupId;
    std::vector<DoTemp> TempStore;
    for (auto It = RAtLootGroupStore.begin(); It != RAtLootGroupStore.end(); It++)
    {
        if (It->RAtLootGroup == GroupId)
        {
            DoTemp St;
            St.ItemId = It->ItemId;
            St.Chance = It->Chance;
            St.SpGTempId = It->SpGTempId;
            TempStore.push_back(St);
        }
    }

    if (TempStore.empty())
        return;

    uint32 GCount = urand(Itr->second.RAtLootMin, Itr->second.RAtLootMax);
    auto TempVect = DoGenerateGroupLootItemAndStTempId(TempStore, GCount);
    if (!TempVect.empty())
    {
        if (OnLoot->empty())
            OnLoot->FillLoot(0, LootTemplates_Creature, Looter, false, true, Me->GetLootMode(), Me);

        for (auto It = TempVect.begin(); It != TempVect.end(); It++)
        {
            LootStoreItem StoreItem = LootStoreItem(uint32(It->first), 0, 100.0f, false, LOOT_MODE_DEFAULT, 0, 1, 1);
            OnLoot->AddItem(StoreItem, Looter, It->second);
        }
    }
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
