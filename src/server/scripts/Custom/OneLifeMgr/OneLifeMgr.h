
#ifndef _ONELIFEMGR_H
#define _ONELIFEMGR_H

class OneLifeMgr
{
public:
    static OneLifeMgr* Instance()
    {
        static OneLifeMgr Instance;
        return &Instance;
    }

    void OnPlayerDieNow(Player* player);

private:

};
#define sOneLifeMgr OneLifeMgr::Instance()

#endif
