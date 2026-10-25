
#ifndef _AURASPELLMGR_H
#define _AURASPELLMGR_H

#include "../_Helper/AsInt64.h"

#define CtMiscValue -1000  //自定义转法术强度用

enum AuraTriggerIgnoreMask
{
    AURA_TRIGGER_IGNORE_BOSS = 1,
    AURA_TRIGGER_IGNORE_DUNGEON = 2,
    AURA_TRIGGER_IGNORE_NO_DUNGEON = 4,
    AURA_TRIGGER_IGNORE_PLAYER = 8,
    AURA_TRIGGER_IGNORE_UNIT = 16,
};

struct AsTemp
{
    float XMod;
    float PVEDmgBeforeMod;
    float PVEDmgMod;
    float PVERetMod;
    float PVPDmgMod;
    float PVPRetMod;
    float LootOddsMod;
    float LootOddsKnMod;
    float LootCountOdds;
    uint32 LootCountMod;
    float AXsMod;
    float SXsMod;
    float StatMod;
    float PVPMelee;
    float PVPSpell;
    float PVEMelee;
    float PVESpell;
    AsInt64 GMDmg;
    float LootChance;
    uint32 LootId;
    float LootCountPt;   //倍率
    uint32 LootCountEx;   //加值
};
typedef std::map<uint32, AsTemp> OnAsMap;
extern OnAsMap OnAsStore;

struct AyThrough
{
    uint32 TP_gh;
    uint32 TP_bl;
};
typedef std::map<uint32, AyThrough> OnAyThrough;
extern OnAyThrough m_OnAyThrough;

struct SpellModTemplate
{
    uint32 reqId;
    float dmgMod;
    float healMod;
    AsInt64 nowdmg;
    bool disable;
    int32 castingtime;
    uint32 duration;
    uint32 cooldown;
    uint32 procChance;
    uint32 Periodic[MAX_SPELL_EFFECTS];
};
typedef std::unordered_map<uint32, SpellModTemplate> SpellModTemplateMap;
extern SpellModTemplateMap SpellModMap;

struct SpellModBaseTemplate
{
    uint32    EffectImplicitTargetA[MAX_SPELL_EFFECTS];
    uint32    Effect[MAX_SPELL_EFFECTS];
    uint32    EffectApplyAuraName[MAX_SPELL_EFFECTS];
    int32     EffectMiscValue[MAX_SPELL_EFFECTS];
    int32     EffectBasePoints[MAX_SPELL_EFFECTS];
    uint32    EffectTriggerSpell[MAX_SPELL_EFFECTS];
    flag96    EffectSpellClassMask[MAX_SPELL_EFFECTS];
    uint32    SpellFamilyName;
    uint32    procChance;
    uint32    procCharges;
    uint32	  procFlags;
    int32     RecoveryTime;
};
typedef std::unordered_map<uint32, SpellModBaseTemplate> SpellModBaseTemplateMap;
extern SpellModBaseTemplateMap SpellModBaseMap;

struct AuraModStatTemplate
{
    uint32 aura;
    uint32 auraType[MAX_SPELL_EFFECTS];
    int32 basePoints[MAX_SPELL_EFFECTS];
    int32 misc[MAX_SPELL_EFFECTS];
    int32 miscB[MAX_SPELL_EFFECTS];
};
typedef std::unordered_map<uint32, AuraModStatTemplate> AuraModStatTemplateMap;
extern AuraModStatTemplateMap AuraModStatMap;

struct AuraPctTemplate
{
    uint32 aura;
    uint32 auraType[MAX_SPELL_EFFECTS];
    int32 basePoints[MAX_SPELL_EFFECTS];
    uint32 misc[MAX_SPELL_EFFECTS];
    uint32 miscB[MAX_SPELL_EFFECTS];
};
typedef std::unordered_map<uint32, AuraPctTemplate> AuraPctTempMap;
extern AuraPctTempMap AuraPctMap;

struct AuraTriggerSpellTemplate
{
    uint32 auraId;
    std::vector<uint32>triggerSpellData;
    float procChance;
    std::vector<uint32>linkSpellData;
    float bp0ApPct;
    float bp1ApPct;
    float bp2ApPct;
    float bp0SpPct;
    float bp1SpPct;
    float bp2SpPct;
    uint32 cooldown;
    uint32 ignoreMask;

    float TargetRange;
    uint32 Targets;

    uint32 procFlags;
};
typedef std::unordered_map<uint32, AuraTriggerSpellTemplate> AuraTriggerSpellTemplateMap;
extern AuraTriggerSpellTemplateMap AuraTriggerSpellMap;

struct AuraStackTriggerTemplate
{
    uint8 Stacks;
    std::vector<uint32> TriggerSpellVec;
    uint8 RemoveStacks;
};
typedef std::unordered_map<uint32, AuraStackTriggerTemplate> AuraStackTriggerTemplateMap;
extern AuraStackTriggerTemplateMap AuraStackTriggerMap;

struct AuraTriggerOnDeathTemplate
{
    uint32 CoolDown;
    uint32 AttackerTriggerSpell;
    float AttackerTriggerChance;
    uint32 SelfTriggerSpell;
    float SelfTriggerChance;
    bool PreventLastDamage;
};
typedef std::unordered_map<uint32, AuraTriggerOnDeathTemplate> AuraTriggerOnDeathTemplateMap;
extern AuraTriggerOnDeathTemplateMap AuraTriggerOnDeathMap;

typedef std::unordered_map<uint32, uint32> TSpellCusTargetMaskMap;
extern TSpellCusTargetMaskMap SpellCusTargetMaskMap;

typedef std::map<uint32, std::string> SpellCastMap;
extern SpellCastMap SpellCastStore;

struct BusAuraTemp
{
    uint32 TarSpellId;                //加强技能编号
    float PVEDmgMod;
    float PVEHeMod;
    float PVPDmgMod;
    float PVPHeMod;
    AsInt64 AddDmg;
    uint32 StatMask;                 //属性掩码
    float StatUpValue;                //强化率
    float StatValue;                //转换率
    float AddSPMod;
    float AddAPMod;
};
typedef std::multimap<uint32, BusAuraTemp> BusAuraNMap;
extern BusAuraNMap BusAuraStore;

struct SpellAbsorbTemp
{
    uint32 NowAbsorb;
    uint32 AddAbsorb;
    float APMod;
    float SPMod;
    std::set<uint32> LinkSpells;
    float LinkMod;
};

typedef std::map<uint32, SpellAbsorbTemp> SpellAbsorbMap;
extern SpellAbsorbMap SpellAbsorbStore;

struct BusXsAuraTemp
{
    uint32 ShowSpellId;                //施行技能编号
    float PVEMod;
    float PVPMod;
    AsInt64 PVEValue;
    AsInt64 PVPValue;
};
typedef std::map<uint32, BusXsAuraTemp> BusXsAuraMap;
extern BusXsAuraMap BusXsAuraStore;

struct LinkAuraStackTemp
{
    uint32 SAuraId;              //主要
    uint32 LAuraId;              //链接
    uint8 Stacks;
    std::set<uint32> SpellSet;
    uint8 RemoveStacks;
};
typedef std::vector<LinkAuraStackTemp> LinkAuraStackTempMap;
extern LinkAuraStackTempMap LinkAuraStackMap;

class AuraSpellMgr
{
public:
    static AuraSpellMgr* Instance()
    {
        static AuraSpellMgr Instance;
        return &Instance;
    }

    float ThroughAuraTP(Player* player);  // 返回值改为 float

    // 获取应用减伤后的突破伤害倍率（玩家目标）
    float ThroughAuraTPWithPlayerRet(Player* player, bool IsPVP);

    // 获取应用减伤后的突破伤害倍率（生物目标）
    float ThroughAuraTPWithCreatureRet(Player* attacker, Creature* creature);

    void LoadAuraSpellMgr();
    void HealXsOnDamage(Unit* Caster, Unit* Target, AsInt64 const& Damage, uint32 CastId = 0);

    void AutoPVEDmg(Player* player, uint32& Damage, uint32 CastId = 0);
    void AutoInt32PVEDmg(Player* player, int32& Damage, uint32 CastId = 0);
    void AutoPVEDmgBefore(Player* player, AsInt64& Damage, uint32 CastId = 0);
    void AutoPVEDmg(Player* player, AsInt64& Damage, uint32 CastId = 0);
    void AutoPVERet(Player* player, AsInt64& Damage, uint32 CastId = 0);
    void AutoPVPDmg(Player* player, AsInt64& Damage, uint32 CastId = 0);
    void AutoPVPRet(Player* player, AsInt64& Damage, uint32 CastId = 0);
    void AutoPVERet(Player* player, uint32& Damage, uint32 CastId = 0);
    void AutoPVPDmg(Player* player, uint32& Damage, uint32 CastId = 0);
    void AutoPVPRet(Player* player, uint32& Damage, uint32 CastId = 0);

    float GetLootOddsMod(Player* player);
    void AutoLootOddsKn(Player* player, float& Chance);

    uint32 GetLootCountMod(Player* player);

    AsInt64 GetStatNowHook(Player* player, AsInt64 const& Amount);

    void LoadCustomSpellMgr();

    bool Enable(Player* player, uint32 spellId);
    uint32 GetReqId(uint32 spellId);
    float GetXhDmgMod(uint32 spellId);
    float GetXhHealMod(uint32 spellId);
    int32 GetXhNowDmgMod(uint32 spellId);
    AsInt64 GetAsXhNowDmgMod(uint32 spellId);

    bool Ignore(uint32 ignoreMask, Unit* target);
    bool AuraTrigger(Unit* caster, Unit* victim, SpellInfo const* auraSpellInfo, SpellInfo const* procSpell, uint32 procFlags, Unit*& target, AuraEffect* triggeredByAura);
    void GetParams_PctOnStat(const char* strAuraType, const char* strStatType, AuraType& auraType, int32& misc, int32& miscB);
    void GetParams_Pct(const char* strAuraType, const char* strModType, AuraType& auraType, int32& misc, int32& miscB);

    float AutoPVPMeleeCritAuraMod(Unit const* player);
    float AutoPVPSpellCritAuraMod(Unit const* player);
    float AutoPVEMeleeCritAuraMod(Unit const* player);
    float AutoPVESpellCritAuraMod(Unit const* player);
    uint32 AutoGMDmgAuraMod(Player* player);
    AsInt64 AutoAsGMDmgAuraMod(Player* player);

    float AutoPVEDmgMod(Player* player, uint32 TarSpellId);
    float AutoPVPDmgMod(Player* player, uint32 TarSpellId);
    float AutoPVEHeMod(Player* player, uint32 TarSpellId);
    float AutoPVPHeMod(Player* player, uint32 TarSpellId);

    Stats AutoGetStatForId(uint32 Id);
    float GetCallUpStatNow(Player* player, uint32 CheckValue, float StatUpValue);
    uint32 AutoAddStatNow(Player* player, uint32 CheckValue, float StatValue);

    void HookAutoAddDmg(Player* player, uint32 TarSpellId, uint32& Damage);
    void HookAutoAddDmg(Player* player, uint32 TarSpellId, AsInt64& Damage);

    void OnAuraSpellSetStackAmountHandler(Unit* Caster, Aura* LinkAura, uint8 StackAmount);

    void BusHealXsOnDamage(Unit* Caster, Unit* Target, AsInt64 const& Damage, uint32 CastId = 0);

private:

};
#define sAuraSpellMgr AuraSpellMgr::Instance()

#endif
