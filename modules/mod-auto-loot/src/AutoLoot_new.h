#ifndef MODULE_AUTO_LOOT_H
#define MODULE_AUTO_LOOT_H

#include "ScriptMgr.h"
#include "Player.h"
#include "Creature.h"
#include "Group.h"
#include "LootMgr.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Config.h"
#include "World.h"
#include <vector>

class AutoLootPlayerScript : public PlayerScript
{
public:
    AutoLootPlayerScript();

    // 当玩家击杀生物时触发
    void OnCreatureKill(Player* player, Creature* killed) override;

    // 处理战利品分配
    void HandleLoot(Player* player, Creature* creature);

private:
    // 处理金币拾取
    void HandleMoneyLoot(Player* player, Creature* creature);

    // 处理物品拾取
    void HandleItemLoot(Player* player, Creature* creature);

    // 处理团队/小队战利品分配
    void HandleGroupLoot(Player* player, Creature* creature);

    // 检查是否启用调试模式
    bool IsDebugEnabled() const;
};

// 自动拾取模块加载器，使用WorldScript的OnUpdate钩子实现延迟加载
class AutoLootModuleLoader : public WorldScript
{
public:
    AutoLootModuleLoader();

    // 配置加载后触发
    void OnAfterConfigLoad(bool reload) override;

    // 服务器更新时触发
    void OnUpdate(uint32 diff) override;

private:
    bool _loaded;        // 是否已加载
    uint32 _startTime;   // 开始时间
};

#endif // MODULE_AUTO_LOOT_H
