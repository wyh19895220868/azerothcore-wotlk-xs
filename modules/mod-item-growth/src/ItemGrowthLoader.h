#ifndef MODULE_ITEM_GROWTH_LOADER_H
#define MODULE_ITEM_GROWTH_LOADER_H

// 主要脚本
void AddSC_ItemGrowth();
void AddSC_ItemGrowthCommands();
void AddSC_ItemGrowthUIScript();

// 添加所有脚本
void AddItemGrowthScripts()
{
    // 主要脚本
    AddSC_ItemGrowth();
    AddSC_ItemGrowthCommands();
    AddSC_ItemGrowthUIScript();
}

#endif // MODULE_ITEM_GROWTH_LOADER_H
