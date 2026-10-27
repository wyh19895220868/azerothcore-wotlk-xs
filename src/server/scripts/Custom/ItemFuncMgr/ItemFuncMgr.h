
#ifndef _ITEMFUNCMGR_H
#define _ITEMFUNCMGR_H

#include "../_Helper/AsInt64.h"

struct CFunction
{
    uint32 OEntry;
    uint32 Num;
    int32 MenuId;
    std::string Ttitle;
    int32 AMenuId;
    uint32 FactionId;
    uint32 ShowNeedTempId;
    uint32 NTempId;
    uint32 PosId;
    std::string Cmd;
};
typedef std::multimap<uint32, CFunction> FunctionMap;
typedef std::map<uint32, CFunction> FunctionMenuMap;
extern FunctionMap FunctionStore;
extern FunctionMenuMap FunctionMenuStore;

typedef std::vector<uint32> ItemInStatVect;
extern ItemInStatVect ItemInStatStore;

struct LuckTemp
{
    uint32 ItemId;
    uint32 Count;
    float Odds;
    uint32 Ann;
};
typedef std::vector<LuckTemp> LuckTempVect;
extern LuckTempVect LuckTempStore;

struct ItemCommandTemp
{
    std::string Cmd;
    std::string RCmd;
    uint32 OnDel;
    uint32 NTempId;
};
typedef std::unordered_map<uint32, ItemCommandTemp> ItemCommandMap;
extern ItemCommandMap ItemCommandStore;

struct UpRandomTemp
{
    uint32 TempId;
    uint32 OnSave;
    uint32 NewId1;
    float Chance1;
    uint32 NewId2;
    float Chance2;
    uint32 NewId3;
    float Chance3;
    uint32 XyItem;                      //幸运宝石
    float XyOdds;                      //每个的幸运几率
    uint32 XyCount;                      //幸运数量
};
typedef std::unordered_map<uint32, UpRandomTemp> ItemUpRandomMap;
extern ItemUpRandomMap ItemUpRandomStore;

struct UpChoiceTemp
{
    uint32 TempId;
    uint32 OnSave;
    uint32 NewId1;
    float Chance1;
    uint32 NewId2;
    float Chance2;
    uint32 NewId3;
    float Chance3;
    uint32 XyItem;                      //幸运宝石
    float XyOdds;                      //每个的幸运几率
    uint32 XyCount;                      //幸运数量
    uint32 FBItem;                      //防爆宝石
    uint32 Flag;                             //是否碎裂
};
typedef std::unordered_map<uint32, UpChoiceTemp> ItemUpChoiceTempMap;
extern ItemUpChoiceTempMap ItemUpChoiceTempStore;

typedef std::unordered_map<uint32, uint32> SoulItemConfigMap;
extern SoulItemConfigMap SoulItemConfigStore;

struct SoulItemStruct
{
    Item* DoItem;
    uint32 ItemEntry;
    uint32 OwnerGuidLow;
    uint32 Slot;
};
typedef std::unordered_map<uint32, SoulItemStruct> SoulItemMap;
extern SoulItemMap SoulItemStore;

struct SummonStruct
{
    uint32 ItemId;
    uint32 NTempId;
    int32 SEntry;
    std::string Text;
    uint32 DoTime;
    uint32 OnDel;
};
typedef std::unordered_map<uint32, SummonStruct> SummonStructMap;
extern SummonStructMap SummonStructStore;

struct ItemVsInfo
{
    int32 ItemId;
    uint32 VendorId;
    uint32 NTempId;
    uint32 RTempId;
};
typedef std::vector<ItemVsInfo> ItemVsInfoVect;
extern ItemVsInfoVect ItemVsInfoStore;

struct ItemEnchTemp
{
    float EnchSlot1;
    uint32 EnchGroup1;
    float EnchSlot2;
    uint32 EnchGroup2;
    float EnchSlot3;
    uint32 EnchGroup3;
    float EnchSlot4;
    uint32 EnchGroup4;
    float EnchSlot5;
    uint32 EnchGroup5;
    int32 Rts;                              //洗练次数
    uint32 NTempId;                              //需求
};
typedef std::map<uint32, ItemEnchTemp> ItemEnchMap;
extern ItemEnchMap ItemEnchStore;

struct EnchGroupTemp
{
    uint32 EnchGroup;
    uint32 EnchId;
    float Chance;
};

typedef std::vector<EnchGroupTemp> EnchGroupList;
typedef std::unordered_map<uint32, EnchGroupList> EnchGroupMap;
extern EnchGroupMap EnchGroupStore;

struct EnchResetAtItemSt
{
    uint32 UseItem;
    uint32 TarItem;
    float EnchSlot1;
    uint32 EnchGroup1;
    float EnchSlot2;
    uint32 EnchGroup2;
    float EnchSlot3;
    uint32 EnchGroup3;
    float EnchSlot4;
    uint32 EnchGroup4;
    float EnchSlot5;
    uint32 EnchGroup5;
};

typedef std::vector<EnchResetAtItemSt> EnchResetAtItemStVect;
extern EnchResetAtItemStVect EnchResetAtItemStore;

enum SoulItemSlots : uint8
{
    SOUL_SLOT_START = 0,
    SOUL_SLOT_HEAD = 0,
    SOUL_SLOT_NECK = 1,
    SOUL_SLOT_SHOULDERS = 2,
    SOUL_SLOT_BODY = 3,
    SOUL_SLOT_CHEST = 4,
    SOUL_SLOT_WAIST = 5,
    SOUL_SLOT_LEGS = 6,
    SOUL_SLOT_FEET = 7,
    SOUL_SLOT_WRISTS = 8,
    SOUL_SLOT_HANDS = 9,
    SOUL_SLOT_FINGER1 = 10,
    SOUL_SLOT_FINGER2 = 11,
    SOUL_SLOT_TRINKET1 = 12,
    SOUL_SLOT_TRINKET2 = 13,
    SOUL_SLOT_BACK = 14,
    SOUL_SLOT_MAINHAND = 15,
    SOUL_SLOT_OFFHAND = 16,
    SOUL_SLOT_END = 17,
    SOUL_SLOT_MAX = 18,
};

typedef std::map<std::string, uint32> CdkeyMap;
extern CdkeyMap CdkeyStore;

#define MAX_GUID_INDEX 100000000

struct EnchantTemp
{
    uint32 EnchantId1;
    uint32 EnchantId2;
    uint32 EnchantId3;
    uint32 EnchantId4;
    uint32 EnchantId5;
};

typedef std::unordered_map<uint32, EnchantTemp> EnchantAtMap;
extern EnchantAtMap EnchantAtStore;

struct ItemRyTemp
{
    uint32 RwItemId1;
    uint32 RwCount1;
    uint32 RwItemId2;
    uint32 RwCount2;
    uint32 RwItemId3;
    uint32 RwCount3;
    uint32 RwItemId4;
    uint32 RwCount4;
    uint32 RwItemId5;
    uint32 RwCount5;
    uint32 RwToken;
    uint32 Menu;
};

typedef std::map<uint32, ItemRyTemp> ItemRyMap;
extern ItemRyMap ItemRyStore;

typedef std::map<uint32, std::string> ItemRyGroupMap;
extern ItemRyGroupMap ItemRyGroupStore;

typedef std::map<uint32, uint32> ItemClearBindMap;
extern ItemClearBindMap ItemClearBindStore;

struct AtLimit
{
    uint32 CharDayLimt;    //角色每日
    uint32 CharFrLimt;       //角色永久
    uint32 AccDayLimt;     //账号每日
    uint32 AccFrLimt;        //账号永久
};

typedef std::unordered_map<uint32, AtLimit> AtAddItemLimitMap;
extern AtAddItemLimitMap AtAddItemLimitStore;

struct UseItemLimit
{
    uint32 UseDayLimt;    //角色每日使用
    uint32 UseFrLimt;       //角色永久使用
};

typedef std::unordered_map<uint32, UseItemLimit> UseItemLimitMap;
extern UseItemLimitMap UseItemLimitStore;

typedef std::map<uint32, uint32> ItemEquipMap;
extern ItemEquipMap ItemEquipStore;

typedef std::map<uint32, uint32> IDTransmogMap;
extern IDTransmogMap IDTransmogStore;

typedef std::map<uint32, std::string> ItemSubNameMap;
extern ItemSubNameMap ItemSubNameStore;

#define AuraIndex8 8    //组合 8
#define AuraIndex6 6    //组合 6
#define AuraIndex4 4    //组合 4
#define AuraIndex2 2    //组合 2

struct ItemCardsTemp
{
    uint32 Group1;
    uint32 Group2;
    uint32 Group3;
    uint32 NTempId;
    bool Bind;
};
typedef std::map<uint32, ItemCardsTemp> ItemCardsMap;
extern ItemCardsMap ItemCardsStore;

struct CardsGpTemp
{
    uint32 Aura2;
    uint32 Aura4;
    uint32 Aura6;
    uint32 Aura8;
};
typedef std::map<uint32, CardsGpTemp> CardsGpMap;
extern CardsGpMap CardsGpStore;

typedef std::map<uint32, uint32> ItemScriptOnDeleteMap;
extern ItemScriptOnDeleteMap ItemScriptOnDeleteStore;

typedef std::map<uint32, uint32> ItemEnchTaskMap;
extern ItemEnchTaskMap ItemEnchTaskStore;

struct NewFmZyTemp
{
    uint32 ItemId;
    uint32 NTempId;
    uint32 Slot;
    uint32 TarItemId;
};
typedef std::vector<NewFmZyTemp> NewFmZyVect;
extern NewFmZyVect NewFmZyStore;

typedef std::map<uint32, std::string> ItemEnchAtCountMap;
extern ItemEnchAtCountMap ItemEnchAtCountStore;

typedef std::set<uint32> ItemNotTaskMap;
extern ItemNotTaskMap ItemNotTaskStore;

typedef std::unordered_map<uint32, uint32> EnchLimitMap;
extern EnchLimitMap EnchLimitStore;

class ItemFuncMgr
{
public:
    static ItemFuncMgr* Instance()
    {
        static ItemFuncMgr Instance;
        return &Instance;
    }

    ItemFuncMgr();
    ~ItemFuncMgr();

    void LoadItemFunc();
    void LoadSoulItemStore();

    bool IsFunctionItem(std::string ScriptName);
    bool IsItemInStat(uint32 ItemId);

    std::pair<uint32, uint32> PullItemAndCount();
    std::pair<uint32, uint32> PullTenItemAndCount();
    bool IsLuckAnn(uint32 ItemId);
    uint32 MaxLuck();
    void DoNewLuckDataTo(Player* player);
    std::string GetCommands(uint32 ItemId);

    void DoOnLoginItemInStat(Player* player);

    bool IsUpItem(uint32 ItemId);
    bool IsInItemUpRandom(Item* Tar);
    bool IsInItemUpChoice(Item* Tar);
    bool IsInItemTaskSock(Item* Tar);

    bool IsSoulItemConfigStore(Player* player, uint32 ItemId);
    uint32 GetSoulNTempId(uint32 ItemId);
    bool IsSoulItem(uint32 ItemGuidLow);
    Item* GetEquipSoulItem(uint32 ItemGuidLow);
    void ApplySoulItemData(uint32 ItemGuidLow, Item* DoItem, uint32 ItemEntry, uint32 OwnerLow, uint8 Slot);
    void RemoveSoulItemData(uint32 ItemGuidLow);
    void SaveEquipSoulItemToDB(CharacterDatabaseTransaction& Trans, uint32 OwnerLow, uint32 ItemEntry, uint32 ItemGuidLow, uint8 Slot);
    void RemoveSoulItemDB(uint32 ItemGuidLow);
    uint32 GetSoulItemEntry(uint32 OwnerLow, uint8 Slot);
    uint32 GetSoulItemGuid(uint32 OwnerLow, uint8 Slot);
    std::pair<AsInt64, AsInt64> GetSoulWeaponDamageRange(Player* player);
    void RefreshSoulItemStats(Player* player);
    void SendClientSoulData(Player* player);
    void SendClientSoulOneData(Player* player, Item* DoItem, uint8 Slot);
    void SendClientSoulTaskOneData(Player* player, uint8 Slot);

    std::string GetSoulSlotName(uint8 Slot);
    std::string GetSoulSlotIcon(uint8 Slot, uint32 W, uint32 H, int X, int Y);
    uint8 GetOriginalSlot(uint8 CustomSlot);
    bool CanEquipSoul(Player* player, Item* NewItem, uint8 Slot);
    bool SuitableForEquipSou(Player* player, ItemTemplate const* Proto);
    void ApplySoulItemStatsAndSpells(Player* player, Item* DoItem, uint8 Slot, bool Apply);
    void UpdateSoulItem(Player* player, Item* NewItem, uint8 Slot);
    void AutoSoulItemStatsAndSpells(Player* player);
    float GetSoulItemPtData(Player* player, uint32 ItemId);
    bool HasItemTotemCategory(Player* player, ItemTemplate const* NewTemplate);

    void SendItemVsAccept(Player* player, std::string Text, uint32 SendId = 0);
    bool IsItemVsInfo(int32 ItemId);
    uint32 GetNTempId(int32 ItemId, uint32 VendorId);
    uint32 GetRTempId(int32 ItemId, uint32 VEntry, bool Sort = false);
    bool DoItemVsAccept(Player* player, uint32 SendId);
    bool DoTheSellItem(Player* player, uint32 VEntry, int32 ItemId, uint32 Count);

    uint32 GetCustomEnchNTempId(int32 ItemId);
    bool DoUseCustomEnch(Item* NewItem);
    void DoUseCustomEnchOne(Item* NewItem, EnchantmentSlot EnchSlot);
    void DoRandomEnch(Item* NewItem, uint32 Group, EnchantmentSlot EnchSlot);
    void DoRandomEnchSwitchOne(Player* player, Item* NewItem, Item* OldItem, EnchantmentSlot EnchSlot);
    void SendUpdateShowEnchInfo1(Player* player, Item* OnItem);
    void SendUpdateShowEnchInfo2(Player* player, Item* OnItem);
    void DoUseCustomEnchOneNoPacket(Item* NewItem, EnchantmentSlot EnchSlot);

    std::string GetDesForEnchant(uint32 EnchantId);

    void DeleteItemFuncFromDB(uint32 ItemGuldLow, CharacterDatabaseTransaction* Trans);

    bool IsNotFuncRecovery();
    bool IsItemRecovery(uint32 ItemId, uint32 Menu);
    void DoItemRecovery(Player* player, uint32 Menu);
    bool IsHasRecovery(Player* player, uint32 Menu);

    void ResetLimitItem();

    uint32 GetEquipTempId(uint32 ItemId);

    uint32 GetLimitNowCount(Player* player, uint32 ItemId);
    bool CheckBuyItemLimit(Player* player, uint32 BuyItemId, uint32 BuyCount);

    bool IsInAtEnchItem(uint32 UseItem, uint32 TarItem);
    bool DoAtEnchItem(Player* player, Item* TargetItem, uint32 UseItem);

    bool CanDoInItemCards(Player* player, uint32 ItemId);
    void HandlersItemCardsAuraSpellOnPlayer(Player* player, uint32 Num);
    void OnLoginCastCardsAuraSpell(Player* player);
    void OnClearItemCardsAuraSpell(Player* player);
    void SendUpdateCardsAuraSpellData(Player* player);

    void SendUpdateEnchAtCount(Player* player, uint32 GuidLow, uint32 RtsCount, int32 AwRts);
    void DoCreateRts(Item* DoItem);
    void LoadEnchRtsDB(Item* DoItem);
    int32 GetAwEnchRts(uint32 ItemId);
    void UpdateEnchRtsDB(Player* player, Item* DoItem);

    bool OnItemUseNow(Player* player, Item* PItem);

    bool InItemEnchTaskStoreLeft(uint32 ItemId);
    bool InKtItemEnchTaskStore(uint32 LeftItem, uint32 RightItem);
    bool InSlotItemEnchTaskStore(uint32 LeftItem, uint32 RightItem, uint32 AtSlot);
    uint32 GetTaskNTempId(uint32 LeftItem, uint32 RightItem, uint32 AtSlot);
    void NewSendUpdateShowEnchInfo1(Player* player, Item* OnItem);
    void  NewSendUpdateShowEnchInfo2(Player* player, Item* OnItem);

    bool LimitHookResItemUseNow(Player* player, Item* PItem);
    void ReseDayUsetLimitItem();

    bool InItemNotTask(uint32 ItemId);

    uint32 GetEnchMaxCount(uint32 EnchantId);
    bool InEnchMaxCount(Player* player, bool Apply, uint32 EnchantId);

private:

};
#define sItemFuncMgr ItemFuncMgr::Instance()

#endif //!
