
#ifndef _MULTINPCMGR_H
#define _MULTINPCMGR_H

struct VendorStruct
{
    uint32 NpcEntry;
    uint32 VTEntry;
    uint32 Num;
    std::string Gossip;
    uint32 Tean;
    uint32 Class;
    uint32 ShowNeedTempId;
};
typedef std::multimap<uint32, VendorStruct> VendorStructMap;
extern VendorStructMap VendorStructStore;

struct VendorNum
{
    uint32 VTEntry;
    uint32 Num;
    std::string Gossip;
};

class MultiNpcMgr
{
public:
    static MultiNpcMgr* Instance()
    {
        static MultiNpcMgr Instance;
        return &Instance;
    }

    void LoadMultiVendorStore();

private:

};
#define sMultiNpcMgr MultiNpcMgr::Instance()

#endif
