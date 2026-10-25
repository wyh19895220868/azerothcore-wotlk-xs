#include "ItemEnhancement.h"
#include "ScriptMgr.h"
#include "Logging/Log.h"
#include "World.h"
#include "GameTime.h"

// 【已废弃】此WorldScript已被ItemEnhancementLoader替代
// 保留空实现以保持API兼容性，避免链接错误
// 实际的加载逻辑由 ItemEnhancementLoader.cpp 中的 ItemEnhancementLoader 类处理

// 添加脚本（空实现）
void AddSC_ItemEnhancementWorldScript()
{
    // 不再创建WorldScript实例，避免重复加载
    // 加载逻辑已统一到 ItemEnhancementLoader
    LOG_DEBUG("module.itemenhancement", "ItemEnhancementWorldScript: Skipped (using ItemEnhancementLoader instead)");
}
