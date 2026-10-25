#include "ItemAttributesGlobalScript.h"
#include "ItemAttributesLoader.h"
#include "Config.h"
#include "Log.h"

ItemAttributesGlobalScript::ItemAttributesGlobalScript()
    : GlobalScript("ItemAttributesGlobalScript")
{
}

void ItemAttributesGlobalScript::OnItemDelFromDB(CharacterDatabaseTransaction trans, ObjectGuid::LowType itemGuid)
{
    if (!sConfigMgr->GetOption<bool>("ItemAttributes.Enable", true))
        return;

    // 注意：这里不立即删除属性数据！
    // 原因：玩家出售装备到商店后，可以通过回购功能买回
    // 如果立即清理属性数据，买回的装备将失去所有自定义属性
    //
    // 清理策略：
    // 1. 玩家登出时，清理所有孤立的属性数据（OnLogout）
    // 2. 这样商店回购期间，属性数据仍然保留
    // 3. 玩家买回装备后，属性依然存在 ✅
    // 4. 玩家登出后，未买回的装备属性会被清理

    // 这里什么都不做，由 OnLogout 统一清理
}

// 注册脚本
void AddSC_ItemAttributesGlobalScript()
{
    new ItemAttributesGlobalScript();
}
