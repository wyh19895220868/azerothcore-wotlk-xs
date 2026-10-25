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
#include "CombatPetMgr.h"
#include <Time\GameTime.h>
#include <Custom/ItemFuncMgr/ItemFuncMgr.h>
#pragma execution_character_set("utf-8")

CPetTempVect CPetTempStore;
CPetGasMap CPetGasStore;
CPetUseMap CPetUseStore;
CPetGroupRankVect CPetGroupRankStore;

namespace
{
AsInt64 ScaleCombatPetAsValue(AsInt64 const& value, float modifier)
{
    if (value <= 0)
        return AsInt64Zero();

    int64 scale = int64(modifier * 10000.0f + (modifier >= 0.0f ? 0.5f : -0.5f));
    return (value * scale) / 10000;
}

AsInt64 GetCombatPetAsMaxHp(uint32 playerLevel, CreatureTemplate const* cInfo, uint32 creEntry, uint32 rank)
{
    AsInt64 maxHp = AsInt64(1);
    if (!cInfo)
        return maxHp;

    if (PetLevelInfo const* petInfo = sObjectMgr->GetPetLevelInfo(creEntry, playerLevel))
        maxHp = petInfo->asHealth > 0 ? petInfo->asHealth : AsInt64(petInfo->health);
    else if (CreatureBaseStats const* baseStats = sObjectMgr->GetCreatureBaseStats(playerLevel, cInfo->unit_class))
    {
        maxHp = baseStats->AsBaseHealth[cInfo->expansion] > 0 ? baseStats->AsBaseHealth[cInfo->expansion] : AsInt64(baseStats->BaseHealth[cInfo->expansion]);
        maxHp = ScaleCombatPetAsValue(maxHp, std::min(1.0f, cInfo->ModHealth));
    }

    if (maxHp < 1)
        maxHp = 1;

    uint32 stTempId = sCombatPetMgr->GetStTempIdForCombatPetRank(creEntry, rank);
    auto itr = AtCreatureStatTempStore.find(stTempId);
    if (itr != AtCreatureStatTempStore.end())
    {
        if (!AsInt64IsZero(itr->second.NowHp))
            maxHp = itr->second.NowHp;
        else
            maxHp = ScaleCombatPetAsValue(maxHp, itr->second.HealthMultiplier);
    }

    return maxHp > 0 ? maxHp : AsInt64(1);
}

CPetTemp const* FindCombatPetTemp(uint32 creEntry, uint32 rank)
{
    for (CPetTemp const& temp : CPetTempStore)
        if (temp.CreId == creEntry && temp.Rank == rank)
            return &temp;

    return nullptr;
}

void ResetCombatPetFitBonusState(Player* player)
{
    player->CombatPetFitAppliedIndex = 0;
    player->CombatPetFitAppliedHealth = AsInt64Zero();
    player->CombatPetFitAppliedStrength = 0;
    player->CombatPetFitAppliedAgility = 0;
    player->CombatPetFitAppliedIntellect = 0;
    player->CombatPetFitAppliedSpirit = 0;
    player->CombatPetFitAppliedAuras.clear();
}

bool ApplyCombatPetFitBonus(Player* player, uint32 createIndex)
{
    if (!player || !createIndex)
        return false;

    if (player->CombatPetFitAppliedIndex)
        return player->CombatPetFitAppliedIndex == createIndex;

    auto petItr = player->CombatPetStStore.find(createIndex);
    if (petItr == player->CombatPetStStore.end() || !petItr->second.CreEntry)
        return false;

    CPetTemp const* temp = FindCombatPetTemp(petItr->second.CreEntry, petItr->second.Rank);
    CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(petItr->second.CreEntry);
    if (!temp || !cInfo)
        return false;

    AsInt64 maxHp = GetCombatPetAsMaxHp(player->GetLevel(), cInfo, petItr->second.CreEntry, petItr->second.Rank);
    player->CombatPetFitAppliedIndex = createIndex;
    player->CombatPetFitAppliedHealth = ScaleCombatPetAsValue(maxHp, temp->FitMod);
    player->CombatPetFitAppliedStrength = int32(temp->Stat1 * temp->FitMod);
    player->CombatPetFitAppliedAgility = int32(temp->Stat2 * temp->FitMod);
    player->CombatPetFitAppliedIntellect = int32(temp->Stat3 * temp->FitMod);
    player->CombatPetFitAppliedSpirit = int32(temp->Stat4 * temp->FitMod);

    player->DoApplyStat(ITEM_MOD_HEALTH, player->CombatPetFitAppliedHealth);
    player->DoApplyStat(ITEM_MOD_STRENGTH, player->CombatPetFitAppliedStrength);
    player->DoApplyStat(ITEM_MOD_AGILITY, player->CombatPetFitAppliedAgility);
    player->DoApplyStat(ITEM_MOD_INTELLECT, player->CombatPetFitAppliedIntellect);
    player->DoApplyStat(ITEM_MOD_SPIRIT, player->CombatPetFitAppliedSpirit);

    for (uint32 auraId : temp->FitAuras)
    {
        if (sSpellMgr->GetSpellInfo(auraId) && !player->HasAura(auraId) && player->AddAura(auraId, player))
            player->CombatPetFitAppliedAuras.insert(auraId);
    }

    return true;
}

bool RemoveCombatPetFitBonus(Player* player, uint32 expectedIndex)
{
    if (!player || !player->CombatPetFitAppliedIndex)
        return true;

    if (expectedIndex && player->CombatPetFitAppliedIndex != expectedIndex)
    {
        LOG_ERROR("server.worldserver", "战宠合体属性撤销失败: 玩家 {} 记录槽位 {} 与请求槽位 {} 不一致", player->GetGUIDLow(), player->CombatPetFitAppliedIndex, expectedIndex);
        return false;
    }

    player->DoApplyStat(ITEM_MOD_HEALTH, player->CombatPetFitAppliedHealth, false);
    player->DoApplyStat(ITEM_MOD_STRENGTH, player->CombatPetFitAppliedStrength, false);
    player->DoApplyStat(ITEM_MOD_AGILITY, player->CombatPetFitAppliedAgility, false);
    player->DoApplyStat(ITEM_MOD_INTELLECT, player->CombatPetFitAppliedIntellect, false);
    player->DoApplyStat(ITEM_MOD_SPIRIT, player->CombatPetFitAppliedSpirit, false);

    for (uint32 auraId : player->CombatPetFitAppliedAuras)
        if (player->HasAura(auraId))
            player->RemoveAurasDueToSpell(auraId);

    ResetCombatPetFitBonusState(player);
    return true;
}
}

void CombatPetMgr::LoadCombatPetData()
{
    //记录 授权验证
    if (!sHelper->InCheckFunc(FuncPet))
    {
        //LOG_ERROR("server.worldserver", ">> 没有授权 加载 战宠相关的表");
        return;
    }

    CPetTempStore.clear();
    std::set<std::pair<uint32, uint32>> loadedCombatPetTemps;
    QueryResult Result = WorldDatabase.Query("SELECT 生物编号, 宠物阶级, 所需经验, 资质评分, 属性模版, 力量, 敏捷, 智力, 精神, 技能1, 技能2, 技能3, 技能4, 技能5, 技能6, 技能7, 技能8, 合体光环集合, 合体加成比例 FROM _战宠基础配置表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            CPetTemp St;
            St.CreId = Fields[0].Get<uint32>();
            St.Rank = Fields[1].Get<uint32>();
            if (!loadedCombatPetTemps.insert(std::make_pair(St.CreId, St.Rank)).second)
            {
                LOG_ERROR("server.worldserver", "[_战宠基础配置表] 存在重复配置，已忽略: 生物编号 {} 宠物阶级 {}", St.CreId, St.Rank);
                continue;
            }

            St.NeedXp = Fields[2].Get<uint32>();
            St.Score = Fields[3].Get<uint64>();
            St.StatTempId = Fields[4].Get<uint32>();
            St.Stat1 = Fields[5].Get<uint32>();
            St.Stat2 = Fields[6].Get<uint32>();
            St.Stat3 = Fields[7].Get<uint32>();
            St.Stat4 = Fields[8].Get<uint32>();

            for (int8 i = 0; i < MaxSpellCount; i++)
                St.SpellIds[i] = Fields[i + 9].Get<uint32>();

            Tokenizer tokens(Fields[17].Get<std::string>(), '#');
            for (Tokenizer::const_iterator itr = tokens.begin(); itr != tokens.end(); ++itr)
                St.FitAuras.insert(uint32(atol(*itr)));

            St.FitMod = Fields[18].Get<float>();

            CPetTempStore.push_back(St);
        } while (Result->NextRow());
    }

    CPetGasStore.clear();
    Result = WorldDatabase.Query("SELECT 怪物或物品编号, 直达阶级, 奖励经验, 启用加1级 FROM _战宠经验奖励表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ObjId = Fields[0].Get<uint32>();
            CPetGasTemp St;
            St.ToRank = Fields[1].Get<uint32>();
            St.RewXp = Fields[2].Get<uint32>();
            St.AddRank = Fields[3].Get<bool>();
            CPetGasStore.insert(std::make_pair(ObjId, St));

        } while (Result->NextRow());
    }

    CPetUseStore.clear();
    Result = WorldDatabase.Query("SELECT 使用物品编号, 宠物编号, 阶级组 FROM _战宠蛋使用配置表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 UseItemId = Fields[0].Get<uint32>();
            CPetUseTemp St;
            St.CreEntry = Fields[1].Get<uint32>();
            St.GroupRank = Fields[2].Get<uint32>();
            CPetUseStore.insert(std::make_pair(UseItemId, St));

        } while (Result->NextRow());
    }

    CPetGroupRankStore.clear();
    Result = WorldDatabase.Query("SELECT 阶级组号, 直接阶级, 概率 FROM _战宠蛋阶级组表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            CPetGroupRankTemp St;
            St.GroupId = Fields[0].Get<uint32>();
            St.ToRank = Fields[1].Get<uint32>();
            St.GroupOdds = Fields[2].Get<float>();
            CPetGroupRankStore.push_back(St);

        } while (Result->NextRow());
    }
}

bool CombatPetMgr::IsCombatPetData(Creature* Me)
{
    float In = false;
    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
    {
        if (Itr->CreId == Me->GetEntry())
        {
            In = true;
            break;
        }
    }

    return In;
}

bool CombatPetMgr::IsCombatPetData(uint32 CreEntry)
{
    float In = false;
    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
    {
        if (Itr->CreId == CreEntry)
        {
            In = true;
            break;
        }
    }

    return In;
}

uint32 CombatPetMgr::GetCombatPetNowRankByIndex(Player* player)
{
    uint32 Index = player->GetTempAtUInt32(AT7001);
    auto Itep = player->CombatPetStStore.find(Index);
    if (Itep == player->CombatPetStStore.end())
        return 0;

    if (Itep->second.CreEntry > 0)
        return Itep->second.Rank;

    return 0;
}

uint32 CombatPetMgr::GetCombatPetMaxRank(Creature* Me)
{
    uint32 Max = 0;
    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
    {
        if (Itr->CreId == Me->GetEntry() && Itr->Rank > Max)
        {
            Max = Itr->Rank;
        }
    }

    return Max;
}

uint32 CombatPetMgr::GetCombatPetNextXp(uint32 CreEntry, uint32 NowRank)
{
    uint32 NeedXp = 0;
    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
    {
        if (Itr->CreId == CreEntry && Itr->Rank == (NowRank + 1))
        {
            NeedXp = Itr->NeedXp;
        }
    }

    return NeedXp;
}

uint32 CombatPetMgr::GetStTempIdForCombatPetRank(Creature* Me)
{
    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
        if (Itr->CreId == Me->GetEntry() && Itr->Rank == Me->CombatPetRank)
            return Itr->StatTempId;

    return 0;
}

uint32 CombatPetMgr::GetStTempIdForCombatPetRank(uint32 CreEntry, uint32 NowRank)
{
    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
        if (Itr->CreId == CreEntry && Itr->Rank == NowRank)
            return Itr->StatTempId;

    return 0;
}

std::pair<uint32, uint32> CombatPetMgr::GetCreateSumPetEntryAndRank(uint32 UseItemId)
{
    std::pair<uint32, uint32> Result;
    Result.first = 0;
    Result.second = 0;

    auto Itr = CPetUseStore.find(UseItemId);
    if (Itr == CPetUseStore.end())
        return Result;

    if (!Itr->second.GroupRank)
    {
        Result.first = Itr->second.CreEntry;
        Result.second = 0;
        return Result;
    }

    struct ChanceTemp { uint32 Rank; float Chance; };
    std::vector<ChanceTemp> ChanceList;
    ChanceList.clear();

    for (auto It = CPetGroupRankStore.begin(); It != CPetGroupRankStore.end(); It++)
    {
        if (It->GroupId == Itr->second.GroupRank)
        {
            ChanceTemp St;
            St.Rank = It->ToRank;
            St.Chance = It->GroupOdds;
            ChanceList.push_back(St);
        }
    }

    if (!ChanceList.empty())
    {
        float RIdx = 0.0f;
        for (auto it = ChanceList.begin(); it != ChanceList.end(); it++)
            RIdx += it->Chance;

        for (auto It = ChanceList.begin(); It != ChanceList.end(); It++)
        {
            if (frand(0, RIdx) <= It->Chance)
            {
                Result.first = Itr->second.CreEntry;
                Result.second = It->Rank;
                break;
            }
            else
                RIdx -= It->Chance;
        }

        ChanceList.clear();
    }

    return Result;
}

uint64 CombatPetMgr::GetCombatPetScore(uint32 CreEntry, uint32 NowRank)
{
    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
        if (Itr->CreId == CreEntry && Itr->Rank == NowRank)
            return Itr->Score;

    return 0;
}

bool CombatPetMgr::CreateCombatPet(Player* player, uint32 CreateIndex, uint32 CreEntry, uint32 ToRank)
{
    Map* NowMap = player->GetMap();
    if (!NowMap)
        return false;

    Creature* OldCre = player->GetWorldSoltIndexCombatPet();
    if (OldCre)
    {
        OldCre->CombatStop();
        if (TempSummon* summon = OldCre->ToTempSummon())
        summon->UnSummon();
    }

    SummonPropertiesEntry const* SProperties = sSummonPropertiesStore.LookupEntry(67);
    if (!SProperties)
        return false;

    TempSummon* NewCre = NowMap->SummonCreature(CreEntry, player->GetPosition(), SProperties, 0, player);
    if (!NewCre)
        return false;

    NewCre->AttackStop();
    NewCre->InterruptNonMeleeSpells(false);
    NewCre->ClearInPetCombat();
    NewCre->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST, NewCre->GetFollowAngle());

    CharmInfo* CmInfo = NewCre->GetCharmInfo();
    if (CmInfo)
    {
        CmInfo->SetCommandState(COMMAND_FOLLOW);
        CmInfo->SetIsCommandAttack(false);
        CmInfo->SetIsAtStay(false);
        CmInfo->SetIsReturning(true);
        CmInfo->SetIsCommandFollow(true);
        CmInfo->SetIsFollowing(false);
        CmInfo->RemoveStayPosition();
        CmInfo->SetForcedSpell(0);
        CmInfo->SetForcedTargetGUID();

        NewCre->SetReactState(REACT_DEFENSIVE);
    }

    //指定好等级后
    NewCre->CombatPetRank = ToRank;

    //处理技能栏
    NewCre->m_CreatureSpellCooldowns.clear();
    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
    {
        if (Itr->CreId == NewCre->GetEntry() && Itr->Rank == NewCre->CombatPetRank)
        {
            NewCre->SetCreateStat(STAT_STRENGTH, Itr->Stat1);
            NewCre->SetCreateStat(STAT_AGILITY, Itr->Stat2);
            NewCre->SetCreateStat(STAT_INTELLECT, Itr->Stat3);
            NewCre->SetCreateStat(STAT_SPIRIT, Itr->Stat4);

            for (int8 i = 0; i < MaxSpellCount; i++)
                NewCre->m_spells[i] = Itr->SpellIds[i];
        }
    }
    CmInfo->InitCharmCreateSpells();
    player->CharmSpellInitialize();

    NewCre->StTempId = sCombatPetMgr->GetStTempIdForCombatPetRank(NewCre);
    NewCre->SelectLevel(false);

    player->ResetPetEntry = 0;
    NewCre->Say(sHelper->GetGts(9501), LANG_UNIVERSAL);
    uint64 PetScore = GetCombatPetScore(CreEntry, ToRank);
    player->SendSysMsg(sHelper->GetGts(9502), NewCre->GetName().c_str(), ToRank, PetScore);

    auto Itep = player->CombatPetStStore.find(CreateIndex);
    if (Itep != player->CombatPetStStore.end())
    {
        Itep->second.CreEntry = CreEntry;
        Itep->second.Rank = ToRank;
        Itep->second.NowXp = 0;
    }

    CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_COMBAT_PET_DATA);
    Utmt->SetData(0, player->GetGUIDLow());
    Utmt->SetData(1, CreateIndex);
    Utmt->SetData(2, CreEntry);
    Utmt->SetData(3, ToRank);
    Utmt->SetData(4, 0);
    CharacterDatabase.Execute(Utmt);

    player->UpdateTempAt(AT7001, CreateIndex);
    AutoFillCombatPetPlayerDataOne(player, CreateIndex);

    return true;
}

void CombatPetMgr::AddonToggleSummonComCurrentbatPet(Player* player, uint32 CreateIndex)
{
    auto Itep = player->CombatPetStStore.find(CreateIndex);
    if (Itep == player->CombatPetStStore.end())
        return;

    uint32 FitIndex = player->GetTempAtUInt32(AT7002);
    if (FitIndex)
    {
        player->SendSysMsg(sHelper->GetGts(9513));
        return;
    }

    if (!Itep->second.CreEntry)
        return;

    Map* NowMap = player->GetMap();
    if (!NowMap)
        return;

    Creature* OldCre = player->GetWorldSoltIndexCombatPet();
    if (OldCre)
    {
        player->SendSysMsg(sHelper->GetGts(9512));
        return;
    }

    SummonPropertiesEntry const* SProperties = sSummonPropertiesStore.LookupEntry(67);
    if (!SProperties)
        return;

    TempSummon* NewCre = NowMap->SummonCreature(Itep->second.CreEntry, player->GetPosition(), SProperties, 0, player);
    if (!NewCre)
        return;

    NewCre->AttackStop();
    NewCre->InterruptNonMeleeSpells(false);
    NewCre->ClearInPetCombat();
    NewCre->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST, NewCre->GetFollowAngle());

    CharmInfo* CmInfo = NewCre->GetCharmInfo();
    if (CmInfo)
    {
        CmInfo->SetCommandState(COMMAND_FOLLOW);
        CmInfo->SetIsCommandAttack(false);
        CmInfo->SetIsAtStay(false);
        CmInfo->SetIsReturning(true);
        CmInfo->SetIsCommandFollow(true);
        CmInfo->SetIsFollowing(false);
        CmInfo->RemoveStayPosition();
        CmInfo->SetForcedSpell(0);
        CmInfo->SetForcedTargetGUID();

        NewCre->SetReactState(REACT_DEFENSIVE);
    }

    //指定好等级后
    NewCre->CombatPetRank = Itep->second.Rank;

    //处理技能栏
    NewCre->m_CreatureSpellCooldowns.clear();
    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
    {
        if (Itr->CreId == NewCre->GetEntry() && Itr->Rank == NewCre->CombatPetRank)
        {
            NewCre->SetCreateStat(STAT_STRENGTH, Itr->Stat1);
            NewCre->SetCreateStat(STAT_AGILITY, Itr->Stat2);
            NewCre->SetCreateStat(STAT_INTELLECT, Itr->Stat3);
            NewCre->SetCreateStat(STAT_SPIRIT, Itr->Stat4);

            for (int8 i = 0; i < MaxSpellCount; i++)
                NewCre->m_spells[i] = Itr->SpellIds[i];
        }
    }
    CmInfo->InitCharmCreateSpells();
    player->CharmSpellInitialize();

    NewCre->StTempId = sCombatPetMgr->GetStTempIdForCombatPetRank(NewCre);
    NewCre->SelectLevel(false);

    player->ResetPetEntry = 0;
    player->UpdateTempAt(AT7001, CreateIndex);
}

void CombatPetMgr::ResetSummonComCurrentbatPet(Player* player)
{
    if (!player->ResetPetEntry)
        return;

    Map* NowMap = player->GetMap();
    if (!NowMap)
        return;

    Creature* OldCre = player->GetWorldSoltIndexCombatPet();
    if (OldCre)
    {
        OldCre->CombatStop();
        if (TempSummon* summon = OldCre->ToTempSummon())
        summon->UnSummon();
    }

    SummonPropertiesEntry const* SProperties = sSummonPropertiesStore.LookupEntry(67);
    if (!SProperties)
        return;

    TempSummon* NewCre = NowMap->SummonCreature(player->ResetPetEntry, player->GetPosition(), SProperties, 0, player);
    if (!NewCre)
        return;

    NewCre->AttackStop();
    NewCre->InterruptNonMeleeSpells(false);
    NewCre->ClearInPetCombat();
    NewCre->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST, NewCre->GetFollowAngle());

    CharmInfo* CmInfo = NewCre->GetCharmInfo();
    if (CmInfo)
    {
        CmInfo->SetCommandState(COMMAND_FOLLOW);
        CmInfo->SetIsCommandAttack(false);
        CmInfo->SetIsAtStay(false);
        CmInfo->SetIsReturning(true);
        CmInfo->SetIsCommandFollow(true);
        CmInfo->SetIsFollowing(false);
        CmInfo->RemoveStayPosition();
        CmInfo->SetForcedSpell(0);
        CmInfo->SetForcedTargetGUID();

        NewCre->SetReactState(REACT_DEFENSIVE);
    }

    //指定好等级后
    uint32 NowRank = GetCombatPetNowRankByIndex(player);
    NewCre->CombatPetRank = NowRank;

    //处理技能栏
    NewCre->m_CreatureSpellCooldowns.clear();
    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
    {
        if (Itr->CreId == NewCre->GetEntry() && Itr->Rank == NewCre->CombatPetRank)
        {
            NewCre->SetCreateStat(STAT_STRENGTH, Itr->Stat1);
            NewCre->SetCreateStat(STAT_AGILITY, Itr->Stat2);
            NewCre->SetCreateStat(STAT_INTELLECT, Itr->Stat3);
            NewCre->SetCreateStat(STAT_SPIRIT, Itr->Stat4);

            for (int8 i = 0; i < MaxSpellCount; i++)
                NewCre->m_spells[i] = Itr->SpellIds[i];
        }
    }
    CmInfo->InitCharmCreateSpells();
    player->CharmSpellInitialize();

    NewCre->StTempId = sCombatPetMgr->GetStTempIdForCombatPetRank(NewCre);
    NewCre->SelectLevel(false);

    player->ResetPetEntry = 0;
}

void CombatPetMgr::LoginSummonComCurrentbatPet(Player* player)
{
    if (player->CombatPetStStore.empty())
        return;

    uint32 Index = player->GetTempAtUInt32(AT7001);
    auto Itep = player->CombatPetStStore.find(Index);
    if (Itep == player->CombatPetStStore.end())
        return;

    if (!Itep->second.CreEntry)
        return;

    Map* NowMap = player->GetMap();
    if (!NowMap)
        return;

    Creature* OldCre = player->GetWorldSoltIndexCombatPet();
    if (OldCre)
    {
        OldCre->CombatStop();
        if (TempSummon* summon = OldCre->ToTempSummon())
        summon->UnSummon();
    }

    SummonPropertiesEntry const* SProperties = sSummonPropertiesStore.LookupEntry(67);
    if (!SProperties)
        return;

    TempSummon* NewCre = NowMap->SummonCreature(Itep->second.CreEntry, player->GetPosition(), SProperties, 0, player);
    if (!NewCre)
        return;

    NewCre->AttackStop();
    NewCre->InterruptNonMeleeSpells(false);
    NewCre->ClearInPetCombat();
    NewCre->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST, NewCre->GetFollowAngle());

    CharmInfo* CmInfo = NewCre->GetCharmInfo();
    if (CmInfo)
    {
        CmInfo->SetCommandState(COMMAND_FOLLOW);
        CmInfo->SetIsCommandAttack(false);
        CmInfo->SetIsAtStay(false);
        CmInfo->SetIsReturning(true);
        CmInfo->SetIsCommandFollow(true);
        CmInfo->SetIsFollowing(false);
        CmInfo->RemoveStayPosition();
        CmInfo->SetForcedSpell(0);
        CmInfo->SetForcedTargetGUID();

        NewCre->SetReactState(REACT_DEFENSIVE);
    }

    //指定好等级后
    uint32 NowRank = GetCombatPetNowRankByIndex(player);
    NewCre->CombatPetRank = NowRank;

    //处理技能栏
    NewCre->m_CreatureSpellCooldowns.clear();
    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
    {
        if (Itr->CreId == NewCre->GetEntry() && Itr->Rank == NewCre->CombatPetRank)
        {
            NewCre->SetCreateStat(STAT_STRENGTH, Itr->Stat1);
            NewCre->SetCreateStat(STAT_AGILITY, Itr->Stat2);
            NewCre->SetCreateStat(STAT_INTELLECT, Itr->Stat3);
            NewCre->SetCreateStat(STAT_SPIRIT, Itr->Stat4);

            for (int8 i = 0; i < MaxSpellCount; i++)
                NewCre->m_spells[i] = Itr->SpellIds[i];
        }
    }
    CmInfo->InitCharmCreateSpells();
    player->CharmSpellInitialize();

    NewCre->StTempId = sCombatPetMgr->GetStTempIdForCombatPetRank(NewCre);
    NewCre->SelectLevel(false);

    player->ResetPetEntry = 0;
    AutoFillCombatPetPlayerDataOne(player, Index);
}

void CombatPetMgr::ApartSummonComCurrentbatPet(Player* player, uint32 AddonIndex)
{
    if (player->CombatPetStStore.empty())
        return;

    uint32 CreateIndex = player->GetTempAtUInt32(AT7002);
    if (!CreateIndex)
        return;

    if (AddonIndex != CreateIndex)
    {
        player->SendSysMsg(sHelper->GetGts(9519));
        return;
    }

    auto Itep = player->CombatPetStStore.find(AddonIndex);
    if (Itep == player->CombatPetStStore.end())
        return;

    if (!Itep->second.CreEntry)
        return;

    Map* NowMap = player->GetMap();
    if (!NowMap)
        return;

    Creature* OldCre = player->GetWorldSoltIndexCombatPet();
    if (OldCre)
    {
        OldCre->CombatStop();
        if (TempSummon* summon = OldCre->ToTempSummon())
            summon->UnSummon();
    }

    SummonPropertiesEntry const* SProperties = sSummonPropertiesStore.LookupEntry(67);
    if (!SProperties)
        return;

    TempSummon* NewCre = NowMap->SummonCreature(Itep->second.CreEntry, player->GetPosition(), SProperties, 0, player);
    if (!NewCre)
        return;

    NewCre->AttackStop();
    NewCre->InterruptNonMeleeSpells(false);
    NewCre->ClearInPetCombat();
    NewCre->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST, NewCre->GetFollowAngle());

    CharmInfo* CmInfo = NewCre->GetCharmInfo();
    if (CmInfo)
    {
        CmInfo->SetCommandState(COMMAND_FOLLOW);
        CmInfo->SetIsCommandAttack(false);
        CmInfo->SetIsAtStay(false);
        CmInfo->SetIsReturning(true);
        CmInfo->SetIsCommandFollow(true);
        CmInfo->SetIsFollowing(false);
        CmInfo->RemoveStayPosition();
        CmInfo->SetForcedSpell(0);
        CmInfo->SetForcedTargetGUID();

        NewCre->SetReactState(REACT_DEFENSIVE);
    }

    //指定好等级后
    uint32 NowRank = Itep->second.Rank;
    NewCre->CombatPetRank = NowRank;

    //处理技能栏
    NewCre->m_CreatureSpellCooldowns.clear();
    for (auto Itrbc = CPetTempStore.begin(); Itrbc != CPetTempStore.end(); ++Itrbc)
    {
        if (Itrbc->CreId == NewCre->GetEntry() && Itrbc->Rank == NewCre->CombatPetRank)
        {
            NewCre->SetCreateStat(STAT_STRENGTH, Itrbc->Stat1);
            NewCre->SetCreateStat(STAT_AGILITY, Itrbc->Stat2);
            NewCre->SetCreateStat(STAT_INTELLECT, Itrbc->Stat3);
            NewCre->SetCreateStat(STAT_SPIRIT, Itrbc->Stat4);

            for (int8 i = 0; i < MaxSpellCount; i++)
                NewCre->m_spells[i] = Itrbc->SpellIds[i];
        }
    }
    CmInfo->InitCharmCreateSpells();
    player->CharmSpellInitialize();

    NewCre->StTempId = sCombatPetMgr->GetStTempIdForCombatPetRank(NewCre);
    NewCre->SelectLevel(false);

    player->ResetPetEntry = 0;
    if (!RemoveCombatPetFitBonus(player, CreateIndex))
    {
        if (TempSummon* summon = NewCre->ToTempSummon())
            summon->UnSummon();
        return;
    }

    player->UpdateTempAt(AT7001, CreateIndex);
    player->UpdateTempAt(AT7002, 0);

    sDoAddon->SendPacketToClient(player, "DoApartCombatPetNow", std::to_string(AddonIndex));
}

void CombatPetMgr::RemoveCombatPetMapWorldForWithinDistRange(Player* player)
{
    Creature* Cre = player->GetWorldSoltIndexCombatPet();
    if (!Cre)
        return;

    if (Cre && !Cre->IsWithinDist3d(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetMap()->GetVisibilityRange()))
    {
        uint32 CreEntry = Cre->GetEntry();
        Cre->CombatStop();
        if (TempSummon* summon = Cre->ToTempSummon())
    summon->UnSummon();
        player->ResetPetEntry = CreEntry;
    }
}

void CombatPetMgr::RemoveCombatPetMapWorldForWithinDistRangePossessed(Player* player)
{
    Creature* Cre = player->GetWorldSoltIndexCombatPet();
    if (!Cre)
        return;

    if (Cre && !Cre->IsWithinDistInMap(player, player->GetMap()->GetVisibilityRange()) && !Cre->isPossessed())
    {
        uint32 CreEntry = Cre->GetEntry();
        Cre->CombatStop();
        if (TempSummon* summon = Cre->ToTempSummon())
            summon->UnSummon();
        player->ResetPetEntry = CreEntry;
    }
}

void CombatPetMgr::ActiveHandleDismissCritter(Player* player, uint32 AddonIndex)
{
    Creature* Cre = player->GetWorldSoltIndexCombatPet();
    if (!Cre)
        return;

    auto Itep = player->CombatPetStStore.find(AddonIndex);
    if (Itep == player->CombatPetStStore.end())
        return;

    if (Itep->second.CreEntry != Cre->GetEntry())
    {
        player->SendSysMsg(sHelper->GetGts(9517));
        return;
    }

    Cre->CombatStop();
    if (TempSummon* summon = Cre->ToTempSummon())
        summon->UnSummon();

    player->UpdateTempAt(AT7001, 0);
}

bool CombatPetMgr::HasCreateNowIndex(Player* player)
{
    bool Has = false;
    for (auto Itr = player->CombatPetStStore.begin(); Itr != player->CombatPetStStore.end(); ++Itr)
    {
        if (Itr->second.CreEntry)
        {
            Has = true;
            break;
        }
    }
    return Has;
}

uint32 CombatPetMgr::GetCreateNowIndex(Player* player)
{
    uint32 CreateIndex = 0;
    for (auto Itr = player->CombatPetStStore.begin(); Itr != player->CombatPetStStore.end(); ++Itr)
    {
        if (!Itr->second.CreEntry)
        {
            CreateIndex = Itr->first;
            break;
        }
    }
    return CreateIndex;
}

uint32 CombatPetMgr::GetPlayerDataPetCount(Player* player)
{
    uint32 Count = 0;
    for (auto Itr = player->CombatPetStStore.begin(); Itr != player->CombatPetStStore.end(); ++Itr)
    {
        if (Itr->second.CreEntry > 0)
        {
            Count++;
        }
    }
    return Count;
}

void CombatPetMgr::ActiveDeleteDB(Player* player, uint32 AddonIndex)
{
    uint32 FitIndex = player->GetTempAtUInt32(AT7002);
    if (FitIndex > 0 && AddonIndex == FitIndex)
    {
        if (!RemoveCombatPetFitBonus(player, FitIndex))
            RemoveCombatPetFitBonus(player, 0);
        player->UpdateTempAt(AT7002, 0);
    }

    uint32 CreateIndex = player->GetTempAtUInt32(AT7001);
    if (AddonIndex == CreateIndex)
    {
        Creature* Cre = player->GetWorldSoltIndexCombatPet();
        if (Cre)
        {
            Cre->CombatStop();
            if (TempSummon* summon = Cre->ToTempSummon())
                summon->UnSummon();
        }

        player->UpdateTempAt(AT7001, 0);
    }

    auto Itep = player->CombatPetStStore.find(AddonIndex);
    if (Itep != player->CombatPetStStore.end())
    {
        Itep->second.CreEntry = 0;
        Itep->second.Rank = 0;
        Itep->second.NowXp = 0;

        CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_COMBAT_PET_DATA);
        Utmt->SetData(0, player->GetGUIDLow());
        Utmt->SetData(1, AddonIndex);
        Utmt->SetData(2, 0);
        Utmt->SetData(3, 0);
        Utmt->SetData(4, 0);
        CharacterDatabase.Execute(Utmt);
    }

    if (!HasCreateNowIndex(player))
        player->UpdateTempAt(AT7001, 0);

    player->SendSysMsg(sHelper->GetGts(9520));
    sDoAddon->SendPacketToClient(player, "DoApartCombatPetNow", std::to_string(AddonIndex));
}

void CombatPetMgr::AutoFillCombatPetPlayerDataAll(Player* player)
{
    uint32 PLevel = player->GetLevel();
    for (auto Itr = player->CombatPetStStore.begin(); Itr != player->CombatPetStStore.end(); ++Itr)
    {
        std::string Name = "Unk";
        AsInt64 MaxHp = AsInt64(1);
        AsInt64 MinDmg = AsInt64(1);
        AsInt64 MaxDmg = AsInt64(2);
        AsInt64 Stat1 = AsInt64(1);
        AsInt64 Stat2 = AsInt64(1);
        AsInt64 Stat3 = AsInt64(1);
        AsInt64 Stat4 = AsInt64(1);
        uint32 Sp1 = 0;
        uint32 Sp2 = 0;
        uint32 Sp3 = 0;
        uint32 Sp4 = 0;
        uint32 Sp5 = 0;
        uint32 Sp6 = 0;
        uint32 Sp7 = 0;
        uint32 Sp8 = 0;
        GetCombatPetSendInfo(PLevel, Itr->second.CreEntry, Itr->second.Rank, Name, MaxHp, MinDmg, MaxDmg, Stat1, Stat2, Stat3, Stat4, Sp1, Sp2, Sp3, Sp4, Sp5, Sp6, Sp7, Sp8);
        uint32 NeedXp = GetCombatPetNextXp(Itr->second.CreEntry, Itr->second.Rank);
        uint64 Score = GetCombatPetScore(Itr->second.CreEntry, Itr->second.Rank);
        std::string InfoLink = Name + "#" + AsInt64ToString(MaxHp) + "#" + AsInt64ToString(MinDmg) + "#" + AsInt64ToString(MaxDmg) + "#" + AsInt64ToString(Stat1) + "#" + AsInt64ToString(Stat2) + "#" + AsInt64ToString(Stat3) + "#" + AsInt64ToString(Stat4) + "#";
        std::string SpLink = std::to_string(Sp1) + "#" + std::to_string(Sp2) + "#" + std::to_string(Sp3) + "#" + std::to_string(Sp4) + "#" + std::to_string(Sp5) + "#" + std::to_string(Sp6) + "#" + std::to_string(Sp7) + "#" + std::to_string(Sp8);
        std::string SendMsg;
        SendMsg.append(std::to_string(Itr->first) + "#" + std::to_string(Itr->second.CreEntry) + "#" + std::to_string(Itr->second.Rank) + "#" + std::to_string(Itr->second.NowXp) + "#" + std::to_string(NeedXp) + "#" + std::to_string(Score) + "#" + InfoLink + SpLink);
        sDoAddon->SendPacketToClient(player, "FillCombatPetPlayerDataAll", SendMsg);

        std::string StatMsg = std::to_string(Itr->first) + "#" + AsInt64ToString(MaxHp) + "#" + AsInt64ToString(MinDmg) + "#" + AsInt64ToString(MaxDmg) + "#" + AsInt64ToString(Stat1) + "#" + AsInt64ToString(Stat2) + "#" + AsInt64ToString(Stat3) + "#" + AsInt64ToString(Stat4) + "#" + (Itr->first == 1 ? "1" : "0");
        sDoAddon->SendPacketToClient(player, "FillCombatPetPlayerStats", StatMsg);
        sDoAddon->SendPacketToClient(player, "FillCombatPetPlayerSpells", std::to_string(Itr->first) + "#" + SpLink);
    }
}

void CombatPetMgr::AutoFillCombatPetPlayerDataOne(Player* player, uint32 CreateIndex)
{
    uint32 PLevel = player->GetLevel();
    auto Itr = player->CombatPetStStore.find(CreateIndex);
    if (Itr == player->CombatPetStStore.end())
        return;

    if (!Itr->second.CreEntry)
        return;

    std::string Name = "Unk";
    AsInt64 MaxHp = AsInt64(1);
    AsInt64 MinDmg = AsInt64(1);
    AsInt64 MaxDmg = AsInt64(2);
    AsInt64 Stat1 = AsInt64(1);
    AsInt64 Stat2 = AsInt64(1);
    AsInt64 Stat3 = AsInt64(1);
    AsInt64 Stat4 = AsInt64(1);
    uint32 Sp1 = 0;
    uint32 Sp2 = 0;
    uint32 Sp3 = 0;
    uint32 Sp4 = 0;
    uint32 Sp5 = 0;
    uint32 Sp6 = 0;
    uint32 Sp7 = 0;
    uint32 Sp8 = 0;
    GetCombatPetSendInfo(PLevel, Itr->second.CreEntry, Itr->second.Rank, Name, MaxHp, MinDmg, MaxDmg, Stat1, Stat2, Stat3, Stat4, Sp1, Sp2, Sp3, Sp4, Sp5, Sp6, Sp7, Sp8);
    uint32 NeedXp = GetCombatPetNextXp(Itr->second.CreEntry, Itr->second.Rank);
    uint64 Score = GetCombatPetScore(Itr->second.CreEntry, Itr->second.Rank);
    std::string InfoLink = Name + "#" + AsInt64ToString(MaxHp) + "#" + AsInt64ToString(MinDmg) + "#" + AsInt64ToString(MaxDmg) + "#" + AsInt64ToString(Stat1) + "#" + AsInt64ToString(Stat2) + "#" + AsInt64ToString(Stat3) + "#" + AsInt64ToString(Stat4) + "#";
    std::string SpLink = std::to_string(Sp1) + "#" + std::to_string(Sp2) + "#" + std::to_string(Sp3) + "#" + std::to_string(Sp4) + "#" + std::to_string(Sp5) + "#" + std::to_string(Sp6) + "#" + std::to_string(Sp7) + "#" + std::to_string(Sp8) + "#";
    std::string SendMsg;
    SendMsg.append(std::to_string(Itr->first) + "#" + std::to_string(Itr->second.CreEntry) + "#" + std::to_string(Itr->second.Rank) + "#" + std::to_string(Itr->second.NowXp) + "#" + std::to_string(NeedXp) + "#" + std::to_string(Score) + "#" + InfoLink + SpLink);
    sDoAddon->SendPacketToClient(player, "FillCombatPetPlayerDataOne", SendMsg);

    std::string StatMsg = std::to_string(Itr->first) + "#" + AsInt64ToString(MaxHp) + "#" + AsInt64ToString(MinDmg) + "#" + AsInt64ToString(MaxDmg) + "#" + AsInt64ToString(Stat1) + "#" + AsInt64ToString(Stat2) + "#" + AsInt64ToString(Stat3) + "#" + AsInt64ToString(Stat4) + "#1";
    sDoAddon->SendPacketToClient(player, "FillCombatPetPlayerStats", StatMsg);
    sDoAddon->SendPacketToClient(player, "FillCombatPetPlayerSpells", std::to_string(Itr->first) + "#" + SpLink);
}

void CombatPetMgr::AutoUpXpAndRankApplyStatTemp(Player* player, Creature* Me, uint32 ToRank)
{
    //指定好等级后
    Me->CombatPetRank = ToRank;
    //处理技能栏
    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
    {
        if (Itr->CreId == Me->GetEntry() && Itr->Rank == Me->CombatPetRank)
        {
            Me->SetCreateStat(STAT_STRENGTH, Itr->Stat1);
            Me->SetCreateStat(STAT_AGILITY, Itr->Stat2);
            Me->SetCreateStat(STAT_INTELLECT, Itr->Stat3);
            Me->SetCreateStat(STAT_SPIRIT, Itr->Stat4);

            for (int8 i = 0; i < MaxSpellCount; i++)
                Me->m_spells[i] = Itr->SpellIds[i];
        }
    }
    CharmInfo* CmInfo = Me->GetCharmInfo();
    if (CmInfo)
        CmInfo->InitCharmCreateSpells();

    player->CharmSpellInitialize();

    Me->StTempId = sCombatPetMgr->GetStTempIdForCombatPetRank(Me);
    Me->SelectLevel(false);
    Me->SetCanModifyStats(true);
    Me->UpdateAllStats();
}

void CombatPetMgr::GetCombatPetSendInfo(uint32 PlayerLevel, uint32 CreEntry, uint32 NowRank, std::string& Name, AsInt64& MaxHp, AsInt64& MinDamage, AsInt64& MaxDamage, AsInt64& Stat1, AsInt64& Stat2, AsInt64& Stat3, AsInt64& Stat4, uint32& Spell1, uint32& Spell2, uint32& Spell3, uint32& Spell4, uint32& Spell5, uint32& Spell6, uint32& Spell7, uint32& Spell8)
{
    // 初始化所有输出参数
    Name.clear();
    MaxHp = AsInt64Zero();
    MinDamage = AsInt64Zero();
    MaxDamage = AsInt64Zero();
    Stat1 = AsInt64Zero();
    Stat2 = AsInt64Zero();
    Stat3 = AsInt64Zero();
    Stat4 = AsInt64Zero();
    Spell1 = 0;
    Spell2 = 0;
    Spell3 = 0;
    Spell4 = 0;
    Spell5 = 0;
    Spell6 = 0;
    Spell7 = 0;
    Spell8 = 0;

    CreatureTemplate const* CInfo = sObjectMgr->GetCreatureTemplate(CreEntry);
    if (!CInfo)
        return;

    Name = CInfo->Name;

    PetLevelInfo const* PetInfo = sObjectMgr->GetPetLevelInfo(CreEntry, PlayerLevel);
    if (PetInfo)
    {
        MaxHp = PetInfo->asHealth > 0 ? PetInfo->asHealth : AsInt64(PetInfo->health);
        MinDamage = PetInfo->asMinDmg > 0 ? PetInfo->asMinDmg : AsInt64(PetInfo->min_dmg);
        MaxDamage = PetInfo->asMaxDmg > 0 ? PetInfo->asMaxDmg : AsInt64(PetInfo->max_dmg);
    }
    else
    {
        CreatureBaseStats const* BaseStats = sObjectMgr->GetCreatureBaseStats(PlayerLevel, CInfo->unit_class);
        MaxHp = BaseStats->AsBaseHealth[CInfo->expansion] > 0 ? BaseStats->AsBaseHealth[CInfo->expansion] : AsInt64(BaseStats->BaseHealth[CInfo->expansion]);
        MaxHp = ScaleCombatPetAsValue(MaxHp, std::min(1.0f, CInfo->ModHealth));
        if (MaxHp < 1)
            MaxHp = 1;
        MinDamage = AsInt64(PlayerLevel - (PlayerLevel / 4));
        MaxDamage = AsInt64(PlayerLevel + (PlayerLevel / 4));
    }

    uint32 StTempId = GetStTempIdForCombatPetRank(CreEntry, NowRank);
    auto Itr = AtCreatureStatTempStore.find(StTempId);
    if (Itr != AtCreatureStatTempStore.end())
    {
        MaxHp = !AsInt64IsZero(Itr->second.NowHp) ? Itr->second.NowHp : ScaleCombatPetAsValue(MaxHp, Itr->second.HealthMultiplier);
        MinDamage = !AsInt64IsZero(Itr->second.DmgNow) ? Itr->second.DmgNow : (!AsInt64IsZero(Itr->second.DmgWMin) ? Itr->second.DmgWMin : MinDamage);
        MaxDamage = !AsInt64IsZero(Itr->second.DmgNow) ? Itr->second.DmgNow : (!AsInt64IsZero(Itr->second.DmgWMax) ? Itr->second.DmgWMax : MaxDamage);
    }

    for (auto Itr = CPetTempStore.begin(); Itr != CPetTempStore.end(); ++Itr)
    {
        if (Itr->CreId == CreEntry && Itr->Rank == NowRank)
        {
            Stat1 = Itr->Stat1;
            Stat2 = Itr->Stat2;
            Stat3 = Itr->Stat3;
            Stat4 = Itr->Stat4;
            Spell1 = Itr->SpellIds[0];
            Spell2 = Itr->SpellIds[1];
            Spell3 = Itr->SpellIds[2];
            Spell4 = Itr->SpellIds[3];
            Spell5 = Itr->SpellIds[4];
            Spell6 = Itr->SpellIds[5];
            Spell7 = Itr->SpellIds[6];
            Spell8 = Itr->SpellIds[7];
        }
    }
}

void CombatPetMgr::UpdateFitCombatPetNow(Player* player, uint32 AddonIndex)
{
    Creature* Cre = player->GetWorldSoltIndexCombatPet();
    if (!Cre)
    {
        player->SendSysMsg(sHelper->GetGts(9509));
        return;
    }

    if (player->GetTempAt(AT7002) > 0 || player->CombatPetFitAppliedIndex > 0)
    {
        player->SendSysMsg(sHelper->GetGts(9510));
        return;
    }

    if (AddonIndex != player->GetTempAt(AT7001))
    {
        player->SendSysMsg(sHelper->GetGts(9518));
        return;
    }

    auto Itep = player->CombatPetStStore.find(AddonIndex);
    if (Itep == player->CombatPetStStore.end())
        return;

    uint32 CreEntry = Itep->second.CreEntry;
    uint32 NowRank = Itep->second.Rank;
    if (!FindCombatPetTemp(CreEntry, NowRank) || !sObjectMgr->GetCreatureTemplate(CreEntry))
        return;

    Cre->CombatStop();
    if (TempSummon* summon = Cre->ToTempSummon())
        summon->UnSummon();

    if (!ApplyCombatPetFitBonus(player, AddonIndex))
        return;

    player->UpdateTempAt(AT7002, AddonIndex);
    sDoAddon->SendPacketToClient(player, "DoUpdateFitCombatPetNow", std::to_string(AddonIndex));
}

void CombatPetMgr::LoginUpdateFitCombatPetNow(Player* player)
{
    uint32 CreateIndex = player->GetTempAtUInt32(AT7002);
    if (!CreateIndex)
        return;

    auto Itep = player->CombatPetStStore.find(CreateIndex);
    if (Itep == player->CombatPetStStore.end())
        return;

    if (!Itep->second.CreEntry)
        return;

    Map* NowMap = player->GetMap();
    if (!NowMap)
        return;

    Creature* OldCre = player->GetWorldSoltIndexCombatPet();
    if (OldCre)
    {
        OldCre->CombatStop();
        if (TempSummon* summon = OldCre->ToTempSummon())
            summon->UnSummon();
    }

    if (!ApplyCombatPetFitBonus(player, CreateIndex))
        LOG_ERROR("server.worldserver", "战宠合体属性登录恢复失败: 玩家 {} 槽位 {}", player->GetGUIDLow(), CreateIndex);
}

bool CombatPetMgr::IsInPlayerPetEntry(Player* player, uint32 CreEntry)
{
    bool In = false;
    for (auto Itr = player->CombatPetStStore.begin(); Itr != player->CombatPetStStore.end(); ++Itr)
    {
        if (Itr->second.CreEntry == CreEntry)
        {
            In = true;
        }
    }
    return In;
}

void Player::InitCombatPetPlayerData()
{
    for (int8 Index = 1; Index < MAX_COMBAT_PET + 1; Index++)
    {
        CombatPetSt St;
        St.CreEntry = 0;
        St.Rank = 0;
        St.NowXp = 0;
        CombatPetStStore[Index] = St;
    }

    if (IsXsPlayerbot(this))
        return;

    uint32 GuidLow = GetGUIDLow();
    CharacterDatabasePreparedStatement* CStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_COMBAT_PET_DATA);
    CStmt->SetData(0, GuidLow);
    PreparedQueryResult CResult = CharacterDatabase.Query(CStmt);
    if (CResult)
    {
        do
        {
            Field* Fields = CResult->Fetch();
            uint32 Index = Fields[0].Get<uint32>();
            CombatPetSt St;
            St.CreEntry = Fields[1].Get<uint32>();
            St.Rank = Fields[2].Get<uint32>();
            St.NowXp = Fields[3].Get<uint32>();
            CombatPetStStore[Index] = St;

        } while (CResult->NextRow());
    }
}

void CombatPetMgr::RemoveCombatPetMapWorldForTeleport(Player* player)
{
    Creature* Cre = player->GetWorldSoltIndexCombatPet();
    if (!Cre)
        return;

    uint32 CreEntry = Cre->GetEntry();
    Cre->CombatStop();
    if (TempSummon* summon = Cre->ToTempSummon())
        summon->UnSummon();
    player->ResetPetEntry = CreEntry;
}

Creature* Player::GetWorldSoltIndexCombatPet() const
{
    Map* NowMap = GetMap();
    if (!NowMap)
        return nullptr;

    if (Creature* Cre = NowMap->GetCreature(GetPetGUID()))
    {
        if (IsInWorld() && Cre->IsInWorld() && sCombatPetMgr->IsCombatPetData(Cre))
            return Cre;
    }

    return nullptr;
}

class ItemUseSumPet : public ItemScript
{
public:
    ItemUseSumPet() : ItemScript("ItemUseSumPet") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        if (!CastItem)
            return true;

        uint32 NowCount = sCombatPetMgr->GetPlayerDataPetCount(player);
        uint32 Limit = sHelper->GetUInt32(18002);
        if (Limit > 0 && NowCount >= Limit)
        {
            player->SendSysMsg(sHelper->GetGts(9515), Limit);
            return false;
        }

        uint32 CreateIndex = sCombatPetMgr->GetCreateNowIndex(player);
        if (!CreateIndex)
        {
            player->SendSysMsg(sHelper->GetGts(9505), MAX_COMBAT_PET);
            return false;
        }

        //当有合体时不能使用新的战宠蛋
        uint32 FitIndex = player->GetTempAtUInt32(AT7002);
        if (FitIndex)
        {
            player->SendSysMsg(sHelper->GetGts(9522));
            return false;
        }

        //记录 授权验证
        if (!sHelper->InCheckFunc(FuncPet))
        {
            //LOG_ERROR("server.worldserver", ">> 没有授权 无法使用该功能");
            player->SendSysMsg("[提示]: 没有授权 无法使用该功能");
            return false;
        }

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "", CreateIndex, 888, Acore::StringFormat(sHelper->GetGts(9504), CreateIndex), 0, false);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, CastItem->GetGUID());
        return true;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        if (Action == 888)
        {
            uint32 FuncItemId = OnItem->GetEntry();

            auto Itr = CPetUseStore.find(FuncItemId);
            if (Itr == CPetUseStore.end())
                return;

            //当有合体时不能使用新的战宠蛋
            uint32 FitIndex = player->GetTempAtUInt32(AT7002);
            if (FitIndex)
            {
                player->SendSysMsg(sHelper->GetGts(9522));
                return;
            }

            auto [CreEntry, ToRank] = sCombatPetMgr->GetCreateSumPetEntryAndRank(FuncItemId);
            if (!CreEntry && !ToRank)
                return;

            if (!sHelper->GetBoolean(18003) && sCombatPetMgr->IsInPlayerPetEntry(player, CreEntry))
            {
                player->SendSysMsg(sHelper->GetGts(9516));
                return;
            }

            //记录 授权验证
            if (!sHelper->InCheckFunc(FuncPet))
            {
                //LOG_ERROR("server.worldserver", ">> 没有授权 无法使用该功能");
                player->SendSysMsg("[提示]: 没有授权 无法使用该功能");
                return;
            }

            if (sCombatPetMgr->CreateCombatPet(player, Sender, CreEntry, ToRank))
            {
                auto Iter = ItemScriptOnDeleteStore.find(FuncItemId);
                if (Iter != ItemScriptOnDeleteStore.end())
                {
                    if (Iter->second > 0)
                    {
                        player->DestroyItemCount(FuncItemId, 1, true);
                    }
                }
            }
        }
    }
};

class DoUseSumPetRankXp : public ItemScript
{
public:
    DoUseSumPetRankXp() : ItemScript("DoUseSumPetRankXp") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (!CastItem)
            return true;

        Creature* Cre = player->GetWorldSoltIndexCombatPet();
        if (!Cre)
            return false;

        uint32 CreateIndex = player->GetTempAtUInt32(AT7001);
        if (!CreateIndex)
            return false;

        auto Itep = player->CombatPetStStore.find(CreateIndex);
        if (Itep == player->CombatPetStStore.end())
            return false;

        if (!Itep->second.CreEntry)
            return false;

        uint32 FuncItemId = CastItem->GetEntry();
        auto Itr = CPetGasStore.find(FuncItemId);
        if (Itr == CPetGasStore.end())
            return false;

        //记录 授权验证
        if (!sHelper->InCheckFunc(FuncPet))
        {
            //LOG_ERROR("server.worldserver", ">> 没有授权 无法使用该功能");
            player->SendSysMsg("[提示]: 没有授权 无法使用该功能");
            return false;
        }

        bool On1 = false;
        uint32 TarRank = Itr->second.ToRank;
        uint32 Max = sCombatPetMgr->GetCombatPetMaxRank(Cre);
        uint32 NowRank = Itep->second.Rank;
        if (TarRank)
        {
            if (TarRank > NowRank && NowRank < Max)
            {
                TarRank = (TarRank > Max) ? Max : TarRank;
                Itep->second.Rank += (TarRank - NowRank);

                player->CastSpell(player, TxLevelSpell, false);
                player->SendSysMsg(sHelper->GetGts(9506), TarRank);
                On1 = true;
            }
        }

        bool On2 = false;
        uint32 NXp = Itr->second.RewXp;
        if (NXp)
        {
            int32 NextXp = int32(sCombatPetMgr->GetCombatPetNextXp(Itep->second.CreEntry, Itep->second.Rank));
            if (!NextXp)
            {
                player->SendSysMsg(sHelper->GetGts(9508));
            }
            else
            {
                int32 NewXp = Itep->second.NowXp + NXp;
                uint32 MaxRank = sCombatPetMgr->GetCombatPetMaxRank(Cre);
                while (NewXp >= NextXp && NextXp && Itep->second.Rank < MaxRank)
                {
                    NewXp -= NextXp;

                    if (NextXp)
                    {
                        Itep->second.Rank += 1;
                        player->CastSpell(player, TxLevelSpell, false);
                        player->SendSysMsg(sHelper->GetGts(9506), Itep->second.Rank);
                    }

                    NextXp = int32(sCombatPetMgr->GetCombatPetNextXp(Itep->second.CreEntry, Itep->second.Rank));
                }

                Itep->second.NowXp = NewXp;
                player->SendSysMsg(sHelper->GetGts(9507), NXp);
                On2 = true;
            }
        }

        bool On3 = false;
        bool AddRank = Itr->second.AddRank;
        if (AddRank)
        {
            uint32 ARank = NowRank + 1;
            if (NowRank < Max)
            {
                ARank = (ARank > Max) ? Max : ARank;
                Itep->second.Rank = ARank;
                player->CastSpell(player, TxLevelSpell, false);
                player->SendSysMsg(sHelper->GetGts(9506), Itep->second.Rank);
                On3 = true;
            }
        }

        if (On1 || On2 || On3)
        {
            sCombatPetMgr->AutoUpXpAndRankApplyStatTemp(player, Cre, Itep->second.Rank);

            CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_COMBAT_PET_DATA);
            Utmt->SetData(0, player->GetGUIDLow());
            Utmt->SetData(1, CreateIndex);
            Utmt->SetData(2, Itep->second.CreEntry);
            Utmt->SetData(3, Itep->second.Rank);
            Utmt->SetData(4, Itep->second.NowXp);
            CharacterDatabase.Execute(Utmt);

            //通知客户端
            sDoAddon->SendPacketToClient(player, "UpdatePlayerDataXpAndRankOne", std::to_string(CreateIndex) + "#" + std::to_string(Itep->second.Rank) + "#" + std::to_string(Itep->second.NowXp) + "#" + std::to_string(sCombatPetMgr->GetCombatPetNextXp(Itep->second.CreEntry, Itep->second.Rank)));

            auto Iter = ItemScriptOnDeleteStore.find(FuncItemId);
            if (Iter != ItemScriptOnDeleteStore.end())
            {
                if (Iter->second > 0)
                {
                    player->DestroyItemCount(FuncItemId, Iter->second, true);
                    return true;
                }
            }
        }

        return false;
    }
};

class OnCombatPetPlayerMod : public PlayerScript
{
public:
    OnCombatPetPlayerMod() : PlayerScript("OnCombatPetPlayerMod") {  }

    void OnLogin(Player* player) override
    {
        //记录 授权验证
        if (!sHelper->InCheckFunc(FuncPet))
            return;

        if (IsXsPlayerbot(player))
            return;

        uint32 CreateIndex = player->GetTempAtUInt32(AT7002);
        if (CreateIndex > 0)
        {
            sCombatPetMgr->LoginUpdateFitCombatPetNow(player);
            return;
        }

        sCombatPetMgr->LoginSummonComCurrentbatPet(player);
    }

    void OnLogout(Player* player) override
    {
        //记录 授权验证
        if (!sHelper->InCheckFunc(FuncPet) || IsXsPlayerbot(player))
            return;

        uint32 fitIndex = player->GetTempAtUInt32(AT7002);
        if (fitIndex)
        {
            if (!RemoveCombatPetFitBonus(player, fitIndex))
                RemoveCombatPetFitBonus(player, 0);
            player->UpdateTempAt(AT7002, 0);
        }
    }

    void OnUpdate(Player* player, uint32 ptime) override
    {
        //记录 授权验证
        if (!sHelper->InCheckFunc(FuncPet))
            return;

        if (IsXsPlayerbot(player))
            return;

        sCombatPetMgr->RemoveCombatPetMapWorldForWithinDistRangePossessed(player);
    }

    void OnCreatureKill(Player* player, Creature* Me) override
    {
        //记录 授权验证
        if (!sHelper->InCheckFunc(FuncPet))
            return;

        if (IsXsPlayerbot(player))
            return;

        Creature* Cre = player->GetWorldSoltIndexCombatPet();
        if (!Cre)
            return;

        uint32 CreateIndex = player->GetTempAtUInt32(AT7001);
        if (!CreateIndex)
            return;
        auto Itep = player->CombatPetStStore.find(CreateIndex);
        if (Itep == player->CombatPetStStore.end())
            return;

        if (!Itep->second.CreEntry)
            return;

        uint32 CreatureId = Me->GetEntry();
        auto Itr = CPetGasStore.find(CreatureId);
        if (Itr == CPetGasStore.end())
            return;

        bool On1 = false;
        uint32 TarRank = Itr->second.ToRank;
        uint32 Max = sCombatPetMgr->GetCombatPetMaxRank(Cre);
        uint32 NowRank = Itep->second.Rank;
        if (TarRank)
        {
            if (TarRank > NowRank && NowRank < Max)
            {
                TarRank = (TarRank > Max) ? Max : TarRank;
                Itep->second.Rank += (TarRank - NowRank);

                player->CastSpell(player, TxLevelSpell, false);
                player->SendSysMsg(sHelper->GetGts(9506), TarRank);
                On1 = true;
            }
        }

        bool On2 = false;
        uint32 NXp = Itr->second.RewXp;
        if (NXp)
        {
            int32 NextXp = int32(sCombatPetMgr->GetCombatPetNextXp(Itep->second.CreEntry, Itep->second.Rank));
            if (!NextXp)
            {
                player->SendSysMsg(sHelper->GetGts(9508));
            }
            else
            {
                int32 NewXp = Itep->second.NowXp + NXp;
                uint32 MaxRank = sCombatPetMgr->GetCombatPetMaxRank(Cre);
                while (NewXp >= NextXp && NextXp && Itep->second.Rank < MaxRank)
                {
                    NewXp -= NextXp;

                    if (NextXp)
                    {
                        Itep->second.Rank += 1;
                        player->CastSpell(player, TxLevelSpell, false);
                        player->SendSysMsg(sHelper->GetGts(9506), Itep->second.Rank);
                    }

                    NextXp = int32(sCombatPetMgr->GetCombatPetNextXp(Itep->second.CreEntry, Itep->second.Rank));
                }

                Itep->second.NowXp = NewXp;
                player->SendSysMsg(sHelper->GetGts(9507), NXp);
                On2 = true;
            }
        }

        bool On3 = false;
        bool AddRank = Itr->second.AddRank;
        if (AddRank)
        {
            uint32 ARank = NowRank + 1;
            if (NowRank < Max)
            {
                ARank = (ARank > Max) ? Max : ARank;
                Itep->second.Rank = ARank;
                player->CastSpell(player, TxLevelSpell, false);
                player->SendSysMsg(sHelper->GetGts(9506), Itep->second.Rank);
                On3 = true;
            }
        }

        if (On1 || On2 || On3)
        {
            sCombatPetMgr->AutoUpXpAndRankApplyStatTemp(player, Cre, Itep->second.Rank);

            CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_COMBAT_PET_DATA);
            Utmt->SetData(0, player->GetGUIDLow());
            Utmt->SetData(1, CreateIndex);
            Utmt->SetData(2, Itep->second.CreEntry);
            Utmt->SetData(3, Itep->second.Rank);
            Utmt->SetData(4, Itep->second.NowXp);
            CharacterDatabase.Execute(Utmt);

            //通知客户端
            sDoAddon->SendPacketToClient(player, "UpdatePlayerDataXpAndRankOne", std::to_string(CreateIndex) + "#" + std::to_string(Itep->second.Rank) + "#" + std::to_string(Itep->second.NowXp) + "#" + std::to_string(sCombatPetMgr->GetCombatPetNextXp(Itep->second.CreEntry, Itep->second.Rank)));
        }
    }
};

void AddSC_CombatPetScripts()
{
    new ItemUseSumPet();
    new OnCombatPetPlayerMod();
    new DoUseSumPetRankXp();
}
