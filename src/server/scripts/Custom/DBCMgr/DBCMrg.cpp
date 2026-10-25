#pragma execution_character_set("utf-8")
#include "DBCMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include <fstream>
#include <iostream>
#include <Windows.h>

void DBCMgr::OutDBC(std::string file, std::vector<std::vector<int32>> dbcData)
{
    LOG_INFO("server.worldserver", ">>{} - 正在写入数据目录...", file.c_str());

    std::unordered_map<std::string, uint32> stringMap;

    int32 wdbc = 0x43424457;
    int32 recordCount = dbcData.size();
    int32 fieldCount = 8;
    int32 recordSize = fieldCount * 4;

    std::ofstream f(file.c_str(), std::ios::binary);

    f.write((char*)&wdbc, sizeof(int32));
    f.write((char*)&recordCount, sizeof(int32));
    f.write((char*)&fieldCount, sizeof(int32));
    f.write((char*)&recordSize, sizeof(int32));


    int32 size = 0;
    f.write((char*)&size, sizeof(int32));

    for (int32 i = 0; i < recordCount; i++)
    {
        std::vector<int32> dataList = dbcData.at(i);

        for (int32 j = 0; j < fieldCount; j++)
        {
            int32 value = dataList[j];
            f.write((char*)&value, sizeof(int32));
        }

        uint32 x = (i + 1) * 100 / recordCount;
        uint32 y = (i + 2) * 100 / recordCount;

        if (x != 0 && (x != y || x == 100))
            LOG_INFO("server.worldserver", ">>{} - 写入数据 {}% ", file.c_str(), x);
    }

    f.close();
}

void DBCMgr::GenerateItemDBC()
{
    std::vector<std::vector<int32>> dbcData;
    std::string file = "Item.dbc";
    uint32 index = 0;

    if (QueryResult result = WorldDatabase.Query("SELECT entry, class, subclass, SoundOverrideSubclass, Material, displayid, InventoryType, sheath FROM item_template ORDER BY entry ASC;"))
    {
        do
        {
            std::vector<int32> v;
            Field* fields = result->Fetch();

            for (uint8 i = 0; i < 8; i++)
                v.push_back(fields[i].Get<int32>());

            dbcData.push_back(v);

            index++;

            if (index % 100 == 0)
                LOG_INFO("server.worldserver", ">>{} - 读入数据 {}%", file.c_str(), index);

        } while (result->NextRow());
    }

    OutDBC(file, dbcData);
}
