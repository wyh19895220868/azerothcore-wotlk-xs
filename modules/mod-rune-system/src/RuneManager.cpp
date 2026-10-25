#include "RuneManager.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "RuneUtils.h"
#include "RuneSystem.h"
#include "Item.h"
#include <sstream>
#include "ModuleFormat.h"

RuneManager* RuneManager::_instance = nullptr;

RuneManager* RuneManager::instance()
{
    if (!_instance)
    {
        _instance = new RuneManager();
    }
    return _instance;
}

void RuneManager::Initialize()
{
    // 初始化逻辑
}

uint32 RuneManager::CreateRune(const std::string& name, uint32 runeSlot, uint32 runeQuality,
                              uint32 spellId, uint32 cooldown, uint32 procChance, uint32 effectType)
{
    // 生成新的符文ID
    uint32 newRuneId = 0;

    QueryResult result = WorldDatabase.Query("SELECT MAX(id) FROM _模板_物品符文");
    if (result)
    {
        newRuneId = (*result)[0].Get<uint32>() + 1;
    }
    else
    {
        newRuneId = 1;
    }

    // 使用参数化查询防止SQL注入
    WorldDatabase.Execute(ModuleOldFormat("INSERT INTO _模板_物品符文 (id, 物品ID, 组, 名称, 符文槽位, 符文品质, 法术ID, 冷却时间, 触发几率, 效果类型, 效果值, 效果描述) VALUES (%s, 0, 0, '%s', %s, %s, %s, %s, %s, %s, '', '默认效果')", newRuneId, name, runeSlot, runeQuality, spellId, cooldown, procChance, effectType).c_str());

    // 重新加载符文数据
    sRuneSystem->LoadRunesFromDB();

    return newRuneId;
}

bool RuneManager::ModifyRune(uint32 runeId, const std::string& name, uint32 runeSlot, uint32 runeQuality,
                            uint32 spellId, uint32 cooldown, uint32 procChance, uint32 effectType)
{
    // 检查符文是否存在
    CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfo(runeId);
    if (!runeInfo)
    {
        return false;
    }

    // 使用参数化查询防止SQL注入
    WorldDatabase.Execute(ModuleOldFormat("UPDATE _模板_物品符文 SET 名称 = '%s', 符文槽位 = %s, 符文品质 = %s, 法术ID = %s, 冷却时间 = %s, 触发几率 = %s, 效果类型 = %s WHERE id = %s", name, runeSlot, runeQuality, spellId, cooldown, procChance, effectType, runeId).c_str());

    // 重新加载符文数据
    sRuneSystem->LoadRunesFromDB();

    return true;
}

bool RuneManager::DeleteRune(uint32 runeId)
{
    // 检查符文是否存在
    CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfo(runeId);
    if (!runeInfo)
    {
        return false;
    }

    // 构建SQL查询
    std::string query = "DELETE FROM _模板_物品符文 WHERE id = " + std::to_string(runeId);
    WorldDatabase.Execute(query.c_str());

    // 重新加载符文数据
    sRuneSystem->LoadRunesFromDB();

    return true;
}

bool RuneManager::AddRuneSlotToItem(Item* item, uint32 slotCount)
{
    if (!item || slotCount == 0 || slotCount > 20)
    {
        return false;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 characterGuid = item->GetOwnerGUID().GetCounter();
    uint32 itemId = item->GetEntry();

    // 检查是否已经有插槽记录
    std::string checkQuery = "SELECT ID, 插槽ID, 符文物品ID FROM 符文系统_数据 WHERE 物品GUID = " + std::to_string(itemGuid);
    QueryResult result = CharacterDatabase.Query(checkQuery.c_str());

    if (result)
    {
        // 已经有记录，不能重复添加
        return false;
    }

    // 创建插槽ID列表 (1,2,3,...,slotCount)
    std::string slotIds = RuneUtils::CreateSlotIdList(slotCount);
    // 创建空的符文ID列表 (0,0,0,...)
    std::string runeIds = RuneUtils::CreateEmptyRuneIdList(slotCount);

    // 使用 INSERT ... ON DUPLICATE KEY UPDATE 避免重复键错误
    std::ostringstream query;
    query << "INSERT INTO 符文系统_数据 (角色GUID, 物品ID, 物品GUID, 插槽数量, 插槽ID, 符文物品ID) VALUES ("
          << characterGuid << ", "
          << itemId << ", "
          << itemGuid << ", "
          << slotCount << ", '"
          << slotIds << "', '"
          << runeIds << "') "
          << "ON DUPLICATE KEY UPDATE 插槽数量 = VALUES(插槽数量), 插槽ID = VALUES(插槽ID), 符文物品ID = VALUES(符文物品ID)";

    // 【数据丢失修复】改同步写：下一行同步重读本表刷新内存缓存，
    // 异步写未落库时重读为空 → 开槽"成功"但内存无插槽
    CharacterDatabase.DirectExecute(query.str().c_str());

    // 重新加载物品符文插槽数据
    sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);

    LOG_INFO("rune.manager", ModuleOldFormat("为物品 %s 添加了 %s 个符文插槽", itemGuid, slotCount).c_str());

    return true;
}

bool RuneManager::UnlockRuneSlot(Item* item, uint32 slotId)
{
    // 最简化方案不需要解锁插槽，开槽后即可使用
    return true;
}

uint32 RuneManager::GetItemRuneSlotCount(Item* item)
{
    if (!item)
        return 0;

    uint32 itemGuid = item->GetGUID().GetCounter();
    const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);

    if (!storage)
        return 0;

    // 解析插槽ID列表
    auto slots = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
    return static_cast<uint32>(slots.size());
}

uint32 RuneManager::GetItemUnlockedRuneSlotCount(Item* item)
{
    // 最简化方案：所有开启的槽位都是解锁的
    return GetItemRuneSlotCount(item);
}

bool RuneManager::CanAddRuneToItem(Item* item)
{
    if (!item)
    {
        return false;
    }

    // 检查物品类型是否支持符文
    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
    {
        return false;
    }

    // 只允许武器和装备添加符文
    if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
    {
        return false;
    }

    // 检查是否有可用的符文插槽
    uint32 itemGuid = item->GetGUID().GetCounter();
    const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);

    if (!storage)
        return false;

    // 解析符文ID列表，检查是否有空槽
    auto runeIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);
    for (uint32 runeId : runeIds)
    {
        if (runeId == 0)
            return true; // 有空插槽
    }

    return false;
}

std::vector<CustomRuneInfo> RuneManager::GetRuneList(uint32 runeSlot, uint32 runeQuality)
{
    std::vector<CustomRuneInfo> result;

    // 构建查询条件
    std::ostringstream whereClause;
    bool hasCondition = false;

    if (runeSlot > 0)
    {
        whereClause << "符文槽位 = " << runeSlot;
        hasCondition = true;
    }

    if (runeQuality > 0)
    {
        if (hasCondition)
        {
            whereClause << " AND ";
        }
        whereClause << "符文品质 = " << runeQuality;
        hasCondition = true;
    }

    // 构建完整查询
    std::string query = "SELECT * FROM _模板_物品符文";
    if (hasCondition)
    {
        query += " WHERE " + whereClause.str();
    }

    // 执行查询
    QueryResult queryResult = WorldDatabase.Query(query.c_str());

    if (!queryResult)
    {
        return result;
    }

    // 处理查询结果
    do
    {
        Field* fields = queryResult->Fetch();

        CustomRuneInfo rune;
        uint32 index = 0;

        rune.runeId = fields[index++].Get<uint32>();
        rune.itemId = fields[index++].Get<uint32>();
        index++; // 跳过组字段
        rune.name = fields[index++].Get<std::string>();
        rune.runeSlot = fields[index++].Get<uint32>();
        rune.runeQuality = fields[index++].Get<uint32>();
        rune.spellId = fields[index++].Get<uint32>();
        rune.cooldown = fields[index++].Get<uint32>();
        rune.procChance = fields[index++].Get<uint32>();
        rune.effectType = fields[index++].Get<uint32>();
        rune.effectValues = fields[index++].Get<std::string>();
        rune.effectDescription = fields[index++].Get<std::string>();

        result.push_back(std::move(rune));

    } while (queryResult->NextRow());

    return result;
}
