
#ifndef _ASCREATURE_H
#define _ASCREATURE_H

#include "../_Helper/AsInt64.h"

struct CreatureStruct
{
    uint32 CreId;                        //生物编号
    uint32 Diff;                            //难度
    uint32 SpGroupId;                  //控制组
    uint32 GtsFlag;                        //提示方式
    std::string RespawnGts;             //刷新公告
    uint32 Respawntime;                //刷新时间
};
typedef std::vector<CreatureStruct> CreatureOneVect;
extern CreatureOneVect CreatureStructStore;

struct OneCreGroupTemp
{
    uint32 GroupId;
    uint32 StTempId;
    uint32 ModeId;
    float Scale;
    uint32 EqWp1;
    uint32 EqWp2;
    uint32 EqWp3;
    float Chance;
};
typedef std::vector<OneCreGroupTemp> OneCreGroupVect;
extern OneCreGroupVect OneCreGroupStore;

struct MazCreatureStruct
{
    int32 MapOrAreaId;                        //地图或区域
    uint32 Diff;                            //难度
    bool Flag;                            //类型
    uint32 CreTempId;                  //属性模板
};

typedef std::vector<MazCreatureStruct> CreatureStructVect;
extern CreatureStructVect MazCreatureStructStore;

struct MazChallenge
{
    uint32 MapId;                        //副本地图
    uint32 Diff;                            //难度
    bool Flag;                            //类型
    uint32 CreTempId;                  //属性模板
    std::string GossipText;        //菜单文字
    uint32 NTempId;                  //需求模板
    uint32 ReItem;                            //重置的物品
    bool AutoReset;                        //出本自动重置（1=是，0=否）
};

typedef std::vector<MazChallenge> MazChallengeVect;
extern MazChallengeVect MazChallengeStore;

typedef std::unordered_map<uint32, uint32> MazChallengeItemMap;
extern MazChallengeItemMap MazChallengeItemStore;

struct ChallengeData
{
    uint32 MapId;
    uint32 GuidLow;
    uint32 Level;
    bool Bind;
};
typedef std::vector<ChallengeData> ChallengeDataVect;
extern ChallengeDataVect ChallengeDataStore;

struct StageTemplate
{
    uint32 ChapterId = 0;
    uint32 ChapterStage = 0;
    uint32 SummonsGroupId;
    uint32 RewardGameobjectId;
    std::string Description;
    uint32 PositionId = 0;
    uint32 PortalGameobjectId = 0;
    bool IsBossStage = false;
    uint32 RewardTempId = 0;
};
typedef std::unordered_map<uint32, StageTemplate> StageTemplateMap;
extern StageTemplateMap StageStore;

struct StageChapterTemplate
{
    uint32 ChapterId = 0;
    std::string Name;
    uint32 MapId = 0;
    uint32 EntryPositionId = 0;
    uint32 ExitPositionId = 0;
    uint32 RewardTempId = 0;
    uint32 NextChapterId = 0;
    uint32 DefaultPortalGameobjectId = 0;
    bool Enabled = false;
};
typedef std::map<uint32, StageChapterTemplate> StageChapterTemplateMap;
extern StageChapterTemplateMap StageChapterStore;

struct StageGroupTemp
{
    uint32 GroupId;
    uint32 CreId;
    uint32 Count;
    float X;
    float Y;
    float Z;
    float O;
};

typedef std::vector<StageGroupTemp> StageGroupList;
typedef std::unordered_map<uint32, StageGroupList> StageGroupMap;
extern StageGroupMap StageGroupStore;

struct NtRankTemp
{
    uint32 SGroupId;
    uint32 RItemId;
    std::string Des;
    uint32 RewardTempId;
};
typedef std::unordered_map<uint32, NtRankTemp> NtRankTempMap;
extern NtRankTempMap NtRankStore;

struct NtRankGroupTemp
{
    uint32 GroupId;
    uint32 CreId;
    uint32 Count;
    float X;
    float Y;
    float Z;
    float O;
};

typedef std::unordered_map<ObjectGuid, uint32> AsPlayerNtRankMap;
extern AsPlayerNtRankMap AsPlayerNtRankStore;

typedef std::vector<NtRankGroupTemp> NtRankGroupList;
typedef std::unordered_map<uint32, NtRankGroupList> NtRankGroupMap;
extern NtRankGroupMap NtRankGroupStore;

struct OKillCreTemp
{
    std::string NCmd;
    std::string RCmd;
    std::string CreOrOb;
    uint32 RTempId;
    float Chance;
    uint32 RmGroupId;
    uint32 RmCount;
    float RmChance;
};
typedef std::unordered_map<uint32, OKillCreTemp> OKillCreMap;
extern OKillCreMap OKillCreStore;

struct RmSumTemp
{
    uint32 GroupId;
    uint32 RmCreId;
    uint32 RmPosId;
    uint32 TimeFn;
};
typedef std::vector<RmSumTemp> RmSumTempVect;
extern RmSumTempVect RmSumStore;

struct DieMapGravTemp
{
    uint32 Faction;
    uint32 PosMin;
    uint32 PosMax;
    float HMPct;
};
typedef std::multimap<int32, DieMapGravTemp> DieMapGravMap;
extern DieMapGravMap DieMapGravStore;

class AsCreature
{
public:
    static AsCreature* Instance()
    {
        static AsCreature Instance;
        return &Instance;
    }

    void LoadAsCreatureStore();
    void UpdateAsPlayerNtRank(ObjectGuid Guid, uint32 Rank);
    void DoNtRankDaySendReward();

    bool IsNotFor(Creature* Me);

    bool IsOneCreStore(Creature* Me);
    bool IsLFGCustomBoss(Creature* Me);
    bool IsCustomBoss(Creature* Me);


    void DoLevelMod(Creature* Me, uint8& MeLevel);
    float GetHpMod(Creature* Me);
    float GetManaMod(Creature* Me);
    float GetMeleeMod(Creature* Me);
    float GetSpellMod(Creature* Me);
    uint32 GetCastSpeedValue(Creature* Me);
    uint32 GetArmorPenetration(Creature* Me);
    float GetArmorPenetrationPct(Creature* Me);
    uint32 GetBlockPenetration(Creature* Me);
    float GetBlockPenetrationPct(Creature* Me);
    float GetRetMod(Creature* Me);
    std::string GetOneLootIds(Creature* Me);
    uint32 GetHpNow(Creature* Me);
    AsInt64 GetAsHpNow(Creature* Me);
    uint32 GetRespawntime(Creature* Me);
    uint32 GetDmgTakenNow(Creature* Me);
    AsInt64 GetAsDmgTakenNow(Creature* Me);
    uint32 GetDmgNow(Creature* Me);
    AsInt64 GetAsDmgNow(Creature* Me);
    float GetKtHpMod(Creature* Me);

    void CreAddAuras(Creature* Me);
    void AutoRespawnGts(Creature* Me, bool Select = true);

    std::pair<float, float> AutoDmgCreatureMinAndMax(Creature* Me);
    std::pair<AsInt64, AsInt64> AutoAsDmgCreatureMinAndMax(Creature* Me);
    int32 AutoDmgSpellBeginCreature(Creature* Me);
    AsInt64 AutoAsDmgSpellBeginCreature(Creature* Me);

    bool IsOnEtLoot(Creature* Me);
    void AsCustomFillLoot(Creature* Me, Loot* OnLoot, Player* Looter);

    uint32 AsCustomAttackTime(Creature* Me);
    float AsCustomArmor(Creature* Me);
    float AsCustomSchool1(Creature* Me);
    float AsCustomSchool2(Creature* Me);
    float AsCustomSchool3(Creature* Me);
    float AsCustomSchool4(Creature* Me);
    float AsCustomSchool5(Creature* Me);
    float AsCustomSchool6(Creature* Me);

    void DoMazLevelMod(Creature* Me, uint8& MeLevel);
    float GetMazHpMod(Creature* Me);
    float GetMazManaMod(Creature* Me);
    float GetMazMeleeMod(Creature* Me);
    float GetMazSpellMod(Creature* Me);
    float GetMazRetMod(Creature* Me);
    std::string GetMazLootIds(Creature* Me);
    uint32 GetMazHpNow(Creature* Me);
    AsInt64 GetAsMazHpNow(Creature* Me);
    uint32 GetMazDmgTakenNow(Creature* Me);
    AsInt64 GetAsMazDmgTakenNow(Creature* Me);
    uint32 GetMazDmgNow(Creature* Me);
    AsInt64 GetAsMazDmgNow(Creature* Me);
    float GetMazKtHpMod(Creature* Me);

    void DoAsAddAuras(Creature* Me);
    void MazAddAuras(Creature* Me);
    bool IsInCustom(Creature* Me);

    void ChallengeAddAuras(Creature* Me);
    bool UseLegacyMapCreatureAttributes() const;
    bool IsChallengeMap(uint32 MapId);
    MazChallenge const* GetChallengeConfig(uint32 MapId, uint32 Rank) const;
    void DoApplyChallengeLogin(Player* player, uint32 MapId);
    void DoApplyChallenge(Player* player, uint32 MapId, uint32 Rank, bool showMsg = true);
    bool InChallenge(Player* player, uint32 MapId, uint32 Rank);
    void ResetChallengeLevel(Player* player, uint32 MapId, uint32 Rank);
    void AddGossipForChallenge(Player* player, uint32 TriggerId);
    uint32 GetChallengeNTempId(uint32 MapId, uint32 Rank);

    uint32 GetSumId(uint32 Stage);
    uint32 GetGobId(uint32 Stage);
    uint32 SummonCreatureGroup(Creature* Me, uint32 SumId);
    std::string GetDescription(uint32 Stage);
    void HandleStageChallengeInstanceReset(Player* player, bool isRaid);

    uint32 GetNtSumId(uint32 Rank);
    uint32 GetNtRItemId(uint32 Rank);
    uint32 NtSumCreatureGroup(Creature* Me, uint32 NtSumId);
    std::string GetNtDes(uint32 Rank);

    void OnOneInitEntry(Creature* Me, uint32& AtModeId, float& AtScale, uint32& AtWp1, uint32& AtWp2, uint32& AtWp3);
    void GetSumTelePos(uint32 PosId, uint32& MapId, float& X, float& Y, float& Z, float& O);

    void SendUpdateMultipleHp(Player* player, Creature* Killed);
    void SendResetMultipleHp(Player* player, uint32 CreGuidLow);

private:

};
#define sAsCreature AsCreature::Instance()

#endif
