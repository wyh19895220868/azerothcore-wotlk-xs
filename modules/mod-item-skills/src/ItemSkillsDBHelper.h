#ifndef MOD_ITEM_SKILLS_DBHELPER_H
#define MOD_ITEM_SKILLS_DBHELPER_H

#include "Common.h"
#include "Item.h"
#include "DatabaseEnv.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <ctime>

// 物品技能数据结构（对应新表结构）
struct ItemSkillData
{
    uint32 itemId;           // 物品ID
    uint64 itemGuid;         // 物品GUID
    uint32 skillTemplateId;  // 技能模板ID
    uint32 skillId;          // 技能ID（法术ID）
    uint8 triggerType;       // 技能触发类型
    float triggerChance;     // 触发几率
};

// 物品技能数据库辅助类
class ItemSkillsDBHelper
{
public:
    static ItemSkillsDBHelper* instance();

    // 查询物品的所有技能数据（新方法，返回完整信息）
    std::vector<ItemSkillData> GetItemSkillsData(uint64 itemGuid);
    std::vector<ItemSkillData> GetItemSkillsData(Item* item);

    // 批量查询多个物品的技能数据（性能优化）
    std::map<uint64, std::vector<ItemSkillData>> GetItemSkillsDataBatch(const std::vector<uint64>& itemGuids);

    // 添加技能到物品（从模板读取并复制关键字段）
    void AddSkillToItem(Item* item, uint32 skillTemplateId);

    // 从物品移除技能
    void RemoveSkillFromItem(Item* item, uint32 skillTemplateId);

    // 清除物品的所有技能
    void ClearItemSkills(Item* item);

    // 检查物品是否有技能数据
    bool HasItemSkills(Item* item);

    // 清除缓存（在技能数据被修改时调用）
    void ClearCache(uint64 itemGuid);

private:
    ItemSkillsDBHelper() = default;

    // 技能数据缓存
    struct SkillsCache {
        std::vector<ItemSkillData> skills;
        time_t cacheTime;
    };
    std::unordered_map<uint64, SkillsCache> _skillsCache;
    const uint32 CACHE_EXPIRE_TIME = 900;  // 【性能优化】15分钟过期（原来是5分钟）
};

#define sItemSkillsDBHelper ItemSkillsDBHelper::instance()

#endif // MOD_ITEM_SKILLS_DBHELPER_H
