#include "DatabaseEnv.h"
#include "Log.h"
#include "RuneSystem.h"
#include "ScriptMgr.h"
#include "World.h"
#include "Logging/Log.h"

// 使用WorldScript的OnUpdate钩子函数实现延迟加载
class RuneSystemWorldScript : public WorldScript
{
public:
    RuneSystemWorldScript() : WorldScript("RuneSystemWorldScript")
    {
        _loadTimer = 0;
        _isLoaded = false;
        _loadDelay = 1 * IN_MILLISECONDS; // 1秒延迟，确保数据库连接就绪
    }

    void OnStartup() override
    {
        // 静默加载
    }

    void OnUpdate(uint32 diff) override
    {
        if (_isLoaded)
            return;

        _loadTimer += diff;
        if (_loadTimer >= _loadDelay)
        {
            // 延迟5秒后加载符文数据
            LoadModule();
            _isLoaded = true;
        }
    }

private:
    uint32 _loadTimer;
    uint32 _loadDelay;
    bool _isLoaded;

    uint32 LoadModule()
    {
        // 初始化符文系统（这会调用LoadRunesFromDB）
        sRuneSystem->Initialize();

        // 获取加载的符文数量
        uint32 count = 0;
        for (uint32 i = 1; i < 10000; i++) {
            if (sRuneSystem->GetRuneInfo(i)) {
                count++;
            }
        }

        LOG_INFO("server.loading", "→_模板_物品符文 加载完成√");

        return count;
    }
};

// 添加脚本
void AddSC_RuneSystemLoader()
{
    new RuneSystemWorldScript();
}
