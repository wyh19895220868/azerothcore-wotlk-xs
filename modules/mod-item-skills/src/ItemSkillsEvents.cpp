#include "ItemSkillsEvents.h"
#include "ItemSkillsEffects.h"
#include "ItemSkillsDBHelper.h"
#include "ItemSkillsManager.h"
#include "Logging/Log.h"
#include "Spell.h"
#include "SpellDefines.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Bag.h"
#include "ObjectGuid.h"
#include "EventProcessor.h"
#include "Chat.h"
#include "Timer.h"
#include "WorldSession.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include <limits>
#include "ModuleFormat.h"

// 外部函数声明（定义在ItemSkillsCommands.cpp）
extern void ForceClientRefreshItemTemplate(Player* player, uint32 itemId, Item* item);

// 延迟应用装备触发技能的事件（分批处理，避免一次性施放大量技能）
class ApplyEquipSkillsEvent : public BasicEvent
{
public:
    ApplyEquipSkillsEvent(Player* player, uint8 startSlot = EQUIPMENT_SLOT_START)
        : _player(player), _currentSlot(startSlot) {}

    bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) override
    {
        if (!sHelper->GetBoolean(22011))
            return true;

        if (!_player || !_player->IsInWorld() || !_player->GetSession())
            return true;

        uint64 playerGuid = _player->GetGUID().GetCounter();
        uint32 processedCount = 0;
        const uint32 MAX_PER_BATCH = 3;  // 每批最多处理3个槽位

        // 分批处理装备槽位
        while (_currentSlot < EQUIPMENT_SLOT_END && processedCount < MAX_PER_BATCH)
        {
            if (Item* item = _player->GetItemByPos(INVENTORY_SLOT_BAG_0, _currentSlot))
            {
                uint64 itemGuid = item->GetGUID().GetCounter();
                auto skills = sItemSkillsManager->GetItemSkillsFromMemory(playerGuid, itemGuid);

                for (const auto& skillData : skills)
                {
                    if (skillData.triggerType == ITEM_SKILL_TRIGGER_ON_EQUIP)
                    {
                        _player->CastSpell(_player, skillData.skillId, true);
                    }
                    else if (skillData.triggerType == ITEM_SKILL_TRIGGER_LEARN_SPELL)
                    {
                        if (!_player->HasSpell(skillData.skillId))
                        {
                            _player->learnSpell(skillData.skillId);
                        }
                    }
                }
            }
            _currentSlot++;
            processedCount++;
        }

        // 如果还有剩余槽位，安排下一批
        if (_currentSlot < EQUIPMENT_SLOT_END)
        {
            _player->m_Events.AddEvent(
                new ApplyEquipSkillsEvent(_player, _currentSlot),
                _player->m_Events.CalculateTime(100)  // 100ms后处理下一批
            );
        }

        return true;
    }

private:
    Player* _player;
    uint8 _currentSlot;
};

// 延迟发送物品技能的事件
class SendItemSkillsEvent : public BasicEvent
{
public:
    SendItemSkillsEvent(Player* player) : _player(player) {}

    bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) override
    {
        // 检查玩家是否仍在线
        if (!sHelper->GetBoolean(22011))
            return true;

        if (!_player || !_player->IsInWorld() || !_player->GetSession())
            return true;

        uint64 playerGuid = _player->GetGUID().GetCounter();
        int itemCount = 0;

        // 只遍历装备栏（背包物品按需处理）
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (Item* item = _player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                uint64 itemGuid = item->GetGUID().GetCounter();

                // 从内存获取技能数据
                std::vector<ItemSkillData> skills = sItemSkillsManager->GetItemSkillsFromMemory(playerGuid, itemGuid);

                if (!skills.empty())
                {
                    // 检查是否有使用触发技能
                    bool hasUseSkill = false;
                    for (const auto& skillData : skills)
                    {
                        if (skillData.triggerType == 0)  // 使用触发
                        {
                            hasUseSkill = true;
                            break;
                        }
                    }

                    if (hasUseSkill)
                    {
                        ForceClientRefreshItemTemplate(_player, item->GetEntry(), item);
                        itemCount++;
                    }
                }
            }
        }

        LOG_DEBUG("module.itemskills", ModuleOldFormat("[性能优化] 玩家 %s 登录完成，已为 %s 个装备发送模板", _player->GetName(), itemCount).c_str());

        return true;
    }

private:
    Player* _player;
};

// 静态成员初始化
std::map<uint64, std::map<uint8, uint64>> ItemSkillsEvents::_equippedItems;

// 玩家登录时加载技能配置
void ItemSkillsEvents::OnLogin(Player* player)
{
    auto loginStart = std::chrono::high_resolution_clock::now();

    if (!player || !sHelper->GetBoolean(22011))
        return;

    // 加载玩家的物品技能配置到内存（只加载数据，不施放技能）
    auto step1Start = std::chrono::high_resolution_clock::now();
    sItemSkillsManager->LoadPlayerItemSkills(player);
    auto step1End = std::chrono::high_resolution_clock::now();
    auto step1Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step1End - step1Start).count();

    // 更新击中技能缓存
    auto step2Start = std::chrono::high_resolution_clock::now();
    sItemSkillsEffects->UpdatePlayerHitSkillsCache(player);
    auto step2End = std::chrono::high_resolution_clock::now();
    auto step2Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step2End - step2Start).count();

    // 延迟500ms后分批应用装备触发技能（避免一次性施放大量技能导致卡顿）
    player->m_Events.AddEvent(new ApplyEquipSkillsEvent(player), player->m_Events.CalculateTime(500));

    // 延迟3秒后发送ItemTemplate（等待物品加载完成）
    player->m_Events.AddEvent(new SendItemSkillsEvent(player), player->m_Events.CalculateTime(3000));

    auto loginEnd = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loginEnd - loginStart).count();
}

// 玩家登出时清理内存数据
void ItemSkillsEvents::OnLogout(Player* player)
{
    if (!player)
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();

    // 移除所有装备的技能效果
    auto playerItr = _equippedItems.find(playerGuid);
    if (playerItr != _equippedItems.end())
    {
        for (const auto& slotPair : playerItr->second)
        {
            uint64 itemGuid = slotPair.second;
            sItemSkillsEffects->RemoveItemSkillEffectsByGuid(player, itemGuid);
        }
    }

    // 清理内存中的玩家数据
    sItemSkillsManager->ClearPlayerData(playerGuid);

    // 清理击中技能缓存
    sItemSkillsEffects->ClearPlayerHitSkillsCache(playerGuid);

    // 清理装备跟踪映射
    _equippedItems.erase(playerGuid);
}

// 装备物品时
void ItemSkillsEvents::OnEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update)
{
    if (!player || !item || !sHelper->GetBoolean(22011))
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();

    // 检查该槽位是否已有旧装备
    auto playerItr = _equippedItems.find(playerGuid);
    if (playerItr != _equippedItems.end())
    {
        auto slotItr = playerItr->second.find(slot);
        if (slotItr != playerItr->second.end())
        {
            uint64 oldItemGuid = slotItr->second;
            uint64 newItemGuid = item->GetGUID().GetCounter();

            // 如果是不同的物品，移除旧物品的技能
            if (oldItemGuid != newItemGuid)
            {
                sItemSkillsEffects->RemoveItemSkillEffectsByGuid(player, oldItemGuid);
            }
        }
    }

    // 记录新装备
    _equippedItems[playerGuid][slot] = item->GetGUID().GetCounter();

    // 初始化物品技能数据（如果还没有）
    InitializeItemSkills(player, item);

    // 按需加载物品技能数据到内存（从背包装备时）
    sItemSkillsManager->LoadItemSkillsOnDemand(player, item);

    // 应用装备技能效果
    ApplyEquipEffects(player, item);

    // 更新击中技能缓存（装备变化后）
    sItemSkillsEffects->UpdatePlayerHitSkillsCache(player);
}

void ItemSkillsEvents::InitializeItemSkills(Player* player, Item* item)
{
    if (!player || !item || !sHelper->GetBoolean(22011))
        return;

    // 【修复低风险问题】检查是否启用随机生成
    if (!sItemSkillsManager->IsRandomGenerationEnabled())
        return; // 随机生成已禁用

    // 检查物品是否已有技能数据
    if (sItemSkillsDBHelper->HasItemSkills(item))
        return; // 已经初始化过了

    // 获取物品模板关联的技能
    uint32 itemId = item->GetEntry();
    std::vector<ItemSkillTemplate const*> skillTemplates = sItemSkillsManager->GetItemSkillTemplates(itemId);

    if (skillTemplates.empty())
        return; // 没有关联技能

    uint32 addedCount = 0;
    for (const auto* skillTemplate : skillTemplates)
    {
        if (!skillTemplate || !skillTemplate->enabled)
            continue;

        // 使用新的AddSkillToItem方法（从模板复制关键字段）
        sItemSkillsDBHelper->AddSkillToItem(item, skillTemplate->id);
        addedCount++;
    }

    if (addedCount > 0)
    {
        LOG_INFO("module.itemskills", ModuleOldFormat("玩家 %s 的物品 %s (GUID: %s) 初始化了 %s 个技能", player->GetName(), item->GetEntry(), item->GetGUID().GetCounter(), addedCount).c_str());
    }
}

void ItemSkillsEvents::ApplyEquipEffects(Player* player, Item* item)
{
    if (!player || !item || !sHelper->GetBoolean(22011))
        return;

    // 应用装备触发类型的技能效果
    sItemSkillsEffects->ApplyItemSkillEffects(player, item);
}

void ItemSkillsEvents::RemoveEquipEffects(Player* player, Item* item)
{
    if (!player || !item || !sHelper->GetBoolean(22011))
        return;

    // 移除装备触发类型的技能效果
    sItemSkillsEffects->RemoveItemSkillEffects(player, item);
}

// 物品从背包移走时（包括卸下装备）
void ItemSkillsEvents::OnAfterMoveItemFromInventory(Player* player, Item* item, uint8 bag, uint8 slot, bool update)
{
    if (!player || !item || !sHelper->GetBoolean(22011))
        return;

    // 只处理装备栏的物品
    // bag参数在这里可能不可靠，直接检查slot是否在装备槽位范围
    if (slot < EQUIPMENT_SLOT_START || slot >= EQUIPMENT_SLOT_END)
        return;

    uint64 playerGuid = player->GetGUID().GetCounter();
    uint64 itemGuid = item->GetGUID().GetCounter();

    LOG_DEBUG("module.itemskills", ModuleOldFormat("玩家 %s 卸下装备: ItemID=%s, GUID=%s, Slot=%s", player->GetName(), item->GetEntry(), itemGuid, slot).c_str());

    // 移除装备技能效果
    RemoveEquipEffects(player, item);

    // 清除装备跟踪映射
    auto playerItr = _equippedItems.find(playerGuid);
    if (playerItr != _equippedItems.end())
    {
        playerItr->second.erase(slot);
    }

    // 更新击中技能缓存（装备变化后）
    sItemSkillsEffects->UpdatePlayerHitSkillsCache(player);
}

// ItemSkillsItemScript - CanItemUse 实现（全局拦截）
// 客户端已经收到了ItemTemplate（所以能右键使用）
// 但服务器端需要检查item_template表，如果spellid=0就不会触发
// 所以我们需要拦截并手动触发技能
bool ItemSkillsItemScript::CanItemUse(Player* player, Item* item, SpellCastTargets const& targets)
{
    if (!player || !item || !sHelper->GetBoolean(22011))
        return false; // 不拦截，允许默认处理

    uint64 playerGuid = player->GetGUID().GetCounter();
    uint64 itemGuid = item->GetGUID().GetCounter();

    // 优先从内存读取技能数据；背包物品不在内存中时读取数据库缓存
    std::vector<ItemSkillData> skills = sItemSkillsManager->GetItemSkillsFromMemory(playerGuid, itemGuid);
    if (skills.empty())
    {
        skills = sItemSkillsDBHelper->GetItemSkillsData(item);
    }

    // 收集所有使用触发技能（只收集已启用的）
    std::vector<ItemSkillData> useSkills;
    for (const auto& skillData : skills)
    {
        if (skillData.triggerType == 0) // 使用触发
        {
            // 检查技能模板是否启用
            ItemSkillTemplate const* tmpl = sItemSkillsManager->GetSkillTemplate(skillData.skillTemplateId);
            if (tmpl && tmpl->enabled)
            {
                useSkills.push_back(skillData);
            }
        }
    }

    // 如果没有使用触发技能，不拦截
    if (useSkills.empty())
        return false;

    // 找到第一个不在冷却中的技能（循环使用）
    bool hasUseSkill = false;
    uint32 useSkillId = 0;
    uint32 skillTemplateId = 0;

    for (const auto& skillData : useSkills)
    {
        // 获取技能模板检查冷却模式
        ItemSkillTemplate const* tmpl = sItemSkillsManager->GetSkillTemplate(skillData.skillTemplateId);

        // 如果是无冷却模式(cooldownMode=1)，直接使用该技能
        if (tmpl && tmpl->cooldownMode == 1)
        {
            hasUseSkill = true;
            useSkillId = skillData.skillId;
            skillTemplateId = skillData.skillTemplateId;
            break;
        }

        // 其他模式检查该技能是否在冷却中
        if (!player->HasSpellCooldown(skillData.skillId))
        {
            hasUseSkill = true;
            useSkillId = skillData.skillId;
            skillTemplateId = skillData.skillTemplateId;
            break;
        }
    }

    // 如果所有技能都在冷却中，使用第一个（让它显示冷却提示）
    if (!hasUseSkill && !useSkills.empty())
    {
        hasUseSkill = true;
        useSkillId = useSkills[0].skillId;
        skillTemplateId = useSkills[0].skillTemplateId;
    }

    if (hasUseSkill && useSkillId > 0)
    {
        // LOG_INFO("module.itemskills", "[Use] 拦截物品使用并施放技能: ItemID={}, GUID={}, 技能={}, 总技能数={}",
        //     item->GetEntry(), item->GetGUID().GetCounter(), useSkillId, useSkills.size());

        // 获取技能信息
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(useSkillId);
        if (!spellInfo)
        {
            LOG_ERROR("module.itemskills", ModuleOldFormat("[Use] 技能ID不存在: %s", useSkillId).c_str());
            player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
            return true; // 拦截，避免使用失败
        }

        // 检查是否正在施法（避免快速点击导致重复施法）
        if (player->IsNonMeleeSpellCast(false))
        {
            // LOG_INFO("module.itemskills", "[Use] 玩家正在施法中，忽略本次请求");
            player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
            return true;
        }

        // 检查技能冷却
        if (player->HasSpellCooldown(useSkillId))
        {
            // uint32 curTime = getMSTime();
            // uint32 remainingMs = player->GetSpellCooldownDelay(useSkillId);
            // auto& cooldowns = player->GetSpellCooldownMap();
            // auto itr = cooldowns.find(useSkillId);
            // uint32 cooldownEnd = (itr != cooldowns.end()) ? itr->second.end : 0;
            // LOG_INFO("module.itemskills", "[Use] 技能在冷却中: {} (当前: {}, 结束: {}, 剩余: {}ms)",
            //     useSkillId, curTime, cooldownEnd, remainingMs);

            Spell::SendCastResult(player, spellInfo, 0, SPELL_FAILED_NOT_READY);
            player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
            return true;
        }

        // 检查技能是否需要目标
        if (spellInfo->Targets & (TARGET_FLAG_UNIT | TARGET_FLAG_UNIT_ENEMY | TARGET_FLAG_UNIT_ALLY))
        {
            if (!targets.GetUnitTarget())
            {
                LOG_DEBUG("module.itemskills", ModuleOldFormat("[Use] 技能需要目标但没有选择: %s", useSkillId).c_str());
                Spell::SendCastResult(player, spellInfo, 0, SPELL_FAILED_BAD_TARGETS);
                player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
                return true;
            }
        }

        // 获取技能模板配置（用于强化）
        ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillTemplateId);

        // 根据技能模板配置构建TriggerCastFlags
        TriggerCastFlags triggerFlags = TRIGGERED_NONE;

        bool useCustomCooldown = false;
        uint32 customCooldownMs = 0;

        if (skillTemplate)
        {
            // 1. 冷却时间强化
            if (skillTemplate->cooldownMode == 1)
            {
                // 无冷却：完全忽略冷却
                triggerFlags = TriggerCastFlags(triggerFlags | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD);
                LOG_DEBUG("module.itemskills", "[Use] 应用无冷却");
            }
            else if (skillTemplate->cooldownMode == 2 && skillTemplate->cooldownValue > 0)
            {
                // 自定义冷却：完全忽略官方冷却系统
                useCustomCooldown = true;
                customCooldownMs = skillTemplate->cooldownValue;
                triggerFlags = TriggerCastFlags(triggerFlags | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD);

                // LOG_INFO("module.itemskills", "[Use] 自定义冷却模式: cooldownMs={}, triggerFlags=0x{:X}, 包含IGNORE_CD={}",
                //     customCooldownMs, static_cast<uint32>(triggerFlags),
                //     (triggerFlags & TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD) ? "是" : "否");
            }
            else
            {
                // cooldownMode == 0: 使用官方冷却，不设置任何标志
                LOG_DEBUG("module.itemskills", "[Use] 使用官方冷却");
            }

            // 2. 消耗强化
            if (skillTemplate->costMode == 1)
            {
                // 无消耗：忽略法力和材料消耗
                triggerFlags = TriggerCastFlags(triggerFlags | TRIGGERED_IGNORE_POWER_AND_REAGENT_COST);
                LOG_DEBUG("module.itemskills", "[Use] 应用无消耗");
            }

            // 3. 施法时间强化
            if (skillTemplate->castMode == 1)
            {
                // 无施法时间：立即施放（瞬发）
                triggerFlags = TriggerCastFlags(triggerFlags | TRIGGERED_CAST_DIRECTLY);
                LOG_DEBUG("module.itemskills", "[Use] 应用瞬发");
            }
        }

        // 创建Spell对象并应用触发标志
        Spell* spell = new Spell(player, spellInfo, triggerFlags);

        // 设置CastItem（用于技能来源识别）
        spell->m_CastItem = item;
        spell->m_castItemGUID = item->GetGUID();

        // 应用技能强化（在prepare之前设置）
        if (skillTemplate)
        {
            // 1. 伤害/治疗强化（只对伤害和治疗效果生效）
            if (skillTemplate->powerMode == 1 && skillTemplate->powerValue != 0)
            {
                // 固定值增幅
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                {
                    // 只对伤害和治疗效果应用强化
                    uint32 effect = spellInfo->Effects[i].Effect;
                    if (effect != SPELL_EFFECT_SCHOOL_DAMAGE &&
                        effect != SPELL_EFFECT_WEAPON_DAMAGE &&
                        effect != SPELL_EFFECT_WEAPON_PERCENT_DAMAGE &&
                        effect != SPELL_EFFECT_NORMALIZED_WEAPON_DMG &&
                        effect != SPELL_EFFECT_HEAL)
                        continue;

                    // 获取当前基础伤害值
                    SpellValue const* spellValue = spell->GetSpellValue();
                    int32 baseValue = spellValue->EffectBasePoints[i];

                    // 应用固定增幅
                    spell->SetSpellValue(SpellValueMod(SPELLVALUE_BASE_POINT0 + i), baseValue + skillTemplate->powerValue);
                }
                LOG_DEBUG("module.itemskills", ModuleOldFormat("[Use] 应用固定值强化: +%s", skillTemplate->powerValue).c_str());
            }
            else if (skillTemplate->powerMode == 2 && skillTemplate->powerValue != 0)
            {
                // 百分比增幅（10 = 110%）
                float multiplier = 1.0f + (skillTemplate->powerValue / 100.0f);
                for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
                {
                    // 只对伤害和治疗效果应用强化
                    uint32 effect = spellInfo->Effects[i].Effect;
                    if (effect != SPELL_EFFECT_SCHOOL_DAMAGE &&
                        effect != SPELL_EFFECT_WEAPON_DAMAGE &&
                        effect != SPELL_EFFECT_WEAPON_PERCENT_DAMAGE &&
                        effect != SPELL_EFFECT_NORMALIZED_WEAPON_DMG &&
                        effect != SPELL_EFFECT_HEAL)
                        continue;

                    // 获取当前基础伤害值
                    SpellValue const* spellValue = spell->GetSpellValue();
                    int32 baseValue = spellValue->EffectBasePoints[i];

                    // 应用百分比增幅
                    spell->SetSpellValue(SpellValueMod(SPELLVALUE_BASE_POINT0 + i),
                        static_cast<int32>(baseValue * multiplier));
                }
                LOG_DEBUG("module.itemskills", ModuleOldFormat("[Use] 应用百分比强化: x%s%%", 100 + skillTemplate->powerValue).c_str());
            }

            // 2. Buff持续时间修改
            if (skillTemplate->skillType == 1 && skillTemplate->skillDuration > 0)
            {
                // 修改技能持续时间为指定分钟数（分钟转毫秒）
                int32 durationMs = skillTemplate->skillDuration * 60 * 1000;
                spell->SetSpellValue(SPELLVALUE_AURA_DURATION, durationMs);
                LOG_DEBUG("module.itemskills", ModuleOldFormat("[Use] 应用Buff持续时间: %s分钟 (%sms)", skillTemplate->skillDuration, durationMs).c_str());
            }
        }

        // 新方案：不在prepare前设置，让官方冷却正常添加

        // 直接使用传入的targets对象
        SpellCastResult result = spell->prepare(&targets);

        if (result != SPELL_CAST_OK)
        {
            // LOG_INFO("module.itemskills", "[Use] 施法准备失败: result={} ({})",
            //     static_cast<uint32>(result),
            //     result == SPELL_FAILED_NOT_READY ? "NOT_READY" :
            //     result == SPELL_FAILED_BAD_TARGETS ? "BAD_TARGETS" :
            //     result == SPELL_FAILED_SPELL_IN_PROGRESS ? "SPELL_IN_PROGRESS" : "其他");
            // prepare失败时Spell对象会自动delete，不需要手动delete
            // 只需要发送完成通知
            player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
            return true; // 拦截
        }

        // prepare成功后立即添加自定义冷却
        if (useCustomCooldown && customCooldownMs > 0)
        {
            // 应用冷却修正系数后添加
            uint32 effectiveCooldown = static_cast<uint32>(customCooldownMs * sItemSkillsManager->GetCooldownModifier());
            player->AddSpellCooldown(useSkillId, 0xFFFFFFFF, effectiveCooldown, true);

            // LOG_INFO("module.itemskills", "[Use] 已立即设置自定义冷却: 技能={}, 冷却={}ms",
            //     useSkillId, effectiveCooldown);
        }

        // LOG_INFO("module.itemskills", "[Use] 技能施放成功: {}", useSkillId);

        // 拦截默认处理，我们已经处理了
        return true;
    }

    // 没有使用触发技能，不拦截，允许默认处理
    return false;
}

// ItemSkillsUnitEvents - ModifyMeleeDamage 实现
// 只在玩家真正的近战攻击时触发，不会被技能伤害、DoT、反弹伤害等触发
void ItemSkillsUnitEvents::ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage)
{
    if (!attacker || !target)
        return;

    // 检查攻击者或受害者是否已死亡
    if (!attacker->IsAlive() || !target->IsAlive())
        return;

    Player* player = attacker->ToPlayer();
    if (!player)
        return;

    // 检查玩家是否在世界中（避免登出/传送等状态触发）
    if (!player->IsInWorld())
        return;

    // 触发击中触发技能（只在近战攻击时触发一次）
    sItemSkillsEffects->OnPlayerDealDamage(player, target, player->PeekAsDamageOverride(target, damage), nullptr);
}

// ItemSkillsUnitEvents - ModifySpellDamageTaken 实现
// 处理法术伤害（远程法术、魔法攻击等）
void ItemSkillsUnitEvents::ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo)
{
    if (!attacker || !target)
        return;

    // 检查攻击者或受害者是否已死亡
    if (!attacker->IsAlive() || !target->IsAlive())
        return;

    // 只处理玩家造成的伤害
    Player* player = attacker->ToPlayer();
    if (!player)
        return;

    // 检查玩家是否在世界中
    if (!player->IsInWorld())
        return;

    // 触发击中触发技能（传入spellInfo以便后续扩展判断）
    sItemSkillsEffects->OnPlayerDealDamage(player, target, player->PeekAsDamageOverride(target, uint32(std::max(0, damage))), spellInfo);
}
