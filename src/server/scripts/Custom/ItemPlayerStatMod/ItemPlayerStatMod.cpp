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
#include "ItemPlayerStatMod.h"
#include <Custom/EventMgr/EventMgr.h>
#include <array>
#pragma execution_character_set("utf-8")

ZsStatTempMap ZsStatTempStore;

namespace
{
struct ZsStatFieldDef
{
    int32 Mod;
    char const* Name;
    AsInt64 ZsStatTemp::*Value;
};

std::array<ZsStatFieldDef, 44> const ZsStatFields =
{ {
    { ITEM_MOD_MANA, "法力值", &ZsStatTemp::S00 },
    { ITEM_MOD_HEALTH, "生命值", &ZsStatTemp::S01 },
    { ITEM_MOD_AGILITY, "敏捷", &ZsStatTemp::S03 },
    { ITEM_MOD_STRENGTH, "力量", &ZsStatTemp::S04 },
    { ITEM_MOD_INTELLECT, "智力", &ZsStatTemp::S05 },
    { ITEM_MOD_SPIRIT, "精神", &ZsStatTemp::S06 },
    { ITEM_MOD_STAMINA, "耐力", &ZsStatTemp::S07 },
    { ITEM_MOD_DEFENSE_SKILL_RATING, "防御等级", &ZsStatTemp::S12 },
    { ITEM_MOD_DODGE_RATING, "躲闪等级", &ZsStatTemp::S13 },
    { ITEM_MOD_PARRY_RATING, "招架等级", &ZsStatTemp::S14 },
    { ITEM_MOD_BLOCK_RATING, "格挡等级", &ZsStatTemp::S15 },
    { ITEM_MOD_HIT_MELEE_RATING, "近战命中等级", &ZsStatTemp::S16 },
    { ITEM_MOD_HIT_RANGED_RATING, "远程命中等级", &ZsStatTemp::S17 },
    { ITEM_MOD_HIT_SPELL_RATING, "法术命中等级", &ZsStatTemp::S18 },
    { ITEM_MOD_CRIT_MELEE_RATING, "近战暴击等级", &ZsStatTemp::S19 },
    { ITEM_MOD_CRIT_RANGED_RATING, "远程暴击等级", &ZsStatTemp::S20 },
    { ITEM_MOD_CRIT_SPELL_RATING, "法术暴击等级", &ZsStatTemp::S21 },
    { ITEM_MOD_HIT_TAKEN_MELEE_RATING, "近战命中躲闪等级", &ZsStatTemp::S22 },
    { ITEM_MOD_HIT_TAKEN_RANGED_RATING, "远程命中躲闪等级", &ZsStatTemp::S23 },
    { ITEM_MOD_HIT_TAKEN_SPELL_RATING, "法术命中躲闪等级", &ZsStatTemp::S24 },
    { ITEM_MOD_CRIT_TAKEN_MELEE_RATING, "近战暴击躲闪等级", &ZsStatTemp::S25 },
    { ITEM_MOD_CRIT_TAKEN_RANGED_RATING, "远程暴击躲闪等级", &ZsStatTemp::S26 },
    { ITEM_MOD_CRIT_TAKEN_SPELL_RATING, "法术暴击躲闪等级", &ZsStatTemp::S27 },
    { ITEM_MOD_HASTE_MELEE_RATING, "近战急速等级", &ZsStatTemp::S28 },
    { ITEM_MOD_HASTE_RANGED_RATING, "远程急速等级", &ZsStatTemp::S29 },
    { ITEM_MOD_HASTE_SPELL_RATING, "法术急速等级", &ZsStatTemp::S30 },
    { ITEM_MOD_HIT_RATING, "命中等级", &ZsStatTemp::S31 },
    { ITEM_MOD_CRIT_RATING, "暴击等级", &ZsStatTemp::S32 },
    { ITEM_MOD_HIT_TAKEN_RATING, "命中躲闪等级", &ZsStatTemp::S33 },
    { ITEM_MOD_CRIT_TAKEN_RATING, "暴击躲闪等级", &ZsStatTemp::S34 },
    { ITEM_MOD_RESILIENCE_RATING, "韧性等级", &ZsStatTemp::S35 },
    { ITEM_MOD_HASTE_RATING, "急速等级", &ZsStatTemp::S36 },
    { ITEM_MOD_EXPERTISE_RATING, "精准等级", &ZsStatTemp::S37 },
    { ITEM_MOD_ATTACK_POWER, "攻击强度", &ZsStatTemp::S38 },
    { ITEM_MOD_RANGED_ATTACK_POWER, "远程攻击强度", &ZsStatTemp::S39 },
    { -1, "形态攻击强度", &ZsStatTemp::S40 },
    { ITEM_MOD_SPELL_HEALING_DONE, "法术治疗效果", &ZsStatTemp::S41 },
    { ITEM_MOD_SPELL_DAMAGE_DONE, "法术伤害效果", &ZsStatTemp::S42 },
    { ITEM_MOD_MANA_REGENERATION, "每5秒回复法力", &ZsStatTemp::S43 },
    { ITEM_MOD_ARMOR_PENETRATION_RATING, "护甲穿透等级", &ZsStatTemp::S44 },
    { ITEM_MOD_SPELL_POWER, "法术强度提高", &ZsStatTemp::S45 },
    { ITEM_MOD_HEALTH_REGEN, "每5秒回复生命", &ZsStatTemp::S46 },
    { ITEM_MOD_SPELL_PENETRATION, "法术穿透", &ZsStatTemp::S47 },
    { ITEM_MOD_BLOCK_VALUE, "格挡值提高", &ZsStatTemp::S48 }
} };

AsInt64 ReadAsInt64(Field* fields, uint32 index)
{
    return AsInt64FromString(fields[index].Get<std::string>());
}

void SendAsStatApplyMessage(Player* player, char const* name, AsInt64 const& value)
{
    std::string text = AsInt64ToString(value);
    player->SendSysMsg("[属性突破]: %s 增加 |cFFFF6699%s|r", name, text.c_str());
}

bool IsCombatRatingItemMod(ItemModType mod)
{
    switch (mod)
    {
    case ITEM_MOD_DEFENSE_SKILL_RATING:
    case ITEM_MOD_DODGE_RATING:
    case ITEM_MOD_PARRY_RATING:
    case ITEM_MOD_BLOCK_RATING:
    case ITEM_MOD_HIT_MELEE_RATING:
    case ITEM_MOD_HIT_RANGED_RATING:
    case ITEM_MOD_HIT_SPELL_RATING:
    case ITEM_MOD_CRIT_MELEE_RATING:
    case ITEM_MOD_CRIT_RANGED_RATING:
    case ITEM_MOD_CRIT_SPELL_RATING:
    case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
    case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
    case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
    case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
    case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
    case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
    case ITEM_MOD_HASTE_MELEE_RATING:
    case ITEM_MOD_HASTE_RANGED_RATING:
    case ITEM_MOD_HASTE_SPELL_RATING:
    case ITEM_MOD_HIT_RATING:
    case ITEM_MOD_CRIT_RATING:
    case ITEM_MOD_HIT_TAKEN_RATING:
    case ITEM_MOD_CRIT_TAKEN_RATING:
    case ITEM_MOD_RESILIENCE_RATING:
    case ITEM_MOD_HASTE_RATING:
    case ITEM_MOD_EXPERTISE_RATING:
    case ITEM_MOD_ARMOR_PENETRATION_RATING:
        return true;
    default:
        return false;
    }
}

int32 GetCompatValueForItemMod(ItemModType mod, AsInt64 const& value)
{
    // Combat ratings feed fixed 3.3.5 int32 client/server rating fields. Keep the
    // real AsInt64 value in the custom stat store, but only send a safe value to
    // the vanilla rating formulas; hit chance itself is capped to 100% later.
    if (IsCombatRatingItemMod(mod))
        return AsInt64ToInt32Clamp(value, 1000000);

    return AsInt64ToInt32Clamp(value);
}
}

void ItemPlayerStatMod::LoadItemPlayerStatMod()
{
    ZsStatTempStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 物品编号, 是否消耗, 法力值, 生命值, 敏捷, 力量, 智力, 精神, 耐力, 防御等级, 躲闪等级, 招架等级, 格挡等级,"
        "近战命中等级, 远程命中等级, 法术命中等级, 近战暴击等级, 远程暴击等级, 法术暴击等级, 近战命中躲闪等级, 远程命中躲闪等级, 法术命中躲闪等级, 近战暴击躲闪等级, 远程暴击躲闪等级, 法术暴击躲闪等级, 近战急速等级, 远程急速等级, 法术急速等级, 命中等级,"
        "暴击等级, 命中躲闪等级, 暴击躲闪等级, 韧性等级, 急速等级, 精准等级, 攻击强度, 远程攻击强度, 形态攻击强度, 法术治疗效果, 法术伤害效果, 每5秒回复法力, 护甲穿透等级, 法术强度提高, 每5秒回复生命, 法术穿透, 格挡值提高 FROM _物品属性奖励表;");

    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            ZsStatTemp St;
            St.OnDel = Fields[1].Get<bool>();

            for (uint32 i = 0; i < ZsStatFields.size(); ++i)
                St.*(ZsStatFields[i].Value) = ReadAsInt64(Fields, i + 2);

            ZsStatTempStore.insert(std::make_pair(ItemId, St));

        } while (Result->NextRow());
    }
}

bool ItemPlayerStatMod::OnItemUseNow(Player* player, Item* PItem)
{
    if (!PItem)
        return true;

    uint32 ItemId = PItem->GetEntry();
    auto Itr = ZsStatTempStore.find(ItemId);
    if (Itr != ZsStatTempStore.end())
    {
        for (ZsStatFieldDef const& def : ZsStatFields)
        {
            AsInt64 const& value = Itr->second.*(def.Value);
            if (!AsInt64IsZero(value))
            {
                if (def.Mod >= 0)
                    player->DoApplyStat(ItemModType(def.Mod), value);
                SendAsStatApplyMessage(player, def.Name, value);
            }
        }

        CharacterDatabasePreparedStatement* SelectStats = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ZS_STAT_PLAYER);
        SelectStats->SetData(0, player->GetGUIDLow());
        PreparedQueryResult CurrentStats = CharacterDatabase.Query(SelectStats);
        if (!CurrentStats)
        {
            LOG_WARN("sql.sql", "无法更新玩家 {} 的属性奖励存档：找不到玩家记录", player->GetGUID().ToString());
            return true;
        }

        Field* CurrentFields = CurrentStats->Fetch();
        std::array<AsInt64, ZsStatFields.size()> NewValues{};
        for (uint32 i = 0; i < ZsStatFields.size(); ++i)
            NewValues[i] = ReadAsInt64(CurrentFields, i) + Itr->second.*(ZsStatFields[i].Value);

        CharacterDatabasePreparedStatement* UItmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ZS_STAT_PLAYER);
        for (uint32 i = 0; i < NewValues.size(); ++i)
            UItmt->SetData(i, AsInt64ToString(NewValues[i]));

        UItmt->SetData(uint32(NewValues.size()), player->GetGUIDLow());
        CharacterDatabase.Execute(UItmt);

        if (Itr->second.OnDel)
        {
            player->DestroyItemCount(ItemId, 1, true);
            return true;
        }
    }

    return false;
}

void ItemPlayerStatMod::OnPlayerLoginLoadStat(Player* player)
{
    if (IsXsPlayerbot(player))
        return;

    CharacterDatabasePreparedStatement* Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ZS_STAT_PLAYER);
    Stmt->SetData(0, player->GetGUIDLow());
    PreparedQueryResult Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();

            for (uint32 i = 0; i < ZsStatFields.size(); ++i)
            {
                AsInt64 value = ReadAsInt64(Fields, i);
                if (!AsInt64IsZero(value) && ZsStatFields[i].Mod >= 0)
                    player->DoApplyStat(ItemModType(ZsStatFields[i].Mod), value);
            }

        } while (Result->NextRow());
    }
    else
    {
        CharacterDatabasePreparedStatement* Itmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_ZS_STAT_PLAYER);
        Itmt->SetData(0, player->GetGUIDLow());
        CharacterDatabase.Execute(Itmt);
    }
}

namespace
{
void ApplyAsCombatRating(Player* player, CombatRating rating, AsInt64 const& value, AsInt64 const& compatValue, bool apply)
{
    if (!player || AsInt64IsZero(value))
        return;

    uint32 key = uint32(rating);
    player->AsCombatRatingBonusStore[key] += apply ? value : -value;
    player->AsCombatRatingCompatBonusStore[key] += apply ? compatValue : -compatValue;
}

void ApplyAsCombatRatingForItemMod(Player* player, ItemModType type, AsInt64 const& value, AsInt64 const& compatValue, bool apply)
{
    switch (type)
    {
    case ITEM_MOD_DEFENSE_SKILL_RATING:
        ApplyAsCombatRating(player, CR_DEFENSE_SKILL, value, compatValue, apply);
        break;
    case ITEM_MOD_DODGE_RATING:
        ApplyAsCombatRating(player, CR_DODGE, value, compatValue, apply);
        break;
    case ITEM_MOD_PARRY_RATING:
        ApplyAsCombatRating(player, CR_PARRY, value, compatValue, apply);
        break;
    case ITEM_MOD_BLOCK_RATING:
        ApplyAsCombatRating(player, CR_BLOCK, value, compatValue, apply);
        break;
    case ITEM_MOD_HIT_MELEE_RATING:
        ApplyAsCombatRating(player, CR_HIT_MELEE, value, compatValue, apply);
        break;
    case ITEM_MOD_HIT_RANGED_RATING:
        ApplyAsCombatRating(player, CR_HIT_RANGED, value, compatValue, apply);
        break;
    case ITEM_MOD_HIT_SPELL_RATING:
        ApplyAsCombatRating(player, CR_HIT_SPELL, value, compatValue, apply);
        break;
    case ITEM_MOD_CRIT_MELEE_RATING:
        ApplyAsCombatRating(player, CR_CRIT_MELEE, value, compatValue, apply);
        break;
    case ITEM_MOD_CRIT_RANGED_RATING:
        ApplyAsCombatRating(player, CR_CRIT_RANGED, value, compatValue, apply);
        break;
    case ITEM_MOD_CRIT_SPELL_RATING:
        ApplyAsCombatRating(player, CR_CRIT_SPELL, value, compatValue, apply);
        break;
    case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
        ApplyAsCombatRating(player, CR_HIT_TAKEN_MELEE, value, compatValue, apply);
        break;
    case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
        ApplyAsCombatRating(player, CR_HIT_TAKEN_RANGED, value, compatValue, apply);
        break;
    case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
        ApplyAsCombatRating(player, CR_HIT_TAKEN_SPELL, value, compatValue, apply);
        break;
    case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
        ApplyAsCombatRating(player, CR_CRIT_TAKEN_MELEE, value, compatValue, apply);
        break;
    case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
        ApplyAsCombatRating(player, CR_CRIT_TAKEN_RANGED, value, compatValue, apply);
        break;
    case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
        ApplyAsCombatRating(player, CR_CRIT_TAKEN_SPELL, value, compatValue, apply);
        break;
    case ITEM_MOD_HASTE_MELEE_RATING:
        ApplyAsCombatRating(player, CR_HASTE_MELEE, value, compatValue, apply);
        break;
    case ITEM_MOD_HASTE_RANGED_RATING:
        ApplyAsCombatRating(player, CR_HASTE_RANGED, value, compatValue, apply);
        break;
    case ITEM_MOD_HASTE_SPELL_RATING:
        ApplyAsCombatRating(player, CR_HASTE_SPELL, value, compatValue, apply);
        break;
    case ITEM_MOD_HIT_RATING:
        ApplyAsCombatRating(player, CR_HIT_MELEE, value, compatValue, apply);
        ApplyAsCombatRating(player, CR_HIT_RANGED, value, compatValue, apply);
        ApplyAsCombatRating(player, CR_HIT_SPELL, value, compatValue, apply);
        break;
    case ITEM_MOD_CRIT_RATING:
        ApplyAsCombatRating(player, CR_CRIT_MELEE, value, compatValue, apply);
        ApplyAsCombatRating(player, CR_CRIT_RANGED, value, compatValue, apply);
        ApplyAsCombatRating(player, CR_CRIT_SPELL, value, compatValue, apply);
        break;
    case ITEM_MOD_HIT_TAKEN_RATING:
        ApplyAsCombatRating(player, CR_HIT_TAKEN_MELEE, value, compatValue, apply);
        ApplyAsCombatRating(player, CR_HIT_TAKEN_RANGED, value, compatValue, apply);
        ApplyAsCombatRating(player, CR_HIT_TAKEN_SPELL, value, compatValue, apply);
        break;
    case ITEM_MOD_CRIT_TAKEN_RATING:
    case ITEM_MOD_RESILIENCE_RATING:
        ApplyAsCombatRating(player, CR_CRIT_TAKEN_MELEE, value, compatValue, apply);
        ApplyAsCombatRating(player, CR_CRIT_TAKEN_RANGED, value, compatValue, apply);
        ApplyAsCombatRating(player, CR_CRIT_TAKEN_SPELL, value, compatValue, apply);
        break;
    case ITEM_MOD_HASTE_RATING:
        ApplyAsCombatRating(player, CR_HASTE_MELEE, value, compatValue, apply);
        ApplyAsCombatRating(player, CR_HASTE_RANGED, value, compatValue, apply);
        ApplyAsCombatRating(player, CR_HASTE_SPELL, value, compatValue, apply);
        break;
    case ITEM_MOD_EXPERTISE_RATING:
        ApplyAsCombatRating(player, CR_EXPERTISE, value, compatValue, apply);
        break;
    case ITEM_MOD_ARMOR_PENETRATION_RATING:
        ApplyAsCombatRating(player, CR_ARMOR_PENETRATION, value, compatValue, apply);
        break;
    default:
        break;
    }
}
}

void Player::ApplyAsStatBonus(ItemModType Try, AsInt64 const& Value, AsInt64 const& CompatValue, bool Apply)
{
    if (AsInt64IsZero(Value))
        return;

    uint32 key = uint32(Try);
    AsStatBonusStore[key] += Apply ? Value : -Value;
    AsStatCompatBonusStore[key] += Apply ? CompatValue : -CompatValue;
    ApplyAsCombatRatingForItemMod(this, Try, Value, CompatValue, Apply);
}

AsInt64 Player::GetAsStatBonus(ItemModType Try) const
{
    auto itr = AsStatBonusStore.find(uint32(Try));
    return itr != AsStatBonusStore.end() ? itr->second : AsInt64Zero();
}

AsInt64 Player::GetAsStatCompatBonus(ItemModType Try) const
{
    auto itr = AsStatCompatBonusStore.find(uint32(Try));
    return itr != AsStatCompatBonusStore.end() ? itr->second : AsInt64Zero();
}

AsInt64 Player::GetAsCombatRatingBonus(CombatRating Rating) const
{
    auto itr = AsCombatRatingBonusStore.find(uint32(Rating));
    return itr != AsCombatRatingBonusStore.end() ? itr->second : AsInt64Zero();
}

AsInt64 Player::GetAsCombatRatingCompatBonus(CombatRating Rating) const
{
    auto itr = AsCombatRatingCompatBonusStore.find(uint32(Rating));
    return itr != AsCombatRatingCompatBonusStore.end() ? itr->second : AsInt64Zero();
}

AsInt64 Player::GetAsRealStat(Stats Stat) const
{
    return Stat < MAX_STATS ? AsRealStats[Stat] : AsInt64Zero();
}

AsInt64 Player::GetAsRealMaxHealth() const
{
    return AsRealMaxHealth;
}

AsInt64 Player::GetAsRealMaxPower(Powers Power) const
{
    return Power < MAX_POWERS ? AsRealMaxPowers[Power] : AsInt64Zero();
}

void Player::SetAsRealStat(Stats Stat, AsInt64 const& Value)
{
    if (Stat < MAX_STATS)
        AsRealStats[Stat] = Value;
}

void Player::SetAsRealMaxHealth(AsInt64 const& Value)
{
    AsRealMaxHealth = Value;
}

void Player::SetAsRealMaxPower(Powers Power, AsInt64 const& Value)
{
    if (Power < MAX_POWERS)
        AsRealMaxPowers[Power] = Value;
}

void Player::DoApplyStat(ItemModType Try, int32 Value, bool Apply)
{
    DoApplyStat(Try, AsInt64(Value), Apply);
}

void Player::DoApplyStat(ItemModType Try, AsInt64 const& Value, bool Apply)
{
    if (AsInt64IsZero(Value))
        return;

    int32 CompatValue = GetCompatValueForItemMod(Try, Value);
    ApplyAsStatBonus(Try, Value, AsInt64(CompatValue), Apply);

    switch (Try)
    {
    case ITEM_MOD_MANA:
        HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(CompatValue), Apply);
        break;
    case ITEM_MOD_HEALTH:                           // modify HP
        HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(CompatValue), Apply);
        break;
    case ITEM_MOD_AGILITY:                          // modify agility
        HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(CompatValue), Apply);
        ApplyStatBuffMod(STAT_AGILITY, float(CompatValue), Apply);
        break;
    case ITEM_MOD_STRENGTH:                         //modify strength
        HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(CompatValue), Apply);
        ApplyStatBuffMod(STAT_STRENGTH, float(CompatValue), Apply);
        break;
    case ITEM_MOD_INTELLECT:                        //modify intellect
        HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(CompatValue), Apply);
        ApplyStatBuffMod(STAT_INTELLECT, float(CompatValue), Apply);
        break;
    case ITEM_MOD_SPIRIT:                           //modify spirit
        HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(CompatValue), Apply);
        ApplyStatBuffMod(STAT_SPIRIT, float(CompatValue), Apply);
        break;
    case ITEM_MOD_STAMINA:                          //modify stamina
        HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(CompatValue), Apply);
        ApplyStatBuffMod(STAT_STAMINA, float(CompatValue), Apply);
        break;
    case ITEM_MOD_DEFENSE_SKILL_RATING:
        ApplyRatingMod(CR_DEFENSE_SKILL, CompatValue, Apply);
        break;
    case ITEM_MOD_DODGE_RATING:
        ApplyRatingMod(CR_DODGE, CompatValue, Apply);
        break;
    case ITEM_MOD_PARRY_RATING:
        ApplyRatingMod(CR_PARRY, CompatValue, Apply);
        break;
    case ITEM_MOD_BLOCK_RATING:
        ApplyRatingMod(CR_BLOCK, CompatValue, Apply);
        break;
    case ITEM_MOD_HIT_MELEE_RATING:
        ApplyRatingMod(CR_HIT_MELEE, CompatValue, Apply);
        break;
    case ITEM_MOD_HIT_RANGED_RATING:
        ApplyRatingMod(CR_HIT_RANGED, CompatValue, Apply);
        break;
    case ITEM_MOD_HIT_SPELL_RATING:
        ApplyRatingMod(CR_HIT_SPELL, CompatValue, Apply);
        break;
    case ITEM_MOD_CRIT_MELEE_RATING:
        ApplyRatingMod(CR_CRIT_MELEE, CompatValue, Apply);
        break;
    case ITEM_MOD_CRIT_RANGED_RATING:
        ApplyRatingMod(CR_CRIT_RANGED, CompatValue, Apply);
        break;
    case ITEM_MOD_CRIT_SPELL_RATING:
        ApplyRatingMod(CR_CRIT_SPELL, CompatValue, Apply);
        break;
    case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
        ApplyRatingMod(CR_HIT_TAKEN_MELEE, CompatValue, Apply);
        break;
    case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
        ApplyRatingMod(CR_HIT_TAKEN_RANGED, CompatValue, Apply);
        break;
    case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
        ApplyRatingMod(CR_HIT_TAKEN_SPELL, CompatValue, Apply);
        break;
    case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
        ApplyRatingMod(CR_CRIT_TAKEN_MELEE, CompatValue, Apply);
        break;
    case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
        ApplyRatingMod(CR_CRIT_TAKEN_RANGED, CompatValue, Apply);
        break;
    case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
        ApplyRatingMod(CR_CRIT_TAKEN_SPELL, CompatValue, Apply);
        break;
    case ITEM_MOD_HASTE_MELEE_RATING:
        ApplyRatingMod(CR_HASTE_MELEE, CompatValue, Apply);
        break;
    case ITEM_MOD_HASTE_RANGED_RATING:
        ApplyRatingMod(CR_HASTE_RANGED, CompatValue, Apply);
        break;
    case ITEM_MOD_HASTE_SPELL_RATING:
        ApplyRatingMod(CR_HASTE_SPELL, CompatValue, Apply);
        break;
    case ITEM_MOD_HIT_RATING:
        ApplyRatingMod(CR_HIT_MELEE, CompatValue, Apply);
        ApplyRatingMod(CR_HIT_RANGED, CompatValue, Apply);
        ApplyRatingMod(CR_HIT_SPELL, CompatValue, Apply);
        break;
    case ITEM_MOD_CRIT_RATING:
        ApplyRatingMod(CR_CRIT_MELEE, CompatValue, Apply);
        ApplyRatingMod(CR_CRIT_RANGED, CompatValue, Apply);
        ApplyRatingMod(CR_CRIT_SPELL, CompatValue, Apply);
        break;
    case ITEM_MOD_HIT_TAKEN_RATING:
        ApplyRatingMod(CR_HIT_TAKEN_MELEE, CompatValue, Apply);
        ApplyRatingMod(CR_HIT_TAKEN_RANGED, CompatValue, Apply);
        ApplyRatingMod(CR_HIT_TAKEN_SPELL, CompatValue, Apply);
        break;
    case ITEM_MOD_CRIT_TAKEN_RATING:
    case ITEM_MOD_RESILIENCE_RATING:
        ApplyRatingMod(CR_CRIT_TAKEN_MELEE, CompatValue, Apply);
        ApplyRatingMod(CR_CRIT_TAKEN_RANGED, CompatValue, Apply);
        ApplyRatingMod(CR_CRIT_TAKEN_SPELL, CompatValue, Apply);
        break;
    case ITEM_MOD_HASTE_RATING:
        ApplyRatingMod(CR_HASTE_MELEE, CompatValue, Apply);
        ApplyRatingMod(CR_HASTE_RANGED, CompatValue, Apply);
        ApplyRatingMod(CR_HASTE_SPELL, CompatValue, Apply);
        break;
    case ITEM_MOD_EXPERTISE_RATING:
        ApplyRatingMod(CR_EXPERTISE, CompatValue, Apply);
        break;
    case ITEM_MOD_ATTACK_POWER:
        HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(CompatValue), Apply);
        HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(CompatValue), Apply);
        break;
    case ITEM_MOD_RANGED_ATTACK_POWER:
        HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(CompatValue), Apply);
        break;
        //            case ITEM_MOD_FERAL_ATTACK_POWER:
        //                ApplyFeralAPBonus(int32(val), apply);
        //                break;
    case ITEM_MOD_MANA_REGENERATION:
        ApplyManaRegenBonus(CompatValue, Apply);
        break;
    case ITEM_MOD_ARMOR_PENETRATION_RATING:
        ApplyRatingMod(CR_ARMOR_PENETRATION, CompatValue, Apply);
        break;
    case ITEM_MOD_SPELL_POWER:
        ApplySpellPowerBonus(CompatValue, Apply);
        break;
    case ITEM_MOD_HEALTH_REGEN:
        ApplyHealthRegenBonus(CompatValue, Apply);
        break;
    case ITEM_MOD_SPELL_PENETRATION:
        ApplySpellPenetrationBonus(CompatValue, Apply);
        break;
    case ITEM_MOD_BLOCK_VALUE:
        HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(CompatValue), Apply);
        break;
        // deprecated item mods
    case ITEM_MOD_SPELL_HEALING_DONE:
    case ITEM_MOD_SPELL_DAMAGE_DONE:
        break;
    }
}

class OnItemPlayerStatPlayer : public PlayerScript
{
public:
    OnItemPlayerStatPlayer() : PlayerScript("OnItemPlayerStatPlayer") {  }

    void OnLogin(Player* player) override
    {
        sItemPlayerStatMod->OnPlayerLoginLoadStat(player);
    }
};

void AddSC_ItemPlayerStatPlayerScript()
{
    new OnItemPlayerStatPlayer();
}
