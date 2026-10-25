#include "AsInt256.h"
#include "StringConvert.h"
#include "ItemEnhancement.h"
#include "ItemEnhancementMgr.h"
#include "ItemEnchantmentMgr.h"
#include "Configuration/Config.h"
#include "Logging/Log.h"
#include "StringConvert.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include "ModuleFormat.h"
#include <limits>

// 物品强化系统配置（默认值）
bool ItemEnhancementEnabled = true;
bool ItemEnhancementFailureProtectionEnabled = true;
bool ItemEnhancementShowSuccessRateEnabled = true;
bool ItemEnhancementAnnounceEnabled = true;
uint32 ItemEnhancementAnnounceLevel = 10;
float ItemEnhancementGoldCostRate = 1.0f;
float ItemEnhancementMaterialCostRate = 1.0f;
float ItemEnhancementSuccessRateRate = 1.0f;
float ItemEnhancementAttributeRate = 1.0f;
uint32 ItemEnhancementCooldown = 0;
uint32 ItemEnhancementMaxLevel = 20;
bool ItemEnhancementDebugMode = false;



// 使用头文件中定义的 EnhancementTemplate 结构体
// 使用头文件中定义的 EnhancementRecord 结构体

// 全局数据存储
std::unordered_map<uint32, EnhancementTemplate> enhancementTemplates;
std::unordered_map<uint32, EnhancementRecord> enhancementRecords;
std::map<std::pair<uint32, uint32>, uint32> enhancementTemplatesByGroupLevel;

namespace
{
AsInt64 SaturatingAddInt64(AsInt64 const& left, AsInt64 const& right)
{
    if (right > 0 && left > std::numeric_limits<AsInt64>::max() - right)
        return std::numeric_limits<AsInt64>::max();

    if (right < 0 && left < std::numeric_limits<AsInt64>::min() - right)
        return std::numeric_limits<AsInt64>::min();

    return left + right;
}
}

// 加载配置
void LoadItemEnhancementConfig()
{
    // 从配置中读取值
    bool enabled = sConfigMgr->GetOption("ItemEnhancement.Enable", true);
    bool failureProtectionEnabled = sConfigMgr->GetOption("ItemEnhancement.FailureProtection", true);
    bool showSuccessRateEnabled = sConfigMgr->GetOption("ItemEnhancement.ShowSuccessRate", true);
    bool announceEnabled = sConfigMgr->GetOption("ItemEnhancement.Announce", true);
    uint32 announceLevel = sConfigMgr->GetOption("ItemEnhancement.AnnounceLevel", 10u);
    float goldCostRate = sConfigMgr->GetOption("ItemEnhancement.GoldCostRate", 1.0f);
    float materialCostRate = sConfigMgr->GetOption("ItemEnhancement.MaterialCostRate", 1.0f);
    float successRateRate = sConfigMgr->GetOption("ItemEnhancement.SuccessRateRate", 1.0f);
    float attributeRate = sConfigMgr->GetOption("ItemEnhancement.AttributeRate", 1.0f);
    uint32 cooldown = sConfigMgr->GetOption("ItemEnhancement.Cooldown", 0u);
    uint32 maxLevel = sConfigMgr->GetOption("ItemEnhancement.MaxLevel", 20u);
    bool debugMode = sConfigMgr->GetOption("ItemEnhancement.DebugMode", false);

    // 设置全局变量
    ItemEnhancementEnabled = enabled && sHelper->GetBoolean(22011);
    ItemEnhancementFailureProtectionEnabled = failureProtectionEnabled;
    ItemEnhancementShowSuccessRateEnabled = showSuccessRateEnabled;
    ItemEnhancementAnnounceEnabled = announceEnabled;
    ItemEnhancementAnnounceLevel = announceLevel;
    ItemEnhancementGoldCostRate = goldCostRate;
    ItemEnhancementMaterialCostRate = materialCostRate;
    ItemEnhancementSuccessRateRate = successRateRate;
    ItemEnhancementAttributeRate = attributeRate;
    ItemEnhancementCooldown = cooldown;
    ItemEnhancementMaxLevel = maxLevel;
    ItemEnhancementDebugMode = debugMode;
}

// 加载物品强化模板数据
void LoadEnhancementTemplates()
{
    enhancementTemplates.clear();
    enhancementTemplatesByGroupLevel.clear();



    // 添加错误处理和安全检查
    try
    {
        bool hasProtectionColumns = WorldDatabase.Query("SHOW COLUMNS FROM `_模板_物品强化` LIKE '失败降级等级'") != nullptr;
        std::string templateQuery = "SELECT `注释`, `id`, `组`, `等级`, `几率`, `物品属性_模板_组`, "
            "`平均随机分配`, `属性值1`, `属性值2`, `属性百分比1`, `属性百分比2`, "
            "`达到该强化等级后的奖励属性`, `达到该强化等级后的奖励属性百分比`, "
            "`物品技能_模板_id_多个逗号隔开`, `强化需求`, `失败处理`";
        if (hasProtectionColumns)
            templateQuery += ", `失败降级等级`, `保底失败次数`, `每次失败增加成功率`, `幸运物品`, `幸运物品数量`, `幸运增加成功率`, `保护物品`, `保护物品数量`";
        else
            templateQuery += ", 1, 0, 0, 0, 0, 0, 0, 0";
        templateQuery += " FROM _模板_物品强化";
        QueryResult result = WorldDatabase.Query(templateQuery.c_str());
        if (!result)
        {

            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            EnhancementTemplate temp;
            uint32 index = 0;

            // 按照数据库表结构正确读取字段
            // 1. 注释 (varchar)
            if (!fields[index].IsNull())
                temp.comment = fields[index].Get<std::string>();
            else
                temp.comment = "";
            index++;

            // 2. id (int)
            temp.id = fields[index++].Get<uint32>();

            // 3. 组 (int)
            temp.group = fields[index++].Get<uint32>();

            // 4. 等级 (int)
            temp.level = fields[index++].Get<uint32>();

            // 5. 几率 (float)
            temp.chance = fields[index++].Get<float>();

            // 6. 物品属性_模板_组 (int)
            temp.itemStatGroup = fields[index++].Get<uint32>();

            // 7. 平均随机分配 (int)
            temp.distributionType = fields[index++].Get<uint32>();

            // 8. 属性值1 (decimal)
            temp.statValue1 = AsInt64FromString(fields[index++].Get<std::string>());

            // 9. 属性值2 (decimal)
            temp.statValue2 = AsInt64FromString(fields[index++].Get<std::string>());

            // 10. 属性百分比1 (decimal)
            temp.statPercent1 = AsInt64FromString(fields[index++].Get<std::string>());

            // 11. 属性百分比2 (decimal)
            temp.statPercent2 = AsInt64FromString(fields[index++].Get<std::string>());

            // 12. 达到该强化等级后的奖励属性 (decimal)
            temp.bonusStat = AsInt64FromString(fields[index++].Get<std::string>());

            // 13. 达到该强化等级后的奖励属性百分比 (decimal)
            temp.bonusStatPercent = AsInt64FromString(fields[index++].Get<std::string>());

            // 14. 物品技能_模板_id_多个逗号隔开 (varchar)
            if (!fields[index].IsNull())
                temp.skillIds = fields[index].Get<std::string>();
            else
                temp.skillIds = "";
            index++;

            // 15. 强化需求 (int)
            temp.requirementId = fields[index++].Get<uint32>();

            // 16. 失败处理 (tinyint)
            temp.failureType = fields[index++].Get<uint8>();
            temp.failureDropLevels = fields[index++].Get<uint32>();
            temp.pityFailureCount = fields[index++].Get<uint32>();
            temp.pityChancePerFailure = fields[index++].Get<float>();
            temp.luckItemId = fields[index++].Get<uint32>();
            temp.luckItemCount = fields[index++].Get<uint32>();
            temp.luckChanceBonus = fields[index++].Get<float>();
            temp.protectionItemId = fields[index++].Get<uint32>();
            temp.protectionItemCount = fields[index++].Get<uint32>();

            enhancementTemplates[temp.id] = temp;
            enhancementTemplatesByGroupLevel[std::make_pair(temp.group, temp.level)] = temp.id;

            // 添加调试信息，特别是组0的数据
            if (temp.group == 0)
            {
                // LOG_INFO("server.loading", "[ItemEnhancement.cpp] 加载组0强化模板: ID={}, 等级={}, 成功率={:.1f}%, 失败类型={}, 属性值1={}, 属性值2={}",
                //     temp.id, temp.level, temp.chance, temp.failureType, temp.statValue1, temp.statValue2);
            }

            count++;
        } while (result->NextRow());

        //LOG_INFO("server.loading", ">> 加载了 {} 条物品强化模板数据", count);
    }
    catch (const std::exception& e)
    {
        //LOG_ERROR("server.loading", "加载物品强化模板数据时发生错误: {}", e.what());
    }
}

// 加载物品强化记录
void LoadEnhancementRecords()
{
    enhancementRecords.clear();



    // 添加错误处理和安全检查
    try
    {
        QueryResult result = CharacterDatabase.Query("SELECT * FROM 物品强化_记录");
        if (!result)
        {

            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            EnhancementRecord record;
            uint32 index = 0;

            // 基本信息
            record.itemGuid = fields[index++].Get<uint32>();                // guid
            record.ownerGuid = fields[index++].Get<uint32>();               // owner_guid
            record.itemTemplateId = fields[index++].Get<uint32>();          // 物品模板ID
            record.group = fields[index++].Get<uint32>();                   // 强化组

            // 强化信息
            record.level = fields[index++].Get<uint32>();                   // 强化等级
            record.enhancementExp = fields[index++].Get<uint32>();          // 强化经验
            record.statValues = fields[index++].Get<std::string>();         // 属性值

            // 品质信息
            record.qualityLevel = fields[index++].Get<uint32>();            // 品质等级
            record.qualityUpgradeCount = fields[index++].Get<uint32>();     // 品质进阶次数

            // 统计信息
            record.enhancementCount = fields[index++].Get<uint32>();        // 强化次数
            record.failureCount = fields[index++].Get<uint32>();            // 失败次数

            // 时间信息
            record.lastEnhanceTime = fields[index++].Get<uint32>();         // 最后强化时间

            enhancementRecords[record.itemGuid] = record;

            count++;
        } while (result->NextRow());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("server.loading", ModuleOldFormat("加载物品强化记录时发生错误: %s", e.what()).c_str());
    }
}

// 保存物品强化记录
void SaveEnhancementRecord(const EnhancementRecord& record)
{
    // 新实现：统一走 ItemEnhancementMgr，确保缓存与数据库、旧接口保持一致
    if (sItemEnhancementMgr)
    {
        sItemEnhancementMgr->SaveEnhancementRecord(record);

        // 仍然维护旧的内存映射，兼容可能直接访问 enhancementRecords 的旧代码
        enhancementRecords[record.itemGuid] = record;
        return;
    }

    // 旧实现保留作降级路径
    try
    {
        std::string sql = "REPLACE INTO 物品强化_记录 (guid, owner_guid, 物品模板ID, 强化组, 强化等级, 强化经验, 属性值, "
            "品质等级, 品质进阶次数, 强化次数, 失败次数, 最后强化时间) VALUES (";

        sql += Acore::ToString(record.itemGuid) + ", ";
        sql += Acore::ToString(record.ownerGuid) + ", ";
        // 【修复】使用记录中的实际值，而不是写死为0
        sql += Acore::ToString(record.itemTemplateId) + ", ";  // 物品模板ID
        sql += Acore::ToString(record.group) + ", ";  // 强化组
        sql += Acore::ToString(record.level) + ", ";
        sql += Acore::ToString(record.enhancementExp) + ", ";  // 强化经验
        sql += "'" + record.statValues + "', ";  // 属性值
        sql += Acore::ToString(record.qualityLevel) + ", ";
        sql += Acore::ToString(record.qualityUpgradeCount) + ", ";
        sql += Acore::ToString(record.enhancementCount) + ", ";
        sql += Acore::ToString(record.failureCount) + ", ";
        sql += Acore::ToString(record.lastEnhanceTime) + ")";

        CharacterDatabase.Execute(sql);

        enhancementRecords[record.itemGuid] = record;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("server.loading", ModuleOldFormat("保存物品强化记录时发生错误: %s", e.what()).c_str());
    }
}

// 删除物品强化记录
void DeleteEnhancementRecord(uint32 itemGuid)
{
    if (sItemEnhancementMgr)
    {
        sItemEnhancementMgr->DeleteEnhancementRecord(itemGuid);
    }
    else
    {
        std::string sql = "DELETE FROM 物品强化_记录 WHERE guid = " + Acore::ToString(itemGuid);
        CharacterDatabase.Execute(sql);
    }

    enhancementRecords.erase(itemGuid);
}

// 获取物品强化模板
EnhancementTemplate const* GetEnhancementTemplate(uint32 id)
{
    if (sItemEnhancementMgr)
        return sItemEnhancementMgr->GetEnhancementTemplate(id);

    auto itr = enhancementTemplates.find(id);
    if (itr != enhancementTemplates.end())
        return &itr->second;
    return nullptr;
}

// 根据组和等级获取物品强化模板
EnhancementTemplate const* GetEnhancementTemplateByGroupAndLevel(uint32 group, uint32 level)
{
    if (sItemEnhancementMgr)
        return sItemEnhancementMgr->GetEnhancementTemplateByGroupAndLevel(group, level);

    auto itr = enhancementTemplatesByGroupLevel.find(std::make_pair(group, level));
    if (itr != enhancementTemplatesByGroupLevel.end())
        return GetEnhancementTemplate(itr->second);
    return nullptr;
}

// 获取物品强化记录
EnhancementRecord const* GetEnhancementRecord(uint32 itemGuid)
{
    if (sItemEnhancementMgr)
        return sItemEnhancementMgr->GetEnhancementRecord(itemGuid);

    auto itr = enhancementRecords.find(itemGuid);
    if (itr != enhancementRecords.end())
        return &itr->second;
    return nullptr;
}

// 判断物品是否可以强化（简化版本，只检查物品本身）
bool CanEnhanceItem(Item* item)
{
    if (!sHelper->GetBoolean(22011))
        return false;

    if (!item)
        return false;

    if (!ItemEnhancementEnabled)
        return false;

    // 检查物品类型
    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return false;

    // 只允许强化武器和装备
    if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
        return false;

    // 不再根据品质限制，允许所有品质的装备强化

    return true;
}

// 判断物品是否可以强化（完整版本，包含玩家检查）
bool CanEnhanceItem(Player* player, Item* item)
{
    // 【修复】先检查空指针，避免在player为空时调用player->GetSession()
    if (!player || !item)
    {
        return false;
    }

    ChatHandler handler(player->GetSession());
    // handler.PSendSysMessage("开始检查物品是否可以强化...");

    // 先检查物品本身是否可以强化
    if (!CanEnhanceItem(item))
    {
        handler.PSendSysMessage("物品本身不可强化（类型或品质限制）");
        return false;
    }
    // handler.PSendSysMessage("物品本身可以强化");

    // 移除绑定检查 - 任何装备都可以强化
    // handler.PSendSysMessage("跳过绑定检查，任何装备都可以强化");

    // 检查物品是否装备
    ObjectGuid itemOwner = item->GetOwnerGUID();
    ObjectGuid playerGuid = player->GetGUID();
    // handler.PSendSysMessage("所有者检查: 物品所有者={}, 玩家GUID={}", itemOwner.GetCounter(), playerGuid.GetCounter());

    if (itemOwner != playerGuid)
    {
        handler.PSendSysMessage("物品所有者不匹配");
        return false;
    }
    // handler.PSendSysMessage("物品所有者检查通过");

    // 检查冷却时间
    EnhancementRecord const* record = GetEnhancementRecord(item->GetGUID().GetCounter());
    if (record && ItemEnhancementCooldown > 0)
    {
        uint32 lastTime = record->lastEnhanceTime;
        uint32 now = static_cast<uint32>(GameTime::GetGameTime().count());
        handler.PSendSysMessage("%s", ModuleOldFormat("冷却检查: 上次强化=%s, 当前时间=%s, 冷却时间=%s", lastTime, now, ItemEnhancementCooldown).c_str());

        if (now < lastTime + ItemEnhancementCooldown)
        {
            handler.PSendSysMessage("物品仍在冷却中");
            return false;
        }
    }
    // handler.PSendSysMessage("冷却检查通过");

    // handler.PSendSysMessage("所有检查通过，物品可以强化");
    return true;
}

// 在玩家背包和装备中查找指定ID的物品
Item* FindPlayerItem(Player* player, uint32 itemId)
{
    if (!player)
        return nullptr;

    ChatHandler handler(player->GetSession());
    // handler.PSendSysMessage("开始查找物品ID: {}", itemId);

    // 先检查背包（优先背包中的物品，因为通常强化的是背包中的装备）
    // handler.PSendSysMessage("检查主背包...");
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item)
        {
            // handler.PSendSysMessage("主背包位 {}: 物品ID={}, GUID={}", i, item->GetEntry(), item->GetGUID().GetCounter());
            if (item->GetEntry() == itemId)
            {
                // handler.PSendSysMessage("在主背包找到匹配物品！");
                return item;
            }
        }
    }

    // 再检查装备栏
    // handler.PSendSysMessage("检查装备栏...");
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item)
        {
            // handler.PSendSysMessage("装备栏位 {}: 物品ID={}, GUID={}", i, item->GetEntry(), item->GetGUID().GetCounter());
            if (item->GetEntry() == itemId)
            {
                // handler.PSendSysMessage("在装备栏找到匹配物品！");
                return item;
            }
        }
    }

    // 检查其他背包
    // handler.PSendSysMessage("检查其他背包...");
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bag = player->GetBagByPos(i);
        if (bag)
        {
            // handler.PSendSysMessage("检查背包 {}, 大小: {}", i, bag->GetBagSize());
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                if (item)
                {
                    // handler.PSendSysMessage("背包{}位{}: 物品ID={}, GUID={}", i, j, item->GetEntry(), item->GetGUID().GetCounter());
                    if (item->GetEntry() == itemId)
                    {
                        // handler.PSendSysMessage("在背包{}找到匹配物品！", i);
                        return item;
                    }
                }
            }
        }
    }

    // handler.PSendSysMessage("未找到物品ID: {}", itemId);
    return nullptr;
}

// 根据guid查找玩家物品（增强版，支持装备链接GUID查找）
Item* FindPlayerItemByGuid(Player* player, uint32 itemGuid)
{
    if (!player || itemGuid == 0)
        return nullptr;

    ChatHandler handler(player->GetSession());
    //handler.PSendSysMessage("强化系统：按GUID查找物品: {}", itemGuid);

    // 先检查背包（优先背包中的物品）
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item)
        {
            uint32 actualGuid = item->GetGUID().GetCounter();
            int32 storedGuid = item->GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID);

          //  handler.PSendSysMessage("强化系统：主背包位 {}: 实际GUID={}, 装备链接GUID={}", i, actualGuid, storedGuid);

            // 优先匹配实际GUID
            if (actualGuid == itemGuid)
            {
              //  handler.PSendSysMessage("强化系统：按实际GUID在主背包找到物品！");
                return item;
            }

            // 如果实际GUID不匹配，检查装备链接中存储的GUID
            if (storedGuid > 0 && static_cast<uint32>(storedGuid) == itemGuid)
            {
              //  handler.PSendSysMessage("强化系统：按装备链接GUID在主背包找到物品！");
                return item;
            }
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
                if (item)
                {
                    uint32 actualGuid = item->GetGUID().GetCounter();
                    int32 storedGuid = item->GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID);

                  //  handler.PSendSysMessage("强化系统：背包{}位{}: 实际GUID={}, 装备链接GUID={}", i, j, actualGuid, storedGuid);

                    // 优先匹配实际GUID
                    if (actualGuid == itemGuid)
                    {
                      //  handler.PSendSysMessage("强化系统：按实际GUID在背包{}找到物品！", i);
                        return item;
                    }

                    // 如果实际GUID不匹配，检查装备链接中存储的GUID
                    if (storedGuid > 0 && static_cast<uint32>(storedGuid) == itemGuid)
                    {
                       // handler.PSendSysMessage("强化系统：按装备链接GUID在背包{}找到物品！", i);
                        return item;
                    }
                }
            }
        }
    }

    // 最后检查装备栏
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item)
        {
            uint32 actualGuid = item->GetGUID().GetCounter();
            int32 storedGuid = item->GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID);

           // handler.PSendSysMessage("强化系统：装备栏位 {}: 实际GUID={}, 装备链接GUID={}", i, actualGuid, storedGuid);

            // 优先匹配实际GUID
            if (actualGuid == itemGuid)
            {
              //  handler.PSendSysMessage("强化系统：按实际GUID在装备栏找到物品！");
                return item;
            }

            // 如果实际GUID不匹配，检查装备链接中存储的GUID
            if (storedGuid > 0 && static_cast<uint32>(storedGuid) == itemGuid)
            {
              //  handler.PSendSysMessage("强化系统：按装备链接GUID在装备栏找到物品！");
                return item;
            }
        }
    }

   // handler.PSendSysMessage("强化系统：按GUID未找到物品: {}", itemGuid);
    return nullptr;
}

#define MODULE_ITEM_IDENTIFICATION_SYSTEM
#include "mod-item-identification-system/src/ItemIdentificationSystem.h"

// 【安全修复 2026-06-11】旧版自由函数 EnhanceItem 已删除：该路径不做需求模板校验/消耗，存在零成本强化漏洞，且与 ItemEnhancementMgr::EnhanceItem 行为分叉。所有调用统一走 sItemEnhancementMgr->EnhanceItem。

// 显示物品强化信息
void ShowEnhancementInfo(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    EnhancementRecord const* record = GetEnhancementRecord(itemGuid);

    if (!record)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("该物品未强化");
        return;
    }

    std::string info = "物品强化信息:\n";
    info += "强化等级: +" + Acore::ToString(record->level) + "\n";

    ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("%s", info.c_str()).c_str());
}

// 【已废弃】不再将GUID存储到randomPropertyId字段
// 原因：randomPropertyId字段用于存储物品的随机属性/随机后缀，覆盖会导致随机附魔丢失
// 强化系统现在完全依赖 物品强化_记录 表来存储和查询强化信息
void StoreGuidInEnchantmentSlot(Item* item)
{
    // 此函数已废弃，保留空实现以保持API兼容性
    // 强化信息通过 物品强化_记录 表的 guid 字段关联物品
    (void)item;
}

// 注意：ItemEnhancement_WorldScript类已被移除，使用ItemEnhancementLoader替代
// ItemEnhancementLoader类继承自ModuleDelayedLoaderBase，实现了延迟加载功能

// 注册脚本
void AddSC_ItemEnhancement()
{
    // 不再需要创建ItemEnhancement_WorldScript实例，因为ItemEnhancementLoader已经在AddSC_ItemEnhancementLoader中创建
}
