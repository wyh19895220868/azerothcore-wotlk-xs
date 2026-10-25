#include "AsInt256.h"
#include "StringConvert.h"
#include "ItemGrowthMgr.h"

template <typename... Args>
static void ItemGrowthTraceLog(Args&&...)
{
}
#include "Log.h"
#include "ScriptMgr.h"
#include "GameTime.h"
#include "Tokenize.h"
#include "Chat.h"
#include "ItemTemplate.h"
#include "DBCStores.h"
#include "Random.h"
#include "StringConvert.h"
#include "Util.h"
#include "WorldPacket.h"
#include "ObjectAccessor.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include "ModuleFormat.h"
#ifdef MODULE_ITEM_IDENTIFICATION_SYSTEM
#include "ItemIdentificationSystem.h"
#endif
#include <algorithm>
#include <limits>
#include <sstream>
#include <mutex>
#include <chrono>

namespace
{
uint64 RandomUInt64InRange(uint64 minValue, uint64 maxValue)
{
    if (maxValue <= minValue)
        return minValue;

    uint64 span = maxValue - minValue;
    uint64 randomValue = 0;
    for (uint8 i = 0; i < 4; ++i)
    {
        randomValue <<= 32;
        randomValue += rand32();
    }

    if (span == std::numeric_limits<uint64>::max())
        return randomValue;

    return minValue + (randomValue % (span + 1));
}

AsInt64 RandomInt64InRange(AsInt64 minValue, AsInt64 maxValue)
{
    return RandomAsInt256InRange(minValue, maxValue, [] { return rand32(); });
}

AsInt64 SaturatingAddInt64(AsInt64 const& left, AsInt64 const& right)
{
    if (right > 0 && left > std::numeric_limits<AsInt64>::max() - right)
        return std::numeric_limits<AsInt64>::max();

    if (right < 0 && left < std::numeric_limits<AsInt64>::min() - right)
        return std::numeric_limits<AsInt64>::min();

    return left + right;
}

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

#define MODULE_ITEM_ATTRIBUTES
#include "ItemAttributesGenerator.h"
#include "ItemAttributesLoader.h"

#define MODULE_ITEM_IDENTIFICATION_SYSTEM
#include "mod-item-identification-system/src/ItemIdentificationSystem.h"

void ItemGrowthMgr::Initialize()
{
    // 从配置文件加载设置
    _enabled = sConfigMgr->GetOption<bool>("ItemGrowth.Enable", true);
    _enabled = sConfigMgr->GetOption<bool>("ItemGrowth.Enable", true) && sHelper->GetBoolean(22011);
    _maxLevel = sConfigMgr->GetOption<uint32>("ItemGrowth.MaxLevel", 100);
    _baseExpMultiplier = sConfigMgr->GetOption<float>("ItemGrowth.BaseExpMultiplier", 1.0f);
    _upgradeSuccessRate = sConfigMgr->GetOption<uint32>("ItemGrowth.UpgradeSuccessRate", 80);
    _requireSoulbound = sConfigMgr->GetOption<bool>("ItemGrowth.RequireSoulbound", true);
    _autoApplyAttributes = sConfigMgr->GetOption<bool>("ItemGrowth.AutoApplyAttributes", true);

    if (_enabled)
    {
        //LOG_INFO("server.loading", "物品成长系统已启用");
        LoadFromDB();
    }
    else
    {
        LOG_INFO("server.loading", "物品成长系统已禁用");
    }
}

// 确保物品具备稳定的唯一seed（用于超链接唯一识别和客户端区分装备实例）
// 【修复】不再强制覆盖ITEM_FIELD_PROPERTY_SEED，该字段参与随机后缀/随机附魔属性计算
// 覆盖原seed会导致随机属性变化、链接校验异常或属性错乱
// 现在只在物品没有seed时才设置，保留原有随机属性
void ItemGrowthMgr::EnsureItemUniqueSeed(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint32 currentSeed = item->GetUInt32Value(ITEM_FIELD_PROPERTY_SEED);

    // 【修复】只有当seed为0时才设置，不覆盖已有的seed
    // 这样可以保留物品原有的随机属性
    if (currentSeed != 0)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("EnsureItemUniqueSeed: 物品%s(GUID:%s) 已有seed %s，保留原值避免破坏随机属性", item->GetEntry(), item->GetGUID().GetCounter(), currentSeed).c_str());
        return;
    }

    // 只有seed为0时才设置新值
    uint32 guidLow = item->GetGUID().GetCounter();
    item->SetUInt32Value(ITEM_FIELD_PROPERTY_SEED, guidLow);

    item->SetState(ITEM_CHANGED, player);

    // 强制刷新客户端：根据物品位置采取不同的刷新策略
    if (item->IsEquipped())
    {
        // 装备在身上：更新可见槽位，这会发送完整的物品数据包
        player->SetVisibleItemSlot(item->GetSlot(), item);
    }
    else
    {
        // 装备在背包：需要发送物品更新包
        item->SendUpdateToPlayer(player);

        // 保存到数据库，确保客户端同步时能获取到最新数据
        if (!_identificationWrite)
            item->SaveToDB(nullptr);
    }

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("EnsureItemUniqueSeed: 物品%s(GUID:%s) seed为0，设置新seed %s", item->GetEntry(), guidLow, guidLow).c_str());
}

void ItemGrowthMgr::LoadFromDB()
{
    // 清空现有数据
    _itemGrowthTemplates.clear();
    _playerItemRecords.clear();

    // 加载物品成长模板
    LoadItemGrowthTemplates();

    // 加载玩家物品成长记录
    uint32 oldMSTime = getMSTime();

    QueryResult result = CharacterDatabase.Query("SELECT id, 玩家GUID, 物品GUID, 物品模板ID, 当前等级, 当前经验, 升级经验, 物品属性_模板_组, 成长次数, 失败次数, 成长属性, 百分比属性 FROM 物品成长_玩家记录");

    if (!result)
    {
    }
    else
    {
        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            // 字段顺序：id(0), 玩家GUID(1), 物品GUID(2), 物品模板ID(3), 当前等级(4), 当前经验(5),
            //          升级经验(6), 物品属性_模板_组(7), 成长次数(8), 失败次数(9), 成长属性(10), 百分比属性(11)

            uint32 id = fields[0].Get<uint32>();
            uint32 itemGuid = fields[2].Get<uint32>();

            ItemGrowthPlayerRecord& record = _playerItemRecords[itemGuid];

            record.id = id;
            record.playerGuid = fields[1].Get<uint32>();           // 玩家GUID
            record.itemGuid = itemGuid;                            // 物品GUID
            record.itemEntry = fields[3].Get<uint32>();            // 物品模板ID
            record.currentLevel = fields[4].Get<uint32>();         // 当前等级
            record.currentExp = fields[5].Get<uint32>();           // 当前经验
            record.requiredExp = fields[6].Get<uint32>();          // 升级经验
            record.itemPropertyGroup = fields[7].Get<uint32>();    // 物品属性_模板_组
            record.growthCount = fields[8].Get<uint32>();          // 成长次数
            record.failCount = fields[9].Get<uint32>();            // 失败次数

            // 成长属性
            record.growthAttributes = fields[10].Get<std::string>(); // 成长属性
            record.percentAttributes = fields[11].Get<std::string>(); // 百分比属性

            // 兼容性字段（保持为0）
            record.currentAttributeValue = 0;
            record.currentAttributePercent = 0;

            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("LoadFromDB: 加载记录 - 物品GUID:%s 物品ID:%s 等级:%s 属性组:%s 成长属性:'%s' 百分比属性:'%s'", record.itemGuid, record.itemEntry, record.currentLevel, record.itemPropertyGroup, record.growthAttributes, record.percentAttributes).c_str());

            ++count;
        } while (result->NextRow());

        //LOG_INFO("server.loading", ">> 加载了{}条玩家物品成长记录数据，耗时{}毫秒", count, GetMSTimeDiffToNow(oldMSTime));
    }
}

void ItemGrowthMgr::LoadItemGrowthTemplates()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT * FROM _模板_物品成长");

    if (!result)
    {
        LOG_INFO("server.loading", ">> _模板_物品成长 表为空");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        // 根据实际数据库表结构读取字段
        // 字段顺序：注释(0), id(1), 需求模板(2), 组(3), 等级(4), 几率(5), 成长经验(6), 物品属性_模板_组(7),
        //          平均随机分配(8), 属性值1(9), 属性值2(10), 属性百分比1(11), 属性百分比2(12),
        //          达到该成长等级后的奖励属性(13), 达到该成长等级后的奖励属性百分比(14), 物品技能_模板id_多个逗号隔开(15)

        uint32 id = fields[1].Get<uint32>(); // id字段在索引1

        ItemGrowthTemplate& temp = _itemGrowthTemplates[id];

        temp.id = id;
        temp.requirementTemplateId = fields[2].Get<uint32>();           // 需求模板
        temp.group = fields[3].Get<uint32>();                           // 组
        temp.level = fields[4].Get<uint32>();                           // 等级
        temp.probability = fields[5].Get<uint32>();                     // 几率
        temp.requiredExp = fields[6].Get<uint32>();                     // 成长经验
        temp.itemPropertyGroup = fields[7].Get<uint32>();               // 物品属性_模板_组
        temp.attributeDistributionType = fields[8].Get<uint32>();       // 平均随机分配
        temp.minAttributeValue = AsInt64FromString(fields[9].Get<std::string>());               // 属性值1
        temp.maxAttributeValue = AsInt64FromString(fields[10].Get<std::string>());              // 属性值2
        temp.minAttributePercent = AsInt64FromString(fields[11].Get<std::string>());            // 属性百分比1
        temp.maxAttributePercent = AsInt64FromString(fields[12].Get<std::string>());            // 属性百分比2
        temp.levelRewardAttribute = AsInt64FromString(fields[13].Get<std::string>());           // 达到该成长等级后的奖励属性
        temp.levelRewardAttributePercent = AsInt64FromString(fields[14].Get<std::string>());    // 达到该成长等级后的奖励属性百分比
        temp.skillIds = fields[15].Get<std::string>();                  // 物品技能_模板id_多个逗号隔开

        ++count;
    } while (result->NextRow());

    //LOG_INFO("server.loading", ">> 加载了{}条物品成长模板数据，耗时{}毫秒", count, GetMSTimeDiffToNow(oldMSTime));
}

ItemGrowthTemplate const* ItemGrowthMgr::GetItemGrowthTemplate(uint32 id) const
{
    auto itr = _itemGrowthTemplates.find(id);
    if (itr != _itemGrowthTemplates.end())
        return &itr->second;

    return nullptr;
}

ItemGrowthTemplate const* ItemGrowthMgr::GetItemGrowthTemplateByLevel(uint32 level, uint32 group) const
{
    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GetItemGrowthTemplateByLevel: 查找等级 %s 属性组 %s 的模板，当前共有 %s 个模板", level, group, _itemGrowthTemplates.size()).c_str());

    // 严格按照指定的组查找模板
    // 如果没有找到对应组的模板，则表示该物品无法成长
    uint32 matchingGroupCount = 0;
    for (auto const& pair : _itemGrowthTemplates)
    {
        if (pair.second.group == group)
        {
            matchingGroupCount++;
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GetItemGrowthTemplateByLevel: 检查同组模板 ID %s 组 %s 等级 %s (目标等级 %s)", pair.second.id, pair.second.group, pair.second.level, level).c_str());
        }

        if (pair.second.group == group && pair.second.level == level)
        {
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GetItemGrowthTemplateByLevel: ✓找到匹配模板 ID %s 组 %s 等级 %s 经验 %s", pair.second.id, pair.second.group, pair.second.level, pair.second.requiredExp).c_str());
            return &pair.second;
        }
    }

    // 静默处理：未找到模板表示已达到最高等级，不输出任何信息

    return nullptr;
}

ItemGrowthPlayerRecord* ItemGrowthMgr::GetPlayerItemRecord(uint32 itemGuid)
{
    auto itr = _playerItemRecords.find(itemGuid);
    if (itr != _playerItemRecords.end())
        return &itr->second;

    return nullptr;
}

ItemGrowthPlayerRecord const* ItemGrowthMgr::GetPlayerItemRecord(uint32 itemGuid) const
{
    auto itr = _playerItemRecords.find(itemGuid);
    if (itr != _playerItemRecords.end())
        return &itr->second;

    return nullptr;
}

void ItemGrowthMgr::SavePlayerItemRecord(ItemGrowthPlayerRecord* record)
{
    if (!record)
        return;

    // 添加调试日志
    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("SavePlayerItemRecord: 保存物品GUID %s 等级 %s 当前经验 %s 升级经验 %s 属性组 %s ID:%s", record->itemGuid, record->currentLevel, record->currentExp, record->requiredExp, record->itemPropertyGroup, record->id).c_str());

    // 【审计修复】对属性字符串进行SQL转义，防止SQL注入
    // 属性字符串可能来自数据库或用户操作，需要转义特殊字符
    auto escapeString = [](const std::string& str) -> std::string {
        std::string result;
        result.reserve(str.size() * 2);
        for (char c : str)
        {
            switch (c)
            {
                case '\'': result += "''"; break;    // 单引号转义
                case '\\': result += "\\\\"; break;  // 反斜杠转义
                case '\0': result += "\\0"; break;   // NULL字符转义
                case '\n': result += "\\n"; break;   // 换行符转义
                case '\r': result += "\\r"; break;   // 回车符转义
                case '\x1a': result += "\\Z"; break; // Ctrl+Z转义
                default: result += c; break;
            }
        }
        return result;
    };

    std::string escapedGrowthAttributes = escapeString(record->growthAttributes);
    std::string escapedPercentAttributes = escapeString(record->percentAttributes);

    // 使用 INSERT ... ON DUPLICATE KEY UPDATE 避免重复键错误和竞争条件
    // 这种方式在异步执行时更安全，不需要先查询再决定INSERT或UPDATE
    std::string sql = "INSERT INTO 物品成长_玩家记录 (玩家GUID, 物品GUID, 物品模板ID, 当前等级, 当前经验, "
          "升级经验, 物品属性_模板_组, 成长次数, 失败次数, 成长属性, 百分比属性) VALUES ("
          + Acore::ToString(record->playerGuid) + ", "
          + Acore::ToString(record->itemGuid) + ", "
          + Acore::ToString(record->itemEntry) + ", "
          + Acore::ToString(record->currentLevel) + ", "
          + Acore::ToString(record->currentExp) + ", "
          + Acore::ToString(record->requiredExp) + ", "
          + Acore::ToString(record->itemPropertyGroup) + ", "
          + Acore::ToString(record->growthCount) + ", "
          + Acore::ToString(record->failCount) + ", "
          + "'" + escapedGrowthAttributes + "', "
          + "'" + escapedPercentAttributes + "') "
          "ON DUPLICATE KEY UPDATE "
          "玩家GUID = VALUES(玩家GUID), "
          "物品模板ID = VALUES(物品模板ID), "
          "当前等级 = VALUES(当前等级), "
          "当前经验 = VALUES(当前经验), "
          "升级经验 = VALUES(升级经验), "
          "物品属性_模板_组 = VALUES(物品属性_模板_组), "
          "成长次数 = VALUES(成长次数), "
          "失败次数 = VALUES(失败次数), "
          "成长属性 = VALUES(成长属性), "
          "百分比属性 = VALUES(百分比属性)";

    // 【读路径已内存化】运行期全部读取（ApplyGrowthAura/RemoveGrowthAura）已改走内存记录，
    // 不再存在"写后立即从 DB 回读"的依赖，恢复异步写（此前为修回读拿旧数据被迫同步）
    if (_identificationWrite)
        CharacterDatabase.DirectExecute(sql);
    else
        CharacterDatabase.Execute(sql);

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("SavePlayerItemRecord: 同步写入数据库完成 - 物品GUID: %s 等级: %s 经验: %s", record->itemGuid, record->currentLevel, record->currentExp).c_str());
}

void ItemGrowthMgr::DeletePlayerRecords(uint32 playerGuid)
{
    // 从内存中删除该玩家的所有记录
    auto it = _playerItemRecords.begin();
    while (it != _playerItemRecords.end())
    {
        if (it->second.playerGuid == playerGuid)
        {
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("DeletePlayerRecords: 从内存删除物品GUID %s 的记录", it->second.itemGuid).c_str());
            it = _playerItemRecords.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // 【审计修复】从数据库中删除该玩家的所有记录 - 使用同步DirectExecute确保数据一致性
    // 原因：角色删除是不可逆操作，必须确保数据库清理在操作完成前完成
    // 使用异步Execute可能导致角色删除完成后数据库仍有残留记录
    std::string sql = "DELETE FROM 物品成长_玩家记录 WHERE 玩家GUID = " + Acore::ToString(playerGuid);
    CharacterDatabase.DirectExecute(sql);
}

void ItemGrowthMgr::CreatePlayerItemRecord(Player* player, Item* item, uint32 propertyGroup)
{
    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();

    // 检查记录是否已存在
    if (GetPlayerItemRecord(itemGuid))
        return;

    ItemGrowthPlayerRecord record;
    record.id = 0; // 自动生成
    record.playerGuid = player->GetGUID().GetCounter();
    record.itemGuid = itemGuid;
    record.itemEntry = item->GetEntry();
    record.currentLevel = 1; // 修复：初始等级应该是1，而不是0
    record.currentExp = 0;
    record.requiredExp = 100; // 默认升级经验
    record.itemPropertyGroup = propertyGroup; // 使用传入的属性组参数
    record.growthCount = 0;
    record.failCount = 0;
    record.currentAttributeValue = 0;
    record.currentAttributePercent = 0;
    record.growthAttributes = ""; // 初始无成长属性
    record.percentAttributes = ""; // 初始无百分比属性

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CreatePlayerItemRecord: 创建物品GUID %s (物品ID:%s) 的成长记录，初始等级设置为 1，属性组: %s", itemGuid, item->GetEntry(), propertyGroup).c_str());

    // 注释掉基础属性检查：允许没有官方属性的装备也能成长
    // 成长将基于配置表中的属性模板组进行
    /*
    ItemTemplate const* itemTemplate = item->GetTemplate();
    bool hasBaseAttributes = false;

    if (itemTemplate)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (itemTemplate->ItemStat[i].ItemStatType > 0 && itemTemplate->AsItemStatValue[i] > 0)
            {
                hasBaseAttributes = true;
                break;
            }
        }
    }

    if (!hasBaseAttributes)
    {
        LOG_WARN("module.itemgrowth", ModuleOldFormat("CreatePlayerItemRecord: 装备 %s 没有基础属性，不能设置成长", item->GetEntry()).c_str());
        // 不创建成长记录，让调用者处理这种情况
        return;
    }
    */

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CreatePlayerItemRecord: 装备 %s 即使没有官方基础属性也允许成长，将基于属性组配置进行成长", item->GetEntry()).c_str());

    // 为有基础属性的装备设置成长模板
    ItemGrowthTemplate const* level1Template = GetItemGrowthTemplateByLevel(1, record.itemPropertyGroup);
    if (level1Template)
    {
        LOG_DEBUG("module.itemgrowth", "CreatePlayerItemRecord: 装备有基础属性，设置1级成长模板");

        // 正确设置升级经验需求：1级物品需要查找2级模板的经验要求
        ItemGrowthTemplate const* level2Template = GetItemGrowthTemplateByLevel(2, record.itemPropertyGroup);
        if (level2Template)
        {
            record.requiredExp = level2Template->requiredExp;
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CreatePlayerItemRecord: 1级物品升级到2级需要经验: %s", record.requiredExp).c_str());
        }
        else
        {
            // 如果没有2级模板，说明这个组只有1级，设置为0表示无法升级
            record.requiredExp = 0;
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CreatePlayerItemRecord: 属性组%s只有1级，无法升级", record.itemPropertyGroup).c_str());
        }

        // 为1级生成初始成长属性：
        // 当配置了属性组时，直接基于等级1模板和属性组生成一组成长属性，
        // 这样客户端在物品刚被设置为可成长时就能看到完整的成长属性列表。
        if (record.itemPropertyGroup > 0 && record.growthAttributes.empty())
        {
            GenerateLevelUpAttributesBasedOnItem(player, item, &record, level1Template);
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CreatePlayerItemRecord: 为物品GUID %s 生成1级初始成长属性: '%s'", record.itemGuid, record.growthAttributes).c_str());
        }
    }
    else
    {
        LOG_WARN("module.itemgrowth", ModuleOldFormat("CreatePlayerItemRecord: 找不到1级模板，属性组: %s", record.itemPropertyGroup).c_str());
    }

    // 第1步：保存到内存和数据库
    _playerItemRecords[itemGuid] = record;
    SavePlayerItemRecord(&_playerItemRecords[itemGuid]);

    // 第2步：为成长装备设置唯一seed（关键步骤）
    // 这样可以：
    // 1. 解决聊天链接"invalid link"问题
    // 2. 让客户端能够区分相同itemId的不同装备实例
    // 3. 让客户端插件能识别成长装备
    EnsureItemUniqueSeed(player, item);

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CreatePlayerItemRecord: 已保存1级记录到数据库并设置seed，属性: '%s'", record.growthAttributes).c_str());
}

bool ItemGrowthMgr::CanItemGrow(Item* item) const
{
    if (!_enabled || !sHelper->GetBoolean(22011))
        return false;

    if (!item)
    {
        LOG_DEBUG("module.itemgrowth", "CanItemGrow: 物品为空");
        return false;
    }

    // 检查物品是否可以成长
    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
    {
        LOG_DEBUG("module.itemgrowth", "CanItemGrow: 无法获取物品模板");
        return false;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CanItemGrow: 检查物品 %s (GUID:%s) 类型:%s 子类型:%s 品质:%s", itemTemplate->Name1, itemGuid, itemTemplate->Class, itemTemplate->SubClass, itemTemplate->Quality).c_str());

    // 检查物品类型
    // 只允许武器、护甲和饰品成长
    if (itemTemplate->Class != ITEM_CLASS_WEAPON &&
        itemTemplate->Class != ITEM_CLASS_ARMOR &&
        itemTemplate->Class != ITEM_CLASS_MISC)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CanItemGrow: 物品 %s 类型 %s 不支持成长", itemTemplate->Name1, itemTemplate->Class).c_str());
        return false;
    }

    // 检查物品品质
    // 只允许普通及以上品质的物品成长
    if (itemTemplate->Quality < ITEM_QUALITY_NORMAL)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CanItemGrow: 物品 %s 品质 %s 过低，不支持成长", itemTemplate->Name1, itemTemplate->Quality).c_str());
        return false;
    }

    // 检查物品是否绑定（如果配置要求）
    if (_requireSoulbound && !item->IsSoulBound())
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CanItemGrow: 物品 %s 未绑定，配置要求绑定才能成长", itemTemplate->Name1).c_str());
        return false;
    }

    // 首先检查是否有对应的成长记录
    ItemGrowthPlayerRecord const* record = GetPlayerItemRecord(itemGuid);
    if (record)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CanItemGrow: 物品GUID %s 有成长记录，等级:%s 属性组:%s", itemGuid, record->currentLevel, record->itemPropertyGroup).c_str());

        // 注意：0级记录的修复将在装备时或登录时进行，这里只检查是否有记录
        if (record->currentLevel == 0)
        {
            LOG_DEBUG("module.itemgrowth", "CanItemGrow: 发现0级记录，将在装备时修复");
        }

        // 如果有记录，则物品可成长
        return true;
    }

    // 修复：不能仅仅因为装备存储了GUID就认为它可成长
    // 因为强化系统等其他系统也可能使用相同字段存储GUID
    // 只有同时满足以下条件才认为装备可成长：
    // 1. 装备存储了GUID
    // 2. 有对应的成长记录
    int32 storedRandomProp = item->GetItemRandomPropertyId();
    int32 expectedRandomProp = static_cast<int32>(itemGuid);

    if (storedRandomProp == static_cast<int32>(itemGuid) || storedRandomProp == expectedRandomProp)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CanItemGrow: 物品GUID %s 存储了GUID标识(%s，但没有成长记录，可能是其他系统（如强化系统）使用", itemGuid, storedRandomProp).c_str());
        // 不返回true，因为没有成长记录就不应该被认为是可成长的
    }

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CanItemGrow: 物品 %s (GUID:%s) 不可成长 - 无成长记录", itemTemplate->Name1, itemGuid).c_str());
    return false;
}

void ItemGrowthMgr::SetItemCanGrow(Player* player, Item* item, uint32 propertyGroup)
{
    if (!_enabled || !sHelper->GetBoolean(22011))
        return;

    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();

    // 确保唯一seed，保证聊天链接包含唯一ID
    EnsureItemUniqueSeed(player, item);

    // 检查装备是否已经有随机属性
    int32 existingRandomProperty = 0;

    // 如果装备已经有随机属性，我们需要考虑是否覆盖（禁用：不再覆盖随机附魔字段）
    do { } while (0);

    // 不再将GUID写入随机附魔字段，避免与随机附魔系统冲突
    int32 guidAsRandomProperty = 0;

    // 根据装备位置进行不同的处理（完全复制强化系统逻辑）
    if (item->IsEquipped())
    {
        // 如果装备已装备，发送装备更新
        player->SetVisibleItemSlot(item->GetSlot(), item);
    }
    else
    {
        // 如果装备在背包中，需要特殊处理
    }

    // 发送装备更新到客户端（参考强化系统）
    item->SendUpdateToPlayer(player);

    // 立即验证存储结果（占位）
    // 始终认为验证通过，不进行随机附魔字段写入/重试

    // 创建或更新物品成长记录
    ItemGrowthPlayerRecord* record = GetPlayerItemRecord(itemGuid);
    if (!record)
    {
        CreatePlayerItemRecord(player, item, propertyGroup);
        record = GetPlayerItemRecord(itemGuid);

        // 检查是否创建成功（如果装备没有基础属性，CreatePlayerItemRecord会直接返回）
        if (!record)
        {
            LOG_WARN("module.itemgrowth", ModuleOldFormat("SetItemCanGrow: 无法为装备 %s 创建成长记录，可能是因为装备没有基础属性", item->GetEntry()).c_str());
            ChatHandler(player->GetSession()).PSendSysMessage("该装备没有基础属性，无法设置成长");
            return;
        }
    }

    if (record)
    {
        // 设置属性组
        record->itemPropertyGroup = propertyGroup;

        // 根据属性组设置初始升级经验（查找等级1的模板）
        ItemGrowthTemplate const* template1 = GetItemGrowthTemplateByLevel(1, propertyGroup);
        if (template1)
        {
            record->requiredExp = template1->requiredExp;
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("SetItemCanGrow: 装备已设置为可成长，等级1，属性组: %s，升级需要经验: %s", propertyGroup, record->requiredExp).c_str());
        }
        else
        {
            // 如果没有找到对应组的等级1模板，说明该组无法成长
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("错误：属性组 %s 没有配置等级1的成长模板，物品无法成长", propertyGroup).c_str());
            return;
        }

        // 第1步：保存记录到数据库
        SavePlayerItemRecord(record);

        // 第2步：强制保存物品到数据库，确保seed等字段持久化
        if (!_identificationWrite)
            item->SaveToDB(nullptr);

        // 第3步：强制刷新客户端显示
        // 根据物品位置采取不同的刷新策略
        if (item->IsEquipped())
        {
            // 装备在身上：更新可见槽位
            player->SetVisibleItemSlot(item->GetSlot(), item);
            LOG_INFO("module.itemgrowth", "SetItemCanGrow: 装备在身上，已更新可见槽位");
        }
        else
        {
            // 装备在背包：发送更新包
            item->SendUpdateToPlayer(player);
        }

        // 成长属性只在升级时生成，不在设置成长时生成
        // ChatHandler(player->GetSession()).PSendSysMessage("装备已设置为可成长，属性组：{}", propertyGroup);
        // ChatHandler(player->GetSession()).PSendSysMessage("提示：如果装备链接仍显示错误，请重新打开背包或重新登录");
    }
}

// 为鉴定系统设置成长：在创建成长记录和生成1级初始属性后，根据鉴定模板中的配置做一次性约束
void ItemGrowthMgr::SetItemCanGrowForIdentification(Player* player, Item* item, uint32 propertyGroup,
    uint32 minAttrCount, uint32 maxAttrCount, AsInt64 minAttrValue, AsInt64 maxAttrValue)
{
    // 鉴定需要即时验证及恢复结果，不能留下稍后覆盖恢复数据的异步写入。
    struct WriteScope
    {
        bool& flag;
        bool previous;
        ~WriteScope() { flag = previous; }
    } scope{_identificationWrite, _identificationWrite};
    _identificationWrite = true;

    // 先按正常流程创建成长记录并生成1级属性
    SetItemCanGrow(player, item, propertyGroup);

    if (!_enabled || !sHelper->GetBoolean(22011))
        return;

    uint32 itemGuid = item ? item->GetGUID().GetCounter() : 0;
    if (!itemGuid)
        return;

    ItemGrowthPlayerRecord* record = GetPlayerItemRecord(itemGuid);
    if (!record)
        return;

    // 如果没有任何成长属性，直接返回
    if (record->growthAttributes.empty())
        return;

    std::vector<std::pair<uint32, AsInt64>> attrs;
    ParseGrowthAttributes(record->growthAttributes, attrs);

    if (attrs.empty())
        return;

    // 1) 随机决定本次要保留的成长属性条目数量
    uint32 availableCount = static_cast<uint32>(attrs.size());
    uint32 targetCount = availableCount;

    // 如果鉴定模板配置了条目数量范围，则在该范围内随机取一个期望数量
    if (minAttrCount > 0 || maxAttrCount > 0)
    {
        uint32 minCount = minAttrCount;
        uint32 maxCount = maxAttrCount ? maxAttrCount : availableCount;

        if (minCount == 0)
            minCount = 1;

        if (minCount > maxCount)
            std::swap(minCount, maxCount);

        // 使用AzerothCore随机数在[minCount, maxCount]之间取一个值
        targetCount = urand(minCount, maxCount);
        if (targetCount > availableCount)
            targetCount = availableCount;
    }

    // 当可用条目数大于目标条目数时，从中随机抽取 targetCount 条
    if (availableCount > targetCount && targetCount > 0)
    {
        for (uint32 i = 0; i < targetCount; ++i)
        {
            uint32 j = urand(i, availableCount - 1);
            std::swap(attrs[i], attrs[j]);
        }
        attrs.resize(targetCount);
    }

    // 2) 按数值范围裁剪（如果配置了最小/最大属性值）
    bool hasRange = (minAttrValue > 0 || maxAttrValue > 0);
    if (hasRange)
    {
        // 处理 min/max 颠倒的情况
        if (maxAttrValue > 0 && minAttrValue > maxAttrValue)
            std::swap(minAttrValue, maxAttrValue);

        for (auto& p : attrs)
        {
            AsInt64 v = p.second;

            if (minAttrValue > 0 && v < minAttrValue)
                v = minAttrValue;

            if (maxAttrValue > 0 && v > maxAttrValue)
                v = maxAttrValue;

            p.second = v;
        }
    }

    // 3) 重新格式化回字符串
    record->growthAttributes = FormatGrowthAttributes(attrs);

    // 按照新的属性重新保存记录
    SavePlayerItemRecord(record);

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("SetItemCanGrowForIdentification: 约束后成长属性='%s' (minCount=%s, maxCount=%s, minValue=%s, maxValue=%s)", record->growthAttributes, minAttrCount, maxAttrCount, Acore::ToString(minAttrValue), Acore::ToString(maxAttrValue)).c_str());
}

bool ItemGrowthMgr::IsItemAttributesModuleAvailable() const
{
    return true;
}



void ItemGrowthMgr::ParseGrowthAttributes(const std::string& attributeString, std::vector<std::pair<uint32, AsInt64>>& attributes)
{
    attributes.clear();

    if (attributeString.empty())
        return;

    // 解析格式：7 5,3 10,4 8
    std::string str = attributeString;
    size_t pos = 0;

    while (pos < str.length())
    {
        // 查找下一个逗号或字符串结尾
        size_t commaPos = str.find(',', pos);
        if (commaPos == std::string::npos)
            commaPos = str.length();

        // 提取当前属性对
        std::string attrPair = str.substr(pos, commaPos - pos);

        // 去除前后空格
        size_t start = attrPair.find_first_not_of(" \t");
        size_t end = attrPair.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos)
        {
            attrPair = attrPair.substr(start, end - start + 1);

            // 查找空格分隔符
            size_t spacePos = attrPair.find(' ');
            if (spacePos != std::string::npos)
            {
                std::string typeStr = attrPair.substr(0, spacePos);
                std::string valueStr = attrPair.substr(spacePos + 1);

                Optional<uint32> type = Acore::StringTo<uint32>(typeStr);
                Optional<AsInt64> value = Acore::StringTo<AsInt64>(valueStr);
                if (type && value)
                {
                    attributes.push_back(std::make_pair(*type, *value));
                }
            }
        }

        pos = commaPos + 1;
    }
}

std::string ItemGrowthMgr::FormatGrowthAttributes(std::vector<std::pair<uint32, AsInt64>> const& attributes)
{
    if (attributes.empty())
        return "";

    std::string result;
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        if (i > 0)
            result += ",";

        result += Acore::ToString(attributes[i].first) + " " + Acore::ToString(attributes[i].second);
    }

    return result;
}

void ItemGrowthMgr::ApplyGrowthAttributes(Player* player, Item* item, const std::string& attributeString)
{
    if (!player || !item || attributeString.empty())
        return;

    std::vector<std::pair<uint32, AsInt64>> attributes;
    ParseGrowthAttributes(attributeString, attributes);

    if (attributes.empty())
        return;

    // 这里可以实现具体的属性应用逻辑
    // 例如：修改物品的附魔、添加临时属性等
    // 目前先记录日志
    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("为物品 %s 应用成长属性：%s", item->GetEntry(), attributeString).c_str());

    // 通知玩家
    ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("应用成长属性：%s", attributeString).c_str());
}

uint32 ItemGrowthMgr::CalculateRequiredExp(uint32 currentLevel, uint32 propertyGroup)
{
    uint32 nextLevel = currentLevel + 1;

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CalculateRequiredExp: 开始计算升级经验 - 当前等级 %s 属性组 %s 查找下一级 %s", currentLevel, propertyGroup, nextLevel).c_str());

    // 获取下一级的模板
    ItemGrowthTemplate const* nextTemplate = GetItemGrowthTemplateByLevel(nextLevel, propertyGroup);
    if (nextTemplate)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("CalculateRequiredExp: 找到下一级模板 - 当前等级 %s 属性组 %s 下一级(%s)需要经验 %s", currentLevel, propertyGroup, nextLevel, nextTemplate->requiredExp).c_str());
        return nextTemplate->requiredExp;
    }

    // 静默处理：已达到最高等级，返回0表示无法继续成长

    return 0;
}

void ItemGrowthMgr::UpdateRequiredExp(ItemGrowthPlayerRecord* record)
{
    if (!record)
        return;

    uint32 oldRequiredExp = record->requiredExp;
    record->requiredExp = CalculateRequiredExp(record->currentLevel, record->itemPropertyGroup);

    if (oldRequiredExp != record->requiredExp)
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("UpdateRequiredExp: 物品GUID %s 等级 %s 属性组 %s - 升级经验从 %s 更新为 %s", record->itemGuid, record->currentLevel, record->itemPropertyGroup, oldRequiredExp, record->requiredExp).c_str());
}

bool ItemGrowthMgr::AddItemExp(Player* player, Item* item, uint32 exp)
{
    if (!player || !item || exp == 0)
        return false;

    if (!CanItemGrow(item))
        return false;

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = GetPlayerItemRecord(itemGuid);

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("AddItemExp: 开始处理物品GUID %s (物品ID:%s) 添加经验 %s", itemGuid, item->GetEntry(), exp).c_str());

    // 如果物品没有成长记录，创建一个
    if (!record)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("AddItemExp: 物品GUID %s 没有成长记录，创建新记录", itemGuid).c_str());
        CreatePlayerItemRecord(player, item);
        record = GetPlayerItemRecord(itemGuid);
        if (!record)
        {
            LOG_ERROR("module.itemgrowth", ModuleOldFormat("AddItemExp: 无法为物品GUID %s 创建成长记录", itemGuid).c_str());
            return false;
        }
    }

    // 【审计修复】检查并修复异常数据
    // 调整阈值：等级上限从1000提高到10000，属性组上限从100提高到10000
    // 这样可以避免误判合法配置使用了更大组号的情况
    // 如果需要自定义阈值，可以通过配置文件设置
    constexpr uint32 MAX_VALID_LEVEL = 10000;          // 最大有效等级
    constexpr uint32 MAX_VALID_PROPERTY_GROUP = 10000; // 最大有效属性组号

    if (record->currentLevel > MAX_VALID_LEVEL || record->itemPropertyGroup > MAX_VALID_PROPERTY_GROUP)
    {
        LOG_ERROR("module.itemgrowth", ModuleOldFormat("AddItemExp: 发现异常数据 - 物品GUID %s 等级:%s 属性组:%s 数据异常（阈值: 等级<=%s, 属性组<=%s）！", itemGuid, record->currentLevel, record->itemPropertyGroup, MAX_VALID_LEVEL, MAX_VALID_PROPERTY_GROUP).c_str());

        ChatHandler(player->GetSession()).PSendSysMessage("检测到物品数据异常，正在修复...");
        ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("异常数据：等级 %s 属性组 %s", record->currentLevel, record->itemPropertyGroup).c_str());

        // 重置异常数据
        record->currentLevel = 1;
        record->currentExp = 0;
        record->itemPropertyGroup = 1; // 重置为默认属性组（从1开始）
        record->growthAttributes = "";
        record->requiredExp = 100;
        SavePlayerItemRecord(record);

        ChatHandler(player->GetSession()).PSendSysMessage("物品数据已重置：等级 1，属性组 1");
        LOG_WARN("module.itemgrowth", ModuleOldFormat("AddItemExp: 物品GUID %s 异常数据已重置", itemGuid).c_str());
    }

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("AddItemExp: 物品GUID %s 当前状态 - 等级:%s 经验:%s 升级经验:%s 属性组:%s", itemGuid, record->currentLevel, record->currentExp, record->requiredExp, record->itemPropertyGroup).c_str());

    // 检查是否还有下一级的模板（基于数据库配置的最高等级）
    uint32 nextLevel = record->currentLevel + 1;
    ItemGrowthTemplate const* nextTemplate = GetItemGrowthTemplateByLevel(nextLevel, record->itemPropertyGroup);

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("AddItemExp: 物品GUID %s 当前等级 %s 属性组 %s 寻找下一级 %s 的模板", record->itemGuid, record->currentLevel, record->itemPropertyGroup, nextLevel).c_str());

    if (!nextTemplate)
    {
        // 静默处理：已达到最高等级，不获取经验
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("AddItemExp: 物品GUID %s 已达到最高等级 %s，不再获取经验", record->itemGuid, record->currentLevel).c_str());
        return false;
    }

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("AddItemExp: 找到下一级模板 - 等级 %s 需要经验 %s", nextLevel, nextTemplate->requiredExp).c_str());

    // 获取当前等级的模板
    // 修复：如果当前等级为0，说明是旧数据，需要升级到等级1
    if (record->currentLevel == 0)
    {
        LOG_WARN("module.itemgrowth", ModuleOldFormat("AddItemExp: 物品GUID %s 等级为0，这是旧数据，自动升级到等级1", record->itemGuid).c_str());
        record->currentLevel = 1;
        SavePlayerItemRecord(record);
    }

    ItemGrowthTemplate const* currentTemplate = GetItemGrowthTemplateByLevel(record->currentLevel, record->itemPropertyGroup);
    if (!currentTemplate)
    {
        LOG_ERROR("module.itemgrowth", ModuleOldFormat("AddItemExp: 无法找到物品GUID %s 当前等级 %s 属性组 %s 的模板", record->itemGuid, record->currentLevel, record->itemPropertyGroup).c_str());
        return false;
    }

    // 应用基础经验倍率
    uint32 finalExp = static_cast<uint32>(exp * _baseExpMultiplier);

    // 更新物品经验
    record->currentExp += finalExp;

    // 检查是否可以升级
    bool levelUp = false;
    std::string preUpgradeAttributes = record->growthAttributes; // 保存升级前的属性

    while (true)
    {
        // 更新升级所需经验
        UpdateRequiredExp(record);

        // 检查经验是否足够升级
        // 【数据损坏修复】补 requiredExp > 0 防护：满级时 requiredExp=0，`currentExp >= 0` 恒真，
        // 原逻辑会先 currentLevel++ 再发现模板缺失才 break——等级被推到 max+1 并持久化。
        // 与 ProcessAccumulatedExp 的正确实现保持一致：升级前先确认下一级模板存在。
        if (record->requiredExp > 0 && record->currentExp >= record->requiredExp)
        {
            // 升级前确认下一级模板存在（不存在=已达最高等级，等级不再变动）
            ItemGrowthTemplate const* newTemplate = GetItemGrowthTemplateByLevel(record->currentLevel + 1, record->itemPropertyGroup);
            if (!newTemplate)
            {
                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("AddItemExp: 物品GUID %s 已达最高等级 %s，停止升级", record->itemGuid, record->currentLevel).c_str());
                break;
            }

            // 保存当前等级的属性（升级前）
            std::string currentLevelAttributes = record->growthAttributes;

            // 升级
            record->currentExp -= record->requiredExp;
            uint32 oldLevel = record->currentLevel;
            record->currentLevel++;
            levelUp = true;

            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("AddItemExp: 物品GUID %s 从等级 %s 升级到等级 %s (升级前属性:'%s')", record->itemGuid, oldLevel, record->currentLevel, currentLevelAttributes).c_str());

            // 生成本次升级的属性奖励 - 基于装备原有属性
            LOG_DEBUG("module.itemgrowth", "AddItemExp: 升级时使用基于装备原有属性的成长逻辑");
            GenerateLevelUpAttributesBasedOnItem(player, item, record, newTemplate);

            // 通知玩家
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("物品升级到%s级！", record->currentLevel).c_str());
        }
        else
        {
            break;
        }
    }

    // 更新升级经验需求
    UpdateRequiredExp(record);

    // 如果没有升级，直接保存记录
    if (!levelUp)
    {
        SavePlayerItemRecord(record);
    }

    // 处理升级后的属性刷新
    if (levelUp)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("AddItemExp: 物品GUID %s 升级完成，等级:%s 升级前属性:'%s' 升级后属性:'%s'", itemGuid, record->currentLevel, preUpgradeAttributes, record->growthAttributes).c_str());

        // 检查物品是否装备，只有装备状态下才刷新属性
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

        if (isEquipped)
        {
            SavePlayerItemRecord(record);

            ApplyGrowthAura(player, itemGuid);
            player->SetVisibleItemSlot(item->GetSlot(), item);
            player->UpdateAllStats();
            player->UpdateAttackPowerAndDamage();
            player->UpdateAttackPowerAndDamage(true);
            player->UpdateMaxHealth();
            player->UpdateMaxPower(POWER_MANA);
        }
        else
        {
            LOG_DEBUG("module.itemgrowth", "AddItemExp: 物品未装备，只保存数据不刷新属性");
            // 即使未装备也要保存升级后的数据
            SavePlayerItemRecord(record);
        }
    }
    else
    {
        // 静默处理：不显示经验获取通知
    }

    // 【性能优化】只有在升级时才发送更新到客户端
    // 避免每次获得少量经验都发送大量数据导致服务器卡顿
    if (levelUp)
    {
#ifdef MODULE_ITEM_IDENTIFICATION_SYSTEM
        // 升级后只清除该物品的缓存，不发送完整数据
        // 客户端会在下次查询时获取最新数据
        sItemIdentificationSystem->ClearItemCache(item->GetEntry(), itemGuid);
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("AddItemExp: 升级后已清除缓存 - 物品ID:%s GUID:%s", item->GetEntry(), itemGuid).c_str());
#endif

        // 【轻量更新】直接发送成长数据更新给客户端，不查询所有模块
        // 格式: GROWTH_UPDATE:itemID:guid:level:exp:requiredExp:growthAttrs:percentAttrs
        std::ostringstream growthUpdate;
        growthUpdate << "GROWTH_UPDATE:" << item->GetEntry() << ":" << itemGuid << ":"
                     << record->currentLevel << ":" << record->currentExp << ":"
                     << record->requiredExp << ":" << record->growthAttributes << ":"
                     << record->percentAttributes;

        std::string updateMsg = "UITQ\t" + growthUpdate.str();
        WorldPacket pkt;
        ChatHandler::BuildChatPacket(pkt, CHAT_MSG_WHISPER, LANG_ADDON,
                                     player, player, updateMsg, 0);
        player->SendDirectMessage(&pkt);
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("AddItemExp: 发送轻量成长更新 - %s", growthUpdate.str()).c_str());
    }

    return true;
}

// 将物品GUID存储到随机附魔槽中（参考强化系统）
void ItemGrowthMgr::StoreGuidInRandomProperty(Item* item)
{
    if (!item)
    {
        LOG_ERROR("module.itemgrowth", "StoreGuidInRandomProperty: item为空");
        return;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();
    Player* owner = item->GetOwner();

    if (!owner)
    {
        LOG_ERROR("module.itemgrowth", ModuleOldFormat("StoreGuidInRandomProperty: 物品ID %s GUID %s 没有所有者", item->GetEntry(), itemGuid).c_str());
        return;
    }

    LOG_INFO("module.itemgrowth", ModuleOldFormat("StoreGuidInRandomProperty: 跳过将物品ID %s GUID %s 写入随机属性字段（避免冲突）", item->GetEntry(), itemGuid).c_str());

    // 不再读取/依赖随机属性字段
    int32 currentRandomProperty = 0;

    // 不再将guid直接存储到随机属性ID字段
    // 这对应装备链接中的第8个字段（从0开始计数）

    // 完全按照强化系统的逻辑存储GUID（禁用）

    // 标记装备已修改
    item->SetState(ITEM_CHANGED, owner);

    // 根据装备位置进行不同的处理（完全复制强化系统逻辑）
    if (item->IsEquipped())
    {
        // 如果装备已装备，发送装备更新
        owner->SetVisibleItemSlot(item->GetSlot(), item);
        LOG_INFO("module.itemgrowth", "StoreGuidInRandomProperty: 装备已穿戴，更新装备槽位显示");
    }
    else
    {
        // 如果装备在背包中，需要特殊处理
        LOG_INFO("module.itemgrowth", ModuleOldFormat("StoreGuidInRandomProperty: 装备在背包中，位置: 背包%s 槽位%s", item->GetBagSlot(), item->GetSlot()).c_str());
    }

    // 不需要向客户端发送与随机属性相关的更新
    // 保持物品更新仅限必要的可见槽位刷新
}

bool ItemGrowthMgr::UpgradeItem(Player* player, Item* item)
{
    if (!player || !item)
        return false;

    if (!CanItemGrow(item))
        return false;

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = GetPlayerItemRecord(itemGuid);

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("UpgradeItem: 开始处理物品GUID %s 升级", itemGuid).c_str());

    // 如果物品没有成长记录，创建一个
    if (!record)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("UpgradeItem: 物品GUID %s 没有成长记录，创建新记录", itemGuid).c_str());
        CreatePlayerItemRecord(player, item);
        record = GetPlayerItemRecord(itemGuid);
        if (!record)
        {
            LOG_ERROR("module.itemgrowth", ModuleOldFormat("UpgradeItem: 无法为物品GUID %s 创建成长记录", itemGuid).c_str());
            return false;
        }

        // GUID存储已在SetItemCanGrow函数中完成，这里不需要重复存储
        LOG_DEBUG("module.itemgrowth", "UpgradeItem: 新创建的成长物品，GUID已在设置组时存储");
    }
    else
    {
        // 旧版本曾经依赖随机属性字段存储GUID；现在不再依赖，保留检查为Debug信息即可。
        int32 storedGuid = item->GetItemRandomPropertyId();
        if (storedGuid != static_cast<int32>(itemGuid))
        {
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("UpgradeItem: 物品GUID %s 的随机属性字段值为 %s，当前成长系统不再依赖该字段", itemGuid, storedGuid).c_str());
        }
        else
        {
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("UpgradeItem: 物品GUID %s 已正确存储在随机属性字段", itemGuid).c_str());
        }
    }

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("UpgradeItem: 物品GUID %s 当前状态 - 等级:%s 经验:%s 升级经验:%s 属性组:%s", itemGuid, record->currentLevel, record->currentExp, record->requiredExp, record->itemPropertyGroup).c_str());

    // 修复：如果当前等级为0，说明是旧数据，需要升级到等级1
    if (record->currentLevel == 0)
    {
        LOG_WARN("module.itemgrowth", ModuleOldFormat("UpgradeItem: 物品GUID %s 等级为0，这是旧数据，自动升级到等级1", record->itemGuid).c_str());
        record->currentLevel = 1;
        SavePlayerItemRecord(record);
    }

    // 检查是否还有下一级的模板（基于数据库配置的最高等级）
    ItemGrowthTemplate const* nextTemplate = GetItemGrowthTemplateByLevel(record->currentLevel + 1, record->itemPropertyGroup);
    if (!nextTemplate)
    {
        // 静默处理：已达到最高等级
        UpdateRequiredExp(record);
        SavePlayerItemRecord(record);
        return false;
    }

    // 获取当前等级的模板
    ItemGrowthTemplate const* currentTemplate = GetItemGrowthTemplateByLevel(record->currentLevel, record->itemPropertyGroup);
    if (!currentTemplate)
        return false;

    // 检查下一级模板的需求模板
    if (nextTemplate->requirementTemplateId > 0)
    {
        bool requirementMet = CheckNeedTemplate(player, nextTemplate->requirementTemplateId, true);
        if (!requirementMet)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("不满足升级到等级%s的需求条件。", record->currentLevel + 1).c_str());
            return false;
        }

        bool consumed = ConsumeNeedTemplate(player, nextTemplate->requirementTemplateId, "装备成长升级需求消耗");
        if (!consumed)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("消耗升级需求的资源失败。");
            return false;
        }
        ChatHandler(player->GetSession()).PSendSysMessage("已消耗升级需求的资源（金币等）");
    }

    // 检查升级成功率
    uint32 successRate = _upgradeSuccessRate;
    uint32 roll = urand(1, 100);

    if (roll > successRate)
    {
        // 升级失败
        record->failCount++;
        SavePlayerItemRecord(record);

        // 通知玩家
        ChatHandler(player->GetSession()).PSendSysMessage("物品升级失败！");
        return false;
    }

    // 保存升级前的属性（用于后续移除）
    std::string oldAttributes = record->growthAttributes;

    // 升级成功
    uint32 oldLevel = record->currentLevel;
    record->currentLevel++;
    record->currentExp = 0;
    record->growthCount++;

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("UpgradeItem: 物品GUID %s 从等级 %s 升级到等级 %s (升级前属性:'%s')", record->itemGuid, oldLevel, record->currentLevel, oldAttributes).c_str());

    // 获取新等级的模板
    ItemGrowthTemplate const* newTemplate = GetItemGrowthTemplateByLevel(record->currentLevel, record->itemPropertyGroup);
    if (newTemplate)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("UpgradeItem: 找到等级 %s 属性组 %s 的模板，ID:%s", record->currentLevel, record->itemPropertyGroup, newTemplate->id).c_str());

        // 所有属性组都使用基于装备原有属性的成长逻辑
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("UpgradeItem: 使用基于装备原有属性的成长逻辑 (属性组%s)", record->itemPropertyGroup).c_str());
        GenerateLevelUpAttributesBasedOnItem(player, item, record, newTemplate);
    }
    else
    {
        LOG_WARN("module.itemgrowth", ModuleOldFormat("UpgradeItem: 物品GUID %s 升级到等级 %s 后找不到对应模板 (属性组:%s)", record->itemGuid, record->currentLevel, record->itemPropertyGroup).c_str());
    }

    // 更新升级经验需求
    UpdateRequiredExp(record);

    // 注意：保存记录将在属性应用时进行，确保时序正确

    // 检查物品是否装备，只有装备状态下才应用属性
    bool isEquipped = false;
    uint8 equippedSlot = 255;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (equippedItem && equippedItem->GetGUID().GetCounter() == record->itemGuid)
        {
            isEquipped = true;
            equippedSlot = slot;
            break;
        }
    }

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("UpgradeItem: 物品GUID %s 装备状态检查 - %s (槽位:%s)", record->itemGuid, isEquipped ? "已装备" : "未装备", equippedSlot).c_str());

    if (isEquipped)
    {
        // 修复时序问题：先移除旧属性，再保存新数据，最后刷新属性
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("UpgradeItem: 开始处理装备属性刷新，物品GUID:%s 升级前属性:'%s' 升级后属性:'%s'", record->itemGuid, oldAttributes, record->growthAttributes).c_str());

        // 第1步：先移除升级前的旧属性
        if (!oldAttributes.empty())
        {
            LOG_INFO("module.itemgrowth", ModuleOldFormat("UpgradeItem: 第1步 - 移除升级前的属性: '%s'", oldAttributes).c_str());
            RemoveGrowthAttributesDirect(player, oldAttributes);
        }

        // 第2步：保存新的属性数据到数据库（确保数据库有最新数据）
        LOG_INFO("module.itemgrowth", "UpgradeItem: 第2步 - 保存升级后的新属性到数据库");
        SavePlayerItemRecord(record);

        // GUID存储已在SetItemCanGrow函数中完成，升级时不需要重复存储

        // 第3步：刷新属性
        LOG_INFO("module.itemgrowth", ModuleOldFormat("UpgradeItem: 第3步 - 刷新最新属性 - 物品GUID:%s", record->itemGuid).c_str());

        ApplyGrowthAura(player, itemGuid);
        player->SetVisibleItemSlot(item->GetSlot(), item);
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateMaxHealth();
        player->UpdateMaxPower(POWER_MANA);
    }
    else
    {
        LOG_DEBUG("module.itemgrowth", "UpgradeItem: 物品未装备，只保存数据不应用属性");
        // 即使未装备也要保存升级后的数据
        SavePlayerItemRecord(record);

        // GUID存储已在SetItemCanGrow函数中完成，这里不需要重复存储
    }

    // 通知玩家升级结果
    ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("物品升级成功！当前等级：%s", record->currentLevel).c_str());
    if (!record->growthAttributes.empty())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("获得成长属性：%s", record->growthAttributes).c_str());

        // 解析并显示详细属性信息
        std::vector<std::pair<uint32, AsInt64>> attributes;
        ParseGrowthAttributes(record->growthAttributes, attributes);
        if (!attributes.empty())
        {
            std::string detailStr = "详细属性：";
            for (size_t i = 0; i < attributes.size(); ++i)
            {
                if (i > 0) detailStr += ", ";
                detailStr += "类型" + Acore::ToString(attributes[i].first) + "+" + Acore::ToString(attributes[i].second);
            }
            ChatHandler(player->GetSession()).PSendSysMessage(detailStr.c_str());
        }

        // 只有装备时才显示属性应用状态和验证信息
        if (isEquipped)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("属性已应用到角色身上！");
            // 验证属性是否正确应用
            VerifyAttributesApplied(player, record->growthAttributes);
        }
        else
        {
            ChatHandler(player->GetSession()).PSendSysMessage("装备该物品后属性将生效！");
        }
    }
    else
    {
        LOG_WARN("module.itemgrowth", ModuleOldFormat("UpgradeItem: 物品GUID %s 升级后成长属性为空！", record->itemGuid).c_str());
    }

#ifdef MODULE_ITEM_IDENTIFICATION_SYSTEM
    // 成长属性变化后，立即刷新全局Tooltip插件缓存。
    // 否则锻造UI能看到新成长值，但物品悬停Tooltip仍显示旧缓存，直到强化等其他模块触发刷新。
    sItemIdentificationSystem->ClearItemCache(item->GetEntry(), itemGuid);
    sItemIdentificationSystem->SendAllModuleDataAddon(player, item->GetEntry(), itemGuid);
#endif

    return true;
}

void ItemGrowthMgr::ApplyItemAttributes(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = GetPlayerItemRecord(itemGuid);

    if (!record)
        return;

    // 修复：如果当前等级为0，说明是旧数据，需要升级到等级1
    if (record->currentLevel == 0)
    {
        LOG_WARN("module.itemgrowth", ModuleOldFormat("ApplyItemAttributes: 物品GUID %s 等级为0，这是旧数据，自动升级到等级1", record->itemGuid).c_str());
        record->currentLevel = 1;
        SavePlayerItemRecord(record);
    }

    // 获取当前等级的模板
    ItemGrowthTemplate const* currentTemplate = GetItemGrowthTemplateByLevel(record->currentLevel, record->itemPropertyGroup);
    if (!currentTemplate)
        return;

    // 物品属性组关联功能
    // 注意：现在所有组都是自定义属性组（从1开始），不再有官方装备成长
    if (_autoApplyAttributes)
    {
        // 非0组：自定义成长 - 使用mod-item-attributes模块
        // 如果模板中指定了物品属性组ID（>0），且启用了自动应用属性配置，
        // 则尝试与mod-item-attributes模块关联，为物品生成对应属性组的随机属性
#ifdef MODULE_ITEM_ATTRIBUTES
        // 调用物品属性系统来应用属性
        // 注意：此功能需要mod-item-attributes模块已安装并启用
        try
        {
            // 【BUG修复】检查物品是否已经通过鉴定系统生成过追加属性
            // 若已有鉴定来源的属性记录，跳过随机重新生成，避免属性叠加超限
#ifdef MODULE_ITEM_IDENTIFICATION_SYSTEM
            bool hasIdentifiedAttributes = sItemIdentificationSystem->IsItemIdentified(item->GetGUID().GetCounter());
            if (hasIdentifiedAttributes)
            {
                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("物品成长系统：物品 %s 已有鉴定属性，跳过随机属性生成，仅做成长增量", item->GetEntry()).c_str());
                // 已有鉴定属性时不重新随机生成，属性增量由成长属性字段(growthAttributes)单独维护
            }
            else
#endif
            {
                // 设置属性生成选项
                ItemAttributeGenerateOptions options;
                options.attributeGroup = currentTemplate->itemPropertyGroup;        // 使用模板中指定的属性组ID
                options.minAttributes = 1;                                          // 最少生成1个属性
                options.maxAttributes = 3;                                          // 基础最多生成3个属性
                options.minItemLevel = item->GetTemplate()->ItemLevel;              // 使用物品原始等级作为最低等级
                options.maxItemLevel = item->GetTemplate()->ItemLevel + record->currentLevel * 5; // 根据成长等级提升物品等级用于属性计算
                options.respectChance = true;                                       // 遵循属性模板中的获取几率设置
                options.allowDuplicateTypes = false;                               // 不允许重复的属性类型

                // 根据成长等级动态调整属性数量，等级越高属性越多
                if (record->currentLevel >= 10)
                    options.maxAttributes = 4;                                      // 10级以上最多4个属性
                if (record->currentLevel >= 20)
                    options.maxAttributes = 5;                                      // 20级以上最多5个属性

                // 生成随机属性
                bool success = sItemAttributesGenerator->GenerateRandomAttributes(item, options);

                if (success)
                {
                    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("物品成长系统：成功为物品 %s 应用属性组 %s 的属性", item->GetEntry(), currentTemplate->itemPropertyGroup).c_str());

                    // 通知玩家
                    ChatHandler(player->GetSession()).PSendSysMessage("物品获得了新的属性！");
                }
                else
                {
                    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("物品成长系统：为物品 %s 应用属性组 %s 失败", item->GetEntry(), currentTemplate->itemPropertyGroup).c_str());
                }
            }
        }
        catch (...)
        {
            LOG_ERROR("module.itemgrowth", "物品成长系统：调用物品属性模块时发生异常");
        }
#else
        // 如果没有物品属性模块，只记录日志
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("物品成长系统：物品 %s 关联属性组 %s，但物品属性模块未启用", item->GetEntry(), currentTemplate->itemPropertyGroup).c_str());
#endif
    }

    // 应用基础属性加成
    // 这里可以实现基础的属性应用逻辑，如生命值、法力值等
    // 注意：这些属性应用需要通过适当的方式实现，避免与其他系统冲突

    // 示例：根据成长等级提升物品的基础属性
    if (record->currentAttributeValue > 0 || record->currentAttributePercent > 0)
    {
    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("物品成长系统：物品 %s 当前属性值：%s，属性百分比：%s%%", item->GetEntry(), Acore::ToString(record->currentAttributeValue), Acore::ToString(record->currentAttributePercent)).c_str());
    }
}

// 强制应用物品属性组效果
// 此方法忽略AutoApplyAttributes配置设置，强制为物品应用属性组效果
// 主要用于手动命令触发，如 .物品成长 属性 命令
void ItemGrowthMgr::ForceApplyItemAttributes(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = GetPlayerItemRecord(itemGuid);

    if (!record)
        return;

    // 修复：如果当前等级为0，说明是旧数据，需要升级到等级1
    if (record->currentLevel == 0)
    {
        LOG_WARN("module.itemgrowth", ModuleOldFormat("ForceApplyItemAttributes: 物品GUID %s 等级为0，这是旧数据，自动升级到等级1", record->itemGuid).c_str());
        record->currentLevel = 1;
        SavePlayerItemRecord(record);
    }

    // 获取当前等级的模板
    ItemGrowthTemplate const* currentTemplate = GetItemGrowthTemplateByLevel(record->currentLevel, record->itemPropertyGroup);
    if (!currentTemplate)
        return;

    // 强制应用属性组，忽略AutoApplyAttributes配置设置
    // 注意：现在所有组都是自定义属性组（从1开始），不再有官方装备成长
    {
#ifdef MODULE_ITEM_ATTRIBUTES
        // 调用物品属性系统来应用属性
        try
        {
            // 【BUG修复】强制应用时也需检查是否已有鉴定来源的属性，避免属性叠加超限
#ifdef MODULE_ITEM_IDENTIFICATION_SYSTEM
            bool hasIdentifiedAttributes = sItemIdentificationSystem->IsItemIdentified(item->GetGUID().GetCounter());
            if (hasIdentifiedAttributes)
            {
                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("物品成长系统：ForceApply - 物品 %s 已有鉴定属性，跳过随机属性重新生成", item->GetEntry()).c_str());
                ChatHandler(player->GetSession()).PSendSysMessage("物品已有鉴定属性，成长增量已更新！");
                return;
            }
#endif
            // 设置属性生成选项
            ItemAttributeGenerateOptions options;
            options.attributeGroup = currentTemplate->itemPropertyGroup;
            options.minAttributes = 1;
            options.maxAttributes = 3; // 根据成长等级调整属性数量
            options.minItemLevel = item->GetTemplate()->ItemLevel;
            options.maxItemLevel = item->GetTemplate()->ItemLevel + record->currentLevel * 5; // 根据成长等级提升物品等级
            options.respectChance = true;
            options.allowDuplicateTypes = false;

            // 根据成长等级调整属性数量
            if (record->currentLevel >= 10)
                options.maxAttributes = 4;
            if (record->currentLevel >= 20)
                options.maxAttributes = 5;

            // 生成随机属性
            bool success = sItemAttributesGenerator->GenerateRandomAttributes(item, options);

            if (success)
            {
                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("物品成长系统：强制为物品 %s 应用属性组 %s 的属性", item->GetEntry(), currentTemplate->itemPropertyGroup).c_str());

                // 通知玩家
                ChatHandler(player->GetSession()).PSendSysMessage("物品获得了新的属性！");
            }
            else
            {
                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("物品成长系统：强制为物品 %s 应用属性组 %s 失败", item->GetEntry(), currentTemplate->itemPropertyGroup).c_str());

                // 通知玩家
                ChatHandler(player->GetSession()).PSendSysMessage("属性应用失败！");
            }
        }
        catch (...)
        {
            LOG_ERROR("module.itemgrowth", "物品成长系统：强制调用物品属性模块时发生异常");
            ChatHandler(player->GetSession()).PSendSysMessage("属性应用时发生错误！");
        }
#else
        // 如果没有物品属性模块，通知玩家
        ChatHandler(player->GetSession()).PSendSysMessage("物品属性模块未启用，无法应用属性组效果！");
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("物品成长系统：物品 %s 关联属性组 %s，但物品属性模块未启用", item->GetEntry(), currentTemplate->itemPropertyGroup).c_str());
#endif
    }
}

// 应用官方装备成长（0组）
// 基于装备原有的官方属性进行成长
void ItemGrowthMgr::ApplyOfficialItemGrowth(Player* player, Item* item, ItemGrowthPlayerRecord* record, const ItemGrowthTemplate* currentTemplate)
{
    if (!player || !item || !record || !currentTemplate)
        return;

    // 获取装备的原始模板
    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
        return;

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("应用官方装备成长：物品 %s 等级 %s", item->GetEntry(), record->currentLevel).c_str());

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: 模板配置 - 属性值1:%s 属性值2:%s 等级奖励属性:%s 等级奖励属性百分比:%s", Acore::ToString(currentTemplate->minAttributeValue), Acore::ToString(currentTemplate->maxAttributeValue), Acore::ToString(currentTemplate->levelRewardAttribute), Acore::ToString(currentTemplate->levelRewardAttributePercent)).c_str());

    // 基于装备原有属性进行成长
    // 对于属性组0，优先使用levelRewardAttribute作为成长值
    AsInt64 growthValue = 0;

    if (currentTemplate->levelRewardAttribute > 0)
    {
        // 使用等级奖励属性作为成长值
        growthValue = currentTemplate->levelRewardAttribute;
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: 使用等级奖励属性作为成长值: %s", Acore::ToString(growthValue)).c_str());
    }
    else if (currentTemplate->minAttributeValue > 0 || currentTemplate->maxAttributeValue > 0)
    {
        // 如果没有等级奖励属性，则使用属性值1和属性值2的平均值
        growthValue = (currentTemplate->minAttributeValue + currentTemplate->maxAttributeValue) / 2;
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: 使用属性值平均值作为成长值: %s (%s+%s)/2", Acore::ToString(growthValue), Acore::ToString(currentTemplate->minAttributeValue), Acore::ToString(currentTemplate->maxAttributeValue)).c_str());
    }

    if (growthValue > 0)
    {
        // 构建成长属性字符串，基于装备原有的属性类型
        std::vector<std::pair<uint32, AsInt64>> growthAttributes;

        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: 开始检查装备原有属性，物品ID:%s 成长值:%s", item->GetEntry(), Acore::ToString(growthValue)).c_str());

        // 检查装备原有的基础属性并进行成长
        bool foundOriginalAttributes = false;

        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: 开始遍历装备的 %s 个属性槽", MAX_ITEM_PROTO_STATS).c_str());

        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            uint32 statType = itemTemplate->ItemStat[i].ItemStatType;
            AsInt64 statValue = itemTemplate->AsItemStatValue[i];
            std::string statValueText = Acore::ToString(statValue);

            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: 属性槽%s - 类型:%s 值:%s", i, statType, statValueText).c_str());

            if (statType > 0 && statValue > 0)
            {
                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: ✓ 发现有效属性 - 类型:%s 原始值:%s", statType, statValueText).c_str());

                // 基于原始属性值计算成长值
                // 使用固定成长值，确保成长是可预测的
                AsInt64 finalGrowthValue = growthValue;

                // 可选：基于原始属性值的比例成长（取消注释以启用）
                // AsInt64 finalGrowthValue = growthValue;

                if (finalGrowthValue > 0)
                {
                    growthAttributes.push_back(std::make_pair(statType, finalGrowthValue));
                    foundOriginalAttributes = true;

                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: ✓ 添加成长属性 - 类型:%s 成长值:%s (基于原始值:%s)", statType, Acore::ToString(finalGrowthValue), statValueText).c_str());

                    // 通知玩家具体的成长信息
                    ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("属性类型%s从%s增长%s点", statType, statValueText, Acore::ToString(finalGrowthValue)).c_str());
                }
            }
            else if (statType > 0 || statValue != 0)
            {
                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: ✗ 跳过无效属性 - 类型:%s 值:%s", statType, statValueText).c_str());
            }
        }

        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: 属性槽遍历完成，找到%s个有效属性", foundOriginalAttributes ? growthAttributes.size() : 0).c_str());

        if (!foundOriginalAttributes)
        {
            LOG_WARN("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: 装备 %s 没有任何基础属性可以成长！", item->GetEntry()).c_str());

            // 输出装备的详细信息用于调试
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: 装备详细信息 - ID:%s 名称:'%s' 类型:%s 子类型:%s 品质:%s 等级:%s", item->GetEntry(), itemTemplate->Name1, itemTemplate->Class, itemTemplate->SubClass, itemTemplate->Quality, itemTemplate->ItemLevel).c_str());

            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: 装备护甲:%s 耐久度:%s", Acore::ToString(itemTemplate->AsArmor), itemTemplate->MaxDurability).c_str());

            // 输出所有属性槽的信息
            for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
            {
                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ApplyOfficialItemGrowth: 属性槽%s - 类型:%s 值:%s", i, itemTemplate->ItemStat[i].ItemStatType, Acore::ToString(itemTemplate->AsItemStatValue[i])).c_str());
            }

            // 对于没有基础属性的装备，不强制添加任何属性
            // 让玩家知道这个装备无法通过官方成长方式获得属性
            ChatHandler(player->GetSession()).PSendSysMessage("该装备没有基础属性，无法进行官方装备成长！");
            ChatHandler(player->GetSession()).PSendSysMessage("建议使用其他属性组进行成长，或检查装备是否适合成长系统。");
        }

        // 如果有成长属性，应用到物品记录中
        if (!growthAttributes.empty())
        {
            // 获取现有的成长属性
            std::vector<std::pair<uint32, AsInt64>> existingAttributes;
            ParseGrowthAttributes(record->growthAttributes, existingAttributes);

            // 合并属性
            for (const auto& newAttr : growthAttributes)
            {
                bool found = false;
                for (auto& existingAttr : existingAttributes)
                {
                    if (existingAttr.first == newAttr.first)
                    {
                        existingAttr.second = SaturatingAddInt64(existingAttr.second, newAttr.second);
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    existingAttributes.push_back(newAttr);
                }
            }

            // 更新成长属性字符串
            record->growthAttributes = FormatGrowthAttributes(existingAttributes);

            // 通知玩家具体的成长信息
            std::string growthInfo = "装备基于原始属性获得成长：";
            for (const auto& attr : growthAttributes)
            {
                growthInfo += "属性" + Acore::ToString(attr.first) + "+" + Acore::ToString(attr.second) + " ";
            }
            ChatHandler(player->GetSession()).PSendSysMessage(growthInfo.c_str());

            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("官方装备成长完成：物品 %s 成长属性 %s", item->GetEntry(), record->growthAttributes).c_str());
        }
    }
    else
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("官方装备成长模板 %s 没有设置成长值", currentTemplate->id).c_str());
    }
}

// 生成升级时的属性奖励 - 基于装备原有属性进行成长
// 注意：此函数已弃用，请使用 GenerateLevelUpAttributesBasedOnItem 替代
void ItemGrowthMgr::GenerateLevelUpAttributes(ItemGrowthPlayerRecord* record, const ItemGrowthTemplate* levelTemplate)
{
    if (!record || !levelTemplate)
    {
        LOG_ERROR("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 参数为空 - record:%s levelTemplate:%s", (void*)record, (void*)levelTemplate).c_str());
        return;
    }

    LOG_WARN("module.itemgrowth", "GenerateLevelUpAttributes: 此函数已弃用，无法获取装备信息进行基于原有属性的成长");
    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 为物品GUID %s 等级 %s 生成属性奖励 (使用默认逻辑)", record->itemGuid, record->currentLevel).c_str());

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 模板信息 - ID:%s 分配类型:%s 等级奖励属性:%s 等级奖励属性百分比:%s 最小属性值:%s 最大属性值:%s 最小属性百分比:%s 最大属性百分比:%s", levelTemplate->id, levelTemplate->attributeDistributionType, Acore::ToString(levelTemplate->levelRewardAttribute), Acore::ToString(levelTemplate->levelRewardAttributePercent), Acore::ToString(levelTemplate->minAttributeValue), Acore::ToString(levelTemplate->maxAttributeValue), Acore::ToString(levelTemplate->minAttributePercent), Acore::ToString(levelTemplate->maxAttributePercent)).c_str());

    // 由于无法获取装备信息，只能使用默认的成长逻辑
    std::vector<std::pair<uint32, AsInt64>> newAttributes;

    // 计算成长值
    AsInt64 growthValue = 0;
    if (levelTemplate->levelRewardAttribute > 0)
    {
        growthValue = levelTemplate->levelRewardAttribute;
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 使用等级奖励属性作为成长值: %s", Acore::ToString(growthValue)).c_str());
    }
    else if (levelTemplate->attributeDistributionType == 1)
    {
        // 随机分配模式：使用随机值
        if (levelTemplate->maxAttributeValue >= levelTemplate->minAttributeValue)
        {
            growthValue = RandomInt64InRange(levelTemplate->minAttributeValue, levelTemplate->maxAttributeValue);
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 使用随机成长值: %s (范围:%s-%s)", Acore::ToString(growthValue), Acore::ToString(levelTemplate->minAttributeValue), Acore::ToString(levelTemplate->maxAttributeValue)).c_str());
        }
        else
        {
            LOG_ERROR("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 配置错误 - maxAttributeValue(%s) < minAttributeValue(%s)", Acore::ToString(levelTemplate->maxAttributeValue), Acore::ToString(levelTemplate->minAttributeValue)).c_str());
            growthValue = levelTemplate->minAttributeValue;
        }
    }
    else
    {
        // 平均分配模式：使用平均值
        growthValue = (levelTemplate->minAttributeValue + levelTemplate->maxAttributeValue) / 2;
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 使用平均成长值: %s (%s+%s)/2", Acore::ToString(growthValue), Acore::ToString(levelTemplate->minAttributeValue), Acore::ToString(levelTemplate->maxAttributeValue)).c_str());
    }

    if (growthValue == 0)
    {
        LOG_WARN("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 物品GUID %s 等级 %s 计算出的成长值为0！模板ID:%s", record->itemGuid, record->currentLevel, levelTemplate->id).c_str());
        return;
    }

    // 由于无法获取装备原有属性，使用默认的属性分配逻辑
    if (levelTemplate->attributeDistributionType == 0)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 使用平均分配模式（默认耐力），成长值:%s", Acore::ToString(growthValue)).c_str());

        // 0 = 平均分配：由于无法获取装备原有属性，使用默认耐力属性
        if (growthValue > 0)
        {
            // 添加耐力属性作为默认成长属性
            newAttributes.push_back(std::make_pair(7, growthValue)); // 耐力
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 添加默认耐力属性 %s 点", Acore::ToString(growthValue)).c_str());
        }

        // 如果有百分比奖励，也添加
        if (levelTemplate->levelRewardAttributePercent > 0)
        {
            newAttributes.push_back(std::make_pair(0, levelTemplate->levelRewardAttributePercent)); // 法力值
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 添加法力值属性 %s 点", Acore::ToString(levelTemplate->levelRewardAttributePercent)).c_str());
        }
    }
    else
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 使用随机分配模式，成长值:%s", Acore::ToString(growthValue)).c_str());

        // 1 = 随机分配：随机选择属性类型并分配成长值
        if (growthValue > 0)
        {
            // 随机选择一个基础属性类型
            // 基础属性：敏捷(3)、力量(4)、智力(5)、精神(6)、耐力(7)
            uint32 basicAttributes[] = {3, 4, 5, 6, 7};
            uint32 randomIndex = urand(0, 4);
            uint32 randomAttrType = basicAttributes[randomIndex];
            newAttributes.push_back(std::make_pair(randomAttrType, growthValue));
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 添加随机基础属性类型 %s 数值 %s", randomAttrType, Acore::ToString(growthValue)).c_str());
        }

        // 如果有百分比奖励，也随机分配
        if (levelTemplate->levelRewardAttributePercent > 0)
        {
            if (levelTemplate->maxAttributePercent >= levelTemplate->minAttributePercent)
            {
                AsInt64 randomAttributePercent = RandomInt64InRange(levelTemplate->minAttributePercent, levelTemplate->maxAttributePercent);
                if (randomAttributePercent > 0)
                {
                    // 随机选择法力值(0)或生命值(1)
                    uint32 percentAttributes[] = {0, 1};
                    uint32 randomIndex = urand(0, 1);
                    uint32 randomPercentType = percentAttributes[randomIndex];
                    newAttributes.push_back(std::make_pair(randomPercentType, randomAttributePercent));
                    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 添加随机百分比属性类型 %s 数值 %s", randomPercentType, Acore::ToString(randomAttributePercent)).c_str());
                }
            }
            else
            {
                LOG_ERROR("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 配置错误 - maxAttributePercent(%s) < minAttributePercent(%s)", Acore::ToString(levelTemplate->maxAttributePercent), Acore::ToString(levelTemplate->minAttributePercent)).c_str());
            }
        }
        else if (levelTemplate->minAttributePercent > 0 || levelTemplate->maxAttributePercent > 0)
        {
            if (levelTemplate->maxAttributePercent >= levelTemplate->minAttributePercent)
            {
                AsInt64 randomAttributePercent = RandomInt64InRange(levelTemplate->minAttributePercent, levelTemplate->maxAttributePercent);
                if (randomAttributePercent > 0)
                {
                    // 随机选择法力值(0)或生命值(1)
                    uint32 percentAttributes[] = {0, 1};
                    uint32 randomIndex = urand(0, 1);
                    uint32 randomPercentType = percentAttributes[randomIndex];
                    newAttributes.push_back(std::make_pair(randomPercentType, randomAttributePercent));
                    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 添加随机百分比属性类型 %s 数值 %s", randomPercentType, Acore::ToString(randomAttributePercent)).c_str());
                }
            }
            else
            {
                LOG_ERROR("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 配置错误 - maxAttributePercent(%s) < minAttributePercent(%s)", Acore::ToString(levelTemplate->maxAttributePercent), Acore::ToString(levelTemplate->minAttributePercent)).c_str());
            }
        }
    }

    // 将新属性添加到成长属性中
    if (!newAttributes.empty())
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 生成了 %s 个新属性", newAttributes.size()).c_str());

        std::vector<std::pair<uint32, AsInt64>> existingAttributes;
        ParseGrowthAttributes(record->growthAttributes, existingAttributes);

        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 现有属性字符串: '%s'", record->growthAttributes).c_str());
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 解析出 %s 个现有属性", existingAttributes.size()).c_str());

        // 合并属性
        for (const auto& newAttr : newAttributes)
        {
            bool found = false;
            for (auto& existingAttr : existingAttributes)
            {
                if (existingAttr.first == newAttr.first)
                {
                    AsInt64 mergedValue = SaturatingAddInt64(existingAttr.second, newAttr.second);
                    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 合并属性类型 %s 原值:%s 新增:%s 合计:%s", existingAttr.first, Acore::ToString(existingAttr.second), Acore::ToString(newAttr.second), Acore::ToString(mergedValue)).c_str());
                    existingAttr.second = mergedValue;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 添加新属性类型 %s 数值 %s", newAttr.first, Acore::ToString(newAttr.second)).c_str());
                existingAttributes.push_back(newAttr);
            }
        }

        // 更新成长属性字符串
        std::string oldAttributes = record->growthAttributes;
        record->growthAttributes = FormatGrowthAttributes(existingAttributes);

        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 物品GUID %s 属性更新 - 旧:'%s'  新:'%s'", record->itemGuid, oldAttributes, record->growthAttributes).c_str());
    }
    else
    {
        LOG_WARN("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributes: 物品GUID %s 等级 %s 没有生成任何新属性！", record->itemGuid, record->currentLevel).c_str());
    }
}



// 应用单个属性到玩家身上 - 使用正确的属性修改器系统
void ItemGrowthMgr::ApplyAttributeToPlayer(Player* player, uint32 attrType, AsInt64 attrValue, bool isAdd)
{
    if (!player || attrValue == 0)
        return;

    // Keep the full value in the player stat store and use only a safe compatibility projection.
    player->DoApplyStat(ItemModType(attrType), attrValue, isAdd);
    player->UpdateAllStats();

    if (attrType == ITEM_MOD_ATTACK_POWER)
        player->UpdateAttackPowerAndDamage();
    else if (attrType == ITEM_MOD_RANGED_ATTACK_POWER)
        player->UpdateAttackPowerAndDamage(true);
}


// 应用百分比属性到玩家身上 - 真正的百分比计算
void ItemGrowthMgr::ApplyPercentAttributeToPlayer(Player* player, uint32 attrType, AsInt64 percentValue, bool isAdd)
{
    if (!player || percentValue <= 0)
        return;

    LOG_INFO("module.itemgrowth", ModuleOldFormat("ApplyPercentAttributeToPlayer: %s 属性类型:%s 百分比:%s%% 到玩家:%s", isAdd ? "应用" : "移除", attrType, Acore::ToString(percentValue), player->GetName()).c_str());

    AsInt64 actualValue = 0;
    switch (attrType)
    {
        case ITEM_MOD_MANA:
            actualValue = Acore::Number::CalculatePct(static_cast<AsInt64>(player->GetCreateMana()), percentValue);
            break;
        case ITEM_MOD_HEALTH:
            actualValue = Acore::Number::CalculatePct(static_cast<AsInt64>(player->GetCreateHealth()), percentValue);
            break;
        case ITEM_MOD_AGILITY:
            actualValue = Acore::Number::CalculatePct(static_cast<AsInt64>(player->GetCreateStat(STAT_AGILITY)), percentValue);
            break;
        case ITEM_MOD_STRENGTH:
            actualValue = Acore::Number::CalculatePct(static_cast<AsInt64>(player->GetCreateStat(STAT_STRENGTH)), percentValue);
            break;
        case ITEM_MOD_INTELLECT:
            actualValue = Acore::Number::CalculatePct(static_cast<AsInt64>(player->GetCreateStat(STAT_INTELLECT)), percentValue);
            break;
        case ITEM_MOD_SPIRIT:
            actualValue = Acore::Number::CalculatePct(static_cast<AsInt64>(player->GetCreateStat(STAT_SPIRIT)), percentValue);
            break;
        case ITEM_MOD_STAMINA:
            actualValue = Acore::Number::CalculatePct(static_cast<AsInt64>(player->GetCreateStat(STAT_STAMINA)), percentValue);
            break;
        default:
            // Other attribute types use the configured percentage as a direct bonus.
            actualValue = percentValue;
            break;
    }

    if (actualValue > 0)
        ApplyAttributeToPlayer(player, attrType, actualValue, isAdd);

    LOG_INFO("module.itemgrowth", ModuleOldFormat("ApplyPercentAttributeToPlayer: 完成%s 百分比属性 - 类型:%s 百分比:%s%% 实际值:%s", isAdd ? "应用" : "移除", attrType, Acore::ToString(percentValue), Acore::ToString(actualValue)).c_str());
}



// 基于装备原有属性生成成长属性的新方法
void ItemGrowthMgr::GenerateLevelUpAttributesBasedOnItem(Player* player, Item* item, ItemGrowthPlayerRecord* record, const ItemGrowthTemplate* levelTemplate)
{
    if (!player || !item || !record || !levelTemplate)
    {
        LOG_ERROR("module.itemgrowth", "GenerateLevelUpAttributesBasedOnItem: 参数为空");
        return;
    }

    // 获取装备的原始模板
    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
    {
        LOG_ERROR("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 无法获取物品模板，物品ID:%s", item->GetEntry()).c_str());
        return;
    }

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 为物品GUID %s (ID:%s) 等级 %s 基于原有属性生成成长属性", record->itemGuid, item->GetEntry(), record->currentLevel).c_str());

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 模板信息 - ID:%s 分配类型:%s 等级奖励属性:%s 等级奖励属性百分比:%s 最小属性值:%s 最大属性值:%s 最小属性百分比:%s 最大属性百分比:%s", levelTemplate->id, levelTemplate->attributeDistributionType, Acore::ToString(levelTemplate->levelRewardAttribute), Acore::ToString(levelTemplate->levelRewardAttributePercent), Acore::ToString(levelTemplate->minAttributeValue), Acore::ToString(levelTemplate->maxAttributeValue), Acore::ToString(levelTemplate->minAttributePercent), Acore::ToString(levelTemplate->maxAttributePercent)).c_str());

    // 检查是否有属性值配置
    bool hasAttributeValue = (levelTemplate->levelRewardAttribute > 0) ||
                            (levelTemplate->minAttributeValue > 0) ||
                            (levelTemplate->maxAttributeValue > 0);

    // 检查是否有百分比属性配置
    bool hasPercentAttribute = (levelTemplate->levelRewardAttributePercent > 0) ||
                              (levelTemplate->minAttributePercent > 0) ||
                              (levelTemplate->maxAttributePercent > 0);

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 配置检查 - 有属性值:%s 有百分比属性:%s", hasAttributeValue, hasPercentAttribute).c_str());

    // 计算成长值
    AsInt64 growthValue = 0;
    bool useAttributeValue = false;

    if (hasAttributeValue)
    {
        useAttributeValue = true;
        if (levelTemplate->levelRewardAttribute > 0)
        {
            growthValue = levelTemplate->levelRewardAttribute;
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 使用等级奖励属性作为成长值: %s", Acore::ToString(growthValue)).c_str());
        }
        else if (levelTemplate->attributeDistributionType == 1)
        {
            // 随机分配模式：使用随机值
            // 修复：确保min <= max，避免urand断言错误
            AsInt64 minVal = levelTemplate->minAttributeValue;
            AsInt64 maxVal = levelTemplate->maxAttributeValue;
            if (minVal > maxVal)
            {
                LOG_WARN("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 配置错误 - minAttributeValue(%s) > maxAttributeValue(%s)，交换数值", Acore::ToString(minVal), Acore::ToString(maxVal)).c_str());
                std::swap(minVal, maxVal);
            }
            growthValue = RandomInt64InRange(minVal, maxVal);
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 使用随机成长值: %s (范围:%s-%s)", Acore::ToString(growthValue), Acore::ToString(minVal), Acore::ToString(maxVal)).c_str());
        }
        else
        {
            // 平均分配模式：使用平均值
            growthValue = (levelTemplate->minAttributeValue + levelTemplate->maxAttributeValue) / 2;
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 使用平均成长值: %s (%s+%s)/2", Acore::ToString(growthValue), Acore::ToString(levelTemplate->minAttributeValue), Acore::ToString(levelTemplate->maxAttributeValue)).c_str());
        }
    }
    else if (hasPercentAttribute)
    {
        // 只有百分比属性，不需要基础成长值
        LOG_DEBUG("module.itemgrowth", "GenerateLevelUpAttributesBasedOnItem: 只配置了百分比属性，不使用基础成长值");
        useAttributeValue = false;
        growthValue = 0;
    }
    else
    {
        // 都没有配置，使用默认值
        LOG_WARN("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 物品GUID %s 等级 %s 没有配置任何属性！模板ID:%s - 使用默认成长值1", record->itemGuid, record->currentLevel, levelTemplate->id).c_str());
        useAttributeValue = true;
        growthValue = 1;
    }

    // 完全基于配置表的属性模板组生成成长属性（不再检查官方属性）
    std::vector<std::pair<uint32, AsInt64>> newAttributes;
    bool foundAttributes = false;

    // 只有在需要使用属性值时才处理基础属性成长
    if (useAttributeValue && growthValue > 0)
    {
        // 统一从属性模板组获取属性
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 从属性组 %s 获取属性模板，物品ID:%s 成长值:%s", levelTemplate->itemPropertyGroup, item->GetEntry(), Acore::ToString(growthValue)).c_str());

#ifdef MODULE_ITEM_ATTRIBUTES
        if (levelTemplate->itemPropertyGroup > 0)
        {
            // 从属性模板组中获取属性列表
            auto attributeTemplates = sItemAttributesLoader->GetItemAttributeTemplatesByGroup(levelTemplate->itemPropertyGroup);

            if (!attributeTemplates.empty())
            {
                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 从属性组 %s 中找到 %s 个属性模板", levelTemplate->itemPropertyGroup, attributeTemplates.size()).c_str());

                // 为每个属性模板生成成长属性
                for (auto const* attrTemplate : attributeTemplates)
                {
                    if (attrTemplate && attrTemplate->attributeType > 0)
                    {
                        // 使用属性类型作为成长属性类型
                        newAttributes.push_back(std::make_pair(attrTemplate->attributeType, growthValue));
                        foundAttributes = true;

                        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 基于属性模板 ID:%s 类型:%s 名称:'%s' 增长 %s 点", attrTemplate->id, attrTemplate->attributeType, attrTemplate->clientDisplay, Acore::ToString(growthValue)).c_str());
                    }
                }
            }
            else
            {
                LOG_WARN("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 属性组 %s 中没有找到任何属性模板", levelTemplate->itemPropertyGroup).c_str());
            }
        }
        else
        {
            ItemGrowthTraceLog("module.itemgrowth", "GenerateLevelUpAttributesBasedOnItem: 成长模板未配置属性组 (itemPropertyGroup = 0)");
        }
#else
        LOG_WARN("module.itemgrowth", "GenerateLevelUpAttributesBasedOnItem: 物品属性模块未启用，无法从属性组获取属性");
#endif

        // 如果没有找到任何属性，记录警告并返回
        if (!foundAttributes)
        {
            ItemGrowthTraceLog("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 装备 %s 没有配置有效的属性组，跳过属性成长", item->GetEntry()).c_str());
            return;
        }
    }
    else
    {
        LOG_DEBUG("module.itemgrowth", "GenerateLevelUpAttributesBasedOnItem: 跳过基础属性成长，只处理百分比属性");
    }

    // 不在这里处理百分比属性，统一在后面和基础属性一起处理
    // 这样可以确保两者使用完全相同的逻辑和数值

    // 统一处理基础属性和百分比属性，确保使用完全相同的逻辑和数值
    std::vector<std::pair<uint32, AsInt64>> baseAttributes;  // 基础成长属性
    std::vector<std::pair<uint32, AsInt64>> percentAttributes; // 百分比属性

    // 先计算属性分配结果，然后基础属性和百分比属性都使用这个结果
    std::vector<std::pair<uint32, AsInt64>> attributeDistribution;

    if (useAttributeValue && growthValue > 0)
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 开始计算属性分配，growthValue = %s", Acore::ToString(growthValue)).c_str());

        // 统一从属性模板组获取属性并分配
        LOG_DEBUG("module.itemgrowth", "GenerateLevelUpAttributesBasedOnItem: 从属性组获取属性进行分配");

#ifdef MODULE_ITEM_ATTRIBUTES
        if (levelTemplate->itemPropertyGroup > 0)
        {
            auto attributeTemplates = sItemAttributesLoader->GetItemAttributeTemplatesByGroup(levelTemplate->itemPropertyGroup);

            if (!attributeTemplates.empty())
            {
                for (auto const* attrTemplate : attributeTemplates)
                {
                    if (attrTemplate && attrTemplate->attributeType > 0)
                    {
                        // 平均分配：每个属性模板都获得相同的成长值
                        attributeDistribution.push_back(std::make_pair(attrTemplate->attributeType, growthValue));
                        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 基于属性模板分配 - 类型:%s 名称:'%s' 数值:%s", attrTemplate->attributeType, attrTemplate->clientDisplay, Acore::ToString(growthValue)).c_str());
                    }
                }
            }
        }
#endif

        // 如果没有配置属性组，记录警告并返回
        if (attributeDistribution.empty())
        {
            ItemGrowthTraceLog("module.itemgrowth", "GenerateLevelUpAttributesBasedOnItem: 装备没有配置有效的属性组，无法进行属性成长");
            return;
        }

        // 基础属性使用计算出的分配结果
        baseAttributes = attributeDistribution;
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 基础属性分配完成，共 %s 个", baseAttributes.size()).c_str());

        // 百分比属性使用完全相同的分配结果
        if (hasPercentAttribute)
        {
            percentAttributes = attributeDistribution;
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 百分比属性分配完成，使用和基础属性相同的数值，共 %s 个", percentAttributes.size()).c_str());
        }
    }

    // 处理基础成长属性
    if (!baseAttributes.empty())
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 处理 %s 个基础属性", baseAttributes.size()).c_str());

        std::vector<std::pair<uint32, AsInt64>> existingBaseAttributes;
        ParseGrowthAttributes(record->growthAttributes, existingBaseAttributes);

        // 合并基础属性
        for (const auto& newAttr : baseAttributes)
        {
            bool found = false;
            for (auto& existingAttr : existingBaseAttributes)
            {
                if (existingAttr.first == newAttr.first)
                {
                    AsInt64 mergedValue = SaturatingAddInt64(existingAttr.second, newAttr.second);
                    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 合并基础属性类型 %s 原值:%s 新增:%s 合计:%s", existingAttr.first, Acore::ToString(existingAttr.second), Acore::ToString(newAttr.second), Acore::ToString(mergedValue)).c_str());
                    existingAttr.second = mergedValue;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 添加新基础属性类型 %s 数值 %s", newAttr.first, Acore::ToString(newAttr.second)).c_str());
                existingBaseAttributes.push_back(newAttr);
            }
        }

        // 更新基础成长属性字符串
        std::string oldBaseAttributes = record->growthAttributes;
        record->growthAttributes = FormatGrowthAttributes(existingBaseAttributes);

        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 基础属性更新 - 旧:'%s'  新:'%s'", oldBaseAttributes, record->growthAttributes).c_str());
    }

    // 处理百分比属性
    if (!percentAttributes.empty())
    {
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 处理 %s 个百分比属性", percentAttributes.size()).c_str());

        std::vector<std::pair<uint32, AsInt64>> existingPercentAttributes;
        ParseGrowthAttributes(record->percentAttributes, existingPercentAttributes);

        // 合并百分比属性
        for (const auto& newAttr : percentAttributes)
        {
            bool found = false;
            for (auto& existingAttr : existingPercentAttributes)
            {
                if (existingAttr.first == newAttr.first)
                {
                    AsInt64 mergedValue = SaturatingAddInt64(existingAttr.second, newAttr.second);
                    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 合并百分比属性类型 %s 原值:%s 新增:%s 合计:%s", existingAttr.first, Acore::ToString(existingAttr.second), Acore::ToString(newAttr.second), Acore::ToString(mergedValue)).c_str());
                    existingAttr.second = mergedValue;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 添加新百分比属性类型 %s 数值 %s", newAttr.first, Acore::ToString(newAttr.second)).c_str());
                existingPercentAttributes.push_back(newAttr);
            }
        }

        // 更新百分比属性字符串
        std::string oldPercentAttributes = record->percentAttributes;
        record->percentAttributes = FormatGrowthAttributes(existingPercentAttributes);

        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 百分比属性更新 - 旧:'%s'  新:'%s'", oldPercentAttributes, record->percentAttributes).c_str());
    }

    // 检查是否生成了任何属性
    if (baseAttributes.empty() && percentAttributes.empty())
    {
        LOG_WARN("module.itemgrowth", ModuleOldFormat("GenerateLevelUpAttributesBasedOnItem: 物品GUID %s 等级 %s 没有生成任何新属性！", record->itemGuid, record->currentLevel).c_str());
    }
    else
    {
        // 属性生成完成
    }
}

// 刷新并应用成长属性到玩家身上 - 先移除旧属性，再应用新属性
void ItemGrowthMgr::RefreshAndApplyGrowthAttributes(Player* player, Item* item, ItemGrowthPlayerRecord* record)
{
    if (!player || !item || !record)
        return;

    uint32 itemGuid = record->itemGuid;
    LOG_INFO("module.itemgrowth", ModuleOldFormat("RefreshAndApplyGrowthAttributes: 为玩家 %s 刷新物品GUID %s 的成长属性", player->GetName(), itemGuid).c_str());

    // 关键修改：不再手动移除和应用属性，完全由幻境系统统一处理
    // 获取当前内存中的属性（升级前的属性）
    // std::string oldBaseAttributes = record->growthAttributes;
    // std::string oldPercentAttributes = record->percentAttributes;

    // 先移除旧的成长属性（使用内存中的旧属性值）
    // if (!oldBaseAttributes.empty())
    // {
    //     LOG_INFO("module.itemgrowth", "RefreshAndApplyGrowthAttributes: 移除旧基础属性: '{}'", oldBaseAttributes);
    //     RemoveGrowthAttributesDirect(player, oldBaseAttributes);
    // }

    // if (!oldPercentAttributes.empty())
    // {
    //     LOG_INFO("module.itemgrowth", "RefreshAndApplyGrowthAttributes: 移除旧百分比属性: '{}'", oldPercentAttributes);
    //     RemoveGrowthAttributesDirect(player, oldPercentAttributes);
    // }

    // 重新从数据库读取最新的属性并应用
    // ApplyGrowthAura(player, itemGuid);  // 已禁用

    ApplyGrowthAura(player, itemGuid);

    LOG_INFO("module.itemgrowth", ModuleOldFormat("RefreshAndApplyGrowthAttributes: 完成物品GUID %s 的属性刷新", itemGuid).c_str());
}

// 验证属性是否正确应用到玩家身上
void ItemGrowthMgr::VerifyAttributesApplied(Player* player, const std::string& attributeString)
{
    if (!player || attributeString.empty())
        return;

    LOG_INFO("module.itemgrowth", ModuleOldFormat("VerifyAttributesApplied: 开始验证玩家 %s 的属性应用情况", player->GetName()).c_str());

    std::vector<std::pair<uint32, AsInt64>> attributes;
    ParseGrowthAttributes(attributeString, attributes);

    if (attributes.empty())
    {
        LOG_WARN("module.itemgrowth", "VerifyAttributesApplied: 没有属性需要验证");
        return;
    }

    ChatHandler(player->GetSession()).PSendSysMessage("=== 属性应用验证 ===");

    for (const auto& attr : attributes)
    {
        uint32 attrType = attr.first;
        AsInt64 attrValue = attr.second;
        std::string attrValueText = Acore::ToString(attrValue);

        switch (attrType)
        {
            case 0: // 法力值
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("法力值: 应用+%s, 当前最大法力: %s", attrValueText, player->GetMaxPower(POWER_MANA)).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证法力值: 应用+%s, 当前最大法力: %s", attrValueText, player->GetMaxPower(POWER_MANA)).c_str());
                break;
            case 1: // 生命值
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("生命值: 应用+%s, 当前最大生命: %s", attrValueText, player->GetMaxHealth()).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证生命值: 应用+%s, 当前最大生命: %s", attrValueText, player->GetMaxHealth()).c_str());
                break;
            case 3: // 敏捷
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("敏捷: 应用+%s, 当前敏捷: %s", attrValueText, player->GetStat(STAT_AGILITY)).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证敏捷: 应用+%s, 当前敏捷: %s", attrValueText, player->GetStat(STAT_AGILITY)).c_str());
                break;
            case 4: // 力量
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("力量: 应用+%s, 当前力量: %s", attrValueText, player->GetStat(STAT_STRENGTH)).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证力量: 应用+%s, 当前力量: %s", attrValueText, player->GetStat(STAT_STRENGTH)).c_str());
                break;
            case 5: // 智力
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("智力: 应用+%s, 当前智力: %s", attrValueText, player->GetStat(STAT_INTELLECT)).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证智力: 应用+%s, 当前智力: %s", attrValueText, player->GetStat(STAT_INTELLECT)).c_str());
                break;
            case 6: // 精神
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("精神: 应用+%s, 当前精神: %s", attrValueText, player->GetStat(STAT_SPIRIT)).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证精神: 应用+%s, 当前精神: %s", attrValueText, player->GetStat(STAT_SPIRIT)).c_str());
                break;
            case 7: // 耐力
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("耐力: 应用+%s, 当前耐力: %s", attrValueText, player->GetStat(STAT_STAMINA)).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证耐力: 应用+%s, 当前耐力: %s", attrValueText, player->GetStat(STAT_STAMINA)).c_str());
                break;
            case 31: // 命中等级
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("命中等级: 应用+%s", attrValueText).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证命中等级: 应用+%s", attrValueText).c_str());
                break;
            case 32: // 暴击等级
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("暴击等级: 应用+%s", attrValueText).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证暴击等级: 应用+%s", attrValueText).c_str());
                break;
            case 36: // 急速等级
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("急速等级: 应用+%s", attrValueText).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证急速等级: 应用+%s", attrValueText).c_str());
                break;
            case 38: // 攻击强度
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("攻击强度: 应用+%s", attrValueText).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证攻击强度: 应用+%s", attrValueText).c_str());
                break;
            case 45: // 法术强度
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("法术强度: 应用+%s", attrValueText).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证法术强度: 应用+%s", attrValueText).c_str());
                break;
            default:
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("属性类型%s: 应用+%s", attrType, attrValueText).c_str());
                LOG_INFO("module.itemgrowth", ModuleOldFormat("验证属性类型%s: 应用+%s", attrType, attrValueText).c_str());
                break;
        }
    }

    ChatHandler(player->GetSession()).PSendSysMessage("=== 验证完成 ===");
    LOG_INFO("module.itemgrowth", ModuleOldFormat("VerifyAttributesApplied: 完成验证，共验证了 %s 个属性", attributes.size()).c_str());
}

// 使用自定义Aura系统应用成长属性 - 从数据库读取最新属性
void ItemGrowthMgr::ApplyGrowthAura(Player* player, uint32 itemGuid)
{
    if (!player)
        return;

    // 修复：首先检查物品是否真的装备在身上
    bool isEquipped = false;
    Item* equippedItem = nullptr;
    uint8 equippedSlot = 255;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item && item->GetGUID().GetCounter() == itemGuid)
        {
            isEquipped = true;
            equippedItem = item;
            equippedSlot = slot;
            break;
        }
    }

    if (!isEquipped)
    {
        return;
    }

    // 确保唯一seed，保证聊天链接包含唯一ID
    if (equippedSlot != 255)
    {
        Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, equippedSlot);
        EnsureItemUniqueSeed(player, equippedItem);
    }

    // 不再向随机附魔槽写入/同步GUID，避免与随机附魔系统冲突

    // 【读路径内存化】内存记录即权威（所有写库均源于内存 record），
    // 此前每次卸装/重应用都同步查库读"最新"属性——这正是 SavePlayerItemRecord
    // 被迫用同步 DirectExecute 的原因（写后回读依赖）
    ItemGrowthPlayerRecord const* record = GetPlayerItemRecord(itemGuid);
    if (!record)
    {
        return;
    }

    std::string const& latestBaseAttributes = record->growthAttributes;
    std::string const& latestPercentAttributes = record->percentAttributes;

    if (latestBaseAttributes.empty() && latestPercentAttributes.empty())
    {
        return;
    }

    // 解析基础成长属性
    std::vector<std::pair<uint32, AsInt64>> baseAttributes;
    std::vector<std::pair<uint32, AsInt64>> percentAttributes;

    if (!latestBaseAttributes.empty())
    {
        ParseGrowthAttributes(latestBaseAttributes, baseAttributes);
    }

    if (!latestPercentAttributes.empty())
    {
        ParseGrowthAttributes(latestPercentAttributes, percentAttributes);
    }

    if (baseAttributes.empty() && percentAttributes.empty())
    {
        return;
    }

    // 应用基础属性
    for (const auto& attr : baseAttributes)
    {
        uint32 attrType = attr.first;
        AsInt64 attrValue = attr.second;

        if (attrValue <= 0)
            continue;

        // 应用属性到玩家
        ApplyAttributeToPlayer(player, attrType, attrValue, true);
    }

    // 应用百分比属性
    for (const auto& attr : percentAttributes)
    {
        uint32 attrType = attr.first;
        AsInt64 percentValue = attr.second;

        if (percentValue <= 0)
            continue;

        // 应用百分比属性到玩家
        ApplyPercentAttributeToPlayer(player, attrType, percentValue, true);
    }

    // 在应用所有属性后更新玩家状态
    player->UpdateAllStats();
    player->UpdateAttackPowerAndDamage();
    player->UpdateAttackPowerAndDamage(true);
    player->UpdateMaxHealth();
    player->UpdateMaxPower(POWER_MANA);
}

// 移除成长Aura - 按内存记录的当前属性值进行移除
void ItemGrowthMgr::RemoveGrowthAura(Player* player, uint32 itemGuid)
{
    if (!player)
        return;

    // 【读路径内存化】内存记录即权威，不再同步查库（理由同 ApplyGrowthAura）
    ItemGrowthPlayerRecord const* record = GetPlayerItemRecord(itemGuid);
    if (!record)
    {
        return;
    }

    std::string const& currentBaseAttributes = record->growthAttributes;
    std::string const& currentPercentAttributes = record->percentAttributes;

    if (currentBaseAttributes.empty() && currentPercentAttributes.empty())
    {
        return;
    }

    // 解析基础属性和百分比属性
    std::vector<std::pair<uint32, AsInt64>> baseAttributes;
    std::vector<std::pair<uint32, AsInt64>> percentAttributes;

    if (!currentBaseAttributes.empty())
    {
        ParseGrowthAttributes(currentBaseAttributes, baseAttributes);
    }

    if (!currentPercentAttributes.empty())
    {
        ParseGrowthAttributes(currentPercentAttributes, percentAttributes);
    }

    if (baseAttributes.empty() && percentAttributes.empty())
    {
        return;
    }

    // 移除基础属性
    for (const auto& attr : baseAttributes)
    {
        uint32 attrType = attr.first;
        AsInt64 attrValue = attr.second;

        if (attrValue <= 0)
            continue;

        // 移除属性从玩家
        ApplyAttributeToPlayer(player, attrType, attrValue, false);
    }

    // 移除百分比属性
    for (const auto& attr : percentAttributes)
    {
        uint32 attrType = attr.first;
        AsInt64 percentValue = attr.second;

        if (percentValue <= 0)
            continue;

        // 移除百分比属性从玩家
        ApplyPercentAttributeToPlayer(player, attrType, percentValue, false);
    }

    // 在移除属性后更新玩家状态
    player->UpdateAllStats();
    player->UpdateAttackPowerAndDamage();
    player->UpdateAttackPowerAndDamage(true);
    player->UpdateMaxHealth();
    player->UpdateMaxPower(POWER_MANA);
}

// 直接移除指定的成长属性（不从数据库读取）
void ItemGrowthMgr::RemoveGrowthAttributesDirect(Player* player, const std::string& attributeString)
{
    if (!player || attributeString.empty())
        return;

    // 解析成长属性
    std::vector<std::pair<uint32, AsInt64>> attributes;
    ParseGrowthAttributes(attributeString, attributes);

    if (attributes.empty())
    {
        return;
    }

    // 移除每个属性
    for (const auto& attr : attributes)
    {
        uint32 attrType = attr.first;
        AsInt64 attrValue = attr.second;

        if (attrValue <= 0)
            continue;

        // 判断是基础属性还是百分比属性
        if (attrType == 0 || attrType == 1)
        {
            // 百分比属性
            ApplyPercentAttributeToPlayer(player, attrType, attrValue, false);
        }
        else
        {
            // 基础属性
            ApplyAttributeToPlayer(player, attrType, attrValue, false);
        }
    }

    // 在移除属性后更新玩家状态
    player->UpdateAllStats();
    player->UpdateAttackPowerAndDamage();
    player->UpdateAttackPowerAndDamage(true);
    player->UpdateMaxHealth();
    player->UpdateMaxPower(POWER_MANA);

    // 完成移除属性
}

// 修复0级记录的专用函数 - 只修复等级，不自动生成属性
bool ItemGrowthMgr::FixZeroLevelRecord(Player* player, Item* item, uint32 itemGuid)
{
    if (!player || !item)
        return false;

    ItemGrowthPlayerRecord* record = GetPlayerItemRecord(itemGuid);
    if (!record)
        return false;

    // 检查是否需要修复等级
    if (record->currentLevel != 0)
        return false;

    LOG_WARN("module.itemgrowth", ModuleOldFormat("FixZeroLevelRecord: 修复物品GUID %s 的0级记录为1级 - 当前属性:'%s'", itemGuid, record->growthAttributes).c_str());

    // 只修复等级为1，不自动生成属性
    record->currentLevel = 1;

    // 【审计修复】设置升级经验需求：1级物品需要查找2级模板的经验要求
    // 与 CreatePlayerItemRecord 和 UpdateRequiredExp 保持一致
    // 原来错误地使用了1级模板的requiredExp，这会导致经验需求与正常流程不一致
    ItemGrowthTemplate const* level2Template = GetItemGrowthTemplateByLevel(2, record->itemPropertyGroup);
    if (level2Template)
    {
        record->requiredExp = level2Template->requiredExp;
        LOG_INFO("module.itemgrowth", ModuleOldFormat("FixZeroLevelRecord: 1级物品升级到2级需要经验: %s", record->requiredExp).c_str());
    }
    else
    {
        // 如果没有2级模板，说明这个组只有1级，设置为0表示无法升级
        record->requiredExp = 0;
        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("FixZeroLevelRecord: 属性组%s只有1级，无法升级", record->itemPropertyGroup).c_str());
    }

    // 保存到数据库
    SavePlayerItemRecord(record);

    LOG_INFO("module.itemgrowth", "FixZeroLevelRecord: 修复完成 - 等级已设为1，属性需要玩家升级时生成");
    return true;
}

// ==================== 【性能优化】异步批量经验处理实现 ====================

// 累积经验（不立即处理，等待批量处理）
void ItemGrowthMgr::AccumulateItemExp(Player* player, Item* item, uint32 exp)
{
    if (!player || !item || exp == 0)
        return;

    if (!CanItemGrow(item))
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 itemGuid = item->GetGUID().GetCounter();

    // 线程安全地累积经验
    {
        std::lock_guard<std::mutex> lock(_expAccumulatorMutex);

        auto& accumulator = _playerExpAccumulators[playerGuid];
        uint32& accumulatedExp = accumulator.itemExpMap[itemGuid];
        accumulatedExp = exp > std::numeric_limits<uint32>::max() - accumulatedExp
            ? std::numeric_limits<uint32>::max()
            : accumulatedExp + exp;
        accumulator.needsUpdate = true;

        // 记录时间戳（使用当前游戏时间）
        if (accumulator.lastProcessTime == 0)
        {
            accumulator.lastProcessTime = static_cast<uint32>(GameTime::GetGameTimeMS().count());
        }
    }

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("AccumulateItemExp: 玩家 %s 物品GUID %s 累积经验 %s (异步模式)", player->GetName(), itemGuid, exp).c_str());
}

// 处理单个玩家累积的经验
void ItemGrowthMgr::ProcessAccumulatedExp(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 获取并清空该玩家的累积经验
    std::unordered_map<uint32, uint32> expToProcess;
    {
        std::lock_guard<std::mutex> lock(_expAccumulatorMutex);

        auto it = _playerExpAccumulators.find(playerGuid);
        if (it == _playerExpAccumulators.end() || !it->second.needsUpdate)
            return;

        expToProcess = std::move(it->second.itemExpMap);
        it->second.itemExpMap.clear();
        it->second.needsUpdate = false;
        it->second.lastProcessTime = static_cast<uint32>(GameTime::GetGameTimeMS().count());
    }

    if (expToProcess.empty())
        return;

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ProcessAccumulatedExp: 开始处理玩家 %s 的 %s 件物品累积经验", player->GetName(), expToProcess.size()).c_str());

    // 批量处理所有累积的经验
    std::vector<uint32> upgradedItems;  // 记录升级的物品

    for (auto const& [itemGuid, totalExp] : expToProcess)
    {
        // 查找物品
        Item* item = nullptr;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (equippedItem && equippedItem->GetGUID().GetCounter() == itemGuid)
            {
                item = equippedItem;
                break;
            }
        }

        if (!item)
        {
            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ProcessAccumulatedExp: 物品GUID %s 不在装备槽，跳过", itemGuid).c_str());
            continue;
        }

        ItemGrowthPlayerRecord* record = GetPlayerItemRecord(itemGuid);
        if (!record)
            continue;

        // 应用基础经验倍率
        double scaledExp = static_cast<double>(totalExp) * static_cast<double>(_baseExpMultiplier);
        uint32 finalExp = scaledExp <= 0.0
            ? 0
            : static_cast<uint32>(std::min<double>(scaledExp, std::numeric_limits<uint32>::max()));
        if (finalExp == 0)
            continue;

        // 检查是否可以升级
        ItemGrowthTemplate const* nextTemplate = GetItemGrowthTemplateByLevel(record->currentLevel + 1, record->itemPropertyGroup);
        if (!nextTemplate)
        {
            // 已达最高等级
            continue;
        }

        // 更新经验
        uint32 oldLevel = record->currentLevel;
        record->currentExp = finalExp > std::numeric_limits<uint32>::max() - record->currentExp
            ? std::numeric_limits<uint32>::max()
            : record->currentExp + finalExp;

        // 处理升级逻辑
        bool levelUp = false;
        size_t levelUpCount = 0;
        size_t const maxLevelUps = std::max<size_t>(_itemGrowthTemplates.size() + 1, 1);
        while (record->currentExp >= record->requiredExp && record->requiredExp > 0 && levelUpCount < maxLevelUps)
        {
            record->currentExp -= record->requiredExp;
            record->currentLevel++;
            ++levelUpCount;
            levelUp = true;

            // 获取新等级的模板
            ItemGrowthTemplate const* newTemplate = GetItemGrowthTemplateByLevel(record->currentLevel, record->itemPropertyGroup);
            if (!newTemplate)
                break;

            // 生成升级属性
            GenerateLevelUpAttributesBasedOnItem(player, item, record, newTemplate);

            // 更新升级经验需求
            UpdateRequiredExp(record);

            // 检查是否还能继续升级
            nextTemplate = GetItemGrowthTemplateByLevel(record->currentLevel + 1, record->itemPropertyGroup);
            if (!nextTemplate)
                break;
        }

        if (levelUpCount >= maxLevelUps && record->requiredExp > 0 && record->currentExp >= record->requiredExp)
        {
            LOG_WARN("module.itemgrowth", "玩家 {} 的物品 {} 单次升级达到安全上限 {}，剩余经验保留到下一批处理",
                playerGuid, itemGuid, maxLevelUps);
        }

        // 标记为脏数据，稍后批量保存
        {
            std::lock_guard<std::mutex> lock(_dirtyRecordsMutex);
            _dirtyRecordGuids.insert(itemGuid);
        }

        if (levelUp)
        {
            upgradedItems.push_back(itemGuid);

            // 只有升级时才通知玩家
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("物品升级到%s级！", record->currentLevel).c_str());

            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ProcessAccumulatedExp: 物品GUID %s 从等级 %s 升级到 %s", itemGuid, oldLevel, record->currentLevel).c_str());
        }
    }

    // 批量刷新升级物品的属性（延迟处理，减少开销）
    if (!upgradedItems.empty())
    {
        // 使用延迟事件来刷新属性，避免在杀怪循环中执行重操作
        player->m_Events.AddEventAtOffset([this, playerGuid, upgradedItems]()
        {
            Player* p = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(playerGuid));
            if (!p)
                return;

            for (uint32 itemGuid : upgradedItems)
            {
                // 查找物品
                Item* item = nullptr;
                for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                {
                    Item* equippedItem = p->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                    if (equippedItem && equippedItem->GetGUID().GetCounter() == itemGuid)
                    {
                        item = equippedItem;
                        break;
                    }
                }

                if (!item)
                    continue;

                ApplyGrowthAura(p, itemGuid);
            }

            // 一次性更新所有属性
            p->UpdateAllStats();
            p->UpdateAttackPowerAndDamage();
            p->UpdateAttackPowerAndDamage(true);
            p->UpdateMaxHealth();
            p->UpdateMaxPower(POWER_MANA);

            LOG_DEBUG("module.itemgrowth", ModuleOldFormat("ProcessAccumulatedExp: 延迟刷新了 %s 件物品的属性", upgradedItems.size()).c_str());
        }, 500ms);  // 延迟500ms执行
    }
}

// 处理所有玩家的待处理经验（由WorldScript定时调用）
void ItemGrowthMgr::ProcessAllPendingExp()
{
    uint32 currentTime = static_cast<uint32>(GameTime::GetGameTimeMS().count());

    // 获取需要处理的玩家列表
    std::vector<uint32> playersToProcess;
    {
        std::lock_guard<std::mutex> lock(_expAccumulatorMutex);

        for (auto& [playerGuid, accumulator] : _playerExpAccumulators)
        {
            if (accumulator.needsUpdate &&
                (currentTime - accumulator.lastProcessTime) >= _expProcessInterval)
            {
                playersToProcess.push_back(playerGuid);
            }
        }
    }

    // 处理每个玩家
    for (uint32 playerGuid : playersToProcess)
    {
        Player* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(playerGuid));
        if (player)
        {
            ProcessAccumulatedExp(player);
        }
    }

    // 检查是否需要批量保存脏数据
    if ((currentTime - _lastBatchSaveTime) >= _batchSaveInterval)
    {
        SaveDirtyRecordsAsync();
        _lastBatchSaveTime = currentTime;
    }
}

// 玩家下线时强制处理
void ItemGrowthMgr::FlushPlayerExp(Player* player)
{
    if (!player)
        return;

    // 先处理累积的经验
    ProcessAccumulatedExp(player);

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 清理该玩家的累积器
    {
        std::lock_guard<std::mutex> lock(_expAccumulatorMutex);
        _playerExpAccumulators.erase(playerGuid);
    }

    // 立即保存该玩家相关的脏记录
    std::vector<uint32> playerDirtyGuids;
    {
        std::lock_guard<std::mutex> lock(_dirtyRecordsMutex);

        for (uint32 itemGuid : _dirtyRecordGuids)
        {
            auto* record = GetPlayerItemRecord(itemGuid);
            if (record && record->playerGuid == playerGuid)
            {
                playerDirtyGuids.push_back(itemGuid);
            }
        }

        // 从脏记录集合中移除
        for (uint32 guid : playerDirtyGuids)
        {
            _dirtyRecordGuids.erase(guid);
        }
    }

    // 同步保存（下线时需要确保数据完整性）
    for (uint32 itemGuid : playerDirtyGuids)
    {
        auto* record = GetPlayerItemRecord(itemGuid);
        if (record)
        {
            SavePlayerItemRecord(record);
        }
    }

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("FlushPlayerExp: 玩家 %s 下线，保存了 %s 条记录", player->GetName(), playerDirtyGuids.size()).c_str());
}

// 批量保存脏数据（同步版本）
void ItemGrowthMgr::SaveDirtyRecords()
{
    std::vector<uint32> guidsToSave;
    {
        std::lock_guard<std::mutex> lock(_dirtyRecordsMutex);
        guidsToSave.assign(_dirtyRecordGuids.begin(), _dirtyRecordGuids.end());
        _dirtyRecordGuids.clear();
    }

    if (guidsToSave.empty())
        return;

    for (uint32 itemGuid : guidsToSave)
    {
        auto* record = GetPlayerItemRecord(itemGuid);
        if (record)
        {
            SavePlayerItemRecord(record);
        }
    }

    LOG_DEBUG("module.itemgrowth", ModuleOldFormat("SaveDirtyRecords: 批量保存了 %s 条记录", guidsToSave.size()).c_str());
}

// 异步批量保存脏数据
void ItemGrowthMgr::SaveDirtyRecordsAsync()
{
    std::vector<uint32> guidsToSave;
    {
        std::lock_guard<std::mutex> lock(_dirtyRecordsMutex);
        guidsToSave.assign(_dirtyRecordGuids.begin(), _dirtyRecordGuids.end());
        _dirtyRecordGuids.clear();
    }

    if (guidsToSave.empty())
        return;

    // 【审计修复】SQL转义函数 - 防止SQL注入
    auto escapeString = [](const std::string& str) -> std::string {
        std::string result;
        result.reserve(str.size() * 2);
        for (char c : str)
        {
            switch (c)
            {
                case '\'': result += "''"; break;    // 单引号转义
                case '\\': result += "\\\\"; break;  // 反斜杠转义
                case '\0': result += "\\0"; break;   // NULL字符转义
                case '\n': result += "\\n"; break;   // 换行符转义
                case '\r': result += "\\r"; break;   // 回车符转义
                case '\x1a': result += "\\Z"; break; // Ctrl+Z转义
                default: result += c; break;
            }
        }
        return result;
    };

    // 构建批量SQL
    std::ostringstream sql;
    bool first = true;

    for (uint32 itemGuid : guidsToSave)
    {
        auto* record = GetPlayerItemRecord(itemGuid);
        if (!record)
            continue;

        if (first)
        {
            sql << "INSERT INTO 物品成长_玩家记录 (玩家GUID, 物品GUID, 物品模板ID, 当前等级, 当前经验, "
                << "升级经验, 物品属性_模板_组, 成长次数, 失败次数, 成长属性, 百分比属性) VALUES ";
            first = false;
        }
        else
        {
            sql << ",";
        }

        // 【审计修复】对属性字符串进行SQL转义
        std::string escapedGrowthAttributes = escapeString(record->growthAttributes);
        std::string escapedPercentAttributes = escapeString(record->percentAttributes);

        sql << "(" << record->playerGuid << ","
            << record->itemGuid << ","
            << record->itemEntry << ","
            << record->currentLevel << ","
            << record->currentExp << ","
            << record->requiredExp << ","
            << record->itemPropertyGroup << ","
            << record->growthCount << ","
            << record->failCount << ","
            << "'" << escapedGrowthAttributes << "',"
            << "'" << escapedPercentAttributes << "')";
    }

    if (!first)
    {
        sql << " ON DUPLICATE KEY UPDATE "
            << "玩家GUID = VALUES(玩家GUID), "
            << "物品模板ID = VALUES(物品模板ID), "
            << "当前等级 = VALUES(当前等级), "
            << "当前经验 = VALUES(当前经验), "
            << "升级经验 = VALUES(升级经验), "
            << "物品属性_模板_组 = VALUES(物品属性_模板_组), "
            << "成长次数 = VALUES(成长次数), "
            << "失败次数 = VALUES(失败次数), "
            << "成长属性 = VALUES(成长属性), "
            << "百分比属性 = VALUES(百分比属性)";

        // 异步执行
        CharacterDatabase.Execute(sql.str());

        LOG_DEBUG("module.itemgrowth", ModuleOldFormat("SaveDirtyRecordsAsync: 异步批量保存了 %s 条记录", guidsToSave.size()).c_str());
    }
}
