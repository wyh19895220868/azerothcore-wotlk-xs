
#ifndef _ITEMRANDOMATCMGR_H
#define _ITEMRANDOMATCMGR_H
#include "Item.h"

#define MAX_EN_COUNT 5       //最多随机附魔

struct GenerateItemAt
{
    uint32 GroupTempId;                       //超级模板组
    uint32 NTempId;                       //需求模板
    uint32 OnCover;
    float Chance;
    uint32 _Attris;
};
typedef std::unordered_map<uint32, GenerateItemAt> GenerateItemAtMap;
extern GenerateItemAtMap GenerateItemAtStore;

struct GTempItemAt
{
    uint32 GroupTempId;                       //超级模板组
    uint32 Num;
    float Chance;
    std::string Prefix;
    std::string Suffix;
    AsInt64 MinWpDmg1;
    AsInt64 MinWpDmg2;
    AsInt64 MaxWpDmg1;
    AsInt64 MaxWpDmg2;
    uint32 StatGroupId;
    uint32 StatCount;
    uint32 SpellGroupId;
    uint32 SpellCount;
    uint32 EnchGroupId;
    uint32 EnchCount;
};

typedef std::vector<GTempItemAt> GTempItemList;
typedef std::unordered_map<uint32, GTempItemList> GTempItemListMap;
extern GTempItemListMap GTempItemAtStore;

struct StatItemAt
{
    uint32 GroupId;
    int32 SType;
    AsInt64 MinValue;
    AsInt64 MaxValue;
    float Chance;
};

typedef std::vector<StatItemAt> StatItemGroupList;
typedef std::unordered_map<uint32, StatItemGroupList> StatItemGroupMap;
extern StatItemGroupMap StatItemAtStore;

struct SpellItemAt
{
    uint32 GroupId;
    uint32 SpellId;
    float Chance;
};

typedef std::vector<SpellItemAt> SpellItemGroupList;
typedef std::unordered_map<uint32, SpellItemGroupList> SpellItemGroupMap;
extern SpellItemGroupMap SpellItemAtStore;

struct UItemAtTemp
{
    uint32 ItemId;
    uint32 ULevel;
    float NewAddChance;     //生成/获取时直接的等级
    float Mod;             //强化百分比
    uint32 SpellGroupId;   //技能组
    uint32 SpellCount;     //技能条数
    uint32 NTempId;             //升级需求模板编号
    uint32 OnSave;
    uint32 OnAtUpSave;
    float UChance;             //成功率
    float DChance;             //失败后被降级的几率
    uint32 DLevel;             //被降低的等级
    uint32 NewId;             //新编号
    std::string LevelStr;
    uint32 XyItem;                      //幸运宝石
    float XyOdds;                      //每个的幸运几率
    uint32 XyCount;                      //幸运数量
    uint32 FBItem;                      //防爆宝石
    bool Flag;                             //是否碎裂
};

typedef std::vector<UItemAtTemp> UItemAtTempList;
typedef std::unordered_map<uint32, UItemAtTempList> UItemAtTempMap;
extern UItemAtTempMap UItemAtTempStore;

struct SuperUpSellTemp
{
    uint32 ItemId;
    uint32 Level;
    uint32 RTempId;
};
typedef std::vector<SuperUpSellTemp> SuperUpSellTempVect;
extern SuperUpSellTempVect SuperUpSellStore;

struct EnchItemAt
{
    uint32 GroupId;
    uint32 EnchId;         //附魔编号
    float Chance;
};

typedef std::vector<EnchItemAt> EnchItemGroupList;
typedef std::unordered_map<uint32, EnchItemGroupList> EnchItemGroupMap;
extern EnchItemGroupMap EnchItemAtStore;

struct ItemAtCacheTemp
{
    uint32 ItemId;
    uint32 ULevel;
    std::string Prefix;
    std::string Suffix;
    std::string LevelStr;
    AsInt64 MinWpDmg;
    AsInt64 MaxWpDmg;
    RmAtStatVect AtStatStore = { { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0}, { -1, 0} };
    uint32 AtSpell[MAX_AT_B_COUNT] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    AsInt64 _AtSpell[MAX_SX_B_COUNT] = { AsInt64(1), AsInt64(1), AsInt64(1), AsInt64(1), AsInt64(1), AsInt64(1), AsInt64(1) };
    float Mod;
};
typedef std::unordered_map<uint32, ItemAtCacheTemp> ItemAtCacheMap;
extern ItemAtCacheMap ItemAtCacheStore;

struct OnUpRankAt
{
    uint32 UseItemId;
    uint32 TarItemId;
    uint32 ORank;
    float OChance;
};
typedef std::vector<OnUpRankAt> OnUpRankAtVect;
extern OnUpRankAtVect OnUpRankAtStore;

struct ResetResAt
{
    uint32 UseItemId;
    uint32 TarItemId;
    uint32 GroupTempId;             //超级模板组号
    uint32 NTempId;             //需求模板编号
    uint32 OnSave;
    uint32 OnAtUpSave;
    float UChance;
};
typedef std::vector<ResetResAt> ResetResAtVect;
extern ResetResAtVect ResetResAtStore;

struct RAtLootGroupTemp
{
    uint32 RAtLootGroup;
    uint32 ItemId;
    float Chance;
    uint32 SpGTempId;             //超级模版组号
};

typedef std::vector<RAtLootGroupTemp> RAtLootGroupVect;
extern RAtLootGroupVect RAtLootGroupStore;

struct DoTemp { uint32 ItemId; float Chance; int32 SpGTempId; };

class ItemRandomAtMgr
{
public:
    static ItemRandomAtMgr* Instance()
    {
        static ItemRandomAtMgr Instance;
        return &Instance;
    }

    void LoadItemRandomAt();
    bool IsGenerateItemAt(uint32 ItemId);
    bool InGTempItemAtStore(uint32 GroupTempId);
    float GetModForRank(uint32 ItemId, uint32 Level);
    uint32 AutoGroupNum(uint32 GroupTempId);

    RmAtStatVect DoGenerateGroupStatTypeValue(StatItemGroupList& DoVect, uint32 Count);
    std::vector<uint32> DoGenerateGroupSpellId(SpellItemGroupList& DoVect, uint32 Count);
    std::vector<uint32> DoGenerateGroupEnchId(EnchItemGroupList& DoVect, uint32 Count);

    void DoGenerateItemAt(Item* DoItem, uint32 CreateLevel = 0, bool Reset = true);
    void DoGenerateItemAtForDevelop(Item* DoItem, uint32 GroupTempId);
    void DoGenerateItemAtLoot(Item* DoItem, uint32 CreateLevel, int32 TempGroupId);
    void DoUpPowerItemAt(Item* DoItem, uint32 SetLevel, bool Reset = true);
    bool DoResetItemAt(Item* DoItem, uint32 NTempId, uint32 GTempId, bool ResetEnch, bool ResetAtUp);
    void DoSaveDBItemAtAndUpdate(Item* DoItem);
    void DoItemAtLoadForDB(Item* DoItem);
    uint32 GetSuperUpRTempId(uint32 ItemId, uint32 Level);
    bool DoSuperUpSellItem(Player* player, uint32 ItemId, uint32 Level, uint32 Count);

    bool IsInItemSuperSellNow(uint32 ItemId);
    bool IsInItemSuperUp(uint32 ItemId);
    uint32 GetNowULevel(uint32 GuidLow);
    uint32 GetNextULevel(uint32 GuidLow, uint32 ItemId);
    uint32 GetMaxULevel(uint32 ItemId);
    float GetUMod(uint32 ItemId, uint32 ULevel);
    uint32 GetUNTempId(uint32 ItemId, uint32 ULevel);
    bool GetUOnSave(uint32 ItemId, uint32 ULevel);
    float GetUChance(uint32 ItemId, uint32 ULevel);
    float GetDChance(uint32 ItemId, uint32 ULevel);
    uint32 GetDLevel(uint32 ItemId, uint32 ULevel);
    uint32 GetNewAddToLevel(uint32 ItemId);
    uint32 GetNewId(uint32 ItemId, uint32 ULevel);

    void OnApplyItemAtStoreStat(Player* player, Item* KItem, uint8 Slot, bool Apply);
    void OnApplyItemAtStoreSpell(Player* player, Item* KItem, bool Apply);

    void OnApplyItemAtStoreStatToHF(Player* player, Item* KItem, bool Apply);
    void OnApplyItemAtStoreSpellToHF(Player* player, Item* KItem, bool Apply);

    float GetOChance(uint32 ItemId, uint32 TargetItemId);
    bool InUpDo(uint32 ItemId, uint32 TargetItemId);
    uint32 GetInUpRank(uint32 ItemId, uint32 TargetItemId);

    bool IsWpItem(uint8 Slot, ItemTemplate const* Proto);
    float AutoWpMinDamage(Player* player, uint8 Slot, ItemTemplate const* Proto, Item* KItem);
    float AutoWpMaxDamage(Player* player, uint8 Slot, ItemTemplate const* Proto, Item* KItem);

    uint32 GetResetNTempId(uint32 ItemId, uint32 TargetItemId);
    float GetResetOChance(uint32 ItemId, uint32 TargetItemId);
    bool InResetOnSave(uint32 ItemId, uint32 TargetItemId);
    bool InResetOnAtUpSave(uint32 ItemId, uint32 TargetItemId);
    bool IntResetDo(uint32 ItemId, uint32 TargetItemId);
    uint32 GetResetSpGroupId(uint32 ItemId, uint32 TargetItemId);
    std::string GetLevelStr(uint32 ItemId, uint32 Level);

    uint32 GetStTempId(Creature* Me);
    void DeleteForDoVect(std::vector<DoTemp> &DoVect, int32 ItemId);
    std::vector<std::pair<int32, int32>> DoGenerateGroupLootItemAndStTempId(std::vector<DoTemp> DoVect, uint32 Count);
    void AsRandomAtCustomFillLoot(Creature* Me, Loot* OnLoot, Player* Looter);

    uint32 GetSuperXyItem(uint32 ItemId, uint32 Level);
    float GetSuperXyOdds(uint32 ItemId, uint32 Level);
    uint32 GetSuperXyCount(uint32 ItemId, uint32 Level);
    uint32 GetSuperFBItem(uint32 ItemId, uint32 Level);
    bool GetSuperFBFlag(uint32 ItemId, uint32 Level);

private:

};
#define sItemRandomAtMgr ItemRandomAtMgr::Instance()

#endif //!
