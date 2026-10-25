
#ifndef _ATHENAEVENTMGR_H
#define _ATHENAEVENTMGR_H

struct CreAthenaTemp
{
    uint32 CreEntry;                   //生物编号
    float MoveSpeed;                   //移动速度
    uint32 OnRspCount;                   //刷新波数
    uint32 RspInterval;                   //刷新间隔
};

typedef std::map<uint32, CreAthenaTemp> CreAthenaTempMap;
extern CreAthenaTempMap CreAthenaTempStore;

typedef std::unordered_map<uint32, std::string> CreAthenaConfigMap;
extern CreAthenaConfigMap CreAthenaConfigStore;

class AthenaEventMgr
{
public:
    static AthenaEventMgr* Instance()
    {
        static AthenaEventMgr Instance;
        return &Instance;
    }

    uint32 AreaId = 0;
    bool KillAs = false;
    std::set<uint32> AthenaEventGroupList;
    std::set<ObjectGuid> PlayerInList;
    // GUIDs, not Creature*: summons can be freed without any AI callback
    // (grid unload), so raw pointers here dangled and crashed OnStop.
    GuidSet SumCreList;
    uint32 SumMapId = 0;
    uint32 SumInstanceId = 0;

    void LoadAthenaEventData();
    uint32 GetAthenaConfig(uint32 Index);
    uint32 GetAthenaEntryCountAs();
    bool InAthenaEntry(uint32 Entry);

    void AtOnCreatureUpdate(Creature* Me, uint32 Diff);

private:

};
#define sAthenaEventMgr AthenaEventMgr::Instance()

#endif
