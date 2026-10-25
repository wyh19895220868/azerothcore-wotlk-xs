#include "ItemSkillsDisplay.h"
#include "ItemSkillsDBHelper.h"
#include "ItemSkillsEffects.h"
#include "ItemSkillsManager.h"
#include "Chat.h"
#include "Logging/Log.h"
#include <sstream>

ItemSkillsDisplay* ItemSkillsDisplay::instance()
{
    static ItemSkillsDisplay instance;
    return &instance;
}

void ItemSkillsDisplay::Initialize()
{
    // 初始化
}

void ItemSkillsDisplay::AddItemSkillsToTooltip(Player* player, Item* item, std::vector<std::string>& lines)
{
    if (!player || !item)
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();
    uint64 itemGuid = item->GetGUID().GetCounter();

    // 优先从内存读取
    std::vector<ItemSkillData> skills = sItemSkillsManager->GetItemSkillsFromMemory(playerGuid, itemGuid);
    if (skills.empty())
    {
        skills = sItemSkillsDBHelper->GetItemSkillsData(item);
    }

    if (skills.empty())
        return;

    // 添加技能标题
    lines.push_back("|cFF00FF00物品技能:|r");

    // 添加各个技能描述
    for (const auto& skillData : skills)
    {
        ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillData.skillTemplateId);
        if (!skillTemplate)
            continue;

        std::string description = sItemSkillsEffects->GetSkillDescription(item, skillData.skillTemplateId);
        std::string colorCode = GetSkillColorCode(skillTemplate);

        lines.push_back(colorCode + description + "|r");
    }
}

std::string ItemSkillsDisplay::GenerateItemSkillsDescription(Item* item)
{
    if (!item)
        return "";

    std::vector<ItemSkillData> skills = sItemSkillsDBHelper->GetItemSkillsData(item);

    if (skills.empty())
        return "";

    std::ostringstream ss;
    ss << "|cFF00FF00物品技能:|r";

    for (const auto& skillData : skills)
    {
        ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillData.skillTemplateId);
        if (!skillTemplate)
            continue;

        std::string description = FormatSkillDescription(skillTemplate, 1);
        std::string colorCode = GetSkillColorCode(skillTemplate);

        ss << "\n" << colorCode << description << "|r";
    }

    return ss.str();
}

std::string ItemSkillsDisplay::GetSkillColorCode(ItemSkillTemplate const* skillTemplate)
{
    if (!skillTemplate)
        return "|cFFFFFFFF"; // 白色

    // 根据触发类型返回不同颜色
    switch (skillTemplate->triggerType)
    {
        case ITEM_SKILL_TRIGGER_ON_USE:
            return "|cFF00FF00"; // 绿色 - 使用触发
        case ITEM_SKILL_TRIGGER_ON_EQUIP:
            return "|cFF0080FF"; // 蓝色 - 装备触发
        case ITEM_SKILL_TRIGGER_ON_HIT:
            return "|cFFFF8000"; // 橙色 - 击中触发
        case ITEM_SKILL_TRIGGER_LEARN_SPELL:
            return "|cFFFF00FF"; // 品红色 - 学习技能
        default:
            return "|cFFFFFFFF"; // 白色
    }
}

// SendSkillInfoToClient 已删除 - 旧格式 ITEMSKILLS: 消息
// 技能数据现在通过 ItemIdentificationSystem 的批量查询 (HandleAddonBatchQuery) 返回
// 不再需要单独发送技能信息

std::string ItemSkillsDisplay::FormatSkillDescription(ItemSkillTemplate const* skillTemplate, uint32 level)
{
    if (!skillTemplate)
        return "";

    std::ostringstream ss;

    // 根据触发类型添加前缀
    switch (skillTemplate->triggerType)
    {
        case ITEM_SKILL_TRIGGER_ON_USE:
            ss << "使用: ";
            break;
        case ITEM_SKILL_TRIGGER_ON_EQUIP:
            ss << "装备: ";
            break;
        case ITEM_SKILL_TRIGGER_ON_HIT:
            ss << "击中时: ";
            break;
        case ITEM_SKILL_TRIGGER_LEARN_SPELL:
            ss << "学习: ";
            break;
        default:
            break;
    }

    // 添加技能名称
    ss << skillTemplate->clientDisplay;

    // 添加等级信息
    if (level > 1)
        ss << " (等级 " << level << ")";

    // 添加触发几率信息
    if (skillTemplate->triggerChance < 100.0f)
        ss << " [" << skillTemplate->triggerChance << "% 几率]";

    // 添加PPM信息
    if (skillTemplate->triggerType == ITEM_SKILL_TRIGGER_ON_HIT && skillTemplate->ppmRate > 0)
        ss << " [每分钟 " << skillTemplate->ppmRate << " 次]";

    // 添加使用次数信息
    if (skillTemplate->charges != 0)
    {
        if (skillTemplate->charges > 0)
            ss << " (" << skillTemplate->charges << " 次使用)";
        else
            ss << " (" << abs(skillTemplate->charges) << " 次使用后消失)";
    }

    return ss.str();
}
