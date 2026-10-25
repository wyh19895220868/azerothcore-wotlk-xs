#include "AsInt256.h"
#pragma execution_character_set("utf-8")

#include "BeastCardMgr.h"

#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GossipDef.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "AI/ScriptedAI/ScriptedGossip.h"
#include <Custom/AddonActive/DoAddon.h>
#include <Custom/_Helper/Helper.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace Acore::ChatCommands;

BeastCardTemplateMap BeastCardTemplateStore;
BeastCardQualityTemplateMap BeastCardQualityTemplateStore;
BeastCardBondTemplateMap BeastCardBondTemplateStore;

namespace
{
using BeastCardKillProgressMap = std::unordered_map<uint32, std::pair<uint32, uint32>>;
using BeastCardOpenedMap = std::unordered_map<uint32, uint8>;

struct BeastCardSkillBonusCache
{
    int64 FixedAll = 0;
    int64 PercentAll = 0;
    std::unordered_map<uint32, std::pair<int64, int64>> PerSpell;
};

std::mutex BeastCardSkillBonusMutex;
std::unordered_map<uint32, BeastCardSkillBonusCache> BeastCardSkillBonusStore;

struct BeastCardQualityDisplay
{
    char const* Name;
    char const* Color;
};

constexpr std::array<BeastCardQualityDisplay, BEAST_CARD_MAX_QUALITY + 1> BeastCardQualityDisplays =
{{
    { "D", "9d9d9d" },
    { "C", "ffffff" },
    { "B", "1eff00" },
    { "A", "0070dd" },
    { "S", "a335ee" },
    { "SS", "ff8000" },
    { "SSR", "c69b6d" },
    { "SSS", "ff2020" },
    { "SR", "ff4fd8" },
    { "UR", "ff4fd8" }
}};

uint32 MakeQualityConfigKey(uint32 cardId, uint8 quality)
{
    return (cardId << 8) | quality;
}

BeastCardQualityTemplate const* GetQualityConfig(uint32 cardId, uint8 quality)
{
    auto itr = BeastCardQualityTemplateStore.find(MakeQualityConfigKey(cardId, quality));
    if (itr != BeastCardQualityTemplateStore.end())
        return &itr->second;

    itr = BeastCardQualityTemplateStore.find(MakeQualityConfigKey(0, quality));
    return itr != BeastCardQualityTemplateStore.end() ? &itr->second : nullptr;
}

char const* GetQualityName(uint8 quality)
{
    return BeastCardQualityDisplays[std::min<uint8>(quality, BEAST_CARD_MAX_QUALITY)].Name;
}

std::string GetColoredQualityName(uint8 quality)
{
    quality = std::min<uint8>(quality, BEAST_CARD_MAX_QUALITY);
    if (quality == BEAST_CARD_MAX_QUALITY)
        return "|cffff3030U|cff8f4fffR|r";

    return "|cff" + std::string(BeastCardQualityDisplays[quality].Color) + GetQualityName(quality) + "|r";
}

int64 AddInt64Saturated(int64 const& left, int64 const& right)
{
    int64 const maxValue = std::numeric_limits<int64>::max();
    int64 const minValue = std::numeric_limits<int64>::min();

    if (right > 0 && left > maxValue - right)
        return maxValue;
    if (right < 0 && left < minValue - right)
        return minValue;

    return left + right;
}

int64 ScaleInt64ByPercent(int64 const& baseValue, int64 const& percentValue)
{
    return Acore::Number::ToInt64Saturated(
        Acore::Number::ToLongDouble(baseValue) * Acore::Number::ToLongDouble(percentValue) / 100.0L);
}

bool TryParseCardId(std::string const& value, uint32& cardId)
{
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        return false;

    cardId = uint32(std::strtoul(value.c_str(), nullptr, 10));
    return cardId > 0 && cardId <= BEAST_CARD_MAX_COUNT;
}

bool TryParseUpgradeRequest(std::string const& value, uint32& cardId, uint8& quality)
{
    std::size_t separator = value.find('#');
    if (separator == std::string::npos || value.find('#', separator + 1) != std::string::npos)
        return false;

    std::string qualityValue = value.substr(separator + 1);
    if (!TryParseCardId(value.substr(0, separator), cardId) || qualityValue.empty() ||
        qualityValue.find_first_not_of("0123456789") != std::string::npos)
        return false;

    uint32 parsedQuality = uint32(std::strtoul(qualityValue.c_str(), nullptr, 10));
    if (parsedQuality > BEAST_CARD_MAX_QUALITY)
        return false;

    quality = uint8(parsedQuality);
    return true;
}

std::vector<uint32> ParseCardIdList(std::string const& value)
{
    std::vector<uint32> cardIds;
    std::unordered_set<uint32> seenCardIds;
    std::istringstream stream(value);
    std::string token;
    while (std::getline(stream, token, '#'))
    {
        token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char character) { return std::isspace(character); }), token.end());
        uint32 cardId = 0;
        if (TryParseCardId(token, cardId) && seenCardIds.insert(cardId).second)
            cardIds.push_back(cardId);
    }
    return cardIds;
}

std::vector<BeastCardEffectValue> ParseEffectValues(std::string value)
{
    std::vector<BeastCardEffectValue> values;
    std::replace(value.begin(), value.end(), '#', ',');
    std::replace(value.begin(), value.end(), ';', ',');

    std::istringstream stream(value);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        std::istringstream tokenStream(token);
        BeastCardEffectValue effectValue;
        std::string trailing;
        if (tokenStream >> effectValue.AttributeType >> effectValue.Value && !(tokenStream >> trailing))
            values.push_back(effectValue);
    }
    return values;
}

std::string SanitizeAddonField(std::string value)
{
    for (char& character : value)
        if (character == '#' || character == '$' || character == '\r')
            character = ' ';

    return value;
}

BeastCardOpenedMap GetOpenedCards(Player* player)
{
    BeastCardOpenedMap openedCards;
    if (!player)
        return openedCards;

    QueryResult result = CharacterDatabase.Query(
        "SELECT 卡牌编号, 品质等级 FROM _神兽翻牌存档 WHERE 玩家GUID = {}", player->GetGUID().GetCounter());
    if (!result)
        return openedCards;

    do
    {
        Field* fields = result->Fetch();
        uint32 cardId = fields[0].Get<uint32>();
        if (cardId > 0 && cardId <= BEAST_CARD_MAX_COUNT)
            openedCards[cardId] = std::min<uint8>(fields[1].Get<uint8>(), BEAST_CARD_MAX_QUALITY);
    } while (result->NextRow());

    return openedCards;
}

BeastCardKillProgressMap GetKillProgress(Player* player)
{
    BeastCardKillProgressMap progress;
    if (!player)
        return progress;

    QueryResult result = CharacterDatabase.Query(
        "SELECT 卡牌编号, 生物编号, 击杀数量 FROM _神兽翻牌击杀进度 WHERE 玩家GUID = {}",
        player->GetGUID().GetCounter());
    if (!result)
        return progress;

    do
    {
        Field* fields = result->Fetch();
        uint32 cardId = fields[0].Get<uint32>();
        if (cardId > 0 && cardId <= BEAST_CARD_MAX_COUNT)
            progress[cardId] = { fields[1].Get<uint32>(), fields[2].Get<uint32>() };
    } while (result->NextRow());

    return progress;
}

bool SupportsPercentAttribute(uint32 attributeType)
{
    switch (attributeType)
    {
        case ITEM_MOD_MANA:
        case ITEM_MOD_HEALTH:
        case 2: // 护甲
        case ITEM_MOD_AGILITY:
        case ITEM_MOD_STRENGTH:
        case ITEM_MOD_INTELLECT:
        case ITEM_MOD_SPIRIT:
        case ITEM_MOD_STAMINA:
        case ITEM_MOD_ATTACK_POWER:
        case ITEM_MOD_RANGED_ATTACK_POWER:
            return true;
        default:
            return false;
    }
}

bool ValidateEffect(BeastCardEffectTemplate& effect, std::string const& source)
{
    effect.ParsedValues = ParseEffectValues(effect.EffectValues);
    if (effect.EffectType > 4)
    {
        LOG_ERROR("server.loading", "{} 的效果类型 {} 无效，有效范围为 0-4", source, uint32(effect.EffectType));
        return false;
    }

    if (effect.EffectType == 0)
        return true;

    if (effect.ParsedValues.empty())
    {
        LOG_ERROR("server.loading", "{} 未配置有效效果值，格式应为 属性ID 数值,属性ID 数值", source);
        return false;
    }

    if (effect.EffectType == 1 || effect.EffectType == 3)
    {
        for (BeastCardEffectValue const& value : effect.ParsedValues)
        {
            if (value.AttributeType >= MAX_ITEM_MOD)
            {
                LOG_ERROR("server.loading", "{} 的属性ID {} 无效，有效范围为 0-{}", source, value.AttributeType, MAX_ITEM_MOD - 1);
                return false;
            }
            if (effect.EffectType == 3 && !SupportsPercentAttribute(value.AttributeType))
            {
                LOG_ERROR("server.loading", "{} 的属性ID {} 不支持百分比效果", source, value.AttributeType);
                return false;
            }
        }
        return true;
    }

    bool hasSkillValue = std::any_of(effect.ParsedValues.begin(), effect.ParsedValues.end(), [](BeastCardEffectValue const& value)
    {
        return value.AttributeType == 99;
    });
    if (!hasSkillValue)
    {
        LOG_ERROR("server.loading", "{} 的技能效果值必须使用 99 数值 格式", source);
        return false;
    }
    if (effect.SpellId && !sSpellMgr->GetSpellInfo(effect.SpellId))
    {
        LOG_ERROR("server.loading", "{} 配置的技能ID {} 不存在", source, effect.SpellId);
        return false;
    }

    return true;
}

std::string GetAttributeName(uint32 attributeType)
{
    switch (attributeType)
    {
        case ITEM_MOD_MANA: return "法力值";
        case ITEM_MOD_HEALTH: return "生命值";
        case 2: return "护甲";
        case ITEM_MOD_AGILITY: return "敏捷";
        case ITEM_MOD_STRENGTH: return "力量";
        case ITEM_MOD_INTELLECT: return "智力";
        case ITEM_MOD_SPIRIT: return "精神";
        case ITEM_MOD_STAMINA: return "耐力";
        case ITEM_MOD_DEFENSE_SKILL_RATING: return "防御等级";
        case ITEM_MOD_DODGE_RATING: return "躲闪等级";
        case ITEM_MOD_PARRY_RATING: return "招架等级";
        case ITEM_MOD_BLOCK_RATING: return "格挡等级";
        case ITEM_MOD_HIT_MELEE_RATING: return "近战命中等级";
        case ITEM_MOD_HIT_RANGED_RATING: return "远程命中等级";
        case ITEM_MOD_HIT_SPELL_RATING: return "法术命中等级";
        case ITEM_MOD_CRIT_MELEE_RATING: return "近战暴击等级";
        case ITEM_MOD_CRIT_RANGED_RATING: return "远程暴击等级";
        case ITEM_MOD_CRIT_SPELL_RATING: return "法术暴击等级";
        case ITEM_MOD_HASTE_MELEE_RATING: return "近战急速等级";
        case ITEM_MOD_HASTE_RANGED_RATING: return "远程急速等级";
        case ITEM_MOD_HASTE_SPELL_RATING: return "法术急速等级";
        case ITEM_MOD_HIT_RATING: return "命中等级";
        case ITEM_MOD_CRIT_RATING: return "暴击等级";
        case ITEM_MOD_RESILIENCE_RATING: return "韧性等级";
        case ITEM_MOD_HASTE_RATING: return "急速等级";
        case ITEM_MOD_EXPERTISE_RATING: return "精准等级";
        case ITEM_MOD_ATTACK_POWER: return "攻击强度";
        case ITEM_MOD_RANGED_ATTACK_POWER: return "远程攻击强度";
        case ITEM_MOD_MANA_REGENERATION: return "法力回复";
        case ITEM_MOD_ARMOR_PENETRATION_RATING: return "护甲穿透等级";
        case ITEM_MOD_SPELL_POWER: return "法术强度";
        case ITEM_MOD_HEALTH_REGEN: return "生命回复";
        case ITEM_MOD_SPELL_PENETRATION: return "法术穿透";
        case ITEM_MOD_BLOCK_VALUE: return "格挡值";
        default: return "属性 " + std::to_string(attributeType);
    }
}

std::string GetAttributeRewardText(BeastCardTemplate const& card)
{
    if (!card.HasAttributeReward())
        return "未配置属性奖励";
    if (!card.AttributeValid)
        return "属性配置无效";

    int32 attributeValue = card.AttributeValue;
    std::string value = attributeValue > 0 ? "+" + std::to_string(attributeValue) : std::to_string(attributeValue);
    if (card.AttributeMode == 1)
        value += "%";

    return GetAttributeName(card.AttributeType) + " " + value + (card.AttributeMode == 1 ? "（百分比）" : "（固定值）");
}

std::string GetEffectDescription(BeastCardEffectTemplate const* effect)
{
    if (!effect || !effect->Valid)
        return "未配置品质效果";
    if (!effect->Description.empty())
        return effect->Description;

    if (effect->EffectType == 0)
        return "无额外效果";

    if (effect->EffectType == 1 || effect->EffectType == 3)
    {
        std::string text;
        for (BeastCardEffectValue const& value : effect->ParsedValues)
        {
            if (!text.empty())
                text += "\n";
            text += GetAttributeName(value.AttributeType) + " " + (value.Value > 0 ? "+" : "") + std::to_string(value.Value);
            if (effect->EffectType == 3)
                text += "%";
        }
        return text.empty() ? "无额外效果" : text;
    }

    int64 bonus = 0;
    for (BeastCardEffectValue const& value : effect->ParsedValues)
        if (value.AttributeType == 99)
            bonus += value.Value;

    std::string spellName = effect->SpellId ? sHelper->GetSpellName(effect->SpellId) : "全部技能";
    return spellName + " " + (bonus > 0 ? "+" : "") + std::to_string(bonus) + (effect->EffectType == 4 ? "%" : "");
}

void SendFlipRejected(Player* player, uint32 cardId)
{
    if (player)
        sDoAddon->SendPacketToClient(player, "BeastCardFlipRejected", std::to_string(cardId));
}

void SendUpgradeRejected(Player* player, uint32 cardId)
{
    if (player)
        sDoAddon->SendPacketToClient(player, "BeastCardUpgradeRejected", std::to_string(cardId));
}
}

void BeastCardMgr::LoadConfig(bool isReload)
{
    BeastCardTemplateStore.clear();
    BeastCardQualityTemplateStore.clear();
    BeastCardBondTemplateStore.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT 卡牌编号, 卡牌名称, 卡牌介绍, 是否启用, 需求模板ID, 奖励模板ID, 击杀生物编号, 需要击杀数量, "
        "激活后奖励属性编号, 激活后奖励属性数值, 属性模式 FROM _神兽翻牌配置 ORDER BY 卡牌编号");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 cardId = fields[0].Get<uint32>();
            if (!cardId || cardId > BEAST_CARD_MAX_COUNT)
            {
                LOG_ERROR("server.loading", "神兽翻牌配置存在无效卡牌编号 {}，有效范围为 1-{}", cardId, BEAST_CARD_MAX_COUNT);
                continue;
            }

            BeastCardTemplate card;
            card.Name = fields[1].Get<std::string>();
            card.Description = fields[2].Get<std::string>();
            card.Enabled = fields[3].Get<uint8>() != 0;
            card.NeedTempId = fields[4].Get<int32>();
            card.RewardTempId = fields[5].Get<uint32>();
            card.CreatureEntry = fields[6].Get<int32>();
            card.RequiredKills = fields[7].Get<uint32>();
            card.AttributeType = fields[8].Get<uint32>();
            card.AttributeValue = fields[9].Get<int32>();
            card.AttributeMode = fields[10].Get<uint8>();

            if (card.Name.empty())
                card.Name = "神兽牌 " + (cardId < 10 ? std::string("0") : std::string()) + std::to_string(cardId);
            if (card.Description.empty())
                card.Description = "翻开「" + card.Name + "」后永久激活配置属性。";

            if (card.NeedTempId < -1)
                LOG_WARN("server.loading", "神兽牌 {} 的需求模板ID {} 无效，只有大于0时才启用需求条件", cardId, card.NeedTempId);
            if (card.CreatureEntry < -1)
                LOG_WARN("server.loading", "神兽牌 {} 的击杀生物编号 {} 无效，只有大于0时才启用击杀条件", cardId, card.CreatureEntry);
            if (card.CreatureEntry > 0 && card.RequiredKills == 0)
                LOG_WARN("server.loading", "神兽牌 {} 已配置击杀生物编号但击杀数量为0，将按无击杀门槛处理", cardId);
            if (card.CreatureEntry <= 0 && card.RequiredKills > 0)
                LOG_WARN("server.loading", "神兽牌 {} 未配置有效击杀生物编号，击杀数量配置将被忽略", cardId);

            if (card.AttributeMode > 1 || (card.HasAttributeReward() && card.AttributeType >= MAX_ITEM_MOD) ||
                (card.HasAttributeReward() && card.AttributeMode == 1 && !SupportsPercentAttribute(card.AttributeType)))
            {
                card.AttributeValid = false;
                LOG_ERROR("server.loading", "神兽牌 {} 的属性配置无效: 属性编号 {}, 数值 {}, 模式 {}", cardId,
                    card.AttributeType, card.AttributeValue, uint32(card.AttributeMode));
            }

            BeastCardTemplateStore[cardId] = std::move(card);
        } while (result->NextRow());
    }

    result = WorldDatabase.Query(
        "SELECT 卡牌编号, 品质等级, 是否启用, 升级需求模板ID, 技能ID, 效果类型, 效果值, 效果描述 "
        "FROM _神兽翻牌品质配置 ORDER BY 卡牌编号, 品质等级");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 cardId = fields[0].Get<uint32>();
            uint8 quality = fields[1].Get<uint8>();
            if (cardId > BEAST_CARD_MAX_COUNT || quality > BEAST_CARD_MAX_QUALITY)
            {
                LOG_ERROR("server.loading", "神兽翻牌品质配置无效: 卡牌编号 {}, 品质等级 {}", cardId, uint32(quality));
                continue;
            }

            BeastCardQualityTemplate qualityConfig;
            qualityConfig.Enabled = fields[2].Get<uint8>() != 0;
            qualityConfig.NeedTempId = fields[3].Get<uint32>();
            qualityConfig.Effect.SpellId = fields[4].Get<uint32>();
            qualityConfig.Effect.EffectType = fields[5].Get<uint8>();
            qualityConfig.Effect.EffectValues = fields[6].Get<std::string>();
            qualityConfig.Effect.Description = fields[7].Get<std::string>();
            qualityConfig.Effect.Valid = ValidateEffect(qualityConfig.Effect,
                "神兽牌 " + std::to_string(cardId) + " 品质 " + std::to_string(quality));
            if (!qualityConfig.Effect.Valid)
                qualityConfig.Enabled = false;

            BeastCardQualityTemplateStore[MakeQualityConfigKey(cardId, quality)] = qualityConfig;
        } while (result->NextRow());
    }

    result = WorldDatabase.Query(
        "SELECT 羁绊编号, 羁绊名称, 卡牌组合, 激活数量, 技能ID, 效果类型, 效果值, 效果描述, 是否启用, 排序 "
        "FROM _神兽翻牌羁绊配置 ORDER BY 排序, 羁绊编号");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            BeastCardBondTemplate bond;
            bond.Id = fields[0].Get<uint32>();
            bond.Name = fields[1].Get<std::string>();
            bond.CardIds = ParseCardIdList(fields[2].Get<std::string>());
            bond.RequiredCount = fields[3].Get<uint32>();
            bond.Effect.SpellId = fields[4].Get<uint32>();
            bond.Effect.EffectType = fields[5].Get<uint8>();
            bond.Effect.EffectValues = fields[6].Get<std::string>();
            bond.Effect.Description = fields[7].Get<std::string>();
            bond.Enabled = fields[8].Get<uint8>() != 0;
            bond.SortOrder = fields[9].Get<uint32>();

            if (!bond.Id)
            {
                LOG_ERROR("server.loading", "神兽翻牌羁绊存在无效羁绊编号 0");
                continue;
            }
            if (bond.Name.empty())
                bond.Name = "神兽羁绊 " + std::to_string(bond.Id);
            if (bond.CardIds.empty())
            {
                LOG_ERROR("server.loading", "神兽羁绊 {} 未配置有效卡牌组合", bond.Id);
                bond.Enabled = false;
            }
            if (!bond.RequiredCount)
                bond.RequiredCount = uint32(bond.CardIds.size());
            if (bond.RequiredCount > bond.CardIds.size())
            {
                LOG_ERROR("server.loading", "神兽羁绊 {} 需要激活数量 {} 超过组合卡牌数量 {}", bond.Id,
                    bond.RequiredCount, bond.CardIds.size());
                bond.Enabled = false;
            }
            bond.Effect.Valid = ValidateEffect(bond.Effect, "神兽羁绊 " + std::to_string(bond.Id));
            if (!bond.Effect.Valid)
                bond.Enabled = false;

            BeastCardBondTemplateStore[bond.Id] = std::move(bond);
        } while (result->NextRow());
    }

    if (!isReload)
    {
        LOG_INFO("server.loading", "功能表 >> 加载 [_神兽翻牌配置] 总计 {} 条数据", BeastCardTemplateStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_神兽翻牌品质配置] 总计 {} 条数据", BeastCardQualityTemplateStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_神兽翻牌羁绊配置] 总计 {} 条数据", BeastCardBondTemplateStore.size());
    }
}

void BeastCardMgr::Reload()
{
    LOG_INFO("server.loading", ">> 重载神兽翻牌、品质与羁绊配置...");
    LoadConfig(true);
    LOG_INFO("server.loading", ">> 重载神兽翻牌配置生效，在线角色需重新登录以刷新已激活属性");
}

bool BeastCardMgr::IsEnabled() const
{
    return sHelper->GetBoolean(BEAST_CARD_FEATURE_CONFIG_ID) && !BeastCardTemplateStore.empty();
}

bool BeastCardMgr::IsOpened(Player* player, uint32 cardId) const
{
    if (!player)
        return false;

    QueryResult result = CharacterDatabase.Query(
        "SELECT 1 FROM _神兽翻牌存档 WHERE 玩家GUID = {} AND 卡牌编号 = {} LIMIT 1",
        player->GetGUID().GetCounter(), cardId);
    return result != nullptr;
}

uint8 BeastCardMgr::GetQuality(Player* player, uint32 cardId) const
{
    if (!player)
        return 0;

    QueryResult result = CharacterDatabase.Query(
        "SELECT 品质等级 FROM _神兽翻牌存档 WHERE 玩家GUID = {} AND 卡牌编号 = {} LIMIT 1",
        player->GetGUID().GetCounter(), cardId);
    return result ? std::min<uint8>(result->Fetch()[0].Get<uint8>(), BEAST_CARD_MAX_QUALITY) : 0;
}

uint32 BeastCardMgr::GetKillCount(Player* player, uint32 cardId, uint32 creatureEntry) const
{
    if (!player || !creatureEntry)
        return 0;

    QueryResult result = CharacterDatabase.Query(
        "SELECT 击杀数量 FROM _神兽翻牌击杀进度 WHERE 玩家GUID = {} AND 卡牌编号 = {} AND 生物编号 = {} LIMIT 1",
        player->GetGUID().GetCounter(), cardId, creatureEntry);
    return result ? result->Fetch()[0].Get<uint32>() : 0;
}

bool BeastCardMgr::ApplyAttribute(Player* player, BeastCardTemplate const& card, bool apply) const
{
    if (!player || !card.HasAttributeReward())
        return true;
    if (!card.AttributeValid)
        return false;

    int32 attributeValue = card.AttributeValue;
    if (card.AttributeMode == 0)
    {
        if (card.AttributeType == 2)
            return player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, float(attributeValue), apply);
        player->DoApplyStat(ItemModType(card.AttributeType), attributeValue, apply);
        return true;
    }

    float value = float(attributeValue);
    switch (card.AttributeType)
    {
        case ITEM_MOD_MANA:
            return player->HandleStatModifier(UNIT_MOD_MANA, TOTAL_PCT, value, apply);
        case ITEM_MOD_HEALTH:
            return player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_PCT, value, apply);
        case 2:
            return player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_PCT, value, apply);
        case ITEM_MOD_AGILITY:
            return player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_PCT, value, apply);
        case ITEM_MOD_STRENGTH:
            return player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_PCT, value, apply);
        case ITEM_MOD_INTELLECT:
            return player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_PCT, value, apply);
        case ITEM_MOD_SPIRIT:
            return player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_PCT, value, apply);
        case ITEM_MOD_STAMINA:
            return player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_PCT, value, apply);
        case ITEM_MOD_ATTACK_POWER:
        {
            bool meleeApplied = player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, value, apply);
            bool rangedApplied = player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_PCT, value, apply);
            return meleeApplied && rangedApplied;
        }
        case ITEM_MOD_RANGED_ATTACK_POWER:
            return player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_PCT, value, apply);
        default:
            return false;
    }
}

bool BeastCardMgr::ApplyEffect(Player* player, BeastCardEffectTemplate const& effect, bool apply) const
{
    if (!player || !effect.Valid)
        return false;
    if (effect.EffectType == 0 || effect.EffectType == 2 || effect.EffectType == 4)
        return true;

    bool success = true;
    for (BeastCardEffectValue const& effectValue : effect.ParsedValues)
    {
        int64 clamped = std::clamp<int64>(effectValue.Value, std::numeric_limits<int32>::min(), std::numeric_limits<int32>::max());
        if (effect.EffectType == 1)
        {
            if (effectValue.AttributeType == 2)
            {
                success = player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, float(clamped), apply) && success;
                continue;
            }
            player->DoApplyStat(ItemModType(effectValue.AttributeType), int32(clamped), apply);
            continue;
        }

        float value = float(clamped);
        switch (effectValue.AttributeType)
        {
            case ITEM_MOD_MANA:
                success = player->HandleStatModifier(UNIT_MOD_MANA, TOTAL_PCT, value, apply) && success;
                break;
            case ITEM_MOD_HEALTH:
                success = player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_PCT, value, apply) && success;
                break;
            case 2:
                success = player->HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_PCT, value, apply) && success;
                break;
            case ITEM_MOD_AGILITY:
                success = player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_PCT, value, apply) && success;
                break;
            case ITEM_MOD_STRENGTH:
                success = player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_PCT, value, apply) && success;
                break;
            case ITEM_MOD_INTELLECT:
                success = player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_PCT, value, apply) && success;
                break;
            case ITEM_MOD_SPIRIT:
                success = player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_PCT, value, apply) && success;
                break;
            case ITEM_MOD_STAMINA:
                success = player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_PCT, value, apply) && success;
                break;
            case ITEM_MOD_ATTACK_POWER:
                success = player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, value, apply) && success;
                success = player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_PCT, value, apply) && success;
                break;
            case ITEM_MOD_RANGED_ATTACK_POWER:
                success = player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_PCT, value, apply) && success;
                break;
            default:
                success = false;
                break;
        }
    }

    return success;
}

void BeastCardMgr::RebuildSkillBonusCache(Player* player) const
{
    if (!player)
        return;

    BeastCardSkillBonusCache bonus;
    auto addEffect = [&bonus](BeastCardEffectTemplate const& effect)
    {
        if (!effect.Valid || (effect.EffectType != 2 && effect.EffectType != 4))
            return;

        int64 effectValue = 0;
        for (BeastCardEffectValue const& value : effect.ParsedValues)
            if (value.AttributeType == 99)
                effectValue = AddInt64Saturated(effectValue, int64(value.Value));

        bool isPercent = effect.EffectType == 4;
        if (!effect.SpellId)
        {
            int64& target = isPercent ? bonus.PercentAll : bonus.FixedAll;
            target = AddInt64Saturated(target, effectValue);
            return;
        }

        auto& entry = bonus.PerSpell[effect.SpellId];
        int64& target = isPercent ? entry.second : entry.first;
        target = AddInt64Saturated(target, effectValue);
    };

    BeastCardOpenedMap openedCards = GetOpenedCards(player);
    for (auto const& [cardId, quality] : openedCards)
        if (BeastCardQualityTemplate const* qualityConfig = GetQualityConfig(cardId, quality); qualityConfig && qualityConfig->Enabled)
            addEffect(qualityConfig->Effect);

    for (auto const& [bondId, bond] : BeastCardBondTemplateStore)
        if (IsBondActive(bond, openedCards))
            addEffect(bond.Effect);

    std::lock_guard<std::mutex> lock(BeastCardSkillBonusMutex);
    BeastCardSkillBonusStore.insert_or_assign(player->GetGUID().GetCounter(), std::move(bonus));
}

std::pair<int64, int64> BeastCardMgr::GetSkillBonus(Player* player, uint32 spellId) const
{
    if (!player)
        return { 0, 0 };

    std::lock_guard<std::mutex> lock(BeastCardSkillBonusMutex);
    auto itr = BeastCardSkillBonusStore.find(player->GetGUID().GetCounter());
    if (itr == BeastCardSkillBonusStore.end())
        return { 0, 0 };

    int64 fixed = itr->second.FixedAll;
    int64 percent = itr->second.PercentAll;
    auto spellItr = itr->second.PerSpell.find(spellId);
    if (spellItr != itr->second.PerSpell.end())
    {
        fixed = AddInt64Saturated(fixed, spellItr->second.first);
        percent = AddInt64Saturated(percent, spellItr->second.second);
    }
    return { fixed, percent };
}

void BeastCardMgr::ClearPlayerEffectCache(Player* player) const
{
    if (!player)
        return;

    std::lock_guard<std::mutex> lock(BeastCardSkillBonusMutex);
    BeastCardSkillBonusStore.erase(player->GetGUID().GetCounter());
}

void BeastCardMgr::ApplyOpenedCardAttributes(Player* player) const
{
    if (!player || !IsEnabled())
        return;

    BeastCardOpenedMap openedCards = GetOpenedCards(player);
    for (auto const& [cardId, quality] : openedCards)
    {
        auto itr = BeastCardTemplateStore.find(cardId);
        if (itr != BeastCardTemplateStore.end() && itr->second.Enabled)
        {
            ApplyAttribute(player, itr->second);
            if (BeastCardQualityTemplate const* qualityConfig = GetQualityConfig(cardId, quality); qualityConfig && qualityConfig->Enabled)
                ApplyEffect(player, qualityConfig->Effect);
        }
    }

    RebuildSkillBonusCache(player);
}

bool BeastCardMgr::IsBondActive(BeastCardBondTemplate const& bond, std::unordered_map<uint32, uint8> const& openedCards,
    uint32* openedCount) const
{
    uint32 count = 0;
    for (uint32 cardId : bond.CardIds)
        if (openedCards.find(cardId) != openedCards.end())
            ++count;

    if (openedCount)
        *openedCount = count;
    return bond.Enabled && count >= bond.RequiredCount;
}

void BeastCardMgr::ApplyBondEffects(Player* player, bool isLogin, uint32 newlyOpenedCardId) const
{
    if (!player || !IsEnabled())
        return;

    BeastCardOpenedMap openedCards = GetOpenedCards(player);
    for (auto const& [bondId, bond] : BeastCardBondTemplateStore)
    {
        uint32 openedCount = 0;
        if (!IsBondActive(bond, openedCards, &openedCount))
            continue;

        bool newlyActivated = newlyOpenedCardId && openedCount == bond.RequiredCount &&
            std::find(bond.CardIds.begin(), bond.CardIds.end(), newlyOpenedCardId) != bond.CardIds.end();
        if (!isLogin && !newlyActivated)
            continue;

        ApplyEffect(player, bond.Effect);
        if (!isLogin)
            player->SendSysMsg("达成神兽羁绊 [%s]：%s", bond.Name.c_str(), GetEffectDescription(&bond.Effect).c_str());
    }

    RebuildSkillBonusCache(player);
}

void BeastCardMgr::SendState(Player* player) const
{
    if (!player)
        return;

    if (!IsEnabled())
    {
        sDoAddon->SendPacketToClient(player, "BeastCardStateBegin", "0#0");
        sDoAddon->SendPacketToClient(player, "BeastCardStateEnd");
        return;
    }

    BeastCardOpenedMap openedCards = GetOpenedCards(player);
    BeastCardKillProgressMap killProgress = GetKillProgress(player);
    uint32 enabledCount = 0;
    uint32 openedCount = 0;

    for (auto const& [cardId, card] : BeastCardTemplateStore)
    {
        if (!card.Enabled)
            continue;

        ++enabledCount;
        if (openedCards.find(cardId) != openedCards.end())
            ++openedCount;
    }

    sDoAddon->SendPacketToClient(player, "BeastCardStateBegin", std::to_string(enabledCount) + "#" + std::to_string(openedCount));

    for (uint32 cardId = 1; cardId <= BEAST_CARD_MAX_COUNT; ++cardId)
    {
        auto itr = BeastCardTemplateStore.find(cardId);
        if (itr == BeastCardTemplateStore.end())
            continue;

        BeastCardTemplate const& card = itr->second;
        auto openedItr = openedCards.find(cardId);
        bool opened = openedItr != openedCards.end();
        uint8 quality = opened ? openedItr->second : 0;
        uint32 killNeed = card.HasKillRequirement() ? card.RequiredKills : 0;
        uint32 killCount = 0;
        auto progressItr = killProgress.find(cardId);
        if (card.HasKillRequirement() && progressItr != killProgress.end() &&
            progressItr->second.first == static_cast<uint32>(card.CreatureEntry))
            killCount = progressItr->second.second;

        bool ready = card.Enabled && !opened && (!card.HasKillRequirement() || killCount >= card.RequiredKills);
        BeastCardQualityTemplate const* nextQualityConfig = quality < BEAST_CARD_MAX_QUALITY ?
            GetQualityConfig(cardId, quality + 1) : nullptr;
        bool canUpgrade = opened && nextQualityConfig && nextQualityConfig->Enabled;
        sDoAddon->SendPacketToClient(player, "BeastCardStateCard",
            std::to_string(cardId) + "#" + std::to_string(card.Enabled ? 1 : 0) + "#" + std::to_string(ready ? 1 : 0) + "#" +
            std::to_string(opened ? 1 : 0) + "#" + std::to_string(killCount) + "#" + std::to_string(killNeed) + "#" +
            SanitizeAddonField(card.Name) + "#" + std::to_string(quality) + "#" + std::to_string(canUpgrade ? 1 : 0));
    }

    sDoAddon->SendPacketToClient(player, "BeastCardStateEnd");
}

void BeastCardMgr::SendCardDetail(Player* player, uint32 cardId) const
{
    auto itr = BeastCardTemplateStore.find(cardId);
    if (!player || itr == BeastCardTemplateStore.end())
        return;

    BeastCardTemplate const& card = itr->second;
    uint32 killNeed = card.HasKillRequirement() ? card.RequiredKills : 0;
    uint32 killCount = card.HasKillRequirement() ? GetKillCount(player, cardId, static_cast<uint32>(card.CreatureEntry)) : 0;
    bool opened = IsOpened(player, cardId);
    uint8 quality = opened ? GetQuality(player, cardId) : 0;
    bool ready = card.Enabled && !opened && (!card.HasKillRequirement() || killCount >= card.RequiredKills);
    BeastCardQualityTemplate const* nextQualityConfig = quality < BEAST_CARD_MAX_QUALITY ?
        GetQualityConfig(cardId, quality + 1) : nullptr;
    bool canUpgrade = opened && nextQualityConfig && nextQualityConfig->Enabled;

    std::string needText = card.HasNeedRequirement() ?
        sHelper->GetNeedGts(static_cast<uint32>(card.NeedTempId)) : "无需额外消耗";
    std::string rewardText = card.RewardTempId ? sHelper->GetRewardGts(card.RewardTempId) : "无物品奖励";
    if (card.HasAttributeReward())
        rewardText += "\n永久属性：" + GetAttributeRewardText(card);

    BeastCardQualityTemplate const* currentQualityConfig = GetQualityConfig(cardId, quality);
    std::string currentEffectText = currentQualityConfig && currentQualityConfig->Enabled ?
        GetEffectDescription(&currentQualityConfig->Effect) : "未配置品质效果";
    std::string upgradeNeedText = "已达最高品质";
    std::string nextEffectText = "UR品质已满";
    if (quality < BEAST_CARD_MAX_QUALITY)
    {
        if (!nextQualityConfig || !nextQualityConfig->Enabled)
        {
            upgradeNeedText = "下一品质尚未开放";
            nextEffectText = "暂无下一品质配置";
        }
        else
        {
            uint32 upgradeNeedTempId = nextQualityConfig->NeedTempId ? nextQualityConfig->NeedTempId :
                (card.HasNeedRequirement() ? static_cast<uint32>(card.NeedTempId) : 0);
            upgradeNeedText = upgradeNeedTempId ? sHelper->GetNeedGts(upgradeNeedTempId) : "免费升级";
            nextEffectText = GetEffectDescription(&nextQualityConfig->Effect);
        }
    }
    std::string creatureText = "无需击杀";
    if (card.HasKillRequirement())
    {
        if (CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(static_cast<uint32>(card.CreatureEntry)))
            creatureText = creatureTemplate->Name + "（ID " + std::to_string(card.CreatureEntry) + "）";
        else
            creatureText = "生物 ID " + std::to_string(card.CreatureEntry);
    }

    sDoAddon->SendPacketToClient(player, "BeastCardDetail",
        std::to_string(cardId) + "#" + SanitizeAddonField(card.Name) + "#" + SanitizeAddonField(card.Description) + "#" +
        SanitizeAddonField(needText) + "#" + SanitizeAddonField(rewardText) + "#" + SanitizeAddonField(creatureText) + "#" +
        std::to_string(killCount) + "#" + std::to_string(killNeed) + "#" +
        SanitizeAddonField(currentEffectText) + "#0#" +
        std::to_string(opened ? 1 : 0) + "#" + std::to_string(ready ? 1 : 0) + "#" + std::to_string(quality) + "#" +
        std::to_string(canUpgrade ? 1 : 0) + "#" + SanitizeAddonField(upgradeNeedText) + "#" +
        SanitizeAddonField(nextEffectText));
}

void BeastCardMgr::SendBondState(Player* player) const
{
    if (!player)
        return;

    BeastCardOpenedMap openedCards = GetOpenedCards(player);
    std::vector<BeastCardBondTemplate const*> bonds;
    for (auto const& [bondId, bond] : BeastCardBondTemplateStore)
        if (bond.Enabled)
            bonds.push_back(&bond);

    std::sort(bonds.begin(), bonds.end(), [](BeastCardBondTemplate const* left, BeastCardBondTemplate const* right)
    {
        if (left->SortOrder != right->SortOrder)
            return left->SortOrder < right->SortOrder;
        return left->Id < right->Id;
    });

    sDoAddon->SendPacketToClient(player, "BeastCardBondBegin", std::to_string(bonds.size()));
    for (BeastCardBondTemplate const* bond : bonds)
    {
        uint32 openedCount = 0;
        bool active = IsBondActive(*bond, openedCards, &openedCount);
        std::string cardText;
        for (uint32 cardId : bond->CardIds)
        {
            if (!cardText.empty())
                cardText += "、";

            auto cardItr = BeastCardTemplateStore.find(cardId);
            std::string cardName = cardItr != BeastCardTemplateStore.end() ? cardItr->second.Name : "神兽牌 " + std::to_string(cardId);
            cardText += openedCards.find(cardId) != openedCards.end() ? "|cff55ff77" + cardName + "|r" : "|cff888888" + cardName + "|r";
        }

        std::string description = GetEffectDescription(&bond->Effect);

        sDoAddon->SendPacketToClient(player, "BeastCardBondData",
            std::to_string(bond->Id) + "#" + std::to_string(active ? 1 : 0) + "#" + std::to_string(openedCount) + "#" +
            std::to_string(bond->RequiredCount) + "#" + std::to_string(bond->CardIds.size()) + "#" +
            SanitizeAddonField(bond->Name) + "#" + SanitizeAddonField(cardText) + "#" + SanitizeAddonField(description));
    }
    sDoAddon->SendPacketToClient(player, "BeastCardBondEnd");
}

void BeastCardMgr::FlipCard(Player* player, uint32 cardId)
{
    if (!player)
        return;

    if (!IsEnabled())
    {
        SendFlipRejected(player, cardId);
        return;
    }

    auto itr = BeastCardTemplateStore.find(cardId);
    if (itr == BeastCardTemplateStore.end() || !itr->second.Enabled)
    {
        player->SendSysMsg("该神兽牌尚未开放。");
        SendFlipRejected(player, cardId);
        return;
    }

    BeastCardTemplate const& card = itr->second;
    if (card.RewardTempId && RewardTempStore.find(card.RewardTempId) == RewardTempStore.end())
    {
        player->SendSysMsg("该神兽牌的奖励模板未配置，请联系管理员。");
        SendFlipRejected(player, cardId);
        return;
    }

    if (card.HasNeedRequirement() && NeedTempStore.find(static_cast<uint32>(card.NeedTempId)) == NeedTempStore.end())
    {
        player->SendSysMsg("该神兽牌的需求模板未配置，请联系管理员。");
        SendFlipRejected(player, cardId);
        return;
    }

    if (card.HasAttributeReward() && !card.AttributeValid)
    {
        player->SendSysMsg("该神兽牌的属性奖励配置无效，请联系管理员。");
        SendFlipRejected(player, cardId);
        return;
    }

    if (IsOpened(player, cardId))
    {
        player->SendSysMsg("该神兽牌已经翻开。");
        SendState(player);
        return;
    }

    if (card.HasKillRequirement() &&
        GetKillCount(player, cardId, static_cast<uint32>(card.CreatureEntry)) < card.RequiredKills)
    {
        player->SendSysMsg("该神兽牌的击杀条件尚未达成。");
        SendFlipRejected(player, cardId);
        SendCardDetail(player, cardId);
        return;
    }

    if (card.HasNeedRequirement() &&
        !sHelper->NotDeleteCheckNeed(player, static_cast<uint32>(card.NeedTempId)))
    {
        SendFlipRejected(player, cardId);
        return;
    }

    // 先落库再扣除，避免客户端重复发包造成重复奖励。
    CharacterDatabase.DirectExecute("INSERT INTO _神兽翻牌存档 (玩家GUID, 卡牌编号, 品质等级) VALUES ({}, {}, 0)",
        player->GetGUID().GetCounter(), cardId);
    if (!IsOpened(player, cardId))
    {
        player->SendSysMsg("神兽翻牌存档失败，本次未扣除任何消耗。");
        SendFlipRejected(player, cardId);
        return;
    }

    if (card.HasNeedRequirement() &&
        !sHelper->CheckNeed(player, static_cast<uint32>(card.NeedTempId), "神兽翻牌消耗"))
    {
        CharacterDatabase.DirectExecute("DELETE FROM _神兽翻牌存档 WHERE 玩家GUID = {} AND 卡牌编号 = {}",
            player->GetGUID().GetCounter(), cardId);
        SendFlipRejected(player, cardId);
        return;
    }

    if (card.RewardTempId)
        sHelper->DoTheReward(player, card.RewardTempId, "神兽翻牌奖励");
    ApplyAttribute(player, card);
    if (BeastCardQualityTemplate const* qualityConfig = GetQualityConfig(cardId, 0); qualityConfig && qualityConfig->Enabled)
        ApplyEffect(player, qualityConfig->Effect);
    ApplyBondEffects(player, false, cardId);

    BeastCardQualityTemplate const* nextQualityConfig = GetQualityConfig(cardId, 1);
    bool canUpgrade = nextQualityConfig && nextQualityConfig->Enabled;
    player->SendSysMsg("神兽牌 [%s] 已激活，当前品质：%s。", card.Name.c_str(), GetColoredQualityName(0).c_str());
    sDoAddon->SendPacketToClient(player, "BeastCardFlipResult",
        std::to_string(cardId) + "#0#" + std::to_string(canUpgrade ? 1 : 0));
}

void BeastCardMgr::UpgradeCard(Player* player, uint32 cardId, uint8 expectedQuality)
{
    if (!player || !IsEnabled())
    {
        SendUpgradeRejected(player, cardId);
        return;
    }

    auto itr = BeastCardTemplateStore.find(cardId);
    if (itr == BeastCardTemplateStore.end() || !itr->second.Enabled || !IsOpened(player, cardId))
    {
        player->SendSysMsg("该神兽牌尚未激活，无法提升品质。");
        SendUpgradeRejected(player, cardId);
        return;
    }

    BeastCardTemplate const& card = itr->second;
    uint8 oldQuality = GetQuality(player, cardId);
    if (oldQuality != expectedQuality)
    {
        player->SendSysMsg("神兽牌品质状态已变化，请重新操作。");
        SendUpgradeRejected(player, cardId);
        SendCardDetail(player, cardId);
        return;
    }

    if (oldQuality >= BEAST_CARD_MAX_QUALITY)
    {
        player->SendSysMsg("该神兽牌已达到UR最高品质。");
        SendUpgradeRejected(player, cardId);
        return;
    }

    uint8 newQuality = oldQuality + 1;
    BeastCardQualityTemplate const* qualityConfig = GetQualityConfig(cardId, newQuality);
    if (!qualityConfig || !qualityConfig->Enabled)
    {
        player->SendSysMsg("该神兽牌的下一品质尚未开放。");
        SendUpgradeRejected(player, cardId);
        return;
    }

    uint32 needTempId = qualityConfig->NeedTempId ? qualityConfig->NeedTempId :
        (card.HasNeedRequirement() ? static_cast<uint32>(card.NeedTempId) : 0);
    if (needTempId && NeedTempStore.find(needTempId) == NeedTempStore.end())
    {
        player->SendSysMsg("该神兽牌的品质升级需求模板未配置，请联系管理员。");
        SendUpgradeRejected(player, cardId);
        return;
    }

    if (needTempId && !sHelper->NotDeleteCheckNeed(player, needTempId))
    {
        SendUpgradeRejected(player, cardId);
        return;
    }

    // 用客户端携带的旧品质作为条件，重复发包不会连续跳过多个品质。
    CharacterDatabase.DirectExecute(
        "UPDATE _神兽翻牌存档 SET 品质等级 = {} WHERE 玩家GUID = {} AND 卡牌编号 = {} AND 品质等级 = {}",
        uint32(newQuality), player->GetGUID().GetCounter(), cardId, uint32(oldQuality));
    if (GetQuality(player, cardId) != newQuality)
    {
        player->SendSysMsg("神兽牌品质存档失败，本次未扣除任何消耗。");
        SendUpgradeRejected(player, cardId);
        return;
    }

    if (needTempId && !sHelper->CheckNeed(player, needTempId, "神兽品质升级消耗"))
    {
        CharacterDatabase.DirectExecute(
            "UPDATE _神兽翻牌存档 SET 品质等级 = {} WHERE 玩家GUID = {} AND 卡牌编号 = {} AND 品质等级 = {}",
            uint32(oldQuality), player->GetGUID().GetCounter(), cardId, uint32(newQuality));
        SendUpgradeRejected(player, cardId);
        return;
    }

    if (BeastCardQualityTemplate const* oldQualityConfig = GetQualityConfig(cardId, oldQuality);
        oldQualityConfig && oldQualityConfig->Enabled)
        ApplyEffect(player, oldQualityConfig->Effect, false);
    ApplyEffect(player, qualityConfig->Effect);
    RebuildSkillBonusCache(player);

    BeastCardQualityTemplate const* followingQualityConfig = newQuality < BEAST_CARD_MAX_QUALITY ?
        GetQualityConfig(cardId, newQuality + 1) : nullptr;
    bool canUpgradeAgain = followingQualityConfig && followingQualityConfig->Enabled;

    player->SendSysMsg("神兽牌 [%s] 品质提升至 %s。", card.Name.c_str(), GetColoredQualityName(newQuality).c_str());
    sDoAddon->SendPacketToClient(player, "BeastCardUpgradeResult",
        std::to_string(cardId) + "#" + std::to_string(newQuality) + "#" + std::to_string(canUpgradeAgain ? 1 : 0));
}

void BeastCardMgr::OnCreatureKill(Player* player, Creature* creature) const
{
    if (!player || !creature || !IsEnabled())
        return;

    uint32 creatureEntry = creature->GetEntry();
    BeastCardOpenedMap openedCards = GetOpenedCards(player);
    for (auto const& [cardId, card] : BeastCardTemplateStore)
    {
        if (!card.Enabled || !card.HasKillRequirement() || static_cast<uint32>(card.CreatureEntry) != creatureEntry ||
            openedCards.find(cardId) != openedCards.end())
            continue;

        uint32 oldKillCount = GetKillCount(player, cardId, creatureEntry);
        if (oldKillCount >= card.RequiredKills)
            continue;

        CharacterDatabase.DirectExecute(
            "INSERT INTO _神兽翻牌击杀进度 (玩家GUID, 卡牌编号, 生物编号, 击杀数量) VALUES ({}, {}, {}, 1) "
            "ON DUPLICATE KEY UPDATE 击杀数量 = IF(生物编号 = VALUES(生物编号), LEAST(击杀数量 + 1, {}), 1), "
            "生物编号 = VALUES(生物编号)",
            player->GetGUID().GetCounter(), cardId, creatureEntry, card.RequiredKills);

        uint32 newKillCount = GetKillCount(player, cardId, creatureEntry);
        bool ready = newKillCount >= card.RequiredKills;
        sDoAddon->SendPacketToClient(player, "BeastCardKillProgress",
            std::to_string(cardId) + "#" + std::to_string(newKillCount) + "#" +
            std::to_string(card.RequiredKills) + "#" + std::to_string(ready ? 1 : 0));

        if (ready && oldKillCount < card.RequiredKills)
            player->SendSysMsg("神兽牌 [%s] 的击杀条件已达成，可以翻牌。", card.Name.c_str());
    }
}

bool BeastCardMgr::HandleAddonMessage(Player* player, std::string const& header, std::string const& message)
{
    if (header == "BeastCardState")
    {
        SendState(player);
        return true;
    }

    if (header == "BeastCardBondState")
    {
        SendBondState(player);
        return true;
    }

    uint32 cardId = 0;
    if (header == "BeastCardDetail")
    {
        if (TryParseCardId(message, cardId))
            SendCardDetail(player, cardId);
        return true;
    }

    if (header == "BeastCardFlip")
    {
        if (TryParseCardId(message, cardId))
            FlipCard(player, cardId);
        return true;
    }

    if (header == "BeastCardUpgrade")
    {
        uint8 quality = 0;
        if (TryParseUpgradeRequest(message, cardId, quality))
            UpgradeCard(player, cardId, quality);
        return true;
    }

    return false;
}

void BeastCardMgr::ShowUi(Player* player)
{
    if (!player)
        return;

    if (!IsEnabled())
    {
        player->SendSysMsg("神兽翻牌功能暂未开启。");
        return;
    }

    sDoAddon->SendPacketToClient(player, "OpenBeastCardUI");
}

class beast_card_player_script : public PlayerScript
{
public:
    beast_card_player_script() : PlayerScript("beast_card_player_script") { }

    void OnLogin(Player* player) override
    {
        sBeastCardMgr->ClearPlayerEffectCache(player);
        sBeastCardMgr->ApplyOpenedCardAttributes(player);
        sBeastCardMgr->ApplyBondEffects(player, true);
    }

    void OnLogout(Player* player) override
    {
        sBeastCardMgr->ClearPlayerEffectCache(player);
    }

    void OnCreatureKill(Player* player, Creature* creature) override
    {
        sBeastCardMgr->OnCreatureKill(player, creature);
    }
};

class beast_card_unit_script : public UnitScript
{
public:
    beast_card_unit_script() : UnitScript("beast_card_unit_script") { }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!attacker || !attacker->IsPlayer() || !spellInfo)
            return;

        auto const [fixedBonus, percentBonus] = sBeastCardMgr->GetSkillBonus(attacker->ToPlayer(), spellInfo->Id);
        if (fixedBonus == 0 && percentBonus == 0)
            return;

        uint32 compatDamage = uint32(std::max<int32>(0, damage));
        AsInt64 baseDamage = attacker->PeekAsDamageOverride(target, compatDamage);
        AsInt64 finalDamage = ClampAsInt256(boost::multiprecision::int512_t(baseDamage) + fixedBonus + MultiplyDivideAsInt256(baseDamage, percentBonus, 100));
        if (finalDamage < 0)
            finalDamage = 0;

        uint32 compatFinalDamage = AsInt64ToUInt32Clamp(finalDamage);
        damage = compatFinalDamage > uint32(std::numeric_limits<int32>::max()) ?
            std::numeric_limits<int32>::max() : int32(compatFinalDamage);
        if (target)
            attacker->SetAsDamageOverride(target->GetGUID(), finalDamage > damage ? finalDamage : AsInt64Zero());
    }

    void ModifyHealRecieved(Unit* healer, Unit* target, uint32 castId, uint32& heal) override
    {
        if (!healer || !healer->IsPlayer() || !castId)
            return;

        auto const [fixedBonus, percentBonus] = sBeastCardMgr->GetSkillBonus(healer->ToPlayer(), castId);
        if (fixedBonus == 0 && percentBonus == 0)
            return;

        AsInt64 baseHeal = healer->PeekAsHealOverride(target, heal);
        AsInt64 finalHeal = ClampAsInt256(boost::multiprecision::int512_t(baseHeal) + fixedBonus + MultiplyDivideAsInt256(baseHeal, percentBonus, 100));
        if (finalHeal < 0)
            finalHeal = 0;

        heal = AsInt64ToUInt32Clamp(finalHeal);
        if (target)
            healer->SetAsHealOverride(target->GetGUID(), finalHeal > heal ? finalHeal : AsInt64Zero());
    }
};

class beast_card_commandscript : public CommandScript
{
public:
    beast_card_commandscript() : CommandScript("beast_card_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "beastcard", HandleBeastCardCommand, SEC_PLAYER, Console::No },
        };
        return commandTable;
    }

    static bool HandleBeastCardCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler && handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        sBeastCardMgr->ShowUi(player);
        return true;
    }
};

class npc_beast_card : public CreatureScript
{
public:
    npc_beast_card() : CreatureScript("npc_beast_card") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        sBeastCardMgr->ShowUi(player);
        CloseGossipMenuFor(player);
        return true;
    }
};

void AddSC_BeastCardMgr()
{
    new beast_card_player_script();
    new beast_card_unit_script();
    new beast_card_commandscript();
    new npc_beast_card();
}
