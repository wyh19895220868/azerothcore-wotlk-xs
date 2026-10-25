#ifndef _SPECIALIZATION_MGR_H_
#define _SPECIALIZATION_MGR_H_

#include "Common.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Logging/Log.h"
#include <map>
#include <vector>
#include <unordered_map>

// 专精系统数据结构
struct SpecializationEntry
{
    uint32 SpecID;                // 专精ID
    std::string Name;             // 专精名称
    std::vector<uint32> SkillIDs; // 技能ID列表
    int32 ExtraDamage;            // 附加伤害
    uint8 AttributeType;          // 属性类型
    int32 AttributeValue;         // 属性值

    // 外观相关
    uint32 IconID;                // 专精图标ID
    std::string Description;      // 专精描述
    uint32 VisualID;              // 专精激活时的视觉特效ID

    // 使用限制
    uint8 LevelRequirement;       // 等级要求
    uint32 ClassRequirement;      // 职业要求(位掩码)
    uint32 RaceRequirement;       // 种族要求(位掩码)

    // 技能机制
    uint32 Cooldown;              // 专精技能冷却时间(毫秒)
    uint32 Duration;              // 专精效果持续时间(毫秒)
    uint16 ProcChance;            // 专精效果触发几率(百分比)
    uint8 MaxStacks;              // 专精效果最大叠加层数

    // 资源相关
    uint8 CostType;               // 使用专精的消耗类型(0无,1法力,2怒气等)
    uint32 CostValue;             // 使用专精的消耗值

    // 附加效果
    uint32 ExtraEffectID;         // 附加效果ID(如光环、变形等)
    int32 ExtraEffectValue;       // 附加效果值
    uint32 AuraID;                // 专精提供的状态效果ID

    // 高级属性
    uint8 DamageType;             // 专精增强的伤害类型(物理、火焰、冰霜等)
    float DamagePercent;          // 专精提供的伤害百分比加成
    float CritChance;             // 专精提供的暴击几率加成
    float CritDamage;             // 专精提供的暴击伤害加成

    // 目标相关
    uint8 TargetType;             // 专精效果的目标类型(自身、友方、敌方等)
    uint8 TargetCount;            // 专精效果可影响的目标数量
    float TargetRange;            // 专精效果的作用范围(码)
};

// 玩家专精数据
struct PlayerSpecialization
{
    uint32 SpecID;                // 激活的专精ID
    uint32 ActivationTime;        // 激活时间
    bool IsActive;                // 是否激活
};

// 专精系统管理类
class SpecializationMgr
{
public:
    static SpecializationMgr* instance();

    // 初始化
    void Initialize();

    // 加载专精数据
    void LoadSpecializations();

    // 获取专精信息
    SpecializationEntry const* GetSpecialization(uint32 specID) const;

    // 获取所有专精
    std::map<uint32, SpecializationEntry> const& GetAllSpecializations() const { return _specializations; }

    // 检查玩家是否满足专精要求
    bool CheckRequirements(Player* player, SpecializationEntry const* spec) const;

    // 激活专精
    bool ActivateSpecialization(Player* player, uint32 specID);

    // 停用专精
    bool DeactivateSpecialization(Player* player, uint32 specID);

    // 获取玩家激活的专精
    std::vector<uint32> GetPlayerActiveSpecializations(ObjectGuid playerGuid) const;

    // 应用专精效果
    void ApplySpecializationEffects(Player* player, uint32 specID, bool apply);

    // 专精系统不需要数据库存储

    // 加载玩家专精数据
    void LoadPlayerSpecializations();

    // 获取配置
    bool IsEnabled() const { return _enabled; }
    uint32 GetMaxActiveSpecializations() const { return _maxActiveSpecs; }
    uint32 GetCooldown() const { return _cooldown; }
    bool IsDebugMode() const { return _debugMode; }

private:
    SpecializationMgr();
    ~SpecializationMgr();

    // 解析技能ID字符串
    std::vector<uint32> ParseSkillIDs(std::string const& skillIDsStr);

    // 专精数据
    std::map<uint32, SpecializationEntry> _specializations;

    // 玩家专精数据 <玩家GUID, <专精ID, 专精数据>>
    std::unordered_map<ObjectGuid, std::map<uint32, PlayerSpecialization>> _playerSpecializations;

    // 配置
    bool _enabled;
    uint32 _maxActiveSpecs;
    uint32 _cooldown;
    bool _debugMode;
};

#define sSpecializationMgr SpecializationMgr::instance()

#endif // _SPECIALIZATION_MGR_H_
