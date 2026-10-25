#include "AsInt256.h"
#include "Custom/_Helper/AsInt64.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "RuneSystem.h"
#include "RuneEffectHandler.h"
#include "Item.h"
#include "GameTime.h"
#include "Util.h"
#include <algorithm>
#include <limits>
#include <map>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace
{
AsInt64 AddInt64Saturated(AsInt64 const& left, AsInt64 const& right)
{
    AsInt64 const maxValue = std::numeric_limits<AsInt64>::max();
    AsInt64 const minValue = std::numeric_limits<AsInt64>::min();

    if (right > 0 && left > maxValue - right)
        return maxValue;

    if (right < 0 && left < minValue - right)
        return minValue;

    return left + right;
}

AsInt64 ScaleInt64ByPercent(AsInt64 const& baseValue, AsInt64 const& percentValue)
{
    return MultiplyDivideAsInt256(baseValue, percentValue, 100);
}

}

// 战斗热路径的 per-player 技能符文加成缓存。
// 旧实现每次法术伤害/治疗对 19 个装备槽做字符串解析 + O(N) 符文查找，且固定/百分比各跑一遍；
// 现在每玩家每 TTL 周期只汇总一次，之后纯内存读。符文镶嵌/卸载/转移走 InvalidateSpellBonusCache
// 立即失效，装备穿脱由 TTL 周期刷新。
namespace RuneCombat
{
namespace
{
    constexpr uint32 SPELL_BONUS_CACHE_TTL_MS = 2000;
    constexpr size_t SPELL_BONUS_CACHE_SWEEP_THRESHOLD = 512;

    struct PlayerRuneSpellBonus
    {
        AsInt64 fixedAll = 0;    // 通配（符文未指定法术）固定加成总和
        AsInt64 percentAll = 0;  // 通配百分比加成总和
        std::unordered_map<uint32, std::pair<AsInt64, AsInt64>> perSpell; // spellId -> (固定, 百分比)
        uint32 expireMs = 0;
    };

    std::mutex _spellBonusCacheMutex;
    std::unordered_map<uint32, PlayerRuneSpellBonus> _spellBonusCache;

    PlayerRuneSpellBonus BuildSpellBonus(Player* player)
    {
        PlayerRuneSpellBonus bonus;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            CustomRuneStorage const* storage = sRuneSystem->GetItemRuneStorage(item->GetGUID().GetCounter());
            if (!storage)
                continue;

            auto runeItemIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);
            for (uint32 runeItemId : runeItemIds)
            {
                if (runeItemId == 0)
                    continue;

                CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeItemId);
                if (!runeInfo)
                    continue;

                // 效果类型2 = 技能固定加成, 4 = 技能百分比加成
                if (runeInfo->effectType != 2 && runeInfo->effectType != 4)
                    continue;

                auto values = RuneUtils::ParseEffectValues(runeInfo->effectValues);
                if (values.empty() || values[0].first != 99)
                    continue;

                bool const isPercent = (runeInfo->effectType == 4);
                if (runeInfo->spellId == 0)
                {
                    AsInt64& target = isPercent ? bonus.percentAll : bonus.fixedAll;
                    target = AddInt64Saturated(target, values[0].second);
                }
                else
                {
                    auto& entry = bonus.perSpell[runeInfo->spellId];
                    AsInt64& target = isPercent ? entry.second : entry.first;
                    target = AddInt64Saturated(target, values[0].second);
                }
            }
        }
        return bonus;
    }

    // 返回该法术的 (固定加成总和, 百分比总和)
    std::pair<AsInt64, AsInt64> GetSpellBonus(Player* player, uint32 spellId)
    {
        uint32 const guid = player->GetGUID().GetCounter();
        uint32 const now = getMSTime();

        auto readBonus = [spellId](PlayerRuneSpellBonus const& bonus)
        {
            AsInt64 fixed = bonus.fixedAll;
            AsInt64 percent = bonus.percentAll;
            auto spellItr = bonus.perSpell.find(spellId);
            if (spellItr != bonus.perSpell.end())
            {
                fixed = AddInt64Saturated(fixed, spellItr->second.first);
                percent = AddInt64Saturated(percent, spellItr->second.second);
            }
            return std::make_pair(fixed, percent);
        };

        {
            std::lock_guard<std::mutex> lock(_spellBonusCacheMutex);
            auto itr = _spellBonusCache.find(guid);
            if (itr != _spellBonusCache.end() && now < itr->second.expireMs)
                return readBonus(itr->second);

            if (_spellBonusCache.size() >= SPELL_BONUS_CACHE_SWEEP_THRESHOLD)
            {
                for (auto sweepItr = _spellBonusCache.begin(); sweepItr != _spellBonusCache.end();)
                {
                    if (now >= sweepItr->second.expireMs)
                        sweepItr = _spellBonusCache.erase(sweepItr);
                    else
                        ++sweepItr;
                }
            }
        }

        // 装备扫描和字符串解析不持有全局锁，避免一个玩家缓存过期时阻塞其他地图线程。
        PlayerRuneSpellBonus rebuilt = BuildSpellBonus(player);
        rebuilt.expireMs = now + SPELL_BONUS_CACHE_TTL_MS;

        {
            std::lock_guard<std::mutex> lock(_spellBonusCacheMutex);
            auto itr = _spellBonusCache.find(guid);
            if (itr == _spellBonusCache.end() || now >= itr->second.expireMs)
                itr = _spellBonusCache.insert_or_assign(guid, std::move(rebuilt)).first;

            return readBonus(itr->second);
        }
    }
}

void InvalidateSpellBonusCache(uint32 playerGuid)
{
    std::lock_guard<std::mutex> lock(_spellBonusCacheMutex);
    _spellBonusCache.erase(playerGuid);
}
}

// 符文触发冷却管理器
class RuneProcCooldownManager
{
private:
    // 键: 玩家GUID_符文ID, 值: 上次触发时间
    static std::map<std::string, uint32> _cooldowns;

public:
    static bool CheckAndSetCooldown(uint32 playerGuid, uint32 runeId, uint32 cooldownMs)
    {
        std::string key = Acore::ToString(playerGuid) + "_" + Acore::ToString(runeId);
        uint32 currentTime = getMSTime();

        auto it = _cooldowns.find(key);
        if (it != _cooldowns.end())
        {
            uint32 timeSinceLast = currentTime - it->second;
            if (timeSinceLast < cooldownMs)
            {
                return false; // 还在冷却中
            }
        }

        // 设置新的冷却时间
        _cooldowns[key] = currentTime;
        return true;
    }

    static void ClearCooldowns(uint32 playerGuid)
    {
        // 清理该玩家的所有冷却
        std::string prefix = Acore::ToString(playerGuid) + "_";
        for (auto it = _cooldowns.begin(); it != _cooldowns.end();)
        {
            if (it->first.find(prefix) == 0)
            {
                it = _cooldowns.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
};

// 静态成员初始化
std::map<std::string, uint32> RuneProcCooldownManager::_cooldowns;

/* 符文战斗脚本 - 处理攻击触发
 * 注意：这些战斗事件钩子在您的AzerothCore版本中可能不存在或签名不同
 * 请参考 src/server/game/Scripting/ScriptMgr.h 中 PlayerScript 的实际定义
 */
class RuneCombatScript : public PlayerScript
{
public:
    RuneCombatScript() : PlayerScript("RuneCombatScript") { }

    /* 以下事件已注释 - 需要根据您的版本调整

    void OnDamageDealt(Player* player, Unit* victim, uint32& damage, DamageEffectType damageType) override
    {
        ProcessRuneProc(player, victim, RUNE_PROC_ON_DAMAGE);
    }

    void OnTakeDamage(Player* player, Unit* attacker, uint32& damage, DamageEffectType damageType) override
    {
        ProcessRuneProc(player, attacker, RUNE_PROC_ON_TAKE_DAMAGE);
    }

    void OnKilledCreature(Player* player, Creature* victim) override
    {
        ProcessRuneProc(player, victim, RUNE_PROC_ON_KILL);
    }

    void OnLogout(Player* player) override
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();
        RuneProcCooldownManager::ClearCooldowns(playerGuid);
    }
    */

    /* ProcessRuneProc和相关方法已注释 - 仅在事件钩子启用后使用
private:
    enum RuneProcType
    {
        RUNE_PROC_ON_DAMAGE = 0,
        RUNE_PROC_ON_TAKE_DAMAGE = 1,
        RUNE_PROC_ON_KILL = 2
    };

    void ProcessRuneProc(Player* player, Unit* target, RuneProcType procType)
    {
        uint32 playerGuid = player->GetGUID().GetCounter();

        // 获取玩家所有装备的符文
        auto& storageList = sRuneSystem->GetCharacterRuneStore(playerGuid);

        for (const auto& storage : storageList)
        {
            // 检查装备是否穿戴
            Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, storage.itemGuid));
            if (!item || !item->IsEquipped())
                continue;

            // 解析符文物品ID列表
            auto runeItemIds = RuneUtils::ParseCommaSeparatedUInt32(storage.runeIds);

            for (uint32 runeItemId : runeItemIds)
            {
                if (runeItemId == 0)
                    continue;

                // 通过物品ID查找符文配置
                QueryResult result = WorldDatabase.Query(
                    "SELECT id, 法术ID, 冷却时间, 触发几率 FROM _模板_物品符文 WHERE 物品ID = {}",
                    runeItemId
                );

                if (!result)
                    continue;

                Field* fields = result->Fetch();
                uint32 runeId = fields[0].Get<uint32>();
                uint32 spellId = fields[1].Get<uint32>();
                uint32 cooldown = fields[2].Get<uint32>();
                uint32 procChance = fields[3].Get<uint32>();

                // 检查是否有法术ID（只有技能型符文才触发）
                if (spellId == 0)
                    continue;

                // 检查触发几率
                if (procChance > 0 && procChance < 100)
                {
                    uint32 roll = urand(1, 100);
                    if (roll > procChance)
                        continue; // 未触发
                }

                // 检查冷却
                if (!RuneProcCooldownManager::CheckAndSetCooldown(playerGuid, runeId, cooldown))
                    continue; // 还在冷却中

                // 触发法术
                TriggerRuneSpell(player, target, spellId, runeId);
            }
        }
    }

    void TriggerRuneSpell(Player* player, Unit* target, uint32 spellId, uint32 runeId)
    {
        if (!player || !target)
            return;

        // 验证法术存在
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
        {
            LOG_ERROR("rune.combat", "符文 {} 的法术ID {} 不存在", runeId, spellId);
            return;
        }

        // 施放法术
        player->CastSpell(target, spellId, true);

        LOG_DEBUG("rune.combat", "玩家 {} 触发符文 {} 的法术 {}",
            player->GetGUID().GetCounter(), runeId, spellId);
    }
    */
};

/* 符文法术修改脚本 - SpellScript系统在您的版本中可能不同
class RuneSpellScript : public SpellSC_CastHandler
{
public:
    RuneSpellScript() : SpellSC_CastHandler("RuneSpellScript") { }

    // 法术造成伤害前
    void OnBeforeHit(Spell* spell) override
    {
        if (!spell)
            return;

        Unit* caster = spell->GetCaster();
        if (!caster || !caster->IsPlayer())
            return;

        Player* player = caster->ToPlayer();
        uint32 spellId = spell->GetSpellInfo()->Id;

        // 查找是否有符文增强这个法术
        int32 bonusDamage = CalculateRuneSpellBonus(player, spellId);

        if (bonusDamage > 0)
        {
            // 增加法术伤害
            spell->SetSpellValue(SPELLVALUE_BASE_POINT0,
                spell->GetSpellValue(SPELLVALUE_BASE_POINT0) + bonusDamage);

            LOG_DEBUG("rune.spell", "符文增强法术 {} 伤害 +{}", spellId, bonusDamage);
        }
    }

private:
    int32 CalculateRuneSpellBonus(Player* player, uint32 spellId)
    {
        int32 totalBonus = 0;
        uint32 playerGuid = player->GetGUID().GetCounter();

        // 获取玩家所有装备的符文
        auto& storageList = sRuneSystem->GetCharacterRuneStore(playerGuid);

        for (const auto& storage : storageList)
        {
            // 检查装备是否穿戴
            Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, storage.itemGuid));
            if (!item || !item->IsEquipped())
                continue;

            // 解析符文物品ID列表
            auto runeItemIds = RuneUtils::ParseCommaSeparatedUInt32(storage.runeIds);

            for (uint32 runeItemId : runeItemIds)
            {
                if (runeItemId == 0)
                    continue;

                // 查找符文配置
                const CustomRuneInfo* runeInfo = nullptr;

                // 遍历查找匹配的符文
                QueryResult result = WorldDatabase.Query(
                    "SELECT id, 效果类型, 效果值, 法术ID FROM _模板_物品符文 WHERE 物品ID = {}",
                    runeItemId
                );

                if (!result)
                    continue;

                Field* fields = result->Fetch();
                uint32 runeSpellId = fields[3].Get<uint32>();

                // 检查是否是同一个法术
                if (runeSpellId == spellId)
                {
                    uint32 effectType = fields[1].Get<uint32>();
                    std::string effectValues = fields[2].Get<std::string>();

                    // 解析效果值
                    auto values = RuneUtils::ParseEffectValues(effectValues);
                    if (!values.empty() && values[0].first == 99)
                    {
                        totalBonus += values[0].second;
                    }
                }
            }
        }

        return totalBonus;
    }
};
*/

// 使用 UnitScript 拦截法术伤害
class RuneSpellDamageUnitScript : public UnitScript
{
public:
    RuneSpellDamageUnitScript() : UnitScript("RuneSpellDamageUnitScript") { }

    // 修改法术伤害
    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        if (!attacker || !attacker->IsPlayer() || !spellInfo)
            return;

        Player* player = attacker->ToPlayer();
        auto const [fixedBonus, percentBonus] = RuneCombat::GetSpellBonus(player, spellInfo->Id);
        if (fixedBonus <= 0 && percentBonus <= 0)
            return;

        uint32 const compatDamage = uint32(std::max<int32>(0, damage));
        AsInt64 const baseDamage = attacker->PeekAsDamageOverride(target, compatDamage);
        AsInt64 const totalBonus = ClampAsInt256(boost::multiprecision::int512_t(fixedBonus) + MultiplyDivideAsInt256(baseDamage, percentBonus, 100));

        if (totalBonus > 0)
        {
            AsInt64 const finalDamage = ClampAsInt256(boost::multiprecision::int512_t(baseDamage) + totalBonus);
            uint32 const compatFinalDamage = AsInt64ToUInt32Clamp(finalDamage);
            damage = compatFinalDamage > uint32(std::numeric_limits<int32>::max())
                ? std::numeric_limits<int32>::max()
                : int32(compatFinalDamage);

            if (target)
                attacker->SetAsDamageOverride(target->GetGUID(), finalDamage > damage ? finalDamage : AsInt64Zero());
        }
    }

    // 修改治疗量
    void ModifyHealRecieved(Unit* healer, Unit* target, uint32 CastId, uint32& heal) override
    {
        if (!healer || !healer->IsPlayer() || !CastId)
            return;

        Player* player = healer->ToPlayer();
        auto const [fixedBonus, percentBonus] = RuneCombat::GetSpellBonus(player, CastId);
        if (fixedBonus <= 0 && percentBonus <= 0)
            return;

        AsInt64 const baseHeal = healer->PeekAsHealOverride(target, heal);
        AsInt64 const totalBonus = ClampAsInt256(boost::multiprecision::int512_t(fixedBonus) + MultiplyDivideAsInt256(baseHeal, percentBonus, 100));

        if (totalBonus > 0)
        {
            AsInt64 const finalHeal = ClampAsInt256(boost::multiprecision::int512_t(baseHeal) + totalBonus);
            heal = AsInt64ToUInt32Clamp(finalHeal);

            if (target)
                healer->SetAsHealOverride(target->GetGUID(), finalHeal > heal ? finalHeal : AsInt64Zero());
        }
    }
};

void AddSC_rune_combat_scripts()
{
    new RuneCombatScript();
    new RuneSpellDamageUnitScript();
}
