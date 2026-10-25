/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "ItemEnhancementMgr.h"
#include "ModuleFormat.h"

class ItemEnhancementPlayerScript : public PlayerScript
{
public:
    ItemEnhancementPlayerScript() : PlayerScript("ItemEnhancementPlayerScript") { }

    // 当角色被删除时调用
    void OnDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        // 删除该角色的所有强化记录
        if (sItemEnhancementMgr)
        {
            sItemEnhancementMgr->DeleteAllEnhancementRecordsByPlayer(guid.GetCounter());
        }
        else
        {
            LOG_ERROR("server.loading", "ItemEnhancementPlayerScript: sItemEnhancementMgr 为空！");
        }
    }

    // 当角色从数据库删除时调用
    void OnDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
    {
        // 【安全修复】使用传入的事务对象执行删除，保证事务一致性
        // 如果外部事务回滚，强化记录删除也会回滚
        if (trans)
        {
            std::string sql = "DELETE FROM 物品强化_记录 WHERE owner_guid = " + std::to_string(guid);
            trans->Append(sql);
        }
        else
        {
            LOG_ERROR("server.loading", ModuleOldFormat("强化系统: 事务对象为空，跳过删除玩家 %s 的强化记录", guid).c_str());
        }

        // 清理内存缓存
        if (sItemEnhancementMgr)
        {
            sItemEnhancementMgr->ClearPlayerRecordCache(guid);
        }
    }
};

// 注册脚本
void AddSC_ItemEnhancementPlayerScript()
{
     // LOG_INFO("server.loading", "ItemEnhancementPlayerScript: 正在注册强化系统角色删除脚本");
    new ItemEnhancementPlayerScript();
     // LOG_INFO("server.loading", "ItemEnhancementPlayerScript: 强化系统角色删除脚本注册完成");
}
