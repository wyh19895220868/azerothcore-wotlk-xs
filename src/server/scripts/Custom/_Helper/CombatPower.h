#ifndef CUSTOM_COMBAT_POWER_H
#define CUSTOM_COMBAT_POWER_H

#include "Item.h"
#include "ItemTemplate.h"
#include "Player.h"

#include <algorithm>

namespace CustomCombatPower
{
inline AsInt64 Positive(AsInt64 const& value)
{
    return value > 0 ? value : AsInt64Zero();
}

inline AsInt64 RealCombatRating(Player* player, CombatRating rating)
{
    if (!player)
        return AsInt64Zero();

    AsInt64 compatValue = player->GetUInt32Value(uint16(PLAYER_FIELD_COMBAT_RATING_1) + uint16(rating));
    return Positive(compatValue + player->GetAsCombatRatingBonus(rating) - player->GetAsCombatRatingCompatBonus(rating));
}

inline AsInt64 RealArmor(Player* player)
{
    if (!player)
        return AsInt64Zero();

    AsInt64 armor = player->GetArmor();
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item const* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item || item->IsBroken())
            continue;

        ItemTemplate const* itemTemplate = item->GetTemplate();
        if (itemTemplate && itemTemplate->AsArmor > itemTemplate->Armor)
            armor += itemTemplate->AsArmor - itemTemplate->Armor;
    }

    return Positive(armor);
}

inline uint64 EquipmentScore(Player* player)
{
    if (!player)
        return 0;

    uint64 score = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_BODY || slot == EQUIPMENT_SLOT_TABARD)
            continue;

        Item const* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        ItemTemplate const* itemTemplate = item ? item->GetTemplate() : nullptr;
        if (itemTemplate)
            score += itemTemplate->ItemLevel;
    }

    return score;
}

inline AsInt64 Calculate(Player* player)
{
    if (!player)
        return AsInt64Zero();

    AsInt64 maxHealth = Positive(player->GetAsRealMaxHealth());
    if (AsInt64IsZero(maxHealth))
        maxHealth = player->GetMaxHealth();

    AsInt64 maxMana = Positive(player->GetAsRealMaxPower(POWER_MANA));
    if (AsInt64IsZero(maxMana))
        maxMana = player->GetMaxPower(POWER_MANA);

    AsInt64 primaryStats = 0;
    for (uint8 stat = STAT_STRENGTH; stat < MAX_STATS; ++stat)
        primaryStats += Positive(player->GetAsRealStat(Stats(stat)));

    AsInt64 meleePower = Positive(player->GetAsTotalAttackPowerValue(BASE_ATTACK));
    AsInt64 rangedPower = Positive(player->GetAsTotalAttackPowerValue(RANGED_ATTACK));
    AsInt64 spellPower = 0;
    for (uint8 school = SPELL_SCHOOL_HOLY; school < MAX_SPELL_SCHOOL; ++school)
        spellPower = std::max(spellPower, Positive(player->GetAsSpellPowerValue(SpellSchoolMask(1 << school))));
    AsInt64 offensivePower = std::max(meleePower, std::max(rangedPower, spellPower));

    AsInt64 hitRating = std::max(RealCombatRating(player, CR_HIT_MELEE),
        std::max(RealCombatRating(player, CR_HIT_RANGED), RealCombatRating(player, CR_HIT_SPELL)));
    AsInt64 critRating = std::max(RealCombatRating(player, CR_CRIT_MELEE),
        std::max(RealCombatRating(player, CR_CRIT_RANGED), RealCombatRating(player, CR_CRIT_SPELL)));
    AsInt64 hasteRating = std::max(RealCombatRating(player, CR_HASTE_MELEE),
        std::max(RealCombatRating(player, CR_HASTE_RANGED), RealCombatRating(player, CR_HASTE_SPELL)));

    AsInt64 ratings = hitRating + critRating + hasteRating;
    ratings += RealCombatRating(player, CR_DEFENSE_SKILL);
    ratings += RealCombatRating(player, CR_DODGE);
    ratings += RealCombatRating(player, CR_PARRY);
    ratings += RealCombatRating(player, CR_BLOCK);
    ratings += RealCombatRating(player, CR_CRIT_TAKEN_MELEE);
    ratings += RealCombatRating(player, CR_EXPERTISE);
    ratings += RealCombatRating(player, CR_ARMOR_PENETRATION);

    AsInt64 combatPower = maxHealth / 10;
    combatPower += maxMana / 20;
    combatPower += primaryStats * 2;
    combatPower += RealArmor(player) / 5;
    combatPower += offensivePower * 3;
    combatPower += ratings * 4;
    combatPower += AsInt64(EquipmentScore(player)) * 10;

    return combatPower > 0 ? combatPower : AsInt64(1);
}
}

#endif
