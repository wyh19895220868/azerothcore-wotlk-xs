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
#include "SpellAuraEffects.h"
#include "Item.h"
#include "Bag.h"
#include "Pet.h"
#include <CharacterCache.h>
#include <algorithm>
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include "AuraSpellMgr.h"
#include <Grids/Notifiers/GridNotifiers.h>
#include "CellImpl.h"
#include <Custom/ClassConfig/ClassMod.h>      // sCustomClass
#include <Custom/CreatureMod/AsCreature.h>    // sAsCreature
#pragma execution_character_set("utf-8")

OnAsMap OnAsStore;

OnAyThrough m_OnAyThrough;

SpellModTemplateMap SpellModMap;
SpellModBaseTemplateMap SpellModBaseMap;
AuraModStatTemplateMap AuraModStatMap;
AuraPctTempMap AuraPctMap;
AuraTriggerSpellTemplateMap AuraTriggerSpellMap;
AuraStackTriggerTemplateMap AuraStackTriggerMap;
AuraTriggerOnDeathTemplateMap AuraTriggerOnDeathMap;
TSpellCusTargetMaskMap SpellCusTargetMaskMap;
SpellCastMap SpellCastStore;
BusAuraNMap BusAuraStore;
SpellAbsorbMap SpellAbsorbStore;
BusXsAuraMap BusXsAuraStore;
LinkAuraStackTempMap LinkAuraStackMap;

namespace
{
uint64 ProcHealDisplaySeq = 0;

void SendProcHealDisplay(Player* player, AsInt64 const& amount, SpellInfo const* spellInfo)
{
    if (!player || amount <= 0 || !spellInfo)
        return;

    std::ostringstream msg;
    msg << "HEAL:"
        << AsInt64ToString(amount) << ':'
        << uint32(spellInfo->GetSchoolMask()) << ':'
        << spellInfo->Id << ':'
        << ++ProcHealDisplaySeq;
    sDoAddon->SendPacketToClient(player, "PATTRPANEL", msg.str());
}

AsInt64 ScaleAuraAsDamage(AsInt64 const& damage, float modifier)
{
    if (damage <= 0 || modifier == 0.0f)
        return AsInt64Zero();

    float absModifier = modifier < 0.0f ? -modifier : modifier;
    AsInt64 scaled = ScaleAsInt64ByDouble(damage, double(absModifier));
    return modifier < 0.0f ? -scaled : scaled;
}

uint32 GetAuraOrLearnedSpellCount(Unit const* unit, uint32 spellId)
{
    if (!unit)
        return 0;

    uint32 count = unit->GetAtAuraCount(spellId);
    return count ? count : (unit->HasSpell(spellId) ? 1 : 0);
}

void AddAuraAsDamageMod(Player* player, uint32 auraOrSpellId, uint32 castId, float modifier, AsInt64 const& baseDamage, AsInt64& amount)
{
    if (modifier == 0.0f)
        return;

    if (player->HasAura(auraOrSpellId) || player->HasSpell(auraOrSpellId))
    {
        uint32 count = GetAuraOrLearnedSpellCount(player, auraOrSpellId);
        for (uint32 i = 0; i < count; ++i)
            amount += ScaleAuraAsDamage(baseDamage, modifier);
    }
    else if (castId && auraOrSpellId == castId)
    {
        amount += ScaleAuraAsDamage(baseDamage, modifier);
    }
}

AsInt64 ClampInt64(long double value)
{
    return Acore::Number::ToInt64Saturated(value);
}

AsInt64 GetAsStatMaskValue(Player* player, uint32 checkValue)
{
    if (!player || !checkValue)
        return AsInt64Zero();

    AsInt64 now = AsInt64Zero();
    uint32 Mask[] = { 1, 2, 4, 8, 16, 32 };
    for (auto const& Id : Mask)
    {
        if (!(checkValue & Id))
            continue;

        if (Id == 32)
        {
            int32 val = player->GetRatingBonusValue(CR_HASTE_MELEE);
            if (val > 0)
                now += val;
        }
        else
        {
            Stats st = sAuraSpellMgr->AutoGetStatForId(Id);
            AsInt64 val = player->GetAsRealStat(st);
            if (val <= 0)
                val = AsInt64(static_cast<int64>(std::max(0.0f, player->GetTotalStatValue(st))));
            now += val;
        }
    }

    return now > 0 ? now : AsInt64Zero();
}

AsInt64 ScaleAsDamageByStatRatio(AsInt64 const& damage, AsInt64 const& statValue, float statUpValue)
{
    if (damage <= 0)
        return AsInt64Zero();

    if (statValue <= 0)
        return AsInt64Zero();

    if (statUpValue <= 0.0f)
        return damage;

    int64 divisor = int64(statUpValue * 10000.0f + 0.5f);
    if (divisor <= 0)
        return damage;
    long double result = static_cast<long double>(damage) * static_cast<long double>(statValue) * 10000.0L / static_cast<long double>(divisor);
    return ClampInt64(result);
}
}

void AuraSpellMgr::LoadAuraSpellMgr()
{
    OnAsStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 技能光环编号, 吸血倍率, PVE增伤倍率之前, PVE增伤倍率, PVE减伤倍率, PVP增伤倍率, PVP减伤倍率, 增加物品爆率, 物品爆率比例, 多倍掉落几率, 多倍掉落, 护盾吸收攻强比, 护盾吸收法强比, 斗气经验比例, PVP物理暴击伤害比例, PVP技能暴击伤害比例, PVE物理暴击伤害比例, PVE技能暴击伤害比例, 附带真实伤害值, 额外掉落几率, 额外掉落编号, 掉落数量比例, 掉落数量加值 FROM _技能光环控制表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 AuraId = Fields[0].Get<uint32>();
            AsTemp St;
            St.XMod = Fields[1].Get<float>();
            St.PVEDmgBeforeMod = Fields[2].Get<float>();
            St.PVEDmgMod = Fields[3].Get<float>();
            St.PVERetMod = Fields[4].Get<float>();
            if (St.PVERetMod >= 1.0f)
                St.PVERetMod = 0.99999f;
            St.PVPDmgMod = Fields[5].Get<float>();
            St.PVPRetMod = Fields[6].Get<float>();
            if (St.PVPRetMod >= 1.0f)
                St.PVPRetMod = 0.99999f;
            St.LootOddsMod = Fields[7].Get<float>();
            St.LootOddsKnMod = Fields[8].Get<float>();
            St.LootCountOdds = Fields[9].Get<float>();
            St.LootCountMod = Fields[10].Get<uint32>();
            St.AXsMod = Fields[11].Get<float>();
            St.SXsMod = Fields[12].Get<float>();
            St.StatMod = Fields[13].Get<float>();
            St.PVPMelee = Fields[14].Get<float>();
            St.PVPSpell = Fields[15].Get<float>();
            St.PVEMelee = Fields[16].Get<float>();
            St.PVESpell = Fields[17].Get<float>();
            St.GMDmg = AsInt64FromString(Fields[18].Get<std::string>());
            St.LootChance = Fields[19].Get<float>();
            St.LootId = Fields[20].Get<uint32>();
            St.LootCountPt = Fields[21].Get<float>();
            if (St.LootCountPt < 1.0f)
                St.LootCountPt = 1.0f;

            St.LootCountEx = Fields[22].Get<uint32>();

            if (AuraId == TxLevelSpell)
                continue;

            OnAsStore.insert(std::make_pair(AuraId, St));
        } while (Result->NextRow());
    }

    m_OnAyThrough.clear();

    SpellCastStore.clear();
    Result = WorldDatabase.Query("SELECT 技能编号, GM命令组 FROM _技能施法完成gm命令表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 SpellId = Fields[0].Get<uint32>();
            std::string Cmds = Fields[1].Get<std::string>();

            if (SpellId == TxLevelSpell)
                continue;

            SpellCastStore.insert(std::make_pair(SpellId, Cmds));
        } while (Result->NextRow());
    }

    SpellAbsorbStore.clear();
    Result = WorldDatabase.Query("SELECT 技能编号, 固定吸收量, 增加吸收量, 攻强加成比例, 法强加成比例, 关联的光环, 关联后加成比例 FROM _吸收盾技能特殊控制表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            SpellAbsorbTemp St;
            uint32 SpellId = Fields[0].Get<uint32>();
            St.NowAbsorb = Fields[1].Get<uint32>();
            St.AddAbsorb = Fields[2].Get<uint32>();
            St.APMod = Fields[3].Get<float>();
            St.SPMod = Fields[4].Get<float>();

            St.LinkSpells.clear();
            std::string StrAuras = Fields[5].Get<std::string>();
            if (!StrAuras.empty())
            {
                Tokenizer AurasData(StrAuras, '#');
                for (auto const& Temp : AurasData)
                    if (sSpellMgr->GetSpellInfo(uint32(atoi(Temp))))
                        St.LinkSpells.insert(uint32(atoi(Temp)));
            }
            St.LinkMod = Fields[6].Get<float>();

            if (SpellId == TxLevelSpell)
                continue;

            SpellAbsorbStore.insert(std::make_pair(SpellId, St));
        } while (Result->NextRow());
    }
}

float AuraSpellMgr::ThroughAuraTP(Player* player)
{
    if (m_OnAyThrough.empty())
        return 0.0f;

    float Amount = 0.0f;
    for (auto Itr = m_OnAyThrough.begin(); Itr != m_OnAyThrough.end(); ++Itr)
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            uint32 Count = player->GetAtAuraCount(Itr->first);
            for (uint32 N = 0; N < Count; N++)
                Amount += Itr->second.TP_bl / 100.0f;  // 除以100，使数据库填1=1%
        }
    }

    return Amount;
}

// 玩家目标 - 应用玩家减伤
float AuraSpellMgr::ThroughAuraTPWithPlayerRet(Player* player, bool IsPVP)
{
    float Amount = ThroughAuraTP(player);

    if (Amount <= 0.0f)
        return 0.0f;

    // 转换为伤害值处理（减伤函数需要uint32引用）
    uint32 Damage = (uint32)(Amount * 10000);

    if (IsPVP)
    {
        // 应用__模版玩家属性表 PVP免伤率
        Damage -= (Damage * sCustomClass->GetPVPRetModForClass(player));
        // 应用技能光环 PVP减伤
        AutoPVPRet(player, Damage);
    }
    else
    {
        // 应用__模版玩家属性表 PVE免伤率
        Damage -= (Damage * sCustomClass->GetPVERetModForClass(player));
        // 应用技能光环 PVE减伤
        AutoPVERet(player, Damage);
    }

    return Damage / 10000.0f;
}

// 生物目标 - 应用生物减伤
float AuraSpellMgr::ThroughAuraTPWithCreatureRet(Player* attacker, Creature* creature)
{
    float Amount = ThroughAuraTP(attacker);

    if (Amount <= 0.0f)
        return 0.0f;

    // 应用生物属性模板减伤
    Amount -= (Amount * sAsCreature->GetRetMod(creature));
    // 应用迷宫生物免伤
    Amount -= (Amount * sAsCreature->GetMazRetMod(creature));

    return Amount;
}

void AuraSpellMgr::HealXsOnDamage(Unit* Caster, Unit* Target, AsInt64 const& Damage, uint32 CastId)
{
    if (!Caster || !Target || Damage <= 0 || !Caster->IsInWorld() || !Target->IsInWorld() || Caster == Target)
        return;

    if (OnAsStore.empty())
        return;

    if (Caster->ToPlayer())
    {
        AsInt64 const& RealDamage = Damage;
        for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
        {
            AsInt64 Amount = AsInt64Zero();
            SpellInfo const* SInfo = sSpellMgr->GetSpellInfo(Itr->first);
            if (Caster->ToPlayer()->IsAlive() && SInfo)
            {
                if (Caster->ToPlayer()->HasAura(Itr->first))
                {
            uint32 Count = Caster->ToPlayer()->GetAtAuraCount(Itr->first);
                    for (uint32 N = 0; N < Count; N++)
                    {
                        Amount += ScaleAuraAsDamage(RealDamage, Itr->second.XMod);
                    }

                    if (Amount)
                    {
                        AsInt64 effectiveDamage = Target->GetAsTotalHealth();
                        if (effectiveDamage > 0 && Amount > effectiveDamage)
                            Amount = effectiveDamage;
                        uint32 CompatAmount = AsInt64ToUInt32Clamp(Amount, uint32(std::numeric_limits<int32>::max()));
                        Caster->SetAsHealOverride(Caster->GetGUID(), Amount);
                        SendProcHealDisplay(Caster->ToPlayer(), Amount, SInfo);
                        HealInfo HInfo(Caster, Caster, CompatAmount, SInfo, SInfo->GetSchoolMask());
                        Caster->ToPlayer()->HealBySpell(HInfo);
                    }
                }
                else
                {
                    if (Caster->ToPlayer()->HasSpell(Itr->first))
                    {
                        if (CastId && Itr->first == CastId)
                        {
                            Amount += ScaleAuraAsDamage(RealDamage, Itr->second.XMod);
                            if (Amount)
                            {
                            AsInt64 effectiveDamage = Target->GetAsTotalHealth();
                            if (effectiveDamage > 0 && Amount > effectiveDamage)
                                Amount = effectiveDamage;
                            uint32 CompatAmount = AsInt64ToUInt32Clamp(Amount, uint32(std::numeric_limits<int32>::max()));
                                Caster->SetAsHealOverride(Caster->GetGUID(), Amount);
                                SendProcHealDisplay(Caster->ToPlayer(), Amount, SInfo);
                                HealInfo HInfo(Caster, Caster, CompatAmount, SInfo, SInfo->GetSchoolMask());
                                Caster->ToPlayer()->HealBySpell(HInfo);
                            }
                        }
                    }
                }
            }
        }
    }
}

void AuraSpellMgr::AutoPVEDmg(Player* player, uint32& Damage, uint32 CastId)
{
    if (!Damage)
        return;

    AsInt64 realDamage = AsInt64(Damage);
    AutoPVEDmg(player, realDamage, CastId);
    Damage = AsInt64ToUInt32Clamp(realDamage, uint32(std::numeric_limits<int32>::max()));
}

void AuraSpellMgr::AutoInt32PVEDmg(Player* player, int32& Damage, uint32 CastId)
{
    if (Damage <= 0)
        return;

    AsInt64 realDamage = AsInt64(Damage);
    AutoPVEDmgBefore(player, realDamage, CastId);
    Damage = AsInt64ToInt32Clamp(realDamage, std::numeric_limits<int32>::max());
}

void AuraSpellMgr::AutoPVEDmgBefore(Player* player, AsInt64& Damage, uint32 CastId)
{
    if (OnAsStore.empty() || Damage <= 0)
        return;

    AsInt64 Amount = Damage;
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
        AddAuraAsDamageMod(player, Itr->first, CastId, Itr->second.PVEDmgBeforeMod, Damage, Amount);

    Damage = Amount > 0 ? Amount : AsInt64Zero();
}

void AuraSpellMgr::AutoPVEDmg(Player* player, AsInt64& Damage, uint32 CastId)
{
    if (OnAsStore.empty() || Damage <= 0)
        return;

    AsInt64 Amount = Damage;
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
        AddAuraAsDamageMod(player, Itr->first, CastId, Itr->second.PVEDmgMod, Damage, Amount);

    Damage = Amount > 0 ? Amount : AsInt64Zero();
}

void AuraSpellMgr::AutoPVERet(Player* player, AsInt64& Damage, uint32 CastId)
{
    if (OnAsStore.empty() || Damage <= 0)
        return;

    AsInt64 Amount = AsInt64Zero();
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
        AddAuraAsDamageMod(player, Itr->first, CastId, Itr->second.PVERetMod, Damage, Amount);

    if (Amount > 0)
        Damage = Amount >= Damage ? AsInt64(1) : Damage - Amount;
}

void AuraSpellMgr::AutoPVPDmg(Player* player, AsInt64& Damage, uint32 CastId)
{
    if (OnAsStore.empty() || Damage <= 0)
        return;

    AsInt64 Amount = Damage;
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
        AddAuraAsDamageMod(player, Itr->first, CastId, Itr->second.PVPDmgMod, Damage, Amount);

    Damage = Amount > 0 ? Amount : AsInt64Zero();
}

void AuraSpellMgr::AutoPVPRet(Player* player, AsInt64& Damage, uint32 CastId)
{
    if (OnAsStore.empty() || Damage <= 0)
        return;

    AsInt64 Amount = AsInt64Zero();
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
        AddAuraAsDamageMod(player, Itr->first, CastId, Itr->second.PVPRetMod, Damage, Amount);

    if (Amount > 0)
        Damage = Amount >= Damage ? AsInt64(1) : Damage - Amount;
}

void AuraSpellMgr::AutoPVERet(Player* player, uint32& Damage, uint32 CastId)
{
    if (!Damage)
        return;

    AsInt64 realDamage = AsInt64(Damage);
    AutoPVERet(player, realDamage, CastId);
    Damage = AsInt64ToUInt32Clamp(realDamage, uint32(std::numeric_limits<int32>::max()));
}

void AuraSpellMgr::AutoPVPDmg(Player* player, uint32& Damage, uint32 CastId)
{
    if (!Damage)
        return;

    AsInt64 realDamage = AsInt64(Damage);
    AutoPVPDmg(player, realDamage, CastId);
    Damage = AsInt64ToUInt32Clamp(realDamage, uint32(std::numeric_limits<int32>::max()));
}

void AuraSpellMgr::AutoPVPRet(Player* player, uint32& Damage, uint32 CastId)
{
    if (!Damage)
        return;

    AsInt64 realDamage = AsInt64(Damage);
    AutoPVPRet(player, realDamage, CastId);
    Damage = AsInt64ToUInt32Clamp(realDamage, uint32(std::numeric_limits<int32>::max()));
}

float AuraSpellMgr::GetLootOddsMod(Player* player)
{
    if (OnAsStore.empty())
        return 0.0f;

    float Mod = 0.0f;
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            uint32 Count = player->GetAtAuraCount(Itr->first);
            for (uint32 N = 0; N < Count; N++)
                Mod += Itr->second.LootOddsMod;
        }
    }

    return Mod;
}

void AuraSpellMgr::AutoLootOddsKn(Player* player, float& Chance)
{
    if (OnAsStore.empty())
        return;

    float Amount = Chance;
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            uint32 Count = player->GetAtAuraCount(Itr->first);
            for (uint32 N = 0; N < Count; N++)
                Amount += Chance * Itr->second.LootOddsKnMod;
        }
    }

    Chance = Amount;
}

uint32 AuraSpellMgr::GetLootCountMod(Player* player)
{
    if (OnAsStore.empty())
        return 0;

    uint32 Mod = 0;
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            if (roll_chance_f(Itr->second.LootCountOdds))
            {
                uint32 Count = player->GetAtAuraCount(Itr->first);
                for (uint32 N = 0; N < Count; N++)
                    Mod += Itr->second.LootCountMod;
            }
        }
    }

    return Mod;
}

AsInt64 AuraSpellMgr::GetStatNowHook(Player* player, AsInt64 const& Amount)
{
    if (OnAsStore.empty())
        return Amount;

    float Mod = 0.0f;
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            uint32 Count = player->GetAtAuraCount(Itr->first);
            for (uint32 N = 0; N < Count; N++)
                Mod += Itr->second.StatMod;
        }
    }

    AsInt64 AddAmount = ScaleAsInt64ByDouble(Amount, Mod);
    return Amount + AddAmount;
}

void AuraSpellMgr::LoadCustomSpellMgr()
{
    GlobalConfigStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 序号, 功能值 FROM __控制总表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Id = Fields[0].Get<uint32>();
            std::string Value = Fields[1].Get<std::string>();
            GlobalConfigStore.insert(std::make_pair(Id, Value));
        } while (Result->NextRow());
    }

    SpellModMap.clear();
    Result = WorldDatabase.Query("SELECT 技能ID, 释放技能需求模板ID,伤害倍率,治疗倍率,固定伤害,是否禁用,施法时间,持续时间,冷却时间,触发机率,周期时间1,周期时间2,周期时间3 FROM _技能;");
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();
            uint32 spellId = fields[0].Get<uint32>();
            SpellModTemplate Temp;
            Temp.reqId = fields[1].Get<uint32>();
            Temp.dmgMod = fields[2].Get<float>();
            Temp.healMod = fields[3].Get<float>();
            Temp.nowdmg = AsInt64FromString(fields[4].Get<std::string>());
            Temp.disable = fields[5].Get<bool>();
            Temp.castingtime = fields[6].Get<int32>();
            Temp.duration = fields[7].Get<uint32>();
            Temp.cooldown = fields[8].Get<uint32>();
            Temp.procChance = fields[9].Get<uint32>();

            for (size_t i = 0; i < MAX_SPELL_EFFECTS; i++)
                Temp.Periodic[i] = fields[10 + i].Get<uint32>();

            if (spellId == TxLevelSpell)
                continue;

            SpellModMap.insert(std::make_pair(spellId, Temp));
        } while (Result->NextRow());
    }

    SpellModBaseMap.clear();
    Result = WorldDatabase.Query("SELECT spellId,"
        "Effect1,Effect2,Effect3,"
        "AuraName1,AuraName2,AuraName3,MiscValue1,MiscValue2,MiscValue3,"
        "BasePoints1,BasePoints2,BasePoints3,"
        "SpellMaskA1,SpellMaskA2,SpellMaskA3,"
        "SpellMaskB1,SpellMaskB2,SpellMaskB3,"
        "SpellMaskC1,SpellMaskC2,SpellMaskC3,"
        "EffectTarget1,EffectTarget2,EffectTarget3,"
        "targetspell1,targetspell2,targetspell3,"
        "cooldown,targetflag,chance,charges,SpellFamilyName FROM _技能_数据库修改;");
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();
            uint32 spellId = fields[0].Get<uint32>();
            SpellModBaseTemplate Temp;
            Temp.Effect[0] = fields[1].Get<uint32>();
            Temp.Effect[1] = fields[2].Get<uint32>();
            Temp.Effect[2] = fields[3].Get<uint32>();
            Temp.EffectApplyAuraName[0] = fields[4].Get<uint32>();
            Temp.EffectApplyAuraName[1] = fields[5].Get<uint32>();
            Temp.EffectApplyAuraName[2] = fields[6].Get<uint32>();
            Temp.EffectMiscValue[0] = fields[7].Get<int32>();
            Temp.EffectMiscValue[1] = fields[8].Get<int32>();
            Temp.EffectMiscValue[2] = fields[9].Get<int32>();
            Temp.EffectBasePoints[0] = fields[10].Get<int32>();
            Temp.EffectBasePoints[1] = fields[11].Get<int32>();
            Temp.EffectBasePoints[2] = fields[12].Get<int32>();
            Temp.EffectSpellClassMask[0] = flag96(fields[13].Get<uint32>(), fields[14].Get<uint32>(), fields[15].Get<uint32>());
            Temp.EffectSpellClassMask[1] = flag96(fields[16].Get<uint32>(), fields[17].Get<uint32>(), fields[18].Get<uint32>());
            Temp.EffectSpellClassMask[2] = flag96(fields[19].Get<uint32>(), fields[20].Get<uint32>(), fields[21].Get<uint32>());
            Temp.EffectImplicitTargetA[0] = fields[22].Get<uint32>();
            Temp.EffectImplicitTargetA[1] = fields[23].Get<uint32>();
            Temp.EffectImplicitTargetA[2] = fields[24].Get<uint32>();
            Temp.EffectTriggerSpell[0] = fields[25].Get<uint32>();
            Temp.EffectTriggerSpell[1] = fields[26].Get<uint32>();
            Temp.EffectTriggerSpell[2] = fields[27].Get<uint32>();
            Temp.RecoveryTime = fields[28].Get<int32>();
            uint32 procFlags = fields[29].Get<uint32>();

            switch (procFlags)
            {
            case 1:Temp.procFlags = 20;					break;//物理
            case 2:Temp.procFlags = 320;				break;//远程
            case 4:Temp.procFlags = 81920;				break;//法术
            case 7:Temp.procFlags = 20 + 320 + 81920;	break;//所有
            case 3:Temp.procFlags = 20 + 320;			break;
            case 5:Temp.procFlags = 20 + 81920;			break;
            case 6:Temp.procFlags = 320 + 81920;		break;
            default:Temp.procFlags = 0;					break;
            }

            Temp.procChance = fields[30].Get<float>();
            Temp.procCharges = fields[31].Get<int32>();
            Temp.SpellFamilyName = fields[32].Get<uint32>();

            if (spellId == TxLevelSpell)
                continue;

            SpellModBaseMap.insert(std::make_pair(spellId, Temp));
        } while (Result->NextRow());
    }

    AuraModStatMap.clear();
    Result = WorldDatabase.Query("SELECT 光环技能ID,光环类型1,属性类型1,转换百分比1,光环类型2,属性类型2,转换百分比2,光环类型3,属性类型3,转换百分比3 FROM _技能_主属性转换;");
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();
            uint32 aura = fields[0].Get<uint32>();
            AuraModStatTemplate Temp;
            AuraType auraType = SPELL_AURA_NONE;
            int32 misc = 0;
            int32 miscB = 0;

            GetParams_PctOnStat(fields[1].Get<std::string_view>().data(), fields[2].Get<std::string_view>().data(), auraType, misc, miscB);
            Temp.auraType[0] = auraType;
            Temp.misc[0] = misc;
            Temp.miscB[0] = miscB;
            Temp.basePoints[0] = fields[3].Get<int32>();

            auraType = SPELL_AURA_NONE;
            misc = 0;
            miscB = 0;
            GetParams_PctOnStat(fields[4].Get<std::string_view>().data(), fields[5].Get<std::string_view>().data(), auraType, misc, miscB);
            Temp.auraType[1] = auraType;
            Temp.misc[1] = misc;
            Temp.miscB[1] = miscB;
            Temp.basePoints[1] = fields[6].Get<int32>();

            auraType = SPELL_AURA_NONE;
            misc = 0;
            miscB = 0;
            GetParams_PctOnStat(fields[7].Get<std::string_view>().data(), fields[8].Get<std::string_view>().data(), auraType, misc, miscB);
            Temp.auraType[2] = auraType;
            Temp.misc[2] = misc;
            Temp.miscB[2] = miscB;
            Temp.basePoints[2] = fields[9].Get<int32>();

            if (aura == TxLevelSpell)
                continue;

            AuraModStatMap.insert(std::make_pair(aura, Temp));
        } while (Result->NextRow());
    }

    AuraPctMap.clear();
    Result = WorldDatabase.Query("SELECT 光环技能ID,效果类型1,提升类型1,提升数值1,效果类型2,提升类型2,提升数值2,效果类型3,提升类型3,提升数值3 FROM _技能_光环百分比效果;");
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();
            uint32 aura = fields[0].Get<uint32>();
            AuraPctTemplate Temp;
            AuraType auraType = SPELL_AURA_NONE;
            int32 misc = 0;
            int32 miscB = 0;
            GetParams_Pct(fields[1].Get<std::string_view>().data(), fields[2].Get<std::string_view>().data(), auraType, misc, miscB);
            Temp.auraType[0] = auraType;
            Temp.misc[0] = misc;
            Temp.miscB[0] = miscB;
            Temp.basePoints[0] = fields[3].Get<int32>();

            auraType = SPELL_AURA_NONE;
            misc = 0;
            miscB = 0;
            GetParams_Pct(fields[4].Get<std::string_view>().data(), fields[5].Get<std::string_view>().data(), auraType, misc, miscB);
            Temp.auraType[1] = auraType;
            Temp.misc[1] = misc;
            Temp.miscB[1] = miscB;
            Temp.basePoints[1] = fields[6].Get<int32>();

            auraType = SPELL_AURA_NONE;
            misc = 0;
            miscB = 0;
            GetParams_Pct(fields[7].Get<std::string_view>().data(), fields[8].Get<std::string_view>().data(), auraType, misc, miscB);
            Temp.auraType[2] = auraType;
            Temp.misc[2] = misc;
            Temp.miscB[2] = miscB;
            Temp.basePoints[2] = fields[9].Get<int32>();

            if (aura == TxLevelSpell)
                continue;

            AuraPctMap.insert(std::make_pair(aura, Temp));
        } while (Result->NextRow());
    }

    AuraTriggerSpellMap.clear();
    Result = WorldDatabase.Query("SELECT 光环ID,攻强百分比增加基础值1,攻强百分比增加基础值2,攻强百分比增加基础值3,法伤百分比增加基础值1,法伤百分比增加基础值2,法伤百分比增加基础值3,链接技能组,触发几率,触发技能组,冷却时间,忽略目标类型,目标最大数量,目标范围,触发类型  FROM _技能_光环触发;");
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();
            uint32 auraId = fields[0].Get<uint32>();
            AuraTriggerSpellTemplate Temp;
            Temp.bp0ApPct = fields[1].Get<float>();
            Temp.bp1ApPct = fields[2].Get<float>();
            Temp.bp2ApPct = fields[3].Get<float>();
            Temp.bp0SpPct = fields[4].Get<float>();
            Temp.bp1SpPct = fields[5].Get<float>();
            Temp.bp2SpPct = fields[6].Get<float>();

            Tokenizer tokens(fields[7].Get<std::string>(), ' ');
            for (Tokenizer::const_iterator itr = tokens.begin(); itr != tokens.end(); ++itr)
                Temp.linkSpellData.push_back(uint32(atol(*itr)));

            Temp.procChance = fields[8].Get<uint32>();

            Tokenizer tokens1(fields[9].Get<std::string>(), ' ');
            for (Tokenizer::const_iterator itr = tokens1.begin(); itr != tokens1.end(); ++itr)
            {
                uint32 TSpellId = uint32(atol(*itr));
                const SpellInfo* SInfo = sSpellMgr->GetSpellInfo(TSpellId);
                if (!SInfo)
                    continue;
                if (SInfo->SpellFamilyName == SPELLFAMILY_WARRIOR && (SInfo->Id == 1680 || SInfo->Id == 44949))
                    continue;

                Temp.triggerSpellData.push_back(TSpellId);
            }

            Temp.cooldown = fields[10].Get<uint32>();
            Temp.ignoreMask = fields[11].Get<uint32>();
            Temp.Targets = fields[12].Get<uint32>();
            Temp.TargetRange = fields[13].Get<float>();
            Temp.procFlags = fields[14].Get<uint32>();

            if (auraId == TxLevelSpell)
                continue;

            AuraTriggerSpellMap.insert(std::make_pair(auraId, Temp));
        } while (Result->NextRow());
    }

    AuraStackTriggerMap.clear();
    Result = WorldDatabase.Query("SELECT 光环技能ID, 叠加层数, 触发技能组, 触发技能后移除层数  FROM _技能_光环叠加;");
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();
            uint32 Aura = fields[0].Get<uint32>();
            AuraStackTriggerTemplate Temp;
            Temp.Stacks = fields[1].Get<uint8>();

            Tokenizer tokens(fields[2].Get<std::string>(), ' ');
            for (Tokenizer::const_iterator itr = tokens.begin(); itr != tokens.end(); ++itr)
                Temp.TriggerSpellVec.push_back(uint32(atol(*itr)));

            Temp.RemoveStacks = fields[3].Get<uint8>();

            if (Temp.RemoveStacks > Temp.Stacks)
                Temp.RemoveStacks = Temp.Stacks;

            if (Aura == TxLevelSpell)
                continue;

            AuraStackTriggerMap.insert(std::make_pair(Aura, Temp));
        } while (Result->NextRow());
    }

    AuraTriggerOnDeathMap.clear();
    Result = WorldDatabase.Query("SELECT 光环技能ID, 冷却时间, 对攻击者施放技能ID, 对攻击者施放技能几率, 对自身施放技能ID,对自身施放技能几率,是否阻止死亡  FROM _技能_光环死亡触发;");
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();
            uint32 AuraId = fields[0].Get<uint32>();
            AuraTriggerOnDeathTemplate Temp;
            Temp.CoolDown = fields[1].Get<uint32>();
            Temp.AttackerTriggerSpell = fields[2].Get<uint32>();
            Temp.AttackerTriggerChance = fields[3].Get<float>();
            Temp.SelfTriggerSpell = fields[4].Get<uint32>();
            Temp.SelfTriggerChance = fields[5].Get<float>();
            Temp.PreventLastDamage = fields[6].Get<bool>();

            if (AuraId == TxLevelSpell)
                continue;

            AuraTriggerOnDeathMap.insert(std::make_pair(AuraId, Temp));
        } while (Result->NextRow());
    }

    SpellCusTargetMaskMap.clear();
    Result = WorldDatabase.Query("SELECT 技能ID,目标掩码 FROM _技能_目标限制;");
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();

            if (fields[0].Get<uint32>() == TxLevelSpell)
                continue;

            SpellCusTargetMaskMap.insert(std::make_pair(fields[0].Get<uint32>(), fields[1].Get<uint32>()));
        } while (Result->NextRow());
    }

    BusAuraStore.clear();
    Result = WorldDatabase.Query("SELECT 技能光环编号, 加强技能编号, PVE增伤倍率, PVE治疗倍率, PVP增伤倍率, PVP治疗倍率, 全局伤害加值, 属性掩码, 属性强化伤害率, 属性转换伤害率, 附加法术强度比例伤害, 附加攻击强度比例伤害 FROM _技能加强关联控制表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 AuraId = Fields[0].Get<uint32>();
            BusAuraTemp St;
            St.TarSpellId = Fields[1].Get<uint32>();
            St.PVEDmgMod = Fields[2].Get<float>();
            St.PVEHeMod = Fields[3].Get<float>();
            St.PVPDmgMod = Fields[4].Get<float>();
            St.PVPHeMod = Fields[5].Get<float>();
            St.AddDmg = AsInt64FromString(Fields[6].Get<std::string>());
            St.StatMask = Fields[7].Get<uint32>();
            St.StatUpValue = Fields[8].Get<float>();
            St.StatValue = Fields[9].Get<float>();
            St.AddSPMod = Fields[10].Get<float>();
            St.AddAPMod = Fields[11].Get<float>();

            if (AuraId == TxLevelSpell)
                continue;

            BusAuraStore.insert(std::make_pair(AuraId, St));

        } while (Result->NextRow());
    }

    BusXsAuraStore.clear();
    Result = WorldDatabase.Query("SELECT 技能光环编号, 施行技能编号, PVE吸血倍率, PVP吸血倍率, PVE固定吸血, PVP固定吸血 FROM _技能特殊吸血控制表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 AuraId = Fields[0].Get<uint32>();
            BusXsAuraTemp St;
            St.ShowSpellId = Fields[1].Get<uint32>();
            St.PVEMod = Fields[2].Get<float>();
            St.PVPMod = Fields[3].Get<float>();
            St.PVEValue = AsInt64FromString(Fields[4].Get<std::string>());
            St.PVPValue = AsInt64FromString(Fields[5].Get<std::string>());

            if (AuraId == TxLevelSpell)
                continue;

            BusXsAuraStore.insert(std::make_pair(AuraId, St));

        } while (Result->NextRow());
    }

    LinkAuraStackMap.clear();
    Result = WorldDatabase.Query("SELECT 主光环编号, 关联光环编号, 关联光环层数, 触发技能组, 触发技能后移除层数  FROM _光环控制光环关联层数表;");
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();
            LinkAuraStackTemp Temp;
            Temp.SAuraId = fields[0].Get<uint32>();
            Temp.LAuraId = fields[1].Get<uint32>();
            Temp.Stacks = fields[2].Get<uint8>();

            Tokenizer tokens(fields[3].Get<std::string>(), '#');
            for (Tokenizer::const_iterator itr = tokens.begin(); itr != tokens.end(); ++itr)
                Temp.SpellSet.insert(uint32(atol(*itr)));

            Temp.RemoveStacks = fields[4].Get<uint8>();

            if (Temp.RemoveStacks > Temp.Stacks)
                Temp.RemoveStacks = Temp.Stacks;

            if (Temp.SAuraId == TxLevelSpell)
                continue;

            LinkAuraStackMap.push_back(Temp);
        } while (Result->NextRow());
    }
}

bool AuraSpellMgr::Enable(Player* player, uint32 spellId)
{
    auto iter = SpellModMap.find(spellId);
    if (iter != SpellModMap.end())
    {
        if (iter->second.disable)
            return false;

        uint32 reqId = GetReqId(spellId);

        if (sHelper->CheckNeed(player, reqId, "技能释放消耗"))
        {
            return true;
        }
        else
            return false;
    }

    return true;
}

uint32 AuraSpellMgr::GetReqId(uint32 spellId)
{
    auto iter = SpellModMap.find(spellId);
    if (iter != SpellModMap.end())
        return iter->second.reqId;

    return 0;
}

float AuraSpellMgr::GetXhDmgMod(uint32 spellId)
{
    auto iter = SpellModMap.find(spellId);
    if (iter != SpellModMap.end())
        return iter->second.dmgMod <= 0 ? 1.0f : iter->second.dmgMod;
    return 1.0f;
}

float AuraSpellMgr::GetXhHealMod(uint32 spellId)
{
    auto iter = SpellModMap.find(spellId);
    if (iter != SpellModMap.end())
        return iter->second.healMod <= 0 ? 1.0f : iter->second.healMod;
    return 1.0f;
}

int32 AuraSpellMgr::GetXhNowDmgMod(uint32 spellId)
{
    auto iter = SpellModMap.find(spellId);
    if (iter != SpellModMap.end())
        return AsInt64ToInt32Clamp(iter->second.nowdmg);
    return 0;
}

AsInt64 AuraSpellMgr::GetAsXhNowDmgMod(uint32 spellId)
{
    auto iter = SpellModMap.find(spellId);
    if (iter != SpellModMap.end())
        return iter->second.nowdmg;
    return AsInt64Zero();
}

bool AuraSpellMgr::Ignore(uint32 ignoreMask, Unit* target)
{
    if (!target)
        return true;

    bool ignore = false;

    if (target->GetMap()->IsDungeon())
        if ((ignoreMask & AURA_TRIGGER_IGNORE_DUNGEON) == AURA_TRIGGER_IGNORE_DUNGEON)
            ignore = true;

    if (!target->GetMap()->IsDungeon())
        if ((ignoreMask & AURA_TRIGGER_IGNORE_NO_DUNGEON) == AURA_TRIGGER_IGNORE_NO_DUNGEON)
            ignore = true;

    if (Creature* creature = target->ToCreature())
        if (creature->IsDungeonBoss() || creature->isWorldBoss())
            if ((ignoreMask & AURA_TRIGGER_IGNORE_BOSS) == AURA_TRIGGER_IGNORE_BOSS)
                ignore = true;

    if (target->GetTypeId() == TYPEID_PLAYER)
        if ((ignoreMask & AURA_TRIGGER_IGNORE_PLAYER) == AURA_TRIGGER_IGNORE_PLAYER)
            ignore = true;

    if (target->GetTypeId() == TYPEID_UNIT)
        if ((ignoreMask & AURA_TRIGGER_IGNORE_UNIT) == AURA_TRIGGER_IGNORE_UNIT)
            ignore = true;

    return ignore;
}

void GetEnemyList(std::list<Unit*>& EnemyList, Unit* caster, float range)
{
    Acore::AnyAoETargetUnitInObjectRangeCheck u_check(caster, caster, range);
    Acore::UnitListSearcher<Acore::AnyAoETargetUnitInObjectRangeCheck> searcher(caster, EnemyList, u_check);
    Cell::VisitAllObjects(caster, searcher, range);
}

void GetFriendList(std::list<Unit*>& FriendList, Unit* caster, float range)
{
    Acore::AnyFriendlyUnitInObjectRangeCheck u_check(caster, caster, range, true);
    Acore::UnitListSearcher<Acore::AnyFriendlyUnitInObjectRangeCheck> searcher(caster, FriendList, u_check);
    Cell::VisitAllObjects(caster, searcher, range);
}

struct UnitSortTemplate
{
    Unit* unit;
    float distance;
};

void GetNearestUnitVec(std::vector<Unit*>& UnitVec, std::list<Unit*> UnitList, Unit* caster, uint32 MaxTarget)
{
    std::vector<UnitSortTemplate> UnitSortVec;

    for (std::list<Unit*>::iterator itr = UnitList.begin(); itr != UnitList.end(); ++itr)
    {
        UnitSortTemplate Temp;
        Temp.unit = *itr;
        Temp.distance = (*itr)->GetDistance(caster);
        UnitSortVec.push_back(Temp);
    }

    std::sort(UnitSortVec.begin(), UnitSortVec.end(), [](const UnitSortTemplate& V1, const UnitSortTemplate& V2) {return V1.distance < V2.distance; });

    for (size_t i = 0; i < UnitSortVec.size(); i++)
    {
        if (i < MaxTarget)
            UnitVec.push_back(UnitSortVec[i].unit);
    }
}

bool AuraSpellMgr::AuraTrigger(Unit* caster, Unit* victim, SpellInfo const* auraSpellInfo, SpellInfo const* procSpell, uint32 procFlags, Unit*& target, AuraEffect* triggeredByAura)
{
    auto itr = SpellModBaseMap.find(auraSpellInfo->Id);
    if (itr != SpellModBaseMap.end())
    {
        for (size_t i = 0; i < MAX_SPELL_EFFECTS; i++)
        {
            if (const SpellInfo* triggerSpellInfo = sSpellMgr->GetSpellInfo(itr->second.EffectTriggerSpell[i]))
            {
                if (itr->second.procFlags != 0)
                    continue;

                if (!roll_chance_f(itr->second.procChance))
                    continue;

                if (procSpell->Effects[i].SpellClassMask != auraSpellInfo->Effects[i].SpellClassMask)
                    continue;

                if (target == nullptr)
                    target = !(procFlags & (PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_POS | PROC_FLAG_DONE_SPELL_NONE_DMG_CLASS_POS)) && triggerSpellInfo && triggerSpellInfo->IsPositive() ? caster : victim;

                caster->CastSpell(target, triggerSpellInfo, true, nullptr, triggeredByAura);
            }
        }
    }

    auto iter = AuraTriggerSpellMap.find(auraSpellInfo->Id);
    if (iter != AuraTriggerSpellMap.end())
    {
        if (iter->second.triggerSpellData.empty())
            return true;

        uint32 triggerSpell = iter->second.triggerSpellData[urand(0, iter->second.triggerSpellData.size() - 1)];
        const SpellInfo* triggerSpellInfo = sSpellMgr->GetSpellInfo(triggerSpell);
        if (!triggerSpellInfo)
            return true;

        if (target == nullptr)
            target = !(procFlags & (PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_POS | PROC_FLAG_DONE_SPELL_NONE_DMG_CLASS_POS)) && triggerSpellInfo && triggerSpellInfo->IsPositive() ? caster : victim;

        if (Ignore(iter->second.ignoreMask, target))
            return true;

        if (caster->HasSpellCooldown(triggerSpell))
        {
            if (Player* player = caster->ToPlayer())
            {
                if (player->GetSpellCooldownDelay(triggerSpell) <= iter->second.cooldown)
                    return true;
            }
            else
                return true;
        }

        bool canTrigger = false;
        if (iter->second.linkSpellData.empty() || procSpell && std::find(iter->second.linkSpellData.begin(), iter->second.linkSpellData.end(), procSpell->Id) != iter->second.linkSpellData.end())
            canTrigger = true;

        if (!canTrigger || !roll_chance_f(iter->second.procChance))
            return true;

        AsInt64 ap = std::max(caster->GetAsTotalAttackPowerValue(BASE_ATTACK), caster->GetAsTotalAttackPowerValue(RANGED_ATTACK));
        if (ap < 0)
            ap = AsInt64Zero();

        AsInt64 sp = caster->GetAsSpellPowerValue(SPELL_SCHOOL_MASK_MAGIC);
        if (sp < 0)
            sp = AsInt64Zero();

        auto BuildTriggerBasePoint = [&](uint8 effIndex, float apPct, float spPct) -> int32
            {
                AsInt64 value = AsInt64(triggerSpellInfo->Effects[effIndex].BasePoints);
                value += ScaleAuraAsDamage(ap, apPct / 100.0f);
                value += ScaleAuraAsDamage(sp, spPct / 100.0f);
                return AsInt64ToInt32Clamp(value);
            };

        int32 bp0 = BuildTriggerBasePoint(EFFECT_0, iter->second.bp0ApPct, iter->second.bp0SpPct);
        int32 bp1 = BuildTriggerBasePoint(EFFECT_1, iter->second.bp1ApPct, iter->second.bp1SpPct);
        int32 bp2 = BuildTriggerBasePoint(EFFECT_2, iter->second.bp2ApPct, iter->second.bp2SpPct);

        if (iter->second.Targets == 1)
        {
            caster->CastCustomSpell(target, triggerSpell, &bp0, &bp1, &bp2, true, nullptr, triggeredByAura);
            caster->AddSpellCooldown(triggerSpell, 0, iter->second.cooldown);
            return true;
        }

        std::list<Unit*> UnitList;
        if (target == victim)
        {
            GetEnemyList(UnitList, caster, iter->second.TargetRange);
            if (UnitList.empty())
                return true;

            std::vector<Unit*> UnitVec;
            GetNearestUnitVec(UnitVec, UnitList, caster, iter->second.Targets);

            if (UnitVec.empty())
                return true;

            for (std::vector<Unit*>::iterator itr = UnitVec.begin(); itr != UnitVec.end(); ++itr)
                caster->CastCustomSpell((*itr), triggerSpell, &bp0, &bp1, &bp2, true, nullptr, triggeredByAura);

            caster->AddSpellCooldown(triggerSpell, 0, iter->second.cooldown);
        }
        else if (target == caster)
        {
            GetFriendList(UnitList, caster, iter->second.TargetRange);

            if (UnitList.empty())
                return true;

            std::vector<Unit*> UnitVec;
            GetNearestUnitVec(UnitVec, UnitList, caster, iter->second.Targets);

            if (UnitVec.empty())
                return true;

            for (std::vector<Unit*>::iterator itr = UnitVec.begin(); itr != UnitVec.end(); ++itr)
                (*itr)->CastCustomSpell((*itr), triggerSpell, &bp0, &bp1, &bp2, true, nullptr, triggeredByAura);

            caster->AddSpellCooldown(triggerSpell, 0, iter->second.cooldown);
        }
        return true;
    }

    return false;
}

void AuraSpellMgr::GetParams_PctOnStat(const char* strAuraType, const char* strStatType, AuraType& auraType, int32& misc, int32& miscB)
{
    int32 i = -1;

    if (strcmp(strStatType, "精神") == 0)
        i = STAT_SPIRIT;
    else if (strcmp(strStatType, "力量") == 0)
        i = STAT_STRENGTH;
    else if (strcmp(strStatType, "敏捷") == 0)
        i = STAT_AGILITY;
    else if (strcmp(strStatType, "耐力") == 0)
        i = STAT_STAMINA;
    else if (strcmp(strStatType, "智力") == 0)
        i = STAT_INTELLECT;

    if (strcmp(strAuraType, "法术强度") == 0)
    {
        auraType = SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT;
        misc = SPELL_SCHOOL_MASK_MAGIC;
        miscB = i;
    }

    if (strcmp(strAuraType, "近战攻击强度") == 0)
    {
        auraType = SPELL_AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT;
        misc = i;
    }

    if (strcmp(strAuraType, "远程攻击强度") == 0)
    {
        auraType = SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT;
        misc = i;
    }

    if (strcmp(strAuraType, "治疗效果") == 0)
    {
        auraType = SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT;
        misc = i;
    }
}

void AuraSpellMgr::GetParams_Pct(const char* strAuraType, const char* strModType, AuraType& auraType, int32& misc, int32& miscB)
{
    if (strcmp(strAuraType, "施法速度") == 0)
        auraType = SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK;
    if (strcmp(strAuraType, "近战攻击速度") == 0)
        auraType = SPELL_AURA_MOD_MELEE_HASTE;
    if (strcmp(strAuraType, "远程攻击速度") == 0)
        auraType = SPELL_AURA_MOD_RANGED_HASTE;

    if (strcmp(strAuraType, "造成暴击伤害") == 0)
    {
        auraType = SPELL_AURA_MOD_CRIT_DAMAGE_BONUS;
        misc = SPELL_SCHOOL_MASK_ALL;
    }

    bool pct = strcmp(strModType, "百分比") == 0;

    if (strcmp(strAuraType, "生命值") == 0)
        pct ? auraType = SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT : auraType = SPELL_AURA_MOD_INCREASE_HEALTH;

    if (strcmp(strAuraType, "全属性") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_PERCENT_STAT : auraType = SPELL_AURA_MOD_STAT;
        misc = -1;
    }

    if (strcmp(strAuraType, "耐力") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_PERCENT_STAT : auraType = SPELL_AURA_MOD_STAT;
        misc = STAT_STAMINA;
    }

    if (strcmp(strAuraType, "敏捷") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_PERCENT_STAT : auraType = SPELL_AURA_MOD_STAT;
        misc = STAT_AGILITY;
    }

    if (strcmp(strAuraType, "力量") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_PERCENT_STAT : auraType = SPELL_AURA_MOD_STAT;
        misc = STAT_STRENGTH;
    }

    if (strcmp(strAuraType, "智力") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_PERCENT_STAT : auraType = SPELL_AURA_MOD_STAT;
        misc = STAT_INTELLECT;
    }

    if (strcmp(strAuraType, "精神") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_PERCENT_STAT : auraType = SPELL_AURA_MOD_STAT;
        misc = STAT_SPIRIT;
    }

    if (strcmp(strAuraType, "近战攻击强度") == 0)
        pct ? auraType = SPELL_AURA_MOD_ATTACK_POWER_PCT : auraType = SPELL_AURA_MOD_ATTACK_POWER;

    if (strcmp(strAuraType, "远程攻击强度") == 0)
        pct ? auraType = SPELL_AURA_MOD_RANGED_ATTACK_POWER_PCT : auraType = SPELL_AURA_MOD_RANGED_ATTACK_POWER;

    if (strcmp(strAuraType, "法术强度") == 0)
    {
        if (pct)
        {
            auraType = SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT;
            miscB = CtMiscValue;
        }
        else
            auraType = SPELL_AURA_MOD_DAMAGE_DONE;

        misc = SPELL_SCHOOL_MASK_MAGIC;
    }

    if (strcmp(strAuraType, "奥术法术强度") == 0)
    {
        if (pct)
        {
            auraType = SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT;
            miscB = CtMiscValue;
        }
        else
            auraType = SPELL_AURA_MOD_DAMAGE_DONE;

        misc = SPELL_SCHOOL_MASK_ARCANE;
    }

    if (strcmp(strAuraType, "火焰法术强度") == 0)
    {
        if (pct)
        {
            auraType = SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT;
            miscB = CtMiscValue;
        }
        else
            auraType = SPELL_AURA_MOD_DAMAGE_DONE;

        misc = SPELL_SCHOOL_MASK_FIRE;
    }

    if (strcmp(strAuraType, "冰霜法术强度") == 0)
    {
        if (pct)
        {
            auraType = SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT;
            miscB = CtMiscValue;
        }
        else
            auraType = SPELL_AURA_MOD_DAMAGE_DONE;

        misc = SPELL_SCHOOL_MASK_FROST;
    }

    if (strcmp(strAuraType, "神圣法术强度") == 0)
    {
        if (pct)
        {
            auraType = SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT;
            miscB = CtMiscValue;
        }
        else
            auraType = SPELL_AURA_MOD_DAMAGE_DONE;

        misc = SPELL_SCHOOL_MASK_HOLY;
    }

    if (strcmp(strAuraType, "暗影法术强度") == 0)
    {
        if (pct)
        {
            auraType = SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT;
            miscB = CtMiscValue;
        }
        else
            auraType = SPELL_AURA_MOD_DAMAGE_DONE;

        misc = SPELL_SCHOOL_MASK_SHADOW;
    }

    if (strcmp(strAuraType, "自然法术强度") == 0)
    {
        if (pct)
        {
            auraType = SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT;
            miscB = CtMiscValue;
        }
        else
            auraType = SPELL_AURA_MOD_DAMAGE_DONE;

        misc = SPELL_SCHOOL_MASK_NATURE;
    }

    if (strcmp(strAuraType, "治疗效果") == 0)
    {
        if (pct)
        {
            auraType = SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT;
            miscB = CtMiscValue;
        }
        else
            auraType = SPELL_AURA_MOD_HEALING;

        misc = SPELL_SCHOOL_MASK_MAGIC;
    }

    if (strcmp(strAuraType, "造成所有伤害") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_DAMAGE_PERCENT_DONE : auraType = SPELL_AURA_MOD_DAMAGE_DONE;
        misc = SPELL_SCHOOL_MASK_ALL;
    }

    if (strcmp(strAuraType, "造成物理伤害") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_DAMAGE_PERCENT_DONE : auraType = SPELL_AURA_MOD_DAMAGE_DONE;
        misc = SPELL_SCHOOL_MASK_NORMAL;
    }

    if (strcmp(strAuraType, "造成法术伤害") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_DAMAGE_PERCENT_DONE : auraType = SPELL_AURA_MOD_DAMAGE_DONE;
        misc = SPELL_SCHOOL_MASK_MAGIC;
    }

    if (strcmp(strAuraType, "造成奥术伤害") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_DAMAGE_PERCENT_DONE : auraType = SPELL_AURA_MOD_DAMAGE_DONE;
        misc = SPELL_SCHOOL_MASK_ARCANE;
    }

    if (strcmp(strAuraType, "造成火焰伤害") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_DAMAGE_PERCENT_DONE : auraType = SPELL_AURA_MOD_DAMAGE_DONE;
        misc = SPELL_SCHOOL_MASK_FIRE;
    }
    if (strcmp(strAuraType, "造成冰霜伤害") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_DAMAGE_PERCENT_DONE : auraType = SPELL_AURA_MOD_DAMAGE_DONE;
        misc = SPELL_SCHOOL_MASK_FROST;
    }
    if (strcmp(strAuraType, "造成神圣伤害") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_DAMAGE_PERCENT_DONE : auraType = SPELL_AURA_MOD_DAMAGE_DONE;
        misc = SPELL_SCHOOL_MASK_HOLY;
    }
    if (strcmp(strAuraType, "造成暗影伤害") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_DAMAGE_PERCENT_DONE : auraType = SPELL_AURA_MOD_DAMAGE_DONE;
        misc = SPELL_SCHOOL_MASK_SHADOW;
    }
    if (strcmp(strAuraType, "造成自然伤害") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_DAMAGE_PERCENT_DONE : auraType = SPELL_AURA_MOD_DAMAGE_DONE;
        misc = SPELL_SCHOOL_MASK_NATURE;
    }

    if (strcmp(strAuraType, "受到所有伤害") == 0)
    {
        pct ? auraType = SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN : auraType = SPELL_AURA_MOD_DAMAGE_TAKEN;
        misc = SPELL_SCHOOL_MASK_ALL;
    }
}

float AuraSpellMgr::AutoPVPMeleeCritAuraMod(Unit const* player)
{
    if (OnAsStore.empty())
        return 1.0f;

    float Mod = 0.0f;
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            uint32 Count = player->GetAtAuraCount(Itr->first);
            for (uint32 N = 0; N < Count; N++)
                Mod += Itr->second.PVPMelee;
        }
    }

    Mod = (Mod <= 0.0f) ? 1.0f : (Mod + 1.0f);
    return Mod;
}

float AuraSpellMgr::AutoPVPSpellCritAuraMod(Unit const* player)
{
    if (OnAsStore.empty())
        return 1.0f;

    float Mod = 0.0f;
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            uint32 Count = player->GetAtAuraCount(Itr->first);
            for (uint32 N = 0; N < Count; N++)
                Mod += Itr->second.PVPSpell;
        }
    }

    Mod = (Mod <= 0.0f) ? 1.0f : (Mod + 1.0f);
    return Mod;
}

float AuraSpellMgr::AutoPVEMeleeCritAuraMod(Unit const* player)
{
    if (OnAsStore.empty())
        return 1.0f;

    float Mod = 0.0f;
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            uint32 Count = player->GetAtAuraCount(Itr->first);
            for (uint32 N = 0; N < Count; N++)
                Mod += Itr->second.PVEMelee;
        }
    }

    Mod = (Mod <= 0.0f) ? 1.0f : (Mod + 1.0f);
    return Mod;
}

float AuraSpellMgr::AutoPVESpellCritAuraMod(Unit const* player)
{
    if (OnAsStore.empty())
        return 1.0f;

    float Mod = 0.0f;
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            uint32 Count = player->GetAtAuraCount(Itr->first);
            for (uint32 N = 0; N < Count; N++)
                Mod += Itr->second.PVESpell;
        }
    }

    Mod = (Mod <= 0.0f) ? 1.0f : (Mod + 1.0f);
    return Mod;
}

uint32 AuraSpellMgr::AutoGMDmgAuraMod(Player* player)
{
    return AsInt64ToUInt32Clamp(AutoAsGMDmgAuraMod(player), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 AuraSpellMgr::AutoAsGMDmgAuraMod(Player* player)
{
    if (OnAsStore.empty())
        return AsInt64Zero();

    AsInt64 At = AsInt64Zero();
    for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            uint32 Count = player->GetAtAuraCount(Itr->first);
            for (uint32 N = 0; N < Count; N++)
                At += Itr->second.GMDmg;
        }
    }

    return At;
}

float AuraSpellMgr::AutoPVEDmgMod(Player* player, uint32 TarSpellId)
{
    if (BusAuraStore.empty())
        return 1.0f;

    float Mod = 0.0f;
    for (auto Itr = BusAuraStore.begin(); Itr != BusAuraStore.end(); Itr = BusAuraStore.upper_bound(Itr->first))
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            auto Range = BusAuraStore.equal_range(Itr->first);
            for (auto Ator = Range.first; Ator != Range.second; ++Ator)
            {
                if (Ator->second.TarSpellId == TarSpellId)
                {
                    uint32 Count = GetAuraOrLearnedSpellCount(player, Itr->first);
                    for (uint32 N = 0; N < Count; N++)
                        Mod += Ator->second.PVEDmgMod;
                }
            }
        }
    }

    Mod = (Mod <= 0.0f) ? 1.0f : (Mod + 1.0f);
    return Mod;
}

float AuraSpellMgr::AutoPVPDmgMod(Player* player, uint32 TarSpellId)
{
    if (BusAuraStore.empty())
        return 1.0f;

    float Mod = 0.0f;
    for (auto Itr = BusAuraStore.begin(); Itr != BusAuraStore.end(); Itr = BusAuraStore.upper_bound(Itr->first))
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            auto Range = BusAuraStore.equal_range(Itr->first);
            for (auto Ator = Range.first; Ator != Range.second; ++Ator)
            {
                if (Ator->second.TarSpellId == TarSpellId)
                {
                    uint32 Count = GetAuraOrLearnedSpellCount(player, Itr->first);
                    for (uint32 N = 0; N < Count; N++)
                        Mod += Ator->second.PVPDmgMod;
                }
            }
        }
    }

    Mod = (Mod <= 0.0f) ? 1.0f : (Mod + 1.0f);
    return Mod;
}

float AuraSpellMgr::AutoPVEHeMod(Player* player, uint32 TarSpellId)
{
    if (BusAuraStore.empty())
        return 1.0f;

    float Mod = 0.0f;
    for (auto Itr = BusAuraStore.begin(); Itr != BusAuraStore.end(); Itr = BusAuraStore.upper_bound(Itr->first))
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            auto Range = BusAuraStore.equal_range(Itr->first);
            for (auto Ator = Range.first; Ator != Range.second; ++Ator)
            {
                if (Ator->second.TarSpellId == TarSpellId)
                {
                    uint32 Count = GetAuraOrLearnedSpellCount(player, Itr->first);
                    for (uint32 N = 0; N < Count; N++)
                        Mod += Ator->second.PVEHeMod;
                }
            }
        }
    }

    Mod = (Mod <= 0.0f) ? 1.0f : (Mod + 1.0f);
    return Mod;
}

float AuraSpellMgr::AutoPVPHeMod(Player* player, uint32 TarSpellId)
{
    if (BusAuraStore.empty())
        return 1.0f;

    float Mod = 0.0f;
    for (auto Itr = BusAuraStore.begin(); Itr != BusAuraStore.end(); Itr = BusAuraStore.upper_bound(Itr->first))
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            auto Range = BusAuraStore.equal_range(Itr->first);
            for (auto Ator = Range.first; Ator != Range.second; ++Ator)
            {
                if (Ator->second.TarSpellId == TarSpellId)
                {
                    uint32 Count = GetAuraOrLearnedSpellCount(player, Itr->first);
                    for (uint32 N = 0; N < Count; N++)
                        Mod += Ator->second.PVPHeMod;
                }
            }
        }
    }

    Mod = (Mod <= 0.0f) ? 1.0f : (Mod + 1.0f);
    return Mod;
}

Stats AuraSpellMgr::AutoGetStatForId(uint32 Id)
{
    switch (Id)
    {
    case 1: return STAT_STRENGTH;
    case 2: return STAT_AGILITY;
    case 4: return STAT_STAMINA;
    case 8: return STAT_INTELLECT;
    case 16: return STAT_SPIRIT;
    }

    return STAT_STRENGTH;
}

float AuraSpellMgr::GetCallUpStatNow(Player* player, uint32 CheckValue, float StatUpValue)
{
    uint32 Now = 0;
    uint32 Mask[] = { 1, 2, 4, 8, 16, 32 };
    for (auto const& Id : Mask)
    {
        if (CheckValue & Id)
        {
            int32 Val = 0;
            if (Id == 32)
            {
                Val = player->GetRatingBonusValue(CR_HASTE_MELEE);
            }
            else
            {
                Stats St = AutoGetStatForId(Id);
                Val = (int32)player->GetTotalStatValue(St);
            }

            Val = (Val < 0) ? 0 : Val;
            Now += Val;
        }
    }

    if (StatUpValue <= 0.0f)
        StatUpValue = float(Now);

    float Ps = (Now / StatUpValue);
    return Ps;
}

uint32 AuraSpellMgr::AutoAddStatNow(Player* player, uint32 CheckValue, float StatValue)
{
    uint32 Now = 0;
    uint32 Mask[] = { 1, 2, 4, 8, 16, 32 };
    for (auto const& Id : Mask)
    {
        if (CheckValue & Id)
        {
            int32 Val = 0;
            if (Id == 32)
            {
                Val = player->GetRatingBonusValue(CR_HASTE_MELEE);
            }
            else
            {
                Stats St = AutoGetStatForId(Id);
                Val = (int32)player->GetTotalStatValue(St);
            }

            Val = (Val < 0) ? 0 : Val;
            Now += Val;
        }
    }

    return uint32(Now * StatValue);
}

void AuraSpellMgr::HookAutoAddDmg(Player* player, uint32 TarSpellId, uint32& Damage)
{
    if (!Damage)
        return;

    AsInt64 realDamage = AsInt64(Damage);
    HookAutoAddDmg(player, TarSpellId, realDamage);
    Damage = AsInt64ToUInt32Clamp(realDamage, uint32(std::numeric_limits<int32>::max()));
}

void AuraSpellMgr::HookAutoAddDmg(Player* player, uint32 TarSpellId, AsInt64& Damage)
{
    if (BusAuraStore.empty())
        return;

    for (auto Itr = BusAuraStore.begin(); Itr != BusAuraStore.end(); Itr = BusAuraStore.upper_bound(Itr->first))
    {
        if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
        {
            auto Range = BusAuraStore.equal_range(Itr->first);
            for (auto& Ator = Range.first; Ator != Range.second; Ator++)
            {
                if (Ator->second.TarSpellId == TarSpellId)
                {
                    AsInt64 Ap = std::max(player->GetAsTotalAttackPowerValue(BASE_ATTACK), player->GetAsTotalAttackPowerValue(RANGED_ATTACK));
                    AsInt64 Sp = player->GetAsSpellPowerValue(SpellSchoolMask(SPELL_SCHOOL_MASK_MAGIC));
                    AsInt64 StatValue = GetAsStatMaskValue(player, Ator->second.StatMask);

                    uint32 Count = GetAuraOrLearnedSpellCount(player, Itr->first);
                    for (uint32 N = 0; N < Count; N++)
                    {
                        Damage += Ator->second.AddDmg;
                        if (Ator->second.StatMask)
                            Damage = ScaleAsDamageByStatRatio(Damage, StatValue, Ator->second.StatUpValue);
                        Damage += ScaleAuraAsDamage(Sp, Ator->second.AddSPMod);
                        Damage += ScaleAuraAsDamage(Ap, Ator->second.AddAPMod);
                        if (Ator->second.StatMask)
                            Damage += ScaleAuraAsDamage(StatValue, Ator->second.StatValue);
                    }
                }
            }
        }
    }
}

void AuraSpellMgr::OnAuraSpellSetStackAmountHandler(Unit* Caster, Aura* LinkAura, uint8 StackAmount)
{
    {
        auto Itr = AuraStackTriggerMap.find(LinkAura->GetId());
        if (Itr != AuraStackTriggerMap.end())
        {
            if (StackAmount == Itr->second.Stacks)
            {
                if (Itr->second.RemoveStacks)
                {
                    uint8 Now = StackAmount - Itr->second.RemoveStacks;
                    if (Now)
                        LinkAura->SetStackAmount(StackAmount - Itr->second.RemoveStacks);
                    else
                        LinkAura->Remove();
                }
                for (auto It = Itr->second.TriggerSpellVec.begin(); It != Itr->second.TriggerSpellVec.end(); It++)
                    Caster->CastSpell(Caster, *It, true);
            }
        }
    }

    {
        for (auto Itr = LinkAuraStackMap.begin(); Itr != LinkAuraStackMap.end(); Itr++)
        {
            if (LinkAura->GetId() == Itr->LAuraId && Caster->HasAura(Itr->SAuraId) && StackAmount == Itr->Stacks)
            {
                if (Itr->RemoveStacks)
                {
                    uint8 Now = StackAmount - Itr->RemoveStacks;
                    if (Now)
                        LinkAura->SetStackAmount(StackAmount - Itr->RemoveStacks);
                    else
                        LinkAura->Remove();
                }
                for (auto It = Itr->SpellSet.begin(); It != Itr->SpellSet.end(); It++)
                    Caster->CastSpell(Caster, *It, true);
            }
        }
    }
}

void AuraSpellMgr::BusHealXsOnDamage(Unit* Caster, Unit* Target, AsInt64 const& Damage, uint32 CastId)
{
    if (!Caster || !Target || Damage <= 0 || !Caster->IsInWorld() || !Target->IsInWorld() || Caster == Target)
        return;

    if (Caster->ToPlayer())
    {
        AsInt64 const& RealDamage = Damage;
        if (Target->GetTypeId() != TYPEID_PLAYER)  //玩家对单位 表示PVE
        {
            for (auto Itr = BusXsAuraStore.begin(); Itr != BusXsAuraStore.end(); ++Itr)
            {
                AsInt64 Amount = AsInt64Zero();
                if (Caster->ToPlayer()->HasAura(Itr->first) && Caster->ToPlayer()->IsAlive())
                {
                    uint32 Count = Caster->ToPlayer()->GetAtAuraCount(Itr->first);
                    for (uint32 N = 0; N < Count; N++)
                    {
                        Amount += ScaleAuraAsDamage(RealDamage, Itr->second.PVEMod);
                        Amount += Itr->second.PVEValue;
                    }

                    if (Amount && Itr->second.ShowSpellId && Itr->second.ShowSpellId == CastId)
                    {
                        if (SpellInfo const* SInfo = sSpellMgr->GetSpellInfo(Itr->second.ShowSpellId))
                        {
                            AsInt64 effectiveDamage = Target->GetAsTotalHealth();
                            if (effectiveDamage > 0 && Amount > effectiveDamage)
                                Amount = effectiveDamage;
                            uint32 CompatAmount = AsInt64ToUInt32Clamp(Amount, uint32(std::numeric_limits<int32>::max()));
                            Caster->SetAsHealOverride(Caster->GetGUID(), Amount);
                            SendProcHealDisplay(Caster->ToPlayer(), Amount, SInfo);
                            HealInfo HInfo(Caster, Caster, CompatAmount, SInfo, SInfo->GetSchoolMask());
                            Caster->ToPlayer()->HealBySpell(HInfo);
                        }
                    }
                }
            }
        }
        else if (Target->GetTypeId() == TYPEID_PLAYER)  //玩家对玩家 表示PVP
        {
            for (auto Itr = BusXsAuraStore.begin(); Itr != BusXsAuraStore.end(); ++Itr)
            {
                AsInt64 Amount = AsInt64Zero();
                if (Caster->ToPlayer()->HasAura(Itr->first) && Caster->ToPlayer()->IsAlive())
                {
                    uint32 Count = Caster->ToPlayer()->GetAtAuraCount(Itr->first);
                    for (uint32 N = 0; N < Count; N++)
                    {
                        Amount += ScaleAuraAsDamage(RealDamage, Itr->second.PVPMod);
                        Amount += Itr->second.PVPValue;
                    }

                    if (Amount && Itr->second.ShowSpellId && Itr->second.ShowSpellId == CastId)
                    {
                        if (SpellInfo const* SInfo = sSpellMgr->GetSpellInfo(Itr->second.ShowSpellId))
                        {
                            AsInt64 effectiveDamage = Target->GetAsTotalHealth();
                            if (effectiveDamage > 0 && Amount > effectiveDamage)
                                Amount = effectiveDamage;
                            uint32 CompatAmount = AsInt64ToUInt32Clamp(Amount, uint32(std::numeric_limits<int32>::max()));
                            Caster->SetAsHealOverride(Caster->GetGUID(), Amount);
                            SendProcHealDisplay(Caster->ToPlayer(), Amount, SInfo);
                            HealInfo HInfo(Caster, Caster, CompatAmount, SInfo, SInfo->GetSchoolMask());
                            Caster->ToPlayer()->HealBySpell(HInfo);
                        }
                    }
                }
            }
        }
    }
}

class SpellModPlayerScripts : PlayerScript
{
public:
    SpellModPlayerScripts() : PlayerScript("SpellModPlayerScripts") {}

    void OnUpdateZone(Player* player, uint32 newZone, uint32 newArea) override
    {
        for (auto iter = SpellModMap.begin(); iter != SpellModMap.end(); ++iter)
            if (player->HasAura(iter->first))
                if (!sAuraSpellMgr->Enable(player, iter->first))
                    player->RemoveAurasDueToSpell(iter->first);
    }

    void OnCreatureKill(Player* player, Creature* Killed) override
    {
        if (OnAsStore.empty())
            return;

        for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
        {
            if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
            {
                float Chance = 0.0f;
                uint32 Count = player->GetAtAuraCount(Itr->first);
                for (uint32 N = 0; N < Count; N++)
                    Chance += Itr->second.LootChance;

                if (roll_chance_f(Chance) && Itr->second.LootId)
                {
                    Loot* OnLoot = &Killed->loot;
                    OnLoot->FillLoot(Itr->second.LootId, LootTemplates_Creature, player, false, false, Killed->GetLootMode());
                }
            }
        }
    }
};

class CustomSpellHookAbsorb : public UnitScript
{
public:
    CustomSpellHookAbsorb() : UnitScript("CustomSpellHookAbsorb") { }

    void OnScriptEffectAbsorbEnd(Unit* Caster, Unit* UnitOwner, AuraEffect const* AEffect, int32& Amount) override
    {
        if (!Caster->IsInWorld() || !UnitOwner->IsInWorld())
            return;

        auto Itr = SpellAbsorbStore.find(AEffect->GetId());
        if (Itr == SpellAbsorbStore.end())
            return;

        Amount = 0;

        if (Itr->second.NowAbsorb > 0)
        {
            Amount = Itr->second.NowAbsorb;
            return;
        }

        AsInt64 asAmount = AsInt64(Itr->second.AddAbsorb);

        AsInt64 Ap = std::max(UnitOwner->GetAsTotalAttackPowerValue(BASE_ATTACK), UnitOwner->GetAsTotalAttackPowerValue(RANGED_ATTACK));
        if (Ap < 0)
            Ap = AsInt64Zero();

        AsInt64 Sp = UnitOwner->GetAsSpellPowerValue(SPELL_SCHOOL_MASK_MAGIC);
        if (Sp < 0)
            Sp = AsInt64Zero();

        asAmount += ScaleAuraAsDamage(Ap, Itr->second.APMod);
        asAmount += ScaleAuraAsDamage(Sp, Itr->second.SPMod);

        if (!Itr->second.LinkSpells.empty())
        {
            AsInt64 addCount = AsInt64Zero();

            for (auto It = Itr->second.LinkSpells.begin(); It != Itr->second.LinkSpells.end(); ++It)
            {
                uint32 SpellId = *It;
                if (UnitOwner->HasAura(SpellId) || UnitOwner->HasSpell(SpellId))
                    addCount += ScaleAuraAsDamage(asAmount, Itr->second.LinkMod);
            }

            if (addCount > 0)
                asAmount = addCount;
        }

        Amount = AsInt64ToInt32Clamp(asAmount, std::numeric_limits<int32>::max());
    }
};

void AddSC_CustomSpellScripts()
{
    new SpellModPlayerScripts();
    new CustomSpellHookAbsorb();
}
