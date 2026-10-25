#include "DatabaseEnv.h"
#include "Log.h"
#include "World.h"
#include "ItemEnhancement.h"
#include "ItemEnhancementMgr.h"
#include "ScriptMgr.h"
#include "GameTime.h"
#include "ModuleFormat.h"

// 世界脚本类，使用WorldScript的OnUpdate钩子函数实现延迟加载
class ItemEnhancementLoader : public WorldScript
{
public:
    ItemEnhancementLoader() :
        WorldScript("ItemEnhancementLoader"),
        _initialized(false),
        _loadTimer(0),
        _delayTime(1 * IN_MILLISECONDS) // 1秒延迟
    {
        // 加载配置
        LoadItemEnhancementConfig();
    }

    void OnUpdate(uint32 diff) override
    {
        // 如果已经初始化，不再执行
        if (_initialized)
            return;

        // 累加时间
        _loadTimer += diff;

        // 当达到延迟时间时，加载模块
        if (_loadTimer >= _delayTime)
        {
            uint32 count = 0;

            // 从数据库加载数据
            try
            {
                // 初始化并加载ItemEnhancementMgr的数据
                sItemEnhancementMgr->LoadConfig();
                sItemEnhancementMgr->LoadEnhancementTemplates();

                // 注释掉重复的加载调用，避免数据被加载两次
                // LoadEnhancementTemplates();

                // 不再预加载强化记录，改为实时从数据库读取

                // 获取加载的数据条数
                QueryResult result = WorldDatabase.Query("SELECT COUNT(*) FROM _模板_物品强化");
                if (result)
                {
                    Field* fields = result->Fetch();
                    count = fields[0].Get<uint32>();
                }

                LOG_INFO("server.loading", "→_模板_物品强化 加载完成√");
                        }
            catch (const std::exception& e)
            {
                LOG_ERROR("module.item-enhancement", ModuleOldFormat("加载物品强化系统数据时发生错误: %s", e.what()).c_str());
            }

            _initialized = true;
        }
    }

    void OnShutdown() override
    {
        // 服务器关闭时的清理工作
        LOG_INFO("module.itemenhancement", "物品强化系统已关闭");
    }

private:
    bool _initialized;     // 是否已初始化
    uint32 _loadTimer;     // 计时器
    uint32 _delayTime;     // 延迟时间
};

// 添加脚本
void AddSC_ItemEnhancementLoader()
{
    new ItemEnhancementLoader();
}
