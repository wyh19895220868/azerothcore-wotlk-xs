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

#include "ScriptMgr.h"
#include "ScriptMgrMacros.h"
#include "UnitScript.h"

uint32 ScriptMgr::DealDamage(Unit* AttackerUnit, Unit* pVictim, uint32 damage, DamageEffectType damagetype)
{
    if (ScriptRegistry<UnitScript>::ScriptPointerList.empty())
    {
        return damage;
    }

    for (auto const& [scriptID, script] : ScriptRegistry<UnitScript>::ScriptPointerList)
    {
        auto const& dmg = script->DealDamage(AttackerUnit, pVictim, damage, damagetype);
        if (dmg != damage)
        {
            return damage;
        }
    }

    return damage;
}

void ScriptMgr::OnHeal(Unit* healer, Unit* reciever, uint32& gain)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnHeal(healer, reciever, gain);
        });
}

void ScriptMgr::OnDamage(Unit* attacker, Unit* victim, uint32& damage)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnDamage(attacker, victim, damage);
        });
}

void ScriptMgr::ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->ModifyPeriodicDamageAurasTick(target, attacker, damage);
        });
}

void ScriptMgr::ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->ModifyMeleeDamage(target, attacker, damage);
        });
}

void ScriptMgr::ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->ModifySpellDamageTaken(target, attacker, damage, spellInfo);
        });
}

void ScriptMgr::ModifyHealRecieved(Unit* attacker, Unit* target, uint32 CastId, uint32& damage)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->ModifyHealRecieved(attacker, target, CastId, damage);
        });
}

void ScriptMgr::OnBeforeRollMeleeOutcomeAgainst(Unit const* attacker, Unit const* victim, WeaponAttackType attType, int32& attackerMaxSkillValueForLevel, int32& victimMaxSkillValueForLevel, int32& attackerWeaponSkill, int32& victimDefenseSkill, int32& crit_chance, int32& miss_chance, int32& dodge_chance, int32& parry_chance, int32& block_chance)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnBeforeRollMeleeOutcomeAgainst(attacker, victim, attType, attackerMaxSkillValueForLevel, victimMaxSkillValueForLevel, attackerWeaponSkill, victimDefenseSkill, crit_chance, miss_chance, dodge_chance, parry_chance, block_chance);
        });
}

void ScriptMgr::OnAuraRemove(Unit* unit, AuraApplication* aurApp, AuraRemoveMode mode)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnAuraRemove(unit, aurApp, mode);
        });
}

bool ScriptMgr::IfNormalReaction(Unit const* unit, Unit const* target, ReputationRank& repRank)
{
    auto ret = IsValidBoolScript<UnitScript>([&](UnitScript* script)
        {
            return !script->IfNormalReaction(unit, target, repRank);
        });

    if (ret && *ret)
    {
        return false;
    }

    return true;
}

bool ScriptMgr::IsNeedModSpellDamagePercent(Unit const* unit, AuraEffect* auraEff, float& doneTotalMod, SpellInfo const* spellProto)
{
    auto ret = IsValidBoolScript<UnitScript>([&](UnitScript* script)
        {
            return !script->IsNeedModSpellDamagePercent(unit, auraEff, doneTotalMod, spellProto);
        });

    if (ret && *ret)
    {
        return false;
    }

    return true;
}

bool ScriptMgr::IsNeedModMeleeDamagePercent(Unit const* unit, AuraEffect* auraEff, float& doneTotalMod, SpellInfo const* spellProto)
{
    auto ret = IsValidBoolScript<UnitScript>([&](UnitScript* script)
        {
            return !script->IsNeedModMeleeDamagePercent(unit, auraEff, doneTotalMod, spellProto);
        });

    if (ret && *ret)
    {
        return false;
    }

    return true;
}

bool ScriptMgr::IsNeedModHealPercent(Unit const* unit, AuraEffect* auraEff, float& doneTotalMod, SpellInfo const* spellProto)
{
    auto ret = IsValidBoolScript<UnitScript>([&](UnitScript* script)
        {
            return !script->IsNeedModHealPercent(unit, auraEff, doneTotalMod, spellProto);
        });

    if (ret && *ret)
    {
        return false;
    }

    return true;
}

bool ScriptMgr::CanSetPhaseMask(Unit const* unit, uint32 newPhaseMask, bool update)
{
    auto ret = IsValidBoolScript<UnitScript>([&](UnitScript* script)
        {
            return !script->CanSetPhaseMask(unit, newPhaseMask, update);
        });

    if (ret && *ret)
    {
        return false;
    }

    return true;
}

bool ScriptMgr::IsCustomBuildValuesUpdate(Unit const* unit, uint8 updateType, ByteBuffer& fieldBuffer, Player const* target, uint16 index)
{
    auto ret = IsValidBoolScript<UnitScript>([&](UnitScript* script)
        {
            return script->IsCustomBuildValuesUpdate(unit, updateType, fieldBuffer, target, index);
        });

    if (ret && *ret)
    {
        return true;
    }

    return false;
}

bool ScriptMgr::OnBuildValuesUpdate(Unit const* unit, uint8 updateType, ByteBuffer& fieldBuffer, Player* target, uint16 index)
{
    auto ret = IsValidBoolScript<UnitScript>([&](UnitScript* script) { return script->OnBuildValuesUpdate(unit, updateType, fieldBuffer, target, index); });

    if (ret && *ret)
    {
        return true;
    }

    return false;
}

void ScriptMgr::OnUnitUpdate(Unit* unit, uint32 diff)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnUnitUpdate(unit, diff);
        });
}

void ScriptMgr::OnDisplayIdChange(Unit* unit, uint32 displayId)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnDisplayIdChange(unit, displayId);
        });
}

void ScriptMgr::OnUnitEnterEvadeMode(Unit* unit, uint8 evadeReason)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnUnitEnterEvadeMode(unit, evadeReason);
        });
}

void ScriptMgr::OnDamageWpMinMax(Unit* attacker, float& MinDmg, float& MaxDmg)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnDamageWpMinMax(attacker, MinDmg, MaxDmg);
        });
}

void ScriptMgr::OnDamageMeleeNow(Unit* attacker, Unit* victim, uint32& damage)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnDamageMeleeNow(attacker, victim, damage);
        });
}

void ScriptMgr::OnDamageMeleeCritNow(Unit* attacker, Unit* victim, uint32& damage)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnDamageMeleeCritNow(attacker, victim, damage);
        });
}

void ScriptMgr::OnDamageMeleeFinal(Unit* attacker, Unit* victim, AsInt64 const& damage, bool critical)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnDamageMeleeFinal(attacker, victim, damage, critical);
        });
}

void ScriptMgr::OnDamageSpellEnd(Unit* attacker, Unit* victim, uint32 CastId, uint32& damage)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnDamageSpellEnd(attacker, victim, CastId, damage);
        });
}

void ScriptMgr::OnDamageSpellCritEnd(Unit const* attacker, Unit const* victim, uint32 CastId, int32& damage)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnDamageSpellCritEnd(attacker, victim, CastId, damage);
        });
}

void ScriptMgr::OnDamageLuaMeleeNow(Unit* attacker, Unit* victim, uint32& damage)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnDamageLuaMeleeNow(attacker, victim, damage);
        });
}

void ScriptMgr::OnDamageLuaSpellEnd(Unit* attacker, Unit* victim, uint32 CastId, uint32& damage)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnDamageLuaSpellEnd(attacker, victim, CastId, damage);
        });
}

void ScriptMgr::OnDamageSpellFinal(Unit* attacker, Unit* victim, uint32 CastId, AsInt64 const& damage, bool critical)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnDamageSpellFinal(attacker, victim, CastId, damage, critical);
        });
}

void ScriptMgr::OnDamageSpellBefore(Unit* attacker, Unit* victim, uint32 CastId, int32& damage)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnDamageSpellBefore(attacker, victim, CastId, damage);
        });
}

void ScriptMgr::OnCreKillToCreOrDie(Unit* Killer, Unit* Killed)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnCreKillToCreOrDie(Killer, Killed);
        });
}

void ScriptMgr::OnScriptEffectAbsorbEnd(Unit* Caster, Unit* UnitOwner, AuraEffect const* AEffect, int32& Amount)
{
    ExecuteScript<UnitScript>([&](UnitScript* script)
        {
            script->OnScriptEffectAbsorbEnd(Caster, UnitOwner, AEffect, Amount);
        });
}

UnitScript::UnitScript(char const* name, bool addToScripts, std::vector<uint16> enabledHooks)
    : ScriptObject(name, UNITHOOK_END)
{
    if (addToScripts)
    {
        if (enabledHooks.empty())
            for (uint16 i = 0; i < UNITHOOK_END; ++i)
                enabledHooks.emplace_back(i);

        ScriptRegistry<UnitScript>::AddScript(this, std::move(enabledHooks));
    }
}

template class AC_GAME_API ScriptRegistry<UnitScript>;
