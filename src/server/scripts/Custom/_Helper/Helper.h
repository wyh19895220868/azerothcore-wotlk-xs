#ifndef _HELPER_H
#define _HELPER_H

#include "AsInt64.h"
#include "SharedDefines.h"
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class Creature;
class Item;
class ItemTemplate;
class ObjectGuid;
class Player;
class SpellInfo;
class Unit;
class WorldPacket;
class WorldSession;
struct AuctionEntry;

enum CoreCheckFunc
{
    FuncPet = 0,
    FuncMax = 50,
};

#define ToStr(S) std::to_string(S)

#define TxLevelSpell 47292                                                              //升级特效技能

#define KeepAliveStr std::string("KeepAlive@KeepAlive")

typedef std::map<uint32, std::string> GlobalConfigMap;
extern GlobalConfigMap GlobalConfigStore;

typedef std::map<uint32, std::string> GtsMap;
extern GtsMap GtsStore;

#define MNC 8                                                              //最多8组

#define SignLq 111                                                              //连签标识
#define SignLp 222                                                              //累签标识

struct NeedTemp
{
    uint32 ItemId[MNC];                                                              //物品组
    uint32 Count[MNC];                                                              //数量组
    uint32 VipLevel;                                                                    //需要会员等级
    uint64 Token;                                                                       //消耗积分
    uint64 AsToken;                                                                   //达到累计积分
    int32 PLevel;                                                                       //需要人物等级(正数=大于等于,负数=小于等于)
    uint32 Hr;                                                                              //消耗荣誉点
    uint32 An;                                                                             //消耗竞技点
    uint32 Money;                                                                      //消耗金币
    uint64 Mobi;                                                                       //消耗魔币
    uint64 AsMobi;                                                                   //达到累计魔币
    uint64 Paodian;                                                                  //消耗泡点
    uint32 Class;                                                                         //需要职业掩码
    uint32 Race;                                                                          //需要种族掩码
    uint32 Team;                                                                         //需要阵营
    uint32 FID;                                                                         //需要声望编号
    uint32 FRank;                                                                   //需要声望等级
    int32 MapId;                                                                       //需要地图
    uint32 AreaId;                                                                      //需要区域
    uint32 AchCount;                                                                 //达到成就点数
    uint32 Title;                                                                      //需要军衔等级1-28
    uint32 StatRank;                                                                 //需要斗气等级
    std::string Auras;                                                    //需要光环Buff组
    std::string ComAnch;                                            //通过成就组
    std::string ComQuest;                                           //完成任务组
    uint32 StatId;                                                                         //主属性掩码
    uint32 StatValue;                                                                         //主属性达到值
    std::string InSpell;                                           //学会或拥有技能组
    uint32 NsRank;                                                                         //赞助等级
    uint32 AsEqRank;                                                                         //全部装备栏等级
    uint32 Reincarnation;                                                                    //需要转生等级

};
typedef std::unordered_map<uint32, NeedTemp> NeedTempMap;
extern NeedTempMap NeedTempStore;

struct RewardTemp
{
    uint32 ItemId[MNC];                                                              //物品组
    uint32 Count[MNC];                                                              //数量组
    uint32 NowVip;                                                                      //设置会员等级
    uint64 AddToken;                                                                  //奖励积分
    uint32 Xp;                                                                              //奖励人物经验
    uint32 Hr;                                                                              //奖励荣誉点
    uint32 An;                                                                             //奖励竞技点
    uint32 Money;                                                                      //奖励金币
    uint32 AddMobi;		                                                            //奖励魔币
    uint64 AddPaodian;                                                              //奖励泡点
    std::string Auras;                                                    //奖励光环Buff组
    std::string ComAnch;                                            //奖励成就组
    std::string ComQuest;                                           //奖励完成任务组
    std::string Cmd;                                           //奖励GM命令组
};
typedef std::unordered_map<uint32, RewardTemp> RewardTempMap;
extern RewardTempMap RewardTempStore;

struct PosTemplate
{
    uint32 MapId;
    float X;
    float Y;
    float Z;
    float O;
};

typedef std::unordered_map<uint32, PosTemplate> PosTemplateMap;
extern PosTemplateMap PosMap;

struct AuraTemplate
{
    uint32 GroupId;
    uint32 AuraId;
};

typedef std::vector<AuraTemplate> AuraTemplateVect;
extern AuraTemplateVect AuraVect;

struct AtCreatureStatTemp
{
    int32 MinLevel = -1;                  //新地图控制：生物等级范围最小
    int32 MaxLevel = -1;                  //新地图控制：生物等级范围最大
    uint32 Level;                           //等级
    uint32 HpCount;                   //血条数量
    AsInt64 NowHp;                   //固定生命值
    AsInt64 DmgTakenNow;       //受到的伤害固定值
    AsInt64 DmgNow;                   //造成的伤害固定值
    AsInt64 DmgWMin;                   //物理伤害Min
    AsInt64 DmgWMax;                   //物理伤害Max
    AsInt64 MinSpellDmg;                   //技能法术伤害Min
    AsInt64 MaxSpellDmg;                   //技能法术伤害Max
    float KtHp;                              //切割比例
    float Spell;                             //技能法术伤害比
    std::string PLootIds;                        //额外掉落
    float CreAuraChance;                               //生物光环几率
    uint32 CreAuraGroup;                        //生物光环组
    uint32 CreCount;                        //生物光环数量
    std::set<uint32> AtAuras;                       //固定光环
    AsInt64 Armor;                        //护甲
    int32 School1;                        //神圣抗性
    int32 School2;                        //火焰抗性
    int32 School3;                        //自然抗性
    int32 School4;                        //冰霜抗性
    int32 School5;                        //暗影抗性
    int32 School6;                        //奥术抗性
    uint32 RAtLootGroupId;                   //超级掉落组号
    float RAtLootChance;                   //超级掉落几率
    uint32 RAtLootMin;                   //超级掉落数量Min
    uint32 RAtLootMax;                   //超级掉落数量Max

    // 地图生物控制和旧生物调用共用同一套属性字段。
    float HealthMultiplier = 1.0f;
    AsInt64 ManaValue = 0;
    float ManaMultiplier = 1.0f;
    float MinDamageMultiplier = 1.0f;
    float MaxDamageMultiplier = 1.0f;
    AsInt64 MinDamageValue = 0;
    AsInt64 MaxDamageValue = 0;
    AsInt64 TrueDamageValue = 0;
    float ArmorMultiplier = 1.0f;
    float FireResistMultiplier = 1.0f;
    float NatureResistMultiplier = 1.0f;
    float FrostResistMultiplier = 1.0f;
    float ShadowResistMultiplier = 1.0f;
    float ArcaneResistMultiplier = 1.0f;
    float AttackPowerMultiplier = 1.0f;
    AsInt64 AttackPowerValue = 0;
    float SpellPowerMultiplier = 1.0f;
    AsInt64 SpellPowerValue = 0;
    float MoveSpeedMultiplier = 1.0f;
    uint32 MoveSpeedValue = 0;
    float AttackSpeedMultiplier = 1.0f;
    uint32 AttackSpeedValue = 0;
    float CastSpeedMultiplier = 1.0f;
    uint32 CastSpeedValue = 0;
    float HitChanceBonus = 0.0f;
    uint32 ArmorPenetration = 0;
    float ArmorPenetrationPct = 0.0f;
    uint32 BlockPenetration = 0;
    float BlockPenetrationPct = 0.0f;
    int64 DamageReductionPct = 0;
    uint32 OfficialDrop = 1;
    float EquipAttrMultiplier = 1.0f;
    uint32 EquipAttrValue = 0;
    int32 EquipAttrMode = 0;
    uint32 IdentificationGroup = 0;
    uint8 IdentificationQuality = 0;
    uint32 IdentificationItemLevel = 0;
};

typedef std::unordered_map<uint32, AtCreatureStatTemp> AtCreatureStatTempMap;
extern AtCreatureStatTempMap AtCreatureStatTempStore;

struct BigItemTemplateStatTemp
{
    uint32 StatType;
    AsInt64 StatValue;
};

typedef std::unordered_map<uint64, BigItemTemplateStatTemp> BigItemTemplateStatMap;
extern BigItemTemplateStatMap BigItemTemplateStatStore;

struct BigCreatureTemplateStatTemp
{
    uint32 HpCount;
    AsInt64 NowHp;
    AsInt64 DmgTakenNow;
    AsInt64 DmgNow;
    AsInt64 DmgWMin;
    AsInt64 DmgWMax;
    AsInt64 MinSpellDmg;
    AsInt64 MaxSpellDmg;
};

typedef std::unordered_map<uint32, BigCreatureTemplateStatTemp> BigCreatureTemplateStatMap;
extern BigCreatureTemplateStatMap BigCreatureTemplateStatStore;

struct BigCreatureClassLevelStatTemp
{
    AsInt64 BaseHealth[3];
    AsInt64 BaseMana;
    AsInt64 BaseArmor;
    AsInt64 AttackPower;
    AsInt64 RangedAttackPower;
    AsInt64 BaseDamage[3];
};
typedef std::unordered_map<uint32, BigCreatureClassLevelStatTemp> BigCreatureClassLevelStatMap;
extern BigCreatureClassLevelStatMap BigCreatureClassLevelStatStore;

#define CT_MAX_INT 2100000000         //最大int32值
#define CT_MAX_FLOAT 2100000000.0f         //最大Float值
#define CT_MAX_WP 2000000000.0f         //最大武器伤害

class Helper
{
public:
    static Helper* Instance()
    {
        static Helper Instance;
        return &Instance;
    }

    std::string ToExString(float Value);
    std::string GetCustomHome();
    std::string GetUserNameByAccountId(uint32 AccountId);
    std::string GetRaceString(uint8 Race);
    std::string GetClassString(uint8 Class);
    std::string GetGossipClassString(uint8 Class);
    std::string GetNtTeamStr(uint8 TeamId);
    std::string GetBGTeamStr(TeamId Team);

    std::string GetPlayerNameByGuid(ObjectGuid Guid);
    const char* GetSpellName(uint32 SpellId);
    std::string GetAtSpellName(uint32 SpellId);
    std::string GetSpellIdName(uint32 SpellId);
    std::string GetSpellDes(uint32 SpellId);
    const char* GetAchievementName(uint32 AchId);
    const char* GetTitlesName(uint32 TitleId);
    const char* GetQuestName(uint32 QuestId);

    std::string GetGtsFactionName(uint32 Faction);
    const char* GetFactionName(uint32 Faction);
    std::string GetFactionRankName(uint8 Rank);
    const char* GetProName(uint32 Pro);
    std::string GetGenderString(uint8 Gr);
    const char* GetMapName(int32 MapId);
    const char* GetAreaName(uint32 Id);
    std::string GetEnchName(uint32 EnchId);
    std::string GetCreatureScriptName(Creature* Cr);
    std::string GetCreatureScriptName(uint32 CreatureId);
    std::string GetItemScriptName(Item* DoItem);
    std::string GetItemScriptName(uint32 ItemId);
    std::string GetCreatureNameByEntry(uint32 CreatureId);
    std::string GetCreatureName(Creature* Cr);

    std::string GetTitleName(uint32 Title);

    std::string GetClassNameById(uint32 Id);
    std::string PullMaskClassName(uint32 Value);
    bool IsClassInMask(uint32 ClassId, uint32 Value);

    std::string GetRaceNameById(uint32 Id);
    std::string PullMaskRaceName(uint32 Value);
    bool IsRaceInMask(uint32 RaceId, uint32 Value);

    std::string GetStatNameById(uint32 Id);
    std::string PullMaskStatName(uint32 Value);
    Stats GetStatForId(uint32 Id);
    bool IsStatInMask(Player* player, uint32 IdValue, uint32 CheckValue);
    bool IsStatInNow(Unit* player, uint32 CheckValue);

    void SendGMMessage(const char* Format, ...);
    void SendSerMessage(const char* Format, ...);
    void SendSerNewMessage(const char* Format, ...);
    void SendSerAllMessage(const char* Format, ...);
    void SendSerSpecialMessage(const char* Format, ...);
    void SendTeamMessage(Player* player, const char* Format, ...);
    void SendScreenMessage(const char* Text, Player* player = nullptr, bool GM = false, TeamId Id = TEAM_NEUTRAL);
    void SendAreaRangeMessage(uint32 AreaId, const char* Format, ...);

    std::string GetItemIcon(uint32 ItemId, uint32 Width, uint32 Height, int X, int Y) const;
    std::string GetItemName(uint32 ItemId) const;
    std::string GetNItemLink(Item* DoItem) const;
    std::string GetItemLink(uint32 ItemId) const;
    std::string GetWorldChatNameLinkColor(Player* player);
    std::string GetNameLinkColor(Player* player);
    std::string GetNameLinkColor(std::string PlayerName, uint8 Class);
    std::string GetVipPlayerChatInfo(Player* player, uint32 IconSize = 12);
    std::string GetWorldChatPlayerChatInfo(Player* player, uint32 IconSize = 12);
    std::string GetPlayerChatInfo(Player* player, uint32 IconSize = 12);
    std::string GetPlayerChatInfo(std::string PlayerName, uint8 Class, uint8 Race, uint8 Gender, uint32 IconSize = 12);
    std::string GetPlayerChatInfo(std::string PlayerName, uint8 Class, uint32 IconSize = 12);
    void SendMailToName(std::string PlayerName, std::string Sub, std::string Body, uint32 ItemId, uint32 Count, uint32 Money = 0);
    void SendMailToGuid(ObjectGuid Guid, std::string Sub, std::string Body, uint32 ItemId, uint32 Count, uint32 Money = 0);

    void SendMailFromHouseToken(ObjectGuid Guid, ObjectGuid BidderGuid, int64 Count);
    void SendMailFromHouseMB(ObjectGuid Guid, ObjectGuid BidderGuid, int64 Count);
    void SendMailFromHousePaodian(ObjectGuid Guid, ObjectGuid BidderGuid, int64 Count);
    void SendWonAHItemBidder(ObjectGuid BidderGuid, ObjectGuid OwnerGuid, Item* pItem, uint32 Count);
    void SendExpiredAH(ObjectGuid OwnerGuid, Item* pItem);
    void SendCancelledAH(ObjectGuid OwnerGuid, Item* pItem);
    void AuctionHouseCustomMsg(Player* player, AuctionEntry* AH, Item* pItem, bool OnSell);
    void SendItemHouseMailToGuid(ObjectGuid Guid, ObjectGuid BidderGuid, uint32 Count);

    void LoadGlobalConfig();
    const char* GetConfig(uint32 Index);
    std::string GetStrConfig(uint32 Index);
    int64 GetInt(uint32 Index);
    uint64 GetUInt(uint32 Index);
    uint32 GetUInt32(uint32 Index);
    int32 GetInt32(uint32 Index);
    float GetFloat(uint32 Index);
    bool GetBoolean(uint32 Index);
    bool GetBoolean(uint32 Index, bool defaultValue);

    void LoadGlobalGts();
    const char* GetGts(uint32 Id);

    void LoadAsTemp();
    uint32 GetBigItemTemplateStatType(uint32 ItemId, uint8 Index, uint32 DefaultType);
    AsInt64 GetBigItemTemplateStatValue(uint32 ItemId, uint8 Index, AsInt64 const& DefaultValue);
    uint32 GetBigCreatureTemplateHpCount(uint32 CreatureId);
    AsInt64 GetAsBigCreatureTemplateHp(uint32 CreatureId);
    AsInt64 GetAsBigCreatureTemplateDmgTakenNow(uint32 CreatureId);
    AsInt64 GetAsBigCreatureTemplateDmgNow(uint32 CreatureId);
    AsInt64 GetAsBigCreatureTemplateDmgWMin(uint32 CreatureId);
    AsInt64 GetAsBigCreatureTemplateDmgWMax(uint32 CreatureId);
    AsInt64 GetAsBigCreatureTemplateMinSpellDmg(uint32 CreatureId);
    AsInt64 GetAsBigCreatureTemplateMaxSpellDmg(uint32 CreatureId);
    AsInt64 GetAsCreatureClassLevelHealth(uint8 Level, uint8 Class, uint8 Expansion);
    AsInt64 GetAsCreatureClassLevelMana(uint8 Level, uint8 Class);
    AsInt64 GetAsCreatureClassLevelArmor(uint8 Level, uint8 Class);
    AsInt64 GetAsCreatureClassLevelAttackPower(uint8 Level, uint8 Class);
    AsInt64 GetAsCreatureClassLevelRangedAttackPower(uint8 Level, uint8 Class);
    AsInt64 GetAsCreatureClassLevelBaseDamage(uint8 Level, uint8 Class, uint8 Expansion);
    std::string GetNeedItemUpGts(uint32 TempId, uint32 Mod = 1);
    std::string GetNeedGts(uint32 TempId, uint32 Mod = 1);
    std::string GetNeedLuaGts(uint32 TempId);
    bool CheckNeed(Player* player, uint32 TempId, std::string Info, uint32 Mod = 1);
    std::string GetNeedTalentGts(Player* player, uint32 UseTalentSpellId, uint32 TalentRank, uint32 Mod = 1);
    bool CheckNeedTalent(Player* player, uint32 UseTalentSpellId, uint32 TalentRank, std::string Info, uint32 Mod = 1);
    bool AtCheckNeed(Player* player, uint32 TempId);
    bool NotDeleteCheckNeed(Player* player, uint32 TempId, uint32 Mod = 1);
    bool ShowCheckNeed(Player* player, uint32 TempId, uint32 Mod = 1);
    void DeleteCheckNeed(Player* player, uint32 TempId, std::string Info, uint32 Mod = 1);
    std::string GetRewardGts(uint32 TempId, uint32 Mod = 1);
    std::string GetRewardLuaGts(uint32 TempId);
    void DoTheReward(Player* player, uint32 TempId, std::string Info, uint32 Mod = 1);
    bool DoBackReward(Player* player, uint32 TempId, std::string Info, uint32 Mod = 1);
    std::string GetDoTimeString(uint64 Time);
    std::string GetSecsDoTimeString(uint64 Time);
    std::string GetTeamIdIcon(TeamId Id);
    bool IsMultiTrainerNpc(Creature* Cr);
    bool IsMultiVendorNpc(Creature* Cr);
    void ResetPlayerDefault(Player* player);
    void ClearResetForTgSpec(Player* player);
    void SetClassToNew(Player* player, uint8 NewClass);
    void UpdateCreTempForEntry(Creature* Cre);
    uint32 GetSeriesDays(std::vector<uint32> DoList);
    void SignRewardExtra(Player* player, uint32 SeriesDays, uint32 Total);
    uint32 GetDaysInMonth(uint32 NowYear, uint32 NowMonth);
    std::string SecTimeString(uint64 timeInSecs, bool shortText = true);
    bool IsAsNum(std::string Str);
    void PopToPlayerDialog(Player* player, std::string OnText, uint32 Sender = 0, uint32 Action = 0, uint32 Menu = 0);
    std::string GetItemSlotName(uint32 Slot);

    bool IsCheckString(std::string CheckString, std::string LogReason, WorldSession* Session, WorldPacket& RecvData);
    bool IsCheckString(std::string CheckString, std::string LogReason, WorldSession* Session);
    uint32 GetItemIdByGlyph(uint32 GlyphId);
    bool IsDKEns(Item* DoItem);
    void ClearDKEns(Player* player);

    bool IsStoreNewBag(Player* player, uint32 NewItemId);
    bool IsCustomAllowedMount(Unit* Caster);
    bool IsIgnoreMapCheck(uint32 MapId);
    bool IsIgnoreAreaCheck(uint32 AreaId);
    bool IsIgnoreAccountId(uint32 AccountId);
    std::string GetTimeHourStr(time_t KTime);
    std::string GetTimeSecondStr(time_t KTime);
    bool CanCustomFly(Player* player);
    bool OnDungeon(uint32 MapId);
    bool NotRemoveSpellClass(uint32 SepllId);
    float GetWpDPS(uint32 Min, uint32 Max, uint32 ItemId);
    std::string GetWpDPSString(AsInt64 const& Min, AsInt64 const& Max, uint32 ItemId);
    bool IsVectEqual(std::vector<uint32>& A, std::vector<uint32>& B);
    void ApplyPlayerWpModFloatVar(float& Var, float Value, bool Apply);
    void DoClearInCombat(Player* player);
    void CustomPrintClose();

    void CustomPrint(std::string BindFuncStr, const char* StrEndTime);
    long StandardToStamp(const char* StrTime);
    std::string GetNowVer();

    std::string GetUserBindData() { return UserBindData; };
    void SetUserBindData(std::string UBindData) { UserBindData = UBindData; };
    bool InCheckFunc(CoreCheckFunc Num);

    uint8 GetNowNum(Item* TargetItem);
    uint8 GetLastNum(Item* TargetItem);
    uint8 GetDressItemSlot(ItemTemplate const* Proto);

    bool ClassNewQuestCheck(Player* player, uint32 SpellId);
    void PrintKeepDone(uint32 Times);
    void PrintResetRestoreDone(uint32 Count);
    void PrintKeepError(uint32 Count);

    bool CheckMapAreaItemToBank(Item* DoItem, uint32 MapId, uint32 AreaId);

    /*-----------------------------------------怪物属性模板---------------------------------------------------*/

    uint32 GetCreLevelByTempId(uint32 TempId);
    uint32 GetCreHpCountByTempId(uint32 TempId);
    uint32 GetCreNowHpByTempId(uint32 TempId);
    AsInt64 GetAsCreNowHpByTempId(uint32 TempId);
    uint32 GetCreDmgTakenNowByTempId(uint32 TempId);
    AsInt64 GetAsCreDmgTakenNowByTempId(uint32 TempId);
    uint32 GetCreDmgNowByTempId(uint32 TempId);
    AsInt64 GetAsCreDmgNowByTempId(uint32 TempId);

    float GetCreDmgWMinByTempId(uint32 TempId);
    float GetCreDmgWMaxByTempId(uint32 TempId);
    AsInt64 GetAsCreDmgWMinByTempId(uint32 TempId);
    AsInt64 GetAsCreDmgWMaxByTempId(uint32 TempId);
    float GetCreKtHpByTempId(uint32 TempId);
    float GetCreHpByTempId(uint32 TempId);
    float GetCreManaByTempId(uint32 TempId);
    float GetCreMeleeByTempId(uint32 TempId);
    float GetCreSpellByTempId(uint32 TempId);
    float GetCreRetByTempId(uint32 TempId);
    bool GetCreOnEtByTempId(uint32 TempId);

    std::string GetCrePLootIdsByTempId(uint32 TempId);

    float GetCreAuraChanceByTempId(uint32 TempId);
    uint32 GetCreAuraGroupByTempId(uint32 TempId);
    uint32 GetCreAuraCountByTempId(uint32 TempId);
    std::set<uint32> GetCreAtAurasByTempId(uint32 TempId);
    uint32 GetCreAttackTimeByTempId(uint32 TempId);
    AsInt64 GetCreArmorByTempId(uint32 TempId);

    int32 GetCreSchool1ByTempId(uint32 TempId);
    int32 GetCreSchool2ByTempId(uint32 TempId);
    int32 GetCreSchool3ByTempId(uint32 TempId);
    int32 GetCreSchool4ByTempId(uint32 TempId);
    int32 GetCreSchool5ByTempId(uint32 TempId);
    int32 GetCreSchool6ByTempId(uint32 TempId);

    int32 GetCreMinDmgSpellByTempId(uint32 TempId);
    AsInt64 GetAsCreMinDmgSpellByTempId(uint32 TempId);
    int32 GetCreMaxDmgSpellByTempId(uint32 TempId);
    AsInt64 GetAsCreMaxDmgSpellByTempId(uint32 TempId);

    /*-----------------------------------------怪物属性模板---------------------------------------------------*/

    void AutoResetLuaNowTime();
    bool CheckItemInBag(Player* player, uint32 ItemId);
    bool HasItemCastCd(Player* player, Item* CastItem);
    bool CanItemLootBag(Player* player);
    uint32 GetOnLinePlayerCount();

    void WorldDoAllLoad();
    void DoZoneInCombatForPlayer(Creature* creature);
    void SendItemQuerySingleOpcode(Player* player, uint32 ItemId);
    bool CreInCustomCanAttack(uint32 CreEntry);
    void LogPlayerTokenNow(ObjectGuid Guld, int64 DToken, std::string Info);
    void LogPlayerItemInfoNow(ObjectGuid Guld, uint32 ItemId, int32 Count, std::string Info);
    bool ItemHasEnch(Item* OnItem);
    bool CheckMechanicForSpellInfo(SpellInfo const* Info, uint32 MaskValue);
    bool NoCanSellItem(Item* OnItem);

    void DoAsLoadDataCacheStore();

private:
    std::string UserBindData;
};
#define sHelper Helper::Instance()

#endif //!
