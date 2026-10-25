#ifndef MOD_ITEM_SKILLS_MANAGER_H
#define MOD_ITEM_SKILLS_MANAGER_H

#include "Common.h"
#include "Player.h"
#include "Item.h"
#include "SharedDefines.h"
#include "DatabaseEnv.h"
#include "ItemSkillsDBHelper.h"  // 必须包含，以使用ItemSkillData
#include <map>
#include <vector>
#include <string>
#include <unordered_map>

// 技能触发类型
enum ItemSkillTriggerType
{
    ITEM_SKILL_TRIGGER_ON_USE      = 0,  // 使用触发
    ITEM_SKILL_TRIGGER_ON_EQUIP    = 1,  // 装备触发
    ITEM_SKILL_TRIGGER_ON_HIT      = 2,  // 击中触发
    ITEM_SKILL_TRIGGER_LEARN_SPELL = 100 // 学习技能
};

// 技能触发类型枚举已足够，不需要额外的升级失败枚举

// 物品技能模板结构
struct ItemSkillTemplate
{
    uint32 id;                           // 技能模板唯一ID
    uint32 itemId;                       // 物品模板ID
    std::string comment;                 // 技能的中文描述
    std::string clientDisplay;           // 客户端显示的技能描述
    uint32 group;                        // 技能组ID
    uint32 level;                        // 技能在组内的等级
    uint32 spellId;                      // 游戏中实际的技能ID
    float chance;                        // 选择权重（几率）：同组内多个技能时的选择权重
    uint8 triggerType;                   // 技能触发类型
    float triggerChance;                 // 触发几率：0-100，表示技能触发的百分比概率
    int16 charges;                       // 技能使用次数
    float ppmRate;                       // 技能每分钟触发次数
    uint8 cooldownMode;                  // 技能冷却时间模式：0=官方冷却，1=无冷却，2=自定义冷却值
    int32 cooldownValue;                 // 技能冷却值，单位毫秒（1000=1秒），仅当cooldownMode=2时生效
    uint8 costMode;                      // 技能消耗模式：0=官方消耗（法力/能量等），1=无消耗
    uint8 castMode;                      // 技能施法时间模式：0=官方施法时间，1=无施法时间（瞬发）
    uint8 powerMode;                     // 技能伤害/治疗强化模式：0=官方，1=固定值增幅，2=百分比增幅
    int32 powerValue;                    // 技能伤害/治疗增幅值：当powerMode=1时为固定附加值，当powerMode=2时为百分比（10=110%）
    uint8 skillType;                     // 技能类型：0=不修改，1=buff（修改持续时间）
    int32 skillDuration;                 // 技能持续时间，单位分钟，仅当skillType=1时生效
    bool enabled;                        // 启用状态：true=启用，false=禁用
    int32 priority;                      // 技能优先级，数值越大优先级越高
};




// 物品技能管理器类
class ItemSkillsManager
{
public:
    static ItemSkillsManager* instance();

    // 初始化
    void Initialize();

    // 加载数据
    uint32 LoadItemSkillTemplates();

    // 获取技能模板
    ItemSkillTemplate const* GetSkillTemplate(uint32 templateId) const;

    // 获取物品关联的技能模板（所有技能）
    std::vector<ItemSkillTemplate const*> GetItemSkillTemplates(uint32 itemId) const;

    // 从同组技能中按权重随机选择一个（用于物品初始化）
    ItemSkillTemplate const* SelectSkillByWeight(uint32 itemId, uint32 group) const;

    // 获取物品的有效技能（已启用的）
    std::vector<ItemSkillTemplate const*> GetEnabledItemSkills(uint32 itemId) const;

    // 获取所有技能模板
    const std::unordered_map<uint32, ItemSkillTemplate>& GetAllSkillTemplates() const { return _skillTemplates; }

    // ===== 新增：玩家数据管理方法 =====

    // 加载玩家的物品技能配置（玩家上线时调用，仅装备栏）
    void LoadPlayerItemSkills(Player* player);

    // 按需加载单个物品的技能数据（装备时调用）
    void LoadItemSkillsOnDemand(Player* player, Item* item);

    // 【修复】刷新单个物品的技能数据到内存（添加/移除技能后调用）
    void RefreshItemSkillsInMemory(uint64 playerGuid, Item* item);

    // 清理玩家数据（玩家下线时调用）
    void ClearPlayerData(uint64 playerGuid);

    // 从内存获取物品的技能数据
    std::vector<ItemSkillData> GetItemSkillsFromMemory(uint64 playerGuid, uint64 itemGuid);

    // 注册外部虚拟物品的技能数据（如图鉴、套装等）
    void SetExternalItemSkills(uint64 playerGuid, uint64 itemGuid, uint32 itemId, std::vector<uint32> const& skillTemplateIds);

    // 移除外部虚拟物品的技能数据
    void RemoveExternalItemSkills(uint64 playerGuid, uint64 itemGuid);

    // 获取玩家注册的外部虚拟物品技能
    std::map<uint64, std::vector<ItemSkillData>> const* GetExternalItemSkills(uint64 playerGuid) const;

    // 检查是否有其他物品仍然提供相同技能
    bool HasOtherItemProvidingSpell(uint64 playerGuid, uint64 excludedItemGuid, uint32 spellId, uint8 triggerType) const;

    // 检查技能冷却（内存）
    bool CheckSkillCooldown(uint64 playerGuid, uint64 itemGuid, uint32 skillId, uint32 cooldownMs);

    // 设置技能冷却（内存）
    void SetSkillCooldown(uint64 playerGuid, uint64 itemGuid, uint32 skillId);

    // 检查技能使用次数（内存）
    bool HasSkillCharge(uint64 playerGuid, uint64 itemGuid, uint32 skillId);

    // 减少使用次数（内存）
    void ConsumeSkillCharge(uint64 playerGuid, uint64 itemGuid, uint32 skillId);

    // 初始化物品的技能使用次数（从模板读取）
    void InitializeSkillCharges(uint64 playerGuid, uint64 itemGuid, const std::vector<ItemSkillData>& skills);

private:
    // 单例实例
    static ItemSkillsManager* _instance;

    // 数据存储
    std::unordered_map<uint32, ItemSkillTemplate> _skillTemplates;                  // 技能模板 [模板ID] = 模板
    std::unordered_map<uint32, std::vector<uint32>> _itemSkillMap;                  // 物品技能映射 [物品ID] = [技能模板ID列表]

    // ===== 新增：玩家内存数据（上线加载，下线清理）=====

    // 玩家物品技能映射 [玩家GUID][物品GUID] = 技能数据列表
    std::map<uint64, std::map<uint64, std::vector<ItemSkillData>>> _playerItemSkills;

    // 外部虚拟物品技能映射 [玩家GUID][物品GUID] = 技能数据列表
    std::map<uint64, std::map<uint64, std::vector<ItemSkillData>>> _externalItemSkills;

    // 技能冷却 [玩家GUID][物品GUID][技能ID] = 上次触发时间
    std::map<uint64, std::map<uint64, std::map<uint32, uint64>>> _skillCooldowns;

    // 技能使用次数 [玩家GUID][物品GUID][技能ID] = 剩余次数
    std::map<uint64, std::map<uint64, std::map<uint32, int32>>> _skillCharges;

    // ===== 配置项 =====
    float _chanceModifier;           // 几率修正
    float _cooldownModifier;         // 冷却修正
    float _ppmModifier;              // PPM修正
    float _chargesModifier;          // 使用次数修正
    bool _enableRandomGeneration;    // 启用随机生成
    uint32 _minGMRankForCommands;    // 命令所需最低GM等级

public:
    // 获取配置值
    float GetChanceModifier() const { return _chanceModifier; }
    float GetCooldownModifier() const { return _cooldownModifier; }
    float GetPPMModifier() const { return _ppmModifier; }
    float GetChargesModifier() const { return _chargesModifier; }
    bool IsRandomGenerationEnabled() const { return _enableRandomGeneration; }
    uint32 GetMinGMRankForCommands() const { return _minGMRankForCommands; }
};

#define sItemSkillsManager ItemSkillsManager::instance()

#endif // MOD_ITEM_SKILLS_MANAGER_H
