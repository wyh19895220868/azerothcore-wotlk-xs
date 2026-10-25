#include "Custom/_Helper/XsPlayerbotCompat.h"
#include "Player.h"
#include "Player.h"
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
#include "Item.h"
#include "Bag.h"
#include "Pet.h"
#include <CharacterCache.h>
#include <algorithm>
#include <tuple>
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include "PicGuideMgr.h"
#include <Time\GameTime.h>
#pragma execution_character_set("utf-8")

PicItemGuideConfigMap PicItemGuideConfigStore;
PageItemBondsMap PageItemBondsStore;
PicItemGuidePageMultMap PicItemGuidePageMultStore;
PicItemGuideBondsMap PicItemGuideBondsStore;

PicCreatureGuideConfigMap PicCreatureGuideConfigStore;
PageCreatureBondsMap PageCreatureBondsStore;
PicCreatureGuidePageMultMap PicCreatureGuidePageMultStore;
PicCreatureGuideBondsMap PicCreatureGuideBondsStore;

void PicGuideMgr::LoadPicGuideData()
{
    PicItemGuideConfigStore.clear();
    PageItemBondsStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 序号, 子项名称, 子项图标, 关联的页面编号, 羁绊编号 FROM zs_图鉴_物品_配置项目 ORDER BY 序号;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            PicItemGuideConfig St{};
            St.Num = Fields[0].Get<uint32>();
            St.SubName = Fields[1].Get<std::string>();
            St.SubIcon = Fields[2].Get<std::string>();
            St.SubPageId = Fields[3].Get<uint32>();
            St.BondsId = Fields[4].Get<uint32>();
            PicItemGuideConfigStore.insert(std::make_pair(St.Num, St));
            PageItemBondsStore.insert(std::make_pair(St.SubPageId, St.BondsId));
        } while (Result->NextRow());
    }

    PicItemGuidePageMultStore.clear();
    Result = WorldDatabase.Query("SELECT 页面编号, 物品编号, 奖励模版, 激活是否生效属性, 激活是否生效技能, 是否扣除, 排序 FROM zs_图鉴_物品_页面关联 ORDER BY 排序;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            PicItemGuidePage St{};
            St.PageId = Fields[0].Get<uint32>();
            St.ItemId = Fields[1].Get<uint32>();
            St.RTempId = Fields[2].Get<uint32>();
            St.IsActStat = Fields[3].Get<bool>();
            St.IsActSpell = Fields[4].Get<bool>();
            St.IsDelect = Fields[5].Get<bool>();
            St.Sort = Fields[6].Get<uint32>();
            PicItemGuidePageMultStore.insert(std::make_pair(St.PageId, St));
        } while (Result->NextRow());
    }

    PicItemGuideBondsStore.clear();
    Result = WorldDatabase.Query("SELECT 羁绊编号, 激活件数, 羁绊效果, 效果说明 FROM zs_图鉴_物品_设置羁绊;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            PicItemGuideBonds St{};
            St.BondsId = Fields[0].Get<uint32>();
            St.BondsCount = Fields[1].Get<uint32>();
            std::string StrSpells = Fields[2].Get<std::string>();
            if (!StrSpells.empty())
            {
                Tokenizer SpellsData(StrSpells, '#');
                for (auto const& Temp : SpellsData)
                    if (sSpellMgr->GetSpellInfo(uint32(atoi(Temp))))
                        St.BondsSpells.insert(uint32(atoi(Temp)));
            }
            St.Comment = Fields[3].Get<std::string>();
            PicItemGuideBondsStore.insert(std::make_pair(St.BondsId, St));
        } while (Result->NextRow());
    }

    PicCreatureGuideConfigStore.clear();
    PageCreatureBondsStore.clear();
    Result = WorldDatabase.Query("SELECT 序号, 子项名称, 子项图标, 关联的页面编号, 羁绊编号 FROM zs_图鉴_生物_配置项目 ORDER BY 序号;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            PicCreatureGuideConfig St{};
            St.Num = Fields[0].Get<uint32>();
            St.SubName = Fields[1].Get<std::string>();
            St.SubIcon = Fields[2].Get<std::string>();
            St.SubPageId = Fields[3].Get<uint32>();
            St.BondsId = Fields[4].Get<uint32>();
            PicCreatureGuideConfigStore.insert(std::make_pair(St.Num, St));
            PageCreatureBondsStore.insert(std::make_pair(St.SubPageId, St.BondsId));
        } while (Result->NextRow());
    }

    PicCreatureGuidePageMultStore.clear();
    Result = WorldDatabase.Query("SELECT 页面编号, 生物编号, 难度, 需要击杀数量, 奖励模版, 激活后奖励属性编号, 激活后奖励属性数值, 可以叠加属性, 等级, 排序 FROM zs_图鉴_生物_页面关联 ORDER BY 页面编号, 生物编号, 难度, 等级;");
    if (Result)
    {
        std::map<std::tuple<uint32, uint32, uint32>, PicCreatureGuidePage> GroupedPages;
        do
        {
            Field* Fields = Result->Fetch();
            uint32 PageId = Fields[0].Get<uint32>();
            uint32 CreId = Fields[1].Get<uint32>();
            uint32 Diff = Fields[2].Get<uint32>();
            std::string KillCountText = Fields[3].Get<std::string>();
            std::string StatTypeText = Fields[5].Get<std::string>();
            std::string StatValueText = Fields[6].Get<std::string>();
            if (KillCountText.find('#') != std::string::npos || StatTypeText.find('#') != std::string::npos || StatValueText.find('#') != std::string::npos)
            {
                LOG_ERROR("server.loading", "图鉴生物配置错误: 页面 {}, 生物 {}, 分级字段不再支持#，请拆成多行配置", PageId, CreId);
                continue;
            }

            uint32 StageKillCount = uint32(std::strtoul(KillCountText.c_str(), nullptr, 10));
            uint32 StatType = uint32(std::strtoul(StatTypeText.c_str(), nullptr, 10));
            int32 StatValue = int32(std::strtol(StatValueText.c_str(), nullptr, 10));
            if (!StageKillCount || StatType >= MAX_ITEM_MOD)
            {
                LOG_ERROR("server.loading", "图鉴生物配置错误: 页面 {}, 生物 {}, 等级 {} 的配置无效", PageId, CreId, Fields[8].Get<uint32>());
                continue;
            }

            uint32 Level = Fields[8].Get<uint32>();
            if (!Level)
            {
                LOG_ERROR("server.loading", "图鉴生物配置错误: 页面 {}, 生物 {}, 等级不能为0", PageId, CreId);
                continue;
            }

            auto Key = std::make_tuple(PageId, CreId, Diff);
            auto [Itr, Inserted] = GroupedPages.try_emplace(Key);
            PicCreatureGuidePage& St = Itr->second;
            if (Inserted)
            {
                St.PageId = PageId;
                St.CreId = CreId;
                St.Diff = Diff;
                St.RTempId = Fields[4].Get<uint32>();
                St.Sort = Fields[9].Get<uint32>();
                St.StackStats = Fields[7].Get<bool>();
            }

            uint32 CumulativeKillCount = StageKillCount + (St.KillRewards.empty() ? 0 : St.KillRewards.back().KillCount);
            St.KillRewards.push_back({ Level, CumulativeKillCount, StageKillCount, StatType, StatValue });
        } while (Result->NextRow());

        std::vector<PicCreatureGuidePage> SortedPages;
        for (auto& [Key, St] : GroupedPages)
            SortedPages.push_back(std::move(St));
        std::sort(SortedPages.begin(), SortedPages.end(), [](auto const& Left, auto const& Right)
        {
            return Left.Sort < Right.Sort;
        });
        for (auto& St : SortedPages)
            PicCreatureGuidePageMultStore.insert(std::make_pair(St.PageId, std::move(St)));
    }

    PicCreatureGuideBondsStore.clear();
    Result = WorldDatabase.Query("SELECT 羁绊编号, 激活件数, 羁绊效果, 效果说明 FROM zs_图鉴_生物_设置羁绊;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            PicCreatureGuideBonds St{};
            St.BondsId = Fields[0].Get<uint32>();
            St.BondsCount = Fields[1].Get<uint32>();
            std::string StrSpells = Fields[2].Get<std::string>();
            if (!StrSpells.empty())
            {
                Tokenizer SpellsData(StrSpells, '#');
                for (auto const& Temp : SpellsData)
                    if (sSpellMgr->GetSpellInfo(uint32(atoi(Temp))))
                        St.BondsSpells.insert(uint32(atoi(Temp)));
            }
            St.Comment = Fields[3].Get<std::string>();
            PicCreatureGuideBondsStore.insert(std::make_pair(St.BondsId, St));
        } while (Result->NextRow());
    }
}

uint32 PicGuideMgr::GetItemBondsIdFromSubPage(uint32 SubPageId)
{
    auto Itr = PageItemBondsStore.find(SubPageId);
    return (Itr != PageItemBondsStore.end() ? Itr->second : 0);
}

PicItemGuidePage* PicGuideMgr::GetItemPageData(uint32 ItemId, uint32 SubPageId)
{
    auto Range = PicItemGuidePageMultStore.equal_range(SubPageId);
    if (Range.first == Range.second)
        return nullptr;

    for (auto Itr = Range.first; Itr != Range.second; Itr++)
    {
        if (Itr->second.ItemId == ItemId)
        {
            return &Itr->second;
        }
    }
    return nullptr;
}

void Player::LoadItemPicGuideCache()
{
    ItemPicGuideCacheStore.clear();
    CharacterDatabasePreparedStatement* Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PIC_ITEM_GUIDE_DATA);
    Stmt->SetData(0, GetGUIDLow());
    PreparedQueryResult Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            ItemPicGuideCache St{};
            St.ItemId = Fields[0].Get<uint32>();
            St.SubPageId = Fields[1].Get<uint32>();
            ItemPicGuideCacheStore.emplace_back(St);
        } while (Result->NextRow());
    }
}

void Player::UpdateItemPicGuideCache(uint32 ItemId, uint32 SubPageId)
{
    CharacterDatabasePreparedStatement* Stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_PIC_ITEM_GUIDE_DATA);
    Stmt->SetData(0, GetGUIDLow());
    Stmt->SetData(1, ItemId);
    Stmt->SetData(2, SubPageId);
    CharacterDatabase.Execute(Stmt);

    auto Itr = std::find(ItemPicGuideCacheStore.begin(), ItemPicGuideCacheStore.end(), ItemPicGuideCache(ItemId, SubPageId));
    if (Itr == ItemPicGuideCacheStore.end())
    {
        ItemPicGuideCacheStore.emplace_back(ItemPicGuideCache(ItemId, SubPageId));
    }
    else
    {
        Itr->ItemId = ItemId;
        Itr->SubPageId = SubPageId;
    }
}

bool Player::IsActItemPicGuide(uint32 ItemId, uint32 SubPageId)
{
    auto Itr = std::find(ItemPicGuideCacheStore.begin(), ItemPicGuideCacheStore.end(), ItemPicGuideCache(ItemId, SubPageId));
    return (Itr != ItemPicGuideCacheStore.end());
}

uint32 Player::ActItemPicGuideBondsCount(uint32 SubPageId)
{
    return std::count_if(ItemPicGuideCacheStore.begin(), ItemPicGuideCacheStore.end(), [=](const auto& St) { return St.SubPageId == SubPageId; });
}

void Player::ApplyItemStatsAndEquipSpells(uint32 ItemId, bool IsActStat, bool IsActSpell)
{
    ItemTemplate const* Template = sObjectMgr->GetItemTemplate(ItemId);
    if (!Template)
        return;

    bool Apply = true;
    if (IsActStat)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (i >= Template->StatsCount)
                continue;

            uint32 statType = Template->ItemStat[i].ItemStatType;
            int32 val = Template->ItemStat[i].ItemStatValue;

            AsInt64 realVal = i < Template->StatsCount ? Template->AsItemStatValue[i] : AsInt64(val);
            if (AsInt64IsZero(realVal))
                continue;

            if (statType < MAX_ITEM_MOD)
            {
                DoApplyStat(ItemModType(statType), realVal, Apply);
                continue;
            }

            switch (statType)
            {
            case ITEM_MOD_MANA:
                HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(val), Apply);
                break;
            case ITEM_MOD_HEALTH:                           // modify HP
                HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(val), Apply);
                break;
            case ITEM_MOD_AGILITY:                          // modify agility
                HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), Apply);
                ApplyStatBuffMod(STAT_AGILITY, float(val), Apply);
                break;
            case ITEM_MOD_STRENGTH:                         //modify strength
                HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), Apply);
                ApplyStatBuffMod(STAT_STRENGTH, float(val), Apply);
                break;
            case ITEM_MOD_INTELLECT:                        //modify intellect
                HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), Apply);
                ApplyStatBuffMod(STAT_INTELLECT, float(val), Apply);
                break;
            case ITEM_MOD_SPIRIT:                           //modify spirit
                HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), Apply);
                ApplyStatBuffMod(STAT_SPIRIT, float(val), Apply);
                break;
            case ITEM_MOD_STAMINA:                          //modify stamina
                HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), Apply);
                ApplyStatBuffMod(STAT_STAMINA, float(val), Apply);
                break;
            case ITEM_MOD_DEFENSE_SKILL_RATING:
                ApplyRatingMod(CR_DEFENSE_SKILL, int32(val), Apply);
                break;
            case ITEM_MOD_DODGE_RATING:
                ApplyRatingMod(CR_DODGE, int32(val), Apply);
                break;
            case ITEM_MOD_PARRY_RATING:
                ApplyRatingMod(CR_PARRY, int32(val), Apply);
                break;
            case ITEM_MOD_BLOCK_RATING:
                ApplyRatingMod(CR_BLOCK, int32(val), Apply);
                break;
            case ITEM_MOD_HIT_MELEE_RATING:
                ApplyRatingMod(CR_HIT_MELEE, int32(val), Apply);
                break;
            case ITEM_MOD_HIT_RANGED_RATING:
                ApplyRatingMod(CR_HIT_RANGED, int32(val), Apply);
                break;
            case ITEM_MOD_HIT_SPELL_RATING:
                ApplyRatingMod(CR_HIT_SPELL, int32(val), Apply);
                break;
            case ITEM_MOD_CRIT_MELEE_RATING:
                ApplyRatingMod(CR_CRIT_MELEE, int32(val), Apply);
                break;
            case ITEM_MOD_CRIT_RANGED_RATING:
                ApplyRatingMod(CR_CRIT_RANGED, int32(val), Apply);
                break;
            case ITEM_MOD_CRIT_SPELL_RATING:
                ApplyRatingMod(CR_CRIT_SPELL, int32(val), Apply);
                break;
            case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), Apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), Apply);
                break;
            case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), Apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), Apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), Apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), Apply);
                break;
            case ITEM_MOD_HASTE_MELEE_RATING:
                ApplyRatingMod(CR_HASTE_MELEE, int32(val), Apply);
                break;
            case ITEM_MOD_HASTE_RANGED_RATING:
                ApplyRatingMod(CR_HASTE_RANGED, int32(val), Apply);
                break;
            case ITEM_MOD_HASTE_SPELL_RATING:
                ApplyRatingMod(CR_HASTE_SPELL, int32(val), Apply);
                break;
            case ITEM_MOD_HIT_RATING:
                ApplyRatingMod(CR_HIT_MELEE, int32(val), Apply);
                ApplyRatingMod(CR_HIT_RANGED, int32(val), Apply);
                ApplyRatingMod(CR_HIT_SPELL, int32(val), Apply);
                break;
            case ITEM_MOD_CRIT_RATING:
                ApplyRatingMod(CR_CRIT_MELEE, int32(val), Apply);
                ApplyRatingMod(CR_CRIT_RANGED, int32(val), Apply);
                ApplyRatingMod(CR_CRIT_SPELL, int32(val), Apply);
                break;
            case ITEM_MOD_HIT_TAKEN_RATING:
                ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), Apply);
                ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), Apply);
                ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), Apply);
                break;
            case ITEM_MOD_CRIT_TAKEN_RATING:
            case ITEM_MOD_RESILIENCE_RATING:
                ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), Apply);
                ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), Apply);
                ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), Apply);
                break;
            case ITEM_MOD_HASTE_RATING:
                ApplyRatingMod(CR_HASTE_MELEE, int32(val), Apply);
                ApplyRatingMod(CR_HASTE_RANGED, int32(val), Apply);
                ApplyRatingMod(CR_HASTE_SPELL, int32(val), Apply);
                break;
            case ITEM_MOD_EXPERTISE_RATING:
                ApplyRatingMod(CR_EXPERTISE, int32(val), Apply);
                break;
            case ITEM_MOD_ATTACK_POWER:
                HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(val), Apply);
                HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), Apply);
                break;
            case ITEM_MOD_RANGED_ATTACK_POWER:
                HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), Apply);
                break;
                //            case ITEM_MOD_FERAL_ATTACK_POWER:
                //                ApplyFeralAPBonus(int32(val), apply);
                //                break;
            case ITEM_MOD_MANA_REGENERATION:
                ApplyManaRegenBonus(int32(val), Apply);
                break;
            case ITEM_MOD_ARMOR_PENETRATION_RATING:
                ApplyRatingMod(CR_ARMOR_PENETRATION, int32(val), Apply);
                break;
            case ITEM_MOD_SPELL_POWER:
                ApplySpellPowerBonus(int32(val), Apply);
                break;
            case ITEM_MOD_HEALTH_REGEN:
                ApplyHealthRegenBonus(int32(val), Apply);
                break;
            case ITEM_MOD_SPELL_PENETRATION:
                ApplySpellPenetrationBonus(val, Apply);
                break;
            case ITEM_MOD_BLOCK_VALUE:
                HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(val), Apply);
                break;
                // deprecated item mods
            case ITEM_MOD_SPELL_HEALING_DONE:
            case ITEM_MOD_SPELL_DAMAGE_DONE:
                break;
            }
        }


        uint32 armor = Template->Armor;
        if (armor)
            HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, float(armor), Apply);

        if (Template->Block)
            HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(Template->Block), Apply);

        if (Template->HolyRes)
            HandleStatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(Template->HolyRes), Apply);

        if (Template->FireRes)
            HandleStatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(Template->FireRes), Apply);

        if (Template->NatureRes)
            HandleStatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(Template->NatureRes), Apply);

        if (Template->FrostRes)
            HandleStatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(Template->FrostRes), Apply);

        if (Template->ShadowRes)
            HandleStatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(Template->ShadowRes), Apply);

        if (Template->ArcaneRes)
            HandleStatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(Template->ArcaneRes), Apply);
    }

    if (IsActSpell)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            _Spell const& spellData = Template->Spells[i];
            if (!spellData.SpellId)
                continue;

            SpellInfo const* spellproto = sSpellMgr->GetSpellInfo(spellData.SpellId);
            if (!spellproto)
                continue;

            if (Apply)
            {
                if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
                {
                    continue;
                }

                if (spellproto->CheckShapeshift(GetShapeshiftForm()) != SPELL_CAST_OK)
                    return;

                CastSpell(this, spellproto, true);
            }
            else
            {
                if (spellData.SpellId == 11826)
                {
                    continue;
                }

                RemoveAurasDueToSpell(spellproto->Id);
                for (uint8 i = EFFECT_0; i < MAX_SPELL_EFFECTS; ++i)
                {
                    if (spellproto->Effects[i].TriggerSpell)
                        RemoveAurasDueToSpell(spellproto->Effects[i].TriggerSpell);

                    if (spellproto->Effects[i].Effect == SPELL_EFFECT_SUMMON)
                        RemoveAllMinionsByEntry(spellproto->Effects[i].MiscValue);
                }
            }
        }
    }
}

void Player::UpdateItemClickBondsAuraId(uint32 SubPageId, bool Login)
{
    uint32 ActCount = ActItemPicGuideBondsCount(SubPageId);
    //更新激活的羁绊件数
    if (!Login)
        sDoAddon->SendPacketToClient(this, "PicGuideBondsCount", std::to_string(SubPageId) + "#" + std::to_string(ActCount));

    uint32 BondsId = sPicGuideMgr->GetItemBondsIdFromSubPage(SubPageId);
    auto Range = PicItemGuideBondsStore.equal_range(BondsId);
    for (auto Itr = Range.first; Itr != Range.second; Itr++)
    {
        if (ActCount == Itr->second.BondsCount)
        {
            for (auto Iter = Itr->second.BondsSpells.begin(); Iter != Itr->second.BondsSpells.end(); Iter++)
            {
                if (!HasAura(*Iter))
                {
                    AddAura(*Iter, this);
                    SendSysMsg(sHelper->GetGts(11002), sHelper->GetSpellName(*Iter));
                }
            }
        }
    }
}

void Player::LoadCrePicGuideCache()
{
    CrePicGuideCacheStore.clear();
    CharacterDatabasePreparedStatement* Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PIC_CRE_GUIDE_DATA);
    Stmt->SetData(0, GetGUIDLow());
    PreparedQueryResult Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            CrePicGuideCache St{};
            St.CreId = Fields[0].Get<uint32>();
            St.KillCount = Fields[1].Get<uint32>();
            St.SubPageId = Fields[2].Get<uint32>();
            CrePicGuideCacheStore.emplace_back(St);
        } while (Result->NextRow());
    }
}

void Player::UpdateCrePicGuideCache(uint32 CreId, uint32 OldKillCount, uint32 NewKillCount, uint32 SubPageId)
{
    CharacterDatabasePreparedStatement* Stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_PIC_CRE_GUIDE_DATA);
    Stmt->SetData(0, GetGUIDLow());
    Stmt->SetData(1, CreId);
    Stmt->SetData(2, NewKillCount);
    Stmt->SetData(3, SubPageId);
    CharacterDatabase.Execute(Stmt);

    auto Itr = std::find_if(CrePicGuideCacheStore.begin(), CrePicGuideCacheStore.end(),
                           [CreId, SubPageId](const auto& St) { return St.CreId == CreId && St.SubPageId == SubPageId; });
    if (Itr == CrePicGuideCacheStore.end())
    {
        CrePicGuideCacheStore.emplace_back(CrePicGuideCache(CreId, NewKillCount, SubPageId));
    }
    else
    {
        Itr->CreId = CreId;
        Itr->KillCount = NewKillCount;
        Itr->SubPageId = SubPageId;
    }
}

bool Player::IsActCreaturePicGuide(uint32 CreId, uint32 NeedKillCount, uint32 SubPageId)
{
    auto Itr = std::find_if(CrePicGuideCacheStore.begin(), CrePicGuideCacheStore.end(),
                           [CreId, SubPageId](const auto& St) { return St.CreId == CreId && St.SubPageId == SubPageId; });
    if (Itr == CrePicGuideCacheStore.end())
        return false;

    return Itr->KillCount >= NeedKillCount;
}

uint32 Player::ActCrePicGuideBondsCount(uint32 SubPageId)
{
    uint32 ActCount = 0;
    auto Range = PicCreatureGuidePageMultStore.equal_range(SubPageId);
    for (auto Itr = Range.first; Itr != Range.second; Itr++)
    {
        if (IsActCreaturePicGuide(Itr->second.CreId, Itr->second.GetMaxKillCount(), Itr->second.PageId))
            ActCount++;
    }

    return ActCount;
}

uint32 Player::GetCrePicGuideKillCount(uint32 CreId, uint32 SubPageId)
{
    for (auto Itr = CrePicGuideCacheStore.begin(); Itr != CrePicGuideCacheStore.end(); Itr++)
        if (Itr->CreId == CreId && Itr->SubPageId == SubPageId)
            return Itr->KillCount;

    return 0;
}

void Player::UpdateCreatureClickBondsAuraId(uint32 SubPageId, bool Login)
{
    uint32 ActCount = ActCrePicGuideBondsCount(SubPageId);
    //更新激活的羁绊件数
    if (!Login)
        sDoAddon->SendPacketToClient(this, "PicGuideBondsCount", std::to_string(SubPageId) + "#" + std::to_string(ActCount));

    uint32 BondsId = sPicGuideMgr->GetCreatureBondsIdFromSubPage(SubPageId);
    auto Range = PicCreatureGuideBondsStore.equal_range(BondsId);
    for (auto Itr = Range.first; Itr != Range.second; Itr++)
    {
        if (ActCount == Itr->second.BondsCount)
        {
            for (auto Iter = Itr->second.BondsSpells.begin(); Iter != Itr->second.BondsSpells.end(); Iter++)
            {
                if (!HasAura(*Iter))
                {
                    AddAura(*Iter, this);
                    SendSysMsg(sHelper->GetGts(11002), sHelper->GetSpellName(*Iter));
                }
            }
        }
    }
}

uint32 PicGuideMgr::GetCreatureBondsIdFromSubPage(uint32 SubPageId)
{
    auto Itr = PageCreatureBondsStore.find(SubPageId);
    return (Itr != PageCreatureBondsStore.end() ? Itr->second : 0);
}

PicCreatureGuidePage* PicGuideMgr::GetCreaturePageData(uint32 CreId, uint32 SubPageId)
{
    auto Range = PicCreatureGuidePageMultStore.equal_range(SubPageId);
    if (Range.first == Range.second)
        return nullptr;

    for (auto Itr = Range.first; Itr != Range.second; Itr++)
    {
        if (Itr->second.CreId == CreId)
        {
            return &Itr->second;
        }
    }
    return nullptr;
}

class OnPicGuideAllPlayer : public PlayerScript
{
public:
    OnPicGuideAllPlayer() : PlayerScript("OnPicGuideAllPlayer") {}

    void OnLogin(Player* player) override
    {
        if (IsXsPlayerbot(player))
            return;

        //这里是处理登录后加载的 因为属性是临时的 需要处理 不要改动这里的任何代码

        for (auto Itr = player->ItemPicGuideCacheStore.begin(); Itr != player->ItemPicGuideCacheStore.end(); Itr++)
        {
            PicItemGuidePage* St = sPicGuideMgr->GetItemPageData(Itr->ItemId, Itr->SubPageId);
            if (St)
                player->ApplyItemStatsAndEquipSpells(St->ItemId, St->IsActStat, St->IsActSpell);
        }

        for (auto Itr = PicItemGuideConfigStore.begin(); Itr != PicItemGuideConfigStore.end(); Itr++)
            player->UpdateItemClickBondsAuraId(Itr->second.SubPageId, true);

        for (auto Itr = player->CrePicGuideCacheStore.begin(); Itr != player->CrePicGuideCacheStore.end(); Itr++)
        {
            PicCreatureGuidePage* St = sPicGuideMgr->GetCreaturePageData(Itr->CreId, Itr->SubPageId);
            if (St)
            {
                PicCreatureGuidePage::KillReward const* LastReward = nullptr;
                for (auto const& Reward : St->KillRewards)
                {
                    if (Itr->KillCount < Reward.KillCount)
                        break;

                    LastReward = &Reward;
                    if (St->StackStats)
                        player->DoApplyStat(ItemModType(Reward.StatType), Reward.StatValue);
                }

                if (!St->StackStats && LastReward)
                    player->DoApplyStat(ItemModType(LastReward->StatType), LastReward->StatValue);
            }
        }

        for (auto Itr = PicCreatureGuidePageMultStore.begin(); Itr != PicCreatureGuidePageMultStore.end(); Itr++)
        {
            bool IsAct = player->IsActCreaturePicGuide(Itr->second.CreId, Itr->second.GetMaxKillCount(), Itr->second.PageId);
            if (IsAct)
                player->UpdateCreatureClickBondsAuraId(Itr->second.PageId, true);
        }
    }

    void OnCreatureKill(Player* player, Creature* Killed) override
    {
        if (IsXsPlayerbot(player))
            return;

        Map* CurrMap = player->GetMap();
        if (!CurrMap)
            return;

        uint32 CreEntry = Killed->GetEntry();
        uint32 CreDiff = uint32(CurrMap->GetDifficulty());
        for (auto Itr = PicCreatureGuidePageMultStore.begin(); Itr != PicCreatureGuidePageMultStore.end(); Itr++)
        {
            if (Itr->second.CreId == CreEntry && Itr->second.Diff == CreDiff)
            {
                uint32 MaxKillCount = Itr->second.GetMaxKillCount();
                bool IsAct = player->IsActCreaturePicGuide(Itr->second.CreId, MaxKillCount, Itr->second.PageId);
                if (!IsAct)
                {
                    uint32 OldKillCount = player->GetCrePicGuideKillCount(Itr->second.CreId, Itr->second.PageId);
                    uint32 NewKillCount = OldKillCount + 1;
                    //更新缓存
                    player->UpdateCrePicGuideCache(CreEntry, OldKillCount, NewKillCount, Itr->second.PageId);
                    //通知客户端
                    sDoAddon->SendPacketToClient(player, "PicGuideCreatureKillCount", std::to_string(Itr->second.PageId) + "#" + std::to_string(Itr->second.CreId) + "#" + std::to_string(NewKillCount) + "#" + std::to_string(MaxKillCount));

                    if (Itr->second.StackStats)
                    {
                        for (auto const& Reward : Itr->second.KillRewards)
                            if (OldKillCount < Reward.KillCount && NewKillCount >= Reward.KillCount)
                                player->DoApplyStat(ItemModType(Reward.StatType), Reward.StatValue);
                    }
                    else
                    {
                        PicCreatureGuidePage::KillReward const* OldReward = nullptr;
                        PicCreatureGuidePage::KillReward const* NewReward = nullptr;
                        for (auto const& Reward : Itr->second.KillRewards)
                        {
                            if (OldKillCount >= Reward.KillCount)
                                OldReward = &Reward;
                            if (NewKillCount >= Reward.KillCount)
                                NewReward = &Reward;
                        }

                        if (NewReward != OldReward)
                        {
                            if (OldReward)
                                player->DoApplyStat(ItemModType(OldReward->StatType), OldReward->StatValue, false);
                            if (NewReward)
                                player->DoApplyStat(ItemModType(NewReward->StatType), NewReward->StatValue);
                        }
                    }

                    if (NewKillCount >= MaxKillCount)
                    {
                        //处理加属性
                        //奖励模版
                        sHelper->DoTheReward(player, Itr->second.RTempId, "图鉴怪物击杀奖励");
                        //处理羁绊加成
                        player->UpdateCreatureClickBondsAuraId(Itr->second.PageId, false);
                    }
                    player->CastSpell(player, TxLevelSpell, false);
                }
            }
        }
    }
};

void AddSC_PicGuideAllPlayerScript()
{
    new OnPicGuideAllPlayer();
}
