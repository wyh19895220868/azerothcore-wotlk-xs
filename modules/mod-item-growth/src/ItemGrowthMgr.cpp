#include "ItemGrowthMgr.h"
#include "Log.h"
#include "ScriptMgr.h"
#include "GameTime.h"
#include "Tokenize.h"
#include "Chat.h"
#include <algorithm>

// 核心管理功能实现

ItemGrowthMgr* ItemGrowthMgr::instance()
{
    static ItemGrowthMgr instance;
    return &instance;
}

ItemGrowthMgr::ItemGrowthMgr() :
    _enabled(false),
    _maxLevel(100),
    _baseExpMultiplier(1.0f),
    _upgradeSuccessRate(80),
    _requireSoulbound(true),
    _lastBatchSaveTime(0),
    _batchSaveInterval(5000),      // 5秒批量保存一次
    _expProcessInterval(2000)      // 2秒处理一次累积经验
{
}

// 以下方法已移动到其他文件:
// - Initialize -> ItemGrowthCore.cpp
// - LoadFromDB -> ItemGrowthCore.cpp
// - GetItemGrowthTemplate -> ItemGrowthCore.cpp
// - GetItemGrowthTemplateByLevel -> ItemGrowthCore.cpp
// - GetPlayerItemRecord -> ItemGrowthCore.cpp
// - SavePlayerItemRecord -> ItemGrowthCore.cpp
// - CreatePlayerItemRecord -> ItemGrowthCore.cpp
// - CanItemGrow -> ItemGrowthCore.cpp
// - TimeToTimestampStr -> ItemGrowthCore.cpp
// - ApplyItemAttributes -> ItemGrowthCore.cpp
