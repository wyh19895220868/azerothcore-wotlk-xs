
#ifndef _NOTICEMGR_H
#define _NOTICEMGR_H

typedef std::unordered_map<uint32, std::string> KillCreNoticeMap;
extern KillCreNoticeMap KillCreNoticeStore;

typedef std::unordered_map<int32, std::string> KillPlayerNoticeMap;
extern KillPlayerNoticeMap KillPlayerNoticeStore;

typedef std::unordered_map<uint32, std::string> LootItemNoticeMap;
extern LootItemNoticeMap LootItemNoticeStore;

typedef std::unordered_map<uint32, std::string> ItemNewNoticeMap;
extern ItemNewNoticeMap ItemNewNoticeStore;

class NoticeMgr
{
public:
    static NoticeMgr* Instance()
    {
        static NoticeMgr Instance;
        return &Instance;
    }

    void LoadNoticeMgr();
    void OnKillCreNotice(Player* player, Creature* Killed);
    void OnKillPlayerNotice(Player* Killer, Player* Killed);
    void OnLootItemNotice(Player* player, Item* OnItem, uint32 Count, ObjectGuid LootGuid);
    void OnItemNewNotice(Player* player, Item* OnItem, uint32 Count);

private:

};
#define sNoticeMgr NoticeMgr::Instance()

#endif
