
#ifndef _ITEMUPMGR_H
#define _ITEMUPMGR_H

struct MUpTemp
{
    uint32 NewId;
    uint32 MType;
    uint32 TempId;
    bool OnSave;                 //随机附魔
    bool OnWp;                 //超级武器伤害
    bool OnSt;                 //超级属性
    bool OnSp;                 //超级技能
    float Chance;
    uint32 XyItem;                      //幸运宝石
    float XyOdds;                      //每个的幸运几率
    uint32 XyCount;                      //幸运数量
    uint32 FBItem;                      //防爆宝石
    bool Flag;                             //是否碎裂
};

typedef std::unordered_map<uint32, MUpTemp> MUpMap;
extern MUpMap MUpStore;

class ItemUpMgr
{
public:
    static ItemUpMgr* Instance()
    {
        static ItemUpMgr Instance;
        return &Instance;
    }

    void LoadItemUpData();
    bool InItemUpData(uint32 ItemId);

private:

};
#define sItemUpMgr ItemUpMgr::Instance()

#endif
