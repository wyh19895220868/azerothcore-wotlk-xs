
#ifndef _ITEMMAGICMGR_H
#define _ITEMMAGICMGR_H

enum MagicTypes
{
    M_NULL,
    M_1,                 //金
    M_2,                 //木
    M_3,                 //水
    M_4,                 //火
    M_5,                 //土
    M_6,                 //天
    M_7,                 //地
    M_8,                 //玄
};

struct ItemMagicStruct
{
    MagicTypes OType;                       //法宝类型
    uint32 SpellId;                       //技能编号
    bool IsSpell;                      //是否主动被动
    bool ToBind;                             //是否绑定
    uint32 MaxCount;                         //全局限制数量
};
typedef std::unordered_map<uint32, ItemMagicStruct> ItemMagicMap;
extern ItemMagicMap ItemMagicStore;

struct TypeMagicTemp
{
    uint32 FAuraId;                       //类型数量激活的光环
    bool IsSpell;                      //是否主动被动
};
typedef std::unordered_map<MagicTypes, TypeMagicTemp> TypeMagicMap;
extern TypeMagicMap TypeMagicStore;

struct ZodiacTemp
{
    uint32 ItemAura;                       //自带光环
    uint32 Slot;                       //归属位置
    uint32 InTempId;                       //放入需求
    uint32 OutTempId;                       //取出需求
};

typedef std::map<uint32, ZodiacTemp> ZodiacMap;
extern ZodiacMap ZodiacStore;

struct ZodiacCenterTemp
{
    uint32 Item1;
    uint32 Item2;
    uint32 Item3;
    uint32 Item4;
    uint32 Item5;
    uint32 Item6;
    uint32 Item7;
    uint32 Item8;
    uint32 Item9;
    uint32 Item10;
    uint32 Item11;
    uint32 Item12;
    uint32 CenterAuraId;
};

typedef std::map<uint32, ZodiacCenterTemp> ZodiacCenterMap;
extern ZodiacCenterMap ZodiacCenterStore;

class ItemMagicMgr
{
public:
    static ItemMagicMgr* Instance()
    {
        static ItemMagicMgr Instance;
        return &Instance;
    }




    void LoadItemMagicData();
    bool GetToBind(uint32 ItemId);
    uint32 GetMagicAuraId(uint32 ItemId);
    std::string GetTStr(uint32 ItemId);
    uint32 GetMagicItemMax(uint32 ItemId);
    MagicTypes GetOType(uint32 ItemId);
    MagicTypes AutoForClientGroupId(uint32 GroupId, uint32 Num);
    void UpdateMagicOnPlayerHandle(Player* player, MagicTypes Typ, uint32 AtGroup, uint32 ItemId, uint32 Num, bool Add);
    bool IsSpellForItem(uint32 ItemId);
    uint32 GetMagicFitAuraId(MagicTypes OType);
    bool IsSpellForOType(MagicTypes OType);
    void RepOrDeleteItemMagic(uint32 PlayerGuidLow, uint32 ItemIdOrAuraId, uint32 Num, uint32 AtGroup, MagicTypes OType, bool Ins);

    uint32 GetPlayerActCountForOType(Player* player, uint32 ItemId);
    void PlayerLoginLoad(Player* player);

    void LoadItemZodiacData();
    bool InZodiacStoreData(uint32 ItemId);
    uint32 GetZodiacAuraId(uint32 ItemId);
    uint32 GetZodiacSlot(uint32 ItemId);
    uint32 GetZodiacItemId(uint32 AuraId);
    uint32 GetZodiacTempId(uint32 ItemId, bool In);
    void CheckUpdateCenterAsZodiacPlayer(Player* player, bool In);
    void PlayerLoginLoadZodiac(Player* player);

private:

};
#define sItemMagicMgr ItemMagicMgr::Instance()

#endif
