#include "AsInt256.h"
#include "StringConvert.h"
#include "ItemAttributesLoader.h"
#include "ItemAttributesEffects.h"
#include "ItemAttributesDBHelper.h"
#include "DatabaseEnv.h"
#include "Logging/Log.h"
#include "ScriptMgr.h"
#include "Configuration/Config.h"
#include "Item.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Random.h"
#include "StringConvert.h"
#include "Util.h"
#include <random>
#include <sstream>
#include <algorithm>
#include <limits>
#include "ModuleFormat.h"

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

AsInt64 ScaleAttributeValueByItemLevel(AsInt64 const& value, uint32 itemLevel)
{
    return Acore::Number::CalculatePct(value, itemLevel);
}
}

// 【线程安全】Meyer's Singleton - C++11保证静态局部变量初始化的线程安全性
// 编译器会自动添加同步机制，确保多线程并发调用时只初始化一次
ItemAttributesLoader* ItemAttributesLoader::instance()
{
    static ItemAttributesLoader instance;
    return &instance;
}

// 【根本性修复】安全获取实例方法
ItemAttributesLoader* ItemAttributesLoader::SafeInstance()
{
    ItemAttributesLoader* inst = instance();
    return (inst && inst->IsInitialized()) ? inst : nullptr;
}

void ItemAttributesLoader::LoadItemAttributeTemplates()
{
    // 【审计修复】移除重复初始化检查，允许重载命令生效
    // 如果已初始化，清空现有数据以便重新加载
    if (_isInitialized)
    {
        LOG_INFO("module.item-attributes", "重新加载物品属性模板...");
    }

    _itemAttributeTemplateStore.clear();

    uint32 oldMSTime = getMSTime();

    // 检查表是否存在，如果不存在则跳过加载
    QueryResult checkTable = WorldDatabase.Query("SHOW TABLES LIKE '_模板_物品属性'");
    if (!checkTable || checkTable->GetRowCount() == 0)
    {
        LOG_ERROR("module.item-attributes", ">> 表 `_模板_物品属性` 不存在，请导入SQL文件");
        LOG_ERROR("module.item-attributes", ">> 请确保已导入 modules/mod-item-attributes/data/sql/db_world/ 目录下的所有SQL文件");
        _isInitialized = false;  // 【根本性修复】标记初始化失败
        return;
    }

    QueryResult result = WorldDatabase.Query("SELECT `id`, `注释`, `客户端显示`, `组`, `属性类型`, `获取几率`, `属性最小百分比`, `属性最大百分比`, `属性计算方式`, `技能模板_组`, `属性品质要求`, `属性等级要求`, `属性职业要求`, `属性颜色` FROM `_模板_物品属性`");

    if (!result)
    {
        LOG_WARN("module.item-attributes", ">> 未找到任何物品属性模板数据");
        _isInitialized = false;  // 【根本性修复】标记初始化失败
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        ItemAttributeTemplate attributeTemplate;
        attributeTemplate.id = fields[0].Get<uint32>();
        attributeTemplate.comment = fields[1].Get<std::string>();
        attributeTemplate.clientDisplay = fields[2].Get<std::string>();
        attributeTemplate.group = fields[3].Get<uint32>();
        attributeTemplate.attributeType = fields[4].Get<uint32>();
        attributeTemplate.chance = fields[5].Get<uint32>();
        attributeTemplate.minPercent = AsInt64FromString(fields[6].Get<std::string>());
        attributeTemplate.maxPercent = AsInt64FromString(fields[7].Get<std::string>());
        attributeTemplate.calcType = fields[8].Get<uint32>();
        attributeTemplate.skillGroup = fields[9].Get<uint32>();
        attributeTemplate.qualityRequirement = fields[10].Get<uint32>();
        attributeTemplate.levelRequirement = fields[11].Get<uint32>();
        attributeTemplate.classRequirement = fields[12].Get<uint32>();
        attributeTemplate.color = fields[13].Get<std::string>();

        _itemAttributeTemplateStore[attributeTemplate.id] = attributeTemplate;
        ++count;
    } while (result->NextRow());

    // 【根本性修复】标记初始化成功
    _isInitialized = true;
}

ItemAttributeTemplate const* ItemAttributesLoader::GetItemAttributeTemplate(uint32 id) const
{
    // 【根本性修复】检查初始化状态
    if (!_isInitialized)
    {
        LOG_ERROR("module.item-attributes", "【致命错误】GetItemAttributeTemplate 被调用，但系统尚未初始化！");
        return nullptr;
    }

    auto itr = _itemAttributeTemplateStore.find(id);
    if (itr != _itemAttributeTemplateStore.end())
        return &itr->second;

    return nullptr;
}

std::vector<ItemAttributeTemplate const*> ItemAttributesLoader::GetItemAttributeTemplatesByGroup(uint32 group) const
{
    std::vector<ItemAttributeTemplate const*> result;

    // 【根本性修复】检查初始化状态
    if (!_isInitialized)
    {
        LOG_ERROR("module.item-attributes", "【致命错误】GetItemAttributeTemplatesByGroup 被调用，但系统尚未初始化！");
        return result;
    }

    for (auto const& pair : _itemAttributeTemplateStore)
    {
        if (pair.second.group == group)
            result.push_back(&pair.second);
    }

    return result;
}

std::vector<ItemAttributeTemplate const*> ItemAttributesLoader::GetItemAttributeTemplatesByType(uint32 type) const
{
    std::vector<ItemAttributeTemplate const*> result;

    for (auto const& pair : _itemAttributeTemplateStore)
    {
        if (pair.second.attributeType == type)
            result.push_back(&pair.second);
    }

    return result;
}

ItemAttributeTemplate const* ItemAttributesLoader::GetItemAttributeTemplateByType(uint32 type) const
{
    // 【根本性修复】检查初始化状态（这是最高频调用的方法）
    if (!_isInitialized)
    {
        LOG_ERROR("module.item-attributes", ModuleOldFormat("【致命错误】GetItemAttributeTemplateByType 被调用，但系统尚未初始化！type=%s", type).c_str());
        return nullptr;
    }

    for (auto const& pair : _itemAttributeTemplateStore)
    {
        if (pair.second.attributeType == type)
            return &pair.second;
    }

    return nullptr;
}

std::vector<ItemAttributeTemplate const*> ItemAttributesLoader::GetAllItemAttributeTemplates() const
{
    std::vector<ItemAttributeTemplate const*> result;
    result.reserve(_itemAttributeTemplateStore.size());

    for (auto const& pair : _itemAttributeTemplateStore)
    {
        result.push_back(&pair.second);
    }

    return result;
}

ItemAttributeResult ItemAttributesLoader::ApplyAttributeToItem(Item* item, uint32 attributeId, Player* player, AttributeCategory category, AsInt64 minValue, AsInt64 maxValue)
{
    if (!item)
        return ItemAttributeResult::INVALID_ITEM;

    ItemAttributeTemplate const* attributeTemplate = GetItemAttributeTemplate(attributeId);
    if (!attributeTemplate)
        return ItemAttributeResult::INVALID_ATTRIBUTE;

    // 检查物品是否已经有该属性（使用属性类型检查）
    if (HasAttributeByType(item, attributeTemplate->attributeType))
        return ItemAttributeResult::ALREADY_APPLIED;

    // 检查物品品质是否符合要求
    if (attributeTemplate->qualityRequirement > 0 && item->GetTemplate()->Quality < attributeTemplate->qualityRequirement)
        return ItemAttributeResult::ITEM_NOT_SUITABLE;

    // 检查物品等级是否符合要求
    if (attributeTemplate->levelRequirement > 0 && item->GetTemplate()->ItemLevel < attributeTemplate->levelRequirement)
        return ItemAttributeResult::ITEM_NOT_SUITABLE;

    // 检查物品职业要求
    if (attributeTemplate->classRequirement > 0 && !(item->GetTemplate()->AllowableClass & attributeTemplate->classRequirement))
        return ItemAttributeResult::ITEM_NOT_SUITABLE;

    // 计算属性值
    AsInt64 attributeValue;
    if (minValue != 0 || maxValue != 0)
    {
        // 如果指定了自定义值范围，使用自定义范围
        attributeValue = CalculateAttributeValueWithRange(item, attributeTemplate, minValue, maxValue);
    }
    else
    {
        // 否则使用模板中的默认范围
        attributeValue = CalculateAttributeValue(item, attributeTemplate);
    }

    // 使用新的DBHelper API保存属性
    // 【重要】保存属性类型而不是属性模板ID，确保数据一致性
    uint64 itemGuid = item->GetGUID().GetCounter();
    uint32 itemId = item->GetEntry();
    uint32 attrTypeToSave = attributeTemplate->attributeType;  // 使用属性类型

    bool success = false;
    if (category == AttributeCategory::BASE)
    {
        // 添加到基础属性（保存属性类型）
        success = ItemAttributesDBHelper::AddBaseAttributeToItem(itemGuid, itemId, attrTypeToSave, attributeValue);
    }
    else
    {
        // 添加到追加属性（保存属性类型）
        success = ItemAttributesDBHelper::AddAdditionalAttributeToItem(itemGuid, itemId, attrTypeToSave, attributeValue);
    }

    if (!success)
        return ItemAttributeResult::FAILED;

    // 如果有玩家参数，可以发送消息通知
    if (player)
    {
        std::string categoryStr = (category == AttributeCategory::BASE) ? "基础属性" : "追加属性";
        std::string message = "物品 " + std::string(item->GetTemplate()->Name1) + " 已添加" + categoryStr + ": " + attributeTemplate->clientDisplay;
        player->GetSession()->SendAreaTriggerMessage(message.c_str());
    }

    return ItemAttributeResult::SUCCESS;
}

ItemAttributeResult ItemAttributesLoader::RemoveAttributeFromItem(Item* item, uint32 attributeId)
{
    if (!item)
        return ItemAttributeResult::INVALID_ITEM;

    // 检查物品是否有该属性
    if (!HasAttribute(item, attributeId))
        return ItemAttributeResult::FAILED;

    // 使用DBHelper移除属性
    uint64 itemGuid = item->GetGUID().GetCounter();
    bool success = ItemAttributesDBHelper::RemoveAttributeFromItem(itemGuid, attributeId);

    return success ? ItemAttributeResult::SUCCESS : ItemAttributeResult::FAILED;
}

bool ItemAttributesLoader::HasAttribute(Item* item, uint32 attributeId) const
{
    if (!item)
        return false;

    std::vector<uint32> attributes;
    std::vector<AsInt64> values;
    if (!GetItemAttributesWithValues(item, attributes, values))
        return false;

    return std::find(attributes.begin(), attributes.end(), attributeId) != attributes.end();
}

bool ItemAttributesLoader::HasAttributeByType(Item* item, uint32 attributeType) const
{
    if (!item)
        return false;

    std::vector<uint32> attributes;
    std::vector<AsInt64> values;
    if (!GetItemAttributesWithValues(item, attributes, values))
        return false;

    return std::find(attributes.begin(), attributes.end(), attributeType) != attributes.end();
}

bool ItemAttributesLoader::GetItemAttributesWithValues(Item* item, std::vector<uint32>& attributes, std::vector<AsInt64>& values, uint32* itemId) const
{
    attributes.clear();
    values.clear();

    if (!item)
        return false;

    uint64 itemGuid = item->GetGUID().GetCounter();
    auto data = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);  // 【智能指针修复】自动管理内存

    if (!data)
        return false;

    if (itemId)
        *itemId = data->itemId;

    attributes.reserve(data->baseAttributeIds.size() + data->additionalAttributeIds.size());
    values.reserve(data->baseAttributeValues.size() + data->additionalAttributeValues.size());

    attributes.insert(attributes.end(), data->baseAttributeIds.begin(), data->baseAttributeIds.end());
    attributes.insert(attributes.end(), data->additionalAttributeIds.begin(), data->additionalAttributeIds.end());

    values.insert(values.end(), data->baseAttributeValues.begin(), data->baseAttributeValues.end());
    values.insert(values.end(), data->additionalAttributeValues.begin(), data->additionalAttributeValues.end());

    // 【智能指针修复】移除手动 delete，unique_ptr 自动清理
    return true;
}

std::vector<uint32> ItemAttributesLoader::GetItemAttributes(Item* item) const
{
    std::vector<uint32> result;

    if (!item)
        return result;

    std::vector<AsInt64> values;
    GetItemAttributesWithValues(item, result, values);
    return result;
}

std::vector<AsInt64> ItemAttributesLoader::GetItemAttributeValues(Item* item) const
{
    std::vector<AsInt64> result;

    if (!item)
        return result;

    std::vector<uint32> attributes;
    GetItemAttributesWithValues(item, attributes, result);
    return result;
}

AsInt64 ItemAttributesLoader::CalculateAttributeValue(Item* item, ItemAttributeTemplate const* attributeTemplate) const
{
    if (!item || !attributeTemplate)
        return 0;

    AsInt64 baseValue = 0;
    AsInt64 minValue = attributeTemplate->minPercent;
    AsInt64 maxValue = attributeTemplate->maxPercent;
    if (minValue > maxValue)
        std::swap(minValue, maxValue);

    // 根据计算方式确定基础值
    switch (attributeTemplate->calcType)
    {
        case 0: // 常规
            baseValue = minValue;
            break;
        case 1: // 乘以物品等级
            baseValue = ScaleAttributeValueByItemLevel(minValue, item->GetTemplate()->ItemLevel);
            break;
        case 2: // 固定值
            baseValue = minValue;
            break;
        default:
            baseValue = minValue;
            break;
    }

    // 如果最小值和最大值不同，随机生成一个范围内的值
    if (minValue != maxValue)
    {
        AsInt64 randomValue = RandomInt64InRange(minValue, maxValue);

        if (attributeTemplate->calcType == 1) // 乘以物品等级
            baseValue = ScaleAttributeValueByItemLevel(randomValue, item->GetTemplate()->ItemLevel);
        else
            baseValue = randomValue;
    }

    return baseValue;
}

AsInt64 ItemAttributesLoader::CalculateAttributeValueWithRange(Item* item, ItemAttributeTemplate const* attributeTemplate, AsInt64 minValue, AsInt64 maxValue) const
{
    if (!item || !attributeTemplate)
        return 0;

    // 使用指定的值范围直接生成属性值
    if (minValue == 0 && maxValue == 0)
    {
        LOG_DEBUG("module.item-attributes", ModuleOldFormat("属性模板 %s 的指定范围为0，返回空属性值", attributeTemplate->id).c_str());
        return 0;
    }

    // 确保最小值不大于最大值
    if (minValue > maxValue)
    {
        std::swap(minValue, maxValue);
    }

    // 在指定范围内随机生成属性值
    return RandomInt64InRange(minValue, maxValue);
}

void ItemAttributesLoader::DumpItemAttributes(Item* item) const
{
    if (!item)
    {
        LOG_ERROR("module.item-attributes", "ItemAttributesLoader::DumpItemAttributes: 无效的物品");
        return;
    }

    ItemTemplate const* itemTemplate = item->GetTemplate();
    LOG_INFO("module.item-attributes", ModuleOldFormat("物品属性信息 - [%s] %s:", itemTemplate->ItemId, itemTemplate->Name1).c_str());

    std::vector<uint32> attributes;
    std::vector<AsInt64> values;
    uint32 itemId = 0;

    if (!GetItemAttributesWithValues(item, attributes, values, &itemId) || attributes.empty())
    {
        LOG_INFO("module.item-attributes", "  该物品没有任何属性");
        return;
    }

    // 显示属性基本信息
    LOG_INFO("module.item-attributes", ModuleOldFormat("  物品ID: %s", itemId).c_str());
    LOG_INFO("module.item-attributes", ModuleOldFormat("  属性总数: %s", static_cast<uint32>(attributes.size())).c_str());

    // 显示每个属性的详细信息
    LOG_INFO("module.item-attributes", "  属性列表:");
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        uint32 attributeType = attributes[i];
        // 【重要】attributes中存的是属性类型，不是模板ID
        ItemAttributeTemplate const* attributeTemplate = GetItemAttributeTemplateByType(attributeType);
        if (!attributeTemplate)
        {
            LOG_ERROR("module.item-attributes", ModuleOldFormat("    无效的属性类型: %s", attributeType).c_str());
            continue;
        }

        AsInt64 value = (i < values.size()) ? values[i] : 0;
        LOG_INFO("module.item-attributes", ModuleOldFormat("    [%s] %s: %s (类型: %s, 组: %s)", attributeTemplate->id, attributeTemplate->clientDisplay, Acore::ToString(value), attributeTemplate->attributeType, attributeTemplate->group).c_str());

        // 【关键修复】添加空指针检查
        // 显示属性的详细描述
        if (sItemAttributesEffects)
        {
            std::string description = sItemAttributesEffects->GetAttributeDescription(item, attributeType);
            LOG_INFO("module.item-attributes", ModuleOldFormat("      描述: %s", description).c_str());
        }
    }
}

void ItemAttributesLoader::DeleteItemAttributeData(uint64 itemGuid)
{
    if (itemGuid == 0)
        return;

    ItemAttributesDBHelper::ClearItemAttributes(itemGuid);
}

void ItemAttributesLoader::CleanupOrphanedAttributeData()
{
    // 【数据库错误处理】添加 try-catch 保护
    try
    {
        CharacterDatabase.Execute(
            "DELETE a FROM `物品属性_数据` a "
            "LEFT JOIN `item_instance` i ON a.`物品GUID` = i.`guid` "
            "LEFT JOIN `character_inventory` ci ON ci.`item` = a.`物品GUID` AND ci.`bag` = 200 "
            "WHERE i.`guid` IS NULL AND ci.`item` IS NULL"
        );
        ItemAttributesDBHelper::FlushCache();
        LOG_DEBUG("module.item-attributes", "CleanupOrphanedAttributeData 完成");
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR("module.item-attributes", ModuleOldFormat("【数据库错误】CleanupOrphanedAttributeData 失败: %s", ex.what()).c_str());
    }
}
