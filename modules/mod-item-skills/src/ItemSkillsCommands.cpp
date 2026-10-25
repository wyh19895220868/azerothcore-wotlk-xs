#include "AsInt256.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include "Config.h"
#include "Player.h"
#include "ItemSkillsManager.h"
#include "ItemSkillsDBHelper.h"
#include "ItemSkillsDisplay.h"
#include "ItemSkillsEffects.h"
#include "Language.h"
#include "ObjectMgr.h"
#include "Configuration/Config.h"
#include "Bag.h"
#include "ItemTemplate.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include <limits>
#include <sstream>
#include "ModuleFormat.h"

using namespace Acore::ChatCommands;

namespace
{
int32 ToClientInt32(int64 value)
{
    if (value > std::numeric_limits<int32>::max())
        return std::numeric_limits<int32>::max();

    if (value < std::numeric_limits<int32>::min())
        return std::numeric_limits<int32>::min();

    return static_cast<int32>(value);
}

uint32 ToClientUInt32(uint64 value)
{
    return value > std::numeric_limits<uint32>::max() ? std::numeric_limits<uint32>::max() : static_cast<uint32>(value);
}

uint32 ToClientUInt32(uint32 value)
{
    return value;
}

uint32 ToClientUInt32(int64 value)
{
    if (value <= 0)
        return 0;

    return value > static_cast<int64>(std::numeric_limits<uint32>::max()) ? std::numeric_limits<uint32>::max() : static_cast<uint32>(value);
}
}

// ============================================================
// 辅助函数 - 强制刷新客户端物品模板缓存
// ============================================================

// 核心函数：发送SMSG_ITEM_QUERY_SINGLE_RESPONSE包
// 直接接受技能数组，避免时序问题
static void ForceClientRefreshItemTemplateWithSkills(Player* player, uint32 itemId, const std::vector<ItemSkillData>& itemSkills)
{
    ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(itemId);
    if (!pProto)
        return;

    WorldSession* session = player->GetSession();
    if (!session)
        return;

    // 调试：输出每个技能的详细信息
    for (size_t i = 0; i < itemSkills.size(); ++i)
    {
        LOG_DEBUG("module.itemskills", ModuleOldFormat("  技能[%s]: SkillID=%s, TriggerType=%s, TemplateID=%s", i, itemSkills[i].skillId, itemSkills[i].triggerType, itemSkills[i].skillTemplateId).c_str());
    }

    std::string Name = pProto->Name1;
    std::string Description = pProto->Description;

    int loc_idx = session->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        if (ItemLocale const* il = sObjectMgr->GetItemLocale(pProto->ItemId))
        {
            ObjectMgr::GetLocaleString(il->Name, loc_idx, Name);
            ObjectMgr::GetLocaleString(il->Description, loc_idx, Description);
        }
    }

    // 构建物品查询响应包（与ItemHandler.cpp中的逻辑相同）
    WorldPacket queryData(SMSG_ITEM_QUERY_SINGLE_RESPONSE, 600);
    queryData << pProto->ItemId;
    queryData << pProto->Class;
    queryData << pProto->SubClass;
    queryData << pProto->SoundOverrideSubclass;
    queryData << Name;
    queryData << uint8(0x00);
    queryData << uint8(0x00);
    queryData << uint8(0x00);
    queryData << pProto->DisplayInfoID;
    queryData << pProto->Quality;
    queryData << pProto->Flags;
    queryData << pProto->Flags2;
    queryData << ToClientInt32(pProto->BuyPrice);
    queryData << ToClientUInt32(pProto->SellPrice);
    queryData << pProto->InventoryType;
    queryData << pProto->AllowableClass;
    queryData << pProto->AllowableRace;
    queryData << pProto->ItemLevel;
    queryData << pProto->RequiredLevel;
    queryData << pProto->RequiredSkill;
    queryData << pProto->RequiredSkillRank;
    queryData << pProto->RequiredSpell;
    queryData << pProto->RequiredHonorRank;
    queryData << pProto->RequiredCityRank;
    queryData << pProto->RequiredReputationFaction;
    queryData << pProto->RequiredReputationRank;
    queryData << int32(pProto->MaxCount);
    queryData << int32(pProto->Stackable);
    queryData << pProto->ContainerSlots;
    queryData << pProto->StatsCount;
    for (uint32 i = 0; i < pProto->StatsCount; ++i)
    {
        queryData << pProto->ItemStat[i].ItemStatType;
        queryData << AsInt64ToInt32Clamp(pProto->AsItemStatValue[i], std::numeric_limits<int32>::max());
    }
    queryData << pProto->ScalingStatDistribution;
    queryData << ToClientUInt32(pProto->ScalingStatValue);
    for (int i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
    {
        queryData << float(pProto->Damage[i].DamageMin);
        queryData << float(pProto->Damage[i].DamageMax);
        queryData << pProto->Damage[i].DamageType;
    }

    // 抗性
    queryData << AsInt64ToUInt32Clamp(pProto->AsArmor);
    queryData << pProto->HolyRes;
    queryData << pProto->FireRes;
    queryData << pProto->NatureRes;
    queryData << pProto->FrostRes;
    queryData << pProto->ShadowRes;
    queryData << pProto->ArcaneRes;

    queryData << pProto->Delay;
    queryData << pProto->AmmoType;
    queryData << pProto->RangedModRange;

    // 关键部分：发送Spell信息
    // 如果有物品实例技能数据，使用实例技能；否则使用模板技能
    uint32 firstSpellId = 0; // 记录第一个技能ID，用于日志

    for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
    {
        uint32 spellId = 0;
        uint32 spellTrigger = 0;
        int32 spellCharges = 0;
        int32 spellCooldown = -1;
        uint32 spellCategory = 0;
        int32 spellCategoryCooldown = -1;

        // 优先使用物品实例的技能数据（从物品技能_数据表读取）
        if (!itemSkills.empty() && s < static_cast<int>(itemSkills.size()))
        {
            const auto& skillData = itemSkills[s];
            if (skillData.triggerType == 0) // 只处理使用触发类型
            {
                // 发送真实的技能ID让客户端显示正确的图标和提示
                // 使用触发和冷却由服务器端CanItemUse Hook处理
                spellId = skillData.skillId;
                spellTrigger = 0; // 使用触发
                spellCharges = 0; // 无限使用
                spellCooldown = -1; // 不显示冷却（由服务器控制）
                spellCategory = 0;
                spellCategoryCooldown = -1;

                LOG_DEBUG("module.itemskills", ModuleOldFormat("发送物品技能: Slot=%s, SkillID=%s, TemplateID=%s", s, skillData.skillId, skillData.skillTemplateId).c_str());
            }
        }

        // 如果没有实例技能数据，使用原始模板的技能
        if (spellId == 0 && pProto->Spells[s].SpellId != 0)
        {
            spellId = pProto->Spells[s].SpellId;
            spellTrigger = pProto->Spells[s].SpellTrigger;
            spellCharges = pProto->Spells[s].SpellCharges;
            spellCooldown = pProto->Spells[s].SpellCooldown;
            spellCategory = pProto->Spells[s].SpellCategory;
            spellCategoryCooldown = pProto->Spells[s].SpellCategoryCooldown;
        }

        // 记录第一个技能ID
        if (s == 0)
            firstSpellId = spellId;

        // 发送技能数据
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(spellId);
        if (spell)
        {
            bool db_data = spellCooldown >= 0 || spellCategoryCooldown >= 0;

            queryData << spellId;
            queryData << spellTrigger;
            queryData << spellCharges;

            if (db_data)
            {
                queryData << uint32(spellCooldown);
                queryData << spellCategory;
                queryData << uint32(spellCategoryCooldown);
            }
            else
            {
                queryData << uint32(spell->RecoveryTime);
                queryData << uint32(spell->GetCategory());
                queryData << uint32(spell->CategoryRecoveryTime);
            }
        }
        else
        {
            queryData << uint32(0);
            queryData << uint32(0);
            queryData << uint32(0);
            queryData << uint32(-1);
            queryData << uint32(0);
            queryData << uint32(-1);
        }
    }
    queryData << pProto->Bonding;
    queryData << Description;
    queryData << pProto->PageText;
    queryData << pProto->LanguageID;
    queryData << pProto->PageMaterial;
    queryData << pProto->StartQuest;
    queryData << pProto->LockID;
    queryData << int32(pProto->Material);
    queryData << pProto->Sheath;
    queryData << pProto->RandomProperty;
    queryData << pProto->RandomSuffix;
    queryData << pProto->Block;
    queryData << pProto->ItemSet;
    queryData << pProto->MaxDurability;
    queryData << pProto->Area;
    queryData << pProto->Map;
    queryData << pProto->BagFamily;
    queryData << pProto->TotemCategory;
    for (int s = 0; s < MAX_ITEM_PROTO_SOCKETS; ++s)
    {
        queryData << pProto->Socket[s].Color;
        queryData << pProto->Socket[s].Content;
    }
    queryData << pProto->socketBonus;
    queryData << pProto->GemProperties;
    queryData << pProto->RequiredDisenchantSkill;
    queryData << float(pProto->ArmorDamageModifier);
    queryData << pProto->Duration;
    queryData << pProto->ItemLimitCategory;
    queryData << pProto->HolidayId;

    // 发送包到客户端，强制刷新缓存
    session->SendPacket(&queryData);
}

// 兼容版本：从数据库查询技能（用于登录时）
void ForceClientRefreshItemTemplate(Player* player, uint32 itemId, Item* item = nullptr)
{
    std::vector<ItemSkillData> itemSkills;
    if (item)
    {
        itemSkills = sItemSkillsDBHelper->GetItemSkillsData(item);
    }
    ForceClientRefreshItemTemplateWithSkills(player, itemId, itemSkills);
}

// ============================================================
// 辅助函数 - 查找玩家物品（参考mod-item-attributes）
// ============================================================

// 根据物品ID和GUID查找玩家的物品
static Item* FindPlayerItemByIdAndGuid(Player* player, uint32 itemId, uint64 clientGuid)
{
    if (!player || itemId == 0)
        return nullptr;

    // 检查主背包
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetEntry() == itemId)
        {
            uint64 actualGuid = item->GetGUID().GetCounter();
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
                    uint64 actualGuid = item->GetGUID().GetCounter();
                    if (actualGuid == clientGuid)
                        return item;
                }
            }
        }
    }

    // 检查装备栏
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetEntry() == itemId)
        {
            uint64 actualGuid = item->GetGUID().GetCounter();
            if (actualGuid == clientGuid)
                return item;
        }
    }

    return nullptr;
}

// 根据位置查找玩家的物品 (bag/slot/equipFlag)
static Item* FindPlayerItemByPosition(Player* player, uint32 itemId, int32 bag, int32 slot, bool isEquip)
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
    if (bag >= 0 && slot >= 0)
    {
        if (bag == 0)
        {
            // 主背包，客户端槽位是1-based
            uint8 serverSlot = (slot > 0) ? static_cast<uint8>(slot - 1) : static_cast<uint8>(slot);
            Item* it = player->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
            if (it && it->GetEntry() == itemId)
                return it;
        }
        else
        {
            // 其他背包
            uint8 bagPos = INVENTORY_SLOT_BAG_START + static_cast<uint8>(bag - 1);
            Bag* bagObj = player->GetBagByPos(bagPos);
            if (bagObj)
            {
                uint8 serverSlot = (slot > 0) ? static_cast<uint8>(slot - 1) : static_cast<uint8>(slot);
                if (serverSlot < bagObj->GetBagSize())
                {
                    Item* it = bagObj->GetItemByPos(serverSlot);
                    if (it && it->GetEntry() == itemId)
                        return it;
                }
            }
        }
    }

    return nullptr;
}

// 物品技能信息命令
class ItemSkills_Command_Info : public CommandScript
{
public:
    ItemSkills_Command_Info() : CommandScript("ItemSkills_Command_Info") { }

    // 辅助函数：检查玩家命令权限
    static bool CheckPlayerCommandPermission(ChatHandler* handler)
    {
        uint32 security = handler->GetSession()->GetSecurity();

        // 如果是普通玩家，检查是否允许使用命令
        if (security == SEC_PLAYER)
        {
            bool allowPlayerCommands = sConfigMgr->GetOption<bool>("ItemSkills.AllowPlayerCommands", true);
            if (!allowPlayerCommands)
            {
                handler->SendSysMessage("该命令当前已被禁用。");
                handler->SetSentErrorMessage(true);
                return false;
            }
        }
        else
        {
            // GM等级检查
            uint32 minGMRank = sItemSkillsManager->GetMinGMRankForCommands();
            if (security < minGMRank)
            {
                handler->SendSysMessage("您的权限不足以使用此命令。");
                handler->SetSentErrorMessage(true);
                return false;
            }
        }
        return true;
    }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable itemSkillsCommandTable =
        {
            { "信息",      HandleItemSkillsInfoCommand,      SEC_PLAYER,     Console::No },
            { "查询",      HandleItemSkillsQueryCommand,     SEC_PLAYER,     Console::No },
            { "列表",      HandleItemSkillsListCommand,      SEC_GAMEMASTER, Console::No },
            { "添加",      HandleItemSkillsAddCommand,       SEC_GAMEMASTER, Console::No },
            { "移除",      HandleItemSkillsRemoveCommand,    SEC_GAMEMASTER, Console::No },
            { "重载",      HandleItemSkillsReloadCommand,    SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "物品技能", itemSkillsCommandTable },
        };

        return commandTable;
    }

    // .物品技能 信息 - 显示物品的技能信息
    static bool HandleItemSkillsInfoCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 检查命令权限
        if (!CheckPlayerCommandPermission(handler))
            return false;

        // 获取目标物品
        Item* item = nullptr;

        if (*args)
        {
            // 尝试通过物品ID获取物品
            uint32 itemId = static_cast<uint32>(strtoul(args, nullptr, 10));
            if (itemId > 0)
            {
                item = player->GetItemByEntry(itemId);
            }
        }
        else
        {
            // 使用当前选中的物品
            ObjectGuid targetGuid = player->GetTarget();
            if (targetGuid.IsItem())
            {
                item = player->GetItemByGuid(targetGuid);
            }

            // 如果没有选中物品，使用装备中的主手武器
            if (!item)
            {
                item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            }
        }

        if (!item)
        {
            handler->SendSysMessage("请选择一个物品或提供有效的物品ID。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 获取物品的技能信息（从物品实例数据表查询）
        ItemTemplate const* proto = item->GetTemplate();
        std::vector<ItemSkillData> skillsData = sItemSkillsDBHelper->GetItemSkillsData(item);

        handler->PSendSysMessage("%s", ModuleOldFormat("物品: |cffffffff%s|r (ID: %s, GUID: %s)", proto->Name1.c_str(), proto->ItemId, item->GetGUID().GetCounter()).c_str());

        if (skillsData.empty())
        {
            handler->SendSysMessage("该物品没有关联的技能。");
            return true;
        }

        handler->SendSysMessage("关联的技能:");
        for (const auto& skillData : skillsData)
        {
            // 从技能模板获取详细信息
            ItemSkillTemplate const* skill = sItemSkillsManager->GetSkillTemplate(skillData.skillTemplateId);
            if (!skill)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("- [未知技能模板 ID: %s]", skillData.skillTemplateId).c_str());
                continue;
            }

            std::string triggerType;
            switch (skillData.triggerType)
            {
                case ITEM_SKILL_TRIGGER_ON_USE:
                    triggerType = "使用触发";
                    break;
                case ITEM_SKILL_TRIGGER_ON_EQUIP:
                    triggerType = "装备触发";
                    break;
                case ITEM_SKILL_TRIGGER_ON_HIT:
                    triggerType = "击中触发";
                    break;
                case ITEM_SKILL_TRIGGER_LEARN_SPELL:
                    triggerType = "学习技能";
                    break;
                default:
                    triggerType = "未知";
                    break;
            }

            handler->PSendSysMessage("%s", ModuleOldFormat("- |cff00ff00%s|r (ID: %s, 组: %s, 等级: %s)", skill->comment.c_str(), skill->id, skill->group, skill->level).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("  技能ID: %s, 触发类型: %s, 触发几率: %s%%", skillData.skillId, triggerType.c_str(), skillData.triggerChance).c_str());

            if (skill->charges != 0)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("  使用次数: %s", skill->charges).c_str());
            }

            // 显示冷却信息
            if (skill->cooldownMode == 1)
            {
                handler->PSendSysMessage("  冷却时间: 无冷却");
            }
            else if (skill->cooldownMode == 2 && skill->cooldownValue > 0)
            {
                handler->PSendSysMessage("  冷却时间: %.1f秒 (自定义)", skill->cooldownValue / 1000.0f);
            }
            else
            {
                handler->PSendSysMessage("  冷却时间: 官方默认");
            }

            if (skill->ppmRate > 0)
            {
                handler->PSendSysMessage("  每分钟触发次数: %.2f", skill->ppmRate);
            }

            // 显示其他强化信息
            if (skill->costMode == 1)
            {
                handler->PSendSysMessage("  消耗: 无消耗");
            }

            if (skill->castMode == 1)
            {
                handler->PSendSysMessage("  施法时间: 瞬发");
            }

            if (skill->powerMode == 1 && skill->powerValue != 0)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("  伤害强化: +%s (固定值)", skill->powerValue).c_str());
            }
            else if (skill->powerMode == 2 && skill->powerValue != 0)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("  伤害强化: %s%% (百分比)", 100 + skill->powerValue).c_str());
            }

            if (skill->skillType == 1 && skill->skillDuration > 0)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("  Buff持续时间: %s分钟", skill->skillDuration).c_str());
            }
        }

        return true;
    }

    // .物品技能 查询 - 查询物品技能数据并发送到客户端插件（参考mod-item-attributes）
    // 格式: .物品技能 查询 itemId clientGuid [bag] [slot] [equip] [rpId]
    static bool HandleItemSkillsQueryCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 【修复低风险问题】查询命令也检查权限
        if (!CheckPlayerCommandPermission(handler))
            return true;  // 静默返回，不输出错误信息（插件会重试）

        // 静默处理：无参数时直接返回
        if (!args || !*args)
            return true;

        // 解析参数: itemID clientGuid [bag] [slot] [equipFlag] [rpId]
        char* itemIdStr = strtok((char*)args, " ");
        char* clientGuidStr = strtok(nullptr, " ");
        char* bagStr = strtok(nullptr, " ");
        char* slotStr = strtok(nullptr, " ");
        char* equipStr = strtok(nullptr, " ");
        char* rpIdStr = strtok(nullptr, " ");

        if (!itemIdStr || !clientGuidStr)
            return true; // 静默失败

        uint32 itemId = static_cast<uint32>(strtoul(itemIdStr, nullptr, 10));
        uint64 clientGuid = static_cast<uint64>(strtoull(clientGuidStr, nullptr, 10));
        int32 bag = bagStr ? static_cast<int32>(strtol(bagStr, nullptr, 10)) : -1;
        int32 slot = slotStr ? static_cast<int32>(strtol(slotStr, nullptr, 10)) : -1;
        bool isEquip = equipStr ? (strtol(equipStr, nullptr, 10) != 0) : false;

        if (itemId == 0)
        {
            // 空响应
            std::string response = "ITEMSKILLS:" + std::to_string(itemId) + ":" + std::to_string(clientGuid) + ":::0:-1:-1:0";
            handler->PSendSysMessage(response.c_str());
            return true;
        }

        // 多种方式查找物品（参考mod-item-attributes）
        Item* item = FindPlayerItemByIdAndGuid(player, itemId, clientGuid);
        if (!item)
            item = FindPlayerItemByPosition(player, itemId, bag, slot, isEquip);

        // 【修复中风险问题】物品归属校验：只有找到玩家拥有的物品才查询技能数据
        // 防止通过客户端提供的GUID获取他人物品的技能信息
        if (!item)
        {
            // 未找到物品，返回空响应（不使用客户端GUID查询数据库）
            std::string response = "ITEMSKILLS:" + std::to_string(itemId) + ":" + std::to_string(clientGuid) + ":::0:-1:-1:0";
            handler->PSendSysMessage(response.c_str());
            return true;
        }

        // 使用找到的物品的真实GUID
        uint64 realGuid = item->GetGUID().GetCounter();

        // 从数据库查询技能数据（现在只会查询玩家拥有的物品）
        std::vector<ItemSkillData> skills = sItemSkillsDBHelper->GetItemSkillsData(item);

        // 构建技能描述字符串
        std::string skillsForClient;
        bool first = true;
        for (const auto& skillData : skills)
        {
            ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillData.skillTemplateId);
            if (!skillTemplate)
                continue;

            if (!first)
                skillsForClient += ",";
            first = false;

            // 格式: 技能名
            skillsForClient += skillTemplate->clientDisplay;
        }

        // 计算物品位置信息
        int32 outBag = -1;
        int32 outSlot = -1;
        int32 outEquip = 0;

        if (item)
        {
            if (item->IsEquipped())
            {
                outBag = 255;  // 装备栏标识
                outSlot = item->GetSlot() + 1;  // 客户端1-based
                outEquip = 1;
            }
            else if (item->IsInBag())
            {
                outBag = item->GetBagSlot() - INVENTORY_SLOT_BAG_START + 1;  // 客户端1-based
                outSlot = item->GetSlot() + 1;  // 客户端1-based
                outEquip = 0;
            }
            else
            {
                outBag = 0;  // 主背包
                outSlot = item->GetSlot() + 1;  // 客户端1-based
                outEquip = 0;
            }
        }

        // 如果有使用触发技能，发送ItemTemplate让客户端更新
        bool hasUseSkill = false;
        for (const auto& skillData : skills)
        {
            if (skillData.triggerType == 0) // 使用触发
            {
                hasUseSkill = true;
                break;
            }
        }

        if (hasUseSkill && item && !skills.empty())
        {
            LOG_DEBUG("module.itemskills", ModuleOldFormat("[Trigger] 查询到使用触发技能，发送占位符ItemTemplate: ItemID=%s, GUID=%s", itemId, realGuid).c_str());
            // 发送占位符技能，让客户端知道物品可以右键使用
            // 但不会在tooltip显示技能（因为发送的是占位符ID）
            ForceClientRefreshItemTemplateWithSkills(player, itemId, skills);
        }

        // 构造响应: ITEMSKILLS:itemID:clientGuid:skills:realGuid:bag:slot:equip
        std::string response = "ITEMSKILLS:" +
                              std::to_string(itemId) + ":" +
                              std::to_string(clientGuid) + ":" +
                              skillsForClient + ":" +
                              std::to_string(realGuid) + ":" +
                              std::to_string(outBag) + ":" +
                              std::to_string(outSlot) + ":" +
                              std::to_string(outEquip);

        handler->PSendSysMessage(response.c_str());

        LOG_DEBUG("module.itemskills", ModuleOldFormat("Query: Player %s itemID %s clientGuid %s realGuid %s - skills: '%s'", player->GetName(), itemId, clientGuid, realGuid, skillsForClient).c_str());

        return true;
    }

    // .物品技能 列表 - 列出所有技能模板
    static bool HandleItemSkillsListCommand(ChatHandler* handler, const char* args)
    {
        // 检查GM权限
        uint32 minGMRank = sItemSkillsManager->GetMinGMRankForCommands();
        if (handler->GetSession()->GetSecurity() < minGMRank)
        {
            handler->SendSysMessage("您的权限不足以使用此命令。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string filter = args;

        const auto& templates = sItemSkillsManager->GetAllSkillTemplates();

        uint32 count = 0;
        handler->SendSysMessage("物品技能模板列表:");

        for (const auto& pair : templates)
        {
            const ItemSkillTemplate& tmpl = pair.second;

            // 如果有过滤条件，检查是否匹配
            if (!filter.empty())
            {
                if (tmpl.comment.find(filter) == std::string::npos)
                    continue;
            }

            handler->PSendSysMessage("%s", ModuleOldFormat("ID: %s, 名称: %s, 组: %s, 等级: %s, 技能ID: %s", tmpl.id, tmpl.comment.c_str(), tmpl.group, tmpl.level, tmpl.spellId).c_str());

            count++;

            // 限制输出数量，避免刷屏
            if (count >= 25 && !filter.empty())
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("显示了 %s 个匹配的技能模板，可能还有更多结果。请使用更具体的过滤条件。", count).c_str());
                break;
            }
        }

        if (count == 0)
        {
            handler->SendSysMessage("没有找到匹配的技能模板。");
        }
        else if (filter.empty())
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("共显示了 %s 个技能模板。", count).c_str());
        }

        return true;
    }

    // .物品技能 添加 [物品ID] [技能模板ID] - 添加技能到物品实例
    static bool HandleItemSkillsAddCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 检查GM权限
        uint32 minGMRank = sItemSkillsManager->GetMinGMRankForCommands();
        if (handler->GetSession()->GetSecurity() < minGMRank)
        {
            handler->SendSysMessage("您的权限不足以使用此命令。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!*args)
        {
            handler->SendSysMessage("用法: .物品技能 添加 [物品ID] [技能模板ID]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        char* itemIdStr = strtok((char*)args, " ");
        char* skillIdStr = strtok(nullptr, " ");

        if (!itemIdStr || !skillIdStr)
        {
            handler->SendSysMessage("用法: .物品技能 添加 [物品ID] [技能模板ID]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = static_cast<uint32>(strtoul(itemIdStr, nullptr, 10));
        uint32 skillId = static_cast<uint32>(strtoul(skillIdStr, nullptr, 10));

        // 检查物品是否存在
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("物品ID %s 不存在。", itemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 检查技能模板是否存在
        ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillId);
        if (!skillTemplate)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("技能模板ID %s 不存在。", skillId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 查找玩家身上的物品实例
        Item* item = nullptr;

        // 先检查装备栏
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* tempItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (tempItem && tempItem->GetEntry() == itemId)
            {
                item = tempItem;
                break;
            }
        }

        // 检查主背包
        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
            {
                Item* tempItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                if (tempItem && tempItem->GetEntry() == itemId)
                {
                    item = tempItem;
                    break;
                }
            }
        }

        // 检查其他背包
        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                Bag* bag = player->GetBagByPos(i);
                if (bag)
                {
                    for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                    {
                        Item* tempItem = bag->GetItemByPos(j);
                        if (tempItem && tempItem->GetEntry() == itemId)
                        {
                            item = tempItem;
                            break;
                        }
                    }
                    if (item)
                        break;
                }
            }
        }

        if (!item)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("未在背包中找到物品ID: %s。请先获得该物品。", itemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 添加技能到物品实例（保存到物品技能_数据表）
        sItemSkillsDBHelper->AddSkillToItem(item, skillId);

        // 【修复中风险问题】刷新内存缓存，确保新增技能立即生效
        uint64 playerGuid = player->GetGUID().GetCounter();
        uint64 itemGuid = item->GetGUID().GetCounter();

        // 重新从数据库加载该物品的技能数据到内存
        sItemSkillsManager->RefreshItemSkillsInMemory(playerGuid, item);

        // 如果是击中触发技能，更新击中技能缓存
        if (skillTemplate->triggerType == ITEM_SKILL_TRIGGER_ON_HIT)
        {
            sItemSkillsEffects->UpdatePlayerHitSkillsCache(player);
        }

        handler->PSendSysMessage("%s", ModuleOldFormat("成功为物品 [%s] (GUID: %s) 添加技能: %s", itemTemplate->Name1, item->GetGUID().GetCounter(), skillTemplate->comment).c_str());

        // 针对使用触发类型，发送该物品实例的ItemTemplate
        if (skillTemplate->triggerType == 0) // ITEM_SKILL_TRIGGER_ON_USE
        {
            LOG_INFO("module.itemskills", ModuleOldFormat("检测到使用触发技能，发送物品实例模板: ItemID=%s, GUID=%s", itemId, item->GetGUID().GetCounter()).c_str());

            // 重要：数据库写入是异步的，立即查询可能读不到数据
            // 解决方案：先手动构造技能数据，然后构造包含该技能的ItemTemplate发送
            std::vector<ItemSkillData> skillsForSend;
            ItemSkillData skillData;
            skillData.itemId = itemId;
            skillData.itemGuid = item->GetGUID().GetCounter();
            skillData.skillTemplateId = skillId;
            skillData.skillId = skillTemplate->spellId;
            skillData.triggerType = skillTemplate->triggerType;
            skillData.triggerChance = skillTemplate->triggerChance;
            skillsForSend.push_back(skillData);

            // 查询已有的其他技能（可能有多个）
            std::vector<ItemSkillData> existingSkills = sItemSkillsDBHelper->GetItemSkillsData(item);
            for (const auto& existing : existingSkills)
            {
                // 避免重复添加刚才的技能
                if (existing.skillTemplateId != skillId)
                {
                    skillsForSend.push_back(existing);
                }
            }

            // 发送占位符技能，让客户端知道物品可以右键使用
            ForceClientRefreshItemTemplateWithSkills(player, itemId, skillsForSend);

            handler->SendSysMessage("→已添加技能并热加载！");
            handler->SendSysMessage("→右键使用物品即可触发附加技能");
            handler->SendSysMessage("→客户端插件会在tooltip显示附加技能详情");
            handler->SendSysMessage("→此技能仅对该物品实例生效，不影响其他同ID物品");
        }
        else if (item->IsEquipped())
        {
            handler->SendSysMessage("→物品已装备，请重新装备以应用技能效果");
        }

        return true;
    }

    // .物品技能 移除 [物品ID] [技能模板ID] - 从物品实例移除技能
    static bool HandleItemSkillsRemoveCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 检查GM权限
        uint32 minGMRank = sItemSkillsManager->GetMinGMRankForCommands();
        if (handler->GetSession()->GetSecurity() < minGMRank)
        {
            handler->SendSysMessage("您的权限不足以使用此命令。");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!*args)
        {
            handler->SendSysMessage("用法: .物品技能 移除 [物品ID] [技能模板ID]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        char* itemIdStr = strtok((char*)args, " ");
        char* skillIdStr = strtok(nullptr, " ");

        if (!itemIdStr || !skillIdStr)
        {
            handler->SendSysMessage("用法: .物品技能 移除 [物品ID] [技能模板ID]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = static_cast<uint32>(strtoul(itemIdStr, nullptr, 10));
        uint32 skillId = static_cast<uint32>(strtoul(skillIdStr, nullptr, 10));

        // 检查物品是否存在
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("物品ID %s 不存在。", itemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 检查技能模板是否存在
        ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillId);
        if (!skillTemplate)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("技能模板ID %s 不存在。", skillId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 查找玩家身上的物品实例
        Item* item = nullptr;

        // 先检查装备栏
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* tempItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (tempItem && tempItem->GetEntry() == itemId)
            {
                item = tempItem;
                break;
            }
        }

        // 检查主背包
        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
            {
                Item* tempItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                if (tempItem && tempItem->GetEntry() == itemId)
                {
                    item = tempItem;
                    break;
                }
            }
        }

        // 检查其他背包
        if (!item)
        {
            for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                Bag* bag = player->GetBagByPos(i);
                if (bag)
                {
                    for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                    {
                        Item* tempItem = bag->GetItemByPos(j);
                        if (tempItem && tempItem->GetEntry() == itemId)
                        {
                            item = tempItem;
                            break;
                        }
                    }
                    if (item)
                        break;
                }
            }
        }

        if (!item)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("未在背包中找到物品ID: %s。", itemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 从物品实例移除技能（从_物品技能_数据表移除）
        sItemSkillsDBHelper->RemoveSkillFromItem(item, skillId);

        // 【修复中风险问题】刷新内存缓存，确保移除后的技能立即失效
        uint64 playerGuid = player->GetGUID().GetCounter();

        // 重新从数据库加载该物品的技能数据到内存
        sItemSkillsManager->RefreshItemSkillsInMemory(playerGuid, item);

        // 更新击中技能缓存（无论什么类型都更新，确保移除生效）
        sItemSkillsEffects->UpdatePlayerHitSkillsCache(player);

        // 如果物品已装备，移除技能效果
        if (item->IsEquipped())
        {
            sItemSkillsEffects->RemoveItemSkillEffects(player, item);
            // 重新应用剩余技能效果
            sItemSkillsEffects->ApplyItemSkillEffects(player, item);
        }

        handler->PSendSysMessage("%s", ModuleOldFormat("成功从物品 [%s] (GUID: %s) 移除技能: %s", itemTemplate->Name1, item->GetGUID().GetCounter(), skillTemplate->comment).c_str());

        return true;
    }

    // .物品技能 重载 - 重新加载物品技能数据
    static bool HandleItemSkillsReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        sItemSkillsManager->Initialize();
        handler->SendSysMessage("技能模板数据已重新加载。");
        return true;
    }
};

// 添加命令
void AddItemSkillsCommands()
{
    new ItemSkills_Command_Info();
}
