#include "ItemSkillsDBHelper.h"
#include "ItemSkillsManager.h"
#include "DatabaseEnv.h"
#include "Logging/Log.h"
#include "Item.h"
#include <sstream>
#include <vector>
#include <type_traits>
#include <iomanip>
#include "ModuleFormat.h"

// 辅助函数：格式化浮点数，移除多余的0
static std::string FormatFloat(float value)
{
    // 如果是整数，直接返回整数形式
    if (value == static_cast<int>(value))
    {
        return std::to_string(static_cast<int>(value));
    }

    // 否则保留最多2位小数，去除尾部的0
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    std::string str = oss.str();

    // 移除尾部的0
    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
    // 如果最后是小数点，也移除
    if (str.back() == '.')
        str.pop_back();

    return str;
}

// 辅助函数：分割逗号分隔的字符串
template<typename T>
static std::vector<T> SplitString(const std::string& str)
{
    std::vector<T> result;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, ','))
    {
        if (!item.empty())
        {
            // 对于uint8，需要先转成int再转成uint8，避免被当作字符处理
            if (std::is_same<T, uint8>::value || std::is_same<T, int8>::value)
            {
                int tempValue;
                std::stringstream converter(item);
                converter >> tempValue;
                result.push_back(static_cast<T>(tempValue));
            }
            else
            {
                std::stringstream converter(item);
                T value;
                converter >> value;
                result.push_back(value);
            }
        }
    }

    return result;
}

ItemSkillsDBHelper* ItemSkillsDBHelper::instance()
{
    static ItemSkillsDBHelper instance;
    return &instance;
}

// 查询物品的所有技能数据（通过GUID）
// 新格式：一行存储多个技能，逗号分隔
std::vector<ItemSkillData> ItemSkillsDBHelper::GetItemSkillsData(uint64 itemGuid)
{
    std::vector<ItemSkillData> result;

    if (itemGuid == 0)
    {
        LOG_DEBUG("module.itemskills", "GetItemSkillsData: itemGuid=0，跳过查询");
        return result;
    }

    // 优化：先从缓存读取
    auto it = _skillsCache.find(itemGuid);
    if (it != _skillsCache.end() &&
        (time(nullptr) - it->second.cacheTime) < CACHE_EXPIRE_TIME)
    {
        // 缓存命中
        LOG_DEBUG("module.itemskills", ModuleOldFormat("[Query] 缓存命中，跳过数据库查询: GUID=%s", itemGuid).c_str());
        return it->second.skills;
    }

    // 缓存未命中或已过期，查询数据库
    LOG_DEBUG("module.itemskills", ModuleOldFormat("[Query] 缓存未命中，从数据库加载: GUID=%s", itemGuid).c_str());

    QueryResult queryResult = CharacterDatabase.Query(ModuleOldFormat("SELECT `物品ID`, `物品GUID`, `技能模板ID`, `技能ID`, `技能触发类型`, `触发几率` "
        "FROM `_物品技能_数据` WHERE `物品GUID` = %s", itemGuid).c_str());

    if (!queryResult)
    {
        LOG_DEBUG("module.itemskills", ModuleOldFormat("GetItemSkillsData: 未找到数据，GUID=%s", itemGuid).c_str());

        // 更新缓存（空结果也缓存，避免重复查询不存在的数据）
        SkillsCache cache;
        cache.skills = result;
        cache.cacheTime = time(nullptr);
        _skillsCache[itemGuid] = cache;

        return result;
    }

    LOG_DEBUG("module.itemskills", ModuleOldFormat("GetItemSkillsData: 找到记录，GUID=%s", itemGuid).c_str());

    Field* fields = queryResult->Fetch();

    uint32 itemId = fields[0].Get<uint32>();
    uint64 itemGuidDb = fields[1].Get<uint64>();

    // 解析逗号分隔的字符串
    std::string templateIdsStr = fields[2].Get<std::string>();
    std::string skillIdsStr = fields[3].Get<std::string>();
    std::string triggerTypesStr = fields[4].Get<std::string>();
    std::string triggerChancesStr = fields[5].Get<std::string>();

    LOG_DEBUG("module.itemskills", ModuleOldFormat("GetItemSkillsData: 原始数据 - 模板ID:'%s', 技能ID:'%s', 触发类型:'%s', 几率:'%s'", templateIdsStr, skillIdsStr, triggerTypesStr, triggerChancesStr).c_str());

    std::vector<uint32> templateIds = SplitString<uint32>(templateIdsStr);
    std::vector<uint32> skillIds = SplitString<uint32>(skillIdsStr);
    std::vector<uint8> triggerTypes = SplitString<uint8>(triggerTypesStr);
    std::vector<float> triggerChances = SplitString<float>(triggerChancesStr);

    LOG_DEBUG("module.itemskills", ModuleOldFormat("GetItemSkillsData: 解析结果 - 模板ID数:%s, 技能ID数:%s, 触发类型数:%s, 几率数:%s", templateIds.size(), skillIds.size(), triggerTypes.size(), triggerChances.size()).c_str());

    // 确保所有数组长度一致
    size_t skillCount = templateIds.size();
    if (skillIds.size() < skillCount) skillCount = skillIds.size();
    if (triggerTypes.size() < skillCount) skillCount = triggerTypes.size();
    if (triggerChances.size() < skillCount) skillCount = triggerChances.size();

    LOG_DEBUG("module.itemskills", ModuleOldFormat("GetItemSkillsData: 最终技能数量=%s", skillCount).c_str());

    // 构造技能数据
    for (size_t i = 0; i < skillCount; ++i)
    {
        ItemSkillData data;
        data.itemId = itemId;
        data.itemGuid = itemGuidDb;
        data.skillTemplateId = templateIds[i];
        data.skillId = skillIds[i];
        data.triggerType = triggerTypes[i];
        data.triggerChance = triggerChances[i];

        result.push_back(data);
    }

    // 更新缓存
    SkillsCache cache;
    cache.skills = result;
    cache.cacheTime = time(nullptr);
    _skillsCache[itemGuid] = cache;

    LOG_DEBUG("module.itemskills", ModuleOldFormat("[Query] 缓存已更新: GUID=%s, 技能数=%s", itemGuid, result.size()).c_str());

    return result;
}

// 查询物品的所有技能数据（通过Item对象）
std::vector<ItemSkillData> ItemSkillsDBHelper::GetItemSkillsData(Item* item)
{
    if (!item)
        return {};

    uint64 itemGuid = item->GetGUID().GetCounter();
    return GetItemSkillsData(itemGuid);
}

// 批量查询多个物品的技能数据（性能优化）
std::map<uint64, std::vector<ItemSkillData>> ItemSkillsDBHelper::GetItemSkillsDataBatch(const std::vector<uint64>& itemGuids)
{
    std::map<uint64, std::vector<ItemSkillData>> result;

    if (itemGuids.empty())
    {
        LOG_DEBUG("module.itemskills", "[批量查询] itemGuids为空，跳过查询");
        return result;
    }

    // 1. 先从缓存读取，收集未缓存的GUID
    std::vector<uint64> needQueryGuids;
    time_t currentTime = time(nullptr);

    for (uint64 itemGuid : itemGuids)
    {
        auto it = _skillsCache.find(itemGuid);
        if (it != _skillsCache.end() && (currentTime - it->second.cacheTime) < CACHE_EXPIRE_TIME)
        {
            // 缓存命中
            result[itemGuid] = it->second.skills;
            LOG_DEBUG("module.itemskills", ModuleOldFormat("[批量查询] 缓存命中: GUID=%s", itemGuid).c_str());
        }
        else
        {
            // 缓存未命中或已过期
            needQueryGuids.push_back(itemGuid);
        }
    }

    // 2. 如果所有数据都在缓存中，直接返回
    if (needQueryGuids.empty())
    {
        return result;
    }

    // 3. 构建 IN 子句进行批量查询
    std::string guidList;
    for (size_t i = 0; i < needQueryGuids.size(); ++i)
    {
        if (i > 0) guidList += ",";
        guidList += std::to_string(needQueryGuids[i]);
    }

    LOG_DEBUG("module.itemskills", ModuleOldFormat("[批量查询] SQL IN子句: %s", guidList).c_str());

    // 4. 执行批量查询
    QueryResult queryResult = CharacterDatabase.Query(ModuleOldFormat("SELECT `物品ID`, `物品GUID`, `技能模板ID`, `技能ID`, `技能触发类型`, `触发几率` "
        "FROM `_物品技能_数据` WHERE `物品GUID` IN (%s)", guidList).c_str());

    if (!queryResult)
    {
        LOG_DEBUG("module.itemskills", "[批量查询] 未找到任何数据");

        // 将空结果也缓存，避免重复查询不存在的数据
        for (uint64 itemGuid : needQueryGuids)
        {
            SkillsCache cache;
            cache.skills = {};
            cache.cacheTime = currentTime;
            _skillsCache[itemGuid] = cache;
            result[itemGuid] = {};
        }

        return result;
    }

    // 5. 解析查询结果
    uint32 foundRows = 0;
    do
    {
        Field* fields = queryResult->Fetch();

        uint32 itemId = fields[0].Get<uint32>();
        uint64 itemGuid = fields[1].Get<uint64>();

        // 解析逗号分隔的字符串
        std::string templateIdsStr = fields[2].Get<std::string>();
        std::string skillIdsStr = fields[3].Get<std::string>();
        std::string triggerTypesStr = fields[4].Get<std::string>();
        std::string triggerChancesStr = fields[5].Get<std::string>();

        std::vector<uint32> templateIds = SplitString<uint32>(templateIdsStr);
        std::vector<uint32> skillIds = SplitString<uint32>(skillIdsStr);
        std::vector<uint8> triggerTypes = SplitString<uint8>(triggerTypesStr);
        std::vector<float> triggerChances = SplitString<float>(triggerChancesStr);

        // 确保所有数组长度一致
        size_t skillCount = templateIds.size();
        if (skillIds.size() < skillCount) skillCount = skillIds.size();
        if (triggerTypes.size() < skillCount) skillCount = triggerTypes.size();
        if (triggerChances.size() < skillCount) skillCount = triggerChances.size();

        // 构造技能数据列表
        std::vector<ItemSkillData> skills;
        for (size_t i = 0; i < skillCount; ++i)
        {
            ItemSkillData data;
            data.itemId = itemId;
            data.itemGuid = itemGuid;
            data.skillTemplateId = templateIds[i];
            data.skillId = skillIds[i];
            data.triggerType = triggerTypes[i];
            data.triggerChance = triggerChances[i];
            skills.push_back(data);
        }

        // 添加到结果
        result[itemGuid] = skills;

        // 更新缓存
        SkillsCache cache;
        cache.skills = skills;
        cache.cacheTime = currentTime;
        _skillsCache[itemGuid] = cache;

        foundRows++;

    } while (queryResult->NextRow());

    LOG_INFO("module.itemskills", ModuleOldFormat("[批量查询] 查询完成，找到 %s 个物品的技能数据", foundRows).c_str());

    // 6. 对于查询结果中没有的GUID，也缓存空结果
    for (uint64 itemGuid : needQueryGuids)
    {
        if (result.find(itemGuid) == result.end())
        {
            SkillsCache cache;
            cache.skills = {};
            cache.cacheTime = currentTime;
            _skillsCache[itemGuid] = cache;
            result[itemGuid] = {};
        }
    }

    return result;
}

// 添加技能到物品（从模板读取并复制关键字段）
// 新格式：追加到同一行的逗号分隔列表
void ItemSkillsDBHelper::AddSkillToItem(Item* item, uint32 skillTemplateId)
{
    if (!item)
        return;

    uint32 itemId = item->GetEntry();
    uint64 itemGuid = item->GetGUID().GetCounter();

    // 从模板库获取技能信息
    ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillTemplateId);
    if (!skillTemplate)
    {
        LOG_ERROR("module.itemskills", ModuleOldFormat("AddSkillToItem: 未找到技能模板 ID=%s", skillTemplateId).c_str());
        return;
    }

    // 检查是否已存在该物品的记录
    QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT `技能模板ID`, `技能ID`, `技能触发类型`, `触发几率` "
        "FROM `_物品技能_数据` WHERE `物品GUID` = %s", itemGuid).c_str());

    if (result)
    {
        // 已有记录，追加到现有数据
        Field* fields = result->Fetch();
        std::string templateIdsStr = fields[0].Get<std::string>();
        std::string skillIdsStr = fields[1].Get<std::string>();
        std::string triggerTypesStr = fields[2].Get<std::string>();
        std::string triggerChancesStr = fields[3].Get<std::string>();

        // 检查是否已包含该技能
        std::vector<uint32> templateIds = SplitString<uint32>(templateIdsStr);
        for (uint32 existingId : templateIds)
        {
            if (existingId == skillTemplateId)
            {
                // LOG_WARN("module.itemskills", "AddSkillToItem: 物品 {} 已拥有技能模板 {}", itemGuid, skillTemplateId);
                return;
            }
        }

        // 追加新技能
        if (!templateIdsStr.empty()) templateIdsStr += ",";
        if (!skillIdsStr.empty()) skillIdsStr += ",";
        if (!triggerTypesStr.empty()) triggerTypesStr += ",";
        if (!triggerChancesStr.empty()) triggerChancesStr += ",";

        templateIdsStr += std::to_string(skillTemplateId);
        skillIdsStr += std::to_string(skillTemplate->spellId);
        triggerTypesStr += std::to_string(skillTemplate->triggerType);
        triggerChancesStr += FormatFloat(skillTemplate->triggerChance);

        // 更新记录，使用DirectExecute确保立即执行
        CharacterDatabase.DirectExecute(ModuleOldFormat("UPDATE `_物品技能_数据` SET"
            "`技能模板ID` = '%s', `技能ID` = '%s', `技能触发类型` = '%s', `触发几率` = '%s' "
            "WHERE `物品GUID` = %s", templateIdsStr, skillIdsStr, triggerTypesStr, triggerChancesStr, itemGuid).c_str());
    }
    else
    {
        // 没有记录，插入新行
        // 使用DirectExecute确保立即执行
        CharacterDatabase.DirectExecute(ModuleOldFormat("INSERT INTO `_物品技能_数据` "
            "(`物品ID`, `物品GUID`, `技能模板ID`, `技能ID`, `技能触发类型`, `触发几率`) "
            "VALUES (%s, %s, '%s', '%s', '%s', '%s')", itemId, itemGuid, skillTemplateId, skillTemplate->spellId, skillTemplate->triggerType, FormatFloat(skillTemplate->triggerChance)).c_str());
    }

    // 清除缓存
    ClearCache(itemGuid);
}

// 从物品移除技能
// 新格式：从逗号分隔的列表中移除指定技能
void ItemSkillsDBHelper::RemoveSkillFromItem(Item* item, uint32 skillTemplateId)
{
    if (!item)
        return;

    uint64 itemGuid = item->GetGUID().GetCounter();

    // 读取现有数据
    QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT `技能模板ID`, `技能ID`, `技能触发类型`, `触发几率` "
        "FROM `_物品技能_数据` WHERE `物品GUID` = %s", itemGuid).c_str());

    if (!result)
    {
        LOG_WARN("module.itemskills", ModuleOldFormat("RemoveSkillFromItem: 物品 %s 没有技能数据", itemGuid).c_str());
        return;
    }

    Field* fields = result->Fetch();
    std::vector<uint32> templateIds = SplitString<uint32>(fields[0].Get<std::string>());
    std::vector<uint32> skillIds = SplitString<uint32>(fields[1].Get<std::string>());
    std::vector<uint8> triggerTypes = SplitString<uint8>(fields[2].Get<std::string>());
    std::vector<float> triggerChances = SplitString<float>(fields[3].Get<std::string>());

    // 找到要移除的技能索引
    int removeIndex = -1;
    for (size_t i = 0; i < templateIds.size(); ++i)
    {
        if (templateIds[i] == skillTemplateId)
        {
            removeIndex = static_cast<int>(i);
            break;
        }
    }

    if (removeIndex == -1)
    {
        LOG_WARN("module.itemskills", ModuleOldFormat("RemoveSkillFromItem: 物品 %s 没有技能模板 %s", itemGuid, skillTemplateId).c_str());
        return;
    }

    // 移除该索引的元素（安全检查数组边界）
    size_t idx = static_cast<size_t>(removeIndex);
    if (idx < templateIds.size())
        templateIds.erase(templateIds.begin() + removeIndex);
    if (idx < skillIds.size())
        skillIds.erase(skillIds.begin() + removeIndex);
    if (idx < triggerTypes.size())
        triggerTypes.erase(triggerTypes.begin() + removeIndex);
    if (idx < triggerChances.size())
        triggerChances.erase(triggerChances.begin() + removeIndex);

    // 如果没有剩余技能，删除整行
    if (templateIds.empty())
    {
        CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `_物品技能_数据` WHERE `物品GUID` = %s", itemGuid).c_str());
        LOG_INFO("module.itemskills", ModuleOldFormat("物品 %s 移除最后一个技能，已删除记录", itemGuid).c_str());

        // 清除缓存
        ClearCache(itemGuid);
        return;
    }

    // 重新构建逗号分隔的字符串
    std::string templateIdsStr, skillIdsStr, triggerTypesStr, triggerChancesStr;
    for (size_t i = 0; i < templateIds.size(); ++i)
    {
        if (i > 0)
        {
            templateIdsStr += ",";
            skillIdsStr += ",";
            triggerTypesStr += ",";
            triggerChancesStr += ",";
        }
        templateIdsStr += std::to_string(templateIds[i]);
        skillIdsStr += std::to_string(skillIds[i]);
        triggerTypesStr += std::to_string(triggerTypes[i]);
        triggerChancesStr += std::to_string(triggerChances[i]);
    }

    // 更新数据库
    CharacterDatabase.Execute(ModuleOldFormat("UPDATE `_物品技能_数据` SET"
        "`技能模板ID` = '%s', `技能ID` = '%s', `技能触发类型` = '%s', `触发几率` = '%s' "
        "WHERE `物品GUID` = %s", templateIdsStr, skillIdsStr, triggerTypesStr, triggerChancesStr, itemGuid).c_str());

    LOG_INFO("module.itemskills", ModuleOldFormat("物品 %s 移除技能模板 %s", itemGuid, skillTemplateId).c_str());

    // 清除缓存
    ClearCache(itemGuid);
}

// 清除物品的所有技能
void ItemSkillsDBHelper::ClearItemSkills(Item* item)
{
    if (!item)
        return;

    uint64 itemGuid = item->GetGUID().GetCounter();

    auto* statement = CharacterDatabase.GetPreparedStatement(CHAR_IDENT_SKILLS_DELETE);
    statement->SetData(0, itemGuid);
    CharacterDatabase.DirectExecute(statement);

    LOG_INFO("module.itemskills", ModuleOldFormat("清除物品 %s 的所有技能", itemGuid).c_str());

    // 清除缓存
    ClearCache(itemGuid);
}

// 检查物品是否有技能数据
bool ItemSkillsDBHelper::HasItemSkills(Item* item)
{
    if (!item)
        return false;

    uint64 itemGuid = item->GetGUID().GetCounter();

    QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT 1 FROM `_物品技能_数据` WHERE `物品GUID` = %s LIMIT 1", itemGuid).c_str());

    return result != nullptr;
}

// 清除缓存（在技能数据被修改时调用）
void ItemSkillsDBHelper::ClearCache(uint64 itemGuid)
{
    auto it = _skillsCache.find(itemGuid);
    if (it != _skillsCache.end())
    {
        _skillsCache.erase(it);
        LOG_DEBUG("module.itemskills", ModuleOldFormat("[Cache] 清除缓存: GUID=%s", itemGuid).c_str());
    }
}
