
#ifndef _PICGUIDEMGR_H
#define _PICGUIDEMGR_H

struct PicItemGuideConfig
{
    uint32 Num;                                   //序号
    std::string SubName;                      //子项名称
    std::string SubIcon;                        //子项图标
    uint32 SubPageId;                         //子项的页面编号
    uint32 BondsId;                           //羁绊编号
};

typedef std::map<uint32, PicItemGuideConfig> PicItemGuideConfigMap;
extern PicItemGuideConfigMap PicItemGuideConfigStore; //物品
typedef std::map<uint32, uint32> PageItemBondsMap;
extern PageItemBondsMap PageItemBondsStore;

struct PicItemGuidePage
{
    uint32 PageId;                               //页面编号
    uint32 ItemId;                               //物品编号
    uint32 RTempId;                           //奖励模版
    bool IsActStat;                              //激活是否生效属性
    bool IsActSpell;                            //激活是否生效技能
    bool IsDelect;                               //是否扣除
    uint32 Sort;                                  //排序
};

typedef std::multimap<uint32, PicItemGuidePage> PicItemGuidePageMultMap;
extern PicItemGuidePageMultMap PicItemGuidePageMultStore;

struct PicItemGuideBonds
{
    uint32 BondsId;                               //羁绊编号
    uint32 BondsCount;                        //激活件数
    std::set<uint32> BondsSpells;        //羁绊效果
    std::string Comment;                      //效果说明
};

typedef std::multimap<uint32, PicItemGuideBonds> PicItemGuideBondsMap;
extern PicItemGuideBondsMap PicItemGuideBondsStore;

struct PicCreatureGuideConfig
{
    uint32 Num;                                   //序号
    std::string SubName;                      //子项名称
    std::string SubIcon;                        //子项图标
    uint32 SubPageId;                         //子项的页面编号
    uint32 BondsId;                           //羁绊编号
};

typedef std::map<uint32, PicCreatureGuideConfig> PicCreatureGuideConfigMap;
extern PicCreatureGuideConfigMap PicCreatureGuideConfigStore; //怪物
typedef std::map<uint32, uint32> PageCreatureBondsMap;
extern PageCreatureBondsMap PageCreatureBondsStore;

struct PicCreatureGuidePage
{
    struct KillReward
    {
        uint32 Level;           //数据库配置的等级
        uint32 KillCount;       //达到本级所需的累计击杀数
        uint32 StageKillCount;  //本级单独需要的击杀数
        uint32 StatType;
        int32 StatValue;
    };

    uint32 PageId;                               //页面编号
    uint32 CreId;                               //生物编号
    uint32 Diff;                                 //难度
    uint32 RTempId;                           //奖励模版
    uint32 Sort;                                  //排序
    bool StackStats;                           //各档属性是否叠加
    std::vector<KillReward> KillRewards;       //按击杀数量分档的属性奖励

    uint32 GetMaxKillCount() const
    {
        return KillRewards.empty() ? 0 : KillRewards.back().KillCount;
    }
};

typedef std::multimap<uint32, PicCreatureGuidePage> PicCreatureGuidePageMultMap;
extern PicCreatureGuidePageMultMap PicCreatureGuidePageMultStore;

struct PicCreatureGuideBonds
{
    uint32 BondsId;                               //羁绊编号
    uint32 BondsCount;                        //激活件数
    std::set<uint32> BondsSpells;        //羁绊效果
    std::string Comment;                      //效果说明
};

typedef std::multimap<uint32, PicCreatureGuideBonds> PicCreatureGuideBondsMap;
extern PicCreatureGuideBondsMap PicCreatureGuideBondsStore;

class PicGuideMgr
{
public:
    static PicGuideMgr* Instance()
    {
        static PicGuideMgr Instance;
        return &Instance;
    }

    void LoadPicGuideData();
    uint32 GetItemBondsIdFromSubPage(uint32 SubPageId);
    PicItemGuidePage* GetItemPageData(uint32 ItemId, uint32 SubPageId);

    uint32 GetCreatureBondsIdFromSubPage(uint32 SubPageId);
    PicCreatureGuidePage* GetCreaturePageData(uint32 CreId, uint32 SubPageId);

private:

};
#define sPicGuideMgr PicGuideMgr::Instance()

#endif
