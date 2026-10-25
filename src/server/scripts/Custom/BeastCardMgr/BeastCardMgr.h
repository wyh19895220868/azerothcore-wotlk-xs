#ifndef _BEASTCARDMGR_H
#define _BEASTCARDMGR_H

#include "Common.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Creature;
class Player;

constexpr uint32 BEAST_CARD_FEATURE_CONFIG_ID = 1423;
constexpr uint32 BEAST_CARD_MAX_COUNT = 72;
constexpr uint8 BEAST_CARD_MAX_QUALITY = 9;

struct BeastCardTemplate
{
    bool Enabled = false;
    bool AttributeValid = true;
    std::string Name;
    std::string Description;
    int32 NeedTempId = -1;
    uint32 RewardTempId = 0;
    int32 CreatureEntry = -1;
    uint32 RequiredKills = 0;
    uint32 AttributeType = 1;
    int32 AttributeValue = 0;
    uint8 AttributeMode = 0;

    bool HasNeedRequirement() const { return NeedTempId > 0; }
    bool HasKillRequirement() const { return CreatureEntry > 0 && RequiredKills > 0; }
    bool HasAttributeReward() const { return AttributeValue != 0; }
};

struct BeastCardEffectValue
{
    uint32 AttributeType = 0;
    int64 Value = 0;
};

struct BeastCardEffectTemplate
{
    bool Valid = true;
    uint32 SpellId = 0;
    uint8 EffectType = 0;
    std::string EffectValues;
    std::string Description;
    std::vector<BeastCardEffectValue> ParsedValues;
};

struct BeastCardQualityTemplate
{
    bool Enabled = false;
    uint32 NeedTempId = 0;
    BeastCardEffectTemplate Effect;
};

using BeastCardTemplateMap = std::unordered_map<uint32, BeastCardTemplate>;
extern BeastCardTemplateMap BeastCardTemplateStore;

using BeastCardQualityTemplateMap = std::unordered_map<uint32, BeastCardQualityTemplate>;
extern BeastCardQualityTemplateMap BeastCardQualityTemplateStore;

struct BeastCardBondTemplate
{
    bool Enabled = false;
    uint32 Id = 0;
    std::string Name;
    std::vector<uint32> CardIds;
    uint32 RequiredCount = 0;
    BeastCardEffectTemplate Effect;
    uint32 SortOrder = 0;
};

using BeastCardBondTemplateMap = std::unordered_map<uint32, BeastCardBondTemplate>;
extern BeastCardBondTemplateMap BeastCardBondTemplateStore;

class BeastCardMgr
{
public:
    static BeastCardMgr* Instance()
    {
        static BeastCardMgr instance;
        return &instance;
    }

    void LoadConfig(bool isReload = false);
    void Reload();

    bool HandleAddonMessage(Player* player, std::string const& header, std::string const& message);
    void ShowUi(Player* player);
    void ApplyOpenedCardAttributes(Player* player) const;
    void ApplyBondEffects(Player* player, bool isLogin = false, uint32 newlyOpenedCardId = 0) const;
    void ClearPlayerEffectCache(Player* player) const;
    std::pair<int64, int64> GetSkillBonus(Player* player, uint32 spellId) const;
    void OnCreatureKill(Player* player, Creature* creature) const;

private:
    bool IsEnabled() const;
    bool IsOpened(Player* player, uint32 cardId) const;
    uint8 GetQuality(Player* player, uint32 cardId) const;
    uint32 GetKillCount(Player* player, uint32 cardId, uint32 creatureEntry) const;
    bool ApplyAttribute(Player* player, BeastCardTemplate const& card, bool apply = true) const;
    bool ApplyEffect(Player* player, BeastCardEffectTemplate const& effect, bool apply = true) const;
    void RebuildSkillBonusCache(Player* player) const;
    void SendState(Player* player) const;
    void SendCardDetail(Player* player, uint32 cardId) const;
    void SendBondState(Player* player) const;
    void FlipCard(Player* player, uint32 cardId);
    void UpgradeCard(Player* player, uint32 cardId, uint8 expectedQuality);
    bool IsBondActive(BeastCardBondTemplate const& bond, std::unordered_map<uint32, uint8> const& openedCards,
        uint32* openedCount = nullptr) const;
};

#define sBeastCardMgr BeastCardMgr::Instance()

#endif
