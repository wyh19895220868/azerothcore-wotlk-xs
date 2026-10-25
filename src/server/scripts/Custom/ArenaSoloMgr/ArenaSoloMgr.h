
#ifndef _ARENASOLOMGR_H
#define _ARENASOLOMGR_H

typedef std::unordered_map<uint32, std::string> ArenaSoloConfigMap;
extern ArenaSoloConfigMap ArenaSoloConfigStore;

struct SoloGuidTemp
{
    ObjectGuid AGuid;
    ObjectGuid BGuid;
};

typedef std::unordered_map<Battleground*, SoloGuidTemp> ArenaSoloInFtMap;
extern ArenaSoloInFtMap ArenaSoloInFtStore;

class ArenaSoloMgr
{
public:
    static ArenaSoloMgr* Instance()
    {
        static ArenaSoloMgr Instance;
        return &Instance;
    }

    void LoadArenaSoloConfig();
    const char* GetConfig(uint32 Index);
    std::string GetStrConfig(uint32 Index);
    int64 GetInt(uint32 Index);
    uint64 GetUInt(uint32 Index);
    float GetFloat(uint32 Index);
    bool GetBoolean(uint32 Index);

    bool ArenaSoloCheckTalents(Player* player);
    void AutoInitPlayerInSoloCount();

private:

};
#define sArenaSoloMgr ArenaSoloMgr::Instance()

#endif
