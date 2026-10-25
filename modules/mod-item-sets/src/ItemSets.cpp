#include "ItemSets.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include "Logging/Log.h"
#include "DatabaseEnv.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "ObjectMgr.h"
#include "ItemTemplate.h"
#include "SpellDefines.h"
#include "SharedDefines.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "AccountMgr.h"
#include "Spell.h"
#include "World.h"
#include "StringConvert.h"
#include "Util.h"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <set>
#include "ModuleFormat.h"

using namespace Acore::ChatCommands;

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

//=============================================================================
// ItemSetsConfig 实现
//=============================================================================

ItemSetsConfig* ItemSetsConfig::instance()
{
    static ItemSetsConfig instance;
    return &instance;
}

bool ItemSetsConfig::LoadConfig()
{
    _enabled = sConfigMgr->GetOption<bool>("物品套装.启用", true);
    _enabled = _enabled && sHelper->GetBoolean(22011);
    // 【修复低风险问题】调试模式默认值与配置文件保持一致（默认禁用）
    _debugMode = sConfigMgr->GetOption<bool>("物品套装.调试模式", false);
    _allowPartialBonus = sConfigMgr->GetOption<bool>("物品套装.允许部分加成", true);
    _maxSetsPerPlayer = sConfigMgr->GetOption<int>("物品套装.最大套装数", 10);
    _showBonusNotification = sConfigMgr->GetOption<bool>("物品套装.显示效果提示", true);
    _showSetIconMark = sConfigMgr->GetOption<bool>("物品套装.显示套装标记", true);

    if (_debugMode)
    {
        LOG_INFO("server.loading", ModuleOldFormat("物品套装: 模块已%s", _enabled ? "启用" : "禁用").c_str());
        LOG_INFO("server.loading", ModuleOldFormat("物品套装: 调试模式已%s", _debugMode ? "启用" : "禁用").c_str());
        LOG_INFO("server.loading", ModuleOldFormat("物品套装: 部分套装加成已%s", _allowPartialBonus ? "启用" : "禁用").c_str());
        LOG_INFO("server.loading", ModuleOldFormat("物品套装: 最大套装数 = %s", _maxSetsPerPlayer).c_str());
        LOG_INFO("server.loading", ModuleOldFormat("物品套装: 显示效果提示已%s", _showBonusNotification ? "启用" : "禁用").c_str());
        LOG_INFO("server.loading", ModuleOldFormat("物品套装: 显示套装标记已%s", _showSetIconMark ? "启用" : "禁用").c_str());
    }

    return true;
}

//=============================================================================
// ItemSetsManager 实现
//=============================================================================

ItemSetsManager* ItemSetsManager::instance()
{
    static ItemSetsManager instance;
    return &instance;
}

bool ItemSetsManager::Initialize()
{
    if (!sItemSetsConfig->IsModuleEnabled())
        return false;

    LoadItemSets();
    LoadPlayerSetRecords();
    return true;
}

void ItemSetsManager::LoadItemSets()
{
    // 清空现有数据
    _itemSets.clear();

    try
    {
        // 从数据库加载套装数据
        QueryResult result = WorldDatabase.Query("SELECT `套装ID`, `组`, `套装名称`, `套装描述`, `需求系统ID`, `套装品质`, `物品数量`, `效果类型`, `套装属性`, `效果ID`, `效果数值`, `效果描述`, `几率` FROM `_模板_物品套装` ORDER BY `套装ID`, `物品数量`");
        if (!result)
        {
           // LOG_INFO("module", "物品套装: 未找到套装数据");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            ItemSetData setData;
            setData.SetId = fields[0].Get<uint32>();
            setData.Group = fields[1].Get<uint32>();
            setData.SetName = fields[2].IsNull() ? "" : fields[2].Get<std::string>();
            setData.SetDesc = fields[3].IsNull() ? "" : fields[3].Get<std::string>();
            setData.RequirementId = fields[4].Get<uint32>();
            setData.SetQuality = fields[5].Get<uint8>();
            setData.ItemsCount = fields[6].Get<uint8>();
            setData.EffectType = fields[7].Get<uint8>();
            setData.SetAttributes = fields[8].IsNull() ? "" : fields[8].Get<std::string>();
            setData.EffectId = fields[9].Get<uint32>();
            setData.EffectValue = fields[10].Get<float>();
            setData.EffectDesc = fields[11].IsNull() ? "" : fields[11].Get<std::string>();

            // 添加到套装数据列表
            _itemSets[setData.SetId].push_back(setData);

            count++;
        } while (result->NextRow());

        // 加载套装组信息
        LoadSetGroups();

    }
    catch (const std::exception& e)
    {
        LOG_ERROR("module", ModuleOldFormat("物品套装: 加载套装数据时发生错误: %s", e.what()).c_str());
    }
}

void ItemSetsManager::LoadSetGroups()
{
    // 清空现有数据
    _setGroups.clear();

    try
    {
        // 从套装数据中提取组信息
        QueryResult result = WorldDatabase.Query("SELECT DISTINCT `组`, `套装ID`, `几率` FROM `_模板_物品套装` WHERE `几率` > 0 ORDER BY `组`, `套装ID`");
        if (!result)
        {
            LOG_WARN("module", "物品套装: 未找到套装组数据");
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();
            uint32 groupId = fields[0].Get<uint32>();
            uint32 setId = fields[1].Get<uint32>();
            float probability = fields[2].Get<float>();

            _setGroups[groupId].GroupId = groupId;
            _setGroups[groupId].SetIds.push_back(setId);
            _setGroups[groupId].Probabilities.push_back(probability);

            count++;
        } while (result->NextRow());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("module", ModuleOldFormat("物品套装: 加载套装组时发生错误: %s", e.what()).c_str());
    }
}

void ItemSetsManager::LoadPlayerSetRecords()
{
    // 清空现有数据
    _playerSetStatus.clear();

    try
    {
        // 从新的表结构加载`_物品套装_数据`（表结构已优化，删除了玩家GUID字段）
        QueryResult result = CharacterDatabase.Query(
            "SELECT s.`物品ID`, s.`物品GUID`, s.`套装ID`, ii.`owner_guid` "
            "FROM `_物品套装_数据` s INNER JOIN `item_instance` ii ON ii.`guid` = s.`物品GUID`");
        if (!result)
        {
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();
            uint32 itemId = fields[0].Get<uint32>();
            uint32 itemGuid = fields[1].Get<uint32>();
            uint32 setId = fields[2].Get<uint32>();
            uint32 playerGuid = fields[3].Get<uint32>();

            // 获取或创建`_物品套装_数据`
            PlayerSetStatus& status = _playerSetStatus[playerGuid];
            status.PlayerGuid = playerGuid;

            // 记录物品的套装分配
            status.ItemSetMap[itemGuid] = setId;

            count++;
        } while (result->NextRow());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("module", ModuleOldFormat("物品套装: 加载`_物品套装_数据`时发生错误: %s", e.what()).c_str());
    }
}

const std::vector<ItemSetData>& ItemSetsManager::GetItemSetData(uint32 setId) const
{
    auto itr = _itemSets.find(setId);
    if (itr != _itemSets.end())
        return itr->second;

    static const std::vector<ItemSetData> empty;
    return empty;
}

std::string ItemSetsManager::GetSetName(uint32 setId) const
{
    auto itr = _itemSets.find(setId);
    if (itr != _itemSets.end() && !itr->second.empty())
    {
        std::string setName = itr->second[0].SetName;
        if (sItemSetsConfig->IsDebugMode())
        {
            LOG_INFO("module", ModuleOldFormat("物品套装: GetSetName - 套装ID:%s 套装名称:%s", setId, setName).c_str());
        }
        return setName;
    }

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_WARN("module", ModuleOldFormat("物品套装: GetSetName - 套装ID:%s 未找到", setId).c_str());
    }
    return "";
}

std::vector<uint32> ItemSetsManager::GetAllSetIds() const
{
    std::vector<uint32> setIds;
    for (const auto& pair : _itemSets)
        setIds.push_back(pair.first);
    return setIds;
}

PlayerSetStatus* ItemSetsManager::GetPlayerSetStatus(uint32 playerGuid)
{
    auto itr = _playerSetStatus.find(playerGuid);
    if (itr != _playerSetStatus.end())
        return &itr->second;
    return nullptr;
}

bool ItemSetsManager::AssignRandomSetToItem(Player* player, uint32 itemId, uint8 slot)
{
    if (!player)
        return false;

    // 智能查找指定物品ID的物品（装备栏和背包）
    Item* item = nullptr;

    // 首先检查装备栏
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (equippedItem && equippedItem->GetEntry() == itemId)
        {
            item = equippedItem;
            break;
        }
    }

    // 如果装备栏没找到，检查背包
    if (!item)
    {
        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        {
            for (uint8 slot = 0; slot < MAX_BAG_SIZE; ++slot)
            {
                Item* bagItem = player->GetItemByPos(bag, slot);
                if (bagItem && bagItem->GetEntry() == itemId)
                {
                    item = bagItem;
                    break;
                }
            }
            if (item) break;
        }
    }

    // 如果还没找到，检查主背包
    if (!item)
    {
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            Item* bagItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (bagItem && bagItem->GetEntry() == itemId)
            {
                item = bagItem;
                break;
            }
        }
    }

    if (!item)
    {
        ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("未找到物品ID %s 在你的装备栏或背包中。", itemId).c_str());
        return false;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 itemGuid = item->GetGUID().GetCounter();

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 找到物品 %s (GUID:%s)", itemId, itemGuid).c_str());
    }

    // 检查该物品是否已经分配过套装
    PlayerSetStatus* status = GetPlayerSetStatus(playerGuid);
    if (status && status->ItemSetMap.find(itemGuid) != status->ItemSetMap.end())
    {
        ChatHandler(player->GetSession()).SendSysMessage("该物品已经分配过套装。");
        return false;
    }

    // 随机选择套装组
    if (_setGroups.empty())
    {
        ChatHandler(player->GetSession()).SendSysMessage("没有可用的套装组。");
        return false;
    }

    // 【修复中风险问题】计算组的总几率时，使用组内所有套装几率之和
    // 而不是只取首个概率值
    float totalProbability = 0.0f;
    std::map<uint32, float> groupTotalProbs; // 组ID -> 组总概率

    for (const auto& pair : _setGroups)
    {
        float groupProb = 0.0f;
        for (float prob : pair.second.Probabilities)
        {
            groupProb += prob;
        }
        groupTotalProbs[pair.first] = groupProb;
        totalProbability += groupProb;
    }

    if (totalProbability <= 0.0f)
    {
        ChatHandler(player->GetSession()).SendSysMessage("套装组几率配置错误。");
        return false;
    }

    float randomValue = urand(0, 10000) / 10000.0f * totalProbability;
    float currentSum = 0.0f;
    uint32 selectedGroupId = 0;

    for (const auto& pair : groupTotalProbs)
    {
        currentSum += pair.second;
        if (randomValue <= currentSum)
        {
            selectedGroupId = pair.first;
            break;
        }
    }

    // 从选中的组中随机选择套装（考虑玩家需求）
    uint32 selectedSetId = GetRandomSetForGroup(selectedGroupId, player);
    if (selectedSetId == 0)
    {
        // 显示具体的需求条件
        ShowGroupRequirements(player, selectedGroupId);
        ChatHandler(player->GetSession()).PSendSysMessage("分配套装失败：不满足需求条件。");
        return false;
    }

    // 在分配套装前，消耗需求物品
    if (sItemSetsConfig->IsDebugMode())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("调试: 开始消耗套装 %s 的需求物品", selectedSetId).c_str());
    }

    if (!ConsumeSetRequirements(player, selectedSetId))
    {
        ChatHandler(player->GetSession()).PSendSysMessage("消耗需求物品失败，分配套装取消。");
        // 显示具体的需求材料
        const auto& setDataList = GetItemSetData(selectedSetId);
        if (!setDataList.empty() && setDataList[0].RequirementId > 0)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("|cFFFF0000请准备以下材料：|r");
            CheckNeedTemplate(player, setDataList[0].RequirementId, true);
        }
        return false;
    }

    if (sItemSetsConfig->IsDebugMode())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("调试: 成功消耗套装 %s 的需求物品", selectedSetId).c_str());
    }

    // 创建或更新`_物品套装_数据`
    if (!status)
    {
        PlayerSetStatus newStatus;
        newStatus.PlayerGuid = playerGuid;
        _playerSetStatus[playerGuid] = newStatus;
        status = &_playerSetStatus[playerGuid];
    }

    // 记录物品的套装分配
    status->ItemSetMap[itemGuid] = selectedSetId;

    // 关键修复：将物品GUID存储到随机附魔槽位第8个字段（PROP_ENCHANTMENT_SLOT_1）
    StoreGuidInEnchantmentSlot(item);

    std::string setName = GetSetName(selectedSetId);
    ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("物品 %s 已分配到套装: %s (组%s)", itemId, setName.c_str(), selectedGroupId).c_str());

    // 保存到数据库
    SavePlayerSetStatus(player);

    // 重新计算并应用套装效果
    RefreshPlayerSetEffects(player);

    return true;
}

// 已废弃：不再使用randomPropertyId字段存储GUID
void ItemSetsManager::StoreGuidInEnchantmentSlot(Item* item)
{
    // 此函数已废弃，不再需要将GUID存储到randomPropertyId字段
    // 现在通过物品GUID直接从数据库查询套装分配
    return;
}

uint32 ItemSetsManager::GetRandomSetForGroup(uint32 groupId)
{
    auto itr = _setGroups.find(groupId);
    if (itr == _setGroups.end() || itr->second.SetIds.empty())
        return 0;

    const SetGroupInfo& groupInfo = itr->second;

    // 计算总几率
    float totalProbability = 0.0f;
    for (float prob : groupInfo.Probabilities)
    {
        totalProbability += prob;
    }

    if (totalProbability <= 0.0f)
        return groupInfo.SetIds[0]; // 如果没有几率配置，返回第一个

    // 【修复低风险问题】使用核心随机函数替代 rand()
    float randomValue = urand(0, 10000) / 10000.0f * totalProbability;
    float currentSum = 0.0f;

    for (size_t i = 0; i < groupInfo.SetIds.size() && i < groupInfo.Probabilities.size(); ++i)
    {
        currentSum += groupInfo.Probabilities[i];
        if (randomValue <= currentSum)
        {
            return groupInfo.SetIds[i];
        }
    }

    return groupInfo.SetIds[0]; // 默认返回第一个
}

uint32 ItemSetsManager::GetRandomSetForGroup(uint32 groupId, Player* player)
{
    auto itr = _setGroups.find(groupId);
    if (itr == _setGroups.end() || itr->second.SetIds.empty())
        return 0;

    const SetGroupInfo& groupInfo = itr->second;

    // 收集玩家满足需求的套装
    std::vector<uint32> validSetIds;
    std::vector<float> validProbabilities;

    for (size_t i = 0; i < groupInfo.SetIds.size(); ++i)
    {
        uint32 setId = groupInfo.SetIds[i];

        // 检查玩家是否满足套装需求
        if (CheckSetRequirements(player, setId))
        {
            validSetIds.push_back(setId);
            if (i < groupInfo.Probabilities.size())
                validProbabilities.push_back(groupInfo.Probabilities[i]);
            else
                validProbabilities.push_back(1.0f); // 默认几率
        }
        else if (sItemSetsConfig->IsDebugMode())
        {
            std::string setName = GetSetName(setId);
            LOG_INFO("module", ModuleOldFormat("物品套装: 玩家 %s 不满足套装 %s 的需求，跳过", player->GetName(), setName).c_str());
        }
    }

    // 如果没有满足需求的套装，返回0
    if (validSetIds.empty())
    {
        if (sItemSetsConfig->IsDebugMode())
        {
            LOG_WARN("module", ModuleOldFormat("物品套装: 组 %s 中没有玩家 %s 满足需求的套装", groupId, player->GetName()).c_str());
        }
        return 0;
    }

    // 计算总几率
    float totalProbability = 0.0f;
    for (float prob : validProbabilities)
    {
        totalProbability += prob;
    }

    if (totalProbability <= 0.0f)
        return validSetIds[0]; // 如果没有几率配置，返回第一个

    // 【修复低风险问题】使用核心随机函数替代 rand()
    float randomValue = urand(0, 10000) / 10000.0f * totalProbability;
    float currentSum = 0.0f;

    for (size_t i = 0; i < validSetIds.size() && i < validProbabilities.size(); ++i)
    {
        currentSum += validProbabilities[i];
        if (randomValue <= currentSum)
        {
            if (sItemSetsConfig->IsDebugMode())
            {
                std::string setName = GetSetName(validSetIds[i]);
                LOG_INFO("module", ModuleOldFormat("物品套装: 为玩家 %s 选择了满足需求的套装 %s (ID:%s)", player->GetName(), setName, validSetIds[i]).c_str());
            }
            return validSetIds[i];
        }
    }

    return validSetIds[0]; // 默认返回第一个
}

bool ItemSetsManager::CheckSetRequirements(Player* player, uint32 setId) const
{
    if (!player)
        return false;

    // 获取套装数据
    const auto& setDataList = GetItemSetData(setId);
    if (setDataList.empty())
        return false;

    uint32 requirementId = setDataList[0].RequirementId;
    if (requirementId == 0)
        return true; // 没有需求限制

    return CheckNeedTemplate(player, requirementId, false);
}

bool ItemSetsManager::ConsumeSetRequirements(Player* player, uint32 setId) const
{
    if (!player)
        return false;

    // 获取套装数据
    const auto& setDataList = GetItemSetData(setId);
    if (setDataList.empty())
        return true; // 没有套装数据，默认成功

    uint32 requirementId = setDataList[0].RequirementId;
    if (requirementId == 0)
        return true; // 没有需求限制，默认成功

    return ConsumeNeedTemplate(player, requirementId, "物品套装需求消耗");
}

void ItemSetsManager::ShowGroupRequirements(Player* player, uint32 groupId)
{
    if (!player)
        return;

    auto itr = _setGroups.find(groupId);
    if (itr == _setGroups.end() || itr->second.SetIds.empty())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("组 %s 不存在或为空。", groupId).c_str());
        return;
    }

    const SetGroupInfo& groupInfo = itr->second;
    ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("组 %s 中的套装需求条件:", groupId).c_str());

    // 收集所有套装的需求ID，去重显示
    std::map<uint32, std::vector<std::string>> requirementGroups; // 需求ID -> 套装名称列表

    for (uint32 setId : groupInfo.SetIds)
    {
        const auto& setDataList = GetItemSetData(setId);
        if (setDataList.empty())
            continue;

        std::string setName = GetSetName(setId);
        uint32 requirementId = setDataList[0].RequirementId;

        requirementGroups[requirementId].push_back(setName);
    }

    // 用于去重的已处理需求ID集合
    std::set<uint32> processedRequirements;

    // 按需求ID分组显示
    for (const auto& pair : requirementGroups)
    {
        uint32 requirementId = pair.first;
        const std::vector<std::string>& setNames = pair.second;

        if (requirementId == 0)
        {
            // 显示无需求限制的套装
            for (const std::string& setName : setNames)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("  - %s (无需求限制)", setName.c_str()).c_str());
            }
        }
        else
        {
            // 显示有相同需求ID的套装列表
            std::string setNameList;
            for (size_t i = 0; i < setNames.size(); ++i)
            {
                if (i > 0) setNameList += ", ";
                setNameList += setNames[i];
            }

            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("  - %s (需求ID: %s)", setNameList.c_str(), requirementId).c_str());

            // 去重检查：如果已经检查过这个需求ID，跳过
            if (processedRequirements.find(requirementId) != processedRequirements.end())
            {
                ChatHandler(player->GetSession()).PSendSysMessage("    (需求条件同上)");
                continue;
            }

            // 标记为已处理
            processedRequirements.insert(requirementId);

            bool requirementsMet = CheckNeedTemplate(player, requirementId, false);

            if (!requirementsMet)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("    |cFFFF0000需求不满足|r");
            }
            else
            {
                ChatHandler(player->GetSession()).PSendSysMessage("    |cFF00FF00需求已满足|r");
            }
        }
    }
}

uint32 ItemSetsManager::GetItemSetId(Player* player, uint32 itemGuid) const
{
    if (!player)
        return 0;

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto it = _playerSetStatus.find(playerGuid);
    if (it == _playerSetStatus.end())
    {
        // 如果`_物品套装_数据`没有加载，尝试加载
        const_cast<ItemSetsManager*>(this)->LoadPlayerSetStatus(player);
        it = _playerSetStatus.find(playerGuid);
        if (it == _playerSetStatus.end())
            return 0;
    }

    auto setIt = it->second.ItemSetMap.find(itemGuid);
    if (setIt != it->second.ItemSetMap.end())
        return setIt->second;

    return 0;
}

void ItemSetsManager::LoadPlayerSetStatus(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 清除现有数据，强制重新加载
    _playerSetStatus.erase(playerGuid);
    _playerEquipmentCache.erase(playerGuid); // 同时清除装备缓存

    PlayerSetStatus status;
    status.PlayerGuid = playerGuid;

    // 【优化】使用单次优化的数据库查询，通过JOIN减少查询次数
    // 同时直接构建装备缓存，避免后续扫描背包
    QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT s.`物品GUID`, s.`套装ID` "
        "FROM `_物品套装_数据` s "
        "INNER JOIN item_instance ii ON s.`物品GUID` = ii.guid "
        "WHERE ii.owner_guid = %s", playerGuid).c_str());

    std::map<uint32, uint32> dbSetAssignments; // GUID -> 套装ID

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 oldItemGuid = fields[0].Get<uint32>();
            uint32 setId = fields[1].Get<uint32>();

            dbSetAssignments[oldItemGuid] = setId;
        } while (result->NextRow());
    }

    // 【修复中风险问题】扫描装备栏和背包，确保所有套装物品都进入内存
    EquipmentCache& cache = _playerEquipmentCache[playerGuid];

    // 辅助lambda：处理单个物品的套装分配检测
    auto processItem = [&](Item* item, uint8 slotIndex = 0xFF, bool isEquipped = false) {
        if (!item)
            return;

        uint32 currentGuid = item->GetGUID().GetCounter();

        // 通过当前物品GUID直接从数据库查找套装分配
        auto it = dbSetAssignments.find(currentGuid);
        if (it != dbSetAssignments.end())
        {
            uint32 setId = it->second;
            status.ItemSetMap[currentGuid] = setId;

            // 如果是装备栏物品，同时构建装备缓存
            if (isEquipped && slotIndex != 0xFF)
            {
                cache.slotToItemGuid[slotIndex] = currentGuid;
                cache.slotToSetId[slotIndex] = setId;
                cache.setIdToCount[setId]++;
            }

            if (sItemSetsConfig->IsDebugMode())
            {
                LOG_INFO("module", ModuleOldFormat("物品套装: 通过GUID恢复套装 - %s GUID:%s 套装ID:%s", isEquipped ? "装备栏" : "背包", currentGuid, setId).c_str());
            }
        }
    };

    // 扫描装备栏
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        processItem(item, i, true);
    }

    // 【修复】扫描主背包
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        processItem(item, 0xFF, false);
    }

    // 【修复】扫描其他背包
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bag = player->GetBagByPos(i);
        if (bag)
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                processItem(item, 0xFF, false);
            }
        }
    }

    // 【数据丢失修复】银行/钥匙链等未扫描容器中的套装物品也要保留记录：
    // 登出保存是"先按 owner 全删、再回写 ItemSetMap"，若这里不把数据库中
    // 其余归属本玩家的分配合并进内存，存银行的套装记录一次登入登出即被永久删除。
    // dbSetAssignments 来自 JOIN item_instance（仅当前仍归属玩家的物品），合并安全；
    // emplace 不覆盖扫描阶段已确定的分配。
    for (auto const& pair : dbSetAssignments)
        status.ItemSetMap.emplace(pair.first, pair.second);

    _playerSetStatus[playerGuid] = status;

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 玩家 %s 套装状态加载完成，共 %s 个套装分配，装备缓存已构建", player->GetName(), status.ItemSetMap.size()).c_str());
    }
}

void ItemSetsManager::SavePlayerSetStatus(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerSetStatus* status = GetPlayerSetStatus(playerGuid);
    if (!status)
        return;

    try
    {
        // 【修复】DELETE+逐条 INSERT 包进同一事务：原先各自异步执行，中途宕机=记录全丢
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        // 删除旧记录（使用子查询关联item_instance表）
        trans->Append(
            ModuleOldFormat("DELETE FROM `_物品套装_数据` WHERE `物品GUID` IN"
                "(SELECT guid FROM item_instance WHERE owner_guid = %s)", playerGuid));

        // 插入新记录 - 使用优化的表结构（字段顺序：物品ID、物品GUID、套装ID）
        for (const auto& pair : status->ItemSetMap)
        {
            uint32 itemGuid = pair.first;
            uint32 setId = pair.second;

            // 查找物品ID
            uint32 itemId = 0;
            Item* item = FindItemByGuid(player, itemGuid);
            if (item)
            {
                itemId = item->GetEntry();
            }

            // 【修复低风险问题】检查物品有效性，避免写入 itemId=0 的无效记录
            if (itemId == 0)
            {
                // 物品未找到，尝试从数据库获取物品ID
                QueryResult itemResult = CharacterDatabase.Query(ModuleOldFormat("SELECT itemEntry FROM item_instance WHERE guid = %s", itemGuid).c_str());
                if (itemResult)
                {
                    itemId = itemResult->Fetch()[0].Get<uint32>();
                }

                // 如果仍然无法获取物品ID，记录警告但跳过保存
                if (itemId == 0)
                {
                    if (sItemSetsConfig->IsDebugMode())
                    {
                        LOG_WARN("module", ModuleOldFormat("物品套装: 无法获取物品ID，跳过保存 - 物品GUID:%s 套装ID:%s", itemGuid, setId).c_str());
                    }
                    continue; // 跳过此物品，不写入无效记录
                }
            }

            // 保存套装分配记录，使用新的表结构（不包含玩家GUID字段）
            // 使用 ON DUPLICATE KEY UPDATE 避免主键冲突（同一物品GUID 已存在时更新记录）
            trans->Append(
                ModuleOldFormat("INSERT INTO `_物品套装_数据` (`物品ID`, `物品GUID`, `套装ID`) VALUES (%s, %s, %s) "
                    "ON DUPLICATE KEY UPDATE `物品ID` = %s, `套装ID` = %s",
                    itemId, itemGuid, setId, itemId, setId)
            );

            if (sItemSetsConfig->IsDebugMode())
            {
                LOG_INFO("module", ModuleOldFormat("物品套装: 保存套装分配 - 物品GUID:%s 物品ID:%s 套装ID:%s", itemGuid, itemId, setId).c_str());
            }
        }

        CharacterDatabase.CommitTransaction(trans);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("module", ModuleOldFormat("物品套装: 保存`_物品套装_数据`时发生错误: %s", e.what()).c_str());
    }
}

// 根据GUID查找物品
Item* ItemSetsManager::FindItemByGuid(Player* player, uint32 itemGuid)
{
    if (!player)
        return nullptr;

    // 在装备栏中查找
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetGUID().GetCounter() == itemGuid)
        {
            return item;
        }
    }

    // 在主背包中查找
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetGUID().GetCounter() == itemGuid)
        {
            return item;
        }
    }

    // 在其他背包中查找
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bag = player->GetBagByPos(i);
        if (bag)
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                if (item && item->GetGUID().GetCounter() == itemGuid)
                {
                    return item;
                }
            }
        }
    }

    return nullptr;
}

void ItemSetsManager::RefreshPlayerSetEffects(Player* player)
{
    if (!player)
        return;

    // 优化: 使用增量更新机制替代全量刷新
    RefreshPlayerSetEffectsOptimized(player, 0xFF);
}

// 【新增】增量更新优化函数 - 只更新变化的套装
void ItemSetsManager::RefreshPlayerSetEffectsOptimized(Player* player, uint8 changedSlot)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerSetStatus* status = GetPlayerSetStatus(playerGuid);
    if (!status)
    {
        LoadPlayerSetStatus(player);
        status = GetPlayerSetStatus(playerGuid);
        if (!status)
            return;
    }

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 开始增量刷新玩家 %s 的套装效果 (槽位:%s)", player->GetName(), changedSlot == 0xFF ? "全部" : std::to_string(changedSlot)).c_str());
    }

    EquipmentCache& cache = _playerEquipmentCache[playerGuid];

    // 如果是全量刷新(0xFF)或缓存为空,重建整个缓存
    if (changedSlot == 0xFF || cache.slotToItemGuid.empty())
    {
        RebuildEquipmentCache(player);
        return;
    }

    // 增量更新:只处理变化的槽位
    std::map<uint32, int8> setIdChanges; // 套装ID -> 件数变化 (+1/-1/0)

    // 获取旧的装备信息
    uint32 oldItemGuid = cache.slotToItemGuid[changedSlot];
    uint32 oldSetId = cache.slotToSetId[changedSlot];

    // 获取新的装备信息
    Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, changedSlot);
    uint32 newItemGuid = newItem ? newItem->GetGUID().GetCounter() : 0;
    uint32 newSetId = 0;

    if (newItem)
    {
        auto it = status->ItemSetMap.find(newItemGuid);
        if (it != status->ItemSetMap.end())
        {
            newSetId = it->second;
        }
    }

    // 检测套装变化
    if (oldSetId != newSetId)
    {
        if (oldSetId > 0)
        {
            setIdChanges[oldSetId] = -1;
            cache.setIdToCount[oldSetId]--;
            if (cache.setIdToCount[oldSetId] == 0)
            {
                cache.setIdToCount.erase(oldSetId);
            }
        }

        if (newSetId > 0)
        {
            setIdChanges[newSetId] = +1;
            cache.setIdToCount[newSetId]++;
        }

        // 更新缓存
        cache.slotToItemGuid[changedSlot] = newItemGuid;
        cache.slotToSetId[changedSlot] = newSetId;

        if (sItemSetsConfig->IsDebugMode())
        {
            LOG_INFO("module", ModuleOldFormat("物品套装: 槽位 %s 套装变化 - 旧:%s 新:%s", changedSlot, oldSetId, newSetId).c_str());
        }
    }

    // 只更新变化的套装效果
    for (const auto& change : setIdChanges)
    {
        uint32 setId = change.first;
        uint8 newCount = cache.setIdToCount[setId];

        // 应用这个套装的新效果
        ApplySingleSetEffect(player, setId, newCount);

        std::string setName = GetSetName(setId);
        if (sItemSetsConfig->IsDebugMode())
        {
            LOG_INFO("module", ModuleOldFormat("物品套装: 更新套装 %s 效果 - 件数:%s", setName, newCount).c_str());
        }

        if (sItemSetsConfig->ShowBonusNotification() && newCount > 0)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("套装效果更新: %s (%s件)", setName.c_str(), newCount).c_str());
        }
    }

    // 统一更新玩家属性，避免在每个属性修改中重复全量更新
    if (!setIdChanges.empty())
    {
        UpdatePlayerStats(player);
    }

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 完成增量刷新玩家 %s 的套装效果", player->GetName()).c_str());
    }
}

// 【新增】重建装备缓存 - 用于全量刷新
void ItemSetsManager::RebuildEquipmentCache(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerSetStatus* status = GetPlayerSetStatus(playerGuid);
    if (!status)
        return;

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 重建玩家 %s 的装备缓存", player->GetName()).c_str());
    }

    EquipmentCache& cache = _playerEquipmentCache[playerGuid];
    cache.Clear();

    // 清除旧的套装效果
    for (const auto& pair : status->ActiveAttributes)
    {
        if (sItemSetsConfig->IsDebugMode())
        {
            LOG_INFO("module", ModuleOldFormat("物品套装: 移除套装 %s 的属性: %s", pair.first, pair.second).c_str());
        }
        ApplySetEffects(player, pair.first, pair.second, false);

        // 额外清除光环
        const auto& setDataList = GetItemSetData(pair.first);
        for (const auto& setData : setDataList)
        {
            if (setData.EffectId > 0 && player->HasAura(setData.EffectId))
            {
                player->RemoveAurasDueToSpell(setData.EffectId);
            }
        }
    }
    status->ActiveAttributes.clear();

    // 遍历所有装备槽位,重建缓存
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item)
        {
            uint32 itemGuid = item->GetGUID().GetCounter();
            auto it = status->ItemSetMap.find(itemGuid);
            if (it != status->ItemSetMap.end())
            {
                uint32 setId = it->second;
                cache.slotToItemGuid[i] = itemGuid;
                cache.slotToSetId[i] = setId;
                cache.setIdToCount[setId]++;

                if (sItemSetsConfig->IsDebugMode())
                {
                    LOG_INFO("module", ModuleOldFormat("物品套装: 槽位 %s 发现套装物品 - 套装ID:%s GUID:%s", i, setId, itemGuid).c_str());
                }
            }
        }
    }

    // 为每个套装应用效果
    for (const auto& countPair : cache.setIdToCount)
    {
        uint32 setId = countPair.first;
        uint8 count = countPair.second;

        ApplySingleSetEffect(player, setId, count);

        std::string setName = GetSetName(setId);
        if (sItemSetsConfig->IsDebugMode())
        {
            LOG_INFO("module", ModuleOldFormat("物品套装: 应用套装 %s 效果 - 件数:%s", setName, count).c_str());
        }

        if (sItemSetsConfig->ShowBonusNotification())
        {
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("套装效果激活: %s (%s件)", setName.c_str(), count).c_str());
        }
    }

    // 重建完成后统一更新玩家属性
    UpdatePlayerStats(player);

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 完成重建玩家 %s 的装备缓存，共 %s 个套装", player->GetName(), cache.setIdToCount.size()).c_str());
    }
}

// 【新增】应用单个套装的效果 - 智能增量更新
void ItemSetsManager::ApplySingleSetEffect(Player* player, uint32 setId, uint8 currentCount)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerSetStatus* status = GetPlayerSetStatus(playerGuid);
    if (!status)
        return;

    const auto& setDataList = GetItemSetData(setId);
    if (setDataList.empty())
        return;

    // 【修复】先移除所有旧的EffectId效果（不管件数门槛）
    for (const auto& setData : setDataList)
    {
        if (setData.EffectId > 0)
        {
            ApplyEnhancedEffect(player, setData.EffectId, setData.EffectValue, false);
        }
    }

    // 移除旧的属性效果
    auto it = status->ActiveAttributes.find(setId);
    if (it != status->ActiveAttributes.end())
    {
        if (!it->second.empty())
        {
            ApplyStatEffects(player, it->second, false);
        }
    }

    // 如果件数为0,直接删除并返回
    if (currentCount == 0)
    {
        status->ActiveAttributes.erase(setId);
        return;
    }

    // 计算新的属性，并应用达到门槛的EffectId效果
    std::string combinedAttributes = "";
    for (const auto& setData : setDataList)
    {
        if (setData.ItemsCount <= currentCount)
        {
            // 属性效果
            if (!setData.SetAttributes.empty())
            {
                if (!combinedAttributes.empty())
                    combinedAttributes += ",";
                combinedAttributes += setData.SetAttributes;
            }

            // 【修复】按件数门槛应用EffectId效果
            if (setData.EffectId > 0)
            {
                ApplyEnhancedEffect(player, setData.EffectId, setData.EffectValue, true);
                if (sItemSetsConfig->IsDebugMode())
                {
                    LOG_INFO("module", ModuleOldFormat("物品套装: 应用效果ID:%s (需要%s件，当前%s件)", setData.EffectId, setData.ItemsCount, currentCount).c_str());
                }
            }
        }
    }

    // 应用新的属性效果
    status->ActiveAttributes[setId] = combinedAttributes;
    if (!combinedAttributes.empty())
    {
        ApplyStatEffects(player, combinedAttributes, true);
    }

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 应用套装 %s 的效果 - 件数:%s 属性:%s", setId, currentCount, combinedAttributes).c_str());
    }
}

void ItemSetsManager::CalculateAndApplySetEffects(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerSetStatus* status = GetPlayerSetStatus(playerGuid);
    if (!status)
        return;

    // 计算当前装备的套装件数
    std::map<uint32, uint32> setIdCounts; // 套装ID -> 件数

    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item)
        {
            uint32 itemGuid = item->GetGUID().GetCounter();
            auto it = status->ItemSetMap.find(itemGuid);
            if (it != status->ItemSetMap.end())
            {
                setIdCounts[it->second]++;
                if (sItemSetsConfig->IsDebugMode())
                {
                    LOG_INFO("module", ModuleOldFormat("物品套装: 发现套装物品 GUID:%s 套装ID:%s 当前件数:%s", itemGuid, it->second, setIdCounts[it->second]).c_str());
                }
            }
        }
    }

    // 为每个套装计算并应用效果
    for (const auto& setCountPair : setIdCounts)
    {
        uint32 setId = setCountPair.first;
        uint32 equippedCount = setCountPair.second;

        if (equippedCount == 0)
            continue;

        const auto& setDataList = GetItemSetData(setId);
        std::string combinedAttributes = "";

        // 根据装备件数激活对应的套装效果
        for (const auto& setData : setDataList)
        {
            if (setData.ItemsCount <= equippedCount)
            {
                // 只处理属性数据，效果ID会在ApplySetEffects中单独处理
                if (!setData.SetAttributes.empty())
                {
                    if (!combinedAttributes.empty())
                        combinedAttributes += ",";
                    combinedAttributes += setData.SetAttributes;
                }
            }
        }

        // 无论是否有属性数据，都应用套装效果（因为可能有EffectId）
        // 检查是否已经激活了相同的属性，避免重复应用
        auto activeIt = status->ActiveAttributes.find(setId);
        if (activeIt == status->ActiveAttributes.end() || activeIt->second != combinedAttributes)
        {
            // 如果之前有不同的属性，先移除
            if (activeIt != status->ActiveAttributes.end())
            {
                ApplySetEffects(player, setId, activeIt->second, false);
            }

            // 应用新的属性和效果
            status->ActiveAttributes[setId] = combinedAttributes;
            ApplySetEffects(player, setId, combinedAttributes, true);

            std::string setName = GetSetName(setId);
            if (sItemSetsConfig->IsDebugMode())
            {
                LOG_INFO("module", ModuleOldFormat("物品套装: 应用套装效果 - 套装:%s 件数:%s 属性:%s", setName, equippedCount, combinedAttributes).c_str());
            }
            if (sItemSetsConfig->ShowBonusNotification())
            {
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("套装效果激活: %s (%s件)", setName.c_str(), equippedCount).c_str());
            }
        }
    }

    // 移除不再装备的套装效果
    auto it = status->ActiveAttributes.begin();
    while (it != status->ActiveAttributes.end())
    {
        if (setIdCounts.find(it->first) == setIdCounts.end())
        {
            // 这个套装不再装备，移除效果
            ApplySetEffects(player, it->first, it->second, false);
            it = status->ActiveAttributes.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 套装效果计算完成，共激活 %s 个套装", setIdCounts.size()).c_str());
    }

    // 统一更新玩家属性
    UpdatePlayerStats(player);
}

void ItemSetsManager::ClearPlayerSetStatus(uint32 playerGuid)
{
    // 清理内存缓存（不删除数据库记录，用于登出时释放内存）
    ClearPlayerMemoryCache(playerGuid);

    // 从数据库删除（仅用于角色删除场景）
    CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `_物品套装_数据` WHERE `物品GUID` IN"
        "(SELECT guid FROM item_instance WHERE owner_guid = %s)", playerGuid).c_str());

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 清理玩家GUID:%s 的所有套装数据（角色删除，含数据库）", playerGuid).c_str());
    }
}

// 【内存泄漏修复】只清理内存缓存，不删除数据库记录（用于玩家登出）
void ItemSetsManager::ClearPlayerMemoryCache(uint32 playerGuid)
{
    auto it = _playerSetStatus.find(playerGuid);
    if (it != _playerSetStatus.end())
    {
        _playerSetStatus.erase(it);
    }

    // 清理装备状态缓存
    _lastSentEquipState.erase(playerGuid);

    // 清理装备缓存 (增量更新优化)
    _playerEquipmentCache.erase(playerGuid);

    // 清理技能增强效果
    _playerSpellEnhancements.erase(playerGuid);

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 清理玩家GUID:%s 的内存缓存", playerGuid).c_str());
    }
}

void ItemSetsManager::ClearAllSetEffects(Player* player, bool skipUpdate)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerSetStatus* status = GetPlayerSetStatus(playerGuid);
    if (!status)
        return;

    // 防止重复清除：检查是否已经在处理中
    static std::set<uint32> processingPlayers;

    if (processingPlayers.find(playerGuid) != processingPlayers.end())
    {
        // 正在处理中，避免重复调用
        return;
    }

    // 标记为正在处理
    processingPlayers.insert(playerGuid);

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 开始清除玩家 %s 的所有套装效果", player->GetName()).c_str());
    }

    // 清除所有已激活的套装效果（使用跟踪的精确属性值）
    for (const auto& pair : status->ActiveAttributes)
    {
        uint32 setId = pair.first;
        const std::string& attributes = pair.second;

        if (sItemSetsConfig->IsDebugMode())
        {
            LOG_INFO("module", ModuleOldFormat("物品套装: 清除套装 %s 的属性: %s", setId, attributes).c_str());
        }

        // 关键修复：只移除套装增强的属性，不影响基础属性
        if (!attributes.empty())
        {
            ApplyStatEffects(player, attributes, false);
        }

        // 额外清除可能的光环效果 - 使用更强力的清除方法
        const auto& setDataList = GetItemSetData(setId);
        for (const auto& setData : setDataList)
        {
            if (setData.EffectId > 0)
            {
                // 强制移除所有相关光环，包括debuff
                player->RemoveAurasDueToSpell(setData.EffectId);

                // 额外检查：如果还有残留光环，强制移除
                if (player->HasAura(setData.EffectId))
                {
                    player->RemoveAura(setData.EffectId);
                }

                if (sItemSetsConfig->IsDebugMode())
                {
                    LOG_INFO("module", ModuleOldFormat("物品套装: 强制移除光环效果 %s", setData.EffectId).c_str());
                }
            }
        }
    }

    // 清空激活属性记录
    status->ActiveAttributes.clear();

    // 额外安全措施：清除所有技能增强效果
    ClearAllSpellEnhancements(player);

    // 【性能优化】登录期间跳过属性更新，由 OnLogin 最后统一刷新
    if (!skipUpdate)
    {
        UpdatePlayerStats(player);
    }

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 完成清除玩家 %s 的所有套装效果 (skipUpdate=%s)", player->GetName(), skipUpdate ? 1 : 0).c_str());
    }

    // 移除处理标记
    processingPlayers.erase(playerGuid);
}

void ItemSetsManager::CheckAndUpdatePlayerSets(Player* player)
{
    // 简化实现，直接调用RefreshPlayerSetEffects
    RefreshPlayerSetEffects(player);
}

void ItemSetsManager::SendItemSetDataToClient(Player* player)
{
    // 简化实现，暂时不发送客户端数据
    if (!player)
        return;
}

void ItemSetsManager::AddSpellEnhancement(Player* player, uint32 spellId, float enhancePercent)
{
    if (!player || spellId <= 0)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 检查是否已经存在相同的增强效果
    auto& enhancementList = _playerSpellEnhancements[playerGuid];
    for (auto& enhancement : enhancementList)
    {
        if (enhancement.spellId == spellId)
        {
            // 更新增强数值
            enhancement.enhancePercent = enhancePercent;
            return;
        }
    }

    // 添加新的增强效果
    SpellEnhancement newEnhancement;
    newEnhancement.spellId = spellId;
    newEnhancement.enhancePercent = enhancePercent;

    enhancementList.push_back(newEnhancement);

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 为玩家 %s 添加技能增强 - 技能:%s 增强:%s%%", player->GetName(), spellId, enhancePercent).c_str());
    }
}

void ItemSetsManager::RemoveSpellEnhancement(Player* player, uint32 spellId)
{
    if (!player || spellId <= 0)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    auto it = _playerSpellEnhancements.find(playerGuid);
    if (it == _playerSpellEnhancements.end())
        return;

    // 移除指定的增强效果
    auto& enhancementList = it->second;
    enhancementList.erase(
        std::remove_if(enhancementList.begin(), enhancementList.end(),
            [spellId](const SpellEnhancement& enhancement) { return enhancement.spellId == spellId; }),
        enhancementList.end()
    );

    // 如果列表为空，移除整个条目
    if (enhancementList.empty())
    {
        _playerSpellEnhancements.erase(it);
    }

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 为玩家 %s 移除技能增强 - 技能:%s", player->GetName(), spellId).c_str());
    }
}

void ItemSetsManager::ClearAllSpellEnhancements(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    _playerSpellEnhancements.erase(playerGuid);

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 清除玩家 %s 的所有技能增强", player->GetName()).c_str());
    }
}

std::vector<SpellEnhancement> ItemSetsManager::GetPlayerSpellEnhancements(uint32 playerGuid) const
{
    auto it = _playerSpellEnhancements.find(playerGuid);
    if (it != _playerSpellEnhancements.end())
    {
        return it->second;
    }
    return std::vector<SpellEnhancement>();
}

void ItemSetsManager::ApplySetEffects(Player* player, uint32 setId, const std::string& attributes, bool apply)
{
    if (!player)
        return;

    // 获取套装数据
    const auto& setDataList = GetItemSetData(setId);

    // 应用属性效果
    if (!attributes.empty())
    {
        ApplyStatEffects(player, attributes, apply);
    }

    // 【修复高风险问题】EffectId 需要按件数门槛生效
    // 先获取当前玩家装备的该套装件数
    uint32 playerGuid = player->GetGUID().GetCounter();
    uint8 currentCount = 0;

    // 从装备缓存获取件数
    auto cacheIt = _playerEquipmentCache.find(playerGuid);
    if (cacheIt != _playerEquipmentCache.end())
    {
        auto countIt = cacheIt->second.setIdToCount.find(setId);
        if (countIt != cacheIt->second.setIdToCount.end())
        {
            currentCount = countIt->second;
        }
    }

    // 如果缓存没有，从PlayerSetStatus计算
    if (currentCount == 0)
    {
        PlayerSetStatus* status = GetPlayerSetStatus(playerGuid);
        if (status)
        {
            for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
            {
                Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                if (item)
                {
                    uint32 itemGuid = item->GetGUID().GetCounter();
                    auto it = status->ItemSetMap.find(itemGuid);
                    if (it != status->ItemSetMap.end() && it->second == setId)
                    {
                        currentCount++;
                    }
                }
            }
        }
    }

    // 应用EffectId效果（光环或技能增强）- 只应用达到门槛的效果
    for (const auto& setData : setDataList)
    {
        if (setData.EffectId > 0)
        {
            // 【关键修复】检查件数门槛
            if (apply)
            {
                // 应用时：只有达到门槛才应用
                if (currentCount >= setData.ItemsCount)
                {
                    ApplyEnhancedEffect(player, setData.EffectId, setData.EffectValue, true);
                    if (sItemSetsConfig->IsDebugMode())
                    {
                        LOG_INFO("module", ModuleOldFormat("物品套装: 应用效果ID:%s (需要%s件，当前%s件)", setData.EffectId, setData.ItemsCount, currentCount).c_str());
                    }
                }
            }
            else
            {
                // 移除时：只移除已经应用的效果（门槛检查在应用时已完成）
                // 但为了安全，我们检查当前件数，如果低于门槛才移除
                // 注意：这里的currentCount是移除后的件数，所以直接移除
                ApplyEnhancedEffect(player, setData.EffectId, setData.EffectValue, false);
                if (sItemSetsConfig->IsDebugMode())
                {
                    LOG_INFO("module", ModuleOldFormat("物品套装: 移除效果ID:%s", setData.EffectId).c_str());
                }
            }
        }
    }

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: %s 套装效果 - 套装ID:%s 属性:%s 当前件数:%s", apply ? "应用" : "移除", setId, attributes, currentCount).c_str());
    }
}

void ItemSetsManager::ApplyStatEffects(Player* player, const std::string& attributes, bool apply)
{
    if (!player || attributes.empty())
        return;

    for (auto const& attr : ParseAttributes(attributes))
    {
        uint32 const attributeType = attr.first;
        int64 const& value = attr.second;
        if (value <= 0)
            continue;

        if (attributeType == 47)
        {
            if (sItemSetsConfig->IsDebugMode())
                LOG_WARN("module", ModuleOldFormat("物品套装: 精通等级在WOTLK中不支持 (属性类型 %s)", attributeType).c_str());
            continue;
        }

        // 套装表保留了旧版自定义编号，不能直接把所有编号转换为 ItemModType。
        ItemModType mod;
        switch (attributeType)
        {
            case 0: case 1: case 3: case 4: case 5: case 6: case 7:
            case 12: case 13: case 14: case 15: case 16: case 17: case 18:
            case 19: case 20: case 21: case 22: case 23: case 24: case 25:
            case 26: case 27: case 28: case 29: case 30: case 31: case 32:
            case 33: case 34: case 35: case 36: case 37: case 38: case 39:
                mod = ItemModType(attributeType);
                break;
            case 40: case 41: case 42: case 45:
                mod = ITEM_MOD_SPELL_POWER;
                break;
            case 43:
                mod = ITEM_MOD_SPELL_PENETRATION;
                break;
            case 44:
                mod = ITEM_MOD_HEALTH_REGEN;
                break;
            case 46:
                mod = ITEM_MOD_ARMOR_PENETRATION_RATING;
                break;
            default:
                if (sItemSetsConfig->IsDebugMode())
                    LOG_WARN("module", ModuleOldFormat("物品套装: 未知的属性类型 %s", attributeType).c_str());
                continue;
        }
        player->DoApplyStat(mod, value, apply);

        if (sItemSetsConfig->IsDebugMode())
            LOG_INFO("module", ModuleOldFormat("物品套装: %s 属性 %s 数值 %s", apply ? "应用" : "移除", attributeType, Acore::ToString(value)).c_str());
    }

    UpdatePlayerStats(player);
}


void ItemSetsManager::UpdatePlayerStats(Player* player)
{
    if (!player)
        return;

    player->UpdateAllStats();
    player->UpdateAttackPowerAndDamage();
    player->UpdateAttackPowerAndDamage(true);
    player->UpdateMaxHealth();
    player->UpdateMaxPower(POWER_MANA);
}

void ItemSetsManager::ApplyEnhancedEffect(Player* player, uint32 effectId, float effectValue, bool apply)
{
    if (!player || effectId <= 0)
        return;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(effectId);
    if (!spellInfo)
    {
        if (sItemSetsConfig->IsDebugMode())
        {
            LOG_WARN("module", ModuleOldFormat("物品套装: 无效的效果ID %s", effectId).c_str());
        }
        return;
    }

    // 检查这个技能是否应该作为技能增强而不是光环
    bool shouldBeEnhancement = false;

    // 使用AzerothCore内置方法进行智能判断
    bool isPassive = spellInfo->IsPassive();
    bool hasAnyAura = spellInfo->HasAnyAura();
    bool isPositive = spellInfo->IsPositive();
    bool isSelfCast = spellInfo->IsSelfCast();

    // 判断逻辑：
    // 1. 被动技能且只有光环效果 -> 光环
    // 2. 正面技能且自我施放且有光环效果 -> 光环
    // 3. 其他情况 -> 技能增强

    if (isPassive && hasAnyAura && isSelfCast)
    {
        // 被动光环技能，如属性增强光环
        shouldBeEnhancement = false;
    }
    else if (isPositive && isSelfCast && hasAnyAura && !spellInfo->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE))
    {
        // 正面自我buff光环，如增益光环
        shouldBeEnhancement = false;
    }
    else
    {
        // 攻击技能、负面技能、或其他主动技能
        shouldBeEnhancement = true;
    }

    if (shouldBeEnhancement)
    {
        // 作为技能增强处理
        if (apply)
        {
            AddSpellEnhancement(player, effectId, effectValue);
        }
        else
        {
            RemoveSpellEnhancement(player, effectId);
        }

        if (sItemSetsConfig->IsDebugMode())
        {
            LOG_INFO("module", ModuleOldFormat("物品套装: %s 技能增强 - 技能:%s 增强:%s%%", apply ? "添加" : "移除", effectId, effectValue).c_str());
        }
    }
    else
    {
        // 作为光环处理
        if (apply)
        {
            // 先移除可能存在的旧光环
            player->RemoveAurasDueToSpell(effectId);

            // 添加光环
            if (Aura* aura = player->AddAura(effectId, player))
            {
                // 设置为永久光环
                aura->SetDuration(-1);
                aura->SetMaxDuration(-1);

                if (sItemSetsConfig->IsDebugMode())
                {
                    LOG_INFO("module", ModuleOldFormat("物品套装: 添加永久光环 - 技能:%s (%s)", effectId, spellInfo->SpellName[0]).c_str());
                }
            }
            else if (sItemSetsConfig->IsDebugMode())
            {
                LOG_WARN("module", ModuleOldFormat("物品套装: 添加光环失败 - 技能:%s", effectId).c_str());
            }
        }
        else
        {
            // 移除光环
            player->RemoveAurasDueToSpell(effectId);

            if (sItemSetsConfig->IsDebugMode())
            {
                LOG_INFO("module", ModuleOldFormat("物品套装: 移除光环 - 技能:%s (%s)", effectId, spellInfo->SpellName[0]).c_str());
            }
        }
    }
}

std::vector<std::pair<uint32, int64>> ItemSetsManager::ParseAttributes(const std::string& attributeStr) const
{
    std::vector<std::pair<uint32, int64>> attributes;

    if (attributeStr.empty())
        return attributes;

    std::istringstream iss(attributeStr);
    std::string token;

    while (std::getline(iss, token, ','))
    {
        std::istringstream tokenStream(token);
        uint32 type;
        std::string valueText;

        if (tokenStream >> type >> valueText)
        {
            Optional<int64> value = Acore::StringTo<int64>(valueText);
            if (value)
                attributes.push_back(std::make_pair(type, *value));
        }
    }

    return attributes;
}

std::string ItemSetsManager::GetAttributeName(uint32 attributeType) const
{
    switch (attributeType)
    {
        case 0: return "法力值";
        case 1: return "生命值";
        case 3: return "敏捷";
        case 4: return "力量";
        case 5: return "智力";
        case 6: return "精神";
        case 7: return "耐力";
        case 12: return "防御等级";
        case 13: return "躲闪等级";
        case 14: return "招架等级";
        case 15: return "格挡等级";
        case 16: return "近战命中等级";
        case 17: return "远程命中等级";
        case 18: return "法术命中等级";
        case 19: return "近战暴击等级";
        case 20: return "远程暴击等级";
        case 21: return "法术暴击等级";
        case 22: return "近战被命中等级";
        case 23: return "远程被命中等级";
        case 24: return "法术被命中等级";
        case 25: return "近战被暴击等级";
        case 26: return "远程被暴击等级";
        case 27: return "法术被暴击等级";
        case 28: return "近战急速等级";
        case 29: return "远程急速等级";
        case 30: return "法术急速等级";
        case 31: return "命中等级";
        case 32: return "暴击等级";
        case 33: return "被命中等级";
        case 34: return "被暴击等级";
        case 35: return "韧性等级";
        case 36: return "急速等级";
        case 37: return "精准等级";
        case 38: return "攻击强度";
        case 39: return "远程攻击强度";
        case 40: return "法术治疗";
        case 41: return "法术伤害";
        case 42: return "法术治疗和伤害";
        case 43: return "法术穿透";
        case 44: return "生命值回复";
        case 45: return "法术强度";
        case 46: return "护甲穿透等级";
        case 47: return "精通等级";
        default: return "未知属性";
    }
}

bool ItemSetsManager::DeactivateItemSet(Player* player, uint32 setId)
{
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerSetStatus* status = GetPlayerSetStatus(playerGuid);
    if (!status)
        return false;

    // 查找并移除该套装的激活效果
    auto it = status->ActiveAttributes.find(setId);
    if (it != status->ActiveAttributes.end())
    {
        ApplySetEffects(player, setId, it->second, false);
        status->ActiveAttributes.erase(it);

        // 停用单个套装后更新玩家属性
        UpdatePlayerStats(player);

        if (sItemSetsConfig->IsDebugMode())
        {
            LOG_INFO("module", ModuleOldFormat("物品套装: 停用套装 %s 的效果", setId).c_str());
        }

        return true;
    }

    return false;
}

//=============================================================================
// ItemSetsCommands 实现
//=============================================================================

bool ItemSetsCommands::HandleItemSetsList(ChatHandler* handler, const char* args)
{
    if (!sItemSetsConfig->IsModuleEnabled())
    {
        handler->SendSysMessage("物品套装已禁用。");
        return true;
    }

    std::vector<uint32> setIds = sItemSetsManager->GetAllSetIds();

    if (setIds.empty())
    {
        handler->SendSysMessage("没有找到套装数据。");
        return true;
    }

    handler->SendSysMessage("可用的套装列表:");

    for (uint32 setId : setIds)
    {
        std::string setName = sItemSetsManager->GetSetName(setId);
        const auto& setData = sItemSetsManager->GetItemSetData(setId);

        handler->PSendSysMessage("%s", ModuleOldFormat("ID: %s, 名称: %s, 效果数量: %s", setId, setName.c_str(), (uint32)setData.size()).c_str());
    }

    return true;
}

bool ItemSetsCommands::HandleItemSetsInfo(ChatHandler* handler, const char* args)
{
    if (!sItemSetsConfig->IsModuleEnabled())
    {
        handler->SendSysMessage("物品套装已禁用。");
        return true;
    }

    if (!*args)
    {
        handler->SendSysMessage("用法: .套装 信息 [套装ID]");
        return true;
    }

    // 【修复低风险问题】使用安全的参数解析，避免溢出
    char* endptr = nullptr;
    long long parsedValue = strtoll(args, &endptr, 10);

    // 检查解析是否成功以及值是否在有效范围内
    if (endptr == args || *endptr != '\0' || parsedValue < 0 || parsedValue > UINT32_MAX)
    {
        handler->SendSysMessage("无效的套装ID，请输入有效的数字。");
        return true;
    }

    uint32 setId = static_cast<uint32>(parsedValue);

    const auto& setData = sItemSetsManager->GetItemSetData(setId);
    if (setData.empty())
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("未找到ID为 %s 的套装。", setId).c_str());
        return true;
    }

    std::string setName = sItemSetsManager->GetSetName(setId);
    handler->PSendSysMessage("%s", ModuleOldFormat("套装信息 - ID: %s, 名称: %s", setId, setName.c_str()).c_str());

    // 显示套装效果
    handler->SendSysMessage("套装效果:");
    for (const auto& data : setData)
    {
        std::string effectTypeStr;
        switch (data.EffectType)
        {
            case ITEM_SET_EFFECT_STAT:
                effectTypeStr = "属性";
                break;
            case ITEM_SET_EFFECT_SKILL:
                effectTypeStr = "技能";
                break;
            case ITEM_SET_EFFECT_AURA:
                effectTypeStr = "光环";
                break;
            default:
                effectTypeStr = "未知";
                break;
        }

        handler->PSendSysMessage("%s", ModuleOldFormat("  %s件套: %s (类型: %s, ID: %s, 数值: %s)", data.ItemsCount, data.EffectDesc.c_str(), effectTypeStr.c_str(), data.EffectId, data.EffectValue).c_str());
    }

    return true;
}

bool ItemSetsCommands::HandleItemSetsPlayer(ChatHandler* handler, const char* args)
{
    if (!sItemSetsConfig->IsModuleEnabled())
    {
        handler->SendSysMessage("物品套装已禁用。");
        return true;
    }

    Player* target;

    if (!*args)
    {
        target = handler->getSelectedPlayer();
        if (!target)
        {
            handler->SendSysMessage("请选择一个玩家或提供玩家名称。");
            return true;
        }
    }
    else
    {
        std::string playerName = args;
        target = ObjectAccessor::FindPlayerByName(playerName);
        if (!target)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("未找到玩家: %s", playerName.c_str()).c_str());
            return true;
        }
    }

    handler->PSendSysMessage("%s", ModuleOldFormat("玩家 %s 的套装状态:", target->GetName().c_str()).c_str());

    // 查询玩家的套装状态
    uint32 playerGuid = target->GetGUID().GetCounter();
    PlayerSetStatus* status = sItemSetsManager->GetPlayerSetStatus(playerGuid);

    if (status)
    {
        // 统计套装分配情况
        std::map<uint32, uint32> setIdCounts;
        for (const auto& pair : status->ItemSetMap)
        {
            setIdCounts[pair.second]++;
        }

        if (!setIdCounts.empty())
        {
            handler->SendSysMessage("  套装分配情况:");
            for (const auto& pair : setIdCounts)
            {
                std::string setName = sItemSetsManager->GetSetName(pair.first);
                handler->PSendSysMessage("%s", ModuleOldFormat("    套装%s: %s (%s件)", pair.first, setName.c_str(), pair.second).c_str());
            }
        }
        else
        {
            handler->SendSysMessage("该玩家没有分配任何套装。");
        }
    }
    else
    {
        handler->SendSysMessage("该玩家没有套装状态记录。");
    }

    return true;
}

bool ItemSetsCommands::HandleItemSetsAssign(ChatHandler* handler, const char* args)
{
    if (!sItemSetsConfig->IsModuleEnabled())
    {
        handler->SendSysMessage("物品套装已禁用。");
        return true;
    }

    Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!player)
    {
        handler->SendSysMessage("该命令只能由玩家使用。");
        return true;
    }

    if (!*args)
    {
        handler->SendSysMessage("用法: .物品套装 分配 [物品ID]");
        handler->SendSysMessage("示例: .物品套装 分配 12345");
        handler->SendSysMessage("|cFFFFFF00注意: 物品可以在装备栏或背包中，无需装备|r");
        return true;
    }

    // 【修复低风险问题】使用安全的参数解析，避免溢出
    char* endptr = nullptr;
    long long parsedValue = strtoll(args, &endptr, 10);

    // 检查解析是否成功以及值是否在有效范围内
    if (endptr == args || *endptr != '\0' || parsedValue <= 0 || parsedValue > UINT32_MAX)
    {
        handler->SendSysMessage("请输入有效的物品ID。");
        return true;
    }

    uint32 itemId = static_cast<uint32>(parsedValue);

    // 智能分配：不再要求物品必须装备，AssignRandomSetToItem函数会自动查找物品
    bool success = sItemSetsManager->AssignRandomSetToItem(player, itemId, 0); // slot参数已不再使用
    if (!success)
    {
        handler->SendSysMessage("分配套装失败。");
    }

    return true;
}

bool ItemSetsCommands::HandleItemSetsClear(ChatHandler* handler, const char* args)
{
    if (!sItemSetsConfig->IsModuleEnabled())
    {
        handler->SendSysMessage("物品套装已禁用。");
        return true;
    }

    Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!player)
    {
        handler->SendSysMessage("该命令只能由玩家使用。");
        return true;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 【经济漏洞修复 二-A9】先按跟踪的精确属性值回滚身上已应用的套装效果，再删记录——
    // 原实现只删记录不回滚属性，清除→重新分配可无限叠属性
    sItemSetsManager->ClearAllSetEffects(player, false);

    // 清除玩家的套装状态（使用子查询关联item_instance表）
    CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `_物品套装_数据` WHERE `物品GUID` IN"
        "(SELECT guid FROM item_instance WHERE owner_guid = %s)", playerGuid).c_str());

    // 清除内存中的状态
    sItemSetsManager->ClearPlayerSetStatus(playerGuid);

    handler->SendSysMessage("已清除所有套装分配。");

    return true;
}

bool ItemSetsCommands::HandleItemSetsReload(ChatHandler* handler, const char* args)
{
    sItemSetsManager->LoadItemSets();
    // 重新加载套装数据
    handler->SendSysMessage("套装数据已重新加载。");
    return true;
}

bool ItemSetsCommands::HandleItemSetsCheck(ChatHandler* handler, const char* args)
{
    if (!sItemSetsConfig->IsModuleEnabled())
    {
        handler->SendSysMessage("物品套装已禁用。");
        return true;
    }

    Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!player)
    {
        handler->SendSysMessage("该命令只能由玩家使用。");
        return true;
    }

    // 检查是否有参数 - 支持查询特定物品
    if (args && *args)
    {
        return HandleSpecificItemCheck(handler, args, player);
    }

    handler->PSendSysMessage("%s", ModuleOldFormat("正在检查 %s 的套装状态...", player->GetName().c_str()).c_str());
    handler->SendSysMessage(" ");

    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerSetStatus* status = sItemSetsManager->GetPlayerSetStatus(playerGuid);

    if (!status)
    {
        handler->SendSysMessage("未找到套装状态。");
        return true;
    }

    bool foundSetItems = false;
    std::map<uint32, uint32> groupCounts; // 组ID -> 件数
    std::map<uint32, std::string> groupNames; // 组ID -> 套装名称

    // 检查各部位的套装分配
    const char* slotNames[] = {"头盔", "项链", "肩膀", "衬衣", "上衣", "腰带", "裤子", "鞋子", "护腕", "手套", "戒指1", "戒指2", "饰品1", "饰品2", "披风", "主手武器", "副手武器", "远程武器", "战袍"};

    // 检查当前装备的套装物品
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item)
        {
            uint32 itemGuid = item->GetGUID().GetCounter();
            auto it = status->ItemSetMap.find(itemGuid);
            if (it != status->ItemSetMap.end())
            {
                foundSetItems = true;
                uint32 setId = it->second;
                uint32 itemId = item->GetEntry();

                groupCounts[setId]++;
                if (groupNames.find(setId) == groupNames.end())
                {
                    groupNames[setId] = sItemSetsManager->GetSetName(setId);
                }

                // 确保槽位名称索引正确
                uint8 slotIndex = i - EQUIPMENT_SLOT_START;
                const char* slotName = (slotIndex < EQUIPMENT_SLOT_END) ? slotNames[slotIndex] : "未知";
                handler->PSendSysMessage("%s", ModuleOldFormat("  %s: 套装 %s (ID:%s) |cFF00FF00已装备|r [物品:%s/%s]", slotName, groupNames[setId].c_str(), setId, itemId, itemGuid).c_str());
            }
        }
    }

    if (!foundSetItems)
    {
        handler->SendSysMessage("未分配任何套装。");
        handler->SendSysMessage("|cFFFFFF00提示: 使用 .物品套装 分配 [物品ID] 来分配套装|r");
        handler->SendSysMessage("|cFFFFFF00物品可以在装备栏或背包中，无需装备|r");
        return true;
    }

    handler->SendSysMessage(" ");
    handler->SendSysMessage("|cFFFFD700套装统计:|r");

    for (const auto& pair : groupCounts)
    {
        uint32 setId = pair.first;
        uint32 count = pair.second;
        std::string setName = groupNames[setId];

        // 简化显示，不再区分激活状态
        handler->PSendSysMessage("%s", ModuleOldFormat("  |cFF00FF00套装%s: %s (%s件)|r", setId, setName.c_str(), count).c_str());
    }

    return true;
}

bool ItemSetsCommands::HandleItemSetsRefresh(ChatHandler* handler, const char* args)
{
    if (!sItemSetsConfig->IsModuleEnabled())
    {
        handler->SendSysMessage("物品套装已禁用。");
        return true;
    }

    Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
    if (!player)
    {
        handler->SendSysMessage("该命令只能由玩家使用。");
        return true;
    }

    // 刷新玩家的套装效果
    sItemSetsManager->RefreshPlayerSetEffects(player);
    handler->SendSysMessage("套装效果已刷新。");

    return true;
}

// 处理特定物品的套装检查
bool ItemSetsCommands::HandleSpecificItemCheck(ChatHandler* handler, const char* args, Player* player)
{
    if (!player)
        return false;

    // 解析参数：物品ID 物品GUID
    std::string argsStr(args);
    std::istringstream iss(argsStr);
    uint32 itemId = 0;
    uint32 itemGuid = 0;

    if (!(iss >> itemId >> itemGuid))
    {
        // 移除用法和示例的显示
        return true;
    }

    // 【修复中风险问题】添加物品归属校验
    // 首先检查物品是否属于当前玩家
    uint32 playerGuid = player->GetGUID().GetCounter();

    // 查找指定的物品（在装备栏和背包中搜索）
    Item* targetItem = FindItemByIdAndGuid(player, itemId, itemGuid);

    if (!targetItem)
    {
        // 如果在当前玩家找不到物品，需要验证物品归属权
        // 【安全修复】查询物品所有者，验证是否为当前玩家
        QueryResult ownerResult = CharacterDatabase.Query(ModuleOldFormat("SELECT owner_guid FROM item_instance WHERE guid = %s", itemGuid).c_str());

        if (!ownerResult)
        {
            // 物品不存在
            return true;
        }

        uint32 ownerGuid = ownerResult->Fetch()[0].Get<uint32>();

        // 【安全校验】只允许查询自己的物品，除非是GM
        if (ownerGuid != playerGuid)
        {
            // 检查是否有GM权限
            if (!handler->GetSession() || handler->GetSession()->GetSecurity() < SEC_GAMEMASTER)
            {
                // 普通玩家不允许查询他人物品，静默返回
                if (sItemSetsConfig->IsDebugMode())
                {
                    LOG_WARN("module", ModuleOldFormat("物品套装: 玩家 %s 尝试查询不属于自己的物品 GUID:%s", player->GetName(), itemGuid).c_str());
                }
                return true;
            }
        }

        // 通过归属校验后，从数据库查询套装信息
        QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT `套装ID` FROM `_物品套装_数据` WHERE `物品GUID` = %s", itemGuid).c_str());

        if (!result)
        {
            // 没有套装数据，直接返回（不显示错误消息）
            return true;
        }

        Field* fields = result->Fetch();
        uint32 setId = fields[0].Get<uint32>();

        if (setId == 0)
        {
            // 没有套装分配，直接返回
            return true;
        }

        std::string setName = sItemSetsManager->GetSetName(setId);
        const auto& setDataList = sItemSetsManager->GetItemSetData(setId);

        // 构建属性字符串
        std::string attributes = "";
        std::string effectDesc = "";

        if (!setDataList.empty())
        {
            attributes = setDataList[0].SetAttributes;

            // 收集所有效果描述
            for (const auto& setData : setDataList)
            {
                if (!setData.EffectDesc.empty())
                {
                    if (!effectDesc.empty())
                        effectDesc += "|";
                    effectDesc += std::to_string(setData.ItemsCount) + "件套:" + setData.EffectDesc;
                }
            }
        }

        // 【修复低风险问题】对字符串字段中的冒号进行转义，避免解析错误
        auto escapeColons = [](const std::string& input) -> std::string {
            std::string result;
            result.reserve(input.size() * 2);
            for (char c : input)
            {
                if (c == ':')
                    result += "\\:";
                else if (c == '\\')
                    result += "\\\\";
                else
                    result.push_back(c);
            }
            return result;
        };

        // 发送套装数据给客户端
        std::string itemSetMsg = "SPECIFIC_ITEM_DATA:" + std::to_string(itemId) + ":" + std::to_string(itemGuid) + ":" +
                                 std::to_string(setId) + ":" + escapeColons(setName) + ":" + escapeColons(attributes) + ":" + escapeColons(effectDesc);

        std::string hiddenMsg = "ITEMSETS_HIDDEN:" + itemSetMsg;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, LANG_UNIVERSAL, nullptr, nullptr, hiddenMsg);
        player->GetSession()->SendPacket(&data);

        return true;
    }

    PlayerSetStatus* status = sItemSetsManager->GetPlayerSetStatus(playerGuid);

    if (!status)
    {
        // 尝试加载`_物品套装_数据`
        sItemSetsManager->LoadPlayerSetStatus(player);
        status = sItemSetsManager->GetPlayerSetStatus(playerGuid);
        if (!status)
        {
            handler->SendSysMessage("未找到套装状态。");
            return true;
        }
    }

    // 检查该物品是否有套装分配
    auto it = status->ItemSetMap.find(itemGuid);
    if (it == status->ItemSetMap.end())
    {
        // 如果物品没有分配套装，不发送任何消息，直接返回
        return true;
    }

    uint32 setId = it->second;
    std::string setName = sItemSetsManager->GetSetName(setId);

    // 获取套装详细信息
    const auto& setDataList = sItemSetsManager->GetItemSetData(setId);

    // 发送客户端数据
    SendSpecificItemDataToClient(player, targetItem, setId, setName, setDataList);

    return true;
}

// 查找指定ID和GUID的物品
Item* ItemSetsCommands::FindItemByIdAndGuid(Player* player, uint32 itemId, uint32 itemGuid)
{
    if (!player)
        return nullptr;

    // 首先在装备栏中查找
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetEntry() == itemId && item->GetGUID().GetCounter() == itemGuid)
        {
            return item;
        }
    }

    // 在主背包中查找
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetEntry() == itemId && item->GetGUID().GetCounter() == itemGuid)
        {
            return item;
        }
    }

    // 在其他背包中查找
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* bag = player->GetBagByPos(i);
        if (bag)
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                if (item && item->GetEntry() == itemId && item->GetGUID().GetCounter() == itemGuid)
                {
                    return item;
                }
            }
        }
    }

    return nullptr;
}

// 发送特定物品的套装数据给客户端
void ItemSetsCommands::SendSpecificItemDataToClient(Player* player, Item* item, uint32 setId, const std::string& setName, const std::vector<ItemSetData>& setDataList)
{
    if (!player || !item)
        return;

    uint32 itemId = item->GetEntry();
    uint32 itemGuid = item->GetGUID().GetCounter();

    // 构建属性字符串
    std::string attributes = "";
    std::string effectDesc = "";

    if (!setDataList.empty())
    {
        attributes = setDataList[0].SetAttributes;

        // 收集所有效果描述
        for (const auto& setData : setDataList)
        {
            if (!setData.EffectDesc.empty())
            {
                if (!effectDesc.empty())
                    effectDesc += "|";
                effectDesc += std::to_string(setData.ItemsCount) + "件套:" + setData.EffectDesc;
            }
        }
    }

    // 【修复低风险问题】对字符串字段中的冒号进行转义，避免解析错误
    auto escapeColons = [](const std::string& input) -> std::string {
        std::string result;
        result.reserve(input.size() * 2);
        for (char c : input)
        {
            if (c == ':')
                result += "\\:";
            else if (c == '\\')
                result += "\\\\";
            else
                result.push_back(c);
        }
        return result;
    };

    // 发送套装数据给客户端 - 使用系统消息但添加特殊前缀
    std::string itemSetMsg = "SPECIFIC_ITEM_DATA:" + std::to_string(itemId) + ":" + std::to_string(itemGuid) + ":" +
                             std::to_string(setId) + ":" + escapeColons(setName) + ":" + escapeColons(attributes) + ":" + escapeColons(effectDesc);

    // 添加特殊前缀标识这是插件消息
    std::string hiddenMsg = "ITEMSETS_HIDDEN:" + itemSetMsg;
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, LANG_UNIVERSAL, nullptr, nullptr, hiddenMsg);
    player->GetSession()->SendPacket(&data);
}

//=============================================================================
// ItemSetsCommandScript 实现
//=============================================================================

ItemSetsCommandScript::ItemSetsCommandScript() : CommandScript("ItemSetsCommandScript")
{
}

std::vector<Acore::ChatCommands::ChatCommandBuilder> ItemSetsCommandScript::GetCommands() const
{
    static ChatCommandTable itemSetsCommandTable =
    {
        { "列表",       ItemSetsCommands::HandleItemSetsList,      AccountTypes::SEC_PLAYER,         Console::No },
        { "信息",       ItemSetsCommands::HandleItemSetsInfo,      AccountTypes::SEC_PLAYER,         Console::No },
        { "玩家",       ItemSetsCommands::HandleItemSetsPlayer,    AccountTypes::SEC_PLAYER,         Console::No },
        { "分配",       ItemSetsCommands::HandleItemSetsAssign,    AccountTypes::SEC_PLAYER,         Console::No },
        { "清除",       ItemSetsCommands::HandleItemSetsClear,     AccountTypes::SEC_PLAYER,         Console::No },
        { "检查",       ItemSetsCommands::HandleItemSetsCheck,     AccountTypes::SEC_PLAYER,         Console::No },
        { "刷新",       ItemSetsCommands::HandleItemSetsRefresh,   AccountTypes::SEC_PLAYER,         Console::No },
        { "重载",       ItemSetsCommands::HandleItemSetsReload,    AccountTypes::SEC_ADMINISTRATOR,  Console::Yes }
    };

    static ChatCommandTable commandTable =
    {
        { "套装",       itemSetsCommandTable },
        { "物品套装",   itemSetsCommandTable }
    };

    return commandTable;
}

//=============================================================================
// ItemSetsItemScript 实现
//=============================================================================

ItemSetsItemScript::ItemSetsItemScript() : ItemScript("ItemSetsItemScript")
{
}

bool ItemSetsItemScript::OnTooltipDisplayed(Player* player, Item* item, ItemTemplate const* proto, uint32& count)
{
    // 注意：OnTooltipDisplayed 钩子在某些版本中可能不存在或不工作
    // 这里提供一个替代方案，通过其他方式显示套装信息
    return false;
}

bool ItemSetsItemScript::OnUse(Player* player, Item* item, SpellCastTargets const& targets)
{
    if (!sItemSetsConfig->IsModuleEnabled() || !player || !item)
        return false;

    // 【修复中风险问题】使用物品GUID而不是itemId调用GetItemSetId
    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 setId = sItemSetsManager->GetItemSetId(player, itemGuid);

    if (setId > 0)
    {
        // 显示套装信息
        ShowItemSetInfo(player, setId);
        return true; // 阻止物品的正常使用
    }

    return false;
}

void ItemSetsItemScript::ShowItemSetInfo(Player* player, uint32 setId)
{
    if (!player)
        return;

    std::string setName = sItemSetsManager->GetSetName(setId);
    const auto& setData = sItemSetsManager->GetItemSetData(setId);

    if (setData.empty())
        return;

    // 检查玩家当前装备的套装状态
    uint32 playerGuid = player->GetGUID().GetCounter();
    PlayerSetStatus* status = sItemSetsManager->GetPlayerSetStatus(playerGuid);

    // 计算当前套装的激活件数
    uint8 activeCount = 0;
    if (status)
    {
        // 遍历当前装备，统计该套装的件数
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (item)
            {
                uint32 itemGuid = item->GetGUID().GetCounter();
                auto it = status->ItemSetMap.find(itemGuid);
                if (it != status->ItemSetMap.end() && it->second == setId)
                {
                    activeCount++;
                }
            }
        }
    }

    // 显示套装基本信息
    ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("|cFFFFD700=== 套装信息: %s ===|r", setName.c_str()).c_str());
    ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("|cFF00FF00当前激活: %s件|r", activeCount).c_str());
    ChatHandler(player->GetSession()).SendSysMessage(" ");

    // 显示套装效果
    ChatHandler(player->GetSession()).SendSysMessage("|cFFFFD700套装效果:|r");
    for (const auto& data : setData)
    {
        std::string color = activeCount >= data.ItemsCount ? "|cFF00FF00" : "|cFF808080";
        std::string statusText = activeCount >= data.ItemsCount ? " [已激活]" : "";

        ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("%s(%s) 件套: %s%s|r", color.c_str(), data.ItemsCount, data.EffectDesc.c_str(), statusText.c_str()).c_str());
    }

    ChatHandler(player->GetSession()).SendSysMessage(" ");

    // 显示收集进度
    ChatHandler(player->GetSession()).SendSysMessage("|cFFFFD700收集进度:|r");
    if (status && activeCount > 0)
    {
        // 显示当前装备的该套装物品
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (item)
            {
                uint32 itemGuid = item->GetGUID().GetCounter();
                auto it = status->ItemSetMap.find(itemGuid);
                if (it != status->ItemSetMap.end() && it->second == setId)
                {
                    std::string slotName = GetSlotName(i - EQUIPMENT_SLOT_START);
                    ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("|cFF00FF00%s: %s|r", slotName.c_str(), item->GetTemplate()->Name1.c_str()).c_str());
                }
            }
        }
    }
    else
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cFF808080尚未激活此套装|r");
    }

    ChatHandler(player->GetSession()).SendSysMessage(" ");
    ChatHandler(player->GetSession()).SendSysMessage("|cFFFFFF00使用命令 .物品套装 激活 来激活套装效果|r");
}

std::string ItemSetsItemScript::GetSlotName(uint8 slot)
{
    switch (slot)
    {
        case 0: return "头部";
        case 1: return "颈部";
        case 2: return "肩部";
        case 3: return "胸部";
        case 4: return "腰部";
        case 5: return "腿部";
        case 6: return "脚部";
        case 7: return "手腕";
        default: return "未知";
    }
}

//=============================================================================
// ItemSetsPlayerScript 实现
//=============================================================================

ItemSetsPlayerScript::ItemSetsPlayerScript() : PlayerScript("ItemSetsPlayerScript")
{
}

void ItemSetsPlayerScript::OnLogin(Player* player)
{
    auto loginStart = std::chrono::high_resolution_clock::now();

    if (!sItemSetsConfig->IsModuleEnabled())
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    if (_initializedPlayers.find(playerGuid) != _initializedPlayers.end())
        return;

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 玩家 %s 开始登录，加载套装数据", player->GetName()).c_str());
    }

    // 强制重新加载`_物品套装_数据`
    auto step1Start = std::chrono::high_resolution_clock::now();
    sItemSetsManager->LoadPlayerSetStatus(player);
    auto step1End = std::chrono::high_resolution_clock::now();
    auto step1Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step1End - step1Start).count();

    // 先清除所有套装效果，避免属性叠加
    // 【性能优化】登录期间传入 skipUpdate=true，避免重复调用 UpdateAllStats
    auto step2Start = std::chrono::high_resolution_clock::now();
    sItemSetsManager->ClearAllSetEffects(player, true);  // skipUpdate=true
    sItemSetsManager->ClearAllSpellEnhancements(player);
    auto step2End = std::chrono::high_resolution_clock::now();
    auto step2Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step2End - step2Start).count();

    // 额外安全措施：清除绝对无效的debuff光环
    auto step3Start = std::chrono::high_resolution_clock::now();
    CheckAndRemoveKnownInvalidDebuffs(player);
    auto step3End = std::chrono::high_resolution_clock::now();
    auto step3Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step3End - step3Start).count();

    // 设置1秒延迟后重新应用套装效果，确保清除操作完全完成
    _delayedRefreshTimers[playerGuid] = 1000; // 1秒延迟
    _initializedPlayers.insert(playerGuid);

    if (sItemSetsConfig->IsDebugMode())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("调试: 玩家登录，已清除套装效果，1秒后重新应用");
    }

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 玩家 %s 登录完成，套装数据已发送", player->GetName()).c_str());
    }

    auto loginEnd = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loginEnd - loginStart).count();
}

void ItemSetsPlayerScript::OnPlayerLoadFromDB(Player* player)
{
    if (!sItemSetsConfig->IsModuleEnabled())
        return;

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 玩家 %s 从数据库加载，处理套装数据", player->GetName()).c_str());
    }

    // 强制重新加载`_物品套装_数据`
    sItemSetsManager->LoadPlayerSetStatus(player);

    // 先清除所有套装效果，避免属性叠加（服务器重启时特别重要）
    // 【性能优化】从数据库加载期间传入 skipUpdate=true
    sItemSetsManager->ClearAllSetEffects(player, true);  // skipUpdate=true
    sItemSetsManager->ClearAllSpellEnhancements(player);

    // 额外安全措施：清除绝对无效的debuff光环
    CheckAndRemoveKnownInvalidDebuffs(player);

    // 设置1秒延迟后重新应用套装效果，确保清除操作完全完成
    uint32 playerGuid = player->GetGUID().GetCounter();
    _delayedRefreshTimers[playerGuid] = 1000; // 1秒延迟
    _initializedPlayers.insert(playerGuid);

    if (sItemSetsConfig->IsDebugMode())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("调试: 玩家从数据库加载，已清除套装效果，1秒后重新应用");
        LOG_INFO("module", ModuleOldFormat("物品套装: 玩家 %s 从数据库加载完成，设置延迟刷新", player->GetName()).c_str());
    }
}

void ItemSetsPlayerScript::OnLogout(Player* player)
{
    if (!sItemSetsConfig->IsModuleEnabled() || !player)
        return;

    // 玩家登出时保存套装状态（如果有变更）
    sItemSetsManager->SavePlayerSetStatus(player);

    // 清理延迟刷新计时器和技能增强效果
    uint32 playerGuid = player->GetGUID().GetCounter();
    _delayedRefreshTimers.erase(playerGuid);
    _auraCheckTimers.erase(playerGuid); // 【修复】清理光环检查计时器
    _initializedPlayers.erase(playerGuid);
    sItemSetsManager->ClearAllSpellEnhancements(player);

    // 【内存泄漏修复】清理玩家相关的内存缓存，防止长期运行内存累积
    // 注意：使用ClearPlayerMemoryCache而不是ClearPlayerSetStatus，避免删除数据库记录
    sItemSetsManager->ClearPlayerMemoryCache(playerGuid);

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 玩家 %s 登出，套装状态已保存并清理内存缓存", player->GetName()).c_str());
    }
}

void ItemSetsPlayerScript::OnPlayerDelete(ObjectGuid guid, uint32 accountId)
{
    if (!sItemSetsConfig->IsModuleEnabled())
        return;

    uint32 playerGuid = guid.GetCounter();

    // 清理`_物品套装_数据`数据
    sItemSetsManager->ClearPlayerSetStatus(playerGuid);

    // 清理内存中的缓存数据
    _delayedRefreshTimers.erase(playerGuid);

    if (sItemSetsConfig->IsDebugMode())
    {
        LOG_INFO("module", ModuleOldFormat("物品套装: 角色删除 - 玩家GUID:%s 账户ID:%s 套装数据已清理", playerGuid, accountId).c_str());
    }
}

void ItemSetsPlayerScript::OnEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update)
{
    if (!sItemSetsConfig->IsModuleEnabled() || !player || !item)
        return;

    // 登录恢复装备时，核心会为每件装备触发一次 OnEquip。
    // 套装系统已经在 OnLoadFromDB / OnLogin 中安排了统一延迟刷新，
    // 这里如果继续做增量刷新，会把同一批装备重复计算很多次，明显拖慢登录主线程。
    if (player->isBeingLoaded() || (player->GetSession() && player->GetSession()->PlayerLoading()))
    {
        if (sItemSetsConfig->IsDebugMode())
        {
            LOG_INFO("module", ModuleOldFormat("物品套装: 跳过玩家 %s 登录阶段的 OnEquip 增量刷新 bag=%s slot=%s itemGuid=%s entry=%s", player->GetName(), bag, slot, item->GetGUID().GetCounter(), item->GetEntry()).c_str());
        }
        return;
    }

    if (sItemSetsConfig->IsDebugMode())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("调试: OnEquip事件 - 槽位:%s 物品GUID:%s", slot, item->GetGUID().GetCounter()).c_str());
    }

    // 使用增量更新优化 - 只更新当前变化的槽位
    sItemSetsManager->RefreshPlayerSetEffectsOptimized(player, slot);
}

// 处理装备移除前的事件
bool ItemSetsPlayerScript::CanUnequipItem(Player* player, uint16 pos, bool /*swap*/)
{
    if (!sItemSetsConfig->IsModuleEnabled() || !player)
        return true;

    // 只处理装备槽位
    if (!player->IsEquipmentPos(pos))
        return true;

    uint8 bag = pos >> 8;
    uint8 slot = pos & 255;

    // 获取即将被移除的装备
    Item* item = player->GetItemByPos(bag, slot);
    if (!item)
        return true;

    // 检查是否是套装物品
    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 setId = sItemSetsManager->GetItemSetId(player, itemGuid);

    if (setId > 0)
    {
        if (sItemSetsConfig->IsDebugMode())
        {
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("调试: CanUnequipItem - 套装物品即将移除 槽位:%s 套装ID:%s", slot, setId).c_str());
        }
    }

    return true; // 允许卸装
}

void ItemSetsPlayerScript::OnAfterMoveItemFromInventory(Player* player, Item* item, uint8 bag, uint8 slot, bool update)
{
    if (!sItemSetsConfig->IsModuleEnabled() || !player || !item)
        return;

    // 处理从装备栏移出的物品，使用增量刷新更新对应槽位
    if (bag == INVENTORY_SLOT_BAG_0 && slot >= EQUIPMENT_SLOT_START && slot < EQUIPMENT_SLOT_END)
    {
        uint32 itemGuid = item->GetGUID().GetCounter();
        uint32 setId = sItemSetsManager->GetItemSetId(player, itemGuid);

        if (sItemSetsConfig->IsDebugMode())
        {
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("调试: OnAfterMoveItemFromInventory - 装备 %s (套装ID:%s) 已从槽位 %s 移除", itemGuid, setId, slot).c_str());
        }

        // 套装物品被移出装备栏后，按槽位做一次增量刷新
        if (setId > 0)
        {
            sItemSetsManager->RefreshPlayerSetEffectsOptimized(player, slot);
        }
    }
}

void ItemSetsPlayerScript::OnUpdate(Player* player, uint32 diff)
{
    if (!sItemSetsConfig->IsModuleEnabled() || !player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 处理延迟刷新计时器
    auto delayIt = _delayedRefreshTimers.find(playerGuid);
    if (delayIt != _delayedRefreshTimers.end())
    {
        if (delayIt->second <= diff)
        {
            // 延迟时间到，执行套装效果刷新
            if (sItemSetsConfig->IsDebugMode())
            {
                ChatHandler(player->GetSession()).PSendSysMessage("调试: 延迟刷新套装效果执行");
            }

            sItemSetsManager->RefreshPlayerSetEffectsOptimized(player, 0xFF); // 全量刷新

            // 移除计时器
            _delayedRefreshTimers.erase(delayIt);
        }
        else
        {
            // 减少延迟时间
            delayIt->second -= diff;
        }
    }

    // 【修复中风险问题】光环检查计时器 - 改为实例变量而非静态变量
    // 使用 _auraCheckTimers 来为每个玩家单独维护计时器
    auto timerIt = _auraCheckTimers.find(playerGuid);
    if (timerIt == _auraCheckTimers.end())
    {
        _auraCheckTimers[playerGuid] = 0;
        timerIt = _auraCheckTimers.find(playerGuid);
    }

    timerIt->second += diff;

    if (timerIt->second >= 10000) // 每10秒检查一次
    {
        timerIt->second = 0;

        // 检查玩家是否脱离战斗
        if (!player->IsInCombat())
        {
            CheckAndRestoreSetAuras(player);
        }
    }
}

void ItemSetsPlayerScript::OnSpellCast(Player* player, Spell* spell, bool skipCheck)
{
    if (!sItemSetsConfig->IsModuleEnabled() || !player || !spell)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    SpellInfo const* spellInfo = spell->GetSpellInfo();
    if (!spellInfo)
        return;

    // 获取玩家的技能增强效果
    std::vector<SpellEnhancement> enhancements = sItemSetsManager->GetPlayerSpellEnhancements(playerGuid);
    if (enhancements.empty())
        return;

    // 检查当前法术是否有增强效果
    for (const auto& enhancement : enhancements)
    {
        if (enhancement.spellId == spellInfo->Id)
        {
            // 【修复中风险问题】技能增强功能实现说明
            // 注意：在 OnSpellCast 钩子中无法直接修改法术伤害
            // 法术伤害的修改需要通过以下方式之一实现：
            // 1. SpellScript 的 OnCalculateAmount 钩子
            // 2. ModifySpellDamage 钩子（如果核心支持）
            // 3. 使用光环来提供伤害加成

            // 当前实现：记录增强效果供其他系统使用
            // 如需实际伤害加成，建议使用套装效果中的光环类型（AURA）
            // 配置一个提供伤害加成的被动光环技能

            if (sItemSetsConfig->IsDebugMode())
            {
                LOG_INFO("module", ModuleOldFormat("物品套装: 检测到法术 %s (%s) 有增强效果 %s%%", spellInfo->Id, spellInfo->SpellName[0], enhancement.enhancePercent).c_str());
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("调试: 法术 %s 有 %s%% 增强效果（需通过光环实现）", spellInfo->SpellName[0], (int)enhancement.enhancePercent).c_str());
            }

            break; // 只应用第一个匹配的增强效果
        }
    }
}

// 只清除绝对确定不应该存在的debuff光环
void ItemSetsPlayerScript::CheckAndRemoveKnownInvalidDebuffs(Player* player)
{
    if (!player)
        return;

    // 【修复中风险问题】不再硬编码"绝对无效"的技能ID
    // 原因：Frostbolt(42842)、Ignite(12654)等可能是合法的套装触发效果
    // 改为：只在确认技能不应该作为永久光环时才移除

    // 获取玩家当前应该有的套装效果ID集合
    std::set<uint32> validSetEffectIds;
    uint32 playerGuid = player->GetGUID().GetCounter();
    auto* status = sItemSetsManager->GetPlayerSetStatus(playerGuid);

    if (status)
    {
        for (const auto& activePair : status->ActiveAttributes)
        {
            uint32 setId = activePair.first;
            const auto& setDataList = sItemSetsManager->GetItemSetData(setId);
            for (const auto& setData : setDataList)
            {
                if (setData.EffectId > 0)
                {
                    validSetEffectIds.insert(setData.EffectId);
                }
            }
        }
    }

    // 只移除那些明确是套装效果但不应该作为光环的技能
    // 使用智能判断而不是硬编码列表
    for (uint32 effectId : validSetEffectIds)
    {
        if (player->HasAura(effectId))
        {
            // 检查这个技能是否应该作为技能增强而不是光环
            if (ShouldBeSpellEnhancement(effectId))
            {
                player->RemoveAurasDueToSpell(effectId);
                if (sItemSetsConfig->IsDebugMode())
                {
                    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(effectId);
                    LOG_INFO("module", ModuleOldFormat("物品套装: 移除应为技能增强的光环 %s (%s)", effectId, spellInfo ? spellInfo->SpellName[0] : "未知").c_str());
                }
            }
        }
    }
}

// 检查技能是否应该作为技能增强而不是光环
bool ItemSetsPlayerScript::ShouldBeSpellEnhancement(uint32 spellId)
{
    // 使用静态缓存避免重复检查相同技能
    static std::unordered_map<uint32, bool> enhancementCache;

    // 先检查缓存
    auto cacheIt = enhancementCache.find(spellId);
    if (cacheIt != enhancementCache.end())
    {
        return cacheIt->second;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        enhancementCache[spellId] = false;
        return false;
    }

    // 使用AzerothCore内置方法进行智能判断
    bool isPassive = spellInfo->IsPassive();
    bool hasAnyAura = spellInfo->HasAnyAura();
    bool isPositive = spellInfo->IsPositive();
    bool isSelfCast = spellInfo->IsSelfCast();

    // 判断逻辑：
    // 1. 被动技能且只有光环效果 -> 光环
    // 2. 正面技能且自我施放且有光环效果 -> 光环
    // 3. 其他情况 -> 技能增强

    bool shouldBeEnhancement = false;
    if (isPassive && hasAnyAura && isSelfCast)
    {
        // 被动光环技能，如属性增强光环
        shouldBeEnhancement = false;
    }
    else if (isPositive && isSelfCast && hasAnyAura && !spellInfo->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE))
    {
        // 正面自我buff光环，如增益光环
        shouldBeEnhancement = false;
    }
    else
    {
        // 攻击技能、负面技能、或其他主动技能
        shouldBeEnhancement = true;
    }

    enhancementCache[spellId] = shouldBeEnhancement;
    return shouldBeEnhancement;
}

// 通过分析技能属性判断是否为主动技能
bool ItemSetsPlayerScript::IsActiveSpellByAnalysis(SpellInfo const* spellInfo)
{
    if (!spellInfo)
        return false;

    // 被动技能应该作为光环
    if (spellInfo->IsPassive())
        return false;

    // 检查是否造成伤害
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellInfo->Effects[i].Effect == SPELL_EFFECT_SCHOOL_DAMAGE ||
            spellInfo->Effects[i].Effect == SPELL_EFFECT_WEAPON_DAMAGE ||
            spellInfo->Effects[i].Effect == SPELL_EFFECT_WEAPON_PERCENT_DAMAGE)
        {
            return true; // 造成伤害的技能应该是技能增强
        }
    }

    // 检查是否为控制技能
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellInfo->Effects[i].Effect == SPELL_EFFECT_APPLY_AURA)
        {
            AuraType auraType = AuraType(spellInfo->Effects[i].ApplyAuraName);
            if (auraType == SPELL_AURA_MOD_DECREASE_SPEED ||        // 减速
                auraType == SPELL_AURA_MOD_FEAR ||                  // 恐惧
                auraType == SPELL_AURA_MOD_STUN ||                  // 昏迷
                auraType == SPELL_AURA_MOD_ROOT)                    // 定身
            {
                return true; // 控制技能应该是技能增强
            }
        }
    }

    return false; // 默认作为光环
}

// 检查玩家是否有指定的技能增强
bool ItemSetsPlayerScript::HasSpellEnhancement(Player* player, uint32 spellId)
{
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    std::vector<SpellEnhancement> enhancements = sItemSetsManager->GetPlayerSpellEnhancements(playerGuid);

    for (const auto& enhancement : enhancements)
    {
        if (enhancement.spellId == spellId)
        {
            return true;
        }
    }

    return false;
}

void ItemSetsPlayerScript::CheckAndRestoreSetAuras(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 获取玩家当前应该有的套装效果
    auto* status = sItemSetsManager->GetPlayerSetStatus(playerGuid);
    if (!status)
        return;

    // 首先清除绝对不应该存在的debuff光环（只清除已知的问题技能）
    CheckAndRemoveKnownInvalidDebuffs(player);

    // 检查每个激活的套装效果
    for (const auto& activePair : status->ActiveAttributes)
    {
        uint32 setId = activePair.first;

        // 获取这个套装的数据
        const auto& setDataList = sItemSetsManager->GetItemSetData(setId);

        for (const auto& setData : setDataList)
        {
            // 检查是否有EffectId（光环效果）
            if (setData.EffectId > 0)
            {
                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(setData.EffectId);
                if (!spellInfo)
                    continue;

                // 检查这个技能是否应该作为技能增强而不是光环
                bool shouldBeEnhancement = ShouldBeSpellEnhancement(setData.EffectId);

                if (shouldBeEnhancement)
                {
                    // 这个技能应该是技能增强，如果玩家身上有光环，移除它
                    if (player->HasAura(setData.EffectId))
                    {
                        player->RemoveAurasDueToSpell(setData.EffectId);
                        if (sItemSetsConfig->IsDebugMode())
                        {
                            LOG_WARN("module", ModuleOldFormat("物品套装: 移除应为技能增强的光环 %s (%s)", setData.EffectId, spellInfo->SpellName[0]).c_str());
                            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("调试: 移除应为技能增强的光环 %s", spellInfo->SpellName[0]).c_str());
                        }
                    }
                    // 确保技能增强效果存在
                    if (!HasSpellEnhancement(player, setData.EffectId))
                    {
                        sItemSetsManager->AddSpellEnhancement(player, setData.EffectId, setData.EffectValue);
                        if (sItemSetsConfig->IsDebugMode())
                        {
                            LOG_INFO("module", ModuleOldFormat("物品套装: 恢复技能增强 %s (%s)", setData.EffectId, spellInfo->SpellName[0]).c_str());
                        }
                    }
                }
                else
                {
                    // 这是合法的被动光环，检查是否需要恢复
                    if (!player->HasAura(setData.EffectId))
                    {
                        // 光环丢失，重新添加
                        if (Aura* aura = player->AddAura(setData.EffectId, player))
                        {
                            // 设置为永久光环
                            aura->SetDuration(-1);
                            aura->SetMaxDuration(-1);

                            if (sItemSetsConfig->IsDebugMode())
                            {
                                LOG_INFO("module", ModuleOldFormat("物品套装: 自动恢复永久光环 %s (%s)", setData.EffectId, spellInfo->SpellName[0]).c_str());
                                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("调试: 自动恢复套装永久光环 %s", spellInfo->SpellName[0]).c_str());
                            }
                        }
                    }
                }
            }
        }
    }
}

//=============================================================================
// ItemSets_WorldScript 实现
//=============================================================================

ItemSets_WorldScript::ItemSets_WorldScript() : WorldScript("ItemSets_WorldScript")
{
    _loadTimer = 0;
    _isLoaded = false;
    _loadDelay = 1 * IN_MILLISECONDS; // 1秒延迟
}

void ItemSets_WorldScript::OnAfterConfigLoad(bool /*reload*/)
{
    // 加载配置
    sItemSetsConfig->LoadConfig();
}

void ItemSets_WorldScript::OnUpdate(uint32 diff)
{
    if (_isLoaded)
        return;

    _loadTimer += diff;
    if (_loadTimer >= _loadDelay)
    {
        // 加载模块
        LoadModule();
        _isLoaded = true;
    }
}

uint32 ItemSets_WorldScript::LoadModule()
{
    // 如果模块启用，初始化管理器
    if (!sItemSetsConfig->IsModuleEnabled())
        return 0;

    sItemSetsManager->Initialize();

    // 命令会通过CommandScript自动注册

    // 获取加载的套装数据数量
    std::vector<uint32> setIds = sItemSetsManager->GetAllSetIds();
    uint32 setCount = setIds.size();

    // 计算每个套装的效果数量
    uint32 effectCount = 0;
    for (uint32 setId : setIds)
    {
        const auto& setData = sItemSetsManager->GetItemSetData(setId);
        effectCount += setData.size();
    }

    // 加载完成后显示结果
    LOG_INFO("server.loading", "→_模板_物品套装 加载完成√");

    return setCount;
}

//=============================================================================
// 脚本加载函数
//=============================================================================

// 添加脚本
void LoadItemSetsScripts()
{
    new ItemSets_WorldScript();
    new ItemSetsPlayerScript();
    new ItemSetsItemScript();
    new ItemSetsCommandScript();
}

// 脚本入口
void AddSC_ItemSets()
{
    LoadItemSetsScripts();
}

// 加载器函数
void AddItemSetsScripts()
{
    AddSC_ItemSets();
}
