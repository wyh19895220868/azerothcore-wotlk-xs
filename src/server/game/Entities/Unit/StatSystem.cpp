/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Creature.h"
#include "Config.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "Unit.h"
#include "WorldSessionMgr.h"
#include "..\..\..\scripts\Custom\ClassConfig\ClassMod.h"
#include "..\..\..\scripts\Custom\_Helper\Helper.h"
#include "../../../scripts/Custom/AddonActive/DoAddon.h"
#include "../../../scripts/Custom/ItemFuncMgr/ItemFuncMgr.h"

#include <boost/multiprecision/cpp_int.hpp>
#include <iomanip>
#include <limits>

namespace
{
AsInt64 MultiplyDivideAsInt64(AsInt64 const& value, AsInt64 const& multiplier, AsInt64 const& divisor)
{
    return MultiplyDivideAsInt256(value, multiplier, divisor);
}
}

inline bool _ModifyUInt32(bool apply, uint32& baseValue, int32& amount)
{
    // If amount is negative, change sign and value of apply.
    if (amount < 0)
    {
        apply = !apply;
        amount = -amount;
    }
    if (apply)
        baseValue += amount;
    else
    {
        // Make sure we do not get uint32 overflow.
        if (amount > int32(baseValue))
            amount = baseValue;
        baseValue -= amount;
    }
    return apply;
}

/*#######################################
########                         ########
########    UNIT STAT SYSTEM     ########
########                         ########
#######################################*/

void Unit::UpdateAllResistances()
{
    for (uint8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateResistances(i);
}

namespace
{
bool IsLegacyCompatCapValue(AsInt64 const& value)
{
    return value == AsInt64(2000000000)
        || value == AsInt64(2100000000)
        || value == AsInt64(std::numeric_limits<int32>::max());
}

bool PlayerHasEquippedDebugItem(Player const* player, uint32 entry)
{
    if (!player)
        return false;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            if (item->GetEntry() == entry)
                return true;

    return false;
}
}

void Unit::UpdateDamagePhysical(WeaponAttackType attType)
{
    float minDamage = 0.0f;
    float maxDamage = 0.0f;

    CalculateMinMaxDamage(attType, false, true, minDamage, maxDamage);

    if (ToPlayer())
    {
        auto soulWeaponRange = attType == BASE_ATTACK
            ? sItemFuncMgr->GetSoulWeaponDamageRange(ToPlayer())
            : std::make_pair(AsInt64Zero(), AsInt64Zero());

        AsInt64 nowMin = sCustomClass->GetAsMinWpNow(ToPlayer());
        if (nowMin > 0 && !IsLegacyCompatCapValue(nowMin))
            minDamage = float(AsInt64ToInt32Clamp(nowMin + soulWeaponRange.first, std::numeric_limits<int32>::max()));

        AsInt64 nowMax = sCustomClass->GetAsMaxWpNow(ToPlayer());
        if (nowMax > 0 && !IsLegacyCompatCapValue(nowMax))
            maxDamage = float(AsInt64ToInt32Clamp(nowMax + soulWeaponRange.second, std::numeric_limits<int32>::max()));
    }

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            SetStatFloatValue(UNIT_FIELD_MINDAMAGE, minDamage);
            SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, maxDamage);
            break;
        case OFF_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, minDamage);
            SetStatFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, maxDamage);
            break;
        case RANGED_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, minDamage);
            SetStatFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, maxDamage);
            break;
    }
}

/*#######################################
########                         ########
########   PLAYERS STAT SYSTEM   ########
########                         ########
#######################################*/

namespace
{
ItemModType AsItemModForStat(Stats stat)
{
    switch (stat)
    {
        case STAT_STRENGTH:
            return ITEM_MOD_STRENGTH;
        case STAT_AGILITY:
            return ITEM_MOD_AGILITY;
        case STAT_STAMINA:
            return ITEM_MOD_STAMINA;
        case STAT_INTELLECT:
            return ITEM_MOD_INTELLECT;
        case STAT_SPIRIT:
            return ITEM_MOD_SPIRIT;
        default:
            return ITEM_MOD_MANA;
    }
}

AsInt64 AsInt64FromFloatValue(float value)
{
    if (!std::isfinite(value) || value <= 0.0f)
        return AsInt64Zero();

    return AsInt64(value);
}

AsInt64 AsGetOverflowStat(Player* player, ItemModType mod)
{
    return player->GetAsStatBonus(mod) - player->GetAsStatCompatBonus(mod);
}

AsInt64 AsGetRealStatOverflow(Player* player, Stats stat)
{
    AsInt64 real = player->GetAsRealStat(stat);
    AsInt64 compat = AsInt64FromFloatValue(player->GetStat(stat));
    return real > compat ? real - compat : AsInt64Zero();
}

AsInt64 AsGetAttackPowerBaseOverflow(Player* player, bool ranged)
{
    AsInt64 strength = AsGetRealStatOverflow(player, STAT_STRENGTH);
    AsInt64 agility = AsGetRealStatOverflow(player, STAT_AGILITY);

    if (ranged)
    {
        switch (player->getClass())
        {
            case CLASS_HUNTER:
            case CLASS_ROGUE:
            case CLASS_WARRIOR:
                return agility;
            case CLASS_DRUID:
                switch (player->GetShapeshiftForm())
                {
                    case FORM_CAT:
                    case FORM_BEAR:
                    case FORM_DIREBEAR:
                        return AsInt64Zero();
                    default:
                        return agility;
                }
            default:
                return agility;
        }
    }

    switch (player->getClass())
    {
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
        case CLASS_WARRIOR:
            return strength * 2;
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
        case CLASS_ROGUE:
            return strength + agility;
        case CLASS_DRUID:
            switch (player->GetShapeshiftForm())
            {
                case FORM_CAT:
                    return strength * 2 + agility;
                case FORM_BEAR:
                case FORM_DIREBEAR:
                case FORM_MOONKIN:
                    return strength * 2;
                default:
                    return strength * 2;
            }
        case CLASS_MAGE:
        case CLASS_PRIEST:
        case CLASS_WARLOCK:
            return strength;
        default:
            return AsInt64Zero();
    }
}

AsInt64 AsGetClassStatNow(Player* player, Stats stat)
{
    switch (stat)
    {
        case STAT_STRENGTH:
            return sCustomClass->GetAsSt1Now(player);
        case STAT_AGILITY:
            return sCustomClass->GetAsSt2Now(player);
        case STAT_STAMINA:
            return sCustomClass->GetAsSt3Now(player);
        case STAT_INTELLECT:
            return sCustomClass->GetAsSt4Now(player);
        case STAT_SPIRIT:
            return sCustomClass->GetAsSt5Now(player);
        default:
            return AsInt64Zero();
    }
}

AsInt64 AsGetClassStatMin(Player* player, Stats stat)
{
    switch (stat)
    {
        case STAT_STRENGTH:
            return sCustomClass->GetAsSt1Min(player);
        case STAT_AGILITY:
            return sCustomClass->GetAsSt2Min(player);
        case STAT_STAMINA:
            return sCustomClass->GetAsSt3Min(player);
        case STAT_INTELLECT:
            return sCustomClass->GetAsSt4Min(player);
        case STAT_SPIRIT:
            return sCustomClass->GetAsSt5Min(player);
        default:
            return AsInt64Zero();
    }
}

AsInt64 AsApplyClassStatBounds(Player* player, Stats stat, AsInt64 realValue)
{
    AsInt64 minValue = AsGetClassStatMin(player, stat);
    if (IsLegacyCompatCapValue(minValue))
        minValue = AsInt64Zero();

    if (minValue > 0 && realValue < minValue)
        realValue = minValue;

    AsInt64 nowValue = AsGetClassStatNow(player, stat);
    if (IsLegacyCompatCapValue(nowValue))
        nowValue = AsInt64Zero();

    if (nowValue > 0)
        realValue = nowValue;

    return realValue;
}

AsInt64 ScaleAsInt64Value(AsInt64 const& value, float modifier)
{
    int64 scale = int64(modifier * 10000.0f + (modifier >= 0.0f ? 0.5f : -0.5f));
    return MultiplyDivideAsInt64(value, scale, 10000);
}

int32 ClampFloatToInt32Field(float value)
{
    if (value > float(std::numeric_limits<int32>::max()))
        return std::numeric_limits<int32>::max();

    if (value < float(std::numeric_limits<int32>::min()))
        return std::numeric_limits<int32>::min();

    return int32(value);
}

void ClampAttackPowerFields(int32& baseAttackPower, int32& attackPowerMod)
{
    if (baseAttackPower > 0 && attackPowerMod > std::numeric_limits<int32>::max() - baseAttackPower)
        attackPowerMod = std::numeric_limits<int32>::max() - baseAttackPower;
}
}

bool Player::UpdateStats(Stats stat)
{
    if (stat > STAT_SPIRIT)
        return false;

    // value = ((base_value * base_pct) + total_value) * total_pct
    float value  = GetTotalStatValue(stat);
    ItemModType itemMod = AsItemModForStat(stat);
    AsInt64 realValue = AsInt64FromFloatValue(value) + AsGetOverflowStat(this, itemMod);
    realValue = AsApplyClassStatBounds(this, stat, realValue);
    SetAsRealStat(stat, realValue);

    SetStat(stat, AsInt64ToInt32Clamp(realValue));

    switch (stat)
    {
        case STAT_STRENGTH:
            UpdateShieldBlockValue();
            break;
        case STAT_AGILITY:
            UpdateArmor();
            UpdateAllCritPercentages();
            UpdateDodgePercentage();
            break;
        case STAT_STAMINA:
            UpdateMaxHealth();
            break;
        case STAT_INTELLECT:
            UpdateMaxPower(POWER_MANA);
            UpdateAllSpellCritChances();
            UpdateArmor();                                  //SPELL_AURA_MOD_RESISTANCE_OF_INTELLECT_PERCENT, only armor currently
            break;
        default:
            break;
    }

    if (stat == STAT_STRENGTH)
    {
        UpdateAttackPowerAndDamage(false);
        if (HasAuraTypeWithMiscvalue(SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT, stat))
            UpdateAttackPowerAndDamage(true);
    }
    else if (stat == STAT_AGILITY)
    {
        UpdateAttackPowerAndDamage(false);
        UpdateAttackPowerAndDamage(true);
    }
    else
    {
        // Need update (exist AP from stat auras)
        if (HasAuraTypeWithMiscvalue(SPELL_AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT, stat))
            UpdateAttackPowerAndDamage(false);
        if (HasAuraTypeWithMiscvalue(SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT, stat))
            UpdateAttackPowerAndDamage(true);
    }

    UpdateSpellDamageAndHealingBonus();
    UpdateManaRegen();

    // Update ratings in exist SPELL_AURA_MOD_RATING_FROM_STAT and only depends from stat
    uint32 mask = 0;
    AuraEffectList const& modRatingFromStat = GetAuraEffectsByType(SPELL_AURA_MOD_RATING_FROM_STAT);
    for (AuraEffectList::const_iterator i = modRatingFromStat.begin(); i != modRatingFromStat.end(); ++i)
        if (Stats((*i)->GetMiscValueB()) == stat)
            mask |= (*i)->GetMiscValue();
    if (mask)
    {
        for (uint32 rating = 0; rating < MAX_COMBAT_RATING; ++rating)
            if (mask & (1 << rating))
                ApplyRatingMod(CombatRating(rating), 0, true);
    }
    return true;
}

void Player::ApplySpellPowerBonus(int32 amount, bool apply)
{
    apply = _ModifyUInt32(apply, m_baseSpellPower, amount);

    int32 ENow = sCustomClass->GetEpNow(ToPlayer());
    if (ENow > 0)
    {
        SetUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, ENow);
    }
    else
    {
        // For speed just update for client
        ApplyModUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, amount, apply);
    }

    int32 SNow = sCustomClass->GetSpNow(ToPlayer());
    if (SNow > 0)
    {
        for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
            SetUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, SNow);
    }
    else
    {
        for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
            ApplyModInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, amount, apply);
    }
}

void Player::UpdateSpellDamageAndHealingBonus()
{
    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    // Get healing bonus for all schools
    SetStatInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_ALL));
    // Get damage bonus for all schools
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        SetStatInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, SpellBaseDamageBonusDone(SpellSchoolMask(1 << i)));
}

bool Player::UpdateAllStats()
{
    for (int8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        float value = GetTotalStatValue(Stats(i));
        ItemModType itemMod = AsItemModForStat(Stats(i));
        AsInt64 realValue = AsInt64FromFloatValue(value) + AsGetOverflowStat(this, itemMod);
        realValue = AsApplyClassStatBounds(this, Stats(i), realValue);
        SetAsRealStat(Stats(i), realValue);
        SetStat(Stats(i), AsInt64ToInt32Clamp(realValue));
    }

    UpdateArmor();
    // calls UpdateAttackPowerAndDamage() in UpdateArmor for SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR
    UpdateAttackPowerAndDamage(true);
    UpdateMaxHealth();

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllRatings();
    UpdateAllCritPercentages();
    UpdateAllSpellCritChances();
    UpdateDefenseBonusesMod();
    UpdateShieldBlockValue();
    UpdateSpellDamageAndHealingBonus();
    UpdateManaRegen();
    UpdateExpertise(BASE_ATTACK);
    UpdateExpertise(OFF_ATTACK);
    RecalculateRating(CR_ARMOR_PENETRATION);
    UpdateAllResistances();

    return true;
}

void Player::ApplySpellPenetrationBonus(int32 amount, bool apply)
{
    ApplyModInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE, -amount, apply);
    m_spellPenetrationItemMod += apply ? amount : -amount;
}

void Player::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        // cant use GetTotalAuraModValue because of total pct multiplier :P
        float value = 0.0f;
        UnitMods unitMod = UnitMods(UNIT_MOD_RESISTANCE_START + school);

        value  = GetModifierValue(unitMod, BASE_VALUE);
        value *= GetModifierValue(unitMod, BASE_PCT);
        value += GetModifierValue(unitMod, TOTAL_VALUE);

        AuraEffectList const& mResbyIntellect = GetAuraEffectsByType(SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT);
        for(AuraEffectList::const_iterator i = mResbyIntellect.begin(); i != mResbyIntellect.end(); ++i)
        {
            if((*i)->GetMiscValue() & (1 << (school - 1)) )
                value += int32(GetStat(Stats((*i)->GetMiscValueB())) * (*i)->GetAmount() / 100.0f);
        }

        value *= GetModifierValue(unitMod, TOTAL_PCT);

        SetResistance(SpellSchools(school), int32(value));
    }
    else
        UpdateArmor();
}

void Player::UpdateArmor()
{
    UnitMods unitMod = UNIT_MOD_ARMOR;

    float value = GetModifierValue(unitMod, BASE_VALUE);   // base armor (from items)
    value *= GetModifierValue(unitMod, BASE_PCT);           // armor percent from items

    if (sHelper->GetBoolean(1047))
        value += GetStat(STAT_AGILITY) * 2.0f;                      // armor bonus from stats

    value += GetModifierValue(unitMod, TOTAL_VALUE);

    //add dynamic flat mods
    AuraEffectList const& mResbyIntellect = GetAuraEffectsByType(SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT);
    for (AuraEffectList::const_iterator i = mResbyIntellect.begin(); i != mResbyIntellect.end(); ++i)
    {
        if ((*i)->GetMiscValue() & SPELL_SCHOOL_MASK_NORMAL)
            value += CalculatePct(GetStat(Stats((*i)->GetMiscValueB())), (*i)->GetAmount());
    }

    value *= GetModifierValue(unitMod, TOTAL_PCT);

    float Mod = sCustomClass->GetArrMod(this);
    float Max = float(sCustomClass->GetArrMax(this));
    value *= Mod;
    float Now = float(sCustomClass->GetArrNow(this));
    if (Now > 0)
    {
        Max = Now;
        value = Now;
    }

    if (Max > 0)
        value = value > Max ? Max : value;

    SetArmor(int32(value));

    UpdateAttackPowerAndDamage();                           // armor dependent auras update for SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR
}

float Player::GetHealthBonusFromStamina()
{
    float stamina = GetStat(STAT_STAMINA);

    float baseStam = stamina < 20 ? stamina : 20;
    float moreStam = stamina - baseStam;

    return baseStam + (moreStam * 10.0f);
}

float Player::GetManaBonusFromIntellect()
{
    float intellect = GetStat(STAT_INTELLECT);

    float baseInt = intellect < 20 ? intellect : 20;
    float moreInt = intellect - baseInt;

    return baseInt + (moreInt * 15.0f);
}

void Player::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;

    if (!sHelper->GetBoolean(1041))
    {
        float value = GetModifierValue(unitMod, BASE_VALUE) + GetCreateHealth();
        AsInt64 realValue = AsInt64FromFloatValue(value) + AsGetOverflowStat(this, ITEM_MOD_HEALTH);
        realValue += GetAsAuraHealthFlatBonus();
        AsInt64 createHealthOverflow = GetAsCreateHealth() - AsInt64(GetCreateHealth());
        if (createHealthOverflow > 0)
            realValue += createHealthOverflow;
        SetAsRealMaxHealth(realValue);
        SetAsMaxHealth(realValue);
        return;
    }

    float value = GetModifierValue(unitMod, BASE_VALUE) + GetCreateHealth();
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + GetHealthBonusFromStamina();
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    //???ɶ����
    uint32 Now = sCustomClass->GetHpNow(this);
    if (Now)
    {
        AsInt64 realNow = sCustomClass->GetAsHpNow(this);
        value = float(AsInt64ToUInt32Clamp(realNow));
        SetAsRealMaxHealth(realNow);
        sScriptMgr->OnPlayerAfterUpdateMaxHealth(this, value);
        SetAsMaxHealth(realNow);
        return;
    }

    float Mod = sCustomClass->GetHpMod(this);
    value *= Mod;

    float Min = (float)sCustomClass->GetHpMin(this);
    if (Min > 0.0f)
        value = value < Min ? Min : value;

    float Max = (float)sCustomClass->GetHpMax(this);
    if (Max > 0.0f)
        value = value > Max ? Max : value;

    AsInt64 realValue = AsInt64FromFloatValue(value);
    AsInt64 createHealthOverflow = GetAsCreateHealth() - AsInt64(GetCreateHealth());
    if (createHealthOverflow > 0)
        realValue += ScaleAsInt64Value(createHealthOverflow, GetModifierValue(unitMod, BASE_PCT) * GetModifierValue(unitMod, TOTAL_PCT) * sCustomClass->GetHpMod(this));
    realValue += AsGetOverflowStat(this, ITEM_MOD_HEALTH);
    realValue += AsGetOverflowStat(this, ITEM_MOD_STAMINA) * 10;
    realValue += GetAsAuraHealthFlatBonus();
    SetAsRealMaxHealth(realValue);

    {
        double Gain_HP = AsInt64ToDoubleClamp(realValue, 999999999999999.0);
        float _HeahTcut = 1.0f;
        m_HeahTcut = 1.0f;

        if (Gain_HP > 2100000000.0)
        {
            double Gain_BL = (Gain_HP / 2100000000.0) + 1.0;
            m_HeahTcut = (float)Gain_BL;
            _HeahTcut = m_HeahTcut;
        }

        WorldSessionMgr::SessionMap const& sessions = sWorldSessionMgr->GetAllSessions();
        for (WorldSessionMgr::SessionMap::const_iterator itr = sessions.begin(); itr != sessions.end(); ++itr)
        {
            if (!itr->second)
                continue;

            Player* _player = itr->second->GetPlayer();
            if (!_player || !_player->IsInWorld())
                continue;

            std::ostringstream ss;

            //ss << std::fixed << std::setprecision(2);
            uint32 playerGuid = GetGUID().GetCounter();
            ss << playerGuid << "#" << _HeahTcut << "#1";
            sDoAddon->SendPacketToClient(_player, "ThroughLimitHp", ss.str().c_str());

            //LOG_INFO("server.loading", " 555 {} ", _HeahTcut);
        }
    }

    value = float(AsInt64ToUInt32Clamp(realValue));

    sScriptMgr->OnPlayerAfterUpdateMaxHealth(this, value);
    SetAsMaxHealth(realValue);
}

void Player::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(static_cast<uint16>(UNIT_MOD_POWER_START) + power);

    if (power == POWER_MANA && !sHelper->GetBoolean(1043))
    {
        float value = GetModifierValue(unitMod, BASE_VALUE) + GetCreatePowers(power);
        AsInt64 realValue = AsInt64FromFloatValue(value) + AsGetOverflowStat(this, ITEM_MOD_MANA);
        AsInt64 createManaOverflow = GetAsCreateMana() - AsInt64(GetCreateMana());
        if (createManaOverflow > 0)
            realValue += createManaOverflow;
        SetAsRealMaxPower(power, realValue);
        SetAsMaxPower(power, realValue);
        return;
    }

    float bonusPower = (power == POWER_MANA && GetCreatePowers(power) > 0) ? GetManaBonusFromIntellect() : 0;

    float value = GetModifierValue(unitMod, BASE_VALUE) + GetCreatePowers(power);
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) +  bonusPower;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    if (power == POWER_MANA)
    {
        float Now = float(sCustomClass->GetManaNow(this));
        if (Now > 0.0f)
        {
            AsInt64 realNow = sCustomClass->GetAsManaNow(this);
            value = float(AsInt64ToUInt32Clamp(realNow));
            SetAsRealMaxPower(power, realNow);
            sScriptMgr->OnPlayerAfterUpdateMaxPower(this, power, value);
            SetAsMaxPower(power, realNow);
            return;
        }

        float Mod = sCustomClass->GetManaMod(this);
        value *= Mod;

        float Min = (float)sCustomClass->GetManaMin(this);
        if (Min > 0.0f)
            value = value < Min ? Min : value;

        float Max = (float)sCustomClass->GetManaMax(this);
        if (Max > 0.0f)
            value = value > Max ? Max : value;
    }

    AsInt64 realValue = AsInt64FromFloatValue(value);
    if (power == POWER_MANA)
    {
        AsInt64 createManaOverflow = GetAsCreateMana() - AsInt64(GetCreateMana());
        if (createManaOverflow > 0)
            realValue += ScaleAsInt64Value(createManaOverflow, GetModifierValue(unitMod, BASE_PCT) * GetModifierValue(unitMod, TOTAL_PCT) * sCustomClass->GetManaMod(this));
        realValue += AsGetOverflowStat(this, ITEM_MOD_MANA);
        realValue += AsGetOverflowStat(this, ITEM_MOD_INTELLECT) * 15;
    }

    SetAsRealMaxPower(power, realValue);

    sScriptMgr->OnPlayerAfterUpdateMaxPower(this, power, value);
    SetAsMaxPower(power, realValue);
}

void Player::ApplyFeralAPBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseFeralAP, amount);
    UpdateAttackPowerAndDamage();
}

void Player::UpdateAttackPowerAndDamage(bool ranged)
{
    float val2 = 0.0f;
    float level = float(getLevel());

    sScriptMgr->OnPlayerBeforeUpdateAttackPowerAndDamage(this, level, val2, ranged);

    UnitMods unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    uint16 index = UNIT_FIELD_ATTACK_POWER;
    uint16 index_mod = UNIT_FIELD_ATTACK_POWER_MODS;
    uint16 index_mult = UNIT_FIELD_ATTACK_POWER_MULTIPLIER;

    if (ranged)
    {
        index = UNIT_FIELD_RANGED_ATTACK_POWER;
        index_mod = UNIT_FIELD_RANGED_ATTACK_POWER_MODS;
        index_mult = UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER;

        switch (getClass())
        {
            case CLASS_HUNTER:
                val2 = level * 2.0f + GetStat(STAT_AGILITY) - 10.0f;
                break;
            case CLASS_ROGUE:
            case CLASS_WARRIOR:
                val2 = level + GetStat(STAT_AGILITY) - 10.0f;
                break;
            case CLASS_DRUID:
                switch (GetShapeshiftForm())
                {
                    case FORM_CAT:
                    case FORM_BEAR:
                    case FORM_DIREBEAR:
                        val2 = 0.0f;
                        break;
                    default:
                        val2 = GetStat(STAT_AGILITY) - 10.0f;
                        break;
                }
                break;
            default:
                val2 = GetStat(STAT_AGILITY) - 10.0f;
                break;
        }
    }
    else
    {
        switch (getClass())
        {
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT:
            case CLASS_WARRIOR:
                val2 = level * 3.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
                break;
            case CLASS_HUNTER:
            case CLASS_SHAMAN:
            case CLASS_ROGUE:
                val2 = level * 2.0f + GetStat(STAT_STRENGTH) + GetStat(STAT_AGILITY) - 20.0f;
                break;
            case CLASS_DRUID:
                {
                    // Check if Predatory Strikes is skilled
                    float mLevelMult = 0.0f;
                    float weapon_bonus = 0.0f;
                    if (IsInFeralForm())
                    {
                        Unit::AuraEffectList const& mDummy = GetAuraEffectsByType(SPELL_AURA_DUMMY);
                        for (Unit::AuraEffectList::const_iterator itr = mDummy.begin(); itr != mDummy.end(); ++itr)
                        {
                            AuraEffect* aurEff = *itr;
                            if (aurEff->GetSpellInfo()->SpellIconID == 1563)
                            {
                                switch (aurEff->GetEffIndex())
                                {
                                    case 0: // Predatory Strikes (effect 0)
                                        mLevelMult = CalculatePct(1.0f, aurEff->GetAmount());
                                        break;
                                    case 1: // Predatory Strikes (effect 1)
                                        if (Item* mainHand = m_items[EQUIPMENT_SLOT_MAINHAND])
                                        {
                                            // also gains % attack power from equipped weapon
                                            ItemTemplate const* proto = mainHand->GetTemplate();
                                            if (!proto)
                                                continue;

                                            // A signed wide accumulator is needed: getFeralBonus, ItemStatValue and
                                            // CalcValue all return int32 and may be negative. A
                                            // negative contribution used to wrap the unsigned
                                            // accumulator to ~4e9, which float() then turned into
                                            // a gigantic feral attack power bonus.
                                            int64 ap = proto->getFeralBonus();
                                            // Get AP Bonuses from weapon
                                            for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
                                            {
                                                if (i >= proto->StatsCount)
                                                    break;

                                                if (proto->ItemStat[i].ItemStatType == ITEM_MOD_ATTACK_POWER)
                                                    ap += proto->ItemStat[i].ItemStatValue;
                                            }

                                            // Get AP Bonuses from weapon spells
                                            for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
                                            {
                                                // no spell
                                                if (!proto->Spells[i].SpellId || proto->Spells[i].SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
                                                    continue;

                                                // check if it is valid spell
                                                SpellInfo const* spellproto = sSpellMgr->GetSpellInfo(proto->Spells[i].SpellId);
                                                if (!spellproto)
                                                    continue;

                                                for (uint8 j = 0; j < MAX_SPELL_EFFECTS; ++j)
                                                    if (spellproto->Effects[j].ApplyAuraName == SPELL_AURA_MOD_ATTACK_POWER)
                                                        ap += spellproto->Effects[j].CalcValue();
                                            }

                                            // A net-negative weapon must not subtract feral AP.
                                            weapon_bonus = CalculatePct(float(std::max<int64>(0, ap)), aurEff->GetAmount());
                                        }
                                        break;
                                    default:
                                        break;
                                }
                            }
                        }
                    }

                    switch (GetShapeshiftForm())
                    {
                        case FORM_CAT:
                            val2 = (getLevel() * mLevelMult) + GetStat(STAT_STRENGTH) * 2.0f + GetStat(STAT_AGILITY) - 20.0f + weapon_bonus + m_baseFeralAP;
                            break;
                        case FORM_BEAR:
                        case FORM_DIREBEAR:
                            val2 = (getLevel() * mLevelMult) + GetStat(STAT_STRENGTH) * 2.0f - 20.0f + weapon_bonus + m_baseFeralAP;
                            break;
                        case FORM_MOONKIN:
                            val2 = (getLevel() * mLevelMult) + GetStat(STAT_STRENGTH) * 2.0f - 20.0f + m_baseFeralAP;
                            break;
                        default:
                            val2 = GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
                            break;
                    }
                    break;
                }
            case CLASS_MAGE:
            case CLASS_PRIEST:
            case CLASS_WARLOCK:
                val2 = GetStat(STAT_STRENGTH) - 10.0f;
                break;
        }
    }

    SetModifierValue(unitMod, BASE_VALUE, val2);

    AsInt64 realBaseAttackPower = AsInt64FromFloatValue(GetModifierValue(unitMod, BASE_VALUE));
    realBaseAttackPower += AsGetAttackPowerBaseOverflow(this, ranged);
    realBaseAttackPower = ScaleAsInt64Value(realBaseAttackPower, GetModifierValue(unitMod, BASE_PCT));

    float base_attPower  = GetModifierValue(unitMod, BASE_VALUE) * GetModifierValue(unitMod, BASE_PCT);
    float attPowerMod = GetModifierValue(unitMod, TOTAL_VALUE);
    AsInt64 realAttackPowerMod = AsInt64FromFloatValue(attPowerMod);
    realAttackPowerMod += AsGetOverflowStat(this, ITEM_MOD_ATTACK_POWER);
    if (ranged)
        realAttackPowerMod += AsGetOverflowStat(this, ITEM_MOD_RANGED_ATTACK_POWER);

    //add dynamic flat mods
    if (ranged)
    {
        if ((getClassMask() & CLASSMASK_WAND_USERS) == 0)
        {
            AuraEffectList const& mRAPbyStat = GetAuraEffectsByType(SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT);
            for (AuraEffectList::const_iterator i = mRAPbyStat.begin(); i != mRAPbyStat.end(); ++i)
            {
                attPowerMod += CalculatePct(GetStat(Stats((*i)->GetMiscValue())), (*i)->GetAmount());
                realAttackPowerMod += ScaleAsInt64Value(GetAsRealStat(Stats((*i)->GetMiscValue())), float((*i)->GetAmount()) / 100.0f);
            }
        }
    }
    else
    {
        AuraEffectList const& mAPbyStat = GetAuraEffectsByType(SPELL_AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT);
        for (AuraEffectList::const_iterator i = mAPbyStat.begin(); i != mAPbyStat.end(); ++i)
        {
            attPowerMod += CalculatePct(GetStat(Stats((*i)->GetMiscValue())), (*i)->GetAmount());
            realAttackPowerMod += ScaleAsInt64Value(GetAsRealStat(Stats((*i)->GetMiscValue())), float((*i)->GetAmount()) / 100.0f);
        }

        AuraEffectList const& mAPbyArmor = GetAuraEffectsByType(SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR);
        for (AuraEffectList::const_iterator iter = mAPbyArmor.begin(); iter != mAPbyArmor.end(); ++iter)
            // always: ((*i)->GetModifier()->m_miscvalue == 1 == SPELL_SCHOOL_MASK_NORMAL)
            attPowerMod += int32(GetArmor() / (*iter)->GetAmount());
    }

    float attPowerMultiplier = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;
    AsInt64 realAttackPower = ScaleAsInt64Value(realBaseAttackPower + realAttackPowerMod, GetModifierValue(unitMod, TOTAL_PCT));

    AsInt64 nowAttackPower = sCustomClass->GetAsApNow(this);
    if (nowAttackPower > 0 && !IsLegacyCompatCapValue(nowAttackPower))
    {
        base_attPower = float(AsInt64ToInt32Clamp(nowAttackPower, std::numeric_limits<int32>::max()));
        attPowerMod = 0.0f;
        attPowerMultiplier = 0.0f;
        realAttackPower = nowAttackPower;
    }

    sScriptMgr->OnPlayerAfterUpdateAttackPowerAndDamage(this, level, base_attPower, attPowerMod, attPowerMultiplier, ranged);
    if (!(nowAttackPower > 0 && !IsLegacyCompatCapValue(nowAttackPower)))
    {
        AsInt64 compatAfterScript = AsInt64FromFloatValue((base_attPower + attPowerMod) * (1.0f + attPowerMultiplier));
        if (compatAfterScript > realAttackPower)
            realAttackPower = compatAfterScript;
    }
    if (realAttackPower < 0)
        realAttackPower = AsInt64Zero();
    SetAsAttackPowerValue(ranged ? RANGED_ATTACK : BASE_ATTACK, realAttackPower);

    int32 baseAttackPowerCompat = ClampFloatToInt32Field(base_attPower);
    int32 attackPowerModCompat = ClampFloatToInt32Field(attPowerMod);
    ClampAttackPowerFields(baseAttackPowerCompat, attackPowerModCompat);

    SetInt32Value(index, baseAttackPowerCompat);            //UNIT_FIELD_(RANGED)_ATTACK_POWER field
    SetInt32Value(index_mod, attackPowerModCompat);          //UNIT_FIELD_(RANGED)_ATTACK_POWER_MODS field
    SetFloatValue(index_mult, attPowerMultiplier);          //UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field

    //automatically update weapon damage after attack power modification
    if (ranged)
    {
        UpdateDamagePhysical(RANGED_ATTACK);
    }
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        if (CanDualWield() && haveOffhandWeapon())           //allow update offhand damage only if player knows DualWield Spec and has equipped offhand weapon
            UpdateDamagePhysical(OFF_ATTACK);
        if (getClass() == CLASS_SHAMAN || getClass() == CLASS_PALADIN)                      // mental quickness
            UpdateSpellDamageAndHealingBonus();
    }
}

void Player::UpdateShieldBlockValue()
{
    SetUInt32Value(PLAYER_SHIELD_BLOCK, GetShieldBlockValue());
}

void Player::CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, bool addTotalPct, float& minDamage, float& maxDamage)
{
    UnitMods unitMod;

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
    }

    float attackSpeedMod = GetAPMultiplier(attType, normalized);

    float baseValue  = GetModifierValue(unitMod, BASE_VALUE) + GetTotalAttackPowerValue(attType) / 14.0f * attackSpeedMod;
    float basePct    = GetModifierValue(unitMod, BASE_PCT);
    float totalValue = GetModifierValue(unitMod, TOTAL_VALUE);
    float totalPct   = addTotalPct ? GetModifierValue(unitMod, TOTAL_PCT) : 1.0f;

    float weaponMinDamage = GetWeaponDamageRange(attType, MINDAMAGE);
    float weaponMaxDamage = GetWeaponDamageRange(attType, MAXDAMAGE);

    if (IsInFeralForm()) // check if player is druid and in cat or bear forms
    {
        uint8 lvl = getLevel();
        if (lvl > 60)
            lvl = 60;

        weaponMinDamage = lvl * 0.85f * attackSpeedMod;
        weaponMaxDamage = lvl * 1.25f * attackSpeedMod;
    }
    else if (!CanUseAttackType(attType)) // check if player not in form but still can't use (disarm case)
    {
        // cannot use ranged/off attack, set values to 0
        if (attType != BASE_ATTACK)
        {
            minDamage = 0.0f;
            maxDamage = 0.0f;
            return;
        }
        weaponMinDamage = BASE_MINDAMAGE;
        weaponMaxDamage = BASE_MAXDAMAGE;
    }
    else if (attType == RANGED_ATTACK) // add ammo DPS to ranged damage
    {
        weaponMinDamage += GetAmmoDPS() * attackSpeedMod;
        weaponMaxDamage += GetAmmoDPS() * attackSpeedMod;
    }

    if (attType == BASE_ATTACK && !IsInFeralForm() && CanUseAttackType(attType))
    {
        auto soulWeaponRange = sItemFuncMgr->GetSoulWeaponDamageRange(this);
        weaponMinDamage += float(AsInt64ToInt32Clamp(soulWeaponRange.first, std::numeric_limits<int32>::max()));
        weaponMaxDamage += float(AsInt64ToInt32Clamp(soulWeaponRange.second, std::numeric_limits<int32>::max()));
    }

    minDamage = ((weaponMinDamage + baseValue) * basePct + totalValue) * totalPct;
    maxDamage = ((weaponMaxDamage + baseValue) * basePct + totalValue) * totalPct;

    // Keep client-facing weapon fields inside uint32, real overflow damage is carried separately.
    float maxCompatDamage = float(std::numeric_limits<int32>::max());
    if (minDamage < 0.0f)
        minDamage = 0.0f;
    else if (minDamage > maxCompatDamage)
        minDamage = maxCompatDamage;

    if (maxDamage < 0.0f)
        maxDamage = 0.0f;
    else if (maxDamage > maxCompatDamage)
        maxDamage = maxCompatDamage;
    if (minDamage > maxDamage)
        minDamage = maxDamage;
}

void Player::UpdateDefenseBonusesMod()
{
    UpdateBlockPercentage();
    UpdateParryPercentage();
    UpdateDodgePercentage();
}

void Player::UpdateBlockPercentage()
{
    // No block
    float value = 0.0f;
    if (CanBlock())
    {
        // Base value
        value = 5.0f;
        // Modify value from defense skill
        value += (int32(GetDefenseSkillValue()) - int32(GetMaxSkillValueForLevel())) * 0.04f;
        // Increase from SPELL_AURA_MOD_BLOCK_PERCENT aura
        value += GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_PERCENT);
        // Increase from rating
        value += GetRatingBonusValue(CR_BLOCK);

        float Mod = sCustomClass->GetBlockMod(this);
        float Max = sCustomClass->GetBlockMax(this);

        value *= Mod;

        float Now = sCustomClass->GetBlockNow(this);
        if (Now > 0.0f)
        {
            Max = Now;
            value = Now;
        }

        if (Max > 0.0f)
            value = value > Max ? Max : value;

        value = value < 0.0f ? 0.0f : value;
    }
    SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, value);
}

void Player::UpdateCritPercentage(WeaponAttackType attType)
{
    BaseModGroup modGroup;
    uint16 index;
    CombatRating cr;

    switch (attType)
    {
        case OFF_ATTACK:
            modGroup = OFFHAND_CRIT_PERCENTAGE;
            index = PLAYER_OFFHAND_CRIT_PERCENTAGE;
            cr = CR_CRIT_MELEE;
            break;
        case RANGED_ATTACK:
            modGroup = RANGED_CRIT_PERCENTAGE;
            index = PLAYER_RANGED_CRIT_PERCENTAGE;
            cr = CR_CRIT_RANGED;
            break;
        case BASE_ATTACK:
        default:
            modGroup = CRIT_PERCENTAGE;
            index = PLAYER_CRIT_PERCENTAGE;
            cr = CR_CRIT_MELEE;
            break;
    }

    float value = GetTotalPercentageModValue(modGroup) + GetRatingBonusValue(cr);
    // Modify crit from weapon skill and maximized defense skill of same level victim difference
    value += (int32(GetWeaponSkillValue(attType)) - int32(GetMaxSkillValueForLevel())) * 0.04f;

    value = value < 0.0f ? 0.0f : value;

    float Mod = sCustomClass->GetCritMod(this);
    float Max = sCustomClass->GetCritMax(this);
    value *= Mod;
    float Now = sCustomClass->GetCritNow(this);
    if (Now > 0.0f)
    {
        Max = Now;
        value = Now;
    }

    if (Max > 0.0f)
        value = value > Max ? Max : value;

    SetStatFloatValue(index, value);
}

void Player::UpdateAllCritPercentages()
{
    float value = GetMeleeCritFromAgility();

    SetBaseModValue(CRIT_PERCENTAGE, PCT_MOD, value);
    SetBaseModValue(OFFHAND_CRIT_PERCENTAGE, PCT_MOD, value);
    SetBaseModValue(RANGED_CRIT_PERCENTAGE, PCT_MOD, value);

    UpdateCritPercentage(BASE_ATTACK);
    UpdateCritPercentage(OFF_ATTACK);
    UpdateCritPercentage(RANGED_ATTACK);
}

const float m_diminishing_k[MAX_CLASSES] =
{
    0.9560f,  // Warrior
    0.9560f,  // Paladin
    0.9880f,  // Hunter
    0.9880f,  // Rogue
    0.9830f,  // Priest
    0.9560f,  // DK
    0.9880f,  // Shaman
    0.9830f,  // Mage
    0.9830f,  // Warlock
    0.0f,     // ??
    0.9720f   // Druid
};

float Player::GetMissPercentageFromDefence() const
{
    float const miss_cap[MAX_CLASSES] =
    {
        16.00f,     // Warrior //correct
        16.00f,     // Paladin //correct
        16.00f,     // Hunter  //?
        16.00f,     // Rogue   //?
        16.00f,     // Priest  //?
        16.00f,     // DK      //correct
        16.00f,     // Shaman  //?
        16.00f,     // Mage    //?
        16.00f,     // Warlock //?
        0.0f,       // ??
        16.00f      // Druid   //?
    };

    float diminishing = 0.0f, nondiminishing = 0.0f;
    // Modify value from defense skill (only bonus from defense rating diminishes)
    nondiminishing += (GetSkillValue(SKILL_DEFENSE) - GetMaxSkillValueForLevel()) * 0.04f;
    diminishing += (int32(GetRatingBonusValue(CR_DEFENSE_SKILL))) * 0.04f;

    // apply diminishing formula to diminishing miss chance
    uint32 pclass = getClass() - 1;
    return nondiminishing + (diminishing * miss_cap[pclass] / (diminishing + miss_cap[pclass] * m_diminishing_k[pclass]));
}

void Player::UpdateParryPercentage()
{
    const float parry_cap[MAX_CLASSES] =
    {
        47.003525f,     // Warrior
        47.003525f,     // Paladin
        145.560408f,    // Hunter
        145.560408f,    // Rogue
        0.0f,           // Priest
        47.003525f,     // DK
        145.560408f,    // Shaman
        0.0f,           // Mage
        0.0f,           // Warlock
        0.0f,           // ??
        0.0f            // Druid
    };

    // No parry
    float value = 0.0f;
    m_realParry = 0.0f;
    uint32 pclass = getClass() - 1;
    if (CanParry() && parry_cap[pclass] > 0.0f)
    {
        float nondiminishing  = 5.0f;
        // Parry from rating
        float diminishing = GetRatingBonusValue(CR_PARRY);
        // Modify value from defense skill (only bonus from defense rating diminishes)
        nondiminishing += (GetSkillValue(SKILL_DEFENSE) - GetMaxSkillValueForLevel()) * 0.04f;
        diminishing += (int32(GetRatingBonusValue(CR_DEFENSE_SKILL))) * 0.04f;
        // Parry from SPELL_AURA_MOD_PARRY_PERCENT aura
        nondiminishing += GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT);
        // apply diminishing formula to diminishing parry chance

        //m_realParry = nondiminishing + diminishing * parry_cap[pclass] / (diminishing + parry_cap[pclass] * m_diminishing_k[pclass]);
        //m_realParry = m_realParry < 0.0f ? 0.0f : m_realParry;

        value = std::max(diminishing + nondiminishing, 0.0f);

        float Mod = sCustomClass->GetParryMod(this);
        float Max = sCustomClass->GetParryMax(this);

        value *= Mod;

        float Now = sCustomClass->GetParryNow(this);
        if (Now > 0.0f)
        {
            Max = Now;
            value = Now;
        }

        if (Max > 0.0f)
            value = value > Max ? Max : value;

        value = value < 0.0f ? 0.0f : value;

        m_realParry = value;
    }

    SetStatFloatValue(PLAYER_PARRY_PERCENTAGE, value);
}

void Player::UpdateDodgePercentage()
{
    const float dodge_cap[MAX_CLASSES] =
    {
        88.129021f,     // Warrior
        88.129021f,     // Paladin
        145.560408f,    // Hunter
        145.560408f,    // Rogue
        150.375940f,    // Priest
        88.129021f,     // DK
        145.560408f,    // Shaman
        150.375940f,    // Mage
        150.375940f,    // Warlock
        0.0f,           // ??
        116.890707f     // Druid
    };

    float diminishing = 0.0f, nondiminishing = 0.0f;
    GetDodgeFromAgility(diminishing, nondiminishing);
    // Modify value from defense skill (only bonus from defense rating diminishes)
    nondiminishing += (GetSkillValue(SKILL_DEFENSE) - GetMaxSkillValueForLevel()) * 0.04f;
    diminishing += (int32(GetRatingBonusValue(CR_DEFENSE_SKILL))) * 0.04f;
    // Dodge from SPELL_AURA_MOD_DODGE_PERCENT aura
    nondiminishing += GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT);
    // Dodge from rating
    diminishing += GetRatingBonusValue(CR_DODGE);
    // apply diminishing formula to diminishing dodge chance
    uint32 pclass = getClass() - 1;


    //m_realDodge = nondiminishing + (diminishing * dodge_cap[pclass] / (diminishing + dodge_cap[pclass] * m_diminishing_k[pclass]));
    //m_realDodge = m_realDodge < 0.0f ? 0.0f : m_realDodge;

    float value = std::max(diminishing + nondiminishing, 0.0f);

    float Mod = sCustomClass->GetDodgeMod(this);
    float Max = sCustomClass->GetDodgeMax(this);

    value *= Mod;

    float Now = sCustomClass->GetDodgeNow(this);
    if (Now > 0.0f)
    {
        Max = Now;
        value = Now;
    }

    if (Max > 0.0f)
        value = value > Max ? Max : value;

    value = value < 0.0f ? 0.0f : value;
    m_realDodge = value;

    SetStatFloatValue(PLAYER_DODGE_PERCENTAGE, value);
}

void Player::UpdateSpellCritChance(uint32 school)
{
    // For normal school set zero crit chance
    if (school == SPELL_SCHOOL_NORMAL)
    {
        SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1, 0.0f);
        return;
    }
    // For others recalculate it from:
    float crit = 0.0f;
    // Crit from Intellect
    crit += GetSpellCritFromIntellect();
    // Increase crit from SPELL_AURA_MOD_SPELL_CRIT_CHANCE
    crit += GetTotalAuraModifierAreaExclusive(SPELL_AURA_MOD_SPELL_CRIT_CHANCE);
    // Increase crit from SPELL_AURA_MOD_CRIT_PCT
    crit += GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_PCT);
    // Increase crit by school from SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL
    crit += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, 1 << school);
    // Increase crit from spell crit ratings
    crit += GetRatingBonusValue(CR_CRIT_SPELL);

    float Mod = sCustomClass->GetCritMod(this);
    float Max = sCustomClass->GetCritMax(this);

    crit *= Mod;

    float Now = sCustomClass->GetCritNow(this);
    if (Now > 0.0f)
    {
        Max = Now;
        crit = Now;
    }

    if (Max > 0.0f)
        crit = crit > Max ? Max : crit;

    // Store crit value
    SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + school, crit);
}

void Player::UpdateArmorPenetration(int32 amount)
{
    // Store Rating Value
    SetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + static_cast<uint16>(CR_ARMOR_PENETRATION), amount);
}

void Player::UpdateMeleeHitChances()
{
    m_modMeleeHitChance = (float)GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE);
    m_modMeleeHitChance += GetRatingBonusValue(CR_HIT_MELEE);
}

void Player::UpdateRangedHitChances()
{
    m_modRangedHitChance = (float)GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE);
    m_modRangedHitChance += GetRatingBonusValue(CR_HIT_RANGED);
}

void Player::UpdateSpellHitChances()
{
    m_modSpellHitChance = (float)GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_HIT_CHANCE);
    m_modSpellHitChance += GetRatingBonusValue(CR_HIT_SPELL);
}

void Player::UpdateAllSpellCritChances()
{
    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateSpellCritChance(i);
}

void Player::UpdateExpertise(WeaponAttackType attack)
{
    if (attack == RANGED_ATTACK)
        return;

    int32 expertise = int32(GetRatingBonusValue(CR_EXPERTISE));

    Item* weapon = GetWeaponForAttack(attack, true);

    AuraEffectList const& expAuras = GetAuraEffectsByType(SPELL_AURA_MOD_EXPERTISE);
    for (AuraEffectList::const_iterator itr = expAuras.begin(); itr != expAuras.end(); ++itr)
    {
        // item neutral spell
        if ((*itr)->GetSpellInfo()->EquippedItemClass == -1)
            expertise += (*itr)->GetAmount();
        // item dependent spell
        else if (weapon && weapon->IsFitToSpellRequirements((*itr)->GetSpellInfo()))
            expertise += (*itr)->GetAmount();
    }

    if (expertise < 0)
        expertise = 0;

    float Mod = sCustomClass->GetPreMod(this);
    int32 Max = sCustomClass->GetPreMax(this);
    expertise = int32(expertise * Mod);
    int32 Now = sCustomClass->GetPreNow(this);
    if (Now > 0)
    {
        Max = Now;
        expertise = Now;
    }

    if (Max > 0)
        expertise = expertise > Max ? Max : expertise;

    switch (attack)
    {
        case BASE_ATTACK:
            SetUInt32Value(PLAYER_EXPERTISE, expertise);
            break;
        case OFF_ATTACK:
            SetUInt32Value(PLAYER_OFFHAND_EXPERTISE, expertise);
            break;
        default:
            break;
    }
}

void Player::ApplyManaRegenBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseManaRegen, amount);
    UpdateManaRegen();
}

void Player::ApplyHealthRegenBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseHealthRegen, amount);
}

void Player::UpdateManaRegen()
{
    if( HasAuraTypeWithMiscvalue(SPELL_AURA_PREVENT_REGENERATE_POWER, POWER_MANA + 1) )
    {
        SetStatFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER, 0);
        SetStatFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER, 0);
        return;
    }

    float Intellect = GetStat(STAT_INTELLECT);
    // Mana regen from spirit and intellect
    float power_regen = std::sqrt(Intellect) * OCTRegenMPPerSpirit();
    // Apply PCT bonus from SPELL_AURA_MOD_POWER_REGEN_PERCENT aura on spirit base regen
    power_regen *= GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);

    // Mana regen from SPELL_AURA_MOD_POWER_REGEN aura
    float power_regen_mp5 = (GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA) + m_baseManaRegen) / 5.0f;

    // Get bonus from SPELL_AURA_MOD_MANA_REGEN_FROM_STAT aura
    AuraEffectList const& regenAura = GetAuraEffectsByType(SPELL_AURA_MOD_MANA_REGEN_FROM_STAT);
    for (AuraEffectList::const_iterator i = regenAura.begin(); i != regenAura.end(); ++i)
    {
        power_regen_mp5 += GetStat(Stats((*i)->GetMiscValue())) * (*i)->GetAmount() / 500.0f;
    }

    // Set regen rate in cast state apply only on spirit based regen
    int32 modManaRegenInterrupt = GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT);
    if (modManaRegenInterrupt > 100)
        modManaRegenInterrupt = 100;
    SetStatFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER, power_regen_mp5 + CalculatePct(power_regen, modManaRegenInterrupt));

    SetStatFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER, power_regen_mp5 + power_regen);
}

void Player::UpdateRuneRegen(RuneType rune)
{
    if (GetClass() != CLASS_DEATH_KNIGHT)
        return;

    if (rune >= NUM_RUNE_TYPES)
        return;

    uint32 cooldown = 0;

    for (uint32 i = 0; i < MAX_RUNES; ++i)
    {
        if (GetBaseRune(i) == rune)
        {
            cooldown = GetRuneBaseCooldown(i, true);
            break;
        }
    }

    if (cooldown <= 0)
        return;

    float regen = float(1 * IN_MILLISECONDS) / float(cooldown);
    SetFloatValue(PLAYER_RUNE_REGEN_1 + uint8(rune), regen);
}

void Player::_ApplyAllStatBonuses()
{
    SetCanModifyStats(false);

    _ApplyAllAuraStatMods();
    _ApplyAllItemMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

void Player::_RemoveAllStatBonuses()
{
    SetCanModifyStats(false);

    _RemoveAllItemMods();
    _RemoveAllAuraStatMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

/*#######################################
########                         ########
########    MOBS STAT SYSTEM     ########
########                         ########
#######################################*/

bool Creature::UpdateStats(Stats /*stat*/)
{
    return true;
}

bool Creature::UpdateAllStats()
{
    UpdateMaxHealth();
    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllResistances();

    return true;
}

void Creature::UpdateResistances(uint32 school)
{
    float StVal = 0.0f;
    if (IsGuard() && GetOwner() && GetOwner()->GetTypeId() == TYPEID_PLAYER)
        StVal = float(GetOwner()->GetResistance(SpellSchools(school)) * sHelper->GetFloat(2010));

    if (school > SPELL_SCHOOL_NORMAL)
    {
        float value = GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));
        SetResistance(SpellSchools(school), int32(value + StVal));
    }
    else
        UpdateArmor();
}

void Creature::UpdateArmor()
{
    float StVal = 0.0f;
    if (IsGuard() && GetOwner() && GetOwner()->GetTypeId() == TYPEID_PLAYER)
        StVal = float(GetOwner()->GetArmor() * sHelper->GetFloat(2010));

    float value = GetTotalAuraModValue(UNIT_MOD_ARMOR);
    SetArmor(int32(value + StVal));
}

void Creature::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;
    AsInt64 oldMaxHealth = GetAsMaxHealth();
    AsInt64 oldHealth = GetAsHealth();

    float compatValue = GetTotalAuraModValue(unitMod);
    AsInt64 createHealth = GetAsCreateHealth();
    if (createHealth <= 0)
        createHealth = AsInt64(GetCreateHealth());

    AsInt64 realValue = ScaleAsInt64Value(createHealth, GetModifierValue(unitMod, BASE_PCT));
    realValue += AsInt64FromFloatValue(GetModifierValue(unitMod, TOTAL_VALUE));
    realValue += GetAsAuraHealthFlatBonus();
    realValue = ScaleAsInt64Value(realValue, GetModifierValue(unitMod, TOTAL_PCT));

    // Keep exact health when the compatibility value is only its rounded float projection.
    if (compatValue > static_cast<float>(realValue))
        realValue = std::max(realValue, AsInt64FromFloatValue(compatValue));

    if (IsGuard() && GetOwner() && GetOwner()->GetTypeId() == TYPEID_PLAYER)
        realValue += ScaleAsInt64Value(GetOwner()->GetAsMaxHealth(), sHelper->GetFloat(2010));

    SetAsMaxHealth(realValue);
    if (oldMaxHealth > 0 && oldHealth > 0 && oldHealth < oldMaxHealth)
        SetAsHealth(MultiplyDivideAsInt64(oldHealth, realValue, oldMaxHealth));
}

void Creature::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(static_cast<uint16>(UNIT_MOD_POWER_START) + power);
    AsInt64 oldMaxPower = GetAsMaxPower(power);
    AsInt64 oldPower = GetAsPower(power);

    float compatValue  = GetTotalAuraModValue(unitMod);
    AsInt64 createPower = power == POWER_MANA ? GetAsCreateMana() : AsInt64(GetCreatePowers(power));
    if (createPower <= 0)
        createPower = AsInt64(GetCreatePowers(power));

    AsInt64 realValue = ScaleAsInt64Value(createPower, GetModifierValue(unitMod, BASE_PCT));
    realValue += AsInt64FromFloatValue(GetModifierValue(unitMod, TOTAL_VALUE));
    realValue = ScaleAsInt64Value(realValue, GetModifierValue(unitMod, TOTAL_PCT));

    AsInt64 compatRealValue = AsInt64FromFloatValue(compatValue);
    if (compatRealValue > realValue)
        realValue = compatRealValue;

    if (IsGuard() && GetOwner() && GetOwner()->GetTypeId() == TYPEID_PLAYER)
        realValue += ScaleAsInt64Value(GetOwner()->GetAsMaxPower(power), sHelper->GetFloat(2010));

    SetAsMaxPower(power, realValue);
    if (oldMaxPower > 0 && oldPower > 0 && oldPower < oldMaxPower)
        SetAsPower(power, MultiplyDivideAsInt256(oldPower, realValue, oldMaxPower));
}

void Creature::UpdateAttackPowerAndDamage(bool ranged)
{
    UnitMods unitMod = ranged ? UNIT_MOD_ATTACK_POWER_RANGED : UNIT_MOD_ATTACK_POWER;

    uint16 index = UNIT_FIELD_ATTACK_POWER;
    uint16 indexMod = UNIT_FIELD_ATTACK_POWER_MODS;
    uint16 indexMulti = UNIT_FIELD_ATTACK_POWER_MULTIPLIER;

    if (ranged)
    {
        index = UNIT_FIELD_RANGED_ATTACK_POWER;
        indexMod = UNIT_FIELD_RANGED_ATTACK_POWER_MODS;
        indexMulti = UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER;
    }

    AsInt64 baseAttackPower = ranged ? AsBaseRangedAttackPower : AsBaseAttackPower;
    if (baseAttackPower <= 0)
        baseAttackPower = AsInt64FromFloatValue(GetModifierValue(unitMod, BASE_VALUE));

    AsInt64 realAttackPower = ScaleAsInt64Value(baseAttackPower, GetModifierValue(unitMod, BASE_PCT));
    realAttackPower += AsInt64FromFloatValue(GetModifierValue(unitMod, TOTAL_VALUE));
    realAttackPower = ScaleAsInt64Value(realAttackPower, GetModifierValue(unitMod, TOTAL_PCT));

    if (realAttackPower < 0)
        realAttackPower = AsInt64Zero();

    if (ranged)
        AsRangedAttackPower = realAttackPower;
    else
        AsAttackPower = realAttackPower;

    int32 baseAttackPowerCompat = AsInt64ToInt32Clamp(realAttackPower, std::numeric_limits<int32>::max());

    SetInt32Value(index, baseAttackPowerCompat);      // UNIT_FIELD_(RANGED)_ATTACK_POWER
    SetInt32Value(indexMod, 0);                       // real AP is carried in AsAttackPower/AsRangedAttackPower
    SetFloatValue(indexMulti, 0.0f);                  // keep compat GetTotalAttackPowerValue at final clamped AP

    // automatically update weapon damage after attack power modification
    if (ranged)
        UpdateDamagePhysical(RANGED_ATTACK);
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        UpdateDamagePhysical(OFF_ATTACK);
    }
}

void Creature::CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, bool addTotalPct, float& minDamage, float& maxDamage)
{
    UnitMods unitMod;
    float variance = 1.0f;
    switch (attType)
    {
        case BASE_ATTACK:
        default:
            variance = GetCreatureTemplate()->BaseVariance;
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            variance = GetCreatureTemplate()->BaseVariance;
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            variance = GetCreatureTemplate()->RangeVariance;
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
    }

    if (attType == OFF_ATTACK && !haveOffhandWeapon())
    {
        minDamage = 0.0f;
        maxDamage = 0.0f;
        return;
    }

    float weaponMinDamage = GetWeaponDamageRange(attType, MINDAMAGE);
    float weaponMaxDamage = GetWeaponDamageRange(attType, MAXDAMAGE);

    if (!CanUseAttackType(attType)) // disarm case
    {
        weaponMinDamage = 0.0f;
        weaponMaxDamage = 0.0f;
    }

    float attackPower      = GetTotalAttackPowerValue(attType);
    float attackSpeedMulti = GetAPMultiplier(attType, normalized);
    float baseValue        = GetModifierValue(unitMod, BASE_VALUE) + (attackPower / 14.0f) * variance;
    float basePct          = GetModifierValue(unitMod, BASE_PCT) * attackSpeedMulti;
    float totalValue       = GetModifierValue(unitMod, TOTAL_VALUE);
    float totalPct         = addTotalPct ? GetModifierValue(unitMod, TOTAL_PCT) : 1.0f;
    float dmgMultiplier    = GetCreatureTemplate()->DamageModifier; // = DamageModifier * _GetDamageMod(rank);

    minDamage = ((weaponMinDamage + baseValue) * dmgMultiplier * basePct + totalValue) * totalPct;
    maxDamage = ((weaponMaxDamage + baseValue) * dmgMultiplier * basePct + totalValue) * totalPct;

    // Keep client-facing weapon fields inside uint32, real overflow damage is carried separately.
    float maxCompatDamage = float(std::numeric_limits<int32>::max());
    if (minDamage < 0.0f)
        minDamage = 0.0f;
    else if (minDamage > maxCompatDamage)
        minDamage = maxCompatDamage;

    if (maxDamage < 0.0f)
        maxDamage = 0.0f;
    else if (maxDamage > maxCompatDamage)
        maxDamage = maxCompatDamage;
    if (minDamage > maxDamage)
        minDamage = maxDamage;
}

/*#######################################
########                         ########
########    PETS STAT SYSTEM     ########
########                         ########
#######################################*/

bool Guardian::UpdateStats(Stats stat)
{
    if (stat >= MAX_STATS)
        return false;

    float value = GetTotalStatValue(stat);

    float StVal = 0.0f;
    if (GetOwner() && GetOwner()->GetTypeId() == TYPEID_PLAYER)
        StVal = float(GetOwner()->GetStat(stat) * sHelper->GetFloat(2010));

    SetStat(stat, ClampFloatToInt32Field(value + StVal));

    switch (stat)
    {
        case STAT_STRENGTH:
            UpdateAttackPowerAndDamage();
            break;
        case STAT_AGILITY:
            UpdateArmor();
            break;
        case STAT_STAMINA:
            UpdateMaxHealth();
            break;
        case STAT_INTELLECT:
            UpdateMaxPower(POWER_MANA);
            break;
        case STAT_SPIRIT:
            break;
    }

    return true;
}

bool Guardian::UpdateAllStats()
{
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        UpdateStats(Stats(i));

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllResistances();
    return true;
}

void Guardian::UpdateArmor()
{
    float value = GetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE);
    value *= GetModifierValue(UNIT_MOD_ARMOR, BASE_PCT);
    value += std::max<float>(GetStat(STAT_AGILITY) - GetCreateStat(STAT_AGILITY), 0.0f) * 2.0f;
    value += GetModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE);
    value *= GetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT);
    SetArmor(int32(value));
}

void Guardian::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;
    float stamina = std::max<float>(GetStat(STAT_STAMINA) - GetCreateStat(STAT_STAMINA), 0.0f);

    float multiplicator;
    switch (GetEntry())
    {
        case NPC_IMP:
            multiplicator = 8.4f;
            break;
        case NPC_WATER_ELEMENTAL_TEMP:
            multiplicator = 7.5f;
            break;
        case NPC_WATER_ELEMENTAL_PERM:
            multiplicator = 7.5f;
            break;
        case NPC_VOIDWALKER:
            multiplicator = 11.0f;
            break;
        case NPC_SUCCUBUS:
            multiplicator = 9.1f;
            break;
        case NPC_FELHUNTER:
            multiplicator = 9.5f;
            break;
        case NPC_FELGUARD:
            multiplicator = 11.0f;
            break;
        case NPC_BLOODWORM:
            multiplicator = 1.0f;
            break;
        default:
            multiplicator = 10.0f;
            break;
    }

    float value = GetModifierValue(unitMod, BASE_VALUE);// xinef: Do NOT add base health TWICE + GetCreateHealth();
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + stamina * multiplicator;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    AsInt64 realValue = AsInt64FromFloatValue(value);
    AsInt64 createHealthOverflow = GetAsCreateHealth() - AsInt64(GetCreateHealth());
    if (createHealthOverflow > 0)
        realValue += ScaleAsInt64Value(createHealthOverflow, GetModifierValue(unitMod, BASE_PCT) * GetModifierValue(unitMod, TOTAL_PCT));

    if (Player* owner = GetOwner() ? GetOwner()->ToPlayer() : nullptr)
    {
        AsInt64 ownerStaminaOverflow = ScaleAsInt64Value(AsGetRealStatOverflow(owner, STAT_STAMINA), sHelper->GetFloat(2010));
        if (ownerStaminaOverflow > 0)
            realValue += ScaleAsInt64Value(ownerStaminaOverflow, multiplicator * GetModifierValue(unitMod, TOTAL_PCT));
    }

    SetAsMaxHealth(realValue);
}

void Guardian::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(static_cast<uint16>(UNIT_MOD_POWER_START) + power);

    float addValue = (power == POWER_MANA) ? std::max<float>(GetStat(STAT_INTELLECT) - GetCreateStat(STAT_INTELLECT), 0.0f) : 0.0f;
    float multiplicator = 15.0f;

    switch (GetEntry())
    {
        case NPC_IMP:
        case NPC_WATER_ELEMENTAL_TEMP:
        case NPC_WATER_ELEMENTAL_PERM:
            multiplicator = 4.95f;
            break;
        case NPC_VOIDWALKER:
        case NPC_SUCCUBUS:
        case NPC_FELHUNTER:
        case NPC_FELGUARD:
            multiplicator = 11.5f;
            break;
        default:
            multiplicator = 15.0f;
            break;
    }

    // xinef: Do NOT add base mana TWICE
    float value = GetModifierValue(unitMod, BASE_VALUE) + (power != POWER_MANA ? GetCreatePowers(power) : 0);
    value *= GetModifierValue(unitMod, BASE_PCT);
    value += GetModifierValue(unitMod, TOTAL_VALUE) + addValue * multiplicator;
    value *= GetModifierValue(unitMod, TOTAL_PCT);

    AsInt64 realValue = AsInt64FromFloatValue(value);
    if (power == POWER_MANA)
    {
        AsInt64 createManaOverflow = GetAsCreateMana() - AsInt64(GetCreateMana());
        if (createManaOverflow > 0)
            realValue += ScaleAsInt64Value(createManaOverflow, GetModifierValue(unitMod, BASE_PCT) * GetModifierValue(unitMod, TOTAL_PCT));

        if (Player* owner = GetOwner() ? GetOwner()->ToPlayer() : nullptr)
        {
            AsInt64 ownerIntellectOverflow = ScaleAsInt64Value(AsGetRealStatOverflow(owner, STAT_INTELLECT), sHelper->GetFloat(2010));
            if (ownerIntellectOverflow > 0)
                realValue += ScaleAsInt64Value(ownerIntellectOverflow, multiplicator * GetModifierValue(unitMod, TOTAL_PCT));
        }
    }

    SetAsMaxPower(power, realValue);
}

void Guardian::UpdateAttackPowerAndDamage(bool ranged)
{
    if (ranged)
        return;

    float val = 0.0f;
    UnitMods unitMod = UNIT_MOD_ATTACK_POWER;

    if (GetEntry() == NPC_IMP)                                     // imp's attack power
        val = GetStat(STAT_STRENGTH) - 10.0f;
    else if (IsPetGhoul())                                         // DK's ghoul attack power
        val = 589 /*xinef: base ap!*/ + GetStat(STAT_STRENGTH) + GetStat(STAT_AGILITY);
    else
        val = 2 * GetStat(STAT_STRENGTH) - 20.0f;

    SetModifierValue(unitMod, BASE_VALUE, val);

    //in BASE_VALUE of UNIT_MOD_ATTACK_POWER for creatures we store data of meleeattackpower field in DB
    float base_attPower  = GetModifierValue(unitMod, BASE_VALUE) * GetModifierValue(unitMod, BASE_PCT);
    float attPowerMod = GetModifierValue(unitMod, TOTAL_VALUE);
    float attPowerMultiplier = GetModifierValue(unitMod, TOTAL_PCT) - 1.0f;

    AsInt64 realBaseAttackPower = AsInt64FromFloatValue(GetModifierValue(unitMod, BASE_VALUE));
    if (Player* owner = GetOwner() ? GetOwner()->ToPlayer() : nullptr)
    {
        float ownerScale = sHelper->GetFloat(2010);
        AsInt64 ownerStrengthOverflow = ScaleAsInt64Value(AsGetRealStatOverflow(owner, STAT_STRENGTH), ownerScale);
        AsInt64 ownerAgilityOverflow = ScaleAsInt64Value(AsGetRealStatOverflow(owner, STAT_AGILITY), ownerScale);

        if (GetEntry() == NPC_IMP)
            realBaseAttackPower += ownerStrengthOverflow;
        else if (IsPetGhoul())
            realBaseAttackPower += ownerStrengthOverflow + ownerAgilityOverflow;
        else
            realBaseAttackPower += ownerStrengthOverflow * 2;
    }

    realBaseAttackPower = ScaleAsInt64Value(realBaseAttackPower, GetModifierValue(unitMod, BASE_PCT));
    AsInt64 realAttackPower = realBaseAttackPower + AsInt64FromFloatValue(attPowerMod);
    realAttackPower = ScaleAsInt64Value(realAttackPower, GetModifierValue(unitMod, TOTAL_PCT));
    if (realAttackPower < 0)
        realAttackPower = AsInt64Zero();

    AsBaseAttackPower = realBaseAttackPower > 0 ? realBaseAttackPower : AsInt64Zero();
    AsAttackPower = realAttackPower;

    //UNIT_FIELD_(RANGED)_ATTACK_POWER field
    int32 baseAttackPowerCompat = ClampFloatToInt32Field(base_attPower);
    int32 attackPowerModCompat = ClampFloatToInt32Field(attPowerMod);
    ClampAttackPowerFields(baseAttackPowerCompat, attackPowerModCompat);

    SetInt32Value(UNIT_FIELD_ATTACK_POWER, baseAttackPowerCompat);
    //UNIT_FIELD_(RANGED)_ATTACK_POWER_MODS field
    SetInt32Value(UNIT_FIELD_ATTACK_POWER_MODS, attackPowerModCompat);
    //UNIT_FIELD_(RANGED)_ATTACK_POWER_MULTIPLIER field
    SetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER, attPowerMultiplier);

    //automatically update weapon damage after attack power modification
    UpdateDamagePhysical(BASE_ATTACK);
}

void Guardian::UpdateDamagePhysical(WeaponAttackType attType)
{
    if (attType > BASE_ATTACK)
        return;

    UnitMods unitMod = UNIT_MOD_DAMAGE_MAINHAND;

    float att_speed = float(GetAttackTime(BASE_ATTACK)) / 1000.0f;

    float base_value  = GetModifierValue(unitMod, BASE_VALUE) + GetTotalAttackPowerValue(attType) / 14.0f * att_speed;
    float base_pct    = GetModifierValue(unitMod, BASE_PCT);
    float total_value = GetModifierValue(unitMod, TOTAL_VALUE);
    float total_pct   = GetModifierValue(unitMod, TOTAL_PCT);

    float weapon_mindamage = GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE);
    float weapon_maxdamage = GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE);

    float mindamage = ((base_value + weapon_mindamage) * base_pct + total_value) * total_pct;
    float maxdamage = ((base_value + weapon_maxdamage) * base_pct + total_value) * total_pct;

    float maxCompatDamage = float(std::numeric_limits<int32>::max());
    if (mindamage < 0.0f)
        mindamage = 0.0f;
    else if (mindamage > maxCompatDamage)
        mindamage = maxCompatDamage;
    if (maxdamage < 0.0f)
        maxdamage = 0.0f;
    else if (maxdamage > maxCompatDamage)
        maxdamage = maxCompatDamage;
    if (mindamage > maxdamage)
        mindamage = maxdamage;

    //  Pet's base damage changes depending on happiness
    if (IsHunterPet() && attType == BASE_ATTACK)
    {
        switch (ToPet()->GetHappinessState())
        {
            case HAPPY:
                // 125% of normal damage
                mindamage = mindamage * 1.25f;
                maxdamage = maxdamage * 1.25f;
                break;
            case CONTENT:
                // 100% of normal damage, nothing to modify
                break;
            case UNHAPPY:
                // 75% of normal damage
                mindamage = mindamage * 0.75f;
                maxdamage = maxdamage * 0.75f;
                break;
        }
    }

    SetStatFloatValue(UNIT_FIELD_MINDAMAGE, mindamage);
    SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, maxdamage);
}
