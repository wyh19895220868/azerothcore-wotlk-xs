#include "AsInt256.h"
#include "StringConvert.h"
#include "HuanJingSystem.h"

#include "Log.h"
#include "ObjectMgr.h"
#include "Random.h"
#include "CreatureAI.h"
#include "UpdateData.h"
#include "ItemTemplate.h"
#include "GridDefines.h"
#include "Map.h"
#include "World.h"
#include "Chat.h"
#include "Config.h"
#include "SpellMgr.h"
#include "StringConvert.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "Spell.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "ModuleFormat.h"
#include "../../../src/server/scripts/Custom/_Helper/CombatPower.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include "../../../src/server/scripts/Custom/CreatureMod/AsCreature.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

AsInt64 const CLIENT_VISIBLE_RESOURCE_LIMIT = 2147483520ULL;
constexpr long double FLOAT_DAMAGE_LIMIT = static_cast<long double>(std::numeric_limits<float>::max());
AsInt64 const LEGACY_UINT32_ATTRIBUTE_LIMIT = std::numeric_limits<uint32>::max();
constexpr long double LEGACY_INT64_DAMAGE_LIMIT = 9223372036854775808.0L;
constexpr char MAP_CREATURE_CONTROL_TABLE[] = "_模板_地图生物控制";

uint64 GenerateCreatureKey(Creature const* creature);

bool ParseMythicDifficulties(std::string const& value, std::vector<uint32>& difficulties)
{
    difficulties.clear();
    if (value.empty() || value == "0" || value == "-1" || value == "全部" || value == "*")
        return true;

    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), ',', ' ');
    std::replace(normalized.begin(), normalized.end(), ';', ' ');
    std::replace(normalized.begin(), normalized.end(), '|', ' ');
    for (std::string const& delimiter : { std::string("，"), std::string("；") })
    {
        size_t position = 0;
        while ((position = normalized.find(delimiter, position)) != std::string::npos)
            normalized.replace(position, delimiter.size(), " ");
    }

    std::istringstream stream(normalized);
    std::string token;
    while (stream >> token)
    {
        Optional<uint32> difficulty = Acore::StringTo<uint32>(token);
        if (!difficulty || *difficulty == 0)
            continue;

        if (std::find(difficulties.begin(), difficulties.end(), *difficulty) == difficulties.end())
            difficulties.push_back(*difficulty);
    }

    return false;
}

bool MythicDifficultyMatches(HuanJingCreatureConfig const& config, uint32 difficulty)
{
    return config.allMythicDifficulties ||
        std::find(config.mythicDifficulties.begin(), config.mythicDifficulties.end(), difficulty) != config.mythicDifficulties.end();
}

long double GetDynamicDifficultyScale(HuanJingCreatureConfig const& config, Player* player)
{
    if (!config.dynamicDifficultyEnabled || !player)
        return 1.0L;

    long double configuredMultiplier = std::isfinite(config.difficultyMultiplier)
        ? static_cast<long double>(config.difficultyMultiplier)
        : 1.0L;
    configuredMultiplier = std::clamp(configuredMultiplier, 0.01L, 1000.0L);

    if (config.combatPowerThreshold <= 0)
        return configuredMultiplier;

    AsInt64 combatPower = CustomCombatPower::Calculate(player);
    long double ratio = static_cast<long double>(combatPower) / static_cast<long double>(config.combatPowerThreshold);
    ratio = std::clamp(ratio, 1.0L, 1000000.0L);
    return std::clamp(configuredMultiplier * ratio, 0.01L, 1000000.0L);
}

bool CheckCombatPowerRequirement(Player* player, Creature* creature, bool notify)
{
    if (!player || !creature)
        return true;

    HuanJingCreatureConfig const* config = sHuanJingSystem->GetMapCreatureConfig(creature, player);
    if (!config || !config->dynamicDifficultyEnabled || config->combatPowerThreshold <= 0)
        return true;

    AsInt64 combatPower = CustomCombatPower::Calculate(player);
    if (combatPower >= config->combatPowerThreshold)
        return true;

    if (notify && player->GetSession())
    {
        static std::unordered_map<uint32, uint32> lastWarnings;
        uint32 playerGuid = player->GetGUID().GetCounter();
        uint32 now = getMSTime();
        auto itr = lastWarnings.find(playerGuid);
        if (itr == lastWarnings.end() || now - itr->second >= 2000)
        {
            lastWarnings[playerGuid] = now;
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffff4040战力不足：当前 %s，需要 %s，无法对该难度生物造成伤害。|r",
                AsInt64ToString(combatPower).c_str(), AsInt64ToString(config->combatPowerThreshold).c_str());
        }
    }

    return false;
}

// 安全的float转换函数，防止溢出
inline float SafeFloatValue(AsInt64 value, AsInt64 maxLimit)
{
    if (maxLimit > 0 && value > maxLimit)
        value = maxLimit;

    return Acore::Number::ToFloat(value);
}

inline AsInt64 ApplyOptionalUInt64Limit(AsInt64 value, AsInt64 maxLimit)
{
    return (maxLimit > 0 && value > maxLimit) ? maxLimit : value;
}

inline AsInt64 AbsSignedInt64ToUInt64(AsInt64 value)
{
    return value < 0 ? -value : value;
}

inline long double CalculateHuanJingDamageMultiplier(AsInt64 reductionPct)
{
    AsInt64 value = reductionPct < 0
        ? AbsSignedInt64ToUInt64(reductionPct)
        : static_cast<AsInt64>(reductionPct);

    if (value == 0)
        return 1.0L;

    if (value < 100)
        return static_cast<long double>(100 - value) / 100.0L;

    if (value == 100)
        return 0.0L;

    // 101 => +1%, 200 => +20%, 2000 => +200%.
    long double bonusPct = value < 200
        ? static_cast<long double>(value - 100)
        : static_cast<long double>(value) / 10.0L;

    return 1.0L + bonusPct / 100.0L;
}

inline AsInt64 NormalizeConfiguredUInt64Limit(AsInt64 configuredLimit)
{
    if (configuredLimit == 0 || configuredLimit == LEGACY_UINT32_ATTRIBUTE_LIMIT)
        return 0;

    return configuredLimit;
}

inline AsInt64 ParseUInt64Config(std::string const& value, AsInt64 defaultValue = 0)
{
    Optional<AsInt64> parsed = Acore::StringTo<AsInt64>(value);
    return parsed ? *parsed : defaultValue;
}

inline long double ParseLongDoubleConfig(std::string const& value, long double defaultValue)
{
    try
    {
        return std::stold(value);
    }
    catch (...)
    {
        return defaultValue;
    }
}

inline long double NormalizeConfiguredDamageLimit(long double configuredLimit)
{
    if (configuredLimit <= 0.0L || !std::isfinite(configuredLimit))
        return 0.0L;

    long double limit = configuredLimit;
    if (std::fabs(limit - LEGACY_INT64_DAMAGE_LIMIT) <= 4096.0L)
        return 0.0L;

    return limit;
}

inline bool ExceedsOptionalUInt64Limit(AsInt64 value, AsInt64 maxLimit)
{
    return maxLimit > 0 && value > maxLimit;
}

inline bool ReachesOptionalDamageLimit(float value, long double maxLimit)
{
    return maxLimit > 0.0L && value >= maxLimit;
}

inline std::string FormatOptionalUInt64Limit(AsInt64 maxLimit)
{
    return maxLimit ? Acore::ToString(maxLimit) : "无限制";
}

inline std::string FormatOptionalDamageLimit(long double maxLimit)
{
    if (maxLimit <= 0.0L || !std::isfinite(maxLimit))
        return "无限制";

    if (maxLimit >= static_cast<long double>(std::numeric_limits<AsInt64>::max()))
        return "float最大值";

    return Acore::ToString(static_cast<AsInt64>(maxLimit));
}

inline float SafeDamageFloatValue(long double value, long double maxLimit)
{
    if (value <= 0.0L || !std::isfinite(value))
        return 0.0f;

    long double effectiveLimit = (maxLimit > 0.0L && std::isfinite(maxLimit)) ? maxLimit : FLOAT_DAMAGE_LIMIT;
    if (effectiveLimit > FLOAT_DAMAGE_LIMIT)
        effectiveLimit = FLOAT_DAMAGE_LIMIT;

    if (value > effectiveLimit)
        value = effectiveLimit;

    float floatValue = static_cast<float>(value);
    while (static_cast<long double>(floatValue) > effectiveLimit && floatValue > 0.0f)
        floatValue = std::nextafter(floatValue, 0.0f);

    return floatValue;
}

inline uint32 ToClientHealthValue(AsInt64 value)
{
    return value > CLIENT_VISIBLE_RESOURCE_LIMIT
        ? static_cast<uint32>(CLIENT_VISIBLE_RESOURCE_LIMIT)
        : Acore::Number::ToUInt32Saturated(value);
}

inline uint32 CalculateHealthBarCount(AsInt64 const& totalHealth, AsInt64 const& healthPerBar)
{
    if (totalHealth == 0 || healthPerBar == 0)
        return 0;

    AsInt64 bars = totalHealth / healthPerBar;
    if (totalHealth % healthPerBar != 0)
        ++bars;
    if (bars < 1)
        bars = 1;
    if (bars > 1000)
        bars = 1000;

    return Acore::Number::ToUInt32Saturated(bars);
}

#include "Player.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <sstream>
#include <string>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <iomanip>
#include <memory>
#include <iostream>

#include "../../../src/server/scripts/Custom/_Helper/Helper.h"

// 物品鉴定系统集成
#ifndef MODULE_ITEM_IDENTIFICATION
#define MODULE_ITEM_IDENTIFICATION
#endif
#include "mod-item-identification-system/src/ItemIdentificationSystem.h"

// 物品成长系统集成（目前通过鉴定系统的批量查询接口统一获取成长属性数据）
#ifdef MODULE_ITEM_GROWTH
#include "ItemGrowthMgr.h"
#endif

namespace
{
    bool HasIdentificationBaseAttributes(Item* item)
    {
        if (!item || !sItemIdentificationSystem || !sItemIdentificationSystem->_enabled)
            return false;

        ItemIdentificationSystem::AllModuleData moduleData =
            sItemIdentificationSystem->QueryAllModuleData(item->GetEntry(), item->GetGUID().GetCounter());
        return !moduleData.baseAttributes.empty();
    }

    // 血条系统使用的Addon前缀（客户端插件需要使用同样的前缀进行注册）
    constexpr char HEALTHBAR_ADDON_PREFIX[] = "HBUI";

    bool HasNeedTemplate(uint32 requirementId)
    {
        return !requirementId || NeedTempStore.find(requirementId) != NeedTempStore.end();
    }

    bool CheckNeedTemplate(Player* player, uint32 requirementId, bool showMessages)
    {
        if (!player)
            return false;

        if (!HasNeedTemplate(requirementId))
            return false;

        return showMessages ? sHelper->NotDeleteCheckNeed(player, requirementId) : sHelper->ShowCheckNeed(player, requirementId);
    }

    bool ConsumeNeedTemplate(Player* player, uint32 requirementId, std::string const& info)
    {
        if (!player)
            return false;

        if (!HasNeedTemplate(requirementId))
            return false;

        return sHelper->CheckNeed(player, requirementId, info);
    }

    void SendAddonMessage(Player* player, std::string const& prefix, std::string const& payload)
    {
        if (!player)
            return;

        std::string fullMessage = prefix + "\t" + payload;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }

    AsInt64 GetClientSafeResourceLimit(AsInt64 const& configuredLimit)
    {
        return configuredLimit;
    }

    AsInt64 SaturateToUInt64(long double value)
    {
        if (std::isnan(value) || value <= 0.0L)
            return 0;

        long double maxValue = static_cast<long double>(std::numeric_limits<AsInt64>::max());
        if (std::isinf(value) || value >= maxValue)
            return std::numeric_limits<AsInt64>::max();

        return static_cast<AsInt64>(value);
    }

    std::string FormatUInt64(AsInt64 const& value)
    {
        return Acore::ToString(value);
    }

    AsInt64 ToUInt64Damage(long double value)
    {
        if (std::isnan(value) || value <= 0.0L)
            return 0;

        return AsInt256FromFloating(value);
    }

    AsInt64 ToUInt64Damage(uint32 value)
    {
        return AsInt64(value);
    }

    AsInt64 ClampInt64ToInt64(AsInt64 const& value)
    {
        if (value > std::numeric_limits<AsInt64>::max())
            return std::numeric_limits<AsInt64>::max();
        if (value < std::numeric_limits<AsInt64>::min())
            return std::numeric_limits<AsInt64>::min();

        return static_cast<AsInt64>(value);
    }

    AsInt64 ToAsInt64Saturated(AsInt64 const& value)
    {
        return value > 0 ? value : AsInt64(0);
    }

    AsInt64 ToUInt64FromAsInt64(AsInt64 const& value)
    {
        return value > 0 ? value : AsInt64(0);
    }

    AsInt64 GetCreatureCombatMaxHealth(Creature const* creature)
    {
        if (!creature)
            return 0;

        AsInt64 health = creature->GetAsMaxHealth();
        return health > 0 ? ToUInt64FromAsInt64(health) : AsInt64(creature->GetMaxHealth());
    }

    AsInt64 GetCreatureCombatHealth(Creature const* creature)
    {
        if (!creature)
            return 0;

        AsInt64 health = creature->GetAsHealth();
        return health > 0 ? ToUInt64FromAsInt64(health) : AsInt64(creature->GetHealth());
    }

    AsInt64 GetCreatureCombatMaxPower(Creature const* creature, Powers power)
    {
        if (!creature)
            return 0;

        AsInt64 powerValue = creature->GetAsMaxPower(power);
        return powerValue > 0 ? ToUInt64FromAsInt64(powerValue) : AsInt64(creature->GetMaxPower(power));
    }

    void SetCreatureCombatMaxHealth(Creature* creature, AsInt64 const& value)
    {
        if (!creature)
            return;

        creature->SetAsMaxHealth(ToAsInt64Saturated(value > 0 ? value : AsInt64(1)));
    }

    void SetCreatureCombatCreateHealth(Creature* creature, AsInt64 const& value)
    {
        if (!creature)
            return;

        creature->SetAsCreateHealth(ToAsInt64Saturated(value > 0 ? value : AsInt64(1)));
    }

    void SetCreatureCombatHealth(Creature* creature, AsInt64 const& value)
    {
        // A corpse can remain in the world while delayed health-bar work is
        // still queued. Never turn a dead creature back into a positive-health
        // client field; only the normal respawn flow may restore its health.
        if (!creature || !creature->IsAlive())
            return;

        creature->SetAsHealth(ToAsInt64Saturated(value));
    }

    void SetCreatureCombatMaxPower(Creature* creature, Powers power, AsInt64 const& value)
    {
        if (!creature)
            return;

        creature->SetAsMaxPower(power, ToAsInt64Saturated(value));
    }

    void SetCreatureCombatPower(Creature* creature, Powers power, AsInt64 const& value)
    {
        if (!creature)
            return;

        creature->SetAsPower(power, ToAsInt64Saturated(value));
    }

    void SetCreatureCombatWeaponDamageRange(Creature* creature, WeaponAttackType attackType, WeaponDamageRange damageRange, AsInt64 const& value)
    {
        if (!creature || attackType >= MAX_ATTACK)
            return;

        AsInt64 normalized = value > 0 ? value : AsInt64Zero();
        if (damageRange == MINDAMAGE)
            creature->AsWeaponDamageMin[attackType] = normalized;
        else
            creature->AsWeaponDamageMax[attackType] = normalized;

        creature->SetBaseWeaponDamage(attackType, damageRange, Acore::Number::ToFloat(normalized));
        creature->UpdateDamagePhysical(attackType);
    }

    bool UnitHasCreatureType(Unit const* unit)
    {
        return unit && unit->GetTypeId() == TYPEID_UNIT;
    }

    int32 ToInt32Damage(AsInt64 const& value)
    {
        AsInt64 maxValue = static_cast<AsInt64>(std::numeric_limits<int32>::max());
        return value > maxValue ? std::numeric_limits<int32>::max() : static_cast<int32>(value);
    }

    void StoreDamageForOldCore(Unit* attacker, Unit* victim, AsInt64 const& wideDamage, uint32& damage)
    {
        damage = uint32(ToInt32Damage(wideDamage));
        if (attacker && victim)
            attacker->SetAsDamageOverride(victim->GetGUID(), ToAsInt64Saturated(wideDamage));
    }

    void StoreDamageForOldCore(Unit* attacker, Unit* victim, AsInt64 const& wideDamage, int32& damage)
    {
        damage = ToInt32Damage(wideDamage);
        if (attacker && victim)
            attacker->SetAsDamageOverride(victim->GetGUID(), ToAsInt64Saturated(wideDamage));
    }

    thread_local uint32 s_externalPlayerDamageNoInitialAdjustDepth = 0;

    bool IsExternalPlayerDamageNoInitialAdjustActive()
    {
        return s_externalPlayerDamageNoInitialAdjustDepth != 0;
    }

    std::string FormatInt64(AsInt64 const& value)
    {
        return Acore::ToString(value);
    }

    AsInt64 SaturatingMultiplyUInt64(AsInt64 value, uint32 multiplier)
    {
        if (value == 0 || multiplier == 0)
            return 0;

        if (value > std::numeric_limits<AsInt64>::max() / multiplier)
            return std::numeric_limits<AsInt64>::max();

        return value * multiplier;
    }

    AsInt64 SaturatingAddUInt64(AsInt64 const& left, AsInt64 const& right)
    {
        AsInt64 maxValue = std::numeric_limits<AsInt64>::max();
        if (right > maxValue - left)
            return maxValue;

        return left + right;
    }

    AsInt64 CalculateHealthBarVisibleHealth(uint32 totalBars, AsInt64 const& healthPerBar, uint32 barsRemaining, AsInt64 const& currentBarHealth)
    {
        if (totalBars == 0 || healthPerBar == 0 || barsRemaining == 0 || currentBarHealth == 0)
            return 0;

        if (barsRemaining > totalBars)
            barsRemaining = totalBars;

        AsInt64 currentHealth = currentBarHealth > healthPerBar ? healthPerBar : currentBarHealth;

        boost::multiprecision::int512_t totalRemaining =
            boost::multiprecision::int512_t(healthPerBar) * (barsRemaining - 1) + currentHealth;
        AsInt64 averageRemaining = ClampAsInt256((totalRemaining + totalBars - 1) / totalBars);
        return std::clamp(averageRemaining, AsInt64(1), healthPerBar);
    }

    AsInt64 EstimateCurrentBarHealthFromVisibleHealth(uint32 totalBars, AsInt64 const& healthPerBar, uint32 barsRemaining, AsInt64 const& visibleHealth)
    {
        if (totalBars == 0 || healthPerBar == 0 || barsRemaining == 0 || visibleHealth == 0)
            return 0;

        if (barsRemaining > totalBars)
            barsRemaining = totalBars;

        AsInt64 visibleTotal = SaturatingMultiplyUInt64(visibleHealth, totalBars);
        AsInt64 completedBarsHealth = barsRemaining > 1 ? SaturatingMultiplyUInt64(healthPerBar, barsRemaining - 1) : AsInt64(0);
        if (visibleTotal <= completedBarsHealth)
            return 1;

        AsInt64 currentHealth = visibleTotal - completedBarsHealth;
        if (currentHealth > healthPerBar)
            currentHealth = healthPerBar;
        if (currentHealth == 0)
            currentHealth = 1;

        return currentHealth;
    }

    long double ToFiniteLongDouble(long double value, long double defaultValue = 0.0L)
    {
        if (!std::isfinite(value))
            return defaultValue;
        return value;
    }

    long double ClampLongDouble(long double value, long double minValue, long double maxValue)
    {
        if (!std::isfinite(value))
            return minValue;
        if (value < minValue)
            return minValue;
        if (value > maxValue)
            return maxValue;
        return value;
    }

    void NormalizeHealthBarData(HealthBarData& data)
    {
        if (data.healthPerBar == 0 || data.totalBars == 0)
        {
            data.currentBar = 0;
            data.currentHealth = 0;
            data.visibleHealth = 0;
            return;
        }

        if (data.currentBar > data.totalBars)
            data.currentBar = data.totalBars;

        if (data.currentBar == 0)
        {
            data.currentHealth = 0;
            data.visibleHealth = 0;
            return;
        }

        if (data.currentHealth > data.healthPerBar)
            data.currentHealth = data.healthPerBar;

        if (data.currentHealth == 0)
        {
            data.currentBar = 0;
            data.visibleHealth = 0;
            return;
        }

        AsInt64 calculatedVisibleHealth = CalculateHealthBarVisibleHealth(data.totalBars, data.healthPerBar, data.currentBar, data.currentHealth);
        if (data.visibleHealth == 0 || data.visibleHealth > calculatedVisibleHealth)
            data.visibleHealth = calculatedVisibleHealth;
        else if (data.visibleHealth > data.healthPerBar)
            data.visibleHealth = data.healthPerBar;
    }

    float GetCreatureRankHealthModForHealthBar(uint32 rank)
    {
        switch (rank)
        {
            case CREATURE_ELITE_NORMAL:
                return sWorld->getRate(RATE_CREATURE_NORMAL_HP);
            case CREATURE_ELITE_ELITE:
                return sWorld->getRate(RATE_CREATURE_ELITE_ELITE_HP);
            case CREATURE_ELITE_RAREELITE:
                return sWorld->getRate(RATE_CREATURE_ELITE_RAREELITE_HP);
            case CREATURE_ELITE_WORLDBOSS:
                return sWorld->getRate(RATE_CREATURE_ELITE_WORLDBOSS_HP);
            case CREATURE_ELITE_RARE:
                return sWorld->getRate(RATE_CREATURE_ELITE_RARE_HP);
            default:
                return sWorld->getRate(RATE_CREATURE_ELITE_ELITE_HP);
        }
    }

    AsInt64 GetCreatureTemplateMaxHealthForHealthBar(Creature* creature)
    {
        if (!creature)
            return 0;

        CreatureTemplate const* creatureTemplate = creature->GetCreatureTemplate();
        if (!creatureTemplate)
            return 0;

        CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(creature->GetLevel(), creatureTemplate->unit_class);
        if (!stats)
            return 0;

        AsInt64 baseHealth = ToUInt64FromAsInt64(stats->GenerateAsHealth(creatureTemplate));
        long double rankHealth = Acore::Number::ToLongDouble(baseHealth) * static_cast<long double>(GetCreatureRankHealthModForHealthBar(creatureTemplate->rank));
        return SaturateToUInt64(rankHealth);
    }

    AsInt64 ResolveCreatureServerHealthForHealthBar(Creature* creature, AsInt64 explicitHealthPerBar)
    {
        AsInt64 combatHealth = GetCreatureCombatMaxHealth(creature);
        AsInt64 templateHealth = GetCreatureTemplateMaxHealthForHealthBar(creature);
        return std::max(explicitHealthPerBar, std::max(combatHealth, templateHealth));
    }

    void SyncHealthBarDataWithCreatureMaxHealth(Creature* creature)
    {
        if (!creature)
            return;

        AsInt64 resolvedHealthPerBar = ResolveCreatureServerHealthForHealthBar(creature, 0);
        if (resolvedHealthPerBar == 0)
            return;

        uint64 healthBarKey = GenerateCreatureKey(creature);
        AsInt64 oldHealthPerBar = 0;
        AsInt64 oldCurrentHealth = 0;
        AsInt64 oldVisibleHealth = 0;
        AsInt64 newCurrentHealth = 0;
        AsInt64 newVisibleHealth = 0;
        bool shouldFillCreatureHealth = false;
        bool updated = false;

        {
            std::lock_guard<std::mutex> lock(sHuanJingSystem->_healthBarMutex);
            auto it = sHuanJingSystem->_healthBarData.find(healthBarKey);
            if (it == sHuanJingSystem->_healthBarData.end() || !it->second.enabled)
                return;

            HealthBarData& data = it->second;
            if (resolvedHealthPerBar <= data.healthPerBar)
            {
                NormalizeHealthBarData(data);
                return;
            }

            oldHealthPerBar = data.healthPerBar;
            oldCurrentHealth = data.currentHealth;
            oldVisibleHealth = data.visibleHealth;
            long double currentRatio = oldHealthPerBar > 0
                ? Acore::Number::ToLongDouble(data.currentHealth) / Acore::Number::ToLongDouble(oldHealthPerBar)
                : 1.0L;
            long double visibleRatio = oldHealthPerBar > 0
                ? Acore::Number::ToLongDouble(data.visibleHealth) / Acore::Number::ToLongDouble(oldHealthPerBar)
                : currentRatio;

            if (currentRatio < 0.0L)
                currentRatio = 0.0L;
            if (currentRatio > 1.0L)
                currentRatio = 1.0L;
            if (visibleRatio < 0.0L)
                visibleRatio = 0.0L;
            if (visibleRatio > 1.0L)
                visibleRatio = 1.0L;

            data.healthPerBar = resolvedHealthPerBar;
            data.totalHealth = SaturatingMultiplyUInt64(data.healthPerBar, data.totalBars);

            if (data.currentBar > 0)
            {
                shouldFillCreatureHealth = true;
                if (currentRatio >= 0.999999L)
                    data.currentHealth = data.healthPerBar;
                else
                    data.currentHealth = SaturateToUInt64(std::ceil(Acore::Number::ToLongDouble(data.healthPerBar) * currentRatio));

                if (visibleRatio >= 0.999999L)
                    data.visibleHealth = data.healthPerBar;
                else
                    data.visibleHealth = SaturateToUInt64(std::ceil(Acore::Number::ToLongDouble(data.healthPerBar) * visibleRatio));
            }

            NormalizeHealthBarData(data);
            newCurrentHealth = data.currentHealth;
            newVisibleHealth = data.visibleHealth;
            updated = true;
        }

        if (updated)
        {
            LOG_DEBUG("module.huanjing", ModuleOldFormat("[幻境血条校准] 生物 %s Entry=%s 每条血量从 %s 修正为 %s，当前条血量 %s -> %s，显示血量 %s -> %s", creature->GetName(), creature->GetEntry(), FormatUInt64(oldHealthPerBar), FormatUInt64(resolvedHealthPerBar), FormatUInt64(oldCurrentHealth), FormatUInt64(newCurrentHealth), FormatUInt64(oldVisibleHealth), FormatUInt64(newVisibleHealth)).c_str());

            if (resolvedHealthPerBar > GetCreatureCombatMaxHealth(creature))
            {
                SetCreatureCombatCreateHealth(creature, resolvedHealthPerBar);
                SetCreatureCombatMaxHealth(creature, resolvedHealthPerBar);
                if (shouldFillCreatureHealth)
                    SetCreatureCombatHealth(creature, newVisibleHealth != 0 ? newVisibleHealth : resolvedHealthPerBar);
            }
        }
    }

    void SendHealthBarAddonMessage(Player* player, std::string const& payload)
    {
        if (!player || payload.empty())
            return;

        SendAddonMessage(player, HEALTHBAR_ADDON_PREFIX, payload);
    }

    char NormalizeItemAttributeMode(char mode)
    {
        if (mode == '+' || mode == 1)
            return '+';
        if (mode == '-' || mode == 'n')
            return '-';
        return 'x';
    }

    int32 ItemAttributeModeToDbValue(char mode)
    {
        mode = NormalizeItemAttributeMode(mode);
        int32 result = 0;
        if (mode == '+')
            result = 1;
        else if (mode == '-')
            result = -1;
        else
            result = 0;
        return result;
    }

    char DbValueToItemAttributeMode(int32 value)
    {
        char result = 'x';
        if (value == 1)
            result = '+';
        else if (value == -1)
            result = '-';
        else
            result = 'x';
        return result;
    }

    bool HasItemAttributeEffect(float value, char mode)
    {
        mode = NormalizeItemAttributeMode(mode);
        if (mode == '-')
            return false;
        return mode == '+' ? value > 0.0f : value > 1.0f;
    }

    char ResolveConfiguredItemAttributeMode(uint32 equipAttrValue, char configuredMode)
    {
        if (equipAttrValue == 0)
            return '-';
        return NormalizeItemAttributeMode(configuredMode);
    }

    float ResolveConfiguredItemAttributeValue(uint32 huanJingLevel, Item* item, uint32 equipAttrValue, float equipAttrMultiplier)
    {
        (void)item;

        if (equipAttrValue == 0)
            return 0.0f;

        if (equipAttrValue == 1)
        {
            float coefficient = equipAttrMultiplier > 0.0f ? equipAttrMultiplier : 1.0f;
            return static_cast<float>(huanJingLevel) * coefficient;
        }

        return static_cast<float>(equipAttrValue);
    }

    int32 ClampToInt32(AsInt64 value)
    {
        if (value > std::numeric_limits<int32>::max())
            return std::numeric_limits<int32>::max();
        if (value < std::numeric_limits<int32>::min())
            return std::numeric_limits<int32>::min();
        return static_cast<int32>(value);
    }

    AsInt64 CalculateEnhancedAttributeValue(AsInt64 const& originalValue, float value, char mode)
    {
        mode = NormalizeItemAttributeMode(mode);
        if (mode == '-')
            return originalValue;

        if (!std::isfinite(value))
            return originalValue;
        if (mode == '+')
            return ClampAsInt256(boost::multiprecision::int512_t(originalValue) + AsInt256FromFloating(value));
        return ScaleAsInt64ByDouble(AsInt256FromFloating(originalValue), value);
    }

}

HuanJingSystem* HuanJingSystem::instance()
{
    static HuanJingSystem instance;
    return &instance;
}

// 生成生物的唯一标识键（地图ID + 实例ID + 生物GUID）
uint64 GenerateCreatureKey(Creature const* creature)
{
    if (!creature)
        return 0;

    uint32 mapId = creature->GetMapId();
    uint32 instanceId = 0;
    if (Map const* map = creature->FindMap())
        instanceId = map->GetInstanceId();
    else
        instanceId = creature->GetInstanceId();
    uint32 creatureGuid = creature->GetGUID().GetCounter();

    // 使用64位整数组合：高32位存储(地图ID << 16 | 实例ID)，低32位存储生物GUID
    uint64 key = (uint64(mapId & 0xFFFF) << 48) | (uint64(instanceId & 0xFFFF) << 32) | uint64(creatureGuid);
    return key;
}

namespace
{
    constexpr uint32 CREATURE_ADJUSTMENT_RECHECK_INTERVAL_MS = 3000;
    constexpr float CREATURE_ADJUSTMENT_RECHECK_RANGE = 200.0f;

    std::mutex s_combatProtectionLogMutex;
    std::unordered_map<uint64, std::unordered_set<uint64>> s_combatProtectionLogKeysByCreature;

    Player* GetCombatOwnerPlayer(Unit* unit)
    {
        return unit ? unit->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
    }

    uint32 GetCreatureAdjustedHuanJingLevel(Creature* creature)
    {
        if (!creature)
            return 0;

        uint64 creatureKey = GenerateCreatureKey(creature);
        std::lock_guard<std::mutex> lock(sHuanJingSystem->_creatureMutex);
        auto it = sHuanJingSystem->_creatureAdjustedLevels.find(creatureKey);
        return it != sHuanJingSystem->_creatureAdjustedLevels.end() ? it->second : 0;
    }

    bool IsHuanJingExcludedCreature(Creature const* creature)
    {
        if (!creature)
            return false;

        switch (creature->GetEntry())
        {
            case 37011: // The Damned: ICC event creature must use native death handling.
                return true;
            default:
                return false;
        }
    }

    bool IsChallengeMirageDebugCreature(Creature const* creature)
    {
        if (!creature)
            return false;

        return creature->GetEntry() >= 800000 && creature->GetEntry() <= 800099;
    }

    uint32 GetCreatureMapInstanceId(Creature const* creature)
    {
        if (!creature)
            return 0;

        if (Map const* map = creature->FindMap())
            return map->GetInstanceId();

        return creature->GetInstanceId();
    }

    uint32 GetUnitMapInstanceId(Unit const* unit)
    {
        if (!unit)
            return 0;

        if (Map const* map = unit->FindMap())
            return map->GetInstanceId();

        return unit->GetInstanceId();
    }

    bool IsPlayerInCreatureInstance(Player const* player, Creature const* creature)
    {
        if (!player || !creature || !player->IsInWorld() || !creature->IsInWorld())
            return false;

        return player->GetMapId() == creature->GetMapId()
            && GetUnitMapInstanceId(player) == GetCreatureMapInstanceId(creature);
    }

    bool IsHuanJingCreatureEligibleForPlayer(Creature* creature, Player* player)
    {
        if (!creature || !player || !creature->IsInWorld() || !player->IsInWorld())
            return false;

        if (!creature->IsAlive() || creature->IsPet() || creature->IsControlledByPlayer())
            return false;

        if (IsHuanJingExcludedCreature(creature))
            return false;

        return player->IsValidAttackTarget(creature);
    }

    bool IsCreaturePrimaryCombatTargetForPlayer(Creature* creature, Player* player, Unit* combatVictim = nullptr)
    {
        if (!creature || !player)
            return false;

        Unit* victim = combatVictim ? combatVictim : creature->GetVictim();
        if (!victim)
            return false;

        Player* victimPlayer = GetCombatOwnerPlayer(victim);
        return victimPlayer && victimPlayer->GetGUID() == player->GetGUID();
    }

    bool HasAliveEligiblePlayerNearCreature(Creature* creature, Player* ignoredPlayer = nullptr, float range = CREATURE_ADJUSTMENT_RECHECK_RANGE)
    {
        if (!creature || !creature->IsInWorld())
            return false;

        std::list<Player*> nearbyPlayers;
        Acore::AnyPlayerInObjectRangeCheck check(creature, range);
        Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(creature, nearbyPlayers, check);
        Cell::VisitWorldObjects(creature, searcher, range);

        for (Player* player : nearbyPlayers)
        {
            if (!player || !player->IsInWorld() || !player->IsAlive())
                continue;

            if (ignoredPlayer && player->GetGUID() == ignoredPlayer->GetGUID())
                continue;

            if (player->GetSession() && player->GetSession()->PlayerLogout())
                continue;

            if (IsHuanJingCreatureEligibleForPlayer(creature, player))
                return true;
        }

        return false;
    }

    void ResetNearbyHealthBarCreaturesForPlayer(Player* player, float range = 250.0f, bool forceEvade = true)
    {
        if (!player || !player->IsInWorld())
            return;

        class AliveCreatureInRange
        {
        public:
            AliveCreatureInRange(WorldObject const* object, float maxRange) : _object(object), _range(maxRange) { }

            bool operator()(Creature* creature)
            {
                return creature && creature->IsAlive() && !creature->IsPet() && _object->IsWithinDistInMap(creature, _range);
            }

        private:
            WorldObject const* _object;
            float _range;
        };

        std::list<Creature*> nearbyCreatures;
        AliveCreatureInRange check(player, range);
        Acore::CreatureListSearcher<AliveCreatureInRange> searcher(player, nearbyCreatures, check);
        Cell::VisitGridObjects(player, searcher, range);

        for (Creature* creature : nearbyCreatures)
        {
            if (!creature || !sHuanJingSystem->IsHealthBarEnabled(creature))
                continue;

            if (HasAliveEligiblePlayerNearCreature(creature, player, CREATURE_ADJUSTMENT_RECHECK_RANGE))
                continue;

            sHuanJingSystem->ResetCreatureHealthBarsToFull(creature, true);

            if (forceEvade)
            {
                if (CreatureAI* ai = creature->AI())
                    ai->EnterEvadeMode(CreatureAI::EVADE_REASON_OTHER);
                else
                {
                    creature->GetThreatMgr().ResetAllThreat();
                    creature->CombatStop(true);
                }
            }
        }
    }

    uint64 BuildCombatProtectionLogToken(Player* player, uint32 currentLevel, uint32 requestedLevel)
    {
        uint64 playerGuid = player ? player->GetGUID().GetCounter() : 0;
        uint64 levelKey = (uint64(currentLevel & 0xFFFF) << 16) | uint64(requestedLevel & 0xFFFF);

        return (playerGuid << 32) ^ (playerGuid >> 1) ^ levelKey;
    }

    bool ShouldLogCombatProtection(Creature* creature, Player* player, uint32 currentLevel, uint32 requestedLevel)
    {
        uint64 creatureKey = GenerateCreatureKey(creature);
        uint64 token = BuildCombatProtectionLogToken(player, currentLevel, requestedLevel);
        std::lock_guard<std::mutex> lock(s_combatProtectionLogMutex);
        return s_combatProtectionLogKeysByCreature[creatureKey].insert(token).second;
    }

    void ClearCombatProtectionLogsForCreature(Creature* creature)
    {
        if (!creature)
            return;

        uint64 creatureKey = GenerateCreatureKey(creature);
        std::lock_guard<std::mutex> lock(s_combatProtectionLogMutex);
        s_combatProtectionLogKeysByCreature.erase(creatureKey);
    }

    std::string BuildCreatureHuanJingBroadcast(Creature* creature)
    {
        if (!creature)
            return "";

        std::string display = sHuanJingSystem->GetHealthBarDisplay(creature);
        uint32 adjustedLevel = GetCreatureAdjustedHuanJingLevel(creature);
        if (adjustedLevel == 0)
            return display;

        if (!display.empty())
            display += " ";

        display += "[幻境等级:";
        display += Acore::ToString(adjustedLevel);
        display += "]";
        return display;
    }

    void SendCreatureHuanJingBroadcastToPlayer(Player* player, Creature* creature, char const* reason = "Direct")
    {
        if (!player || !creature || !player->IsInWorld())
            return;

        Unit* selectedUnit = player->GetSelectedUnit();
        bool sameInstance = IsPlayerInCreatureInstance(player, creature);
        bool selectedThisCreature = selectedUnit == static_cast<Unit*>(creature);

        if (!sameInstance || !selectedThisCreature)
        {
            if (IsChallengeMirageDebugCreature(creature))
            {
                Creature const* selectedCreature = UnitHasCreatureType(selectedUnit) ? selectedUnit->ToCreature() : nullptr;
                LOG_DEBUG("module.huanjing", ModuleOldFormat("[ChallengeMirageDebug] HUANJING_HEALTHBAR_SEND_SKIP reason=%s action=%s entry=%s guid=%s rawGuid=%s key=%s map=%s inst=%s player=%s playerGuid=%s playerMap=%s playerInst=%s selectedEntry=%s selectedGuid=%s selectedRawGuid=%s selectedMap=%s selectedInst=%s sameInstance=%s selectedThis=%s", reason ? reason : "Direct", !sameInstance ? "different_instance" : "not_selected", creature->GetEntry(), creature->GetGUID().GetCounter(), creature->GetGUID().GetRawValue(), GenerateCreatureKey(creature), creature->GetMapId(), GetCreatureMapInstanceId(creature), player->GetName(), player->GetGUID().GetCounter(), player->GetMapId(), GetUnitMapInstanceId(player), selectedCreature ? selectedCreature->GetEntry() : 0, selectedUnit ? selectedUnit->GetGUID().GetCounter() : 0, selectedUnit ? selectedUnit->GetGUID().GetRawValue() : 0, selectedUnit ? selectedUnit->GetMapId() : 0, GetUnitMapInstanceId(selectedUnit), sameInstance ? "true" : "false", selectedThisCreature ? "true" : "false").c_str());
            }

            return;
        }

        std::string legacyHealthPayload;
        if (sHuanJingSystem->BuildCustomTargetHealthPacket(creature, legacyHealthPayload))
        {
            SendAddonMessage(player, "CustomTargetHpCount", legacyHealthPayload);
            return;
        }

        std::string display = BuildCreatureHuanJingBroadcast(creature);
        if (!display.empty())
        {
            if (IsChallengeMirageDebugCreature(creature))
            {
                LOG_DEBUG("module.huanjing", ModuleOldFormat("[ChallengeMirageDebug] HUANJING_HEALTHBAR_SEND reason=%s entry=%s guid=%s rawGuid=%s key=%s map=%s inst=%s player=%s playerGuid=%s playerMap=%s playerInst=%s display=%s", reason ? reason : "Direct", creature->GetEntry(), creature->GetGUID().GetCounter(), creature->GetGUID().GetRawValue(), GenerateCreatureKey(creature), creature->GetMapId(), GetCreatureMapInstanceId(creature), player->GetName(), player->GetGUID().GetCounter(), player->GetMapId(), GetUnitMapInstanceId(player), display).c_str());
            }

            SendHealthBarAddonMessage(player, display);
        }
    }
}

void HuanJingSystem::Initialize()
{
    _enabled = sConfigMgr->GetOption<bool>("HuanJingSystem.Enable", true);
    _debug = sConfigMgr->GetOption<bool>("HuanJingSystem.Debug", false);
    _maxLevel = sConfigMgr->GetOption<uint32>("HuanJingSystem.MaxLevel", 2000U);

    LoadHuanJingCreatureConfigs();
    LoadCreatureAttributeOverrideEntries();

    // 加载属性上限配置，防止数值溢出。
    // 0 表示不设置自定义上限，仅受对应字段自身能承载的最大值限制。
    const AsInt64 DEFAULT_HEALTH_LIMIT = 0;
    constexpr long double DEFAULT_DAMAGE_LIMIT = 0.0L;
    const AsInt64 DEFAULT_ARMOR_LIMIT = 0;
    const AsInt64 DEFAULT_ATTACK_POWER_LIMIT = 0;
    AsInt64 const DEFAULT_RESISTANCE_LIMIT = 0;
    const AsInt64 DEFAULT_MANA_LIMIT = 0;
    const AsInt64 DEFAULT_SPELL_POWER_LIMIT = 0;
    constexpr float  DEFAULT_CRIT_LIMIT = 100.0f;               // 100%暴击上限
    constexpr float  DEFAULT_HIT_LIMIT = 100.0f;                // 100%命中上限
    constexpr uint32 DEFAULT_HASTE_LIMIT = std::numeric_limits<uint32>::max();
    constexpr uint32 DEFAULT_EXPERTISE_LIMIT = std::numeric_limits<uint32>::max();
    constexpr float  DEFAULT_DODGE_LIMIT = 100.0f;              // 100%闪避上限
    constexpr float  DEFAULT_PARRY_LIMIT = 100.0f;              // 100%招架上限
    constexpr float  DEFAULT_BLOCK_LIMIT = 100.0f;              // 100%格挡上限
    constexpr float  DEFAULT_MOVEMENT_SPEED_LIMIT = 10.0f;      // 10倍移动速度上限

    AsInt64 configHealth = ParseUInt64Config(sConfigMgr->GetOption<std::string>("HuanJingSystem.MaxHealth", "0"));
    long double configDamage = ParseLongDoubleConfig(sConfigMgr->GetOption<std::string>("HuanJingSystem.MaxDamage", "0.0"), 0.0L);
    AsInt64 configArmor = ParseUInt64Config(sConfigMgr->GetOption<std::string>("HuanJingSystem.MaxArmor", "0"));
    AsInt64 configAttackPower = ParseUInt64Config(sConfigMgr->GetOption<std::string>("HuanJingSystem.MaxAttackPower", "0"));
    AsInt64 configResistance = ParseUInt64Config(sConfigMgr->GetOption<std::string>("HuanJingSystem.MaxResistance", "0"));
    AsInt64 configMana = ParseUInt64Config(sConfigMgr->GetOption<std::string>("HuanJingSystem.MaxMana", "0"));
    AsInt64 configSpellPower = ParseUInt64Config(sConfigMgr->GetOption<std::string>("HuanJingSystem.MaxSpellPower", "0"));
    float configCrit = sConfigMgr->GetOption<float>("HuanJingSystem.MaxCritChance", 0.0f);
    float configHit = sConfigMgr->GetOption<float>("HuanJingSystem.MaxHitChance", 0.0f);
    uint32 configHaste = sConfigMgr->GetOption<uint32>("HuanJingSystem.MaxHaste", 0U);
    uint32 configExpertise = sConfigMgr->GetOption<uint32>("HuanJingSystem.MaxExpertise", 0U);
    float configDodge = sConfigMgr->GetOption<float>("HuanJingSystem.MaxDodgeChance", 0.0f);
    float configParry = sConfigMgr->GetOption<float>("HuanJingSystem.MaxParryChance", 0.0f);
    float configBlock = sConfigMgr->GetOption<float>("HuanJingSystem.MaxBlockChance", 0.0f);
    float configMovementSpeed = sConfigMgr->GetOption<float>("HuanJingSystem.MaxMovementSpeed", 0.0f);

    // 0 表示不设置模块层面的自定义上限；数值最终由核心战斗字段或 float 属性字段承载能力决定。
    _maxHealthValue = NormalizeConfiguredUInt64Limit(configHealth);
    if (_maxHealthValue == 0)
        _maxHealthValue = DEFAULT_HEALTH_LIMIT;
    _maxDamageValue = NormalizeConfiguredDamageLimit(configDamage);
    if (_maxDamageValue <= 0.0L)
        _maxDamageValue = DEFAULT_DAMAGE_LIMIT;
    if (_maxDamageValue > FLOAT_DAMAGE_LIMIT)
        _maxDamageValue = FLOAT_DAMAGE_LIMIT;
    _maxArmorValue = NormalizeConfiguredUInt64Limit(configArmor);
    if (_maxArmorValue == 0)
        _maxArmorValue = DEFAULT_ARMOR_LIMIT;
    _maxAttackPowerValue = NormalizeConfiguredUInt64Limit(configAttackPower);
    if (_maxAttackPowerValue == 0)
        _maxAttackPowerValue = DEFAULT_ATTACK_POWER_LIMIT;
    _maxResistanceValue = NormalizeConfiguredUInt64Limit(configResistance);
    if (_maxResistanceValue == 0)
        _maxResistanceValue = DEFAULT_RESISTANCE_LIMIT;
    _maxManaValue = NormalizeConfiguredUInt64Limit(configMana);
    if (_maxManaValue == 0)
        _maxManaValue = DEFAULT_MANA_LIMIT;
    _maxSpellPowerValue = NormalizeConfiguredUInt64Limit(configSpellPower);
    if (_maxSpellPowerValue == 0)
        _maxSpellPowerValue = DEFAULT_SPELL_POWER_LIMIT;
    _maxCritChance = (configCrit > 0.0f) ? configCrit : DEFAULT_CRIT_LIMIT;
    _maxHitChance = (configHit > 0.0f) ? configHit : DEFAULT_HIT_LIMIT;
    _maxHaste = (configHaste > 0) ? configHaste : DEFAULT_HASTE_LIMIT;
    _maxExpertise = (configExpertise > 0) ? configExpertise : DEFAULT_EXPERTISE_LIMIT;
    _maxDodgeChance = (configDodge > 0.0f) ? configDodge : DEFAULT_DODGE_LIMIT;
    _maxParryChance = (configParry > 0.0f) ? configParry : DEFAULT_PARRY_LIMIT;
    _maxBlockChance = (configBlock > 0.0f) ? configBlock : DEFAULT_BLOCK_LIMIT;
    _maxMovementSpeed = (configMovementSpeed > 0.0f) ? configMovementSpeed : DEFAULT_MOVEMENT_SPEED_LIMIT;

    // 初始化清理系统
    _lastCleanupTime = 0;
    _cleanupInterval = 3600; // 每小时清理一次

    // 启动时执行一次清理
    ScheduleItemCleanup();
}

void HuanJingSystem::LoadHuanJingLevelConfigs()
{
    _levelConfigs.clear();
}

HuanJingLevelConfig HuanJingSystem::GenerateDefaultLevelConfig(uint32 level)
{
    HuanJingLevelConfig config;
    config.level = level;
    config.name = "幻境" + Acore::ToString(level) + "级";
    config.requiredExp = 0;
    config.needTemplateId = 0;
    config.buff = "";
    config.comment = "按 HuanJingSystem.MaxLevel 动态生成";
    return config;
}

bool HuanJingSystem::EnsureLevelConfigExists(uint32 level)
{
    if (level == 0)
        return true;

    if (level > _maxLevel)
        return false;

    if (_levelConfigs.find(level) != _levelConfigs.end())
        return true;

    _levelConfigs.emplace(level, GenerateDefaultLevelConfig(level));
    LOG_DEBUG("module.huanjing", ModuleOldFormat("动态生成幻境等级配置: 等级=%s, MaxLevel=%s", level, _maxLevel).c_str());
    return true;
}

void HuanJingSystem::LoadHuanJingCreatureConfigs()
{
    _creatureConfigs.clear();

    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        _resolvedCreatureConfigs.clear();
    }

    // 配置即将重建，缓存中的指针全部失效，必须先清空
    {
        std::lock_guard<std::mutex> lock(_configMatchCacheMutex);
        _configMatchCache.clear();
    }

    if (!sHelper->GetBoolean(22010))
    {
        ClearHealthBarData();
        LOG_INFO("module", ">> _模板_地图生物控制 已关闭");
        return;
    }

    // 检查表是否存在
    QueryResult checkResult = WorldDatabase.Query(ModuleOldFormat("SHOW TABLES LIKE '%s'", MAP_CREATURE_CONTROL_TABLE).c_str());
    if (!checkResult)
    {
        LOG_WARN("module", ModuleOldFormat(">> %s 表不存在，跳过加载", MAP_CREATURE_CONTROL_TABLE).c_str());
        return;
    }

    std::string query = ModuleOldFormat(
        "SELECT 配置ID, 生物ID, 生物类型, 地图ID, 区域ID, 难度, 秘境难度, 生物控制组, "
        "提示方式, 刷新公告, 刷新时间, 动态难度开关, 战力门槛, 难度倍率 "
        "FROM %s ORDER BY 配置ID", MAP_CREATURE_CONTROL_TABLE);

    QueryResult result = WorldDatabase.Query(query.c_str());

    if (!result)
    {
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        HuanJingCreatureConfig config;
        uint8 index = 0;
        config.configId = fields[index++].Get<uint32>();
        config.creatureId = fields[index++].Get<int32>();
        config.creatureType = fields[index++].Get<int32>();
        config.mapId = fields[index++].Get<int32>();
        config.areaId = fields[index++].Get<int32>();
        config.difficulty = fields[index++].Get<int32>();
        config.mythicDifficulty = fields[index++].Get<std::string>();
        config.allMythicDifficulties = ParseMythicDifficulties(config.mythicDifficulty, config.mythicDifficulties);
        if (!config.allMythicDifficulties && config.mythicDifficulties.empty())
        {
            LOG_WARN("module.huanjing", ModuleOldFormat("地图生物配置 %s 的秘境难度 '%s' 无效，该配置不会匹配", config.configId, config.mythicDifficulty).c_str());
        }
        config.creatureGroupId = fields[index++].Get<uint32>();
        config.noticeMode = fields[index++].Get<uint32>();
        config.respawnMessage = fields[index++].Get<std::string>();
        config.respawnTime = fields[index++].Get<uint32>();
        config.dynamicDifficultyEnabled = fields[index++].Get<uint8>() != 0;
        config.combatPowerThreshold = AsInt64FromString(fields[index++].Get<std::string>());
        config.difficultyMultiplier = fields[index++].Get<float>();
        if (!std::isfinite(config.difficultyMultiplier) || config.difficultyMultiplier <= 0.0f)
            config.difficultyMultiplier = 1.0f;

        _creatureConfigs.push_back(config);
        ++count;
    } while (result->NextRow());

    LOG_INFO("module", ModuleOldFormat(">> %s 加载完成，共 %s 条路由配置", MAP_CREATURE_CONTROL_TABLE, count).c_str());
}

void HuanJingSystem::LoadCreatureAttributeOverrideEntries()
{
    std::unordered_set<uint32> overrideEntries;

    QueryResult checkResult = WorldDatabase.Query("SHOW TABLES LIKE '_属性调整_生物'");
    if (!checkResult)
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        _creatureAttributeOverrideEntries.clear();
        return;
    }

    QueryResult creatureIdColumn = WorldDatabase.Query(
        "SELECT 1 FROM information_schema.columns "
        "WHERE table_schema = DATABASE() AND table_name = '_属性调整_生物' "
        "AND column_name = '生物id' LIMIT 1");
    if (!creatureIdColumn)
    {
        // Newer NAz tables store attribute templates by id and do not map rows directly to creature entries.
        LOG_WARN("module.huanjing", "表 _属性调整_生物 缺少兼容字段 生物id，跳过生物属性覆盖索引");
        std::lock_guard<std::mutex> lock(_creatureMutex);
        _creatureAttributeOverrideEntries.clear();
        return;
    }

    QueryResult result = WorldDatabase.Query(
        "SELECT DISTINCT `生物id` FROM `_属性调整_生物` "
        "WHERE `生物id` > 0 AND ("
        "`血量值` > 0 OR (`血量百分比` > 0 AND `血量百分比` <> 100) OR "
        "(`血条数量` IS NOT NULL AND TRIM(`血条数量`) <> '' AND TRIM(`血条数量`) <> '0') OR "
        "`物理攻击最小值` > 0 OR `物理攻击最大值` > 0 OR (`物理攻击百分比` > 0 AND `物理攻击百分比` <> 100) OR "
        "`魔法攻击最小值` > 0 OR `魔法攻击最大值` > 0 OR (`魔法攻击百分比` > 0 AND `魔法攻击百分比` <> 100) OR "
        "`DOT攻击最小值` > 0 OR `DOT攻击最大值` > 0 OR (`DOT攻击百分比` > 0 AND `DOT攻击百分比` <> 100) OR "
        "`真实伤害值` > 0 OR `被攻击掉血上限` > 0 OR "
        "`物理攻击切割伤害值` > 0 OR `物理攻击切割伤害百分比` > 0 OR "
        "`魔法攻击切割伤害值` > 0 OR `魔法攻击切割伤害百分比` > 0 OR "
        "`DOT攻击切割伤害值` > 0 OR `DOT攻击切割伤害百分比` > 0"
        ")");

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 creatureId = fields[0].Get<uint32>();
            if (creatureId > 0)
                overrideEntries.insert(creatureId);
        } while (result->NextRow());
    }

    size_t overrideCount = overrideEntries.size();
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        _creatureAttributeOverrideEntries = std::move(overrideEntries);
    }
}

void HuanJingSystem::LoadPlayerData(Player* player)
{
    // 幻境等级/经验存档已移除，地图配置直接按地图/区域/生物条件生效。
    (void)player;
}


void HuanJingSystem::SavePlayerData(Player* player)
{
    // 幻境等级/经验存档已移除，无需保存旧等级数据。
    (void)player;
}

// 【线程安全修复】返回值拷贝而非裸指针，避免锁释放后悬空指针风险
std::optional<PlayerHuanJingData> HuanJingSystem::GetPlayerData(uint32 playerGuid)
{
    (void)playerGuid;
    return std::nullopt;
}

std::optional<PlayerHuanJingData> HuanJingSystem::GetPlayerData(uint32 playerGuid) const
{
    (void)playerGuid;
    return std::nullopt;
}

uint32 HuanJingSystem::GetPlayerHuanJingLevel(Player* player) const
{
    if (!player)
        return 0;

    // 幻境地图控制已改为按地图直接生效，不再要求玩家手动设置幻境等级。
    return 1;
}

uint32 HuanJingSystem::GetPlayerHuanJingExp(Player* player)
{
    (void)player;
    return 0;
}

void HuanJingSystem::SetPlayerHuanJingLevel(Player* player, uint32 level)
{
    (void)player;
    (void)level;
}

void HuanJingSystem::SetPlayerHuanJingExp(Player* player, uint32 exp)
{
    (void)player;
    (void)exp;
}

void HuanJingSystem::AddPlayerHuanJingExp(Player* player, uint32 exp)
{
    (void)player;
    (void)exp;
}

void HuanJingSystem::SetPlayerHuanJingBuff(Player* player, const std::string& buff)
{
    (void)player;
    (void)buff;
}

HuanJingLevelConfig* HuanJingSystem::GetLevelConfig(uint32 level)
{
    auto it = _levelConfigs.find(level);
    return it != _levelConfigs.end() ? &it->second : nullptr;
}

const std::unordered_map<uint32, HuanJingLevelConfig>& HuanJingSystem::GetAllLevelConfigs() const
{
    return _levelConfigs;
}

uint32 HuanJingSystem::GetCreatureConfigCount() const
{
    return static_cast<uint32>(_creatureConfigs.size());
}

uint32 HuanJingSystem::ReloadMapCreatureConfigsAndRefresh(Player* player, float range)
{
    LoadHuanJingCreatureConfigs();
    LoadCreatureAttributeOverrideEntries();

    if (!sHelper->GetBoolean(22010) || !player || !player->IsInWorld())
        return 0;

    std::list<Creature*> creatures;

    class AllAliveCreaturesInRange
    {
    public:
        AllAliveCreaturesInRange(WorldObject const* object, float maxRange) : _object(object), _range(maxRange) { }

        bool operator()(Creature* creature)
        {
            return creature && creature->IsAlive() && _object->IsWithinDist(creature, _range, false);
        }

    private:
        WorldObject const* _object;
        float _range;
    };

    AllAliveCreaturesInRange check(player, range);
    Acore::CreatureListSearcher<AllAliveCreaturesInRange> searcher(player, creatures, check);
    Cell::VisitGridObjects(player, searcher, range);

    uint32 refreshedCount = 0;
    for (Creature* creature : creatures)
    {
        if (!creature || !creature->IsAlive() || creature->IsInCombat())
            continue;

        // A configuration reload must not be short-circuited by the previous level record.
        if (GetCreatureAdjustedHuanJingLevel(creature) > 0)
            RestoreCreatureToOriginalStats(creature);

        AdjustCreatureStatsWithHighestLevel(creature, range);
        ++refreshedCount;
    }

    LOG_INFO("module.huanjing", ModuleOldFormat(
        ">> 地图生物配置已重载，已刷新玩家 %s 附近 %s 个非战斗生物",
        player->GetName(), refreshedCount).c_str());

    return refreshedCount;
}

bool HuanJingSystem::HasCreatureAttributeOverride(uint32 creatureId) const
{
    if (creatureId == 0)
        return false;

    std::lock_guard<std::mutex> lock(_creatureMutex);
    return _creatureAttributeOverrideEntries.find(creatureId) != _creatureAttributeOverrideEntries.end();
}

bool HuanJingSystem::HasCreatureAttributeOverride(Creature const* creature) const
{
    return creature && HasCreatureAttributeOverride(creature->GetEntry());
}

namespace
{
void ApplyCreatureAttributeTemplate(HuanJingCreatureConfig& config, AtCreatureStatTemp const& attribute)
{
    config.attributeTemplateId = 0;
    config.minLevel = attribute.MinLevel >= 0 ? attribute.MinLevel : (attribute.Level > 0 ? int32(attribute.Level) : -1);
    config.maxLevel = attribute.MaxLevel >= 0 ? attribute.MaxLevel : config.minLevel;
    config.healthMultiplier = attribute.HealthMultiplier;
    config.healthValue = ToUInt64FromAsInt64(attribute.NowHp);
    config.manaMultiplier = attribute.ManaMultiplier;
    config.manaValue = ToUInt64FromAsInt64(attribute.ManaValue);
    config.minDamageMultiplier = attribute.MinDamageMultiplier;
    config.minDamageValue = ToUInt64FromAsInt64(attribute.MinDamageValue);
    config.maxDamageMultiplier = attribute.MaxDamageMultiplier;
    config.maxDamageValue = ToUInt64FromAsInt64(attribute.MaxDamageValue);
    config.damageTakenValue = ToUInt64FromAsInt64(attribute.DmgTakenNow);
    config.trueDamageValue = ToUInt64FromAsInt64(attribute.TrueDamageValue);
    config.armorMultiplier = attribute.ArmorMultiplier;
    config.armorValue = ToUInt64FromAsInt64(attribute.Armor);
    config.holyResistValue = attribute.School1 > 0 ? AsInt64(attribute.School1) : 0;
    config.fireResistMultiplier = attribute.FireResistMultiplier;
    config.fireResistValue = attribute.School2 > 0 ? AsInt64(attribute.School2) : 0;
    config.natureResistMultiplier = attribute.NatureResistMultiplier;
    config.natureResistValue = attribute.School3 > 0 ? AsInt64(attribute.School3) : 0;
    config.frostResistMultiplier = attribute.FrostResistMultiplier;
    config.frostResistValue = attribute.School4 > 0 ? AsInt64(attribute.School4) : 0;
    config.shadowResistMultiplier = attribute.ShadowResistMultiplier;
    config.shadowResistValue = attribute.School5 > 0 ? AsInt64(attribute.School5) : 0;
    config.arcaneResistMultiplier = attribute.ArcaneResistMultiplier;
    config.arcaneResistValue = attribute.School6 > 0 ? AsInt64(attribute.School6) : 0;
    config.attackPowerMultiplier = attribute.AttackPowerMultiplier;
    config.attackPowerValue = ToUInt64FromAsInt64(attribute.AttackPowerValue);
    config.spellPowerMultiplier = attribute.SpellPowerMultiplier;
    config.spellPowerValue = ToUInt64FromAsInt64(attribute.SpellPowerValue);
    config.moveSpeedMultiplier = attribute.MoveSpeedMultiplier;
    config.moveSpeedValue = attribute.MoveSpeedValue;
    config.attackSpeedMultiplier = attribute.AttackSpeedMultiplier;
    config.attackSpeedValue = attribute.AttackSpeedValue;
    config.castSpeedMultiplier = attribute.CastSpeedMultiplier;
    config.castSpeedValue = attribute.CastSpeedValue;
    // Attribute templates keep their configured bar count, but runtime multi-bar
    // damage and display state is owned by HuanJingSystem.
    config.healthBarCount = attribute.HpCount;
    config.hitChanceBonus = attribute.HitChanceBonus;
    config.armorPenetration = attribute.ArmorPenetration;
    config.armorPenetrationPct = attribute.ArmorPenetrationPct;
    config.blockPenetration = attribute.BlockPenetration;
    config.blockPenetrationPct = attribute.BlockPenetrationPct;
    config.damageReductionPct = attribute.DamageReductionPct;
    config.officialDrop = attribute.OfficialDrop;
    config.lootIds = attribute.PLootIds;
    config.equipAttrMultiplier = attribute.EquipAttrMultiplier;
    config.equipAttrValue = attribute.EquipAttrValue;
    config.equipAttrMode = DbValueToItemAttributeMode(attribute.EquipAttrMode);
    config.identificationGroup = attribute.IdentificationGroup;
    config.identificationQuality = attribute.IdentificationQuality;
    config.identificationItemLevel = attribute.IdentificationItemLevel;
}

OneCreGroupTemp const* SelectCreatureGroupTemplate(uint32 groupId)
{
    OneCreGroupTemp const* first = nullptr;
    OneCreGroupTemp const* lastPositive = nullptr;
    float totalChance = 0.0f;

    for (OneCreGroupTemp const& entry : OneCreGroupStore)
    {
        if (entry.GroupId != groupId)
            continue;

        if (!first)
            first = &entry;

        if (std::isfinite(entry.Chance) && entry.Chance > 0.0f)
        {
            totalChance += entry.Chance;
            lastPositive = &entry;
        }
    }

    if (!first || totalChance <= 0.0f)
        return first;

    float roll = frand(0.0f, totalChance);
    for (OneCreGroupTemp const& entry : OneCreGroupStore)
    {
        if (entry.GroupId != groupId || !std::isfinite(entry.Chance) || entry.Chance <= 0.0f)
            continue;

        if (roll <= entry.Chance)
            return &entry;

        roll -= entry.Chance;
    }

    return lastPositive;
}
}

void HuanJingSystem::ApplyResolvedCreatureAppearance(Creature* creature, HuanJingCreatureConfig const& config, bool applyAuras)
{
    if (!creature)
        return;

    creature->StTempId = config.attributeTemplateId;

    // Reuse the original __模版生物属性表 bar model and CustomTargetHealth UI.
    // The configured health value remains the health of one bar.
    RemoveCreatureHealthBars(creature);
    creature->MultipleHp = 1;
    creature->MaxMultipleHp = 1;

    // Map routing resolves after the creature's original SelectLevel() call.
    // Recalculate immediately so the selected template is already reflected in
    // the first health update instead of waiting for target selection or damage.
    if (config.attributeTemplateId > 0 && creature->IsAlive())
    {
        creature->SelectLevel(true);
        creature->MultipleHp = 1;
        creature->MaxMultipleHp = 1;
    }

    if (config.displayId > 0)
    {
        creature->SetDisplayId(config.displayId);
        creature->SetNativeDisplayId(config.displayId);
    }

    if (config.displayScale > 0.0f)
        creature->SetObjectScale(config.displayScale);

    if (config.weapon1 > 0)
        creature->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID, config.weapon1);
    if (config.weapon2 > 0)
        creature->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 1, config.weapon2);
    if (config.weapon3 > 0)
        creature->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 2, config.weapon3);

    if (applyAuras && config.attributeTemplateId > 0)
        sAsCreature->DoAsAddAuras(creature);
}

HuanJingCreatureConfig const* HuanJingSystem::ResolveCreatureConfig(Creature* creature, HuanJingCreatureConfig const* routeConfig)
{
    if (!creature || !routeConfig)
        return nullptr;

    uint64 creatureKey = GenerateCreatureKey(creature);
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        auto existing = _resolvedCreatureConfigs.find(creatureKey);
        if (existing != _resolvedCreatureConfigs.end())
            return &existing->second;
    }

    HuanJingCreatureConfig resolved = *routeConfig;
    if (resolved.creatureGroupId > 0)
    {
        if (OneCreGroupTemp const* selected = SelectCreatureGroupTemplate(resolved.creatureGroupId))
        {
            resolved.attributeTemplateId = selected->StTempId;
            resolved.displayId = selected->ModeId;
            resolved.displayScale = selected->Scale;
            resolved.weapon1 = selected->EqWp1;
            resolved.weapon2 = selected->EqWp2;
            resolved.weapon3 = selected->EqWp3;

            auto attribute = AtCreatureStatTempStore.find(selected->StTempId);
            if (attribute != AtCreatureStatTempStore.end())
            {
                ApplyCreatureAttributeTemplate(resolved, attribute->second);
                resolved.attributeTemplateId = selected->StTempId;
            }
            else if (selected->StTempId > 0)
            {
                LOG_WARN("module.huanjing", ModuleOldFormat(
                    "地图生物配置 %s 的控制组 %s 引用了不存在的属性模板 %s",
                    resolved.configId, resolved.creatureGroupId, selected->StTempId).c_str());
            }
        }
        else
        {
            LOG_WARN("module.huanjing", ModuleOldFormat(
                "地图生物配置 %s 引用了空的生物控制组 %s",
                resolved.configId, resolved.creatureGroupId).c_str());
        }
    }

    HuanJingCreatureConfig const* result = nullptr;
    bool inserted = false;
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        auto value = _resolvedCreatureConfigs.emplace(creatureKey, std::move(resolved));
        result = &value.first->second;
        inserted = value.second;
    }

    if (inserted && !HasCreatureAttributeOverride(creature))
        ApplyResolvedCreatureAppearance(creature, *result, true);

    return result;
}

void HuanJingSystem::ForgetResolvedCreatureConfig(Creature const* creature)
{
    if (!creature)
        return;

    std::lock_guard<std::mutex> lock(_creatureMutex);
    _resolvedCreatureConfigs.erase(GenerateCreatureKey(creature));
}

HuanJingCreatureConfig const* HuanJingSystem::GetResolvedCreatureConfig(Creature* creature) const
{
    if (!creature || !sHelper->GetBoolean(22010))
        return nullptr;

    std::lock_guard<std::mutex> lock(_creatureMutex);
    auto found = _resolvedCreatureConfigs.find(GenerateCreatureKey(creature));
    return found != _resolvedCreatureConfigs.end() ? &found->second : nullptr;
}

HuanJingCreatureConfig const* HuanJingSystem::GetCreatureLootConfig(Creature* creature) const
{
    // Loot reads the resolved copy only: it must not re-roll the control group
    // or reapply appearance when the corpse is looted.
    if (HuanJingCreatureConfig const* resolved = GetResolvedCreatureConfig(creature))
    {
        if (resolved->attributeTemplateId > 0)
            return resolved;
    }

    // A creature that never reached the route resolver (spawned before the
    // tables finished loading) still resolves once here.
    if (HuanJingCreatureConfig const* config = GetMapCreatureConfig(creature, nullptr))
    {
        if (config->attributeTemplateId > 0)
            return config;
    }

    return nullptr;
}

void HuanJingSystem::InvalidateResolvedCreatureConfigs()
{
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        _resolvedCreatureConfigs.clear();
    }

    std::lock_guard<std::mutex> cacheLock(_configMatchCacheMutex);
    _configMatchCache.clear();
}

bool HuanJingSystem::CreatureUsesDedicatedStats(Creature* creature, Player* player) const
{
    HuanJingCreatureConfig const* config = GetCreatureConfig(creature, player);
    if (!config)
        return false;

    return config->creatureId != -1 || config->mapId != -1 || config->areaId != -1;
}

const HuanJingCreatureConfig* HuanJingSystem::GetCreatureConfig(Creature* creature, Player* player) const
{
    if (!creature)
        return nullptr;

    if (HasCreatureAttributeOverride(creature))
        return nullptr;

    return GetMapCreatureConfig(creature, player);
}

const HuanJingCreatureConfig* HuanJingSystem::GetMapCreatureConfig(Creature* creature, Player* player) const
{
    if (!sHelper->GetBoolean(22010))
        return nullptr;

    if (!creature || !creature->IsInWorld() || creature->IsPet() || creature->IsControlledByPlayer())
        return nullptr;

    if (IsHuanJingExcludedCreature(creature))
        return nullptr;

    if (player && !IsHuanJingCreatureEligibleForPlayer(creature, player))
        return nullptr;

    uint32 huanJingLevel = player ? GetPlayerHuanJingLevel(player) : 1;
    uint32 creatureType = creature->IsDungeonBoss() ? 3 : GetCreatureTypeByEntry(creature->GetEntry());

    HuanJingCreatureConfig* routeConfig = const_cast<HuanJingSystem*>(this)->GetBestMatchConfig(
        huanJingLevel,
        creature->GetEntry(),
        creature->GetMapId(),
        creature->GetAreaId(),
        GetCreatureMapDifficulty(creature),
        GetCreatureMythicDifficulty(creature),
        creature->GetLevel(),
        creatureType);

    return const_cast<HuanJingSystem*>(this)->ResolveCreatureConfig(creature, routeConfig);
}

void HuanJingSystem::AdjustCreatureStats(Creature* creature, Player* player, bool allowInCombat)
{
    if (!sHelper->GetBoolean(22010))
        return;

    if (!creature || !player)
        return;

    if (HasCreatureAttributeOverride(creature))
    {
        if (!creature->IsInCombat() && GetCreatureAdjustedHuanJingLevel(creature) > 0)
            RestoreCreatureToOriginalStats(creature);
        else
        {
            CleanupCreatureAdjustmentData(creature);
            ClearCreatureVirtualMana(creature->GetGUID());
            ClearCreatureSpellPower(creature);
        }

        EnsureCreatureHealthBarsFromConfig(creature, player, false);
        LOG_DEBUG("module.huanjing", ModuleOldFormat(">> [幻境系统] 生物 %s 已由 _属性调整_生物 接管基础属性，跳过幻境属性调整；血条仍由幻境血条系统处理", creature->GetEntry()).c_str());
        return;
    }

    if (!IsHuanJingCreatureEligibleForPlayer(creature, player))
    {
        LOG_DEBUG("module.huanjing", ModuleOldFormat(">> [幻境系统] 跳过不可被玩家攻击的生物: 玩家=%s, 生物=%s Entry=%s", player->GetName(), creature->GetName(), creature->GetEntry()).c_str());
        return;
    }

    if (creature->IsInCombat() && !allowInCombat)
    {
        uint32 requestedLevel = GetPlayerHuanJingLevel(player);
        if (ShouldLogCombatProtection(creature, player, GetCreatureAdjustedHuanJingLevel(creature), requestedLevel))
        {
            SendCreatureHuanJingBroadcastToPlayer(player, creature, "CombatProtection");
        }
        return;
    }

    if (!CreatureUsesDedicatedStats(creature, player))
    {
        if (!creature->IsInCombat() && GetCreatureAdjustedHuanJingLevel(creature) > 0)
            RestoreCreatureToOriginalStats(creature);
        return;
    }

    uint64 creatureKey = GenerateCreatureKey(creature);

    // 递归保护：如果这个生物正在被调整，直接返回（需要在锁内检查和插入）
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        if (_adjustingCreatures.find(creatureKey) != _adjustingCreatures.end())
        {
            return;
        }
        // 标记生物正在调整
        _adjustingCreatures.insert(creatureKey);
    }

    uint32 huanJingLevel = GetPlayerHuanJingLevel(player);

    // 如果玩家幻境等级为0，还原生物到原始属性
    if (huanJingLevel == 0)
    {
        RestoreCreatureToOriginalStats(creature);
        {
            std::lock_guard<std::mutex> lock(_creatureMutex);
            _adjustingCreatures.erase(creatureKey);
        }
        return;
    }

    // 关键修复：每次调整前先强制还原到原版状态，确保倍率计算基于原版属性
    // 清除旧的血条数据
    ObjectGuid guid = creature->GetGUID();
    RemoveCreatureHealthBars(creature);

    // 清除幻境系统的调整标记，防止重复调整
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        _creatureAdjustedLevels.erase(creatureKey);
    }

    // 获取生物模板信息，用于计算真正的原始基础值
    const CreatureTemplate* cInfo = creature->GetCreatureTemplate();
    if (!cInfo) {
        LOG_ERROR("module", ModuleOldFormat(">> 生物 %s 无法获取模板信息", creature->GetEntry()).c_str());
        {
            std::lock_guard<std::mutex> lock(_creatureMutex);
            _adjustingCreatures.erase(creatureKey);
        }
        return;
    }

    // 获取基础统计数据
    CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(creature->GetLevel(), cInfo->unit_class);
    if (!stats) {
        LOG_ERROR("module", ModuleOldFormat(">> 生物 %s 无法获取基础统计数据", creature->GetEntry()).c_str());
        {
            std::lock_guard<std::mutex> lock(_creatureMutex);
            _adjustingCreatures.erase(creatureKey);
        }
        return;
    }

    // 重新初始化生物到原始状态
    creature->SelectLevel(true);

    // 重置命中率相关属性（防止重复调用累加）
    creature->m_modMeleeHitChance = 0.0f;
    creature->m_modRangedHitChance = 0.0f;
    creature->m_modSpellHitChance = 0.0f;

    // 移除所有相关光环
    creature->RemoveAurasByType(SPELL_AURA_MOD_HIT_CHANCE);
    creature->RemoveAurasByType(SPELL_AURA_MOD_TARGET_RESISTANCE);
    creature->RemoveAurasByType(SPELL_AURA_MOD_EXPERTISE);

    // 清除法术强度记录
    ClearCreatureSpellPower(creature);

    // 强制重置所有修改器为原始基础值，确保没有任何额外修改
    // 血量
    AsInt64 originalHealth = ToUInt64FromAsInt64(stats->GenerateAsHealth(cInfo));
    creature->SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, SafeFloatValue(originalHealth, std::numeric_limits<uint32>::max()));
    creature->SetModifierValue(UNIT_MOD_HEALTH, BASE_PCT, 1.0f);
    creature->SetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE, 0.0f);
    creature->SetModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT, 1.0f);

    // 护甲
    float originalArmor = stats->BaseArmor * cInfo->ModArmor;
    creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, originalArmor);
    creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_PCT, 1.0f);
    creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE, 0.0f);
    creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT, 1.0f);

    // 伤害
    float originalBaseDamage = stats->BaseDamage[cInfo->expansion];
    creature->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, originalBaseDamage);
    creature->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, originalBaseDamage * 1.5f);

    // 攻击强度
    creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, Acore::Number::ToFloat(stats->AttackPower));
    creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_PCT, 1.0f);
    creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, 0.0f);
    creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, 1.0f);

    LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 强制重置为原始基础值: 血量=%s, 护甲=%s, 伤害=%s, 攻击强度=%s", creature->GetEntry(), FormatUInt64(originalHealth), originalArmor, originalBaseDamage, FormatUInt64(stats->AttackPower)).c_str());

    // 移除护甲恢复逻辑 - 已禁用
    // if (player)
    // {
    //     uint32 playerGuid = player->GetGUID().GetCounter();
    //     auto it = _playerArmorReductions.find(playerGuid);
    //     if (it != _playerArmorReductions.end())
    //     {
    //         // 恢复原始护甲值
    //         player->SetArmor(it->second);
    //
    //         // 移除记录
    //         _playerArmorReductions.erase(it);
    //
    //         LOG_DEBUG("module", ">> 恢复玩家 {} 护甲: {}", playerGuid, it->second);
    //     }
    // }

    // 移除格挡率修改逻辑 - 已禁用
    // auto blockIt = _playerBlockReductions.find(playerGuid);
    // if (blockIt != _playerBlockReductions.end())
    // {
    //     // 恢复原始格挡率
    //     player->SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, blockIt->second);
    //
    //     // 移除记录
    //     _playerBlockReductions.erase(blockIt);
    //
    //     LOG_DEBUG("module", ">> 恢复玩家 {} 格挡率: {:.1f}%", playerGuid, blockIt->second);
    // }

    // 更新属性以应用还原
    creature->UpdateAllStats();
    SetCreatureCombatHealth(creature, GetCreatureCombatMaxHealth(creature));
    SetCreatureCombatPower(creature, POWER_MANA, GetCreatureCombatMaxPower(creature, POWER_MANA));
    uint32 creatureId = creature->GetEntry();
    uint32 mapId = creature->GetMapId();
    uint32 areaId = creature->GetAreaId();
    uint32 difficulty = GetCreatureMapDifficulty(creature);
    uint32 creatureLevel = creature->GetLevel();
    uint32 creatureType = sHuanJingSystem->GetCreatureTypeByEntry(creatureId);


    HuanJingCreatureConfig const* config = GetCreatureConfig(creature, player);
    long double contentDifficultyScale = CalculateCreatureContentDifficultyScale(creature);
    long double dynamicDifficultyScale = config ? GetDynamicDifficultyScale(*config, player) : 1.0L;
    HuanJingCreatureConfig scaledConfig;
    if (config)
    {
        auto scaleMultiplier = [contentDifficultyScale, dynamicDifficultyScale](float multiplier) -> float
        {
            if (!std::isfinite(multiplier) || multiplier <= 0.0f)
                return 1.0f;

            long double contentScaled = 1.0L + (static_cast<long double>(multiplier) - 1.0L) * contentDifficultyScale;
            long double scaled = contentScaled * dynamicDifficultyScale;
            return static_cast<float>(ClampLongDouble(scaled, 0.01L, 1000000.0L));
        };

        auto scalePercent = [contentDifficultyScale, dynamicDifficultyScale](float value) -> float
        {
            if (!std::isfinite(value))
                return 0.0f;

            return static_cast<float>(static_cast<long double>(value) * contentDifficultyScale * dynamicDifficultyScale);
        };

        auto scaleFixed = [dynamicDifficultyScale](AsInt64 const& value) -> AsInt64
        {
            return ScaleAsInt64ByDouble(value, static_cast<double>(dynamicDifficultyScale));
        };

        auto scaleUInt64 = [dynamicDifficultyScale](AsInt64 value) -> AsInt64
        {
            return ScaleAsInt64ByDouble(value, static_cast<double>(dynamicDifficultyScale));
        };

        auto scaleUInt32 = [dynamicDifficultyScale](uint32 value) -> uint32
        {
            long double scaled = static_cast<long double>(value) * dynamicDifficultyScale;
            if (scaled <= 0.0L)
                return 0;
            if (scaled >= static_cast<long double>(std::numeric_limits<uint32>::max()))
                return std::numeric_limits<uint32>::max();
            return static_cast<uint32>(scaled);
        };

        scaledConfig = *config;
        scaledConfig.healthMultiplier = scaleMultiplier(config->healthMultiplier);
        scaledConfig.manaMultiplier = scaleMultiplier(config->manaMultiplier);
        scaledConfig.minDamageMultiplier = scaleMultiplier(config->minDamageMultiplier);
        scaledConfig.maxDamageMultiplier = scaleMultiplier(config->maxDamageMultiplier);
        scaledConfig.armorMultiplier = scaleMultiplier(config->armorMultiplier);
        scaledConfig.fireResistMultiplier = scaleMultiplier(config->fireResistMultiplier);
        scaledConfig.natureResistMultiplier = scaleMultiplier(config->natureResistMultiplier);
        scaledConfig.frostResistMultiplier = scaleMultiplier(config->frostResistMultiplier);
        scaledConfig.shadowResistMultiplier = scaleMultiplier(config->shadowResistMultiplier);
        scaledConfig.arcaneResistMultiplier = scaleMultiplier(config->arcaneResistMultiplier);
        scaledConfig.attackPowerMultiplier = scaleMultiplier(config->attackPowerMultiplier);
        scaledConfig.spellPowerMultiplier = scaleMultiplier(config->spellPowerMultiplier);
        scaledConfig.hitChanceBonus = scalePercent(config->hitChanceBonus);
        scaledConfig.armorPenetrationPct = scalePercent(config->armorPenetrationPct);
        scaledConfig.blockPenetrationPct = scalePercent(config->blockPenetrationPct);
        scaledConfig.healthValue = scaleFixed(config->healthValue);
        scaledConfig.manaValue = scaleFixed(config->manaValue);
        scaledConfig.minDamageValue = scaleFixed(config->minDamageValue);
        scaledConfig.maxDamageValue = scaleFixed(config->maxDamageValue);
        scaledConfig.trueDamageValue = SaturateToUInt64(Acore::Number::ToLongDouble(config->trueDamageValue) * contentDifficultyScale * dynamicDifficultyScale);
        scaledConfig.armorValue = scaleFixed(config->armorValue);
        scaledConfig.attackPowerValue = scaleFixed(config->attackPowerValue);
        scaledConfig.spellPowerValue = scaleFixed(config->spellPowerValue);
        scaledConfig.holyResistValue = scaleUInt64(config->holyResistValue);
        scaledConfig.fireResistValue = scaleUInt64(config->fireResistValue);
        scaledConfig.natureResistValue = scaleUInt64(config->natureResistValue);
        scaledConfig.frostResistValue = scaleUInt64(config->frostResistValue);
        scaledConfig.shadowResistValue = scaleUInt64(config->shadowResistValue);
        scaledConfig.arcaneResistValue = scaleUInt64(config->arcaneResistValue);
        scaledConfig.armorPenetration = scaleUInt32(config->armorPenetration);
        scaledConfig.blockPenetration = scaleUInt32(config->blockPenetration);
        config = &scaledConfig;
    }

    // 没有专属配置的生物已在函数入口返回。这里只处理带具体生物、地图或区域的配置。
    bool useBasicAdjustment = (config == nullptr);
    AsInt64 configuredHealthForCombat = 0;

    LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 调整模式: %s (配置指针: %s)", creature->GetEntry(), useBasicAdjustment ? "基础调整" : "配置调整", config ? "有配置" : "无配置").c_str());

    // 记录调整前的属性
    uint32 oldLevel = creature->GetLevel();
    uint32 oldHealth = creature->GetMaxHealth();
    uint32 oldMana = creature->GetMaxPower(POWER_MANA);

    // 首先调整生物等级（如果配置了等级范围）
    if (!useBasicAdjustment && config->minLevel != -1 && config->maxLevel != -1)
    {
        uint32 newLevel;
        if (config->minLevel == config->maxLevel)
        {
            // 固定等级
            newLevel = config->minLevel;
        }
        else
        {
            // 随机等级 - 确保参数正确
            uint32 minLvl = std::min(config->minLevel, config->maxLevel);
            uint32 maxLvl = std::max(config->minLevel, config->maxLevel);
            newLevel = urand(minLvl, maxLvl);
        }


        // 先让生物完全重新初始化（这会根据模板重新设置等级和属性）
        creature->SelectLevel(true);

        // 然后强制设置我们想要的等级
        creature->SetLevel(newLevel);

        // 再次重新计算属性，但这次不改变等级
        creature->SelectLevel(false);

    }

    // 现在开始调整属性 - 通过修改基础属性修正值
    // 等级调整后，强制更新一次属性确保基于新等级
    creature->UpdateAllStats();

    // 【重要修复】等级调整后，必须重新获取 CreatureBaseStats
    // 因为前面可能调用了 creature->SetLevel(newLevel)，stats 需要基于新等级重新计算
    // 否则后续属性倍率计算将基于旧等级的 stats，导致属性失真
    stats = sObjectMgr->GetCreatureBaseStats(creature->GetLevel(), cInfo->unit_class);
    if (!stats) {
        LOG_ERROR("module", ModuleOldFormat(">> 生物 %s 等级调整后无法获取基础统计数据", creature->GetEntry()).c_str());
        {
            std::lock_guard<std::mutex> lock(_creatureMutex);
            _adjustingCreatures.erase(creatureKey);
        }
        return;
    }

    if (useBasicAdjustment)
    {
        // 修复倍率计算错误：使用正确的线性倍率计算
        // 1级=2倍，2级=3倍，3级=4倍，4级=5倍...
        float levelMultiplier = static_cast<float>(1.0L + static_cast<long double>(huanJingLevel) * contentDifficultyScale); // 按副本原始强度缩放

        // cInfo 和 stats 已经在前面获取了

        // 计算原始血量：基础血量 * 生物模板的血量修正值
        AsInt64 originalHealth = ToUInt64FromAsInt64(stats->GenerateAsHealth(cInfo));

        // 应用幻境等级的倍率调整，防止计算过程中溢出
        AsInt64 newHealthValue64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalHealth), levelMultiplier);

        AsInt64 adjustedHealthValue = newHealthValue64;
        configuredHealthForCombat = adjustedHealthValue;

        // 设置调整后的血量
        creature->SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, SafeFloatValue(adjustedHealthValue, 0ULL));
        creature->SetModifierValue(UNIT_MOD_HEALTH, BASE_PCT, 1.0f);
        creature->SetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE, 0.0f);
        creature->SetModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT, 1.0f);
        SetCreatureCombatCreateHealth(creature, adjustedHealthValue);
        SetCreatureCombatMaxHealth(creature, adjustedHealthValue);
        SetCreatureCombatHealth(creature, adjustedHealthValue);

        LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 血量调整: 原始=%s, 倍率=%s, 调整后=%s", creature->GetEntry(), FormatUInt64(originalHealth), levelMultiplier, FormatUInt64(adjustedHealthValue)).c_str());

        // 然后检查是否需要应用血条系统
        const HuanJingCreatureConfig* config = GetMapCreatureConfig(creature, player);
        uint32 barCount = 0;

        if (config && config->healthBarCount > 1)
        {
            // 如果配置表中为该生物专门设置了血条数量，则优先使用配置
            barCount = config->healthBarCount;
        }

        if (barCount > 0)
        {
            LOG_DEBUG("huanjing.healthbar", ModuleOldFormat("Using health bar system for creature %s: bars=%s", creature->GetEntry(), barCount).c_str());

            // 在已调整的 64 位真实血量基础上应用血条系统
            InitializeCreatureHealthBars(creature, barCount, configuredHealthForCombat);
        }
        else
        {
            LOG_DEBUG("huanjing.healthbar", ModuleOldFormat("No health bar config for creature %s, using traditional multiplier only", creature->GetEntry()).c_str());
        }

        // 调整法力 - 基于原始法力值计算
        AsInt64 originalMana = SaturateToUInt64(Acore::Number::ToLongDouble(stats->BaseMana) * static_cast<long double>(cInfo->ModMana));
        AsInt64 newMana64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalMana), levelMultiplier);
        AsInt64 adjustedMana64 = newMana64;

        uint32 newMana = ToClientHealthValue(adjustedMana64);
        if (adjustedMana64 > newMana)
            SetCreatureVirtualMana(creature->GetGUID(), adjustedMana64);
        else
            ClearCreatureVirtualMana(creature->GetGUID());

        UnitMods manaMod = UnitMods(static_cast<uint16>(UNIT_MOD_POWER_START) + POWER_MANA);
        creature->SetModifierValue(manaMod, BASE_VALUE, SafeFloatValue(adjustedMana64, 0ULL));
        creature->SetModifierValue(manaMod, BASE_PCT, 1.0f);
        creature->SetModifierValue(manaMod, TOTAL_VALUE, 0.0f);
        creature->SetModifierValue(manaMod, TOTAL_PCT, 1.0f);
        creature->SetCreateMana(newMana);
        creature->SetMaxPower(POWER_MANA, newMana);
        SetCreatureCombatMaxPower(creature, POWER_MANA, adjustedMana64);
        SetCreatureCombatPower(creature, POWER_MANA, adjustedMana64);

        LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 法力调整: 原始=%s, 倍率=%s, 调整后=%s", creature->GetEntry(), FormatUInt64(originalMana), levelMultiplier, FormatUInt64(adjustedMana64)).c_str());

        // 调整伤害 - 基于原始基础伤害计算，使用正确的基础值
        float originalBaseDamage = stats->BaseDamage[cInfo->expansion];
        float originalMinDamage = originalBaseDamage;
        float originalMaxDamage = originalBaseDamage * 1.5f;

        // 计算新伤害值，限制在服务端 AsInt64 可承载范围内
        float newMinDamage = SafeDamageFloatValue(static_cast<long double>(originalMinDamage) * static_cast<long double>(levelMultiplier), _maxDamageValue);
        float newMaxDamage = SafeDamageFloatValue(static_cast<long double>(originalMaxDamage) * static_cast<long double>(levelMultiplier), _maxDamageValue);

        // 设置基础武器伤害
        creature->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, newMinDamage);
        creature->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, newMaxDamage);

        // 设置修改器值以确保正确应用
        creature->SetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, BASE_VALUE, newMinDamage);
        creature->SetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, BASE_PCT, 1.0f);
        creature->SetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_VALUE, 0.0f);
        creature->SetModifierValue(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, 1.0f);

        LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 伤害调整: 原始=%s-%s, 倍率=%s, 调整后=%s-%s", creature->GetEntry(), originalMinDamage, originalMaxDamage, levelMultiplier, newMinDamage, newMaxDamage).c_str());

        if (ReachesOptionalDamageLimit(newMinDamage, _maxDamageValue) || ReachesOptionalDamageLimit(newMaxDamage, _maxDamageValue))
        {
            LOG_WARN("module", ModuleOldFormat(">> 生物 %s 伤害值达到上限 (%s)，已限制在安全范围内", creature->GetEntry(), _maxDamageValue).c_str());
        }

        // 调整护甲 - 基于原始护甲值计算，使用正确的基础值
        float originalArmor = stats->BaseArmor * cInfo->ModArmor;
        AsInt64 newArmorValue64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalArmor), levelMultiplier);
        AsInt64 effectiveArmorLimitBasic = _maxArmorValue;
        AsInt64 newArmorValue = ApplyOptionalUInt64Limit(newArmorValue64, effectiveArmorLimitBasic);

        creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, SafeFloatValue(newArmorValue, effectiveArmorLimitBasic));
        creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_PCT, 1.0f);
        creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE, 0.0f);
        creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT, 1.0f);

        LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 护甲调整: 原始=%s, 倍率=%s, 调整后=%s", creature->GetEntry(), originalArmor, levelMultiplier, FormatUInt64(newArmorValue)).c_str());

        if (ExceedsOptionalUInt64Limit(newArmorValue64, effectiveArmorLimitBasic))
        {
            LOG_WARN("module", ModuleOldFormat(">> 生物 %s 护甲值达到上限 (%s)，已限制在安全范围内", creature->GetEntry(), FormatOptionalUInt64Limit(effectiveArmorLimitBasic)).c_str());
        }

        // 调整攻击强度 - 基于原始攻击强度值计算
        AsInt64 originalAttackPower = stats->AttackPower;
        AsInt64 newAttackPower64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalAttackPower), levelMultiplier);
        AsInt64 effectiveAttackPowerLimitBasic = _maxAttackPowerValue;
        AsInt64 newAttackPower = ApplyOptionalUInt64Limit(newAttackPower64, effectiveAttackPowerLimitBasic);

        creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, SafeFloatValue(newAttackPower, effectiveAttackPowerLimitBasic));
        creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_PCT, 1.0f);
        creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, 0.0f);
        creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, 1.0f);

        LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 攻击强度调整: 原始=%s, 倍率=%s, 调整后=%s", creature->GetEntry(), FormatUInt64(originalAttackPower), levelMultiplier, FormatUInt64(newAttackPower)).c_str());

        if (ExceedsOptionalUInt64Limit(newAttackPower64, effectiveAttackPowerLimitBasic))
        {
            LOG_WARN("module", ModuleOldFormat(">> 生物 %s 攻击强度达到上限 (%s)，已限制在安全范围内", creature->GetEntry(), FormatOptionalUInt64Limit(effectiveAttackPowerLimitBasic)).c_str());
        }

        // 调整法术强度 - 基于等级的基础法术强度
        // 先清除之前的法术强度设置
        ClearCreatureSpellPower(creature);

        uint32 baseSpellPower = creature->GetLevel() * 10; // 基础法术强度 = 等级 * 10
        AsInt64 newSpellPower64 = ScaleAsInt64ByDouble(AsInt256FromFloating(baseSpellPower), levelMultiplier);
        AsInt64 effectiveSpellPowerLimitBasic = _maxSpellPowerValue;
        AsInt64 newSpellPower = ApplyOptionalUInt64Limit(newSpellPower64, effectiveSpellPowerLimitBasic);

        // 应用法术强度到生物 - 存储到系统中
        SetCreatureSpellPower(creature->GetGUID(), newSpellPower);

        // 基础调整：添加命中率和护甲穿透
        // 根据幻境等级给予基础的命中率加成
        float baseHitBonus = static_cast<float>(static_cast<long double>(huanJingLevel) * 5.0L * contentDifficultyScale); // 每级+5%命中率，并按副本强度缩放
        if (baseHitBonus > 0.0f)
        {
            // 直接修改生物的命中率属性 - 这是最直接有效的方式
            creature->m_modMeleeHitChance += baseHitBonus;
            creature->m_modRangedHitChance += baseHitBonus;
            creature->m_modSpellHitChance += baseHitBonus;
        }

        // 移除护甲穿透逻辑 - 已禁用
        // if (huanJingLevel > 0 && player)
        // {
        //     uint32 playerArmor = player->GetArmor();
        //     // 基础护甲穿透：每级穿透玩家护甲的5%，最高50%
        //     float penetrationPct = std::min(huanJingLevel * 5.0f, 50.0f);
        //     uint32 armorPenetration = uint32(playerArmor * (penetrationPct / 100.0f));
        //
        //     // 移除旧的护甲穿透光环
        //     creature->RemoveAurasByType(SPELL_AURA_MOD_TARGET_RESISTANCE);
        //
        //     // 使用最简单直接的方法：直接设置玩家护甲为0来实现完全无视护甲
        //     if (armorPenetration > 0 && player)
        //     {
        //         // 记录玩家原始护甲值
        //         uint32 playerGuid = player->GetGUID().GetCounter();
        //         uint32 originalArmor = player->GetArmor();
        //
        //         // 先恢复之前的护甲（如果有记录）
        //         auto it = _playerArmorReductions.find(playerGuid);
        //         if (it != _playerArmorReductions.end())
        //         {
        //             player->SetArmor(it->second); // 恢复原始护甲
        //         }
        //
        //         // 记录当前护甲值
        //         _playerArmorReductions[playerGuid] = originalArmor;
        //
        //         // 设置护甲为0（完全无视护甲）
        //         player->SetArmor(0);
        //
        //         LOG_DEBUG("module", ">> 玩家 {} 护甲设为0 (原护甲: {})",
        //                   playerGuid, originalArmor);
        //     }
        //
        //     LOG_DEBUG("module", ">> 玩家护甲降低: {} (原护甲: {})",
        //               armorPenetration, playerArmor);
        //
        //     LOG_DEBUG("module", ">> 生物 {} 基础护甲穿透: {:.1f}%, 穿透值={} (玩家护甲: {})",
        //               creature->GetEntry(), penetrationPct, armorPenetration, playerArmor);
        // }

        // 根据幻境等级给予基础的专精值（降低玩家躲闪率）
        // WoW命中机制：dodge_chance -= GetTotalAuraModifier(SPELL_AURA_MOD_EXPERTISE) * 25
        // 玩家躲闪35% = 3500点，需要专精 = 3500/25 = 140点来完全抵消
        // 每级给予 50 点专精值，幻境等级3就能抵消35%躲闪
        uint32 baseExpertise = static_cast<uint32>(static_cast<long double>(huanJingLevel) * 50.0L * contentDifficultyScale); // 每级+50专精值，并按副本强度缩放
        if (baseExpertise > 0)
        {
            // 移除旧的专精光环
            creature->RemoveAurasByType(SPELL_AURA_MOD_EXPERTISE);

            // 使用宠物缩放法术 61017 (Hunter Pet Scaling 04)
            // 该法术 EFFECT_2 具有 SPELL_AURA_MOD_EXPERTISE 效果
            if (Aura* expertiseAura = creature->AddAura(61017, creature))
            {
                expertiseAura->SetDuration(-1); // 永久持续
                // EFFECT_2 是专精效果
                if (AuraEffect* effect = expertiseAura->GetEffect(EFFECT_2))
                {
                    effect->ChangeAmount(int32(baseExpertise)); // 设置专精值
                    LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 添加专精光环: %s点 (可抵消%s%%躲闪/招架)", creature->GetEntry(), baseExpertise, baseExpertise * 0.25f).c_str());
                }
            }
            else
            {
                // 备用方案：尝试使用术士宠物缩放法术
                if (Aura* expertiseAura2 = creature->AddAura(61013, creature))
                {
                    expertiseAura2->SetDuration(-1);
                    if (AuraEffect* effect = expertiseAura2->GetEffect(EFFECT_2))
                    {
                        effect->ChangeAmount(int32(baseExpertise));
                        LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 使用备用专精光环: %s点", creature->GetEntry(), baseExpertise).c_str());
                    }
                }
            }
        }

        // 移除护甲穿透逻辑 - 已禁用
        // 根据幻境等级给予基础的护甲穿透
        // 对于生物，我们通过存储护甲穿透值到一个自定义字段中
        // 然后在伤害计算时使用这个值
        // uint32 baseArmorPenetration = huanJingLevel * 100; // 每级+100护甲穿透
        // if (baseArmorPenetration > 0)
        // {
        //     // 将护甲穿透值存储到生物的自定义数据中
        //     // 这里我们使用一个简单的方法：通过设置一个特殊的光环来标记护甲穿透值
        //     creature->RemoveAurasByType(SPELL_AURA_MOD_TARGET_RESISTANCE);
        //
        //     // 使用一个简单的方法：直接在生物身上添加一个标记光环
        //     if (Aura* armorPenAura = creature->AddAura(7386, creature))
        //     {
        //         armorPenAura->SetDuration(-1);
        //         if (AuraEffect* effect = armorPenAura->GetEffect(EFFECT_0))
        //         {
        //             effect->ChangeAmount(-int32(baseArmorPenetration));
        //         }
        //     }
        // }

        // 移除格挡穿透逻辑 - 已禁用
        // uint32 baseBlockPenetration = 0;
        // float baseBlockPenetrationPct = 0.0f;
        // if (huanJingLevel >= 2 && player) // 从幻境等级2开始有格挡穿透
        // {
        //     // 基础格挡穿透百分比：等级2为50%，等级3及以上为100%
        //     baseBlockPenetrationPct = (huanJingLevel >= 3) ? 100.0f : 50.0f;
        //
        //     float playerBlockChance = player->GetFloatValue(PLAYER_BLOCK_PERCENTAGE);
        //     if (baseBlockPenetrationPct > 0.0f)
        //     {
        //         // 记录玩家原始格挡率
        //         uint32 playerGuid = player->GetGUID().GetCounter();
        //         float originalBlockChance = player->GetFloatValue(PLAYER_BLOCK_PERCENTAGE);
        //
        //         // 先恢复之前的格挡率（如果有记录）
        //         auto it = _playerBlockReductions.find(playerGuid);
        //         if (it != _playerBlockReductions.end())
        //         {
        //             player->SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, it->second); // 恢复原始格挡率
        //         }
        //
        //         // 记录当前格挡率
        //         _playerBlockReductions[playerGuid] = originalBlockChance;
        //
        //         // 计算穿透后的格挡率
        //         float penetratedBlockChance = originalBlockChance * (1.0f - baseBlockPenetrationPct / 100.0f);
        //
        //         // 直接设置玩家的格挡率
        //         player->SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, penetratedBlockChance);
        //
        //         LOG_DEBUG("module", ">> 玩家 {} 格挡率设为{:.1f}% (原格挡率: {:.1f}%)",
        //                   playerGuid, penetratedBlockChance, originalBlockChance);
        //     }
        //
        //     LOG_DEBUG("module", ">> 生物 {} 基础格挡穿透: {:.1f}% (玩家格挡率: {:.1f}%)",
        //               creature->GetEntry(), baseBlockPenetrationPct, playerBlockChance);
        // }

    } // 结束基础调整分支
    else
    {
        // 有配置时，使用原有的详细配置逻辑
        // cInfo 和 stats 已经在前面获取了
        // 调整血量 - 先应用血量配置，再应用血条系统
        if (config->healthValue > 0)
        {
            // 设置固定血量。客户端字段保持安全值，128 位真实血量写入扩展血量供战斗和插件读取。
            AsInt64 targetHealthValue = config->healthValue;
            configuredHealthForCombat = targetHealthValue;

            creature->SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, SafeFloatValue(targetHealthValue, 0ULL));
            creature->SetModifierValue(UNIT_MOD_HEALTH, BASE_PCT, 1.0f);
            creature->SetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE, 0.0f);
            creature->SetModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT, 1.0f);
            SetCreatureCombatCreateHealth(creature, targetHealthValue);
            SetCreatureCombatMaxHealth(creature, targetHealthValue);
            SetCreatureCombatHealth(creature, targetHealthValue);
        }
        else if (config->healthMultiplier != 1.0f)
        {
            // 血量倍率调整 - 修复：基于原始血量值计算，并保留 128 位真实血量。
            // stats 已经在前面获取了
            AsInt64 originalHealth = ToUInt64FromAsInt64(stats->GenerateAsHealth(cInfo));
            AsInt64 newHealthValue64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalHealth), config->healthMultiplier);
            AsInt64 newHealthValue = newHealthValue64;
            configuredHealthForCombat = newHealthValue;

            creature->SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, SafeFloatValue(newHealthValue, 0ULL));
            creature->SetModifierValue(UNIT_MOD_HEALTH, BASE_PCT, 1.0f);
            creature->SetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE, 0.0f);
            creature->SetModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT, 1.0f);
            SetCreatureCombatCreateHealth(creature, newHealthValue);
            SetCreatureCombatMaxHealth(creature, newHealthValue);
            SetCreatureCombatHealth(creature, newHealthValue);

            LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 配置血量倍率调整: 原始=%s, 倍率=%s, 调整后=%s", creature->GetEntry(), FormatUInt64(originalHealth), config->healthMultiplier, FormatUInt64(newHealthValue)).c_str());
        }

        // 血量调整完成后，检查是否需要应用血条系统
        uint32 barCount = 0;
        if (config->healthBarCount > 1)
        {
            // 如果配置表中有血条数量，优先使用配置
            barCount = config->healthBarCount;
        }

        if (barCount > 0)
        {
            // 强制更新生物属性以应用血量修改
            creature->UpdateAllStats();

            if (configuredHealthForCombat > 0)
            {
                SetCreatureCombatCreateHealth(creature, configuredHealthForCombat);
                SetCreatureCombatMaxHealth(creature, configuredHealthForCombat);
                SetCreatureCombatHealth(creature, configuredHealthForCombat);
            }
            else
            {
                SetCreatureCombatHealth(creature, GetCreatureCombatMaxHealth(creature));
            }

            LOG_DEBUG("huanjing.healthbar", ModuleOldFormat("Applying health bar system for creature %s: bars=%s", creature->GetEntry(), barCount).c_str());

            InitializeCreatureHealthBars(creature, barCount, configuredHealthForCombat);
        }

        // 调整法力 - 通过修改基础属性修正值
        UnitMods manaMod = UnitMods(static_cast<uint16>(UNIT_MOD_POWER_START) + POWER_MANA);
        ClearCreatureVirtualMana(creature->GetGUID());
        if (config->manaValue > 0)
        {
            AsInt64 effectiveManaLimit = GetClientSafeResourceLimit(_maxManaValue);
            AsInt64 safeManaValue = config->manaValue;

            if (safeManaValue > ToClientHealthValue(safeManaValue))
                SetCreatureVirtualMana(creature->GetGUID(), safeManaValue);
            else
                ClearCreatureVirtualMana(creature->GetGUID());

            creature->SetModifierValue(manaMod, BASE_VALUE, SafeFloatValue(safeManaValue, effectiveManaLimit));
            creature->SetModifierValue(manaMod, BASE_PCT, 1.0f);
            creature->SetModifierValue(manaMod, TOTAL_VALUE, 0.0f);
            creature->SetModifierValue(manaMod, TOTAL_PCT, 1.0f);
        }
        else if (config->manaMultiplier != 1.0f)
        {
            // 法力倍率调整：基于真正的原始法力值（从BASE_VALUE获取）
            float originalMana = creature->GetModifierValue(manaMod, BASE_VALUE);
            AsInt64 newMana64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalMana), config->manaMultiplier);

            AsInt64 effectiveManaLimit = GetClientSafeResourceLimit(_maxManaValue);
            AsInt64 newMana = newMana64;

            if (newMana > ToClientHealthValue(newMana))
                SetCreatureVirtualMana(creature->GetGUID(), newMana);
            else
                ClearCreatureVirtualMana(creature->GetGUID());

            creature->SetModifierValue(manaMod, BASE_VALUE, SafeFloatValue(newMana, effectiveManaLimit));
            creature->SetModifierValue(manaMod, BASE_PCT, 1.0f);
            creature->SetModifierValue(manaMod, TOTAL_VALUE, 0.0f);
            creature->SetModifierValue(manaMod, TOTAL_PCT, 1.0f);
        }

        // 调整伤害 - 获取生物的真正原始基础伤害（从武器伤害获取）
        float originalMinDamage = creature->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE);
        float originalMaxDamage = creature->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE);
        float newMinDamage, newMaxDamage;
        AsInt64 extendedMinDamage = 0;
        AsInt64 extendedMaxDamage = 0;


        if (config->minDamageValue > 0)
        {
            extendedMinDamage = config->minDamageValue;
            newMinDamage = SafeDamageFloatValue(Acore::Number::ToLongDouble(config->minDamageValue), _maxDamageValue);
        }
        else
        {
            long double scaledMinDamage = static_cast<long double>(originalMinDamage) * static_cast<long double>(config->minDamageMultiplier);
            extendedMinDamage = SaturateToUInt64(scaledMinDamage);
            newMinDamage = SafeDamageFloatValue(scaledMinDamage, _maxDamageValue);
        }

        if (config->maxDamageValue > 0)
        {
            extendedMaxDamage = config->maxDamageValue;
            newMaxDamage = SafeDamageFloatValue(Acore::Number::ToLongDouble(config->maxDamageValue), _maxDamageValue);
        }
        else
        {
            long double scaledMaxDamage = static_cast<long double>(originalMaxDamage) * static_cast<long double>(config->maxDamageMultiplier);
            extendedMaxDamage = SaturateToUInt64(scaledMaxDamage);
            newMaxDamage = SafeDamageFloatValue(scaledMaxDamage, _maxDamageValue);
        }

        // 直接设置最终伤害值，避免被系统重新计算覆盖
        creature->SetStatFloatValue(UNIT_FIELD_MINDAMAGE, newMinDamage);
        creature->SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, newMaxDamage);

        // 同时设置基础武器伤害以保持一致性
        creature->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, newMinDamage);
        creature->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, newMaxDamage);
        SetCreatureCombatWeaponDamageRange(creature, BASE_ATTACK, MINDAMAGE, AsInt256FromFloating(extendedMinDamage));
        SetCreatureCombatWeaponDamageRange(creature, BASE_ATTACK, MAXDAMAGE, AsInt256FromFloating(extendedMaxDamage));


        // 检查是否达到伤害上限
        if (ReachesOptionalDamageLimit(newMinDamage, _maxDamageValue) || ReachesOptionalDamageLimit(newMaxDamage, _maxDamageValue))
        {
            LOG_WARN("module", ModuleOldFormat(">> 生物 %s 配置伤害值达到上限 (%s)，已限制在安全范围内", creature->GetEntry(), _maxDamageValue).c_str());
        }

        // 调整护甲 - 通过修改基础属性修正值，添加上限保护
        LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 护甲配置检查: armorValue=%s, armorMultiplier=%s", creature->GetEntry(), FormatUInt64(config->armorValue), config->armorMultiplier).c_str());
        if (config->armorValue > 0)
        {
            // 护甲上限控制
            AsInt64 effectiveArmorLimit = _maxArmorValue;
            AsInt64 safeArmorValue = ApplyOptionalUInt64Limit(config->armorValue, effectiveArmorLimit);
            creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, SafeFloatValue(safeArmorValue, effectiveArmorLimit));
            creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_PCT, 1.0f);
            creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE, 0.0f);
            creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT, 1.0f);

            if (ExceedsOptionalUInt64Limit(config->armorValue, effectiveArmorLimit))
            {
                LOG_WARN("module", ModuleOldFormat(">> 生物 %s 配置护甲值超过配置上限，已限制为%s", creature->GetEntry(), FormatOptionalUInt64Limit(effectiveArmorLimit)).c_str());
            }
        }
        else if (config->armorMultiplier != 1.0f)
        {
            // 护甲倍率调整：修复 - 基于原始护甲值计算
            // stats 已经在前面获取了
            float originalArmor = stats->BaseArmor * cInfo->ModArmor;
            AsInt64 newArmorValue64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalArmor), config->armorMultiplier);
            AsInt64 effectiveArmorLimit2 = _maxArmorValue;
            AsInt64 newArmorValue = ApplyOptionalUInt64Limit(newArmorValue64, effectiveArmorLimit2);

            creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, SafeFloatValue(newArmorValue, effectiveArmorLimit2));
            creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_PCT, 1.0f);
            creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE, 0.0f);
            creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT, 1.0f);

            LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 配置护甲倍率调整: 原始=%s, 倍率=%s, 调整后=%s", creature->GetEntry(), originalArmor, config->armorMultiplier, FormatUInt64(newArmorValue)).c_str());

            if (ExceedsOptionalUInt64Limit(newArmorValue64, effectiveArmorLimit2))
            {
                LOG_WARN("module", ModuleOldFormat(">> 生物 %s 配置护甲倍率超过配置上限，已限制为%s", creature->GetEntry(), FormatOptionalUInt64Limit(effectiveArmorLimit2)).c_str());
            }
        }

        // 调整抗性 - 通过修改基础属性修正值，添加上限保护
        if (config->holyResistValue > 0)
        {
            UnitMods resistMod = UnitMods(UNIT_MOD_RESISTANCE_START + SPELL_SCHOOL_HOLY);
            AsInt64 effectiveResistLimit = _maxResistanceValue;
            AsInt64 safeResistValue = ApplyOptionalUInt64Limit(config->holyResistValue, effectiveResistLimit);
            creature->SetModifierValue(resistMod, BASE_VALUE, SafeFloatValue(safeResistValue, effectiveResistLimit));
            creature->SetModifierValue(resistMod, BASE_PCT, 1.0f);
            creature->SetModifierValue(resistMod, TOTAL_VALUE, 0.0f);
            creature->SetModifierValue(resistMod, TOTAL_PCT, 1.0f);

            if (ExceedsOptionalUInt64Limit(config->holyResistValue, effectiveResistLimit))
            {
                LOG_WARN("module", ModuleOldFormat(">> 生物 %s 神圣抗性超过上限，已限制为%s", creature->GetEntry(), effectiveResistLimit).c_str());
            }
        }

        if (config->fireResistValue > 0 || config->fireResistMultiplier != 1.0f)
        {
            UnitMods resistMod = UnitMods(UNIT_MOD_RESISTANCE_START + SPELL_SCHOOL_FIRE);
            if (config->fireResistValue > 0)
            {
                AsInt64 effectiveResistLimit = _maxResistanceValue;
                AsInt64 safeResistValue = ApplyOptionalUInt64Limit(config->fireResistValue, effectiveResistLimit);
                creature->SetModifierValue(resistMod, BASE_VALUE, SafeFloatValue(safeResistValue, effectiveResistLimit));
                creature->SetModifierValue(resistMod, BASE_PCT, 1.0f);
                creature->SetModifierValue(resistMod, TOTAL_VALUE, 0.0f);
                creature->SetModifierValue(resistMod, TOTAL_PCT, 1.0f);

                if (ExceedsOptionalUInt64Limit(config->fireResistValue, effectiveResistLimit))
                {
                    LOG_WARN("module", ModuleOldFormat(">> 生物 %s 火抗值超过上限，已限制为%s", creature->GetEntry(), effectiveResistLimit).c_str());
                }
            }
            else if (config->fireResistMultiplier != 1.0f)
            {
                // 获取原始火抗值
                uint32 originalResist = cInfo->resistance[SPELL_SCHOOL_FIRE];
                AsInt64 effectiveResistLimit2 = _maxResistanceValue;
                AsInt64 newResist64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalResist), config->fireResistMultiplier);
                AsInt64 newResist = ApplyOptionalUInt64Limit(newResist64, effectiveResistLimit2);

                creature->SetModifierValue(resistMod, BASE_VALUE, SafeFloatValue(newResist, effectiveResistLimit2));
                creature->SetModifierValue(resistMod, BASE_PCT, 1.0f);
                creature->SetModifierValue(resistMod, TOTAL_VALUE, 0.0f);
                creature->SetModifierValue(resistMod, TOTAL_PCT, 1.0f);

                if (ExceedsOptionalUInt64Limit(newResist64, effectiveResistLimit2))
                {
                    LOG_WARN("module", ModuleOldFormat(">> 生物 %s 火抗倍率超过上限，已限制为%s", creature->GetEntry(), effectiveResistLimit2).c_str());
                }
            }
        }

        // 其他抗性调整 - 添加上限保护
        if (config->natureResistValue > 0 || config->natureResistMultiplier != 1.0f)
        {
            UnitMods resistMod = UnitMods(UNIT_MOD_RESISTANCE_START + SPELL_SCHOOL_NATURE);
            if (config->natureResistValue > 0)
            {
                AsInt64 effectiveResistLimit3 = _maxResistanceValue;
                AsInt64 safeResistValue = ApplyOptionalUInt64Limit(config->natureResistValue, effectiveResistLimit3);
                creature->SetModifierValue(resistMod, BASE_VALUE, SafeFloatValue(safeResistValue, effectiveResistLimit3));
                creature->SetModifierValue(resistMod, BASE_PCT, 1.0f);
                creature->SetModifierValue(resistMod, TOTAL_VALUE, 0.0f);
                creature->SetModifierValue(resistMod, TOTAL_PCT, 1.0f);

                if (ExceedsOptionalUInt64Limit(config->natureResistValue, effectiveResistLimit3))
                {
                    LOG_WARN("module", ModuleOldFormat(">> 生物 %s 自然抗值超过上限，已限制为%s", creature->GetEntry(), effectiveResistLimit3).c_str());
                }
            }
            else if (config->natureResistMultiplier != 1.0f)
            {
                // 获取原始自然抗值
                uint32 originalResist = cInfo->resistance[SPELL_SCHOOL_NATURE];
                AsInt64 effectiveResistLimit4 = _maxResistanceValue;
                AsInt64 newResist64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalResist), config->natureResistMultiplier);
                AsInt64 newResist = ApplyOptionalUInt64Limit(newResist64, effectiveResistLimit4);

                creature->SetModifierValue(resistMod, BASE_VALUE, SafeFloatValue(newResist, effectiveResistLimit4));
                creature->SetModifierValue(resistMod, BASE_PCT, 1.0f);
                creature->SetModifierValue(resistMod, TOTAL_VALUE, 0.0f);
                creature->SetModifierValue(resistMod, TOTAL_PCT, 1.0f);

                if (ExceedsOptionalUInt64Limit(newResist64, effectiveResistLimit4))
                {
                    LOG_WARN("module", ModuleOldFormat(">> 生物 %s 自然抗倍率超过上限，已限制为%s", creature->GetEntry(), effectiveResistLimit4).c_str());
                }
            }
        }

        // 冰霜抗性调整 - 添加上限保护
        if (config->frostResistValue > 0 || config->frostResistMultiplier != 1.0f)
        {
            UnitMods resistMod = UnitMods(UNIT_MOD_RESISTANCE_START + SPELL_SCHOOL_FROST);
            if (config->frostResistValue > 0)
            {
                AsInt64 effectiveResistLimit5 = _maxResistanceValue;
                AsInt64 safeResistValue = ApplyOptionalUInt64Limit(config->frostResistValue, effectiveResistLimit5);
                creature->SetModifierValue(resistMod, BASE_VALUE, SafeFloatValue(safeResistValue, effectiveResistLimit5));
                creature->SetModifierValue(resistMod, BASE_PCT, 1.0f);
                creature->SetModifierValue(resistMod, TOTAL_VALUE, 0.0f);
                creature->SetModifierValue(resistMod, TOTAL_PCT, 1.0f);

                if (ExceedsOptionalUInt64Limit(config->frostResistValue, effectiveResistLimit5))
                {
                    LOG_WARN("module", ModuleOldFormat(">> 生物 %s 冰霜抗值超过上限，已限制为%s", creature->GetEntry(), effectiveResistLimit5).c_str());
                }
            }
            else if (config->frostResistMultiplier != 1.0f)
            {
                // 获取原始冰霜抗值
                uint32 originalResist = cInfo->resistance[SPELL_SCHOOL_FROST];
                AsInt64 effectiveResistLimit6 = _maxResistanceValue;
                AsInt64 newResist64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalResist), config->frostResistMultiplier);
                AsInt64 newResist = ApplyOptionalUInt64Limit(newResist64, effectiveResistLimit6);

                creature->SetModifierValue(resistMod, BASE_VALUE, SafeFloatValue(newResist, effectiveResistLimit6));
                creature->SetModifierValue(resistMod, BASE_PCT, 1.0f);
                creature->SetModifierValue(resistMod, TOTAL_VALUE, 0.0f);
                creature->SetModifierValue(resistMod, TOTAL_PCT, 1.0f);

                if (ExceedsOptionalUInt64Limit(newResist64, effectiveResistLimit6))
                {
                    LOG_WARN("module", ModuleOldFormat(">> 生物 %s 冰霜抗倍率超过上限，已限制为%s", creature->GetEntry(), effectiveResistLimit6).c_str());
                }
            }
        }

        // 暗影抗性调整 - 添加上限保护
        if (config->shadowResistValue > 0 || config->shadowResistMultiplier != 1.0f)
        {
            UnitMods resistMod = UnitMods(UNIT_MOD_RESISTANCE_START + SPELL_SCHOOL_SHADOW);
            if (config->shadowResistValue > 0)
            {
                AsInt64 effectiveResistLimit7 = _maxResistanceValue;
                AsInt64 safeResistValue = ApplyOptionalUInt64Limit(config->shadowResistValue, effectiveResistLimit7);
                creature->SetModifierValue(resistMod, BASE_VALUE, SafeFloatValue(safeResistValue, effectiveResistLimit7));
                creature->SetModifierValue(resistMod, BASE_PCT, 1.0f);
                creature->SetModifierValue(resistMod, TOTAL_VALUE, 0.0f);
                creature->SetModifierValue(resistMod, TOTAL_PCT, 1.0f);

                if (ExceedsOptionalUInt64Limit(config->shadowResistValue, effectiveResistLimit7))
                {
                    LOG_WARN("module", ModuleOldFormat(">> 生物 %s 暗影抗值超过上限，已限制为%s", creature->GetEntry(), effectiveResistLimit7).c_str());
                }
            }
            else if (config->shadowResistMultiplier != 1.0f)
            {
                // 获取原始暗影抗值
                uint32 originalResist = cInfo->resistance[SPELL_SCHOOL_SHADOW];
                AsInt64 effectiveResistLimit8 = _maxResistanceValue;
                AsInt64 newResist64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalResist), config->shadowResistMultiplier);
                AsInt64 newResist = ApplyOptionalUInt64Limit(newResist64, effectiveResistLimit8);

                creature->SetModifierValue(resistMod, BASE_VALUE, SafeFloatValue(newResist, effectiveResistLimit8));
                creature->SetModifierValue(resistMod, BASE_PCT, 1.0f);
                creature->SetModifierValue(resistMod, TOTAL_VALUE, 0.0f);
                creature->SetModifierValue(resistMod, TOTAL_PCT, 1.0f);

                if (ExceedsOptionalUInt64Limit(newResist64, effectiveResistLimit8))
                {
                    LOG_WARN("module", ModuleOldFormat(">> 生物 %s 暗影抗倍率超过上限，已限制为%s", creature->GetEntry(), effectiveResistLimit8).c_str());
                }
            }
        }

        // 奥术抗性调整 - 添加上限保护
        if (config->arcaneResistValue > 0 || config->arcaneResistMultiplier != 1.0f)
        {
            UnitMods resistMod = UnitMods(UNIT_MOD_RESISTANCE_START + SPELL_SCHOOL_ARCANE);
            if (config->arcaneResistValue > 0)
            {
                AsInt64 effectiveResistLimit9 = _maxResistanceValue;
                AsInt64 safeResistValue = ApplyOptionalUInt64Limit(config->arcaneResistValue, effectiveResistLimit9);
                creature->SetModifierValue(resistMod, BASE_VALUE, SafeFloatValue(safeResistValue, effectiveResistLimit9));
                creature->SetModifierValue(resistMod, BASE_PCT, 1.0f);
                creature->SetModifierValue(resistMod, TOTAL_VALUE, 0.0f);
                creature->SetModifierValue(resistMod, TOTAL_PCT, 1.0f);

                if (ExceedsOptionalUInt64Limit(config->arcaneResistValue, effectiveResistLimit9))
                {
                    LOG_WARN("module", ModuleOldFormat(">> 生物 %s 奥术抗值超过上限，已限制为%s", creature->GetEntry(), effectiveResistLimit9).c_str());
                }
            }
            else if (config->arcaneResistMultiplier != 1.0f)
            {
                // 获取原始奥术抗值
                uint32 originalResist = cInfo->resistance[SPELL_SCHOOL_ARCANE];
                AsInt64 effectiveResistLimit10 = _maxResistanceValue;
                AsInt64 newResist64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalResist), config->arcaneResistMultiplier);
                AsInt64 newResist = ApplyOptionalUInt64Limit(newResist64, effectiveResistLimit10);

                creature->SetModifierValue(resistMod, BASE_VALUE, SafeFloatValue(newResist, effectiveResistLimit10));
                creature->SetModifierValue(resistMod, BASE_PCT, 1.0f);
                creature->SetModifierValue(resistMod, TOTAL_VALUE, 0.0f);
                creature->SetModifierValue(resistMod, TOTAL_PCT, 1.0f);

                if (ExceedsOptionalUInt64Limit(newResist64, effectiveResistLimit10))
                {
                    LOG_WARN("module", ModuleOldFormat(">> 生物 %s 奥术抗倍率超过上限，已限制为%s", creature->GetEntry(), effectiveResistLimit10).c_str());
                }
            }
        }

        // 调整攻击强度 - 通过修改基础属性修正值，添加上限保护
        if (config->attackPowerValue > 0 || config->attackPowerMultiplier != 1.0f)
        {
            AsInt64 newAttackPower;

            // 攻击强度上限控制
            AsInt64 effectiveAttackPowerLimit = _maxAttackPowerValue;

            if (config->attackPowerValue > 0)
            {
                // 设置固定攻击强度
                newAttackPower = ApplyOptionalUInt64Limit(config->attackPowerValue, effectiveAttackPowerLimit);

                if (ExceedsOptionalUInt64Limit(config->attackPowerValue, effectiveAttackPowerLimit))
                {
                    LOG_WARN("module", ModuleOldFormat(">> 生物 %s 配置攻击强度值超过配置上限，已限制为%s", creature->GetEntry(), FormatOptionalUInt64Limit(effectiveAttackPowerLimit)).c_str());
                }
            }
            else
            {
                // 攻击强度倍率调整：基于真正的原始攻击强度值（从BASE_VALUE获取）
                float originalAttackPower = creature->GetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE);
                AsInt64 newAttackPower64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalAttackPower), config->attackPowerMultiplier);
                newAttackPower = ApplyOptionalUInt64Limit(newAttackPower64, effectiveAttackPowerLimit);

                if (ExceedsOptionalUInt64Limit(newAttackPower64, effectiveAttackPowerLimit))
                {
                    LOG_WARN("module", ModuleOldFormat(">> 生物 %s 配置攻击强度倍率超过配置上限，已限制为%s", creature->GetEntry(), FormatOptionalUInt64Limit(effectiveAttackPowerLimit)).c_str());
                }
            }

            // 通过修改基础属性修正值来设置攻击强度
            creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, SafeFloatValue(newAttackPower, effectiveAttackPowerLimit));
            creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_PCT, 1.0f);
            creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, 0.0f);
            creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, 1.0f);
        }

        // 调整法术强度（通过调整法术伤害加成）
        // 先清除之前的法术强度设置
        ClearCreatureSpellPower(creature);

        if (config->spellPowerValue > 0 || config->spellPowerMultiplier != 1.0f)
        {
            // 法术强度上限控制
            AsInt64 effectiveSpellPowerLimit = _maxSpellPowerValue;
            AsInt64 newSpellPower = 0;

            if (config->spellPowerValue > 0)
            {
                // 设置固定法术强度
                newSpellPower = ApplyOptionalUInt64Limit(config->spellPowerValue, effectiveSpellPowerLimit);
            }
            else
            {
                // 法术强度倍率调整：基于原始法术强度值
                // 对于生物，我们使用一个基础法术强度值进行倍率计算
                uint32 baseSpellPower = creature->GetLevel() * 10; // 基础法术强度 = 等级 * 10
                AsInt64 newSpellPower64 = ScaleAsInt64ByDouble(AsInt256FromFloating(baseSpellPower), config->spellPowerMultiplier);
                newSpellPower = ApplyOptionalUInt64Limit(newSpellPower64, effectiveSpellPowerLimit);
            }

            // 应用法术强度到生物 - 存储到系统中
            SetCreatureSpellPower(creature->GetGUID(), newSpellPower);
        }

        // 调整移动速度 - 获取原始速度并设置所有移动类型
        if (config->moveSpeedValue > 0 || config->moveSpeedMultiplier != 1.0f)
        {
            // 获取生物模板的原始速度
            float originalWalkSpeed = cInfo->speed_walk;
            float originalRunSpeed = cInfo->speed_run;
            float originalSwimSpeed = cInfo->speed_swim;
            float originalFlightSpeed = cInfo->speed_flight;

            float newWalkSpeed, newRunSpeed, newSwimSpeed, newFlightSpeed;

            if (config->moveSpeedValue > 0)
            {
                // 使用固定速度值（假设输入的是百分比，如100表示100%即1.0倍速）
                float speedMultiplier = config->moveSpeedValue / 100.0f;
                newWalkSpeed = originalWalkSpeed * speedMultiplier;
                newRunSpeed = originalRunSpeed * speedMultiplier;
                newSwimSpeed = originalSwimSpeed * speedMultiplier;
                newFlightSpeed = originalFlightSpeed * speedMultiplier;
            }
            else
            {
                // 使用倍率
                newWalkSpeed = originalWalkSpeed * config->moveSpeedMultiplier;
                newRunSpeed = originalRunSpeed * config->moveSpeedMultiplier;
                newSwimSpeed = originalSwimSpeed * config->moveSpeedMultiplier;
                newFlightSpeed = originalFlightSpeed * config->moveSpeedMultiplier;
            }

            // 关键修复:确保速度不小于最小值 0.01f,防止 MoveSplineInitArgs::Validate 验证失败
            const float MIN_SPEED = 0.01f;
            newWalkSpeed = std::max(newWalkSpeed, MIN_SPEED);
            newRunSpeed = std::max(newRunSpeed, MIN_SPEED);
            newSwimSpeed = std::max(newSwimSpeed, MIN_SPEED);
            newFlightSpeed = std::max(newFlightSpeed, MIN_SPEED);

            // 设置所有移动类型的速度
            creature->SetSpeed(MOVE_WALK, newWalkSpeed);
            creature->SetSpeed(MOVE_RUN, newRunSpeed);
            creature->SetSpeed(MOVE_SWIM, newSwimSpeed);
            creature->SetSpeed(MOVE_FLIGHT, newFlightSpeed);

            LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 速度调整: 行走=%s, 跑步=%s, 游泳=%s, 飞行=%s", creature->GetEntry(), newWalkSpeed, newRunSpeed, newSwimSpeed, newFlightSpeed).c_str());
        }

        // 调整攻击速度 - 获取原始攻击时间并设置所有攻击类型
        if (config->attackSpeedValue > 0 || config->attackSpeedMultiplier != 1.0f)
        {
            // 获取生物模板的原始攻击时间
            uint32 originalBaseAttackTime = cInfo->BaseAttackTime;
            uint32 originalRangedAttackTime = cInfo->RangeAttackTime;

            float newBaseAttackTime, newRangedAttackTime;

            if (config->attackSpeedValue > 0)
            {
                // 使用固定攻击时间值（毫秒）
                newBaseAttackTime = config->attackSpeedValue;
                newRangedAttackTime = config->attackSpeedValue;
            }
            else
            {
                // 攻击速度倍率越大，攻击间隔越小（攻击越快）
                newBaseAttackTime = originalBaseAttackTime / config->attackSpeedMultiplier;
                newRangedAttackTime = originalRangedAttackTime / config->attackSpeedMultiplier;
            }

            // 设置攻击时间（确保不小于100毫秒，避免过快攻击）
            newBaseAttackTime = std::max(newBaseAttackTime, 100.0f);
            newRangedAttackTime = std::max(newRangedAttackTime, 100.0f);

            creature->SetFloatValue(UNIT_FIELD_BASEATTACKTIME, newBaseAttackTime);
            creature->SetFloatValue(UNIT_FIELD_RANGEDATTACKTIME, newRangedAttackTime);

        }

        // 施法速度倍率越大，实际施法时间越短。固定施法时间在 Unit::ModSpellCastTime 中最后覆盖。
        float castTimeScale = 1.0f;
        if (std::isfinite(config->castSpeedMultiplier) && config->castSpeedMultiplier > 0.0f)
            castTimeScale = std::clamp(1.0f / config->castSpeedMultiplier, 0.01f, 100.0f);
        creature->SetFloatValue(UNIT_MOD_CAST_SPEED, castTimeScale);

        // 调整装备属性倍率
        if (config->equipAttrMultiplier != 1.0f)
        {
            // 装备属性倍率的具体实现可能需要在装备相关的地方处理
        }

        // 调整命中率 - 直接修改生物的命中率属性
        if (config->hitChanceBonus != 0.0f)
        {
            // 直接修改生物的命中率属性 - 这是最直接有效的方式
            creature->m_modMeleeHitChance += config->hitChanceBonus;
            creature->m_modRangedHitChance += config->hitChanceBonus;
            creature->m_modSpellHitChance += config->hitChanceBonus;

        }

        // 调整护甲穿透 - 使用配置中的护甲穿透设置
        if ((config->armorPenetration > 0 || config->armorPenetrationPct > 0.0f) && player)
        {
            // 实际穿透在 Unit::CalcArmorReducedDamage 中按本次攻击目标计算。
            // 这里不再修改玩家面板属性，也不添加会影响所有目标的光环。
            // creature->RemoveAurasByType(SPELL_AURA_MOD_TARGET_RESISTANCE);
            //
            // // 使用最简单直接的方法：直接设置玩家护甲为0来实现完全无视护甲
            // if (finalArmorPenetration > 0 && player)
            // {
            //     // 记录玩家原始护甲值
            //     uint32 playerGuid = player->GetGUID().GetCounter();
            //     uint32 originalArmor = player->GetArmor();
            //
            //     // 先恢复之前的护甲（如果有记录）
            //     auto it = _playerArmorReductions.find(playerGuid);
            //     if (it != _playerArmorReductions.end())
            //     {
            //         player->SetArmor(it->second); // 恢复原始护甲
            //     }
            //
            //     // 记录当前护甲值
            //     _playerArmorReductions[playerGuid] = originalArmor;
            //
            //     // 设置护甲为0（完全无视护甲）
            //     player->SetArmor(0);
            //
            //     LOG_DEBUG("module", ">> 玩家 {} 护甲设为0 (原护甲: {})",
            //               playerGuid, originalArmor);
            // }
            //
            // LOG_DEBUG("module", ">> 生物 {} 护甲穿透: 固定值={}, 百分比={:.1f}%, 最终穿透={} (玩家护甲: {})",
            //           creature->GetEntry(), config->armorPenetration, config->armorPenetrationPct,
            //           finalArmorPenetration, playerArmor);
        }

        // 调整护甲穿透 - 使用与基础调整相同的方式
        if (config->armorPenetration > 0)
        {
            // See Unit::CalcArmorReducedDamage.
        }

        // 护甲穿透百分比 - 记录到日志，实际效果需要在伤害计算时实现
        if (config->armorPenetrationPct > 0.0f)
        {
            // See Unit::CalcArmorReducedDamage.
        }

        // 调整格挡穿透 - 使用配置中的格挡穿透设置
        if ((config->blockPenetration > 0 || config->blockPenetrationPct > 0.0f) && player)
        {
            // 实际格挡穿透在 OnBeforeRollMeleeOutcomeAgainst 中按本次攻击计算。
            // 这里不再改写玩家的格挡面板值。
            // if (finalBlockPenetrationPct > 0.0f)
            // {
            //     // 记录玩家原始格挡率
            //     uint32 playerGuid = player->GetGUID().GetCounter();
            //     float originalBlockChance = player->GetFloatValue(PLAYER_BLOCK_PERCENTAGE);
            //
            //     // 先恢复之前的格挡率（如果有记录）
            //     auto it = _playerBlockReductions.find(playerGuid);
            //     if (it != _playerBlockReductions.end())
            //     {
            //         player->SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, it->second); // 恢复原始格挡率
            //     }
            //
            //     // 记录当前格挡率
            //     _playerBlockReductions[playerGuid] = originalBlockChance;
            //
            //     // 计算穿透后的格挡率
            //     float penetratedBlockChance = originalBlockChance * (1.0f - finalBlockPenetrationPct / 100.0f);
            //
            //     // 直接设置玩家的格挡率
            //     player->SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, penetratedBlockChance);
            //
            //     LOG_DEBUG("module", ">> 玩家 {} 格挡率设为{:.1f}% (原格挡率: {:.1f}%)",
            //               playerGuid, penetratedBlockChance, originalBlockChance);
            // }
            //
            // LOG_DEBUG("module", ">> 生物 {} 格挡穿透: 固定值={}, 百分比={:.1f}% (玩家格挡率: {:.1f}%)",
            //           creature->GetEntry(), config->blockPenetration, config->blockPenetrationPct, playerBlockChance);
        }

        // 【修复】有配置时也添加专精效果来降低玩家躲闪/招架率
        // WoW命中机制：dodge_chance -= GetTotalAuraModifier(SPELL_AURA_MOD_EXPERTISE) * 25
        // 玩家躲闪35% = 3500点，需要专精 = 3500/25 = 140点来完全抵消
        // 每级给予 50 点专精值，幻境等级3就能抵消35%躲闪
        uint32 configExpertise = static_cast<uint32>(static_cast<long double>(huanJingLevel) * 50.0L * contentDifficultyScale); // 每级+50专精值，并按副本强度缩放
        if (configExpertise > 0)
        {
            // 移除旧的专精光环
            creature->RemoveAurasByType(SPELL_AURA_MOD_EXPERTISE);

            // 使用宠物缩放法术 61017 (Hunter Pet Scaling 04)
            // 该法术 EFFECT_2 具有 SPELL_AURA_MOD_EXPERTISE 效果
            if (Aura* expertiseAura = creature->AddAura(61017, creature))
            {
                expertiseAura->SetDuration(-1); // 永久持续
                // EFFECT_2 是专精效果
                if (AuraEffect* effect = expertiseAura->GetEffect(EFFECT_2))
                {
                    effect->ChangeAmount(int32(configExpertise)); // 设置专精值
                    LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s (有配置) 添加专精光环: %s点 (可抵消%s%%躲闪/招架)", creature->GetEntry(), configExpertise, configExpertise * 0.25f).c_str());
                }
            }
            else
            {
                // 备用方案：尝试使用术士宠物缩放法术
                if (Aura* expertiseAura2 = creature->AddAura(61013, creature))
                {
                    expertiseAura2->SetDuration(-1);
                    if (AuraEffect* effect = expertiseAura2->GetEffect(EFFECT_2))
                    {
                        effect->ChangeAmount(int32(configExpertise));
                        LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s (有配置) 使用备用专精光环: %s点", creature->GetEntry(), configExpertise).c_str());
                    }
                }
            }
        }

    } // 结束有配置的分支

    // 有配置的分支通过 SetModifierValue 修改基础修正值，需要在最后刷新一次属性；
    // 基础分支已经直接设置了最终生命值，如果这里再 UpdateAllStats 会把我们手动设置的血量覆盖掉，
    // 导致你看到的“倍率正确但最大生命值被重算成其它值”的问题，所以仅在有配置时刷新。
    if (!useBasicAdjustment)
        creature->UpdateAllStats();

    // 在UpdateAllStats之后重新应用属性调整（防止被系统覆盖）
    if (!useBasicAdjustment && config)
    {
        const CreatureTemplate* cInfo = creature->GetCreatureTemplate();
        // 重新应用护甲调整
        if (config->armorValue > 0)
        {
            AsInt64 effectiveArmorLimit4 = _maxArmorValue;
            AsInt64 safeArmorValue = ApplyOptionalUInt64Limit(config->armorValue, effectiveArmorLimit4);
            creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, SafeFloatValue(safeArmorValue, effectiveArmorLimit4));
            creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_PCT, 1.0f);
            creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE, 0.0f);
            creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT, 1.0f);
        }
        else if (config->armorMultiplier != 1.0f)
        {
            // 护甲倍率调整：基于真正的原始护甲值（从BASE_VALUE获取）
            float originalArmor = creature->GetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE);
            AsInt64 newArmorValue64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalArmor), config->armorMultiplier);
            AsInt64 effectiveArmorLimit5 = _maxArmorValue;
            AsInt64 newArmorValue = ApplyOptionalUInt64Limit(newArmorValue64, effectiveArmorLimit5);

            creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, SafeFloatValue(newArmorValue, effectiveArmorLimit5));
            creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_PCT, 1.0f);
            creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE, 0.0f);
            creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT, 1.0f);
        }

        // 重新应用攻击强度调整
        if (config->attackPowerValue > 0 || config->attackPowerMultiplier != 1.0f)
        {
            AsInt64 newAttackPower;

            if (config->attackPowerValue > 0)
            {
                newAttackPower = ApplyOptionalUInt64Limit(config->attackPowerValue, _maxAttackPowerValue);
            }
            else
            {
                // 攻击强度倍率调整：基于真正的原始攻击强度值（从BASE_VALUE获取）
                float originalAttackPower = creature->GetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE);
                AsInt64 newAttackPower64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalAttackPower), config->attackPowerMultiplier);
                AsInt64 effectiveAttackPowerLimit3 = _maxAttackPowerValue;
                newAttackPower = ApplyOptionalUInt64Limit(newAttackPower64, effectiveAttackPowerLimit3);
            }

            AsInt64 effectiveAttackPowerLimit4 = _maxAttackPowerValue;
            creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, SafeFloatValue(newAttackPower, effectiveAttackPowerLimit4));
            creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_PCT, 1.0f);
            creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, 0.0f);
            creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, 1.0f);
        }

        // 重新应用法术强度调整
        // 先清除之前的法术强度设置
        ClearCreatureSpellPower(creature);

        if (config->spellPowerValue > 0 || config->spellPowerMultiplier != 1.0f)
        {
            AsInt64 effectiveSpellPowerLimit2 = _maxSpellPowerValue;
            AsInt64 newSpellPower = 0;

            if (config->spellPowerValue > 0)
            {
                newSpellPower = ApplyOptionalUInt64Limit(config->spellPowerValue, effectiveSpellPowerLimit2);
            }
            else
            {
                // 法术强度倍率调整：基于基础法术强度值
                uint32 baseSpellPower = creature->GetLevel() * 10;
                AsInt64 newSpellPower64 = ScaleAsInt64ByDouble(AsInt256FromFloating(baseSpellPower), config->spellPowerMultiplier);
                newSpellPower = ApplyOptionalUInt64Limit(newSpellPower64, effectiveSpellPowerLimit2);
            }

            // 应用法术强度到生物 - 存储到系统中
            SetCreatureSpellPower(creature->GetGUID(), newSpellPower);

            LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 重新应用法术强度: %s", creature->GetEntry(), FormatUInt64(newSpellPower)).c_str());
        }
    }

    // 为基础调整逻辑也重新应用属性修改
    if (useBasicAdjustment)
    {
        float levelMultiplier = static_cast<float>(1.0L + static_cast<long double>(huanJingLevel) * contentDifficultyScale); // 按副本原始强度缩放
        const CreatureTemplate* cInfo = creature->GetCreatureTemplate();
        CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(creature->GetLevel(), cInfo->unit_class);

        // 重新应用护甲调整 - 基于原始护甲值
        float originalArmor = stats->GenerateArmor(cInfo);
        AsInt64 newArmorValue64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalArmor), levelMultiplier);
        AsInt64 effectiveArmorLimit3 = _maxArmorValue;
        AsInt64 newArmorValue = ApplyOptionalUInt64Limit(newArmorValue64, effectiveArmorLimit3);

        creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, SafeFloatValue(newArmorValue, effectiveArmorLimit3));
        creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_PCT, 1.0f);
        creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE, 0.0f);
        creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT, 1.0f);

        // 重新应用攻击强度调整 - 基于原始攻击强度
        AsInt64 originalAttackPower = stats->AttackPower;
        AsInt64 newAttackPower64 = ScaleAsInt64ByDouble(AsInt256FromFloating(originalAttackPower), levelMultiplier);
        AsInt64 effectiveAttackPowerLimit2 = _maxAttackPowerValue;
        AsInt64 newAttackPower = ApplyOptionalUInt64Limit(newAttackPower64, effectiveAttackPowerLimit2);

        creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, SafeFloatValue(newAttackPower, effectiveAttackPowerLimit2));
        creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_PCT, 1.0f);
        creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, 0.0f);
        creature->SetModifierValue(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, 1.0f);

        // 重新应用法术强度调整 - 基于基础法术强度
        // 先清除之前的法术强度设置
        ClearCreatureSpellPower(creature);

        uint32 baseSpellPower = creature->GetLevel() * 10;
        AsInt64 newSpellPower64 = ScaleAsInt64ByDouble(AsInt256FromFloating(baseSpellPower), levelMultiplier);
        AsInt64 effectiveSpellPowerLimit3 = _maxSpellPowerValue;
        AsInt64 newSpellPower = ApplyOptionalUInt64Limit(newSpellPower64, effectiveSpellPowerLimit3);

        // 应用法术强度到生物 - 存储到系统中
        SetCreatureSpellPower(creature->GetGUID(), newSpellPower);

        LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 基础调整法术强度: %s", creature->GetEntry(), FormatUInt64(newSpellPower)).c_str());

        // 重新应用伤害调整 - 基于原始伤害值
        float originalBaseDamage = stats->GenerateBaseDamage(cInfo);
        float originalMinDamage = originalBaseDamage;
        float originalMaxDamage = originalBaseDamage * 1.5f;

        float newMinDamage = SafeDamageFloatValue(static_cast<long double>(originalMinDamage) * static_cast<long double>(levelMultiplier), _maxDamageValue);
        float newMaxDamage = SafeDamageFloatValue(static_cast<long double>(originalMaxDamage) * static_cast<long double>(levelMultiplier), _maxDamageValue);

        creature->SetStatFloatValue(UNIT_FIELD_MINDAMAGE, newMinDamage);
        creature->SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, newMaxDamage);
        creature->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, newMinDamage);
        creature->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, newMaxDamage);
    }

    // 最后再次更新属性以应用我们的修改
    creature->UpdateAllStats();

    // 设置当前血量和法力为最大值（因为UpdateAllStats只更新最大值）。
    // 配置血量使用扩展数值回写，避免最终属性刷新后被客户端可见上限截断。
    if (configuredHealthForCombat > 0)
    {
        SetCreatureCombatCreateHealth(creature, configuredHealthForCombat);
        SetCreatureCombatMaxHealth(creature, configuredHealthForCombat);
        SetCreatureCombatHealth(creature, configuredHealthForCombat);
    }
    else
    {
        SetCreatureCombatHealth(creature, GetCreatureCombatMaxHealth(creature));
    }
    SetCreatureCombatPower(creature, POWER_MANA, GetCreatureCombatMaxPower(creature, POWER_MANA));

    // 记录调整后的属性
    uint32 newLevel = creature->GetLevel();
    AsInt64 newHealth = GetCreatureCombatMaxHealth(creature);
    AsInt64 newMana = GetCreatureCombatMaxPower(creature, POWER_MANA);
    uint32 newArmor = creature->GetArmor();
    // 重新获取调整后的伤害值
    float newMinDamage = creature->GetFloatValue(UNIT_FIELD_MINDAMAGE);
    float newMaxDamage = creature->GetFloatValue(UNIT_FIELD_MAXDAMAGE);

    // 强制更新所有统计数据以确保修改生效
    creature->UpdateAllStats();
    creature->UpdateArmor();
    creature->UpdateAttackPowerAndDamage();
    creature->UpdateAttackPowerAndDamage(true);

    // 清除递归保护标记
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        _adjustingCreatures.erase(creatureKey);
    }

    LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 属性调整完成", creature->GetEntry()).c_str());
}

void HuanJingSystem::RestoreCreatureToOriginalStats(Creature* creature)
{
    if (!creature || !creature->IsAlive())
        return;

    uint64 creatureKey = GenerateCreatureKey(creature);

    // 【线程安全修复】递归保护检查需要在锁内进行，避免数据竞争
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        // 递归保护：如果这个生物正在被调整，直接返回
        if (_adjustingCreatures.find(creatureKey) != _adjustingCreatures.end())
        {
            return;
        }
    }

    // 检查是否真的需要还原（避免重复操作）
    uint32 currentAdjustedLevel = 0;
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        auto it = _creatureAdjustedLevels.find(creatureKey);
        if (it != _creatureAdjustedLevels.end())
            currentAdjustedLevel = it->second;
    }
    if (currentAdjustedLevel == 0)
    {
        // 生物没有被调整过，不需要还原
        return;
    }

    // 记录调整前的属性用于日志
    uint32 oldLevel = creature->GetLevel();
    uint32 oldHealth = creature->GetMaxHealth();
    uint32 oldMana = creature->GetMaxPower(POWER_MANA);

    // 重新初始化生物，这会将其恢复到模板定义的原始状态
    creature->SelectLevel(true);

    // 重置所有属性修正值到默认状态
    // 重置血量修正
    creature->SetModifierValue(UNIT_MOD_HEALTH, BASE_PCT, 1.0f);
    creature->SetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE, 0.0f);
    creature->SetModifierValue(UNIT_MOD_HEALTH, TOTAL_PCT, 1.0f);

    // 重置法力修正
    UnitMods manaMod = UnitMods(static_cast<uint16>(UNIT_MOD_POWER_START) + POWER_MANA);
    creature->SetModifierValue(manaMod, BASE_PCT, 1.0f);
    creature->SetModifierValue(manaMod, TOTAL_VALUE, 0.0f);
    creature->SetModifierValue(manaMod, TOTAL_PCT, 1.0f);

    // 重置护甲修正
    creature->SetModifierValue(UNIT_MOD_ARMOR, BASE_PCT, 1.0f);
    creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_VALUE, 0.0f);
    creature->SetModifierValue(UNIT_MOD_ARMOR, TOTAL_PCT, 1.0f);

    // 重置抗性修正
    auto resetResistance = [&](SpellSchools school)
    {
        UnitMods resistMod = UnitMods(UNIT_MOD_RESISTANCE_START + school);
        creature->SetModifierValue(resistMod, BASE_PCT, 1.0f);
        creature->SetModifierValue(resistMod, TOTAL_VALUE, 0.0f);
        creature->SetModifierValue(resistMod, TOTAL_PCT, 1.0f);
    };

    resetResistance(SPELL_SCHOOL_HOLY);
    resetResistance(SPELL_SCHOOL_FIRE);
    resetResistance(SPELL_SCHOOL_NATURE);
    resetResistance(SPELL_SCHOOL_FROST);
    resetResistance(SPELL_SCHOOL_SHADOW);
    resetResistance(SPELL_SCHOOL_ARCANE);

    // 重置命中率相关属性
    creature->m_modMeleeHitChance = 0.0f;
    creature->m_modRangedHitChance = 0.0f;
    creature->m_modSpellHitChance = 0.0f;
    creature->SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0f);

    // 移除无视躲闪标志（注释掉，因为FlagsExtra不能在运行时修改）
    // creature->SetFlagsExtra(creature->GetFlagsExtra() & ~CREATURE_FLAG_EXTRA_NO_DODGE);

    // 移除所有相关光环
    creature->RemoveAurasByType(SPELL_AURA_MOD_HIT_CHANCE);
    creature->RemoveAurasByType(SPELL_AURA_MOD_TARGET_RESISTANCE);

    // 清理附近玩家的护甲恢复
    std::list<Player*> nearbyPlayers;
    Acore::AnyPlayerInObjectRangeCheck checker(creature, 100.0f);
    Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(creature, nearbyPlayers, checker);
    Cell::VisitWorldObjects(creature, searcher, 100.0f);

    for (Player* player : nearbyPlayers)
    {
        if (player && player->IsInWorld())
        {
            // 移除护甲和格挡恢复逻辑 - 已禁用
            // uint32 playerGuid = player->GetGUID().GetCounter();
            // auto it = _playerArmorReductions.find(playerGuid);
            // if (it != _playerArmorReductions.end())
            // {
            //     // 恢复原始护甲值
            //     player->SetArmor(it->second);
            //
            //     // 移除记录
            //     _playerArmorReductions.erase(it);
            //
            //     LOG_DEBUG("module", ">> 恢复玩家 {} 护甲: {}", playerGuid, it->second);
            // }
            //
            // // 恢复玩家的格挡率
            // auto blockIt = _playerBlockReductions.find(playerGuid);
            // if (blockIt != _playerBlockReductions.end())
            // {
            //     // 恢复原始格挡率
            //     player->SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, blockIt->second);
            //
            //     // 移除记录
            //     _playerBlockReductions.erase(blockIt);
            //
            //     LOG_DEBUG("module", ">> 恢复玩家 {} 格挡率: {:.1f}%", playerGuid, blockIt->second);
            // }
        }
    }

    // 更新所有属性
    creature->UpdateAllStats();

    // 再次做一次安全血量检查，防止模板本身的血量配置超过上限
    ClampCreatureHealthToMax(creature);

    // 设置当前血量和法力为最大值
    SetCreatureCombatHealth(creature, GetCreatureCombatMaxHealth(creature));
    SetCreatureCombatPower(creature, POWER_MANA, GetCreatureCombatMaxPower(creature, POWER_MANA));

    // 清除幻境等级标记
    ObjectGuid guid = creature->GetGUID();
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        _creatureAdjustedLevels.erase(creatureKey);
        // 修复：同时清理鉴定组ID，防止玩家离开后鉴定ID残留
        _creatureDeathIdentificationGroup.erase(guid);
        _creatureVirtualMaxMana.erase(guid);
    }

    // 重要修复：清除血条数据
    RemoveCreatureHealthBars(creature);
    ClearCombatProtectionLogsForCreature(creature);

    // 记录还原后的属性
    uint32 newLevel = creature->GetLevel();
    uint32 newHealth = creature->GetMaxHealth();
    uint32 newMana = creature->GetMaxPower(POWER_MANA);

}

// 获取生物附近最高的幻境等级
uint32 HuanJingSystem::GetHighestHuanJingLevelNearCreature(Creature* creature, float range)
{
    if (!creature || !creature->IsInWorld())
        return 0;

    uint32 highestLevel = 0;
    Map* map = creature->GetMap();
    if (!map)
        return 0;

    // 修复：在副本中,每个玩家的副本实例是独立的
    // 单人副本:使用该玩家的幻境等级
    // 组队副本:使用队伍中最高的幻境等级
    bool isDungeon = map->IsDungeon();

    // 副本环境:玩家数量少,可以直接遍历所有玩家
    if (isDungeon)
    {
        // 检查副本中是否有多个玩家(组队情况)
        bool isGroupDungeon = false;
        Map::PlayerList const& playersCheck = map->GetPlayers();
        if (playersCheck.getSize() > 1)
        {
            // 副本中有多个玩家,判断为组队副本
            isGroupDungeon = true;
        }

        // 遍历副本中的玩家
        for (Map::PlayerList::const_iterator itr = playersCheck.begin(); itr != playersCheck.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (player && player->IsInWorld() && player->IsAlive())
            {
                if (!IsHuanJingCreatureEligibleForPlayer(creature, player))
                    continue;

                // 检查玩家是否在生物附近
                if (creature->GetDistance(player) <= range)
                {
                    uint32 playerLevel = GetPlayerHuanJingLevel(player);

                    // 修复:区分单人副本和组队副本
                    if (!isGroupDungeon)
                    {
                        // 单人副本:直接返回该玩家的幻境等级(副本是独立实例,无需考虑最高)
                        return playerLevel;
                    }
                    else
                    {
                        // 组队副本:使用最高幻境等级
                        if (playerLevel > highestLevel)
                        {
                            highestLevel = playerLevel;
                        }
                    }
                }
            }
        }
    }
    else
    {
        // 野外环境:优化 - 使用空间索引查询附近玩家,而不是遍历整个地图的所有玩家
        std::list<Player*> players;

        // 自定义检查器:获取范围内所有活着的玩家
        class AllAlivePlayersInRange
        {
        public:
            AllAlivePlayersInRange(WorldObject const* object, float maxRange) : m_pObject(object), m_fRange(maxRange) {}
            bool operator() (Player* player)
            {
                return player && player->IsAlive() && m_pObject->IsWithinDist(player, m_fRange, false);
            }
        private:
            WorldObject const* m_pObject;
            float m_fRange;
        };

        AllAlivePlayersInRange check(creature, range);
        Acore::PlayerListSearcher<AllAlivePlayersInRange> searcher(creature, players, check);
        Cell::VisitWorldObjects(creature, searcher, range);

        // 查找最高幻境等级
        for (Player* player : players)
        {
            if (!player)
                continue;

            if (!IsHuanJingCreatureEligibleForPlayer(creature, player))
                continue;

            uint32 playerLevel = GetPlayerHuanJingLevel(player);
            if (playerLevel > highestLevel)
            {
                highestLevel = playerLevel;
            }
        }
    }

    return highestLevel;
}

// 以附近最高幻境等级为准调整生物属性
void HuanJingSystem::AdjustCreatureStatsWithHighestLevel(Creature* creature, float range)
{
    if (!creature || !creature->IsInWorld())
        return;

    if (HasCreatureAttributeOverride(creature))
    {
        if (!creature->IsInCombat() && GetCreatureAdjustedHuanJingLevel(creature) > 0)
            RestoreCreatureToOriginalStats(creature);
        else
        {
            CleanupCreatureAdjustmentData(creature);
            ClearCreatureVirtualMana(creature->GetGUID());
            ClearCreatureSpellPower(creature);
        }
        return;
    }

    uint64 creatureKey = GenerateCreatureKey(creature);

    // 递归保护：如果这个生物正在被调整，直接返回
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        if (_adjustingCreatures.find(creatureKey) != _adjustingCreatures.end())
        {
            return;
        }
    }

    // 获取附近最高的幻境等级
    uint32 highestLevel = GetHighestHuanJingLevelNearCreature(creature, range);

    // 检查生物当前是否已经按照这个等级调整过了
    uint32 currentAdjustedLevel = 0;
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        auto it = _creatureAdjustedLevels.find(creatureKey);
        if (it != _creatureAdjustedLevels.end())
            currentAdjustedLevel = it->second;
    }

    // 关键修复：即使等级相同，也要检查并更新鉴定组ID，确保玩家切换等级或首次击杀时能正确掉落
    if (currentAdjustedLevel == highestLevel && highestLevel > 0)
    {
        // 等级相同，但仍需确保鉴定组ID已正确保存
        uint32 creatureId = creature->GetEntry();
        uint32 mapId = creature->GetMapId();
        uint32 areaId = creature->GetAreaId();
        uint32 difficulty = GetCreatureMapDifficulty(creature);
        uint32 creatureLevel = creature->GetLevel();
        uint32 creatureType = GetCreatureTypeByEntry(creatureId);

        HuanJingCreatureConfig const* config = GetCreatureConfig(creature, nullptr);

        if (config && config->identificationGroup > 0)
        {
            ObjectGuid guid = creature->GetGUID();

            // 检查是否需要更新鉴定组ID
            uint32 existingGroup = 0;
            bool needUpdate = false;
            {
                std::lock_guard<std::mutex> lock(_creatureMutex);
                auto identIt = _creatureDeathIdentificationGroup.find(guid);
                existingGroup = (identIt != _creatureDeathIdentificationGroup.end()) ? identIt->second.identificationGroupId : 0;
                needUpdate = (existingGroup != config->identificationGroup);
            }

            // 只有当鉴定组ID不存在或不同时才更新
            if (needUpdate)
            {
                std::lock_guard<std::mutex> lock(_creatureMutex);
                _creatureDeathIdentificationGroup[guid] = { config->identificationGroup, ResolveConfiguredItemAttributeMode(config->equipAttrValue, config->equipAttrMode), config->identificationQuality, config->identificationItemLevel, config->equipAttrValue, config->equipAttrMultiplier };
            }
        }
        return;
    }

    // 如果没有玩家在附近或幻境等级为0，恢复生物原始属性
    if (highestLevel == 0)
    {
        if (currentAdjustedLevel > 0)
        {
            RestoreCreatureToOriginalStats(creature);
        }
        return;
    }

    // 找到具有最高幻境等级的玩家
    Player* highestLevelPlayer = nullptr;
    Map* map = creature->GetMap();
    if (map)
    {
        Map::PlayerList const& players = map->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (player && player->IsInWorld() && player->IsAlive())
            {
                if (!IsHuanJingCreatureEligibleForPlayer(creature, player))
                    continue;

                if (creature->GetDistance(player) <= range)
                {
                    uint32 playerLevel = GetPlayerHuanJingLevel(player);
                    if (playerLevel == highestLevel)
                    {
                        highestLevelPlayer = player;
                        break;
                    }
                }
            }
        }
    }

    if (highestLevelPlayer)
    {
        // 先恢复原始属性，然后按新等级调整
        if (currentAdjustedLevel > 0)
        {
            RestoreCreatureToOriginalStats(creature);
        }

        // 按照最高等级调整生物属性
        AdjustCreatureStats(creature, highestLevelPlayer);

        // 更新生物的幻境等级标记
        {
            std::lock_guard<std::mutex> lock(_creatureMutex);
            _creatureAdjustedLevels[creatureKey] = highestLevel;
        }

        // 修复：在调整属性时也保存鉴定组ID，这样玩家上线时就能立即掉落鉴定属性装备
        if (highestLevel > 0)
        {
            uint32 creatureId = creature->GetEntry();
            uint32 mapId = creature->GetMapId();
            uint32 areaId = creature->GetAreaId();
            uint32 difficulty = GetCreatureMapDifficulty(creature);
            uint32 creatureLevel = creature->GetLevel();
            uint32 creatureType = GetCreatureTypeByEntry(creatureId);

            HuanJingCreatureConfig const* config = GetCreatureConfig(creature, highestLevelPlayer);

            if (config && config->identificationGroup > 0)
            {
                ObjectGuid guid = creature->GetGUID();
                {
                    std::lock_guard<std::mutex> lock(_creatureMutex);
                    _creatureDeathIdentificationGroup[guid] = { config->identificationGroup, ResolveConfiguredItemAttributeMode(config->equipAttrValue, config->equipAttrMode), config->identificationQuality, config->identificationItemLevel, config->equipAttrValue, config->equipAttrMultiplier };
                }
                LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 属性调整后，保存鉴定组ID: %s", creature->GetEntry(), config->identificationGroup).c_str());
            }
        }
    }
}

// 清理生物的调整数据
void HuanJingSystem::CleanupCreatureAdjustmentData(Creature* creature, bool keepDeathRecords /*= false*/)
{
    if (!creature)
        return;

    ObjectGuid guid = creature->GetGUID();
    uint32 creatureGuid = guid.GetCounter();
    uint64 creatureKey = GenerateCreatureKey(creature);
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        _creatureAdjustedLevels.erase(creatureKey);
        // 修复：同时清理鉴定组ID，防止玩家离开后鉴定ID残留导致其他玩家拾取错误的倍率装备
        // 【死亡记录自噬修复】死亡路径（keepDeathRecords=true）保留刚写入的鉴定组记录——
        // 原实现死亡时写入后立即在这里被 erase，拾取时缓存必然未命中
        if (!keepDeathRecords)
            _creatureDeathIdentificationGroup.erase(guid);
        _creatureVirtualMaxMana.erase(guid);
    }
    ClearCombatProtectionLogsForCreature(creature);
    LOG_DEBUG("module", ModuleOldFormat(">> 清理生物 %s 的调整数据和鉴定组ID", creature->GetEntry()).c_str());
}

// 玩家离开时重新调整附近生物：根据剩余玩家的最高幻境等级调整，如果没有其他玩家则还原到原始状态
void HuanJingSystem::ReAdjustNearbyCreaturesOnPlayerLeave(Player* player, float range)
{
    if (!player)
        return;

    uint32 leavingPlayerLevel = GetPlayerHuanJingLevel(player);
    if (leavingPlayerLevel == 0)
        return; // 离开的玩家没有幻境等级，不需要处理

    LOG_DEBUG("module", ModuleOldFormat(">> 玩家 %s (幻境等级 %s) 离开，开始检查附近生物需要重新调整", player->GetName(), leavingPlayerLevel).c_str());

    // 收集附近所有生物
    std::list<Creature*> nearbyCreatures;
    Acore::AnyUnitInObjectRangeCheck check(player, range);
    Acore::CreatureListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(player, nearbyCreatures, check);
    Cell::VisitGridObjects(player, searcher, range);

    uint32 adjustedCount = 0;
    uint32 restoredCount = 0;

    for (Creature* creature : nearbyCreatures)
    {
        if (!creature || !creature->IsAlive() || creature->IsPet())
            continue;

        uint64 creatureKey = GenerateCreatureKey(creature);
        uint32 currentAdjustedLevel = 0;

        {
            std::lock_guard<std::mutex> lock(_creatureMutex);
            auto it = _creatureAdjustedLevels.find(creatureKey);
            if (it != _creatureAdjustedLevels.end())
                currentAdjustedLevel = it->second;
        }

        // 如果这个生物没有被调整过，跳过
        if (currentAdjustedLevel == 0)
            continue;

        // 查找附近剩余的最高幻境等级玩家（排除离开的玩家）
        uint32 highestLevel = 0;
        Player* highestPlayer = nullptr;

        std::list<Player*> nearbyPlayers;
        Acore::AnyPlayerInObjectRangeCheck pCheck(creature, range);
        Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> pSearcher(creature, nearbyPlayers, pCheck);
        Cell::VisitWorldObjects(creature, pSearcher, range);

        for (Player* nearbyPlayer : nearbyPlayers)
        {
            if (!nearbyPlayer || nearbyPlayer->GetGUID() == player->GetGUID())
                continue; // 跳过离开的玩家

            if (!IsHuanJingCreatureEligibleForPlayer(creature, nearbyPlayer))
                continue;

            uint32 level = GetPlayerHuanJingLevel(nearbyPlayer);
            if (level > highestLevel)
            {
                highestLevel = level;
                highestPlayer = nearbyPlayer;
            }
        }

        // 根据剩余玩家情况决定如何调整
        if (highestLevel == 0 || highestPlayer == nullptr)
        {
            // 附近没有其他玩家了，还原到原始状态
            LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 附近没有其他玩家，还原到原始状态", creature->GetName()).c_str());
            RestoreCreatureToOriginalStats(creature);
            CleanupCreatureAdjustmentData(creature);
            restoredCount++;
        }
        else if (highestLevel < currentAdjustedLevel)
        {
            // 剩余玩家的最高等级低于当前调整等级，需要降级调整
            LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 需要降级调整：从等级 %s 降到等级 %s", creature->GetName(), currentAdjustedLevel, highestLevel).c_str());

            // 先还原到原始状态
            RestoreCreatureToOriginalStats(creature);

            // 按新的最高等级重新调整
            AdjustCreatureStats(creature, highestPlayer);

            // 更新调整等级记录
            {
                std::lock_guard<std::mutex> lock(_creatureMutex);
                _creatureAdjustedLevels[creatureKey] = highestLevel;
            }

            // 更新鉴定组ID
            uint32 creatureId = creature->GetEntry();
            uint32 mapId = creature->GetMapId();
            uint32 areaId = creature->GetAreaId();
            uint32 difficulty = GetCreatureMapDifficulty(creature);
            uint32 creatureLevel = creature->GetLevel();
            uint32 creatureType = GetCreatureTypeByEntry(creatureId);

            HuanJingCreatureConfig const* config = GetCreatureConfig(creature, highestPlayer);

            if (config && config->identificationGroup > 0)
            {
                ObjectGuid guid = creature->GetGUID();
                std::lock_guard<std::mutex> lock(_creatureMutex);
                _creatureDeathIdentificationGroup[guid] = { config->identificationGroup, ResolveConfiguredItemAttributeMode(config->equipAttrValue, config->equipAttrMode), config->identificationQuality, config->identificationItemLevel, config->equipAttrValue, config->equipAttrMultiplier };
            }

            adjustedCount++;
        }
        // 如果剩余玩家的最高等级 >= 当前调整等级，不需要调整
    }

    LOG_DEBUG("module", ModuleOldFormat(">> 玩家 %s 离开后，重新调整了 %s 个生物，还原了 %s 个生物", player->GetName(), adjustedCount, restoredCount).c_str());
}

// 副本重置时清理指定地图/实例的所有生物调整数据
void HuanJingSystem::CleanupInstanceCreatureAdjustments(uint32 mapId, uint32 instanceId)
{
    LOG_DEBUG("module.huanjing", ModuleOldFormat(">> 开始清理地图 %s 实例 %s 的所有生物调整数据", mapId, instanceId).c_str());

    uint32 cleanupCount = 0;
    std::vector<uint64> keysToRemove;

    // 遍历所有已调整的生物，找出属于指定地图/实例的生物
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);

        for (auto it = _creatureAdjustedLevels.begin(); it != _creatureAdjustedLevels.end(); )
        {
            uint64 creatureKey = it->first;

            // 解析生物键：高32位存储(地图ID << 16 | 实例ID)，低32位存储生物GUID
            uint32 keyMapId = (creatureKey >> 48) & 0xFFFF;
            uint32 keyInstanceId = (creatureKey >> 32) & 0xFFFF;

            // 检查是否匹配目标地图和实例
            bool shouldRemove = false;
            if (instanceId == 0)
            {
                // 如果 instanceId 为 0，清理该地图的所有实例
                shouldRemove = (keyMapId == mapId);
            }
            else
            {
                // 清理指定的地图和实例
                shouldRemove = (keyMapId == mapId && keyInstanceId == instanceId);
            }

            if (shouldRemove)
            {
                keysToRemove.push_back(creatureKey);
                it = _creatureAdjustedLevels.erase(it);
                cleanupCount++;
            }
            else
            {
                ++it;
            }
        }

        // 同时清理鉴定组ID（需要转换为 ObjectGuid）
        // 注意：ObjectGuid 无法直接从 creatureKey 构造，所以这里只清理 _creatureAdjustedLevels
        // 鉴定组ID会在生物重生或下次调整时自动更新
    }

    LOG_DEBUG("module.huanjing", ModuleOldFormat(">> 地图 %s 实例 %s 清理完成，共清理了 %s 个生物的调整数据", mapId, instanceId, cleanupCount).c_str());
}

// 保存生物死亡前的调整等级，用于重生时恢复
void HuanJingSystem::SaveCreatureAdjustmentForRespawn(Creature* creature)
{
    if (!creature)
        return;

    uint64 creatureKey = GenerateCreatureKey(creature);
    uint32 savedLevel = 0;
    bool hasSavedLevel = false;
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        auto it = _creatureAdjustedLevels.find(creatureKey);
        if (it != _creatureAdjustedLevels.end())
        {
            savedLevel = it->second;
            hasSavedLevel = true;
        }
    }
    if (hasSavedLevel)
    {
        // 保存当前的调整等级
        {
            std::lock_guard<std::mutex> lock(_creatureMutex);
            _creatureRespawnAdjustments[creatureKey] = savedLevel;
        }
        LOG_DEBUG("module", ModuleOldFormat(">> 保存生物 %s 重生前的调整等级: %s", creature->GetName(), savedLevel).c_str());
    }
}

// 生物重生后恢复属性调整
void HuanJingSystem::RestoreCreatureAdjustmentAfterRespawn(Creature* creature)
{
    if (!creature)
        return;

    uint64 creatureKey = GenerateCreatureKey(creature);
    uint32 savedLevel = 0;
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        auto respawnIt = _creatureRespawnAdjustments.find(creatureKey);
        if (respawnIt == _creatureRespawnAdjustments.end())
            return;

        savedLevel = respawnIt->second;
        _creatureRespawnAdjustments.erase(respawnIt);
        _creatureAdjustedLevels.erase(creatureKey);
    }

    ObjectGuid guid = creature->GetGUID();
    ClearCreatureDeathHuanJingLevel(guid);
    RemoveCreatureHealthBars(creature);
    ClearCreatureVirtualMana(guid);

    if (!creature->IsAlive())
        return;

    if (!HasCreatureAttributeOverride(creature))
    {
        if (HuanJingCreatureConfig const* config = GetMapCreatureConfig(creature, nullptr))
            ApplyResolvedCreatureAppearance(creature, *config, true);
    }

    uint32 nearbyHighestLevel = GetHighestHuanJingLevelNearCreature(creature, CREATURE_ADJUSTMENT_RECHECK_RANGE);
    if (nearbyHighestLevel == 0)
    {
        LOG_DEBUG("module", ModuleOldFormat(
            ">> 生物 %s 重生：原调整等级=%s，附近无合格幻境玩家，保持原始属性",
            creature->GetName(), savedLevel).c_str());
        return;
    }

    AdjustCreatureStatsWithHighestLevel(creature, CREATURE_ADJUSTMENT_RECHECK_RANGE);
    LOG_DEBUG("module", ModuleOldFormat(
        ">> 生物 %s 重生：已从原调整等级 %s 按附近最高等级 %s 重新应用属性",
        creature->GetName(), savedLevel, nearbyHighestLevel).c_str());
}

// 检查生物是否已经被标记
bool HuanJingSystem::IsCreatureMarked(Creature* creature)
{
    if (!creature)
        return false;

    uint64 creatureKey = GenerateCreatureKey(creature);
    std::lock_guard<std::mutex> lock(_creatureMutex);
    return _creatureAdjustedLevels.find(creatureKey) != _creatureAdjustedLevels.end();
}

// 检查生物是否正在被调整（递归保护）
bool HuanJingSystem::IsCreatureBeingAdjusted(Creature* creature)
{
    if (!creature)
        return false;

    uint64 creatureKey = GenerateCreatureKey(creature);
    std::lock_guard<std::mutex> lock(_creatureMutex);
    return _adjustingCreatures.find(creatureKey) != _adjustingCreatures.end();
}

HuanJingCreatureConfig* HuanJingSystem::GetBestMatchConfig(uint32 huanJingLevel, uint32 creatureId, uint32 mapId, uint32 areaId, uint32 difficulty, uint32 mythicDifficulty, uint32 creatureLevel, uint32 creatureType)
{
    (void)huanJingLevel;
    (void)creatureLevel;
    // 记忆化：配置只在加载/重载时变化，相同参数组合的匹配结果可直接复用，
    // 避免伤害热路径上每次 O(N) 扫描全配置表（creatureLevel 不参与匹配，不进键）
    std::array<uint32, 6> const cacheKey = { creatureId, mapId, areaId, difficulty, mythicDifficulty, creatureType };
    {
        std::lock_guard<std::mutex> lock(_configMatchCacheMutex);
        auto cached = _configMatchCache.find(cacheKey);
        if (cached != _configMatchCache.end())
            return cached->second;
    }

    uint32 checkedConfigs = 0;
    HuanJingCreatureConfig* bestConfig = nullptr;
    uint32 bestScore = 0;

    auto addSpecificityScore = [](int32 configValue, uint32 actualValue, uint32 exactScore) -> uint32
    {
        return configValue == (int32)actualValue ? exactScore : 0;
    };

    for (auto& config : _creatureConfigs)
    {
        checkedConfigs++;

        // 检查生物ID匹配
        bool creatureMatches = MatchesCondition(config.creatureId, creatureId);
        if (!creatureMatches)
        {
            continue;
        }

        // 检查地图ID匹配
        bool mapMatches = MatchesCondition(config.mapId, mapId);
        if (!mapMatches)
        {
            LOG_DEBUG("module", ModuleOldFormat(">> 配置 %s 地图ID不匹配：需要=%s, 实际=%s", config.configId, config.mapId, mapId).c_str());
            continue;
        }

        // 检查区域ID匹配
        bool areaMatches = MatchesCondition(config.areaId, areaId);
        if (!areaMatches)
        {
            LOG_DEBUG("module", ModuleOldFormat(">> 配置 %s 区域ID不匹配：需要=%s, 实际=%s", config.configId, config.areaId, areaId).c_str());
            continue;
        }

        // 检查副本难度匹配
        bool difficultyMatches = MatchesCondition(config.difficulty, difficulty);
        if (!difficultyMatches)
        {
            LOG_DEBUG("module", ModuleOldFormat(">> 配置 %s 难度不匹配：需要=%s, 实际=%s", config.configId, config.difficulty, difficulty).c_str());
            continue;
        }

        if (!MythicDifficultyMatches(config, mythicDifficulty))
        {
            LOG_DEBUG("module", ModuleOldFormat(">> 配置 %s 秘境难度不匹配：需要=%s, 实际=%s", config.configId, config.mythicDifficulty, mythicDifficulty).c_str());
            continue;
        }

        // 注意：生物等级范围字段是用来设置生物等级的，不是用来匹配的
        // 所以这里不检查生物等级范围

        // 检查生物类型匹配
        // 兼容旧地图区域表：0 表示所有非 Boss，1/2/3 仍表示精确类型，-1 表示全部。
        bool typeMatches = config.creatureType == 0
            ? creatureType != 3
            : MatchesCondition(config.creatureType, creatureType);
        if (!typeMatches)
        {
            continue;
        }

        uint32 score =
            addSpecificityScore(config.creatureId, creatureId, 100) +
            (config.creatureType == 0 && creatureType != 3 ? 40 : addSpecificityScore(config.creatureType, creatureType, 50)) +
            addSpecificityScore(config.mapId, mapId, 25) +
            (config.allMythicDifficulties ? 0 : 16) +
            addSpecificityScore(config.areaId, areaId, 10) +
            addSpecificityScore(config.difficulty, difficulty, 8);

        if (!bestConfig || score > bestScore)
        {
            bestConfig = &config;
            bestScore = score;
        }
    }

    {
        std::lock_guard<std::mutex> lock(_configMatchCacheMutex);
        _configMatchCache[cacheKey] = bestConfig; // 未命中也缓存，避免反复全表扫描
    }

    if (bestConfig)
        return bestConfig;

    LOG_DEBUG("module", ModuleOldFormat(">> 未找到匹配配置，已检查 %s 个配置", checkedConfigs).c_str());
    return nullptr;
}

void HuanJingSystem::AdjustItemStats(Item* item, Player* player)
{
    if (!item || !player)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();

    // 首先检查是否已经有属性增强记录（在锁内访问共享容器）
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto it = _itemEnhancements.find(itemGuid);
        if (it != _itemEnhancements.end())
        {
            // 如果已经有记录，移除临时禁用标记（如果有的话）
            _temporarilyDisabledItems.erase(itemGuid);
            return;
        }
    }

    // 检查旧版本记录
    float multiplier = 0.0f;
    bool hasOldRecord = false;
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto oldIt = _itemMultipliers.find(itemGuid);
        if (oldIt != _itemMultipliers.end())
        {
            multiplier = oldIt->second;
            hasOldRecord = true;
            // 如果有旧版本记录，移除临时禁用标记（如果有的话）
            _temporarilyDisabledItems.erase(itemGuid);
        }
    }

    if (hasOldRecord)
    {
        return;
    }

    // 如果没有记录，根据玩家幻境等级自动生成倍率属性
    uint32 huanJingLevel = GetPlayerHuanJingLevel(player);
    if (huanJingLevel > 0)
    {
        // 根据幻境等级计算倍率
        float multiplier = CalculateItemMultiplierByLevel(huanJingLevel, item);

        if (HasItemAttributeEffect(multiplier, 'x'))
        {
            // 应用倍率属性
            ApplyItemAttributeMultiplier(item, multiplier);

            LOG_DEBUG("module", ModuleOldFormat(">> 自动为装备 %s (GUID: %s) 生成倍率属性 %sx (玩家幻境等级: %s)", item->GetEntry(), itemGuid, multiplier, huanJingLevel).c_str());
        }
    }
}

// 使用指定幻境等级调整物品属性（用于修复拾取漏洞）
void HuanJingSystem::AdjustItemStatsWithLevel(Item* item, Player* player, uint32 huanJingLevel)
{
    if (!item || !player)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();

    // 首先检查是否已经有属性增强记录（在锁内访问共享容器）
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto it = _itemEnhancements.find(itemGuid);
        if (it != _itemEnhancements.end())
        {
            _temporarilyDisabledItems.erase(itemGuid);
            return;
        }
    }

    // 检查旧版本记录
    bool hasOldRecord = false;
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto oldIt = _itemMultipliers.find(itemGuid);
        if (oldIt != _itemMultipliers.end())
        {
            hasOldRecord = true;
            // 如果有旧版本记录，移除临时禁用标记（如果有的话）
            _temporarilyDisabledItems.erase(itemGuid);
        }
    }

    if (hasOldRecord)
    {
        return;
    }

    // 如果没有记录，根据指定的幻境等级自动生成倍率属性
    if (huanJingLevel > 0)
    {
        // 根据幻境等级计算倍率
        float multiplier = CalculateItemMultiplierByLevel(huanJingLevel, item);

        if (HasItemAttributeEffect(multiplier, 'x'))
        {
            // 应用倍率属性
            ApplyItemAttributeMultiplier(item, multiplier);

            LOG_DEBUG("module", ModuleOldFormat(">> 自动为装备 %s (GUID: %s) 生成倍率属性 %sx (使用记录的幻境等级: %s)", item->GetEntry(), itemGuid, multiplier, huanJingLevel).c_str());
        }
    }
}

// 记录生物死亡时击杀者的幻境等级
void HuanJingSystem::RecordCreatureDeathHuanJingLevel(ObjectGuid creatureGuid, uint32 huanJingLevel)
{
    std::lock_guard<std::mutex> lock(_creatureMutex);
    _creatureDeathHuanJingLevel[creatureGuid] = huanJingLevel;
}

// 获取生物死亡时记录的幻境等级
uint32 HuanJingSystem::GetCreatureDeathHuanJingLevel(ObjectGuid creatureGuid) const
{
    std::lock_guard<std::mutex> lock(_creatureMutex);
    auto it = _creatureDeathHuanJingLevel.find(creatureGuid);
    if (it != _creatureDeathHuanJingLevel.end())
    {
        return it->second;
    }
    return 0; // 未找到记录返回0
}

// 清除生物死亡时记录的幻境等级
void HuanJingSystem::ClearCreatureDeathHuanJingLevel(ObjectGuid creatureGuid)
{
    _creatureDeathHuanJingLevel.erase(creatureGuid);
    _creatureDeathIdentificationGroup.erase(creatureGuid);
    LOG_DEBUG("module", ModuleOldFormat(">> 清除生物 %s 的死亡幻境等级和鉴定组记录", creatureGuid.GetCounter()).c_str());
}

uint32 HuanJingSystem::GetCreatureTypeByEntry(uint32 entry) const
{
    CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(entry);
    if (!creatureTemplate)
        return 1; // 默认普通

    // 根据生物模板判断类型
    if (creatureTemplate->rank == CREATURE_ELITE_RARE || creatureTemplate->rank == CREATURE_ELITE_ELITE)
        return 2; // 精英
    else if (creatureTemplate->rank == CREATURE_ELITE_RAREELITE || creatureTemplate->rank == CREATURE_ELITE_WORLDBOSS)
        return 3; // BOSS
    else
        return 1; // 普通
}

uint32 HuanJingSystem::GetCreatureMapDifficulty(Creature const* creature) const
{
    if (!creature)
        return 0;

    Map* map = creature->GetMap();
    if (!map)
        return 0;

    if (map->IsRaid())
    {
        if (map->Is25ManRaid())
            return map->IsHeroic() ? 3 : 2;

        return map->IsHeroic() ? 1 : 0;
    }

    return map->IsHeroic() ? 1 : 0;
}

uint32 HuanJingSystem::GetCreatureMythicDifficulty(Creature const* creature) const
{
    if (!creature)
        return 0;

    Map* map = creature->GetMap();
    return map ? map->GetMapRank() : 0;
}

bool HuanJingSystem::MatchesCondition(int32 configValue, uint32 actualValue)
{
    return (configValue == -1) || (configValue == (int32)actualValue);
}

float HuanJingSystem::CalculateItemMultiplierByLevel(uint32 huanJingLevel, Item* item)
{
    if (!item)
        return 1.0f;

    // 幻境等级0或1都不增强属性
    if (huanJingLevel <= 1)
        return 1.0f;

    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
        return 1.0f;

    // 只对有任何可增强属性的装备应用倍率（不再仅限于 ItemStat，有护甲/抗性/伤害也算）
    bool hasStats = false;

    // 1) 常规 ItemStat 属性（力量/敏捷/耐力等）
    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        if (itemTemplate->ItemStat[i].ItemStatType != 0 && itemTemplate->AsItemStatValue[i] != 0)
        {
            hasStats = true;
            break;
        }
    }

    // 2) 只有护甲或抗性的装备
    if (!hasStats)
    {
        if (itemTemplate->AsArmor > 0 ||
            itemTemplate->HolyRes > 0 || itemTemplate->FireRes > 0 ||
            itemTemplate->NatureRes > 0 || itemTemplate->FrostRes > 0 ||
            itemTemplate->ShadowRes > 0 || itemTemplate->ArcaneRes > 0)
        {
            hasStats = true;
        }
    }

    // 3) 只有伤害的武器（白字伤害）
    if (!hasStats)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            if (itemTemplate->Damage[i].DamageMin > 0.0f || itemTemplate->Damage[i].DamageMax > 0.0f)
            {
                hasStats = true;
                break;
            }
        }
    }

    if (!hasStats)
        return 1.0f;

    // 幻境等级直接作为倍率
    // 例如：幻境等级10 = 10倍属性
    float multiplier = static_cast<float>(huanJingLevel);

    LOG_DEBUG("module", ModuleOldFormat(">> 计算装备倍率：幻境等级 %s -> 倍率 %sx", huanJingLevel, multiplier).c_str());

    return multiplier;
}

long double HuanJingSystem::CalculateCreatureContentDifficultyScale(Creature* creature) const
{
    if (!creature)
        return 1.0L;

    uint32 creatureLevel = creature->GetLevel();
    if (CreatureTemplate const* creatureTemplate = creature->GetCreatureTemplate())
    {
        uint32 templateLevel = std::max<uint32>(creatureTemplate->minlevel, creatureTemplate->maxlevel);
        if (templateLevel > 0)
            creatureLevel = templateLevel;
    }

    long double levelProgress = ClampLongDouble(static_cast<long double>(creatureLevel) / 80.0L, 0.05L, 1.05L);
    long double levelScale = 0.08L + 0.92L * std::pow(levelProgress, 1.25L);
    long double mapScale = 0.85L;

    if (Map* map = creature->GetMap())
    {
        if (map->IsRaid())
        {
            mapScale = map->Is25ManRaid() ? 1.10L : 1.00L;
            if (map->IsHeroic())
                mapScale += 0.10L;
        }
        else if (map->IsNonRaidDungeon())
        {
            mapScale = map->IsHeroic() ? 0.95L : 0.85L;
        }
        else if (map->IsDungeon())
        {
            mapScale = 0.90L;
        }
        else
        {
            mapScale = 0.65L;
        }
    }

    if (creature->IsDungeonBoss())
        mapScale += 0.08L;

    return ClampLongDouble(levelScale * mapScale, 0.08L, 1.25L);
}

void HuanJingSystem::ApplyItemAttributeMultiplier(Item* item, float multiplier, char multiplierMode)
{
    multiplierMode = NormalizeItemAttributeMode(multiplierMode);

    if (!item || !HasItemAttributeEffect(multiplier, multiplierMode))
    {
        return;
    }

    uint32 itemGuid = item->GetGUID().GetCounter();
    Player* owner = item->GetOwner();
    if (!owner)
    {
        return;
    }

    // 检查装备是否已经被修改过
    bool alreadyEnhanced = false;
    bool sameEnhancement = false;
    int requestedMultiplier = static_cast<int>(multiplier);
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto enhancementIt = _itemEnhancements.find(itemGuid);
        alreadyEnhanced = (enhancementIt != _itemEnhancements.end());
        if (alreadyEnhanced)
        {
            sameEnhancement = enhancementIt->second.multiplier == requestedMultiplier &&
                              NormalizeItemAttributeMode(enhancementIt->second.multiplierMode) == multiplierMode;
        }
    }
    if (sameEnhancement)
    {
        return;
    }

    // 构建属性数据字符串
    int storedMultiplier = static_cast<int>(multiplier);
    std::string attributeData = BuildAttributeDataString(item, storedMultiplier, multiplierMode);

    // 保存到数据库
    SaveItemAttributeEnhancement(owner->GetGUID().GetCounter(), itemGuid, item->GetEntry(), storedMultiplier, multiplierMode, attributeData);

    // 创建内存中的增强数据
    ItemAttributeEnhancement enhancement;
    enhancement.playerGuid = owner->GetGUID().GetCounter();
    enhancement.itemGuid = itemGuid;
    enhancement.itemId = item->GetEntry();
    enhancement.multiplier = storedMultiplier;
    enhancement.multiplierMode = multiplierMode;
    enhancement.attributeData = attributeData;

    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        _itemEnhancements[itemGuid] = enhancement;
    }

    item->SetState(ITEM_CHANGED, owner);

    // 如果装备已经穿戴，强制更新玩家属性
    if (item->IsEquipped())
    {
        owner->_ApplyItemMods(item, item->GetSlot(), false); // 先移除
        owner->_ApplyItemMods(item, item->GetSlot(), true);  // 再应用
        owner->UpdateAllStats();

    }

}


float HuanJingSystem::GetItemAttributeMultiplier(Item* item) const
{
    if (!item)
        return 1.0f;

    uint32 itemGuid = item->GetGUID().GetCounter();

    std::lock_guard<std::mutex> lock(_itemMutex);

    // 检查内存中的增强数据
    auto it = _itemEnhancements.find(itemGuid);
    if (it != _itemEnhancements.end())
    {
        return it->second.multiplier;
    }

    // 检查旧版本的倍率记录（兼容性）
    auto oldIt = _itemMultipliers.find(itemGuid);
    if (oldIt != _itemMultipliers.end())
    {
        return oldIt->second;
    }

    return 1.0f;
}

char HuanJingSystem::GetItemAttributeMode(Item* item) const
{
    if (!item)
        return 'x';

    uint32 itemGuid = item->GetGUID().GetCounter();

    std::lock_guard<std::mutex> lock(_itemMutex);
    auto it = _itemEnhancements.find(itemGuid);
    if (it != _itemEnhancements.end())
        return NormalizeItemAttributeMode(it->second.multiplierMode);

    return 'x';
}

void HuanJingSystem::RemoveItemAttributeMultiplier(Item* item)
{
    if (!item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();

    bool hasEnhancement = false;
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto it = _itemEnhancements.find(itemGuid);
        if (it != _itemEnhancements.end())
        {
            _itemEnhancements.erase(it);
            hasEnhancement = true;
        }
    }

    if (hasEnhancement)
    {
        // 从数据库删除
        DeleteItemAttributeEnhancement(itemGuid);

        // 更新装备状态
        item->SetState(ITEM_CHANGED, item->GetOwner());

        return;
    }

    // 兼容旧版本的倍率记录
    float multiplier = 0.0f;
    bool hasOldRecord = false;
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto oldIt = _itemMultipliers.find(itemGuid);
        if (oldIt != _itemMultipliers.end())
        {
            multiplier = oldIt->second;
            hasOldRecord = true;
            // 移除倍率记录
            _itemMultipliers.erase(oldIt);
        }
    }

    if (hasOldRecord)
    {
        // 更新装备状态
        item->SetState(ITEM_CHANGED, item->GetOwner());

    }
}

// 应用属性修正到玩家身上（参考 mod-item-growth 的正确实现）
// 【性能优化】添加 skipUpdate 参数，批量应用属性时跳过Update，最后统一更新
void HuanJingSystem::ApplyStatModifier(Player* player, uint32 statType, AsInt64 const& value, bool apply, bool skipUpdate)
{
    if (!player || value == 0)
        return;

    int32 safeInt32Value = ClampToInt32(value);
    int32 legacyValue = safeInt32Value;
    float statModValue = float(safeInt32Value);

    if (statType < MAX_ITEM_MOD && statType != 2 && statType != 40)
    {
        ItemModType const mod = ItemModType(statType);
        AsInt64 compatValue = safeInt32Value;
        switch (mod)
        {
            case ITEM_MOD_MANA:
            case ITEM_MOD_HEALTH:
            case ITEM_MOD_AGILITY:
            case ITEM_MOD_STRENGTH:
            case ITEM_MOD_INTELLECT:
            case ITEM_MOD_SPIRIT:
            case ITEM_MOD_STAMINA:
            case ITEM_MOD_ATTACK_POWER:
            case ITEM_MOD_RANGED_ATTACK_POWER:
            case ITEM_MOD_BLOCK_VALUE:
                compatValue = AsInt64(statModValue);
                break;
            default:
                break;
        }

        player->ApplyAsStatBonus(mod, value, compatValue, apply);
    }

    // 使用正确的属性修改器系统，参考 mod-item-growth 的实现
    switch (statType)
    {
        case ITEM_MOD_MANA:
            player->HandleStatModifier(UNIT_MOD_MANA, TOTAL_VALUE, statModValue, apply);
            if (!skipUpdate) player->UpdateMaxPower(POWER_MANA);
            break;
        case ITEM_MOD_HEALTH:
            player->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, statModValue, apply);
            if (!skipUpdate) player->UpdateMaxHealth();
            break;
        case ITEM_MOD_AGILITY:
            player->HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, statModValue, apply);
            if (!skipUpdate) player->UpdateStats(STAT_AGILITY);
            break;
        case ITEM_MOD_STRENGTH:
            player->HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, statModValue, apply);
            if (!skipUpdate) player->UpdateStats(STAT_STRENGTH);
            break;
        case ITEM_MOD_INTELLECT:
            // 关键修复：使用 TOTAL_VALUE 而不是 BASE_VALUE，避免与核心代码的装备属性处理冲突
            player->HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, statModValue, apply);
            if (!skipUpdate)
            {
                player->UpdateStats(STAT_INTELLECT);
                player->UpdateMaxPower(POWER_MANA);
            }
            break;
        case ITEM_MOD_SPIRIT:
            // 关键修复：使用 TOTAL_VALUE 而不是 BASE_VALUE，避免与核心代码的装备属性处理冲突
            player->HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, statModValue, apply);
            if (!skipUpdate) player->UpdateStats(STAT_SPIRIT);
            break;
        case ITEM_MOD_STAMINA:
            // 关键修复：使用 TOTAL_VALUE 而不是 BASE_VALUE，避免与核心代码的装备属性处理冲突
            player->HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, statModValue, apply);
            if (!skipUpdate)
            {
                player->UpdateStats(STAT_STAMINA);
                player->UpdateMaxHealth();
            }
            break;
        case ITEM_MOD_DEFENSE_SKILL_RATING:
            player->ApplyRatingMod(CR_DEFENSE_SKILL, legacyValue, apply);
            break;
        case ITEM_MOD_DODGE_RATING:
            player->ApplyRatingMod(CR_DODGE, legacyValue, apply);
            break;
        case ITEM_MOD_PARRY_RATING:
            player->ApplyRatingMod(CR_PARRY, legacyValue, apply);
            break;
        case ITEM_MOD_BLOCK_RATING:
            player->ApplyRatingMod(CR_BLOCK, legacyValue, apply);
            break;
        case ITEM_MOD_HIT_MELEE_RATING:
            player->ApplyRatingMod(CR_HIT_MELEE, legacyValue, apply);
            if (!skipUpdate) player->UpdateMeleeHitChances();
            break;
        case ITEM_MOD_HIT_RANGED_RATING:
            player->ApplyRatingMod(CR_HIT_RANGED, legacyValue, apply);
            if (!skipUpdate) player->UpdateRangedHitChances();
            break;
        case ITEM_MOD_HIT_SPELL_RATING:
            player->ApplyRatingMod(CR_HIT_SPELL, legacyValue, apply);
            if (!skipUpdate) player->UpdateSpellHitChances();
            break;
        case ITEM_MOD_CRIT_MELEE_RATING:
            player->ApplyRatingMod(CR_CRIT_MELEE, legacyValue, apply);
            break;
        case ITEM_MOD_CRIT_RANGED_RATING:
            player->ApplyRatingMod(CR_CRIT_RANGED, legacyValue, apply);
            break;
        case ITEM_MOD_CRIT_SPELL_RATING:
            player->ApplyRatingMod(CR_CRIT_SPELL, legacyValue, apply);
            break;
        case ITEM_MOD_HASTE_MELEE_RATING:
            player->ApplyRatingMod(CR_HASTE_MELEE, legacyValue, apply);
            break;
        case ITEM_MOD_HASTE_RANGED_RATING:
            player->ApplyRatingMod(CR_HASTE_RANGED, legacyValue, apply);
            break;
        case ITEM_MOD_HASTE_SPELL_RATING:
            player->ApplyRatingMod(CR_HASTE_SPELL, legacyValue, apply);
            break;
        case ITEM_MOD_HIT_RATING:
            player->ApplyRatingMod(CR_HIT_MELEE, legacyValue, apply);
            player->ApplyRatingMod(CR_HIT_RANGED, legacyValue, apply);
            player->ApplyRatingMod(CR_HIT_SPELL, legacyValue, apply);
            if (!skipUpdate)
            {
                player->UpdateMeleeHitChances();
                player->UpdateRangedHitChances();
                player->UpdateSpellHitChances();
            }
            break;
        case ITEM_MOD_CRIT_RATING:
            player->ApplyRatingMod(CR_CRIT_MELEE, legacyValue, apply);
            player->ApplyRatingMod(CR_CRIT_RANGED, legacyValue, apply);
            player->ApplyRatingMod(CR_CRIT_SPELL, legacyValue, apply);
            break;
        case ITEM_MOD_RESILIENCE_RATING:
            player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, legacyValue, apply);
            player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, legacyValue, apply);
            player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, legacyValue, apply);
            break;
        case ITEM_MOD_HASTE_RATING:
            player->ApplyRatingMod(CR_HASTE_MELEE, legacyValue, apply);
            player->ApplyRatingMod(CR_HASTE_RANGED, legacyValue, apply);
            player->ApplyRatingMod(CR_HASTE_SPELL, legacyValue, apply);
            break;
        case ITEM_MOD_EXPERTISE_RATING:
            player->ApplyRatingMod(CR_EXPERTISE, legacyValue, apply);
            break;
        case ITEM_MOD_ATTACK_POWER:
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, statModValue, apply);
            break;
        case ITEM_MOD_RANGED_ATTACK_POWER:
            player->HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, statModValue, apply);
            break;
        case ITEM_MOD_SPELL_POWER:
            // 法术强度（同时影响伤害和治疗）
            player->ApplySpellPowerBonus(safeInt32Value, apply);
            break;
        case ITEM_MOD_MANA_REGENERATION:
            // 法力恢复
            player->ApplyManaRegenBonus(safeInt32Value, apply);
            break;
        case ITEM_MOD_HEALTH_REGEN:
            // 生命恢复
            player->ApplyHealthRegenBonus(safeInt32Value, apply);
            break;
        case ITEM_MOD_SPELL_PENETRATION:
            // 法术穿透
            player->ApplySpellPenetrationBonus(safeInt32Value, apply);
            break;
        case ITEM_MOD_ARMOR_PENETRATION_RATING:
            player->ApplyRatingMod(CR_ARMOR_PENETRATION, legacyValue, apply);
            // 护甲穿透不需要额外的Update函数，ApplyRatingMod已经处理了
            break;
        default:
            LOG_WARN("module", ModuleOldFormat(">> 未知的属性类型: %s", statType).c_str());
            break;
    }
}

// 应用幻境增强效果（参考强化系统实现）
void HuanJingSystem::ApplyHuanJingEnhancement(Player* player, Item* item, bool updateStats)
{
    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();

    auto perfStart = std::chrono::high_resolution_clock::now();
    auto perfCheckOld = perfStart;

    // 在应用之前，先移除已经应用的旧属性，防止重复叠加
    std::string oldAppliedStats = GetAppliedHuanJingEnhancement(player, item);
    if (!oldAppliedStats.empty())
    {
        RemoveHuanJingEnhancement(player, item);
    }

    auto perfAfterRemove = std::chrono::high_resolution_clock::now();

    // 关键修复：不再调用RemoveGrowthAura，避免BASE_VALUE和TOTAL_VALUE冲突
    // 成长系统使用BASE_VALUE，幻境系统使用TOTAL_VALUE
    // 如果调用RemoveGrowthAura，会导致属性修改器类型不匹配，造成重复应用
    // 现在成长系统已经完全禁用属性应用（在装备和登录时），所以不需要移除
#ifdef MODULE_ITEM_GROWTH
    // if (sItemGrowthMgr && sItemGrowthMgr->CanItemGrow(item))
    // {
    //     sItemGrowthMgr->RemoveGrowthAura(player, itemGuid);  // 已禁用
    //     LOG_DEBUG("module.huanjing", "ApplyHuanJingEnhancement: 已移除成长系统的旧属性");
    // }
#endif

    // 关键修复：确保GUID正确同步到随机属性字段（参考item-growth模块逻辑）
    bool hasEnhancement = false;
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        hasEnhancement = (_itemEnhancements.find(itemGuid) != _itemEnhancements.end());
    }
    float multiplier = GetItemAttributeMultiplier(item);
    char multiplierMode = GetItemAttributeMode(item);

    // 辅助函数：将鉴定/成长/强化模块的自定义属性按幻境倍率应用到玩家身上
    // 重要修改：即使倍率为1.0，也要应用成长属性，确保成长系统的属性正常工作
    auto applyCustomAttributesWithMultiplier = [&](float effMultiplier, char effMode, std::string& trackingData, bool& first)
    {
#ifdef MODULE_ITEM_IDENTIFICATION
        // 修改：移除倍率检查，允许倍率=1.0时也应用属性
        // if (effMultiplier <= 1.0f)
        //     return;

        // 确保倍率至少为1.0（即使没有幻境倍率，也要应用基础值）
        effMode = NormalizeItemAttributeMode(effMode);

        if (effMode == 'x' && effMultiplier < 1.0f)
            effMultiplier = 1.0f;

        if (!sItemIdentificationSystem || !sItemIdentificationSystem->_enabled)
            return;

        uint32 itemEntry = item->GetEntry();
        uint32 guidCounter = item->GetGUID().GetCounter();

        auto perfBeforeCache = std::chrono::high_resolution_clock::now();

        // 【性能关键优化】先从缓存读取，避免重复查询数据库
        ItemIdentificationSystem::AllModuleData moduleData;
        uint64 cacheKey = ((uint64)itemEntry << 32) | guidCounter;
        bool cacheHit = false;

        {
            std::lock_guard<std::mutex> lock(sItemIdentificationSystem->_batchCacheMutex);
            auto it = sItemIdentificationSystem->_batchQueryCache.find(cacheKey);

            if (it != sItemIdentificationSystem->_batchQueryCache.end() &&
                (time(nullptr) - it->second.cacheTime) < sItemIdentificationSystem->BATCH_CACHE_EXPIRE_TIME)
            {
                moduleData = it->second.data;
                cacheHit = true;
            }
        }

        // 如果缓存未命中，才查询数据库（登录时不应该发生，因为已预加载）
        if (!cacheHit)
        {
            moduleData = sItemIdentificationSystem->QueryAllModuleData(itemEntry, guidCounter);

            // 写入缓存
            std::lock_guard<std::mutex> lock(sItemIdentificationSystem->_batchCacheMutex);
            ItemIdentificationSystem::BatchQueryCache cache;
            cache.data = moduleData;
            cache.cacheTime = time(nullptr);
            sItemIdentificationSystem->_batchQueryCache[cacheKey] = cache;
        }

        auto perfAfterCache = std::chrono::high_resolution_clock::now();
        uint32 attrCount = 0; // 统计属性应用次数
        auto perfBeforeParse = std::chrono::high_resolution_clock::now();

        auto parseAndApply = [&](const std::string& attrsStr, bool baseValueAlreadyApplied)
        {
            if (attrsStr.empty())
                return;

            std::istringstream stream(attrsStr);
            std::string attrPair;
            while (std::getline(stream, attrPair, ','))
            {
                if (attrPair.empty())
                    continue;

                std::istringstream pairStream(attrPair);
                uint32 attrType = 0;
                AsInt64 baseValue = 0;
                if (!(pairStream >> attrType >> baseValue))
                    continue;

                // 计算倍率后的完整值。鉴定属性已由物品属性模块应用，这里只补幻境增量；
                // 成长属性由幻境统一托管，仍应用完整值。
                AsInt64 enhancedValue = CalculateEnhancedAttributeValue(baseValue, effMultiplier, effMode);
                AsInt64 valueToApply = baseValueAlreadyApplied ? enhancedValue - baseValue : enhancedValue;
                if (valueToApply <= 0)
                    continue;

                // 【性能优化】登录时跳过Update，避免每个属性都触发重新计算
                ApplyStatModifier(player, attrType, valueToApply, true, !updateStats);
                attrCount++; // 统计

                if (!first)
                    trackingData += ",";
                trackingData += Acore::ToString(attrType) + " " + Acore::ToString(valueToApply);
                first = false;
            }

        };

        // 1) 鉴定系统的基础属性与追加属性
        if (!moduleData.baseAttributes.empty())
        {
            parseAndApply(moduleData.baseAttributes, true);
        }
        if (!moduleData.additionalAttributes.empty())
        {
            parseAndApply(moduleData.additionalAttributes, true);
        }

        // 2) 成长系统属性（growthData 格式：level|curExp|requiredExp|attrs）
        {
            std::string growthAttrs;
            std::string growthPercentAttrs; // 新增：百分比属性

            // 优先使用批量查询返回的数据
            if (!moduleData.growthData.empty())
            {
                std::string growth = moduleData.growthData;
                size_t pos = growth.rfind('|');
                if (pos != std::string::npos && pos + 1 < growth.size())
                {
                    growthAttrs = growth.substr(pos + 1);
                }
            }

            // 【性能优化】完全依赖批量查询返回的数据，不再进行额外的数据库查询
            // 如果批量查询没有数据，说明物品确实没有成长属性，无需额外查询
            // 这避免了每件装备都触发额外的数据库查询，大幅提升性能

            // 备注：百分比属性已在 QueryAllModuleData 的成长系统查询中一并获取
            // 格式：level|curExp|requiredExp|attrs|percentAttrs (新增第5字段)
            if (!moduleData.growthData.empty())
            {
                std::string growth = moduleData.growthData;
                std::vector<std::string> parts;
                std::istringstream ss(growth);
                std::string part;
                while (std::getline(ss, part, '|'))
                {
                    parts.push_back(part);
                }

                // parts[0] = level, parts[1] = curExp, parts[2] = requiredExp,
                // parts[3] = attrs, parts[4] = percentAttrs (如果存在)
                if (parts.size() >= 4)
                {
                    growthAttrs = parts[3];
                }
                if (parts.size() >= 5)
                {
                    growthPercentAttrs = parts[4];
                }
            }

            // 应用基础成长属性
            if (!growthAttrs.empty())
            {
                parseAndApply(growthAttrs, false);
            }

            // 关键新增：应用百分比属性
            // 百分比属性不受幻境倍率影响，因为它们本身就是百分比加成
            if (!growthPercentAttrs.empty())
            {
                parseAndApply(growthPercentAttrs, false);
            }
        }

        // 3) 强化系统属性（enhancementData 格式：level|attrs）
        //    注意：强化系统本身已经直接把「基础强化值」应用到了玩家身上
        //    这里按幻境倍率只额外应用「增量部分」，防止重复叠加：
        //    最终效果 = 基础强化值(来自强化模块) + 基础强化值×(倍率-1)(来自幻境) = 基础强化值×倍率
        if (!moduleData.enhancementData.empty())
        {
            std::string enh = moduleData.enhancementData;
            size_t pos = enh.find('|');
            if (pos != std::string::npos && pos + 1 < enh.size())
            {
                std::string enhAttrs = enh.substr(pos + 1);
                if (!enhAttrs.empty())
                {
                    std::istringstream stream(enhAttrs);
                    std::string attrPair;
                    while (std::getline(stream, attrPair, ','))
                    {
                        if (attrPair.empty())
                            continue;

                        std::istringstream pairStream(attrPair);
                        uint32 attrType = 0;
                        AsInt64 baseValue = 0;
                        if (!(pairStream >> attrType >> baseValue))
                            continue;

                        if (baseValue <= 0)
                            continue;

                        // 计算倍率后的总值，并只应用相对基础值的增量部分
                        AsInt64 enhancedValue = CalculateEnhancedAttributeValue(baseValue, effMultiplier, effMode);
                        AsInt64 diffValue = enhancedValue - baseValue;

                        if (diffValue <= 0)
                            continue;

                        ApplyStatModifier(player, attrType, diffValue, true, !updateStats);

                        if (!first)
                            trackingData += ",";
                        trackingData += Acore::ToString(attrType) + " " + Acore::ToString(diffValue);
                        first = false;
                    }
                }
            }
        }
#endif // MODULE_ITEM_IDENTIFICATION
    };

    // 检查是否有增强数据（在锁内拷贝字符串，锁外解析）
    std::string enhancementData;
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto it = _itemEnhancements.find(itemGuid);
        if (it != _itemEnhancements.end())
        {
            enhancementData = it->second.attributeData;
        }
    }

    if (!enhancementData.empty())
    {
        // 解析增强属性数据，格式：属性类型 原值 增强值,属性类型 原值 增强值
        std::vector<std::string> attributePairs;
        std::stringstream ss(enhancementData);
        std::string pair;

        while (std::getline(ss, pair, ','))
        {
            if (!pair.empty())
                attributePairs.push_back(pair);
        }

        // 构建跟踪数据字符串（格式：属性类型 差值,属性类型 差值）
        std::string trackingData = "";
        bool first = true;

        bool applyTemplateStats = !HasIdentificationBaseAttributes(item);

        // 处理每个属性对。鉴定基础属性替换官方属性时，旧记录中的模板增强也必须跳过。
        if (applyTemplateStats)
        {
            for (const std::string& attrPair : attributePairs)
            {
                std::istringstream iss(attrPair);
                uint32 statType;
                AsInt64 originalValue, enhancedValue;

                if (iss >> statType >> originalValue >> enhancedValue)
                {
                    // 计算增强的差值
                    AsInt64 enhancementDiff = enhancedValue - originalValue;

                    if (enhancementDiff > 0)
                    {

                        // 应用增强的部分
                        ApplyStatModifier(player, statType, enhancementDiff, true, !updateStats);

                        // 添加到跟踪数据
                        if (!first) trackingData += ",";
                        trackingData += Acore::ToString(statType) + " " + Acore::ToString(enhancementDiff);
                        first = false;
                    }
                }
            }
        }

        // 追加：让基础属性 / 追加属性 / 成长属性 / 强化属性按幻境倍率生效
        auto perfBeforeCustom = std::chrono::high_resolution_clock::now();
        applyCustomAttributesWithMultiplier(multiplier, multiplierMode, trackingData, first);
        auto perfAfterCustom = std::chrono::high_resolution_clock::now();

        // 跟踪已应用的幻境增强效果
        if (!trackingData.empty())
        {
            TrackAppliedHuanJingEnhancement(player, item, trackingData);
        }
    }
    else
    {
        // 兼容旧版本的倍率记录
        float multiplier = GetItemAttributeMultiplier(item);
        if (HasItemAttributeEffect(multiplier, multiplierMode))
        {

            // 获取装备的原始属性
            ItemTemplate const* itemTemplate = item->GetTemplate();
            if (itemTemplate)
            {
                // 构建跟踪数据字符串
                std::string trackingData = "";
                bool first = true;

                // 遍历装备的所有属性。存在鉴定基础属性时，模板属性已被替换。
                if (!HasIdentificationBaseAttributes(item))
                {
                    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
                    {
                        if (itemTemplate->ItemStat[i].ItemStatType != 0 && itemTemplate->AsItemStatValue[i] != 0)
                        {
                            uint32 statType = itemTemplate->ItemStat[i].ItemStatType;
                            AsInt64 originalValue = itemTemplate->AsItemStatValue[i];

                            // 计算增强的差值
                            AsInt64 enhancedValue = CalculateEnhancedAttributeValue(originalValue, multiplier, multiplierMode);
                            AsInt64 enhancementDiff = enhancedValue - originalValue;

                            if (enhancementDiff > 0)
                            {

                                // 应用增强的部分
                                ApplyStatModifier(player, statType, enhancementDiff, true, !updateStats);

                                // 添加到跟踪数据
                                if (!first) trackingData += ",";
                                trackingData += Acore::ToString(statType) + " " + Acore::ToString(enhancementDiff);
                                first = false;
                            }
                        }
                    }
                }

                // 追加：让基础属性 / 追加属性 / 成长属性 / 强化属性按幻境倍率生效
                applyCustomAttributesWithMultiplier(multiplier, multiplierMode, trackingData, first);

                // 跟踪已应用的幻境增强效果
                if (!trackingData.empty())
                {
                    TrackAppliedHuanJingEnhancement(player, item, trackingData);
                }
            }
        }
    }

    if (updateStats)
    {
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateMaxHealth();
        player->UpdateMaxPower(POWER_MANA);
    }

    auto perfEnd = std::chrono::high_resolution_clock::now();
    auto perfMs = std::chrono::duration_cast<std::chrono::milliseconds>(perfEnd - perfStart).count();

    // 【性能分析】详细记录各阶段耗时
    if (perfMs >= 10)
    {
        auto removeMs = std::chrono::duration_cast<std::chrono::milliseconds>(perfAfterRemove - perfCheckOld).count();
    }

}

// 移除幻境增强效果（参考 mod-item-growth 的正确实现）
void HuanJingSystem::RemoveHuanJingEnhancement(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 playerGuid = player->GetGUID().GetCounter();

    // 防止重复移除：检查是否已经在处理中
    static std::set<std::pair<uint32, uint32>> processingItems;
    std::pair<uint32, uint32> itemKey = {playerGuid, itemGuid};

    if (processingItems.find(itemKey) != processingItems.end())
    {
        // 正在处理中，避免重复调用
        return;
    }

    // 标记为正在处理
    processingItems.insert(itemKey);

    // 优先使用跟踪系统记录的实际应用值进行移除
    std::string appliedStats = GetAppliedHuanJingEnhancement(player, item);

    if (!appliedStats.empty())
    {
        // 解析跟踪的属性数据，格式：属性类型 差值,属性类型 差值
        std::vector<std::string> attributePairs;
        std::stringstream ss(appliedStats);
        std::string pair;

        while (std::getline(ss, pair, ','))
        {
            if (!pair.empty())
                attributePairs.push_back(pair);
        }

        // 处理每个属性对
        for (const std::string& attrPair : attributePairs)
        {
            std::istringstream iss(attrPair);
            uint32 statType;
            AsInt64 statValue;

            if (iss >> statType >> statValue)
            {
                if (statValue > 0)
                {
                    // 移除属性修正
                    ApplyStatModifier(player, statType, statValue, false);
                }
            }
        }

        // 取消跟踪
        UntrackAppliedHuanJingEnhancement(player, item);
    }
    else
    {
        // 如果没有跟踪数据，尝试从数据库重新计算（备用方案）

        // 检查表是否存在
        QueryResult checkResult = CharacterDatabase.Query("SHOW TABLES LIKE '装备属性增强'");
        if (checkResult)
        {
            QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT 增强属性数据 FROM 装备属性增强 WHERE 物品GUID = %s", itemGuid).c_str());

            if (result)
            {
                Field* fields = result->Fetch();
                std::string attributeData = fields[0].Get<std::string>();

                if (!attributeData.empty())
                {

                    // 解析增强属性数据，格式：属性类型 原值 增强值,属性类型 原值 增强值
                    std::vector<std::string> attributePairs;
                    std::stringstream ss(attributeData);
                    std::string pair;

                    while (std::getline(ss, pair, ','))
                    {
                        if (!pair.empty())
                            attributePairs.push_back(pair);
                    }

                    // 处理每个属性对
                    for (const std::string& attrPair : attributePairs)
                    {
                        std::istringstream iss(attrPair);
                        uint32 statType;
                        AsInt64 originalValue, enhancedValue;

                        if (iss >> statType >> originalValue >> enhancedValue)
                        {
                            // 计算增强的差值
                            AsInt64 enhancementDiff = enhancedValue - originalValue;

                            if (enhancementDiff > 0)
                            {
                                // 移除增强的部分
                                ApplyStatModifier(player, statType, enhancementDiff, false);
                            }
                        }
                    }
                }
            }
        }
        else
        {

            // 兼容旧版本：检查内存中的增强数据（在锁内拷贝字符串，锁外解析）
            std::string enhancementData;
            {
                std::lock_guard<std::mutex> lock(_itemMutex);
                auto it = _itemEnhancements.find(itemGuid);
                if (it != _itemEnhancements.end())
                {
                    enhancementData = it->second.attributeData;
                }
            }

            if (!enhancementData.empty())
            {
                // 解析增强属性数据
                std::vector<std::string> attributePairs;
                std::stringstream ss(enhancementData);
                std::string pair;

                while (std::getline(ss, pair, ','))
                {
                    if (!pair.empty())
                        attributePairs.push_back(pair);
                }

                // 处理每个属性对
                for (const std::string& attrPair : attributePairs)
                {
                    std::istringstream iss(attrPair);
                    uint32 statType;
                    AsInt64 originalValue, enhancedValue;

                    if (iss >> statType >> originalValue >> enhancedValue)
                    {
                        // 计算增强的差值
                        AsInt64 enhancementDiff = enhancedValue - originalValue;

                        if (enhancementDiff > 0)
                        {
                            // 移除增强的部分
                            ApplyStatModifier(player, statType, enhancementDiff, false);
                        }
                    }
                }
            }
            // 兼容更旧版本的倍率记录
            else
            {
                float multiplier = GetItemAttributeMultiplier(item);
                if (HasItemAttributeEffect(multiplier, 'x'))
                {

                    // 获取装备的原始属性
                    ItemTemplate const* itemTemplate = item->GetTemplate();
                    if (itemTemplate)
                    {
                        // 遍历装备的所有属性
                        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
                        {
                            if (itemTemplate->ItemStat[i].ItemStatType != 0 && itemTemplate->AsItemStatValue[i] != 0)
                            {
                                uint32 statType = itemTemplate->ItemStat[i].ItemStatType;
                                AsInt64 originalValue = itemTemplate->AsItemStatValue[i];

                                // 计算增强的差值
                                AsInt64 enhancedValue = CalculateEnhancedAttributeValue(originalValue, multiplier, 'x');
                                AsInt64 enhancementDiff = enhancedValue - originalValue;

                                if (enhancementDiff > 0)
                                {
                                    // 移除增强的部分
                                    ApplyStatModifier(player, statType, enhancementDiff, false);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 更新玩家属性（参考 mod-item-growth 的更新方式）
    player->UpdateAllStats();
    player->UpdateAttackPowerAndDamage(false);
    player->UpdateAttackPowerAndDamage(true);
    player->UpdateMaxHealth();
    player->UpdateMaxPower(POWER_MANA);

    // 移除处理标记
    processingItems.erase(itemKey);
}

// 跟踪已应用的幻境增强效果（参考强化系统实现）
void HuanJingSystem::TrackAppliedHuanJingEnhancement(Player* player, Item* item, const std::string& statValues)
{
    if (!player || !item)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 itemGuid = item->GetGUID().GetCounter();

    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        _appliedHuanJingEnhancements[playerGuid][itemGuid] = statValues;
    }

}

// 取消跟踪已应用的幻境增强效果
void HuanJingSystem::UntrackAppliedHuanJingEnhancement(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 itemGuid = item->GetGUID().GetCounter();

    auto playerItr = _appliedHuanJingEnhancements.find(playerGuid);
    if (playerItr != _appliedHuanJingEnhancements.end())
    {
        auto itemItr = playerItr->second.find(itemGuid);
        if (itemItr != playerItr->second.end())
        {

            playerItr->second.erase(itemItr);

            // 如果玩家没有其他幻境增强装备，删除玩家记录
            if (playerItr->second.empty())
            {
                _appliedHuanJingEnhancements.erase(playerItr);
            }
        }
    }
}

// 移除玩家所有的幻境增强效果（用于登录时重置）
void HuanJingSystem::RemoveAllPlayerHuanJingEnhancements(Player* player, bool updateStats)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 查找该玩家的所有已应用的幻境增强效果
    std::unordered_map<uint32, std::string> playerEnhancements;
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto playerItr = _appliedHuanJingEnhancements.find(playerGuid);
        if (playerItr == _appliedHuanJingEnhancements.end())
        {
            // 没有找到该玩家的记录，直接返回
            return;
        }
        // 复制数据以在锁外处理
        playerEnhancements = playerItr->second;
    }

    // 遍历该玩家的所有已应用效果并移除
    for (const auto& itemEntry : playerEnhancements)
    {
        uint32 itemGuid = itemEntry.first;
        std::string appliedStats = itemEntry.second;

        // 解析跟踪的属性数据，格式：属性类型 差值,属性类型 差值
        std::vector<std::string> attributePairs;
        std::stringstream ss(appliedStats);
        std::string pair;

        while (std::getline(ss, pair, ','))
        {
            if (!pair.empty())
                attributePairs.push_back(pair);
        }

        // 移除每个属性效果
        for (const std::string& attrPair : attributePairs)
        {
            std::istringstream iss(attrPair);
            uint32 statType;
            AsInt64 statValue;

            if (iss >> statType >> statValue)
            {
                if (statValue > 0)
                {
                    // 移除属性修正
                    ApplyStatModifier(player, statType, statValue, false);
                }
            }
        }
    }

    // 清除该玩家的所有跟踪记录
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        _appliedHuanJingEnhancements.erase(playerGuid);
    }

    if (updateStats)
    {
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateMaxHealth();
        player->UpdateMaxPower(POWER_MANA);
    }
}

// 获取已应用的幻境增强效果
std::string HuanJingSystem::GetAppliedHuanJingEnhancement(Player* player, Item* item)
{
    if (!player || !item)
        return "";

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 itemGuid = item->GetGUID().GetCounter();

    std::lock_guard<std::mutex> lock(_itemMutex);
    auto playerItr = _appliedHuanJingEnhancements.find(playerGuid);
    if (playerItr != _appliedHuanJingEnhancements.end())
    {
        auto itemItr = playerItr->second.find(itemGuid);
        if (itemItr != playerItr->second.end())
        {
            return itemItr->second;
        }
    }

    return "";
}

// 清除单个物品的已应用幻境增强效果跟踪数据（不移除属性，只清除跟踪记录）
void HuanJingSystem::ClearAppliedHuanJingEnhancement(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint32 itemGuid = item->GetGUID().GetCounter();

    std::lock_guard<std::mutex> lock(_itemMutex);
    auto playerItr = _appliedHuanJingEnhancements.find(playerGuid);
    if (playerItr != _appliedHuanJingEnhancements.end())
    {
        playerItr->second.erase(itemGuid);
        if (playerItr->second.empty())
        {
            _appliedHuanJingEnhancements.erase(playerItr);
        }
    }
}

// 清理玩家的所有已应用幻境增强效果
void HuanJingSystem::ClearPlayerAppliedHuanJingEnhancements(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        _appliedHuanJingEnhancements.erase(playerGuid);
    }

    // 移除护甲和格挡穿透清理逻辑 - 已禁用
    // auto armorIt = _playerArmorReductions.find(playerGuid);
    // if (armorIt != _playerArmorReductions.end())
    // {
    //     // 恢复原始护甲值
    //     player->SetArmor(armorIt->second);
    //     _playerArmorReductions.erase(armorIt);
    //     LOG_DEBUG("module", ">> 清理时恢复玩家 {} 护甲: {}", playerGuid, armorIt->second);
    // }
    //
    // // 清理格挡穿透记录
    // auto blockIt = _playerBlockReductions.find(playerGuid);
    // if (blockIt != _playerBlockReductions.end())
    // {
    //     // 恢复原始格挡率
    //     player->SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, blockIt->second);
    //     _playerBlockReductions.erase(blockIt);
    //     LOG_DEBUG("module", ">> 清理时恢复玩家 {} 格挡率: {:.1f}%", playerGuid, blockIt->second);
    // }
}

// 【内存泄漏修复】清理玩家内存缓存，防止长期运行内存累积
void HuanJingSystem::ClearPlayerMemoryCache(uint32 playerGuid)
{
    // 清理旧等级缓存
    {
        std::lock_guard<std::mutex> lock(_playerMutex);
        _playerData.erase(playerGuid);
    }

    // 清理玩家的已应用幻境增强效果跟踪
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        _appliedHuanJingEnhancements.erase(playerGuid);
    }

    LOG_DEBUG("module", ModuleOldFormat(">> 幻境系统: 清理玩家GUID:%s 的内存缓存", playerGuid).c_str());
}

// 清理指定装备槽位的所有幻境增强效果（参考强化系统实现）
void HuanJingSystem::ClearSlotHuanJingEnhancements(Player* player, uint8 slot)
{
    if (!player)
        return;


    uint32 playerGuid = player->GetGUID().GetCounter();

    std::unordered_map<uint32, std::string> playerEnhancements;
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto playerItr = _appliedHuanJingEnhancements.find(playerGuid);
        if (playerItr == _appliedHuanJingEnhancements.end())
        {
            return;
        }
        playerEnhancements = playerItr->second;
    }

    // 查找并移除该槽位相关的所有幻境增强效果
    std::vector<uint32> itemsToRemove;
    bool hasRemovedEffects = false;

    for (const auto& itemEntry : playerEnhancements)
    {
        uint32 trackedItemGuid = itemEntry.first;
        std::string appliedStats = itemEntry.second;

        // 检查这个跟踪的物品是否在指定槽位或者已经不在装备中
        bool shouldRemove = false;

        // 检查当前槽位的装备
        Item* currentSlotItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (currentSlotItem && currentSlotItem->GetGUID().GetCounter() == trackedItemGuid)
        {
            // 这是当前槽位的装备，需要移除其幻境增强效果
            shouldRemove = true;

        }
        else
        {
            // 检查这个跟踪的物品是否还在任何装备槽中
            bool stillEquipped = false;
            for (uint8 checkSlot = EQUIPMENT_SLOT_START; checkSlot < EQUIPMENT_SLOT_END; ++checkSlot)
            {
                Item* checkItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, checkSlot);
                if (checkItem && checkItem->GetGUID().GetCounter() == trackedItemGuid)
                {
                    stillEquipped = true;
                    break;
                }
            }

            // 如果物品不再装备，也需要移除
            if (!stillEquipped)
            {
                shouldRemove = true;

            }
        }

        if (shouldRemove)
        {

            // 解析并移除属性效果
            std::vector<std::string> attributePairs;
            std::stringstream ss(appliedStats);
            std::string pair;

            while (std::getline(ss, pair, ','))
            {
                if (!pair.empty())
                    attributePairs.push_back(pair);
            }

            // 处理每个属性对
            for (const std::string& attrPair : attributePairs)
            {
                std::istringstream iss(attrPair);
                uint32 statType;
                AsInt64 statValue;

                if (iss >> statType >> statValue)
                {
                    if (statValue > 0)
                    {

                        ApplyStatModifier(player, statType, statValue, false);
                    }
                }
            }

            itemsToRemove.push_back(trackedItemGuid);
            hasRemovedEffects = true;
        }
    }

    // 移除跟踪记录
    if (!itemsToRemove.empty())
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto playerItr = _appliedHuanJingEnhancements.find(playerGuid);
        if (playerItr != _appliedHuanJingEnhancements.end())
        {
            for (uint32 itemGuid : itemsToRemove)
            {
                playerItr->second.erase(itemGuid);
            }

            if (playerItr->second.empty())
            {
                _appliedHuanJingEnhancements.erase(playerItr);
            }
        }
    }

    // 如果移除了效果，更新玩家属性
    if (hasRemovedEffects)
    {
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateMaxHealth();
        player->UpdateMaxPower(POWER_MANA);
    }
}

void HuanJingSystem::RemoveItemAttributeMultiplierTemporary(Item* item)
{
    if (!item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();

    bool hasEnhancement = false;
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto it = _itemEnhancements.find(itemGuid);
        if (it != _itemEnhancements.end())
        {
            _temporarilyDisabledItems.insert(itemGuid);
            hasEnhancement = true;
        }
    }

    if (hasEnhancement)
    {
        // 不删除数据库记录和内存记录，只更新装备状态
        item->SetState(ITEM_CHANGED, item->GetOwner());

        // 如果装备已经穿戴，强制更新玩家属性
        if (item->IsEquipped())
        {
            Player* owner = item->GetOwner();
            if (owner)
            {
                owner->_ApplyItemMods(item, item->GetSlot(), false); // 先移除
                owner->_ApplyItemMods(item, item->GetSlot(), true);  // 再应用
                owner->UpdateAllStats();
            }
        }

        return;
    }

    // 兼容旧版本的倍率记录
    float multiplier = 0.0f;
    bool hasOldRecord = false;
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        auto oldIt = _itemMultipliers.find(itemGuid);
        if (oldIt != _itemMultipliers.end())
        {
            multiplier = oldIt->second;
            hasOldRecord = true;
        }
    }

    if (hasOldRecord)
    {

        // 添加到临时禁用列表
        {
            std::lock_guard<std::mutex> lock(_itemMutex);
            _temporarilyDisabledItems.insert(itemGuid);
        }

        // 不删除倍率记录，只更新装备状态
        item->SetState(ITEM_CHANGED, item->GetOwner());

        // 如果装备已经穿戴，强制更新玩家属性
        if (item->IsEquipped())
        {
            Player* owner = item->GetOwner();
            if (owner)
            {
                owner->_ApplyItemMods(item, item->GetSlot(), false);
                owner->_ApplyItemMods(item, item->GetSlot(), true);
                owner->UpdateAllStats();

            }
        }

    }
}

bool HuanJingSystem::IsItemTemporarilyDisabled(uint32 itemGuid) const
{
    std::lock_guard<std::mutex> lock(_itemMutex);
    return _temporarilyDisabledItems.find(itemGuid) != _temporarilyDisabledItems.end();
}

void HuanJingSystem::ScheduleItemCleanup()
{
    uint32 currentTime = time(nullptr);

    // 检查是否到了清理时间
    if (currentTime - _lastCleanupTime >= _cleanupInterval)
    {
        _lastCleanupTime = currentTime;

        // 直接执行清理（简化版本）
        CleanupInvalidItems();

    }
}

void HuanJingSystem::CleanupInvalidItems()
{

    uint32 startTime = getMSTime();
    uint32 deletedCount = 0;

    try
    {
        // 先检查各个表是否存在
        std::vector<std::string> tables = {
            "character_inventory", "_玩家双甲", "mail_items", "guild_bank_item",
            "item_refund_instance", "item_loot_storage", "character_equipmentsets",
            "auctionhouse", "character_gifts"
        };

        for (const auto& table : tables)
        {
            QueryResult checkTable = CharacterDatabase.Query("SHOW TABLES LIKE '" + table + "'");
            if (!checkTable)
            {
                LOG_WARN("module", ModuleOldFormat(">> 表 %s 不存在，跳过相关检查", table).c_str());
            }
            else
            {

            }
        }

        // 完整的清理逻辑，检查所有可能持久化引用 item_instance 的表。
        // 必须保持与核心一致：character_inventory（含飞升 bag=200 虚拟背包）、双甲、邮件、拍卖、公会银行、
        // 退款表、掉落容器、装备方案、包装礼物。任意一处仍引用都不能删。
        // 已知遗漏导致玩家上架拍卖/邮件中装备被错杀，重启后倍率属性丢失，故此处补齐 LEFT JOIN。
        std::string referenceJoin =
            "LEFT JOIN character_inventory ci ON ii.guid = ci.item "
            "LEFT JOIN _玩家双甲 soul ON ii.guid = soul.物品标识 AND ii.itemEntry = soul.物品编号 AND ii.owner_guid = soul.玩家编号 "
            "LEFT JOIN mail_items mi ON ii.guid = mi.item_guid "
            "LEFT JOIN guild_bank_item gbi ON ii.guid = gbi.item_guid "
            "LEFT JOIN item_refund_instance iri ON ii.guid = iri.item_guid "
            "LEFT JOIN item_loot_storage ils ON ii.guid = ils.containerGUID "
            "LEFT JOIN auctionhouse ah ON ii.guid = ah.itemguid "
            "LEFT JOIN character_gifts cg ON ii.guid = cg.item_guid "
            "LEFT JOIN character_equipmentsets ces ON ii.guid IN ("
            "ces.item0, ces.item1, ces.item2, ces.item3, ces.item4, ces.item5, "
            "ces.item6, ces.item7, ces.item8, ces.item9, ces.item10, ces.item11, "
            "ces.item12, ces.item13, ces.item14, ces.item15, ces.item16, ces.item17, "
            "ces.item18) ";

        std::string referenceWhere =
            "WHERE ci.item IS NULL "
            "AND soul.物品标识 IS NULL "
            "AND mi.item_guid IS NULL "
            "AND gbi.item_guid IS NULL "
            "AND iri.item_guid IS NULL "
            "AND ils.containerGUID IS NULL "
            "AND ah.itemguid IS NULL "
            "AND cg.item_guid IS NULL "
            "AND ces.setguid IS NULL ";

        std::string cleanupQuery = "DELETE ii FROM item_instance ii " + referenceJoin + referenceWhere;

        // 先统计要清理的记录数量
        QueryResult countQuery1 = CharacterDatabase.Query(
            "SELECT COUNT(*) FROM item_instance ii " + referenceJoin + referenceWhere
        );

        uint32 deletedCount1 = 0;
        if (countQuery1)
        {
            Field* fields = countQuery1->Fetch();
            deletedCount1 = fields[0].Get<uint32>();
        }

        if (deletedCount1 > 0)
            LOG_INFO("module.huanjing", ModuleOldFormat(">> [item_instance 清理] 检测到 %s 条无任何持久化引用的物品记录，将被删除", deletedCount1).c_str());

        CharacterDatabase.Execute(cleanupQuery);

        // 统计要清理的已删除角色装备数量
        QueryResult countQuery2 = CharacterDatabase.Query(
            "SELECT COUNT(*) FROM item_instance ii "
            "LEFT JOIN characters c ON ii.owner_guid = c.guid "
            "WHERE c.guid IS NULL AND ii.owner_guid > 0"
        );

        uint32 deletedCount2 = 0;
        if (countQuery2)
        {
            Field* fields = countQuery2->Fetch();
            deletedCount2 = fields[0].Get<uint32>();
        }

        // 清理已删除角色的装备
        std::string cleanupOwnerQuery =
            "DELETE ii FROM item_instance ii "
            "LEFT JOIN characters c ON ii.owner_guid = c.guid "
            "WHERE c.guid IS NULL AND ii.owner_guid > 0";

        CharacterDatabase.Execute(cleanupOwnerQuery);

        uint32 totalDeleted = deletedCount1 + deletedCount2;

        uint32 endTime = getMSTime();
        uint32 duration = endTime - startTime;

    }
    catch (const std::exception& e)
    {
        LOG_ERROR("module", ModuleOldFormat(">> 装备数据清理失败: %s", e.what()).c_str());
    }
}





void HuanJingSystemCreatureScript::OnCreatureAddWorld(Creature* creature)
{
    if (!creature)
        return;

    // 先做一次基础血量安全检查，防止数据库配置的生命值超过更新字段可承载范围导致生物状态异常
    sHuanJingSystem->EnsureCreatureHealthSafe(creature);

    // 地图路由与控制组在每只生物首次进入世界时解析一次；后续战斗只复用结果。
    sHuanJingSystem->GetMapCreatureConfig(creature, nullptr);

    // 生物进入世界时就按附近玩家应用现有属性调整，避免基础生命值仍保持
    // 官方模板值，直到玩家选中目标或首次攻击时才被重新计算。
    sHuanJingSystem->AdjustCreatureStatsWithHighestLevel(creature, CREATURE_ADJUSTMENT_RECHECK_RANGE);

    // 路由解析会在这里写入 StTempId 和传统多血条数。创建包可能尚未到达
    // 客户端，延迟两次强制字段/插件同步，避免只在重新选中目标后才刷新。
    sHuanJingSystem->EnsureCreatureHealthBarsFromConfig(creature, nullptr, false);
    auto refreshCreatureHealth = [](Creature* target)
    {
        if (!target->IsInWorld() || !target->IsAlive())
            return;

        target->ForceValuesUpdateAtIndex(UNIT_FIELD_HEALTH);
        target->ForceValuesUpdateAtIndex(UNIT_FIELD_MAXHEALTH);
        sHuanJingSystem->SendHealthBarUpdate(target);
        sAsCreature->SendUpdateMultipleHp(nullptr, target);
    };
    refreshCreatureHealth(creature);
    creature->m_Events.AddEventAtOffset([creature, refreshCreatureHealth]() { refreshCreatureHealth(creature); }, Milliseconds(250));
    creature->m_Events.AddEventAtOffset([creature, refreshCreatureHealth]() { refreshCreatureHealth(creature); }, Milliseconds(1000));
}

void HuanJingSystemCreatureScript::OnAllCreatureUpdate(Creature* creature, uint32 diff)
{
    (void)diff;

    if (!creature)
        return;

    // 血条系统现在使用立即切换，不需要队列处理

    // Respawn() changes JUST_RESPAWNED to ALIVE before this end-of-update hook runs.
    // The saved adjustment record is therefore the reliable respawn signal.
    uint64 creatureKey = GenerateCreatureKey(creature);
    bool hasPendingRespawnAdjustment = false;
    {
        std::lock_guard<std::mutex> lock(sHuanJingSystem->_creatureMutex);
        hasPendingRespawnAdjustment = sHuanJingSystem->_creatureRespawnAdjustments.find(creatureKey) != sHuanJingSystem->_creatureRespawnAdjustments.end();
    }

    if (creature->IsAlive() && hasPendingRespawnAdjustment)
        sHuanJingSystem->RestoreCreatureAdjustmentAfterRespawn(creature);

    static std::unordered_map<uint64, uint32> s_lastAdjustmentRecheckTimes;
    uint32 adjustedLevel = GetCreatureAdjustedHuanJingLevel(creature);
    if (adjustedLevel == 0)
    {
        s_lastAdjustmentRecheckTimes.erase(creatureKey);
        return;
    }

    if (!creature->IsInCombat())
        ClearCombatProtectionLogsForCreature(creature);

    if (!creature->IsAlive() || creature->IsPet() || creature->IsInCombat())
        return;

    uint32 now = getMSTime();
    auto lastCheck = s_lastAdjustmentRecheckTimes.find(creatureKey);
    if (lastCheck != s_lastAdjustmentRecheckTimes.end() && now - lastCheck->second < CREATURE_ADJUSTMENT_RECHECK_INTERVAL_MS)
        return;

    s_lastAdjustmentRecheckTimes[creatureKey] = now;

    uint32 nearbyHighestLevel = sHuanJingSystem->GetHighestHuanJingLevelNearCreature(creature, CREATURE_ADJUSTMENT_RECHECK_RANGE);
    if (nearbyHighestLevel == 0)
    {
        sHuanJingSystem->RestoreCreatureToOriginalStats(creature);
        s_lastAdjustmentRecheckTimes.erase(creatureKey);
        return;
    }

    if (nearbyHighestLevel < adjustedLevel)
    {
        sHuanJingSystem->AdjustCreatureStatsWithHighestLevel(creature, CREATURE_ADJUSTMENT_RECHECK_RANGE);
    }
}

// 新的 UnitScript 实现
static void ApplyHuanJingPlayerDamageReduction(Unit* attacker, Unit* victim, AsInt64& damage)
{
    if (!attacker || !victim || damage == 0 || !UnitHasCreatureType(victim))
        return;

    Creature* creature = victim->ToCreature();
    Player* player = GetCombatOwnerPlayer(attacker);
    if (!creature || !player || !creature->IsAlive())
        return;

    if (IsHuanJingExcludedCreature(creature))
        return;

    if (!CheckCombatPowerRequirement(player, creature, true))
    {
        damage = 0;
        return;
    }

    if (sHuanJingSystem->HasCreatureAttributeOverride(creature))
        return;

    uint32 huanJingLevel = GetCreatureAdjustedHuanJingLevel(creature);
    if (huanJingLevel == 0)
        huanJingLevel = sHuanJingSystem->GetPlayerHuanJingLevel(player);

    if (huanJingLevel == 0)
        return;

    uint32 creatureId = creature->GetEntry();
    uint32 mapId = creature->GetMapId();
    uint32 areaId = creature->GetAreaId();
    uint32 difficulty = sHuanJingSystem->GetCreatureMapDifficulty(creature);
    uint32 creatureLevel = creature->GetLevel();
    uint32 creatureType = sHuanJingSystem->GetCreatureTypeByEntry(creatureId);

    HuanJingCreatureConfig const* config = sHuanJingSystem->GetCreatureConfig(creature, player);
    if (!config)
        return;

    if (config->damageTakenValue > 0)
    {
        damage = config->damageTakenValue;
        return;
    }

    if (config->damageReductionPct == 0)
        return;

    AsInt64 reductionPct = config->damageReductionPct;
    long double damageMultiplier = CalculateHuanJingDamageMultiplier(reductionPct);
    AsInt64 oldDamage = damage;

    if (damageMultiplier <= 0.0L)
        damage = 0;
    else if (damageMultiplier != 1.0L)
    {
        AsInt64 scaledDamage = ScaleAsInt64ByDouble(damage, static_cast<double>(damageMultiplier));
        damage = scaledDamage == 0 ? AsInt64(1) : scaledDamage;
    }

    if (oldDamage != damage)
    {
        LOG_DEBUG("module.huanjing", ModuleOldFormat("[幻境减伤] 生物=%s 幻境=%s 减伤=%s 倍率=%s 原伤害=%s 调整后=%s", creatureId, huanJingLevel, reductionPct, static_cast<double>(damageMultiplier), Acore::ToString(oldDamage), Acore::ToString(damage)).c_str());
    }
}

void HuanJingSystemUnitScript::OnBeforeRollMeleeOutcomeAgainst(Unit const* attacker, Unit const* victim, WeaponAttackType /*attType*/,
    int32& /*attackerMaxSkillValueForLevel*/, int32& /*victimMaxSkillValueForLevel*/, int32& /*attackerWeaponSkill*/,
    int32& /*victimDefenseSkill*/, int32& /*critChance*/, int32& missChance, int32& dodgeChance,
    int32& parryChance, int32& blockChance)
{
    if (!attacker || !victim || !UnitHasCreatureType(attacker) || !victim->IsPlayer())
        return;

    Creature* creature = const_cast<Unit*>(attacker)->ToCreature();
    Player* player = const_cast<Unit*>(victim)->ToPlayer();
    if (!creature || !player || creature->IsControlledByPlayer())
        return;

    AsInt64 trueDamageValue = sHuanJingSystem->GetCreatureTrueDamageValue(creature, player);
    if (trueDamageValue > 0)
    {
        missChance = 0;
        dodgeChance = 0;
        parryChance = 0;
        blockChance = 0;
        return;
    }

    uint32 flatPenetration = sAsCreature->GetBlockPenetration(creature);
    float percentPenetration = std::clamp(sAsCreature->GetBlockPenetrationPct(creature), 0.0f, 100.0f);
    if (flatPenetration == 0 && percentPenetration == 0.0f)
        return;

    // The core expresses melee chances in 0.01% units. A fixed value of 1
    // therefore removes one percentage point of block chance; the percentage
    // value then reduces the remaining chance.
    AsInt64 adjustedBlockChance = std::max<AsInt64>(0, AsInt64(blockChance) - AsInt64(flatPenetration) * 100);
    adjustedBlockChance = AsInt64(static_cast<long double>(adjustedBlockChance) * (1.0L - percentPenetration / 100.0L));
    blockChance = int32(std::max<AsInt64>(0, adjustedBlockChance));
}

void HuanJingSystemUnitScript::ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage)
{
    AsInt64 realDamage = attacker ? attacker->PeekAsDamageOverride(target, damage) : AsInt64(damage);
    AsInt64 wideDamage = ToUInt64FromAsInt64(realDamage);
    ApplyHuanJingPlayerDamageReduction(attacker, target, wideDamage);
    StoreDamageForOldCore(attacker, target, wideDamage, damage);
}

void HuanJingSystemCreatureScript::OnCreatureRemoveWorld(Creature* creature)
{
    if (!creature)
        return;

    ObjectGuid guid = creature->GetGUID();
    sHuanJingSystem->CleanupCreatureAdjustmentData(creature);
    sHuanJingSystem->RemoveCreatureHealthBars(creature);
    sHuanJingSystem->ClearCreatureVirtualMana(guid);
    sHuanJingSystem->ClearCreatureSpellPower(creature);
    sHuanJingSystem->ForgetResolvedCreatureConfig(creature);
}

void HuanJingSystemUnitScript::ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo)
{
    if (!target || !attacker || damage <= 0)
        return;

    AsInt64 realDamage = attacker->PeekAsDamageOverride(target, uint32(damage));
    AsInt64 wideDamage = ToUInt64FromAsInt64(realDamage);

    if (UnitHasCreatureType(target))
    {
        ApplyHuanJingPlayerDamageReduction(attacker, target, wideDamage);
        StoreDamageForOldCore(attacker, target, wideDamage, damage);
        return;
    }

    // 只处理怪物对玩家造成的法术伤害
    if (!spellInfo || !UnitHasCreatureType(attacker) || !target->IsPlayer())
        return;

    Creature* creature = attacker->ToCreature();
    if (!creature)
        return;

    if (sHuanJingSystem->HasCreatureAttributeOverride(creature))
        return;

    // 只放大真正的魔法类法术（排除物理系技能）
    if (spellInfo->DmgClass != SPELL_DAMAGE_CLASS_MAGIC)
        return;

    AsInt64 spellPower = GetHuanJingCreatureSpellPower(creature->GetGUID());
    if (spellPower == 0)
        return;

    float baseSpellPower = float(creature->GetLevel() * 10);
    if (baseSpellPower <= 0.0f)
        return;

    long double ratio = Acore::Number::ToLongDouble(spellPower) / static_cast<long double>(baseSpellPower);
    if (ratio <= 0.0f)
        return;

    // 避免配置异常导致伤害爆炸，这里做一个较高的安全上限
    if (ratio > 1000.0L)
        ratio = 1000.0L;

    AsInt64 oldDamage = wideDamage;
    long double newDamage = Acore::Number::ToLongDouble(wideDamage) * ratio;

    wideDamage = ToUInt64Damage(newDamage);
    StoreDamageForOldCore(attacker, target, wideDamage, damage);

    LOG_DEBUG("module.huanjing", ModuleOldFormat("[幻境系统] 法术伤害调整: creature=%s spell=%s target=%s sp=%s baseSp=%s ratio=%s dmg=%s=>%s", creature->GetEntry(), spellInfo->Id, target->GetName(), FormatUInt64(spellPower), baseSpellPower, static_cast<double>(ratio), Acore::ToString(oldDamage), Acore::ToString(wideDamage)).c_str());
}

void HuanJingSystemUnitScript::ModifyPeriodicDamageAurasTick(Unit* /*target*/, Unit* /*attacker*/, uint32& /*damage*/, SpellInfo const* /*spellInfo*/)
{
}

HuanJingExternalHealthBarDamageResult HuanJingSystem::ApplyExternalCreatureHealthBarDamage(Unit* attacker, Creature* creature, AsInt64 const& damage)
{
    HuanJingExternalHealthBarDamageResult result;
    result.beforeHealth = GetCreatureCombatHealth(creature);

    if (!attacker || !creature || !creature->IsAlive() || damage == 0)
    {
        result.afterHealth = GetCreatureCombatHealth(creature);
        return result;
    }

    if (IsHuanJingExcludedCreature(creature))
    {
        ObjectGuid guid = creature->GetGUID();
        CleanupCreatureAdjustmentData(creature);
        RemoveCreatureHealthBars(creature);
        ClearCreatureVirtualMana(guid);
        result.afterHealth = GetCreatureCombatHealth(creature);
        return result;
    }

    ObjectGuid guid = creature->GetGUID();
    uint64 healthBarKey = GenerateCreatureKey(creature);
    bool const noInitialAdjust = IsExternalPlayerDamageNoInitialAdjustActive();
    bool const alreadyHasHealthBar = IsHealthBarEnabled(creature);
    uint32 const alreadyAdjustedLevel = GetCreatureAdjustedHuanJingLevel(creature);
    bool hasCreatureAttributeOverride = HasCreatureAttributeOverride(creature);
    Player* playerAttacker = GetCombatOwnerPlayer(attacker);
    bool attackerIsActivePlayer = playerAttacker && playerAttacker->IsInWorld() && playerAttacker->IsAlive()
        && (!playerAttacker->GetSession() || !playerAttacker->GetSession()->PlayerLogout());

    if (!attackerIsActivePlayer && IsHealthBarEnabled(creature))
    {
        result.handled = true;

        if (!HasAliveEligiblePlayerNearCreature(creature, playerAttacker, CREATURE_ADJUSTMENT_RECHECK_RANGE))
        {
            ResetCreatureHealthBarsToFull(creature, true);

            if (CreatureAI* ai = creature->AI())
                ai->EnterEvadeMode(CreatureAI::EVADE_REASON_OTHER);
            else
            {
                creature->GetThreatMgr().ResetAllThreat();
                creature->CombatStop(true);
            }
        }

        result.afterHealth = GetCreatureCombatHealth(creature);
        return result;
    }

    if (hasCreatureAttributeOverride)
    {
        CleanupCreatureAdjustmentData(creature);
        ClearCreatureVirtualMana(guid);
    }
    else if (attackerIsActivePlayer && (!noInitialAdjust || alreadyHasHealthBar || alreadyAdjustedLevel > 0))
    {
        // 命中结算不是首次进入战斗。初始调整仅由 OnPlayerEnterCombat 处理，
        // 防止动态难度在每次伤害时重新初始化生命值。
        AdjustCreatureStatsForPlayer(creature, playerAttacker);
    }

    SyncHealthBarDataWithCreatureMaxHealth(creature);

    uint32 totalBars = 0;
    AsInt64 healthPerBar = 0;
    bool isEnabled = false;
    uint32 barsRemaining = 0;
    AsInt64 currentBarHealth = 0;
    AsInt64 visibleHealthBefore = 0;

    {
        std::lock_guard<std::mutex> lock(_healthBarMutex);
        auto it = _healthBarData.find(healthBarKey);
        if (it == _healthBarData.end() || !it->second.enabled)
        {
            if (playerAttacker && creature->GetEntry() == 299 && GetCreatureHealthBarCount(creature, playerAttacker) > 0)
            {
                LOG_DEBUG("module.huanjing", ModuleOldFormat("[幻境血条监控-OnDamage] Entry=%s GUID=%s Dmg=%s HP=%s/%s - HealthBar DISABLED or NOT FOUND", creature->GetEntry(), guid.ToString(), Acore::ToString(damage), FormatUInt64(GetCreatureCombatHealth(creature)), FormatUInt64(GetCreatureCombatMaxHealth(creature))).c_str());
            }

            result.afterHealth = GetCreatureCombatHealth(creature);
            return result;
        }

        NormalizeHealthBarData(it->second);
        const HealthBarData& data = it->second;
        totalBars = data.totalBars;
        healthPerBar = data.healthPerBar;
        isEnabled = data.enabled;
        barsRemaining = data.currentBar;
        currentBarHealth = data.currentHealth;
        visibleHealthBefore = data.visibleHealth;
    }

    if (!isEnabled || totalBars <= 1 || healthPerBar == 0 || barsRemaining == 0 || currentBarHealth == 0)
    {
        result.afterHealth = GetCreatureCombatHealth(creature);
        return result;
    }

    if (visibleHealthBefore == 0)
        visibleHealthBefore = currentBarHealth;

    if (healthPerBar > GetCreatureCombatMaxHealth(creature))
    {
        SetCreatureCombatCreateHealth(creature, healthPerBar);
        SetCreatureCombatMaxHealth(creature, healthPerBar);
    }

    // The native health field is only a compatibility projection of the
    // external multi-bar state. It can be rounded or delivered one update late;
    // never use it to reconstruct currentBar/currentHealth, or the avatar value
    // can oscillate between two totals. The cached health-bar state is authoritative.
    SetCreatureCombatHealth(creature, visibleHealthBefore);
    result.beforeHealth = visibleHealthBefore;

    if (!isEnabled || totalBars <= 1 || healthPerBar == 0 || barsRemaining == 0 || currentBarHealth == 0)
    {
        result.afterHealth = GetCreatureCombatHealth(creature);
        return result;
    }

    uint32 currentTime = getMSTime();
    AsInt64 originalDamage = damage;
    AsInt64 remainingDamage = originalDamage;
    AsInt64 absorbedDamage = 0;
    uint32 newBarsRemaining = barsRemaining;
    AsInt64 newBarRemainingHealth = currentBarHealth;

    while (remainingDamage > 0 && newBarsRemaining > 0)
    {
        if (newBarRemainingHealth == 0)
        {
            --newBarsRemaining;
            newBarRemainingHealth = newBarsRemaining > 0 ? healthPerBar : 0;
            continue;
        }

        if (remainingDamage < newBarRemainingHealth)
        {
            newBarRemainingHealth -= remainingDamage;
            absorbedDamage += remainingDamage;
            remainingDamage = 0;
        }
        else
        {
            remainingDamage -= newBarRemainingHealth;
            absorbedDamage += newBarRemainingHealth;
            --newBarsRemaining;
            newBarRemainingHealth = newBarsRemaining > 0 ? healthPerBar : 0;
        }
    }

    result.handled = true;
    result.absorbedDamage = absorbedDamage;
    result.appliedDamage = absorbedDamage;

    if (newBarsRemaining == 0)
    {
        {
            std::lock_guard<std::mutex> lock(_healthBarMutex);
            auto it = _healthBarData.find(healthBarKey);
            if (it != _healthBarData.end())
            {
                it->second.currentBar = 0;
                it->second.currentHealth = 0;
                it->second.visibleHealth = 0;
                NormalizeHealthBarData(it->second);
                it->second.lastSwitchTime = currentTime;
                it->second.lastBroadcastTime = 0;
            }
        }

        // A zero remainder used to be forwarded as one damage while the
        // projected native health could still be large. Explicitly leave one
        // real health, then pass the original hit through so Unit::DealDamage
        // reaches the normal death/loot pipeline without inventing a larger
        // final floating-damage value.
        AsInt64 const coreFatalDamage = originalDamage > 0 ? originalDamage : AsInt64(1);
        SetCreatureCombatHealth(creature, AsInt64(1));
        result.fatal = true;
        result.coreDamage = coreFatalDamage;
        result.afterHealth = GetCreatureCombatHealth(creature);

        SendHealthBarUpdate(creature);
        return result;
    }

    bool updated = false;
    AsInt64 newVisibleHealth = CalculateHealthBarVisibleHealth(totalBars, healthPerBar, newBarsRemaining, newBarRemainingHealth);
    if (visibleHealthBefore != 0 && newVisibleHealth > visibleHealthBefore)
        newVisibleHealth = visibleHealthBefore;

    if (newBarsRemaining > 0 && newVisibleHealth == 0)
        newVisibleHealth = 1;

    {
        std::lock_guard<std::mutex> lock(_healthBarMutex);
        auto it = _healthBarData.find(healthBarKey);
        if (it != _healthBarData.end())
        {
            it->second.currentBar = newBarsRemaining;
            it->second.currentHealth = newBarRemainingHealth;
            it->second.visibleHealth = newVisibleHealth;
            NormalizeHealthBarData(it->second);
            it->second.lastSwitchTime = currentTime;
            updated = true;
        }
    }

    LOG_DEBUG("module", ModuleOldFormat("[血条吸收] 生物 %s 受到伤害 %s，多倍血条吸收 %s，剩余伤害 %s，血条（%s -> %s），新血条剩余血量: %s", creature->GetEntry(), FormatUInt64(originalDamage), FormatUInt64(absorbedDamage), FormatUInt64(remainingDamage), barsRemaining, newBarsRemaining, FormatUInt64(newBarRemainingHealth)).c_str());

    result.coreDamage = remainingDamage;

    SetCreatureCombatHealth(creature, newVisibleHealth);
    result.afterHealth = GetCreatureCombatHealth(creature);

    if (updated)
        BroadcastHealthBarUpdateThrottled(creature, currentTime, 300);

    return result;
}

void HuanJingSystemUnitScript::OnDamage(Unit* attacker, Unit* victim, uint32& damage)
{
    // 性能优化：快速路径检查，避免不必要的处理
    if (!attacker || !victim)
        return;

    AsInt64 realDamage = attacker->PeekAsDamageOverride(victim, damage);
    AsInt64 wideDamage = ToUInt64FromAsInt64(realDamage);

    if (UnitHasCreatureType(attacker) && victim->IsPlayer())
    {
        Creature* creature = attacker->ToCreature();
        Player* player = victim->ToPlayer();
        if (!creature || !player)
            return;

        if (IsHuanJingExcludedCreature(creature))
        {
            sHuanJingSystem->CleanupCreatureAdjustmentData(creature);
            sHuanJingSystem->RemoveCreatureHealthBars(creature);
            sHuanJingSystem->ClearCreatureVirtualMana(creature->GetGUID());
            return;
        }

        if (sHuanJingSystem->HasCreatureAttributeOverride(creature))
        {
            sHuanJingSystem->CleanupCreatureAdjustmentData(creature);
            sHuanJingSystem->ClearCreatureVirtualMana(creature->GetGUID());
            return;
        }

        uint32 adjustedLevel = GetCreatureAdjustedHuanJingLevel(creature);
        if (adjustedLevel > 0)
        {
            // 【全服卡顿源修复】原实现：已调整生物每次攻击玩家都做一次 200 码
            // Cell::VisitWorldObjects 网格全扫描（检查附近是否还有幻境玩家）。
            // ①快速路径：被攻击的玩家自己就是合格幻境玩家时附近必然非空，O(1) 免扫描
            //（战斗中绝大多数命中走此分支）；②慢路径补 3 秒/生物节流（与
            // OnAllCreatureUpdate 的复检节奏一致，战斗中由本路径承担复检）。
            bool victimIsHuanJing = IsHuanJingCreatureEligibleForPlayer(creature, player)
                && sHuanJingSystem->GetPlayerHuanJingLevel(player) > 0;

            if (!victimIsHuanJing)
            {
                static std::unordered_map<uint64, uint32> s_lastDamageRecheckTimes;
                uint64 creatureKey = GenerateCreatureKey(creature);
                uint32 now = getMSTime();

                auto itr = s_lastDamageRecheckTimes.find(creatureKey);
                if (itr == s_lastDamageRecheckTimes.end() || now - itr->second >= CREATURE_ADJUSTMENT_RECHECK_INTERVAL_MS)
                {
                    s_lastDamageRecheckTimes[creatureKey] = now;
                    if (sHuanJingSystem->GetHighestHuanJingLevelNearCreature(creature, CREATURE_ADJUSTMENT_RECHECK_RANGE) == 0)
                    {
                        sHuanJingSystem->RestoreCreatureToOriginalStats(creature);
                        s_lastDamageRecheckTimes.erase(creatureKey);
                        return;
                    }
                }
            }
        }

        AsInt64 trueDamageValue = sHuanJingSystem->GetCreatureTrueDamageValue(creature, player);
        if (trueDamageValue > 0)
        {
            AsInt64 const maxValue = std::numeric_limits<AsInt64>::max();
            wideDamage = (trueDamageValue > maxValue - wideDamage)
                ? maxValue
                : wideDamage + trueDamageValue;
            StoreDamageForOldCore(attacker, victim, wideDamage, damage);
        }

        return;
    }

    if (!UnitHasCreatureType(victim) || !victim->IsAlive() || wideDamage == 0)
        return;

    Creature* creature = victim->ToCreature();
    if (!creature)
        return;

    if (Player* player = GetCombatOwnerPlayer(attacker))
    {
        if (!CheckCombatPowerRequirement(player, creature, true))
        {
            wideDamage = 0;
            StoreDamageForOldCore(attacker, victim, wideDamage, damage);
            return;
        }
    }

    HuanJingExternalHealthBarDamageResult healthBarResult = sHuanJingSystem->ApplyExternalCreatureHealthBarDamage(attacker, creature, wideDamage);
    if (healthBarResult.handled)
    {
        wideDamage = healthBarResult.coreDamage;
        StoreDamageForOldCore(attacker, victim, wideDamage, damage);
    }
}

void HuanJingSystemUnitScript::OnDamageMeleeNow(Unit* /*attacker*/, Unit* /*victim*/, uint32& /*damage*/)
{
}

void HuanJingSystemUnitScript::OnDamageSpellEnd(Unit* /*attacker*/, Unit* /*victim*/, uint32 /*castId*/, uint32& /*damage*/)
{
}

void HuanJingSystemPlayerScript::OnPlayerEnterCombat(Player* player, Unit* enemy)
{
    if (!player || !UnitHasCreatureType(enemy))
        return;

    Creature* creature = enemy->ToCreature();
    if (!creature)
        return;

    if (!CheckCombatPowerRequirement(player, creature, true))
    {
        player->AttackStop();
        player->CombatStop(false);
        return;
    }

    if (!sHuanJingSystem->HasCreatureAttributeOverride(creature))
    {
        // 玩家进入战斗时，只允许本次初始战斗目标按自己的幻境等级调整生物。
        // 已经处于战斗中的生物不会被后来选中或参战的高等级玩家改写等级。
        sHuanJingSystem->AdjustCreatureStatsForPlayer(creature, player, true, player);

        LOG_DEBUG("module", ModuleOldFormat(">> [战斗触发] 玩家 %s 与生物 %s 进入战斗，按玩家幻境等级进行单体调整", player->GetName(), creature->GetEntry()).c_str());
    }

    // 进入战斗瞬间再做一次安全血量检查，避免在战斗开始前就已经超出安全上限
    sHuanJingSystem->EnsureCreatureHealthSafe(creature);

    // 检查是否有血条系统（在锁内访问共享容器，然后拷贝必要数据到栈上）
    uint64 healthBarKey = GenerateCreatureKey(creature);
    uint32 totalBars = 0;
    bool enabled = false;
    {
        std::lock_guard<std::mutex> lock(sHuanJingSystem->_healthBarMutex);
        auto it = sHuanJingSystem->_healthBarData.find(healthBarKey);
        if (it != sHuanJingSystem->_healthBarData.end() && it->second.enabled)
        {
            enabled = true;
            totalBars = it->second.totalBars;
        }
    }

    if (enabled && totalBars > 1)
    {
        LOG_DEBUG("huanjing.healthbar", ModuleOldFormat("[血条系统] 玩家 %s 进入战斗，生物 %s 有 %s 条血条", player->GetName(), creature->GetEntry(), totalBars).c_str());

        // 血条消息没有携带目标身份，只发送给当前选中这只生物的玩家。
        SendCreatureHuanJingBroadcastToPlayer(player, creature, "EnterCombat");
    }
    else
    {
        SendCreatureHuanJingBroadcastToPlayer(player, creature, "EnterCombat");
    }
}

void HuanJingSystemUnitScript::OnCreKillToCreOrDie(Unit* killer, Unit* killed)
{
    OnUnitDeath(killed, killer);
}

void HuanJingSystemPlayerScript::OnPlayerCreatureKill(Player* killer, Creature* killed)
{
}

void HuanJingSystemUnitScript::OnUnitDeath(Unit* unit, Unit* killer)
{
    if (!unit || !UnitHasCreatureType(unit) || !killer)
    {
        return;
    }

    Creature* creature = unit->ToCreature();
    ObjectGuid guid = creature->GetGUID();

    // Loot belongs to the resolved map template even when another system owns stats.
    HuanJingCreatureConfig const* lootConfig = sHuanJingSystem->GetMapCreatureConfig(creature, nullptr);
    if (lootConfig && !lootConfig->attributeTemplateId)
        lootConfig = nullptr;
    if (lootConfig)
    {
        std::lock_guard<std::mutex> lock(sHuanJingSystem->_creatureMutex);
        sHuanJingSystem->_creatureDeathIdentificationGroup[guid] = {
            lootConfig->identificationGroup,
            ResolveConfiguredItemAttributeMode(lootConfig->equipAttrValue, lootConfig->equipAttrMode),
            lootConfig->identificationQuality, lootConfig->identificationItemLevel,
            lootConfig->equipAttrValue, lootConfig->equipAttrMultiplier, true };
    }

    if (sHuanJingSystem->HasCreatureAttributeOverride(creature))
    {
        sHuanJingSystem->CleanupCreatureAdjustmentData(creature, lootConfig != nullptr);
        sHuanJingSystem->RemoveCreatureHealthBars(creature);
        sHuanJingSystem->ClearCreatureVirtualMana(guid);
        return;
    }

    // 在清理血条数据之前，先把最终血条状态打到日志里，便于排查死亡原因
    HealthBarData dataCopy;
    bool hasData = false;
    {
        std::lock_guard<std::mutex> lock(sHuanJingSystem->_healthBarMutex);
        auto it = sHuanJingSystem->_healthBarData.find(GenerateCreatureKey(creature));
        if (it != sHuanJingSystem->_healthBarData.end())
        {
            dataCopy = it->second;
            hasData = true;
        }
    }

    // 只对真正启用了幻境血条系统的生物清理血条数据，避免残留
    bool isHuanJingHealthBarCreature = hasData && dataCopy.totalBars > 1;
    if (isHuanJingHealthBarCreature)
    {
        sHuanJingSystem->RemoveCreatureHealthBars(creature);
    }
    else
    {
        // 非幻境血条生物：静默清理（如果有残留数据）
        sHuanJingSystem->RemoveCreatureHealthBars(creature);
    }
    sHuanJingSystem->ClearCreatureVirtualMana(guid);

    Player* player = GetCombatOwnerPlayer(killer);
    if (!player)
        return;

    // 修复：只有被幻境系统调整过的生物才能掉落倍率装备
    // 检查生物是否在 _creatureAdjustedLevels 中
    uint64 creatureKey = GenerateCreatureKey(creature);
    uint32 creatureAdjustedLevel = 0;

    {
        std::lock_guard<std::mutex> lock(sHuanJingSystem->_creatureMutex);
        auto it = sHuanJingSystem->_creatureAdjustedLevels.find(creatureKey);
        if (it != sHuanJingSystem->_creatureAdjustedLevels.end())
        {
            // 生物被调整过，使用调整时的幻境等级
            creatureAdjustedLevel = it->second;
            LOG_DEBUG("module.huanjing", ModuleOldFormat("生物 %s 被幻境系统调整过，等级=%s", creature->GetEntry(), creatureAdjustedLevel).c_str());
        }
        else
        {
            // 生物未被调整，不掉落倍率装备
            LOG_DEBUG("module.huanjing", ModuleOldFormat("生物 %s 未被幻境系统调整，不掉落倍率装备", creature->GetEntry()).c_str());
        }
    }

    // 记录使用的幻境等级（用于掉落物品）
    sHuanJingSystem->RecordCreatureDeathHuanJingLevel(guid, creatureAdjustedLevel);

    // 【关键修复】获取并保存鉴定组ID时，使用击杀玩家当前的幻境等级
    // 而不是生物被调整时的等级，这样玩家切换等级后立即生效
    if (creatureAdjustedLevel > 0 && !lootConfig)
    {
        // 【修复】使用击杀玩家当前的幻境等级来获取鉴定组配置
        uint32 playerCurrentLevel = sHuanJingSystem->GetPlayerHuanJingLevel(player);

        // 如果玩家当前没有幻境等级（可能切换到0了），使用生物调整时的等级
        uint32 levelForIdentification = (playerCurrentLevel > 0) ? playerCurrentLevel : creatureAdjustedLevel;

        uint32 creatureId = creature->GetEntry();
        uint32 mapId = creature->GetMapId();
        uint32 areaId = creature->GetAreaId();
        uint32 difficulty = sHuanJingSystem->GetCreatureMapDifficulty(creature);
        uint32 creatureLevel = creature->GetLevel();
        uint32 creatureType = sHuanJingSystem->GetCreatureTypeByEntry(creatureId);

        HuanJingCreatureConfig const* config = sHuanJingSystem->GetCreatureConfig(creature, player);

        if (config && config->identificationGroup > 0)
        {
            // 保存鉴定组ID到缓存，这样即使尸体消失也能正常鉴定
            std::lock_guard<std::mutex> lock(sHuanJingSystem->_creatureMutex);
            sHuanJingSystem->_creatureDeathIdentificationGroup[guid] = { config->identificationGroup, ResolveConfiguredItemAttributeMode(config->equipAttrValue, config->equipAttrMode), config->identificationQuality, config->identificationItemLevel, config->equipAttrValue, config->equipAttrMultiplier };

        }
    }

    // 保存生物死亡前的调整等级，用于重生时恢复
    sHuanJingSystem->SaveCreatureAdjustmentForRespawn(creature);

    // 清理生物的幻境调整标记（保留刚写入的死亡鉴定组记录供拾取使用）
    sHuanJingSystem->CleanupCreatureAdjustmentData(creature, true);
}

void HuanJingSystemUnitScript::OnUnitEnterEvadeMode(Unit* unit, uint8 /*evadeReason*/)
{
    if (!UnitHasCreatureType(unit))
        return;

    Creature* creature = unit->ToCreature();
    if (!creature || !creature->IsAlive())
        return;

    if (IsHuanJingExcludedCreature(creature))
    {
        ObjectGuid guid = creature->GetGUID();
        sHuanJingSystem->CleanupCreatureAdjustmentData(creature);
        sHuanJingSystem->RemoveCreatureHealthBars(creature);
        sHuanJingSystem->ClearCreatureVirtualMana(guid);
        return;
    }

    if (sHuanJingSystem->IsHealthBarEnabled(creature))
        sHuanJingSystem->ResetCreatureHealthBarsToFull(creature, true);
}

void HuanJingSystemPlayerScript::OnPlayerLootItem(Player* player, Item* item, uint32 /*count*/, ObjectGuid lootguid)
{
    if (!player || !item)
        return;

    // 【调试日志】记录所有拾取事件
    ItemTemplate const* itemProto = item->GetTemplate();
    LOG_DEBUG("module.huanjing", ModuleOldFormat("[待鉴定调试] 拾取物品: 玩家=%s, 物品ID=%s, 物品GUID=%s, 物品类型=%s, 子类型=%s, lootguid类型=%s", player->GetName(), item->GetEntry(), item->GetGUID().GetCounter(), itemProto ? itemProto->Class : 0, itemProto ? itemProto->SubClass : 0, lootguid.IsCreatureOrVehicle() ? "生物" : (lootguid.IsGameObject() ? "游戏物体" : "其他")).c_str());

    // 只处理怪物或游戏物体（例如宝箱）掉落的物品
    if (!lootguid.IsCreatureOrVehicle() && !lootguid.IsGameObject())
    {
        LOG_DEBUG("module.huanjing", "[待鉴定调试] 跳过: lootguid不是生物或游戏物体");
        return;
    }

    // 【修复】使用拾取者当前的幻境等级来计算鉴定ID
    // 而不是使用击杀时记录的等级，这样每个玩家拾取时按自己的幻境等级获得对应的鉴定
    uint32 huanJingLevel = sHuanJingSystem->GetPlayerHuanJingLevel(player);

    CreatureDeathIdentificationContext deathContext;
    bool foundInCache = false;
    {
        std::lock_guard<std::mutex> lock(sHuanJingSystem->_creatureMutex);
        auto context = sHuanJingSystem->_creatureDeathIdentificationGroup.find(lootguid);
        if (context != sHuanJingSystem->_creatureDeathIdentificationGroup.end())
        {
            deathContext = context->second;
            foundInCache = true;
        }
    }
    HuanJingCreatureConfig const* templateLootConfig = nullptr;
    bool hasTemplateLoot = foundInCache && deathContext.fromMapControl;
    if (!hasTemplateLoot && lootguid.IsCreatureOrVehicle())
    {
        if (Creature* lootCreature = ObjectAccessor::GetCreature(*player, lootguid))
            templateLootConfig = sHuanJingSystem->GetMapCreatureConfig(lootCreature, nullptr);
        if (templateLootConfig && !templateLootConfig->attributeTemplateId)
            templateLootConfig = nullptr;
        hasTemplateLoot = templateLootConfig != nullptr;
    }

    // 【经济漏洞修复 二-A8】生物被调整/死亡记录的等级作为奖励上限：
    // 原实现只按拾取者当前等级解析，低等级杀怪后切高等级再拾取即可低难度拿高倍率装备
    uint32 capLevel = 0;

    // 检查生物是否被幻境系统调整过（只有被调整过的生物才能掉落倍率装备）
    if (lootguid.IsCreatureOrVehicle())
    {
        Creature* creature = ObjectAccessor::GetCreature(*player, lootguid);
        if (creature)
        {
            // 检查生物是否已被幻境系统标记
            uint64 creatureKey = GenerateCreatureKey(creature);
            bool hasAdjustment = false;
            {
                std::lock_guard<std::mutex> lock(sHuanJingSystem->_creatureMutex);
                auto it = sHuanJingSystem->_creatureAdjustedLevels.find(creatureKey);
                if (it != sHuanJingSystem->_creatureAdjustedLevels.end())
                {
                    hasAdjustment = true;
                    capLevel = it->second;
                }
            }

            // 如果生物没有被调整，也检查死亡记录（生物可能已死亡但记录还在）
            if (!hasAdjustment)
            {
                uint32 deathLevel = sHuanJingSystem->GetCreatureDeathHuanJingLevel(lootguid);
                if (deathLevel > 0)
                {
                    hasAdjustment = true;
                    capLevel = deathLevel;
                    LOG_DEBUG("module.huanjing", ModuleOldFormat("拾取：生物 %s 有死亡记录，等级=%s", creature->GetEntry(), deathLevel).c_str());
                }
            }

            if (!hasAdjustment && !hasTemplateLoot)
            {
                // 生物没有被幻境系统调整过，不应该掉落倍率装备
                LOG_DEBUG("module.huanjing", ModuleOldFormat("拾取：生物 %s 未被幻境系统调整，不处理倍率装备", creature->GetEntry()).c_str());
                return;
            }

            LOG_DEBUG("module.huanjing", ModuleOldFormat("拾取：生物 %s 被幻境系统调整过，使用拾取者幻境等级=%s", creature->GetEntry(), huanJingLevel).c_str());
        }
        else
        {
            // 生物对象不存在，检查是否有死亡记录
            uint32 deathLevel = sHuanJingSystem->GetCreatureDeathHuanJingLevel(lootguid);
            if (deathLevel == 0 && !hasTemplateLoot)
            {
                // 没有死亡记录，不处理倍率装备
                LOG_DEBUG("module.huanjing", "拾取：生物对象不存在且无死亡记录，不处理倍率装备");
                return;
            }
            capLevel = deathLevel;
            LOG_DEBUG("module.huanjing", ModuleOldFormat("拾取：生物对象不存在但有死亡记录，使用拾取者幻境等级=%s", huanJingLevel).c_str());
        }

        // 奖励等级封顶于击杀难度（组队中低于该等级的玩家仍按自身等级解析）
        if (capLevel > 0 && huanJingLevel > capLevel)
        {
            LOG_DEBUG("module.huanjing", ModuleOldFormat("拾取：拾取者等级 %s 超过生物调整等级 %s，按生物等级封顶", huanJingLevel, capLevel).c_str());
            huanJingLevel = capLevel;
        }
    }

    // 如果拾取者幻境等级为0，则不处理倍率装备
    if (huanJingLevel == 0 && !hasTemplateLoot)
    {
        LOG_DEBUG("module.huanjing", ModuleOldFormat("[待鉴定调试] 跳过: 拾取者幻境等级为0, 物品ID=%s", item->GetEntry()).c_str());
        return;
    }

    // 只对可鉴定的装备进行处理
    ItemTemplate const* proto = item->GetTemplate();
    if (!proto || (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR))
    {
        LOG_DEBUG("module.huanjing", ModuleOldFormat("[待鉴定调试] 跳过: 物品不是武器或护甲, 物品ID=%s, Class=%s", item->GetEntry(), proto ? proto->Class : 0).c_str());
        return;
    }

    // 获取物品信息
    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 itemId = item->GetEntry();
    uint32 playerGuid = player->GetGUID().GetCounter();

    // 按配置解析幻境装备属性：装备属性值 0=关闭，1=使用幻境等级*装备属性倍率系数，>1=固定值
    uint32 multiplier = 0;
    char multiplierMode = '-';
    auto resolveItemAttributeFromConfig = [&](uint32 equipAttrValue, float equipAttrMultiplier, char equipAttrMode)
    {
        float multiplierFloat = ResolveConfiguredItemAttributeValue(huanJingLevel, item, equipAttrValue, equipAttrMultiplier);
        multiplier = static_cast<uint32>(multiplierFloat);
        multiplierMode = ResolveConfiguredItemAttributeMode(equipAttrValue, equipAttrMode);
    };

    // 优先从缓存中获取鉴定组ID（修复尸体消失后无法鉴定的问题）
    uint32 identificationGroupId = 0;
    uint8 identificationQuality = 0;
    uint32 identificationItemLevel = 0;
    if (foundInCache && (!hasTemplateLoot || deathContext.fromMapControl))
    {
        identificationGroupId = deathContext.identificationGroupId;
        identificationQuality = deathContext.identificationQuality;
        identificationItemLevel = deathContext.identificationItemLevel;
        resolveItemAttributeFromConfig(
            deathContext.equipAttrValue, deathContext.equipAttrMultiplier, deathContext.multiplierMode);
    }
    else
        foundInCache = false;

    // 获取配置以检查品质和物品等级要求
    HuanJingCreatureConfig const* config = nullptr;
    if (!foundInCache)
    {
        // 关键修复：缓存中没有（可能拾取事件在死亡事件之前），尝试从生物对象获取
        Creature* creature = ObjectAccessor::GetCreature(*player, lootguid);
        if (creature)
        {
            uint32 creatureId = creature->GetEntry();
            uint32 mapId = creature->GetMapId();
            uint32 areaId = creature->GetAreaId();
            uint32 difficulty = sHuanJingSystem->GetCreatureMapDifficulty(creature);
            uint32 creatureLevel = creature->GetLevel();
            uint32 creatureType = sHuanJingSystem->GetCreatureTypeByEntry(creatureId);

            config = hasTemplateLoot ? templateLootConfig : sHuanJingSystem->GetCreatureConfig(creature, player);

            if (config)
            {
                identificationGroupId = config->identificationGroup;
                identificationQuality = config->identificationQuality;
                identificationItemLevel = config->identificationItemLevel;
                resolveItemAttributeFromConfig(config->equipAttrValue, config->equipAttrMultiplier, config->equipAttrMode);
            }
        }
    }
    else if (!hasTemplateLoot)
    {
        // Map-control corpses retain the template selected at death.
        Creature* creature = ObjectAccessor::GetCreature(*player, lootguid);
        if (creature)
        {
            uint32 creatureId = creature->GetEntry();
            uint32 mapId = creature->GetMapId();
            uint32 areaId = creature->GetAreaId();
            uint32 difficulty = sHuanJingSystem->GetCreatureMapDifficulty(creature);
            uint32 creatureLevel = creature->GetLevel();
            uint32 creatureType = sHuanJingSystem->GetCreatureTypeByEntry(creatureId);

            config = sHuanJingSystem->GetCreatureConfig(creature, player);

            if (config)
            {
                identificationGroupId = config->identificationGroup;
                identificationQuality = config->identificationQuality;
                identificationItemLevel = config->identificationItemLevel;
                resolveItemAttributeFromConfig(config->equipAttrValue, config->equipAttrMultiplier, config->equipAttrMode);
            }
        }
    }

    LOG_DEBUG("module.huanjing", ModuleOldFormat("[待鉴定调试] 解析装备属性: 物品ID=%s, 幻境等级=%s, 模式=%s, 数值=%s", itemId, huanJingLevel, multiplierMode, multiplier).c_str());

    // 检查物品品质是否满足最低要求
    if (proto->Quality < identificationQuality)
    {
        LOG_DEBUG("module.huanjing", ModuleOldFormat("[待鉴定调试] 跳过: 物品品质不满足要求, 物品ID=%s, 品质=%s, 最低要求=%s", itemId, proto->Quality, identificationQuality).c_str());
        return;
    }

    // 检查物品等级是否满足最低要求
    if (identificationItemLevel > 0 && proto->ItemLevel < identificationItemLevel)
    {
        LOG_DEBUG("module.huanjing", ModuleOldFormat("[待鉴定调试] 跳过: 物品等级不满足要求, 物品ID=%s, 等级=%s, 最低要求=%s", itemId, proto->ItemLevel, identificationItemLevel).c_str());
        return;
    }

    // 只有在“既没有幻境倍率可应用，也没有鉴定组可处理”时才真正跳过
    // 修复：白装等可能不触发倍率增强，但仍应允许进入鉴定流程
    if (!HasItemAttributeEffect(multiplier, multiplierMode) && identificationGroupId == 0)
    {
        LOG_DEBUG("module.huanjing", "[待鉴定调试] 跳过: 幻境属性无效且无鉴定组, 物品ID={}, 模式={}, 数值={}",
            itemId, multiplierMode, multiplier);
        return;
    }

    auto savePendingIdentification = [itemGuid, itemId, playerGuid, multiplier, multiplierMode, identificationGroupId]()
    {
        auto* pending = CharacterDatabase.GetPreparedStatement(CHAR_UPSERT_PENDING_LOOT_IDENTIFICATION);
        pending->SetArguments(itemGuid, itemId, playerGuid, multiplier,
            ItemAttributeModeToDbValue(multiplierMode), identificationGroupId);
        CharacterDatabase.Execute(pending);
    };

    // 配置了鉴定组时，拾取后延迟到物品进入玩家背包后再鉴定。
#ifdef MODULE_ITEM_IDENTIFICATION
    if (identificationGroupId > 0 && sItemIdentificationSystem && sItemIdentificationSystem->_enabled)
    {
        ObjectGuid playerObjectGuid = player->GetGUID();
        bool const hasAttributeEffect = HasItemAttributeEffect(multiplier, multiplierMode);

        LOG_DEBUG("module.huanjing", "[拾取自动鉴定] 已排队: 玩家={}, 物品ID={}, GUID={}, 鉴定组={}, 幻境属性={}{}",
            player->GetName(), itemId, itemGuid, identificationGroupId, multiplierMode, multiplier);

        player->m_Events.AddEventAtOffset([playerObjectGuid, itemId, itemGuid, multiplier, multiplierMode,
            identificationGroupId, hasAttributeEffect, savePendingIdentification]()
        {
            Player* p = ObjectAccessor::FindPlayer(playerObjectGuid);
            if (!p || !p->IsInWorld())
                return;

            Item* liveItem = p->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(itemGuid));
            if (!liveItem || liveItem->GetEntry() != itemId)
            {
                LOG_DEBUG("module.huanjing", "[拾取自动鉴定] 延迟执行未找到物品: 玩家={}, 物品ID={}, GUID={}, 鉴定组={}",
                    p->GetName(), itemId, itemGuid, identificationGroupId);
                return;
            }

            if (!sItemIdentificationSystem || !sItemIdentificationSystem->_enabled)
            {
                savePendingIdentification();

                LOG_DEBUG("module.huanjing", "[拾取自动鉴定] 延迟执行时鉴定系统不可用，已写入待鉴定标记: "
                    "玩家={}, 物品ID={}, GUID={}, 鉴定组={}", p->GetName(), itemId, itemGuid, identificationGroupId);
                return;
            }

            if (hasAttributeEffect)
                sHuanJingSystem->ApplyItemAttributeMultiplier(liveItem, static_cast<float>(multiplier), multiplierMode);

            bool identified = sItemIdentificationSystem->ApplyIdentification(p, liveItem, identificationGroupId);
            if (!identified && sItemIdentificationSystem->IsItemIdentified(itemGuid))
                identified = true;

            if (identified)
            {
                if (hasAttributeEffect)
                {
                    auto* update = CharacterDatabase.GetPreparedStatement(CHAR_UPD_LOOT_IDENTIFICATION_GROUP);
                    update->SetArguments(identificationGroupId, itemGuid);
                    CharacterDatabase.DirectExecute(update);
                    sHuanJingSystem->UpdateItemIdentificationGroup(itemGuid, identificationGroupId);
                }

                auto* erase = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PENDING_LOOT_IDENTIFICATION);
                erase->SetData(0, itemGuid);
                CharacterDatabase.DirectExecute(erase);
                sItemIdentificationSystem->ClearItemCache(itemId, itemGuid);
                sItemIdentificationSystem->AddToIdentifiedCache(itemGuid);
                sItemIdentificationSystem->SendAllModuleDataAddon(p, itemId, itemGuid);

                LOG_DEBUG("module.huanjing", "[拾取自动鉴定] 延迟执行成功: 玩家={}, 物品ID={}, GUID={}, 鉴定组={}",
                    p->GetName(), itemId, itemGuid, identificationGroupId);
                return;
            }

            savePendingIdentification();

            LOG_WARN("module.huanjing", "拾取自动鉴定失败，已保留待鉴定标记: 玩家={}, 物品ID={}, GUID={}, 鉴定组={}",
                p->GetName(), itemId, itemGuid, identificationGroupId);
        }, 500ms);

        return;
    }
#endif

    // 自动鉴定不可用、失败，或仅有幻境倍率无鉴定组时，保留待鉴定标记作为兜底。
    savePendingIdentification();

    LOG_DEBUG("module.huanjing", "[待鉴定兜底] 已写入标记: 玩家={}, 物品ID={}, GUID={}, 鉴定组={}",
        player->GetName(), itemId, itemGuid, identificationGroupId);
}


bool HuanJingSystemItemScript::OnItemCreate(Item* item, ItemTemplate const* /*itemProto*/, Player const* owner)
{
    // 不再在这里调用AdjustItemStats，改为在OnStoreNewItem中处理
    // 这样确保物品真正添加到玩家背包时才应用幻境倍率属性
    return true;
}

bool HuanJingSystemItemScript::OnRemove(Player* player, Item* item)
{
    if (!player || !item)
        return true;

    // 这个钩子现在主要用作备用清理，正常情况下属性应该已经在OnPlayerCanUnequipItem中被移除了
    // 只在物品被删除时进行最后的清理

    return true; // 允许移除
}


// 已删除：HuanJingSystemPlayerScript3 - 功能已禁用且不再需要
// 已删除：g_delayedAdjustments - 不再需要延迟调整机制

// 脚本钩子实现
void HuanJingSystemPlayerScript::OnPlayerLogin(Player* player)
{
    if (!player)
        return;

    // 加载装备幻境属性增强数据；幻境等级/经验存档已移除，不再读取旧等级表。
    sHuanJingSystem->LoadItemAttributeEnhancements(player->GetGUID().GetCounter());

    // 登录时只清空旧的幻境效果跟踪，不再在这里批量解析和应用全部属性，
    // 避免在主线程一次性处理几十件装备导致长时间卡顿。
    sHuanJingSystem->ClearPlayerAppliedHuanJingEnhancements(player);

    // 【核心改动】：将原来在 OnPlayerLogin 中一次性完成的“为所有装备应用幻境属性”
    // 拆分为多个小事件，通过玩家的 m_Events 分批执行，每个事件只处理少量装备，
    // 这样可以显著降低单帧耗时，避免其他玩家在有人登录时被卡住。

    // 预先收集需要处理的装备槽位（只考虑有幻境数据或倍率的装备）
    struct HuanJingPendingSlot
    {
        uint8 slot;
    };

    std::vector<HuanJingPendingSlot> pendingSlots;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        uint32 itemGuid = item->GetGUID().GetCounter();
        ItemAttributeEnhancement* enhancement = sHuanJingSystem->GetItemAttributeEnhancement(itemGuid);
        float multiplier = sHuanJingSystem->GetItemAttributeMultiplier(item);
        char multiplierMode = sHuanJingSystem->GetItemAttributeMode(item);

        if (enhancement || HasItemAttributeEffect(multiplier, multiplierMode))
        {
            pendingSlots.push_back({ slot });
        }
    }

    // 如果玩家当前没有任何需要应用幻境效果的装备，直接返回即可
    if (pendingSlots.empty())
        return;

    // 每件装备之间的延迟（毫秒），用于把负载摊到多帧中
    static constexpr uint32 PER_SLOT_DELAY_MS = 50;

    // 为每个槽位排一个延迟事件：事件中调用 ApplyHuanJingEnhancement，
    // 但使用 updateStats=false，避免每件装备都触发一次全量属性刷新。
    for (uint32 index = 0; index < pendingSlots.size(); ++index)
    {
        uint8 slot = pendingSlots[index].slot;
        auto delay = std::chrono::milliseconds(PER_SLOT_DELAY_MS * index);

        player->m_Events.AddEventAtOffset([player, slot]()
        {
            if (!player || !player->IsInWorld())
                return;

            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                return;

            uint32 itemGuid = item->GetGUID().GetCounter();
            ItemAttributeEnhancement* enhancement = sHuanJingSystem->GetItemAttributeEnhancement(itemGuid);
            float multiplier = sHuanJingSystem->GetItemAttributeMultiplier(item);
            char multiplierMode = sHuanJingSystem->GetItemAttributeMode(item);

            // 只在确实有幻境数据或倍率时才处理，避免无效开销
            if (!(enhancement || HasItemAttributeEffect(multiplier, multiplierMode)))
                return;
            // 关键点：这里使用 updateStats=false，避免每件装备都触发一次全量属性重算，
            // 最终由后面的“统一刷新”事件来做一次总的 UpdateAllStats。
            sHuanJingSystem->ApplyHuanJingEnhancement(player, item, false);
        }, delay);
    }

    // 在所有槽位事件之后，再排一个统一刷新事件
    auto finalDelay = std::chrono::milliseconds(PER_SLOT_DELAY_MS * pendingSlots.size() + 50);
    player->m_Events.AddEventAtOffset([player]()
    {
        if (!player || !player->IsInWorld() || player->GetSession()->PlayerLogout())
            return;

        // 统一刷新一次玩家属性，确保前面的增量全部生效
        player->UpdateAllStats();
        player->UpdateAttackPowerAndDamage();
        player->UpdateAttackPowerAndDamage(true);
        player->UpdateMaxHealth();
        player->UpdateMaxPower(POWER_MANA);

        // 清理未装备物品的属性增强，防止属性叠加
        sHuanJingSystem->CleanupUnequippedItemEnhancements(player);

        // 定期检查是否需要清理装备数据
        sHuanJingSystem->ScheduleItemCleanup();
    }, finalDelay);
}

void HuanJingSystemPlayerScript::OnPlayerLogout(Player* player)
{
    ResetNearbyHealthBarCreaturesForPlayer(player, 250.0f, true);

    // 玩家离开前，重新调整附近生物的属性
    sHuanJingSystem->ReAdjustNearbyCreaturesOnPlayerLeave(player, 200.0f);

    // 【内存泄漏修复】清理玩家内存缓存，防止长期运行内存累积
    uint32 playerGuid = player->GetGUID().GetCounter();
    _lastTargetGuids.erase(playerGuid);
    sHuanJingSystem->ClearPlayerMemoryCache(playerGuid);

    LOG_DEBUG("module", ModuleOldFormat(">> 幻境系统: 玩家 %s 登出，已清理内存缓存", player->GetName()).c_str());
}

void HuanJingSystemPlayerScript::OnPlayerKilledByCreature(Creature* /*killer*/, Player* player)
{
    ResetNearbyHealthBarCreaturesForPlayer(player, 250.0f, true);
}

void HuanJingSystemPlayerScript::OnPlayerPVPKill(Player* /*killer*/, Player* player)
{
    ResetNearbyHealthBarCreaturesForPlayer(player, 250.0f, true);
}

void HuanJingSystemPlayerScript::OnPlayerReleasedGhost(Player* player)
{
    ResetNearbyHealthBarCreaturesForPlayer(player, 250.0f, true);
}

void HuanJingSystemPlayerScript::OnPlayerResurrect(Player* player, float /*restore_percent*/, bool& /*applySickness*/)
{
    ResetNearbyHealthBarCreaturesForPlayer(player, 250.0f, true);
}

// 用于防止装备交换时重复处理的静态集合
static std::set<std::pair<uint32, uint32>> s_recentlyProcessedUnequipItems;
static std::chrono::steady_clock::time_point s_lastUnequipCleanupTime = std::chrono::steady_clock::now();

void HuanJingSystemPlayerScript::OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update)
{
    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 playerGuid = player->GetGUID().GetCounter();
    std::pair<uint32, uint32> itemKey = {playerGuid, itemGuid};

    // 清除该物品的"最近处理"标记，以便下次卸装备时可以正常处理
    s_recentlyProcessedUnequipItems.erase(itemKey);

    // 登录加载阶段（未进世界）跳过属性应用，交给 OnPlayerLogin 批量处理
    if (!player->IsInWorld())
        return;

    ItemAttributeEnhancement* enhancement = sHuanJingSystem->GetItemAttributeEnhancement(itemGuid);
    float multiplier = sHuanJingSystem->GetItemAttributeMultiplier(item);
    char multiplierMode = sHuanJingSystem->GetItemAttributeMode(item);

    if (enhancement || HasItemAttributeEffect(multiplier, multiplierMode))
    {
        // 应用幻境增强效果
        sHuanJingSystem->ApplyHuanJingEnhancement(player, item, true);
    }
}

// 处理装备移除前的事件
bool HuanJingSystemPlayerScript::OnPlayerCanUnequipItem(Player* player, uint16 pos, bool swap)
{
    if (!player)
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

    // 检查是否有幻境增强记录
    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 playerGuid = player->GetGUID().GetCounter();
    ItemAttributeEnhancement* enhancement = sHuanJingSystem->GetItemAttributeEnhancement(itemGuid);
    float multiplier = sHuanJingSystem->GetItemAttributeMultiplier(item);
    char multiplierMode = sHuanJingSystem->GetItemAttributeMode(item);
    std::string appliedStats = sHuanJingSystem->GetAppliedHuanJingEnhancement(player, item);

    std::pair<uint32, uint32> itemKey = {playerGuid, itemGuid};

    // 每5秒清理一次过期记录
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - s_lastUnequipCleanupTime).count() >= 5)
    {
        s_recentlyProcessedUnequipItems.clear();
        s_lastUnequipCleanupTime = now;
    }

    // 如果这个物品最近已经被处理过，跳过（防止钩子被多次调用时重复移除）
    if (s_recentlyProcessedUnequipItems.find(itemKey) != s_recentlyProcessedUnequipItems.end())
        return true;

    if (enhancement || HasItemAttributeEffect(multiplier, multiplierMode) || !appliedStats.empty())
    {
        // 标记为已处理
        s_recentlyProcessedUnequipItems.insert(itemKey);
        // 在装备移除前移除幻境增强效果
        sHuanJingSystem->RemoveHuanJingEnhancement(player, item);
    }

    return true; // 允许卸装备
}

void HuanJingSystemPlayerScript::OnPlayerAfterMoveItemFromInventory(Player* player, Item* item, uint8 bag, uint8 slot, bool /*update*/)
{
    // 这个钩子用于处理从装备槽移动到背包的情况（备用清理）
    if (!player || !item)
        return;

    // 只处理从装备槽移出的情况
    if (bag != INVENTORY_SLOT_BAG_0 || slot >= EQUIPMENT_SLOT_END)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();

    // 检查是否有幻境增强记录
    ItemAttributeEnhancement* enhancement = sHuanJingSystem->GetItemAttributeEnhancement(itemGuid);
    float multiplier = sHuanJingSystem->GetItemAttributeMultiplier(item);
    char multiplierMode = sHuanJingSystem->GetItemAttributeMode(item);
    std::string appliedStats = sHuanJingSystem->GetAppliedHuanJingEnhancement(player, item);

    // 检查物品现在是否还在装备槽（武器互换情况）
    bool stillEquipped = false;
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (equippedItem && equippedItem->GetGUID().GetCounter() == itemGuid)
        {
            stillEquipped = true;
            break;
        }
    }

    if (stillEquipped)
        return;

    // 物品已离开装备槽，移除幻境属性
    if (enhancement || HasItemAttributeEffect(multiplier, multiplierMode) || !appliedStats.empty())
    {
        sHuanJingSystem->RemoveHuanJingEnhancement(player, item);
    }
}

void HuanJingSystemPlayerScript::OnPlayerStoreNewItem(Player* player, Item* item, uint32 /*count*/)
{
    if (!player || !item)
        return;

    // 该钩子保留用于兼容和调试，不再在所有来源的物品上统一应用幻境倍率属性
    // 幻境倍率属性现在仅在拾取生物掉落的装备时通过 OnPlayerLootItem 应用，
    // 以避免商店购买、任务奖励等途径获得的物品也被附加倍率属性
    LOG_DEBUG("module", ModuleOldFormat(">> OnPlayerStoreNewItem: 玩家 %s 获得物品 %s（不再在此处应用幻境倍率属性）", player->GetName(), item->GetEntry()).c_str());
}

void HuanJingSystemPlayerScript::OnPlayerBeforeBuyItemFromVendor(Player* player, ObjectGuid vendorguid, uint32 vendorslot, uint32& item, uint8 count, uint8 bag, uint8 slot)
{
    // 不干预NPC商店购买逻辑，按照官方原版方式处理
}

void HuanJingSystemPlayerScript::OnPlayerUpdate(Player* player, uint32 diff)
{
    // 主动检测附近生物的标记状态
    if (!player || !player->IsInWorld())
        return;

    // 早期退出条件：玩家正在传送、死亡或离线
    if (player->IsBeingTeleported() || !player->IsAlive() || player->GetSession()->PlayerLogout())
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();

    // 当玩家切换目标时，发送一次当前目标血条信息给客户端
    Unit* selectedUnit = player->GetSelectedUnit();
    ObjectGuid currentTargetGuid;
    if (selectedUnit)
        currentTargetGuid = selectedUnit->GetGUID();

    ObjectGuid lastTargetGuid;
    auto lastIt = _lastTargetGuids.find(playerGuid);
    if (lastIt != _lastTargetGuids.end())
        lastTargetGuid = lastIt->second;

    if (currentTargetGuid != lastTargetGuid)
    {
        _lastTargetGuids[playerGuid] = currentTargetGuid;

        if (UnitHasCreatureType(selectedUnit))
        {
            Creature* creature = selectedUnit->ToCreature();
            if (creature && creature->IsAlive())
            {
                // Creatures can enter the world before the delayed route load.
                // Resolve the route again when the player first selects one.
                sHuanJingSystem->GetMapCreatureConfig(creature, nullptr);

                // 选中包已经按专属配置重算过。等级相同会直接返回。
                // 没有具体生物、地图或区域配置的生物不会被改血量。
                sHuanJingSystem->AdjustCreatureStatsForPlayer(creature, player);

                // Attribute templates now use the legacy MultipleHp UI. Send its
                // packet to the player that just selected this creature.
                sAsCreature->SendUpdateMultipleHp(player, creature);

                // 不再向聊天框发送提示，仅通过 Addon 消息通知当前目标的客户端插件。
                SendCreatureHuanJingBroadcastToPlayer(player, creature, "TargetChanged");
            }
        }
    }

    // 调试信息：仅在DEBUG模式下输出，避免影响性能
    #ifdef _DEBUG
    static std::unordered_map<uint32, uint32> debugTimers;
    debugTimers[playerGuid] += diff;
    if (debugTimers[playerGuid] >= 10000) // 每10秒输出一次调试信息
    {
        LOG_DEBUG("module", ModuleOldFormat(">> [幻境系统] OnPlayerUpdate 被调用 - 玩家: %s", player->GetName()).c_str());
        debugTimers[playerGuid] = 0;
    }
    #endif

    // 生物属性调整已改为在战斗事件（选中目标并攻击）中按个体触发，
    // 不再做大范围的周期性扫描，以避免玩家移动时频繁调整大量生物。
}

void HuanJingSystemPlayerScript::OnPlayerMapChanged(Player* player)
{
    if (!player || !player->IsInWorld())
        return;

    // 地图切换仅记录环境变化，幻境倍率调整统一由战斗事件按目标单体触发，
    // 避免在玩家进入新地图时对大范围生物做同步调整。
}

// 幻境系统命令脚本
class HuanJingCommandScript : public CommandScript
{
public:
    HuanJingCommandScript() : CommandScript("HuanJingCommandScript") { }

    Acore::ChatCommands::ChatCommandTable GetCommands() const override
    {
        static Acore::ChatCommands::ChatCommandTable huanjingCommandTable =
        {
            { "重载",       HandleReloadCommand,       SEC_ADMINISTRATOR,  Acore::ChatCommands::Console::Yes },
            { "查看配置",   HandleShowConfigCommand,   SEC_GAMEMASTER,     Acore::ChatCommands::Console::Yes },
            { "测试",       HandleTestCommand,         SEC_GAMEMASTER,     Acore::ChatCommands::Console::No },
            { "刷新",       HandleRefreshCommand,      SEC_GAMEMASTER,     Acore::ChatCommands::Console::No },
            { "装备倍率",   HandleItemMultiplierCommand, SEC_GAMEMASTER,   Acore::ChatCommands::Console::No },
            { "查询倍率",   HandleQueryMultiplierCommand, SEC_PLAYER,      Acore::ChatCommands::Console::No },
            { "清理装备",   HandleCleanupItemsCommand, SEC_ADMINISTRATOR,  Acore::ChatCommands::Console::Yes },
            { "检查生物",   HandleCheckCreatureCommand, SEC_GAMEMASTER,    Acore::ChatCommands::Console::No },
            { "血条测试",   HandleHealthBarTestCommand, SEC_GAMEMASTER,    Acore::ChatCommands::Console::No },
            { "清理副本",   HandleCleanupInstanceCommand, SEC_GAMEMASTER,  Acore::ChatCommands::Console::No }
        };

        static Acore::ChatCommands::ChatCommandTable commandTable =
        {
            { "地图生物", huanjingCommandTable }
        };
        return commandTable;
    }

    // 显示玩家幻境信息
    static bool HandleInfoCommand(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            handler->SendSysMessage("错误：无法获取玩家信息");
            return true;
        }

        uint32 level = sHuanJingSystem->GetPlayerHuanJingLevel(player);
        uint32 exp = sHuanJingSystem->GetPlayerHuanJingExp(player);

        // 获取等级配置
        auto levelConfig = sHuanJingSystem->GetLevelConfig(level);
        auto nextLevelConfig = sHuanJingSystem->GetLevelConfig(level + 1);

        handler->PSendSysMessage("=== 幻境系统信息 ===");
        handler->PSendSysMessage("%s", ModuleOldFormat("当前幻境等级: %s", level).c_str());

        if (levelConfig)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("等级名称: %s", levelConfig->name.c_str()).c_str());
            if (!levelConfig->buff.empty())
                handler->PSendSysMessage("%s", ModuleOldFormat("幻境buff: %s", levelConfig->buff.c_str()).c_str());
        }

        handler->PSendSysMessage("%s", ModuleOldFormat("当前经验: %s", exp).c_str());

        if (nextLevelConfig)
        {
            uint32 requiredExp = nextLevelConfig->requiredExp;
            handler->PSendSysMessage("%s", ModuleOldFormat("升级所需经验: %s", requiredExp).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("距离升级还需: %s 经验", requiredExp > exp ? requiredExp - exp : 0).c_str());
        }
        else
        {
            handler->PSendSysMessage("已达到最高等级");
        }

        return true;
    }

    // 处理 .地图生物 界面 命令 - 打开客户端UI界面
    static bool HandleUICommand(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            handler->SendSysMessage("错误：无法获取玩家信息");
            return false;
        }

        handler->SendSysMessage("幻境等级界面已移除，地图配置会直接按地图/区域/生物条件生效。");

        return true;
    }

    // 处理 .地图生物 等级 命令 - 普通玩家可以修改自己的等级，不能修改别人的
    static bool HandleLevelCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            handler->SendSysMessage("错误：无法获取玩家信息");
            return false;
        }

        // 如果没有参数，只返回当前等级（供玩家和客户端插件使用）
        if (!*args)
        {
            uint32 currentLevel = sHuanJingSystem->GetPlayerHuanJingLevel(player);
            if (currentLevel > 0)
                sHuanJingSystem->EnsureLevelConfigExists(currentLevel);
            auto currentLevelConfig = sHuanJingSystem->GetLevelConfig(currentLevel);
            std::string currentLevelName = currentLevelConfig ? currentLevelConfig->name : "未知";

            // 仅输出一行，方便客户端UI解析
            handler->PSendSysMessage("%s", ModuleOldFormat("当前等级: %s (%s)", currentLevel, currentLevelName).c_str());
            return true;
        }

        // 解析参数
        char* levelStr = strtok((char*)args, " ");
        char* playerName = strtok(nullptr, " ");

        if (!levelStr)
        {
            handler->SendSysMessage("用法: .地图生物 等级 [等级数] [玩家名]");
            handler->SendSysMessage("- 不带参数：显示自己的幻境等级信息");
            handler->PSendSysMessage("%s", ModuleOldFormat("- 等级数：设置幻境等级 (0-%s)", sHuanJingSystem->GetMaxLevel()).c_str());
            handler->SendSysMessage("- 玩家名：仅管理员可用，设置指定玩家的等级");
            return true;
        }

        // 检查是否尝试修改其他玩家的等级
        if (playerName)
        {
            // 检查权限：只有管理员才能修改其他玩家的等级
            if (handler->GetSession()->GetSecurity() < SEC_GAMEMASTER)
            {
                handler->SendSysMessage("普通玩家只能修改自己的幻境等级，不能修改其他玩家的等级。");
                handler->SendSysMessage("使用 .地图生物 等级 <等级> 来设置自己的等级");
                return true;
            }

            // 管理员权限：查找指定玩家
            Player* target = ObjectAccessor::FindPlayerByName(playerName);
            if (!target)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("找不到玩家: %s", playerName).c_str());
                return true;
            }
            player = target; // 设置目标为指定玩家
        }

        uint32 level = atoi(levelStr);
        uint32 maxLevel = sHuanJingSystem->GetMaxLevel();

        if (level > maxLevel)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("错误：等级超出范围，有效等级范围是 0-%s", maxLevel).c_str());
            return true;
        }

        // 验证等级是否有效（0级是有效的，表示重置）
        HuanJingLevelConfig* levelConfig = nullptr;
        if (level > 0)
        {
            if (!sHuanJingSystem->EnsureLevelConfigExists(level))
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("错误：等级 %s 超出配置上限，有效等级范围是 0-%s", level, maxLevel).c_str());
                return true;
            }

            levelConfig = sHuanJingSystem->GetLevelConfig(level);
            if (!levelConfig)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("错误：等级 %s 的配置无法生成，有效等级范围是 0-%s", level, maxLevel).c_str());
                return true;
            }
        }

        // 检查目标等级的需求模板条件
        if (levelConfig && levelConfig->needTemplateId > 0)
        {
            if (!HasNeedTemplate(levelConfig->needTemplateId))
            {
                handler->SendSysMessage("|cffff0000错误：旧核心需求模板不存在，无法切换等级|r");
                LOG_ERROR("module.huanjing", ModuleOldFormat("HandleLevelCommand: 需求模板不存在, 玩家=%s 目标等级=%s 需求模板id=%s", player->GetName(), level, levelConfig->needTemplateId).c_str());
                return true;
            }

            bool meetsRequirements = CheckNeedTemplate(player, levelConfig->needTemplateId, false);

            if (!meetsRequirements)
            {
                // 不满足条件，显示需求模板条件信息
                handler->PSendSysMessage("%s", ModuleOldFormat("|cffff0000切换到幻境等级 %s (%s) 失败！|r", level, levelConfig->name.c_str()).c_str());
                handler->SendSysMessage("|cffffcc00不满足切换条件，请查看需求详情：|r");
                CheckNeedTemplate(player, levelConfig->needTemplateId, true);

                LOG_DEBUG("module.huanjing", ModuleOldFormat("HandleLevelCommand: 玩家=%s 切换到等级=%s 失败，不满足需求模板id=%s 的条件", player->GetName(), level, levelConfig->needTemplateId).c_str());
                return true;
            }

            if (!ConsumeNeedTemplate(player, levelConfig->needTemplateId, "幻境等级切换需求消耗"))
            {
                handler->SendSysMessage("|cffff0000切换等级失败：消耗需求资源时出错！|r");
                LOG_ERROR("module.huanjing", ModuleOldFormat("HandleLevelCommand: 玩家=%s 消耗需求模板id=%s 失败", player->GetName(), levelConfig->needTemplateId).c_str());
                return true;
            }

            LOG_DEBUG("module.huanjing", ModuleOldFormat("HandleLevelCommand: 玩家=%s 满足等级=%s 的需求模板id=%s 条件，已消耗资源", player->GetName(), level, levelConfig->needTemplateId).c_str());
        }

        // 设置新等级（仅更新玩家幻境等级和buff，不再在此处调整范围内所有生物属性）
        sHuanJingSystem->SetPlayerHuanJingLevel(player, level);

        // 应用新等级的buff
        if (level > 0)
        {
            auto levelConfig = sHuanJingSystem->GetLevelConfig(level);
            if (levelConfig && !levelConfig->buff.empty())
            {
                sHuanJingSystem->SetPlayerHuanJingBuff(player, levelConfig->buff);
            }
        }
        else
        {
            // 0级时清除buff
            sHuanJingSystem->SetPlayerHuanJingBuff(player, "");
        }

        // 获取等级名称
        std::string levelName = "无";
        if (level > 0)
        {
            auto levelConfig = sHuanJingSystem->GetLevelConfig(level);
            if (levelConfig)
            {
                levelName = levelConfig->name;
            }
            else
            {
                levelName = "未知";
            }
        }

        // 发送消息
        Player* executor = handler->GetSession()->GetPlayer();
        if (player != executor)
        {
            // 管理员设置其他玩家的等级
            handler->PSendSysMessage("%s", ModuleOldFormat("已将玩家 %s 的幻境等级设置为：%s (%s)", player->GetName().c_str(), level, levelName).c_str());
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("你的幻境等级已被管理员设置为：%s (%s)", level, levelName).c_str());
        }

        return true;
    }

    // 设置玩家幻境等级
    static bool HandleSetLevelCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .地图生物 设置等级 <等级> [玩家名]");
            return false;
        }

        char* levelStr = strtok((char*)args, " ");
        char* playerName = strtok(nullptr, " ");

        if (!levelStr)
        {
            handler->SendSysMessage("请指定等级");
            return false;
        }

        uint32 level = atoi(levelStr);
        uint32 maxLevel = sHuanJingSystem->GetMaxLevel();
        Player* target = handler->GetSession()->GetPlayer();

        if (level > maxLevel)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("错误：等级超出范围，有效等级范围是 0-%s", maxLevel).c_str());
            return false;
        }

        if (playerName)
        {
            target = ObjectAccessor::FindPlayerByName(playerName);
            if (!target)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("找不到玩家: %s", playerName).c_str());
                return false;
            }
        }

        // 检查等级配置是否存在
        HuanJingLevelConfig* levelConfig = nullptr;
        if (level > 0)
        {
            if (!sHuanJingSystem->EnsureLevelConfigExists(level))
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("等级 %s 超出配置上限，有效等级范围是 0-%s", level, maxLevel).c_str());
                return false;
            }

            levelConfig = sHuanJingSystem->GetLevelConfig(level);
            if (!levelConfig)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("等级 %s 的配置无法生成", level).c_str());
                return false;
            }
        }

        std::string levelName = levelConfig ? levelConfig->name : "无";
        uint32 needTemplateId = levelConfig ? levelConfig->needTemplateId : 0;

        LOG_DEBUG("module.huanjing", ModuleOldFormat("HandleSetLevelCommand: 执行者=%s 目标玩家=%s 设置幻境等级=%s (%s) 对应需求模板id=%s", handler->GetSession()->GetPlayer()->GetName(), target->GetName().c_str(), level, levelName.c_str(), needTemplateId).c_str());

        sHuanJingSystem->SetPlayerHuanJingLevel(target, level);

        handler->PSendSysMessage("%s", ModuleOldFormat("已将玩家 %s 的幻境等级设置为 %s (%s)", target->GetName().c_str(), level, levelName.c_str()).c_str());

        if (target != handler->GetSession()->GetPlayer())
        {
            ChatHandler(target->GetSession()).PSendSysMessage("%s", ModuleOldFormat("你的幻境等级已被设置为 %s (%s)", level, levelName.c_str()).c_str());
        }

        return true;
    }

    // 增加玩家幻境经验
    static bool HandleAddExpCommand(ChatHandler* handler, const char* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("用法: .地图生物 增加经验 <经验值> [玩家名]");
            return false;
        }

        char* expStr = strtok((char*)args, " ");
        char* playerName = strtok(nullptr, " ");

        if (!expStr)
        {
            handler->SendSysMessage("请指定经验值");
            return false;
        }

        uint32 exp = atoi(expStr);
        Player* target = handler->GetSession()->GetPlayer();

        if (playerName)
        {
            target = ObjectAccessor::FindPlayerByName(playerName);
            if (!target)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("找不到玩家: %s", playerName).c_str());
                return false;
            }
        }

        uint32 oldExp = sHuanJingSystem->GetPlayerHuanJingExp(target);
        uint32 oldLevel = sHuanJingSystem->GetPlayerHuanJingLevel(target);

        sHuanJingSystem->AddPlayerHuanJingExp(target, exp);

        uint32 newExp = sHuanJingSystem->GetPlayerHuanJingExp(target);
        uint32 newLevel = sHuanJingSystem->GetPlayerHuanJingLevel(target);

        handler->PSendSysMessage("%s", ModuleOldFormat("已为玩家 %s 增加 %s 幻境经验", target->GetName().c_str(), exp).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("经验: %s -> %s", oldExp, newExp).c_str());

        if (newLevel > oldLevel)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("恭喜！等级提升: %s -> %s", oldLevel, newLevel).c_str());
        }

        return true;
    }

    // 重置旧幻境等级数据（兼容命令实现，当前命令表不再暴露）
    static bool HandleResetCommand(ChatHandler* handler, const char* args)
    {
        Player* target = handler->GetSession()->GetPlayer();

        if (*args)
        {
            target = ObjectAccessor::FindPlayerByName(args);
            if (!target)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("找不到玩家: %s", args).c_str());
                return false;
            }
        }

        sHuanJingSystem->SetPlayerHuanJingLevel(target, 0);
        sHuanJingSystem->SetPlayerHuanJingExp(target, 0);

        handler->PSendSysMessage("%s", ModuleOldFormat("已重置玩家 %s 的幻境数据", target->GetName().c_str()).c_str());

        if (target != handler->GetSession()->GetPlayer())
        {
            ChatHandler(target->GetSession()).SendSysMessage("你的幻境数据已被重置");
        }

        return true;
    }

    // 重载地图生物控制配置
    static bool HandleReloadCommand(ChatHandler* handler, const char* /*args*/)
    {
        handler->SendSysMessage("正在重载地图生物控制配置...");

        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        uint32 refreshedCount = sHuanJingSystem->ReloadMapCreatureConfigsAndRefresh(player);

        handler->PSendSysMessage("地图生物控制配置重载完成，已刷新 %u 个附近非战斗生物", refreshedCount);
        return true;
    }

    static bool HandleShowConfigCommand(ChatHandler* handler, const char* args)
    {
        (void)args;
        handler->SendSysMessage("=== 幻境地图配置 ===");
        handler->PSendSysMessage("%s", ModuleOldFormat("已加载地图/生物属性配置: %s 个", sHuanJingSystem->GetCreatureConfigCount()).c_str());
        handler->SendSysMessage("提示：幻境系统现在按地图/区域/生物条件直接生效，不再读取玩家等级配置。");

        return true;
    }

    // 测试幻境系统功能
    static bool HandleTestCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!*args)
        {
            handler->SendSysMessage("测试选项:");
            handler->SendSysMessage("  属性 - 测试生物属性调整");
            handler->SendSysMessage("  伤害 - 详细测试伤害调整逻辑");
            handler->SendSysMessage("  命中 - 测试生物命中率、护甲穿透和格挡穿透");
            handler->SendSysMessage("  玩家命中 - 测试玩家命中率和护甲穿透");
            handler->SendSysMessage("  强制调整 - 强制重新调整选中生物的属性");
            handler->SendSysMessage("  上限 - 显示当前属性上限配置");
            handler->SendSysMessage("  掉落 - 测试掉落系统");
            handler->SendSysMessage("  升级 - 测试升级系统");
            handler->SendSysMessage("  配置 - 检查配置加载情况");
            handler->SendSysMessage("  倍率修复 - 验证倍率计算修复效果");
            return true;
        }

        std::string testType = args;

        if (testType == "属性")
        {
            handler->SendSysMessage("正在测试生物属性调整...");

            // 获取玩家目标生物
            Creature* target = handler->getSelectedCreature();
            if (!target)
            {
                handler->SendSysMessage("请先选择一个生物目标");
                return false;
            }

            // 显示调整前的属性
            uint32 oldHealth = target->GetMaxHealth();
            uint32 oldMana = target->GetMaxPower(POWER_MANA);
            uint32 oldArmor = target->GetArmor();
            uint32 oldAttackPower = target->GetUInt32Value(UNIT_FIELD_ATTACK_POWER);
            float oldMinDamage = target->GetFloatValue(UNIT_FIELD_MINDAMAGE);
            float oldMaxDamage = target->GetFloatValue(UNIT_FIELD_MAXDAMAGE);
            float oldRunSpeed = target->GetSpeedRate(MOVE_RUN);
            float oldAttackTime = target->GetFloatValue(UNIT_FIELD_BASEATTACKTIME);

            handler->SendSysMessage("=== 调整前属性 ===");
            handler->PSendSysMessage("%s", ModuleOldFormat("血量: %s, 法力: %s", oldHealth, oldMana).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("护甲: %s, 攻击强度: %s", oldArmor, oldAttackPower).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("伤害: %s - %s", oldMinDamage, oldMaxDamage).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("跑步速度: %s, 攻击时间: %sms", oldRunSpeed, oldAttackTime).c_str());

            // 强制调整属性
            sHuanJingSystem->AdjustCreatureStats(target, player);

            // 显示调整后的属性
            uint32 newHealth = target->GetMaxHealth();
            uint32 newMana = target->GetMaxPower(POWER_MANA);
            uint32 newArmor = target->GetArmor();
            uint32 newAttackPower = target->GetUInt32Value(UNIT_FIELD_ATTACK_POWER);
            float newMinDamage = target->GetFloatValue(UNIT_FIELD_MINDAMAGE);
            float newMaxDamage = target->GetFloatValue(UNIT_FIELD_MAXDAMAGE);
            float newRunSpeed = target->GetSpeedRate(MOVE_RUN);
            float newAttackTime = target->GetFloatValue(UNIT_FIELD_BASEATTACKTIME);

            handler->SendSysMessage("=== 调整后属性 ===");
            handler->PSendSysMessage("%s", ModuleOldFormat("血量: %s, 法力: %s", newHealth, newMana).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("护甲: %s, 攻击强度: %s", newArmor, newAttackPower).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("伤害: %s - %s", newMinDamage, newMaxDamage).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("跑步速度: %s, 攻击时间: %sms", newRunSpeed, newAttackTime).c_str());

            // 计算变化
            handler->SendSysMessage("=== 属性变化 ===");
            if (newHealth != oldHealth)
                handler->PSendSysMessage("%s", ModuleOldFormat("血量: %s -> %s (倍率: %s)", oldHealth, newHealth, (float)newHealth / oldHealth).c_str());
            if (newMana != oldMana)
                handler->PSendSysMessage("%s", ModuleOldFormat("法力: %s -> %s (倍率: %s)", oldMana, newMana, oldMana > 0 ? (float)newMana / oldMana : 0).c_str());
            if (newArmor != oldArmor)
                handler->PSendSysMessage("%s", ModuleOldFormat("护甲: %s -> %s (倍率: %s)", oldArmor, newArmor, oldArmor > 0 ? (float)newArmor / oldArmor : 0).c_str());
            if (newAttackPower != oldAttackPower)
                handler->PSendSysMessage("%s", ModuleOldFormat("攻击强度: %s -> %s (倍率: %s)", oldAttackPower, newAttackPower, oldAttackPower > 0 ? (float)newAttackPower / oldAttackPower : 0).c_str());
            if (newMinDamage != oldMinDamage || newMaxDamage != oldMaxDamage)
                handler->PSendSysMessage("%s", ModuleOldFormat("伤害: %s-%s -> %s-%s", oldMinDamage, oldMaxDamage, newMinDamage, newMaxDamage).c_str());
            if (newRunSpeed != oldRunSpeed)
                handler->PSendSysMessage("%s", ModuleOldFormat("跑步速度: %s -> %s (倍率: %s)", oldRunSpeed, newRunSpeed, oldRunSpeed > 0 ? newRunSpeed / oldRunSpeed : 0).c_str());
            if (newAttackTime != oldAttackTime)
                handler->PSendSysMessage("%s", ModuleOldFormat("攻击时间: %sms -> %sms (倍率: %s)", oldAttackTime, newAttackTime, oldAttackTime > 0 ? newAttackTime / oldAttackTime : 0).c_str());
        }
        else if (testType == "伤害")
        {
            handler->SendSysMessage("正在测试生物伤害调整逻辑...");

            // 获取玩家目标生物
            Creature* target = handler->getSelectedCreature();
            if (!target)
            {
                handler->SendSysMessage("请先选择一个生物目标");
                return false;
            }

            // 显示调整前的伤害值
            float oldMinDamage = target->GetFloatValue(UNIT_FIELD_MINDAMAGE);
            float oldMaxDamage = target->GetFloatValue(UNIT_FIELD_MAXDAMAGE);
            float baseMinDamage = target->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE);
            float baseMaxDamage = target->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE);

            // 获取原始基础伤害
            const CreatureTemplate* cInfo = target->GetCreatureTemplate();
            CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(target->GetLevel(), cInfo->unit_class);
            float originalBaseDamage = stats->GenerateBaseDamage(cInfo);
            float originalMinDamage = originalBaseDamage;
            float originalMaxDamage = originalBaseDamage * 1.5f;

            handler->PSendSysMessage("%s", ModuleOldFormat("调整前最终伤害 - 最小: %s, 最大: %s", oldMinDamage, oldMaxDamage).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("调整前武器伤害 - 最小: %s, 最大: %s", baseMinDamage, baseMaxDamage).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("原始基础伤害 - 最小: %s, 最大: %s", originalMinDamage, originalMaxDamage).c_str());

            // 获取生物的配置
            const HuanJingCreatureConfig* config = sHuanJingSystem->GetCreatureConfig(target, player);
            if (!config)
            {
                handler->SendSysMessage("该生物没有找到幻境配置");
                return false;
            }

            // 显示配置信息
            handler->PSendSysMessage("%s", ModuleOldFormat("找到配置 - 配置ID: %s, 幻境等级: %s", config->configId, config->huanJingLevel).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("伤害配置 - 最小伤害倍率: %s, 最大伤害倍率: %s", config->minDamageMultiplier, config->maxDamageMultiplier).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("伤害配置 - 最小伤害值: %s, 最大伤害值: %s", FormatUInt64(config->minDamageValue), FormatUInt64(config->maxDamageValue)).c_str());

            // 强制调整属性
            sHuanJingSystem->AdjustCreatureStats(target, player);

            // 显示调整后的伤害值
            float newMinDamage = target->GetFloatValue(UNIT_FIELD_MINDAMAGE);
            float newMaxDamage = target->GetFloatValue(UNIT_FIELD_MAXDAMAGE);
            float newBaseMinDamage = target->GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE);
            float newBaseMaxDamage = target->GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE);

            handler->PSendSysMessage("%s", ModuleOldFormat("调整后最终伤害 - 最小: %s, 最大: %s", newMinDamage, newMaxDamage).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("调整后基础伤害 - 最小: %s, 最大: %s", newBaseMinDamage, newBaseMaxDamage).c_str());

            // 计算变化
            if (newMinDamage != oldMinDamage || newMaxDamage != oldMaxDamage)
            {
                handler->SendSysMessage("伤害调整成功！");
                handler->PSendSysMessage("%s", ModuleOldFormat("最终伤害变化: %s -> %s (倍率: %s)", oldMinDamage, newMinDamage, (oldMinDamage > 0) ? (newMinDamage / oldMinDamage) : 0).c_str());
                handler->PSendSysMessage("%s", ModuleOldFormat("武器伤害变化: %s -> %s (倍率: %s)", baseMinDamage, newBaseMinDamage, (baseMinDamage > 0) ? (newBaseMinDamage / baseMinDamage) : 0).c_str());
                handler->PSendSysMessage("%s", ModuleOldFormat("相对原始伤害倍率: 最小=%s, 最大=%s", (originalMinDamage > 0) ? (newBaseMinDamage / originalMinDamage) : 0, (originalMaxDamage > 0) ? (newBaseMaxDamage / originalMaxDamage) : 0).c_str());
            }
            else
            {
                handler->SendSysMessage("伤害没有变化！可能的原因:");
                handler->SendSysMessage("1. 伤害倍率设置为1.0（无变化）");
                handler->SendSysMessage("2. 伤害值设置为0（使用倍率模式）");
                handler->SendSysMessage("3. 配置没有正确加载");
                handler->SendSysMessage("4. 生物的基础伤害为0");
            }
        }
        else if (testType == "命中")
        {
            handler->SendSysMessage("正在测试生物命中率、护甲穿透和格挡穿透...");

            // 获取玩家目标生物
            Creature* target = handler->getSelectedCreature();
            if (!target)
            {
                handler->SendSysMessage("请先选择一个生物目标");
                return false;
            }

            // 显示玩家和生物的详细信息
            uint32 playerHuanJingLevel = sHuanJingSystem->GetPlayerHuanJingLevel(player);
            uint32 difficulty = sHuanJingSystem->GetCreatureMapDifficulty(target);
            handler->PSendSysMessage("%s", ModuleOldFormat("玩家幻境等级: %s", playerHuanJingLevel).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("生物ID: %s, 地图ID: %s, 区域ID: %s, 难度: %s, 等级: %s", target->GetEntry(), target->GetMapId(), target->GetAreaId(), difficulty, target->GetLevel()).c_str());

            // 查找匹配的配置
            uint32 creatureType = sHuanJingSystem->GetCreatureTypeByEntry(target->GetEntry());
            auto config = sHuanJingSystem->GetCreatureConfig(target, player);

            if (config)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("找到匹配配置ID: %s", config->configId).c_str());
                handler->PSendSysMessage("%s", ModuleOldFormat("配置命中率加成: %s%%, 护甲穿透值: %s, 格挡穿透: %s+%s%%", config->hitChanceBonus, config->armorPenetration, config->blockPenetration, config->blockPenetrationPct).c_str());
            }
            else
            {
                handler->SendSysMessage("未找到匹配的配置，将使用基础调整");
            }

            // 显示调整前的命中率相关属性
            float oldMeleeHit = target->m_modMeleeHitChance;
            float oldRangedHit = target->m_modRangedHitChance;
            float oldSpellHit = target->m_modSpellHitChance;
            uint32 oldPlayerArmor = player ? player->GetArmor() : 0;
            float oldPlayerBlockChance = player ? player->GetFloatValue(PLAYER_BLOCK_PERCENTAGE) : 0.0f;

            handler->PSendSysMessage("%s", ModuleOldFormat("调整前 - 近战命中: %s%%, 远程命中: %s%%, 法术命中: %s%%, 玩家护甲: %s, 玩家格挡率: %s%%", oldMeleeHit, oldRangedHit, oldSpellHit, oldPlayerArmor, oldPlayerBlockChance).c_str());

            // 强制调整属性
            sHuanJingSystem->AdjustCreatureStats(target, player);

            // 显示调整后的命中率相关属性
            float newMeleeHit = target->m_modMeleeHitChance;
            float newRangedHit = target->m_modRangedHitChance;
            float newSpellHit = target->m_modSpellHitChance;
            uint32 newPlayerArmor = player ? player->GetArmor() : 0;
            float newPlayerBlockChance = player ? player->GetFloatValue(PLAYER_BLOCK_PERCENTAGE) : 0.0f;
            uint32 armorReduction = oldPlayerArmor > newPlayerArmor ? oldPlayerArmor - newPlayerArmor : 0;
            float blockReduction = oldPlayerBlockChance > newPlayerBlockChance ? oldPlayerBlockChance - newPlayerBlockChance : 0.0f;

            handler->PSendSysMessage("%s", ModuleOldFormat("调整后 - 近战命中: %s%%, 远程命中: %s%%, 法术命中: %s%%, 玩家护甲: %s (减少: %s)", newMeleeHit, newRangedHit, newSpellHit, newPlayerArmor, armorReduction).c_str());

            // 计算变化
            bool hasChange = false;
            if (std::abs(newMeleeHit - oldMeleeHit) > 0.01f || std::abs(newRangedHit - oldRangedHit) > 0.01f ||
                std::abs(newSpellHit - oldSpellHit) > 0.01f || armorReduction > 0 || blockReduction > 0.01f)
            {
                handler->SendSysMessage("命中率、护甲穿透和格挡穿透调整成功！");
                if (std::abs(newMeleeHit - oldMeleeHit) > 0.01f)
                    handler->PSendSysMessage("%s", ModuleOldFormat("近战命中率变化: %s%% -> %s%% (增加: %s%%)", oldMeleeHit, newMeleeHit, newMeleeHit - oldMeleeHit).c_str());
                if (std::abs(newRangedHit - oldRangedHit) > 0.01f)
                    handler->PSendSysMessage("%s", ModuleOldFormat("远程命中率变化: %s%% -> %s%% (增加: %s%%)", oldRangedHit, newRangedHit, newRangedHit - oldRangedHit).c_str());
                if (std::abs(newSpellHit - oldSpellHit) > 0.01f)
                    handler->PSendSysMessage("%s", ModuleOldFormat("法术命中率变化: %s%% -> %s%% (增加: %s%%)", oldSpellHit, newSpellHit, newSpellHit - oldSpellHit).c_str());
                if (armorReduction > 0)
                    handler->PSendSysMessage("%s", ModuleOldFormat("护甲穿透效果: 玩家护甲 %s -> %s (减少: %s)", oldPlayerArmor, newPlayerArmor, armorReduction).c_str());
                if (blockReduction > 0.01f)
                    handler->PSendSysMessage("%s", ModuleOldFormat("格挡穿透效果: 玩家格挡率 %s%% -> %s%% (减少: %s%%)", oldPlayerBlockChance, newPlayerBlockChance, blockReduction).c_str());
                hasChange = true;
            }

            if (!hasChange)
            {
                handler->SendSysMessage("命中率、护甲穿透和格挡穿透没有变化，请检查配置");
                if (playerHuanJingLevel == 0)
                {
                    handler->SendSysMessage("提示：玩家幻境等级为0，不会应用任何增强效果");
                }
            }
        }
        else if (testType == "玩家命中")
        {
            handler->SendSysMessage("正在测试玩家命中率、护甲穿透和格挡穿透...");

            // 显示调整前的玩家属性
            float oldMeleeHit = player->m_modMeleeHitChance;
            float oldRangedHit = player->m_modRangedHitChance;
            float oldSpellHit = player->m_modSpellHitChance;
            uint32 oldArmorPenRating = player->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + static_cast<uint16>(CR_ARMOR_PENETRATION));

            handler->PSendSysMessage("%s", ModuleOldFormat("调整前 - 近战命中率: %s%%, 远程命中率: %s%%, 法术命中率: %s%%, 护甲穿透Rating: %s", oldMeleeHit, oldRangedHit, oldSpellHit, oldArmorPenRating).c_str());

            // 测试应用属性修改
            int32 testHitRating = 500;  // 测试500点命中率rating
            int32 testArmorPenRating = 300;  // 测试300点护甲穿透rating

            // 应用测试属性
            sHuanJingSystem->ApplyStatModifier(player, ITEM_MOD_HIT_RATING, testHitRating, true);
            sHuanJingSystem->ApplyStatModifier(player, ITEM_MOD_ARMOR_PENETRATION_RATING, testArmorPenRating, true);

            // 显示调整后的玩家属性
            float newMeleeHit = player->m_modMeleeHitChance;
            float newRangedHit = player->m_modRangedHitChance;
            float newSpellHit = player->m_modSpellHitChance;
            uint32 newArmorPenRating = player->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + static_cast<uint16>(CR_ARMOR_PENETRATION));

            handler->PSendSysMessage("%s", ModuleOldFormat("调整后 - 近战命中率: %s%%, 远程命中率: %s%%, 法术命中率: %s%%, 护甲穿透Rating: %s", newMeleeHit, newRangedHit, newSpellHit, newArmorPenRating).c_str());

            // 计算变化
            bool hasChange = false;
            if (std::abs(newMeleeHit - oldMeleeHit) > 0.01f)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("近战命中率变化: %s%% -> %s%% (增加: %s%%)", oldMeleeHit, newMeleeHit, newMeleeHit - oldMeleeHit).c_str());
                hasChange = true;
            }
            if (std::abs(newRangedHit - oldRangedHit) > 0.01f)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("远程命中率变化: %s%% -> %s%% (增加: %s%%)", oldRangedHit, newRangedHit, newRangedHit - oldRangedHit).c_str());
                hasChange = true;
            }
            if (std::abs(newSpellHit - oldSpellHit) > 0.01f)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("法术命中率变化: %s%% -> %s%% (增加: %s%%)", oldSpellHit, newSpellHit, newSpellHit - oldSpellHit).c_str());
                hasChange = true;
            }
            if (newArmorPenRating != oldArmorPenRating)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("护甲穿透Rating变化: %s -> %s (增加: %s)", oldArmorPenRating, newArmorPenRating, newArmorPenRating - oldArmorPenRating).c_str());
                hasChange = true;
            }

            if (hasChange)
            {
                handler->SendSysMessage("玩家命中率、护甲穿透和格挡穿透调整成功！");
            }
            else
            {
                handler->SendSysMessage("玩家命中率、护甲穿透和格挡穿透没有变化，请检查实现");
            }

            // 移除测试属性
            sHuanJingSystem->ApplyStatModifier(player, ITEM_MOD_HIT_RATING, testHitRating, false);
            sHuanJingSystem->ApplyStatModifier(player, ITEM_MOD_ARMOR_PENETRATION_RATING, testArmorPenRating, false);

            handler->SendSysMessage("测试完成，已移除测试属性");
        }
        else if (testType == "上限")
        {
            handler->SendSysMessage("当前幻境系统属性上限配置:");
            handler->PSendSysMessage("%s", ModuleOldFormat("血量上限: %s", FormatOptionalUInt64Limit(sHuanJingSystem->GetMaxHealthValue())).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("伤害上限: %s", FormatOptionalDamageLimit(sHuanJingSystem->GetMaxDamageValue())).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("护甲上限: %s", FormatOptionalUInt64Limit(sHuanJingSystem->GetMaxArmorValue())).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("攻击强度上限: %s", FormatOptionalUInt64Limit(sHuanJingSystem->GetMaxAttackPowerValue())).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("抗性上限: %s", FormatOptionalUInt64Limit(sHuanJingSystem->GetMaxResistanceValue())).c_str());

            handler->SendSysMessage("");
            handler->SendSysMessage("说明:");
            handler->SendSysMessage("- 这些上限用于防止属性倍率过高导致数值溢出");
            handler->SendSysMessage("- 可在配置文件中修改这些上限值");
            handler->SendSysMessage("- 超过上限的属性会被自动限制并记录警告日志");

            // 如果有选中的生物，显示其当前属性与上限的对比
            if (Creature* target = handler->getSelectedCreature())
            {
                handler->SendSysMessage("");
                handler->PSendSysMessage("%s", ModuleOldFormat("选中生物 %s 的当前属性:", target->GetEntry()).c_str());

                AsInt64 currentHealth = GetCreatureCombatMaxHealth(target);
                AsInt64 displayHealthLimit = GetClientSafeResourceLimit(sHuanJingSystem->GetMaxHealthValue());
                float currentMinDamage = target->GetFloatValue(UNIT_FIELD_MINDAMAGE);
                float currentMaxDamage = target->GetFloatValue(UNIT_FIELD_MAXDAMAGE);
                AsInt64 currentArmor = SaturateToUInt64(target->GetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE));
                AsInt64 currentAttackPower = SaturateToUInt64(target->GetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE));

                if (displayHealthLimit > 0)
                    handler->PSendSysMessage("%s", ModuleOldFormat("血量: %s / %s (%s%%)", FormatUInt64(currentHealth), FormatUInt64(displayHealthLimit), static_cast<double>(Acore::Number::ToLongDouble(currentHealth) / Acore::Number::ToLongDouble(displayHealthLimit) * 100.0L)).c_str());
                else
                    handler->PSendSysMessage("%s", ModuleOldFormat("血量: %s / 无限制", FormatUInt64(currentHealth)).c_str());

                long double damageLimit = sHuanJingSystem->GetMaxDamageValue();
                if (damageLimit > 0.0L)
                {
                    handler->PSendSysMessage("%s", ModuleOldFormat("最小伤害: %s / %s (%s%%)", currentMinDamage, FormatOptionalDamageLimit(damageLimit), (static_cast<long double>(currentMinDamage) / damageLimit) * 100.0L).c_str());

                    handler->PSendSysMessage("%s", ModuleOldFormat("最大伤害: %s / %s (%s%%)", currentMaxDamage, FormatOptionalDamageLimit(damageLimit), (static_cast<long double>(currentMaxDamage) / damageLimit) * 100.0L).c_str());
                }
                else
                {
                    handler->PSendSysMessage("%s", ModuleOldFormat("最小伤害: %s / 无限制", currentMinDamage).c_str());
                    handler->PSendSysMessage("%s", ModuleOldFormat("最大伤害: %s / 无限制", currentMaxDamage).c_str());
                }

                AsInt64 armorLimit = sHuanJingSystem->GetMaxArmorValue();
                if (armorLimit > 0)
                    handler->PSendSysMessage("%s", ModuleOldFormat("护甲: %s / %s (%s%%)", FormatUInt64(currentArmor), FormatUInt64(armorLimit), static_cast<double>(Acore::Number::ToLongDouble(currentArmor) / Acore::Number::ToLongDouble(armorLimit) * 100.0L)).c_str());
                else
                    handler->PSendSysMessage("%s", ModuleOldFormat("护甲: %s / 无限制", FormatUInt64(currentArmor)).c_str());

                AsInt64 attackPowerLimit = sHuanJingSystem->GetMaxAttackPowerValue();
                if (attackPowerLimit > 0)
                    handler->PSendSysMessage("%s", ModuleOldFormat("攻击强度: %s / %s (%s%%)", FormatUInt64(currentAttackPower), FormatUInt64(attackPowerLimit), static_cast<double>(Acore::Number::ToLongDouble(currentAttackPower) / Acore::Number::ToLongDouble(attackPowerLimit) * 100.0L)).c_str());
                else
                    handler->PSendSysMessage("%s", ModuleOldFormat("攻击强度: %s / 无限制", FormatUInt64(currentAttackPower)).c_str());
            }
        }
        else if (testType == "掉落")
        {
            handler->SendSysMessage("正在测试掉落系统...");
            // 这里可以添加具体的测试逻辑
            handler->SendSysMessage("掉落系统测试完成");
        }
        else if (testType == "升级")
        {
            handler->SendSysMessage("幻境等级/经验系统已移除，地图配置会直接按地图/区域/生物条件生效。");
        }
        else if (testType == "配置")
        {
            handler->SendSysMessage("正在检查配置加载情况...");

            // 检查等级配置
            handler->SendSysMessage("幻境等级配置已移除，系统按地图/区域/生物条件直接生效。");

            // 检查生物配置
            uint32 creatureConfigCount = sHuanJingSystem->GetCreatureConfigCount();
            handler->PSendSysMessage("%s", ModuleOldFormat("已加载生物属性配置: %s 个", creatureConfigCount).c_str());

            // 显示当前玩家信息
            uint32 playerLevel = sHuanJingSystem->GetPlayerHuanJingLevel(player);
            uint32 mapId = player->GetMapId();
            handler->PSendSysMessage("%s", ModuleOldFormat("当前玩家幻境等级: %s", playerLevel).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("当前地图ID: %s", mapId).c_str());

            // 如果有选中的生物，显示生物信息
            if (Creature* target = handler->getSelectedCreature())
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("选中生物ID: %s", target->GetEntry()).c_str());
                handler->PSendSysMessage("%s", ModuleOldFormat("生物地图ID: %s", target->GetMapId()).c_str());
                handler->PSendSysMessage("%s", ModuleOldFormat("生物难度: %s", sHuanJingSystem->GetCreatureMapDifficulty(target)).c_str());
                handler->PSendSysMessage("%s", ModuleOldFormat("生物等级: %s", target->GetLevel()).c_str());

                // 尝试查找匹配的配置
                uint32 creatureType = sHuanJingSystem->GetCreatureTypeByEntry(target->GetEntry());
                auto config = sHuanJingSystem->GetCreatureConfig(target, player);

                if (config)
                {
                    handler->PSendSysMessage("%s", ModuleOldFormat("找到匹配配置ID: %s", config->configId).c_str());
                    handler->PSendSysMessage("%s", ModuleOldFormat("配置血量倍率: %s", config->healthMultiplier).c_str());
                }
                else
                {
                    handler->SendSysMessage("未找到匹配的配置");
                }
            }

            handler->SendSysMessage("配置检查完成");
        }
        else if (testType == "倍率修复")
        {
            handler->SendSysMessage("正在验证倍率计算修复效果...");

            // 获取玩家目标生物
            Creature* target = handler->getSelectedCreature();
            if (!target)
            {
                handler->SendSysMessage("请先选择一个生物目标");
                return false;
            }

            uint32 playerLevel = sHuanJingSystem->GetPlayerHuanJingLevel(player);
            if (playerLevel == 0)
            {
                handler->SendSysMessage("玩家幻境等级为0，无法测试倍率效果");
                return false;
            }

            handler->PSendSysMessage("%s", ModuleOldFormat("当前玩家幻境等级: %s", playerLevel).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("预期倍率: %s倍", 1.0f + playerLevel).c_str());

            // 获取生物模板信息
            const CreatureTemplate* cInfo = target->GetCreatureTemplate();
            if (!cInfo)
            {
                handler->SendSysMessage("无法获取生物模板信息");
                return false;
            }

            // 获取基础统计数据
            CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(target->GetLevel(), cInfo->unit_class);
            if (!stats)
            {
                handler->SendSysMessage("无法获取基础统计数据");
                return false;
            }

            // 计算原始属性值（与调整逻辑保持一致）
            AsInt64 originalHealth = ToUInt64FromAsInt64(stats->GenerateAsHealth(cInfo));
            float originalArmor = stats->BaseArmor * cInfo->ModArmor;
            float originalBaseDamage = stats->BaseDamage[cInfo->expansion];

            handler->SendSysMessage("=== 原始属性值 ===");
            handler->PSendSysMessage("%s", ModuleOldFormat("原始血量: %s", FormatUInt64(originalHealth)).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("原始护甲: %s", originalArmor).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("原始基础伤害: %s", originalBaseDamage).c_str());

            // 强制调整属性
            sHuanJingSystem->AdjustCreatureStats(target, player);

            // 获取调整后的属性
            uint32 adjustedHealth = target->GetMaxHealth();
            uint32 adjustedArmor = target->GetArmor();
            float adjustedMinDamage = target->GetFloatValue(UNIT_FIELD_MINDAMAGE);

            handler->SendSysMessage("=== 调整后属性值 ===");
            handler->PSendSysMessage("%s", ModuleOldFormat("调整后血量: %s", adjustedHealth).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("调整后护甲: %s", adjustedArmor).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("调整后最小伤害: %s", adjustedMinDamage).c_str());

            // 计算实际倍率
            float healthMultiplier = originalHealth > 0 ? static_cast<float>(static_cast<long double>(adjustedHealth) / Acore::Number::ToLongDouble(originalHealth)) : 0.0f;
            float armorMultiplier = originalArmor > 0 ? (float)adjustedArmor / originalArmor : 0;
            float damageMultiplier = originalBaseDamage > 0 ? adjustedMinDamage / originalBaseDamage : 0;

            handler->SendSysMessage("=== 实际倍率验证 ===");
            handler->PSendSysMessage("%s", ModuleOldFormat("血量倍率: %s倍 (预期: %s倍)", healthMultiplier, 1.0f + playerLevel).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("护甲倍率: %s倍 (预期: %s倍)", armorMultiplier, 1.0f + playerLevel).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("伤害倍率: %s倍 (预期: %s倍)", damageMultiplier, 1.0f + playerLevel).c_str());

            // 验证结果
            float expectedMultiplier = 1.0f + playerLevel;
            bool healthCorrect = std::abs(healthMultiplier - expectedMultiplier) < 0.1f;
            bool armorCorrect = std::abs(armorMultiplier - expectedMultiplier) < 0.1f;
            bool damageCorrect = std::abs(damageMultiplier - expectedMultiplier) < 0.1f;

            handler->SendSysMessage("=== 修复验证结果 ===");
            handler->PSendSysMessage("%s", ModuleOldFormat("血量倍率: %s", healthCorrect ? "[正确]" : "[错误]").c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("护甲倍率: %s", armorCorrect ? "[正确]" : "[错误]").c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("伤害倍率: %s", damageCorrect ? "[正确]" : "[错误]").c_str());

            if (healthCorrect && armorCorrect && damageCorrect)
            {
                handler->SendSysMessage(">> 倍率计算修复成功！所有属性倍率都正确。");
            }
            else
            {
                handler->SendSysMessage(">> 倍率计算仍有问题，需要进一步检查。");
            }
        }
        else
        {
            handler->SendSysMessage("未知的测试类型");
            return false;
        }

        return true;
    }

    // 刷新附近生物属性
    static bool HandleRefreshCommand(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        handler->SendSysMessage("正在刷新附近生物属性...");

        // 使用更可靠的方法查找附近生物
        std::list<Creature*> creatures;

        // 自定义检查器：获取范围内所有活着的生物
        class AllAliveCreaturesInRange
        {
        public:
            AllAliveCreaturesInRange(WorldObject const* object, float maxRange) : m_pObject(object), m_fRange(maxRange) {}
            bool operator() (Creature* creature)
            {
                return creature && creature->IsAlive() && m_pObject->IsWithinDist(creature, m_fRange, false);
            }
        private:
            WorldObject const* m_pObject;
            float m_fRange;
        };

        AllAliveCreaturesInRange check(player, 250.0f);
        Acore::CreatureListSearcher<AllAliveCreaturesInRange> searcher(player, creatures, check);
        Cell::VisitGridObjects(player, searcher, 250.0f);

        handler->PSendSysMessage("%s", ModuleOldFormat("找到 %s 个附近生物", creatures.size()).c_str());

        uint32 count = 0;
        for (Creature* creature : creatures)
        {
            if (creature && creature->IsAlive())
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("正在调整生物ID: %s (等级: %s)", creature->GetEntry(), creature->GetLevel()).c_str());
                sHuanJingSystem->AdjustCreatureStats(creature, player);
                count++;
            }
        }

        handler->PSendSysMessage("%s", ModuleOldFormat("已刷新 %s 个生物的属性", count).c_str());
        return true;
    }

    // 装备属性倍率测试命令
    static bool HandleItemMultiplierCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!*args)
        {
            handler->SendSysMessage("用法: .地图生物 装备倍率 <倍率> - 对当前装备的物品应用属性倍率");
            handler->SendSysMessage("例如: .地图生物 装备倍率 2 - 将装备属性提升2倍");
            handler->SendSysMessage("例如: .地图生物 装备倍率 10000 - 将装备属性提升10000倍");
            return true;
        }

        int multiplier = atoi(args);
        if (multiplier <= 0 || multiplier > 99999999)
        {
            handler->SendSysMessage("倍率必须在 1 到 99999999 之间的整数");
            return false;
        }

        // 获取玩家当前装备的物品
        Item* item = nullptr;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (equippedItem)
            {
                item = equippedItem;
                break; // 使用第一个找到的装备进行测试
            }
        }

        if (!item)
        {
            handler->SendSysMessage("请先装备一件物品进行测试");
            return false;
        }

        // 显示装备信息
        ItemTemplate const* itemTemplate = item->GetTemplate();
        handler->PSendSysMessage("%s", ModuleOldFormat("测试装备: %s (ID: %s)", itemTemplate->Name1, item->GetEntry()).c_str());

        // 显示原始属性
        handler->SendSysMessage("=== 原始属性 ===");
        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (itemTemplate->ItemStat[i].ItemStatType != 0 && itemTemplate->AsItemStatValue[i] > 0)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("属性类型 %s: %s", itemTemplate->ItemStat[i].ItemStatType, Acore::ToString(itemTemplate->AsItemStatValue[i])).c_str());
            }
        }
        if (itemTemplate->AsArmor > 0)
            handler->PSendSysMessage("%s", ModuleOldFormat("护甲: %s", FormatInt64(itemTemplate->AsArmor)).c_str());

        // 显示应用前的玩家属性
        handler->SendSysMessage("=== 应用前玩家属性 ===");
        handler->PSendSysMessage("%s", ModuleOldFormat("力量: %s (扩展: %s)", player->GetStat(STAT_STRENGTH), FormatInt64(player->GetAsRealStat(STAT_STRENGTH))).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("敏捷: %s (扩展: %s)", player->GetStat(STAT_AGILITY), FormatInt64(player->GetAsRealStat(STAT_AGILITY))).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("耐力: %s (扩展: %s)", player->GetStat(STAT_STAMINA), FormatInt64(player->GetAsRealStat(STAT_STAMINA))).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("智力: %s (扩展: %s)", player->GetStat(STAT_INTELLECT), FormatInt64(player->GetAsRealStat(STAT_INTELLECT))).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("精神: %s (扩展: %s)", player->GetStat(STAT_SPIRIT), FormatInt64(player->GetAsRealStat(STAT_SPIRIT))).c_str());

        // 记录装备是否已穿戴
        bool wasEquipped = item->IsEquipped();
        uint8 equipSlot = item->GetSlot();

        // 应用倍率
        sHuanJingSystem->ApplyItemAttributeMultiplier(item, (float)multiplier);

        handler->PSendSysMessage("%s", ModuleOldFormat("装备属性倍率 %s 倍应用成功！", multiplier).c_str());

        // 如果装备已穿戴，强制重新穿戴以刷新显示
        if (wasEquipped)
        {
            handler->SendSysMessage("正在刷新装备显示...");

            // 临时卸下装备
            player->_ApplyItemMods(item, equipSlot, false);

            // 等待一小段时间
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // 重新穿戴装备
            player->_ApplyItemMods(item, equipSlot, true);

            // 强制更新所有属性
            player->UpdateAllStats();

            handler->SendSysMessage("装备显示已刷新！");
        }

        // 显示倍率后的理论属性
        handler->SendSysMessage("=== 倍率后理论属性 ===");
        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (itemTemplate->ItemStat[i].ItemStatType != 0 && itemTemplate->AsItemStatValue[i] > 0)
            {
                AsInt64 originalValue = itemTemplate->AsItemStatValue[i];
                AsInt64 newValue = CalculateEnhancedAttributeValue(originalValue, float(multiplier), 'x');
                handler->PSendSysMessage("%s", ModuleOldFormat("属性类型 %s: %s -> %s", itemTemplate->ItemStat[i].ItemStatType, Acore::ToString(originalValue), Acore::ToString(newValue)).c_str());
            }
        }
        if (itemTemplate->AsArmor > 0)
        {
            AsInt64 newArmor = CalculateEnhancedAttributeValue(itemTemplate->AsArmor, float(multiplier), 'x');
            handler->PSendSysMessage("%s", ModuleOldFormat("护甲: %s -> %s", FormatInt64(itemTemplate->AsArmor), FormatInt64(newArmor)).c_str());
        }

        // 显示应用后的玩家属性
        handler->SendSysMessage("=== 应用后玩家属性 ===");
        handler->PSendSysMessage("%s", ModuleOldFormat("力量: %s (扩展: %s)", player->GetStat(STAT_STRENGTH), FormatInt64(player->GetAsRealStat(STAT_STRENGTH))).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("敏捷: %s (扩展: %s)", player->GetStat(STAT_AGILITY), FormatInt64(player->GetAsRealStat(STAT_AGILITY))).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("耐力: %s (扩展: %s)", player->GetStat(STAT_STAMINA), FormatInt64(player->GetAsRealStat(STAT_STAMINA))).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("智力: %s (扩展: %s)", player->GetStat(STAT_INTELLECT), FormatInt64(player->GetAsRealStat(STAT_INTELLECT))).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("精神: %s (扩展: %s)", player->GetStat(STAT_SPIRIT), FormatInt64(player->GetAsRealStat(STAT_SPIRIT))).c_str());

        handler->SendSysMessage("装备属性倍率已生效！请查看装备tooltip和角色属性变化。");

        return true;
    }

    // 清理无效装备数据命令
    static bool HandleCleanupItemsCommand(ChatHandler* handler, const char* /*args*/)
    {
        handler->SendSysMessage("开始清理无效装备数据...");

        // 强制执行清理
        sHuanJingSystem->CleanupInvalidItems();

        handler->SendSysMessage("装备数据清理完成！请查看服务器日志获取详细信息。");
        return true;
    }

    // 查询装备属性倍率命令（供客户端插件使用）
    static bool HandleQueryMultiplierCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            handler->SendSysMessage("错误：无法获取玩家信息");
            // 返回 true，避免框架再次打印通用的 USAGE 提示
            return true;
        }

        if (!*args)
        {
            handler->SendSysMessage("用法: .地图生物 查询倍率 <物品ID> <物品GUID>");
            // 已经给出中文用法提示，这里返回 true，阻止额外的英文错误信息
            return true;
        }

        // 解析参数
        std::string argsStr(args);
        std::istringstream iss(argsStr);
        uint32 itemId, itemGuid;

        if (!(iss >> itemId >> itemGuid))
        {
            handler->SendSysMessage("参数格式错误，用法: .地图生物 查询倍率 <物品ID> <物品GUID>");
            // 参数错误时同样只显示自定义提示
            return true;
        }

        // 获取玩家的幻境等级
        uint32 huanJingLevel = sHuanJingSystem->GetPlayerHuanJingLevel(player);

        // 首先尝试从装备增强数据表中查询具体的装备倍率和属性数据
        int multiplier = 1;
        char multiplierMode = 'x';
        std::string attributeData = "";
        bool foundSpecificData = false;

        // 查询装备增强数据表
        QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT 属性倍率, 属性倍率模式, 增强属性数据 FROM 玩家装备属性增强 WHERE 玩家GUID = %s AND 装备GUID = %s AND 装备ID = %s", player->GetGUID().GetCounter(), itemGuid, itemId).c_str());

        if (result)
        {
            Field* fields = result->Fetch();
            multiplier = fields[0].Get<int>();
            multiplierMode = DbValueToItemAttributeMode(fields[1].Get<int32>());
            attributeData = fields[2].Get<std::string>();
            foundSpecificData = true;
        }
        else
        {
            multiplier = 1;
            multiplierMode = 'x';
            attributeData = "";
        }

        // 查询鉴定系统的自定义属性数据
        std::string identificationData = "";
#ifdef MODULE_ITEM_IDENTIFICATION
        if (sItemIdentificationSystem && sItemIdentificationSystem->_enabled)
        {
            // 使用鉴定系统的批量查询接口获取基础属性和追加属性
            ItemIdentificationSystem::AllModuleData moduleData =
                sItemIdentificationSystem->QueryAllModuleData(itemId, itemGuid);

            std::vector<std::string> identAttrs;

            auto parseAttributes = [&](const std::string& attrsStr)
            {
                if (attrsStr.empty())
                    return;

                std::istringstream stream(attrsStr);
                std::string attrPair;
                while (std::getline(stream, attrPair, ','))
                {
                    std::istringstream pairStream(attrPair);
                    uint32 attrType;
                    AsInt64 attrValue;
                    if (pairStream >> attrType >> attrValue)
                    {
                        // 计算增强后的值（应用幻境倍率）
                        AsInt64 enhancedValue = CalculateEnhancedAttributeValue(attrValue, multiplier, multiplierMode);
                        // 格式：属性类型 原值 增强值
                        identAttrs.push_back(Acore::ToString(attrType) + " " +
                                             Acore::ToString(attrValue) + " " +
                                             Acore::ToString(enhancedValue));
                    }
                }
            };

            // 基础属性与追加属性都应用幻境倍率
            parseAttributes(moduleData.baseAttributes);
            parseAttributes(moduleData.additionalAttributes);

            // 用逗号连接所有鉴定属性
            if (!identAttrs.empty())
            {
                for (size_t i = 0; i < identAttrs.size(); ++i)
                {
                    if (i > 0)
                        identificationData += ",";
                    identificationData += identAttrs[i];
                }
            }
        }
#endif

        // 发送响应消息（格式化为客户端插件可以解析的格式）
        // 格式：[幻境系统] 装备ID:<id> GUID:<guid> 属性模式:<mode> 属性倍率:<multiplier> 属性数据:<data> 鉴定数据:<identData>
        std::string response = "[幻境系统] 装备ID:" + Acore::ToString(itemId) +
                              " GUID:" + Acore::ToString(itemGuid) +
                              " 属性模式:" + std::string(1, multiplierMode) +
                              " 属性倍率:" + Acore::ToString(multiplier);

        // 如果有官方属性数据，添加到响应中
        if (!attributeData.empty())
        {
            response += " 属性数据:" + attributeData;
        }

        // 如果有鉴定属性数据，添加到响应中
        if (!identificationData.empty())
        {
            response += " 鉴定数据:" + identificationData;
        }

        handler->SendSysMessage(response.c_str());

        return true;
    }

    // 检查生物调整状态命令
    static bool HandleCheckCreatureCommand(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            handler->SendSysMessage("错误：无法获取玩家信息");
            return false;
        }

        // 获取玩家目标
        Unit* target = player->GetSelectedUnit();
        if (!target)
        {
            handler->SendSysMessage("请先选择一个目标生物");
            return false;
        }

        Creature* creature = target->ToCreature();
        if (!creature)
        {
            handler->SendSysMessage("目标不是生物");
            return false;
        }

        uint32 creatureGuid = creature->GetGUID().GetCounter();
        uint32 creatureId = creature->GetEntry();
        uint32 playerHuanJingLevel = sHuanJingSystem->GetPlayerHuanJingLevel(player);

        handler->PSendSysMessage("=== 生物调整状态检查 ===");
        handler->PSendSysMessage("%s", ModuleOldFormat("生物ID: %s", creatureId).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("生物GUID: %s", creatureGuid).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("生物名称: %s", creature->GetName()).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("当前等级: %s", creature->GetLevel()).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("当前血量: %s", creature->GetMaxHealth()).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("当前法力: %s", creature->GetMaxPower(POWER_MANA)).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("地图ID: %s", creature->GetMapId()).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("区域ID: %s", creature->GetAreaId()).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("难度: %s", sHuanJingSystem->GetCreatureMapDifficulty(creature)).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("生物类型: %s", sHuanJingSystem->GetCreatureTypeByEntry(creatureId)).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("玩家幻境等级: %s", playerHuanJingLevel).c_str());

        // 检查是否被标记
        bool isMarked = sHuanJingSystem->IsCreatureMarked(creature);
        handler->PSendSysMessage("%s", ModuleOldFormat("是否已标记: %s", isMarked ? "是" : "否").c_str());

        if (isMarked)
        {
            // 获取标记的等级
            uint64 creatureKey = GenerateCreatureKey(creature);
            auto& adjustedLevels = sHuanJingSystem->_creatureAdjustedLevels;
            auto it = adjustedLevels.find(creatureKey);
            if (it != adjustedLevels.end())
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("标记的幻境等级: %s", it->second).c_str());
            }
        }

        // 检查是否正在调整
        bool isBeingAdjusted = sHuanJingSystem->IsCreatureBeingAdjusted(creature);
        handler->PSendSysMessage("%s", ModuleOldFormat("是否正在调整: %s", isBeingAdjusted ? "是" : "否").c_str());

        // 检查附近最高幻境等级
        uint32 nearbyHighestLevel = sHuanJingSystem->GetHighestHuanJingLevelNearCreature(creature, 100.0f);
        handler->PSendSysMessage("%s", ModuleOldFormat("附近最高幻境等级: %s", nearbyHighestLevel).c_str());

        // 尝试查找匹配的配置
        HuanJingCreatureConfig const* config = sHuanJingSystem->GetCreatureConfig(creature, player);

        if (config)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("找到匹配配置: ID %s", config->configId).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("配置血量倍率: %s", config->healthMultiplier).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("配置血量固定值: %s", FormatUInt64(config->healthValue)).c_str());
        }
        else
        {
            handler->SendSysMessage("未找到匹配的配置");
        }

        // 手动触发调整
        handler->SendSysMessage("正在手动触发属性调整...");
        sHuanJingSystem->AdjustCreatureStats(creature, player);
        handler->SendSysMessage("属性调整完成");

        return true;
    }

    // 清理副本生物调整数据命令
    static bool HandleCleanupInstanceCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 获取当前地图和实例ID
        uint32 mapId = player->GetMapId();
        uint32 instanceId = player->GetInstanceId();

        handler->PSendSysMessage("%s", ModuleOldFormat("正在清理地图 %s 实例 %s 的生物调整数据...", mapId, instanceId).c_str());

        // 调用清理方法
        sHuanJingSystem->CleanupInstanceCreatureAdjustments(mapId, instanceId);

        handler->PSendSysMessage("清理完成！已重置该副本所有生物的幻境调整数据。");
        handler->PSendSysMessage("提示：生物将在下次被玩家攻击时重新按当前玩家的幻境等级调整。");

        return true;
    }

    // 血条系统测试命令
    static bool HandleHealthBarTestCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        Creature* target = handler->getSelectedCreature();
        if (!target)
        {
            handler->SendSysMessage("请先选择一个生物");
            return false;
        }

        // 获取血条信息（在锁内拷贝一份到本地，避免返回悬空指针）
        HealthBarData dataCopy;
        bool hasData = false;
        {
            std::lock_guard<std::mutex> lock(sHuanJingSystem->_healthBarMutex);
            auto it = sHuanJingSystem->_healthBarData.find(GenerateCreatureKey(target));
            if (it != sHuanJingSystem->_healthBarData.end() && it->second.enabled)
            {
                dataCopy = it->second;
                hasData = true;
            }
        }

        if (!hasData)
        {
            handler->SendSysMessage("该生物未启用血条系统");
            return false;
        }

        NormalizeHealthBarData(dataCopy);

        // 显示血条信息
        handler->PSendSysMessage("血条信息:");
        handler->PSendSysMessage("%s", ModuleOldFormat("- 总血条数: %s", dataCopy.totalBars).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("- 当前血条: %s", dataCopy.currentBar).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("- 每血条血量: %s", FormatUInt64(dataCopy.healthPerBar)).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("- 总血量(诊断): %s", FormatUInt64(dataCopy.totalHealth)).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("- 当前条血量: %s", FormatUInt64(dataCopy.currentHealth)).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("- 血条显示: %s", sHuanJingSystem->GetHealthBarDisplay(target)).c_str());

        return true;
    }
};

// 世界脚本 - 处理服务器启动后的延迟初始化
class HuanJingWorldScript : public WorldScript
{
public:
    HuanJingWorldScript() :
        WorldScript("HuanJingWorldScript"),
        _initialized(false),
        _loadTimer(0),
        _delayTime(2 * IN_MILLISECONDS) // 2秒延迟，确保服务器完全启动
    { }

    void OnAfterConfigLoad(bool reload) override
    {
        if (!reload)
        {
            // 配置加载后不立即初始化，等待延迟
        }
    }

    void OnUpdate(uint32 diff) override
    {
        // 延迟加载数据
        if (!_initialized)
        {
            _loadTimer += diff;

            if (_loadTimer >= _delayTime)
            {
                // 执行幻境系统初始化
                sHuanJingSystem->Initialize();

                if (sHelper->GetBoolean(22010))
                    LOG_INFO("server.loading", "→_模板_地图生物控制 加载完成√");
                else
                    LOG_INFO("server.loading", "→_模板_地图生物控制 已关闭");

                _initialized = true;
            }
        }
    }

private:
    bool _initialized;     // 是否已初始化
    uint32 _loadTimer;     // 计时器
    uint32 _delayTime;     // 延迟时间
};

// 按指定玩家的幻境等级单体调整生物属性：
// - 只有玩家可攻击的生物才会被幻境系统影响；
// - 非战斗中按当前触发玩家等级调整/重调，0级也会还原；
// - 已经处于战斗中的生物不允许被后来选中或参战的高等级玩家改写等级；
// - 刚进入战斗的初始攻击者仍按自己的幻境等级覆盖旧的非战斗残留。
void HuanJingSystem::AdjustCreatureStatsForPlayer(Creature* creature, Player* player, bool allowInitialCombatAdjust, Unit* combatVictim)
{
    if (!sHelper->GetBoolean(22010))
        return;

    if (!creature || !player)
        return;

    uint32 huanJingLevel = GetPlayerHuanJingLevel(player);
    uint64 creatureKey = GenerateCreatureKey(creature);
    uint32 currentAdjustedLevel = GetCreatureAdjustedHuanJingLevel(creature);
    bool inCombat = creature->IsInCombat();
    bool isInitialCombatOwner = allowInitialCombatAdjust && IsCreaturePrimaryCombatTargetForPlayer(creature, player, combatVictim);

    if (HasCreatureAttributeOverride(creature))
    {
        if (currentAdjustedLevel > 0 && !inCombat)
            RestoreCreatureToOriginalStats(creature);
        else
        {
            CleanupCreatureAdjustmentData(creature);
            ClearCreatureVirtualMana(creature->GetGUID());
            ClearCreatureSpellPower(creature);
        }

        EnsureCreatureHealthBarsFromConfig(creature, player, false);
        LOG_DEBUG("module.huanjing", ModuleOldFormat(">> [幻境系统] 生物 %s 已由 _属性调整_生物 接管基础属性，跳过按玩家幻境等级调整；血条仍由幻境血条系统处理", creature->GetEntry()).c_str());
        return;
    }

    if (!IsHuanJingCreatureEligibleForPlayer(creature, player))
    {
        uint32 nearbyHighestLevel = currentAdjustedLevel > 0
            ? GetHighestHuanJingLevelNearCreature(creature, CREATURE_ADJUSTMENT_RECHECK_RANGE)
            : 0;

        if (currentAdjustedLevel > 0 && nearbyHighestLevel == 0)
        {
            RestoreCreatureToOriginalStats(creature);
        }
        return;
    }

    if (huanJingLevel == 0)
    {
        if (currentAdjustedLevel > 0)
        {
            if (!inCombat || isInitialCombatOwner)
            {
                RestoreCreatureToOriginalStats(creature);
            }
            else
            {
                if (ShouldLogCombatProtection(creature, player, currentAdjustedLevel, 0))
                {
                    SendCreatureHuanJingBroadcastToPlayer(player, creature, "CombatProtection");
                }
            }
        }
        return; // 0级玩家会在允许覆盖时还原，不再按附近最高等级保留旧倍率
    }

    if (inCombat && !isInitialCombatOwner)
    {
        if (currentAdjustedLevel == 0)
        {
            if (ShouldLogCombatProtection(creature, player, currentAdjustedLevel, huanJingLevel))
            {
                SendCreatureHuanJingBroadcastToPlayer(player, creature, "CombatProtection");
            }
            return;
        }

        if (currentAdjustedLevel != huanJingLevel)
        {
            if (ShouldLogCombatProtection(creature, player, currentAdjustedLevel, huanJingLevel))
            {
                SendCreatureHuanJingBroadcastToPlayer(player, creature, "CombatProtection");
            }
        }
        return;
    }

    if (!CreatureUsesDedicatedStats(creature, player))
    {
        if (!inCombat && currentAdjustedLevel > 0)
            RestoreCreatureToOriginalStats(creature);
        return;
    }

    bool shouldApplyAdjustment = false;

    if (currentAdjustedLevel == 0)
    {
        shouldApplyAdjustment = true;
    }
    else if (currentAdjustedLevel != huanJingLevel)
    {
        shouldApplyAdjustment = true;
    }

    if (!shouldApplyAdjustment)
    {
        HuanJingCreatureConfig const* dynamicConfig = GetMapCreatureConfig(creature, player);
        // 动态难度只允许在非战斗状态重算；首次进入战斗由
        // HuanJingSystemPlayerScript::OnPlayerEnterCombat 单独完成初始化。
        if (dynamicConfig && dynamicConfig->dynamicDifficultyEnabled && !inCombat)
            shouldApplyAdjustment = true;
    }

    if (!shouldApplyAdjustment)
    {
        // 等级相同无需重新调整属性。已有多倍血条时不能重新初始化，否则会把 currentBar/currentHealth 重置满。
        uint32 creatureId = creature->GetEntry();
        uint32 mapId = creature->GetMapId();
        uint32 areaId = creature->GetAreaId();
        uint32 difficulty = GetCreatureMapDifficulty(creature);
        uint32 creatureLevel = creature->GetLevel();
        uint32 creatureType = GetCreatureTypeByEntry(creatureId);
        HuanJingCreatureConfig const* sameLevelConfig = GetCreatureConfig(creature, player);
        if (sameLevelConfig && sameLevelConfig->healthBarCount > 1)
        {
            AsInt64 healthPerBar = sameLevelConfig->healthValue;
            if (healthPerBar == 0 && sameLevelConfig->healthMultiplier != 1.0f)
            {
                if (CreatureTemplate const* cInfo = creature->GetCreatureTemplate())
                {
                    if (CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(creature->GetLevel(), cInfo->unit_class))
                    {
                        long double contentDifficultyScale = CalculateCreatureContentDifficultyScale(creature);
                        long double scaledHealthMultiplier = 1.0L + (static_cast<long double>(sameLevelConfig->healthMultiplier) - 1.0L) * contentDifficultyScale;
                        healthPerBar = SaturateToUInt64(Acore::Number::ToLongDouble(ToUInt64FromAsInt64(stats->GenerateAsHealth(cInfo))) * scaledHealthMultiplier);
                    }
                }
            }

            bool shouldInitializeHealthBars = true;
            {
                std::lock_guard<std::mutex> lock(_healthBarMutex);
                auto it = _healthBarData.find(GenerateCreatureKey(creature));
                if (it != _healthBarData.end() && it->second.enabled)
                {
                    NormalizeHealthBarData(it->second);
                    shouldInitializeHealthBars = it->second.totalBars != sameLevelConfig->healthBarCount || it->second.healthPerBar == 0;
                }
            }

            if (shouldInitializeHealthBars)
            {
                InitializeCreatureHealthBars(creature, sameLevelConfig->healthBarCount, ResolveCreatureServerHealthForHealthBar(creature, healthPerBar));
            }
            else
            {
                SyncHealthBarDataWithCreatureMaxHealth(creature);
            }
        }

        LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 已经是幻境等级 %s，无需重新调整", creature->GetName(), huanJingLevel).c_str());
        return;
    }

    // 使用现有的基础调整逻辑：内部会先还原生物到原始状态，再按该玩家等级重新应用倍率
    AdjustCreatureStats(creature, player, inCombat && isInitialCombatOwner);

    // 记录新的调整等级
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        _creatureAdjustedLevels[creatureKey] = huanJingLevel;
    }

    // 同步更新鉴定组ID缓存，供后续掉落与自动鉴定使用
    uint32 creatureId = creature->GetEntry();
    uint32 mapId = creature->GetMapId();
    uint32 areaId = creature->GetAreaId();
    uint32 difficulty = GetCreatureMapDifficulty(creature);
    uint32 creatureLevel = creature->GetLevel();
    uint32 creatureType = creature->IsDungeonBoss() ? 3 : GetCreatureTypeByEntry(creatureId);

    HuanJingCreatureConfig const* config = GetCreatureConfig(creature, player);

    if (config && config->identificationGroup > 0)
    {
        ObjectGuid guid = creature->GetGUID();
        std::lock_guard<std::mutex> lock(_creatureMutex);
        _creatureDeathIdentificationGroup[guid] = { config->identificationGroup, ResolveConfiguredItemAttributeMode(config->equipAttrValue, config->equipAttrMode), config->identificationQuality, config->identificationItemLevel, config->equipAttrValue, config->equipAttrMultiplier };
    }
}

// 装备属性增强数据库操作函数
void HuanJingSystem::SaveItemAttributeEnhancement(uint32 playerGuid, uint32 itemGuid, uint32 itemId, int multiplier, char multiplierMode, const std::string& attributeData, uint32 identificationGroupId)
{
    multiplierMode = NormalizeItemAttributeMode(multiplierMode);
    int32 dbModeValue = ItemAttributeModeToDbValue(multiplierMode);
    std::string escapedAttributeData = attributeData;
    CharacterDatabase.EscapeString(escapedAttributeData);

    // 插入新记录或更新已存在的记录
    // 鉴定成功后会立即查询该表刷新客户端提示框，这里必须同步写入，避免小退后才可见。
    CharacterDatabase.DirectExecute(ModuleOldFormat("INSERT INTO `玩家装备属性增强` (`玩家GUID`, `装备GUID`, `装备ID`, `属性倍率`, `属性倍率模式`, `增强属性数据`, `鉴定组ID`) "
        "VALUES (%s, %s, %s, %s, %s, '%s', %s) "
        "ON DUPLICATE KEY UPDATE "
        "`属性倍率` = VALUES(`属性倍率`), "
        "`属性倍率模式` = VALUES(`属性倍率模式`), "
        "`增强属性数据` = VALUES(`增强属性数据`), "
        "`鉴定组ID` = VALUES(`鉴定组ID`)", playerGuid, itemGuid, itemId, multiplier, dbModeValue, escapedAttributeData, identificationGroupId).c_str());

}

ItemAttributeEnhancement* HuanJingSystem::GetItemAttributeEnhancement(uint32 itemGuid)
{
    std::lock_guard<std::mutex> lock(_itemMutex);
    auto it = _itemEnhancements.find(itemGuid);
    if (it != _itemEnhancements.end())
    {
        return &it->second;
    }
    return nullptr;
}

void HuanJingSystem::UpdateItemIdentificationGroup(uint32 itemGuid, uint32 identificationGroupId)
{
    std::lock_guard<std::mutex> lock(_itemMutex);
    auto it = _itemEnhancements.find(itemGuid);
    if (it != _itemEnhancements.end())
    {
        it->second.identificationGroupId = identificationGroupId;
    }
}

void HuanJingSystem::LoadItemAttributeEnhancements(uint32 playerGuid)
{
    QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT `记录ID`, `玩家GUID`, `装备GUID`, `装备ID`, `属性倍率`, `属性倍率模式`, `增强属性数据`, `鉴定组ID` "
        "FROM `玩家装备属性增强` WHERE `玩家GUID` = %s", playerGuid).c_str());

    if (!result)
        return;

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        ItemAttributeEnhancement enhancement;
        enhancement.recordId = fields[0].Get<uint32>();
        enhancement.playerGuid = fields[1].Get<uint32>();
        enhancement.itemGuid = fields[2].Get<uint32>();
        enhancement.itemId = fields[3].Get<uint32>();
        enhancement.multiplier = fields[4].Get<int>();
        int32 dbModeValue = fields[5].Get<int32>();
        enhancement.multiplierMode = DbValueToItemAttributeMode(dbModeValue);
        enhancement.attributeData = fields[6].Get<std::string>();
        enhancement.identificationGroupId = fields[7].Get<uint32>();

        {
            std::lock_guard<std::mutex> lock(_itemMutex);
            _itemEnhancements[enhancement.itemGuid] = enhancement;
        }
        count++;

    } while (result->NextRow());

}

void HuanJingSystem::DeleteItemAttributeEnhancement(uint32 itemGuid)
{
    // 使用异步Execute避免死锁（多玩家同时操作时可能产生数据库锁竞争）
    CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `玩家装备属性增强` WHERE `装备GUID` = %s", itemGuid).c_str());
}

std::string HuanJingSystem::BuildAttributeDataString(Item* item, int multiplier, char multiplierMode)
{
    if (!item)
        return "";

    multiplierMode = NormalizeItemAttributeMode(multiplierMode);

    const ItemTemplate* itemTemplate = item->GetTemplate();
    if (!itemTemplate)
        return "";

    std::string attributeData;

    // 处理模板属性；鉴定基础属性存在时由物品属性模块替换，不写入幻境增强记录。
    bool applyTemplateStats = !HasIdentificationBaseAttributes(item);
    if (applyTemplateStats)
    {
        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (itemTemplate->ItemStat[i].ItemStatType != 0 && itemTemplate->AsItemStatValue[i] > 0)
            {
                uint32 statType = itemTemplate->ItemStat[i].ItemStatType;
                AsInt64 originalValue = itemTemplate->AsItemStatValue[i];
                AsInt64 enhancedValue = CalculateEnhancedAttributeValue(originalValue, multiplier, multiplierMode);

                if (!attributeData.empty())
                    attributeData += ",";

                attributeData += Acore::ToString(statType) + " " + Acore::ToString(originalValue) + " " + Acore::ToString(enhancedValue);
            }
        }
    }

    // 处理护甲值
    if (itemTemplate->AsArmor > 0)
    {
        AsInt64 originalArmor = itemTemplate->AsArmor;
        AsInt64 enhancedArmor = CalculateEnhancedAttributeValue(originalArmor, multiplier, multiplierMode);

        if (!attributeData.empty())
            attributeData += ",";

        attributeData += "armor " + FormatInt64(originalArmor) + " " + FormatInt64(enhancedArmor);
    }

    // 处理抗性 - 在AzerothCore中使用单独的字段
    // 处理神圣抗性
    if (itemTemplate->HolyRes > 0)
    {
        AsInt64 originalResist = itemTemplate->HolyRes;
        AsInt64 enhancedResist = CalculateEnhancedAttributeValue(originalResist, multiplier, multiplierMode);

        if (!attributeData.empty())
            attributeData += ",";

        attributeData += "resist0:" + Acore::ToString(enhancedResist);
    }

    // 处理火焰抗性
    if (itemTemplate->FireRes > 0)
    {
        AsInt64 originalResist = itemTemplate->FireRes;
        AsInt64 enhancedResist = CalculateEnhancedAttributeValue(originalResist, multiplier, multiplierMode);

        if (!attributeData.empty())
            attributeData += ",";

        attributeData += "resist1:" + Acore::ToString(enhancedResist);
    }

    // 处理自然抗性
    if (itemTemplate->NatureRes > 0)
    {
        AsInt64 originalResist = itemTemplate->NatureRes;
        AsInt64 enhancedResist = CalculateEnhancedAttributeValue(originalResist, multiplier, multiplierMode);

        if (!attributeData.empty())
            attributeData += ",";

        attributeData += "resist2:" + Acore::ToString(enhancedResist);
    }

    // 处理冰霜抗性
    if (itemTemplate->FrostRes > 0)
    {
        AsInt64 originalResist = itemTemplate->FrostRes;
        AsInt64 enhancedResist = CalculateEnhancedAttributeValue(originalResist, multiplier, multiplierMode);

        if (!attributeData.empty())
            attributeData += ",";

        attributeData += "resist3:" + Acore::ToString(enhancedResist);
    }

    // 处理暗影抗性
    if (itemTemplate->ShadowRes > 0)
    {
        AsInt64 originalResist = itemTemplate->ShadowRes;
        AsInt64 enhancedResist = CalculateEnhancedAttributeValue(originalResist, multiplier, multiplierMode);

        if (!attributeData.empty())
            attributeData += ",";

        attributeData += "resist4:" + Acore::ToString(enhancedResist);
    }

    // 处理奥术抗性
    if (itemTemplate->ArcaneRes > 0)
    {
        AsInt64 originalResist = itemTemplate->ArcaneRes;
        AsInt64 enhancedResist = CalculateEnhancedAttributeValue(originalResist, multiplier, multiplierMode);

        if (!attributeData.empty())
            attributeData += ",";

        attributeData += "resist5:" + Acore::ToString(enhancedResist);
    }

    return attributeData;
}

void HuanJingSystem::CleanupUnequippedItemEnhancements(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    std::vector<uint32> playerItemGuids;

    // 先在锁内收集当前玩家关联的所有装备GUID
    {
        std::lock_guard<std::mutex> lock(_itemMutex);
        for (auto const& pair : _itemEnhancements)
        {
            uint32 itemGuid = pair.first;
            ItemAttributeEnhancement const& enhancement = pair.second;
            if (enhancement.playerGuid == playerGuid)
                playerItemGuids.push_back(itemGuid);
        }
    }

    std::vector<uint32> itemsToCleanup;

    auto findItemByGuid = [&](uint32 searchGuid) -> Item*
    {
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (equippedItem && equippedItem->GetGUID().GetCounter() == searchGuid)
                return equippedItem;
        }

        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            Item* backpackItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (backpackItem && backpackItem->GetGUID().GetCounter() == searchGuid)
                return backpackItem;
        }

        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        {
            if (Bag* bagPtr = player->GetBagByPos(bag))
            {
                for (uint32 slot = 0; slot < bagPtr->GetBagSize(); ++slot)
                {
                    Item* bagItem = bagPtr->GetItemByPos(slot);
                    if (bagItem && bagItem->GetGUID().GetCounter() == searchGuid)
                        return bagItem;
                }
            }
        }

        for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
        {
            Item* bankItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (bankItem && bankItem->GetGUID().GetCounter() == searchGuid)
                return bankItem;
        }

        for (uint8 bag = BANK_SLOT_BAG_START; bag < BANK_SLOT_BAG_END; ++bag)
        {
            if (Bag* bankBag = player->GetBagByPos(bag))
            {
                for (uint32 slot = 0; slot < bankBag->GetBagSize(); ++slot)
                {
                    Item* bagItem = bankBag->GetItemByPos(slot);
                    if (bagItem && bagItem->GetGUID().GetCounter() == searchGuid)
                        return bagItem;
                }
            }
        }

        return nullptr;
    };

    // 装备 GUID 是否仍被任意有效系统引用（邮件/拍卖/公会银行/退款/包装/装备方案/飞升虚拟背包/character_inventory）。
    // 之前只搜玩家身上的 6 类容器，导致玩家上次下线时把装备放邮件、上架拍卖、留在商人回购槽里，
    // 重登瞬间这里就把数据库中的倍率行删掉，玩家会反映"重启后装备倍率丢了"。
    // 使用一条覆盖所有持久化位置的 SQL 做存在性判定，未来再扩展只需补 LEFT JOIN。
    auto isItemReferenced = [&](uint32 searchGuid) -> bool
    {
        QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT 1 FROM `item_instance` ii "
            "LEFT JOIN `character_inventory` ci ON ci.`item` = ii.`guid` "
            "LEFT JOIN `mail_items` mi ON mi.`item_guid` = ii.`guid` "
            "LEFT JOIN `auctionhouse` ah ON ah.`itemguid` = ii.`guid` "
            "LEFT JOIN `guild_bank_item` gbi ON gbi.`item_guid` = ii.`guid` "
            "LEFT JOIN `item_refund_instance` iri ON iri.`item_guid` = ii.`guid` "
            "LEFT JOIN `item_loot_storage` ils ON ils.`containerGUID` = ii.`guid` "
            "LEFT JOIN `character_gifts` cg ON cg.`item_guid` = ii.`guid` "
            "LEFT JOIN `character_equipmentsets` ces ON ii.`guid` IN ("
            "ces.`item0`, ces.`item1`, ces.`item2`, ces.`item3`, ces.`item4`, ces.`item5`, "
            "ces.`item6`, ces.`item7`, ces.`item8`, ces.`item9`, ces.`item10`, ces.`item11`, "
            "ces.`item12`, ces.`item13`, ces.`item14`, ces.`item15`, ces.`item16`, ces.`item17`, "
            "ces.`item18`) "
            "WHERE ii.`guid` = %s AND ("
            "ci.`item` IS NOT NULL OR mi.`item_guid` IS NOT NULL OR ah.`itemguid` IS NOT NULL OR "
            "gbi.`item_guid` IS NOT NULL OR iri.`item_guid` IS NOT NULL OR "
            "ils.`containerGUID` IS NOT NULL OR cg.`item_guid` IS NOT NULL OR "
            "ces.`setguid` IS NOT NULL"
            ") LIMIT 1", searchGuid).c_str());

        return static_cast<bool>(result);
    };

    // 在锁外检查这些GUID对应的物品是否还存在
    for (uint32 itemGuid : playerItemGuids)
    {
        Item* item = findItemByGuid(itemGuid);
        if (item)
            continue;

        // 内存中找不到不一定代表物品已经消失：可能在邮件、拍卖行、公会银行、回购、装备方案等位置。
        // 必须确认 `item_instance` 已不存在或不再被任何有效持久化位置引用，才允许删除增强行。
        if (isItemReferenced(itemGuid))
            continue;

        itemsToCleanup.push_back(itemGuid);
    }

    // 清理标记的装备
    for (uint32 itemGuid : itemsToCleanup)
    {
        // 从数据库删除
        DeleteItemAttributeEnhancement(itemGuid);

        // 从内存移除
        {
            std::lock_guard<std::mutex> lock(_itemMutex);
            _itemEnhancements.erase(itemGuid);
        }
    }
}

// 物品移除事件处理脚本 - 使用AllItemScript来处理所有物品的移除事件
class HuanJingSystemAllItemScript : public AllItemScript
{
public:
    HuanJingSystemAllItemScript() : AllItemScript("HuanJingSystemAllItemScript") { }

    bool CanItemRemove(Player* player, Item* item) override
    {
        // 不在这里处理幻境增强效果的移除，因为这个钩子在装备实际移除前被调用
        // 幻境增强效果的移除应该在装备实际移除后进行，避免时机问题
        return true; // 允许移除物品
    }
};

//=============================================================================
// NPC商店购买需求检查功能实现
//=============================================================================

uint32 HuanJingSystem::GetPurchaseNeedTemplateId(Player* player)
{
    if (!player)
        return 0;

    LOG_DEBUG("module.huanjing", ModuleOldFormat("GetPurchaseNeedTemplateId: 玩家=%s 幻境等级配置已移除，需求模板id=0", player->GetName()).c_str());
    return 0;
}

bool HuanJingSystem::CheckVendorItemRequirements(Player* player)
{
    if (!player)
        return false;

    // 获取玩家幻境等级对应的需求模板ID
    uint32 needTemplateId = GetPurchaseNeedTemplateId(player);

    if (needTemplateId == 0)
    {
        // 需求模板ID为0，表示官方原版逻辑，允许购买
        return true;
    }

    if (!HasNeedTemplate(needTemplateId))
    {
        LOG_ERROR("module.huanjing", ModuleOldFormat("CheckVendorItemRequirements: 需求模板不存在, 玩家=%s 幻境等级=%s 需求模板id=%s", player->GetName(), GetPlayerHuanJingLevel(player), needTemplateId).c_str());
        return false;
    }

    bool ok = CheckNeedTemplate(player, needTemplateId, false);
    LOG_DEBUG("module.huanjing", ModuleOldFormat("CheckVendorItemRequirements: 玩家=%s 幻境等级=%s 需求模板id=%s 检查结果=%s", player->GetName(), GetPlayerHuanJingLevel(player), needTemplateId, ok ? "通过" : "未通过").c_str());
    return ok;
}

std::string HuanJingSystem::GetVendorItemRequirementMessage(Player* player)
{
    if (!player)
        return "系统错误：无法获取玩家信息";

    uint32 playerHuanJingLevel = GetPlayerHuanJingLevel(player);
    uint32 needTemplateId = GetPurchaseNeedTemplateId(player);

    if (needTemplateId == 0)
    {
        return "您的幻境等级 " + Acore::ToString(playerHuanJingLevel) + " 可以正常购买商品";
    }

    return "您的幻境等级 " + Acore::ToString(playerHuanJingLevel) + " 不满足购买条件：" + sHelper->GetNeedGts(needTemplateId);
}

// 幻境系统Addon消息处理脚本
class HuanJingAddonScript : public PlayerScript
{
public:
    HuanJingAddonScript() : PlayerScript("HuanJingAddonScript") { }

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        // 只处理Addon私聊消息
        if (type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
        {
            return;
        }

        // 客户端发送格式："UITQ<TAB>HUANJING_QUERY:itemID:guid"
        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
        {
            return;
        }

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != "UITQ")
        {
            return;
        }

        std::string command = msg.substr(tabPos + 1);
        if (command.find("HUANJING_QUERY:") != 0)
        {
            return;
        }

        std::string params = command.substr(strlen("HUANJING_QUERY:"));
        size_t colonPos = params.find(':');
        if (colonPos == std::string::npos)
        {
            return;
        }

        uint32 itemId = 0;
        uint32 itemGuid = 0;

        try
        {
            itemId = static_cast<uint32>(std::stoul(params.substr(0, colonPos)));
            itemGuid = static_cast<uint32>(std::stoul(params.substr(colonPos + 1)));
        }
        catch (...)
        {
            return;
        }

        if (!itemId || !itemGuid)
        {
            return;
        }

        HandleAddonQueryMultiplier(player, itemId, itemGuid);
    }

private:
    // 【修复】辅助函数：在玩家背包和装备中查找指定itemID的物品的真实GUID
    std::vector<uint32> FindItemGuidsByItemId(Player* player, uint32 itemID)
    {
        std::vector<uint32> guids;

        if (!player)
            return guids;

        // 1. 搜索装备栏
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (item && item->GetEntry() == itemID)
            {
                guids.push_back(item->GetGUID().GetCounter());
            }
        }

        // 2. 搜索主背包
        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (item && item->GetEntry() == itemID)
            {
                guids.push_back(item->GetGUID().GetCounter());
            }
        }

        // 3. 搜索背包袋
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* bag = player->GetBagByPos(i))
            {
                for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                {
                    Item* item = bag->GetItemByPos(j);
                    if (item && item->GetEntry() == itemID)
                    {
                        guids.push_back(item->GetGUID().GetCounter());
                    }
                }
            }
        }

        return guids;
    }

    void HandleAddonQueryMultiplier(Player* player, uint32 itemId, uint32 itemGuid)
    {
        if (!player)
        {
            return;
        }

        int multiplier = 1;
        char multiplierMode = 'x';
        std::string attributeData;
        uint32 usedGuid = itemGuid;  // 记录最终使用的GUID

        // 查询装备增强数据表 - 移除玩家GUID限制，因为装备GUID是唯一的
        // 这样可以查询到其他玩家的装备数据（用于检查其他玩家时显示倍率属性）
        QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT 属性倍率, 属性倍率模式, 增强属性数据 FROM 玩家装备属性增强 WHERE 装备GUID = %s AND 装备ID = %s", itemGuid, itemId).c_str());

        // 【修复】如果使用客户端GUID查询不到数据，尝试使用真实GUID重新查询
        if (!result)
        {
            std::vector<uint32> realGuids = FindItemGuidsByItemId(player, itemId);

            for (uint32 realGuid : realGuids)
            {
                if (realGuid != itemGuid)  // 跳过已经查询过的GUID
                {
                    result = CharacterDatabase.Query(ModuleOldFormat("SELECT 属性倍率, 属性倍率模式, 增强属性数据 FROM 玩家装备属性增强 WHERE 装备GUID = %s AND 装备ID = %s", realGuid, itemId).c_str());

                    if (result)
                    {
                        usedGuid = realGuid;
                        LOG_DEBUG("module.huanjing", ModuleOldFormat("[GUID修复] 幻境倍率使用真实GUID找到数据: itemID=%s, 客户端GUID=%s, 真实GUID=%s", itemId, itemGuid, realGuid).c_str());
                        break;
                    }
                }
            }
        }

        if (result)
        {
            Field* fields = result->Fetch();
            multiplier = fields[0].Get<int>();
            multiplierMode = DbValueToItemAttributeMode(fields[1].Get<int32>());
            attributeData = fields[2].Get<std::string>();
        }
        else
        {
            multiplier = 1;
            multiplierMode = 'x';
            attributeData.clear();
        }

        // 查询鉴定系统的自定义属性数据（使用修复后的GUID）
        std::string identificationData;
#ifdef MODULE_ITEM_IDENTIFICATION
        if (sItemIdentificationSystem && sItemIdentificationSystem->_enabled)
        {
            ItemIdentificationSystem::AllModuleData moduleData =
                sItemIdentificationSystem->QueryAllModuleData(itemId, usedGuid);

            std::vector<std::string> identAttrs;

            auto parseAttributes = [&](const std::string& attrsStr)
            {
                if (attrsStr.empty())
                    return;

                std::istringstream stream(attrsStr);
                std::string attrPair;
                while (std::getline(stream, attrPair, ','))
                {
                    std::istringstream pairStream(attrPair);
                    uint32 attrType;
                    AsInt64 attrValue;
                    if (pairStream >> attrType >> attrValue)
                    {
                        AsInt64 enhancedValue = CalculateEnhancedAttributeValue(attrValue, multiplier, multiplierMode);
                        identAttrs.push_back(Acore::ToString(attrType) + " " +
                                             Acore::ToString(attrValue) + " " +
                                             Acore::ToString(enhancedValue));
                    }
                }
            };

            parseAttributes(moduleData.baseAttributes);
            parseAttributes(moduleData.additionalAttributes);

            if (!identAttrs.empty())
            {
                for (size_t i = 0; i < identAttrs.size(); ++i)
                {
                    if (i > 0)
                        identificationData += ",";
                    identificationData += identAttrs[i];
                }
            }
        }
#endif

        std::ostringstream response;
        response << "HUANJING_DATA:" << itemId << ":" << itemGuid << ":" << multiplierMode << ":" << multiplier;

        if (!attributeData.empty() || !identificationData.empty())
        {
            response << ":" << attributeData;
            if (!identificationData.empty())
            {
                response << "|" << identificationData;
            }
        }

        std::string responseStr = response.str();
        SendAddonMessage(player, "UITQ", responseStr);
    }
};

// 血条系统配置获取方法
uint32 HuanJingSystem::GetCreatureHealthBarCount(Creature* creature, Player* player) const
{
    const HuanJingCreatureConfig* config = GetMapCreatureConfig(creature, player);
    if (config && config->healthBarCount > 1)
        return config->healthBarCount;
    return 0; // 默认不启用血条
}

bool HuanJingSystem::EnsureCreatureHealthBarsFromConfig(Creature* creature, Player* player, bool sendUpdate)
{
    if (!sHelper->GetBoolean(22010))
        return false;

    if (!creature || !creature->IsAlive())
        return false;

    const HuanJingCreatureConfig* config = GetMapCreatureConfig(creature, player);
    uint32 templateBarCount = creature->StTempId > 0
        ? sHelper->GetCreHpCountByTempId(creature->StTempId)
        : sHelper->GetBigCreatureTemplateHpCount(creature->GetEntry());

    uint32 effectiveBarCount = 0;
    if (config && config->healthBarCount > 1)
        effectiveBarCount = config->healthBarCount;
    else if (templateBarCount > 1)
        effectiveBarCount = templateBarCount;

    if (effectiveBarCount <= 1)
    {
        // A template with one (or zero) configured bars must never inherit a
        // stale module/legacy multi-bar state from an earlier stat rebuild.
        creature->MultipleHp = 1;
        creature->MaxMultipleHp = 1;
        RemoveCreatureHealthBars(creature);
        return false;
    }

    AsInt64 configuredHealthPerBar = config ? config->healthValue : AsInt64(0);
    if (configuredHealthPerBar == 0)
        configuredHealthPerBar = ResolveCreatureServerHealthForHealthBar(creature, 0);

    bool shouldInitialize = true;
    {
        std::lock_guard<std::mutex> lock(_healthBarMutex);
        auto it = _healthBarData.find(GenerateCreatureKey(creature));
        if (it != _healthBarData.end() && it->second.enabled)
        {
            NormalizeHealthBarData(it->second);
            shouldInitialize = it->second.totalBars != effectiveBarCount || it->second.healthPerBar == 0;
        }
    }

    if (shouldInitialize)
        InitializeCreatureHealthBars(creature, effectiveBarCount, configuredHealthPerBar);
    else
        SyncHealthBarDataWithCreatureMaxHealth(creature);

    creature->MultipleHp = 1;
    creature->MaxMultipleHp = 1;

    if (sendUpdate)
        SendHealthBarUpdate(creature);

    return true;
}

AsInt64 HuanJingSystem::GetCreatureTrueDamageValue(Creature* creature, Player* player) const
{
    if (!sHelper->GetBoolean(22010))
        return 0;

    if (!creature)
        return 0;

    if (HasCreatureAttributeOverride(creature))
        return 0;

    if (player && !IsHuanJingCreatureEligibleForPlayer(creature, player))
        return 0;

    uint32 huanJingLevel = player ? GetPlayerHuanJingLevel(player) : 0;
    uint64 creatureKey = GenerateCreatureKey(creature);
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        auto it = _creatureAdjustedLevels.find(creatureKey);
        if (it != _creatureAdjustedLevels.end() && it->second > huanJingLevel)
            huanJingLevel = it->second;
    }

    if (huanJingLevel == 0)
        return 0;

    uint32 creatureId = creature->GetEntry();
    uint32 mapId = creature->GetMapId();
    uint32 areaId = creature->GetAreaId();
    uint32 difficulty = GetCreatureMapDifficulty(creature);
    uint32 creatureLevel = creature->GetLevel();
    uint32 creatureType = GetCreatureTypeByEntry(creatureId);

    HuanJingCreatureConfig const* config = GetCreatureConfig(creature, player);

    return config ? config->trueDamageValue : 0;
}


// 注册脚本
void Addmod_huanjinxitongScripts()
{
    LOG_DEBUG("healthbar.system", "[血条系统] 开始注册幻境系统脚本...");
    LOG_DEBUG("module.huanjing", ">> [幻境系统] 开始注册脚本...");

    new HuanJingWorldScript();
    new HuanJingSystemPlayerScript();
    new HuanJingSystemCreatureScript();
    LOG_DEBUG("healthbar.system", "[血条系统] 生物脚本已注册");
    new HuanJingSystemUnitScript();
    LOG_DEBUG("healthbar.system", "[血条系统] 单位脚本已注册 (伤害和死亡钩子)");
    new HuanJingSystemAllItemScript();
    new HuanJingCommandScript();
    new HuanJingAddonScript();

    LOG_DEBUG("healthbar.system", "[血条系统] 所有幻境系统脚本注册成功");
    LOG_DEBUG("module.huanjing", ">> [幻境系统] 脚本注册完成！");
}

// ========================================
// 血条系统实现（集成到幻境系统中）
// ========================================

void HuanJingSystem::InitializeCreatureHealthBars(Creature* creature, uint32 barCount, AsInt64 healthPerBar, AsInt64 totalHealth)
{
    if (!creature || barCount == 0)
    {
        return;
    }

    uint64 healthBarKey = GenerateCreatureKey(creature);
    AsInt64 currentMaxHealth = ResolveCreatureServerHealthForHealthBar(creature, healthPerBar);

    if (currentMaxHealth > GetCreatureCombatMaxHealth(creature))
    {
        SetCreatureCombatCreateHealth(creature, currentMaxHealth);
        SetCreatureCombatMaxHealth(creature, currentMaxHealth);
    }

    // 在锁内完成所有对 _healthBarData 的访问
    std::lock_guard<std::mutex> lock(_healthBarMutex);
    HealthBarData& data = _healthBarData[healthBarKey];
    data.totalBars = barCount;

    // 多倍血条每一条使用服务端 128 位真实血量；不从客户端安全血量字段推导。
    data.healthPerBar = currentMaxHealth;

    // 诊断用总血量可能超过 64 位，不参与战斗扣血。
    data.totalHealth = totalHealth != 0 ? totalHealth : SaturatingMultiplyUInt64(data.healthPerBar, barCount);

    // 战斗扣血只保存当前条血量和剩余条数，避免 697京 * 10 这类总量溢出。
    data.currentBar = barCount;
    data.currentHealth = data.healthPerBar;
    data.visibleHealth = data.healthPerBar;
    NormalizeHealthBarData(data);

    // 初始化广播状态
    data.lastBroadcastTime = 0;
    data.lastBroadcastBars = data.currentBar;

    // 启用血条系统
    data.enabled = true;

    // 重置保护期时间戳
    data.lastSwitchTime = 0;

    creature->MultipleHp = 1;
    creature->MaxMultipleHp = 1;

    SetCreatureCombatHealth(creature, data.visibleHealth);

    if (IsChallengeMirageDebugCreature(creature))
    {
        LOG_DEBUG("module.huanjing", ModuleOldFormat("[ChallengeMirageDebug] HUANJING_HEALTHBAR_INIT entry=%s guid=%s rawGuid=%s key=%s map=%s inst=%s bars=%s currentBar=%s healthPerBar=%s visibleHealth=%s creatureHealth=%s maxHealth=%s inCombat=%s", creature->GetEntry(), creature->GetGUID().GetCounter(), creature->GetGUID().GetRawValue(), healthBarKey, creature->GetMapId(), GetCreatureMapInstanceId(creature), data.totalBars, data.currentBar, FormatUInt64(data.healthPerBar), FormatUInt64(data.visibleHealth), FormatUInt64(GetCreatureCombatHealth(creature)), FormatUInt64(GetCreatureCombatMaxHealth(creature)), creature->IsInCombat() ? "true" : "false").c_str());
    }

    LOG_DEBUG("huanjing.healthbar", ModuleOldFormat("[血条系统] 生物 %s (Key: %s) (Entry: %s) 初始化血条: 总血条=%s, 每条血量=%s, 总血量=%s", creature->GetGUID().ToString(), healthBarKey, creature->GetEntry(), data.totalBars, FormatUInt64(data.healthPerBar), FormatUInt64(data.totalHealth)).c_str());
}

void HuanJingSystem::UpdateCreatureHealth(Creature* creature, AsInt64 newHealth)
{
    if (!creature)
        return;

    uint64 healthBarKey = GenerateCreatureKey(creature);
    std::lock_guard<std::mutex> lock(_healthBarMutex);
    auto it = _healthBarData.find(healthBarKey);
    if (it == _healthBarData.end())
        return;

    // 暂时不需要更新显示，因为客户端会自动显示生物当前血量
}

const HealthBarData* HuanJingSystem::GetHealthBarData(Creature const* creature) const
{
    if (!creature)
        return nullptr;

    uint64 healthBarKey = GenerateCreatureKey(creature);
    std::lock_guard<std::mutex> lock(_healthBarMutex);
    auto itr = _healthBarData.find(healthBarKey);
    if (itr != _healthBarData.end() && itr->second.enabled)
        return &itr->second;
    return nullptr;
}

HealthBarData* HuanJingSystem::GetMutableHealthBarData(Creature const* creature)
{
    if (!creature)
        return nullptr;

    uint64 healthBarKey = GenerateCreatureKey(creature);
    std::lock_guard<std::mutex> lock(_healthBarMutex);
    auto itr = _healthBarData.find(healthBarKey);
    if (itr != _healthBarData.end() && itr->second.enabled)
        return &itr->second;
    return nullptr;
}

bool HuanJingSystem::BuildCustomTargetHealthPacket(Creature const* creature, std::string& payload) const
{
    payload.clear();

    if (!creature)
        return false;

    HealthBarData dataCopy;
    {
        uint64 healthBarKey = GenerateCreatureKey(creature);
        std::lock_guard<std::mutex> lock(_healthBarMutex);
        auto itr = _healthBarData.find(healthBarKey);
        if (itr == _healthBarData.end() || !itr->second.enabled)
            return false;

        dataCopy = itr->second;
    }

    NormalizeHealthBarData(dataCopy);

    if (dataCopy.totalBars <= 1 || dataCopy.healthPerBar == 0)
        return false;

    uint32 remainingBars = dataCopy.currentBar;
    if (remainingBars > dataCopy.totalBars)
        remainingBars = dataCopy.totalBars;

    AsInt64 currentBarHealth = remainingBars > 0 ? dataCopy.currentHealth : 0;
    if (currentBarHealth > dataCopy.healthPerBar)
        currentBarHealth = dataCopy.healthPerBar;

    payload = Acore::ToString(creature->GetGUIDLow()) + "#" +
        Acore::ToString(remainingBars) + "#" +
        Acore::ToString(dataCopy.totalBars) + "#" +
        FormatUInt64(currentBarHealth) + "#" +
        FormatUInt64(dataCopy.healthPerBar);

    return true;
}

void HuanJingSystem::RemoveCreatureHealthBars(Creature const* creature)
{
    if (!creature)
        return;

    uint64 healthBarKey = GenerateCreatureKey(creature);
    std::lock_guard<std::mutex> lock(_healthBarMutex);
    _healthBarData.erase(healthBarKey);
    _healthBarRestoreQueue.erase(healthBarKey);
}

bool HuanJingSystem::IsHealthBarEnabled(Creature const* creature) const
{
    if (!creature)
        return false;

    uint64 healthBarKey = GenerateCreatureKey(creature);
    std::lock_guard<std::mutex> lock(_healthBarMutex);
    auto itr = _healthBarData.find(healthBarKey);
    return itr != _healthBarData.end() && itr->second.enabled;
}

bool HuanJingSystem::GetCreatureVirtualHealth(ObjectGuid guid, AsInt64& currentHealth, AsInt64& maxHealth) const
{
    (void)guid;
    currentHealth = 0;
    maxHealth = 0;

    // 多倍血条的虚拟总血量只给 HealthBarUI 显示和结算使用，不再覆盖主目标面板。
    return false;
}

std::string HuanJingSystem::GetHealthBarDisplay(Creature const* creature) const
{
    if (!creature)
        return "";

    HealthBarData dataCopy;
    bool hasData = false;
    {
        uint64 healthBarKey = GenerateCreatureKey(creature);
        std::lock_guard<std::mutex> lock(_healthBarMutex);
        auto itr = _healthBarData.find(healthBarKey);
        if (itr != _healthBarData.end() && itr->second.enabled)
        {
            dataCopy = itr->second;
            hasData = true;
        }
    }

    if (!hasData || dataCopy.totalBars <= 1)
        return "";

    NormalizeHealthBarData(dataCopy);

    std::ostringstream oss;

    uint32 remainingBars = dataCopy.currentBar;
    if (remainingBars > dataCopy.totalBars)
        remainingBars = dataCopy.totalBars;

    oss << "血条: " << remainingBars << "/" << dataCopy.totalBars;

    AsInt64 healthInCurrentBar = remainingBars > 0 ? dataCopy.currentHealth : 0;
    if (healthInCurrentBar > dataCopy.healthPerBar)
        healthInCurrentBar = dataCopy.healthPerBar;

    if (dataCopy.healthPerBar > 0)
    {
        long double percentage = Acore::Number::ToLongDouble(healthInCurrentBar) / Acore::Number::ToLongDouble(dataCopy.healthPerBar) * 100.0L;
        oss << " (" << std::fixed << std::setprecision(1) << static_cast<double>(percentage) << "%)";
    }

    // 添加插件显示用的每条血量。这里固定使用血条系统初始化时保存的单条血量，不使用 Boss 虚拟总血量。
    oss << " [每条:" << FormatUInt64(dataCopy.healthPerBar) << "]";

    return oss.str();
}

void HuanJingSystem::EnsureCreatureHealthSafe(Creature* creature)
{
    ClampCreatureHealthToMax(creature);
}

// 血量已由核心 64 位扩展值承载，这里保留空实现以兼容旧调用路径。
void HuanJingSystem::ClampCreatureHealthToMax(Creature* creature)
{
    (void)creature;
}

// 按节流规则广播血条更新：
// - 针对单个生物，同一 intervalMs 内最多发送一次
// - 若血条数发生变化，则优先发送（即使未到时间间隔）
void HuanJingSystem::BroadcastHealthBarUpdateThrottled(Creature* creature, uint32 nowMs, uint32 intervalMs)
{
    if (!creature)
        return;

    SyncHealthBarDataWithCreatureMaxHealth(creature);

    uint64 healthBarKey = GenerateCreatureKey(creature);
    uint32 currentBars = 0;
    bool shouldSend = false;

    {
        std::lock_guard<std::mutex> lock(_healthBarMutex);
        auto it = _healthBarData.find(healthBarKey);
        if (it == _healthBarData.end() || !it->second.enabled || it->second.totalBars <= 1)
            return;

        NormalizeHealthBarData(it->second);
        currentBars = it->second.currentBar;

        uint32 lastTime = it->second.lastBroadcastTime;
        uint32 lastBars = it->second.lastBroadcastBars;

        // 血条数变化，或者超过时间间隔，都需要重新广播
        if (currentBars != lastBars || nowMs - lastTime >= intervalMs)
        {
            it->second.lastBroadcastBars = currentBars;
            it->second.lastBroadcastTime = nowMs;
            shouldSend = true;
        }
    }

    if (!shouldSend)
        return;

    // 注意：SendHealthBarUpdate 内部会再次获取 _healthBarMutex（通过 GetHealthBarDisplay），
    // 因此必须在释放锁之后调用，避免死锁
    SendHealthBarUpdate(creature);
}

// 在核心致死逻辑 Unit::Kill 中统一调用：
// - 若目标启用了幻境血条系统，且仍有多条血条未打完，则不直接 Kill，而是按"一次消耗一条血条"处理
// - 这是针对特殊死亡触发（如GM命令、秒杀技能等不经过 OnDamage 的情况）的保护措施
// - 正常的伤害穿透已在 OnDamage 中处理，此处仅处理边缘情况
// - 只有多倍血条已经清空时，才允许真正 Kill
// 返回值：true 表示已由血条系统接管本次致死（调用方应直接 return，不再继续死亡结算）
bool HuanJingSystem::HandleUnitKill(Unit* killer, Unit* victim, SpellInfo const* spellProto)
{
    if (!UnitHasCreatureType(victim))
        return false;

    Creature* creature = victim->ToCreature();
    if (!creature)
        return false;

    if (IsHuanJingExcludedCreature(creature))
    {
        CleanupCreatureAdjustmentData(creature);
        RemoveCreatureHealthBars(creature);
        ObjectGuid guid = creature->GetGUID();
        ClearCreatureVirtualMana(guid);
        return false;
    }

    ObjectGuid guid = creature->GetGUID();
    uint64 healthBarKey = GenerateCreatureKey(creature);

    if (HasCreatureAttributeOverride(creature))
    {
        CleanupCreatureAdjustmentData(creature);
        ClearCreatureVirtualMana(guid);

        if (!IsHealthBarEnabled(creature))
        {
            RemoveCreatureHealthBars(creature);
            return false;
        }
    }

    SyncHealthBarDataWithCreatureMaxHealth(creature);

    HealthBarData dataCopy;
    bool hasData = false;
    {
        std::lock_guard<std::mutex> lock(_healthBarMutex);
        auto it = _healthBarData.find(healthBarKey);
        if (it != _healthBarData.end() && it->second.enabled && it->second.totalBars > 1 && it->second.healthPerBar > 0)
        {
            NormalizeHealthBarData(it->second);
            dataCopy = it->second;
            hasData = true;
        }
    }

    if (!hasData)
        return false;

    AsInt64 perBar = dataCopy.healthPerBar;
    uint32 barsRemaining = dataCopy.currentBar;

    // 多倍血条未清空前，所有秒杀/强制击杀都先消耗多倍血条，不允许直接打本体。
    if (barsRemaining == 0)
        return false;

    // 只剩最后一条时应允许核心继续死亡结算，否则会清空血条数据但阻止死亡状态同步。
    if (barsRemaining == 1)
        return false;

    // 将本次秒杀视为“消耗一条血条”。
    uint32 newBarsRemaining = barsRemaining > 0 ? barsRemaining - 1 : 0;
    AsInt64 newBarHealth = newBarsRemaining > 0 ? perBar : 0;

    {
        std::lock_guard<std::mutex> lock(_healthBarMutex);
        auto it = _healthBarData.find(healthBarKey);
        if (it != _healthBarData.end() && it->second.enabled)
        {
            it->second.currentBar = newBarsRemaining;
            it->second.currentHealth = newBarHealth;
            if (it->second.visibleHealth == 0)
                it->second.visibleHealth = newBarHealth;
            if (it->second.visibleHealth > newBarHealth)
                it->second.visibleHealth = newBarHealth;
            NormalizeHealthBarData(it->second);
            it->second.lastSwitchTime = getMSTime();
        }
    }

    AsInt64 visibleHealth = 0;
    {
        std::lock_guard<std::mutex> lock(_healthBarMutex);
        auto it = _healthBarData.find(healthBarKey);
        if (it != _healthBarData.end() && it->second.enabled)
            visibleHealth = it->second.visibleHealth;
    }

    // 服务端虚拟血条已切换，官方目标框显示血量保持单调，避免跨条回满。
    SetCreatureCombatHealth(creature, visibleHealth != 0 ? visibleHealth : perBar);

    // 通过 Addon 按节流规则刷新客户端血条显示
    BroadcastHealthBarUpdateThrottled(creature, getMSTime(), 3000);

    // 返回 true 告知调用方：本次致死已经被血条系统接管，不应继续执行后续死亡逻辑
    return true;
}

void HuanJingSystem::SendHealthBarUpdate(Creature* creature)
{
    if (!creature)
        return;

    if (!creature->IsAlive())
    {
        RemoveCreatureHealthBars(creature);
        return;
    }

    SyncHealthBarDataWithCreatureMaxHealth(creature);

    // 获取存储的血条数据（所有玩家看到相同的数据，保持同步）
    std::string display = BuildCreatureHuanJingBroadcast(creature);
    if (display.empty())
    {
        LOG_DEBUG("healthbar.system", ModuleOldFormat("[血条系统] 生物 %s 的血条显示为空", creature->GetGUID().ToString()).c_str());
        return;
    }

    LOG_DEBUG("healthbar.system", ModuleOldFormat("[血条系统] 准备发送血条更新 - 生物: %s, 显示内容: %s", creature->GetName(), display).c_str());

    // 通过 Addon 消息向附近玩家发送血条信息（不在聊天框显示）
    Map* map = creature->GetMap();
    if (!map)
        return;

    std::list<Player*> players;
    Acore::AnyPlayerInObjectRangeCheck checker(creature, 50.0f); // 50码范围内
    Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(creature, players, checker);
    Cell::VisitWorldObjects(creature, searcher, 50.0f);

    LOG_DEBUG("healthbar.system", ModuleOldFormat("[血条系统] 找到附近玩家数量: %s", players.size()).c_str());

    for (Player* player : players)
    {
        if (!player || !player->IsInWorld())
            continue;

        LOG_DEBUG("healthbar.system", ModuleOldFormat("[血条系统] 检查是否向玩家 %s 发送血条信息(通过Addon): %s", player->GetName(), display).c_str());
        SendCreatureHuanJingBroadcastToPlayer(player, creature, "HealthBarUpdate");
    }
}

bool HuanJingSystem::ResetCreatureHealthBarsToFull(Creature* creature, bool sendUpdate)
{
    if (!creature || !creature->IsInWorld() || !creature->IsAlive())
        return false;

    SyncHealthBarDataWithCreatureMaxHealth(creature);

    uint64 healthBarKey = GenerateCreatureKey(creature);
    AsInt64 restoredHealth = 0;
    uint32 restoredBars = 0;
    bool reset = false;

    {
        std::lock_guard<std::mutex> lock(_healthBarMutex);
        auto it = _healthBarData.find(healthBarKey);
        if (it == _healthBarData.end() || !it->second.enabled || it->second.totalBars <= 1 || it->second.healthPerBar == 0)
            return false;

        HealthBarData& data = it->second;
        NormalizeHealthBarData(data);

        data.currentBar = data.totalBars;
        data.currentHealth = data.healthPerBar;
        data.visibleHealth = data.healthPerBar;
        data.totalHealth = SaturatingMultiplyUInt64(data.healthPerBar, data.totalBars);
        data.lastSwitchTime = getMSTime();
        data.lastBroadcastTime = 0;
        data.lastBroadcastBars = data.currentBar;
        restoredHealth = data.healthPerBar;
        restoredBars = data.currentBar;
        reset = true;
    }

    if (!reset || restoredHealth == 0)
        return false;

    if (restoredHealth > GetCreatureCombatMaxHealth(creature))
    {
        SetCreatureCombatCreateHealth(creature, restoredHealth);
        SetCreatureCombatMaxHealth(creature, restoredHealth);
    }

    SetCreatureCombatHealth(creature, restoredHealth);

    if (IsChallengeMirageDebugCreature(creature))
    {
        LOG_DEBUG("module.huanjing", ModuleOldFormat("[ChallengeMirageDebug] HUANJING_HEALTHBAR_RESET_FULL entry=%s guid=%s rawGuid=%s key=%s map=%s inst=%s restoredHealth=%s bars=%s sendUpdate=%s inCombat=%s", creature->GetEntry(), creature->GetGUID().GetCounter(), creature->GetGUID().GetRawValue(), healthBarKey, creature->GetMapId(), GetCreatureMapInstanceId(creature), FormatUInt64(restoredHealth), restoredBars, sendUpdate ? "true" : "false", creature->IsInCombat() ? "true" : "false").c_str());
    }

    if (sendUpdate)
        SendHealthBarUpdate(creature);

    return true;
}

void HuanJingSystem::ClearHealthBarData()
{
    _healthBarData.clear();
    std::lock_guard<std::mutex> lock(_creatureMutex);
    _creatureVirtualMaxMana.clear();
}

void HuanJingSystem::CalculateCurrentBar(HealthBarData& data)
{
    NormalizeHealthBarData(data);
}

void HuanJingSystem::ProcessHealthBarRestoreQueue()
{
    // 简化实现：清空队列，实际的血量恢复在伤害处理后立即进行
    // 这个方法主要用于清理过期的恢复请求
    _healthBarRestoreQueue.clear();
}

void HuanJingSystem::AddToHealthBarRestoreQueue(Creature const* creature, AsInt64 health)
{
    if (!creature)
        return;

    uint64 healthBarKey = GenerateCreatureKey(creature);
    std::lock_guard<std::mutex> lock(_healthBarMutex);
    _healthBarRestoreQueue[healthBarKey] = health;
}

bool HuanJingSystem::ProcessCreatureHealthRestore(Creature* creature)
{
    if (!creature)
        return false;

    uint64 healthBarKey = GenerateCreatureKey(creature);
    AsInt64 healthToRestore = 0;
    bool shouldRestore = false;
    {
        std::lock_guard<std::mutex> lock(_healthBarMutex);
        auto it = _healthBarRestoreQueue.find(healthBarKey);
        if (it != _healthBarRestoreQueue.end())
        {
            healthToRestore = it->second;
            shouldRestore = true;
            _healthBarRestoreQueue.erase(it);
        }
    }

    if (shouldRestore)
    {
        SetCreatureCombatHealth(creature, healthToRestore);

        LOG_DEBUG("healthbar.system", ModuleOldFormat("[血条系统] 生物 %s 血条切换完成，血量立即恢复到 %s", creature->GetEntry(), FormatUInt64(healthToRestore)).c_str());
        return true;
    }
    return false;
}

// 法术强度管理方法
void HuanJingSystem::SetCreatureSpellPower(ObjectGuid guid, AsInt64 spellPower)
{
    std::lock_guard<std::mutex> lock(_creatureMutex);
    _creatureSpellPower[guid] = spellPower;
}

AsInt64 HuanJingSystem::GetCreatureSpellPower(ObjectGuid guid) const
{
    std::lock_guard<std::mutex> lock(_creatureMutex);
    auto it = _creatureSpellPower.find(guid);
    return (it != _creatureSpellPower.end()) ? it->second : 0;
}

void HuanJingSystem::SetCreatureVirtualMana(ObjectGuid guid, AsInt64 maxMana)
{
    std::lock_guard<std::mutex> lock(_creatureMutex);
    if (maxMana != 0)
        _creatureVirtualMaxMana[guid] = maxMana;
    else
        _creatureVirtualMaxMana.erase(guid);
}

bool HuanJingSystem::GetCreatureVirtualMana(Creature* creature, AsInt64& currentMana, AsInt64& maxMana) const
{
    if (!creature)
        return false;

    AsInt64 virtualMaxMana = 0;
    {
        std::lock_guard<std::mutex> lock(_creatureMutex);
        auto it = _creatureVirtualMaxMana.find(creature->GetGUID());
        if (it != _creatureVirtualMaxMana.end())
            virtualMaxMana = it->second;
    }

    uint32 clientMaxMana = creature->GetMaxPower(POWER_MANA);
    if (virtualMaxMana == 0 || virtualMaxMana <= clientMaxMana)
        return false;

    maxMana = virtualMaxMana;

    uint32 clientCurrentMana = creature->GetPower(POWER_MANA);
    if (!clientMaxMana)
    {
        currentMana = 0;
        return true;
    }

    if (clientCurrentMana >= clientMaxMana)
        currentMana = maxMana;
    else
        currentMana = MultiplyDivideAsInt256(maxMana, clientCurrentMana, clientMaxMana);

    if (currentMana > maxMana)
        currentMana = maxMana;

    return true;
}

void HuanJingSystem::ClearCreatureVirtualMana(ObjectGuid guid)
{
    std::lock_guard<std::mutex> lock(_creatureMutex);
    _creatureVirtualMaxMana.erase(guid);
}

void HuanJingSystem::ClearCreatureSpellPower(Creature* creature)
{
    if (!creature)
        return;

    ObjectGuid guid = creature->GetGUID();
    AsInt64 currentSpellPower = GetCreatureSpellPower(guid);

    if (currentSpellPower > 0)
    {
        // 从映射中移除
        {
            std::lock_guard<std::mutex> lock(_creatureMutex);
            _creatureSpellPower.erase(guid);
        }

        LOG_DEBUG("module", ModuleOldFormat(">> 生物 %s 清除法术强度: %s", creature->GetEntry(), FormatUInt64(currentSpellPower)).c_str());
    }
}

// 全局函数实现：获取生物的幻境系统法术强度
AsInt64 GetHuanJingCreatureSpellPower(ObjectGuid guid)
{
    if (sHuanJingSystem)
        return sHuanJingSystem->GetCreatureSpellPower(guid);
    return 0;
}

void HuanJingInitializeExternalCreatureHealthBars(Creature* creature, uint32 barCount)
{
    if (!sHuanJingSystem || !creature || barCount <= 1)
        return;

    sHuanJingSystem->InitializeCreatureHealthBars(creature, barCount);
    sHuanJingSystem->SendHealthBarUpdate(creature);
}

void HuanJingRemoveExternalCreatureHealthBars(Creature* creature)
{
    if (sHuanJingSystem)
        sHuanJingSystem->RemoveCreatureHealthBars(creature);
}

bool HuanJingIsExternalCreatureHealthBarEnabled(Creature* creature)
{
    return sHuanJingSystem && sHuanJingSystem->IsHealthBarEnabled(creature);
}

bool HuanJingPrepareExternalCreatureForPlayerDamage(Unit* attacker, Creature* creature)
{
    if (!sHuanJingSystem || !attacker || !creature || !creature->IsAlive())
        return false;

    Player* player = GetCombatOwnerPlayer(attacker);
    if (!player || !player->IsInWorld() || !player->IsAlive())
        return false;

    if (IsExternalPlayerDamageNoInitialAdjustActive()
        && !sHuanJingSystem->IsHealthBarEnabled(creature)
        && GetCreatureAdjustedHuanJingLevel(creature) == 0)
        return false;

    // 外部伤害准备不能被视为首次进入战斗，避免命中时重置生命。
    sHuanJingSystem->AdjustCreatureStatsForPlayer(creature, player);
    SyncHealthBarDataWithCreatureMaxHealth(creature);
    AsInt64 visibleHealth = 0;
    {
        uint64 healthBarKey = GenerateCreatureKey(creature);
        std::lock_guard<std::mutex> lock(sHuanJingSystem->_healthBarMutex);
        auto it = sHuanJingSystem->_healthBarData.find(healthBarKey);
        if (it != sHuanJingSystem->_healthBarData.end() && it->second.enabled)
        {
            NormalizeHealthBarData(it->second);
            visibleHealth = it->second.visibleHealth;
        }
    }

    if (visibleHealth != 0 && GetCreatureCombatHealth(creature) != visibleHealth)
        SetCreatureCombatHealth(creature, visibleHealth);

    return true;
}

HuanJingExternalHealthBarDamageResult HuanJingApplyExternalCreatureHealthBarDamage(Unit* attacker, Creature* creature, AsInt64 const& damage)
{
    if (!sHuanJingSystem)
        return HuanJingExternalHealthBarDamageResult();

    return sHuanJingSystem->ApplyExternalCreatureHealthBarDamage(attacker, creature, damage);
}

void HuanJingEnterExternalPlayerDamageNoInitialAdjust()
{
    ++s_externalPlayerDamageNoInitialAdjustDepth;
}

void HuanJingLeaveExternalPlayerDamageNoInitialAdjust()
{
    if (s_externalPlayerDamageNoInitialAdjustDepth != 0)
        --s_externalPlayerDamageNoInitialAdjustDepth;
}

void HuanJingSendExternalCreatureHealthBarUpdate(Creature* creature)
{
    if (sHuanJingSystem && creature)
        sHuanJingSystem->SendHealthBarUpdate(creature);
}
