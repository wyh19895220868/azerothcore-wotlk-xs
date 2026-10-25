
#ifndef _ITEMDEVELOPMGR_H
#define _ITEMDEVELOPMGR_H

struct DevelopTemp
{
    uint32 ItemId;                    //装备编号
    uint32 Rank;                    //成长等级
    uint32 NeedXp;                    //成长经验
    uint32 NewIdType;                    //满足后物品编号
    uint32 STempId;                    //满足后超级模版
    int32 MapType;                    //地图类型
    uint32 MapEntry;                    //地图类型编号
    uint32 KillXpMin1;                    //击杀普通怪物经验Min
    uint32 KillXpMax1;                    //击杀普通怪物经验Max
    uint32 KillXpMin2;                    //击杀BOSS经验Min
    uint32 KillXpMax2;                    //击杀BOSS经验Max
    std::set<uint32> ToEntrySets;                    //指定怪物编号集合
    uint32 KillXpMin3;                    //指定怪物经验Min
    uint32 KillXpMax3;                    //指定怪物经验Max
    uint32 UpRankType;                    //成长设置选项
};

typedef std::vector<DevelopTemp> DevelopConfigVect;
extern DevelopConfigVect DevelopConfigStore;

struct UseXpGroupTemp
{
    uint32 UseItemId;                    //物品成长卷轴编号
    uint32 TarItemId;                    //目标编号
    uint32 RewXpMin;                    //经验Min
    uint32 RewXpMax;                    //经验Max
};

typedef std::vector<UseXpGroupTemp> UseXpGroupVect;
extern UseXpGroupVect UseXpGroupStore;

struct ItemDevelopCacheTemp
{
    uint32 ItemId;
    uint32 Rank;
    uint32 NowXp;
};
typedef std::unordered_map<uint32, ItemDevelopCacheTemp> ItemDevelopCacheMap;
extern ItemDevelopCacheMap ItemDevelopCacheStore;

class ItemDevelopMgr
{
public:
    static ItemDevelopMgr* Instance()
    {
        static ItemDevelopMgr Instance;
        return &Instance;
    }

    void LoadItemDevelopData();
    bool IsInItemDevelop(uint32 ItemId);
    uint32 GetDevelopNextRankXp(uint32 ItemId, uint32 NextRank);
    std::set<Item*> AutoPlayerAsInDevelop(Player* player);
    uint32 GetUseItemDevelopRankXp(uint32 UseItemId, uint32 TarItemId);
    std::pair<uint32, uint32> GetItemDevelopDataCt(uint32 ItemId, uint32 NextRank);
    void CreateSendDataDevelop(Item* DoItem);
    void LoadFromDBDevelop(Item* DoItem);

private:

};
#define sItemDevelopMgr ItemDevelopMgr::Instance()

#endif
