#include "ItemSkillsLoader.h"
#include "ItemSkillsManager.h"
#include "ItemSkillsEffects.h"
#include "ItemSkillsEvents.h"
#include "ItemSkillsDisplay.h"
#include "Config.h"
#include "ScriptMgr.h"
#include "Logging/Log.h"
#include "World.h"
#include "Configuration/Config.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include "ModuleFormat.h"

// 模块加载函数声明
void AddItemSkillsCommands();

// 世界脚本类，使用WorldScript的OnUpdate钩子函数实现延迟加载
class ItemSkillsModuleLoader : public WorldScript
{
public:
    ItemSkillsModuleLoader() : WorldScript("ItemSkillsModuleLoader")
    {
        _loadTimer = 0;
        _isLoaded = false;
        _loadDelay = 1 * IN_MILLISECONDS; // 1秒延迟

        // 不显示准备中的日志
    }

    void OnUpdate(uint32 diff) override
    {
        if (_isLoaded)
            return;

        _loadTimer += diff;
        if (_loadTimer >= _loadDelay)
        {
            // 加载模块
            LoadModule();
            _isLoaded = true;
        }
    }

private:
    uint32 _loadTimer;
    uint32 _loadDelay;
    bool _isLoaded;

    // 不再需要全局修改ItemTemplate
    // 改为在玩家添加技能时，动态发送该物品实例的SMSG_ITEM_QUERY_SINGLE_RESPONSE包
    // 这样每个物品GUID有独立的技能，不会影响所有相同itemId的物品

    // 加载模块
    uint32 LoadModule()
    {
        // 检查模块是否启用
        bool moduleEnabled = sConfigMgr->GetOption<bool>("ItemSkills.Enable", true) && sHelper->GetBoolean(22011);
        if (!moduleEnabled)
        {
            LOG_INFO("server.loading", "→物品技能系统已禁用");
            return 0;
        }

        // 初始化技能管理器
        sItemSkillsManager->Initialize();

        // 初始化技能效果处理器
        sItemSkillsEffects->Initialize();

        // 初始化技能显示系统
        sItemSkillsDisplay->Initialize();

        // 事件处理器已在AddItemSkillsScripts()中注册，不需要在这里重复注册

        // 获取加载的技能模板数量
        uint32 count = 0;
        try
        {
            QueryResult result = WorldDatabase.Query("SELECT COUNT(*) FROM `_模板_物品技能`");
            if (result)
            {
                Field* fields = result->Fetch();
                count = fields[0].Get<uint32>();
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("server.loading", ModuleOldFormat("查询技能模板数据时发生错误: %s", e.what()).c_str());
        }

        LOG_INFO("server.loading", "→_模板_物品技能 加载完成√");

        return count;
    }
};

// 模块加载函数
void AddItemSkillsScripts()
{
    // 只注册命令和ModuleLoader，其他功能在服务器完全启动后加载
    AddItemSkillsCommands();

    // 注册ModuleLoader
    new ItemSkillsModuleLoader();

    // 注册物品使用脚本（拦截OnUse事件）
    new ItemSkillsItemScript();

    // 注册玩家事件脚本
    new ItemSkillsEvents();

    // 注册单位事件脚本（击中触发）
    new ItemSkillsUnitEvents();
}
