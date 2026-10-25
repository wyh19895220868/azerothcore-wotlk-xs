
#ifndef _GUILDBATTLEMGR_H
#define _GUILDBATTLEMGR_H

#include "../_Helper/AsInt64.h"

typedef std::unordered_map<uint32, std::string> GuildBattleMap;
extern GuildBattleMap GuildBattleStore;

struct GuildBattleEventData
{
    AsInt64 Damage = AsInt64Zero();
    AsInt64 Heal = AsInt64Zero();
};

class GuildBattleMgr
{
public:
    static GuildBattleMgr* Instance()
    {
        static GuildBattleMgr Instance;
        return &Instance;
    }

    void ResetGuildBattleData();
    void LoadGuildBattle();
    std::string GetStrConfig(uint32 Index);
    uint32 GetUInt(uint32 Index);
    float GetFloat(uint32 Index);
    void AtGuildLoadFromDB();
    void InitGuildBattleDataWorldState(bool Start);
    GameObject* GetGuildBattleGO();
    uint32 InRangeGuildPlayerResUpdate(Map* map);
    void UpdateWorldState(uint32 Index, uint32 ResCount);
    void SendGuildBattleUI(Player* player);
    void SendGuildBattleUIToAll();
    void RewPlayerEnd(uint32 WinGuildId);

    void GuildUpdateEventDamage(Player* Att, Player* Tar, AsInt64 const& Damage);
    void GuildUpdateEventHeal(Player* Cast, Player* Tar, AsInt64 const& Heal);
    std::map<ObjectGuid, GuildBattleEventData> GuildBattleAtData;
    void OnDoAuraPlayerGuild(Player* player, uint32 ZoneId);
    void RemoveAurasStop(Player* player);
    std::set<ObjectGuid> GuildBattlePlayers;

    uint32 DefendId;
    uint32 AttackId;
    uint32 WinCount;

    std::unordered_map<uint32, uint32> AtGroupMap;

    bool InStart;
    uint32 DefendResCount;
    uint32 AttackResCount;
    uint32 NowWinId;

private:
};
#define sGuildBattleMgr GuildBattleMgr::Instance()

/*****************************************************************************************************************************************************************/

enum FacGroupType : uint32
{
    GROUP_TEAM_NULL = 0,
    GROUP_TEAM_A = 1,				                           //混排联盟阵营
    GROUP_TEAM_H = 2,				                           //混排部落阵营
};

typedef std::unordered_map<uint32, std::string> AsPlayerBattleMap;
extern AsPlayerBattleMap AsPlayerBattleStore;

struct AsPlayerBattleEventData
{
    FacGroupType Fac;
    AsInt64 Heal = AsInt64Zero();
    AsInt64 Damage = AsInt64Zero();
};

/*全服活动*/
class AsPlayerBattleMgr
{
public:
    static AsPlayerBattleMgr* Instance()
    {
        static AsPlayerBattleMgr Instance;
        return &Instance;
    }

    void LoadAsPlayerBattle();
    std::string GetStrConfig(uint32 Index);
    uint32 GetUInt(uint32 Index);
    float GetFloat(uint32 Index);

    GameObject* GetAsPlayerBattleGO();
    bool Started = false;
    void InitFacGroupBattle();
    void EndFacGroupBattle();
    std::string GetFacStr(FacGroupType Fac);
    bool CheckPlayerInBattle(Player* player);
    void ReStartInGossipBattle(Player* player);
    FacGroupType AutoFacGroup(Player* player);
    void SetTempFaction(Player* player, FacGroupType Fac);
    void ResetTempFaction(Player* player);
    void ResetLoginTempFaction(Player* player);
    uint32 GetFactionCounts(FacGroupType Fac);
    void AllocationGroupAndTele(Player* player, FacGroupType Fac);
    uint32 GetTelePosId(FacGroupType Fac);
    void CheckHandlerPlayers(uint32 ZoneId);
    FacGroupType InRangeFacPlayerResUpdate();
    void UpdateAsWorldState(uint32 Index, uint32 ResCount);
    void SendAsPlayerBattleUI(Player* player);
    void SendAsPlayerBattleUIToAll();
    void RewAsPlayerEnd(FacGroupType WinFac);
    void AsPlayerUpdateEventDamage(Player* Att, Player* Tar, AsInt64 const& Damage);
    void AsPlayerUpdateEventHeal(Player* Cast, Player* Tar, AsInt64 const& Heal);
    void LoginAddAuras(Player* player);
    void AddOrRemoveAuras(Player* player, FacGroupType Fac, bool Add);
    void RemoveAsPlayersGroupMap(uint32 GpGuidLow);


    FacGroupType NowWinFac = GROUP_TEAM_NULL;
    uint32 AResCount = 0;
    uint32 HResCount = 0;

private:
    std::unordered_map<ObjectGuid, AsPlayerBattleEventData> AsPlayersFacMap;
    std::unordered_map<uint32, FacGroupType> AsPlayersGroupMap;

};
#define sAsPlayerBattleMgr AsPlayerBattleMgr::Instance()

#endif
