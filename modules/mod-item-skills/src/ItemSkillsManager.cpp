#include "ItemSkillsManager.h"
#include "ScriptMgr.h"
#include "Config.h"
#include "Logging/Log.h"
#include "Player.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "DatabaseEnv.h"
#include "StringConvert.h"
#include "Chat.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include "ModuleFormat.h"
#include <vector>
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace
{
    bool BuildSkillDataFromTemplate(uint32 itemId, uint64 itemGuid, uint32 skillTemplateId, ItemSkillData& outData)
    {
        ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillTemplateId);
        if (!skillTemplate)
        {
            LOG_WARN("module.itemskills", ModuleOldFormat("未找到技能模板 ID: %s", skillTemplateId).c_str());
            return false;
        }

        outData.itemId = itemId;
        outData.itemGuid = itemGuid;
        outData.skillTemplateId = skillTemplateId;
        outData.skillId = skillTemplate->spellId;
        outData.triggerType = skillTemplate->triggerType;
        outData.triggerChance = skillTemplate->triggerChance;
        return true;
    }
}

// 单例实例初始化
ItemSkillsManager* ItemSkillsManager::_instance = nullptr;

// 获取单例实例
ItemSkillsManager* ItemSkillsManager::instance()
{
    if (!_instance)
    {
        _instance = new ItemSkillsManager();
    }
    return _instance;
}

// 初始化函数
void ItemSkillsManager::Initialize()
{
    // 读取配置项
    if (!sHelper->GetBoolean(22011))
    {
        _skillTemplates.clear();
        _itemSkillMap.clear();
        _enableRandomGeneration = false;
        return;
    }

    _chanceModifier = sConfigMgr->GetOption<float>("ItemSkills.ChanceModifier", 1.0f);
    _cooldownModifier = sConfigMgr->GetOption<float>("ItemSkills.CooldownModifier", 1.0f);
    _ppmModifier = sConfigMgr->GetOption<float>("ItemSkills.PPMModifier", 1.0f);
    _chargesModifier = sConfigMgr->GetOption<float>("ItemSkills.ChargesModifier", 1.0f);
    _enableRandomGeneration = sConfigMgr->GetOption<bool>("ItemSkills.EnableRandomGeneration", true);
    _minGMRankForCommands = sConfigMgr->GetOption<uint32>("ItemSkills.MinGMRankForCommands", 1);

    // 收集所有加载信息
    LoadItemSkillTemplates();
}

// 加载技能模板配置
uint32 ItemSkillsManager::LoadItemSkillTemplates()
{
    _skillTemplates.clear();
    _itemSkillMap.clear();

    if (!sHelper->GetBoolean(22011))
        return 0;

    // 使用新的中文表名查询
    QueryResult result = WorldDatabase.Query("SELECT * FROM `_模板_物品技能`");

    if (!result)
    {
        return 0;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        ItemSkillTemplate temp;

        // 根据SQL表实际字段顺序读取
        temp.id = fields[0].Get<uint32>();                      // id
        temp.comment = fields[1].Get<std::string>();            // 注释
        temp.clientDisplay = fields[2].Get<std::string>();      // 客户端显示
        temp.group = fields[3].Get<uint32>();                   // 组
        temp.level = fields[4].Get<uint32>();                   // 等级
        temp.spellId = fields[5].Get<uint32>();                 // 技能id
        temp.chance = fields[6].Get<float>();                   // 几率
        temp.triggerType = fields[7].Get<uint8>();              // 技能触发类型
        temp.triggerChance = fields[8].Get<float>();            // 触发几率
        temp.charges = fields[9].Get<int16>();                  // 技能使用次数
        temp.ppmRate = fields[10].Get<float>();                 // 技能每分钟触发次数
        temp.cooldownMode = fields[11].Get<uint8>();            // 技能冷却时间模式
        temp.cooldownValue = fields[12].Get<int32>();           // 技能冷却值
        temp.costMode = fields[13].Get<uint8>();                // 技能消耗模式
        temp.castMode = fields[14].Get<uint8>();                // 技能施法时间模式
        temp.powerMode = fields[15].Get<uint8>();               // 技能强化模式
        temp.powerValue = fields[16].Get<int32>();              // 技能伤害值
        temp.skillType = fields[17].Get<uint8>();               // 技能类型
        temp.skillDuration = fields[18].Get<int32>();           // 技能持续时间
        temp.enabled = fields[19].Get<uint8>() == 1;            // 启用状态
        temp.priority = fields[20].Get<int32>();                // 优先级

        // 注意：SQL表中没有itemId字段，这是通过外部关联的
        temp.itemId = 0;  // 不从表中读取，而是通过物品技能_数据表关联

        _skillTemplates[temp.id] = temp;

        // 不再构建itemId映射，因为表中没有这个字段
        // 物品和技能的关系存储在 物品技能_数据 表中

        ++count;
    } while (result->NextRow());

    return count;
}


// 获取技能模板
ItemSkillTemplate const* ItemSkillsManager::GetSkillTemplate(uint32 templateId) const
{
    auto itr = _skillTemplates.find(templateId);
    if (itr != _skillTemplates.end())
    {
        return &itr->second;
    }
    return nullptr;
}

// 获取物品关联的技能模板
std::vector<ItemSkillTemplate const*> ItemSkillsManager::GetItemSkillTemplates(uint32 itemId) const
{
    std::vector<ItemSkillTemplate const*> result;

    auto itr = _itemSkillMap.find(itemId);
    if (itr != _itemSkillMap.end())
    {
        for (uint32 templateId : itr->second)
        {
            if (const ItemSkillTemplate* tmpl = GetSkillTemplate(templateId))
            {
                result.push_back(tmpl);
            }
        }
    }

    return result;
}

// 从同组技能中按权重随机选择一个
// 注意：itemId 参数在这里未被使用，函数直接从所有可用技能模板中选择
// 这是为了支持鉴定系统在创建新物品时分配技能的需求
ItemSkillTemplate const* ItemSkillsManager::SelectSkillByWeight(uint32 itemId, uint32 group) const
{
    std::vector<ItemSkillTemplate const*> groupSkills;
    float totalWeight = 0.0f;

    // 从所有技能模板中筛选指定组且已启用的技能
    // 这支持两种用途：
    // 1. 鉴定系统在创建物品时为其分配技能
    // 2. 物品实例本身已经有了技能数据
    for (const auto& pair : _skillTemplates)
    {
        const ItemSkillTemplate& skill = pair.second;
        if (skill.group == group && skill.enabled)
        {
            groupSkills.push_back(&skill);
            totalWeight += skill.chance;
        }
    }

    if (groupSkills.empty() || totalWeight <= 0)
    {
        LOG_DEBUG("module.itemskills", ModuleOldFormat("SelectSkillByWeight: 找不到组号 %s 的启用技能", group).c_str());
        return nullptr;
    }

    // 按权重随机选择
    float randomValue = frand(0.0f, totalWeight);
    float currentWeight = 0.0f;

    for (const auto* skill : groupSkills)
    {
        currentWeight += skill->chance;
        if (randomValue <= currentWeight)
        {
            LOG_DEBUG("module.itemskills", ModuleOldFormat("SelectSkillByWeight: 为物品 %s 从组 %s 选择了技能 %s（法术ID: %s）", itemId, group, skill->id, skill->spellId).c_str());
            return skill;
        }
    }

    LOG_ERROR("module.itemskills", ModuleOldFormat("SelectSkillByWeight: 组 %s 权重随机未命中，totalWeight=%s", group, totalWeight).c_str());
    return nullptr;
}

// 获取物品的有效技能（仅返回已启用的）
std::vector<ItemSkillTemplate const*> ItemSkillsManager::GetEnabledItemSkills(uint32 itemId) const
{
    std::vector<ItemSkillTemplate const*> result;

    auto itr = _itemSkillMap.find(itemId);
    if (itr != _itemSkillMap.end())
    {
        for (uint32 templateId : itr->second)
        {
            const ItemSkillTemplate* tmpl = GetSkillTemplate(templateId);
            // 只返回已启用的技能
            if (tmpl && tmpl->enabled)
            {
                result.push_back(tmpl);
            }
        }
    }

    return result;
}

// 模块加载函数
void LoadItemSkillsManager()
{
    sItemSkillsManager->Initialize();
}

// ===== 新增：玩家数据管理方法实现 =====

#include <ctime>
#include "Bag.h"

// 加载玩家的物品技能配置（仅加载装备栏，背包物品按需加载）
void ItemSkillsManager::LoadPlayerItemSkills(Player* player)
{
    if (!player)
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();

    // 清理旧数据
    ClearPlayerData(playerGuid);

    // 只获取装备栏的物品GUID（背包物品按需加载）
    std::vector<uint64> itemGuids;

    // 装备栏
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            itemGuids.push_back(item->GetGUID().GetCounter());
    }

    if (itemGuids.empty())
        return;

    // 【性能优化】使用批量查询替代逐个查询
    auto allSkillsData = sItemSkillsDBHelper->GetItemSkillsDataBatch(itemGuids);

    // 加载所有物品的技能数据到内存
    uint32 loadedCount = 0;
    for (const auto& [itemGuid, skills] : allSkillsData)
    {
        if (!skills.empty())
        {
            _playerItemSkills[playerGuid][itemGuid] = skills;

            // 初始化使用次数
            InitializeSkillCharges(playerGuid, itemGuid, skills);
            loadedCount++;
        }
    }

    // 注意：装备触发技能的施放已移至延迟事件 ApplyEquipSkillsEvent 中
    // 避免登录时一次性施放大量技能导致服务器卡顿
}

// 清理玩家数据
void ItemSkillsManager::ClearPlayerData(uint64 playerGuid)
{
    _playerItemSkills.erase(playerGuid);
    _skillCooldowns.erase(playerGuid);
    _skillCharges.erase(playerGuid);
}

// 按需加载单个物品的技能数据（装备时调用）
void ItemSkillsManager::LoadItemSkillsOnDemand(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();
    uint64 itemGuid = item->GetGUID().GetCounter();

    // 检查是否已加载
    auto playerIt = _playerItemSkills.find(playerGuid);
    if (playerIt != _playerItemSkills.end())
    {
        if (playerIt->second.find(itemGuid) != playerIt->second.end())
            return;  // 已加载，无需重复加载
    }

    // 从数据库加载
    std::vector<ItemSkillData> skills = sItemSkillsDBHelper->GetItemSkillsData(item);

    if (!skills.empty())
    {
        _playerItemSkills[playerGuid][itemGuid] = skills;
        InitializeSkillCharges(playerGuid, itemGuid, skills);

        LOG_DEBUG("module.itemskills", ModuleOldFormat("[按需加载] 玩家 %s 物品 %s 加载了 %s 个技能", player->GetName(), item->GetEntry(), skills.size()).c_str());
    }
}

// 【修复】刷新单个物品的技能数据到内存（添加/移除技能后调用）
void ItemSkillsManager::RefreshItemSkillsInMemory(uint64 playerGuid, Item* item)
{
    if (!item)
        return;

    uint64 itemGuid = item->GetGUID().GetCounter();

    // 清除该物品的旧缓存
    auto playerIt = _playerItemSkills.find(playerGuid);
    if (playerIt != _playerItemSkills.end())
    {
        playerIt->second.erase(itemGuid);
    }

    // 清除技能使用次数缓存
    auto chargesIt = _skillCharges.find(playerGuid);
    if (chargesIt != _skillCharges.end())
    {
        chargesIt->second.erase(itemGuid);
    }

    // 清除冷却缓存
    auto cooldownIt = _skillCooldowns.find(playerGuid);
    if (cooldownIt != _skillCooldowns.end())
    {
        cooldownIt->second.erase(itemGuid);
    }

    // 从数据库重新加载
    std::vector<ItemSkillData> skills = sItemSkillsDBHelper->GetItemSkillsData(item);

    if (!skills.empty())
    {
        _playerItemSkills[playerGuid][itemGuid] = skills;
        InitializeSkillCharges(playerGuid, itemGuid, skills);

        LOG_DEBUG("module.itemskills", ModuleOldFormat("[刷新缓存] 物品 %s 刷新了 %s 个技能到内存", item->GetEntry(), skills.size()).c_str());
    }
    else
    {
        LOG_DEBUG("module.itemskills", ModuleOldFormat("[刷新缓存] 物品 %s 已无技能数据", item->GetEntry()).c_str());
    }
}

// 从内存获取物品的技能数据
std::vector<ItemSkillData> ItemSkillsManager::GetItemSkillsFromMemory(uint64 playerGuid, uint64 itemGuid)
{
    auto playerIt = _playerItemSkills.find(playerGuid);
    if (playerIt != _playerItemSkills.end())
    {
        auto itemIt = playerIt->second.find(itemGuid);
        if (itemIt != playerIt->second.end())
            return itemIt->second;
    }

    auto externalIt = _externalItemSkills.find(playerGuid);
    if (externalIt == _externalItemSkills.end())
        return {};

    auto itemIt = externalIt->second.find(itemGuid);
    if (itemIt == externalIt->second.end())
        return {};

    return itemIt->second;
}

void ItemSkillsManager::SetExternalItemSkills(uint64 playerGuid, uint64 itemGuid, uint32 itemId, std::vector<uint32> const& skillTemplateIds)
{
    auto& externalSkills = _externalItemSkills[playerGuid][itemGuid];
    externalSkills.clear();

    std::unordered_set<uint32> seenTemplateIds;
    for (uint32 skillTemplateId : skillTemplateIds)
    {
        if (skillTemplateId == 0 || !seenTemplateIds.insert(skillTemplateId).second)
            continue;

        ItemSkillData skillData{};
        if (BuildSkillDataFromTemplate(itemId, itemGuid, skillTemplateId, skillData))
            externalSkills.push_back(skillData);
    }

    auto chargesIt = _skillCharges.find(playerGuid);
    if (chargesIt != _skillCharges.end())
        chargesIt->second.erase(itemGuid);

    auto cooldownIt = _skillCooldowns.find(playerGuid);
    if (cooldownIt != _skillCooldowns.end())
        cooldownIt->second.erase(itemGuid);

    if (externalSkills.empty())
    {
        _externalItemSkills[playerGuid].erase(itemGuid);
        if (_externalItemSkills[playerGuid].empty())
            _externalItemSkills.erase(playerGuid);
        return;
    }

    InitializeSkillCharges(playerGuid, itemGuid, externalSkills);
}

void ItemSkillsManager::RemoveExternalItemSkills(uint64 playerGuid, uint64 itemGuid)
{
    auto externalIt = _externalItemSkills.find(playerGuid);
    if (externalIt != _externalItemSkills.end())
    {
        externalIt->second.erase(itemGuid);
        if (externalIt->second.empty())
            _externalItemSkills.erase(externalIt);
    }

    auto chargesIt = _skillCharges.find(playerGuid);
    if (chargesIt != _skillCharges.end())
        chargesIt->second.erase(itemGuid);

    auto cooldownIt = _skillCooldowns.find(playerGuid);
    if (cooldownIt != _skillCooldowns.end())
        cooldownIt->second.erase(itemGuid);
}

std::map<uint64, std::vector<ItemSkillData>> const* ItemSkillsManager::GetExternalItemSkills(uint64 playerGuid) const
{
    auto itr = _externalItemSkills.find(playerGuid);
    if (itr == _externalItemSkills.end())
        return nullptr;

    return &itr->second;
}

bool ItemSkillsManager::HasOtherItemProvidingSpell(uint64 playerGuid, uint64 excludedItemGuid, uint32 spellId, uint8 triggerType) const
{
    auto hasSpellInMap = [excludedItemGuid, spellId, triggerType](auto const& itemMap)
    {
        for (auto const& pair : itemMap)
        {
            if (pair.first == excludedItemGuid)
                continue;

            for (ItemSkillData const& skill : pair.second)
            {
                if (skill.skillId == spellId && skill.triggerType == triggerType)
                    return true;
            }
        }

        return false;
    };

    auto playerIt = _playerItemSkills.find(playerGuid);
    if (playerIt != _playerItemSkills.end() && hasSpellInMap(playerIt->second))
        return true;

    auto externalIt = _externalItemSkills.find(playerGuid);
    if (externalIt != _externalItemSkills.end() && hasSpellInMap(externalIt->second))
        return true;

    return false;
}

// 检查技能冷却
bool ItemSkillsManager::CheckSkillCooldown(uint64 playerGuid, uint64 itemGuid, uint32 skillId, uint32 cooldownMs)
{
    if (cooldownMs == 0 || cooldownMs == static_cast<uint32>(-1))
        return true;  // 无冷却

    auto playerIt = _skillCooldowns.find(playerGuid);
    if (playerIt == _skillCooldowns.end())
        return true;

    auto itemIt = playerIt->second.find(itemGuid);
    if (itemIt == playerIt->second.end())
        return true;

    auto skillIt = itemIt->second.find(skillId);
    if (skillIt == itemIt->second.end())
        return true;

    uint64 lastTime = skillIt->second;
    uint64 currentTime = static_cast<uint64>(std::time(nullptr));
    uint64 cooldownSeconds = cooldownMs / 1000;

    return currentTime >= lastTime + cooldownSeconds;
}

// 设置技能冷却
void ItemSkillsManager::SetSkillCooldown(uint64 playerGuid, uint64 itemGuid, uint32 skillId)
{
    uint64 currentTime = static_cast<uint64>(std::time(nullptr));
    _skillCooldowns[playerGuid][itemGuid][skillId] = currentTime;
}

// 检查技能使用次数
bool ItemSkillsManager::HasSkillCharge(uint64 playerGuid, uint64 itemGuid, uint32 skillId)
{
    auto playerIt = _skillCharges.find(playerGuid);
    if (playerIt == _skillCharges.end())
        return true;  // 无记录，默认有次数

    auto itemIt = playerIt->second.find(itemGuid);
    if (itemIt == playerIt->second.end())
        return true;

    auto skillIt = itemIt->second.find(skillId);
    if (skillIt == itemIt->second.end())
        return true;

    int32 charges = skillIt->second;
    return charges != 0;  // 0表示用完
}

// 减少使用次数
void ItemSkillsManager::ConsumeSkillCharge(uint64 playerGuid, uint64 itemGuid, uint32 skillId)
{
    auto playerIt = _skillCharges.find(playerGuid);
    if (playerIt == _skillCharges.end())
        return;

    auto itemIt = playerIt->second.find(itemGuid);
    if (itemIt == playerIt->second.end())
        return;

    auto skillIt = itemIt->second.find(skillId);
    if (skillIt == itemIt->second.end())
        return;

    if (skillIt->second > 0)
        skillIt->second--;  // 减少次数
}

// 初始化技能使用次数
void ItemSkillsManager::InitializeSkillCharges(uint64 playerGuid, uint64 itemGuid, const std::vector<ItemSkillData>& skills)
{
    for (const auto& skillData : skills)
    {
        // 从模板读取初始使用次数
        ItemSkillTemplate const* tmpl = GetSkillTemplate(skillData.skillTemplateId);
        if (tmpl && tmpl->charges != 0)
        {
            // 应用使用次数修正系数
            int32 effectiveCharges = static_cast<int32>(tmpl->charges * _chargesModifier);
            _skillCharges[playerGuid][itemGuid][skillData.skillId] = effectiveCharges;
        }
    }
}
