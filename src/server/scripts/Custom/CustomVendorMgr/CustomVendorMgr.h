
#ifndef _CUSTOMVENDORMGR_H
#define _CUSTOMVENDORMGR_H

#include "CreatureData.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Log.h"
#include "ObjectMgr.h"
#include <ctime>

struct CustomVendorItem
{
    uint32 entry;
    uint32 slot;
    uint32 item;
    int32 maxcount;
    uint32 incrtime;
    uint32 ExtendedCost;
    uint32 NTempId;
    bool CanBuyBack;
};

struct CustomVendorStock
{
    explicit CustomVendorStock(uint32 _vendorEntry, uint32 _item, uint32 _count)
        : vendorEntry(_vendorEntry), itemId(_item), count(_count), lastIncrementTime(time(nullptr)) {}

    uint32 vendorEntry;
    uint32 itemId;
    uint32 count;
    time_t lastIncrementTime;
};

typedef std::vector<CustomVendorItem> CustomVendorItemVect;
typedef std::unordered_map<uint32, CustomVendorItemVect> CustomVendorItemMap;
typedef std::list<CustomVendorStock> CustomVendorStockList;
extern CustomVendorItemMap CustomVendorItemStore;

class CustomVendorMgr
{
public:
    static CustomVendorMgr* Instance()
    {
        static CustomVendorMgr Instance;
        return &Instance;
    }

    void LoadCustomVendorStore()
    {
        CustomVendorItemStore.clear();
        m_vendorStocks.clear();

        QueryResult Result = WorldDatabase.Query("SELECT 生物编号, 排序位置, 物品编号, 最大库存数量, 刷新时间, 扩展消耗编号, 需求模版, 是否回购 FROM _生物自定义售卖表 ORDER BY 生物编号, 排序位置 ASC, 物品编号, 扩展消耗编号;");
        if (!Result)
        {
            return;
        }

        do
        {
            Field* Fields = Result->Fetch();
            CustomVendorItem St;
            St.entry = Fields[0].Get<uint32>();
            St.slot = Fields[1].Get<uint32>();
            St.item = Fields[2].Get<uint32>();
            St.maxcount = Fields[3].Get<int32>();
            St.incrtime = Fields[4].Get<uint32>();
            St.ExtendedCost = Fields[5].Get<uint32>();
            St.NTempId = Fields[6].Get<uint32>();
            St.CanBuyBack = Fields[7].Get<bool>();

            if (!sObjectMgr->GetItemTemplate(St.item))
            {
                continue;
            }

            if (St.ExtendedCost && !sItemExtendedCostStore.LookupEntry(St.ExtendedCost))
            {
                continue;
            }

            if (St.maxcount > 0 && St.incrtime == 0)
            {
                continue;
            }
            else if (St.maxcount == 0 && St.incrtime > 0)
            {
                continue;
            }

            CustomVendorItemVect const* vItems = GetCustomVendorItemList(St.entry);
            if (vItems)
            {
                bool duplicateFound = false;
                for (auto const& itr : *vItems)
                {
                    if (itr.item == St.item && itr.ExtendedCost == St.ExtendedCost)
                    {
                        duplicateFound = true;
                        break;
                    }
                }
                if (duplicateFound)
                    continue;
            }

            CustomVendorItemStore[St.entry].push_back(St);

        } while (Result->NextRow());
    }

    CustomVendorItemVect const* GetCustomVendorItemList(uint32 entry) const
    {
        CustomVendorItemMap::const_iterator iter = CustomVendorItemStore.find(entry);
        if (iter == CustomVendorItemStore.end())
            return nullptr;

        return &iter->second;
    }

    bool IsCustomVendorItem(uint32 entry, uint32 item) const
    {
        CustomVendorItemVect const* items = GetCustomVendorItemList(entry);
        if (!items)
            return false;

        for (auto const& itr : *items)
            if (itr.item == item)
                return true;

        return false;
    }

    CustomVendorItem const* GetCustomVendorItem(uint32 entry, uint32 item) const
    {
        CustomVendorItemVect const* items = GetCustomVendorItemList(entry);
        if (!items)
            return nullptr;

        for (auto const& itr : *items)
            if (itr.item == item)
                return &itr;

        return nullptr;
    }

    uint32 GetCustomVendorNTempId(uint32 entry, uint32 item) const
    {
        CustomVendorItemVect const* items = GetCustomVendorItemList(entry);
        if (!items)
            return 0;

        for (auto const& itr : *items)
            if (itr.item == item)
                return itr.NTempId;

        return 0;
    }

    bool IsCustomVendorCanBuyBack(uint32 entry, uint32 item) const
    {
        CustomVendorItemVect const* items = GetCustomVendorItemList(entry);
        if (!items)
            return false;

        for (auto const& itr : *items)
            if (itr.item == item)
                return itr.CanBuyBack;

        return false;
    }

    uint32 GetCustomVendorCurrentCount(uint32 vendorEntry, uint32 itemId, int32 maxcount, uint32 incrtime)
    {
        if (maxcount <= 0)
            return 0xFFFFFFFF;

        CustomVendorStockList::iterator itr = m_vendorStocks.begin();
        for (; itr != m_vendorStocks.end(); ++itr)
            if (itr->vendorEntry == vendorEntry && itr->itemId == itemId)
                break;

        if (itr == m_vendorStocks.end())
            return uint32(maxcount);

        CustomVendorStock* vCount = &*itr;

        time_t ptime = time(nullptr);

        if (incrtime > 0 && time_t(vCount->lastIncrementTime + incrtime) <= ptime)
        {
            ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(itemId);
            uint32 buyCount = pProto ? pProto->BuyCount : 1;

            uint32 diff = uint32((ptime - vCount->lastIncrementTime) / incrtime);
            if ((vCount->count + diff * buyCount) >= uint32(maxcount))
            {
                m_vendorStocks.erase(itr);
                return uint32(maxcount);
            }

            vCount->count += diff * buyCount;
            vCount->lastIncrementTime = ptime;
        }

        return vCount->count;
    }

    uint32 UpdateCustomVendorCurrentCount(uint32 vendorEntry, uint32 itemId, int32 maxcount, uint32 incrtime, uint32 used_count)
    {
        if (maxcount <= 0)
            return 0;

        CustomVendorStockList::iterator itr = m_vendorStocks.begin();
        for (; itr != m_vendorStocks.end(); ++itr)
            if (itr->vendorEntry == vendorEntry && itr->itemId == itemId)
                break;

        if (itr == m_vendorStocks.end())
        {
            uint32 new_count = uint32(maxcount) > used_count ? uint32(maxcount) - used_count : 0;
            m_vendorStocks.push_back(CustomVendorStock(vendorEntry, itemId, new_count));
            return new_count;
        }

        CustomVendorStock* vCount = &*itr;

        time_t ptime = time(nullptr);

        if (incrtime > 0 && time_t(vCount->lastIncrementTime + incrtime) <= ptime)
        {
            ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(itemId);
            uint32 buyCount = pProto ? pProto->BuyCount : 1;

            uint32 diff = uint32((ptime - vCount->lastIncrementTime) / incrtime);
            if ((vCount->count + diff * buyCount) < uint32(maxcount))
                vCount->count += diff * buyCount;
            else
                vCount->count = uint32(maxcount);
            vCount->lastIncrementTime = ptime;
        }

        if (vCount->count > used_count)
            vCount->count -= used_count;
        else
            vCount->count = 0;

        return vCount->count;
    }

private:
    CustomVendorStockList m_vendorStocks;
};

#define sCustomVendorMgr CustomVendorMgr::Instance()

#endif //!
