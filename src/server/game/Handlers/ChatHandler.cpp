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

#include "AccountMgr.h"
#include "CellImpl.h"
#include "ChannelMgr.h"
#include "Chat.h"
#include "ChatPackets.h"
#include "Common.h"
#include "CreatureData.h"
#include "GameTime.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Language.h"
#include "Item.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "Pet.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "Util.h"
#include "Warden.h"
#include "World.h"

#include <limits>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "StringConvert.h"
#include "NativeUIProtocol.h"
#include "../../scripts/Custom/AddonActive/DoAddon.h"
#include "../../scripts/Custom/AddonActive/FrozenCoreModuleBridge.h"
#include "..\..\scripts\Custom\_Helper\Helper.h"
#include "../../scripts/Custom/_Helper/AsInt64.h"
#include "../../scripts/Custom/_Helper/CombatPower.h"
#include "../../scripts/Custom/ClassConfig/ClassMod.h"
#include "../../scripts/Custom/CreatureMod/AsCreature.h"
#include "../../scripts/Custom/ItemRandomAtMgr/ItemRandomAtMgr.h"
#include "../../scripts/Custom/ItemFuncMgr/ItemFuncMgr.h"
#include "../../scripts/Custom/PlayerModMgr/PlayerModMgr.h"
#include <iostream>
#include <sstream>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#pragma execution_character_set("utf-8")

inline bool isNasty(uint8 c)
{
    if (c == '\t')
    {
        return false;
    }

    if (c <= '\037') // ASCII control block
    {
        return true;
    }

    return false;
}

namespace
{
constexpr char PATTR_PREFIX[] = "PATTRPANEL";
constexpr bool PATTR_DEBUG_LOG = false;

void PattrConsole(std::string const& message)
{
    std::string line = ">> 小赛核心属性面板调试：" + message + "\n";

#ifdef _WIN32
    int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line.c_str(), -1, nullptr, 0);
    if (wideLength > 0)
    {
        std::wstring wideLine;
        wideLine.resize(wideLength - 1);
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line.c_str(), -1, wideLine.data(), wideLength);

        DWORD written = 0;
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        if (console && console != INVALID_HANDLE_VALUE && WriteConsoleW(console, wideLine.c_str(), DWORD(wideLine.size()), &written, nullptr))
            return;
    }
#endif

    std::cout << line;
}

std::string PattrShort(std::string const& text)
{
    std::string result = text;
    for (char& ch : result)
    {
        if (ch == '\t')
            ch = '|';
        else if (ch == '\r' || ch == '\n')
            ch = ' ';
    }

    if (result.size() > 160)
        result = result.substr(0, 160) + "...";

    return result;
}

void PattrSendAddon(Player* player, std::string const& message)
{
    if (!player || !player->GetSession())
        return;

    if (PATTR_DEBUG_LOG)
        PattrConsole("发送插件包 player=" + player->GetName() + " payload=" + message);

    std::string full = std::string(PATTR_PREFIX) + "\t" + message;
    WorldPacket packet(SMSG_MESSAGECHAT, 100);
    packet << uint8(CHAT_MSG_WHISPER);
    packet << int32(LANG_ADDON);
    packet << uint64(player->GetGUID().GetRawValue());
    packet << uint32(0);
    packet << uint64(player->GetGUID().GetRawValue());
    packet << uint32(full.length() + 1);
    packet << full;
    packet << uint8(0);
    player->GetSession()->SendPacket(&packet);
}

std::string AsPattrValue(AsInt64 const& value)
{
    return AsInt64ToString(value);
}

bool PattrIsLegacyCompatCapValue(AsInt64 const& value)
{
    return value == AsInt64(2000000000)
        || value == AsInt64(2100000000)
        || value == AsInt64(std::numeric_limits<int32>::max());
}

AsInt64 AsScaleCurrent(uint32 current, uint32 compatMax, AsInt64 const& realMax)
{
    if (!compatMax || AsInt64IsZero(realMax))
        return AsInt64(current);

    return MultiplyDivideAsInt256(realMax, current, compatMax);
}

void PattrAddField(std::vector<std::string>& fields, char const* key, std::string const& value)
{
    fields.emplace_back(std::string(key) + "=" + value);
}

void PattrAddAttrField(std::vector<std::string>& fields, int32 id, AsInt64 const& value)
{
    PattrAddField(fields, (std::string("ATTR_") + std::to_string(id)).c_str(), AsPattrValue(value));
}

AsInt64 PattrRealItemMod(Player* player, ItemModType mod, AsInt64 const& compatBase = AsInt64Zero())
{
    return compatBase + player->GetAsStatBonus(mod) - player->GetAsStatCompatBonus(mod);
}

AsInt64 PattrRealCombatRating(Player* player, CombatRating rating)
{
    AsInt64 compatValue = player->GetUInt32Value(uint16(PLAYER_FIELD_COMBAT_RATING_1) + uint16(rating));
    return compatValue + player->GetAsCombatRatingBonus(rating) - player->GetAsCombatRatingCompatBonus(rating);
}

AsInt64 PattrFromFloat(float value)
{
    if (value <= 0.0f)
        return AsInt64Zero();

    return std::isfinite(value) ? AsInt64(value) : AsInt64Zero();
}

AsInt64 PattrScaleFloat(AsInt64 const& value, float modifier)
{
    return ScaleAsInt64ByDouble(value, modifier);
}

AsInt64 PattrRealStatOverflow(Player* player, Stats stat)
{
    if (!player || stat >= MAX_STATS)
        return AsInt64Zero();

    AsInt64 real = player->GetAsRealStat(stat);
    AsInt64 compat = AsInt64(player->GetStat(stat));
    return real > compat ? real - compat : AsInt64Zero();
}

AsInt64 PattrRealAttackPowerOverflow(Player* player, WeaponAttackType attType)
{
    if (!player)
        return AsInt64Zero();

    AsInt64 real = player->GetAsTotalAttackPowerValue(attType);
    AsInt64 compat = PattrFromFloat(player->GetTotalAttackPowerValue(attType));
    return real > compat ? real - compat : AsInt64Zero();
}

AsInt64 PattrRealSpellPower(Player* player)
{
    AsInt64 fixedSpellPower = sCustomClass->GetAsSpNow(player);
    if (fixedSpellPower > 0 && !PattrIsLegacyCompatCapValue(fixedSpellPower))
        return fixedSpellPower;

    AsInt64 spellPower = AsInt64Zero();
    for (uint8 school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
    {
        AsInt64 schoolSpellPower = player->GetAsSpellPowerValue(SpellSchoolMask(1 << school));
        if (schoolSpellPower > spellPower)
            spellPower = schoolSpellPower;
    }

    return spellPower;
}

AsInt64 PattrRealArmor(Player* player)
{
    if (!player)
        return AsInt64Zero();

    AsInt64 armor = player->GetArmor();
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item const* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item || item->IsBroken())
            continue;

        ItemTemplate const* proto = item->GetTemplate();
        if (proto && proto->AsArmor > proto->Armor)
            armor += proto->AsArmor - proto->Armor;
    }
    return armor;
}

AsInt64 PattrRealHealingPower(Player* player)
{
    if (!player)
        return AsInt64Zero();

    AsInt64 healingPower = AsInt64Zero();
    for (uint8 school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
        healingPower = std::max(healingPower, player->GetAsHealingPowerValue(SpellSchoolMask(1 << school)));

    return healingPower;
}

UnitMods PattrDamageUnitMod(WeaponAttackType attType)
{
    switch (attType)
    {
        case OFF_ATTACK:
            return UNIT_MOD_DAMAGE_OFFHAND;
        case RANGED_ATTACK:
            return UNIT_MOD_DAMAGE_RANGED;
        case BASE_ATTACK:
        default:
            return UNIT_MOD_DAMAGE_MAINHAND;
    }
}

AsInt64 PattrRealAttackPower(Player* player, WeaponAttackType attType)
{
    AsInt64 real = player->GetAsTotalAttackPowerValue(attType);

    return real > 0 ? real : AsInt64Zero();
}

std::pair<AsInt64, AsInt64> PattrWeaponDamageRange(Player* player, WeaponAttackType attType)
{
    if (!player)
        return { AsInt64Zero(), AsInt64Zero() };

    auto addSoulWeaponDamage = [&](std::pair<AsInt64, AsInt64> range)
    {
        if (attType == BASE_ATTACK)
        {
            auto soulWeaponRange = sItemFuncMgr->GetSoulWeaponDamageRange(player);
            range.first += soulWeaponRange.first;
            range.second += soulWeaponRange.second;
        }

        if (range.first <= 0)
            range.first = range.second;
        if (range.second <= 0)
            range.second = range.first;
        if (range.second < range.first)
            std::swap(range.first, range.second);
        return range;
    };

    AsInt64 fixedMin = sCustomClass->GetAsMinWpNow(player);
    AsInt64 fixedMax = sCustomClass->GetAsMaxWpNow(player);
    if (PattrIsLegacyCompatCapValue(fixedMin))
        fixedMin = AsInt64Zero();
    if (PattrIsLegacyCompatCapValue(fixedMax))
        fixedMax = AsInt64Zero();

    if (fixedMin > 0 || fixedMax > 0)
    {
        if (fixedMin <= 0)
            fixedMin = fixedMax;
        if (fixedMax <= 0)
            fixedMax = fixedMin;
        if (fixedMax < fixedMin)
            std::swap(fixedMin, fixedMax);
        return addSoulWeaponDamage({ fixedMin, fixedMax });
    }

    if (player->IsInFeralForm() || !player->CanUseAttackType(attType))
        return { PattrFromFloat(player->GetWeaponDamageRange(attType, MINDAMAGE)), PattrFromFloat(player->GetWeaponDamageRange(attType, MAXDAMAGE)) };

    Item* item = player->GetWeaponForAttack(attType, true);
    if (!item)
        return addSoulWeaponDamage({ PattrFromFloat(player->GetWeaponDamageRange(attType, MINDAMAGE)), PattrFromFloat(player->GetWeaponDamageRange(attType, MAXDAMAGE)) });

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return addSoulWeaponDamage({ PattrFromFloat(player->GetWeaponDamageRange(attType, MINDAMAGE)), PattrFromFloat(player->GetWeaponDamageRange(attType, MAXDAMAGE)) });

    AsInt64 minDamage = AsInt64Zero();
    AsInt64 maxDamage = AsInt64Zero();
    if (sItemRandomAtMgr->IsGenerateItemAt(proto->ItemId))
    {
        minDamage = item->AsMinWDmg > 0 ? item->AsMinWDmg : proto->AsDamageMin[0];
        maxDamage = item->AsMaxWDmg > 0 ? item->AsMaxWDmg : proto->AsDamageMax[0];
        if (minDamage <= 0)
            minDamage = AsInt64(item->MinWDmg);
        if (maxDamage <= 0)
            maxDamage = AsInt64(item->MaxWDmg);
        float rankMod = sItemRandomAtMgr->GetModForRank(proto->ItemId, item->RmAtLevel);
        minDamage = PattrScaleFloat(minDamage, 1.0f + rankMod);
        maxDamage = PattrScaleFloat(maxDamage, 1.0f + rankMod);
    }
    else
    {
        minDamage = proto->AsDamageMin[0];
        maxDamage = proto->AsDamageMax[0];
        if (minDamage <= 0)
            minDamage = PattrFromFloat(proto->Damage[0].DamageMin);
        if (maxDamage <= 0)
            maxDamage = PattrFromFloat(proto->Damage[0].DamageMax);
    }

    if (minDamage <= 0 && maxDamage <= 0)
        return addSoulWeaponDamage({ PattrFromFloat(player->GetWeaponDamageRange(attType, MINDAMAGE)), PattrFromFloat(player->GetWeaponDamageRange(attType, MAXDAMAGE)) });
    if (minDamage <= 0)
        minDamage = maxDamage;
    if (maxDamage <= 0)
        maxDamage = minDamage;
    if (maxDamage < minDamage)
        std::swap(minDamage, maxDamage);

    float eqMod = sPlayerModMgr->GetModPlayerEqSlot(player, item->GetSlot());
    minDamage = PattrScaleFloat(minDamage, 1.0f + eqMod);
    maxDamage = PattrScaleFloat(maxDamage, 1.0f + eqMod);

    return addSoulWeaponDamage({ minDamage, maxDamage });
}

AsInt64 PattrDamagePoint(Player* player, WeaponAttackType attType, AsInt64 const& weaponDamage, AsInt64 const& attackPower)
{
    UnitMods unitMod = PattrDamageUnitMod(attType);
    float attackSpeedMod = player->GetAPMultiplier(attType, false);
    if (attackSpeedMod <= 0.0f)
        attackSpeedMod = float(player->GetAttackTime(attType)) / 1000.0f;

    AsInt64 apDamage = PattrScaleFloat(attackPower, attackSpeedMod / 14.0f);
    AsInt64 effectiveWeaponDamage = weaponDamage;
    if (attType == RANGED_ATTACK)
        effectiveWeaponDamage += PattrScaleFloat(PattrFromFloat(player->GetAmmoDPS()), attackSpeedMod);

    AsInt64 damage = PattrFromFloat(player->GetModifierValue(unitMod, BASE_VALUE)) + apDamage + effectiveWeaponDamage;
    damage = PattrScaleFloat(damage, player->GetModifierValue(unitMod, BASE_PCT));
    damage += PattrFromFloat(player->GetModifierValue(unitMod, TOTAL_VALUE));
    damage = PattrScaleFloat(damage, player->GetModifierValue(unitMod, TOTAL_PCT));

    return damage > 0 ? damage : AsInt64Zero();
}

std::string PattrDamageRange(Player* player, WeaponAttackType attType, AsInt64 const& attackPower)
{
    if (attType == OFF_ATTACK && !player->haveOffhandWeapon())
        return "0~0";

    AsInt64 fixedDamage = sCustomClass->GetAsDmgNow(player);
    if (fixedDamage > 0 && !PattrIsLegacyCompatCapValue(fixedDamage))
        return AsPattrValue(fixedDamage) + "~" + AsPattrValue(fixedDamage);

    auto weaponRange = PattrWeaponDamageRange(player, attType);
    if (weaponRange.first <= 0 && weaponRange.second <= 0)
        return "0~0";

    AsInt64 minDamage = PattrDamagePoint(player, attType, weaponRange.first, attackPower);
    AsInt64 maxDamage = PattrDamagePoint(player, attType, weaponRange.second, attackPower);
    if (maxDamage < minDamage)
        maxDamage = minDamage;

    return AsPattrValue(minDamage) + "~" + AsPattrValue(maxDamage);
}

void PattrSendChunked(Player* sender, char const* type, std::vector<std::string> const& fields)
{
    if (sender && sender->GetSession()->HasNativeUI())
    {
        uint8 kind = std::string(type) == "STATS" ? 0 : std::string(type) == "TARGET" ? 1 : 2;
        Unit* unit = kind == 0 ? sender : kind == 1 ? sender->GetSelectedUnit() : sender->GetPet();
        sender->GetSession()->SendNativeUISnapshot(kind, unit ? unit->GetGUID() : ObjectGuid::Empty, fields);
        return;
    }

    std::string chunk;
    uint32 chunkCount = 0;
    for (std::string const& field : fields)
    {
        if (!chunk.empty() && chunk.size() + field.size() + 1 > 210)
        {
            PattrSendAddon(sender, std::string(type) + ":" + chunk);
            ++chunkCount;
            chunk.clear();
        }

        if (!chunk.empty())
            chunk += "|";
        chunk += field;
    }

    if (!chunk.empty())
    {
        PattrSendAddon(sender, std::string(type) + ":" + chunk);
        ++chunkCount;
    }

    if (sender)
    {
        if (PATTR_DEBUG_LOG)
        {
            PattrConsole("发送分片 player=" + sender->GetName() + " type=" + type +
                " fields=" + std::to_string(fields.size()) + " chunks=" + std::to_string(chunkCount));
            LOG_INFO("server.worldserver", "[PATTR] send player={} type={} fields={} chunks={}",
                sender->GetName(), type, fields.size(), chunkCount);
        }
    }
}

bool PattrHasEquippedItem(Player* player, uint32 entry)
{
    if (!player)
        return false;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            if (item->GetEntry() == entry)
                return true;

    return false;
}

void PattrSendStats(Player* player)
{
    if (!player)
        return;

    // This is a read-only display request.  Rebuilding all stats here can
    // overwrite the already-loaded high-precision equipment totals with the
    // client's compatibility values, making the character appear weaker after
    // every panel refresh.

    std::vector<std::string> fields;
    PattrAddField(fields, "MONEY", std::to_string(player->GetMoney()));
    AsInt64 realMaxHealth = player->GetAsRealMaxHealth();
    if (AsInt64IsZero(realMaxHealth))
        realMaxHealth = AsInt64(player->GetMaxHealth());

    AsInt64 realMaxMana = player->GetAsRealMaxPower(POWER_MANA);
    if (AsInt64IsZero(realMaxMana))
        realMaxMana = AsInt64(player->GetMaxPower(POWER_MANA));

    PattrAddField(fields, "MAX_HEALTH", AsPattrValue(realMaxHealth));
    PattrAddAttrField(fields, ITEM_MOD_HEALTH, realMaxHealth);
    AsInt64 realCurHealth = player->GetAsHealth();
    if (AsInt64IsZero(realCurHealth))
        realCurHealth = AsScaleCurrent(player->GetHealth(), player->GetMaxHealth(), realMaxHealth);
    PattrAddField(fields, "CUR_HEALTH", AsPattrValue(realCurHealth));
    PattrAddField(fields, "MAX_MANA", AsPattrValue(realMaxMana));
    PattrAddAttrField(fields, ITEM_MOD_MANA, realMaxMana);
    AsInt64 realCurMana = player->GetAsPower(POWER_MANA);
    if (AsInt64IsZero(realCurMana))
        realCurMana = AsScaleCurrent(player->GetPower(POWER_MANA), player->GetMaxPower(POWER_MANA), realMaxMana);
    PattrAddField(fields, "CUR_MANA", AsPattrValue(realCurMana));
    PattrAddField(fields, "STRENGTH", AsPattrValue(player->GetAsRealStat(STAT_STRENGTH)));
    PattrAddAttrField(fields, ITEM_MOD_STRENGTH, player->GetAsRealStat(STAT_STRENGTH));
    PattrAddField(fields, "AGILITY", AsPattrValue(player->GetAsRealStat(STAT_AGILITY)));
    PattrAddAttrField(fields, ITEM_MOD_AGILITY, player->GetAsRealStat(STAT_AGILITY));
    PattrAddField(fields, "STAMINA", AsPattrValue(player->GetAsRealStat(STAT_STAMINA)));
    PattrAddAttrField(fields, ITEM_MOD_STAMINA, player->GetAsRealStat(STAT_STAMINA));
    PattrAddField(fields, "INTELLECT", AsPattrValue(player->GetAsRealStat(STAT_INTELLECT)));
    PattrAddAttrField(fields, ITEM_MOD_INTELLECT, player->GetAsRealStat(STAT_INTELLECT));
    PattrAddField(fields, "SPIRIT", AsPattrValue(player->GetAsRealStat(STAT_SPIRIT)));
    PattrAddAttrField(fields, ITEM_MOD_SPIRIT, player->GetAsRealStat(STAT_SPIRIT));
    PattrAddField(fields, "ARMOR", AsPattrValue(PattrRealArmor(player)));

    PattrAddAttrField(fields, ITEM_MOD_DEFENSE_SKILL_RATING, PattrRealCombatRating(player, CR_DEFENSE_SKILL));
    PattrAddAttrField(fields, ITEM_MOD_DODGE_RATING, PattrRealCombatRating(player, CR_DODGE));
    PattrAddAttrField(fields, ITEM_MOD_PARRY_RATING, PattrRealCombatRating(player, CR_PARRY));
    PattrAddAttrField(fields, ITEM_MOD_BLOCK_RATING, PattrRealCombatRating(player, CR_BLOCK));
    PattrAddAttrField(fields, ITEM_MOD_HIT_MELEE_RATING, PattrRealCombatRating(player, CR_HIT_MELEE));
    PattrAddAttrField(fields, ITEM_MOD_HIT_RANGED_RATING, PattrRealCombatRating(player, CR_HIT_RANGED));
    PattrAddAttrField(fields, ITEM_MOD_HIT_SPELL_RATING, PattrRealCombatRating(player, CR_HIT_SPELL));
    PattrAddAttrField(fields, ITEM_MOD_CRIT_MELEE_RATING, PattrRealCombatRating(player, CR_CRIT_MELEE));
    PattrAddAttrField(fields, ITEM_MOD_CRIT_RANGED_RATING, PattrRealCombatRating(player, CR_CRIT_RANGED));
    PattrAddAttrField(fields, ITEM_MOD_CRIT_SPELL_RATING, PattrRealCombatRating(player, CR_CRIT_SPELL));
    PattrAddAttrField(fields, ITEM_MOD_HIT_TAKEN_MELEE_RATING, PattrRealCombatRating(player, CR_HIT_TAKEN_MELEE));
    PattrAddAttrField(fields, ITEM_MOD_HIT_TAKEN_RANGED_RATING, PattrRealCombatRating(player, CR_HIT_TAKEN_RANGED));
    PattrAddAttrField(fields, ITEM_MOD_HIT_TAKEN_SPELL_RATING, PattrRealCombatRating(player, CR_HIT_TAKEN_SPELL));
    PattrAddAttrField(fields, ITEM_MOD_CRIT_TAKEN_MELEE_RATING, PattrRealCombatRating(player, CR_CRIT_TAKEN_MELEE));
    PattrAddAttrField(fields, ITEM_MOD_CRIT_TAKEN_RANGED_RATING, PattrRealCombatRating(player, CR_CRIT_TAKEN_RANGED));
    PattrAddAttrField(fields, ITEM_MOD_CRIT_TAKEN_SPELL_RATING, PattrRealCombatRating(player, CR_CRIT_TAKEN_SPELL));
    PattrAddAttrField(fields, ITEM_MOD_HASTE_MELEE_RATING, PattrRealCombatRating(player, CR_HASTE_MELEE));
    PattrAddAttrField(fields, ITEM_MOD_HASTE_RANGED_RATING, PattrRealCombatRating(player, CR_HASTE_RANGED));
    PattrAddAttrField(fields, ITEM_MOD_HASTE_SPELL_RATING, PattrRealCombatRating(player, CR_HASTE_SPELL));
    PattrAddAttrField(fields, ITEM_MOD_HIT_RATING, PattrRealCombatRating(player, CR_HIT_MELEE));
    PattrAddAttrField(fields, ITEM_MOD_CRIT_RATING, PattrRealCombatRating(player, CR_CRIT_MELEE));
    PattrAddAttrField(fields, ITEM_MOD_HIT_TAKEN_RATING, PattrRealCombatRating(player, CR_HIT_TAKEN_MELEE));
    PattrAddAttrField(fields, ITEM_MOD_CRIT_TAKEN_RATING, PattrRealCombatRating(player, CR_CRIT_TAKEN_MELEE));
    PattrAddAttrField(fields, ITEM_MOD_RESILIENCE_RATING, PattrRealCombatRating(player, CR_CRIT_TAKEN_MELEE));
    PattrAddAttrField(fields, ITEM_MOD_HASTE_RATING, PattrRealCombatRating(player, CR_HASTE_MELEE));
    PattrAddAttrField(fields, ITEM_MOD_EXPERTISE_RATING, PattrRealCombatRating(player, CR_EXPERTISE));

    AsInt64 attackPower = PattrRealAttackPower(player, BASE_ATTACK);
    PattrAddField(fields, "ATTACK_POWER", AsPattrValue(attackPower));
    PattrAddAttrField(fields, ITEM_MOD_ATTACK_POWER, attackPower);
    std::string mainHandDamage = PattrDamageRange(player, BASE_ATTACK, attackPower);
    std::string offHandDamage = PattrDamageRange(player, OFF_ATTACK, attackPower);
    PattrAddField(fields, "MAINHAND_DAMAGE", mainHandDamage);
    PattrAddField(fields, "OFFHAND_DAMAGE", offHandDamage);

    AsInt64 rangedAttackPower = PattrRealAttackPower(player, RANGED_ATTACK);
    PattrAddField(fields, "RANGED_ATTACK_POWER", AsPattrValue(rangedAttackPower));
    PattrAddAttrField(fields, ITEM_MOD_RANGED_ATTACK_POWER, rangedAttackPower);
    std::string rangedDamage = PattrDamageRange(player, RANGED_ATTACK, rangedAttackPower);
    PattrAddField(fields, "RANGED_DAMAGE", rangedDamage);

    PattrAddAttrField(fields, ITEM_MOD_SPELL_HEALING_DONE, PattrRealHealingPower(player));
    AsInt64 spellPower = PattrRealSpellPower(player);
    PattrAddField(fields, "SPELL_POWER", AsPattrValue(spellPower));
    PattrAddAttrField(fields, ITEM_MOD_SPELL_DAMAGE_DONE, spellPower);
    PattrAddAttrField(fields, ITEM_MOD_SPELL_POWER, spellPower);
    PattrAddAttrField(fields, ITEM_MOD_MANA_REGENERATION, PattrRealItemMod(player, ITEM_MOD_MANA_REGENERATION));
    PattrAddAttrField(fields, ITEM_MOD_ARMOR_PENETRATION_RATING, PattrRealCombatRating(player, CR_ARMOR_PENETRATION));
    PattrAddAttrField(fields, ITEM_MOD_HEALTH_REGEN, PattrRealItemMod(player, ITEM_MOD_HEALTH_REGEN));
    // 客户端字段保存的是“目标抗性修正”，法术穿透 +100 对应字段值 -100。
    // 必须按有符号数读取并取反，避免 -100 被解释为 4294967196。
    AsInt64 spellPenetration = -AsInt64(player->GetInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE));
    PattrAddAttrField(fields, ITEM_MOD_SPELL_PENETRATION,
        PattrRealItemMod(player, ITEM_MOD_SPELL_PENETRATION, spellPenetration));
    PattrAddAttrField(fields, ITEM_MOD_BLOCK_VALUE, PattrRealItemMod(player, ITEM_MOD_BLOCK_VALUE, AsInt64(player->GetUInt32Value(PLAYER_SHIELD_BLOCK))));

    if (player->GetSession()->HasNativeUI())
    {
        for (uint8 rating = 0; rating < MAX_COMBAT_RATING; ++rating)
            PattrAddField(fields, ("RATING_" + std::to_string(rating + 1)).c_str(),
                AsPattrValue(PattrRealCombatRating(player, CombatRating(rating))));

        PattrAddField(fields, "HEALING_POWER", AsPattrValue(PattrRealHealingPower(player)));
        PattrAddField(fields, "DEFENSE", std::to_string(player->GetDefenseSkillValue()));
        PattrAddField(fields, "DODGE_CHANCE", Acore::StringFormat("{:.2f}%",
            player->GetFloatValue(PLAYER_DODGE_PERCENTAGE)));
        PattrAddField(fields, "PARRY_CHANCE", Acore::StringFormat("{:.2f}%",
            player->GetFloatValue(PLAYER_PARRY_PERCENTAGE)));
        PattrAddField(fields, "BLOCK_CHANCE", Acore::StringFormat("{:.2f}%",
            player->GetFloatValue(PLAYER_BLOCK_PERCENTAGE)));
        PattrAddField(fields, "MELEE_CRIT", Acore::StringFormat("{:.2f}%",
            player->GetFloatValue(PLAYER_CRIT_PERCENTAGE)));
        PattrAddField(fields, "RANGED_CRIT", Acore::StringFormat("{:.2f}%",
            player->GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE)));
        float spellCrit = player->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_HOLY);
        for (uint8 school = SPELL_SCHOOL_FIRE; school < MAX_SPELL_SCHOOL; ++school)
            spellCrit = std::min(spellCrit, player->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + school));
        PattrAddField(fields, "SPELL_CRIT", Acore::StringFormat("{:.2f}%", spellCrit));
        PattrAddField(fields, "RESILIENCE", AsPattrValue(std::min({
            PattrRealCombatRating(player, CR_CRIT_TAKEN_MELEE),
            PattrRealCombatRating(player, CR_CRIT_TAKEN_RANGED),
            PattrRealCombatRating(player, CR_CRIT_TAKEN_SPELL)})));
        PattrAddField(fields, "MANA_REGEN", AsPattrValue(PattrRealItemMod(player, ITEM_MOD_MANA_REGENERATION) +
            PattrFromFloat(player->GetFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER + POWER_MANA) * 5.0f)));
        PattrAddField(fields, "EXPERTISE", std::to_string(player->GetUInt32Value(PLAYER_EXPERTISE)) +
            " / " + std::to_string(player->GetUInt32Value(PLAYER_OFFHAND_EXPERTISE)));
    }

    PattrAddField(fields, "COMBAT_POWER", AsInt64ToString(CustomCombatPower::Calculate(player)));
    PattrAddField(fields, "EQUIPMENT_SCORE", std::to_string(CustomCombatPower::EquipmentScore(player)));

    PattrSendChunked(player, "STATS", fields);
}

void PattrSendTarget(Player* player, std::string const& token)
{
    Unit* target = player->GetSelectedUnit();
    if (!target)
    {
        std::vector<std::string> none;
        PattrAddField(none, "TOKEN", token);
        PattrAddField(none, "NONE", "1");
        PattrSendChunked(player, "TARGET", none);
        return;
    }

    Powers power = target->getPowerType();
    std::vector<std::string> fields;
    AsInt64 realMaxHealth = target->GetAsMaxHealth();
    if (AsInt64IsZero(realMaxHealth))
        realMaxHealth = AsInt64(target->GetMaxHealth());

    AsInt64 realCurHealth = target->GetAsHealth();
    if (AsInt64IsZero(realCurHealth))
        realCurHealth = AsScaleCurrent(target->GetHealth(), target->GetMaxHealth(), realMaxHealth);

    AsInt64 realMaxPower = target->GetAsMaxPower(power);
    if (AsInt64IsZero(realMaxPower))
        realMaxPower = AsInt64(target->GetMaxPower(power));

    AsInt64 realCurPower = target->GetAsPower(power);
    if (AsInt64IsZero(realCurPower))
        realCurPower = AsScaleCurrent(target->GetPower(power), target->GetMaxPower(power), realMaxPower);

    if (Player* targetPlayer = target->ToPlayer())
    {
        AsInt64 targetPlayerHealth = targetPlayer->GetAsRealMaxHealth();
        if (!AsInt64IsZero(targetPlayerHealth))
            realMaxHealth = targetPlayerHealth;

        AsInt64 targetPlayerPower = targetPlayer->GetAsRealMaxPower(power);
        if (!AsInt64IsZero(targetPlayerPower))
            realMaxPower = targetPlayerPower;
    }
    else if (Creature* targetCreature = target->ToCreature())
    {
        if (sHelper->GetBoolean(17000) && targetCreature->MaxMultipleHp > 1)
        {
            AsInt64 barMax = targetCreature->GetAsMaxHealth();
            if (barMax <= 0)
                barMax = AsInt64(targetCreature->GetMaxHealth());

            realMaxHealth = MultiplyDivideAsInt256(barMax, targetCreature->MaxMultipleHp, 1);
            realCurHealth = targetCreature->MultipleHp > 0
                ? (player->GetSession()->HasNativeUI() ? barMax : targetCreature->GetAsTotalHealth())
                : AsInt64Zero();
        }
        else
        {
            AsInt64 creatureHp = sAsCreature->GetAsHpNow(targetCreature);
            AsInt64 mapCreatureHp = sAsCreature->GetAsMazHpNow(targetCreature);
            if (!AsInt64IsZero(mapCreatureHp))
                realMaxHealth = mapCreatureHp;
            else if (!AsInt64IsZero(creatureHp))
                realMaxHealth = creatureHp;

            if (realCurHealth > realMaxHealth)
                realCurHealth = realMaxHealth;
        }
    }

    PattrAddField(fields, "TOKEN", token);
    PattrAddField(fields, "MAX_HEALTH", AsPattrValue(realMaxHealth));
    PattrAddField(fields, "CUR_HEALTH", AsPattrValue(realCurHealth));
    PattrAddField(fields, "MAX_MANA", AsPattrValue(realMaxPower));
    PattrAddField(fields, "CUR_MANA", AsPattrValue(realCurPower));
    PattrAddField(fields, "POWER_TYPE", std::to_string(uint32(power)));
    PattrSendChunked(player, "TARGET", fields);
}

AsInt64 PattrPetStat(Player* owner, Pet* pet, Stats stat)
{
    AsInt64 value = PattrFromFloat(pet->GetTotalStatValue(stat));
    if (owner)
        value += PattrScaleFloat(owner->GetAsRealStat(stat), sHelper->GetFloat(2010));
    return value > 0 ? value : AsInt64Zero();
}

std::pair<AsInt64, AsInt64> PattrPetDamageRange(Pet* pet)
{
    CreatureTemplate const* creatureTemplate = pet->GetCreatureTemplate();
    if (!creatureTemplate)
        return { AsInt64Zero(), AsInt64Zero() };

    AsInt64 attackPower = pet->GetAsTotalAttackPowerValue(BASE_ATTACK);
    AsInt64 baseValue = PattrFromFloat(pet->GetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, BASE_VALUE));
    baseValue += PattrScaleFloat(attackPower, creatureTemplate->BaseVariance / 14.0f);

    AsInt64 minWeapon = pet->AsWeaponDamageMin[BASE_ATTACK];
    AsInt64 maxWeapon = pet->AsWeaponDamageMax[BASE_ATTACK];
    CreatureBaseStats const* baseStats = CreatureBaseStats::GetBaseStats(pet->getLevel(), creatureTemplate->unit_class);
    AsInt64 baseStatsWeapon = baseStats ? baseStats->GenerateAsBaseDamage(creatureTemplate) : AsInt64Zero();
    if (minWeapon <= 0)
        minWeapon = baseStatsWeapon > 0 ? baseStatsWeapon : PattrFromFloat(pet->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE));
    if (maxWeapon <= 0)
        maxWeapon = baseStatsWeapon > 0 ? PattrScaleFloat(baseStatsWeapon, 1.5f) : PattrFromFloat(pet->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE));

    auto buildDamage = [&](AsInt64 value)
    {
        value += baseValue;
        if (creatureTemplate->AsDamageModifier > 0)
            value = ScaleAsInt64ByScaled(value, creatureTemplate->AsDamageModifier);
        else
            value = PattrScaleFloat(value, creatureTemplate->DamageModifier);
        value = PattrScaleFloat(value, pet->GetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, BASE_PCT) * pet->GetAPMultiplier(BASE_ATTACK, false));
        value += PattrFromFloat(pet->GetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_VALUE));
        return PattrScaleFloat(value, pet->GetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT));
    };

    AsInt64 minDamage = buildDamage(minWeapon);
    AsInt64 maxDamage = buildDamage(maxWeapon);
    if (pet->IsHunterPet())
    {
        if (pet->GetHappinessState() == HAPPY)
        {
            minDamage = PattrScaleFloat(minDamage, 1.25f);
            maxDamage = PattrScaleFloat(maxDamage, 1.25f);
        }
        else if (pet->GetHappinessState() == UNHAPPY)
        {
            minDamage = PattrScaleFloat(minDamage, 0.75f);
            maxDamage = PattrScaleFloat(maxDamage, 0.75f);
        }
    }
    if (maxDamage < minDamage)
        std::swap(minDamage, maxDamage);
    return { minDamage, maxDamage };
}

void PattrSendPet(Player* player)
{
    if (!player)
        return;

    Pet* pet = player->GetPet();
    if (!pet || !pet->IsInWorld())
    {
        std::vector<std::string> none;
        PattrAddField(none, "NONE", "1");
        PattrSendChunked(player, "PET", none);
        return;
    }

    Powers power = pet->getPowerType();
    AsInt64 maxHealth = pet->GetAsMaxHealth() > 0 ? pet->GetAsMaxHealth() : AsInt64(pet->GetMaxHealth());
    AsInt64 curHealth = pet->GetAsHealth() > 0 ? pet->GetAsHealth() : AsScaleCurrent(pet->GetHealth(), pet->GetMaxHealth(), maxHealth);
    AsInt64 maxPower = pet->GetAsMaxPower(power) > 0 ? pet->GetAsMaxPower(power) : AsInt64(pet->GetMaxPower(power));
    AsInt64 curPower = pet->GetAsPower(power) > 0 ? pet->GetAsPower(power) : AsScaleCurrent(pet->GetPower(power), pet->GetMaxPower(power), maxPower);

    AsInt64 agility = PattrPetStat(player, pet, STAT_AGILITY);
    AsInt64 armor = PattrFromFloat(pet->GetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE));
    armor = PattrScaleFloat(armor, pet->GetModifierValue(UNIT_MOD_ARMOR, BASE_PCT));
    AsInt64 createAgility = PattrFromFloat(pet->GetCreateStat(STAT_AGILITY));
    if (agility > createAgility)
        armor += (agility - createAgility) * 2;
    armor += PattrFromFloat(pet->GetModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE));
    armor = PattrScaleFloat(armor, pet->GetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT));

    AsInt64 attackPower = pet->GetAsTotalAttackPowerValue(BASE_ATTACK);
    auto damageRange = PattrPetDamageRange(pet);
    std::vector<std::string> fields;
    PattrAddField(fields, "GUID", pet->GetGUID().ToString());
    PattrAddField(fields, "MAX_HEALTH", AsPattrValue(maxHealth));
    PattrAddField(fields, "CUR_HEALTH", AsPattrValue(curHealth));
    PattrAddField(fields, "MAX_POWER", AsPattrValue(maxPower));
    PattrAddField(fields, "CUR_POWER", AsPattrValue(curPower));
    PattrAddField(fields, "POWER_TYPE", std::to_string(uint32(power)));
    PattrAddField(fields, "STRENGTH", AsPattrValue(PattrPetStat(player, pet, STAT_STRENGTH)));
    PattrAddField(fields, "AGILITY", AsPattrValue(agility));
    PattrAddField(fields, "STAMINA", AsPattrValue(PattrPetStat(player, pet, STAT_STAMINA)));
    PattrAddField(fields, "INTELLECT", AsPattrValue(PattrPetStat(player, pet, STAT_INTELLECT)));
    PattrAddField(fields, "SPIRIT", AsPattrValue(PattrPetStat(player, pet, STAT_SPIRIT)));
    PattrAddField(fields, "ARMOR", AsPattrValue(armor));
    PattrAddField(fields, "ATTACK_POWER", AsPattrValue(attackPower));
    PattrAddField(fields, "DAMAGE_MIN", AsPattrValue(damageRange.first));
    PattrAddField(fields, "DAMAGE_MAX", AsPattrValue(damageRange.second));
    PattrSendChunked(player, "PET", fields);
}

bool PattrExtractRequest(std::string const& msg, std::string& request)
{
    std::string prefix = std::string(PATTR_PREFIX) + "\t";
    if (msg.rfind(prefix, 0) == 0)
    {
        request = msg.substr(prefix.size());
        return true;
    }

    if (msg == "REQ_STATS" || msg == "REQ_TARGET" || msg == "REQ_PET" || msg == "LDT_READY" || msg.rfind("REQ_TARGET:", 0) == 0)
    {
        request = msg;
        return true;
    }

    return false;
}

bool PattrHandleRequest(Player* player, std::string const& msg)
{
    std::string request;
    if (!PattrExtractRequest(msg, request))
    {
        if (PATTR_DEBUG_LOG)
            PattrConsole("非PATTR请求 rawLen=" + std::to_string(msg.size()) + " raw=" + PattrShort(msg));
        return false;
    }

    if (PATTR_DEBUG_LOG)
    {
        PattrConsole("收到PATTR请求 player=" + (player ? player->GetName() : std::string("<null>")) +
            " request=" + request + " rawLen=" + std::to_string(msg.size()));
        LOG_INFO("server.worldserver", "[PATTR] recv player={} request={} rawLen={}",
            player ? player->GetName() : "<null>", request, msg.size());
    }

    if (request == "REQ_STATS")
    {
        PattrSendStats(player);
        return true;
    }

    if (request == "REQ_TARGET")
    {
        PattrSendTarget(player, "");
        return true;
    }

    if (request == "REQ_PET")
    {
        PattrSendPet(player);
        return true;
    }

    if (request == "LDT_READY")
    {
        PattrSendAddon(player, "LDT_ACK:1");
        if (PATTR_DEBUG_LOG)
            LOG_INFO("server.worldserver", "[PATTR] send player={} type=LDT_ACK enabled=1",
                player ? player->GetName() : "<null>");
        return true;
    }

    std::string debugPrefix = "DBG:";
    if (request.rfind(debugPrefix, 0) == 0)
    {
        PattrConsole("客户端阶段 player=" + (player ? player->GetName() : std::string("<null>")) +
            " stage=" + request.substr(debugPrefix.size()));
        return true;
    }

    std::string prefix = "REQ_TARGET:";
    if (request.rfind(prefix, 0) == 0)
    {
        PattrSendTarget(player, request.substr(prefix.size()));
        return true;
    }

    return false;
}

bool DispatchLegacyAddon(Player* player, uint32 type, std::string const& msg)
{
    if (!player || type == CHAT_MSG_WHISPER || msg.empty())
        return false;

    std::string request;
    if (PattrExtractRequest(msg, request))
        return false;

    size_t split = msg.find('\t');
    if (split == std::string::npos)
        return false;

    std::string header = msg.substr(0, split);
    std::string payload = msg.substr(split + 1);
    if (header.empty())
        return false;

    if (PATTR_DEBUG_LOG)
        PattrConsole("dispatch legacy addon player=" + player->GetName() + " header=" + header +
            " payloadLen=" + std::to_string(payload.size()));

    sDoAddon->OnAddonMessage(player, header.c_str(), payload.c_str());

    if (PATTR_DEBUG_LOG)
        PattrConsole("dispatch legacy addon done player=" + player->GetName() + " header=" + header);

    return true;
}

bool IsAioAddonMessage(std::string const& msg)
{
    return msg.rfind("CAIO\t", 0) == 0 || msg.rfind("SAIO\t", 0) == 0;
}
}

namespace
{
std::vector<std::string> NativeResourceFields(Unit* unit)
{
    AsInt64 health = unit->GetAsHealth();
    AsInt64 maximum = unit->GetAsMaxHealth();
    if (Player* player = unit->ToPlayer())
        if (player->GetAsRealMaxHealth() > 0)
            maximum = player->GetAsRealMaxHealth();
    if (maximum <= 0)
        maximum = unit->GetMaxHealth();
    if (health <= 0 && unit->IsAlive())
        health = unit->GetHealth();
    if (Creature* creature = unit->ToCreature())
    {
        if (sHelper->GetBoolean(17000) && creature->MaxMultipleHp > 1 && maximum > 0)
        {
            uint32 remaining = std::min(creature->MultipleHp, creature->MaxMultipleHp);
            AsInt64 barHealth = maximum;
            maximum = MultiplyDivideAsInt256(barHealth, creature->MaxMultipleHp, 1);
            boost::multiprecision::int512_t total = health;
            if (remaining > 1)
                total += boost::multiprecision::int512_t(barHealth) * (remaining - 1);
            health = !unit->IsAlive() || remaining == 0 ? AsInt64Zero() : ClampAsInt256(total);
        }
    }
    Powers power = unit->getPowerType();
    AsInt64 maxPower = unit->GetAsMaxPower(power);
    AsInt64 curPower = unit->GetAsPower(power);
    if (Player* player = unit->ToPlayer())
        if (player->GetAsRealMaxPower(power) > 0)
            maxPower = player->GetAsRealMaxPower(power);
    if (maxPower <= 0)
        maxPower = unit->GetMaxPower(power);
    return {
        "INT256=1",
        "CUR_HEALTH=" + AsInt64ToString(std::max<AsInt64>(0, health)),
        "MAX_HEALTH=" + AsInt64ToString(std::max<AsInt64>(0, maximum)),
        "CUR_POWER=" + AsInt64ToString(std::max<AsInt64>(0, curPower)),
        "MAX_POWER=" + AsInt64ToString(std::max<AsInt64>(0, maxPower)),
        "POWER_TYPE=" + std::to_string(uint32(power))
    };
}
}

void WorldSession::HandleNativeUIHello(WorldPacket& packet)
{
    if (packet.size() != sizeof(uint16))
        return;

    uint16 version;
    packet >> version;
    _nativeUIVersion = version == NativeUIProtocol::LegacyVersion || version == NativeUIProtocol::WideVersion ? version : 0;
    _nativeUIEnabled = _nativeUIVersion != 0;
    _nativeUITimer = 0;
    _nativeUIUnits.clear();
    for (std::string& snapshot : _nativeUISnapshots)
        snapshot.clear();
}

void WorldSession::HandleNativeUITooltipQuery(WorldPacket& packet)
{
    uint16 version = 0;
    uint32 itemId = 0;
    uint32 guidLow = 0;
    packet >> version >> itemId >> guidLow;
    if (packet.rpos() != packet.size() || version != NativeUIProtocol::TooltipProtocolVersion ||
        itemId == 0 || guidLow == 0 || !_nativeUIEnabled || !_player)
        return;

    Item* item = _player->GetItemByGuid(ObjectGuid(HighGuid::Item, guidLow));
    if (!item || item->GetEntry() != itemId)
        return;

    FrozenCoreSendItemModuleDataNative(_player, itemId, guidLow);
}

void WorldSession::SendNativeUITooltipData(uint32 itemId, uint32 guidLow, std::string const& payload)
{
    if (!_nativeUIEnabled || !_player || !itemId || !guidLow || payload.empty() ||
        payload.size() > NativeUIProtocol::MaxTooltipPayloadBytes)
        return;

    WorldPacket data(SMSG_NATIVE_UI_TOOLTIP_DATA, payload.size() + 12);
    data << NativeUIProtocol::TooltipProtocolVersion << itemId << guidLow << uint16(payload.size());
    data.append(payload.data(), payload.size());
    SendPacket(&data);
}

void WorldSession::UpdateNativeUI(uint32 diff)
{
    if (!_nativeUIEnabled || !_player || !_player->IsInWorld())
        return;
    if (_nativeUITimer > diff)
    {
        _nativeUITimer -= diff;
        return;
    }
    _nativeUITimer = 250;
    PattrSendStats(_player);
    PattrSendTarget(_player, "");
    PattrSendPet(_player);
    // Only objects already visible to this player may be included in the snapshot stream.
    for (ObjectGuid guid : _player->m_clientGUIDs)
        if (Unit* unit = ObjectAccessor::GetUnit(*_player, guid))
            SendNativeUISnapshot(3, guid, NativeResourceFields(unit));

    for (auto it = _nativeUIUnits.begin(); it != _nativeUIUnits.end();)
    {
        if (_player->HaveAtClient(ObjectGuid(it->first)))
            ++it;
        else
        {
            WorldPacket removed(SMSG_NATIVE_UI_SNAPSHOT, 13);
            removed << _nativeUIVersion << uint8(3) << it->first << uint16(0);
            SendPacket(&removed);
            it = _nativeUIUnits.erase(it);
        }
    }
}

// Called from HandleSetSelectionOpcode (MiscHandler.cpp). Kept as a free function
// using only WorldSession's public API so WorldSession.h, which most of the
// server includes, does not change.
void NativeUISendTargetNow(WorldSession* session)
{
    // UpdateNativeUI only refreshes every 250ms. On a fresh selection the client
    // would hold a blank health label until that tick, so push just the target
    // here. Everything else (stats, pet, other visible units) keeps its cadence.
    if (!session || !session->HasNativeUI())
        return;

    Player* player = session->GetPlayer();
    if (!player || !player->IsInWorld())
        return;

    // Sends kind 1, including the "no target" packet on deselect so the frame clears.
    PattrSendTarget(player, "");

    Unit* target = player->GetSelectedUnit();
    if (!target)
        return;

    // Mirrors the tick's invariant: only units already known to this client may
    // enter the kind 3 stream, so the client never receives an unknown GUID.
    if (!player->HaveAtClient(target->GetGUID()))
        return;

    // SendNativeUISnapshot drops payloads identical to the cached one, so the
    // following tick will not resend this and no extra traffic is produced.
    session->SendNativeUISnapshot(3, target->GetGUID(), NativeResourceFields(target));
}

void WorldSession::SendNativeUISnapshot(uint8 kind, ObjectGuid guid, std::vector<std::string> const& fields)
{
    if (!_nativeUIEnabled || !_player || kind > NativeUIProtocol::LastUnitKind || fields.size() > NativeUIProtocol::MaxFields)
        return;

    std::vector<std::string> values = fields;
    if (kind < 3 && guid)
    {
        if (Unit* unit = ObjectAccessor::GetUnit(*_player, guid))
        {
            auto resources = NativeResourceFields(unit);
            for (std::string const& resource : resources)
            {
                // Overwrite the entry in place so the display order stays the one
                // PattrSendStats produced (health/mana first), instead of pushing
                // the resource fields to the end of the snapshot.
                std::string key = resource.substr(0, resource.find('=') + 1);
                auto existing = std::find_if(values.begin(), values.end(), [&key](std::string const& value)
                {
                    return value.compare(0, key.size(), key) == 0;
                });
                if (existing != values.end())
                    *existing = resource;
                else
                    values.push_back(resource);
            }
        }
    }
    if (values.size() > NativeUIProtocol::MaxFields)
        return;

    WorldPacket packet(SMSG_NATIVE_UI_SNAPSHOT, 1024);
    packet << _nativeUIVersion << kind << guid.GetRawValue();
    size_t countOffset = packet.wpos();
    packet << uint16(0);
    uint16 count = 0;
    if (guid)
    {
        for (std::string const& field : values)
        {
            size_t separator = field.find('=');
            if (separator == std::string::npos || separator == 0 || separator > NativeUIProtocol::MaxKeyBytes)
                continue;
            std::string key = field.substr(0, separator);
            if (key == "TOKEN" || key == "NONE")
                continue;
            std::string value = field.substr(separator + 1);
            if (value.size() > NativeUIProtocol::MaxValueBytes)
                continue;
            packet << uint8(key.size());
            packet.append(key.data(), key.size());
            AsInt64 wideNumber;
            if (_nativeUIVersion >= NativeUIProtocol::WideVersion && TryParseAsInt256(value, wideNumber))
            {
                // v2 type 2: canonical signed decimal, up to 256 magnitude bits.
                value = AsInt64ToString(wideNumber);
                packet << uint8(2) << uint16(value.size());
                packet.append(value.data(), value.size());
            }
            else if (auto number = Acore::StringTo<int64>(value))
                packet << uint8(0) << *number;
            else
            {
                packet << uint8(1) << uint16(value.size());
                packet.append(value.data(), value.size());
            }
            ++count;
        }
    }
    packet.put<uint16>(countOffset, count);
    std::string payload(reinterpret_cast<char const*>(packet.contents()), packet.size());
    std::string& previous = kind == 3 ? _nativeUIUnits[guid.GetRawValue()] : _nativeUISnapshots[kind];
    if (previous == payload)
        return;
    previous = std::move(payload);
    SendPacket(&packet);
}

void WorldSession::HandleMessagechatOpcode(WorldPacket& recvData)
{
    uint32 type;
    uint32 lang;

    recvData >> type;
    recvData >> lang;

    if (type >= MAX_CHAT_MSG_TYPE)
    {
        if (PATTR_DEBUG_LOG && lang == LANG_ADDON)
            PattrConsole("收到无效插件聊天类型 type=" + std::to_string(type) + " lang=" + std::to_string(lang));
        LOG_ERROR("network.opcode", "CHAT: Wrong message type received: {}", type);
        recvData.rfinish();
        return;
    }

    /*if (lang == LANG_UNIVERSAL && type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
    {
        LOG_ERROR("entities.player.cheat", "CMSG_MESSAGECHAT: Possible hacking-attempt: {} tried to send a message in universal language", GetPlayerInfo());
        SendNotification(LANG_UNKNOWN_LANGUAGE);
        recvData.rfinish();
        return;
    }*/

    Player* sender = GetPlayer();

    if (PATTR_DEBUG_LOG && lang == LANG_ADDON)
    {
        PattrConsole("收到LANG_ADDON入口 player=" + (sender ? sender->GetName() : std::string("<null>")) +
            " type=" + std::to_string(type) + " lang=" + std::to_string(lang));
    }

    // prevent talking at unknown language (cheating)
    LanguageDesc const* langDesc = GetLanguageDescByID(lang);
    if (!langDesc)
    {
        if (PATTR_DEBUG_LOG && lang == LANG_ADDON)
            PattrConsole("LANG_ADDON语言描述无效，已拒绝 player=" + (sender ? sender->GetName() : std::string("<null>")));
        SendNotification(LANG_UNKNOWN_LANGUAGE);
        recvData.rfinish();
        return;
    }

    if (langDesc->skill_id != 0 && !sender->HasSkill(langDesc->skill_id))
    {
        // also check SPELL_AURA_COMPREHEND_LANGUAGE (client offers option to speak in that language)
        bool foundAura = false;
        for (auto const& auraEff : sender->GetAuraEffectsByType(SPELL_AURA_COMPREHEND_LANGUAGE))
        {
            if (auraEff->GetMiscValue() == int32(lang))
            {
                foundAura = true;
                break;
            }
        }

        if (!foundAura)
        {
            SendNotification(LANG_NOT_LEARNED_LANGUAGE);
            recvData.rfinish();
            return;
        }
    }

    // pussywizard: chatting on most chat types requires 2 hours played to prevent spam/abuse
    if (AccountMgr::IsPlayerAccount(GetSecurity()))
    {
        switch (type)
        {
            case CHAT_MSG_ADDON:
            case CHAT_MSG_PARTY:
            case CHAT_MSG_RAID:
            case CHAT_MSG_GUILD:
            case CHAT_MSG_OFFICER:
            case CHAT_MSG_AFK:
            case CHAT_MSG_DND:
            case CHAT_MSG_RAID_LEADER:
            case CHAT_MSG_RAID_WARNING:
            case CHAT_MSG_BATTLEGROUND:
            case CHAT_MSG_BATTLEGROUND_LEADER:
            case CHAT_MSG_PARTY_LEADER:
                break;
            default:
            {
                if (sWorld->getBoolConfig(CONFIG_CHAT_MUTE_FIRST_LOGIN))
                {
                    uint32 minutes = sWorld->getIntConfig(CONFIG_CHAT_TIME_MUTE_FIRST_LOGIN);

                    if (sender->GetTotalPlayedTime() < minutes * MINUTE)
                    {
                        SendNotification(LANG_MUTED_PLAYER, minutes);
                        recvData.rfinish();
                        return;
                    }
                }
            }
        }
    }

    // pussywizard:
    switch (type)
    {
        case CHAT_MSG_SAY:
        case CHAT_MSG_YELL:
        case CHAT_MSG_EMOTE:
        case CHAT_MSG_TEXT_EMOTE:
        case CHAT_MSG_AFK:
        case CHAT_MSG_DND:
        if (sender->IsSpectator())
        {
            recvData.rfinish();
            return;
        }
    }

    if (sender->HasAura(1852) && type != CHAT_MSG_WHISPER)
    {
        SendNotification(GetAcoreString(LANG_GM_SILENCE), sender->GetName().c_str());
        recvData.rfinish();
        return;
    }

    if (sender->InArenaWatchSolo)
    {
        recvData.rfinish();
        return;
    }

    if (lang == LANG_ADDON)
    {
        // LANG_ADDON is only valid for the following message types
        switch (type)
        {
            case CHAT_MSG_PARTY:
            case CHAT_MSG_RAID:
            case CHAT_MSG_GUILD:
            case CHAT_MSG_BATTLEGROUND:
            case CHAT_MSG_WHISPER:
                // check if addon messages are disabled
                if (!sWorld->getBoolConfig(CONFIG_ADDON_CHANNEL))
                {
                    if (PATTR_DEBUG_LOG)
                        PattrConsole("插件频道关闭 CONFIG_ADDON_CHANNEL=false，已丢弃 player=" +
                            (sender ? sender->GetName() : std::string("<null>")) + " type=" + std::to_string(type));
                    recvData.rfinish();
                    return;
                }
                break;
            default:
                if (PATTR_DEBUG_LOG)
                    PattrConsole("插件消息类型非法，已拒绝 player=" +
                        (sender ? sender->GetName() : std::string("<null>")) + " type=" + std::to_string(type));
                LOG_ERROR("network", "Player {} ({}) sent a chatmessage with an invalid language/message type combination",
                               GetPlayer()->GetName(), GetPlayer()->GetGUID().ToString());

                recvData.rfinish();
                return;
        }
    }
    // LANG_ADDON should not be changed nor be affected by flood control
    else
    {
        uint32 specialMessageLimit = 0;
        // send in universal language if player in .gmon mode (ignore spell effects)
        if (sender->IsGameMaster())
            lang = LANG_UNIVERSAL;
        else
        {
            // send in universal language in two side iteration allowed mode
            if (sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_CHAT))
                lang = LANG_UNIVERSAL;
            else
            {
                switch (type)
                {
                    case CHAT_MSG_PARTY:
                    case CHAT_MSG_PARTY_LEADER:
                    case CHAT_MSG_RAID:
                    case CHAT_MSG_RAID_LEADER:
                    case CHAT_MSG_RAID_WARNING:
                        // allow two side chat at group channel if two side group allowed
                        if (sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP))
                            lang = LANG_UNIVERSAL;

                        specialMessageLimit = 35;
                        break;
                    case CHAT_MSG_GUILD:
                    case CHAT_MSG_OFFICER:
                        // allow two side chat at guild channel if two side guild allowed
                        if (sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD))
                            lang = LANG_UNIVERSAL;

                        specialMessageLimit = 15;
                        break;
                    case CHAT_MSG_WHISPER:
                        if (sender->getLevel() >= 80)
                            specialMessageLimit = 15;
                        break;
                }
            }
            // but overwrite it by SPELL_AURA_MOD_LANGUAGE auras (only single case used)
            Unit::AuraEffectList const& ModLangAuras = sender->GetAuraEffectsByType(SPELL_AURA_MOD_LANGUAGE);
            if (!ModLangAuras.empty())
                lang = ModLangAuras.front()->GetMiscValue();
        }

        if (type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
            sender->UpdateSpeakTime(lang == LANG_ADDON ? Player::ChatFloodThrottle::ADDON
                                                       : Player::ChatFloodThrottle::REGULAR);
    }

    std::string to, channel, msg;
    bool ignoreChecks = false;
    switch (type)
    {
        case CHAT_MSG_SAY:
        case CHAT_MSG_EMOTE:
        case CHAT_MSG_YELL:
        case CHAT_MSG_PARTY:
        case CHAT_MSG_PARTY_LEADER:
        case CHAT_MSG_GUILD:
        case CHAT_MSG_OFFICER:
        case CHAT_MSG_RAID:
        case CHAT_MSG_RAID_LEADER:
        case CHAT_MSG_RAID_WARNING:
        case CHAT_MSG_BATTLEGROUND:
        case CHAT_MSG_BATTLEGROUND_LEADER:
            msg = recvData.ReadCString(lang != LANG_ADDON);
            break;
        case CHAT_MSG_WHISPER:
            recvData >> to;
            msg = recvData.ReadCString(lang != LANG_ADDON);

            if (PATTR_DEBUG_LOG && lang == LANG_ADDON)
            {
                PattrConsole("读取插件WHISPER player=" + (sender ? sender->GetName() : std::string("<null>")) +
                    " to=" + to + " rawLen=" + std::to_string(msg.size()) + " raw=" + PattrShort(msg));
            }

            if (lang == LANG_ADDON && PattrHandleRequest(sender, msg))
                return;

            to = sPlayerModMgr->GetPlayerNameOriginal(to);
            if (!sHelper->IsCheckString(to, "HandleMessagechatOpcode", this, recvData))
                return;

            break;
        case CHAT_MSG_CHANNEL:
            recvData >> channel;
            msg = recvData.ReadCString(lang != LANG_ADDON);
            break;
        case CHAT_MSG_AFK:
        case CHAT_MSG_DND:
            msg = recvData.ReadCString(lang != LANG_ADDON);
            ignoreChecks = true;
            break;
    }

    if (PATTR_DEBUG_LOG && lang == LANG_ADDON && type != CHAT_MSG_WHISPER)
    {
        PattrConsole("读取插件消息 player=" + (sender ? sender->GetName() : std::string("<null>")) +
            " type=" + std::to_string(type) + " rawLen=" + std::to_string(msg.size()) +
            " raw=" + PattrShort(msg));
    }

    // Our Warden module also uses SendAddonMessage as a way to communicate Lua check results to the server, see if this is that
    if (type == CHAT_MSG_GUILD && lang == LANG_ADDON && _warden && _warden->ProcessLuaCheckResponse(msg))
    {
        return;
    }

    if (lang == LANG_ADDON && DispatchLegacyAddon(sender, type, msg))
        return;

    if (type == CHAT_MSG_WHISPER && lang == LANG_ADDON && PattrHandleRequest(sender, msg))
        return;

    if (type == CHAT_MSG_WHISPER && lang == LANG_ADDON && IsAioAddonMessage(msg))
    {
        if (PATTR_DEBUG_LOG)
            PattrConsole("dispatch AIO addon player=" + (sender ? sender->GetName() : std::string("<null>")) +
                " rawLen=" + std::to_string(msg.size()));
        sScriptMgr->OnPlayerBeforeSendChatMessage(_player, type, lang, msg);
        if (PATTR_DEBUG_LOG)
            PattrConsole("dispatch AIO addon done player=" + (sender ? sender->GetName() : std::string("<null>")));
        return;
    }

    // pussywizard:
    if (msg.length() > 255 || (lang != LANG_ADDON && msg.find("|0") != std::string::npos))
        return;

    if (!ignoreChecks)
    {
        if (msg.empty())
            return;

        if (ChatHandler(this).ParseCommands(msg.c_str()))
            return;

        if (!_player->CanSpeak())
        {
            std::string timeStr = secsToTimeString(m_muteTime - GameTime::GetGameTime().count());
            SendNotification(GetAcoreString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
            return;
        }
    }

    // do message validity checks
    if (lang != LANG_ADDON)
    {
        // cut at the first newline or carriage return
        std::string::size_type pos = msg.find_first_of("\n\r");

        if (pos == 0)
        {
            return;
        }
        else if (pos != std::string::npos)
        {
            msg.erase(pos);
        }

        // abort on any sort of nasty character
        for (uint8 c : msg)
        {
            if (isNasty(c))
            {
                LOG_ERROR("network", "Player {} {} sent a message containing invalid character {} - blocked", GetPlayer()->GetName(),
                    GetPlayer()->GetGUID().ToString(), uint8(c));
                return;
            }
        }

        // collapse multiple spaces into one
        if (sWorld->getBoolConfig(CONFIG_CHAT_FAKE_MESSAGE_PREVENTING))
        {
            auto end = std::unique(msg.begin(), msg.end(), [](char c1, char c2) { return (c1 == ' ') && (c2 == ' '); });
            msg.erase(end, msg.end());
        }

        // Validate hyperlinks
        if (!ValidateHyperlinksAndMaybeKick(msg))
        {
            return;
        }
    }

    sScriptMgr->OnPlayerBeforeSendChatMessage(_player, type, lang, msg);

    switch (type)
    {
        case CHAT_MSG_SAY:
        case CHAT_MSG_EMOTE:
        case CHAT_MSG_YELL:
            {
                // Prevent cheating
                if (!sender->IsAlive())
                    return;

                if (sender->getLevel() < sWorld->getIntConfig(CONFIG_CHAT_SAY_LEVEL_REQ))
                {
                    SendNotification(GetAcoreString(LANG_SAY_REQ), sWorld->getIntConfig(CONFIG_CHAT_SAY_LEVEL_REQ));
                    return;
                }

                if (type == CHAT_MSG_SAY)
                    sender->Say(msg, Language(lang));
                else if (type == CHAT_MSG_EMOTE)
                    sender->TextEmote(msg);
                else if (type == CHAT_MSG_YELL)
                    sender->Yell(msg, Language(lang));
            }
            break;
        case CHAT_MSG_WHISPER:
            {
                if (sender->getLevel() < sWorld->getIntConfig(CONFIG_CHAT_WHISPER_LEVEL_REQ))
                {
                    SendNotification(GetAcoreString(LANG_WHISPER_REQ), sWorld->getIntConfig(CONFIG_CHAT_WHISPER_LEVEL_REQ));
                    return;
                }

                if (!normalizePlayerName(to))
                {
                    SendPlayerNotFoundNotice(to);
                    break;
                }

                Player* receiver = ObjectAccessor::FindPlayerByName(to, false);
                bool senderIsPlayer = AccountMgr::IsPlayerAccount(GetSecurity());
                bool receiverIsPlayer = AccountMgr::IsPlayerAccount(receiver ? receiver->GetSession()->GetSecurity() : SEC_PLAYER);
                if (!receiver || (senderIsPlayer && !receiverIsPlayer && !receiver->isAcceptWhispers() && !receiver->IsInWhisperWhiteList(sender->GetGUID())))
                {
                    SendPlayerNotFoundNotice(to);
                    return;
                }

                if (!sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_CHAT) && senderIsPlayer && receiverIsPlayer)
                    if (GetPlayer()->GetTeamId() != receiver->GetTeamId())
                    {
                        SendWrongFactionNotice();
                        return;
                    }

                // pussywizard: optimization
                if (GetPlayer()->HasAura(1852) && !receiver->IsGameMaster())
                {
                    SendNotification(GetAcoreString(LANG_GM_SILENCE), GetPlayer()->GetName().c_str());
                    return;
                }

                // If player is a Gamemaster and doesn't accept whisper, we auto-whitelist every player that the Gamemaster is talking to
                if (!senderIsPlayer && !sender->isAcceptWhispers() && !sender->IsInWhisperWhiteList(receiver->GetGUID()))
                    sender->AddWhisperWhiteList(receiver->GetGUID());

                GetPlayer()->Whisper(msg, Language(lang), receiver);
            }
            break;
        case CHAT_MSG_PARTY:
        case CHAT_MSG_PARTY_LEADER:
            {
                // if player is in battleground, he cannot say to battleground members by /p
                Group* group = GetPlayer()->GetOriginalGroup();
                if (!group)
                {
                    group = sender->GetGroup();
                    if (!group || group->isBGGroup())
                        return;
                }

                if (type == CHAT_MSG_PARTY_LEADER && !group->IsLeader(sender->GetGUID()))
                    return;

                if (!sScriptMgr->CanPlayerUseChat(GetPlayer(), type, lang, msg, group))
                {
                    return;
                }

                sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, group);

                WorldPacket data;
                ChatHandler::BuildChatPacket(data, ChatMsg(type), Language(lang), sender, nullptr, msg);
                group->BroadcastPacket(&data, false, group->GetMemberGroup(GetPlayer()->GetGUID()));
            }
            break;
        case CHAT_MSG_GUILD:
            {
                if (GetPlayer()->GetGuildId())
                {
                    if (Guild* guild = sGuildMgr->GetGuildById(GetPlayer()->GetGuildId()))
                    {
                        if (!sScriptMgr->CanPlayerUseChat(GetPlayer(), type, lang, msg, guild))
                        {
                            return;
                        }

                        sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, guild);

                        guild->BroadcastToGuild(this, false, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                    }
                }
            }
            break;
        case CHAT_MSG_OFFICER:
            {
                if (GetPlayer()->GetGuildId())
                {
                    if (Guild* guild = sGuildMgr->GetGuildById(GetPlayer()->GetGuildId()))
                    {
                        if (!sScriptMgr->CanPlayerUseChat(GetPlayer(), type, lang, msg, guild))
                        {
                            return;
                        }

                        sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, guild);

                        guild->BroadcastToGuild(this, true, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                    }
                }
            }
            break;
        case CHAT_MSG_RAID:
            {
                // if player is in battleground, he cannot say to battleground members by /ra
                Group* group = GetPlayer()->GetOriginalGroup();
                if (!group)
                {
                    group = GetPlayer()->GetGroup();
                    if (!group || group->isBGGroup() || !group->isRaidGroup())
                        return;
                }

                if (!sScriptMgr->CanPlayerUseChat(GetPlayer(), type, lang, msg, group))
                {
                    return;
                }

                sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, group);

                WorldPacket data;
                ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID, Language(lang), sender, nullptr, msg);
                group->BroadcastPacket(&data, false);
            }
            break;
        case CHAT_MSG_RAID_LEADER:
            {
                // if player is in battleground, he cannot say to battleground members by /ra
                Group* group = GetPlayer()->GetOriginalGroup();
                if (!group)
                {
                    group = GetPlayer()->GetGroup();
                    if (!group || group->isBGGroup() || !group->isRaidGroup() || !group->IsLeader(sender->GetGUID()))
                        return;
                }

                if (!sScriptMgr->CanPlayerUseChat(GetPlayer(), type, lang, msg, group))
                {
                    return;
                }

                sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, group);

                WorldPacket data;
                ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_LEADER, Language(lang), sender, nullptr, msg);
                group->BroadcastPacket(&data, false);
            }
            break;
        case CHAT_MSG_RAID_WARNING:
            {
                Group* group = GetPlayer()->GetGroup();
                if (!group || !group->isRaidGroup() || !(group->IsLeader(GetPlayer()->GetGUID()) || group->IsAssistant(GetPlayer()->GetGUID())) || group->isBGGroup())
                    return;

                if (!sScriptMgr->CanPlayerUseChat(GetPlayer(), type, lang, msg, group))
                {
                    return;
                }

                sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, group);

                // In battleground, raid warning is sent only to players in battleground - code is ok
                WorldPacket data;
                ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_WARNING, Language(lang), sender, nullptr, msg);
                group->BroadcastPacket(&data, false);
            }
            break;
        case CHAT_MSG_BATTLEGROUND:
            {
                //battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
                Group* group = GetPlayer()->GetGroup();
                if (!group || !group->isBGGroup())
                    return;

                if (!sScriptMgr->CanPlayerUseChat(GetPlayer(), type, lang, msg, group))
                {
                    return;
                }

                sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, group);

                WorldPacket data;
                ChatHandler::BuildChatPacket(data, CHAT_MSG_BATTLEGROUND, Language(lang), sender, nullptr, msg);
                group->BroadcastPacket(&data, false);
            }
            break;
        case CHAT_MSG_BATTLEGROUND_LEADER:
            {
                // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
                Group* group = GetPlayer()->GetGroup();
                if (!group || !group->isBGGroup() || !group->IsLeader(GetPlayer()->GetGUID()))
                    return;

                if (!sScriptMgr->CanPlayerUseChat(GetPlayer(), type, lang, msg, group))
                {
                    return;
                }

                sScriptMgr->OnPlayerChat(GetPlayer(), type, lang, msg, group);

                WorldPacket data;
                ChatHandler::BuildChatPacket(data, CHAT_MSG_BATTLEGROUND_LEADER, Language(lang), sender, nullptr, msg);
                group->BroadcastPacket(&data, false);
            }
            break;
        case CHAT_MSG_CHANNEL:
            {
                if (AccountMgr::IsPlayerAccount(GetSecurity()))
                {
                    if (sender->getLevel() < sWorld->getIntConfig(CONFIG_CHAT_CHANNEL_LEVEL_REQ))
                    {
                        SendNotification(GetAcoreString(LANG_CHANNEL_REQ), sWorld->getIntConfig(CONFIG_CHAT_CHANNEL_LEVEL_REQ));
                        return;
                    }
                }

                if (ChannelMgr* cMgr = ChannelMgr::forTeam(sender->GetTeamId()))
                {
                    if (Channel* chn = cMgr->GetChannel(channel, sender))
                    {
                        if (!sScriptMgr->CanPlayerUseChat(sender, type, lang, msg, chn))
                        {
                            return;
                        }

                        sScriptMgr->OnPlayerChat(sender, type, lang, msg, chn);

                        chn->Say(sender->GetGUID(), msg.c_str(), lang);
                    }
                }
            }
            break;
        case CHAT_MSG_AFK:
            {
                if (!sender->IsInCombat())
                {
                    if (sender->isAFK())                       // Already AFK
                    {
                        if (msg.empty())
                            sender->ToggleAFK();               // Remove AFK
                        else
                            sender->autoReplyMsg = msg;        // Update message
                    }
                    else                                        // New AFK mode
                    {
                        sender->autoReplyMsg = msg.empty() ? GetAcoreString(LANG_PLAYER_AFK_DEFAULT) : msg;

                        if (sender->isDND())
                            sender->ToggleDND();

                        sender->ToggleAFK();
                    }

                    if (!sScriptMgr->CanPlayerUseChat(sender, type, lang, msg))
                    {
                        return;
                    }

                    sScriptMgr->OnPlayerChat(sender, type, lang, msg);
                }
                break;
            }
        case CHAT_MSG_DND:
            {
                if (sender->isDND())                           // Already DND
                {
                    if (msg.empty())
                        sender->ToggleDND();                   // Remove DND
                    else
                        sender->autoReplyMsg = msg;            // Update message
                }
                else                                            // New DND mode
                {
                    sender->autoReplyMsg = msg.empty() ? GetAcoreString(LANG_PLAYER_DND_DEFAULT) : msg;

                    if (sender->isAFK())
                        sender->ToggleAFK();

                    sender->ToggleDND();
                }

                if (!sScriptMgr->CanPlayerUseChat(sender, type, lang, msg))
                {
                    return;
                }

                sScriptMgr->OnPlayerChat(sender, type, lang, msg);

                break;
            }
        default:
            LOG_ERROR("network.opcode", "CHAT: unknown message type {}, lang: {}", type, lang);
            break;
    }
}

void WorldSession::HandleEmoteOpcode(WorldPackets::Chat::EmoteClient& packet)
{
    if (GetPlayer()->IsSpectator())
        return;

    uint32 emoteId = packet.EmoteID;

    // restrict to the only emotes hardcoded in client
    if (emoteId != EMOTE_ONESHOT_NONE && emoteId != EMOTE_ONESHOT_WAVE)
        return;

    if (!_player->IsAlive() || _player->HasUnitState(UNIT_STATE_DIED))
        return;

    sScriptMgr->OnPlayerEmote(_player, emoteId);
    _player->HandleEmoteCommand(emoteId);
}

namespace Acore
{
    class EmoteChatBuilder
    {
    public:
        EmoteChatBuilder(Player const& player, uint32 text_emote, uint32 emote_num, Unit const* target)
            : i_player(player), i_text_emote(text_emote), i_emote_num(emote_num), i_target(target) {}

        void operator()(WorldPacket& data, LocaleConstant loc_idx)
        {
            std::string const name(i_target ? i_target->GetNameForLocaleIdx(loc_idx) : "");
            uint32 namlen = name.size();

            data.Initialize(SMSG_TEXT_EMOTE, 20 + namlen);
            data << i_player.GetGUID();
            data << uint32(i_text_emote);
            data << uint32(i_emote_num);
            data << uint32(namlen);
            if (namlen > 1)
                data << name;
            else
                data << uint8(0x00);
        }

    private:
        Player const& i_player;
        uint32        i_text_emote;
        uint32        i_emote_num;
        Unit const*   i_target;
    };
}                                                           // namespace Acore

void WorldSession::HandleTextEmoteOpcode(WorldPacket& recvData)
{
    if (!GetPlayer()->IsAlive())
        return;

    GetPlayer()->UpdateSpeakTime(Player::ChatFloodThrottle::REGULAR);

    if (!GetPlayer()->CanSpeak())
    {
        std::string timeStr = secsToTimeString(m_muteTime - GameTime::GetGameTime().count());
        SendNotification(GetAcoreString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
        return;
    }

    if (GetPlayer()->IsSpectator())
        return;

    uint32 text_emote, emoteNum;
    ObjectGuid guid;

    recvData >> text_emote;
    recvData >> emoteNum;
    recvData >> guid;

    sScriptMgr->OnPlayerTextEmote(GetPlayer(), text_emote, emoteNum, guid);

    EmotesTextEntry const* em = sEmotesTextStore.LookupEntry(text_emote);
    if (!em)
        return;

    uint32 emote_anim = em->textid;

    switch (emote_anim)
    {
        case EMOTE_STATE_SLEEP:
        case EMOTE_STATE_SIT:
        case EMOTE_STATE_KNEEL:
        case EMOTE_ONESHOT_NONE:
            break;
        default:
            // Only allow text-emotes for "dead" entities (feign death included)
            if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
                break;
            GetPlayer()->HandleEmoteCommand(emote_anim);
            break;
    }

    Unit* unit = ObjectAccessor::GetUnit(*_player, guid);

    Acore::EmoteChatBuilder emote_builder(*GetPlayer(), text_emote, emoteNum, unit);
    Acore::LocalizedPacketDo<Acore::EmoteChatBuilder > emote_do(emote_builder);
    Acore::PlayerDistWorker<Acore::LocalizedPacketDo<Acore::EmoteChatBuilder > > emote_worker(GetPlayer(), sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_TEXTEMOTE), emote_do);
    Cell::VisitObjects(GetPlayer(), emote_worker, sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_TEXTEMOTE));

    GetPlayer()->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DO_EMOTE, text_emote, 0, unit);

    //Send scripted event call
    if (unit && unit->GetTypeId() == TYPEID_UNIT && ((Creature*)unit)->AI())
        ((Creature*)unit)->AI()->ReceiveEmote(GetPlayer(), text_emote);
}

void WorldSession::HandleChatIgnoredOpcode(WorldPacket& recvData)
{
    ObjectGuid iguid;
    uint8 unk;

    recvData >> iguid;
    recvData >> unk;                                       // probably related to spam reporting

    Player* player = ObjectAccessor::FindConnectedPlayer(iguid);
    if (!player)
        return;

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_IGNORED, LANG_UNIVERSAL, _player, _player, GetPlayer()->GetName());
    player->GetSession()->SendPacket(&data);
}

void WorldSession::HandleChannelDeclineInvite(WorldPacket& recvPacket)
{
    // used only with EXTRA_LOGS
    (void)recvPacket;

    LOG_DEBUG("network", "Opcode {}", recvPacket.GetOpcode());
}

void WorldSession::SendPlayerNotFoundNotice(std::string const& name)
{
    WorldPacket data(SMSG_CHAT_PLAYER_NOT_FOUND, name.size() + 1);
    data << name;
    SendPacket(&data);
}

void WorldSession::SendPlayerAmbiguousNotice(std::string const& name)
{
    WorldPacket data(SMSG_CHAT_PLAYER_AMBIGUOUS, name.size() + 1);
    data << name;
    SendPacket(&data);
}

void WorldSession::SendWrongFactionNotice()
{
    WorldPacket data(SMSG_CHAT_WRONG_FACTION, 0);
    SendPacket(&data);
}

void WorldSession::SendChatRestrictedNotice(ChatRestrictionType restriction)
{
    WorldPacket data(SMSG_CHAT_RESTRICTED, 1);
    data << uint8(restriction);
    SendPacket(&data);
}
