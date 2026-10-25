
#ifndef _PLAYERMODMGR_H
#define _PLAYERMODMGR_H
#include "Item.h"

struct VipTemp
{
    uint32 NTempId;            //需求模版
    uint32 RTempId;            //奖励模版
    float StatsMod;                //额外属性比
    uint32 Tp;                      //增加天赋点
    float LootOdds;                //增加掉率
    float LootKn;                //爆率比例
    float UpOdds;                //升级成功率
    std::string AtAuras;            //附加的光环
    uint32 STempId;            //会员玩家模板
    std::string IconStr;            //图标字符
    std::string MenuStr;            //菜单描述
    std::string VipPix;            //名字前缀
};

typedef std::map<uint32, VipTemp> VipMap;
extern VipMap VipStore;

struct CreDmgRankTemp
{
    uint32 TempId1;            //第1名
    uint32 TempId2;            //第2名
    uint32 TempId3;            //第3名
    uint32 TempId4;            //第4名
    uint32 TempId5;            //第5名
    uint32 TempId6;            //第6名
    uint32 TempId7;            //第7名
    uint32 TempId8;            //第8名
    uint32 TempId9;            //第9名
    uint32 TempId10;          //第10名
    uint32 TempIdAs;          //参与
    AsInt64 MinDmg;          //最低伤害要求
    uint32 IPCount;          //IP地址限制
};

typedef std::map<uint32, CreDmgRankTemp> CreDmgRankTempMap;
extern CreDmgRankTempMap CreDmgRankTempStore;

#define DMGRANKMAX uint32(10)                               //伤害排行的最多显示排名

struct SignTemp
{
    uint32 ItemId1;
    uint32 Count1;
    uint32 ItemId2;
    uint32 Count2;
    uint32 ItemId3;
    uint32 Count3;
    uint32 ItemId4;
    uint32 Count4;
    uint32 ItemId5;
    uint32 Count5;
    uint32 ItemId6;
    uint32 Count6;
};

typedef std::map<uint32, SignTemp> SignRewardMap;
extern SignRewardMap SignRewardStore;

struct AuraLootItem
{
    uint32 ItemId1;
    uint32 Count1;
    uint32 ItemId2;
    uint32 Count2;
    float Odds;
};

typedef std::unordered_map<uint32, AuraLootItem> AuraLootItemMap;
extern AuraLootItemMap AuraLootItemStore;

struct EqSlotTemp
{
    uint32 Rank;
    float Mod;
    uint32 NTempId;
};

typedef std::multimap<uint32, EqSlotTemp> EqSlotTempMap;
extern EqSlotTempMap EqSlotTempStore;

typedef std::unordered_map<uint32, std::set<uint32>> EqAuraMap;
extern EqAuraMap EqAuraStore;

struct PVPKTemp
{
    float Odds;
    uint32 MinCount;
    uint32 MaxCount;
};
typedef std::unordered_map<uint32, PVPKTemp> PVPKTempMap;
extern PVPKTempMap PVPKStore;

struct PVPMapEventTemp
{
    int32 MapOrArea;
    float KillRewOdds;
    uint32 KillRewTempId;
    float KilledRewOdds;
    uint32 KilledRewTempId;
    bool OneIp;
};
typedef std::unordered_map<uint16, PVPMapEventTemp> PVPMapEventMap;
extern PVPMapEventMap PVPMapEventStore;

struct EnMapTemp
{
    uint32 FactionId;
    uint32 Class;
    std::set<uint32> AuraList;
};

typedef std::unordered_map<int32, EnMapTemp> EnMapPlayerMap;
extern EnMapPlayerMap EnMapPlayerStore;

struct CustomTalentTemp
{
    uint32 NTempId;                     //需求模版
    uint32 NeedTabPoints;             //需要当前系点数
    uint32 NeedAsPoints;              //需要全部点数
    std::set<uint32> SpellIdList;     //互斥组
};

typedef std::unordered_map<uint32, CustomTalentTemp> CustomTalentTempMap;
extern CustomTalentTempMap CustomTalentTempStore;

class PlayerModMgr
{
public:
    static PlayerModMgr* Instance()
    {
        static PlayerModMgr Instance;
        return &Instance;
    }

    ~PlayerModMgr();

    void SetVipRank(uint32 AccountId, uint32 NowRank);
    uint32 GetVipRank(uint32 AccountId);
    void AddToken(ObjectGuid Guld, uint32 AccountId, int64 Count, std::string Info);
    uint64 GetToken(uint32 AccountId);
    uint64 GetAsToken(uint32 AccountId);
    void AddTokenAH(ObjectGuid Guld, int64 Count);

    void LoadPlayerMod();
    void LoadDmgRank();
    void LoadPVPKMod();
    void LoadTalentInfoMod();
    uint32 InNotTalentInfo(Player* player, uint32 SpellId);
    bool HasTalentInfoGts(Player* player, uint32 UseTalentSpellId, uint32 TalentRank);

    void OnKillLootPlayer(Player* Killer, Player* Killed);

    uint32 GetVipNTempId(uint32 Level);
    uint32 GetVipRTempId(uint32 Level);
    float GetVipStatsMod(uint32 Level);
    uint32 GetVipTpCount(uint32 Level);
    float GetVipLootOdds(uint32 Level);
    float GetVipLootKn(uint32 Level);
    float GetVipUpOdds(uint32 Level);
    std::string GetVipAuras(uint32 Level);
    void UpdateVipAuras(Player* player, uint32 NewRank);
    uint32 GetVipMaxRank();
    uint32 GetVipSTempId(uint32 Level);
    std::string GetVipMenuStr(uint32 Level);
    std::string GetVipIconStr(uint32 Level);
    std::string GetVipPixStr(uint32 Level);
    std::string GetPlayerNameOriginal(std::string RevName);
    void UpdateNameQuery(Player* player);

    void DoOnlineRewardTimeFunc(Player* player, uint32 ptime);

    bool IsInDmgRank(uint32 CreEntry);
    AsInt64 GetDmgMin(uint32 CreEntry);
    uint32 GetIpCount(uint32 CreEntry);
    bool IpCheck(Creature* Me, Player* player);
    uint32 GetDmgRankAsTempId(uint32 CreEntry);
    uint32 GetDmgRankTempId(uint32 CreEntry, uint32 Rank);
    void AtOnCreatureUpdate(Creature* Me, uint32 Diff);
    void DmgRankOnCreatureReset(Creature* Me);
    void DmgRankOnCreatureDied(Creature* Me);
    void SendUpdateDmgRanking(Creature* Me);
    void SendResrtDmgRanking(Creature* Me);
    void DmgRankOnCreatureDamageTaken(Creature* Me, Player* player, AsInt64 const& Damage);
    std::string GetRankDmgItemLink(uint32 CreEntry, uint32 Rank);

    void PlayerOnLoginJoinGuild(Player* player);
    void PlayerOnLoginAsText(Player* player);
    void PlayerOnLogoutAsText(Player* player);

    void LoadSignMod();
    void OnKillPlayerReward(Player* Killer, Player* Killed);
    void PlayerOnLoginStatPt(Player* player);
    void StatPtOnCreatureKill(Player* player, Creature* Me);
    void AuraLootOnCreatureKill(Player* player, Creature* Me);
    void OnTeleSpellComplete(Player* player, Spell* DoSpell, bool SkipCheck);

    void OnCheckAuraPlayerMapChanged(Player* player);
    void OnKillAuraPlayer(Player* Killer, Player* Killed);
    void PlayerCastSpellComplete(Player* player, Spell* DoSpell, bool SkipCheck);

    void LoadEqSlotUpConfig();
    float GetModPlayerEqSlot(Player* player, uint32 Slot);
    float GetModShowPctSlot(uint32 NowRank, uint32 Slot);
    uint32 GetMaxRank(uint32 Slot);
    uint32 AutoEqSlot(uint32 Slot, bool Auto);
    void SendApplyMeEqSlotData(Player* player);
    uint32 GetEqSlotNTempId(uint32 NowRank, uint32 Slot);
    bool IsAsEqRank(Player* player, uint32 Rank);
    void CheckEqRankGiveAura(Player* player, uint32 DoRank, bool Login = false);
    void HandlerEqRankGiveAuraLogin(Player* player);

    void DoAutoStoreLoot(Player* player, uint32 loot_id, LootStore const& store);
    void OnDKSkipPlayer(Player* player);
    void OnCheckCanFlyPlayerMapChanged(Player* player);

    void LoadEnMapAuraData();
    void HandlerEnMapAura(Player* player);
    void HandlerEnAreaAura(Player* player);

    void CustomKillPlayer(Player* Killer, Player* Killed, uint32 KillRewTempId, float KillRewOdds, uint32 KilledRewTempId, float KilledRewOdds, bool One);

private:
};
#define sPlayerModMgr PlayerModMgr::Instance()

#endif //!
