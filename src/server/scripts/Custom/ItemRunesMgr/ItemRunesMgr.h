
#ifndef _ITEMRUNESMGR_H
#define _ITEMRUNESMGR_H
#include "Item.h"

struct AtRunesTemp
{
    uint8 MinRCount;     //最少符文槽
    uint8 MaxRCount;     //最多符文槽
    uint32 GroupId;
    uint32 MaxOpen;     //最多可开
};
typedef std::unordered_map<uint32, AtRunesTemp> AtRunesTempMap;
extern AtRunesTempMap AtRunesTempStore;

struct ItemRunesTemp
{
    uint32 MeSpellId;       //自带技能
    bool Casted;      //主动或被动
    uint32 NTempId1;       //镶嵌需求
    uint32 NTempId2;       //拆除需求
    bool Bind;      //插入是否绑定
};
typedef std::unordered_map<uint32, ItemRunesTemp> ItemRunesTempMap;
extern ItemRunesTempMap ItemRunesTempStore;

struct SpGroupTemp
{
    uint32 GroupId;
    uint32 Num;
    std::string GName;
    uint32 ResItem1;
    uint32 ResItem2;
    uint32 ResItem3;
    uint32 ResItem4;
    uint32 ResItem5;
    uint32 ResItem6;
    uint32 ResItem7;
    uint32 ResItem8;
    std::string SpellListStr;
};
typedef std::vector<SpGroupTemp> SpGroupTempVect;
extern SpGroupTempVect SpGroupTempStore;

struct ItemASlotRunesTemp
{
    uint32 NTempId;
    float Chance;
};
typedef std::unordered_map<uint32, ItemASlotRunesTemp> ItemASlotRunesTempMap;
extern ItemASlotRunesTempMap ItemASlotRunesTempStore;

struct ItemRunesCacheTemp
{
    uint32 ItemId;
    uint8 NowCount;
    uint8 RunesCount;
    uint32 ResItems[MAX_RUNES_COUNT] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    uint32 OSpells[MAX_RUNES_COUNT] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    std::string GSpellName = "";
    std::string GSpellListStr = "";
};
typedef std::unordered_map<uint32, ItemRunesCacheTemp> ItemRunesCacheMap;
extern ItemRunesCacheMap ItemRunesCacheStore;

class ItemRunesMgr
{
public:
    static ItemRunesMgr* Instance()
    {
        static ItemRunesMgr Instance;
        return &Instance;
    }

    ItemRunesMgr();
    ~ItemRunesMgr();

    void LoadItemRunes();

    bool IsRunesItem(uint32 ItemId);
    bool IsAtRunesItem(uint32 RunesItem);
    void AutoGenerateRandomRunesCount(Item* DoItem);
    uint32 GetAddNtempId(uint32 RunesItem);
    uint32 GetDelNtempId(uint32 RunesItem);
    uint32 GetOSpellId(uint32 RunesItem);

    std::vector<uint32> DeleteZero(std::vector<uint32> Items);
    bool IsFillVectEqual(std::vector<uint32>& A, std::vector<uint32>& B);

    std::pair<std::string, std::string> PullGSpellStore(Item* DoItem, uint32 GroupId);
    void HandlersItemRunesOnNow(Item* DoItem, uint32 GroupId);
    bool DoAddRunes(Item* DoItem, uint32 RunesItem, uint8 Slot, Player* player);
    bool DoDeleteRunes(Item* DoItem, uint32 RunesItem, uint8 Slot, Player* player);
    bool DoDeleteAsRunes(Item* DoItem, Player* player);
    bool DoAddSlotRunes(Item* DoItem, Player* player);
    bool IsTaskBind(uint32 RunesItem);

    void LoadForDBRunes(Item* DoItem);
    void OnApplyItemRunesSpell(Player* player, Item* KItem, bool Apply);

    bool HasPlayerRunesItemIdAlls(Player* player, uint32 ItemId);
    bool IsRunesCasted(uint32 ResItemId);

private:

};
#define sItemRunesMgr ItemRunesMgr::Instance()

#endif //!
