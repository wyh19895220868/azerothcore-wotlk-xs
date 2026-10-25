
#ifndef _DBCMGR_H
#define _DBCMGR_H

#include "Define.h"

#include <string>
#include <vector>

class DBCMgr
{
public:
    static DBCMgr* instance()
    {
        static DBCMgr instance;
        return &instance;
    }

    void OutDBC(std::string file, std::vector<std::vector<int32>> dbcData);
    void GenerateItemDBC();

private:

};
#define sDBCMgr DBCMgr::instance()

#endif //!
