
#ifndef _ITEMSHOPMGR_H
#define _ITEMSHOPMGR_H

struct ItemShopTemp
{
    uint32 MenuTab;
    uint32 NTempId;
};

typedef std::map<uint32, ItemShopTemp> ItemShopMap;
extern ItemShopMap ItemShopStore;

#define MAX_LIST_COUNT 9        //每一页的数量

class ItemShopMgr
{
public:
    static ItemShopMgr* Instance()
    {
        static ItemShopMgr Instance;
        return &Instance;
    }

    void LoadItemShop();

    void SendTabNames(Player* player);
    uint32 GetCountForTab(uint32 Tab);
    uint32 GetPage(uint32 ItemListSize);
    void SendDataForPage(Player* player, uint32 Tab, uint32 Page);
    void SendDataForSearch(Player* player, std::string IdOrName);
    uint32 GetNTempId(uint32 ItemId);
    void SendNTempGtsForPage(Player* player, uint32 ItemId);
    bool IsBuyItem(Player* player, uint32 ItemId);

private:

};
#define sItemShopMgr ItemShopMgr::Instance()

#endif
