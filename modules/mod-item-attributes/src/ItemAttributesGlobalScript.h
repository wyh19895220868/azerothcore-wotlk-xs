#ifndef ITEM_ATTRIBUTES_GLOBAL_SCRIPT_H
#define ITEM_ATTRIBUTES_GLOBAL_SCRIPT_H

#include "ScriptMgr.h"
#include "DatabaseEnv.h"

// 物品属性全局脚本 - 处理物品删除事件
class ItemAttributesGlobalScript : public GlobalScript
{
public:
    ItemAttributesGlobalScript();

    // 当物品从数据库删除时触发
    void OnItemDelFromDB(CharacterDatabaseTransaction trans, ObjectGuid::LowType itemGuid) override;
};

#endif
