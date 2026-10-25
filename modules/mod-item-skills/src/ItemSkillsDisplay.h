#ifndef MOD_ITEM_SKILLS_DISPLAY_H
#define MOD_ITEM_SKILLS_DISPLAY_H

#include "Common.h"
#include "Player.h"
#include "Item.h"
#include "ItemSkillsManager.h"
#include <vector>
#include <string>

// 物品技能显示管理器
class ItemSkillsDisplay
{
public:
    static ItemSkillsDisplay* instance();

    // 初始化
    void Initialize();

    // 添加技能信息到提示框
    void AddItemSkillsToTooltip(Player* player, Item* item, std::vector<std::string>& lines);

    // 生成物品技能描述
    std::string GenerateItemSkillsDescription(Item* item);

    // 获取技能颜色代码
    std::string GetSkillColorCode(ItemSkillTemplate const* skillTemplate);

    // SendSkillInfoToClient 已删除 - 使用批量查询替代
    // void SendSkillInfoToClient(Player* player, Item* item);  // 旧格式，已废弃

private:
    // 格式化技能描述
    std::string FormatSkillDescription(ItemSkillTemplate const* skillTemplate, uint32 level);
};

#define sItemSkillsDisplay ItemSkillsDisplay::instance()

#endif // MOD_ITEM_SKILLS_DISPLAY_H
