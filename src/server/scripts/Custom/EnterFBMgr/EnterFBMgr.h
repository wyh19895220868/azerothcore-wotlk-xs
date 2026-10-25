
#ifndef _ENTERFBMGR_H
#define _ENTERFBMGR_H

struct EnterFBInfo
{
    int32 Diff;
    std::vector<uint32> CItems;
    uint32 NTempId;
    uint32 AsItemLevel;
    std::string NotType;
};

typedef std::multimap<uint32, EnterFBInfo> EnterFBMap;
extern EnterFBMap EnterFBStore;

struct ResetFBInfo
{
    int32 Num;
    int32 Menu;
    std::string Gossip;
    int32 MapId;
    int32 Diff;
    uint32 NTempId;
    uint32 Count;
};

typedef std::unordered_map<uint32, ResetFBInfo> ResetFBMap;
extern ResetFBMap ResetFBStore;

class EnterFBMgr
{
public:
    static EnterFBMgr* Instance()
    {
        static EnterFBMgr Instance;
        return &Instance;
    }

    void LoadEnterFBMgr();
    void PullPlayerItemLevel(Player* player, uint32 MapId, bool IsRaid);
    uint32 GetAsLevel(uint32 MapId, int32 Diff);
    bool InNotType(uint32 InventoryType, std::string NotType);
    bool IsDoCheckItem(Player* player, uint32 MapId, bool IsRaid);
    bool IsDoItemLevel(Player* player, uint32 MapId, bool IsRaid);
    uint32 GetTempId(uint32 MapId, int32 Diff);
    uint32 GetCount(uint32 MapId, uint32 Diff);

private:

};
#define sEnterFBMgr EnterFBMgr::Instance()

#endif
