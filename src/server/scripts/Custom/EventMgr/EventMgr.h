
#ifndef _EVENTMGR_H
#define _EVENTMGR_H

#define MINEVENTSO 100

#define TYPEID_UNIT_ENTRY 8

enum GroupTypes
{
    C_GT_ONE_ALL,				                           //全部阵营
    C_GT_ONE_A,								               //联盟阵营1BL阵营2
    C_GT_ONE_H,						                       //联盟阵营2BL阵营1
    C_GT_RANDOM_A_H,						           //阵营随机
    C_GT_RANDOM_PLAYER,				           //玩家随机
};

enum RejoinTypes
{
    C_RT_BANED,
    C_RT_KEEP_GROUP,
    C_RT_REDIS_GROUP,
};

enum EventTypes
{
    C_ET_NONE,
    C_ET_WS,
    C_ET_AB,
    C_ET_EY,
    C_ET_SA,
    C_ET_AV,
    C_ET_IC,
    C_ET_TELE,
    C_ET_LINE,
    C_ET_GUILD,
};

struct EventPhaseFactionTemplate
{
    uint8 Phase;
    uint32 Faction;
    uint32 GraveyardPos;
    uint32 GraveyardSafe;
    uint32 GraveyardHealer;
    uint32 StopTelePos;
    uint32 StopPlayerKills;
    uint32 StopPlayerKilleds;
    std::string GraveyardSafeNotice;
    std::string StopNotice;
};

struct EventPhaseTemplate
{
    uint8 Phase;
    int32 StopGUID;
};

#define ALT_GUID_MAX 5

struct EventAltTemplate
{
    int32 GUID;
    uint32 AltFlag;
    std::string NoticeText;
    int64 TargetEntry[ALT_GUID_MAX];
    uint32 TargetCount[ALT_GUID_MAX];
    uint32 TargetKillCount[ALT_GUID_MAX];
};

struct EventAltFactionTemplate
{
    int32 GUID;
    uint32 Faction;
    uint32 Kills;
    uint32 Killeds;
};

struct EventStopTemplate
{
    uint32 Faction;
    uint32 StopType;
    uint32 Param;
    uint32 AtCount;
    uint32 Weight;
};

struct EventKillInfoTemplate
{
    uint32 Kills;
    uint32 Killeds;
};

#define WORLDSTATE_ENTRY_MAX 3

struct EventWorldStateTemplate
{
    uint32 ID;
    int32 InitValue;
    int32 Entry[WORLDSTATE_ENTRY_MAX];
    int32 Step[WORLDSTATE_ENTRY_MAX];
    int32 NoticeValue;
    std::string Notice;
};

struct EventWorldStateFactionTemplate
{
    uint32 ID;
    uint32 Faction;
    bool Kill;
    int32 Step;
    int32 NoticeValue;
    std::string Notice;
};

#define MAX_SPAWNNOTICE_COUNT 3

struct EventSpawnTemplate
{
    int32 Entry;
    uint32 Phase;
    uint32 SpawnTime;
    uint32 PosId;
    uint32 MovePosId;
    uint32 Duration;
    uint32 NoticeTime[MAX_SPAWNNOTICE_COUNT];
    std::string SpawnNotice[MAX_SPAWNNOTICE_COUNT];
};

enum TEvents
{
    T1 = 101,
    T2 = 102,
};

// 活动数据结构体
typedef struct EventData
{
    uint32 EventId;
    std::string EventName;
    EventTypes EventType;
    GroupTypes GroupType;
    RejoinTypes RejoinType;
    std::string NoticeText;
    std::string GossipText;
    int32 MapId;
    int32 ZoneId;
    Map* Map;
    uint8 Phase;
    bool RandomAH;
    uint32 WinRewId;
    uint32 LosRewId;
    AsInt64 DamageForRew;
    AsInt64 HealForRew;
    uint32 KillsForRew;
    uint32 KilledForRew;

    std::unordered_map<uint32, std::string> FactionNameMap;
    std::unordered_map<uint32, uint32> StartPosMap;
    std::unordered_map<uint32, uint32> StopPosMap;
    std::unordered_map<uint32, uint32> GroupMap;
    std::vector<uint32> FactionVec;
    std::unordered_map<ObjectGuid, uint32> PlayersMap;
    std::unordered_map<uint32, EventKillInfoTemplate> KillInfoMap;

    std::vector<EventStopTemplate> StopVec;

    std::vector<EventAltTemplate> AltVec;
    std::vector<EventAltFactionTemplate> AltFactionVec;
    std::vector<uint32> AltedVec;

    std::vector<EventPhaseFactionTemplate> PhaseFactionVec;
    std::vector<EventPhaseTemplate> PhaseVec;

    std::unordered_map<uint32, ObjectGuid> HealerMap;

    std::vector<EventWorldStateTemplate> WorldStateVec;
    std::vector<EventWorldStateFactionTemplate> WorldStateFactionVec;

    std::vector<EventSpawnTemplate> SpawnVec;
    uint32 UpdateSpawnTimer;

    uint32 UpdateSafeTimer;
    uint32 UpdateReviveTimer;

    bool DelayStartCall;
    bool DelayStopCall;
    uint32 UpdateDelayNotice;
    uint32 WinFaction;

    // 构造函数
    EventData() : EventId(0), EventType(C_ET_NONE), GroupType(C_GT_RANDOM_PLAYER), RejoinType(C_RT_REDIS_GROUP),
        MapId(-1), ZoneId(-1), Map(nullptr), Phase(1), RandomAH(urand(1, 100) > 50 ? false : true),
        WinRewId(0), LosRewId(0), DamageForRew(0), HealForRew(0), KillsForRew(0), KilledForRew(0),
        UpdateSpawnTimer(0), UpdateSafeTimer(0), UpdateReviveTimer(0),
        DelayStartCall(false), DelayStopCall(false), UpdateDelayNotice(0), WinFaction(0) {}
};

class CustomEventMgr
{
public:
    static CustomEventMgr* Instance()
    {
        static CustomEventMgr Instance;
        return &Instance;
    }

public:

    CustomEventMgr();

    void Reset(uint32 eventId = 0);
    void InitParams(uint32 eventId);
    void Start(uint32 eventId);
    void Stop(uint32 eventId);
    bool CanRew(Player* pl, uint32 eventId);
    void Update(Map* NowMap, uint32 diff);
    void UpdateRevive(uint32 diff, uint32 eventId);
    void UpdateSafe(uint32 diff, uint32 eventId);
    int32 GetWeightLeft(uint32 faction, uint32 eventId);
    float GetDistance(Player* player, uint32 pos);
    void Tele(Player* player, uint32 pos);
    void TeleToGraveyard(Player* player, uint32 eventId);
    void ReviveAll(bool start);
    void StartCall(uint32 eventId);
    void RewAndRest(uint32 eventId);
    void Join(Player* pl, uint32 eventId);
    uint32 Subgroup(Player* pl, uint32 eventId);

    void UpdateActive(Object* obj, bool kill = false, uint32 eventId = 0);
    void UpdatePhase(Object* obj, bool kill = false, uint32 eventId = 0);
    void UpdateWorldState(uint32 ID, uint32 value, uint32 eventId);
    void InitWorldState(uint32 eventId);
    void UpdateWorldState(Object* obj, bool kill = false, uint32 eventId = 0);
    void UpdateSpawn(uint32 diff, uint32 eventId);
    void Process(Object* obj, uint32 eventId = 0);
    uint32 GetFactionMembers(uint32 faction, uint32 eventId);
    bool DiedTele(Player* player, uint32 eventId = 0);
    bool SendAreaSpiritHealerQueryOpcode(Player* player, uint32 eventId = 0);
    void GetTelePos(uint32 posId, uint32& map, float& x, float& y, float& z, float& o);
    bool IpCheck(Player* player, uint32 eventId);
    bool IsFixedTimeBgActive(BattlegroundTypeId bgTypeId);
    int32 GetZoneId(uint32 eventId);
    int32 GetMapId(uint32 eventId);
    void SummonSoulHealer(uint32 eventId);
    void SendScreenMessage(std::string text, uint32 faction = 0, bool onlydead = false, uint32 eventId = 0);
    bool Valid(uint32 eventId);
    void StartPlayerInEvent(Player* player, uint32 eventId);
    GroupTypes GetGType(uint32 eventId);
    bool IsInAltedVec(uint32 GuidLow, uint32 eventId);

    // 获取活动数据
    EventData* GetEventData(uint32 eventId);

    // 更新击杀信息
    void UpdateKillInfo(Player* player, bool kill, uint32 eventId);
private:
    // 活动数据映射表
    std::unordered_map<uint32, EventData> _eventDataMap;

    // 获取阶段墓地位置
    uint32 GetPhaseGraveyardPos(uint32 faction, uint32 eventId);

    // 获取击杀信息
    void GetKillInfo(uint32 faction, uint32 &Kills, uint32 &Killeds, uint32 eventId);
};
#define sEvent CustomEventMgr::Instance()

#endif
