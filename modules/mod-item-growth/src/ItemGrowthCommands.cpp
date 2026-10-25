#include "AsInt256.h"
#include "StringConvert.h"
#include "ItemGrowthCommands.h"
#include "ItemGrowthMgr.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include "Language.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"
#include "ObjectMgr.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Logging/Log.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include "Map.h"
#include "Group.h"  // 【审计修复】用于权限检查
#include "StringConvert.h"
#include "ModuleFormat.h"

namespace
{
bool HasNeedTemplate(uint32 requirementId)
{
    return !requirementId || NeedTempStore.find(requirementId) != NeedTempStore.end();
}

bool CheckNeedTemplate(Player* player, uint32 requirementId, bool showMessages)
{
    if (!requirementId)
        return true;

    if (!HasNeedTemplate(requirementId))
    {
        if (showMessages && player)
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("需求模板%s不存在，请检查 __模版需求总表。", requirementId).c_str());
        return false;
    }

    return showMessages ? sHelper->NotDeleteCheckNeed(player, requirementId) : sHelper->ShowCheckNeed(player, requirementId);
}

bool ConsumeNeedTemplate(Player* player, uint32 requirementId, std::string const& info)
{
    if (!requirementId)
        return true;

    if (!HasNeedTemplate(requirementId))
    {
        if (player)
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("需求模板%s不存在，请检查 __模版需求总表。", requirementId).c_str());
        return false;
    }

    return sHelper->CheckNeed(player, requirementId, info);
}
}

// 属性名称映射 - 基于AzerothCore ItemModType枚举定义
std::string GetAttributeName(uint32 attributeType)
{
    switch (attributeType)
    {
        // 基础属性 (ITEM_MOD_*)
        case 0: return "法力值";                    // ITEM_MOD_MANA
        case 1: return "生命值";                    // ITEM_MOD_HEALTH
        case 3: return "敏捷";                      // ITEM_MOD_AGILITY
        case 4: return "力量";                      // ITEM_MOD_STRENGTH
        case 5: return "智力";                      // ITEM_MOD_INTELLECT
        case 6: return "精神";                      // ITEM_MOD_SPIRIT
        case 7: return "耐力";                      // ITEM_MOD_STAMINA

        // 防御等级属性
        case 12: return "防御技能等级";             // ITEM_MOD_DEFENSE_SKILL_RATING
        case 13: return "躲闪等级";                 // ITEM_MOD_DODGE_RATING
        case 14: return "招架等级";                 // ITEM_MOD_PARRY_RATING
        case 15: return "格挡等级";                 // ITEM_MOD_BLOCK_RATING

        // 命中等级属性
        case 16: return "近战命中等级";             // ITEM_MOD_HIT_MELEE_RATING
        case 17: return "远程命中等级";             // ITEM_MOD_HIT_RANGED_RATING
        case 18: return "法术命中等级";             // ITEM_MOD_HIT_SPELL_RATING

        // 暴击等级属性
        case 19: return "近战暴击等级";             // ITEM_MOD_CRIT_MELEE_RATING
        case 20: return "远程暴击等级";             // ITEM_MOD_CRIT_RANGED_RATING
        case 21: return "法术暴击等级";             // ITEM_MOD_CRIT_SPELL_RATING

        // 命中回避等级属性
        case 22: return "近战命中回避等级";         // ITEM_MOD_HIT_TAKEN_MELEE_RATING
        case 23: return "远程命中回避等级";         // ITEM_MOD_HIT_TAKEN_RANGED_RATING
        case 24: return "法术命中回避等级";         // ITEM_MOD_HIT_TAKEN_SPELL_RATING

        // 暴击回避等级属性
        case 25: return "近战暴击回避等级";         // ITEM_MOD_CRIT_TAKEN_MELEE_RATING
        case 26: return "远程暴击回避等级";         // ITEM_MOD_CRIT_TAKEN_RANGED_RATING
        case 27: return "法术暴击回避等级";         // ITEM_MOD_CRIT_TAKEN_SPELL_RATING

        // 急速等级属性
        case 28: return "近战急速等级";             // ITEM_MOD_HASTE_MELEE_RATING
        case 29: return "远程急速等级";             // ITEM_MOD_HASTE_RANGED_RATING
        case 30: return "法术急速等级";             // ITEM_MOD_HASTE_SPELL_RATING

        // 综合等级属性
        case 31: return "命中等级";                 // ITEM_MOD_HIT_RATING
        case 32: return "暴击等级";                 // ITEM_MOD_CRIT_RATING
        case 33: return "命中回避等级";             // ITEM_MOD_HIT_TAKEN_RATING
        case 34: return "暴击回避等级";             // ITEM_MOD_CRIT_TAKEN_RATING
        case 35: return "韧性等级";                 // ITEM_MOD_RESILIENCE_RATING
        case 36: return "急速等级";                 // ITEM_MOD_HASTE_RATING
        case 37: return "精通等级";                 // ITEM_MOD_EXPERTISE_RATING

        // 攻击强度属性
        case 38: return "攻击强度";                 // ITEM_MOD_ATTACK_POWER
        case 39: return "远程攻击强度";             // ITEM_MOD_RANGED_ATTACK_POWER

        // 法术属性（已废弃）
        case 41: return "法术治疗";                 // ITEM_MOD_SPELL_HEALING_DONE (deprecated)
        case 42: return "法术伤害";                 // ITEM_MOD_SPELL_DAMAGE_DONE (deprecated)

        // 其他属性
        case 43: return "法力回复";                 // ITEM_MOD_MANA_REGENERATION
        case 44: return "护甲穿透等级";             // ITEM_MOD_ARMOR_PENETRATION_RATING
        case 45: return "法术强度";                 // ITEM_MOD_SPELL_POWER
        case 46: return "生命回复";                 // ITEM_MOD_HEALTH_REGEN
        case 47: return "法术穿透";                 // ITEM_MOD_SPELL_PENETRATION
        case 48: return "格挡值";                   // ITEM_MOD_BLOCK_VALUE

        default: return "未知属性(" + Acore::ToString(attributeType) + ")";
    }

}

// 在同一地图内，按 seed(+rpId) 搜索任意玩家的该物品（用于聊天链接等他人物品场景）
// 【审计修复】增加权限限制，只允许查看同队伍或同公会成员的物品
static Item* FindNearbyItemBySeed(Player* requester, uint32 itemId, uint32 seed, int32 rpId)
{
    if (!requester || seed == 0)
        return nullptr;

    Map* map = requester->GetMap();
    if (!map)
        return nullptr;

    auto match = [itemId, seed, rpId](Item* it) -> bool {
        if (!it || it->GetEntry() != itemId)
            return false;
        if (it->GetUInt32Value(ITEM_FIELD_PROPERTY_SEED) != seed)
            return false;
        if (rpId != 0 && it->GetItemRandomPropertyId() != rpId)
            return false;
        return true;
    };

    // 【审计修复】权限检查函数：只允许查看自己、队友或公会成员的物品
    auto canViewPlayerItems = [requester](Player* targetPlayer) -> bool {
        if (!targetPlayer)
            return false;

        // 可以查看自己的物品
        if (targetPlayer == requester)
            return true;

        // 可以查看同队伍成员的物品
        Group* group = requester->GetGroup();
        if (group && group->IsMember(targetPlayer->GetGUID()))
            return true;

        // 可以查看同公会成员的物品
        if (requester->GetGuildId() != 0 && requester->GetGuildId() == targetPlayer->GetGuildId())
            return true;

        // 其他情况不允许查看
        return false;
    };

    Map::PlayerList const& players = map->GetPlayers();
    for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
    {
        Player* p = itr->GetSource();
        if (!p)
            continue;

        // 【审计修复】权限检查
        if (!canViewPlayerItems(p))
            continue;

        // 主背包
        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            Item* it = p->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (match(it)) return it;
        }
        // 其他背包
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            Bag* bagObj = p->GetBagByPos(i);
            if (!bagObj) continue;
            for (uint32 j = 0; j < bagObj->GetBagSize(); ++j)
            {
                Item* it = bagObj->GetItemByPos(j);
                if (match(it)) return it;
            }
        }
        // 装备栏
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* it = p->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (match(it)) return it;
        }
    }

    return nullptr;
}

// 基于 item link 的随机种子(seed) 和 随机属性ID(rpId) 查找玩家物品
// 注意：客户端聊天链接的第8段通常是 property seed（uniqueId），第7段是随机附魔ID/后缀ID
Item* FindPlayerItemBySeed(Player* player, uint32 itemId, uint32 seed, int32 rpId, int32 bag, int32 slot, bool isEquip)
{
    if (!player || seed == 0)
        return nullptr;

    auto match = [itemId, seed, rpId](Item* it) -> bool {
        if (!it || it->GetEntry() != itemId)
            return false;
        uint32 propSeed = it->GetUInt32Value(ITEM_FIELD_PROPERTY_SEED);
        if (propSeed != seed)
            return false;
        if (rpId != 0)
        {
            // 当提供了 rpId 时，要求严格匹配（容忍正负号差异：后缀在服务器为负数，链接中常为正数的SuffixId）
            int32 cur = it->GetItemRandomPropertyId();
            if (!(cur == rpId || (cur < 0 && -cur == rpId) || (rpId < 0 && -rpId == cur)))
                return false;
        }
        return true;
    };

    // 首选按位置尝试（若提供）
    if (isEquip)
    {
        int32 clientSlot = slot;
        uint8 serverSlot = (clientSlot > 0) ? static_cast<uint8>(clientSlot - 1) : static_cast<uint8>(clientSlot);
        if (serverSlot >= EQUIPMENT_SLOT_START && serverSlot < EQUIPMENT_SLOT_END)
        {
            Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
            if (match(it))
                return it;
        }
    }
    if (bag >= 0)
    {
        if (bag == 0)
        {
            int32 clientSlot = slot;
            uint8 serverSlot = (clientSlot > 0) ? static_cast<uint8>(clientSlot - 1) : static_cast<uint8>(clientSlot);
            Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
            if (match(it))
                return it;
        }
        else
        {
            uint8 bagPos = INVENTORY_SLOT_BAG_START + static_cast<uint8>(bag - 1);
            Bag* bagObj = player->GetBagByPos(bagPos);
            if (bagObj)
            {
                int32 clientSlot = slot;
                if (clientSlot >= 1 && static_cast<uint32>(clientSlot - 1) < bagObj->GetBagSize())
                {
                    Item* it = bagObj->GetItemByPos(static_cast<uint32>(clientSlot - 1));
                    if (match(it))
                        return it;
                }
            }
        }
    }

    // 全量扫描：主包、其他背包、装备
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (match(it))
            return it;
    }
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bagObj = player->GetBagByPos(i);
        if (!bagObj) continue;
        for (uint32 j = 0; j < bagObj->GetBagSize(); ++j)
        {
            Item* it = bagObj->GetItemByPos(j);
            if (match(it))
                return it;
        }
    }
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (match(it))
            return it;
    }

    return nullptr;
}

// 添加脚本
void AddSC_ItemGrowthCommands()
{
    new ItemGrowthCommands();
}

ItemGrowthCommands::ItemGrowthCommands() : CommandScript("ItemGrowthCommands") { }

ChatCommandTable ItemGrowthCommands::GetCommands() const
{
    static ChatCommandTable itemGrowthCommandTable =
    {
        { "信息",     HandleItemGrowthInfoCommand,        SEC_PLAYER,     Console::No },
        { "升级",     HandleItemGrowthUpgradeCommand,     SEC_PLAYER,     Console::No },
        { "属性",     HandleItemGrowthAttributesCommand,  SEC_PLAYER,     Console::No },
        { "重载",     HandleItemGrowthReloadCommand,      SEC_GAMEMASTER, Console::Yes },

        // 测试命令
        { "测试",     HandleItemGrowthTestCommand,        SEC_GAMEMASTER, Console::No },
        { "加经验",   HandleItemGrowthAddExpCommand,      SEC_GAMEMASTER, Console::No },
        { "设等级",   HandleItemGrowthSetLevelCommand,    SEC_GAMEMASTER, Console::No },
        { "重置",     HandleItemGrowthResetCommand,       SEC_GAMEMASTER, Console::No },
        { "列表",     HandleItemGrowthListCommand,        SEC_GAMEMASTER, Console::No },
        { "统计",     HandleItemGrowthStatsCommand,       SEC_GAMEMASTER, Console::No },
        { "设置成长", HandleItemGrowthSetGrowableCommand, SEC_GAMEMASTER, Console::No },
        { "设置组",   HandleItemGrowthSetGroupCommand,    SEC_GAMEMASTER, Console::No },
        { "随机组",   HandleItemGrowthRandomGroupCommand, SEC_PLAYER, Console::No },
        { "属性详情", HandleItemGrowthAttributeInfoCommand, SEC_PLAYER,   Console::No },
        { "调试模板", HandleItemGrowthDebugTemplatesCommand, SEC_GAMEMASTER, Console::No },
        { "查看原始属性", HandleItemGrowthViewOriginalStatsCommand, SEC_PLAYER, Console::No },
        { "强制应用", HandleItemGrowthForceApplyCommand, SEC_GAMEMASTER, Console::No },  // 【审计修复】改为GM权限
        { "检查GUID", HandleItemGrowthCheckGuidCommand, SEC_GAMEMASTER, Console::No },   // 【审计修复】改为GM权限
        { "验证属性", HandleItemGrowthVerifyAttributesCommand, SEC_GAMEMASTER, Console::No },  // 【审计修复】改为GM权限
        { "测试Aura", HandleItemGrowthTestAuraCommand, SEC_GAMEMASTER, Console::No },    // 【审计修复】改为GM权限
        { "查询",     HandleItemGrowthQueryCommand, SEC_PLAYER, Console::No },

        { "",         HandleItemGrowthCommand,            SEC_PLAYER,     Console::No }
    };

    static ChatCommandTable commandTable =
    {
        { "物品成长", itemGrowthCommandTable }
    };

    return commandTable;
}

bool ItemGrowthCommands::HandleItemGrowthCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    handler->SendSysMessage("物品成长系统命令：");
    handler->SendSysMessage("=== 基础命令 ===");
    handler->SendSysMessage(".物品成长 信息 - 查看当前装备的成长信息");
    handler->SendSysMessage(".物品成长 升级 - 尝试升级当前装备");
    handler->SendSysMessage(".物品成长 属性 - 手动应用属性组效果");
    handler->SendSysMessage(".物品成长 属性详情 - 查看成长属性详细信息");
    handler->SendSysMessage(".物品成长 查看原始属性 - 查看装备的原始属性信息");
    handler->SendSysMessage(".物品成长 强制应用 - 强制应用当前装备的成长属性到角色");
    handler->SendSysMessage(".物品成长 检查GUID [槽位号] - 检查装备的GUID存储状态（默认主手，槽位15=副手）");
    handler->SendSysMessage(".物品成长 检查GUID - 检查当前选中装备的GUID存储状态");
    handler->SendSysMessage(".物品成长 验证属性 - 验证当前装备的成长属性是否正确应用到角色");
    handler->SendSysMessage(".物品成长 测试Aura - 测试新的Aura系统属性应用");
    handler->SendSysMessage(".物品成长 随机组 [物品ID] - 为指定物品随机分配成长组（基于数据库几率配置）");

    if (handler->GetSession()->GetSecurity() >= SEC_GAMEMASTER)
    {
        handler->SendSysMessage("=== 管理员命令 ===");
        handler->SendSysMessage(".物品成长 重载 - 重新加载物品成长系统数据");
        handler->SendSysMessage(".物品成长 测试 - 运行系统测试");
        handler->SendSysMessage(".物品成长 加经验 [数量] - 为当前物品添加经验");
        handler->SendSysMessage(".物品成长 设等级 [等级] - 设置当前物品等级");
        handler->SendSysMessage(".物品成长 重置 - 重置当前物品成长数据");
        handler->SendSysMessage(".物品成长 列表 - 显示所有成长模板");
        handler->SendSysMessage(".物品成长 统计 - 显示系统统计信息");
        handler->SendSysMessage(".物品成长 设置成长 [属性组] [物品ID] - 设置物品可以成长（0=官方装备成长）");
        handler->SendSysMessage(".物品成长 设置组 [属性组] [物品ID] - 给指定物品设置属性组");
        handler->SendSysMessage(".物品成长 调试模板 [属性组] - 显示指定属性组的所有模板配置");
    }

    return true;
}

bool ItemGrowthCommands::HandleItemGrowthInfoCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 获取玩家当前装备的主手武器
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!item)
    {
        handler->SendSysMessage("你需要装备一件物品才能查看其成长信息。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

    if (!record)
    {
        // 检查物品是否可以成长
        if (sItemGrowthMgr->CanItemGrow(item))
        {
            handler->SendSysMessage("该物品可以成长，但尚未激活成长系统。");
            handler->SendSysMessage("使用 .物品成长 升级 命令激活成长系统。");
        }
        else
        {
            handler->SendSysMessage("该物品不支持成长系统。");
            handler->SendSysMessage("只有已绑定的武器、护甲和饰品可以成长。");
        }
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 获取当前等级的模板
    ItemGrowthTemplate const* currentTemplate = sItemGrowthMgr->GetItemGrowthTemplateByLevel(record->currentLevel);
    if (!currentTemplate)
    {
        handler->SendSysMessage("无法找到物品当前等级的成长模板。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 基本信息
    handler->PSendSysMessage("%s", ModuleOldFormat("物品：%s", item->GetTemplate()->Name1).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("当前等级：%s", record->currentLevel).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("当前经验：%s", record->currentExp).c_str());

    // 显示升级经验信息
    if (record->currentLevel < sItemGrowthMgr->GetMaxLevel())
    {
        // 更新升级经验需求
        sItemGrowthMgr->UpdateRequiredExp(record);

        handler->PSendSysMessage("%s", ModuleOldFormat("升级所需经验：%s/%s (%s%%)", record->currentExp, record->requiredExp, record->requiredExp > 0 ? (float)record->currentExp / record->requiredExp * 100.0f : 0.0f).c_str());

        // 添加调试信息
        if (record->requiredExp == 0)
        {
            handler->PSendSysMessage("调试信息：升级经验为0，表示已达到该属性组的最高等级");
        }
    }
    else
    {
        // 静默处理：已达到最高等级，不显示任何信息
    }

    // 成长属性信息
    if (!record->growthAttributes.empty())
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("成长属性：%s", record->growthAttributes).c_str());

        // 解析并显示详细属性
        std::vector<std::pair<uint32, AsInt64>> attributes;
        sItemGrowthMgr->ParseGrowthAttributes(record->growthAttributes, attributes);

        if (!attributes.empty())
        {
            std::string detailStr = "详细属性：";
            for (size_t i = 0; i < attributes.size(); ++i)
            {
                if (i > 0) detailStr += ", ";

                std::string attrName = GetAttributeName(attributes[i].first);
                detailStr += attrName + "+" + Acore::ToString(attributes[i].second);
            }
            handler->PSendSysMessage(detailStr.c_str());
        }
    }
    else
    {
        handler->PSendSysMessage("成长属性：无");
    }

    // 兼容性显示（旧字段）
    handler->PSendSysMessage("%s", ModuleOldFormat("基础属性加成：%s", Acore::ToString(record->currentAttributeValue)).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("基础属性百分比加成：%s%%", Acore::ToString(record->currentAttributePercent)).c_str());

    // 属性分配类型信息
    std::string distributionType = currentTemplate->attributeDistributionType == 0 ? "平均分配" : "随机分配";
    handler->PSendSysMessage("%s", ModuleOldFormat("属性分配方式：%s", distributionType).c_str());

    if (currentTemplate->attributeDistributionType == 1)
    {
        // 显示随机分配的范围
        handler->PSendSysMessage("%s", ModuleOldFormat("随机属性值范围：%s - %s", Acore::ToString(currentTemplate->minAttributeValue), Acore::ToString(currentTemplate->maxAttributeValue)).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("随机属性百分比范围：%s%% - %s%%", Acore::ToString(currentTemplate->minAttributePercent), Acore::ToString(currentTemplate->maxAttributePercent)).c_str());
    }



    // 成长统计
    handler->PSendSysMessage("%s", ModuleOldFormat("成长次数：%s", record->growthCount).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("失败次数：%s", record->failCount).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("成功率：%s%%", record->growthCount > 0 ?
                            (float)record->growthCount / (record->growthCount + record->failCount) * 100.0f : 0.0f).c_str());



    // 物品属性组关联信息
    // 显示当前等级模板关联的属性组ID，以及物品属性模块的可用状态
    handler->PSendSysMessage("%s", ModuleOldFormat("关联属性组：%s%s", currentTemplate->itemPropertyGroup, currentTemplate->itemPropertyGroup == 0 ? "（官方装备成长）" : "").c_str());

    if (currentTemplate->itemPropertyGroup > 0)
    {

        // 检查并显示mod-item-attributes模块状态
        if (sItemGrowthMgr->IsItemAttributesModuleAvailable())
        {
            handler->PSendSysMessage("物品属性模块：已启用");
            if (sItemGrowthMgr->GetAutoApplyAttributes())
            {
                handler->PSendSysMessage("自动应用属性：已启用（升级时自动应用）");
            }
            else
            {
                handler->PSendSysMessage("自动应用属性：已禁用（需手动使用 .物品成长 属性 命令）");
            }
        }
        else
        {
            handler->PSendSysMessage("物品属性模块：未启用（属性组功能不可用）");
        }
    }
    else
    {
        handler->PSendSysMessage("当前等级未关联属性组");
    }






    return true;
}

bool ItemGrowthCommands::HandleItemGrowthUpgradeCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 获取玩家当前装备的主手武器
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!item)
    {
        handler->SendSysMessage("你需要装备一件物品才能进行升级。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

    if (!record)
    {
        // 创建新记录
        sItemGrowthMgr->CreatePlayerItemRecord(player, item);
        record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

        if (!record)
        {
            handler->SendSysMessage("无法创建物品成长记录。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->SendSysMessage("物品成长系统已激活。");
    }

    // 获取下一级所需经验
    // 【参数写反修复】原调用 GetItemGrowthTemplateByLevel(1, currentLevel+1) 把 level/group
    // 两个参数颠倒（查"组 currentLevel+1 的 1 级模板"），且外层 1000 次循环每次调用
    // 完全相同纯属空转。正确语义：查本物品属性组的下一级模板
    ItemGrowthTemplate const* nextTemplate = sItemGrowthMgr->GetItemGrowthTemplateByLevel(record->currentLevel + 1, record->itemPropertyGroup);

    if (!nextTemplate)
    {
        // 静默处理：已达到最高等级
        return false;
    }

    if (record->currentExp < nextTemplate->requiredExp)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("经验不足，无法升级。当前经验：%s/%s", record->currentExp, nextTemplate->requiredExp).c_str());
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 尝试升级
    if (sItemGrowthMgr->UpgradeItem(player, item))
    {
        handler->SendSysMessage("升级成功！");
        return true;
    }
    else
    {
        handler->SendSysMessage("升级失败。");
        handler->SetSentErrorMessage(true);
        return false;
    }
}



// 处理 .物品成长 属性 命令
// 功能：手动为当前装备的物品应用属性组效果
// 说明：此命令会忽略AutoApplyAttributes配置，强制应用属性组效果
//       需要物品已激活成长系统且当前等级模板关联了属性组
bool ItemGrowthCommands::HandleItemGrowthAttributesCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 获取玩家当前装备的主手武器
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!item)
    {
        handler->SendSysMessage("你需要装备一件物品才能应用属性。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

    if (!record)
    {
        handler->SendSysMessage("该物品尚未激活成长系统。");
        handler->SendSysMessage("使用 .物品成长 升级 命令激活成长系统。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 获取当前等级的模板
    ItemGrowthTemplate const* currentTemplate = sItemGrowthMgr->GetItemGrowthTemplateByLevel(record->currentLevel);
    if (!currentTemplate)
    {
        handler->SendSysMessage("无法找到物品当前等级的成长模板。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查属性组类型
    if (currentTemplate->itemPropertyGroup == 0)
    {
        // 0组：官方装备成长，直接应用
        handler->SendSysMessage("正在为官方装备应用成长属性...");
        sItemGrowthMgr->ForceApplyItemAttributes(player, item);
        return true;
    }

    // 非0组：检查物品属性模块是否可用
    if (!sItemGrowthMgr->IsItemAttributesModuleAvailable())
    {
        handler->SendSysMessage("物品属性模块未启用，无法应用属性组效果。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 手动应用属性
    handler->PSendSysMessage("%s", ModuleOldFormat("正在为物品应用属性组 %s 的效果...", currentTemplate->itemPropertyGroup).c_str());
    sItemGrowthMgr->ForceApplyItemAttributes(player, item);

    return true;
}

bool ItemGrowthCommands::HandleItemGrowthTestCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    handler->SendSysMessage("=== 物品成长系统测试 ===");

    // 测试系统状态
    handler->PSendSysMessage("%s", ModuleOldFormat("系统状态：%s", sItemGrowthMgr->IsEnabled() ? "已启用" : "已禁用").c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("最大等级：%s", sItemGrowthMgr->GetMaxLevel()).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("经验倍率：%s", sItemGrowthMgr->GetBaseExpMultiplier()).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("升级成功率：%s%%", sItemGrowthMgr->GetUpgradeSuccessRate()).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("需要绑定：%s", sItemGrowthMgr->GetRequireSoulbound() ? "是" : "否").c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("自动应用属性：%s", sItemGrowthMgr->GetAutoApplyAttributes() ? "是" : "否").c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("物品属性模块：%s", sItemGrowthMgr->IsItemAttributesModuleAvailable() ? "已启用" : "未启用").c_str());

    // 测试模板数据
    handler->PSendSysMessage("%s", ModuleOldFormat("已加载模板数量：%s", sItemGrowthMgr->GetTemplateCount()).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("玩家记录数量：%s", sItemGrowthMgr->GetPlayerRecordCount()).c_str());

    // 测试当前装备
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (item)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("当前主手物品：%s (ID: %s)", item->GetTemplate()->Name1, item->GetEntry()).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("物品可成长：%s", sItemGrowthMgr->CanItemGrow(item) ? "是" : "否").c_str());

        uint32 itemGuid = item->GetGUID().GetCounter();
        ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);
        handler->PSendSysMessage("%s", ModuleOldFormat("已激活成长：%s", record ? "是" : "否").c_str());
    }
    else
    {
        handler->SendSysMessage("未装备主手物品");
    }

    handler->SendSysMessage("=== 测试完成 ===");
    return true;
}

bool ItemGrowthCommands::HandleItemGrowthAddExpCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 【审计修复】解析经验数量参数，增加范围检查防止溢出
    uint32 expAmount = 100; // 默认100经验
    if (args && *args)
    {
        // 使用 strtoul 进行安全解析
        char* endPtr = nullptr;
        unsigned long parsedValue = strtoul(args, &endPtr, 10);

        // 检查解析是否成功
        if (endPtr == args || *endPtr != '\0')
        {
            handler->SendSysMessage("无效的经验数量，请输入正整数。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 检查范围，防止溢出和过大的值
        if (parsedValue == 0)
        {
            handler->SendSysMessage("经验数量必须大于0。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 限制最大经验值为1亿，防止异常值
        constexpr uint32 MAX_EXP_AMOUNT = 100000000;
        if (parsedValue > MAX_EXP_AMOUNT)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("经验数量过大，最大允许值为%s。", MAX_EXP_AMOUNT).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        expAmount = static_cast<uint32>(parsedValue);
    }

    // 获取当前装备的主手物品
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!item)
    {
        handler->SendSysMessage("你需要装备一件物品才能添加经验。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 添加经验
    if (sItemGrowthMgr->AddItemExp(player, item, expAmount))
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("成功为物品添加%s点经验！", expAmount).c_str());

        // 立即通知客户端更新
        SendGrowthUpdateToClient(handler, player, item);
        return true;
    }
    else
    {
        handler->SendSysMessage("添加经验失败。");
        handler->SetSentErrorMessage(true);
        return false;
    }
}

bool ItemGrowthCommands::HandleItemGrowthReloadCommand(ChatHandler* handler, char const* args)
{
    handler->SendSysMessage("正在重新加载物品成长系统数据...");

    // 重新加载物品成长模板
    sItemGrowthMgr->LoadItemGrowthTemplates();

    handler->SendSysMessage("物品成长系统数据重新加载完成。");
    return true;
}

bool ItemGrowthCommands::HandleItemGrowthSetLevelCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 【审计修复】解析等级参数，增加范围检查防止溢出
    if (!args || !*args)
    {
        handler->SendSysMessage("请指定等级。用法：.物品成长 设等级 [等级]");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 使用 strtoul 进行安全解析
    char* endPtr = nullptr;
    unsigned long parsedValue = strtoul(args, &endPtr, 10);

    // 检查解析是否成功（跳过前导空格）
    const char* trimmedArgs = args;
    while (*trimmedArgs == ' ') trimmedArgs++;
    if (endPtr == trimmedArgs)
    {
        handler->SendSysMessage("无效的等级，请输入正整数。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查范围
    uint32 targetLevel = static_cast<uint32>(parsedValue);
    if (targetLevel == 0 || targetLevel > sItemGrowthMgr->GetMaxLevel())
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("无效的等级。有效范围：1-%s", sItemGrowthMgr->GetMaxLevel()).c_str());
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 获取当前装备的主手物品
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!item)
    {
        handler->SendSysMessage("你需要装备一件物品才能设置等级。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

    if (!record)
    {
        // 创建新记录
        sItemGrowthMgr->CreatePlayerItemRecord(player, item);
        record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);
        if (!record)
        {
            handler->SendSysMessage("无法创建物品成长记录。");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // 验证目标等级是否有对应的模板
    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("HandleItemGrowthSetLevelCommand: 验证等级 %s 属性组 %s 的模板可用性", targetLevel, record->itemPropertyGroup).c_str());

    // 检查从1级到目标等级的所有模板是否都存在
    for (uint32 checkLevel = 1; checkLevel <= targetLevel; ++checkLevel)
    {
        ItemGrowthTemplate const* checkTemplate = sItemGrowthMgr->GetItemGrowthTemplateByLevel(checkLevel, record->itemPropertyGroup);
        if (!checkTemplate)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("错误：属性组 %s 缺少等级 %s 的模板配置，无法设置到等级 %s", record->itemPropertyGroup, checkLevel, targetLevel).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("请使用 .物品成长 调试模板 %s 命令查看该属性组的配置", record->itemPropertyGroup).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // 设置等级
    uint32 oldLevel = record->currentLevel;
    record->currentLevel = targetLevel;
    record->currentExp = 0; // 重置经验

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("HandleItemGrowthSetLevelCommand: 物品GUID %s 等级从 %s 设置为 %s (属性组 %s)", record->itemGuid, oldLevel, targetLevel, record->itemPropertyGroup).c_str());

    // 重新计算属性
    record->currentAttributeValue = 0;
    record->currentAttributePercent = 0;
    record->growthAttributes = ""; // 重置成长属性

    // 应用所有等级的属性加成
    std::vector<std::pair<uint32, AsInt64>> totalAttributes;

    for (uint32 level = 1; level <= targetLevel; ++level)
    {
        ItemGrowthTemplate const* template_ = sItemGrowthMgr->GetItemGrowthTemplateByLevel(level, record->itemPropertyGroup);
        if (template_)
        {
            std::vector<std::pair<uint32, AsInt64>> levelAttributes;

            if (template_->attributeDistributionType == 0)
            {
                // 平均分配
                if (template_->levelRewardAttribute > 0)
                {
                    levelAttributes.push_back(std::make_pair(3, template_->levelRewardAttribute)); // 耐力
                }
                if (template_->levelRewardAttributePercent > 0)
                {
                    levelAttributes.push_back(std::make_pair(7, template_->levelRewardAttributePercent)); // 法力值
                }

                // 兼容性
                record->currentAttributeValue += template_->levelRewardAttribute;
                record->currentAttributePercent += template_->levelRewardAttributePercent;
            }
            else
            {
                // 随机分配（使用平均值）
                AsInt64 avgAttributeValue = (template_->minAttributeValue + template_->maxAttributeValue) / 2;
                AsInt64 avgAttributePercent = (template_->minAttributePercent + template_->maxAttributePercent) / 2;

                if (avgAttributeValue > 0)
                {
                    // 使用固定属性类型进行测试
                    levelAttributes.push_back(std::make_pair(1, avgAttributeValue)); // 力量
                }
                if (avgAttributePercent > 0)
                {
                    levelAttributes.push_back(std::make_pair(6, avgAttributePercent)); // 生命值
                }

                // 兼容性
                record->currentAttributeValue += avgAttributeValue;
                record->currentAttributePercent += avgAttributePercent;
            }

            // 合并属性到总属性中
            for (const auto& attr : levelAttributes)
            {
                bool found = false;
                for (auto& totalAttr : totalAttributes)
                {
                    if (totalAttr.first == attr.first)
                    {
                        totalAttr.second += attr.second;
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    totalAttributes.push_back(attr);
                }
            }
        }
    }

    // 更新成长属性字符串
    if (!totalAttributes.empty())
    {
        record->growthAttributes = sItemGrowthMgr->FormatGrowthAttributes(totalAttributes);
    }

    // 更新升级经验需求
    sItemGrowthMgr->UpdateRequiredExp(record);

    // 保存记录
    sItemGrowthMgr->SavePlayerItemRecord(record);

    // 应用属性
    sItemGrowthMgr->ApplyItemAttributes(player, item);

    handler->PSendSysMessage("%s", ModuleOldFormat("成功将物品等级设置为%s级！", targetLevel).c_str());

    // 立即通知客户端更新
    SendGrowthUpdateToClient(handler, player, item);
    return true;
}

bool ItemGrowthCommands::HandleItemGrowthResetCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 获取当前装备的主手物品
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!item)
    {
        handler->SendSysMessage("你需要装备一件物品才能重置。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

    if (!record)
    {
        handler->SendSysMessage("该物品尚未激活成长系统。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 重置所有数据
    record->currentLevel = 1; // 修复：重置后等级应该是1，而不是0
    record->currentExp = 0;
    record->growthCount = 0;
    record->failCount = 0;
    record->currentAttributeValue = 0;
    record->currentAttributePercent = 0;
    record->growthAttributes = ""; // 重置成长属性

    // 保存记录
    sItemGrowthMgr->SavePlayerItemRecord(record);

    handler->SendSysMessage("物品成长数据已重置！");
    return true;
}

bool ItemGrowthCommands::HandleItemGrowthListCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    handler->SendSysMessage("=== 物品成长模板列表 ===");

    // 获取所有模板
    auto& templates = sItemGrowthMgr->GetItemGrowthTemplates();
    if (templates.empty())
    {
        handler->SendSysMessage("没有找到任何成长模板。");
        return true;
    }

    handler->PSendSysMessage("%s", ModuleOldFormat("共找到%s个成长模板：", templates.size()).c_str());
    handler->SendSysMessage("ID | 组 | 等级 | 经验 | 属性组 | 分配方式 | 属性值范围 | 百分比范围");
    handler->SendSysMessage("---|---|------|------|--------|----------|------------|------------");

    // 按组和等级排序显示
    std::map<uint32, std::map<uint32, const ItemGrowthTemplate*>> sortedTemplates;
    for (auto const& pair : templates)
    {
        const ItemGrowthTemplate& tmpl = pair.second;
        sortedTemplates[tmpl.group][tmpl.level] = &tmpl;
    }

    for (auto const& groupPair : sortedTemplates)
    {
        uint32 group = groupPair.first;
        handler->PSendSysMessage("%s", ModuleOldFormat("=== 属性组 %s ===", group).c_str());

        for (auto const& levelPair : groupPair.second)
        {
            const ItemGrowthTemplate& tmpl = *levelPair.second;
            std::string distributionType = tmpl.attributeDistributionType == 0 ? "平均" : "随机";

            handler->PSendSysMessage("%s", ModuleOldFormat("%s | %s | %s | %s | %s | %s | %s-%s     | %s%%-%s%%", tmpl.id, tmpl.group, tmpl.level, tmpl.requiredExp, tmpl.itemPropertyGroup, distributionType, Acore::ToString(tmpl.minAttributeValue), Acore::ToString(tmpl.maxAttributeValue), Acore::ToString(tmpl.minAttributePercent), Acore::ToString(tmpl.maxAttributePercent)).c_str());
        }
    }

    handler->SendSysMessage("=== 列表结束 ===");
    return true;
}

bool ItemGrowthCommands::HandleItemGrowthStatsCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    handler->SendSysMessage("=== 物品成长系统统计 ===");

    // 系统统计
    handler->PSendSysMessage("%s", ModuleOldFormat("模板数量：%s", sItemGrowthMgr->GetTemplateCount()).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("玩家记录数量：%s", sItemGrowthMgr->GetPlayerRecordCount()).c_str());

    // 分析玩家记录
    auto& records = sItemGrowthMgr->GetPlayerItemRecords();
    if (!records.empty())
    {
        uint32 totalGrowthCount = 0;
        uint32 totalFailCount = 0;
        uint32 maxLevel = 0;
        AsInt64 totalAttributeValue = 0;
        AsInt64 totalAttributePercent = 0;

        for (auto const& pair : records)
        {
            ItemGrowthPlayerRecord const& record = pair.second;
            totalGrowthCount += record.growthCount;
            totalFailCount += record.failCount;
            maxLevel = std::max(maxLevel, record.currentLevel);
            totalAttributeValue += record.currentAttributeValue;
            totalAttributePercent += record.currentAttributePercent;
        }

        float successRate = totalGrowthCount > 0 ?
            (float)totalGrowthCount / (totalGrowthCount + totalFailCount) * 100.0f : 0.0f;

        handler->PSendSysMessage("%s", ModuleOldFormat("总成长次数：%s", totalGrowthCount).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("总失败次数：%s", totalFailCount).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("整体成功率：%s%%", successRate).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("最高等级：%s", maxLevel).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("平均属性值：%s", Acore::Number::ToFloat(totalAttributeValue) / records.size()).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("平均属性百分比：%s%%", Acore::Number::ToFloat(totalAttributePercent) / records.size()).c_str());
    }

    handler->SendSysMessage("=== 统计结束 ===");
    return true;
}



bool ItemGrowthCommands::HandleItemGrowthSetGrowableCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 解析参数：属性组 [物品ID]
    if (!args || !*args)
    {
        handler->SendSysMessage("请指定属性组。用法：.物品成长 设置成长 [属性组] [物品ID]");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 解析属性组和可选的物品ID
    char* groupStr = strtok((char*)args, " ");
    char* itemIdStr = strtok(nullptr, " ");

    uint32 propertyGroup = atoi(groupStr);
    uint32 itemId = 0;

    if (itemIdStr)
    {
        itemId = atoi(itemIdStr);
        if (itemId == 0)
        {
            handler->SendSysMessage("无效的物品ID。");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // 允许设置0组（官方装备成长）
    // 不再限制属性组必须大于0

    // 获取目标物品
    Item* item = nullptr;

    if (itemId > 0)
    {
        // 通过物品ID查找玩家背包中的物品
        item = player->GetItemByEntry(itemId);
        if (!item)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("在你的背包中没有找到物品ID为%s的物品。", itemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        // 使用当前装备的主手物品
        item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        if (!item)
        {
            handler->SendSysMessage("你需要装备一件物品或指定物品ID才能设置成长。");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // 检查物品基本条件
    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
    {
        handler->SendSysMessage("无效的物品。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查物品类型
    if (itemTemplate->Class != ITEM_CLASS_WEAPON &&
        itemTemplate->Class != ITEM_CLASS_ARMOR &&
        itemTemplate->Class != ITEM_CLASS_MISC)
    {
        handler->SendSysMessage("只有武器、护甲和饰品可以设置成长。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查物品品质
    if (itemTemplate->Quality < ITEM_QUALITY_NORMAL)
    {
        handler->SendSysMessage("只有普通及以上品质的物品可以设置成长。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查是否有对应属性组的模板
    ItemGrowthTemplate const* template1 = sItemGrowthMgr->GetItemGrowthTemplateByLevel(1, propertyGroup);
    if (!template1)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("属性组%s没有对应的成长模板。", propertyGroup).c_str());
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 设置物品可以成长
    sItemGrowthMgr->SetItemCanGrow(player, item, propertyGroup);

    handler->PSendSysMessage("%s", ModuleOldFormat("成功设置物品 %s (ID:%s) 为可成长，属性组：%s", item->GetTemplate()->Name1, item->GetEntry(), propertyGroup).c_str());

    // 关键修复：立即发送ITEMGROWTH更新消息给客户端
    SendGrowthUpdateToClient(handler, player, item);

    return true;
}

// 辅助函数：发送成长属性更新通知给客户端
// 这样客户端可以立即看到成长属性，而不需要等待60秒查询间隔或重新登录
void ItemGrowthCommands::SendGrowthUpdateToClient(ChatHandler* handler, Player* player, Item* item)
{
    if (!handler || !player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

    if (!record)
        return;

    // 计算装备位置信息
    int32 outBag = -1;
    int32 outSlot = -1;
    int32 outEquip = item->IsEquipped() ? 1 : 0;

    if (item->IsEquipped())
    {
        outSlot = static_cast<int32>(item->GetSlot()) + 1;
    }
    else
    {
        uint8 bagSlot = item->GetBagSlot();
        if (bagSlot == INVENTORY_SLOT_BAG_0)
            outBag = 0;
        else if (bagSlot >= INVENTORY_SLOT_BAG_START && bagSlot < INVENTORY_SLOT_BAG_END)
            outBag = static_cast<int32>(bagSlot - INVENTORY_SLOT_BAG_START + 1);
        outSlot = static_cast<int32>(item->GetSlot()) + 1;
    }

    // 构造ITEMGROWTH响应消息，立即通知客户端更新
    std::string updateMsg = "ITEMGROWTH:" +
                           Acore::ToString(item->GetEntry()) + ":" +
                           Acore::ToString(itemGuid) + ":" +
                           Acore::ToString(record->currentLevel) + ":" +
                           Acore::ToString(record->currentExp) + ":" +
                           Acore::ToString(record->requiredExp) + ":" +
                           record->growthAttributes + ":" +
                           record->percentAttributes + ":" +
                           Acore::ToString(itemGuid) + ":" +
                           Acore::ToString(outBag) + ":" +
                           Acore::ToString(outSlot) + ":" +
                           Acore::ToString(outEquip);

    handler->PSendSysMessage(updateMsg.c_str());
}

bool ItemGrowthCommands::HandleItemGrowthAttributeInfoCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 获取当前装备的主手物品
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!item)
    {
        handler->SendSysMessage("你需要装备一件物品才能查看属性详情。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

    if (!record)
    {
        handler->SendSysMessage("该物品尚未激活成长系统。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    handler->PSendSysMessage("%s", ModuleOldFormat("=== %s 成长属性详情 ===", item->GetTemplate()->Name1).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("物品等级：%s", record->currentLevel).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("属性组：%s", record->itemPropertyGroup).c_str());

    // 显示成长属性
    if (!record->growthAttributes.empty())
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("成长属性字符串：%s", record->growthAttributes).c_str());

        // 解析并显示详细属性
        std::vector<std::pair<uint32, AsInt64>> attributes;
        sItemGrowthMgr->ParseGrowthAttributes(record->growthAttributes, attributes);

        if (!attributes.empty())
        {
            handler->SendSysMessage("=== 详细属性列表 ===");
            for (const auto& attr : attributes)
            {
                std::string attrName = GetAttributeName(attr.first);
                handler->PSendSysMessage("%s", ModuleOldFormat("属性类型 %s (%s): +%s", attr.first, attrName, Acore::ToString(attr.second)).c_str());
            }
        }
    }
    else
    {
        handler->SendSysMessage("该物品暂无成长属性。");
    }

    // 显示属性类型说明
    handler->SendSysMessage("=== 属性类型说明 ===");
    handler->SendSysMessage("基础属性: 0=法力值, 1=生命值, 3=敏捷, 4=力量, 5=智力, 6=精神, 7=耐力");
    handler->SendSysMessage("防御等级: 12=防御技能等级, 13=躲闪等级, 14=招架等级, 15=格挡等级");
    handler->SendSysMessage("命中等级: 16=近战命中, 17=远程命中, 18=法术命中, 31=命中等级");
    handler->SendSysMessage("暴击等级: 19=近战暴击, 20=远程暴击, 21=法术暴击, 32=暴击等级");
    handler->SendSysMessage("急速等级: 28=近战急速, 29=远程急速, 30=法术急速, 36=急速等级");
    handler->SendSysMessage("其他等级: 35=韧性等级, 37=精通等级, 44=护甲穿透等级");
    handler->SendSysMessage("攻击属性: 38=攻击强度, 39=远程攻击强度, 45=法术强度, 47=法术穿透");
    handler->SendSysMessage("回复属性: 43=法力回复, 46=生命回复, 48=格挡值");
    handler->SendSysMessage("注意: 属性类型基于AzerothCore ItemModType枚举定义");

    handler->SendSysMessage("=== 属性详情结束 ===");
    return true;
}

bool ItemGrowthCommands::HandleItemGrowthSetGroupCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 解析参数：属性组 物品ID
    if (!args || !*args)
    {
        handler->SendSysMessage("请指定属性组和物品ID。用法：.物品成长 设置组 [属性组] [物品ID]");
        handler->SendSysMessage("例如：.物品成长 设置组 1 25  (给物品25设置组1)");
        handler->SetSentErrorMessage(true);
        return false;
    }

    char* groupStr = strtok((char*)args, " ");
    char* itemIdStr = strtok(nullptr, " ");

    if (!groupStr || !itemIdStr)
    {
        handler->SendSysMessage("请同时指定属性组和物品ID。用法：.物品成长 设置组 [属性组] [物品ID]");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 propertyGroup = atoi(groupStr);
    uint32 itemId = atoi(itemIdStr);

    if (itemId == 0)
    {
        handler->SendSysMessage("无效的物品ID。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 查找玩家背包中的物品
    Item* item = player->GetItemByEntry(itemId);
    if (!item)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("在你的背包中没有找到物品ID为%s的物品。", itemId).c_str());
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查物品基本条件
    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
    {
        handler->SendSysMessage("无效的物品。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查物品类型
    if (itemTemplate->Class != ITEM_CLASS_WEAPON &&
        itemTemplate->Class != ITEM_CLASS_ARMOR &&
        itemTemplate->Class != ITEM_CLASS_MISC)
    {
        handler->SendSysMessage("只有武器、护甲和饰品可以设置成长。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查物品品质
    if (itemTemplate->Quality < ITEM_QUALITY_NORMAL)
    {
        handler->SendSysMessage("只有普通及以上品质的物品可以设置成长。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查是否有对应属性组的模板
    ItemGrowthTemplate const* template1 = sItemGrowthMgr->GetItemGrowthTemplateByLevel(1, propertyGroup);
    if (!template1)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("属性组%s没有对应的成长模板。", propertyGroup).c_str());
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 在设置成长组之前，先检查并消耗需求模板的资源
    if (template1->requirementTemplateId > 0)
    {
        bool requirementMet = CheckNeedTemplate(player, template1->requirementTemplateId, true);
        if (!requirementMet)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("不满足需求模板%s的条件，无法设置成长组。", template1->requirementTemplateId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        bool consumed = ConsumeNeedTemplate(player, template1->requirementTemplateId, "装备成长设置组需求消耗");
        if (!consumed)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("消耗需求模板%s的资源失败，无法设置成长组。", template1->requirementTemplateId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        handler->PSendSysMessage("%s", ModuleOldFormat("已消耗需求模板%s的资源（金币等）", template1->requirementTemplateId).c_str());
    }

    // 设置组命令专用逻辑：只调用存储函数，不执行完整的SetItemCanGrow逻辑
    uint32 itemGuid = item->GetGUID().GetCounter();

    // 不再将GUID写入随机附魔字段，避免与随机附魔系统冲突
    // 仅进行必要的可见槽位刷新（若已装备）
    if (item->IsEquipped())
        player->SetVisibleItemSlot(item->GetSlot(), item);

    // 创建或更新物品成长记录
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);
    if (!record)
    {
        sItemGrowthMgr->CreatePlayerItemRecord(player, item, propertyGroup);
        record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);
    }

    if (record)
    {
        // 设置属性组
        record->itemPropertyGroup = propertyGroup;

        // 根据属性组设置初始升级经验（查找等级1的模板）
        ItemGrowthTemplate const* template1 = sItemGrowthMgr->GetItemGrowthTemplateByLevel(1, propertyGroup);
        if (template1)
        {
            record->requiredExp = template1->requiredExp;

            // 为1级生成初始属性（如果还没有属性的话）
            if (record->growthAttributes.empty())
            {
                sItemGrowthMgr->GenerateLevelUpAttributesBasedOnItem(player, item, record, template1);
            }
        }
        else
        {
            // 如果没有找到对应组的等级1模板，说明该组无法成长
            handler->PSendSysMessage("%s", ModuleOldFormat("错误：属性组 %s 没有配置等级1的成长模板，物品无法成长", propertyGroup).c_str());
            return false;
        }

        // 保存记录到数据库
        sItemGrowthMgr->SavePlayerItemRecord(record);

        // 如果物品已装备，应用1级属性
        bool isEquipped = false;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (equippedItem && equippedItem->GetGUID().GetCounter() == itemGuid)
            {
                isEquipped = true;
                break;
            }
        }

        if (isEquipped && !record->growthAttributes.empty())
        {
            // 从数据库读取最新数据并应用属性
            sItemGrowthMgr->ApplyGrowthAura(player, itemGuid);
        }
    }

    handler->PSendSysMessage("%s", ModuleOldFormat("成功给物品 %s (ID:%s) 设置属性组：%s%s", item->GetTemplate()->Name1, item->GetEntry(), propertyGroup, propertyGroup == 0 ? "（官方装备成长）" : "").c_str());

    // 立即通知客户端更新
    SendGrowthUpdateToClient(handler, player, item);
    return true;
}

bool ItemGrowthCommands::HandleItemGrowthRandomGroupCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 解析参数：物品ID
    if (!args || !*args)
    {
        handler->SendSysMessage("请指定物品ID。用法：.物品成长 随机组 [物品ID]");
        handler->SendSysMessage("例如：.物品成长 随机组 35  (给物品ID为35的装备随机分配成长组)");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 itemId = atoi(args);
    if (itemId == 0)
    {
        handler->SendSysMessage("无效的物品ID。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 查找玩家背包中的物品
    Item* item = player->GetItemByEntry(itemId);
    if (!item)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("在你的背包中没有找到物品ID为%s的物品。", itemId).c_str());
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查物品是否已经有成长组
    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* existingRecord = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);
    if (existingRecord)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("物品 %s (ID:%s) 已经有成长组，无法重新分配。", item->GetTemplate()->Name1, item->GetEntry()).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("当前成长组：%s 等级：%s", existingRecord->itemPropertyGroup, existingRecord->currentLevel).c_str());
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查物品基本条件
    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
    {
        handler->SendSysMessage("无效的物品。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查物品类型
    if (itemTemplate->Class != ITEM_CLASS_WEAPON &&
        itemTemplate->Class != ITEM_CLASS_ARMOR &&
        itemTemplate->Class != ITEM_CLASS_MISC)
    {
        handler->SendSysMessage("只有武器、护甲和饰品可以设置成长。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 检查物品品质
    if (itemTemplate->Quality < ITEM_QUALITY_NORMAL)
    {
        handler->SendSysMessage("只有普通及以上品质的物品可以设置成长。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 从数据库获取所有可用的成长组及其几率（查询等级1且几率>0的记录，同时获取成长经验）
    std::vector<std::pair<uint32, uint32>> availableGroups; // <组ID, 几率>

    QueryResult result = WorldDatabase.Query("SELECT `组`, `几率`, `需求模板`, `成长经验` FROM `_模板_物品成长` WHERE `等级` = 1 AND `几率` > 0");
    if (!result)
    {
        handler->SendSysMessage("数据库中没有找到可用的成长组配置。");
        handler->SendSysMessage("请确保数据库中有等级1且几率>0的记录。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 totalWeight = 0;
    uint32 processedGroups = 0;
    uint32 validGroups = 0;
    uint32 requirementFailedGroups = 0;
    uint32 expRequirementFailedGroups = 0;

    do
    {
        Field* fields = result->Fetch();
        uint32 groupId = fields[0].Get<uint32>();
        uint32 probability = fields[1].Get<uint32>();
        uint32 requirementTemplateId = fields[2].Get<uint32>();
        uint32 requiredExp = fields[3].Get<uint32>();
        processedGroups++;

        // 检查需求模板
        bool requirementMet = true;
        if (requirementTemplateId > 0)
        {
            requirementMet = CheckNeedTemplate(player, requirementTemplateId, false);
            if (!requirementMet)
            {
                requirementFailedGroups++;
            }
        }

        // 检查成长经验要求
        bool expRequirementMet = true;
        if (requiredExp > 0)
        {
            // 这里可以添加玩家经验检查逻辑
            // 目前暂时允许所有有成长经验要求的组
            // 如果需要检查玩家是否有足够经验，可以在这里添加逻辑
            expRequirementMet = true;
        }

        if (!requirementMet)
            continue;

        if (!expRequirementMet)
        {
            expRequirementFailedGroups++;
            continue;
        }

        // 验证该组是否有完整的模板配置
        ItemGrowthTemplate const* template1 = sItemGrowthMgr->GetItemGrowthTemplateByLevel(1, groupId);
        if (template1)
        {
            // 数据库查询已经保证了几率>0，直接添加
            availableGroups.push_back(std::make_pair(groupId, probability));
            totalWeight += probability;
            validGroups++;
        }
        else
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("警告：组%s没有对应的等级1模板，跳过", groupId).c_str());
        }
    } while (result->NextRow());

    // 如果没有可用组，显示具体的需求信息
    if (availableGroups.empty() || totalWeight == 0)
    {
        if (requirementFailedGroups > 0)
        {
            handler->SendSysMessage("当前角色不满足以下成长组的需求条件：");

            // 重新查询并显示具体需求（包括成长经验要求）
            QueryResult detailResult = WorldDatabase.Query("SELECT `组`, `需求模板`, `成长经验` FROM `_模板_物品成长` WHERE `等级` = 1 AND `几率` > 0 AND (`需求模板` > 0 OR `成长经验` > 0)");
            if (detailResult)
            {
                do
                {
                    Field* fields = detailResult->Fetch();
                    uint32 groupId = fields[0].Get<uint32>();
                    uint32 requirementTemplateId = fields[1].Get<uint32>();
                    uint32 requiredExp = fields[2].Get<uint32>();

                    // 显示成长组和需求详情
                    handler->PSendSysMessage("%s", ModuleOldFormat("成长组%s: ", groupId).c_str());

                    // 显示成长经验要求
                    if (requiredExp > 0)
                    {
                        handler->PSendSysMessage("%s", ModuleOldFormat("  需要成长经验: %s 点", requiredExp).c_str());
                    }

                    if (requirementTemplateId > 0)
                    {
                        if (HasNeedTemplate(requirementTemplateId))
                            handler->PSendSysMessage("%s", ModuleOldFormat("%s", sHelper->GetNeedGts(requirementTemplateId)).c_str());
                        else
                            handler->PSendSysMessage("%s", ModuleOldFormat("  需求模板ID %s (__模版需求总表 未找到)", requirementTemplateId).c_str());
                    }

                    handler->SendSysMessage(""); // 空行分隔
                } while (detailResult->NextRow());
            }
        }
        else
        {
            handler->SendSysMessage("没有找到可用的成长组配置。");
        }
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 额外的安全检查，确保totalWeight > 0
    if (totalWeight == 0)
    {
        handler->SendSysMessage("错误：总权重为0，无法进行随机选择。");
        handler->SendSysMessage("这通常表示数据库配置有问题或模板缺失。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 基于权重随机选择成长组
    uint32 randomValue = urand(1, totalWeight);
    uint32 currentWeight = 0;
    uint32 selectedGroup = 0;

    for (const auto& groupPair : availableGroups)
    {
        currentWeight += groupPair.second;
        if (randomValue <= currentWeight)
        {
            selectedGroup = groupPair.first;
            break;
        }
    }

    // 检查是否有对应属性组的模板
    ItemGrowthTemplate const* template1 = sItemGrowthMgr->GetItemGrowthTemplateByLevel(1, selectedGroup);
    if (!template1)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("属性组%s没有对应的成长模板。", selectedGroup).c_str());
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 在分配成长组之前，先消耗需求模板的资源
    if (template1->requirementTemplateId > 0)
    {
        bool consumed = ConsumeNeedTemplate(player, template1->requirementTemplateId, "装备成长随机组需求消耗");
        if (!consumed)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("消耗需求模板%s的资源失败，无法分配成长组。", template1->requirementTemplateId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
        handler->PSendSysMessage("%s", ModuleOldFormat("已消耗需求模板%s的资源（金币等）", template1->requirementTemplateId).c_str());
    }

    // 随机组命令专用逻辑：只调用存储函数，不执行完整的SetItemCanGrow逻辑
    // itemGuid已在上面定义，这里直接使用

    // 不再将GUID写入随机附魔字段，避免与随机附魔系统冲突
    if (item->IsEquipped())
        player->SetVisibleItemSlot(item->GetSlot(), item);

    // 创建或更新物品成长记录
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);
    if (!record)
    {
        sItemGrowthMgr->CreatePlayerItemRecord(player, item, selectedGroup);
        record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);
    }

    if (record)
    {
        // 设置属性组
        record->itemPropertyGroup = selectedGroup;

        // 设置初始等级为1
        record->currentLevel = 1;
        record->currentExp = 0;

        // 根据当前等级设置正确的升级经验（查找等级2的模板作为升级目标）
        ItemGrowthTemplate const* nextTemplate = sItemGrowthMgr->GetItemGrowthTemplateByLevel(2, selectedGroup);
        if (nextTemplate)
        {
            record->requiredExp = nextTemplate->requiredExp;
        }
        else
        {
            // 如果没有等级2的模板，说明这个组只有1级，设置为0表示无法升级
            record->requiredExp = 0;
        }

        // 保存记录到数据库
        sItemGrowthMgr->SavePlayerItemRecord(record);
    }

    // 计算选中几率
    uint32 selectedWeight = 0;
    for (const auto& groupPair : availableGroups)
    {
        if (groupPair.first == selectedGroup)
        {
            selectedWeight = groupPair.second;
            break;
        }
    }
    float selectedProbability = (float)selectedWeight / totalWeight * 100.0f;

    handler->PSendSysMessage("%s", ModuleOldFormat("成功给物品 %s (ID:%s) 设置属性组：%s", item->GetTemplate()->Name1, item->GetEntry(), selectedGroup).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("随机结果：组%s (几率: %s%%)", selectedGroup, selectedProbability).c_str());

    // 立即通知客户端更新
    SendGrowthUpdateToClient(handler, player, item);

    // 显示需求模板信息
    ItemGrowthTemplate const* selectedTemplate = sItemGrowthMgr->GetItemGrowthTemplateByLevel(1, selectedGroup);
    if (selectedTemplate && selectedTemplate->requirementTemplateId > 0)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("该组需求模板ID：%s (已满足)", selectedTemplate->requirementTemplateId).c_str());
    }
    else
    {
        handler->SendSysMessage("该组无需求限制");
    }

    // 显示随机结果
    handler->SendSysMessage("可用成长组几率分布：");
    for (const auto& groupPair : availableGroups)
    {
        float probability = (float)groupPair.second / totalWeight * 100.0f;
        std::string groupName;
        switch (groupPair.first)
        {
            case 1: groupName = "基础成长"; break;
            case 2: groupName = "中级成长"; break;
            case 3: groupName = "高级成长"; break;
            case 4: groupName = "极品成长"; break;
            default: groupName = "自定义成长组"; break;
        }
        handler->PSendSysMessage("%s", ModuleOldFormat("  组%s (%s): %s%%", groupPair.first, groupName, probability).c_str());
    }

    return true;
}


bool ItemGrowthCommands::HandleItemGrowthDebugTemplatesCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 解析属性组参数
    uint32 targetGroup = 0;
    if (args && *args)
    {
        targetGroup = atoi(args);
    }

    handler->PSendSysMessage("=== 物品成长模板调试信息 ===");
    handler->PSendSysMessage("%s", ModuleOldFormat("查询属性组：%s (0表示显示所有组)", targetGroup).c_str());

    // 获取所有模板
    auto& templates = sItemGrowthMgr->GetItemGrowthTemplates();
    if (templates.empty())
    {
        handler->SendSysMessage("没有找到任何成长模板！请检查数据库配置。");
        return true;
    }

    handler->PSendSysMessage("%s", ModuleOldFormat("数据库中共有%s个模板", templates.size()).c_str());
    handler->SendSysMessage("ID | 需求 | 组 | 等级 | 几率 | 经验 | 属性组 | 分配 | 属性值范围 | 百分比范围 | 奖励属性 | 奖励百分比");
    handler->SendSysMessage("---|------|---|------|------|------|--------|------|------------|------------|----------|----------");

    // 按组和等级排序显示
    std::map<uint32, std::map<uint32, const ItemGrowthTemplate*>> sortedTemplates;
    for (auto const& pair : templates)
    {
        const ItemGrowthTemplate& tmpl = pair.second;
        // 如果指定了属性组，只显示该组的模板
        if (args && *args && tmpl.group != targetGroup)
            continue;

        sortedTemplates[tmpl.group][tmpl.level] = &tmpl;
    }

    if (sortedTemplates.empty())
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("没有找到属性组%s的模板配置！", targetGroup).c_str());
        return true;
    }

    for (auto const& groupPair : sortedTemplates)
    {
        uint32 group = groupPair.first;
        std::string groupName;
        switch (group)
        {
            case 1: groupName = "基础成长"; break;
            case 2: groupName = "中级成长"; break;
            case 3: groupName = "高级成长"; break;
            case 4: groupName = "极品成长"; break;
            default: groupName = "自定义成长组"; break;
        }
        handler->PSendSysMessage("%s", ModuleOldFormat("=== 属性组 %s (%s) ===", group, groupName).c_str());

        for (auto const& levelPair : groupPair.second)
        {
            const ItemGrowthTemplate& tmpl = *levelPair.second;
            std::string distributionType = tmpl.attributeDistributionType == 0 ? "平均" : "随机";

            // 只在等级1时显示几率，其他等级显示"-"
            std::string probabilityStr = (tmpl.level == 1) ? Acore::ToString(tmpl.probability) : "-";

            // 需求模板显示
            std::string requirementStr = (tmpl.requirementTemplateId > 0) ? Acore::ToString(tmpl.requirementTemplateId) : "-";

            handler->PSendSysMessage("%s", ModuleOldFormat("%s | %s | %s | %s | %s | %s | %s | %s | %s-%s     | %s%%-%s%%   | %s | %s", tmpl.id, requirementStr, tmpl.group, tmpl.level, probabilityStr, tmpl.requiredExp, tmpl.itemPropertyGroup, distributionType, Acore::ToString(tmpl.minAttributeValue), Acore::ToString(tmpl.maxAttributeValue), Acore::ToString(tmpl.minAttributePercent), Acore::ToString(tmpl.maxAttributePercent), Acore::ToString(tmpl.levelRewardAttribute), Acore::ToString(tmpl.levelRewardAttributePercent)).c_str());
        }
    }

    // 如果查询特定属性组，显示缺失的等级
    if (args && *args && sortedTemplates.find(targetGroup) != sortedTemplates.end())
    {
        auto& groupTemplates = sortedTemplates[targetGroup];
        handler->PSendSysMessage("%s", ModuleOldFormat("属性组%s的等级范围：%s-%s", targetGroup, groupTemplates.begin()->first, groupTemplates.rbegin()->first).c_str());

        // 检查是否有缺失的等级
        std::vector<uint32> missingLevels;
        uint32 minLevel = groupTemplates.begin()->first;
        uint32 maxLevel = groupTemplates.rbegin()->first;

        for (uint32 level = minLevel; level <= maxLevel; ++level)
        {
            if (groupTemplates.find(level) == groupTemplates.end())
            {
                missingLevels.push_back(level);
            }
        }

        if (!missingLevels.empty())
        {
            std::string missingStr = "";
            for (size_t i = 0; i < missingLevels.size(); ++i)
            {
                if (i > 0) missingStr += ", ";
                missingStr += Acore::ToString(missingLevels[i]);
            }
            handler->PSendSysMessage("%s", ModuleOldFormat("⚠️ 属性组%s缺失等级：%s", targetGroup, missingStr).c_str());
        }
    }

    handler->SendSysMessage("=== 调试信息结束 ===");
    return true;
}

bool ItemGrowthCommands::HandleItemGrowthViewOriginalStatsCommand(ChatHandler* handler, char const* args)
{
    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 获取玩家当前装备的主手武器
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!item)
    {
        handler->SendSysMessage("你需要装备一件物品才能查看其原始属性。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 获取装备的原始模板
    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
    {
        handler->SendSysMessage("无法获取物品模板信息。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    handler->SendSysMessage("=== 装备原始属性信息 ===");
    handler->PSendSysMessage("%s", ModuleOldFormat("物品：%s (ID: %s)", itemTemplate->Name1, item->GetEntry()).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("类型：%s | 子类型：%s | 品质：%s | 等级：%s", itemTemplate->Class, itemTemplate->SubClass, itemTemplate->Quality, itemTemplate->ItemLevel).c_str());

    // 显示基础属性
    handler->SendSysMessage("--- 基础属性 ---");
    bool hasBasicStats = false;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        if (itemTemplate->ItemStat[i].ItemStatType > 0 && itemTemplate->AsItemStatValue[i] != 0)
        {
            std::string attrName = GetAttributeName(itemTemplate->ItemStat[i].ItemStatType);
            handler->PSendSysMessage("%s", ModuleOldFormat("属性槽%s: %s +%s (类型:%s)", i, attrName, Acore::ToString(itemTemplate->AsItemStatValue[i]), itemTemplate->ItemStat[i].ItemStatType).c_str());
            hasBasicStats = true;
        }
    }

    if (!hasBasicStats)
    {
        handler->SendSysMessage("该装备没有基础属性");
    }

    // 显示护甲值
    if (itemTemplate->AsArmor > 0)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("护甲：%s", Acore::ToString(itemTemplate->AsArmor)).c_str());
    }

    // 显示武器伤害
    bool hasWeaponDamage = false;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
    {
        if (itemTemplate->Damage[i].DamageMin > 0 || itemTemplate->Damage[i].DamageMax > 0)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("伤害%s: %s - %s (类型:%s)", i, itemTemplate->Damage[i].DamageMin, itemTemplate->Damage[i].DamageMax, itemTemplate->Damage[i].DamageType).c_str());
            hasWeaponDamage = true;
        }
    }

    if (hasWeaponDamage && itemTemplate->Delay > 0)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("攻击速度：%s | DPS：%s", itemTemplate->Delay / 1000.0f, itemTemplate->getDPS()).c_str());
    }

    // 显示抗性
    bool hasResistances = false;
    if (itemTemplate->HolyRes > 0) { handler->PSendSysMessage("%s", ModuleOldFormat("神圣抗性：%s", itemTemplate->HolyRes).c_str()); hasResistances = true; }
    if (itemTemplate->FireRes > 0) { handler->PSendSysMessage("%s", ModuleOldFormat("火焰抗性：%s", itemTemplate->FireRes).c_str()); hasResistances = true; }
    if (itemTemplate->NatureRes > 0) { handler->PSendSysMessage("%s", ModuleOldFormat("自然抗性：%s", itemTemplate->NatureRes).c_str()); hasResistances = true; }
    if (itemTemplate->FrostRes > 0) { handler->PSendSysMessage("%s", ModuleOldFormat("冰霜抗性：%s", itemTemplate->FrostRes).c_str()); hasResistances = true; }
    if (itemTemplate->ShadowRes > 0) { handler->PSendSysMessage("%s", ModuleOldFormat("暗影抗性：%s", itemTemplate->ShadowRes).c_str()); hasResistances = true; }
    if (itemTemplate->ArcaneRes > 0) { handler->PSendSysMessage("%s", ModuleOldFormat("奥术抗性：%s", itemTemplate->ArcaneRes).c_str()); hasResistances = true; }

    // 显示其他属性
    if (itemTemplate->Block > 0)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("格挡值：%s", itemTemplate->Block).c_str());
    }

    if (itemTemplate->MaxDurability > 0)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("最大耐久度：%s", itemTemplate->MaxDurability).c_str());
    }

    // 成长建议
    handler->SendSysMessage("--- 成长建议 ---");
    if (hasBasicStats)
    {
        handler->SendSysMessage("✓ 该装备有基础属性，适合使用官方装备成长（属性组0）");
        handler->SendSysMessage("  官方成长会基于装备现有的属性类型进行成长");
    }
    else if (itemTemplate->AsArmor > 0 || hasWeaponDamage)
    {
        handler->SendSysMessage("! 该装备没有基础属性，但有护甲/武器伤害");
        handler->SendSysMessage("  建议使用自定义属性组进行成长");
    }
    else
    {
        handler->SendSysMessage("⚠ 该装备可能不适合成长系统");
        handler->SendSysMessage("  这可能是饰品、消耗品或特殊物品");
    }

    return true;
}

// 强制应用当前装备的成长属性到角色
bool ItemGrowthCommands::HandleItemGrowthForceApplyCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    handler->SendSysMessage("=== 强制应用成长属性 ===");

    uint32 totalApplied = 0;
    uint32 totalEquipment = 0;

    // 遍历所有装备槽位
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        totalEquipment++;
        uint32 itemGuid = item->GetGUID().GetCounter();
        int32 storedGuid = item->GetItemRandomPropertyId();

        handler->PSendSysMessage("%s", ModuleOldFormat("槽位 %s - %s (ID:%s) GUID:%s 存储的GUID:%s", slot, item->GetTemplate()->Name1, item->GetEntry(), itemGuid, storedGuid).c_str());

        ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

        if (!record)
        {
            handler->PSendSysMessage("  └─ 无成长记录");
            continue;
        }

        if (record->growthAttributes.empty())
        {
            handler->PSendSysMessage("  └─ 无成长属性");
            continue;
        }

        // 强制应用属性
        handler->PSendSysMessage("%s", ModuleOldFormat("  └─ 应用属性 '%s'", record->growthAttributes).c_str());

        sItemGrowthMgr->RefreshAndApplyGrowthAttributes(player, item, record);
        totalApplied++;
    }

    handler->PSendSysMessage("=== 应用完成 ===");
    handler->PSendSysMessage("%s", ModuleOldFormat("总装备数量: %s", totalEquipment).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("应用属性数量: %s", totalApplied).c_str());

    if (totalApplied > 0)
    {
        handler->SendSysMessage("所有成长属性已强制应用到角色身上！");
        handler->SendSysMessage("请检查角色属性面板确认效果。");
    }
    else
    {
        handler->SendSysMessage("没有找到可应用的成长属性。");
    }

    return true;
}

// 检查当前选中装备的GUID存储状态
bool ItemGrowthCommands::HandleItemGrowthCheckGuidCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 获取主手武器作为示例，或者让用户指定装备槽位
    Item* item = nullptr;

    // 如果有参数，尝试解析槽位号
    if (args && strlen(args) > 0)
    {
        uint32 slot = atoi(args);
        if (slot >= EQUIPMENT_SLOT_START && slot < EQUIPMENT_SLOT_END)
        {
            item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        }
        else
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("无效的装备槽位号: %s。有效范围: %s-%s", slot, EQUIPMENT_SLOT_START, EQUIPMENT_SLOT_END-1).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        // 默认检查主手武器
        item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    }

    if (!item)
    {
        if (args && strlen(args) > 0)
        {
            handler->SendSysMessage("指定槽位没有装备。");
        }
        else
        {
            handler->SendSysMessage("主手没有装备。请指定装备槽位号，例如：.物品成长 检查GUID 15");
        }
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();
    int32 storedGuid = item->GetItemRandomPropertyId();

    handler->SendSysMessage("=== 装备GUID检查 ===");
    handler->PSendSysMessage("%s", ModuleOldFormat("装备名称: %s", item->GetTemplate()->Name1).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("装备ID: %s", item->GetEntry()).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("实际GUID: %s", itemGuid).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("存储的GUID: %s", storedGuid).c_str());

    if (storedGuid == static_cast<int32>(itemGuid))
    {
        handler->SendSysMessage("✓ GUID存储正确");
    }
    else
    {
        handler->SendSysMessage("✗ GUID存储不正确（提示：随机附魔字段已被禁用为GUID存储，不再尝试写入）");
    }

    // 检查是否有成长记录
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);
    if (record)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("成长记录: 等级%s 属性'%s'", record->currentLevel, record->growthAttributes).c_str());
    }
    else
    {
        handler->SendSysMessage("无成长记录");
    }

    // 检查是否可成长
    bool canGrow = sItemGrowthMgr->CanItemGrow(item);
    handler->PSendSysMessage("%s", ModuleOldFormat("可成长状态: %s", canGrow ? "是" : "否").c_str());

    return true;
}

bool ItemGrowthCommands::HandleItemGrowthVerifyAttributesCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 新增：支持通过参数指定任意装备
    // 用法: .物品成长 验证属性 <物品ID> <物品GUID>
    Item* item = nullptr;
    uint32 itemEntry = 0;
    uint32 itemGuid = 0;

    if (args && *args)
    {
        std::istringstream iss(args);
        if (!(iss >> itemEntry >> itemGuid))
        {
            handler->SendSysMessage("用法: .物品成长 验证属性 <物品ID> <物品GUID>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 尝试在玩家身上根据GUID查找该物品（用于显示物品名称，找不到也不影响验证）
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END && !item; ++slot)
        {
            Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (it && it->GetGUID().GetCounter() == itemGuid)
                item = it;
        }
        if (!item)
        {
            for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END && !item; ++slot)
            {
                Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                if (it && it->GetGUID().GetCounter() == itemGuid)
                    item = it;
            }
        }
        if (!item)
        {
            for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END && !item; ++bagSlot)
            {
                Bag* bag = player->GetBagByPos(bagSlot);
                if (!bag)
                    continue;

                for (uint32 inner = 0; inner < bag->GetBagSize(); ++inner)
                {
                    Item* it = bag->GetItemByPos(inner);
                    if (it && it->GetGUID().GetCounter() == itemGuid)
                    {
                        item = it;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        // 兼容旧用法：默认使用当前主手武器
        item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        if (!item)
        {
            handler->SendSysMessage("你需要装备一件物品才能验证属性，或使用 .物品成长 验证属性 <物品ID> <物品GUID> 指定装备。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        itemEntry = item->GetEntry();
        itemGuid = item->GetGUID().GetCounter();
    }

    // 如果通过参数指定了物品ID但没找到 Item 对象，仍然可以通过成长记录进行验证
    if (!itemEntry && item)
        itemEntry = item->GetEntry();

    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

    if (!record)
    {
        handler->SendSysMessage("该物品尚未激活成长系统。");
        handler->SendSysMessage("使用 .物品成长 升级 命令激活成长系统。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    if (record->growthAttributes.empty())
    {
        handler->SendSysMessage("该物品没有成长属性需要验证。");
        return true;
    }

    // 解析物品名称（优先使用实例，其次使用模板）
    std::string itemName = "未知物品";
    if (item)
    {
        if (ItemTemplate const* proto = item->GetTemplate())
            itemName = proto->Name1;
    }
    else if (itemEntry)
    {
        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemEntry))
            itemName = proto->Name1;
    }

    handler->PSendSysMessage("%s", ModuleOldFormat("正在验证物品 %s (ID:%s GUID:%s) 的成长属性应用情况...", itemName.c_str(), itemEntry, itemGuid).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("物品等级: %s", record->currentLevel).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("成长属性: %s", record->growthAttributes).c_str());

    // 调用验证函数
    sItemGrowthMgr->VerifyAttributesApplied(player, record->growthAttributes);

    return true;
}



bool ItemGrowthCommands::HandleItemGrowthTestAuraCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 获取玩家当前装备的主手武器
    Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!item)
    {
        handler->SendSysMessage("你需要装备一件物品才能测试Aura系统。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);

    if (!record)
    {
        handler->SendSysMessage("该物品尚未激活成长系统。");
        handler->SendSysMessage("使用 .物品成长 升级 命令激活成长系统。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    if (record->growthAttributes.empty())
    {
        handler->SendSysMessage("该物品没有成长属性可以测试。");
        return true;
    }

    handler->PSendSysMessage("%s", ModuleOldFormat("正在测试物品 %s 的Aura系统...", item->GetTemplate()->Name1).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("物品等级: %s", record->currentLevel).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("成长属性: %s", record->growthAttributes).c_str());

    // 记录应用前的属性值
    int32 oldStamina = player->GetStat(STAT_STAMINA);
    int32 oldIntellect = player->GetStat(STAT_INTELLECT);
    int32 oldStrength = player->GetStat(STAT_STRENGTH);
    int32 oldAgility = player->GetStat(STAT_AGILITY);
    int32 oldSpirit = player->GetStat(STAT_SPIRIT);

    handler->PSendSysMessage("%s", ModuleOldFormat("应用前属性: 耐力:%s 智力:%s 力量:%s 敏捷:%s 精神:%s", oldStamina, oldIntellect, oldStrength, oldAgility, oldSpirit).c_str());

    // 测试新的Aura系统
    handler->SendSysMessage("正在应用新的Aura系统...");
    sItemGrowthMgr->ApplyGrowthAura(player, itemGuid);

    // 记录应用后的属性值
    int32 newStamina = player->GetStat(STAT_STAMINA);
    int32 newIntellect = player->GetStat(STAT_INTELLECT);
    int32 newStrength = player->GetStat(STAT_STRENGTH);
    int32 newAgility = player->GetStat(STAT_AGILITY);
    int32 newSpirit = player->GetStat(STAT_SPIRIT);

    handler->PSendSysMessage("%s", ModuleOldFormat("应用后属性: 耐力:%s 智力:%s 力量:%s 敏捷:%s 精神:%s", newStamina, newIntellect, newStrength, newAgility, newSpirit).c_str());

    // 显示变化
    handler->PSendSysMessage("%s", ModuleOldFormat("属性变化: 耐力:%s 智力:%s 力量:%s 敏捷:%s 精神:%s", newStamina - oldStamina, newIntellect - oldIntellect, newStrength - oldStrength, newAgility - oldAgility, newSpirit - oldSpirit).c_str());

    handler->SendSysMessage("Aura系统测试完成！");

    return true;
}

// 基于玩家位置(背包/槽位/装备位)查找物品（当客户端无法提供GUID时使用）
Item* FindPlayerItemByPosition(Player* player, uint32 itemId, int32 bag, int32 slot, bool isEquip)
{
    if (!player)
        return nullptr;

    // 装备位
    if (isEquip)
    {
        // 客户端装备槽是1-based，服务器是0-based
        int32 clientSlot = slot;
        uint8 serverSlot = (clientSlot > 0) ? static_cast<uint8>(clientSlot - 1) : static_cast<uint8>(clientSlot);
        if (serverSlot >= EQUIPMENT_SLOT_START && serverSlot < EQUIPMENT_SLOT_END)
        {
            Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
            if (it && it->GetEntry() == itemId)
                return it;
        }
        return nullptr;
    }

    // 背包位
    if (bag >= 0)
    {
        if (bag == 0)
        {
            // 客户端主背包槽位是1-based，服务器是0-based
            int32 clientSlot = slot;
            uint8 serverSlot = (clientSlot > 0) ? static_cast<uint8>(clientSlot - 1) : static_cast<uint8>(clientSlot);
            Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
            if (it && it->GetEntry() == itemId)
                return it;
        }
        else
        {
            uint8 bagPos = INVENTORY_SLOT_BAG_START + static_cast<uint8>(bag - 1);
            Bag* bagObj = player->GetBagByPos(bagPos);
            if (bagObj)
            {
                // 客户端包内槽位是1-based，服务器是0-based
                int32 clientSlot = slot;
                if (clientSlot >= 1 && static_cast<uint32>(clientSlot - 1) < bagObj->GetBagSize())
                {
                    Item* it = bagObj->GetItemByPos(static_cast<uint32>(clientSlot - 1));
                    if (it && it->GetEntry() == itemId)
                        return it;
                }
            }
        }
    }

    return nullptr;
}

// 处理客户端插件查询命令
// 根据物品ID查找玩家的物品，并从随机附魔槽读取GUID（参考强化系统）
Item* FindPlayerItemByIdAndGuid(Player* player, uint32 itemId, uint32 clientGuid)
{
    if (!player || itemId == 0)
        return nullptr;

    // 先检查背包（仅匹配实际GUID，不再使用随机附魔字段）
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetEntry() == itemId)
        {
            uint32 actualGuid = item->GetGUID().GetCounter();
            if (actualGuid == clientGuid)
                return item;
        }
    }

    // 检查其他背包
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bag = player->GetBagByPos(i);
        if (bag)
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                if (item && item->GetEntry() == itemId)
                {
                    uint32 actualGuid = item->GetGUID().GetCounter();
                    if (actualGuid == clientGuid)
                        return item;
                }
            }
        }
    }

    // 最后检查装备栏
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetEntry() == itemId)
        {
            uint32 actualGuid = item->GetGUID().GetCounter();
            if (actualGuid == clientGuid)
                return item;
        }
    }

    return nullptr;
}

bool ItemGrowthCommands::HandleItemGrowthQueryCommand(ChatHandler* handler, char const* args)
{
    if (!sItemGrowthMgr->IsEnabled())
    {
        handler->SendSysMessage("物品成长系统当前已禁用。");
        handler->SetSentErrorMessage(true);
        return false;
    }

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    if (!args || !*args)
    {
        handler->SendSysMessage("用法: .物品成长 查询 <物品ID> <物品GUID>");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 解析参数：物品ID 物品GUID/seed [bag] [slot] [equipFlag] [rpId]
    char* itemIdStr = strtok((char*)args, " ");
    char* itemGuidStr = strtok(nullptr, " ");
    char* bagStr = strtok(nullptr, " ");
    char* slotStr = strtok(nullptr, " ");
    char* equipStr = strtok(nullptr, " ");
    char* rpIdStr = strtok(nullptr, " ");

    if (!itemIdStr || !itemGuidStr)
    {
        handler->SendSysMessage("用法: .物品成长 查询 <物品ID> <物品GUID>");
        handler->SetSentErrorMessage(true);
        return false;
    }

    uint32 itemId = atoi(itemIdStr);
    uint32 clientGuid = atoi(itemGuidStr);
    int32 bag = bagStr ? atoi(bagStr) : -1;
    int32 slot = slotStr ? atoi(slotStr) : -1;
    bool isEquip = equipStr ? (atoi(equipStr) != 0) : false;
    int32 rpId = rpIdStr ? atoi(rpIdStr) : 0;

    if (itemId == 0)
    {
        handler->SendSysMessage("无效的物品ID");
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 查找玩家的物品：优先使用GUID；若失败则将该参数视为seed尝试；再失败使用位置；仍失败则在同地图玩家中按seed搜索
    Item* item = nullptr;
    if (clientGuid != 0)
    {
        item = FindPlayerItemByIdAndGuid(player, itemId, clientGuid);
        if (!item)
        {
            item = FindPlayerItemBySeed(player, itemId, clientGuid, rpId, bag, slot, isEquip);
        }
    }
    if (!item)
    {
        item = FindPlayerItemByPosition(player, itemId, bag, slot, isEquip);
    }
    if (!item && clientGuid != 0)
    {
        // 最后手段：在同地图玩家里搜（用于他人物品的聊天链接）
        item = FindNearbyItemBySeed(player, itemId, clientGuid, rpId);
    }
    if (!item)
    {
        ItemGrowthPlayerRecord* remoteRecord = nullptr;
        if (clientGuid != 0)
        {
            remoteRecord = sItemGrowthMgr->GetPlayerItemRecord(clientGuid);
            // 不再需要处理GROWTH_GUID_OFFSET，直接使用真实GUID
        }

        if (remoteRecord)
        {
            // 确保升级经验为最新值
            sItemGrowthMgr->UpdateRequiredExp(remoteRecord);

            // 使用记录中的真实GUID，客户端GUID字段保持兼容
            std::string response = "ITEMGROWTH:" +
                                  Acore::ToString(itemId) + ":" +
                                  Acore::ToString(clientGuid) + ":" +
                                  Acore::ToString(remoteRecord->currentLevel) + ":" +
                                  Acore::ToString(remoteRecord->currentExp) + ":" +
                                  Acore::ToString(remoteRecord->requiredExp) + ":" +
                                  remoteRecord->growthAttributes + ":" +
                                  remoteRecord->percentAttributes + ":" +
                                  Acore::ToString(remoteRecord->itemGuid) + ":-1:-1:0";

            handler->PSendSysMessage(response.c_str());

            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ItemGrowthQuery: 玩家 %s 查询物品ID %s 客户端GUID %s - 使用成长记录返回等级:%s 经验:%s/%s 属性:'%s'", player->GetName(), itemId, clientGuid, remoteRecord->currentLevel, remoteRecord->currentExp, remoteRecord->requiredExp, remoteRecord->growthAttributes).c_str());
            return true;
        }

        // 关键修复：静默处理未找到的情况，不发送任何消息给玩家
        // 这种情况通常发生在查询聊天链接中的他人装备时，属于正常情况
        // 发送空响应给客户端插件，让它知道这个物品没有成长属性
        std::string response = "ITEMGROWTH:" + Acore::ToString(itemId) + ":" + Acore::ToString(clientGuid) + ":0:0:0::";
        handler->PSendSysMessage(response.c_str());

        // 只记录调试日志，不显示给玩家
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ItemGrowthQuery: 玩家 %s 查询物品ID %s 客户端GUID %s - 未找到物品（可能是他人装备或聊天链接）", player->GetName(), itemId, clientGuid).c_str());

        // 重要：不调用 SetSentErrorMessage，避免显示错误提示
        return true;
    }

    // 确保seed非零，仅当该物品属于查询者本人时才执行（避免跨玩家修改）
    if (item->GetOwner() == player)
    {
        // 确保 ITEM_FIELD_PROPERTY_SEED 非零，以便链接唯一标识
        uint32 seed = item->GetUInt32Value(ITEM_FIELD_PROPERTY_SEED);
        if (seed == 0)
        {
            uint32 guidLow = item->GetGUID().GetCounter();
            item->SetUInt32Value(ITEM_FIELD_PROPERTY_SEED, guidLow);
            item->SetState(ITEM_CHANGED, player);
            if (item->IsEquipped())
                player->SetVisibleItemSlot(item->GetSlot(), item);
            item->SendUpdateToPlayer(player);
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ItemGrowthQuery: Ensure seed from 0 to %s for item %s (GUID:%s)", guidLow, item->GetEntry(), guidLow).c_str());
        }
    }

    // 获取物品的真实GUID
    uint32 realGuid = item->GetGUID().GetCounter();

    // 获取物品成长记录（使用真实GUID）
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(realGuid);
    if (!record)
    {
        // 发送空响应，表示物品没有成长信息（使用客户端GUID响应，包含百分比属性字段）
        std::string response = "ITEMGROWTH:" + Acore::ToString(itemId) + ":" + Acore::ToString(clientGuid) + ":0:0:0::";
        handler->PSendSysMessage(response.c_str());
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ItemGrowthQuery: 玩家 %s 查询物品ID %s 客户端GUID %s 真实GUID %s - 没有成长记录", player->GetName(), itemId, clientGuid, realGuid).c_str());
        return true;
    }

    // 验证物品ID是否匹配
    if (record->itemEntry != itemId)
    {
        handler->PSendSysMessage("物品ID %u 与记录中的物品ID %u 不匹配", itemId, record->itemEntry);
        handler->SetSentErrorMessage(true);
        return false;
    }

    // 计算实际位置（用于客户端无GUID时精确映射）
    int32 outBag = -1;
    int32 outSlot = -1;
    int32 outEquip = item->IsEquipped() ? 1 : 0;
    if (item->IsEquipped())
    {
        // 响应中发送客户端槽位（1-based），与插件一致
        outSlot = static_cast<int32>(item->GetSlot()) + 1;
    }
    else
    {
        uint8 bagSlot = item->GetBagSlot();
        if (bagSlot == INVENTORY_SLOT_BAG_0)
            outBag = 0;
        else if (bagSlot >= INVENTORY_SLOT_BAG_START && bagSlot < INVENTORY_SLOT_BAG_END)
            outBag = static_cast<int32>(bagSlot - INVENTORY_SLOT_BAG_START + 1);
        // 背包内槽位同样返回客户端索引（1-based）
        outSlot = static_cast<int32>(item->GetSlot()) + 1;
    }

    // 构造响应消息: ITEMGROWTH:ITEMID:GUID:LEVEL:EXP:MAXEXP:ATTRIBUTES:PERCENTATTRIBUTES:REALGUID:BAG:SLOT:EQUIP
    // 注意：GUID字段仍用客户端传入的GUID（兼容），REALGUID用于客户端建立稳定键
    std::string response = "ITEMGROWTH:" +
                          Acore::ToString(itemId) + ":" +
                          Acore::ToString(clientGuid) + ":" +
                          Acore::ToString(record->currentLevel) + ":" +
                          Acore::ToString(record->currentExp) + ":" +
                          Acore::ToString(record->requiredExp) + ":" +
                          record->growthAttributes + ":" +
                          record->percentAttributes + ":" +
                          Acore::ToString(realGuid) + ":" +
                          Acore::ToString(outBag) + ":" +
                          Acore::ToString(outSlot) + ":" +
                          Acore::ToString(outEquip);

    // 发送响应给客户端
    handler->PSendSysMessage(response.c_str());

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ItemGrowthQuery: 玩家 %s 查询物品ID %s 客户端GUID %s 真实GUID %s - 等级:%s 经验:%s/%s 基础属性:'%s' 百分比属性:'%s'", player->GetName(), itemId, clientGuid, realGuid, record->currentLevel, record->currentExp, record->requiredExp, record->growthAttributes, record->percentAttributes).c_str());

    return true;
}
