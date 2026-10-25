#include "RuneSystem.h"
#include "DatabaseEnv.h"
#include "ScriptMgr.h"
#include "GameTime.h"
#include "RuneUtils.h"
#include "ModuleFormat.h"

RuneSystem* RuneSystem::_instance = nullptr;

RuneSystem* RuneSystem::instance()
{
    if (!_instance)
    {
        _instance = new RuneSystem();
    }
    return _instance;
}

void RuneSystem::Initialize()
{
    LoadRunesFromDB();

    // 【性能优化】加载符文效果缓存
    LoadRuneEffectCache();
}

void RuneSystem::LoadRunesFromDB()
{
    _runeInfoStore.clear();
    _runeItemIdIndex.clear();

    // 添加错误处理和安全检查
    try
    {
        QueryResult result = WorldDatabase.Query("SELECT * FROM _模板_物品符文");

        if (!result)
        {
            return;
        }

        uint32 count = 0;
        do
        {
            Field* fields = result->Fetch();

            CustomRuneInfo rune;
            uint32 index = 0;

            rune.runeId = fields[index++].Get<uint32>();
            rune.itemId = fields[index++].Get<uint32>();
            index++; // 跳过组字段

            if (fields[index].IsNull())
                rune.name = "";
            else
                rune.name = fields[index].Get<std::string>();
            index++;

            rune.runeSlot = fields[index++].Get<uint32>();
            rune.runeQuality = fields[index++].Get<uint32>();
            rune.spellId = fields[index++].Get<uint32>();
            rune.cooldown = fields[index++].Get<uint32>();
            rune.procChance = fields[index++].Get<uint32>();
            rune.effectType = fields[index++].Get<uint32>();

            if (fields[index].IsNull())
                rune.effectValues = "";
            else
                rune.effectValues = fields[index].Get<std::string>();
            index++;

            if (fields[index].IsNull())
                rune.effectDescription = "";
            else
                rune.effectDescription = fields[index].Get<std::string>();
            index++;

            _runeInfoStore[rune.runeId] = rune;
            if (rune.itemId != 0)
                _runeItemIdIndex[rune.itemId] = rune.runeId;
            count++;

        } while (result->NextRow());
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("server.loading", ModuleOldFormat("加载符文数据时发生错误: %s", e.what()).c_str());
    }
}

void RuneSystem::LoadRuneStorageFromDB(uint32 characterGuid)
{
    _characterRuneStore[characterGuid].clear();

    // 添加错误处理和安全检查
    try
    {
        // 查询角色相关的符文数据
        std::string query = "SELECT ID, 角色GUID, 物品ID, 物品GUID, 插槽ID, 符文物品ID FROM 符文系统_数据 WHERE 角色GUID = " + std::to_string(characterGuid);
        QueryResult result = CharacterDatabase.Query(query.c_str());

        if (!result)
        {
            return;
        }

        do
        {
            Field* fields = result->Fetch();

            CustomRuneStorage storage;
            uint32 index = 0;

            storage.recordId = fields[index++].Get<uint32>();
            storage.characterGuid = fields[index++].Get<uint32>();
            storage.itemId = fields[index++].Get<uint32>();
            storage.itemGuid = fields[index++].Get<uint32>();

            if (fields[index].IsNull())
                storage.slotIds = "";
            else
                storage.slotIds = fields[index].Get<std::string>();
            index++;

            if (fields[index].IsNull())
                storage.runeIds = "";
            else
                storage.runeIds = fields[index].Get<std::string>();
            index++;

            _itemRuneSlotStore[storage.itemGuid] = storage; // 同步填充按物品索引的缓存，登录后无需再逐件查库
            _characterRuneStore[characterGuid].push_back(std::move(storage));

        } while (result->NextRow());

        if (sConfigMgr->GetOption<bool>("RuneSystem.DebugMode", false))
        {
            LOG_DEBUG("rune.system", ModuleOldFormat("加载角色 %s 的 %s 个装备符文数据", characterGuid, _characterRuneStore[characterGuid].size()).c_str());
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("server.loading", ModuleOldFormat("加载角色符文数据时发生错误: %s", e.what()).c_str());
    }
}

void RuneSystem::LoadItemRuneStorageFromDB(uint32 itemGuid)
{
    LOG_DEBUG("rune.system", ModuleOldFormat("LoadItemRuneStorageFromDB: 开始加载物品GUID %s", itemGuid).c_str());

    _itemRuneSlotStore.erase(itemGuid);

    // 添加错误处理和安全检查
    try
    {
        // 查询物品相关的符文数据
        std::string query = "SELECT ID, 角色GUID, 物品ID, 物品GUID, 插槽ID, 符文物品ID FROM 符文系统_数据 WHERE 物品GUID = " + std::to_string(itemGuid);
        QueryResult result = CharacterDatabase.Query(query.c_str());

        if (!result)
        {
            LOG_DEBUG("rune.system", ModuleOldFormat("LoadItemRuneStorageFromDB: 物品GUID %s 没有符文数据", itemGuid).c_str());
            return;
        }

        Field* fields = result->Fetch();

        CustomRuneStorage storage;
        uint32 index = 0;

        storage.recordId = fields[index++].Get<uint32>();
        storage.characterGuid = fields[index++].Get<uint32>();
        storage.itemId = fields[index++].Get<uint32>();
        storage.itemGuid = fields[index++].Get<uint32>();

        if (fields[index].IsNull())
            storage.slotIds = "";
        else
            storage.slotIds = fields[index].Get<std::string>();
        index++;

        if (fields[index].IsNull())
            storage.runeIds = "";
        else
            storage.runeIds = fields[index].Get<std::string>();
        index++;

        LOG_INFO("rune.system", ModuleOldFormat("LoadItemRuneStorageFromDB: 成功加载物品GUID %s, 插槽ID: %s, 符文ID: %s", itemGuid, storage.slotIds, storage.runeIds).c_str());

        _itemRuneSlotStore[itemGuid] = std::move(storage);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("rune.system", ModuleOldFormat("LoadItemRuneStorageFromDB: 加载物品符文数据时发生错误: %s", e.what()).c_str());
    }
}

CustomRuneStorage const* RuneSystem::GetItemRuneStorage(uint32 itemGuid) const
{
    auto itr = _itemRuneSlotStore.find(itemGuid);
    if (itr != _itemRuneSlotStore.end())
    {
        return &itr->second;
    }
    return nullptr;
}

CustomRuneInfo const* RuneSystem::GetRuneInfo(uint32 runeId) const
{
    auto itr = _runeInfoStore.find(runeId);
    if (itr != _runeInfoStore.end())
    {
        return &itr->second;
    }

    return nullptr;
}

CustomRuneInfo const* RuneSystem::GetRuneInfoByItemId(uint32 itemId) const
{
    // 战斗热路径每符文调用一次，O(N) 全表遍历改 itemId 索引 O(1)
    auto idxItr = _runeItemIdIndex.find(itemId);
    if (idxItr != _runeItemIdIndex.end())
        return GetRuneInfo(idxItr->second);

    return nullptr;
}

std::vector<CustomRuneStorage> RuneSystem::GetCharacterRunes(uint32 characterGuid) const
{
    auto itr = _characterRuneStore.find(characterGuid);
    if (itr != _characterRuneStore.end())
    {
        return itr->second;
    }

    return std::vector<CustomRuneStorage>();
}

// 这个方法已废弃，使用GetItemRuneStorage代替
// std::vector<CustomRuneStorage> RuneSystem::GetItemRuneSlots(uint32 itemGuid) const

bool RuneSystem::AddRuneToItem(Player* player, Item* item, uint32 runeId, uint32 slotId)
{
    if (!player || !item)
    {
        LOG_ERROR("rune.system", "AddRuneToItem: player或item为空");
        return false;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();

    // 验证符文是否存在并获取符文信息
    const CustomRuneInfo* runeInfo = GetRuneInfoByItemId(runeId);
    if (!runeInfo)
    {
        LOG_ERROR("rune.system", ModuleOldFormat("AddRuneToItem: 符文物品ID %s 未在符文表中配置", runeId).c_str());
        return false;
    }

    // 校验符文槽位是否匹配（runeSlot=0表示任意槽位可用）
    if (runeInfo->runeSlot != 0 && runeInfo->runeSlot != slotId)
    {
        LOG_ERROR("rune.system", ModuleOldFormat("AddRuneToItem: 符文 %s 的槽位类型 %s 与目标槽位 %s 不匹配", runeId, runeInfo->runeSlot, slotId).c_str());
        return false;
    }

    // 检查物品是否已开槽（先检查内存，如果没有则尝试从数据库加载）
    auto it = _itemRuneSlotStore.find(itemGuid);
    if (it == _itemRuneSlotStore.end())
    {
        LOG_INFO("rune.system", ModuleOldFormat("AddRuneToItem: 内存中没有物品GUID %s 的数据，尝试从数据库加载", itemGuid).c_str());

        // 尝试从数据库加载
        LoadItemRuneStorageFromDB(itemGuid);

        // 再次检查
        it = _itemRuneSlotStore.find(itemGuid);
        if (it == _itemRuneSlotStore.end())
        {
            LOG_ERROR("rune.system", ModuleOldFormat("AddRuneToItem: 物品GUID %s 还没有开槽（数据库中也没有记录）", itemGuid).c_str());
            return false;
        }
    }

    CustomRuneStorage* storage = &it->second;

    // 解析现有的槽位和符文ID
    auto slotIdList = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
    auto runeIdList = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

    LOG_INFO("rune.system", ModuleOldFormat("AddRuneToItem: 物品GUID %s, 槽位列表: %s, 符文列表: %s", itemGuid, storage->slotIds, storage->runeIds).c_str());

    // 确保槽位ID存在
    bool slotExists = false;
    size_t slotIndex = 0;
    for (size_t i = 0; i < slotIdList.size(); ++i)
    {
        if (slotIdList[i] == slotId)
        {
            slotExists = true;
            slotIndex = i;
            break;
        }
    }

    if (!slotExists)
    {
        LOG_ERROR("rune.system", ModuleOldFormat("AddRuneToItem: 槽位 %s 不存在", slotId).c_str());
        return false;
    }

    // 检查槽位是否已有符文
    if (slotIndex < runeIdList.size() && runeIdList[slotIndex] != 0)
    {
        LOG_DEBUG("rune.system", ModuleOldFormat("AddRuneToItem: 槽位 %s 已经有符文 %s，忽略本次镶嵌请求", slotId, runeIdList[slotIndex]).c_str());
        return false;
    }

    // 先检查玩家背包中是否有该符文物品（在修改数据之前检查）
    Item* runeItem = player->GetItemByEntry(runeId);
    if (!runeItem)
    {
        LOG_ERROR("rune.system", ModuleOldFormat("AddRuneToItem: 玩家背包中没有符文物品 %s", runeId).c_str());
        return false;
    }

    LOG_INFO("rune.system", ModuleOldFormat("AddRuneToItem: 找到符文物品 %s, 数量: %s", runeId, runeItem->GetCount()).c_str());

    // 更新符文ID
    if (slotIndex < runeIdList.size())
        runeIdList[slotIndex] = runeId;
    else
        runeIdList.push_back(runeId);

    // 将列表转回字符串
    storage->slotIds = RuneUtils::JoinUInt32WithComma(slotIdList);
    storage->runeIds = RuneUtils::JoinUInt32WithComma(runeIdList);

    // 保存到数据库 - 使用INSERT ON DUPLICATE KEY UPDATE，确保记录存在时更新，不存在时插入
    // 【数据丢失修复】改同步写并删除"写后立即重读"：原先异步 Execute 后马上
    // LoadItemRuneStorageFromDB，异步写常未落库 → 内存缓存被旧数据回滚（槽位显示为空
    // 可再次镶嵌，第二次写覆盖第一次，已消耗的符文丢失）。内存态即权威，写成功无需重读。
    uint32 slotCount = static_cast<uint32>(slotIdList.size());
    CharacterDatabase.DirectExecute(ModuleOldFormat("INSERT INTO 符文系统_数据 (物品GUID, 角色GUID, 物品ID, 插槽数量, 插槽ID, 符文物品ID) "
        "VALUES (%s, %s, %s, %s, '%s', '%s') "
        "ON DUPLICATE KEY UPDATE 插槽ID = VALUES(插槽ID), 符文物品ID = VALUES(符文物品ID), 插槽数量 = VALUES(插槽数量)", itemGuid, storage->characterGuid, storage->itemId, slotCount, storage->slotIds, storage->runeIds).c_str());

    // 删除玩家背包中的符文物品（消耗1个）
    LOG_DEBUG("rune.system", ModuleOldFormat("AddRuneToItem: 正在消耗符文物品 %s", runeId).c_str());
    player->DestroyItemCount(runeId, 1, true);

    LOG_DEBUG("rune.system", "AddRuneToItem: 成功添加符文到物品");
    RuneCombat::InvalidateSpellBonusCache(player->GetGUID().GetCounter());
    return true;
}

bool RuneSystem::RemoveRuneFromItem(Player* player, Item* item, uint32 slotId)
{
    if (!player || !item)
        return false;

    uint32 itemGuid = item->GetGUID().GetCounter();

    // 获取物品的符文存储（先检查内存，如果没有则尝试从数据库加载）
    auto it = _itemRuneSlotStore.find(itemGuid);
    if (it == _itemRuneSlotStore.end())
    {
        // 尝试从数据库加载
        LoadItemRuneStorageFromDB(itemGuid);

        // 再次检查
        it = _itemRuneSlotStore.find(itemGuid);
        if (it == _itemRuneSlotStore.end())
            return false;
    }

    CustomRuneStorage* storage = &it->second;

    // 解析槽位和符文ID
    auto slotIdList = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
    auto runeIdList = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

    // 查找槽位索引
    for (size_t i = 0; i < slotIdList.size(); ++i)
    {
        if (slotIdList[i] == slotId)
        {
            // 清空该槽位的符文
            if (i < runeIdList.size())
                runeIdList[i] = 0;

            // 更新字符串
            storage->runeIds = RuneUtils::JoinUInt32WithComma(runeIdList);

            // 保存到数据库（同时更新插槽数量）
            // 【数据丢失修复】同步写 + 删除"写后重读"（理由同 AddRuneToItem）
            uint32 slotCount = static_cast<uint32>(slotIdList.size());
            CharacterDatabase.DirectExecute(ModuleOldFormat("UPDATE 符文系统_数据 SET 符文物品ID = '%s', 插槽数量 = %s WHERE 物品GUID = %s", storage->runeIds, slotCount, itemGuid).c_str());

            RuneCombat::InvalidateSpellBonusCache(player->GetGUID().GetCounter());
            return true;
        }
    }

    return false;
}

bool RuneSystem::UpgradeRune(Player* player, Item* item, uint32 slotId)
{
    // 符文升级功能：当前版本暂不支持，返回false
    // TODO: 实现符文升级逻辑（需要额外的等级和经验字段）
    return false;
}

bool RuneSystem::TransferRune(Player* player, Item* sourceItem, uint32 sourceSlot, Item* targetItem, uint32 targetSlot)
{
    if (!player || !sourceItem || !targetItem)
        return false;

    uint32 sourceGuid = sourceItem->GetGUID().GetCounter();
    uint32 targetGuid = targetItem->GetGUID().GetCounter();

    // 获取源物品存储（先检查内存，如果没有则尝试从数据库加载）
    auto sourceIt = _itemRuneSlotStore.find(sourceGuid);
    if (sourceIt == _itemRuneSlotStore.end())
    {
        LoadItemRuneStorageFromDB(sourceGuid);
        sourceIt = _itemRuneSlotStore.find(sourceGuid);
        if (sourceIt == _itemRuneSlotStore.end())
            return false;
    }

    // 获取目标物品存储（先检查内存，如果没有则尝试从数据库加载）
    if (_itemRuneSlotStore.find(targetGuid) == _itemRuneSlotStore.end())
    {
        LoadItemRuneStorageFromDB(targetGuid);

        // 如果数据库中也没有，说明目标物品还没有开槽，不能转移符文
        if (_itemRuneSlotStore.find(targetGuid) == _itemRuneSlotStore.end())
        {
            LOG_ERROR("rune.system", ModuleOldFormat("TransferRune: 目标物品 %s 没有开槽", targetGuid).c_str());
            return false;
        }
    }

    CustomRuneStorage* sourceStorage = &sourceIt->second;
    CustomRuneStorage* targetStorage = &_itemRuneSlotStore[targetGuid];

    // 解析源符文
    auto sourceSlotIds = RuneUtils::ParseCommaSeparatedUInt32(sourceStorage->slotIds);
    auto sourceRuneIds = RuneUtils::ParseCommaSeparatedUInt32(sourceStorage->runeIds);

    // 查找要转移的符文
    uint32 runeIdToTransfer = 0;
    size_t sourceIndex = 0;
    for (size_t i = 0; i < sourceSlotIds.size(); ++i)
    {
        if (sourceSlotIds[i] == sourceSlot && i < sourceRuneIds.size())
        {
            runeIdToTransfer = sourceRuneIds[i];
            sourceIndex = i;
            break;
        }
    }

    if (runeIdToTransfer == 0)
    {
        LOG_ERROR("rune.system", ModuleOldFormat("TransferRune: 源槽位 %s 没有符文", sourceSlot).c_str());
        return false;
    }

    // 解析目标符文槽位
    auto targetSlotIds = RuneUtils::ParseCommaSeparatedUInt32(targetStorage->slotIds);
    auto targetRuneIds = RuneUtils::ParseCommaSeparatedUInt32(targetStorage->runeIds);

    // 检查目标槽位是否存在
    bool targetSlotExists = false;
    size_t targetIndex = 0;
    for (size_t i = 0; i < targetSlotIds.size(); ++i)
    {
        if (targetSlotIds[i] == targetSlot)
        {
            targetSlotExists = true;
            targetIndex = i;
            break;
        }
    }

    if (!targetSlotExists)
    {
        LOG_ERROR("rune.system", ModuleOldFormat("TransferRune: 目标槽位 %s 不存在", targetSlot).c_str());
        return false;
    }

    // 检查目标槽位是否为空
    if (targetIndex < targetRuneIds.size() && targetRuneIds[targetIndex] != 0)
    {
        LOG_ERROR("rune.system", ModuleOldFormat("TransferRune: 目标槽位 %s 已经有符文 %s", targetSlot, targetRuneIds[targetIndex]).c_str());
        return false;
    }

    // 获取符文信息，检查槽位类型是否匹配
    const CustomRuneInfo* runeInfo = GetRuneInfoByItemId(runeIdToTransfer);
    if (runeInfo && runeInfo->runeSlot != 0 && runeInfo->runeSlot != targetSlot)
    {
        LOG_ERROR("rune.system", ModuleOldFormat("TransferRune: 符文 %s 的槽位类型 %s 与目标槽位 %s 不匹配", runeIdToTransfer, runeInfo->runeSlot, targetSlot).c_str());
        return false;
    }

    // 清除源槽位
    sourceRuneIds[sourceIndex] = 0;
    sourceStorage->runeIds = RuneUtils::JoinUInt32WithComma(sourceRuneIds);

    // 添加到目标槽位
    if (targetIndex < targetRuneIds.size())
        targetRuneIds[targetIndex] = runeIdToTransfer;
    else
        targetRuneIds.push_back(runeIdToTransfer);

    targetStorage->slotIds = RuneUtils::JoinUInt32WithComma(targetSlotIds);
    targetStorage->runeIds = RuneUtils::JoinUInt32WithComma(targetRuneIds);

    // 保存到数据库（更新源物品）
    // 【数据丢失修复】两条写改同步 + 删除"写后重读"（理由同 AddRuneToItem：
    // 异步写未落库时重读会把内存回滚成旧数据，转移中的符文可凭空复制或丢失）
    uint32 sourceSlotCount = static_cast<uint32>(sourceSlotIds.size());
    CharacterDatabase.DirectExecute(ModuleOldFormat("UPDATE 符文系统_数据 SET 符文物品ID = '%s', 插槽数量 = %s WHERE 物品GUID = %s", sourceStorage->runeIds, sourceSlotCount, sourceGuid).c_str());

    // 目标物品使用INSERT ON DUPLICATE KEY UPDATE，防止记录不存在
    uint32 targetSlotCount = static_cast<uint32>(targetSlotIds.size());
    CharacterDatabase.DirectExecute(ModuleOldFormat("INSERT INTO 符文系统_数据 (物品GUID, 角色GUID, 物品ID, 插槽数量, 插槽ID, 符文物品ID) "
        "VALUES (%s, %s, %s, %s, '%s', '%s') "
        "ON DUPLICATE KEY UPDATE 插槽ID = VALUES(插槽ID), 符文物品ID = VALUES(符文物品ID), 插槽数量 = VALUES(插槽数量)", targetGuid, targetStorage->characterGuid, targetStorage->itemId, targetSlotCount, targetStorage->slotIds, targetStorage->runeIds).c_str());

    RuneCombat::InvalidateSpellBonusCache(player->GetGUID().GetCounter());
    return true;
}

// ApplyRuneEffects已移至RuneApplyManager
void RuneSystem::ApplyRuneEffects(Player* /*player*/)
{
    // 此方法已废弃，请使用RuneApplyManager::ApplyAllPlayerRunes
    // 实际效果应用逻辑已在RuneApplyManager中实现
}

// RemoveRuneEffects已移至RuneApplyManager
void RuneSystem::RemoveRuneEffects(Player* /*player*/)
{
    // 此方法已废弃，请使用RuneApplyManager::RemoveAllPlayerRunes
    // 实际效果移除逻辑已在RuneApplyManager中实现
}

// SaveRuneStorage - 最简化方案不需要此方法
void RuneSystem::SaveRuneStorage(const CustomRuneStorage& /*storage*/)
{
    // 此方法已废弃，数据保存由RuneEquipHandler处理
}

// DeleteRuneStorage - 最简化方案不需要此方法
void RuneSystem::DeleteRuneStorage(uint32 /*itemGuid*/, uint32 /*slotId*/)
{
    // 此方法已废弃，数据删除由RuneEquipHandler处理
}

// 【性能优化】加载符文效果缓存（服务器启动时执行一次）
void RuneSystem::LoadRuneEffectCache()
{
    _runeEffectCache.clear();
    _cachedRuneEffectMessages.clear();

    try
    {
        QueryResult result = WorldDatabase.Query(
            "SELECT `物品ID`, `效果描述` FROM `_模板_物品符文` "
            "WHERE `物品ID` > 0 AND `效果描述` IS NOT NULL AND `效果描述` != ''"
        );

        if (!result)
        {
            LOG_WARN("server.loading", "符文系统：未找到符文效果数据");
            _effectCacheInitialized = true;
            return;
        }

        std::ostringstream currentBatch;
        currentBatch << "[RUNEITEMEFFECT]";
        bool first = true;
        const size_t MAX_BATCH_SIZE = 200; // 留出安全余量（255字节限制）
        uint32 runeCount = 0;

        do
        {
            Field* fields = result->Fetch();
            uint32 itemId = fields[0].Get<uint32>();
            std::string effectDesc = fields[1].Get<std::string>();

            if (effectDesc.empty())
                continue;

            // 存储到map缓存
            _runeEffectCache[itemId] = effectDesc;

            // 构建单条记录
            std::ostringstream tempEntry;
            if (!first)
                tempEntry << "|";
            tempEntry << itemId << ":" << effectDesc;

            std::string tempStr = tempEntry.str();

            // 检查是否会超过限制
            if (currentBatch.str().length() + tempStr.length() > MAX_BATCH_SIZE)
            {
                // 保存当前批次
                _cachedRuneEffectMessages.push_back(currentBatch.str());

                // 重置为新批次
                currentBatch.str("");
                currentBatch.clear();
                currentBatch << "[RUNEITEMEFFECT]";
                first = true;

                // 添加当前记录（不需要分隔符）
                currentBatch << itemId << ":" << effectDesc;
                first = false;
            }
            else
            {
                // 添加到当前批次
                currentBatch << tempStr;
                first = false;
            }

            runeCount++;

        } while (result->NextRow());

        // 保存最后一批
        if (currentBatch.str() != "[RUNEITEMEFFECT]")
        {
            _cachedRuneEffectMessages.push_back(currentBatch.str());
        }

        _effectCacheInitialized = true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("server.loading", ModuleOldFormat("符文系统：加载符文效果缓存时发生错误: %s", e.what()).c_str());
        _effectCacheInitialized = false;
    }
}

// Addon"符文查询效果"应答直接复用预构建批次，避免每次请求全表查询
std::vector<std::string> const& RuneSystem::GetCachedRuneEffectMessages()
{
    if (!_effectCacheInitialized)
        LoadRuneEffectCache();

    return _cachedRuneEffectMessages;
}

// 【性能优化】发送符文效果数据到玩家（使用缓存，无需查询数据库）
void RuneSystem::SendRuneEffectsToPlayer(Player* player)
{
    if (!player || !player->IsInWorld())
        return;

    // 如果缓存未初始化，先加载缓存
    if (!_effectCacheInitialized)
    {
        LoadRuneEffectCache();
    }

    // 如果仍然没有数据，直接返回
    if (_cachedRuneEffectMessages.empty())
    {
        LOG_DEBUG("module.runesystem", "SendRuneEffectsToPlayer: 无符文效果数据需要发送");
        return;
    }

    ChatHandler handler(player->GetSession());

    // 发送所有预构建的批次消息
    for (size_t i = 0; i < _cachedRuneEffectMessages.size(); ++i)
    {
        handler.PSendSysMessage(_cachedRuneEffectMessages[i].c_str());
        LOG_DEBUG("module.runesystem", ModuleOldFormat("SendRuneEffectsToPlayer: 发送批次 %s/%s, 大小: %s 字节", i + 1, _cachedRuneEffectMessages.size(), _cachedRuneEffectMessages[i].length()).c_str());
    }

    LOG_DEBUG("module.runesystem", ModuleOldFormat("SendRuneEffectsToPlayer: 玩家 %s 符文效果数据发送完成，共 %s 批", player->GetName(), _cachedRuneEffectMessages.size()).c_str());
}
