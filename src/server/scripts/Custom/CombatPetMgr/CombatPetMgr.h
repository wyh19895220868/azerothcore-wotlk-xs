
#ifndef _COMBATPETMGR_H
#define _COMBATPETMGR_H

#define MaxSpellCount 8

struct CPetTemp
{
    uint32 CreId;                               //编号
    uint32 Rank;                               //阶级
    uint32 NeedXp;                               //升级所需经验
    uint64 Score;                               //资质评分
    uint32 StatTempId;                               //属性模版
    uint32 Stat1;                               //力量
    uint32 Stat2;                               //敏捷
    uint32 Stat3;                               //智力
    uint32 Stat4;                               //精神
    uint32 SpellIds[MaxSpellCount];              //技能集合
    std::set<uint32> FitAuras;              //合体光环集合
    float FitMod;                               //合体加成
};

typedef std::vector<CPetTemp> CPetTempVect;
extern CPetTempVect CPetTempStore;

struct CPetGasTemp
{
    uint32 ToRank;                    //直达阶级
    uint32 RewXp;                    //奖励经验
    bool AddRank;                    //是否加1级
};
typedef std::unordered_map<uint32, CPetGasTemp> CPetGasMap;
extern CPetGasMap CPetGasStore;

struct CPetUseTemp
{
    uint32 CreEntry;                    //宠物编号
    uint32 GroupRank;                    //阶级组
};
typedef std::unordered_map<uint32, CPetUseTemp> CPetUseMap;
extern CPetUseMap CPetUseStore;

struct CPetGroupRankTemp
{
    uint32 GroupId;                    //阶级组
    uint32 ToRank;                    //直接阶级
    float GroupOdds;                    //概率
};
typedef std::vector<CPetGroupRankTemp> CPetGroupRankVect;
extern CPetGroupRankVect CPetGroupRankStore;

class CombatPetMgr
{
public:
    static CombatPetMgr* Instance()
    {
        static CombatPetMgr Instance;
        return &Instance;
    }

    void LoadCombatPetData();
    bool IsCombatPetData(Creature* Me);
    bool IsCombatPetData(uint32 CreEntry);
    uint32 GetCombatPetNowRankByIndex(Player* player);
    uint32 GetCombatPetMaxRank(Creature* Me);
    uint32 GetCombatPetNextXp(uint32 CreEntry, uint32 NowRank);
    uint32 GetStTempIdForCombatPetRank(Creature* Me);
    uint32 GetStTempIdForCombatPetRank(uint32 CreEntry, uint32 NowRank);
    std::pair<uint32, uint32> GetCreateSumPetEntryAndRank(uint32 UseItemId);
    uint64 GetCombatPetScore(uint32 CreEntry, uint32 NowRank);

    bool CreateCombatPet(Player* player, uint32 CreateIndex, uint32 CreEntry, uint32 ToRank);
    void AddonToggleSummonComCurrentbatPet(Player* player, uint32 CreateIndex);
    void ResetSummonComCurrentbatPet(Player* player);
    void LoginSummonComCurrentbatPet(Player* player);
    void ApartSummonComCurrentbatPet(Player* player, uint32 AddonIndex);
    void RemoveCombatPetMapWorldForTeleport(Player* player);
    void RemoveCombatPetMapWorldForWithinDistRange(Player* player);
    void RemoveCombatPetMapWorldForWithinDistRangePossessed(Player* player);
    void ActiveHandleDismissCritter(Player* player, uint32 AddonIndex);
    bool HasCreateNowIndex(Player* player);
    uint32 GetCreateNowIndex(Player* player);
    uint32 GetPlayerDataPetCount(Player* player);
    void ActiveDeleteDB(Player* player, uint32 AddonIndex);
    void AutoFillCombatPetPlayerDataAll(Player* player);
    void AutoFillCombatPetPlayerDataOne(Player* player, uint32 CreateIndex);
    void AutoUpXpAndRankApplyStatTemp(Player* player, Creature* Me, uint32 ToRank);
    void GetCombatPetSendInfo(uint32 PlayerLevel, uint32 CreEntry, uint32 NowRank, std::string& Name, AsInt64& MaxHp, AsInt64& MinDamage, AsInt64& MaxDamage, AsInt64& Stat1, AsInt64& Stat2, AsInt64& Stat3, AsInt64& Stat4, uint32& Spell1, uint32& Spell2, uint32& Spell3, uint32& Spell4, uint32& Spell5, uint32& Spell6, uint32& Spell7, uint32& Spell8);
    void UpdateFitCombatPetNow(Player* player, uint32 AddonIndex);
    void LoginUpdateFitCombatPetNow(Player* player);
    bool IsInPlayerPetEntry(Player* player, uint32 CreEntry);

private:

};
#define sCombatPetMgr CombatPetMgr::Instance()

#endif
