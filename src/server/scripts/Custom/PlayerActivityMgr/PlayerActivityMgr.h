
#ifndef _ACTIVITYMGR_H
#define _ACTIVITYMGR_H

enum ActivityTypes
{
    A_KILL_CRE = 1,								               //击杀怪物
    A_KT_ITEM,						                       //收集物品
    A_QUEST,						                           //完成任务
    A_KILL_PLR,						                           //击杀玩家
    A_BG_WIN,						                           //赢得战场
};

struct ActivityTemp
{
    uint32 Num;
    ActivityTypes AType;
    uint32 Param1;
    uint32 Param2;
    uint32 ActivityValue;
    std::string Info;
};

typedef std::vector<ActivityTemp> ActivityTempVect;
extern ActivityTempVect ActivityStore;

typedef std::map<uint32, uint32> RewActivityTempMap;
extern RewActivityTempMap RewActivityStore;

class ActivityMgr
{
public:
    static ActivityMgr* Instance()
    {
        static ActivityMgr Instance;
        return &Instance;
    }

    void LoadActivityData();
    std::string GetActivityInfoByAType(ActivityTypes AType, uint32 Param);
    uint32 GetAsActivity();
    void SendPacketActivityData(Player* player);
    void DayResetSendPacketActivityData();
    void SendPlayerActivityData(Player* player);
    void UpdatePlayerOnePopActivity(Player* player);
    bool IsOnRewPopValue(Player* player, uint32 PopValue);
    void SetOnRewPopValueAndUpdateDB(Player* player, uint32 PopValue);

    uint32 GetRewActivity(uint32 Num, uint32 AType, uint32 Param1);
    uint32 GetRewActivitySc(uint32 Num, uint32 AType);
    bool IsComOrMax(Player* player, uint32 Num, uint32 AType, uint32 Param1, uint32 DBCount);
    bool IsComOrMaxSc(Player* player, uint32 Num, uint32 AType, uint32 DBCount);
    std::pair<uint32, uint32> GetScNumDBCountByParamAType(uint32 AType);
    std::pair<uint32, uint32> GetNumDBCountByParamAType(uint32 AType, uint32 Param1);
    void UpdatePlayerActivityDataDB(Player* player, uint32 DBCount, uint32 Num, uint32 AType, uint32 Param1, uint32 Param2 = 1);
    void UpdatePlayerUIDataActivity(Player* player);

private:

};
#define sActivityMgr ActivityMgr::Instance()

#endif
