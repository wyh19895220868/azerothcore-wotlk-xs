#include "Helper.h"
#include "Common.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ArenaTeamMgr.h"
#include "GuildMgr.h"
#include "GroupMgr.h"
#include "SpellMgr.h"
#include "UpdateMask.h"
#include "World.h"
#include "ArenaTeam.h"
#include "Transport.h"
#include "Language.h"
#include "GameEventMgr.h"
#include "Spell.h"
#include "Chat.h"
#include "AccountMgr.h"
#include "InstanceSaveMgr.h"
#include "SpellAuras.h"
#include "Util.h"
#include "GossipDef.h"
#include "Vehicle.h"
#include "AchievementMgr.h"
#include "DisableMgr.h"
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "PoolMgr.h"
#include "Config.h"
#include "WorldSession.h"
#include "Player.h"
#include "Item.h"
#include "ItemEnchantmentMgr.h"
#include "Bag.h"
#include "Pet.h"
#include "ReputationMgr.h"
#include <CharacterCache.h>
#include "AuctionHouseMgr.h"
#include <algorithm>
#include <limits>
#include "UpdateTime.h"

#include <psapi.h>
#include "WorldSocket.h"
#pragma comment(lib,"psapi.lib")

#include <Custom/AsItemMgr/AsItemMgr.h>
#include <Custom/ItemFuncMgr/ItemFuncMgr.h>
#include <Mail.h>
#include <Custom/PlayerModMgr/PlayerModMgr.h>
#include <Custom/ClassConfig/ClassMod.h>
#include <Custom/EventMgr/EventMgr.h>
#include <Custom/BGAsTeam/BGAs.h>
#include <Custom/CreatureMod/AsCreature.h>
#include <Custom/NoticeMgr/NoticeMgr.h>
#include <Custom/AuraSpellMgr/AuraSpellMgr.h>
#include <Custom/EnterFBMgr/EnterFBMgr.h>
#include <Custom/MultiNpcMgr/MultiNpcMgr.h>
#include <Custom/CustomVendorMgr/CustomVendorMgr.h>
#include <Custom/CreatureMod/AICreatureScripts.h>
#include <Custom/ItemShopMgr/ItemShopMgr.h>
#include <AI/ScriptedAI/ScriptedGossip.h>

#include "boost/asio.hpp"
#include <Time/GameTime.h>
#include <Custom\ItemRandomAtMgr\ItemRandomAtMgr.h>
#include <Custom\ItemRunesMgr\ItemRunesMgr.h>
#include <Custom\AddonActive\DoAddon.h>
#include <Custom/ArenaSoloMgr/ArenaSoloMgr.h>
#include <Custom/PlayerActivityMgr/PlayerActivityMgr.h>
#include <Custom/ItemMagicMgr/ItemMagicMgr.h>
#include <Custom/ItemUpMgr/ItemUpMgr.h>
#include <Custom/ItemPlayerStatMod/ItemPlayerStatMod.h>
#include <Utilities/Tokenize.h>
#include <Custom/ItemDevelopMgr/ItemDevelopMgr.h>
#include <Custom/GuildBattleMgr/GuildBattleMgr.h>
#include <Custom/AsItemMgr/NpcBeastmaster.h>
#include <Custom/CombatPetMgr/CombatPetMgr.h>
#include <Custom/AthenaEventMgr/AthenaEventMgr.h>
#include <Custom/CharPvpTop/CharPvpTop.h>
#include <Custom/MapAreaFuncMgr/MapAreaFunc.h>
#include <Custom/PicGuideMgr/PicGuideMgr.h>
#include <Custom/PlayerAction/PlayerAction.h>
#include <Custom/ReincarnationMgr/ReincarnationMgr.h>
#include <Custom/BeastCardMgr/BeastCardMgr.h>
#include <Custom/ChatFilterMgr/ChatFilterMgr.h>
#include <Custom/SpellCooldownMgr/SpellCooldownMgr.h>
#include <Custom/AntiGCDHackMgr/AntiGCDHackMgr.h>

#include <atomic>
#include <limits>
#include <sstream>
#include <vector>

#pragma execution_character_set("utf-8")


GlobalConfigMap GlobalConfigStore;
GtsMap GtsStore;
NeedTempMap NeedTempStore;
RewardTempMap RewardTempStore;
PosTemplateMap PosMap;
AuraTemplateVect AuraVect;

AtCreatureStatTempMap AtCreatureStatTempStore;
BigItemTemplateStatMap BigItemTemplateStatStore;
BigCreatureTemplateStatMap BigCreatureTemplateStatStore;
BigCreatureClassLevelStatMap BigCreatureClassLevelStatStore;

namespace
{
constexpr char DAMAGE_DISPLAY_PREFIX[] = "PATTRPANEL";
std::atomic<uint32> DamageDisplaySeq{0};

bool IsLegacyDamageLimitValue(AsInt64 const& value)
{
    constexpr int64 CompatCap = std::numeric_limits<int32>::max();
    return value == AsInt64(2000000000)
        || value == AsInt64(2100000000)
        || (value >= AsInt64(CompatCap - 1024) && value <= AsInt64(CompatCap + 1024));
}

AsInt64 GetPlayerDisplayDamage(Player* player, AsInt64 const& damage)
{
    if (!player)
        return damage;

    // 使用伤害链当前的最终结算值，不能用配置的固定伤害覆盖护甲、吸收和减伤结果。
    AsInt64 max = sCustomClass->GetAsDmgMax(player);
    if (max > 0 && !IsLegacyDamageLimitValue(max) && damage > max)
        return max;

    return damage;
}

AsInt64 GetOwnedUnitDisplayDamage(Player* owner, Unit* attacker, AsInt64 const& damage)
{
    if (!owner || !attacker)
        return damage;

    if (attacker == owner)
        return GetPlayerDisplayDamage(owner, damage);

    return damage;
}

void SendPlayerDisplayDamage(Player* player, AsInt64 amount, bool critical, uint32 schoolMask, uint32 spellId)
{
    if (!player || AsInt64IsZero(amount))
        return;

    std::ostringstream msg;
    msg << "DMG:"
        << AsInt64ToString(amount) << ':'
        << (critical ? 1 : 0) << ':'
        << schoolMask << ':'
        << spellId << ':'
        << ++DamageDisplaySeq;

    sDoAddon->SendPacketToClient(player, DAMAGE_DISPLAY_PREFIX, msg.str());
}

void SendPlayerIncomingDamage(Player* player, Unit* attacker, AsInt64 amount, uint32 schoolMask, uint32 spellId)
{
    if (!player || !attacker || AsInt64IsZero(amount))
        return;

    std::string sourceName = attacker->GetName();
    std::replace(sourceName.begin(), sourceName.end(), ':', ' ');

    std::ostringstream msg;
    msg << "CLOG:"
        << AsInt64ToString(amount) << ':'
        << schoolMask << ':'
        << spellId << ':'
        << ++DamageDisplaySeq << ':'
        << sourceName;

    sDoAddon->SendPacketToClient(player, DAMAGE_DISPLAY_PREFIX, msg.str());
}

void SendPlayerDisplayHeal(Player* player, AsInt64 amount, uint32 schoolMask, uint32 spellId)
{
    if (!player || AsInt64IsZero(amount) || IsLegacyDamageLimitValue(amount))
        return;

    std::ostringstream msg;
    msg << "HEAL:"
        << AsInt64ToString(amount) << ':'
        << schoolMask << ':'
        << spellId << ':'
        << ++DamageDisplaySeq;

    sDoAddon->SendPacketToClient(player, DAMAGE_DISPLAY_PREFIX, msg.str());
}

void LogCustomDamageStage(char const* stage, Unit* attacker, Unit* victim, uint32 damage, AsInt64 const& realDamage, uint32 spellId = 0)
{
    (void)stage;
    (void)attacker;
    (void)victim;
    (void)damage;
    (void)realDamage;
    (void)spellId;
}

AsInt64 ScaleAsInt64Value(AsInt64 const& value, float modifier)
{
    if (value <= 0 || modifier <= 0.0f)
        return AsInt64Zero();

    return ScaleAsInt64ByDouble(value, modifier);
}

void ApplyAsDamageOverride(Unit* attacker, Unit* victim, AsInt64 const& damageValue, uint32& damage)
{
    if (damageValue <= 0)
    {
        if (attacker && victim)
            attacker->SetAsDamageOverride(victim->GetGUID(), AsInt64Zero());
        damage = 0;
        return;
    }

    if (attacker && victim)
        attacker->SetAsDamageOverride(victim->GetGUID(), damageValue);

    damage = AsInt64ToUInt32Clamp(damageValue, uint32(std::numeric_limits<int32>::max()));
}

void ApplyAsDamageOverride(Unit* attacker, Unit* victim, AsInt64 const& damageValue, int32& damage)
{
    if (damageValue <= 0)
    {
        if (attacker && victim)
            attacker->SetAsDamageOverride(victim->GetGUID(), AsInt64Zero());
        damage = 0;
        return;
    }

    if (attacker && victim)
        attacker->SetAsDamageOverride(victim->GetGUID(), damageValue);

    damage = AsInt64ToInt32Clamp(damageValue, std::numeric_limits<int32>::max());
}

void AddAsDamagePct(AsInt64& damage, float pct)
{
    if (damage <= 0 || pct == 0.0f)
        return;

    damage += ScaleAsInt64Value(damage, pct);
    if (damage < 0)
        damage = AsInt64Zero();
}

void ScaleAsDamagePct(AsInt64& damage, float modifier)
{
    damage = ScaleAsInt64Value(damage, modifier);
}

AsInt64 ScaleAsDamageByCompat(AsInt64 const& value, uint32 beforeDamage, uint32 afterDamage)
{
    if (value <= 0 || afterDamage == 0)
        return AsInt64Zero();

    if (beforeDamage == 0 || beforeDamage == afterDamage)
        return value;

    return MultiplyDivideAsInt256(value, afterDamage, beforeDamage);
}

void ApplyAsDamageRet(AsInt64& damage, float ret)
{
    if (damage <= 0 || ret <= 0.0f)
        return;

    if (ret >= 1.0f)
    {
        damage = AsInt64(1);
        return;
    }

    damage -= ScaleAsInt64Value(damage, ret);
    if (damage <= 0)
        damage = AsInt64(1);
}

void ApplyPlayerDamageLimit(Player* player, Unit* victim, uint32& damage)
{
    if (!player || !damage)
        return;

    AsInt64 now = sCustomClass->GetAsDmgNow(player);
    if (now > 0 && !IsLegacyDamageLimitValue(now))
    {
        ApplyAsDamageOverride(player, victim, now, damage);
        return;
    }

    AsInt64 max = sCustomClass->GetAsDmgMax(player);
    if (max > 0 && !IsLegacyDamageLimitValue(max))
    {
        AsInt64 currentDamage = victim ? player->PeekAsDamageOverride(victim, damage) : AsInt64(damage);
        if (currentDamage > max)
            ApplyAsDamageOverride(player, victim, max, damage);
    }
}

void DealSupplementalSpellDamagePreserveOverride(Unit* attacker, Unit* victim, SpellInfo const* spellInfo, AsInt64 const& realDamage, uint32 compatDamage)
{
    if (!attacker || !victim || !spellInfo || !compatDamage)
        return;

    AsInt64 savedOverride = attacker->PeekAsDamageOverride(victim, 0);
    attacker->SetAsDamageOverride(victim->GetGUID(), AsInt64Zero());

    if (realDamage > compatDamage)
        attacker->SetAsDamageOverride(victim->GetGUID(), realDamage);

    SpellNonMeleeDamage damageInfo(attacker, victim, spellInfo, spellInfo->GetSchoolMask());
    damageInfo.damage = compatDamage;
    damageInfo.absorb = 0;
    attacker->SendSpellNonMeleeDamageLog(&damageInfo);
    attacker->DealSpellDamage(&damageInfo, true);

    if (savedOverride > 0)
        attacker->SetAsDamageOverride(victim->GetGUID(), savedOverride);
    else
        attacker->SetAsDamageOverride(victim->GetGUID(), AsInt64Zero());
}

uint64 BigItemTemplateStatKey(uint32 itemId, uint8 index)
{
    return (uint64(itemId) << 8) | index;
}

void StoreBigItemTemplateStat(uint32 itemId, uint8 index, uint32 statType, AsInt64 const& statValue)
{
    if (index >= MAX_ITEM_PROTO_STATS || statType >= MAX_ITEM_MOD || AsInt64IsZero(statValue))
        return;

    BigItemTemplateStatTemp stat;
    stat.StatType = statType;
    stat.StatValue = statValue;
    BigItemTemplateStatStore[BigItemTemplateStatKey(itemId, index)] = stat;
}

void FillBigItemTemplateStatCacheFromProto(uint32 itemId, ItemTemplate const* proto)
{
    if (!proto)
        return;

    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS && i < proto->StatsCount; ++i)
    {
        uint64 key = BigItemTemplateStatKey(itemId, i);
        if (BigItemTemplateStatStore.find(key) != BigItemTemplateStatStore.end())
            continue;

        StoreBigItemTemplateStat(itemId, i, proto->ItemStat[i].ItemStatType, proto->AsItemStatValue[i]);
    }
}

void ReloadBigItemTemplateStatCacheForEntry(uint32 itemId, ItemTemplate const* proto)
{
    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        BigItemTemplateStatStore.erase(BigItemTemplateStatKey(itemId, i));

    FillBigItemTemplateStatCacheFromProto(itemId, proto);
}

void ReloadBigItemTemplateStatCache()
{
    BigItemTemplateStatStore.clear();

    QueryResult bigItemTemplateResult = WorldDatabase.Query(
        "SELECT entry, "
        "stat_type1, stat_value1, stat_type2, stat_value2, stat_type3, stat_value3, stat_type4, stat_value4, stat_type5, stat_value5, "
        "stat_type6, stat_value6, stat_type7, stat_value7, stat_type8, stat_value8, stat_type9, stat_value9, stat_type10, stat_value10 "
        "FROM item_template;");
    if (!bigItemTemplateResult)
        return;

    do
    {
        Field* fields = bigItemTemplateResult->Fetch();
        uint32 itemId = fields[0].Get<uint32>();

        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            uint64 key = BigItemTemplateStatKey(itemId, i);
            if (BigItemTemplateStatStore.find(key) != BigItemTemplateStatStore.end())
                continue;

            StoreBigItemTemplateStat(itemId, i, fields[1 + i * 2].Get<uint32>(), AsInt64FromString(fields[2 + i * 2].Get<std::string>()));
        }
    } while (bigItemTemplateResult->NextRow());
}

void ClearItemTemplateInt64Fields(ItemTemplate& itemTemplate)
{
    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        itemTemplate.ItemStat[i].ItemStatType = 0;
        itemTemplate.ItemStat[i].ItemStatValue = 0;
        itemTemplate.AsItemStatValue[i] = AsInt64Zero();
    }

    for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
    {
        itemTemplate.Damage[i].DamageMin = 0.0f;
        itemTemplate.Damage[i].DamageMax = 0.0f;
        itemTemplate.Damage[i].DamageType = 0;
        itemTemplate.AsDamageMin[i] = AsInt64Zero();
        itemTemplate.AsDamageMax[i] = AsInt64Zero();
    }

    itemTemplate.Armor = 0;
    itemTemplate.AsArmor = AsInt64Zero();
}

std::vector<std::pair<Player*, uint8>> RemoveReloadedItemModsForOnlinePlayers(uint32 entry)
{
    std::vector<std::pair<Player*, uint8>> touchedSlots;

    SessionMap const& sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator itr = sessions.begin(); itr != sessions.end(); ++itr)
    {
        Player* player = itr->second ? itr->second->GetPlayer() : nullptr;
        if (!player || !player->IsInWorld())
            continue;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item || item->GetEntry() != entry || item->IsBroken() || !player->CanUseAttackType(Player::GetAttackBySlot(slot)))
                continue;

            player->_ApplyItemMods(item, slot, false);
            touchedSlots.emplace_back(player, slot);
        }
    }

    return touchedSlots;
}

void ApplyReloadedItemModsForOnlinePlayers(std::vector<std::pair<Player*, uint8>> const& touchedSlots)
{
    std::set<Player*> touchedPlayers;

    for (std::pair<Player*, uint8> const& slotInfo : touchedSlots)
    {
        Player* player = slotInfo.first;
        if (!player || !player->IsInWorld())
            continue;

        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slotInfo.second);
        if (!item || item->IsBroken() || !player->CanUseAttackType(Player::GetAttackBySlot(slotInfo.second)))
            continue;

        player->_ApplyItemMods(item, slotInfo.second, true);
        touchedPlayers.insert(player);
    }

    for (Player* player : touchedPlayers)
        if (player && player->IsInWorld())
            player->UpdateAllStats();
}

std::vector<Player*> RemoveAllItemModsForOnlinePlayers()
{
    std::vector<Player*> touchedPlayers;

    SessionMap const& sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator itr = sessions.begin(); itr != sessions.end(); ++itr)
    {
        Player* player = itr->second ? itr->second->GetPlayer() : nullptr;
        if (!player || !player->IsInWorld())
            continue;

        player->_RemoveAllItemMods();
        touchedPlayers.push_back(player);
    }

    return touchedPlayers;
}

void ApplyAllItemModsForOnlinePlayers(std::vector<Player*> const& touchedPlayers)
{
    for (Player* player : touchedPlayers)
    {
        if (!player || !player->IsInWorld())
            continue;

        player->_ApplyAllItemMods();
        player->UpdateAllStats();
    }
}
}

#pragma region 不常用

void Helper::LoadGlobalConfig()
{

    GlobalConfigStore.clear();

    QueryResult Result = WorldDatabase.Query("SELECT 序号, 功能值 FROM __控制总表;");
    if (!Result)
    {
        return;
    }

    do
    {
        Field* Fields = Result->Fetch();
        uint32 Id = Fields[0].Get<uint32>();
        std::string Value = Fields[1].Get<std::string>();

        GlobalConfigStore.insert(std::make_pair(Id, Value));

    } while (Result->NextRow());
}

void Helper::LoadGlobalGts()
{
    GtsStore.clear();

    QueryResult Result = WorldDatabase.Query("SELECT 编号, 提示内容 FROM _文本提示表;");
    if (!Result)
    {
        return;
    }

    uint32 Ct = 0;
    do
    {
        Field* Fields = Result->Fetch();
        uint32 Id = Fields[0].Get<uint32>();
        std::string Value = Fields[1].Get<std::string>();

        GtsStore.insert(std::make_pair(Id, Value));
        ++Ct;

    } while (Result->NextRow());
}

#pragma endregion 不常用

void Helper::LoadAsTemp()
{
    ReloadBigItemTemplateStatCache();

    BigCreatureTemplateStatStore.clear();

    BigCreatureClassLevelStatStore.clear();
    QueryResult BigCreatureClassResult = WorldDatabase.Query(
        "SELECT level, class, basehp0, basehp1, basehp2, basemana, basearmor, attackpower, rangedattackpower, damage_base, damage_exp1, damage_exp2 "
        "FROM creature_classlevelstats;");
    if (BigCreatureClassResult)
    {
        do
        {
            Field* Fields = BigCreatureClassResult->Fetch();
            uint8 Level = Fields[0].Get<uint8>();
            uint8 Class = Fields[1].Get<uint8>();

            BigCreatureClassLevelStatTemp St;
            St.BaseHealth[0] = AsInt64FromString(Fields[2].Get<std::string>());
            St.BaseHealth[1] = AsInt64FromString(Fields[3].Get<std::string>());
            St.BaseHealth[2] = AsInt64FromString(Fields[4].Get<std::string>());
            St.BaseMana = AsInt64FromString(Fields[5].Get<std::string>());
            St.BaseArmor = AsInt64FromString(Fields[6].Get<std::string>());
            St.AttackPower = AsInt64FromString(Fields[7].Get<std::string>());
            St.RangedAttackPower = AsInt64FromString(Fields[8].Get<std::string>());
            St.BaseDamage[0] = AsInt64FromString(Fields[9].Get<std::string>());
            St.BaseDamage[1] = AsInt64FromString(Fields[10].Get<std::string>());
            St.BaseDamage[2] = AsInt64FromString(Fields[11].Get<std::string>());

            uint32 Key = (uint32(Level) << 8) | uint32(Class);
            BigCreatureClassLevelStatStore[Key] = St;
        } while (BigCreatureClassResult->NextRow());
    }

    NeedTempStore.clear();
    uint32 Ct = 0;
    QueryResult Result = WorldDatabase.Query("SELECT 模版编号, 消耗材料1, 消耗数量1, 消耗材料2, 消耗数量2, 消耗材料3, 消耗数量3, 消耗材料4, 消耗数量4,"
        "消耗材料5, 消耗数量5, 消耗材料6, 消耗数量6, 消耗材料7, 消耗数量7, 消耗材料8, 消耗数量8, 需要会员等级, 消耗积分, 达到累计积分,"
        "需要人物等级, 消耗荣誉点数, 消耗竞技场点数, 消耗金币, 消耗魔币, 达到累计魔币, 消耗泡点, 需要职业掩码, 需要种族掩码, 需要阵营, 需要声望编号, 需要声望等级, 需要位于地图, 需要位于区域,"
        "需要达到成就点数, 需要军衔等级, 需要斗气等级, 需要拥有光环组, 需要通过成就组, 需要完成任务组, 需要主属性类型掩码, 需要主属性达到值, 需要学会技能组, 需要赞助等级, 需要全部装备栏等级, 需要转生等级 FROM __模版需求总表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 TempId = Fields[0].Get<uint32>();
            NeedTemp St;
            for (size_t i = 0; i < MNC; i++)
            {
                St.ItemId[i] = Fields[i * 2 + 1].Get<uint32>();
                St.Count[i] = Fields[i * 2 + 2].Get<uint32>();
            }

            St.VipLevel = Fields[17].Get<uint32>();
            St.Token = Fields[18].Get<uint64>();
            St.AsToken = Fields[19].Get<uint64>();
            St.PLevel = Fields[20].Get<int32>();
            St.Hr = Fields[21].Get<uint32>();
            St.An = Fields[22].Get<uint32>();
            St.Money = Fields[23].Get<uint32>();
            St.Mobi = Fields[24].Get<uint32>();//新增魔币
            St.AsMobi = Fields[25].Get<uint32>();//新增累计魔币
            St.Paodian = Fields[26].Get<uint64>();//新增泡点
            St.Class = Fields[27].Get<uint32>();
            St.Race = Fields[28].Get<uint32>();
            St.Team = Fields[29].Get<uint32>();
            St.FID = Fields[30].Get<uint32>();
            St.FRank = Fields[31].Get<uint32>();

            St.MapId = Fields[32].Get<int32>();
            St.AreaId = Fields[33].Get<uint32>();
            St.AchCount = Fields[34].Get<uint32>();
            St.Title = Fields[35].Get<uint32>();
            if (St.Title > 14)
                St.Title = 14;
            St.StatRank = Fields[36].Get<uint32>();
            St.Auras = Fields[37].Get<std::string>();
            St.ComAnch = Fields[38].Get<std::string>();
            St.ComQuest = Fields[39].Get<std::string>();
            St.StatId = Fields[40].Get<uint32>();
            St.StatValue = Fields[41].Get<uint32>();
            St.InSpell = Fields[42].Get<std::string>();
            St.NsRank = Fields[43].Get<uint32>();
            St.AsEqRank = Fields[44].Get<uint32>();
            St.Reincarnation = Fields[45].Get<uint32>();


            NeedTempStore.insert(std::make_pair(TempId, St));
            ++Ct;
        } while (Result->NextRow());
    }

    RewardTempStore.clear();
    Ct = 0;
    Result = WorldDatabase.Query("SELECT 模版编号, 奖励材料1, 奖励数量1, 奖励材料2, 奖励数量2, 奖励材料3, 奖励数量3, 奖励材料4, 奖励数量4,"
        "奖励材料5, 奖励数量5, 奖励材料6, 奖励数量6, 奖励材料7, 奖励数量7, 奖励材料8, 奖励数量8, 设置会员等级,"
        "奖励积分, 奖励人物经验, 奖励荣誉点数, 奖励竞技场点数, 奖励金币, 奖励魔币, 奖励泡点, 奖励光环组, 完成成就组, 完成任务组, GM命令组 FROM __模版奖励总表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 TempId = Fields[0].Get<uint32>();
            RewardTemp St;
            for (size_t i = 0; i < MNC; i++)
            {
                St.ItemId[i] = Fields[i * 2 + 1].Get<uint32>();
                St.Count[i] = Fields[i * 2 + 2].Get<uint32>();
            }

            St.NowVip = Fields[17].Get<uint32>();
            St.AddToken = Fields[18].Get<uint64>();
            St.Xp = Fields[19].Get<uint32>();
            St.Hr = Fields[20].Get<uint32>();
            St.An = Fields[21].Get<uint32>();
            St.Money = Fields[22].Get<uint32>();
            St.AddMobi = Fields[23].Get<uint32>();//新增魔币
            St.AddPaodian = Fields[24].Get<uint64>();//新增泡点
            St.Auras = Fields[25].Get<std::string>();
            St.ComAnch = Fields[26].Get<std::string>();
            St.ComQuest = Fields[27].Get<std::string>();
            St.Cmd = Fields[28].Get<std::string>();


            RewardTempStore.insert(std::make_pair(TempId, St));
            ++Ct;
        } while (Result->NextRow());
    }

    PosMap.clear();
    Ct = 0;
    Result = WorldDatabase.Query("SELECT 坐标编号, 地图编号, X坐标, Y坐标, Z坐标, O坐标 FROM __模版坐标");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Num = Fields[0].Get<uint32>();
            PosTemplate St;
            St.MapId = Fields[1].Get<uint32>();
            St.X = Fields[2].Get<float>();
            St.Y = Fields[3].Get<float>();
            St.Z = Fields[4].Get<float>();
            St.O = Fields[5].Get<float>();
            PosMap.insert(std::make_pair(Num, St));
            ++Ct;
        } while (Result->NextRow());
    }

    AuraVect.clear();
    Ct = 0;
    Result = WorldDatabase.Query("SELECT 组号, 技能编号 FROM __模版技能光环");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            AuraTemplate St;
            St.GroupId = Fields[0].Get<uint32>();
            St.AuraId = Fields[1].Get<uint32>();

            if (St.AuraId == TxLevelSpell)
                continue;

            AuraVect.push_back(St);
            ++Ct;
        } while (Result->NextRow());
    }

    AtCreatureStatTempStore.clear();
    // 新地图控制仍会通过生物控制组随机引用这些属性模板。
    Result = WorldDatabase.Query("SELECT 模版编号, 生物等级, 生物等级范围最小, 生物等级范围最大, 血条数量, 固定生命值, 血量倍率, 法力倍率, 法力值, "
        "受到的伤害固定值, 伤害减免百分比, 切割比例, 造成的伤害固定值, 真实伤害值, 物理伤害Min, 物理伤害Max, 最小伤害倍率, 最大伤害倍率, "
        "技能法术伤害Min, 技能法术伤害Max, 技能法术比例, 攻击强度倍率, 攻击强度值, 法术强度倍率, 法术强度值, 护甲, 护甲倍率, "
        "神圣抗性, 火焰抗性, 火抗倍率, 自然抗性, 自然抗倍率, 冰霜抗性, 冰霜抗倍率, 暗影抗性, 暗影抗倍率, 奥术抗性, 奥术抗倍率, "
        "攻击速度倍率, 攻击速度值, 施法速度倍率, 施法速度值, 移动速度倍率, 移动速度值, 命中率加成, 护甲穿透值, 护甲穿透百分比, "
        "格挡穿透值, 格挡穿透百分比, 生物光环组, 生物光环几率, 生物光环数量, 生物固定光环, 官方掉落, 额外掉落编号, "
        "超级掉落组号, 超级掉落几率, 超级掉落抽取数量Min, 超级掉落抽取数量Max, 装备属性倍率, 装备属性值, 装备属性倍率模式, 鉴定组, 鉴定品质, 鉴定物品等级 FROM __模版生物属性表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint8 index = 0;
            uint32 TempId = Fields[index++].Get<uint32>();
            AtCreatureStatTemp St;
            St.Level = Fields[index++].Get<uint32>();
            St.MinLevel = Fields[index++].Get<int32>();
            St.MaxLevel = Fields[index++].Get<int32>();
            St.HpCount = Fields[index++].Get<uint32>();
            St.NowHp = AsInt64FromString(Fields[index++].Get<std::string>());
            St.HealthMultiplier = Fields[index++].Get<float>();
            St.ManaMultiplier = Fields[index++].Get<float>();
            St.ManaValue = AsInt64FromString(Fields[index++].Get<std::string>());
            St.DmgTakenNow = AsInt64FromString(Fields[index++].Get<std::string>());
            St.DamageReductionPct = Fields[index++].Get<int64>();
            St.KtHp = Fields[index++].Get<float>();
            St.DmgNow = AsInt64FromString(Fields[index++].Get<std::string>());
            St.TrueDamageValue = AsInt64FromString(Fields[index++].Get<std::string>());
            St.DmgWMin = AsInt64FromString(Fields[index++].Get<std::string>());
            St.DmgWMax = AsInt64FromString(Fields[index++].Get<std::string>());
            St.MinDamageValue = St.DmgWMin;
            St.MaxDamageValue = St.DmgWMax;
            St.MinDamageMultiplier = Fields[index++].Get<float>();
            St.MaxDamageMultiplier = Fields[index++].Get<float>();
            St.MinSpellDmg = AsInt64FromString(Fields[index++].Get<std::string>());
            St.MaxSpellDmg = AsInt64FromString(Fields[index++].Get<std::string>());
            St.Spell = Fields[index++].Get<float>();
            St.AttackPowerMultiplier = Fields[index++].Get<float>();
            St.AttackPowerValue = AsInt64FromString(Fields[index++].Get<std::string>());
            St.SpellPowerMultiplier = Fields[index++].Get<float>();
            St.SpellPowerValue = AsInt64FromString(Fields[index++].Get<std::string>());
            St.Armor = AsInt64FromString(Fields[index++].Get<std::string>());
            St.ArmorMultiplier = Fields[index++].Get<float>();
            St.School1 = Fields[index++].Get<int32>();
            St.School2 = Fields[index++].Get<int32>();
            St.FireResistMultiplier = Fields[index++].Get<float>();
            St.School3 = Fields[index++].Get<int32>();
            St.NatureResistMultiplier = Fields[index++].Get<float>();
            St.School4 = Fields[index++].Get<int32>();
            St.FrostResistMultiplier = Fields[index++].Get<float>();
            St.School5 = Fields[index++].Get<int32>();
            St.ShadowResistMultiplier = Fields[index++].Get<float>();
            St.School6 = Fields[index++].Get<int32>();
            St.ArcaneResistMultiplier = Fields[index++].Get<float>();
            St.AttackSpeedMultiplier = Fields[index++].Get<float>();
            St.AttackSpeedValue = Fields[index++].Get<uint32>();
            St.CastSpeedMultiplier = Fields[index++].Get<float>();
            St.CastSpeedValue = Fields[index++].Get<uint32>();
            St.MoveSpeedMultiplier = Fields[index++].Get<float>();
            St.MoveSpeedValue = Fields[index++].Get<uint32>();
            St.HitChanceBonus = Fields[index++].Get<float>();
            St.ArmorPenetration = Fields[index++].Get<uint32>();
            St.ArmorPenetrationPct = Fields[index++].Get<float>();
            St.BlockPenetration = Fields[index++].Get<uint32>();
            St.BlockPenetrationPct = Fields[index++].Get<float>();
            St.CreAuraGroup = Fields[index++].Get<uint32>();
            St.CreAuraChance = Fields[index++].Get<float>();
            St.CreCount = Fields[index++].Get<uint32>();

            St.AtAuras.clear();
            std::string StrAuras = Fields[index++].Get<std::string>();
            if (!StrAuras.empty())
            {
                Tokenizer AurasData(StrAuras, '#');
                for (auto const& Temp : AurasData)
                    if (sSpellMgr->GetSpellInfo(uint32(atoi(Temp))))
                        St.AtAuras.insert(uint32(atoi(Temp)));
            }

            St.OfficialDrop = Fields[index++].Get<uint32>();
            St.PLootIds = Fields[index++].Get<std::string>();
            St.RAtLootGroupId = Fields[index++].Get<uint32>();
            St.RAtLootChance = Fields[index++].Get<float>();
            St.RAtLootMin = Fields[index++].Get<uint32>();
            St.RAtLootMax = Fields[index++].Get<uint32>();
            St.EquipAttrMultiplier = Fields[index++].Get<float>();
            St.EquipAttrValue = Fields[index++].Get<uint32>();
            St.EquipAttrMode = Fields[index++].Get<int32>();
            St.IdentificationGroup = Fields[index++].Get<uint32>();
            St.IdentificationQuality = Fields[index++].Get<uint8>();
            St.IdentificationItemLevel = Fields[index++].Get<uint32>();

            if (St.HealthMultiplier <= 0)
                St.HealthMultiplier = 1.0f;
            if (St.ManaMultiplier <= 0)
                St.ManaMultiplier = 1.0f;
            if (St.MinDamageMultiplier <= 0)
                St.MinDamageMultiplier = 1.0f;
            if (St.MaxDamageMultiplier <= 0)
                St.MaxDamageMultiplier = 1.0f;
            if (St.Spell <= 0)
                St.Spell = 1.0f;
            if (St.KtHp >= 1.0f)
                St.KtHp = 0.999f;

            if (St.DmgWMin <= 0)
                St.DmgWMin = 1;

            AtCreatureStatTempStore.insert(std::make_pair(TempId, St));
        } while (Result->NextRow());
    }
}

uint32 Helper::GetBigItemTemplateStatType(uint32 ItemId, uint8 Index, uint32 DefaultType)
{
    uint64 Key = (uint64(ItemId) << 8) | Index;
    auto Itr = BigItemTemplateStatStore.find(Key);
    return Itr != BigItemTemplateStatStore.end() ? Itr->second.StatType : DefaultType;
}

AsInt64 Helper::GetBigItemTemplateStatValue(uint32 ItemId, uint8 Index, AsInt64 const& DefaultValue)
{
    uint64 Key = (uint64(ItemId) << 8) | Index;
    auto Itr = BigItemTemplateStatStore.find(Key);
    return Itr != BigItemTemplateStatStore.end() ? Itr->second.StatValue : DefaultValue;
}

uint32 Helper::GetBigCreatureTemplateHpCount(uint32 CreatureId)
{
    auto Itr = BigCreatureTemplateStatStore.find(CreatureId);
    return Itr != BigCreatureTemplateStatStore.end() ? std::max<uint32>(1, Itr->second.HpCount) : 1;
}

AsInt64 Helper::GetAsBigCreatureTemplateHp(uint32 CreatureId)
{
    auto Itr = BigCreatureTemplateStatStore.find(CreatureId);
    return Itr != BigCreatureTemplateStatStore.end() ? Itr->second.NowHp : AsInt64Zero();
}

AsInt64 Helper::GetAsBigCreatureTemplateDmgTakenNow(uint32 CreatureId)
{
    auto Itr = BigCreatureTemplateStatStore.find(CreatureId);
    return Itr != BigCreatureTemplateStatStore.end() ? Itr->second.DmgTakenNow : AsInt64Zero();
}

AsInt64 Helper::GetAsBigCreatureTemplateDmgNow(uint32 CreatureId)
{
    auto Itr = BigCreatureTemplateStatStore.find(CreatureId);
    return Itr != BigCreatureTemplateStatStore.end() ? Itr->second.DmgNow : AsInt64Zero();
}

AsInt64 Helper::GetAsBigCreatureTemplateDmgWMin(uint32 CreatureId)
{
    auto Itr = BigCreatureTemplateStatStore.find(CreatureId);
    return Itr != BigCreatureTemplateStatStore.end() ? Itr->second.DmgWMin : AsInt64Zero();
}

AsInt64 Helper::GetAsBigCreatureTemplateDmgWMax(uint32 CreatureId)
{
    auto Itr = BigCreatureTemplateStatStore.find(CreatureId);
    return Itr != BigCreatureTemplateStatStore.end() ? Itr->second.DmgWMax : AsInt64Zero();
}

AsInt64 Helper::GetAsBigCreatureTemplateMinSpellDmg(uint32 CreatureId)
{
    auto Itr = BigCreatureTemplateStatStore.find(CreatureId);
    return Itr != BigCreatureTemplateStatStore.end() ? Itr->second.MinSpellDmg : AsInt64Zero();
}

AsInt64 Helper::GetAsBigCreatureTemplateMaxSpellDmg(uint32 CreatureId)
{
    auto Itr = BigCreatureTemplateStatStore.find(CreatureId);
    return Itr != BigCreatureTemplateStatStore.end() ? Itr->second.MaxSpellDmg : AsInt64Zero();
}

BigCreatureClassLevelStatTemp const* GetBigCreatureClassLevelStat(uint8 Level, uint8 Class)
{
    uint32 Key = (uint32(Level) << 8) | uint32(Class);
    auto Itr = BigCreatureClassLevelStatStore.find(Key);
    return Itr != BigCreatureClassLevelStatStore.end() ? &Itr->second : nullptr;
}

AsInt64 Helper::GetAsCreatureClassLevelHealth(uint8 Level, uint8 Class, uint8 Expansion)
{
    if (BigCreatureClassLevelStatTemp const* Stat = GetBigCreatureClassLevelStat(Level, Class))
        return Stat->BaseHealth[std::min<uint8>(Expansion, 2)];

    return AsInt64Zero();
}

AsInt64 Helper::GetAsCreatureClassLevelMana(uint8 Level, uint8 Class)
{
    if (BigCreatureClassLevelStatTemp const* Stat = GetBigCreatureClassLevelStat(Level, Class))
        return Stat->BaseMana;

    return AsInt64Zero();
}

AsInt64 Helper::GetAsCreatureClassLevelArmor(uint8 Level, uint8 Class)
{
    if (BigCreatureClassLevelStatTemp const* Stat = GetBigCreatureClassLevelStat(Level, Class))
        return Stat->BaseArmor;

    return AsInt64Zero();
}

AsInt64 Helper::GetAsCreatureClassLevelAttackPower(uint8 Level, uint8 Class)
{
    if (BigCreatureClassLevelStatTemp const* Stat = GetBigCreatureClassLevelStat(Level, Class))
        return Stat->AttackPower;

    return AsInt64Zero();
}

AsInt64 Helper::GetAsCreatureClassLevelRangedAttackPower(uint8 Level, uint8 Class)
{
    if (BigCreatureClassLevelStatTemp const* Stat = GetBigCreatureClassLevelStat(Level, Class))
        return Stat->RangedAttackPower;

    return AsInt64Zero();
}

AsInt64 Helper::GetAsCreatureClassLevelBaseDamage(uint8 Level, uint8 Class, uint8 Expansion)
{
    if (BigCreatureClassLevelStatTemp const* Stat = GetBigCreatureClassLevelStat(Level, Class))
        return Stat->BaseDamage[std::min<uint8>(Expansion, 2)];

    return AsInt64Zero();
}

std::string Helper::GetNeedItemUpGts(uint32 TempId, uint32 Mod)
{
    std::ostringstream Nss;

    if (!TempId)
    {
        Nss << "\n";
        return Nss.str();
    }

    auto Itr = NeedTempStore.find(TempId);
    if (Itr == NeedTempStore.end())
    {
        Nss << "\n";
        return Nss.str();
    }

    bool NoItem = true;
    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            NoItem = false;
    }

    bool NoNeed = (NoItem
        && Itr->second.VipLevel == 0
        && Itr->second.Token == 0
        && Itr->second.AsToken == 0
        && Itr->second.PLevel == 0
        && Itr->second.Hr == 0
        && Itr->second.An == 0
        && Itr->second.Money == 0
        && Itr->second.Class == 0
        && Itr->second.Race == 0
        && Itr->second.Team == 0
        && Itr->second.FID == 0
        && Itr->second.MapId == 0
        && Itr->second.AreaId == 0
        && Itr->second.AchCount == 0
        && Itr->second.Title == 0
        && Itr->second.StatRank == 0
        && Itr->second.Auras.empty()
        && Itr->second.ComAnch.empty()
        && Itr->second.ComQuest.empty()
        && Itr->second.StatId == 0
        && Itr->second.StatValue == 0
        && Itr->second.InSpell.empty()
        && Itr->second.NsRank == 0
        && Itr->second.AsEqRank == 0
        && Itr->second.Reincarnation == 0
        && Itr->second.Mobi == 0//新增魔币
        && Itr->second.AsMobi == 0//新增魔币
        );
    if (NoNeed)
    {
        Nss << "\n";
        return Nss.str();
    }

    Nss << "\n";

    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
        {
            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
                Nss << "|cFF00FF00需要材料|r " << GetItemLink(Itr->second.ItemId[i]) << " X |cFFFF0000" << Itr->second.Count[i] * Mod << "|r\n";
        }
    }

    if (Itr->second.VipLevel > 0)
        Nss << "|cFF00FF00需要达到会员等级|r [|cFFFF0000" << Itr->second.VipLevel << "|r]\n";

    if (Itr->second.Token > 0)
        Nss << "|cFF00FF00需要消耗积分|r |cFFFF6699" << Itr->second.Token * Mod << "|r\n";

    if (Itr->second.AsToken > 0)
        Nss << "|cFF00FF00需要达到累计积分|r |cFFFF6699" << Itr->second.AsToken << "|r\n";

    if (Itr->second.Mobi > 0)
        Nss << "|cFF00FF00需要消耗魔币|r |cFFFF6699" << Itr->second.Mobi * Mod << "|r\n";//新增魔币

    if (Itr->second.AsMobi > 0)
        Nss << "|cFF00FF00需要达到累计魔币|r |cFFFF6699" << Itr->second.AsMobi << "|r\n";//新增魔币

    if (Itr->second.Paodian > 0)
        Nss << "|cFF00FF00需要消耗泡点|r |cFFFF6699" << Itr->second.Paodian << "|r\n";//新增泡点

    if (Itr->second.PLevel != 0)
    {
        if (Itr->second.PLevel > 0)
            Nss << "|cFF00FF00需要达到人物等级|r [|cFFFF0000" << Itr->second.PLevel << "|r]\n";
        else
            Nss << "|cFF00FF00需要人物等级小于等于|r [|cFFFF0000" << -Itr->second.PLevel << "|r]\n";
    }

    if (Itr->second.Hr > 0)
        Nss << "|cFF00FF00需要消耗荣誉点数|r X |cFFFF0000" << Itr->second.Hr * Mod << "|r\n";

    if (Itr->second.An > 0)
        Nss << "|cFF00FF00需要消耗竞技点数|r X |cFFFF0000" << Itr->second.An * Mod << "|r\n";

    if (Itr->second.Money > 0)
        Nss << "|cFF00FF00需要消耗金币|r X |cFFFF0000" << Itr->second.Money * Mod << "|r\n";

    if (Itr->second.Class > 0)
        Nss << "|cFF00FF00需要职业|r [|cFFFF0000" << PullMaskClassName(Itr->second.Class) << "|r]\n";

    if (Itr->second.Race > 0)
        Nss << "|cFF00FF00需要种族|r [|cFFFF0000" << PullMaskRaceName(Itr->second.Race) << "|r]\n";

    if (Itr->second.Team > 0)
        Nss << "|cFF00FF00需要阵营|r [|cFFFF0000" << GetNtTeamStr(Itr->second.Team) << "|r]\n";

    if (Itr->second.FID > 0)
        Nss << "|cFF00FF00需要声望|r [|cFFFF0000" << GetGtsFactionName(Itr->second.FID) << "|r] 达到 [|cFF00FF00" << GetFactionRankName(Itr->second.FRank) << "|r]\n";

    if (Itr->second.MapId >= 0)
        Nss << "|cFF00FF00需要位于地图|r [|cFFFF0000" << GetMapName(Itr->second.MapId) << "|r]\n";

    if (Itr->second.AreaId > 0)
        Nss << "|cFF00FF00需要位于区域|r [|cFFFF0000" << GetAreaName(Itr->second.AreaId) << "|r]\n";

    if (Itr->second.AchCount > 0)
        Nss << "|cFF00FF00需要达到成就点数|r [|cFFFF0000" << Itr->second.AchCount << "点|r]\n";

    if (Itr->second.Title > 0)
        Nss << "|cFF00FF00需要军衔等级|r [|cFFFF0000" << GetTitleName(Itr->second.Title) << "|r]\n";

    if (Itr->second.StatRank > 0)
        Nss << "|cFF00FF00需要达到斗气等级|r [|cFFFF0000" << Itr->second.StatRank << "|r]\n";

    Tokenizer AuraData(Itr->second.Auras, '#');
    for (auto const& Temp : AuraData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
            Nss << "|cFF00FF00需要拥有光环Buff|r " << GetSpellName(atoi(Temp)) << "\n";
    }

    Tokenizer ComAnchData(Itr->second.ComAnch, '#');
    for (auto const& Temp : ComAnchData)
    {
        if (AchievementEntry const* AEntry = sAchievementStore.LookupEntry(atoi(Temp)))
            Nss << "|cFF00FF00需要完成成就|r " << GetAchievementName(atoi(Temp)) << "\n";
    }

    Tokenizer ComQuestData(Itr->second.ComQuest, '#');
    for (auto const& Temp : ComQuestData)
    {
        if (Quest const* QInfo = sObjectMgr->GetQuestTemplate(atoi(Temp)))
            Nss << "|cFF00FF00需要完成任务|r " << GetQuestName(atoi(Temp)) << "\n";
    }

    if (Itr->second.StatId > 0 && Itr->second.StatValue > 0)
        Nss << "|cFF00FF00需要主属性|r [|cFFFF0000" << PullMaskStatName(Itr->second.StatId) << "|r] 达到 [|cFFFF0000" << Itr->second.StatValue << "|r]\n";

    Tokenizer InSpellData(Itr->second.InSpell, '#');
    for (auto const& Temp : InSpellData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
            Nss << "|cFF00FF00需要学会或拥有技能|r " << GetSpellName(atoi(Temp)) << "\n";
    }

    if (Itr->second.NsRank > 0)
        Nss << "|cFF00FF00需要达到赞助等级|r [|cFFFF0000" << Itr->second.NsRank << "|r]\n";

    if (Itr->second.AsEqRank > 0)
        Nss << "|cFF00FF00需要全身装备栏等级达到|r [|cFFFF0000" << Itr->second.AsEqRank << "|r]\n";

    if (Itr->second.Reincarnation > 0)
        Nss << "|cFF00FF00需要转生等级达到|r [|cFFFF0000" << Itr->second.Reincarnation << "|r]\n";

    Nss << "\n";

    return Nss.str();
}

std::string Helper::GetNeedGts(uint32 TempId, uint32 Mod)
{
    std::ostringstream Nss;

    if (!TempId)
    {
        Nss << "\n";
        return Nss.str();
    }

    auto Itr = NeedTempStore.find(TempId);
    if (Itr == NeedTempStore.end())
    {
        Nss << "\n";
        return Nss.str();
    }

    bool NoItem = true;
    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            NoItem = false;
    }

    bool NoNeed = (NoItem
        && Itr->second.VipLevel == 0
        && Itr->second.Token == 0
        && Itr->second.AsToken == 0
        && Itr->second.PLevel == 0
        && Itr->second.Hr == 0
        && Itr->second.An == 0
        && Itr->second.Money == 0
        && Itr->second.Class == 0
        && Itr->second.Race == 0
        && Itr->second.Team == 0
        && Itr->second.FID == 0
        && Itr->second.MapId == 0
        && Itr->second.AreaId == 0
        && Itr->second.AchCount == 0
        && Itr->second.Title == 0
        && Itr->second.StatRank == 0
        && Itr->second.Auras.empty()
        && Itr->second.ComAnch.empty()
        && Itr->second.ComQuest.empty()
        && Itr->second.StatId == 0
        && Itr->second.StatValue == 0
        && Itr->second.InSpell.empty()
        && Itr->second.NsRank == 0
        && Itr->second.AsEqRank == 0
        && Itr->second.Mobi == 0//新增魔币
        && Itr->second.AsMobi == 0//新增魔币
        );
    if (NoNeed)
    {
        Nss << "\n";
        return Nss.str();
    }

    Nss << "\n";

    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
        {
            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
                Nss << "|cFF00FF00需要材料|r " << GetItemLink(Itr->second.ItemId[i]) << " X |cFFFF0000" << Itr->second.Count[i] * Mod << "|r\n";
        }
    }

    if (Itr->second.VipLevel > 0)
        Nss << "|cFF00FF00需要达到会员等级|r [|cFFFF0000" << Itr->second.VipLevel << "|r]\n";

    if (Itr->second.Token > 0)
        Nss << "|cFF00FF00需要消耗积分|r |cFFFF6699" << Itr->second.Token * Mod << "|r\n";

    if (Itr->second.AsToken > 0)
        Nss << "|cFF00FF00需要达到累计积分|r |cFFFF6699" << Itr->second.AsToken << "|r\n";

    if (Itr->second.Mobi > 0)
        Nss << "|cFF00FF00需要消耗魔币|r |cFFFF6699" << Itr->second.Mobi * Mod << "|r\n";//新增魔币

    if (Itr->second.AsMobi > 0)
        Nss << "|cFF00FF00需要达到累计魔币|r |cFFFF6699" << Itr->second.AsMobi << "|r\n";//新增魔币

    if (Itr->second.Paodian > 0)
        Nss << "|cFF00FF00需要消耗泡点|r |cFFFF6699" << Itr->second.Paodian << "|r\n";//新增泡点

    if (Itr->second.PLevel != 0)
    {
        if (Itr->second.PLevel > 0)
            Nss << "|cFF00FF00需要达到人物等级|r [|cFFFF0000" << Itr->second.PLevel << "|r]\n";
        else
            Nss << "|cFF00FF00需要人物等级小于等于|r [|cFFFF0000" << -Itr->second.PLevel << "|r]\n";
    }

    if (Itr->second.Hr > 0)
        Nss << "|cFF00FF00需要消耗荣誉点数|r X |cFFFF0000" << Itr->second.Hr * Mod << "|r\n";

    if (Itr->second.An > 0)
        Nss << "|cFF00FF00需要消耗竞技点数|r X |cFFFF0000" << Itr->second.An * Mod << "|r\n";

    if (Itr->second.Money > 0)
        Nss << "|cFF00FF00需要消耗金币|r X |cFFFF0000" << Itr->second.Money * Mod << "|r\n";

    if (Itr->second.Class > 0)
        Nss << "|cFF00FF00需要职业|r [|cFFFF0000" << PullMaskClassName(Itr->second.Class) << "|r]\n";

    if (Itr->second.Race > 0)
        Nss << "|cFF00FF00需要种族|r [|cFFFF0000" << PullMaskRaceName(Itr->second.Race) << "|r]\n";

    if (Itr->second.Team > 0)
        Nss << "|cFF00FF00需要阵营|r [|cFFFF0000" << GetNtTeamStr(Itr->second.Team) << "|r]\n";

    if (Itr->second.FID > 0)
        Nss << "|cFF00FF00需要声望|r [|cFFFF0000" << GetGtsFactionName(Itr->second.FID) << "|r] 达到 [|cFF00FF00" << GetFactionRankName(Itr->second.FRank) << "|r]\n";

    if (Itr->second.MapId >= 0)
        Nss << "|cFF00FF00需要位于地图|r [|cFFFF0000" << GetMapName(Itr->second.MapId) << "|r]\n";

    if (Itr->second.AreaId > 0)
        Nss << "|cFF00FF00需要位于区域|r [|cFFFF0000" << GetAreaName(Itr->second.AreaId) << "|r]\n";

    if (Itr->second.AchCount > 0)
        Nss << "|cFF00FF00需要达到成就点数|r [|cFFFF0000" << Itr->second.AchCount << "点|r]\n";

    if (Itr->second.Title > 0)
        Nss << "|cFF00FF00需要军衔等级|r [|cFFFF0000" << GetTitleName(Itr->second.Title) << "|r]\n";

    if (Itr->second.StatRank > 0)
        Nss << "|cFF00FF00需要达到斗气等级|r [|cFFFF0000" << Itr->second.StatRank << "|r]\n";

    Tokenizer AuraData(Itr->second.Auras, '#');
    for (auto const& Temp : AuraData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
            Nss << "|cFF00FF00需要拥有光环Buff|r " << GetSpellName(atoi(Temp)) << "\n";
    }

    Tokenizer ComAnchData(Itr->second.ComAnch, '#');
    for (auto const& Temp : ComAnchData)
    {
        if (AchievementEntry const* AEntry = sAchievementStore.LookupEntry(atoi(Temp)))
            Nss << "|cFF00FF00需要完成成就|r " << GetAchievementName(atoi(Temp)) << "\n";
    }

    Tokenizer ComQuestData(Itr->second.ComQuest, '#');
    for (auto const& Temp : ComQuestData)
    {
        if (Quest const* QInfo = sObjectMgr->GetQuestTemplate(atoi(Temp)))
            Nss << "|cFF00FF00需要完成任务|r " << GetQuestName(atoi(Temp)) << "\n";
    }

    if (Itr->second.StatId > 0 && Itr->second.StatValue > 0)
        Nss << "|cFF00FF00需要主属性|r [|cFFFF0000" << PullMaskStatName(Itr->second.StatId) << "|r] 达到 [|cFFFF0000" << Itr->second.StatValue << "|r]\n";

    Tokenizer InSpellData(Itr->second.InSpell, '#');
    for (auto const& Temp : InSpellData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
            Nss << "|cFF00FF00需要学会或拥有技能|r " << GetSpellName(atoi(Temp)) << "\n";
    }

    if (Itr->second.NsRank > 0)
        Nss << "|cFF00FF00需要达到赞助等级|r [|cFFFF0000" << Itr->second.NsRank << "|r]\n";

    if (Itr->second.AsEqRank > 0)
        Nss << "|cFF00FF00需要全身装备栏等级达到|r [|cFFFF0000" << Itr->second.AsEqRank << "|r]\n";

    Nss << "\n";

    return Nss.str();
}

std::string Helper::GetNeedLuaGts(uint32 TempId)
{
    std::ostringstream Nss;
    Nss << "{\n";

    auto Itr = NeedTempStore.find(TempId);
    if (Itr != NeedTempStore.end())
    {
        for (size_t i = 0; i < MNC; i++)
            Nss << "                    \"" << Itr->second.ItemId[i] << "\",\n" << "                    \"" << Itr->second.Count[i] << "\",\n";

        Nss << "                    \"" << Itr->second.VipLevel << "\",\n";
        Nss << "                    \"" << Itr->second.Token << "\",\n";
        Nss << "                    \"" << Itr->second.AsToken << "\",\n";
        Nss << "                    \"" << Itr->second.PLevel << "\",\n";
        Nss << "                    \"" << Itr->second.Hr << "\",\n";
        Nss << "                    \"" << Itr->second.An << "\",\n";
        Nss << "                    \"" << Itr->second.Money << "\",\n";

        std::string ClassStr = sHelper->PullMaskClassName(Itr->second.Class) == "无需" ? "0" : sHelper->PullMaskClassName(Itr->second.Class);
        std::string RaceStr = sHelper->PullMaskRaceName(Itr->second.Race) == "无需" ? "0" : sHelper->PullMaskRaceName(Itr->second.Race);
        std::string FnStr = sHelper->GetNtTeamStr(Itr->second.Team) == "未知" ? "0" : sHelper->GetNtTeamStr(Itr->second.Team);
        std::string FIDStr = sHelper->GetGtsFactionName(Itr->second.FID) == "未知" ? "0" : "[" + sHelper->GetGtsFactionName(Itr->second.FID) + "]" + sHelper->GetFactionRankName(Itr->second.FRank);
        std::string InMap = std::string(sHelper->GetMapName(Itr->second.MapId)) == "未知" ? "0" : std::string(sHelper->GetMapName(Itr->second.MapId));
        std::string Area = std::string(sHelper->GetAreaName(Itr->second.AreaId)) == "" ? "0" : std::string(sHelper->GetAreaName(Itr->second.AreaId));
        std::string AchCount = std::to_string(Itr->second.AchCount);
        std::string RTitle = sHelper->GetTitleName(Itr->second.Title) == "未知" ? "0" : sHelper->GetTitleName(Itr->second.Title);
        std::string StatRank = std::to_string(Itr->second.StatRank);

        Nss << "                    \"" << ClassStr << "\",\n";
        Nss << "                    \"" << RaceStr << "\",\n";
        Nss << "                    \"" << FnStr << "\",\n";
        Nss << "                    \"" << FIDStr << "\",\n";
        Nss << "                    \"" << InMap << "\",\n";
        Nss << "                    \"" << Area << "\",\n";
        Nss << "                    \"" << AchCount << "\",\n";
        Nss << "                    \"" << RTitle << "\",\n";
        Nss << "                    \"" << StatRank << "\",\n";
        Nss << "                    \"" << Itr->second.Mobi << "\",\n";//新增魔币
        Nss << "                    \"" << Itr->second.AsMobi << "\",\n";//新增魔币

    }

    Nss << "                }";
    return Nss.str();
}

/*属于检查成功后扣除*/
bool Helper::CheckNeed(Player* player, uint32 TempId, std::string Info, uint32 Mod)
{
    bool Ret = true;

    if (!TempId)
        return Ret;

    auto Itr = NeedTempStore.find(TempId);
    if (Itr == NeedTempStore.end())
        return Ret;

    bool NoItem = true;
    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            NoItem = false;
    }

    bool NoNeed = (NoItem
        && Itr->second.VipLevel == 0
        && Itr->second.Token == 0
        && Itr->second.AsToken == 0
        && Itr->second.PLevel == 0
        && Itr->second.Hr == 0
        && Itr->second.An == 0
        && Itr->second.Money == 0
        && Itr->second.Class == 0
        && Itr->second.Race == 0
        && Itr->second.Team == 0
        && Itr->second.FID == 0
        && Itr->second.MapId == 0
        && Itr->second.AreaId == 0
        && Itr->second.AchCount == 0
        && Itr->second.Title == 0
        && Itr->second.StatRank == 0
        && Itr->second.Auras.empty()
        && Itr->second.ComAnch.empty()
        && Itr->second.ComQuest.empty()
        && Itr->second.StatId == 0
        && Itr->second.StatValue == 0
        && Itr->second.InSpell.empty()
        && Itr->second.NsRank == 0
        && Itr->second.AsEqRank == 0
        && Itr->second.Reincarnation == 0
        && Itr->second.Mobi == 0//新增魔币
        && Itr->second.AsMobi == 0//新增魔币
        && Itr->second.Paodian == 0//新增泡点
        );

    if (NoNeed)
        return Ret;

    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
        {
            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
            {
                if (!player->HasItemCount(Itr->second.ItemId[i], Itr->second.Count[i] * Mod))
                {
                    Ret = false;
                    player->SendSysMsg(GetGts(1011), GetItemLink(Itr->second.ItemId[i]).c_str(), Itr->second.Count[i] * Mod);
                }
            }
        }
    }

    if (Itr->second.VipLevel > 0)
    {
        if (player->GetVipRank() < Itr->second.VipLevel)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1012), Itr->second.VipLevel);
        }
    }

    if (Itr->second.Token > 0)
    {
        if (player->GetToken() < Itr->second.Token * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1013), Itr->second.Token * Mod);
        }
    }

    if (Itr->second.AsToken > 0)
    {
        if (player->GetAsToken() < Itr->second.AsToken)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1014), Itr->second.AsToken);
        }
    }
    if (Itr->second.Mobi > 0)//新增魔币
    {
        if (player->GetTokenMB() < Itr->second.Mobi * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(10200), Itr->second.Mobi * Mod);
        }
    }

    if (Itr->second.AsMobi > 0)
    {
        if (player->GetAsTokenMB() < Itr->second.AsMobi)
        {
            Ret = false;
            player->SendSysMsg(GetGts(10201), Itr->second.AsMobi);
        }
    }//新增魔币

    if (Itr->second.Paodian > 0)//新增泡点
    {
        if (player->GetPaodian() < Itr->second.Paodian * Mod)
        {
            Ret = false;
            player->SendSysMsg("[系统]: 泡点不足，需要 |cFFFF6699%lld|r 泡点", Itr->second.Paodian * Mod);
        }
    }

    if (Itr->second.PLevel != 0)
    {
        // PLevel > 0: 需要等级 >= PLevel
        // PLevel < 0: 需要等级 <= |PLevel| (负数表示小于等于)
        int32 requiredLevel = Itr->second.PLevel;
        if (requiredLevel > 0)
        {
            // 正数：需要等级大于等于
            if (player->GetLevel() < requiredLevel)
            {
                Ret = false;
                player->SendSysMsg(GetGts(1017), requiredLevel);
            }
        }
        else
        {
            // 负数：需要等级小于等于
            int32 maxLevel = -requiredLevel;
            if (player->GetLevel() > maxLevel)
            {
                Ret = false;
                player->SendSysMsg("需要等级小于等于 %d 级", maxLevel);
            }
        }
    }

    if (Itr->second.Hr > 0)
    {
        if (player->GetHonorPoints() < Itr->second.Hr * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1018), Itr->second.Hr * Mod);
        }
    }

    if (Itr->second.An > 0)
    {
        if (player->GetArenaPoints() < Itr->second.An * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1019), Itr->second.An * Mod);
        }
    }

    if (Itr->second.Money > 0)
    {
        uint32 Need = uint32(Itr->second.Money * GOLD * Mod);
        if (!Need)
            Need = 0x7fffffff;

        if (!player->HasEnoughMoney(Need))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1020), Itr->second.Money * Mod);
        }
    }

    if (Itr->second.Class > 0)
    {
        if (!IsClassInMask(player->GetClass(), Itr->second.Class))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1021), PullMaskClassName(Itr->second.Class).c_str());
        }
    }

    if (Itr->second.Race > 0)
    {
        if (!IsRaceInMask(player->GetRace(), Itr->second.Race))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1022), PullMaskRaceName(Itr->second.Race).c_str());
        }
    }

    if (Itr->second.Team > 0)
    {
        if (player->GetTeamId() != (Itr->second.Team - 1))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1023), GetNtTeamStr(Itr->second.Team).c_str());
        }
    }

    if (Itr->second.FID > 0)
    {
        if ((uint32(player->GetReputationRank(Itr->second.FID)) < Itr->second.FRank))
        {
            Ret = false;
            player->SendSysMsg(GetGts(7200), GetGtsFactionName(Itr->second.FID).c_str(), GetFactionRankName(Itr->second.FRank).c_str());
        }
    }

    if (Itr->second.MapId > 0)
    {
        if (player->GetMapId() != Itr->second.MapId)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1024), GetMapName(Itr->second.MapId));
        }
    }

    if (Itr->second.AreaId > 0)
    {
        if (player->GetAreaId() != Itr->second.AreaId)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1025), GetAreaName(Itr->second.AreaId));
        }
    }

    if (Itr->second.AchCount > 0)
    {
        if (player->GetAchievementPoints() < Itr->second.AchCount)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1026), Itr->second.AchCount);
        }
    }

    if (Itr->second.Title > 0)
    {
        uint32 NeedTitle = (player->GetTeamId() == TEAM_ALLIANCE) ? Itr->second.Title : 14 + Itr->second.Title;
        if (CharTitlesEntry const* TInfo = sCharTitlesStore.LookupEntry(NeedTitle))
        {
            if (!player->HasTitle(TInfo))
            {
                Ret = false;
                player->SendSysMsg(GetGts(2901), Itr->second.Title);
            }
        }
        else
        {
            Ret = false;
            player->SendSysMsg(GetGts(2901), Itr->second.Title);
        }
    }

    if (Itr->second.StatRank > 0)
    {
        if (uint32(player->GetTempAt(AT11)) < Itr->second.StatRank)
        {
            Ret = false;
            player->SendSysMsg(GetGts(2902), Itr->second.StatRank);
        }
    }

    Tokenizer AuraData(Itr->second.Auras, '#');
    for (auto const& Temp : AuraData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
        {
            if (!player->HasAura(atoi(Temp)))
            {
                Ret = false;
                player->SendSysMsg(GetGts(1027), GetSpellName(atoi(Temp)));
            }
        }
    }

    Tokenizer ComAnchData(Itr->second.ComAnch, '#');
    for (auto const& Temp : ComAnchData)
    {
        if (AchievementEntry const* AEntry = sAchievementStore.LookupEntry(atoi(Temp)))
        {
            if (!player->HasAchieved(atoi(Temp)))
            {
                Ret = false;
                player->SendSysMsg(GetGts(1028), GetAchievementName(atoi(Temp)));
            }
        }
    }

    Tokenizer ComQuestData(Itr->second.ComQuest, '#');
    for (auto const& Temp : ComQuestData)
    {
        if (Quest const* QInfo = sObjectMgr->GetQuestTemplate(atoi(Temp)))
        {
            if (!player->GetQuestRewardStatus(atoi(Temp)))
            {
                Ret = false;
                player->SendSysMsg(GetGts(1029), GetQuestName(atoi(Temp)));
            }
        }
    }

    if (Itr->second.StatId > 0 && Itr->second.StatValue > 0)
    {
        if (!IsStatInMask(player, Itr->second.StatId, Itr->second.StatValue))
        {
            Ret = false;
            player->SendSysMsg(GetGts(3502), PullMaskStatName(Itr->second.StatId).c_str(), Itr->second.StatValue);
        }
    }

    Tokenizer InSpellData(Itr->second.InSpell, '#');
    for (auto const& Temp : InSpellData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
        {
            if (!player->HasSpell(atoi(Temp)))
            {
                Ret = false;
                player->SendSysMsg(GetGts(3501), GetSpellName(atoi(Temp)));
            }
        }
    }

    if (Itr->second.NsRank > 0)
    {
        if (player->GetTempAt(AT4001) < Itr->second.NsRank)
        {
            Ret = false;
            player->SendSysMsg(GetGts(4001), Itr->second.NsRank);
        }
    }

    if (Itr->second.AsEqRank > 0)
    {
        if (!sPlayerModMgr->IsAsEqRank(player, Itr->second.AsEqRank))
        {
            Ret = false;
            player->SendSysMsg(GetGts(6011), Itr->second.AsEqRank);
        }
    }

    if (Itr->second.Reincarnation > 0)
    {
        if (player->reincarnationLv < Itr->second.Reincarnation)
        {
            Ret = false;
            player->SendSysMsg("需要转生等级达到 [%u]", Itr->second.Reincarnation);
        }
    }

    if (Ret)
    {
        for (size_t i = 0; i < MNC; i++)
        {
            if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            {
                if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
                {
                    player->DestroyItemCount(Itr->second.ItemId[i], Itr->second.Count[i] * Mod, true);
                    player->SendSysMsg(GetGts(1030), GetItemLink(Itr->second.ItemId[i]).c_str(), Itr->second.Count[i] * Mod);
                }
            }
        }

        if (Itr->second.Hr > 0)
        {
            player->ModifyHonorPoints(-int32(Itr->second.Hr * Mod));
            player->SendSysMsg(GetGts(1031), Itr->second.Hr * Mod);
        }
        if (Itr->second.An > 0)
        {
            player->ModifyArenaPoints(-int32(Itr->second.An * Mod));
            player->SendSysMsg(GetGts(1032), Itr->second.An * Mod);
        }
        if (Itr->second.Token > 0)
            player->AddToken(-int64(Itr->second.Token * Mod), Info);

        if (Itr->second.Mobi > 0)
            player->AddTokenMB(-int64(Itr->second.Mobi * Mod), Info);

        if (Itr->second.Paodian > 0)
            player->AddPaodian(-int64(Itr->second.Paodian * Mod), Info);

        if (Itr->second.Money > 0)
        {
            uint32 Need = uint32(Itr->second.Money * GOLD * Mod);
            if (!Need)
                Need = 0x7fffffff;

            player->ModifyMoney(-int32(Need));
            player->SendSysMsg(GetGts(1033), Itr->second.Money * Mod);
        }
    }

    return Ret;
}

std::string Helper::GetNeedTalentGts(Player* player, uint32 UseTalentSpellId, uint32 TalentRank, uint32 Mod)
{
    std::ostringstream Nss;

    if (player->GetFreeTalentPoints() == 0)
    {
        Nss << "";
        return Nss.str();
    }

    if (TalentRank >= MAX_TALENT_RANK)
    {
        Nss << "";
        return Nss.str();
    }

    TalentEntry const* TalentInfo = sTalentStore.LookupEntry(UseTalentSpellId);
    if (!TalentInfo)
    {
        Nss << "";
        return Nss.str();
    }

    TalentTabEntry const* TalentTabInfo = sTalentTabStore.LookupEntry(TalentInfo->TalentTab);
    if (!TalentTabInfo)
    {
        Nss << "";
        return Nss.str();
    }

    if ((player->getClassMask() & TalentTabInfo->ClassMask) == 0)
    {
        Nss << "";
        return Nss.str();
    }

    uint32 SpellId = TalentInfo->RankID[TalentRank];
    if (SpellId == 0)
    {
        Nss << "";
        return Nss.str();
    }

    SpellInfo const* SInfo = sSpellMgr->GetSpellInfo(SpellId);
    if (!SInfo)
    {
        Nss << "";
        return Nss.str();
    }

    auto Itt = CustomTalentTempStore.find(SpellId);
    if (Itt == CustomTalentTempStore.end())
    {
        Nss << "";
        return Nss.str();
    }

    if (Itt->second.NeedTabPoints > 0)
        Nss << "|cFF00FF00需要当前天赋系 [" << TalentTabInfo->TalentTabID << "] |cFF00FF00达到 " << Itt->second.NeedTabPoints << " 点|r\n";

    if (Itt->second.NeedAsPoints > 0)
        Nss << "|cFF00FF00需要所有已投入天赋点数达到 " << Itt->second.NeedAsPoints << " 点|r\n";

    uint32 TempId = Itt->second.NTempId;
    if (!TempId)
    {
        Nss << "";
        return Nss.str();
    }

    auto Itr = NeedTempStore.find(TempId);
    if (Itr == NeedTempStore.end())
    {
        Nss << "";
        return Nss.str();
    }

    bool NoItem = true;
    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            NoItem = false;
    }

    bool NoNeed = (NoItem
        && Itr->second.VipLevel == 0
        && Itr->second.Token == 0
        && Itr->second.AsToken == 0
        && Itr->second.PLevel == 0
        && Itr->second.Hr == 0
        && Itr->second.An == 0
        && Itr->second.Money == 0
        && Itr->second.Class == 0
        && Itr->second.Race == 0
        && Itr->second.Team == 0
        && Itr->second.FID == 0
        && Itr->second.MapId == 0
        && Itr->second.AreaId == 0
        && Itr->second.AchCount == 0
        && Itr->second.Title == 0
        && Itr->second.StatRank == 0
        && Itr->second.Auras.empty()
        && Itr->second.ComAnch.empty()
        && Itr->second.ComQuest.empty()
        && Itr->second.StatId == 0
        && Itr->second.StatValue == 0
        && Itr->second.InSpell.empty()
        && Itr->second.NsRank == 0
        && Itr->second.AsEqRank == 0
        && Itr->second.Mobi == 0//新增魔币
        && Itr->second.AsMobi == 0//新增魔币
        );
    if (NoNeed)
    {
        Nss << "\n";
        return Nss.str();
    }

    Nss << "\n";

    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
        {
            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
                Nss << "|cFF00FF00需要材料|r " << GetItemLink(Itr->second.ItemId[i]) << " X |cFFFF0000" << Itr->second.Count[i] * Mod << "|r\n";
        }
    }

    if (Itr->second.VipLevel > 0)
        Nss << "|cFF00FF00需要达到会员等级|r [|cFFFF0000" << Itr->second.VipLevel << "|r]\n";

    if (Itr->second.Token > 0)
        Nss << "|cFF00FF00需要消耗积分|r |cFFFF6699" << Itr->second.Token * Mod << "|r\n";

    if (Itr->second.AsToken > 0)
        Nss << "|cFF00FF00需要达到累计积分|r |cFFFF6699" << Itr->second.AsToken << "|r\n";

    if (Itr->second.Mobi > 0)
        Nss << "|cFF00FF00需要消耗魔币|r |cFFFF6699" << Itr->second.Mobi * Mod << "|r\n";

    if (Itr->second.AsMobi > 0)
        Nss << "|cFF00FF00需要达到累计魔币|r |cFFFF6699" << Itr->second.AsMobi << "|r\n";

    if (Itr->second.PLevel > 0)
        Nss << "|cFF00FF00需要达到人物等级|r [|cFFFF0000" << Itr->second.PLevel << "|r]\n";

    if (Itr->second.Hr > 0)
        Nss << "|cFF00FF00需要消耗荣誉点数|r X |cFFFF0000" << Itr->second.Hr * Mod << "|r\n";

    if (Itr->second.An > 0)
        Nss << "|cFF00FF00需要消耗竞技点数|r X |cFFFF0000" << Itr->second.An * Mod << "|r\n";

    if (Itr->second.Money > 0)
        Nss << "|cFF00FF00需要消耗金币|r X |cFFFF0000" << Itr->second.Money * Mod << "|r\n";

    if (Itr->second.Class > 0)
        Nss << "|cFF00FF00需要职业|r [|cFFFF0000" << PullMaskClassName(Itr->second.Class) << "|r]\n";

    if (Itr->second.Race > 0)
        Nss << "|cFF00FF00需要种族|r [|cFFFF0000" << PullMaskRaceName(Itr->second.Race) << "|r]\n";

    if (Itr->second.Team > 0)
        Nss << "|cFF00FF00需要阵营|r [|cFFFF0000" << GetNtTeamStr(Itr->second.Team) << "|r]\n";

    if (Itr->second.FID > 0)
        Nss << "|cFF00FF00需要声望|r [|cFFFF0000" << GetGtsFactionName(Itr->second.FID) << "|r] 达到 [|cFF00FF00" << GetFactionRankName(Itr->second.FRank) << "|r]\n";

    if (Itr->second.MapId >= 0)
        Nss << "|cFF00FF00需要位于地图|r [|cFFFF0000" << GetMapName(Itr->second.MapId) << "|r]\n";

    if (Itr->second.AreaId > 0)
        Nss << "|cFF00FF00需要位于区域|r [|cFFFF0000" << GetAreaName(Itr->second.AreaId) << "|r]\n";

    if (Itr->second.AchCount > 0)
        Nss << "|cFF00FF00需要达到成就点数|r [|cFFFF0000" << Itr->second.AchCount << "点|r]\n";

    if (Itr->second.Title > 0)
        Nss << "|cFF00FF00需要军衔等级|r [|cFFFF0000" << GetTitleName(Itr->second.Title) << "|r]\n";

    if (Itr->second.StatRank > 0)
        Nss << "|cFF00FF00需要达到斗气等级|r [|cFFFF0000" << Itr->second.StatRank << "|r]\n";

    Tokenizer AuraData(Itr->second.Auras, '#');
    for (auto const& Temp : AuraData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
            Nss << "|cFF00FF00需要拥有光环Buff|r " << GetSpellName(atoi(Temp)) << "\n";
    }

    Tokenizer ComAnchData(Itr->second.ComAnch, '#');
    for (auto const& Temp : ComAnchData)
    {
        if (AchievementEntry const* AEntry = sAchievementStore.LookupEntry(atoi(Temp)))
            Nss << "|cFF00FF00需要完成成就|r " << GetAchievementName(atoi(Temp)) << "\n";
    }

    Tokenizer ComQuestData(Itr->second.ComQuest, '#');
    for (auto const& Temp : ComQuestData)
    {
        if (Quest const* QInfo = sObjectMgr->GetQuestTemplate(atoi(Temp)))
            Nss << "|cFF00FF00需要完成任务|r " << GetQuestName(atoi(Temp)) << "\n";
    }

    if (Itr->second.StatId > 0 && Itr->second.StatValue > 0)
        Nss << "|cFF00FF00需要主属性|r [|cFFFF0000" << PullMaskStatName(Itr->second.StatId) << "|r] 达到 [|cFFFF0000" << Itr->second.StatValue << "|r]\n";

    Tokenizer InSpellData(Itr->second.InSpell, '#');
    for (auto const& Temp : InSpellData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
            Nss << "|cFF00FF00需要学会或拥有技能|r " << GetSpellName(atoi(Temp)) << "\n";
    }

    if (Itr->second.NsRank > 0)
        Nss << "|cFF00FF00需要达到赞助等级|r [|cFFFF0000" << Itr->second.NsRank << "|r]\n";

    if (Itr->second.AsEqRank > 0)
        Nss << "|cFF00FF00需要全身装备栏等级达到|r [|cFFFF0000" << Itr->second.AsEqRank << "|r]\n";

    Nss << "\n";

    return Nss.str();
}

bool Helper::CheckNeedTalent(Player* player, uint32 UseTalentSpellId, uint32 TalentRank, std::string Info, uint32 Mod)
{
    bool Ret = true;

    if (player->GetFreeTalentPoints() == 0)
        return Ret;

    if (TalentRank >= MAX_TALENT_RANK)
        return Ret;

    TalentEntry const* TalentInfo = sTalentStore.LookupEntry(UseTalentSpellId);
    if (!TalentInfo)
        return Ret;

    TalentTabEntry const* TalentTabInfo = sTalentTabStore.LookupEntry(TalentInfo->TalentTab);
    if (!TalentTabInfo)
        return Ret;

    if ((player->getClassMask() & TalentTabInfo->ClassMask) == 0)
        return Ret;

    uint32 SpellId = TalentInfo->RankID[TalentRank];
    if (SpellId == 0)
        return Ret;

    SpellInfo const* SInfo = sSpellMgr->GetSpellInfo(SpellId);
    if (!SInfo)
        return Ret;

    auto Itt = CustomTalentTempStore.find(SpellId);
    if (Itt == CustomTalentTempStore.end())
        return Ret;

    uint32 SpentPoints = 0;
    if (TalentInfo->Row > 0)
    {
        const PlayerTalentMap& PTalentMap = player->GetTalentMap();
        for (PlayerTalentMap::const_iterator It = PTalentMap.begin(); It != PTalentMap.end(); ++It)
            if (TalentSpellPos const* TalentPos = GetTalentSpellPos(It->first))
                if (TalentEntry const* ItTalentInfo = sTalentStore.LookupEntry(TalentPos->talent_id))
                    if (ItTalentInfo->TalentTab == TalentInfo->TalentTab)
                        if (It->second->State != PLAYERSPELL_REMOVED && It->second->IsInSpec(player->GetActiveSpec()))
                            SpentPoints += TalentPos->rank + 1;
    }

    if (SpentPoints < Itt->second.NeedTabPoints)
    {
        player->SendSysMsg(GetGts(9301), TalentTabInfo->TalentTabID, Itt->second.NeedTabPoints);
        return false;
    }

    if (player->GetUsedTalentCount() < Itt->second.NeedAsPoints)
    {
        player->SendSysMsg(GetGts(9302), Itt->second.NeedAsPoints);
        return false;
    }

    uint32 HcSpellId = sPlayerModMgr->InNotTalentInfo(player, SpellId);
    if (HcSpellId)
    {
        player->SendSysMsg(GetGts(9303), sHelper->GetSpellName(HcSpellId));
        return false;
    }

    uint32 TempId = Itt->second.NTempId;
    if (!TempId)
        return Ret;

    auto Itr = NeedTempStore.find(TempId);
    if (Itr == NeedTempStore.end())
        return Ret;

    bool NoItem = true;
    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            NoItem = false;
    }

    bool NoNeed = (NoItem
        && Itr->second.VipLevel == 0
        && Itr->second.Token == 0
        && Itr->second.AsToken == 0
        && Itr->second.PLevel == 0
        && Itr->second.Hr == 0
        && Itr->second.An == 0
        && Itr->second.Money == 0
        && Itr->second.Class == 0
        && Itr->second.Race == 0
        && Itr->second.Team == 0
        && Itr->second.FID == 0
        && Itr->second.MapId == 0
        && Itr->second.AreaId == 0
        && Itr->second.AchCount == 0
        && Itr->second.Title == 0
        && Itr->second.StatRank == 0
        && Itr->second.Auras.empty()
        && Itr->second.ComAnch.empty()
        && Itr->second.ComQuest.empty()
        && Itr->second.StatId == 0
        && Itr->second.StatValue == 0
        && Itr->second.InSpell.empty()
        && Itr->second.NsRank == 0
        && Itr->second.AsEqRank == 0
        && Itr->second.Mobi == 0//新增魔币
        && Itr->second.AsMobi == 0//新增魔币
        );

    if (NoNeed)
        return Ret;

    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
        {
            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
            {
                if (!player->HasItemCount(Itr->second.ItemId[i], Itr->second.Count[i] * Mod))
                {
                    Ret = false;
                    player->SendSysMsg(GetGts(1011), GetItemLink(Itr->second.ItemId[i]).c_str(), Itr->second.Count[i] * Mod);
                }
            }
        }
    }

    if (Itr->second.VipLevel > 0)
    {
        if (player->GetVipRank() < Itr->second.VipLevel)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1012), Itr->second.VipLevel);
        }
    }

    if (Itr->second.Token > 0)
    {
        if (player->GetToken() < Itr->second.Token * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1013), Itr->second.Token * Mod);
        }
    }

    if (Itr->second.AsToken > 0)
    {
        if (player->GetAsToken() < Itr->second.AsToken)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1014), Itr->second.AsToken);
        }
    }

    if (Itr->second.Mobi > 0)
    {
        if (player->GetTokenMB() < Itr->second.Mobi * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(10200), Itr->second.Mobi * Mod);
        }
    }

    if (Itr->second.AsMobi > 0)
    {
        if (player->GetAsTokenMB() < Itr->second.AsMobi)
        {
            Ret = false;
            player->SendSysMsg(GetGts(10201), Itr->second.AsMobi);
        }
    }

    if (Itr->second.PLevel != 0)
    {
        // PLevel > 0: 需要等级 >= PLevel
        // PLevel < 0: 需要等级 <= |PLevel| (负数表示小于等于)
        int32 requiredLevel = Itr->second.PLevel;
        if (requiredLevel > 0)
        {
            // 正数：需要等级大于等于
            if (player->GetLevel() < requiredLevel)
            {
                Ret = false;
                player->SendSysMsg(GetGts(1017), requiredLevel);
            }
        }
        else
        {
            // 负数：需要等级小于等于
            int32 maxLevel = -requiredLevel;
            if (player->GetLevel() > maxLevel)
            {
                Ret = false;
                player->SendSysMsg("需要等级小于等于 %d 级", maxLevel);
            }
        }
    }

    if (Itr->second.Hr > 0)
    {
        if (player->GetHonorPoints() < Itr->second.Hr * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1018), Itr->second.Hr * Mod);
        }
    }

    if (Itr->second.An > 0)
    {
        if (player->GetArenaPoints() < Itr->second.An * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1019), Itr->second.An * Mod);
        }
    }

    if (Itr->second.Money > 0)
    {
        uint32 Need = uint32(Itr->second.Money * GOLD * Mod);
        if (!Need)
            Need = 0x7fffffff;

        if (!player->HasEnoughMoney(Need))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1020), Itr->second.Money * Mod);
        }
    }

    if (Itr->second.Class > 0)
    {
        if (!IsClassInMask(player->GetClass(), Itr->second.Class))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1021), PullMaskClassName(Itr->second.Class).c_str());
        }
    }

    if (Itr->second.Race > 0)
    {
        if (!IsRaceInMask(player->GetRace(), Itr->second.Race))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1022), PullMaskRaceName(Itr->second.Race).c_str());
        }
    }

    if (Itr->second.Team > 0)
    {
        if (player->GetTeamId() != (Itr->second.Team - 1))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1023), GetNtTeamStr(Itr->second.Team).c_str());
        }
    }

    if (Itr->second.FID > 0)
    {
        if ((uint32(player->GetReputationRank(Itr->second.FID)) < Itr->second.FRank))
        {
            Ret = false;
            player->SendSysMsg(GetGts(7200), GetGtsFactionName(Itr->second.FID).c_str(), GetFactionRankName(Itr->second.FRank).c_str());
        }
    }

    if (Itr->second.MapId > 0)
    {
        if (player->GetMapId() != Itr->second.MapId)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1024), GetMapName(Itr->second.MapId));
        }
    }

    if (Itr->second.AreaId > 0)
    {
        if (player->GetAreaId() != Itr->second.AreaId)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1025), GetAreaName(Itr->second.AreaId));
        }
    }

    if (Itr->second.AchCount > 0)
    {
        if (player->GetAchievementPoints() < Itr->second.AchCount)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1026), Itr->second.AchCount);
        }
    }

    if (Itr->second.Title > 0)
    {
        uint32 NeedTitle = (player->GetTeamId() == TEAM_ALLIANCE) ? Itr->second.Title : 14 + Itr->second.Title;
        if (CharTitlesEntry const* TInfo = sCharTitlesStore.LookupEntry(NeedTitle))
        {
            if (!player->HasTitle(TInfo))
            {
                Ret = false;
                player->SendSysMsg(GetGts(2901), Itr->second.Title);
            }
        }
        else
        {
            Ret = false;
            player->SendSysMsg(GetGts(2901), Itr->second.Title);
        }
    }

    if (Itr->second.StatRank > 0)
    {
        if (uint32(player->GetTempAt(AT11)) < Itr->second.StatRank)
        {
            Ret = false;
            player->SendSysMsg(GetGts(2902), Itr->second.StatRank);
        }
    }

    Tokenizer AuraData(Itr->second.Auras, '#');
    for (auto const& Temp : AuraData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
        {
            if (!player->HasAura(atoi(Temp)))
            {
                Ret = false;
                player->SendSysMsg(GetGts(1027), GetSpellName(atoi(Temp)));
            }
        }
    }

    Tokenizer ComAnchData(Itr->second.ComAnch, '#');
    for (auto const& Temp : ComAnchData)
    {
        if (AchievementEntry const* AEntry = sAchievementStore.LookupEntry(atoi(Temp)))
        {
            if (!player->HasAchieved(atoi(Temp)))
            {
                Ret = false;
                player->SendSysMsg(GetGts(1028), GetAchievementName(atoi(Temp)));
            }
        }
    }

    Tokenizer ComQuestData(Itr->second.ComQuest, '#');
    for (auto const& Temp : ComQuestData)
    {
        if (Quest const* QInfo = sObjectMgr->GetQuestTemplate(atoi(Temp)))
        {
            if (!player->GetQuestRewardStatus(atoi(Temp)))
            {
                Ret = false;
                player->SendSysMsg(GetGts(1029), GetQuestName(atoi(Temp)));
            }
        }
    }

    if (Itr->second.StatId > 0 && Itr->second.StatValue > 0)
    {
        if (!IsStatInMask(player, Itr->second.StatId, Itr->second.StatValue))
        {
            Ret = false;
            player->SendSysMsg(GetGts(3502), PullMaskStatName(Itr->second.StatId).c_str(), Itr->second.StatValue);
        }
    }

    Tokenizer InSpellData(Itr->second.InSpell, '#');
    for (auto const& Temp : InSpellData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
        {
            if (!player->HasSpell(atoi(Temp)))
            {
                Ret = false;
                player->SendSysMsg(GetGts(3501), GetSpellName(atoi(Temp)));
            }
        }
    }

    if (Itr->second.NsRank > 0)
    {
        if (player->GetTempAt(AT4001) < Itr->second.NsRank)
        {
            Ret = false;
            player->SendSysMsg(GetGts(4001), Itr->second.NsRank);
        }
    }

    if (Itr->second.AsEqRank > 0)
    {
        if (!sPlayerModMgr->IsAsEqRank(player, Itr->second.AsEqRank))
        {
            Ret = false;
            player->SendSysMsg(GetGts(6011), Itr->second.AsEqRank);
        }
    }

    if (Ret)
    {
        for (size_t i = 0; i < MNC; i++)
        {
            if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            {
                if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
                {
                    player->DestroyItemCount(Itr->second.ItemId[i], Itr->second.Count[i] * Mod, true);
                    player->SendSysMsg(GetGts(1030), GetItemLink(Itr->second.ItemId[i]).c_str(), Itr->second.Count[i] * Mod);
                }
            }
        }

        if (Itr->second.Hr > 0)
        {
            player->ModifyHonorPoints(-int32(Itr->second.Hr * Mod));
            player->SendSysMsg(GetGts(1031), Itr->second.Hr * Mod);
        }
        if (Itr->second.An > 0)
        {
            player->ModifyArenaPoints(-int32(Itr->second.An * Mod));
            player->SendSysMsg(GetGts(1032), Itr->second.An * Mod);
        }
        if (Itr->second.Token > 0)
            player->AddToken(-int64(Itr->second.Token * Mod), Info);

        if (Itr->second.Mobi > 0)
            player->AddTokenMB(-int64(Itr->second.Mobi * Mod), Info);

        if (Itr->second.Money > 0)
        {
            uint32 Need = uint32(Itr->second.Money * GOLD * Mod);
            if (!Need)
                Need = 0x7fffffff;

            player->ModifyMoney(-int32(Need));
            player->SendSysMsg(GetGts(1033), Itr->second.Money * Mod);
        }
    }

    return Ret;
}

bool Helper::AtCheckNeed(Player* player, uint32 TempId)
{
    bool Ret = true;

    if (!TempId)
        return Ret;

    auto Itr = NeedTempStore.find(TempId);
    if (Itr == NeedTempStore.end())
        return Ret;

    bool NoNeed = (Itr->second.VipLevel == 0
        && Itr->second.PLevel == 0
        && Itr->second.Class == 0
        && Itr->second.Race == 0
        && Itr->second.Team == 0
        && Itr->second.FID == 0
        && Itr->second.MapId == 0
        && Itr->second.AreaId == 0
        && Itr->second.AchCount == 0
        && Itr->second.Title == 0
        && Itr->second.StatRank == 0
        && Itr->second.Auras.empty()
        && Itr->second.ComAnch.empty()
        && Itr->second.ComQuest.empty()
        && Itr->second.StatId == 0
        && Itr->second.StatValue == 0
        && Itr->second.NsRank == 0
        && Itr->second.AsEqRank == 0
        && Itr->second.Reincarnation == 0
        && Itr->second.InSpell.empty()
        );

    if (NoNeed)
        return Ret;

    if (Itr->second.VipLevel > 0)
    {
        if (player->GetVipRank() < Itr->second.VipLevel)
        {
            Ret = false;
        }
    }

    if (Itr->second.PLevel > 0)
    {
        if (player->GetLevel() < Itr->second.PLevel)
        {
            Ret = false;
        }
    }

    if (Itr->second.Class > 0)
    {
        if (!IsClassInMask(player->GetClass(), Itr->second.Class))
        {
            Ret = false;
        }
    }

    if (Itr->second.Race > 0)
    {
        if (!IsRaceInMask(player->GetRace(), Itr->second.Race))
        {
            Ret = false;
        }
    }

    if (Itr->second.Team > 0)
    {
        if (player->GetTeamId() != (Itr->second.Team - 1))
        {
            Ret = false;
        }
    }

    if (Itr->second.FID > 0)
    {
        if ((uint32(player->GetReputationRank(Itr->second.FID)) < Itr->second.FRank))
        {
            Ret = false;
        }
    }

    if (Itr->second.MapId > 0)
    {
        if (player->GetMapId() != Itr->second.MapId)
        {
            Ret = false;
        }
    }

    if (Itr->second.AreaId > 0)
    {
        if (player->GetAreaId() != Itr->second.AreaId)
        {
            Ret = false;
        }
    }

    if (Itr->second.AchCount > 0)
    {
        if (player->GetAchievementPoints() < Itr->second.AchCount)
        {
            Ret = false;
        }
    }

    if (Itr->second.Title > 0)
    {
        uint32 NeedTitle = (player->GetTeamId() == TEAM_ALLIANCE) ? Itr->second.Title : 14 + Itr->second.Title;
        if (CharTitlesEntry const* TInfo = sCharTitlesStore.LookupEntry(NeedTitle))
        {
            if (!player->HasTitle(TInfo))
            {
                Ret = false;
            }
        }
        else
        {
            Ret = false;
        }
    }

    if (Itr->second.StatRank > 0)
    {
        if (uint32(player->GetTempAt(AT11)) < Itr->second.StatRank)
        {
            Ret = false;
        }
    }

    Tokenizer AuraData(Itr->second.Auras, '#');
    for (auto const& Temp : AuraData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
        {
            if (!player->HasAura(atoi(Temp)))
            {
                Ret = false;
            }
        }
    }

    Tokenizer ComAnchData(Itr->second.ComAnch, '#');
    for (auto const& Temp : ComAnchData)
    {
        if (AchievementEntry const* AEntry = sAchievementStore.LookupEntry(atoi(Temp)))
        {
            if (!player->HasAchieved(atoi(Temp)))
            {
                Ret = false;
            }
        }
    }

    Tokenizer ComQuestData(Itr->second.ComQuest, '#');
    for (auto const& Temp : ComQuestData)
    {
        if (Quest const* QInfo = sObjectMgr->GetQuestTemplate(atoi(Temp)))
        {
            if (!player->GetQuestRewardStatus(atoi(Temp)))
            {
                Ret = false;
            }
        }
    }

    if (Itr->second.StatId > 0 && Itr->second.StatValue > 0)
    {
        if (!IsStatInMask(player, Itr->second.StatId, Itr->second.StatValue))
        {
            Ret = false;
        }
    }

    Tokenizer InSpellData(Itr->second.InSpell, '#');
    for (auto const& Temp : InSpellData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
        {
            if (!player->HasSpell(atoi(Temp)))
            {
                Ret = false;
            }
        }
    }

    if (Itr->second.NsRank > 0)
    {
        if (player->GetTempAt(AT4001) < Itr->second.NsRank)
        {
            Ret = false;
        }
    }

    if (Itr->second.AsEqRank > 0)
    {
        if (!sPlayerModMgr->IsAsEqRank(player, Itr->second.AsEqRank))
        {
            Ret = false;
        }
    }

    if (Itr->second.Reincarnation > 0)
    {
        if (player->reincarnationLv < Itr->second.Reincarnation)
        {
            Ret = false;
        }
    }

    return Ret;
}

bool Helper::NotDeleteCheckNeed(Player* player, uint32 TempId, uint32 Mod)
{
    bool Ret = true;

    if (!TempId)
        return Ret;

    auto Itr = NeedTempStore.find(TempId);
    if (Itr == NeedTempStore.end())
        return Ret;

    bool NoItem = true;
    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            NoItem = false;
    }

    bool NoNeed = (NoItem
        && Itr->second.VipLevel == 0
        && Itr->second.Token == 0
        && Itr->second.AsToken == 0
        && Itr->second.PLevel == 0
        && Itr->second.Hr == 0
        && Itr->second.An == 0
        && Itr->second.Money == 0
        && Itr->second.Class == 0
        && Itr->second.Race == 0
        && Itr->second.Team == 0
        && Itr->second.FID == 0
        && Itr->second.MapId == 0
        && Itr->second.AreaId == 0
        && Itr->second.AchCount == 0
        && Itr->second.Title == 0
        && Itr->second.StatRank == 0
        && Itr->second.Auras.empty()
        && Itr->second.ComAnch.empty()
        && Itr->second.ComQuest.empty()
        && Itr->second.StatId == 0
        && Itr->second.StatValue == 0
        && Itr->second.InSpell.empty()
        && Itr->second.NsRank == 0
        && Itr->second.AsEqRank == 0
        && Itr->second.Reincarnation == 0
        && Itr->second.Mobi == 0
        && Itr->second.AsMobi == 0
        && Itr->second.Mobi == 0
        && Itr->second.AsMobi == 0
        );

    if (NoNeed)
        return Ret;

    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
        {
            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
            {
                if (!player->HasItemCount(Itr->second.ItemId[i], Itr->second.Count[i] * Mod))
                {
                    Ret = false;
                    player->SendSysMsg(GetGts(1011), GetItemLink(Itr->second.ItemId[i]).c_str(), Itr->second.Count[i] * Mod);
                }
            }
        }
    }

    if (Itr->second.VipLevel > 0)
    {
        if (player->GetVipRank() < Itr->second.VipLevel)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1012), Itr->second.VipLevel);
        }
    }

    if (Itr->second.Token > 0)
    {
        if (player->GetToken() < Itr->second.Token * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1013), Itr->second.Token * Mod);
        }
    }

    if (Itr->second.AsToken > 0)
    {
        if (player->GetAsToken() < Itr->second.AsToken)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1014), Itr->second.AsToken);
        }
    }
    if (Itr->second.Mobi > 0)
    {
        if (player->GetTokenMB() < Itr->second.Mobi * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(10200), Itr->second.Mobi * Mod);
        }
    }

    if (Itr->second.AsMobi > 0)
    {
        if (player->GetAsTokenMB() < Itr->second.AsMobi)
        {
            Ret = false;
            player->SendSysMsg(GetGts(10201), Itr->second.AsMobi);
        }
    }
    if (Itr->second.PLevel != 0)
    {
        // PLevel > 0: 需要等级 >= PLevel
        // PLevel < 0: 需要等级 <= |PLevel| (负数表示小于等于)
        int32 requiredLevel = Itr->second.PLevel;
        if (requiredLevel > 0)
        {
            // 正数：需要等级大于等于
            if (player->GetLevel() < requiredLevel)
            {
                Ret = false;
                player->SendSysMsg(GetGts(1017), requiredLevel);
            }
        }
        else
        {
            // 负数：需要等级小于等于
            int32 maxLevel = -requiredLevel;
            if (player->GetLevel() > maxLevel)
            {
                Ret = false;
                player->SendSysMsg("需要等级小于等于 %d 级", maxLevel);
            }
        }
    }

    if (Itr->second.Hr > 0)
    {
        if (player->GetHonorPoints() < Itr->second.Hr * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1018), Itr->second.Hr * Mod);
        }
    }

    if (Itr->second.An > 0)
    {
        if (player->GetArenaPoints() < Itr->second.An * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1019), Itr->second.An * Mod);
        }
    }

    if (Itr->second.Money > 0)
    {
        uint32 Need = uint32(Itr->second.Money * GOLD * Mod);
        if (!Need)
            Need = 0x7fffffff;

        if (!player->HasEnoughMoney(Need))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1020), Itr->second.Money * Mod);
        }
    }

    if (Itr->second.Class > 0)
    {
        if (!IsClassInMask(player->GetClass(), Itr->second.Class))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1021), PullMaskClassName(Itr->second.Class).c_str());
        }
    }

    if (Itr->second.Race > 0)
    {
        if (!IsRaceInMask(player->GetRace(), Itr->second.Race))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1022), PullMaskRaceName(Itr->second.Race).c_str());
        }
    }

    if (Itr->second.Team > 0)
    {
        if (player->GetTeamId() != (Itr->second.Team - 1))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1023), GetNtTeamStr(Itr->second.Team).c_str());
        }
    }

    if (Itr->second.FID > 0)
    {
        if ((uint32(player->GetReputationRank(Itr->second.FID)) < Itr->second.FRank))
        {
            Ret = false;
            player->SendSysMsg(GetGts(7200), GetGtsFactionName(Itr->second.FID).c_str(), GetFactionRankName(Itr->second.FRank).c_str());
        }
    }

    if (Itr->second.MapId > 0)
    {
        if (player->GetMapId() != Itr->second.MapId)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1024), GetMapName(Itr->second.MapId));
        }
    }

    if (Itr->second.AreaId > 0)
    {
        if (player->GetAreaId() != Itr->second.AreaId)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1025), GetAreaName(Itr->second.AreaId));
        }
    }

    if (Itr->second.AchCount > 0)
    {
        if (player->GetAchievementPoints() < Itr->second.AchCount)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1026), Itr->second.AchCount);
        }
    }

    if (Itr->second.Title > 0)
    {
        uint32 NeedTitle = (player->GetTeamId() == TEAM_ALLIANCE) ? Itr->second.Title : 14 + Itr->second.Title;
        if (CharTitlesEntry const* TInfo = sCharTitlesStore.LookupEntry(NeedTitle))
        {
            if (!player->HasTitle(TInfo))
            {
                Ret = false;
                player->SendSysMsg(GetGts(2901), Itr->second.Title);
            }
        }
        else
        {
            Ret = false;
            player->SendSysMsg(GetGts(2901), Itr->second.Title);
        }
    }

    if (Itr->second.StatRank > 0)
    {
        if (uint32(player->GetTempAt(AT11)) < Itr->second.StatRank)
        {
            Ret = false;
            player->SendSysMsg(GetGts(2902), Itr->second.StatRank);
        }
    }

    Tokenizer AuraData(Itr->second.Auras, '#');
    for (auto const& Temp : AuraData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
        {
            if (!player->HasAura(atoi(Temp)))
            {
                Ret = false;
                player->SendSysMsg(GetGts(1027), GetSpellName(atoi(Temp)));
            }
        }
    }

    Tokenizer ComAnchData(Itr->second.ComAnch, '#');
    for (auto const& Temp : ComAnchData)
    {
        if (AchievementEntry const* AEntry = sAchievementStore.LookupEntry(atoi(Temp)))
        {
            if (!player->HasAchieved(atoi(Temp)))
            {
                Ret = false;
                player->SendSysMsg(GetGts(1028), GetAchievementName(atoi(Temp)));
            }
        }
    }

    Tokenizer ComQuestData(Itr->second.ComQuest, '#');
    for (auto const& Temp : ComQuestData)
    {
        if (Quest const* QInfo = sObjectMgr->GetQuestTemplate(atoi(Temp)))
        {
            if (!player->GetQuestRewardStatus(atoi(Temp)))
            {
                Ret = false;
                player->SendSysMsg(GetGts(1029), GetQuestName(atoi(Temp)));
            }
        }
    }

    if (Itr->second.StatId > 0 && Itr->second.StatValue > 0)
    {
        if (!IsStatInMask(player, Itr->second.StatId, Itr->second.StatValue))
        {
            Ret = false;
            player->SendSysMsg(GetGts(3502), PullMaskStatName(Itr->second.StatId).c_str(), Itr->second.StatValue);
        }
    }

    Tokenizer InSpellData(Itr->second.InSpell, '#');
    for (auto const& Temp : InSpellData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
        {
            if (!player->HasSpell(atoi(Temp)))
            {
                Ret = false;
                player->SendSysMsg(GetGts(3501), GetSpellName(atoi(Temp)));
            }
        }
    }

    if (Itr->second.NsRank > 0)
    {
        if (player->GetTempAt(AT4001) < Itr->second.NsRank)
        {
            Ret = false;
            player->SendSysMsg(GetGts(4001), Itr->second.NsRank);
        }
    }

    if (Itr->second.AsEqRank > 0)
    {
        if (!sPlayerModMgr->IsAsEqRank(player, Itr->second.AsEqRank))
        {
            Ret = false;
            player->SendSysMsg(GetGts(6011), Itr->second.AsEqRank);
        }
    }

    if (Itr->second.Reincarnation > 0)
    {
        if (player->reincarnationLv < Itr->second.Reincarnation)
        {
            Ret = false;
            player->SendSysMsg("需要转生等级达到 [%u]", Itr->second.Reincarnation);
        }
    }

    return Ret;
}

bool Helper::ShowCheckNeed(Player* player, uint32 TempId, uint32 Mod)
{
    bool Ret = true;

    if (!TempId)
        return Ret;

    auto Itr = NeedTempStore.find(TempId);
    if (Itr == NeedTempStore.end())
        return Ret;

    bool NoItem = true;
    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            NoItem = false;
    }

    bool NoNeed = (NoItem
        && Itr->second.VipLevel == 0
        && Itr->second.Token == 0
        && Itr->second.AsToken == 0
        && Itr->second.PLevel == 0
        && Itr->second.Hr == 0
        && Itr->second.An == 0
        && Itr->second.Money == 0
        && Itr->second.Class == 0
        && Itr->second.Race == 0
        && Itr->second.Team == 0
        && Itr->second.FID == 0
        && Itr->second.MapId == 0
        && Itr->second.AreaId == 0
        && Itr->second.AchCount == 0
        && Itr->second.Title == 0
        && Itr->second.StatRank == 0
        && Itr->second.Auras.empty()
        && Itr->second.ComAnch.empty()
        && Itr->second.ComQuest.empty()
        && Itr->second.StatId == 0
        && Itr->second.StatValue == 0
        && Itr->second.InSpell.empty()
        && Itr->second.NsRank == 0
        && Itr->second.AsEqRank == 0
        && Itr->second.Reincarnation == 0
        && Itr->second.Mobi == 0
        && Itr->second.AsMobi == 0
        );

    if (NoNeed)
        return Ret;

    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
        {
            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
            {
                if (!player->HasItemCount(Itr->second.ItemId[i], Itr->second.Count[i] * Mod))
                    Ret = false;
            }
        }
    }

    if (Itr->second.VipLevel > 0)
        if (player->GetVipRank() < Itr->second.VipLevel)
            Ret = false;

    if (Itr->second.Token > 0)
        if (player->GetToken() < Itr->second.Token * Mod)
            Ret = false;

    if (Itr->second.AsToken > 0)
        if (player->GetAsToken() < Itr->second.AsToken)
            Ret = false;

    if (Itr->second.Mobi > 0)
        if (player->GetTokenMB() < Itr->second.Mobi * Mod)
            Ret = false;

    if (Itr->second.AsMobi > 0)
        if (player->GetAsTokenMB() < Itr->second.AsMobi)
            Ret = false;

    if (Itr->second.PLevel != 0)
    {
        int32 requiredLevel = Itr->second.PLevel;
        if (requiredLevel > 0)
        {
            if (player->GetLevel() < requiredLevel)
                Ret = false;
        }
        else
        {
            int32 maxLevel = -requiredLevel;
            if (player->GetLevel() > maxLevel)
                Ret = false;
        }
    }

    if (Itr->second.Hr > 0)
        if (player->GetHonorPoints() < Itr->second.Hr * Mod)
            Ret = false;

    if (Itr->second.An > 0)
        if (player->GetArenaPoints() < Itr->second.An * Mod)
            Ret = false;

    if (Itr->second.Money > 0)
    {
        uint32 Need = uint32(Itr->second.Money * GOLD * Mod);
        if (!Need)
            Need = 0x7fffffff;
        if (!player->HasEnoughMoney(Need))
            Ret = false;
    }

    if (Itr->second.Class > 0)
        if (!IsClassInMask(player->GetClass(), Itr->second.Class))
            Ret = false;

    if (Itr->second.Race > 0)
        if (!IsRaceInMask(player->GetRace(), Itr->second.Race))
            Ret = false;

    if (Itr->second.Team > 0)
        if (player->GetTeamId() != (Itr->second.Team - 1))
            Ret = false;

    if (Itr->second.FID > 0)
        if ((uint32(player->GetReputationRank(Itr->second.FID)) < Itr->second.FRank))
            Ret = false;

    if (Itr->second.MapId > 0)
        if (player->GetMapId() != Itr->second.MapId)
            Ret = false;

    if (Itr->second.AreaId > 0)
        if (player->GetAreaId() != Itr->second.AreaId)
            Ret = false;

    if (Itr->second.AchCount > 0)
        if (player->GetAchievementPoints() < Itr->second.AchCount)
            Ret = false;

    if (Itr->second.Title > 0)
    {
        uint32 NeedTitle = (player->GetTeamId() == TEAM_ALLIANCE) ? Itr->second.Title : 14 + Itr->second.Title;
        if (CharTitlesEntry const* TInfo = sCharTitlesStore.LookupEntry(NeedTitle))
            if (!player->HasTitle(TInfo))
                Ret = false;
    }

    if (Itr->second.StatRank > 0)
        if (uint32(player->GetTempAt(AT11)) < Itr->second.StatRank)
            Ret = false;

    Tokenizer AuraData(Itr->second.Auras, '#');
    for (auto const& Temp : AuraData)
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
            if (!player->HasAura(atoi(Temp)))
                Ret = false;

    Tokenizer ComAnchData(Itr->second.ComAnch, '#');
    for (auto const& Temp : ComAnchData)
        if (AchievementEntry const* AEntry = sAchievementStore.LookupEntry(atoi(Temp)))
            if (!player->HasAchieved(atoi(Temp)))
                Ret = false;

    Tokenizer ComQuestData(Itr->second.ComQuest, '#');
    for (auto const& Temp : ComQuestData)
        if (Quest const* QInfo = sObjectMgr->GetQuestTemplate(atoi(Temp)))
            if (!player->GetQuestRewardStatus(atoi(Temp)))
                Ret = false;

    if (Itr->second.StatId > 0 && Itr->second.StatValue > 0)
        if (!IsStatInMask(player, Itr->second.StatId, Itr->second.StatValue))
            Ret = false;

    Tokenizer InSpellData(Itr->second.InSpell, '#');
    for (auto const& Temp : InSpellData)
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
            if (!player->HasSpell(atoi(Temp)))
                Ret = false;

    if (Itr->second.NsRank > 0)
        if (player->GetTempAt(AT4001) < Itr->second.NsRank)
            Ret = false;

    if (Itr->second.AsEqRank > 0)
        if (!sPlayerModMgr->IsAsEqRank(player, Itr->second.AsEqRank))
            Ret = false;

    if (Itr->second.Reincarnation > 0)
        if (player->reincarnationLv < Itr->second.Reincarnation)
            Ret = false;

    return Ret;
}

void Helper::DeleteCheckNeed(Player* player, uint32 TempId, std::string Info, uint32 Mod)
{
    if (!TempId)
        return;

    auto Itr = NeedTempStore.find(TempId);
    if (Itr == NeedTempStore.end())
        return;

    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
        {
            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
            {
                player->DestroyItemCount(Itr->second.ItemId[i], Itr->second.Count[i] * Mod, true);
                player->SendSysMsg(GetGts(1030), GetItemLink(Itr->second.ItemId[i]).c_str(), Itr->second.Count[i] * Mod);
            }
        }
    }

    if (Itr->second.Hr > 0)
    {
        player->ModifyHonorPoints(-int32(Itr->second.Hr * Mod));
        player->SendSysMsg(GetGts(1031), Itr->second.Hr * Mod);
    }
    if (Itr->second.An > 0)
    {
        player->ModifyArenaPoints(-int32(Itr->second.An * Mod));
        player->SendSysMsg(GetGts(1032), Itr->second.An * Mod);
    }
    if (Itr->second.Token > 0)
        player->AddToken(-int64(Itr->second.Token * Mod), Info);

    if (Itr->second.Mobi > 0)
        player->AddTokenMB(-int64(Itr->second.Mobi * Mod), Info);

    if (Itr->second.Money > 0)
    {
        uint32 Need = uint32(Itr->second.Money * GOLD * Mod);
        if (!Need)
            Need = 0x7fffffff;

        player->ModifyMoney(-int32(Need));
        player->SendSysMsg(GetGts(1033), Itr->second.Money * Mod);
    }
}

std::string Helper::GetRewardGts(uint32 TempId, uint32 Mod)
{
    std::ostringstream Nss;
    if (!TempId)
    {
        Nss << "无奖励\n";
        return Nss.str();
    }

    auto Itr = RewardTempStore.find(TempId);
    if (Itr == RewardTempStore.end())
    {
        Nss << "无奖励\n";
        return Nss.str();
    }

    bool NoItem = true;
    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            NoItem = false;
    }

    bool Not = (NoItem
        && Itr->second.NowVip == 0
        && Itr->second.AddToken == 0
        && Itr->second.Xp == 0
        && Itr->second.Hr == 0
        && Itr->second.An == 0
        && Itr->second.Money == 0
        && Itr->second.Auras.empty()
        && Itr->second.ComAnch.empty()
        && Itr->second.ComQuest.empty()
        && Itr->second.Cmd.empty()
        && Itr->second.AddMobi == 0
        && Itr->second.AddPaodian == 0
        );

    if (Not)
    {
        Nss << "无奖励\n";
        return Nss.str();
    }

    Nss << "\n";

    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
                Nss << GetItemLink(Itr->second.ItemId[i]) << " X |cFFFF0000" << Itr->second.Count[i] * Mod << "|r\n";
    }

    if (Itr->second.NowVip > 0)
        Nss << "会员等级提升到 [|cFFFF0000" << Itr->second.NowVip << "|r]\n";

    if (Itr->second.AddToken > 0)
        Nss << "积分增加 [|cFFFF0000" << Itr->second.AddToken * Mod << "|r]\n";

    if (Itr->second.AddMobi > 0)
        Nss << "魔币增加 [|cFFFF0000" << Itr->second.AddMobi * Mod << "|r]\n";

    if (Itr->second.AddPaodian > 0)
        Nss << "泡点增加 [|cFFFF0000" << Itr->second.AddPaodian * Mod << "|r]\n";

    if (Itr->second.Xp > 0)
        Nss << "人物经验增加 [|cFFFF0000" << Itr->second.Xp * Mod << "|r]\n";

    if (Itr->second.Hr > 0)
        Nss << "荣誉点数增加 [|cFFFF0000" << Itr->second.Hr * Mod << "|r]\n";

    if (Itr->second.An > 0)
        Nss << "竞技点数增加 [|cFFFF0000" << Itr->second.An * Mod << "|r]\n";

    if (Itr->second.Money > 0)
        Nss << "金币增加 [|cFFFF0000" << Itr->second.Money * Mod << "|r]\n";

    Tokenizer AuraData(Itr->second.Auras, '#');
    for (auto const& Temp : AuraData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
        {
            Nss << "获得光环BUFF " << GetSpellName(Info->Id) << "\n";
        }
    }

    Tokenizer ComAnchData(Itr->second.ComAnch, '#');
    for (auto const& Temp : ComAnchData)
    {
        if (AchievementEntry const* AEntry = sAchievementStore.LookupEntry(atoi(Temp)))
        {
            Nss << "完成成就 " << GetAchievementName(AEntry->ID) << "\n";
        }
    }

    Tokenizer ComQuestData(Itr->second.ComQuest, '#');
    for (auto const& Temp : ComQuestData)
    {
        if (Quest const* QInfo = sObjectMgr->GetQuestTemplate(atoi(Temp)))
            Nss << "完成任务 " << GetQuestName(QInfo->GetQuestId()) << "\n";
    }

    if (!Itr->second.Cmd.empty())
        Nss << "其他奖励\n";

    return Nss.str();
}

std::string Helper::GetRewardLuaGts(uint32 TempId)
{
    std::ostringstream Nss;
    Nss << "{\n";

    auto Itr = RewardTempStore.find(TempId);
    if (Itr != RewardTempStore.end())
    {
        for (size_t i = 0; i < MNC; i++)
        {
            Nss << "                    \"" << Itr->second.ItemId[i] << "\",\n" << "                    \"" << Itr->second.Count[i] << "\",\n";
        }

        Nss << "                    \"" << Itr->second.NowVip << "\",\n";
        Nss << "                    \"" << Itr->second.AddToken << "\",\n";

        Nss << "                    \"" << Itr->second.Xp << "\",\n";

        Nss << "                    \"" << Itr->second.Hr << "\",\n";

        Nss << "                    \"" << Itr->second.An << "\",\n";
        Nss << "                    \"" << Itr->second.Money << "\",\n";
        Nss << "                    \"" << Itr->second.AddMobi << "\",\n";
    }

    Nss << "                }";
    return Nss.str();
}

void Helper::DoTheReward(Player* player, uint32 TempId, std::string Info, uint32 Mod)
{
    if (!TempId)
        return;

    auto Itr = RewardTempStore.find(TempId);
    if (Itr == RewardTempStore.end())
        return;

    bool NoItem = true;
    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            NoItem = false;
    }

    bool Not = (NoItem
        && Itr->second.NowVip == 0
        && Itr->second.AddToken == 0
        && Itr->second.Xp == 0
        && Itr->second.Hr == 0
        && Itr->second.An == 0
        && Itr->second.Money == 0
        && Itr->second.Auras.empty()
        && Itr->second.ComAnch.empty()
        && Itr->second.ComQuest.empty()
        && Itr->second.Cmd.empty()
        && Itr->second.AddMobi == 0
        && Itr->second.AddPaodian == 0
        );

    if (Not)
        return;

    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
                player->AddItem(Itr->second.ItemId[i], Itr->second.Count[i] * Mod, Info);
    }

    if (Itr->second.NowVip > 0)
    {
        uint32 NowRank = player->GetVipRank();
        if (NowRank < Itr->second.NowVip)
            player->CastSpell(player, TxLevelSpell, false);

        player->SetVipRank(Itr->second.NowVip);
    }

    if (Itr->second.AddToken > 0)
        player->AddToken(Itr->second.AddToken * Mod, Info);

    if (Itr->second.AddMobi > 0)
        player->AddTokenMB(Itr->second.AddMobi * Mod, Info);

    if (Itr->second.AddPaodian > 0)
        player->AddPaodian(Itr->second.AddPaodian * Mod, Info);

    if (Itr->second.Xp > 0)
        player->GiveXP(Itr->second.Xp * Mod, nullptr);

    if (Itr->second.Hr > 0)
        player->ModifyHonorPoints(Itr->second.Hr * Mod);

    if (Itr->second.An > 0)
        player->ModifyArenaPoints(Itr->second.An * Mod);

    if (Itr->second.Money > 0)
        player->ModifyMoney(Itr->second.Money * GOLD * Mod);

    Tokenizer AuraData(Itr->second.Auras, '#');
    for (auto const& Temp : AuraData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
        {
            if (!player->HasAura(atoi(Temp)))
                player->AddAura(atoi(Temp), player);
        }
    }

    Tokenizer ComAnchData(Itr->second.ComAnch, '#');
    for (auto const& Temp : ComAnchData)
    {
        if (AchievementEntry const* AEntry = sAchievementStore.LookupEntry(atoi(Temp)))
        {
            if (!player->HasAchieved(atoi(Temp)))
                player->CompletedAchievement(AEntry);
        }
    }

    Tokenizer ComQuestData(Itr->second.ComQuest, '#');
    for (auto const& Temp : ComQuestData)
    {
        if (Quest const* QInfo = sObjectMgr->GetQuestTemplate(atoi(Temp)))
            player->CustomCompleteQuest(atoi(Temp));
    }

    player->DoCommands(Itr->second.Cmd);
}

//回扣奖励
bool Helper::DoBackReward(Player* player, uint32 TempId, std::string Info, uint32 Mod)
{
    if (!TempId)
        return true;

    auto Itr = RewardTempStore.find(TempId);
    if (Itr == RewardTempStore.end())
        return true;

    bool NoItem = true;
    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
            NoItem = false;
    }

    bool Not = (NoItem
        && Itr->second.NowVip == 0
        && Itr->second.AddToken == 0
        && Itr->second.Xp == 0
        && Itr->second.Hr == 0
        && Itr->second.An == 0
        && Itr->second.Money == 0
        && Itr->second.Auras.empty()
        && Itr->second.ComAnch.empty()
        && Itr->second.ComQuest.empty()
        && Itr->second.AddMobi == 0
        && Itr->second.AddPaodian == 0
        );

    if (Not)
        return true;

    bool Ret = true;
    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
        {
            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
            {
                if (!player->HasItemCount(Itr->second.ItemId[i], Itr->second.Count[i] * Mod))
                {
                    Ret = false;
                    player->SendSysMsg(GetGts(1011), GetItemLink(Itr->second.ItemId[i]).c_str(), Itr->second.Count[i] * Mod);
                }
            }
        }
    }

    if (Itr->second.AddToken > 0)
    {
        if (player->GetToken() < Itr->second.AddToken * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1013), Itr->second.AddToken * Mod);
        }
    }
    if (Itr->second.AddMobi > 0)
    {
        if (player->GetTokenMB() < Itr->second.AddMobi * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(10200), Itr->second.AddMobi * Mod);
        }
    }
    if (Itr->second.AddPaodian > 0)
    {
        if (player->GetPaodian() < Itr->second.AddPaodian * Mod)
        {
            Ret = false;
            player->SendSysMsg("[系统]: 泡点不足，需要 |cFFFF6699%lld|r 泡点", Itr->second.AddPaodian * Mod);
        }
    }
    if (Itr->second.Hr > 0)
    {
        if (player->GetHonorPoints() < Itr->second.Hr * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1018), Itr->second.Hr * Mod);
        }
    }

    if (Itr->second.An > 0)
    {
        if (player->GetArenaPoints() < Itr->second.An * Mod)
        {
            Ret = false;
            player->SendSysMsg(GetGts(1019), Itr->second.An * Mod);
        }
    }

    if (Itr->second.Money > 0)
    {
        uint32 Need = uint32(Itr->second.Money * GOLD * Mod);
        if (!Need)
            Need = 0x7fffffff;

        if (!player->HasEnoughMoney(Need))
        {
            Ret = false;
            player->SendSysMsg(GetGts(1020), Itr->second.Money * Mod);
        }
    }

    if (!Ret)
        return false;

    for (size_t i = 0; i < MNC; i++)
    {
        if (Itr->second.ItemId[i] > 0 && Itr->second.Count[i] > 0)
        {
            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Itr->second.ItemId[i]))
            {
                player->DestroyItemCount(Itr->second.ItemId[i], Itr->second.Count[i] * Mod, true);
                player->SendSysMsg(GetGts(1030), GetItemLink(Itr->second.ItemId[i]).c_str(), Itr->second.Count[i] * Mod);
            }
        }
    }

    if (Itr->second.AddToken > 0)
        player->AddToken(-int64(Itr->second.AddToken * Mod), Info);

    if (Itr->second.AddMobi > 0)
        player->AddTokenMB(-int64(Itr->second.AddMobi * Mod), Info);

    if (Itr->second.AddPaodian > 0)
        player->AddPaodian(-int64(Itr->second.AddPaodian * Mod), Info);

    if (Itr->second.Hr > 0)
    {
        player->ModifyHonorPoints(-int32(Itr->second.Hr * Mod));
        player->SendSysMsg(GetGts(1031), Itr->second.Hr * Mod);
    }
    if (Itr->second.An > 0)
    {
        player->ModifyArenaPoints(-int32(Itr->second.An * Mod));
        player->SendSysMsg(GetGts(1032), Itr->second.An * Mod);
    }
    if (Itr->second.Money > 0)
    {
        uint32 Need = uint32(Itr->second.Money * GOLD * Mod);
        if (!Need)
            Need = 0x7fffffff;

        player->ModifyMoney(-int32(Need));
        player->SendSysMsg(GetGts(1033), Itr->second.Money * Mod);
    }

    Tokenizer AuraData(Itr->second.Auras, '#');
    for (auto const& Temp : AuraData)
    {
        if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(atoi(Temp)))
        {
            if (player->HasAura(atoi(Temp)))
                player->RemoveAurasDueToSpell(atoi(Temp));
        }
    }

    return true;
}

std::string Helper::GetDoTimeString(uint64 Time)
{
    if (!Time)
        return "";

    uint64 DoTime = uint64(Time / IN_MILLISECONDS);
    uint64 Secs = DoTime % MINUTE;
    uint64 Minutes = DoTime % HOUR / MINUTE;
    uint64 Hours = DoTime % DAY / HOUR;
    uint64 Days = DoTime / DAY;

    if (Days)
        return Acore::StringFormat("%u天%u小时%u分钟%u秒", Days, Hours, Minutes, Secs);
    else if (Hours)
        return Acore::StringFormat("%u小时%u分钟%u秒", Hours, Minutes, Secs);
    else if (Minutes)
        return Acore::StringFormat("%u分钟%u秒", Minutes, Secs);
    else
        return Acore::StringFormat("%u秒", Secs);
}

std::string Helper::GetSecsDoTimeString(uint64 Time)
{
    if (!Time)
        return "";

    uint64 Secs = Time % MINUTE;
    uint64 Minutes = Time % HOUR / MINUTE;
    uint64 Hours = Time % DAY / HOUR;
    uint64 Days = Time / DAY;

    std::ostringstream Oss;
    if (Days)
    {
        Oss << Days << "天";
    }
    if (Hours)
    {
        Oss << Hours << "小时";
    }
    if (Minutes)
    {
        Oss << Minutes << "分钟";
    }
    if (Secs || (!Days && !Hours && !Minutes))
    {
        Oss << Secs << "秒";
    }

    std::string TimeStr = Oss.str();

    if (!TimeStr.empty() && TimeStr[TimeStr.size() - 1] == ' ')
        TimeStr.resize(TimeStr.size() - 1);

    return TimeStr;
}

std::string Helper::GetTeamIdIcon(TeamId Id)
{
    switch (Id)
    {
    case TEAM_ALLIANCE:
        return "|TInterface/ICONS/Achievement_PVP_H_A.blp:30:30:-20:0|t";
    case TEAM_HORDE:
        return "|TInterface/ICONS/Achievement_PVP_H_H.blp:30:30:-20:0|t";
    case TEAM_NEUTRAL:
        return "|TInterface/ICONS/Achievement_Level_80.blp:30:30:-20:0|t";
    }
    return "|TInterface/ICONS/Achievement_Level_80.blp:30:30:-20:0|t";
}

bool Helper::IsMultiTrainerNpc(Creature* Cr)
{
    return strcmp(sHelper->GetCreatureScriptName(Cr).c_str(), "MultiTrainerNpc") == 0;
}

bool Helper::IsMultiVendorNpc(Creature* Cr)
{
    return strcmp(sHelper->GetCreatureScriptName(Cr).c_str(), "MultiVendorNpc") == 0;
}

void Helper::ResetPlayerDefault(Player* player)
{
    PlayerSpellMap Smap = player->GetSpellMap();
    for (PlayerSpellMap::const_iterator Itr = Smap.begin(); Itr != Smap.end(); ++Itr)
    {
        SpellInfo const* SInfo = sSpellMgr->GetSpellInfo(Itr->first);
        if (!SInfo)
            continue;

        if (!SpellMgr::IsSpellValid(SInfo))
            continue;

        if (player->IsSpellFitByClassAndRace(SInfo->Id))
            continue;

        player->removeSpell(Itr->first, false, false);
    }
}

void Helper::ClearResetForTgSpec(Player* player)
{
    //重置天赋
    player->resetTalents(true);
    player->InitTalentForLevel();
    player->SendTalentsInfoData(false);

    /*
    //移除雕文
    for (uint32 Slot = 0; Slot < MAX_GLYPH_SLOT_INDEX; ++Slot)
    {
        if (uint32 Glyph = player->GetGlyph(Slot))
        {
            if (GlyphPropertiesEntry const* GpEntry = sGlyphPropertiesStore.LookupEntry(Glyph))
            {
                player->RemoveAurasDueToSpell(GpEntry->SpellId);
                player->SetGlyph(Slot, 0, true);
                player->SendTalentsInfoData(false);

                //退回给玩家雕文物品
                if (uint32 ItemId = sHelper->GetItemIdByGlyph(GpEntry->Id))
                    player->AddItem(ItemId, 1);
            }
        }
    }
    */
}

void Helper::SetClassToNew(Player* player, uint8 NewClass)
{
    if (Pet* PlrPet = player->GetPet())
    {
        uint32 PGuid = PlrPet->GetCharmInfo()->GetPetNumber();
        player->RemovePet(PlrPet, PET_SAVE_NOT_IN_SLOT);
        CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();
        CharacterDatabasePreparedStatement* Stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PET_DECLINEDNAME);
        Stmt->SetData(0, PGuid);
        Trans->Append(Stmt);
        Stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PET_AURAS);
        Stmt->SetData(0, PGuid);
        Trans->Append(Stmt);
        Stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PET_SPELLS);
        Stmt->SetData(0, PGuid);
        Trans->Append(Stmt);
        Stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_PET_SPELL_COOLDOWNS);
        Stmt->SetData(0, PGuid);
        Trans->Append(Stmt);
        Stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PET_BY_OWNER);
        Stmt->SetData(0, player->GetGUIDLow());
        Trans->Append(Stmt);
        CharacterDatabase.CommitTransaction(Trans);
    }

    //---------------------------------------------战士---骑士---猎人---盗贼---牧师----DK----萨满----法师---术士--------小德--------------------//
    uint32 const DBclass[MAX_CLASSES] = { 51001, 51002, 51003, 51004, 51005, 51006, 51007, 51008, 51009, 0, 51011 };
    TrainerSpellData const* Spells = sObjectMgr->GetNpcTrainerSpells(DBclass[player->GetClass() - 1]);
    if (Spells)
    {
        for (TrainerSpellMap::const_iterator Itr = Spells->spellList.begin(); Itr != Spells->spellList.end(); ++Itr)
        {
            if (!player->IsSpellFitByClassAndRace(Itr->second.spell))
                continue;
            SpellInfo const* SInfo = sSpellMgr->GetSpellInfo(Itr->second.spell);
            if (!SInfo)
                continue;
            if (!SpellMgr::IsSpellValid(SInfo))
                continue;
            player->removeSpell(Itr->second.spell, SPEC_MASK_ALL, false);
        }
    }

    //遗忘出生默认的技能 包含种族特长啥的
    PlayerInfo const* PInfo = sObjectMgr->GetPlayerInfo(player->GetRace(), player->GetClass());
    if (PInfo)
    {
        for (PlayerCreateInfoSpells::const_iterator Itr = PInfo->customSpells.begin(); Itr != PInfo->customSpells.end(); ++Itr)
            if (!NotRemoveSpellClass(*Itr))
                player->removeSpell(*Itr, SPEC_MASK_ALL, false);

        for (PlayerCreateInfoSkills::const_iterator It = PInfo->skills.begin(); It != PInfo->skills.end(); ++It)
        {
            for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
                if (SkillLineAbilityEntry const* pAbility = sSkillLineAbilityStore.LookupEntry(j))
                    if (pAbility->SkillLine == It->SkillId)
                        if (!NotRemoveSpellClass(sSpellMgr->GetFirstSpellInChain(pAbility->Spell)))
                            player->removeSpell(sSpellMgr->GetFirstSpellInChain(pAbility->Spell), SPEC_MASK_ALL, false);
        }
    }

    //某些特定的职业技能都移除了
    std::vector<uint32> RSpells = { 691, 697, 712, 31801, 7328,
                                                       53736, 28272, 5394, 32182,
                                                       355, 2458, 7386, 71, 18960,
                                                       6795, 5487, 6807, 33943, 883,
                                                       6991, 982, 1528, 2641, 5149,
                                                       1515, 42459 };

    for (auto SpellId : RSpells)
    {
        if (player->HasSpell(SpellId))
            player->removeSpell(SpellId, SPEC_MASK_ALL, false);
    }

    std::string SepllStr = GetConfig(1033);
    if (!SepllStr.empty())
    {
        Tokenizer Data(SepllStr, '#');
        for (auto const& SId : Data)
        {
            uint32 DSpellId = uint32(atoi(SId));
            if (SpellInfo const* SInfo = sSpellMgr->GetSpellInfo(DSpellId))
            {
                if (player->HasSpell(DSpellId))
                    player->removeSpell(DSpellId, SPEC_MASK_ALL, false);
            }
        }
    }

    player->SetSkill(771, 0, 0, 0);

    //清理DK的附魔武器残留
    ClearDKEns(player);

    //防止使用多天赋BUG
    player->ActivateSpec(0);
    ClearResetForTgSpec(player);

    //修复单天赋转职卡灰色和卡BUG
    if (player->GetSpecsCount() > 1)
    {
        player->ActivateSpec(1);
        ClearResetForTgSpec(player);
    }

    //执行转职
    player->SetByteValue(UNIT_FIELD_BYTES_0, 1, NewClass);
    //一定要加上这个方法 没有就自己写一个 否则GG
    sCharacterCache->UpdateCharacterDataOnClass(player->GetGUID(), NewClass);

    //强制把身上不符合新职业的武器装备卸下 有位置就放 没位置自动发到邮箱里
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            //判断不符合要求的
            uint16 Cdest = 0;
            ItemTemplate const* pProto = pItem->GetTemplate();
            InventoryResult Check = player->CanEquipItem(NULL_SLOT, Cdest, pItem, false);
            if ((pProto->AllowableClass & player->GetClassMask()) == 0 || (pProto->RequiredSpell != 0 && !player->HasSpell(pProto->RequiredSpell)) || Check != EQUIP_ERR_OK)
            {
                ItemPosCountVec Dest;
                uint8 Msg = player->CanStoreItem(NULL_BAG, NULL_SLOT, Dest, pItem, false);
                if (Msg == EQUIP_ERR_OK)
                {
                    player->RemoveItem(INVENTORY_SLOT_BAG_0, pItem->GetSlot(), true);
                    player->StoreItem(Dest, pItem, true);
                }
                else
                {
                    player->MoveItemFromInventory(INVENTORY_SLOT_BAG_0, pItem->GetSlot(), true);
                    CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();
                    pItem->DeleteFromInventoryDB(Trans);
                    pItem->SaveToDB(Trans);
                    MailDraft("转职", "这些装备不支持新职业").AddItem(pItem).SendMailTo(Trans, player, MailSender(player, MAIL_STATIONERY_AUCTION));
                    CharacterDatabase.CommitTransaction(Trans);
                }
            }
        }
    }

    if (NewClass == CLASS_DEATH_KNIGHT)
        player->InitRunes();

    player->GetSession()->LogoutPlayer(true);
}

bool ObjectMgr::UpdateItemTempForEntry(uint32 Entry)
{
    bool Add = false;
    ItemTemplate* Temp = const_cast<ItemTemplate*>(GetItemTemplate(Entry));
    if (!Temp)
        Add = true;

    //                                                 0      1       2               3              4        5        6       7          8         9        10        11           12
    QueryResult Result = WorldDatabase.Query("SELECT entry, class, subclass, SoundOverrideSubclass, name, displayid, Quality, Flags, FlagsExtra, BuyCount, BuyPrice, SellPrice, InventoryType, "
        //                                              13              14           15          16             17               18                19              20
        "AllowableClass, AllowableRace, ItemLevel, RequiredLevel, RequiredSkill, RequiredSkillRank, requiredspell, requiredhonorrank, "
        //                                              21                      22                       23               24        25          26             27           28
        "RequiredCityRank, RequiredReputationFaction, RequiredReputationRank, maxcount, stackable, ContainerSlots, StatsCount, stat_type1, "
        //                                            29           30          31           32          33           34          35           36          37           38
        "stat_value1, stat_type2, stat_value2, stat_type3, stat_value3, stat_type4, stat_value4, stat_type5, stat_value5, stat_type6, "
        //                                            39           40          41           42           43          44           45           46           47
        "stat_value6, stat_type7, stat_value7, stat_type8, stat_value8, stat_type9, stat_value9, stat_type10, stat_value10, "
        //                                                   48                    49           50        51        52         53        54         55      56      57        58
        "ScalingStatDistribution, ScalingStatValue, dmg_min1, dmg_max1, dmg_type1, dmg_min2, dmg_max2, dmg_type2, armor, holy_res, fire_res, "
        //                                            59          60         61          62       63       64            65            66          67               68
        "nature_res, frost_res, shadow_res, arcane_res, delay, ammo_type, RangedModRange, spellid_1, spelltrigger_1, spellcharges_1, "
        //                                              69              70                71                 72                 73           74               75
        "spellppmRate_1, spellcooldown_1, spellcategory_1, spellcategorycooldown_1, spellid_2, spelltrigger_2, spellcharges_2, "
        //                                              76               77              78                  79                 80           81               82
        "spellppmRate_2, spellcooldown_2, spellcategory_2, spellcategorycooldown_2, spellid_3, spelltrigger_3, spellcharges_3, "
        //                                              83               84              85                  86                 87           88               89
        "spellppmRate_3, spellcooldown_3, spellcategory_3, spellcategorycooldown_3, spellid_4, spelltrigger_4, spellcharges_4, "
        //                                              90               91              92                  93                  94          95               96
        "spellppmRate_4, spellcooldown_4, spellcategory_4, spellcategorycooldown_4, spellid_5, spelltrigger_5, spellcharges_5, "
        //                                              97               98              99                  100                 101        102         103       104          105
        "spellppmRate_5, spellcooldown_5, spellcategory_5, spellcategorycooldown_5, bonding, description, PageText, LanguageID, PageMaterial, "
        //                                            106       107     108      109          110            111       112     113         114       115   116     117
        "startquest, lockid, Material, sheath, RandomProperty, RandomSuffix, block, itemset, MaxDurability, area, Map, BagFamily, "
        //                                            118             119             120             121             122            123              124            125
        "TotemCategory, socketColor_1, socketContent_1, socketColor_2, socketContent_2, socketColor_3, socketContent_3, socketBonus, "
        //                                            126                 127                     128            129            130            131         132         133
        "GemProperties, RequiredDisenchantSkill, ArmorDamageModifier, duration, ItemLimitCategory, HolidayId, ScriptName, DisenchantID, "
        //                                           134        135            136
        "FoodType, minMoneyLoot, maxMoneyLoot, flagsCustom FROM item_template WHERE entry={};", Entry);

    if (!Result)
        return false;

    std::vector<std::pair<Player*, uint8>> touchedSlots;
    if (!Add)
        touchedSlots = RemoveReloadedItemModsForOnlinePlayers(Entry);

    if (Add)
    {
        uint32 ReSize = _itemTemplateStore.size() + 1;
        _itemTemplateStore.rehash(ReSize);
        Temp = &_itemTemplateStore[Entry];
    }

    Field* fields = Result->Fetch();
    uint32 entry = fields[0].Get<uint32>();
    ClearItemTemplateInt64Fields(*Temp);

    Temp->ItemId = entry;
    Temp->Class = uint32(fields[1].Get<uint8>());
    Temp->SubClass = uint32(fields[2].Get<uint8>());
    Temp->SoundOverrideSubclass = int32(fields[3].Get<int8>());
    Temp->Name1 = fields[4].Get<std::string>();
    Temp->DisplayInfoID = fields[5].Get<uint32>();
    Temp->Quality = uint32(fields[6].Get<uint8>());
    Temp->Flags = fields[7].Get<uint32>();
    Temp->Flags2 = fields[8].Get<uint32>();
    Temp->BuyCount = uint32(fields[9].Get<uint8>());
    Temp->BuyPrice = int32(fields[10].Get<int64>() * sWorld->getRate((ServerConfigs)(RATE_BUYVALUE_ITEM_POOR + Temp->Quality)));
    Temp->SellPrice = uint32(fields[11].Get<uint32>() * sWorld->getRate((ServerConfigs)(RATE_SELLVALUE_ITEM_POOR + Temp->Quality)));
    Temp->InventoryType = uint32(fields[12].Get<uint8>());
    Temp->AllowableClass = fields[13].Get<int32>();
    Temp->AllowableRace = fields[14].Get<int32>();
    Temp->ItemLevel = uint32(fields[15].Get<uint16>());
    Temp->RequiredLevel = uint32(fields[16].Get<uint8>());
    Temp->RequiredSkill = uint32(fields[17].Get<uint16>());
    Temp->RequiredSkillRank = uint32(fields[18].Get<uint16>());
    Temp->RequiredSpell = fields[19].Get<uint32>();
    Temp->RequiredHonorRank = fields[20].Get<uint32>();
    Temp->RequiredCityRank = fields[21].Get<uint32>();
    Temp->RequiredReputationFaction = uint32(fields[22].Get<uint16>());
    Temp->RequiredReputationRank = uint32(fields[23].Get<uint16>());
    Temp->MaxCount = fields[24].Get<int32>();
    Temp->Stackable = fields[25].Get<int32>();
    Temp->ContainerSlots = uint32(fields[26].Get<uint8>());
    uint32 dbStatsCount = uint32(fields[27].Get<uint8>());
    if (dbStatsCount > MAX_ITEM_PROTO_STATS)
        LOG_ERROR("sql.sql", "Item (Entry: {}) has too large value in statscount ({}), replace by hardcoded limit ({}) before hot reload.", entry, dbStatsCount, MAX_ITEM_PROTO_STATS);
    Temp->StatsCount = std::min<uint32>(dbStatsCount, MAX_ITEM_PROTO_STATS);

    for (uint8 i = 0; i < Temp->StatsCount; ++i)
    {
        Temp->ItemStat[i].ItemStatType = uint32(fields[28 + i * 2].Get<uint8>());
        Temp->AsItemStatValue[i] = AsInt64FromString(fields[29 + i * 2].Get<std::string>());
        Temp->ItemStat[i].ItemStatValue = AsInt64ToInt32Clamp(Temp->AsItemStatValue[i]);
    }

    Temp->ScalingStatDistribution = uint32(fields[48].Get<uint16>());
    Temp->ScalingStatValue = fields[49].Get<int32>();

    for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
    {
        Temp->AsDamageMin[i] = AsInt64FromString(fields[50 + i * 3].Get<std::string>());
        Temp->Damage[i].DamageMin = AsInt64ToLegacyFloatClamp(Temp->AsDamageMin[i]);

        Temp->AsDamageMax[i] = AsInt64FromString(fields[51 + i * 3].Get<std::string>());
        Temp->Damage[i].DamageMax = AsInt64ToLegacyFloatClamp(Temp->AsDamageMax[i]);

        Temp->Damage[i].DamageType = uint32(fields[52 + i * 3].Get<uint8>());
    }

    Temp->AsArmor = AsInt64FromString(fields[56].Get<std::string>());
    Temp->Armor = AsInt64ToUInt32Clamp(Temp->AsArmor);
    Temp->HolyRes = uint32(fields[57].Get<uint8>());
    Temp->FireRes = uint32(fields[58].Get<uint8>());
    Temp->NatureRes = uint32(fields[59].Get<uint8>());
    Temp->FrostRes = uint32(fields[60].Get<uint8>());
    Temp->ShadowRes = uint32(fields[61].Get<uint8>());
    Temp->ArcaneRes = uint32(fields[62].Get<uint8>());
    Temp->Delay = uint32(fields[63].Get<uint16>());
    Temp->AmmoType = uint32(fields[64].Get<uint8>());
    Temp->RangedModRange = fields[65].Get<float>();

    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        Temp->Spells[i].SpellId = fields[66 + i * 7].Get<int32>();
        Temp->Spells[i].SpellTrigger = uint32(fields[67 + i * 7].Get<uint8>());
        Temp->Spells[i].SpellCharges = int32(fields[68 + i * 7].Get<int16>());
        Temp->Spells[i].SpellPPMRate = fields[69 + i * 7].Get<float>();
        Temp->Spells[i].SpellCooldown = fields[70 + i * 7].Get<int32>();
        Temp->Spells[i].SpellCategory = uint32(fields[71 + i * 7].Get<uint16>());
        Temp->Spells[i].SpellCategoryCooldown = fields[72 + i * 7].Get<int32>();
    }

    Temp->Bonding = uint32(fields[101].Get<uint8>());
    Temp->Description = fields[102].Get<std::string>();
    Temp->PageText = fields[103].Get<uint32>();
    Temp->LanguageID = uint32(fields[104].Get<uint8>());
    Temp->PageMaterial = uint32(fields[105].Get<uint8>());
    Temp->StartQuest = fields[106].Get<uint32>();
    Temp->LockID = fields[107].Get<uint32>();
    Temp->Material = int32(fields[108].Get<int8>());
    Temp->Sheath = uint32(fields[109].Get<uint8>());
    Temp->RandomProperty = fields[110].Get<int32>();
    Temp->RandomSuffix = 0;
    Temp->Block = fields[112].Get<uint32>();
    Temp->ItemSet = fields[113].Get<uint32>();
    Temp->MaxDurability = uint32(fields[114].Get<uint16>());
    Temp->Area = fields[115].Get<uint32>();
    Temp->Map = uint32(fields[116].Get<uint16>());
    Temp->BagFamily = fields[117].Get<uint32>();
    Temp->TotemCategory = fields[118].Get<uint32>();

    for (uint8 i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
    {
        Temp->Socket[i].Color = uint32(fields[119 + i * 2].Get<uint8>());
        Temp->Socket[i].Content = fields[120 + i * 2].Get<uint32>();
    }

    Temp->socketBonus = fields[125].Get<uint32>();
    Temp->GemProperties = fields[126].Get<uint32>();
    Temp->RequiredDisenchantSkill = uint32(fields[127].Get<int16>());
    Temp->ArmorDamageModifier = fields[128].Get<float>();
    Temp->Duration = fields[129].Get<uint32>();
    Temp->ItemLimitCategory = uint32(fields[130].Get<int16>());
    Temp->HolidayId = fields[131].Get<uint32>();
    Temp->ScriptId = sObjectMgr->GetScriptId(fields[132].Get<std::string>());
    Temp->DisenchantID = fields[133].Get<uint32>();
    Temp->FoodType = uint32(fields[134].Get<uint8>());
    Temp->MinMoneyLoot = fields[135].Get<uint32>();
    Temp->MaxMoneyLoot = fields[136].Get<uint32>();
    Temp->FlagsCu = fields[137].Get<uint32>();

    // Checks
    if (Temp->Class >= MAX_ITEM_CLASS)
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Class value ({})", entry, Temp->Class);
        Temp->Class = ITEM_CLASS_MISC;
    }

    if (Temp->SubClass >= MaxItemSubclassValues[Temp->Class])
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Subclass value ({}) for class {}", entry, Temp->SubClass, Temp->Class);
        Temp->SubClass = 0;// exist for all item classes
    }

    if (Temp->Quality >= MAX_ITEM_QUALITY)
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Quality value ({})", entry, Temp->Quality);
        Temp->Quality = ITEM_QUALITY_NORMAL;
    }

    if (Temp->Flags2 & ITEM_FLAGS_EXTRA_HORDE_ONLY)
    {
        if (FactionEntry const* faction = sFactionStore.LookupEntry(HORDE))
            if ((Temp->AllowableRace & faction->BaseRepRaceMask[0]) == 0)
                LOG_ERROR("sql.sql", "Item (Entry: {}) has value ({}) in `AllowableRace` races, not compatible with ITEM_FLAGS_EXTRA_HORDE_ONLY ({}) in Flags field, item cannot be equipped or used by these races.",
                    entry, Temp->AllowableRace, ITEM_FLAGS_EXTRA_HORDE_ONLY);

        if (Temp->Flags2 & ITEM_FLAGS_EXTRA_ALLIANCE_ONLY)
            LOG_ERROR("sql.sql", "Item (Entry: {}) has value ({}) in `Flags2` flags (ITEM_FLAGS_EXTRA_ALLIANCE_ONLY) and ITEM_FLAGS_EXTRA_HORDE_ONLY ({}) in Flags field, this is a wrong combination.",
                entry, ITEM_FLAGS_EXTRA_ALLIANCE_ONLY, ITEM_FLAGS_EXTRA_HORDE_ONLY);
    }
    else if (Temp->Flags2 & ITEM_FLAGS_EXTRA_ALLIANCE_ONLY)
    {
        if (FactionEntry const* faction = sFactionStore.LookupEntry(ALLIANCE))
            if ((Temp->AllowableRace & faction->BaseRepRaceMask[0]) == 0)
                LOG_ERROR("sql.sql", "Item (Entry: {}) has value ({}) in `AllowableRace` races, not compatible with ITEM_FLAGS_EXTRA_ALLIANCE_ONLY ({}) in Flags field, item cannot be equipped or used by these races.",
                    entry, Temp->AllowableRace, ITEM_FLAGS_EXTRA_ALLIANCE_ONLY);
    }

    if (Temp->BuyCount <= 0)
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong BuyCount value ({}), set to default(1).", entry, Temp->BuyCount);
        Temp->BuyCount = 1;
    }

    if (Temp->InventoryType >= MAX_INVTYPE)
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong InventoryType value ({})", entry, Temp->InventoryType);
        Temp->InventoryType = INVTYPE_NON_EQUIP;
    }

    if (Temp->RequiredSkill >= MAX_SKILL_TYPE)
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong RequiredSkill value ({})", entry, Temp->RequiredSkill);
        Temp->RequiredSkill = 0;
    }

    {
        // can be used in equip slot, as page read use in inventory, or spell casting at use
        bool req = Temp->InventoryType != INVTYPE_NON_EQUIP || Temp->PageText;
        if (!req)
            for (uint8 j = 0; j < MAX_ITEM_PROTO_SPELLS; ++j)
            {
                if (Temp->Spells[j].SpellId)
                {
                    req = true;
                    break;
                }
            }

        if (req)
        {
            if (!(Temp->AllowableClass & CLASSMASK_ALL_PLAYABLE))
                LOG_ERROR("sql.sql", "Item (Entry: {}) does not have any playable classes ({}) in `AllowableClass` and can't be equipped or used.", entry, Temp->AllowableClass);

            if (!(Temp->AllowableRace & RACEMASK_ALL_PLAYABLE))
                LOG_ERROR("sql.sql", "Item (Entry: {}) does not have any playable races ({}) in `AllowableRace` and can't be equipped or used.", entry, Temp->AllowableRace);
        }
    }

    if (Temp->RequiredSpell && !sSpellMgr->GetSpellInfo(Temp->RequiredSpell))
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has a wrong (non-existing) spell in RequiredSpell ({})", entry, Temp->RequiredSpell);
        Temp->RequiredSpell = 0;
    }

    if (Temp->RequiredReputationRank >= MAX_REPUTATION_RANK)
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong reputation rank in RequiredReputationRank ({}), item can't be used.", entry, Temp->RequiredReputationRank);

    if (Temp->RequiredReputationFaction)
    {
        if (!sFactionStore.LookupEntry(Temp->RequiredReputationFaction))
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong (not existing) faction in RequiredReputationFaction ({})", entry, Temp->RequiredReputationFaction);
            Temp->RequiredReputationFaction = 0;
        }

        if (Temp->RequiredReputationRank == MIN_REPUTATION_RANK)
            LOG_ERROR("sql.sql", "Item (Entry: {}) has min. reputation rank in RequiredReputationRank (0) but RequiredReputationFaction > 0, faction setting is useless.", entry);
    }

    if (Temp->MaxCount < -1)
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has too large negative in maxcount ({}), replace by value (-1) no storing limits.", entry, Temp->MaxCount);
        Temp->MaxCount = -1;
    }

    if (Temp->Stackable == 0)
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong value in stackable ({}), replace by default 1.", entry, Temp->Stackable);
        Temp->Stackable = 1;
    }
    else if (Temp->Stackable < -1)
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has too large negative in stackable ({}), replace by value (-1) no stacking limits.", entry, Temp->Stackable);
        Temp->Stackable = -1;
    }

    if (Temp->ContainerSlots > MAX_BAG_SIZE)
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has too large value in ContainerSlots ({}), replace by hardcoded limit ({}).", entry, Temp->ContainerSlots, MAX_BAG_SIZE);
        Temp->ContainerSlots = MAX_BAG_SIZE;
    }

    if (Temp->StatsCount > MAX_ITEM_PROTO_STATS)
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has too large value in statscount ({}), replace by hardcoded limit ({}).", entry, Temp->StatsCount, MAX_ITEM_PROTO_STATS);
        Temp->StatsCount = MAX_ITEM_PROTO_STATS;
    }

    for (uint8 j = 0; j < Temp->StatsCount; ++j)
    {
        // for ItemStatValue != 0
        if (Temp->ItemStat[j].ItemStatValue && Temp->ItemStat[j].ItemStatType >= MAX_ITEM_MOD)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong (non-existing?) stat_type{} ({})", entry, j + 1, Temp->ItemStat[j].ItemStatType);
            Temp->ItemStat[j].ItemStatType = 0;
        }

        switch (Temp->ItemStat[j].ItemStatType)
        {
        case ITEM_MOD_SPELL_HEALING_DONE:
        case ITEM_MOD_SPELL_DAMAGE_DONE:
            LOG_ERROR("sql.sql", "Item (Entry: {}) has deprecated stat_type{} ({})", entry, j + 1, Temp->ItemStat[j].ItemStatType);
            break;
        default:
            break;
        }
    }

    for (uint8 j = 0; j < MAX_ITEM_PROTO_DAMAGES; ++j)
    {
        if (Temp->Damage[j].DamageType >= MAX_SPELL_SCHOOL)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong dmg_type{} ({})", entry, j + 1, Temp->Damage[j].DamageType);
            Temp->Damage[j].DamageType = 0;
        }
    }

    // special format
    if ((Temp->Spells[0].SpellId == 483) || (Temp->Spells[0].SpellId == 55884))
    {
        // spell_1
        if (Temp->Spells[0].SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong item spell trigger value in spelltrigger_{} ({}) for special learning format", entry, 0 + 1, Temp->Spells[0].SpellTrigger);
            Temp->Spells[0].SpellId = 0;
            Temp->Spells[0].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
            Temp->Spells[1].SpellId = 0;
            Temp->Spells[1].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
        }

        // spell_2 have learning spell
        if (Temp->Spells[1].SpellTrigger != ITEM_SPELLTRIGGER_LEARN_SPELL_ID)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong item spell trigger value in spelltrigger_{} ({}) for special learning format.", entry, 1 + 1, Temp->Spells[1].SpellTrigger);
            Temp->Spells[0].SpellId = 0;
            Temp->Spells[1].SpellId = 0;
            Temp->Spells[1].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
        }
        else if (!Temp->Spells[1].SpellId)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) does not have an expected spell in spellid_{} in special learning format.", entry, 1 + 1);
            Temp->Spells[0].SpellId = 0;
            Temp->Spells[1].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
        }
        else if (Temp->Spells[1].SpellId != -1)
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(Temp->Spells[1].SpellId);
            if (!spellInfo && !DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, Temp->Spells[1].SpellId, nullptr))
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong (not existing) spell in spellid_{} ({})", entry, 1 + 1, Temp->Spells[1].SpellId);
                Temp->Spells[0].SpellId = 0;
                Temp->Spells[1].SpellId = 0;
                Temp->Spells[1].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
            }
            // allowed only in special format
            else if ((Temp->Spells[1].SpellId == 483) || (Temp->Spells[1].SpellId == 55884))
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has broken spell in spellid_{} ({})", entry, 1 + 1, Temp->Spells[1].SpellId);
                Temp->Spells[0].SpellId = 0;
                Temp->Spells[1].SpellId = 0;
                Temp->Spells[1].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
            }
        }

        // spell_3*, spell_4*, spell_5* is empty
        for (uint8 j = 2; j < MAX_ITEM_PROTO_SPELLS; ++j)
        {
            if (Temp->Spells[j].SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong item spell trigger value in spelltrigger_{} ({})", entry, j + 1, Temp->Spells[j].SpellTrigger);
                Temp->Spells[j].SpellId = 0;
                Temp->Spells[j].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
            }
            else if (Temp->Spells[j].SpellId != 0)
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong spell in spellid_{} ({}) for learning special format", entry, j + 1, Temp->Spells[j].SpellId);
                Temp->Spells[j].SpellId = 0;
            }
        }
    }
    // normal spell list
    else
    {
        for (uint8 j = 0; j < MAX_ITEM_PROTO_SPELLS; ++j)
        {
            if (Temp->Spells[j].SpellTrigger >= MAX_ITEM_SPELLTRIGGER || Temp->Spells[j].SpellTrigger == ITEM_SPELLTRIGGER_LEARN_SPELL_ID)
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong item spell trigger value in spelltrigger_{} ({})", entry, j + 1, Temp->Spells[j].SpellTrigger);
                Temp->Spells[j].SpellId = 0;
                Temp->Spells[j].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
            }

            if (Temp->Spells[j].SpellId && Temp->Spells[j].SpellId != -1)
            {
                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(Temp->Spells[j].SpellId);
                if (!spellInfo && !DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, Temp->Spells[j].SpellId, nullptr))
                {
                    LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong (not existing) spell in spellid_{} ({})", entry, j + 1, Temp->Spells[j].SpellId);
                    Temp->Spells[j].SpellId = 0;
                }
                // allowed only in special format
                else if ((Temp->Spells[j].SpellId == 483) || (Temp->Spells[j].SpellId == 55884))
                {
                    LOG_ERROR("sql.sql", "Item (Entry: {}) has broken spell in spellid_{} ({})", entry, j + 1, Temp->Spells[j].SpellId);
                    Temp->Spells[j].SpellId = 0;
                }
            }
        }
    }

    if (Temp->Bonding >= MAX_BIND_TYPE)
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Bonding value ({})", entry, Temp->Bonding);

    if (Temp->PageText && !sObjectMgr->GetPageText(Temp->PageText))
        LOG_ERROR("sql.sql", "Item (Entry: {}) has non existing first page (Id:{})", entry, Temp->PageText);

    if (Temp->LockID && !sLockStore.LookupEntry(Temp->LockID))
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong LockID ({})", entry, Temp->LockID);

    if (Temp->Sheath >= MAX_SHEATHETYPE)
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Sheath ({})", entry, Temp->Sheath);
        Temp->Sheath = SHEATHETYPE_NONE;
    }

    if (Temp->RandomProperty)
    {
        // To be implemented later
        if (Temp->RandomProperty == -1)
            Temp->RandomProperty = 0;

        else if (!sItemRandomPropertiesStore.LookupEntry(GetItemEnchantMod(Temp->RandomProperty)))
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has unknown (wrong or not listed in `item_enchantment_template`) RandomProperty ({})", entry, Temp->RandomProperty);
            Temp->RandomProperty = 0;
        }
    }

    if (Temp->RandomSuffix && !sItemRandomSuffixStore.LookupEntry(GetItemEnchantMod(Temp->RandomSuffix)))
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong RandomSuffix ({})", entry, Temp->RandomSuffix);
        Temp->RandomSuffix = 0;
    }

    if (Temp->ItemSet && !sItemSetStore.LookupEntry(Temp->ItemSet))
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) have wrong ItemSet ({})", entry, Temp->ItemSet);
        Temp->ItemSet = 0;
    }

    if (Temp->Area && !sAreaTableStore.LookupEntry(Temp->Area))
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Area ({})", entry, Temp->Area);

    if (Temp->Map && !sMapStore.LookupEntry(Temp->Map))
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Map ({})", entry, Temp->Map);

    if (Temp->BagFamily)
    {
        // check bits
        for (uint32 j = 0; j < sizeof(Temp->BagFamily) * 8; ++j)
        {
            uint32 mask = 1 << j;
            if ((Temp->BagFamily & mask) == 0)
                continue;

            ItemBagFamilyEntry const* bf = sItemBagFamilyStore.LookupEntry(j + 1);
            if (!bf)
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has bag family bit set not listed in ItemBagFamily.dbc, remove bit", entry);
                Temp->BagFamily &= ~mask;
                continue;
            }

            if (BAG_FAMILY_MASK_CURRENCY_TOKENS & mask)
            {
                CurrencyTypesEntry const* ctEntry = sCurrencyTypesStore.LookupEntry(Temp->ItemId);
                if (!ctEntry)
                {
                    LOG_ERROR("sql.sql", "Item (Entry: {}) has currency bag family bit set in BagFamily but not listed in CurrencyTypes.dbc, remove bit", entry);
                    Temp->BagFamily &= ~mask;
                }
            }
        }
    }

    if (Temp->TotemCategory && !sTotemCategoryStore.LookupEntry(Temp->TotemCategory))
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong TotemCategory ({})", entry, Temp->TotemCategory);

    for (uint8 j = 0; j < MAX_ITEM_PROTO_SOCKETS; ++j)
    {
        if (Temp->Socket[j].Color && (Temp->Socket[j].Color & SOCKET_COLOR_ALL) != Temp->Socket[j].Color)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong socketColor_{} ({})", entry, j + 1, Temp->Socket[j].Color);
            Temp->Socket[j].Color = 0;
        }
    }

    if (Temp->GemProperties && !sGemPropertiesStore.LookupEntry(Temp->GemProperties))
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong GemProperties ({})", entry, Temp->GemProperties);

    if (Temp->FoodType >= MAX_PET_DIET)
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong FoodType value ({})", entry, Temp->FoodType);
        Temp->FoodType = 0;
    }

    if (Temp->ItemLimitCategory && !sItemLimitCategoryStore.LookupEntry(Temp->ItemLimitCategory))
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong LimitCategory value ({})", entry, Temp->ItemLimitCategory);
        Temp->ItemLimitCategory = 0;
    }

    if (Temp->HolidayId && !sHolidaysStore.LookupEntry(Temp->HolidayId))
    {
        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong HolidayId value ({})", entry, Temp->HolidayId);
        Temp->HolidayId = 0;
    }

    if (Temp->FlagsCu & ITEM_FLAGS_CU_DURATION_REAL_TIME && !Temp->Duration)
    {
        LOG_ERROR("sql.sql", "Item (Entry {}) has flag ITEM_FLAGS_CU_DURATION_REAL_TIME but it does not have duration limit", entry);
        Temp->FlagsCu &= ~ITEM_FLAGS_CU_DURATION_REAL_TIME;
    }

    // Fill categories map
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (Temp->Spells[i].SpellId && Temp->Spells[i].SpellCategory && Temp->Spells[i].SpellCategoryCooldown)
        {
            SpellCategoryStore::iterator ct = sSpellsByCategoryStore.find(Temp->Spells[i].SpellCategory);
            if (ct != sSpellsByCategoryStore.end())
            {
                ct->second.emplace(true, Temp->Spells[i].SpellId);
            }
            else
                sSpellsByCategoryStore[Temp->Spells[i].SpellCategory].emplace(true, Temp->Spells[i].SpellId);
        }
    }

    if (Add)
    {
        uint32 max = 0;
        for (ItemTemplateContainer::const_iterator itr = _itemTemplateStore.begin(); itr != _itemTemplateStore.end(); ++itr)
            if (itr->first > max)
                max = itr->first;
        if (max)
        {
            _itemTemplateStoreFast.clear();
            _itemTemplateStoreFast.resize(max + 1, nullptr);

            CustomItemTemplateStoreOs.clear();
            CustomItemTemplateStoreOs.resize(max + 1, nullptr);

            for (ItemTemplateContainer::iterator itr = _itemTemplateStore.begin(); itr != _itemTemplateStore.end(); ++itr)
            {
                _itemTemplateStoreFast[itr->first] = &(itr->second);

                CustomItemTemplateStoreOs[itr->first] = &(itr->second);
            }
        }

        Temp->InitializeQueryData();

        std::set<uint32> notFoundOutfit;
        for (uint32 i = 1; i < sCharStartOutfitStore.GetNumRows(); ++i)
        {
            CharStartOutfitEntry const* entry = sCharStartOutfitStore.LookupEntry(i);
            if (!entry)
                continue;

            for (int j = 0; j < MAX_OUTFIT_ITEMS; ++j)
            {
                if (entry->ItemId[j] <= 0)
                    continue;

                uint32 item_id = entry->ItemId[j];

                if (!GetItemTemplate(item_id))
                    notFoundOutfit.insert(item_id);
            }
        }
    }

    ReloadBigItemTemplateStatCacheForEntry(Temp->ItemId, Temp);
    ApplyReloadedItemModsForOnlinePlayers(touchedSlots);


    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        WorldPacket Packet;
        Packet << Temp->ItemId;
        Itr->second->HandleItemQuerySingleOpcode(Packet);
        sDoAddon->SendPacketToClient(Itr->second->GetPlayer(), "ReloadItemTemplateData", std::to_string(Temp->ItemId));
    }

    return true;
}

void ObjectMgr::UpdateItemTempForAll()
{
    //                                                 0      1       2               3              4        5        6       7          8         9        10        11           12
    QueryResult result = WorldDatabase.Query("SELECT entry, class, subclass, SoundOverrideSubclass, name, displayid, Quality, Flags, FlagsExtra, BuyCount, BuyPrice, SellPrice, InventoryType, "
        //                                              13              14           15          16             17               18                19              20
        "AllowableClass, AllowableRace, ItemLevel, RequiredLevel, RequiredSkill, RequiredSkillRank, requiredspell, requiredhonorrank, "
        //                                              21                      22                       23               24        25          26             27           28
        "RequiredCityRank, RequiredReputationFaction, RequiredReputationRank, maxcount, stackable, ContainerSlots, StatsCount, stat_type1, "
        //                                            29           30          31           32          33           34          35           36          37           38
        "stat_value1, stat_type2, stat_value2, stat_type3, stat_value3, stat_type4, stat_value4, stat_type5, stat_value5, stat_type6, "
        //                                            39           40          41           42           43          44           45           46           47
        "stat_value6, stat_type7, stat_value7, stat_type8, stat_value8, stat_type9, stat_value9, stat_type10, stat_value10, "
        //                                                   48                    49           50        51        52         53        54         55      56      57        58
        "ScalingStatDistribution, ScalingStatValue, dmg_min1, dmg_max1, dmg_type1, dmg_min2, dmg_max2, dmg_type2, armor, holy_res, fire_res, "
        //                                            59          60         61          62       63       64            65            66          67               68
        "nature_res, frost_res, shadow_res, arcane_res, delay, ammo_type, RangedModRange, spellid_1, spelltrigger_1, spellcharges_1, "
        //                                              69              70                71                 72                 73           74               75
        "spellppmRate_1, spellcooldown_1, spellcategory_1, spellcategorycooldown_1, spellid_2, spelltrigger_2, spellcharges_2, "
        //                                              76               77              78                  79                 80           81               82
        "spellppmRate_2, spellcooldown_2, spellcategory_2, spellcategorycooldown_2, spellid_3, spelltrigger_3, spellcharges_3, "
        //                                              83               84              85                  86                 87           88               89
        "spellppmRate_3, spellcooldown_3, spellcategory_3, spellcategorycooldown_3, spellid_4, spelltrigger_4, spellcharges_4, "
        //                                              90               91              92                  93                  94          95               96
        "spellppmRate_4, spellcooldown_4, spellcategory_4, spellcategorycooldown_4, spellid_5, spelltrigger_5, spellcharges_5, "
        //                                              97               98              99                  100                 101        102         103       104          105
        "spellppmRate_5, spellcooldown_5, spellcategory_5, spellcategorycooldown_5, bonding, description, PageText, LanguageID, PageMaterial, "
        //                                            106       107     108      109          110            111       112     113         114       115   116     117
        "startquest, lockid, Material, sheath, RandomProperty, RandomSuffix, block, itemset, MaxDurability, area, Map, BagFamily, "
        //                                            118             119             120             121             122            123              124            125
        "TotemCategory, socketColor_1, socketContent_1, socketColor_2, socketContent_2, socketColor_3, socketContent_3, socketBonus, "
        //                                            126                 127                     128            129            130            131         132         133
        "GemProperties, RequiredDisenchantSkill, ArmorDamageModifier, duration, ItemLimitCategory, HolidayId, ScriptName, DisenchantID, "
        //                                           134        135            136
        "FoodType, minMoneyLoot, maxMoneyLoot, flagsCustom FROM item_template;");

    if (!result)
    {
        LOG_WARN("server.loading", ">> Loaded 0 item templates. DB table `item_template` is empty.");
        LOG_INFO("server.loading", " ");
        return;
    }

    std::vector<Player*> touchedPlayers = RemoveAllItemModsForOnlinePlayers();

    _itemTemplateStore.rehash(result->GetRowCount());
    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].Get<uint32>();

        ItemTemplate& itemTemplate = _itemTemplateStore[entry];
        ClearItemTemplateInt64Fields(itemTemplate);

        itemTemplate.ItemId = entry;
        itemTemplate.Class = uint32(fields[1].Get<uint8>());
        itemTemplate.SubClass = uint32(fields[2].Get<uint8>());
        itemTemplate.SoundOverrideSubclass = int32(fields[3].Get<int8>());
        itemTemplate.Name1 = fields[4].Get<std::string>();
        itemTemplate.DisplayInfoID = fields[5].Get<uint32>();
        itemTemplate.Quality = uint32(fields[6].Get<uint8>());
        itemTemplate.Flags = fields[7].Get<uint32>();
        itemTemplate.Flags2 = fields[8].Get<uint32>();
        itemTemplate.BuyCount = uint32(fields[9].Get<uint8>());
        itemTemplate.BuyPrice = int32(fields[10].Get<int64>() * sWorld->getRate((ServerConfigs)(RATE_BUYVALUE_ITEM_POOR + itemTemplate.Quality)));
        itemTemplate.SellPrice = uint32(fields[11].Get<uint32>() * sWorld->getRate((ServerConfigs)(RATE_SELLVALUE_ITEM_POOR + itemTemplate.Quality)));
        itemTemplate.InventoryType = uint32(fields[12].Get<uint8>());
        itemTemplate.AllowableClass = fields[13].Get<int32>();
        itemTemplate.AllowableRace = fields[14].Get<int32>();
        itemTemplate.ItemLevel = uint32(fields[15].Get<uint16>());
        itemTemplate.RequiredLevel = uint32(fields[16].Get<uint8>());
        itemTemplate.RequiredSkill = uint32(fields[17].Get<uint16>());
        itemTemplate.RequiredSkillRank = uint32(fields[18].Get<uint16>());
        itemTemplate.RequiredSpell = fields[19].Get<uint32>();
        itemTemplate.RequiredHonorRank = fields[20].Get<uint32>();
        itemTemplate.RequiredCityRank = fields[21].Get<uint32>();
        itemTemplate.RequiredReputationFaction = uint32(fields[22].Get<uint16>());
        itemTemplate.RequiredReputationRank = uint32(fields[23].Get<uint16>());
        itemTemplate.MaxCount = fields[24].Get<int32>();
        itemTemplate.Stackable = fields[25].Get<int32>();
        itemTemplate.ContainerSlots = uint32(fields[26].Get<uint8>());
        uint32 dbStatsCount = uint32(fields[27].Get<uint8>());
        if (dbStatsCount > MAX_ITEM_PROTO_STATS)
            LOG_ERROR("sql.sql", "Item (Entry: {}) has too large value in statscount ({}), replace by hardcoded limit ({}) before hot reload.", entry, dbStatsCount, MAX_ITEM_PROTO_STATS);
        itemTemplate.StatsCount = std::min<uint32>(dbStatsCount, MAX_ITEM_PROTO_STATS);

        for (uint8 i = 0; i < itemTemplate.StatsCount; ++i)
        {
            itemTemplate.ItemStat[i].ItemStatType = uint32(fields[28 + i * 2].Get<uint8>());
            itemTemplate.AsItemStatValue[i] = AsInt64FromString(fields[29 + i * 2].Get<std::string>());
            itemTemplate.ItemStat[i].ItemStatValue = AsInt64ToInt32Clamp(itemTemplate.AsItemStatValue[i]);
        }

        itemTemplate.ScalingStatDistribution = uint32(fields[48].Get<uint16>());
        itemTemplate.ScalingStatValue = fields[49].Get<int32>();

        for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            itemTemplate.AsDamageMin[i] = AsInt64FromString(fields[50 + i * 3].Get<std::string>());
            itemTemplate.Damage[i].DamageMin = AsInt64ToLegacyFloatClamp(itemTemplate.AsDamageMin[i]);

            itemTemplate.AsDamageMax[i] = AsInt64FromString(fields[51 + i * 3].Get<std::string>());
            itemTemplate.Damage[i].DamageMax = AsInt64ToLegacyFloatClamp(itemTemplate.AsDamageMax[i]);

            itemTemplate.Damage[i].DamageType = uint32(fields[52 + i * 3].Get<uint8>());
        }

        itemTemplate.AsArmor = AsInt64FromString(fields[56].Get<std::string>());
        itemTemplate.Armor = AsInt64ToUInt32Clamp(itemTemplate.AsArmor);
        itemTemplate.HolyRes = uint32(fields[57].Get<uint8>());
        itemTemplate.FireRes = uint32(fields[58].Get<uint8>());
        itemTemplate.NatureRes = uint32(fields[59].Get<uint8>());
        itemTemplate.FrostRes = uint32(fields[60].Get<uint8>());
        itemTemplate.ShadowRes = uint32(fields[61].Get<uint8>());
        itemTemplate.ArcaneRes = uint32(fields[62].Get<uint8>());
        itemTemplate.Delay = uint32(fields[63].Get<uint16>());
        itemTemplate.AmmoType = uint32(fields[64].Get<uint8>());
        itemTemplate.RangedModRange = fields[65].Get<float>();

        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            itemTemplate.Spells[i].SpellId = fields[66 + i * 7].Get<int32>();
            itemTemplate.Spells[i].SpellTrigger = uint32(fields[67 + i * 7].Get<uint8>());
            itemTemplate.Spells[i].SpellCharges = int32(fields[68 + i * 7].Get<int16>());
            itemTemplate.Spells[i].SpellPPMRate = fields[69 + i * 7].Get<float>();
            itemTemplate.Spells[i].SpellCooldown = fields[70 + i * 7].Get<int32>();
            itemTemplate.Spells[i].SpellCategory = uint32(fields[71 + i * 7].Get<uint16>());
            itemTemplate.Spells[i].SpellCategoryCooldown = fields[72 + i * 7].Get<int32>();
        }

        itemTemplate.Bonding = uint32(fields[101].Get<uint8>());
        itemTemplate.Description = fields[102].Get<std::string>();
        itemTemplate.PageText = fields[103].Get<uint32>();
        itemTemplate.LanguageID = uint32(fields[104].Get<uint8>());
        itemTemplate.PageMaterial = uint32(fields[105].Get<uint8>());
        itemTemplate.StartQuest = fields[106].Get<uint32>();
        itemTemplate.LockID = fields[107].Get<uint32>();
        itemTemplate.Material = int32(fields[108].Get<int8>());
        itemTemplate.Sheath = uint32(fields[109].Get<uint8>());
        itemTemplate.RandomProperty = fields[110].Get<int32>();
        itemTemplate.RandomSuffix = 0;
        itemTemplate.Block = fields[112].Get<uint32>();
        itemTemplate.ItemSet = fields[113].Get<uint32>();
        itemTemplate.MaxDurability = uint32(fields[114].Get<uint16>());
        itemTemplate.Area = fields[115].Get<uint32>();
        itemTemplate.Map = uint32(fields[116].Get<uint16>());
        itemTemplate.BagFamily = fields[117].Get<uint32>();
        itemTemplate.TotemCategory = fields[118].Get<uint32>();

        for (uint8 i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
        {
            itemTemplate.Socket[i].Color = uint32(fields[119 + i * 2].Get<uint8>());
            itemTemplate.Socket[i].Content = fields[120 + i * 2].Get<uint32>();
        }

        itemTemplate.socketBonus = fields[125].Get<uint32>();
        itemTemplate.GemProperties = fields[126].Get<uint32>();
        itemTemplate.RequiredDisenchantSkill = uint32(fields[127].Get<int16>());
        itemTemplate.ArmorDamageModifier = fields[128].Get<float>();
        itemTemplate.Duration = fields[129].Get<uint32>();
        itemTemplate.ItemLimitCategory = uint32(fields[130].Get<int16>());
        itemTemplate.HolidayId = fields[131].Get<uint32>();
        itemTemplate.ScriptId = sObjectMgr->GetScriptId(fields[132].Get<std::string>());
        itemTemplate.DisenchantID = fields[133].Get<uint32>();
        itemTemplate.FoodType = uint32(fields[134].Get<uint8>());
        itemTemplate.MinMoneyLoot = fields[135].Get<uint32>();
        itemTemplate.MaxMoneyLoot = fields[136].Get<uint32>();
        itemTemplate.FlagsCu = fields[137].Get<uint32>();

        // Checks
        if (itemTemplate.Class >= MAX_ITEM_CLASS)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Class value ({})", entry, itemTemplate.Class);
            itemTemplate.Class = ITEM_CLASS_MISC;
        }

        if (itemTemplate.SubClass >= MaxItemSubclassValues[itemTemplate.Class])
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Subclass value ({}) for class {}", entry, itemTemplate.SubClass, itemTemplate.Class);
            itemTemplate.SubClass = 0;// exist for all item classes
        }

        if (itemTemplate.Quality >= MAX_ITEM_QUALITY)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Quality value ({})", entry, itemTemplate.Quality);
            itemTemplate.Quality = ITEM_QUALITY_NORMAL;
        }

        if (itemTemplate.Flags2 & ITEM_FLAGS_EXTRA_HORDE_ONLY)
        {
            if (FactionEntry const* faction = sFactionStore.LookupEntry(HORDE))
                if ((itemTemplate.AllowableRace & faction->BaseRepRaceMask[0]) == 0)
                    LOG_ERROR("sql.sql", "Item (Entry: {}) has value ({}) in `AllowableRace` races, not compatible with ITEM_FLAGS_EXTRA_HORDE_ONLY ({}) in Flags field, item cannot be equipped or used by these races.",
                        entry, itemTemplate.AllowableRace, ITEM_FLAGS_EXTRA_HORDE_ONLY);

            if (itemTemplate.Flags2 & ITEM_FLAGS_EXTRA_ALLIANCE_ONLY)
                LOG_ERROR("sql.sql", "Item (Entry: {}) has value ({}) in `Flags2` flags (ITEM_FLAGS_EXTRA_ALLIANCE_ONLY) and ITEM_FLAGS_EXTRA_HORDE_ONLY ({}) in Flags field, this is a wrong combination.",
                    entry, ITEM_FLAGS_EXTRA_ALLIANCE_ONLY, ITEM_FLAGS_EXTRA_HORDE_ONLY);
        }
        else if (itemTemplate.Flags2 & ITEM_FLAGS_EXTRA_ALLIANCE_ONLY)
        {
            if (FactionEntry const* faction = sFactionStore.LookupEntry(ALLIANCE))
                if ((itemTemplate.AllowableRace & faction->BaseRepRaceMask[0]) == 0)
                    LOG_ERROR("sql.sql", "Item (Entry: {}) has value ({}) in `AllowableRace` races, not compatible with ITEM_FLAGS_EXTRA_ALLIANCE_ONLY ({}) in Flags field, item cannot be equipped or used by these races.",
                        entry, itemTemplate.AllowableRace, ITEM_FLAGS_EXTRA_ALLIANCE_ONLY);
        }

        if (itemTemplate.BuyCount <= 0)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong BuyCount value ({}), set to default(1).", entry, itemTemplate.BuyCount);
            itemTemplate.BuyCount = 1;
        }

        if (itemTemplate.InventoryType >= MAX_INVTYPE)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong InventoryType value ({})", entry, itemTemplate.InventoryType);
            itemTemplate.InventoryType = INVTYPE_NON_EQUIP;
        }

        if (itemTemplate.RequiredSkill >= MAX_SKILL_TYPE)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong RequiredSkill value ({})", entry, itemTemplate.RequiredSkill);
            itemTemplate.RequiredSkill = 0;
        }

        {
            // can be used in equip slot, as page read use in inventory, or spell casting at use
            bool req = itemTemplate.InventoryType != INVTYPE_NON_EQUIP || itemTemplate.PageText;
            if (!req)
                for (uint8 j = 0; j < MAX_ITEM_PROTO_SPELLS; ++j)
                {
                    if (itemTemplate.Spells[j].SpellId)
                    {
                        req = true;
                        break;
                    }
                }

            if (req)
            {
                if (!(itemTemplate.AllowableClass & CLASSMASK_ALL_PLAYABLE))
                    LOG_ERROR("sql.sql", "Item (Entry: {}) does not have any playable classes ({}) in `AllowableClass` and can't be equipped or used.", entry, itemTemplate.AllowableClass);

                if (!(itemTemplate.AllowableRace & RACEMASK_ALL_PLAYABLE))
                    LOG_ERROR("sql.sql", "Item (Entry: {}) does not have any playable races ({}) in `AllowableRace` and can't be equipped or used.", entry, itemTemplate.AllowableRace);
            }
        }

        if (itemTemplate.RequiredSpell && !sSpellMgr->GetSpellInfo(itemTemplate.RequiredSpell))
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has a wrong (non-existing) spell in RequiredSpell ({})", entry, itemTemplate.RequiredSpell);
            itemTemplate.RequiredSpell = 0;
        }

        if (itemTemplate.RequiredReputationRank >= MAX_REPUTATION_RANK)
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong reputation rank in RequiredReputationRank ({}), item can't be used.", entry, itemTemplate.RequiredReputationRank);

        if (itemTemplate.RequiredReputationFaction)
        {
            if (!sFactionStore.LookupEntry(itemTemplate.RequiredReputationFaction))
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong (not existing) faction in RequiredReputationFaction ({})", entry, itemTemplate.RequiredReputationFaction);
                itemTemplate.RequiredReputationFaction = 0;
            }

            if (itemTemplate.RequiredReputationRank == MIN_REPUTATION_RANK)
                LOG_ERROR("sql.sql", "Item (Entry: {}) has min. reputation rank in RequiredReputationRank (0) but RequiredReputationFaction > 0, faction setting is useless.", entry);
        }

        if (itemTemplate.MaxCount < -1)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has too large negative in maxcount ({}), replace by value (-1) no storing limits.", entry, itemTemplate.MaxCount);
            itemTemplate.MaxCount = -1;
        }

        if (itemTemplate.Stackable == 0)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong value in stackable ({}), replace by default 1.", entry, itemTemplate.Stackable);
            itemTemplate.Stackable = 1;
        }
        else if (itemTemplate.Stackable < -1)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has too large negative in stackable ({}), replace by value (-1) no stacking limits.", entry, itemTemplate.Stackable);
            itemTemplate.Stackable = -1;
        }

        if (itemTemplate.ContainerSlots > MAX_BAG_SIZE)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has too large value in ContainerSlots ({}), replace by hardcoded limit ({}).", entry, itemTemplate.ContainerSlots, MAX_BAG_SIZE);
            itemTemplate.ContainerSlots = MAX_BAG_SIZE;
        }

        if (itemTemplate.StatsCount > MAX_ITEM_PROTO_STATS)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has too large value in statscount ({}), replace by hardcoded limit ({}).", entry, itemTemplate.StatsCount, MAX_ITEM_PROTO_STATS);
            itemTemplate.StatsCount = MAX_ITEM_PROTO_STATS;
        }

        for (uint8 j = 0; j < itemTemplate.StatsCount; ++j)
        {
            // for ItemStatValue != 0
            if (itemTemplate.ItemStat[j].ItemStatValue && itemTemplate.ItemStat[j].ItemStatType >= MAX_ITEM_MOD)
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong (non-existing?) stat_type{} ({})", entry, j + 1, itemTemplate.ItemStat[j].ItemStatType);
                itemTemplate.ItemStat[j].ItemStatType = 0;
            }

            switch (itemTemplate.ItemStat[j].ItemStatType)
            {
            case ITEM_MOD_SPELL_HEALING_DONE:
            case ITEM_MOD_SPELL_DAMAGE_DONE:
                LOG_ERROR("sql.sql", "Item (Entry: {}) has deprecated stat_type{} ({})", entry, j + 1, itemTemplate.ItemStat[j].ItemStatType);
                break;
            default:
                break;
            }
        }

        for (uint8 j = 0; j < MAX_ITEM_PROTO_DAMAGES; ++j)
        {
            if (itemTemplate.Damage[j].DamageType >= MAX_SPELL_SCHOOL)
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong dmg_type{} ({})", entry, j + 1, itemTemplate.Damage[j].DamageType);
                itemTemplate.Damage[j].DamageType = 0;
            }
        }

        // special format
        if ((itemTemplate.Spells[0].SpellId == 483) || (itemTemplate.Spells[0].SpellId == 55884))
        {
            // spell_1
            if (itemTemplate.Spells[0].SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong item spell trigger value in spelltrigger_{} ({}) for special learning format", entry, 0 + 1, itemTemplate.Spells[0].SpellTrigger);
                itemTemplate.Spells[0].SpellId = 0;
                itemTemplate.Spells[0].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
                itemTemplate.Spells[1].SpellId = 0;
                itemTemplate.Spells[1].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
            }

            // spell_2 have learning spell
            if (itemTemplate.Spells[1].SpellTrigger != ITEM_SPELLTRIGGER_LEARN_SPELL_ID)
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong item spell trigger value in spelltrigger_{} ({}) for special learning format.", entry, 1 + 1, itemTemplate.Spells[1].SpellTrigger);
                itemTemplate.Spells[0].SpellId = 0;
                itemTemplate.Spells[1].SpellId = 0;
                itemTemplate.Spells[1].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
            }
            else if (!itemTemplate.Spells[1].SpellId)
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) does not have an expected spell in spellid_{} in special learning format.", entry, 1 + 1);
                itemTemplate.Spells[0].SpellId = 0;
                itemTemplate.Spells[1].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
            }
            else if (itemTemplate.Spells[1].SpellId != -1)
            {
                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(itemTemplate.Spells[1].SpellId);
                if (!spellInfo && !DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, itemTemplate.Spells[1].SpellId, nullptr))
                {
                    LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong (not existing) spell in spellid_{} ({})", entry, 1 + 1, itemTemplate.Spells[1].SpellId);
                    itemTemplate.Spells[0].SpellId = 0;
                    itemTemplate.Spells[1].SpellId = 0;
                    itemTemplate.Spells[1].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
                }
                // allowed only in special format
                else if ((itemTemplate.Spells[1].SpellId == 483) || (itemTemplate.Spells[1].SpellId == 55884))
                {
                    LOG_ERROR("sql.sql", "Item (Entry: {}) has broken spell in spellid_{} ({})", entry, 1 + 1, itemTemplate.Spells[1].SpellId);
                    itemTemplate.Spells[0].SpellId = 0;
                    itemTemplate.Spells[1].SpellId = 0;
                    itemTemplate.Spells[1].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
                }
            }

            // spell_3*, spell_4*, spell_5* is empty
            for (uint8 j = 2; j < MAX_ITEM_PROTO_SPELLS; ++j)
            {
                if (itemTemplate.Spells[j].SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
                {
                    LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong item spell trigger value in spelltrigger_{} ({})", entry, j + 1, itemTemplate.Spells[j].SpellTrigger);
                    itemTemplate.Spells[j].SpellId = 0;
                    itemTemplate.Spells[j].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
                }
                else if (itemTemplate.Spells[j].SpellId != 0)
                {
                    LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong spell in spellid_{} ({}) for learning special format", entry, j + 1, itemTemplate.Spells[j].SpellId);
                    itemTemplate.Spells[j].SpellId = 0;
                }
            }
        }
        // normal spell list
        else
        {
            for (uint8 j = 0; j < MAX_ITEM_PROTO_SPELLS; ++j)
            {
                if (itemTemplate.Spells[j].SpellTrigger >= MAX_ITEM_SPELLTRIGGER || itemTemplate.Spells[j].SpellTrigger == ITEM_SPELLTRIGGER_LEARN_SPELL_ID)
                {
                    LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong item spell trigger value in spelltrigger_{} ({})", entry, j + 1, itemTemplate.Spells[j].SpellTrigger);
                    itemTemplate.Spells[j].SpellId = 0;
                    itemTemplate.Spells[j].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
                }

                if (itemTemplate.Spells[j].SpellId && itemTemplate.Spells[j].SpellId != -1)
                {
                    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(itemTemplate.Spells[j].SpellId);
                    if (!spellInfo && !DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, itemTemplate.Spells[j].SpellId, nullptr))
                    {
                        LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong (not existing) spell in spellid_{} ({})", entry, j + 1, itemTemplate.Spells[j].SpellId);
                        itemTemplate.Spells[j].SpellId = 0;
                    }
                    // allowed only in special format
                    else if ((itemTemplate.Spells[j].SpellId == 483) || (itemTemplate.Spells[j].SpellId == 55884))
                    {
                        LOG_ERROR("sql.sql", "Item (Entry: {}) has broken spell in spellid_{} ({})", entry, j + 1, itemTemplate.Spells[j].SpellId);
                        itemTemplate.Spells[j].SpellId = 0;
                    }
                }
            }
        }

        if (itemTemplate.Bonding >= MAX_BIND_TYPE)
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Bonding value ({})", entry, itemTemplate.Bonding);

        if (itemTemplate.PageText && !GetPageText(itemTemplate.PageText))
            LOG_ERROR("sql.sql", "Item (Entry: {}) has non existing first page (Id:{})", entry, itemTemplate.PageText);

        if (itemTemplate.LockID && !sLockStore.LookupEntry(itemTemplate.LockID))
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong LockID ({})", entry, itemTemplate.LockID);

        if (itemTemplate.Sheath >= MAX_SHEATHETYPE)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Sheath ({})", entry, itemTemplate.Sheath);
            itemTemplate.Sheath = SHEATHETYPE_NONE;
        }

        if (itemTemplate.RandomProperty)
        {
            // To be implemented later
            if (itemTemplate.RandomProperty == -1)
                itemTemplate.RandomProperty = 0;

            else if (!sItemRandomPropertiesStore.LookupEntry(GetItemEnchantMod(itemTemplate.RandomProperty)))
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has unknown (wrong or not listed in `item_enchantment_template`) RandomProperty ({})", entry, itemTemplate.RandomProperty);
                itemTemplate.RandomProperty = 0;
            }
        }

        if (itemTemplate.RandomSuffix && !sItemRandomSuffixStore.LookupEntry(GetItemEnchantMod(itemTemplate.RandomSuffix)))
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong RandomSuffix ({})", entry, itemTemplate.RandomSuffix);
            itemTemplate.RandomSuffix = 0;
        }

        if (itemTemplate.ItemSet && !sItemSetStore.LookupEntry(itemTemplate.ItemSet))
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) have wrong ItemSet ({})", entry, itemTemplate.ItemSet);
            itemTemplate.ItemSet = 0;
        }

        if (itemTemplate.Area && !sAreaTableStore.LookupEntry(itemTemplate.Area))
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Area ({})", entry, itemTemplate.Area);

        if (itemTemplate.Map && !sMapStore.LookupEntry(itemTemplate.Map))
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong Map ({})", entry, itemTemplate.Map);

        if (itemTemplate.BagFamily)
        {
            // check bits
            for (uint32 j = 0; j < sizeof(itemTemplate.BagFamily) * 8; ++j)
            {
                uint32 mask = 1 << j;
                if ((itemTemplate.BagFamily & mask) == 0)
                    continue;

                ItemBagFamilyEntry const* bf = sItemBagFamilyStore.LookupEntry(j + 1);
                if (!bf)
                {
                    LOG_ERROR("sql.sql", "Item (Entry: {}) has bag family bit set not listed in ItemBagFamily.dbc, remove bit", entry);
                    itemTemplate.BagFamily &= ~mask;
                    continue;
                }

                if (BAG_FAMILY_MASK_CURRENCY_TOKENS & mask)
                {
                    CurrencyTypesEntry const* ctEntry = sCurrencyTypesStore.LookupEntry(itemTemplate.ItemId);
                    if (!ctEntry)
                    {
                        LOG_ERROR("sql.sql", "Item (Entry: {}) has currency bag family bit set in BagFamily but not listed in CurrencyTypes.dbc, remove bit", entry);
                        itemTemplate.BagFamily &= ~mask;
                    }
                }
            }
        }

        if (itemTemplate.TotemCategory && !sTotemCategoryStore.LookupEntry(itemTemplate.TotemCategory))
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong TotemCategory ({})", entry, itemTemplate.TotemCategory);

        for (uint8 j = 0; j < MAX_ITEM_PROTO_SOCKETS; ++j)
        {
            if (itemTemplate.Socket[j].Color && (itemTemplate.Socket[j].Color & SOCKET_COLOR_ALL) != itemTemplate.Socket[j].Color)
            {
                LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong socketColor_{} ({})", entry, j + 1, itemTemplate.Socket[j].Color);
                itemTemplate.Socket[j].Color = 0;
            }
        }

        if (itemTemplate.GemProperties && !sGemPropertiesStore.LookupEntry(itemTemplate.GemProperties))
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong GemProperties ({})", entry, itemTemplate.GemProperties);

        if (itemTemplate.FoodType >= MAX_PET_DIET)
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong FoodType value ({})", entry, itemTemplate.FoodType);
            itemTemplate.FoodType = 0;
        }

        if (itemTemplate.ItemLimitCategory && !sItemLimitCategoryStore.LookupEntry(itemTemplate.ItemLimitCategory))
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong LimitCategory value ({})", entry, itemTemplate.ItemLimitCategory);
            itemTemplate.ItemLimitCategory = 0;
        }

        if (itemTemplate.HolidayId && !sHolidaysStore.LookupEntry(itemTemplate.HolidayId))
        {
            LOG_ERROR("sql.sql", "Item (Entry: {}) has wrong HolidayId value ({})", entry, itemTemplate.HolidayId);
            itemTemplate.HolidayId = 0;
        }

        if (itemTemplate.FlagsCu & ITEM_FLAGS_CU_DURATION_REAL_TIME && !itemTemplate.Duration)
        {
            LOG_ERROR("sql.sql", "Item (Entry {}) has flag ITEM_FLAGS_CU_DURATION_REAL_TIME but it does not have duration limit", entry);
            itemTemplate.FlagsCu &= ~ITEM_FLAGS_CU_DURATION_REAL_TIME;
        }

        // Fill categories map
        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            if (itemTemplate.Spells[i].SpellId && itemTemplate.Spells[i].SpellCategory && itemTemplate.Spells[i].SpellCategoryCooldown)
            {
                SpellCategoryStore::iterator ct = sSpellsByCategoryStore.find(itemTemplate.Spells[i].SpellCategory);
                if (ct != sSpellsByCategoryStore.end())
                {
                    ct->second.emplace(true, itemTemplate.Spells[i].SpellId);
                }
                else
                    sSpellsByCategoryStore[itemTemplate.Spells[i].SpellCategory].emplace(true, itemTemplate.Spells[i].SpellId);
            }

        ++count;
    } while (result->NextRow());

    // pussywizard:
    {
        uint32 max = 0;
        for (ItemTemplateContainer::const_iterator itr = _itemTemplateStore.begin(); itr != _itemTemplateStore.end(); ++itr)
            if (itr->first > max)
                max = itr->first;
        if (max)
        {
            _itemTemplateStoreFast.clear();
            _itemTemplateStoreFast.resize(max + 1, nullptr);

            CustomItemTemplateStoreOs.clear();
            CustomItemTemplateStoreOs.resize(max + 1, nullptr);

            for (ItemTemplateContainer::iterator itr = _itemTemplateStore.begin(); itr != _itemTemplateStore.end(); ++itr)
            {
                _itemTemplateStoreFast[itr->first] = &(itr->second);

                CustomItemTemplateStoreOs[itr->first] = &(itr->second);
            }
        }
    }

    for (ItemTemplateContainer::iterator itr = _itemTemplateStore.begin(); itr != _itemTemplateStore.end(); ++itr)
        itr->second.InitializeQueryData();

    ReloadBigItemTemplateStatCache();
    ApplyAllItemModsForOnlinePlayers(touchedPlayers);

    // Check if item templates for DBC referenced character start outfit are present
    std::set<uint32> notFoundOutfit;
    for (uint32 i = 1; i < sCharStartOutfitStore.GetNumRows(); ++i)
    {
        CharStartOutfitEntry const* entry = sCharStartOutfitStore.LookupEntry(i);
        if (!entry)
            continue;

        for (int j = 0; j < MAX_OUTFIT_ITEMS; ++j)
        {
            if (entry->ItemId[j] <= 0)
                continue;

            uint32 item_id = entry->ItemId[j];

            if (!GetItemTemplate(item_id))
                notFoundOutfit.insert(item_id);
        }
    }

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        Player* player = Itr->second->GetPlayer();

        for (auto const& ItemTemplatePair : _itemTemplateStore)
        {
            WorldPacket Packet;
            Packet << ItemTemplatePair.second.ItemId;
            Itr->second->HandleItemQuerySingleOpcode(Packet);
        }

        sDoAddon->SendPacketToClient(player, "ReloadAllItemTemplateData", "1");
    }
}

void ObjectMgr::UpdateCreTempForEntry(uint32 CreId)
{
    bool Add = false;
    CreatureTemplate* CInfo = const_cast<CreatureTemplate*>(GetCreatureTemplate(CreId));
    if (!CInfo)
        Add = true;

    QueryResult Result = WorldDatabase.Query("SELECT entry, difficulty_entry_1, difficulty_entry_2, difficulty_entry_3, KillCredit1, KillCredit2, modelid1, modelid2, modelid3, "
        //                        9         10    11       12        13              14        15        16   17       18       19          20         21          22
        "modelid4, name, subname, IconName, gossip_menu_id, minlevel, maxlevel, exp, faction, npcflag, speed_walk, speed_run, speed_swim, speed_flight, "
        //                        23               24     25      26         27              28              29               30            31             32          33          34
        "detection_range, scale, `rank`, dmgschool, DamageModifier, BaseAttackTime, RangeAttackTime, BaseVariance, RangeVariance, unit_class, unit_flags, unit_flags2, "
        //                        35            36      37            38             39             40            41
        "dynamicflags, family, trainer_type, trainer_spell, trainer_class, trainer_race, type, "
        //                        42          43      44              45        46              47         48       49       50      51
        "type_flags, lootid, pickpocketloot, skinloot, PetSpellDataId, VehicleId, mingold, maxgold, AIName, MovementType, "
        //                        52          53        54          55          56         57          58                         59           60              61            62             63
        "ctm.Ground, ctm.Swim, ctm.Flight, ctm.Rooted, ctm.Chase, ctm.Random, ctm.InteractionPauseTimer, HoverHeight, HealthModifier, ManaModifier, ArmorModifier, ExperienceModifier, "
        //                        64            65          66           67                    68                        69           70
        "RacialLeader, movementId, RegenHealth, mechanic_immune_mask, spell_school_immune_mask, flags_extra, ScriptName "
        "FROM creature_template ct LEFT JOIN creature_template_movement ctm ON ct.entry = ctm.CreatureId WHERE ct.entry = {};", CreId);

    if (!Result)
        return;

    if (Add)
    {
        uint32 ReSize = _creatureTemplateStore.size() + 1;
        _creatureTemplateStore.rehash(ReSize);
        CInfo = &_creatureTemplateStore[CreId];
    }

    Field* fields = Result->Fetch();
    CInfo->Entry = CreId;

    for (uint8 i = 0; i < MAX_DIFFICULTY - 1; ++i)
    {
        CInfo->DifficultyEntry[i] = fields[1 + i].Get<uint32>();
    }

    for (uint8 i = 0; i < MAX_KILL_CREDIT; ++i)
    {
        CInfo->KillCredit[i] = fields[4 + i].Get<uint32>();
    }

    CInfo->Modelid1 = fields[6].Get<uint32>();
    CInfo->Modelid2 = fields[7].Get<uint32>();
    CInfo->Modelid3 = fields[8].Get<uint32>();
    CInfo->Modelid4 = fields[9].Get<uint32>();
    CInfo->Name = fields[10].Get<std::string>();
    CInfo->SubName = fields[11].Get<std::string>();
    CInfo->IconName = fields[12].Get<std::string>();
    CInfo->GossipMenuId = fields[13].Get<uint32>();
    CInfo->minlevel = fields[14].Get<uint8>();
    CInfo->maxlevel = fields[15].Get<uint8>();
    CInfo->expansion = uint32(fields[16].Get<int16>());
    CInfo->faction = uint32(fields[17].Get<uint16>());
    CInfo->npcflag = fields[18].Get<uint32>();
    CInfo->speed_walk = fields[19].Get<float>();
    CInfo->speed_run = fields[20].Get<float>();
    CInfo->speed_swim = fields[21].Get<float>();
    CInfo->speed_flight = fields[22].Get<float>();
    CInfo->detection_range = fields[23].Get<float>();
    CInfo->scale = fields[24].Get<float>();
    CInfo->rank = uint32(fields[25].Get<uint8>());
    CInfo->dmgschool = uint32(fields[26].Get<int8>());
    CInfo->DamageModifier = fields[27].Get<float>();
    CInfo->AsDamageModifier = AsDecimalToScaledInt64(fields[27].Get<std::string>());
    CInfo->BaseAttackTime = fields[28].Get<uint32>();
    CInfo->RangeAttackTime = fields[29].Get<uint32>();
    CInfo->BaseVariance = fields[30].Get<float>();
    CInfo->RangeVariance = fields[31].Get<float>();
    CInfo->unit_class = uint32(fields[32].Get<uint8>());
    CInfo->unit_flags = fields[33].Get<uint32>();
    CInfo->unit_flags2 = fields[34].Get<uint32>();
    CInfo->dynamicflags = fields[35].Get<uint32>();
    CInfo->family = uint32(fields[36].Get<uint8>());
    CInfo->trainer_type = uint32(fields[37].Get<uint8>());
    CInfo->trainer_spell = fields[38].Get<uint32>();
    CInfo->trainer_class = uint32(fields[39].Get<uint8>());
    CInfo->trainer_race = uint32(fields[40].Get<uint8>());
    CInfo->type = uint32(fields[41].Get<uint8>());
    CInfo->type_flags = fields[42].Get<uint32>();
    CInfo->lootid = fields[43].Get<uint32>();
    CInfo->pickpocketLootId = fields[44].Get<uint32>();
    CInfo->SkinLootId = fields[45].Get<uint32>();

    for (uint8 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
    {
        CInfo->resistance[i] = 0;
    }

    for (uint8 i = 0; i < MAX_CREATURE_SPELLS; ++i)
    {
        CInfo->spells[i] = 0;
    }

    CInfo->PetSpellDataId = fields[46].Get<uint32>();
    CInfo->VehicleId = fields[47].Get<uint32>();
    CInfo->mingold = fields[48].Get<uint32>();
    CInfo->maxgold = fields[49].Get<uint32>();
    CInfo->AIName = fields[50].Get<std::string>();
    CInfo->MovementType = uint32(fields[51].Get<uint8>());
    if (!fields[52].IsNull())
    {
        CInfo->Movement.Ground = static_cast<CreatureGroundMovementType>(fields[52].Get<uint8>());
    }

    CInfo->Movement.Swim = fields[53].Get<bool>();
    if (!fields[54].IsNull())
    {
        CInfo->Movement.Flight = static_cast<CreatureFlightMovementType>(fields[54].Get<uint8>());
    }

    CInfo->Movement.Rooted = fields[55].Get<bool>();
    if (!fields[56].IsNull())
    {
        CInfo->Movement.Chase = static_cast<CreatureChaseMovementType>(fields[56].Get<uint8>());
    }
    if (!fields[57].IsNull())
    {
        CInfo->Movement.Random = static_cast<CreatureRandomMovementType>(fields[57].Get<uint8>());
    }
    if (!fields[58].IsNull())
    {
        CInfo->Movement.InteractionPauseTimer = fields[58].Get<uint32>();
    }

    CInfo->HoverHeight = fields[59].Get<float>();
    CInfo->ModHealth = fields[60].Get<float>();
    CInfo->AsModHealth = AsDecimalToScaledInt64(fields[60].Get<std::string>());
    CInfo->ModMana = fields[61].Get<float>();
    CInfo->AsModMana = AsDecimalToScaledInt64(fields[61].Get<std::string>());
    CInfo->ModArmor = fields[62].Get<float>();
    CInfo->AsModArmor = AsDecimalToScaledInt64(fields[62].Get<std::string>());
    CInfo->ModExperience = fields[63].Get<float>();
    CInfo->RacialLeader = fields[64].Get<bool>();
    CInfo->movementId = fields[65].Get<uint32>();
    CInfo->RegenHealth = fields[66].Get<bool>();
    CInfo->MechanicImmuneMask = fields[67].Get<uint32>();
    CInfo->SpellSchoolImmuneMask = fields[68].Get<uint8>();
    CInfo->flags_extra = fields[69].Get<uint32>();
    CInfo->ScriptID = GetScriptId(fields[70].Get<std::string>());

    if (CreId == 34816)
        CInfo->npcflag = 1;

    // pussywizard:
    {
        uint32 max = 0;
        for (CreatureTemplateContainer::const_iterator itr = _creatureTemplateStore.begin(); itr != _creatureTemplateStore.end(); ++itr)
            if (itr->first > max)
                max = itr->first;
        if (max)
        {
            _creatureTemplateStoreFast.clear();
            _creatureTemplateStoreFast.resize(max + 1, nullptr);
            for (CreatureTemplateContainer::iterator itr = _creatureTemplateStore.begin(); itr != _creatureTemplateStore.end(); ++itr)
                _creatureTemplateStoreFast[itr->first] = &(itr->second);
        }
    }

    Result = WorldDatabase.Query("SELECT CreatureID, School, Resistance FROM creature_template_resistance WHERE CreatureID = {};", CreId);
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();

            uint32 creatureID = fields[0].Get<uint32>();
            uint8 school = fields[1].Get<uint8>();

            if (school == SPELL_SCHOOL_NORMAL || school >= MAX_SPELL_SCHOOL)
                continue;

            CreatureTemplateContainer::iterator itr = _creatureTemplateStore.find(creatureID);
            if (itr == _creatureTemplateStore.end())
                continue;

            CreatureTemplate& creatureTemplate = itr->second;
            creatureTemplate.resistance[school] = fields[2].Get<int16>();

        } while (Result->NextRow());
    }

    Result = WorldDatabase.Query("SELECT CreatureID, `Index`, Spell FROM creature_template_spell WHERE CreatureID = {};", CreId);
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();

            uint32 creatureID = fields[0].Get<uint32>();
            uint8 index = fields[1].Get<uint8>();

            if (index >= MAX_CREATURE_SPELLS)
                continue;

            CreatureTemplateContainer::iterator itr = _creatureTemplateStore.find(creatureID);
            if (itr == _creatureTemplateStore.end())
                continue;

            CreatureTemplate& creatureTemplate = itr->second;
            creatureTemplate.spells[index] = fields[2].Get<uint32>();

        } while (Result->NextRow());
    }

    CheckCreatureTemplate(CInfo);

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        WorldPacket Packet;
        Packet << CInfo->Entry;
        Packet << ObjectGuid::Empty;
        Itr->second->HandleCreatureQueryOpcode(Packet);
    }
}

void Helper::UpdateCreTempForEntry(Creature* Cre)
{
    CreatureTemplate* CInfo = const_cast<CreatureTemplate*>(Cre->GetCreatureTemplate());
    if (!CInfo)
        return;

    QueryResult Result = WorldDatabase.Query("SELECT entry, difficulty_entry_1, difficulty_entry_2, difficulty_entry_3, KillCredit1, KillCredit2, modelid1, modelid2, modelid3, "
        //                        9         10    11       12        13              14        15        16   17       18       19          20         21          22
        "modelid4, name, subname, IconName, gossip_menu_id, minlevel, maxlevel, exp, faction, npcflag, speed_walk, speed_run, speed_swim, speed_flight, "
        //                        23               24     25      26         27              28              29               30            31             32          33          34
        "detection_range, scale, `rank`, dmgschool, DamageModifier, BaseAttackTime, RangeAttackTime, BaseVariance, RangeVariance, unit_class, unit_flags, unit_flags2, "
        //                        35            36      37            38             39             40            41
        "dynamicflags, family, trainer_type, trainer_spell, trainer_class, trainer_race, type, "
        //                        42          43      44              45        46              47         48       49       50      51
        "type_flags, lootid, pickpocketloot, skinloot, PetSpellDataId, VehicleId, mingold, maxgold, AIName, MovementType, "
        //                        52          53        54          55          56         57          58                         59           60              61            62             63
        "ctm.Ground, ctm.Swim, ctm.Flight, ctm.Rooted, ctm.Chase, ctm.Random, ctm.InteractionPauseTimer, HoverHeight, HealthModifier, ManaModifier, ArmorModifier, ExperienceModifier, "
        //                        64            65          66           67                    68                        69           70
        "RacialLeader, movementId, RegenHealth, mechanic_immune_mask, spell_school_immune_mask, flags_extra, ScriptName "
        "FROM creature_template ct LEFT JOIN creature_template_movement ctm ON ct.entry = ctm.CreatureId WHERE ct.entry = {};", Cre->GetEntry());

    if (!Result)
        return;

    Field* fields = Result->Fetch();
    CInfo->Entry = Cre->GetEntry();

    for (uint8 i = 0; i < MAX_DIFFICULTY - 1; ++i)
    {
        CInfo->DifficultyEntry[i] = fields[1 + i].Get<uint32>();
    }

    for (uint8 i = 0; i < MAX_KILL_CREDIT; ++i)
    {
        CInfo->KillCredit[i] = fields[4 + i].Get<uint32>();
    }

    CInfo->Modelid1 = fields[6].Get<uint32>();
    CInfo->Modelid2 = fields[7].Get<uint32>();
    CInfo->Modelid3 = fields[8].Get<uint32>();
    CInfo->Modelid4 = fields[9].Get<uint32>();
    CInfo->Name = fields[10].Get<std::string>();
    CInfo->SubName = fields[11].Get<std::string>();
    CInfo->IconName = fields[12].Get<std::string>();
    CInfo->GossipMenuId = fields[13].Get<uint32>();
    CInfo->minlevel = fields[14].Get<uint8>();
    CInfo->maxlevel = fields[15].Get<uint8>();
    CInfo->expansion = uint32(fields[16].Get<int16>());
    CInfo->faction = uint32(fields[17].Get<uint16>());
    CInfo->npcflag = fields[18].Get<uint32>();
    CInfo->speed_walk = fields[19].Get<float>();
    CInfo->speed_run = fields[20].Get<float>();
    CInfo->speed_swim = fields[21].Get<float>();
    CInfo->speed_flight = fields[22].Get<float>();
    CInfo->detection_range = fields[23].Get<float>();
    CInfo->scale = fields[24].Get<float>();
    CInfo->rank = uint32(fields[25].Get<uint8>());
    CInfo->dmgschool = uint32(fields[26].Get<int8>());
    CInfo->DamageModifier = fields[27].Get<float>();
    CInfo->AsDamageModifier = AsDecimalToScaledInt64(fields[27].Get<std::string>());
    CInfo->BaseAttackTime = fields[28].Get<uint32>();
    CInfo->RangeAttackTime = fields[29].Get<uint32>();
    CInfo->BaseVariance = fields[30].Get<float>();
    CInfo->RangeVariance = fields[31].Get<float>();
    CInfo->unit_class = uint32(fields[32].Get<uint8>());
    CInfo->unit_flags = fields[33].Get<uint32>();
    CInfo->unit_flags2 = fields[34].Get<uint32>();
    CInfo->dynamicflags = fields[35].Get<uint32>();
    CInfo->family = uint32(fields[36].Get<uint8>());
    CInfo->trainer_type = uint32(fields[37].Get<uint8>());
    CInfo->trainer_spell = fields[38].Get<uint32>();
    CInfo->trainer_class = uint32(fields[39].Get<uint8>());
    CInfo->trainer_race = uint32(fields[40].Get<uint8>());
    CInfo->type = uint32(fields[41].Get<uint8>());
    CInfo->type_flags = fields[42].Get<uint32>();
    CInfo->lootid = fields[43].Get<uint32>();
    CInfo->pickpocketLootId = fields[44].Get<uint32>();
    CInfo->SkinLootId = fields[45].Get<uint32>();

    for (uint8 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
    {
        CInfo->resistance[i] = 0;
    }

    for (uint8 i = 0; i < MAX_CREATURE_SPELLS; ++i)
    {
        CInfo->spells[i] = 0;
    }

    CInfo->PetSpellDataId = fields[46].Get<uint32>();
    CInfo->VehicleId = fields[47].Get<uint32>();
    CInfo->mingold = fields[48].Get<uint32>();
    CInfo->maxgold = fields[49].Get<uint32>();
    CInfo->AIName = fields[50].Get<std::string>();
    CInfo->MovementType = uint32(fields[51].Get<uint8>());
    if (!fields[52].IsNull())
    {
        CInfo->Movement.Ground = static_cast<CreatureGroundMovementType>(fields[52].Get<uint8>());
    }

    CInfo->Movement.Swim = fields[53].Get<bool>();
    if (!fields[54].IsNull())
    {
        CInfo->Movement.Flight = static_cast<CreatureFlightMovementType>(fields[54].Get<uint8>());
    }

    CInfo->Movement.Rooted = fields[55].Get<bool>();
    if (!fields[56].IsNull())
    {
        CInfo->Movement.Chase = static_cast<CreatureChaseMovementType>(fields[56].Get<uint8>());
    }
    if (!fields[57].IsNull())
    {
        CInfo->Movement.Random = static_cast<CreatureRandomMovementType>(fields[57].Get<uint8>());
    }
    if (!fields[58].IsNull())
    {
        CInfo->Movement.InteractionPauseTimer = fields[58].Get<uint32>();
    }

    CInfo->HoverHeight = fields[59].Get<float>();
    CInfo->ModHealth = fields[60].Get<float>();
    CInfo->AsModHealth = AsDecimalToScaledInt64(fields[60].Get<std::string>());
    CInfo->ModMana = fields[61].Get<float>();
    CInfo->AsModMana = AsDecimalToScaledInt64(fields[61].Get<std::string>());
    CInfo->ModArmor = fields[62].Get<float>();
    CInfo->AsModArmor = AsDecimalToScaledInt64(fields[62].Get<std::string>());
    CInfo->ModExperience = fields[63].Get<float>();
    CInfo->RacialLeader = fields[64].Get<bool>();
    CInfo->movementId = fields[65].Get<uint32>();
    CInfo->RegenHealth = fields[66].Get<bool>();
    CInfo->MechanicImmuneMask = fields[67].Get<uint32>();
    CInfo->SpellSchoolImmuneMask = fields[68].Get<uint8>();
    CInfo->flags_extra = fields[69].Get<uint32>();
    CInfo->ScriptID = sObjectMgr->GetScriptId(fields[70].Get<std::string>());

    if (Cre->GetEntry() == 34816)
        CInfo->npcflag = 1;

    Result = WorldDatabase.Query("SELECT CreatureID, School, Resistance FROM creature_template_resistance WHERE CreatureID = {};", Cre->GetEntry());
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();

            uint32 creatureID = fields[0].Get<uint32>();
            uint8 school = fields[1].Get<uint8>();

            if (school == SPELL_SCHOOL_NORMAL || school >= MAX_SPELL_SCHOOL)
                continue;

            CInfo->resistance[school] = fields[2].Get<int16>();

        } while (Result->NextRow());
    }

    Result = WorldDatabase.Query("SELECT CreatureID, `Index`, Spell FROM creature_template_spell WHERE CreatureID = {};", Cre->GetEntry());
    if (Result)
    {
        do
        {
            Field* fields = Result->Fetch();

            uint32 creatureID = fields[0].Get<uint32>();
            uint8 index = fields[1].Get<uint8>();

            if (index >= MAX_CREATURE_SPELLS)
                continue;

            CInfo->spells[index] = fields[2].Get<uint32>();

        } while (Result->NextRow());
    }

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        WorldPacket Packet;
        Packet << CInfo->Entry;
        Packet << ObjectGuid::Empty;
        Itr->second->HandleCreatureQueryOpcode(Packet);
    }

    Cre->UpdateEntry(Cre->GetEntry());
}

//取容器中最长的连续数的数量 检查签到或补签时是否命中连续签到奖励 排序法 例: {1, 2, 3, 4, 7, 8} 返回4
uint32 Helper::GetSeriesDays(std::vector<uint32> DoList)
{
    uint32 Maxlen = 0;
    if (DoList.empty())
        return Maxlen;

    if (DoList.size() == 1)
        return DoList.size();

    std::sort(DoList.begin(), DoList.end(), [](const uint32& V1, const uint32& V2) {return V1 < V2; });

    uint32 TemMaxLen = 1;
    for (uint32 i = 1; i < DoList.size(); i++)
    {
        if (DoList[i] == DoList[i - 1] + 1)
        {
            TemMaxLen++;
        }
        else
        {
            TemMaxLen = 1;
        }

        if (TemMaxLen > Maxlen)
        {
            Maxlen = TemMaxLen;
        }
    }

    return Maxlen;
}

void Helper::SignRewardExtra(Player* player, uint32 SeriesDays, uint32 Total)
{
    if (player->SignContinuous && player->SignTotal)
        return;

    uint32 Days = uint32(GetUInt(1051));
    uint32 AccountId = player->GetSession()->GetAccountId();

    //命中连续天数
    if (SeriesDays >= Days && !player->SignContinuous)
    {
        LoginDatabasePreparedStatement* AStmt = LoginDatabase.GetPreparedStatement(LOGIN_REP_LX_SIGN);
        AStmt->SetData(0, AccountId);
        AStmt->SetData(1, 1);
        AStmt->SetData(2, 0);
        LoginDatabase.Execute(AStmt);
        player->SignContinuous = true;

        auto Itr = SignRewardStore.find(SignLq);
        if (Itr != SignRewardStore.end())
        {
            if (Itr->second.ItemId1 > 0 && Itr->second.Count1 > 0)
                player->AddItem(Itr->second.ItemId1, Itr->second.Count1, "连续签到获得");
            if (Itr->second.ItemId2 > 0 && Itr->second.Count2 > 0)
                player->AddItem(Itr->second.ItemId2, Itr->second.Count2, "连续签到获得");
            if (Itr->second.ItemId3 > 0 && Itr->second.Count3 > 0)
                player->AddItem(Itr->second.ItemId3, Itr->second.Count3, "连续签到获得");
            if (Itr->second.ItemId4 > 0 && Itr->second.Count4 > 0)
                player->AddItem(Itr->second.ItemId4, Itr->second.Count4, "连续签到获得");
            if (Itr->second.ItemId5 > 0 && Itr->second.Count5 > 0)
                player->AddItem(Itr->second.ItemId5, Itr->second.Count5, "连续签到获得");
            if (Itr->second.ItemId6 > 0 && Itr->second.Count6 > 0)
                player->AddItem(Itr->second.ItemId6, Itr->second.Count6, "连续签到获得");
        }

        player->SendSysMsg(sHelper->GetGts(1126), Days);
    }

    Days = uint32(GetUInt(1052));

    //命中累计天数
    if (Total >= Days && !player->SignTotal)
    {
        LoginDatabasePreparedStatement* BStmt = LoginDatabase.GetPreparedStatement(LOGIN_REP_LX_SIGN);
        BStmt->SetData(0, AccountId);
        BStmt->SetData(1, 1);
        BStmt->SetData(2, 1);
        LoginDatabase.Execute(BStmt);
        player->SignTotal = true;

        auto It = SignRewardStore.find(SignLp);
        if (It != SignRewardStore.end())
        {
            if (It->second.ItemId1 > 0 && It->second.Count1 > 0)
                player->AddItem(It->second.ItemId1, It->second.Count1, "累计签到获得");
            if (It->second.ItemId2 > 0 && It->second.Count2 > 0)
                player->AddItem(It->second.ItemId2, It->second.Count2, "累计签到获得");
            if (It->second.ItemId3 > 0 && It->second.Count3 > 0)
                player->AddItem(It->second.ItemId3, It->second.Count3, "累计签到获得");
            if (It->second.ItemId4 > 0 && It->second.Count4 > 0)
                player->AddItem(It->second.ItemId4, It->second.Count4, "累计签到获得");
            if (It->second.ItemId5 > 0 && It->second.Count5 > 0)
                player->AddItem(It->second.ItemId5, It->second.Count5, "累计签到获得");
            if (It->second.ItemId6 > 0 && It->second.Count6 > 0)
                player->AddItem(It->second.ItemId6, It->second.Count6, "累计签到获得");
        }

        player->SendSysMsg(sHelper->GetGts(1127), Days);
    }
}

uint32 Helper::GetDaysInMonth(uint32 NowYear, uint32 NowMonth)
{
    uint32 Days;
    uint32 DayVe[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (2 == NowMonth)
    {
        Days = (((0 == NowYear % 4) && (0 != NowYear % 100) || (0 == NowYear % 400)) ? 29 : 28);
    }
    else
    {
        Days = DayVe[NowMonth - 1];
    }
    return Days;
}

std::string Helper::SecTimeString(uint64 TimeInSecs, bool ShortText)
{
    uint64 Secs = TimeInSecs % MINUTE;
    uint64 Minutes = TimeInSecs % HOUR / MINUTE;
    uint64 Hours = TimeInSecs % DAY / HOUR;
    uint64 Days = TimeInSecs / DAY;

    std::ostringstream Oss;
    if (Days)
        Oss << Days << (ShortText ? "天" : "天");
    if (Hours)
        Oss << Hours << (ShortText ? "时" : "小时");
    if (Minutes)
        Oss << Minutes << (ShortText ? "分" : "分钟");
    if (Secs || (!Days && !Hours && !Minutes))
        Oss << Secs << (ShortText ? "秒" : "秒钟");

    std::string Str = Oss.str();

    if (!ShortText && !Str.empty() && Str[Str.size() - 1] == ' ')
        Str.resize(Str.size() - 1);

    return Str;
}

bool Helper::IsAsNum(std::string Str)
{
    for (int i = 0; i < Str.size(); i++)
    {
        int Tmp = (int)Str[i];
        if (Tmp >= 48 && Tmp <= 57)
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    return true;
}

void Helper::PopToPlayerDialog(Player* player, std::string OnText, uint32 Sender, uint32 Action, uint32 Menu)
{
    if (player->isDead())
    {
        player->ResurrectPlayer(1.0f);
        player->SpawnCorpseBones();
    }

    ClearGossipMenuFor(player);
    AddGossipItemFor(player, GOSSIP_ICON_TALK, "", Sender, Action, OnText, 0, false);
    GossipSetMenuId(player, Menu);
    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
}

std::string Helper::GetItemSlotName(uint32 Slot)
{
    switch (Slot)
    {
    case EQUIPMENT_SLOT_HEAD:
        return "头部";
    case EQUIPMENT_SLOT_NECK:
        return "项链";
    case EQUIPMENT_SLOT_SHOULDERS:
        return "肩膀";
    case EQUIPMENT_SLOT_BODY:
        return "衬衣";
    case EQUIPMENT_SLOT_CHEST:
        return "胸甲";
    case EQUIPMENT_SLOT_WAIST:
        return "腰带";
    case EQUIPMENT_SLOT_LEGS:
        return "裤子";
    case EQUIPMENT_SLOT_FEET:
        return "鞋子";
    case EQUIPMENT_SLOT_WRISTS:
        return "手腕";
    case EQUIPMENT_SLOT_HANDS:
        return "手套";
    case EQUIPMENT_SLOT_FINGER1:
        return "上戒";
    case EQUIPMENT_SLOT_FINGER2:
        return "下戒";
    case EQUIPMENT_SLOT_TRINKET1:
        return "上饰";
    case EQUIPMENT_SLOT_TRINKET2:
        return "下饰";
    case EQUIPMENT_SLOT_BACK:
        return "背部";
    case EQUIPMENT_SLOT_MAINHAND:
        return "主手";
    case EQUIPMENT_SLOT_OFFHAND:
        return "副手";
    case EQUIPMENT_SLOT_RANGED:
        return "远程";
    case EQUIPMENT_SLOT_TABARD:
        return "战袍";
    }

    return "未知";
}

bool Helper::IsCheckString(std::string CheckString, std::string LogReason, WorldSession* Session, WorldPacket& RecvData)
{
    if (CheckString.empty())
        return true;

    int Mum = CheckString.size();
    int i = 0;
    while (i < Mum)
    {
        int Count = 1;

        if (CheckString[i] & 0x80)
        {
            char Temp = CheckString[i];
            Temp <<= 1;
            do {
                Temp <<= 1;
                ++Count;
            } while (Temp & 0x80);
        }

        std::string Subs;
        Subs = CheckString.substr(i, Count);

        switch (Count)
        {
        case 1:
            break;
        case 3:
        {
            uint32 At = ((Subs[0] & 0xFF) << 16) | ((Subs[1] & 0xFF) << 8) | (Subs[2] & 0xFF);

            bool IsMark = false;

            switch (At)
            {
            case 0xE38082://。
            case 0xE38090://【】
            case 0xE38091:
            case 0xEFBC8C://，
            case 0xE2809C://“”
            case 0xE2809D:
            case 0xEfBC9B://；
            case 0xE38081://、
            case 0xEFBC9F://？
            case 0xEFBC88://（）
            case 0xEFBC89:
            case 0xefbc87://＇
            case 0xe288b6://∶
            case 0xefbd9e://～
            case 0xe38085://々
            case 0xefb99f://﹟
            case 0xefbd80://｀
            case 0xefb9a9://﹩
            case 0xe3808e://『』
            case 0xe3808f:
            case 0xe38096://〖〗
            case 0xe38097:
            case 0xefbcbb://［］
            case 0xefbcbd:
            case 0xe3808a://《》
            case 0xe3808b:
            case 0xefb99b://﹛﹜
            case 0xefb99c:
            case 0xe3808c://「」
            case 0xe3808d:
            case 0xe285a0://Ⅰ-Ⅹ
            case 0xe285a1:
            case 0xe285a2:
            case 0xe285a3:
            case 0xe285a4:
            case 0xe285a5:
            case 0xe285a6:
            case 0xe285a7:
            case 0xe285a8:
            case 0xe285a9:
            case 0xe2978b://○
            case 0xe29787://◇
            case 0xe296a1://□
            case 0xe296b3://△
            case 0xe296bd://▽
            case 0xe29886://☆
            case 0xe2978f://●
            case 0xe29786://◆
            case 0xe296b2://▲
            case 0xe296bc://▼
            case 0xe29980://♀
            case 0xe29982://♂
            case 0xe2889a://?√
            case 0xe2978e://◎
            case 0xe28a99://⊙
            case 0xe28690://←→
            case 0xe28692://
                IsMark = true;
            }

            if (!IsMark && (At < 0xE4B880 || At > 0xE9BEA5))
            {
                /*if (Session)
                    LOG_ERROR("server.worldserver", "检测违规 >> 此IP: {} 正非法违规: {} 请GM特别注意", Session->GetRemoteAddress().c_str(), LogReason.c_str());*/

                RecvData.rfinish();
                return false;
            }
        }
        break;
        default:
            /*if (Session)
                LOG_ERROR("server.worldserver", "检测违规 >> 此IP: {} 正非法违规: {} 请GM特别注意", Session->GetRemoteAddress().c_str(), LogReason.c_str());*/

            RecvData.rfinish();
            return false;
        }

        i += Count;
    }

    return true;
}

bool Helper::IsCheckString(std::string CheckString, std::string LogReason, WorldSession* Session)
{
    if (CheckString.empty())
        return true;

    int Mum = CheckString.size();
    int i = 0;
    while (i < Mum)
    {
        int Count = 1;

        if (CheckString[i] & 0x80)
        {
            char Temp = CheckString[i];
            Temp <<= 1;
            do {
                Temp <<= 1;
                ++Count;
            } while (Temp & 0x80);
        }

        std::string Subs;
        Subs = CheckString.substr(i, Count);

        switch (Count)
        {
        case 1:
            break;
        case 3:
        {
            uint32 At = ((Subs[0] & 0xFF) << 16) | ((Subs[1] & 0xFF) << 8) | (Subs[2] & 0xFF);

            bool IsMark = false;

            switch (At)
            {
            case 0xE38082://。
            case 0xE38090://【】
            case 0xE38091:
            case 0xEFBC8C://，
            case 0xE2809C://“”
            case 0xE2809D:
            case 0xEfBC9B://；
            case 0xE38081://、
            case 0xEFBC9F://？
            case 0xEFBC88://（）
            case 0xEFBC89:
            case 0xefbc87://＇
            case 0xe288b6://∶
            case 0xefbd9e://～
            case 0xe38085://々
            case 0xefb99f://﹟
            case 0xefbd80://｀
            case 0xefb9a9://﹩
            case 0xe3808e://『』
            case 0xe3808f:
            case 0xe38096://〖〗
            case 0xe38097:
            case 0xefbcbb://［］
            case 0xefbcbd:
            case 0xe3808a://《》
            case 0xe3808b:
            case 0xefb99b://﹛﹜
            case 0xefb99c:
            case 0xe3808c://「」
            case 0xe3808d:
            case 0xe285a0://Ⅰ-Ⅹ
            case 0xe285a1:
            case 0xe285a2:
            case 0xe285a3:
            case 0xe285a4:
            case 0xe285a5:
            case 0xe285a6:
            case 0xe285a7:
            case 0xe285a8:
            case 0xe285a9:
            case 0xe2978b://○
            case 0xe29787://◇
            case 0xe296a1://□
            case 0xe296b3://△
            case 0xe296bd://▽
            case 0xe29886://☆
            case 0xe2978f://●
            case 0xe29786://◆
            case 0xe296b2://▲
            case 0xe296bc://▼
            case 0xe29980://♀
            case 0xe29982://♂
            case 0xe2889a://?√
            case 0xe2978e://◎
            case 0xe28a99://⊙
            case 0xe28690://←→
            case 0xe28692://
                IsMark = true;
            }

            if (!IsMark && (At < 0xE4B880 || At > 0xE9BEA5))
            {
                /*if (Session)
                    LOG_ERROR("server.worldserver", "检测违规 >> 此IP: {} 正非法违规: {} 请GM特别注意", Session->GetRemoteAddress().c_str(), LogReason.c_str());*/

                return false;
            }
        }
        break;
        default:
            /*if (Session)
                LOG_ERROR("server.worldserver", "检测违规 >> 此IP: {} 正非法违规: {} 请GM特别注意", Session->GetRemoteAddress().c_str(), LogReason.c_str());*/

            return false;
        }

        i += Count;
    }

    return true;
}

uint32 Helper::GetItemIdByGlyph(uint32 GlyphId)
{
    uint32 SpellId = 0;
    for (SpellEntry const* SEntry : sSpellStore)
    {
        bool IsGlyphEffect = (SEntry->Effect[0] == SPELL_EFFECT_APPLY_GLYPH
            || SEntry->Effect[1] == SPELL_EFFECT_APPLY_GLYPH
            || SEntry->Effect[2] == SPELL_EFFECT_APPLY_GLYPH
            );

        bool IsGlyphMiscValue = (SEntry->EffectMiscValue[0] == GlyphId
            || SEntry->EffectMiscValue[1] == GlyphId
            || SEntry->EffectMiscValue[2] == GlyphId
            );

        if (SEntry->Targets == TARGET_FLAG_GLYPH_SLOT && IsGlyphEffect && IsGlyphMiscValue)
            SpellId = SEntry->Id;
    }

    uint32 ItemId = 0;
    if (SpellId)
    {
        ItemTemplateContainer const* ItemTemplates = sObjectMgr->GetItemTemplateStore();
        for (ItemTemplateContainer::const_iterator Itr = ItemTemplates->begin(); Itr != ItemTemplates->end(); ++Itr)
        {
            bool IsGlyphSpell = (Itr->second.Spells[0].SpellId == SpellId
                || Itr->second.Spells[1].SpellId == SpellId
                || Itr->second.Spells[2].SpellId == SpellId
                || Itr->second.Spells[3].SpellId == SpellId
                || Itr->second.Spells[4].SpellId == SpellId
                );

            if (Itr->second.Class == ITEM_CLASS_GLYPH && IsGlyphSpell)
                ItemId = Itr->first;
        }
    }

    return ItemId;
}

bool Helper::IsDKEns(Item* DoItem)
{
    bool In = false;
    for (uint8 i = 0; i < MAX_ENCHANTMENT_SLOT; ++i)
    {
        uint32 EnchantId = DoItem->GetEnchantmentId(EnchantmentSlot(i));
        if (EnchantId)
        {
            if (EnchantId == 3370
                || EnchantId == 3368
                || EnchantId == 3366
                || EnchantId == 3367
                || EnchantId == 3595
                || EnchantId == 3369
                || EnchantId == 3847
                || EnchantId == 3594
                || EnchantId == 3883
                || EnchantId == 3365)
            {
                In = true;
            }
        }
    }

    return In;
}

void Helper::ClearDKEns(Player* player)
{
    if (player->GetClass() != CLASS_DEATH_KNIGHT)
        return;

    std::vector<Item*> ItemList;
    ItemList.clear();
    //身上和主包
    for (uint8 i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; i++)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            ItemList.push_back(pItem);

    //额外四包
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
        if (Bag* pBag = player->GetBagByPos(i))
            for (uint32 j = 0; j < pBag->GetBagSize(); j++)
                if (Item* pItem = player->GetItemByPos(i, j))
                    ItemList.push_back(pItem);

    //银行
    for (uint8 i = BANK_SLOT_ITEM_START; i < BANK_SLOT_BAG_END; ++i)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            ItemList.push_back(pItem);

    //银行包
    for (uint8 i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        if (Bag* pBag = player->GetBagByPos(i))
            for (uint32 j = 0; j < pBag->GetBagSize(); j++)
                if (Item* pItem = player->GetItemByPos(i, j))
                    ItemList.push_back(pItem);

    if (!ItemList.empty())
    {
        for (auto Itr = ItemList.begin(); Itr != ItemList.end(); ++Itr)
        {
            Item* DoItem = *Itr;
            for (uint8 i = 0; i < MAX_ENCHANTMENT_SLOT; ++i)
            {
                uint32 EnchantId = DoItem->GetEnchantmentId(EnchantmentSlot(i));
                if (EnchantId)
                {
                    if (EnchantId == 3370
                        || EnchantId == 3368
                        || EnchantId == 3366
                        || EnchantId == 3367
                        || EnchantId == 3595
                        || EnchantId == 3369
                        || EnchantId == 3847
                        || EnchantId == 3594
                        || EnchantId == 3883
                        || EnchantId == 3365)
                    {
                        DoItem->ClearEnchantment(EnchantmentSlot(i));
                    }
                }
            }
        }
        ItemList.clear();
    }
}

std::string Helper::GetCustomHome()
{
    return ".go -248.26 922.14 85.37 530 4.75";
}

std::string Helper::ToExString(float Value)
{
    char str[1024];
    if (Value > 0.0f)
    {
        sprintf(str, "%2.2f", Value);
        return std::string(str) + "%";
    }

    return std::string("00%");
}

std::string Helper::GetUserNameByAccountId(uint32 AccountId)
{
    LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_GET_USERNAME_BY_ID);
    Stmt->SetData(0, AccountId);
    PreparedQueryResult Result = LoginDatabase.Query(Stmt);
    return (Result) ? (*Result)[0].Get<std::string>() : "";
}

std::string Helper::GetRaceString(uint8 Race)
{
    switch (Race)
    {
    case RACE_HUMAN:
        return "人类";
    case RACE_ORC:
        return "兽人";
    case RACE_DWARF:
        return "矮人";
    case RACE_NIGHTELF:
        return "暗夜精灵";
    case RACE_UNDEAD_PLAYER:
        return "亡灵";
    case RACE_TAUREN:
        return "牛头人";
    case RACE_GNOME:
        return "侏儒";
    case RACE_TROLL:
        return "巨魔";
    case RACE_BLOODELF:
        return "血精灵";
    case RACE_DRAENEI:
        return "德莱尼";
    }

    return "未知";
}

std::string Helper::GetClassString(uint8 Class)
{
    switch (Class)
    {
    case CLASS_WARRIOR:
        return "战士";
    case CLASS_PALADIN:
        return "骑士";
    case CLASS_HUNTER:
        return "猎人";
    case CLASS_ROGUE:
        return "盗贼";
    case CLASS_PRIEST:
        return "牧师";
    case CLASS_DEATH_KNIGHT:
        return "死亡骑士";
    case CLASS_SHAMAN:
        return "萨满";
    case CLASS_MAGE:
        return "法师";
    case CLASS_WARLOCK:
        return "术士";
    case CLASS_DRUID:
        return "德鲁伊";
    }

    return "未知";
}

std::string Helper::GetGossipClassString(uint8 Class)
{
    switch (Class)
    {
    case CLASS_WARRIOR:
        return "|TInterface/Icons/INV_Sword_27.png:30:30:-20:0|t 转职 [战士]";
    case CLASS_PALADIN:
        return "|TInterface/Icons/INV_Hammer_01.blp:30:30:-20:0|t 转职 [骑士]";
    case CLASS_HUNTER:
        return "|TInterface/Icons/INV_Weapon_Bow_07.blp:30:30:-20:0|t 转职 [猎人]";
    case CLASS_ROGUE:
        return "|TInterface/Icons/INV_ThrowingKnife_04.blp:30:30:-20:0|t 转职 [盗贼]";
    case CLASS_PRIEST:
        return "|TInterface/Icons/INV_Staff_30.blp:30:30:-20:0|t 转职 [牧师]";
    case CLASS_DEATH_KNIGHT:
        return "|TInterface/Icons/Spell_Deathknight_ClassIcon.blp:30:30:-20:0|t 转职 [死亡骑士]";
    case CLASS_SHAMAN:
        return "|TInterface/Icons/Spell_Nature_BloodLust.blp:30:30:-20:0|t 转职 [萨满]";
    case CLASS_MAGE:
        return "|TInterface/Icons/INV_Staff_13.blp:30:30:-20:0|t 转职 [法师]";
    case CLASS_WARLOCK:
        return "|TInterface/Icons/Spell_Nature_FaerieFire.blp:30:30:-20:0|t 转职 [术士]";
    case CLASS_DRUID:
        return "|TInterface/Icons/Ability_Druid_Maul.blp:30:30:-20:0|t 转职 [德鲁伊]";
    }

    return "未知";
}

std::string Helper::GetNtTeamStr(uint8 TeamId)
{
    switch (TeamId)
    {
    case 1:
        return "联盟";
    case 2:
        return "部落";
    }
    return "未知";
}

std::string Helper::GetBGTeamStr(TeamId Team)
{
    switch (Team)
    {
    case TEAM_ALLIANCE:
        return "|cFF0000FF联盟|r";
    case TEAM_HORDE:
        return "|cFFFF0000部落|r";
    }
    return "未知";
}

std::string Helper::GetPlayerNameByGuid(ObjectGuid Guid)
{
    if (CharacterCacheEntry const* CacheEntry = sCharacterCache->GetCharacterCacheByGuid(Guid))
        return CacheEntry->Name;;

    return "";
}

const char* Helper::GetSpellName(uint32 SpellId)
{
    if (SpellInfo const* SInfo = sSpellMgr->GetSpellInfo(SpellId))
    {
        std::ostringstream Link;
        Link << "|cffffffff|Hspell:" << SpellId << "|h[" << SInfo->SpellName[LOCALE_zhCN] << "]|h|r";
        return Link.str().c_str();
    }

    return "";
}

std::string Helper::GetAtSpellName(uint32 SpellId)
{
    if (SpellInfo const* SInfo = sSpellMgr->GetSpellInfo(SpellId))
    {
        std::ostringstream Link;
        Link << "|cffffffff|Hspell:" << SpellId << "|h[" << SInfo->SpellName[LOCALE_zhCN] << "]|h|r";
        return Link.str();
    }

    return "";
}

std::string Helper::GetSpellIdName(uint32 SpellId)
{
    if (SpellEntry const* SInfo = sSpellStore.LookupEntry(SpellId))
        return std::string(SInfo->SpellName[LOCALE_zhCN]);

    return "";
}

std::string Helper::GetSpellDes(uint32 SpellId)
{
    if (SpellEntry const* SInfo = sSpellStore.LookupEntry(SpellId))
        return std::string(SInfo->Rank[LOCALE_zhCN]);

    return "nullptr";
}

const char* Helper::GetAchievementName(uint32 AchId)
{
    if (AchievementEntry const* AEntry = sAchievementStore.LookupEntry(AchId))
    {
        std::ostringstream Link;
        Link << "|cffffff00|Hachievement:" << AchId << ":0000000000000001:0:0:0:-1:0:0:0:0|h[" << AEntry->name[LOCALE_zhCN] << "]|h|r";
        return Link.str().c_str();
    }

    return "";
}

const char* Helper::GetTitlesName(uint32 TitleId)
{
    if (CharTitlesEntry const* TitlesEntry = sCharTitlesStore.LookupEntry(TitleId))
        return TitlesEntry->nameMale[LOCALE_zhCN];

    return "";
}

const char* Helper::GetQuestName(uint32 QuestId)
{
    if (Quest const* QInfo = sObjectMgr->GetQuestTemplate(QuestId))
    {
        std::ostringstream Link;
        Link << "|cFFEFFD00|Hquest:" << QInfo->GetQuestId() << ':' << QInfo->GetQuestLevel() << "|h[" << QInfo->GetTitle() << "]|h|r";
        return Link.str().c_str();
    }

    return "";
}

std::string Helper::GetGtsFactionName(uint32 Faction)
{
    if (FactionEntry const* FEntry = sFactionStore.LookupEntry(Faction))
        return std::string(FEntry->name[LOCALE_zhCN]);

    return "未知";
}

const char* Helper::GetFactionName(uint32 Faction)
{
    if (FactionEntry const* FEntry = sFactionStore.LookupEntry(Faction))
    {
        std::ostringstream Link;
        Link << "|cffffffff|Hfaction:" << FEntry->ID << "|h[" << FEntry->name[LOCALE_zhCN] << "]|h|r";
        return Link.str().c_str();
    }

    return "";
}

std::string Helper::GetFactionRankName(uint8 Rank)
{
    switch (Rank)
    {
    case 0:
        return "仇恨";
    case 1:
        return "敌对";
    case 2:
        return "冷淡";
    case 3:
        return "中立";
    case 4:
        return "友善";
    case 5:
        return "尊敬";
    case 6:
        return "崇敬";
    case 7:
        return "崇拜";
    }
    return "未知";
}

const char* Helper::GetProName(uint32 Pro)
{
    if (SkillLineEntry const* SLine = sSkillLineStore.LookupEntry(Pro))
        return SLine->name[LOCALE_zhCN];

    return "";
}

std::string Helper::GetGenderString(uint8 Gr)
{
    switch (Gr)
    {
    case 1:
        return "男";
    case 2:
        return "女";
    }
    return "未知";
}

const char* Helper::GetMapName(int32 MapId)
{
    if (MapId < 0)
        return "未知";
    if (MapEntry const* MEntry = sMapStore.LookupEntry(MapId))
        return MEntry->name[LOCALE_zhCN];

    return "未知";
}

const char* Helper::GetAreaName(uint32 Id)
{
    if (AreaTableEntry const* AEntry = sAreaTableStore.LookupEntry(Id))
        return AEntry->area_name[LOCALE_zhCN];

    return "";
}

std::string Helper::GetEnchName(uint32 EnchId)
{
    if (SpellItemEnchantmentEntry const* SEntry = sSpellItemEnchantmentStore.LookupEntry(EnchId))
        return std::string(SEntry->description[LOCALE_zhCN]);

    return "nullptr";
}

uint32 Player::GetAchievementPoints()
{
    const CriteriaProgressMap Amap = GetAchievementMgr()->GetCompletedCriteriaProgress();
    auto Itr = Amap.find(12698);
    return Itr != Amap.end() ? Itr->second.counter : 0;
}

void Player::CustomCompleteQuest(uint32 QuestId)
{
    Quest const* QInfo = sObjectMgr->GetQuestTemplate(QuestId);
    if (!QInfo || GetQuestStatus(QuestId) == QUEST_STATUS_NONE || DisableMgr::IsDisabledFor(DISABLE_TYPE_QUEST, QuestId, nullptr))
        return;

    for (uint8 x = 0; x < QUEST_ITEM_OBJECTIVES_COUNT; ++x)
    {
        uint32 Id = QInfo->RequiredItemId[x];
        uint32 Count = QInfo->RequiredItemCount[x];
        if (!Id || !Count)
            continue;

        uint32 CurItemCount = GetItemCount(Id, true);

        ItemPosCountVec Dest;
        uint8 Msg = CanStoreNewItem(NULL_BAG, NULL_SLOT, Dest, Id, Count - CurItemCount);
        if (Msg == EQUIP_ERR_OK)
        {
            Item* pItem = StoreNewItem(Dest, Id, true, "完成任务获得");
            SendNewItem(pItem, Count - CurItemCount, true, false);
        }
    }

    for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        int32 Creature = QInfo->RequiredNpcOrGo[i];
        uint32 CreatureCount = QInfo->RequiredNpcOrGoCount[i];

        if (Creature > 0)
        {
            if (CreatureTemplate const* CreatureInfo = sObjectMgr->GetCreatureTemplate(Creature))
                for (uint16 z = 0; z < CreatureCount; ++z)
                    KilledMonster(CreatureInfo, ObjectGuid::Empty);
        }
        else if (Creature < 0)
            for (uint16 z = 0; z < CreatureCount; ++z)
                KillCreditGO(Creature);
    }

    if (QInfo->HasSpecialFlag(QUEST_SPECIAL_FLAGS_PLAYER_KILL))
        if (uint32 ReqPlayers = QInfo->GetPlayersSlain())
            KilledPlayerCreditForQuest(ReqPlayers, QInfo);

    if (uint32 RepFaction = QInfo->GetRepObjectiveFaction())
    {
        uint32 RepValue = QInfo->GetRepObjectiveValue();
        uint32 CurRep = GetReputationMgr().GetReputation(RepFaction);
        if (CurRep < RepValue)
            if (FactionEntry const* FEntry = sFactionStore.LookupEntry(RepFaction))
                GetReputationMgr().SetReputation(FEntry, RepValue);
    }

    if (uint32 RepFaction2 = QInfo->GetRepObjectiveFaction2())
    {
        uint32 RepValue2 = QInfo->GetRepObjectiveValue2();
        uint32 CurRep2 = GetReputationMgr().GetReputation(RepFaction2);
        if (CurRep2 < RepValue2)
            if (FactionEntry const* FEntry2 = sFactionStore.LookupEntry(RepFaction2))
                GetReputationMgr().SetReputation(FEntry2, RepValue2);
    }

    int32 ReqOrRewMoney = QInfo->GetRewOrReqMoney();
    if (ReqOrRewMoney < 0)
        ModifyMoney(-ReqOrRewMoney);

    if (sWorld->getBoolConfig(CONFIG_QUEST_ENABLE_QUEST_TRACKER))
    {
        CharacterDatabasePreparedStatement* Stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_QUEST_TRACK_GM_COMPLETE);
        Stmt->SetData(0, QInfo->GetQuestId());
        Stmt->SetData(1, GetGUIDLow());
        CharacterDatabase.Execute(Stmt);
    }

    CompleteQuest(QuestId);
}

std::string Helper::GetCreatureScriptName(Creature* Cr)
{
    if (Cr)
        return sObjectMgr->GetScriptName(Cr->GetScriptId());

    return "";
}

std::string Helper::GetCreatureScriptName(uint32 CreatureId)
{
    if (CreatureTemplate const* CInfo = sObjectMgr->GetCreatureTemplate(CreatureId))
        return  sObjectMgr->GetScriptName(CInfo->ScriptID);

    return "";
}

std::string Helper::GetItemScriptName(Item* DoItem)
{
    if (DoItem)
        return sObjectMgr->GetScriptName(DoItem->GetScriptId());

    return "";
}

std::string Helper::GetItemScriptName(uint32 ItemId)
{
    if (ItemTemplate const* Temp = sObjectMgr->GetItemTemplate(ItemId))
        return sObjectMgr->GetScriptName(Temp->ScriptId);

    return "";
}

std::string Helper::GetCreatureNameByEntry(uint32 CreatureId)
{
    const CreatureTemplate* Info = sObjectMgr->GetCreatureTemplate(CreatureId);
    if (!Info)
        return "未知";

    // creature_template.name is the English row. The client shows this string, so
    // prefer the localized row the same way the generated item links do.
    LocaleConstant Index = sWorld->GetDefaultDbcLocale();
    if (CreatureLocale const* Locale = sObjectMgr->GetCreatureLocale(CreatureId))
        if (Index < Locale->Name.size() && !Locale->Name[Index].empty())
            return Locale->Name[Index];

    return Info->Name;
}

std::string Helper::GetCreatureName(Creature* Cr)
{
    const CreatureTemplate* Info = Cr->GetCreatureTemplate();
    if (!Info)
        return "未知";

    return Info->Name;
}

std::string Helper::GetTitleName(uint32 Title)
{
    switch (Title)
    {
    case 1: return "R1[列兵]/R1[斥候]";
    case 2: return "R2[下士]/R2[步兵]";
    case 3: return "R3[中士]/R3[中士]";
    case 4: return "R4[军士长]/R4[高阶军士]";
    case 5: return "R5[士官长]/R5[一等军士长]";
    case 6: return "R6[骑士]/R6[石头守卫]";
    case 7: return "R7[骑士中尉]/R7[血卫士]";
    case 8: return "R8[骑士队长]/R8[军团士兵]";
    case 9: return "R9[护卫骑士]/R9[百夫长]";
    case 10: return "R10[[少校]/R10[勇士]";
    case 11: return "R11[司令]/R11[中将]";
    case 12: return "R12[统帅]/R12[将军]";
    case 13: return "R13[元帅]/R13[督军]";
    case 14: return "R14[大元帅]/R14[高阶督军]";
    }
    return "未知";
}

std::string Helper::GetClassNameById(uint32 Id)
{
    switch (Id)
    {
    case 1: return "战士";
    case 2: return "骑士";
    case 4: return "猎人";
    case 8: return "盗贼";
    case 16: return "牧师";
    case 32: return "死亡骑士";
    case 64: return "萨满";
    case 128: return "法师";
    case 256: return "术士";
    case 1024: return "德鲁伊";
    }
    return "未知";
}

//根据掩码输出所有职业名 用于提示
std::string Helper::PullMaskClassName(uint32 Value)
{
    if (!Value)
        return "无需";
    std::vector<uint32> IdList;
    IdList.clear();
    uint32 Mask[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 1024 };
    for (auto const& T : Mask)
    {
        if (Value & T)
            IdList.push_back(T);
    }
    std::string Oss;
    Oss.clear();
    for (auto const& Id : IdList)
    {
        std::string Name = GetClassNameById(Id);
        Oss.append(Name + "#");
    }
    IdList.clear();
    return Oss;
}

bool Helper::IsClassInMask(uint32 ClassId, uint32 Value)
{
    if (!Value)
        return true;

    return ((1 << (ClassId - 1)) & Value);
}

std::string Helper::GetRaceNameById(uint32 Id)
{
    switch (Id)
    {
    case 1: return "人类";
    case 2: return "兽人";
    case 4: return "矮人";
    case 8: return "暗夜精灵";
    case 16: return "亡灵";
    case 32: return "牛头人";
    case 64: return "侏儒";
    case 128: return "巨魔";
    case 512: return "血精灵";
    case 1024: return "德莱尼";
    }
    return "未知";
}

//根据掩码输出所有种族名 用于提示
std::string Helper::PullMaskRaceName(uint32 Value)
{
    if (!Value)
        return "无需";
    std::vector<uint32> IdList;
    IdList.clear();
    uint32 Mask[] = { 1, 2, 4, 8, 16, 32, 64, 128, 512, 1024 };
    for (auto const& T : Mask)
    {
        if (Value & T)
            IdList.push_back(T);
    }
    std::string Oss;
    Oss.clear();
    for (auto const& Id : IdList)
    {
        std::string Name = GetRaceNameById(Id);
        Oss.append(Name + "#");
    }
    IdList.clear();
    return Oss;
}

bool Helper::IsRaceInMask(uint32 RaceId, uint32 Value)
{
    if (!Value)
        return true;

    return ((1 << (RaceId - 1)) & Value);
}

std::string Helper::GetStatNameById(uint32 Id)
{
    switch (Id)
    {
    case 1: return "力量";
    case 2: return "敏捷";
    case 4: return "耐力";
    case 8: return "智力";
    case 16: return "精神";
    }
    return "未知";
}

std::string Helper::PullMaskStatName(uint32 Value)
{
    if (!Value)
        return "无需";
    std::vector<uint32> IdList;
    IdList.clear();
    uint32 Mask[] = { 1, 2, 4, 8, 16 };
    for (auto const& T : Mask)
    {
        if (Value & T)
            IdList.push_back(T);
    }
    std::string Oss;
    Oss.clear();
    for (auto const& Id : IdList)
    {
        std::string Name = GetStatNameById(Id);
        Oss.append(Name + "#");
    }
    IdList.clear();
    return Oss;
}

Stats Helper::GetStatForId(uint32 Id)
{
    switch (Id)
    {
    case 1: return STAT_STRENGTH;
    case 2: return STAT_AGILITY;
    case 4: return STAT_STAMINA;
    case 8: return STAT_INTELLECT;
    case 16: return STAT_SPIRIT;
    }

    return STAT_STRENGTH;
}

bool Helper::IsStatInMask(Player* player, uint32 IdValue, uint32 CheckValue)
{
    bool In = true;
    std::vector<uint32> IdList;
    IdList.clear();
    uint32 Mask[] = { 1, 2, 4, 8, 16 };
    for (auto const& T : Mask)
    {
        if (IdValue & T)
            IdList.push_back(T);
    }

    for (auto const& Id : IdList)
    {
        Stats St = GetStatForId(Id);
        int32 Val = (int32)player->GetTotalStatValue(St);
        if (Val < (int32)CheckValue)
            In = false;
    }
    IdList.clear();

    return In;
}

bool Helper::IsStatInNow(Unit* player, uint32 CheckValue)
{
    bool In = true;
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        int32 Val = (int32)player->GetTotalStatValue(Stats(i));
        if (Val < (int32)CheckValue)
            In = false;
    }
    return In;
}

//获取图标
std::string Helper::GetItemIcon(uint32 ItemId, uint32 Width, uint32 Height, int X, int Y) const
{
    std::ostringstream Oss;
    Oss << "|TInterface";
    const ItemTemplate* Temp = sObjectMgr->GetItemTemplate(ItemId);
    const ItemDisplayInfoEntry* DispInfo = nullptr;
    if (Temp)
    {
        DispInfo = sItemDisplayInfoStore.LookupEntry(Temp->DisplayInfoID);
        if (DispInfo)
            Oss << "/Icons/" << DispInfo->inventoryIcon;
    }

    if (!DispInfo)
        Oss << "/InventoryItems/WoWUnknownItem01";

    Oss << ":" << Width << ":" << Height << ":" << X << ":" << Y << "|t";
    return Oss.str();
}

//获取物品名字
std::string Helper::GetItemName(uint32 ItemId) const
{
    const ItemTemplate* Temp = sObjectMgr->GetItemTemplate(ItemId);
    return Temp ? Temp->Name1 : "";
}

std::string Helper::GetNItemLink(Item* DoItem) const
{
    if (!DoItem)
        return "";
    const ItemTemplate* Temp = DoItem->GetTemplate();
    if (!Temp)
        return "";

    uint8 Level = (DoItem->GetOwner()) ? DoItem->GetOwner()->GetLevel() : 80;

    std::string Name = Temp->Name1;

    LocaleConstant Index = sWorld->GetDefaultDbcLocale();

    if (ItemLocale const* Locale = sObjectMgr->GetItemLocale(Temp->ItemId))
        ObjectMgr::GetLocaleString(Locale->Name, Index, Name);

    if (int32 ItemRandPropId = DoItem->GetItemRandomPropertyId())
    {
        std::string DoName = "";
        if (ItemRandPropId < 0)
        {
            const ItemRandomSuffixEntry* SuffixEntry = sItemRandomSuffixStore.LookupEntry(-DoItem->GetItemRandomPropertyId());
            if (SuffixEntry)
                DoName = SuffixEntry->Name[LOCALE_zhCN];
        }
        else
        {
            const ItemRandomPropertiesEntry* PropertiesEntry = sItemRandomPropertiesStore.LookupEntry(DoItem->GetItemRandomPropertyId());
            if (PropertiesEntry)
                DoName = PropertiesEntry->Name[LOCALE_zhCN];
        }

        if (!DoName.empty())
            Name += DoName;
    }

    std::ostringstream Oss;
    Oss << "|c" << std::hex << ItemQualityColors[Temp->Quality] << std::dec
        << "|Hitem:" << Temp->ItemId << ":" <<
        DoItem->GetEnchantmentId(PERM_ENCHANTMENT_SLOT) << ":" <<
        DoItem->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT) << ":" <<
        DoItem->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_2) << ":" <<
        DoItem->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_3) << ":" <<
        DoItem->GetEnchantmentId(BONUS_ENCHANTMENT_SLOT) << ":" <<
        DoItem->GetItemRandomPropertyId() << ":" << DoItem->GetItemSuffixFactor() << ":" <<
        Level << "|h[" << Name << "]|h|r";

    return Oss.str();
}

//通过ItemId取Link
std::string Helper::GetItemLink(uint32 ItemId) const
{
    const ItemTemplate* Temp = sObjectMgr->GetItemTemplate(ItemId);
    if (!Temp)
        return " ";

    std::string Name = Temp->Name1;
    LocaleConstant Index = sWorld->GetDefaultDbcLocale();

    if (ItemLocale const* Locale = sObjectMgr->GetItemLocale(Temp->ItemId))
        ObjectMgr::GetLocaleString(Locale->Name, Index, Name);

    std::ostringstream Oss;
    Oss << "|c" << std::hex << ItemQualityColors[Temp->Quality] << std::dec <<
        "|Hitem:" << ItemId << ":0:0:0:0:0:0:0:0:0|h[" << Name << "]|h|r";

    return Oss.str();
}

std::string Helper::GetWorldChatNameLinkColor(Player* player)
{
    if (!player)
        return "";

    std::string Color;
    switch (player->GetClass())
    {
    case CLASS_WARRIOR: Color = "|cffC79C6E"; break;
    case CLASS_PALADIN: Color = "|cffF58CBA"; break;
    case CLASS_HUNTER: Color = "|cffABD473"; break;
    case CLASS_ROGUE: Color = "|cffFFF569"; break;
    case CLASS_PRIEST: Color = "|cffFFFFFF"; break;
    case CLASS_DEATH_KNIGHT: Color = "|cffC41F3B"; break;
    case CLASS_SHAMAN: Color = "|cff2459FF"; break;
    case CLASS_MAGE: Color = "|cff69CCF0"; break;
    case CLASS_WARLOCK: Color = "|cff9482C9"; break;
    case CLASS_DRUID: Color = "|cffFF7D0A"; break;
    default: break;
    }

    std::string PlayerName = player->GetName();
    std::string Prefix = sPlayerModMgr->GetVipPixStr(player->VipRank);
    if (!Prefix.empty())
        PlayerName = Prefix + "-" + PlayerName;

    if (GetBoolean(13000))
        return "|Hplayer:" + PlayerName + "|h[" + PlayerName + "]|h";

    return "|Hplayer:" + PlayerName + "|h[" + Color + PlayerName + "|r]|h";
}

//按职业显示对应玩家名字颜色Link
std::string Helper::GetNameLinkColor(Player* player)
{
    if (!player)
        return "";

    std::string Color;
    switch (player->GetClass())
    {
    case CLASS_WARRIOR: Color = "|cffC79C6E"; break;
    case CLASS_PALADIN: Color = "|cffF58CBA"; break;
    case CLASS_HUNTER: Color = "|cffABD473"; break;
    case CLASS_ROGUE: Color = "|cffFFF569"; break;
    case CLASS_PRIEST: Color = "|cffFFFFFF"; break;
    case CLASS_DEATH_KNIGHT: Color = "|cffC41F3B"; break;
    case CLASS_SHAMAN: Color = "|cff2459FF"; break;
    case CLASS_MAGE: Color = "|cff69CCF0"; break;
    case CLASS_WARLOCK: Color = "|cff9482C9"; break;
    case CLASS_DRUID: Color = "|cffFF7D0A"; break;
    default: break;
    }

    if (GetBoolean(13000))
        return "|Hplayer:" + player->GetName() + "|h[" + player->GetName() + "]|h";

    return "|Hplayer:" + player->GetName() + "|h[" + Color + player->GetName() + "|r]|h";
}

std::string Helper::GetNameLinkColor(std::string PlayerName, uint8 Class)
{
    std::string Color;
    switch (Class)
    {
    case CLASS_WARRIOR: Color = "|cffC79C6E"; break;
    case CLASS_PALADIN: Color = "|cffF58CBA"; break;
    case CLASS_HUNTER: Color = "|cffABD473"; break;
    case CLASS_ROGUE: Color = "|cffFFF569"; break;
    case CLASS_PRIEST: Color = "|cffFFFFFF"; break;
    case CLASS_DEATH_KNIGHT: Color = "|cffC41F3B"; break;
    case CLASS_SHAMAN: Color = "|cff2459FF"; break;
    case CLASS_MAGE: Color = "|cff69CCF0"; break;
    case CLASS_WARLOCK: Color = "|cff9482C9"; break;
    case CLASS_DRUID: Color = "|cffFF7D0A"; break;
    default: break;
    }

    if (GetBoolean(13000))
        return "|Hplayer:" + PlayerName + "|h[" + PlayerName + "]|h";

    return "|Hplayer:" + PlayerName + "|h[" + Color + PlayerName + "|r]|h";
}

std::string Helper::GetVipPlayerChatInfo(Player* player, uint32 IconSize)
{
    std::string VipIcon = sPlayerModMgr->GetVipIconStr(player->GetVipRank());
    VipIcon = VipIcon == "" ? VipIcon : VipIcon + " ";
    return VipIcon + GetNameLinkColor(player);
}

std::string Helper::GetWorldChatPlayerChatInfo(Player* player, uint32 IconSize)
{
    std::string ClassIcon, RaceIcon;

    switch (player->GetClass())
    {
    case CLASS_WARRIOR:
        ClassIcon = "|TInterface/icons/INV_Sword_27.png:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_PALADIN:
        ClassIcon = "|TInterface/icons/INV_Hammer_01:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_HUNTER:
        ClassIcon = "|TInterface/icons/INV_Weapon_Bow_07:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_ROGUE:
        ClassIcon = "|TInterface/icons/INV_ThrowingKnife_04:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_PRIEST:
        ClassIcon = "|TInterface/icons/INV_Staff_30:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_DEATH_KNIGHT:
        ClassIcon = "|TInterface/icons/Spell_Deathknight_ClassIcon:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_SHAMAN:
        ClassIcon = "|TInterface/icons/Spell_Nature_BloodLust:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_MAGE:
        ClassIcon = "|TInterface/icons/INV_Staff_13:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_WARLOCK:
        ClassIcon = "|TInterface/icons/Spell_Nature_FaerieFire:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_DRUID:
        ClassIcon = "|TInterface/icons/Ability_Druid_Maul:" + std::to_string(IconSize) + "|t";
        break;
    }

    switch (player->GetRace())
    {
    case RACE_HUMAN:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Human_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Human_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_ORC:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Orc_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Orc_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_DWARF:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Dwarf_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Dwarf_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_NIGHTELF:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Nightelf_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Nightelf_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_UNDEAD_PLAYER:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Undead_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Undead_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_TAUREN:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Tauren_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Tauren_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_GNOME:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Gnome_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Gnome_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_TROLL:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Troll_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Troll_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_BLOODELF:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Bloodelf_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Bloodelf_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_DRAENEI:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Draenei_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Draenei_Female:" + std::to_string(IconSize) + "|t";
        break;
    }

    return ClassIcon + " " + RaceIcon + " " + GetWorldChatNameLinkColor(player);
}

//获取玩家 种族 职业 图标
std::string Helper::GetPlayerChatInfo(Player* player, uint32 IconSize)
{
    std::string ClassIcon, RaceIcon;

    switch (player->GetClass())
    {
    case CLASS_WARRIOR:
        ClassIcon = "|TInterface/icons/INV_Sword_27.png:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_PALADIN:
        ClassIcon = "|TInterface/icons/INV_Hammer_01:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_HUNTER:
        ClassIcon = "|TInterface/icons/INV_Weapon_Bow_07:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_ROGUE:
        ClassIcon = "|TInterface/icons/INV_ThrowingKnife_04:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_PRIEST:
        ClassIcon = "|TInterface/icons/INV_Staff_30:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_DEATH_KNIGHT:
        ClassIcon = "|TInterface/icons/Spell_Deathknight_ClassIcon:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_SHAMAN:
        ClassIcon = "|TInterface/icons/Spell_Nature_BloodLust:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_MAGE:
        ClassIcon = "|TInterface/icons/INV_Staff_13:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_WARLOCK:
        ClassIcon = "|TInterface/icons/Spell_Nature_FaerieFire:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_DRUID:
        ClassIcon = "|TInterface/icons/Ability_Druid_Maul:" + std::to_string(IconSize) + "|t";
        break;
    }

    switch (player->GetRace())
    {
    case RACE_HUMAN:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Human_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Human_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_ORC:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Orc_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Orc_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_DWARF:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Dwarf_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Dwarf_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_NIGHTELF:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Nightelf_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Nightelf_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_UNDEAD_PLAYER:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Undead_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Undead_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_TAUREN:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Tauren_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Tauren_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_GNOME:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Gnome_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Gnome_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_TROLL:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Troll_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Troll_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_BLOODELF:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Bloodelf_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Bloodelf_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_DRAENEI:
        if (player->GetGender() == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Draenei_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Draenei_Female:" + std::to_string(IconSize) + "|t";
        break;
    }

    return ClassIcon + " " + RaceIcon + " " + GetNameLinkColor(player);
}

std::string Helper::GetPlayerChatInfo(std::string PlayerName, uint8 Class, uint8 Race, uint8 Gender, uint32 IconSize)
{
    std::string NameColor, ClassIcon, RaceIcon;

    switch (Class)
    {
    case CLASS_WARRIOR:
        NameColor = "|cffC79C6E" + PlayerName + "|r";
        ClassIcon = "|TInterface/icons/INV_Sword_27.png:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_PALADIN:
        NameColor = "|cffF58CBA" + PlayerName + "|r";
        ClassIcon = "|TInterface/icons/INV_Hammer_01:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_HUNTER:
        NameColor = "|cffABD473" + PlayerName + "|r";
        ClassIcon = "|TInterface/icons/INV_Weapon_Bow_07:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_ROGUE:
        NameColor = "|cffFFF569" + PlayerName + "|r";
        ClassIcon = "|TInterface/icons/INV_ThrowingKnife_04:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_PRIEST:
        NameColor = "|cffFFFFFF" + PlayerName + "|r";
        ClassIcon = "|TInterface/icons/INV_Staff_30:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_DEATH_KNIGHT:
        NameColor = "|cffC41F3B" + PlayerName + "|r";
        ClassIcon = "|TInterface/icons/Spell_Deathknight_ClassIcon:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_SHAMAN:
        NameColor = "|cff2459FF" + PlayerName + "|r";
        ClassIcon = "|TInterface/icons/Spell_Nature_BloodLust:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_MAGE:
        NameColor = "|cff69CCF0" + PlayerName + "|r";
        ClassIcon = "|TInterface/icons/INV_Staff_13:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_WARLOCK:
        NameColor = "|cff9482C9" + PlayerName + "|r";
        ClassIcon = "|TInterface/icons/Spell_Nature_FaerieFire:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_DRUID:
        NameColor = "|cffFF7D0A" + PlayerName + "|r";
        ClassIcon = "|TInterface/icons/Ability_Druid_Maul:" + std::to_string(IconSize) + "|t";
        break;
    }

    switch (Race)
    {
    case RACE_HUMAN:
        if (Gender == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Human_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Human_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_ORC:
        if (Gender == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Orc_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Orc_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_DWARF:
        if (Gender == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Dwarf_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Dwarf_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_NIGHTELF:
        if (Gender == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Nightelf_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Nightelf_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_UNDEAD_PLAYER:
        if (Gender == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Undead_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Undead_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_TAUREN:
        if (Gender == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Tauren_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Tauren_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_GNOME:
        if (Gender == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Gnome_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Gnome_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_TROLL:
        if (Gender == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Troll_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Troll_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_BLOODELF:
        if (Gender == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Bloodelf_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Bloodelf_Female:" + std::to_string(IconSize) + "|t";
        break;
    case RACE_DRAENEI:
        if (Gender == GENDER_MALE)
            RaceIcon = "|TInterface/Icons/Achievement_Character_Draenei_Male:" + std::to_string(IconSize) + "|t";
        else
            RaceIcon = "|TInterface/Icons/Achievement_Character_Draenei_Female:" + std::to_string(IconSize) + "|t";
        break;
    }

    return ClassIcon + " " + RaceIcon + " " + NameColor;
}

std::string Helper::GetPlayerChatInfo(std::string PlayerName, uint8 Class, uint32 IconSize)
{
    std::string ClassIcon = "";

    switch (Class)
    {
    case CLASS_WARRIOR:
        ClassIcon = "|TInterface/icons/INV_Sword_27.png:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_PALADIN:
        ClassIcon = "|TInterface/icons/INV_Hammer_01:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_HUNTER:
        ClassIcon = "|TInterface/icons/INV_Weapon_Bow_07:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_ROGUE:
        ClassIcon = "|TInterface/icons/INV_ThrowingKnife_04:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_PRIEST:
        ClassIcon = "|TInterface/icons/INV_Staff_30:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_DEATH_KNIGHT:
        ClassIcon = "|TInterface/icons/Spell_Deathknight_ClassIcon:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_SHAMAN:
        ClassIcon = "|TInterface/icons/Spell_Nature_BloodLust:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_MAGE:
        ClassIcon = "|TInterface/icons/INV_Staff_13:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_WARLOCK:
        ClassIcon = "|TInterface/icons/Spell_Nature_FaerieFire:" + std::to_string(IconSize) + "|t";
        break;
    case CLASS_DRUID:
        ClassIcon = "|TInterface/icons/Ability_Druid_Maul:" + std::to_string(IconSize) + "|t";
        break;
    }

    return ClassIcon + " " + PlayerName;
}

//发送邮件 玩家名字
void Helper::SendMailToName(std::string PlayerName, std::string Sub, std::string Body, uint32 ItemId, uint32 Count, uint32 Money)
{
    ObjectGuid Guid = sCharacterCache->GetCharacterGuidByName(PlayerName);
    if (Guid == ObjectGuid::Empty)
        return;

    ObjectGuid::LowType GuidLow = Guid.ToGuidLow();

    ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(ItemId);
    if (!Proto)
        return;

    if (Count < 1 || (Proto->MaxCount > 0 && Count > uint32(Proto->MaxCount)))
        return;

    typedef std::pair<uint32, uint32> ItemPair;
    typedef std::list<ItemPair> ItemPairs;
    ItemPairs MailItems;

    while (Count > Proto->GetMaxStackSize())
    {
        MailItems.push_back(ItemPair(ItemId, Proto->GetMaxStackSize()));
        Count -= Proto->GetMaxStackSize();
    }

    MailItems.push_back(ItemPair(ItemId, Count));

    if (MailItems.size() > MAX_MAIL_ITEMS)
        return;

    MailSender Sender(MAIL_NORMAL, 0, MAIL_STATIONERY_AUCTION);

    MailDraft Draft(Sub, Body);
    CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();

    if (Money > 0)
        Draft.AddMoney(Money);

    for (auto Itr = MailItems.begin(); Itr != MailItems.end(); ++Itr)
    {
        if (Item* DoItem = Item::CreateItem(Itr->first, Itr->second))
        {
            DoItem->SaveToDB(Trans);
            Draft.AddItem(DoItem);
        }
    }

    Draft.SendMailTo(Trans, MailReceiver(GuidLow), Sender);
    CharacterDatabase.CommitTransaction(Trans);

    if (Player* player = ObjectAccessor::FindPlayerByName(PlayerName))
        player->SendSysMsg(GetGts(1006));
}

//发送邮件 玩家Guid
void Helper::SendMailToGuid(ObjectGuid Guid, std::string Sub, std::string Body, uint32 ItemId, uint32 Count, uint32 Money)
{
    if (!sCharacterCache->HasCharacterCacheEntry(Guid))
        return;

    ObjectGuid::LowType GuidLow = Guid.GetCounter();

    ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(ItemId);
    if (!Proto)
        return;

    if (Count < 1 || (Proto->MaxCount > 0 && Count > uint32(Proto->MaxCount)))
        return;

    typedef std::pair<uint32, uint32> ItemPair;
    typedef std::list<ItemPair> ItemPairs;
    ItemPairs MailItems;

    while (Count > Proto->GetMaxStackSize())
    {
        MailItems.push_back(ItemPair(ItemId, Proto->GetMaxStackSize()));
        Count -= Proto->GetMaxStackSize();
    }

    MailItems.push_back(ItemPair(ItemId, Count));

    if (MailItems.size() > MAX_MAIL_ITEMS)
        return;

    MailSender Sender(MAIL_NORMAL, 0, MAIL_STATIONERY_AUCTION);

    MailDraft Draft(Sub, Body);
    CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();

    if (Money > 0)
        Draft.AddMoney(Money);

    for (auto Itr = MailItems.begin(); Itr != MailItems.end(); ++Itr)
    {
        if (Item* DoItem = Item::CreateItem(Itr->first, Itr->second))
        {
            DoItem->SaveToDB(Trans);
            Draft.AddItem(DoItem);
        }
    }

    Draft.SendMailTo(Trans, MailReceiver(GuidLow), Sender);
    CharacterDatabase.CommitTransaction(Trans);

    if (Player* player = ObjectAccessor::FindPlayer(Guid))
        player->SendSysMsg(GetGts(1006));
}

//发送邮件来自AH--积分空邮件 仅作明细
void Helper::SendMailFromHouseToken(ObjectGuid Guid, ObjectGuid BidderGuid, int64 Count)
{
    if (!sCharacterCache->HasCharacterCacheEntry(Guid))
        return;


    float Ptn = sHelper->GetFloat(1700);
    Ptn = Ptn < 1.0f ? Ptn : 0.0f;
    float NCount = Count * Ptn;
    int64 ZCount = (int64)(NCount * 10.0f + 5.0f) / 10.0f;
    int64 KCount = Count - ZCount;

    sPlayerModMgr->AddTokenAH(Guid, KCount);

    ObjectGuid::LowType GuidLow = Guid.GetCounter();
    std::string Sub = "物品已出售";

    std::ostringstream Oss;
    Oss << "\n明细如下:\n";
    Oss << "竞拍人: " << sCharacterCache->GetCharacterNameForGuid(BidderGuid) << "\n";
    Oss << "一口价: " << " 积分 X " << Count << "\n";
    Oss << "手续费: " << ToExString(Ptn) << " 扣除: " << ZCount << "\n";
    Oss << "积分已到账 此邮件仅作明细\n";
    std::string Body = Oss.str();

    MailSender Sender(uint32(GetUInt(1007)));

    MailDraft Draft(Sub, Body);
    CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();

    Draft.SendMailTo(Trans, MailReceiver(GuidLow), Sender);
    CharacterDatabase.CommitTransaction(Trans);

    if (Player* player = ObjectAccessor::FindPlayer(Guid))
        player->SendSysMsg(GetGts(1006));
}

void Helper::SendMailFromHouseMB(ObjectGuid Guid, ObjectGuid BidderGuid, int64 Count)
{
    if (!sCharacterCache->HasCharacterCacheEntry(Guid))
        return;

    float Ptn = sHelper->GetFloat(1700);
    Ptn = Ptn < 1.0f ? Ptn : 0.0f;
    float NCount = Count * Ptn;
    int64 ZCount = (int64)(NCount * 10.0f + 5.0f) / 10.0f;
    int64 KCount = Count - ZCount;

    uint32 AccountId = sCharacterCache->GetCharacterAccountIdByGuid(Guid);
    if (AccountId)
    {
        uint64 OldMB = 0;
        LoginDatabasePreparedStatement* SelStmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_VIP);
        SelStmt->SetData(0, AccountId);
        if (PreparedQueryResult Result = LoginDatabase.Query(SelStmt))
            OldMB = (*Result)[3].Get<uint64>();

        int64 NowMB = int64(OldMB) + KCount;
        NowMB = NowMB > 0 ? NowMB : 0;

        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN_MB);
        Utmt->SetData(0, NowMB);
        Utmt->SetData(1, AccountId);
        LoginDatabase.Execute(Utmt);

        if (Player* player = ObjectAccessor::FindPlayer(Guid))
            player->SendSysMsg("[温馨提示]: 您增加了 |cFFFF6699%lld|r 魔币 当前魔币: |cFFFF6699%lld|r", KCount, NowMB);
    }

    ObjectGuid::LowType GuidLow = Guid.GetCounter();
    std::string Sub = "物品已出售";

    std::ostringstream Oss;
    Oss << "\n明细如下:\n";
    Oss << "竞拍人: " << sCharacterCache->GetCharacterNameForGuid(BidderGuid) << "\n";
    Oss << "一口价: " << " 魔币 X " << Count << "\n";
    Oss << "手续费: " << ToExString(Ptn) << " 扣除: " << ZCount << "\n";
    Oss << "魔币已到账 此邮件仅作明细\n";
    std::string Body = Oss.str();

    MailSender Sender(uint32(GetUInt(1007)));

    MailDraft Draft(Sub, Body);
    CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();

    Draft.SendMailTo(Trans, MailReceiver(GuidLow), Sender);
    CharacterDatabase.CommitTransaction(Trans);

    if (Player* player = ObjectAccessor::FindPlayer(Guid))
        player->SendSysMsg(GetGts(1006));
}

void Helper::SendMailFromHousePaodian(ObjectGuid Guid, ObjectGuid BidderGuid, int64 Count)
{
    if (!sCharacterCache->HasCharacterCacheEntry(Guid))
        return;

    float Ptn = sHelper->GetFloat(1700);
    Ptn = Ptn < 1.0f ? Ptn : 0.0f;
    float NCount = Count * Ptn;
    int64 ZCount = (int64)(NCount * 10.0f + 5.0f) / 10.0f;
    int64 KCount = Count - ZCount;

    uint32 AccountId = sCharacterCache->GetCharacterAccountIdByGuid(Guid);
    if (AccountId)
    {
        uint64 OldPaodian = 0;
        LoginDatabasePreparedStatement* SelStmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_PAODIAN);
        SelStmt->SetData(0, AccountId);
        if (PreparedQueryResult Result = LoginDatabase.Query(SelStmt))
            OldPaodian = (*Result)[0].Get<uint64>();

        int64 NowPaodian = int64(OldPaodian) + KCount;
        NowPaodian = NowPaodian > 0 ? NowPaodian : 0;

        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_PAODIAN);
        Utmt->SetData(0, KCount);
        Utmt->SetData(1, AccountId);
        LoginDatabase.Execute(Utmt);

        if (Player* player = ObjectAccessor::FindPlayer(Guid))
            player->SendSysMsg("[温馨提示]: 您增加了 |cFFFF6699%lld|r 泡点 当前泡点: |cFFFF6699%lld|r", KCount, NowPaodian);
    }

    ObjectGuid::LowType GuidLow = Guid.GetCounter();
    std::string Sub = "物品已出售";

    std::ostringstream Oss;
    Oss << "\n明细如下:\n";
    Oss << "竞拍人: " << sCharacterCache->GetCharacterNameForGuid(BidderGuid) << "\n";
    Oss << "一口价: " << " 泡点 X " << Count << "\n";
    Oss << "手续费: " << ToExString(Ptn) << " 扣除: " << ZCount << "\n";
    Oss << "泡点已到账 此邮件仅作明细\n";
    std::string Body = Oss.str();

    MailSender Sender(uint32(GetUInt(1007)));

    MailDraft Draft(Sub, Body);
    CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();

    Draft.SendMailTo(Trans, MailReceiver(GuidLow), Sender);
    CharacterDatabase.CommitTransaction(Trans);

    if (Player* player = ObjectAccessor::FindPlayer(Guid))
        player->SendSysMsg(GetGts(1006));
}

void Helper::SendWonAHItemBidder(ObjectGuid BidderGuid, ObjectGuid OwnerGuid, Item* pItem, uint32 Count)
{
    if (!sCharacterCache->HasCharacterCacheEntry(BidderGuid))
        return;

    int64 Try = sHelper->GetInt(1005);
    uint32 TokenItem = GetUInt(1018);

    ObjectGuid::LowType GuidLow = BidderGuid.GetCounter();

    MailSender Sender(uint32(GetUInt(1007)));
    std::string Sub = "竞拍获胜";

    std::ostringstream Oss;
    Oss << "\n明细如下:\n";
    Oss << "发布者: " << sCharacterCache->GetCharacterNameForGuid(OwnerGuid) << "\n";
    switch (Try)
    {
    case 1:
        Oss << "一口价: 消耗 " << "积分 X " << Count << "\n";
        break;
    case 2:
        Oss << "一口价: 消耗 " << GetItemName(TokenItem) << " X " << Count << "\n";
        break;
    case 3:
        Oss << "一口价: 消耗 " << "魔币 X " << Count << "\n";
        break;
    case 4:
        Oss << "一口价: 消耗 " << "泡点 X " << Count << "\n";
        break;
    }

    std::string Body = Oss.str();
    MailDraft Draft(Sub, Body);
    CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();

    if (pItem != NULL)
        Draft.AddItem(pItem);

    Draft.SendMailTo(Trans, MailReceiver(GuidLow), Sender);
    CharacterDatabase.CommitTransaction(Trans);

    if (Player* player = ObjectAccessor::FindPlayer(BidderGuid))
        player->SendSysMsg(GetGts(1006));
}

void Helper::SendExpiredAH(ObjectGuid OwnerGuid, Item* pItem)
{
    if (!sCharacterCache->HasCharacterCacheEntry(OwnerGuid))
        return;

    ObjectGuid::LowType GuidLow = OwnerGuid.GetCounter();

    MailSender Sender(uint32(GetUInt(1007)));

    std::string Sub = "拍卖到期";
    std::string Body = "拍卖已到期 请重新拍卖";
    MailDraft Draft(Sub, Body);
    CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();

    if (pItem != nullptr)
        Draft.AddItem(pItem);

    Draft.SendMailTo(Trans, MailReceiver(GuidLow), Sender);
    CharacterDatabase.CommitTransaction(Trans);

    if (Player* player = ObjectAccessor::FindPlayer(OwnerGuid))
        player->SendSysMsg(GetGts(1006));
}

void Helper::SendCancelledAH(ObjectGuid OwnerGuid, Item* pItem)
{
    if (!sCharacterCache->HasCharacterCacheEntry(OwnerGuid))
        return;

    ObjectGuid::LowType GuidLow = OwnerGuid.GetCounter();

    MailSender Sender(uint32(GetUInt(1007)));

    std::string Sub = "取消拍卖";
    std::string Body = "自主取消了拍卖 请重新拍卖";
    MailDraft Draft(Sub, Body);
    CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();

    if (pItem != nullptr)
        Draft.AddItem(pItem);

    Draft.SendMailTo(Trans, MailReceiver(GuidLow), Sender);
    CharacterDatabase.CommitTransaction(Trans);

    if (Player* player = ObjectAccessor::FindPlayer(OwnerGuid))
        player->SendSysMsg(GetGts(1006));
}

void Helper::AuctionHouseCustomMsg(Player* player, AuctionEntry* AH, Item* pItem, bool OnSell)
{
    int64 Try = GetInt(1005);
    if (!Try)
        return;

    if (!GetBoolean(1006))
        return;

    std::string SItemLink = GetNItemLink(pItem);

    uint32 TokenItem = GetUInt(1018);
    if (OnSell)
    {
        switch (Try)
        {
        case 1:
            SendSerMessage(GetGts(1044), GetNameLinkColor(player).c_str(), SItemLink.c_str(), AH->itemCount, AH->buyout);
            break;
        case 2:
            SendSerMessage(GetGts(2701), GetNameLinkColor(player).c_str(), SItemLink.c_str(), AH->itemCount, GetItemLink(TokenItem).c_str(), AH->buyout);
            break;
        case 3:
            SendSerMessage(GetGts(2703), GetNameLinkColor(player).c_str(), AH->itemCount, SItemLink.c_str(), AH->buyout);
            break;
        case 4:
            SendSerMessage(GetGts(2704), GetNameLinkColor(player).c_str(), AH->itemCount, SItemLink.c_str(), AH->buyout);
            break;
        }
    }
    else
    {
        switch (Try)
        {
        case 1:
            SendSerMessage(GetGts(1068), GetNameLinkColor(player).c_str(), AH->buyout, SItemLink.c_str(), AH->itemCount);
            break;
        case 2:
            SendSerMessage(GetGts(2702), GetNameLinkColor(player).c_str(), GetItemLink(TokenItem).c_str(), AH->buyout, SItemLink.c_str(), AH->itemCount);
            break;
        case 3:
            SendSerMessage(GetGts(2705), GetNameLinkColor(player).c_str(), AH->buyout, SItemLink.c_str(), AH->itemCount);
            break;
        case 4:
            SendSerMessage(GetGts(2706), GetNameLinkColor(player).c_str(), AH->buyout, SItemLink.c_str(), AH->itemCount);
            break;
        }
    }
}

void Helper::SendItemHouseMailToGuid(ObjectGuid Guid, ObjectGuid BidderGuid, uint32 Count)
{
    if (!sCharacterCache->HasCharacterCacheEntry(Guid))
        return;

    if (!Count)
        return;

    float Ptn = sHelper->GetFloat(1700);
    Ptn = Ptn < 1.0f ? Ptn : 0.0f;
    float NCount = Count * Ptn;
    uint32 ZCount = (uint32)(NCount * 10.0f + 5.0f) / 10.0f;
    uint32 KCount = Count - ZCount;

    if (NCount < 1.0f)
    {
        ZCount = 0;
        KCount = Count;
    }

    if (KCount < 1)
        return;

    ObjectGuid::LowType GuidLow = Guid.GetCounter();

    uint32 TokenItem = sHelper->GetUInt(1018);
    ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(TokenItem);
    if (!Proto)
        return;

    if (Proto->MaxCount > 0 && KCount > uint32(Proto->MaxCount))
        return;

    typedef std::pair<uint32, uint32> ItemPair;
    typedef std::list<ItemPair> ItemPairs;
    ItemPairs MailItems;

    while (KCount > Proto->GetMaxStackSize())
    {
        MailItems.push_back(ItemPair(TokenItem, Proto->GetMaxStackSize()));
        KCount -= Proto->GetMaxStackSize();
    }

    MailItems.push_back(ItemPair(TokenItem, KCount));

    if (MailItems.size() > MAX_MAIL_ITEMS)
        return;

    std::string Sub = "物品已出售";

    std::ostringstream Oss;
    Oss << "\n明细如下:\n";
    Oss << "竞拍人: " << sCharacterCache->GetCharacterNameForGuid(BidderGuid) << "\n";
    Oss << "一口价: " << GetItemName(TokenItem) << " X " << Count << "\n";
    Oss << "手续费: " << ToExString(Ptn) << " 扣除: " << ZCount << "\n";
    std::string Body = Oss.str();

    MailSender Sender(uint32(GetUInt(1007)));

    MailDraft Draft(Sub, Body);

    CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();

    for (auto Itr = MailItems.begin(); Itr != MailItems.end(); ++Itr)
    {
        if (Item* DoItem = Item::CreateItem(Itr->first, Itr->second))
        {
            DoItem->SaveToDB(Trans);
            Draft.AddItem(DoItem);
        }
    }

    Draft.SendMailTo(Trans, MailReceiver(GuidLow), Sender);
    CharacterDatabase.CommitTransaction(Trans);

    if (Player* player = ObjectAccessor::FindPlayer(Guid))
        player->SendSysMsg(GetGts(1006));
}

//GM频道发言 仅GM看到
void Helper::SendGMMessage(const char* Format, ...)
{
    if (!Format || !*Format)
        return;

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (auto Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        if (AccountMgr::IsPlayerAccount(Itr->second->GetSecurity()))
            continue;

        va_list Ap;
        char Buff[1024];
        Buff[0] = '\0';
        va_start(Ap, Format);
        vsnprintf(Buff, 1024, Format, Ap);
        va_end(Ap);
        ChatHandler(Itr->second->GetPlayer()->GetSession()).SendSysMessage(Buff);
    }
}

//系统频道新函数1 仅聊天窗口
void Helper::SendSerMessage(const char* Format, ...)
{
    if (!Format || !*Format)
        return;


    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (auto Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        va_list Ap;
        char Buff[1024];
        Buff[0] = '\0';
        va_start(Ap, Format);
        vsnprintf(Buff, 1024, Format, Ap);
        va_end(Ap);
        ChatHandler(Itr->second->GetPlayer()->GetSession()).SendSysMessage(Buff);
    }
}

//系统频道新函数2 仅屏幕上方
void Helper::SendSerNewMessage(const char* Format, ...)
{
    if (!Format || !*Format)
        return;

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (auto Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        va_list Ap;
        char Buff[1024];
        Buff[0] = '\0';
        va_start(Ap, Format);
        vsnprintf(Buff, 1024, Format, Ap);
        va_end(Ap);
        Itr->second->GetPlayer()->GetSession()->SendAreaTriggerMessage(Buff);
    }
}

//系统频道新函数1+2 聊天窗口+屏幕上方
void Helper::SendSerAllMessage(const char* Format, ...)
{
    if (!Format || !*Format)
        return;

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (auto Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        va_list Ap;
        char Buff[1024];
        Buff[0] = '\0';
        va_start(Ap, Format);
        vsnprintf(Buff, 1024, Format, Ap);
        va_end(Ap);
        ChatHandler(Itr->second->GetPlayer()->GetSession()).SendSysMessage(Buff);
        Itr->second->GetPlayer()->GetSession()->SendAreaTriggerMessage(Buff);
    }
}

//系统频道新函数特效
void Helper::SendSerSpecialMessage(const char* Format, ...)
{
    if (!Format || !*Format)
        return;

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (auto Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        va_list Ap;
        char Buff[1024];
        Buff[0] = '\0';
        va_start(Ap, Format);
        vsnprintf(Buff, 1024, Format, Ap);
        va_end(Ap);

        std::map<uint32, WorldPacket> Packets;
        if (Packets.find(Itr->second->GetPlayer()->GetSession()->GetSessionDbLocaleIndex()) == Packets.end())
            ChatHandler::BuildChatPacket(Packets[Itr->second->GetPlayer()->GetSession()->GetSessionDbLocaleIndex()], CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, nullptr, nullptr, Buff);

        Itr->second->GetPlayer()->SendDirectMessage(&Packets[Itr->second->GetPlayer()->GetSession()->GetSessionDbLocaleIndex()]);
    }
}

void Helper::SendTeamMessage(Player* player, const char* Format, ...)
{
    if (!Format || !*Format)
        return;

    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (auto Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld() || player->GetTeamId() != Itr->second->GetPlayer()->GetTeamId())
            continue;

        va_list Ap;
        char Buff[1024];
        Buff[0] = '\0';
        va_start(Ap, Format);
        vsnprintf(Buff, 1024, Format, Ap);
        va_end(Ap);
        ChatHandler(Itr->second->GetPlayer()->GetSession()).SendSysMessage(Buff);
    }
}

void Helper::SendScreenMessage(const char* Text, Player* player, bool GM, TeamId Id)
{
    if (!Text || !*Text)
        return;

    WorldPacket Data;
    ChatHandler::BuildChatPacket(Data, CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, nullptr, player, Text);

    if (player && player->GetSession())
        player->GetSession()->SendPacket(&Data);
    else if (GM)
        sWorld->SendGlobalGMMessage(&Data, nullptr, Id);
    else
        sWorld->SendGlobalMessage(&Data, nullptr, Id);
}

void Helper::SendAreaRangeMessage(uint32 AreaId, const char* Format, ...)
{
    if (!Format || !*Format)
        return;
    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (auto Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        if (Itr->second->GetPlayer()->GetAreaId() == AreaId)
        {
            va_list Ap;
            char Buff[1024];
            Buff[0] = '\0';
            va_start(Ap, Format);
            vsnprintf(Buff, 1024, Format, Ap);
            va_end(Ap);

            std::map<uint32, WorldPacket> Packets;
            if (Packets.find(Itr->second->GetPlayer()->GetSession()->GetSessionDbLocaleIndex()) == Packets.end())
                ChatHandler::BuildChatPacket(Packets[Itr->second->GetPlayer()->GetSession()->GetSessionDbLocaleIndex()], CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, nullptr, nullptr, Buff);

            Itr->second->GetPlayer()->SendDirectMessage(&Packets[Itr->second->GetPlayer()->GetSession()->GetSessionDbLocaleIndex()]);
        }
    }
}

void Player::SendSysMsg(const char* Text, ...)
{
    if (Text)
    {
        va_list Ap;
        char Buff[1024];
        Buff[0] = '\0';
        va_start(Ap, Text);
        vsnprintf(Buff, 1024, Text, Ap);
        va_end(Ap);

        ChatHandler(GetSession()).SendSysMessage(Buff);
    }
}

void Player::SendATMsg(const char* Text, ...)
{
    if (Text)
    {
        va_list Ap;
        char Buff[1024];
        Buff[0] = '\0';
        va_start(Ap, Text);
        vsnprintf(Buff, 1024, Text, Ap);
        va_end(Ap);

        GetSession()->SendAreaTriggerMessage(Buff);
    }
}

void Player::SendAsMsg(const char* Text, ...)
{
    if (Text)
    {
        va_list Ap;
        char Buff[1024];
        Buff[0] = '\0';
        va_start(Ap, Text);
        vsnprintf(Buff, 1024, Text, Ap);
        va_end(Ap);

        ChatHandler(GetSession()).SendSysMessage(Buff);
        GetSession()->SendAreaTriggerMessage(Buff);
    }
}

void Player::SendSpecialMsg(const char* Text, ...)
{
    if (Text)
    {
        va_list Ap;
        char Buff[1024];
        Buff[0] = '\0';
        va_start(Ap, Text);
        vsnprintf(Buff, 1024, Text, Ap);
        va_end(Ap);

        std::map<uint32, WorldPacket> Packets;
        if (Packets.find(GetSession()->GetSessionDbLocaleIndex()) == Packets.end())
            ChatHandler::BuildChatPacket(Packets[GetSession()->GetSessionDbLocaleIndex()], CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, nullptr, nullptr, Buff);

        SendDirectMessage(&Packets[GetSession()->GetSessionDbLocaleIndex()]);
    }
}

void Player::AsSetFactionForRace(uint8 Race)
{
    SetFactionForRace(GetRace(true));
}

const char* Helper::GetConfig(uint32 Index)
{
    auto Itr = GlobalConfigStore.find(Index);
    return Itr != GlobalConfigStore.end() ? Itr->second.c_str() : "";
}

std::string Helper::GetStrConfig(uint32 Index)
{
    auto Itr = GlobalConfigStore.find(Index);
    return Itr != GlobalConfigStore.end() ? Itr->second : "";
}

int64 Helper::GetInt(uint32 Index)
{
    auto Itr = GlobalConfigStore.find(Index);
    if (Itr != GlobalConfigStore.end())
        return _atoi64(Itr->second.c_str());

    return -1;
}

uint64 Helper::GetUInt(uint32 Index)
{
    auto Itr = GlobalConfigStore.find(Index);
    if (Itr != GlobalConfigStore.end())
        return uint64(_atoi64(Itr->second.c_str()));

    if (Index == 1502)
        return 1;

    return 0;
}

uint32 Helper::GetUInt32(uint32 Index)
{
    auto Itr = GlobalConfigStore.find(Index);
    if (Itr != GlobalConfigStore.end())
        return uint32(atoi(Itr->second.c_str()));

    if (Index == 1502)
        return 1;

    return 0;
}

int32 Helper::GetInt32(uint32 Index)
{
    auto Itr = GlobalConfigStore.find(Index);
    if (Itr != GlobalConfigStore.end())
        return int32(atoi(Itr->second.c_str()));

    if (Index == 1502)
        return 1;

    return 0;
}

float Helper::GetFloat(uint32 Index)
{
    auto Itr = GlobalConfigStore.find(Index);
    if (Itr != GlobalConfigStore.end())
        return atof(Itr->second.c_str());

    if (Index == 13501)
        return 1.0f;

    if (Index == 13908)
        return 1.0f;

    return 0.0f;
}

bool Helper::GetBoolean(uint32 Index)
{
    auto Itr = GlobalConfigStore.find(Index);
    if (Itr != GlobalConfigStore.end())
    {
        std::transform(Itr->second.begin(), Itr->second.end(), Itr->second.begin(), ::toupper);
        return (Itr->second == "1" || Itr->second == "YES" || Itr->second == "TRUE" || Itr->second == "ON" || Itr->second == "是");
    }

    return false;
}

// 带默认值的GetBoolean版本
bool Helper::GetBoolean(uint32 Index, bool defaultValue)
{
    auto Itr = GlobalConfigStore.find(Index);
    if (Itr != GlobalConfigStore.end())
    {
        std::string val = Itr->second;
        std::transform(val.begin(), val.end(), val.begin(), ::toupper);
        return (val == "1" || val == "YES" || val == "TRUE" || val == "ON" || val == "是");
    }

    return defaultValue;
}

const char* Helper::GetGts(uint32 Id)
{
    auto Itr = GtsStore.find(Id);
    return Itr != GtsStore.end() ? Itr->second.c_str() : "";
}

bool Helper::IsStoreNewBag(Player* player, uint32 NewItemId)
{
    uint32 Count = 1;
    uint32 SpaceForCount = 0;
    ItemPosCountVec Dest;
    InventoryResult Msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, Dest, NewItemId, Count, &SpaceForCount);
    if (Msg != EQUIP_ERR_OK)
        Count -= SpaceForCount;

    if (Count == 0 || Dest.empty())
    {
        player->SendSysMsg(GetGts(6668));
        return false;
    }

    return true;
}

bool Helper::IsCustomAllowedMount(Unit* Caster)
{
    if (Caster->GetTypeId() != TYPEID_PLAYER)
        return true;

    return !(sHelper->GetBoolean(1900) && Caster->HasAuraType(SPELL_AURA_TRANSFORM));
}

bool Helper::IsIgnoreMapCheck(uint32 MapId)
{
    std::string CMapStr = GetConfig(2044);
    if (CMapStr.empty())
        return false;

    // 支持格式：正数=地图ID, 负数=区域ID, 多个用#或,或空格分隔
    std::string str = CMapStr;

    // 替换所有分隔符为统一的#
    for (char& c : str) {
        if (c == ',' || c == ' ' || c == '|')
            c = '#';
    }

    Tokenizer Data(str, '#');
    for (auto const& MId : Data)
    {
        std::string midStr = MId;
        // 去除前后空格
        size_t start = midStr.find_first_not_of(" \t");
        size_t end = midStr.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos)
            midStr = midStr.substr(start, end - start + 1);

        if (midStr.empty())
            continue;

        int32 IdValue = atoi(midStr.c_str());
        // 正数为地图ID
        if (IdValue > 0 && uint32(IdValue) == MapId)
            return true;
    }

    return false;
}

bool Helper::IsIgnoreAreaCheck(uint32 AreaId)
{
    std::string CMapStr = GetConfig(2044);
    if (CMapStr.empty())
        return false;

    // 支持格式：正数=地图ID, 负数=区域ID, 多个用#或,或空格分隔
    size_t pos = 0;
    std::string token;
    std::string str = CMapStr;

    // 替换所有分隔符为统一的#
    for (char& c : str) {
        if (c == ',' || c == ' ' || c == '|')
            c = '#';
    }

    Tokenizer Data(str, '#');
    for (auto const& AId : Data)
    {
        std::string aidStr = AId;
        // 去除前后空格
        size_t start = aidStr.find_first_not_of(" \t");
        size_t end = aidStr.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos)
            aidStr = aidStr.substr(start, end - start + 1);

        if (aidStr.empty())
            continue;

        int32 IdValue = atoi(aidStr.c_str());
        // 负数为区域ID，取绝对值比较
        if (IdValue < 0 && uint32(-IdValue) == AreaId)
            return true;
    }

    return false;
}

bool Helper::IsIgnoreAccountId(uint32 AccountId)
{
    std::string CAccStr = GetConfig(2046);
    if (CAccStr.empty())
        return false;

    bool Ignore = false;
    Tokenizer Data(CAccStr, '#');
    for (auto const& AId : Data)
    {
        if (uint32(atoi(AId)) == AccountId)
            Ignore = true;
    }

    return Ignore;
}

std::string Helper::GetTimeHourStr(time_t KTime)
{
    tm aTm;
    localtime_r(&KTime, &aTm);
    return Acore::StringFormat("%02d时%02d分", aTm.tm_hour, aTm.tm_min);
}

std::string Helper::GetTimeSecondStr(time_t KTime)
{
    tm aTm;
    localtime_r(&KTime, &aTm);
    return Acore::StringFormat("%02d秒", aTm.tm_sec);
}

bool Helper::CanCustomFly(Player* player)
{
    std::string CMapStr = GetConfig(2901);
    if (CMapStr.empty())
        return true;

    bool Ignore = true;
    Tokenizer Data(CMapStr, '#');
    for (auto const& MId : Data)
    {
        int32 Id = int32(atoi(MId));
        if (Id >= 0)
        {
            if (Id == player->GetMapId())
                Ignore = false;
        }
        else
        {
            if (abs(Id) == player->GetAreaId())
                Ignore = false;
        }
    }

    return Ignore;
}

bool Helper::OnDungeon(uint32 MapId)
{
    MapEntry const* MEntry = sMapStore.LookupEntry(MapId);
    if (!MEntry)
        return false;

    return MEntry->IsDungeon();
}

bool Helper::NotRemoveSpellClass(uint32 SepllId)
{
    std::string SepllStr = GetConfig(1032);
    if (SepllStr.empty())
        return false;

    bool Ignore = false;
    Tokenizer Data(SepllStr, '#');
    for (auto const& SId : Data)
    {
        if (uint32(atoi(SId)) == SepllId)
            Ignore = true;
    }

    return Ignore;
}

float Helper::GetWpDPS(uint32 Min, uint32 Max, uint32 ItemId)
{
    ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(ItemId);
    if (!Proto)
        return 0.f;

    if (!Proto->Delay)
        return 0.f;

    float Temp = float(Min + Max);
    return Temp * 500.f / Proto->Delay;
}

std::string Helper::GetWpDPSString(AsInt64 const& Min, AsInt64 const& Max, uint32 ItemId)
{
    ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(ItemId);
    if (!Proto || !Proto->Delay)
        return "0";

    AsInt64 Dps10 = (Min + Max) * AsInt64(5000) / AsInt64(Proto->Delay);
    AsInt64 DpsInteger = Dps10 / AsInt64(10);
    AsInt64 DpsDecimal = Dps10 % AsInt64(10);

    return AsInt64ToString(DpsInteger) + "." + AsInt64ToString(DpsDecimal);
}

bool Helper::IsVectEqual(std::vector<uint32>& A, std::vector<uint32>& B)
{
    if (A.empty() && B.empty())
        return false;

    bool Ｃomp = (A == B);
    return Ｃomp;
}

void Helper::ApplyPlayerWpModFloatVar(float& Var, float Value, bool Apply)
{
    if (Value == -100.0f)
        Value = -99.99f;

    if (GetBoolean(4000))
    {
        Var += (Apply ? (Value / 100.0f) : -(Value / 100.0f));
        if (Var <= -100.0f)
            Var = -99.99f;
        if (Var >= 100.0f)
            Var = 99.99f;
    }
    else
        Var *= (Apply ? (100.0f + Value) / 100.0f : 100.0f / (100.0f + Value));
}

void Helper::DoClearInCombat(Player* player)
{
    if (player && player->IsInWorld())
    {
        player->CombatStop();
        player->getHostileRefMgr().deleteReferences();
    }
}

std::string Helper::GetNowVer()
{
    return 	"2025-04-26-284";
}

bool Helper::InCheckFunc(CoreCheckFunc Num)
{
    if (UserBindData.find("#") == std::string::npos)
        return false;

    Tokenizer Datas(UserBindData, '#');
    if (Datas.IsEmpty() || Datas.size() < 50)
        return false;

    uint32 Index = (uint32)Num;
    if (Index >= FuncMax)
        return false;

    return strcmp(Datas[Index], "1") == 0;
}

void Helper::CustomPrint(std::string BindFuncStr, const char* StrEndTime)
{
    LOG_INFO("server.worldserver", ">> 当前核心版本号: {} 授权到期时间: {}", GetNowVer(), StrEndTime);

    /*long EndTime = StandardToStamp(StrEndTime);
    long NowTime = time(NULL);
    long DiffTime = EndTime - NowTime;
    if (DiffTime > 720)
        LOG_INFO("server.worldserver", ">> 当前核心版本号: {} 授权到期时间: 永久授权 永不过期", GetNowVer());
    else
        LOG_INFO("server.worldserver", ">> 当前核心版本号: {} 授权到期时间: {}", GetNowVer(), StrEndTime);*/

    UserBindData = BindFuncStr;
}

long Helper::StandardToStamp(const char* StrTime)
{
    struct tm stm;
    int iY, iM, iD, iH, iMin, iS;
    memset(&stm, 0, sizeof(stm));
    iY = atoi(StrTime);
    iM = atoi(StrTime + 5);
    iD = atoi(StrTime + 8);
    iH = atoi(StrTime + 11);
    iMin = atoi(StrTime + 14);
    iS = atoi(StrTime + 17);
    stm.tm_year = iY - 1900;
    stm.tm_mon = iM - 1;
    stm.tm_mday = iD;
    stm.tm_hour = iH;
    stm.tm_min = iMin;
    stm.tm_sec = iS;
    return (long)mktime(&stm);
}

void Helper::CustomPrintClose()
{
    LOG_INFO("server.worldserver", ">> 进程正在关闭 处理后台线程中......请等待2-4秒");
}

uint8 Helper::GetNowNum(Item* TargetItem)
{
    if (TargetItem->ResItems[0] == 0)
        return 0;
    else if (TargetItem->ResItems[1] == 0)
        return 1;
    else if (TargetItem->ResItems[2] == 0)
        return 2;
    else if (TargetItem->ResItems[3] == 0)
        return 3;
    else if (TargetItem->ResItems[4] == 0)
        return 4;
    else if (TargetItem->ResItems[5] == 0)
        return 5;
    else if (TargetItem->ResItems[6] == 0)
        return 6;
    else if (TargetItem->ResItems[7] == 0)
        return 7;

    return 8;
}

uint8 Helper::GetLastNum(Item* TargetItem)
{
    if (TargetItem->ResItems[7] != 0)
        return 7;
    else if (TargetItem->ResItems[6] != 0)
        return 6;
    else if (TargetItem->ResItems[5] != 0)
        return 5;
    else if (TargetItem->ResItems[4] != 0)
        return 4;
    else if (TargetItem->ResItems[3] != 0)
        return 3;
    else if (TargetItem->ResItems[2] != 0)
        return 2;
    else if (TargetItem->ResItems[1] != 0)
        return 1;
    else if (TargetItem->ResItems[0] != 0)
        return 0;

    return 8;
}

uint8 Helper::GetDressItemSlot(ItemTemplate const* Proto)
{
    switch (Proto->InventoryType)
    {
    case INVTYPE_HEAD:
        return EQUIPMENT_SLOT_HEAD;
    case INVTYPE_SHOULDERS:
        return EQUIPMENT_SLOT_SHOULDERS;
    case INVTYPE_BODY:
        return EQUIPMENT_SLOT_BODY;
    case INVTYPE_CHEST:
        return EQUIPMENT_SLOT_CHEST;
    case INVTYPE_WAIST:
        return EQUIPMENT_SLOT_WAIST;
    case INVTYPE_LEGS:
        return EQUIPMENT_SLOT_LEGS;
    case INVTYPE_FEET:
        return EQUIPMENT_SLOT_FEET;
    case INVTYPE_WRISTS:
        return EQUIPMENT_SLOT_WRISTS;
    case INVTYPE_HANDS:
        return EQUIPMENT_SLOT_HANDS;
    case INVTYPE_WEAPON:
        return EQUIPMENT_SLOT_MAINHAND;
    case INVTYPE_2HWEAPON:
        return EQUIPMENT_SLOT_MAINHAND;
    case INVTYPE_WEAPONMAINHAND:
        return EQUIPMENT_SLOT_MAINHAND;
    case INVTYPE_WEAPONOFFHAND:
        return EQUIPMENT_SLOT_OFFHAND;
    case INVTYPE_SHIELD:
        return EQUIPMENT_SLOT_OFFHAND;
    case INVTYPE_RANGEDRIGHT:
        return EQUIPMENT_SLOT_RANGED;
    case INVTYPE_RANGED:
        return EQUIPMENT_SLOT_RANGED;
    case INVTYPE_TABARD:
        return EQUIPMENT_SLOT_TABARD;
    case INVTYPE_CLOAK:
        return EQUIPMENT_SLOT_BACK;
    case INVTYPE_ROBE:
        return EQUIPMENT_SLOT_CHEST;
    case INVTYPE_THROWN:
        return EQUIPMENT_SLOT_RANGED;
    }

    return EQUIPMENT_SLOT_END;
}

bool Helper::ClassNewQuestCheck(Player* player, uint32 SpellId)
{
    if (player->GetClass() != CLASS_WARRIOR)
    {
        if (SpellId == 1498 || SpellId == 1665 || SpellId == 1678 || SpellId == 1819 || SpellId == 9582 || SpellId == 10350 || SpellId == 1719)
            return false;
    }
    else if (player->GetClass() != CLASS_PALADIN)
    {
        if (SpellId == 1785 || SpellId == 1788 || SpellId == 9600 || SpellId == 9685)
            return false;
    }
    else if (player->GetClass() != CLASS_HUNTER)
    {
        if (SpellId == 6082 || SpellId == 6085 || SpellId == 6088 || SpellId == 6102 || SpellId == 9485 || SpellId == 9593)
            return false;
    }
    else if (player->GetClass() != CLASS_SHAMAN)
    {
        if (SpellId == 1527 || SpellId == 9555 || SpellId == 96 || SpellId == 9509)
            return false;
    }
    else if (player->GetClass() != CLASS_WARLOCK)
    {
        if (SpellId == 1795 || SpellId == 7603 || SpellId == 1470 || SpellId == 1485 || SpellId == 1598 || SpellId == 1599 || SpellId == 8344 || SpellId == 1474 || SpellId == 1513 || SpellId == 1739 || SpellId == 1471 || SpellId == 1504 || SpellId == 1689 || SpellId == 9619)
            return false;
    }
    else if (player->GetClass() != CLASS_DRUID)
    {
        if (SpellId == 31 || SpellId == 5061 || SpellId == 6001 || SpellId == 6002 || SpellId == 11001)
            return false;
    }

    return true;
}

void Helper::PrintKeepDone(uint32 Times)
{
    LOG_INFO("server.worldserver", ">> [服务端验证] 心跳连接验证服务器成功 下一次心跳在{}秒后", Times);
}

void Helper::PrintResetRestoreDone(uint32 Count)
{
    LOG_INFO("server.worldserver", ">> [服务端验证] 此次服务器故障为 {} 分钟 验证服务器现在已恢复正常", Count * 3);
}

void Helper::PrintKeepError(uint32 Count)
{
    LOG_ERROR("server.worldserver", ">> [服务端验证] 心跳连接验证服务器失败 继续重连中...... 次数 {}", Count);
}

bool Helper::CheckMapAreaItemToBank(Item* DoItem, uint32 MapId, uint32 AreaId)
{
    if (!DoItem)
        return false;

    ItemTemplate const* PTemp = DoItem->GetTemplate();
    if (!PTemp)
        return false;

    if (PTemp->Map && PTemp->Map == MapId)
        return false;

    if (PTemp->Area && PTemp->Area == AreaId)
        return false;

    return true;
}

/*等级*/
uint32 Helper::GetCreLevelByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.Level : 0;
}

uint32 Helper::GetCreHpCountByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? std::max<uint32>(1, Itr->second.HpCount) : 1;
}

/*固定生命值*/
uint32 Helper::GetCreNowHpByTempId(uint32 TempId)
{
    return AsInt64ToUInt32Clamp(GetAsCreNowHpByTempId(TempId), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 Helper::GetAsCreNowHpByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.NowHp : 0;
}

uint32 Helper::GetCreDmgTakenNowByTempId(uint32 TempId)
{
    return AsInt64ToUInt32Clamp(GetAsCreDmgTakenNowByTempId(TempId), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 Helper::GetAsCreDmgTakenNowByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.DmgTakenNow : 0;
}

uint32 Helper::GetCreDmgNowByTempId(uint32 TempId)
{
    return AsInt64ToUInt32Clamp(GetAsCreDmgNowByTempId(TempId), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 Helper::GetAsCreDmgNowByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.DmgNow : 0;
}

float Helper::GetCreDmgWMinByTempId(uint32 TempId)
{
    return float(AsInt64ToDoubleClamp(GetAsCreDmgWMinByTempId(TempId), double(std::numeric_limits<int32>::max())));
}

float Helper::GetCreDmgWMaxByTempId(uint32 TempId)
{
    return float(AsInt64ToDoubleClamp(GetAsCreDmgWMaxByTempId(TempId), double(std::numeric_limits<int32>::max())));
}

AsInt64 Helper::GetAsCreDmgWMinByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.DmgWMin : AsInt64Zero();
}

AsInt64 Helper::GetAsCreDmgWMaxByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.DmgWMax : AsInt64Zero();
}

float Helper::GetCreKtHpByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.KtHp : 0.0f;
}

float Helper::GetCreHpByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.HealthMultiplier : 1.0f;
}

float Helper::GetCreManaByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.ManaMultiplier : 1.0f;
}

float Helper::GetCreMeleeByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    if (Itr == AtCreatureStatTempStore.end())
        return 1.0f;

    return (Itr->second.MinDamageMultiplier + Itr->second.MaxDamageMultiplier) * 0.5f;
}

float Helper::GetCreSpellByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.Spell : 1.0f;
}

float Helper::GetCreRetByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    if (Itr == AtCreatureStatTempStore.end() || Itr->second.DamageReductionPct <= 0 || Itr->second.DamageReductionPct > 100)
        return 0.0f;

    return float(Itr->second.DamageReductionPct) / 100.0f;
}

bool Helper::GetCreOnEtByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() && Itr->second.OfficialDrop == 0;
}

std::string Helper::GetCrePLootIdsByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.PLootIds : "";
}

float Helper::GetCreAuraChanceByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.CreAuraChance : 0.0f;
}

uint32 Helper::GetCreAuraGroupByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.CreAuraGroup : 0;
}

uint32 Helper::GetCreAuraCountByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.CreCount : 0;
}

std::set<uint32> Helper::GetCreAtAurasByTempId(uint32 TempId)
{
    std::set<uint32> Temps;
    auto Itr = AtCreatureStatTempStore.find(TempId);
    if (Itr != AtCreatureStatTempStore.end())
    {
        for (auto const& Temp : Itr->second.AtAuras)
            Temps.insert(Temp);
    }
    return Temps;
}

uint32 Helper::GetCreAttackTimeByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.AttackSpeedValue : 0;
}

AsInt64 Helper::GetCreArmorByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.Armor : AsInt64Zero();
}

int32 Helper::GetCreSchool1ByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.School1 : 0;
}

int32 Helper::GetCreSchool2ByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.School2 : 0;
}

int32 Helper::GetCreSchool3ByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.School3 : 0;
}

int32 Helper::GetCreSchool4ByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.School4 : 0;
}

int32 Helper::GetCreSchool5ByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.School5 : 0;
}

int32 Helper::GetCreSchool6ByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.School6 : 0;
}

int32 Helper::GetCreMinDmgSpellByTempId(uint32 TempId)
{
    return AsInt64ToInt32Clamp(GetAsCreMinDmgSpellByTempId(TempId));
}

AsInt64 Helper::GetAsCreMinDmgSpellByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.MinSpellDmg : 0;
}

int32 Helper::GetCreMaxDmgSpellByTempId(uint32 TempId)
{
    return AsInt64ToInt32Clamp(GetAsCreMaxDmgSpellByTempId(TempId));
}

AsInt64 Helper::GetAsCreMaxDmgSpellByTempId(uint32 TempId)
{
    auto Itr = AtCreatureStatTempStore.find(TempId);
    return Itr != AtCreatureStatTempStore.end() ? Itr->second.MaxSpellDmg : 0;
}

void Helper::AutoResetLuaNowTime()
{
    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        sDoAddon->SendPacketToClient(Itr->second->GetPlayer(), "ResetLuaCache");
    }
}

bool Helper::CheckItemInBag(Player* player, uint32 ItemId)
{
    bool In = false;
    //主包
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; i++)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetEntry() == ItemId)
                In = true;

    //额外四包
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
        if (Bag* pBag = player->GetBagByPos(i))
            for (uint32 j = 0; j < pBag->GetBagSize(); j++)
                if (Item* pItem = player->GetItemByPos(i, j))
                    if (pItem->GetEntry() == ItemId)
                        In = true;

    return In;
}

bool Helper::HasItemCastCd(Player* player, Item* CastItem)
{
    bool Cool = false;
    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (SpellInfo const* SllInfo = sSpellMgr->GetSpellInfo(CastItem->GetTemplate()->Spells[i].SpellId))
        {
            if (player->HasSpellItemCooldown(SllInfo->Id, CastItem->GetTemplate()->ItemId))
                Cool = true;
        }
    }
    return Cool;
}

bool Helper::CanItemLootBag(Player* player)
{
    bool BagNoFull = false;
    //主包
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; i++)
    {
        Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (!pItem)
            BagNoFull = true;
    }

    //额外四包
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
    {
        if (Bag* pBag = player->GetBagByPos(i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); j++)
            {
                Item* pItem = player->GetItemByPos(i, j);
                if (!pItem)
                    BagNoFull = true;
            }
        }
    }

    return BagNoFull;
}

uint32 Helper::GetOnLinePlayerCount()
{
    uint32 PCount = 0;
    SessionMap const& Sessions = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
    {
        if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
            continue;

        if (Itr->second->IsHeadless())
            continue;

        PCount++;
    }

    return PCount;
}

void Helper::WorldDoAllLoad()
{
    LoadGlobalConfig();
    LoadGlobalGts();
    LoadAsTemp();

    sItemFuncMgr->LoadItemFunc();
    sPlayerModMgr->LoadPlayerMod();
    sPlayerModMgr->LoadPVPKMod();
    sPlayerModMgr->LoadTalentInfoMod();
    sCustomClass->LoadCustomClass();
    sPlayerModMgr->LoadDmgRank();
    sAsCreature->LoadAsCreatureStore();
    sItemFuncMgr->LoadSoulItemStore();
    sNoticeMgr->LoadNoticeMgr();
    sAuraSpellMgr->LoadAuraSpellMgr();
    sEnterFBMgr->LoadEnterFBMgr();
    sMultiNpcMgr->LoadMultiVendorStore();
    sCustomVendorMgr->LoadCustomVendorStore();
    sPlayerModMgr->LoadSignMod();
    sItemShopMgr->LoadItemShop();
    sBGAs->LoadBGConfig();
    sPlayerModMgr->LoadEqSlotUpConfig();
    sItemRandomAtMgr->LoadItemRandomAt();
    sItemRunesMgr->LoadItemRunes();
    sArenaSoloMgr->LoadArenaSoloConfig();
    sActivityMgr->LoadActivityData();
    sPlayerModMgr->LoadEnMapAuraData();
    sItemMagicMgr->LoadItemMagicData();
    sItemMagicMgr->LoadItemZodiacData();
    sItemUpMgr->LoadItemUpData();
    sItemPlayerStatMod->LoadItemPlayerStatMod();
    sGuildBattleMgr->LoadGuildBattle();
    sAsPlayerBattleMgr->LoadAsPlayerBattle();
    sNpcBeastMaster->LoadSystem();
    sCombatPetMgr->LoadCombatPetData();
    sAthenaEventMgr->LoadAthenaEventData();
    sCharPvpTop->LoadTopSys();
    sMapAreaFunc->LoadMapAreaFuncData();
    sPicGuideMgr->LoadPicGuideData();
    sPlayerAction->LoadPlayerAction();

}

void Helper::DoZoneInCombatForPlayer(Creature* creature)
{
    if (creature->IsInEvadeMode())
        return;

    if (creature->IsInCombat())
        return;

    Map* map = creature->GetMap();
    if (!map->IsDungeon())
        return;

    Map::PlayerList const& playerList = map->GetPlayers();
    if (playerList.IsEmpty())
        return;

    for (Map::PlayerList::const_iterator itr = playerList.begin(); itr != playerList.end(); ++itr)
    {
        if (Player* player = itr->GetSource())
        {
            creature->SetInCombatWith(player);
            player->SetInCombatWith(creature);

            if (creature->CanHaveThreatList())
                creature->AddThreat(player, 0.0f);
        }
    }
}

void Helper::SendItemQuerySingleOpcode(Player* player, uint32 ItemId)
{
    if (ItemId < 56807)
        return;
    WorldPacket Packet;
    Packet << ItemId;
    player->GetSession()->HandleItemQuerySingleOpcode(Packet);
}

bool Helper::CreInCustomCanAttack(uint32 CreEntry)
{
    bool In = false;
    std::string EntryStr = GetConfig(12002);
    if (EntryStr.empty())
        return In;

    Tokenizer Data(EntryStr, '#');
    for (auto const& EId : Data)
    {
        if (CreEntry == uint32(atoi(EId)))
            In = true;
    }

    return In;
}

void Helper::LogPlayerTokenNow(ObjectGuid Guld, int64 DToken, std::string Info)
{
    if (GetBoolean(12666) && !Info.empty())
    {
        uint32 GuidLow = Guld.GetCounter();
        std::string PlayerName = sCharacterCache->GetCharacterNameForGuid(Guld);
        uint32 AccountId = sCharacterCache->GetCharacterAccountIdByGuid(Guld);
        CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_PLAYER_TOKEN_INFO);
        Rtmt->SetData(0, GuidLow);
        Rtmt->SetData(1, PlayerName);
        Rtmt->SetData(2, AccountId);
        Rtmt->SetData(3, DToken);
        Rtmt->SetData(4, Info);
        CharacterDatabase.Execute(Rtmt);
    }
}

void Helper::LogPlayerItemInfoNow(ObjectGuid Guld, uint32 ItemId, int32 Count, std::string Info)
{
    if (GetBoolean(12888) && !Info.empty())
    {
        uint32 GuidLow = Guld.GetCounter();
        std::string PlayerName = sCharacterCache->GetCharacterNameForGuid(Guld);
        uint32 AccountId = sCharacterCache->GetCharacterAccountIdByGuid(Guld);
        CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_PLAYER_ITEM_INFO);
        Rtmt->SetData(0, GuidLow);
        Rtmt->SetData(1, PlayerName);
        Rtmt->SetData(2, AccountId);
        Rtmt->SetData(3, ItemId);
        Rtmt->SetData(4, Count);
        Rtmt->SetData(5, Info);
        CharacterDatabase.Execute(Rtmt);
    }
}

bool Helper::ItemHasEnch(Item* OnItem)
{
    if (!OnItem)
        return false;

    uint32 OEnch1 = OnItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_0);
    uint32 OEnch2 = OnItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_1);
    uint32 OEnch3 = OnItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_2);
    uint32 OEnch4 = OnItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_3);
    uint32 OEnch5 = OnItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_4);

    return (OEnch1 > 0 || OEnch2 > 0 || OEnch3 > 0 || OEnch4 > 0 || OEnch5 > 0);
}

bool Helper::CheckMechanicForSpellInfo(SpellInfo const* Info, uint32 MaskValue)
{
    if (!Info->Mechanic)
        return false;

    uint32 Mechanic = (1 << (Info->Mechanic - 1));
    return (Mechanic & MaskValue);
}

bool Helper::NoCanSellItem(Item* OnItem)
{
    if (!OnItem)
        return true;

    uint32 OEnch1 = OnItem->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT);
    uint32 OEnch2 = OnItem->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_2);
    uint32 OEnch3 = OnItem->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_3);

    return (OEnch1 || OEnch2 || OEnch3);
}

void Helper::DoAsLoadDataCacheStore()
{
    LOG_INFO("server.worldserver", "\n");
    LOG_INFO("server.worldserver", ">> 正在加载重要缓存数据中......请耐心等待......");

    QueryResult Result = CharacterDatabase.Query("SELECT 装备标识, 装备编号, 强化等级, 前缀名称, 后缀名称, 强化名称, 武器最小伤害, 武器最大伤害, 属性类型1, 属性值1, "
        "属性类型2, 属性值2, 属性类型3, 属性值3, 属性类型4, 属性值4, 属性类型5, 属性值5, 属性类型6, 属性值6, 属性类型7, 属性值7, 属性类型8, 属性值8, 属性类型9, 属性值9, "
        "属性类型10, 属性值10, 属性类型11, 属性值11, 属性类型12, 属性值12, 属性类型13, 属性值13, 属性类型14, 属性值14, 属性类型15, 属性值15, 属性类型16, 属性值16, "
        "技能1, 技能2, 技能3, 技能4, 技能5, 技能6, 技能7, 技能8, 随机属性1, 随机属性2, 随机属性3, 随机属性4, 随机属性5, 随机属性6, 随机属性7 FROM _装备超级随机存档;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            ItemAtCacheTemp Temp;
            uint32 GUIDLow = Fields[0].Get<uint32>();
            Temp.ItemId = Fields[1].Get<uint32>();
            Temp.ULevel = Fields[2].Get<uint32>();
            Temp.Prefix = Fields[3].Get<std::string>();
            Temp.Suffix = Fields[4].Get<std::string>();
            Temp.LevelStr = Fields[5].Get<std::string>();
            Temp.MinWpDmg = AsInt64FromString(Fields[6].Get<std::string>());
            Temp.MaxWpDmg = AsInt64FromString(Fields[7].Get<std::string>());

            for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
            {
                Temp.AtStatStore[i].first = Fields[8 + (i * 2)].Get<int32>();
                Temp.AtStatStore[i].second = AsInt64FromString(Fields[9 + (i * 2)].Get<std::string>());
            }

            for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
                Temp.AtSpell[i] = Fields[40 + i].Get<uint32>();

            //------------   随机属性倍率   ------------
            for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
            {
                Temp._AtSpell[i] = AsInt64FromString(Fields[48 + i].Get<std::string>());
                if (Temp._AtSpell[i] <= 0)
                    Temp._AtSpell[i] = AsInt64(1);
            }
            //------------   随机属性倍率   ------------

            float Mod = sItemRandomAtMgr->GetModForRank(Temp.ItemId, Temp.ULevel);
            Temp.Mod = Mod;

            ItemAtCacheStore.insert(std::make_pair(GUIDLow, Temp));

        } while (Result->NextRow());
    }

    Result = CharacterDatabase.Query("SELECT guid, enchantments FROM item_instance WHERE enchantments <> '0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0';");
    if (Result)
    {
        uint32 Count = MAX_ENCHANTMENT_SLOT * MAX_ENCHANTMENT_OFFSET;
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemGuid = Fields[0].Get<uint32>();
            std::string Data = Fields[1].Get<std::string>();
            if (Data.empty())
                continue;
            std::vector<std::string_view> Tokens = Acore::Tokenize(Data, ' ', false);
            if (Tokens.size() != Count)
                continue;

            Optional<uint32> EnchantId1 = Acore::StringTo<uint32>(Tokens[21]);
            Optional<uint32> EnchantId2 = Acore::StringTo<uint32>(Tokens[24]);
            Optional<uint32> EnchantId3 = Acore::StringTo<uint32>(Tokens[27]);
            Optional<uint32> EnchantId4 = Acore::StringTo<uint32>(Tokens[30]);
            Optional<uint32> EnchantId5 = Acore::StringTo<uint32>(Tokens[33]);
            EnchantTemp Temp;
            Temp.EnchantId1 = *EnchantId1;
            Temp.EnchantId2 = *EnchantId2;
            Temp.EnchantId3 = *EnchantId3;
            Temp.EnchantId4 = *EnchantId4;
            Temp.EnchantId5 = *EnchantId5;
            EnchantAtStore.insert(std::make_pair(ItemGuid, Temp));
        } while (Result->NextRow());
    }

    Result = CharacterDatabase.Query("SELECT 装备标识, 装备编号, 当前插入数量, 符文插槽数量, 符文宝石1, 符文宝石2, 符文宝石3, 符文宝石4, 符文宝石5, 符文宝石6, 符文宝石7, 符文宝石8, 符文宝石9, 符文宝石10, 符文宝石11, 符文宝石12, 符文宝石13, 符文宝石14, 符文宝石15, 符文宝石16, 符文宝石17, 符文宝石18, 符文宝石19, 符文宝石20, 符文宝石21, 符文宝石22, 符文宝石23, 符文宝石24, 符文宝石25, 符文宝石26, 符文宝石27, 符文宝石28, 自带技能1, 自带技能2, 自带技能3, 自带技能4, 自带技能5, 自带技能6, 自带技能7, 自带技能8, 自带技能9, 自带技能10, 自带技能11, 自带技能12, 自带技能13, 自带技能14, 自带技能15, 自带技能16, 自带技能17, 自带技能18, 自带技能19, 自带技能20, 自带技能21, 自带技能22, 自带技能23, 自带技能24, 自带技能25, 自带技能26, 自带技能27, 自带技能28, 组合技名称, 组合技能列表 FROM _装备符文存档;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            ItemRunesCacheTemp Temp;
            uint32 ItemGuid = Fields[0].Get<uint32>();
            Temp.ItemId = Fields[1].Get<uint32>();
            Temp.NowCount = Fields[2].Get<uint8>();
            Temp.RunesCount = Fields[3].Get<uint8>();

            for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
                Temp.ResItems[i] = Fields[4 + i].Get<uint32>();
            uint8 spellBase = static_cast<uint8>(4 + MAX_RUNES_COUNT);
            for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
                Temp.OSpells[i] = Fields[spellBase + i].Get<uint32>();
            uint8 nameIndex = static_cast<uint8>(spellBase + MAX_RUNES_COUNT);
            Temp.GSpellName = Fields[nameIndex].Get<std::string>();
            Temp.GSpellListStr = Fields[nameIndex + 1].Get<std::string>();

            ItemRunesCacheStore[ItemGuid] = Temp;

        } while (Result->NextRow());
    }

    Result = CharacterDatabase.Query("SELECT 物品标识, 物品编号, 洗练次数 FROM _附魔洗练存档;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemGuid = Fields[0].Get<uint32>();
            uint32 ItemId = Fields[1].Get<uint32>();
            uint32 Count = Fields[2].Get<uint32>();
            ItemEnchAtCountStore.insert(std::make_pair(ItemGuid, std::to_string(Count) + "#" + std::to_string(sItemFuncMgr->GetAwEnchRts(ItemId))));

        } while (Result->NextRow());
    }

    ItemTempKtCacheStore.clear();
    Result = CharacterDatabase.Query("SELECT 物品标识, 序号, 变量 FROM ______物品;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            ItemTempKtCacheStore[Fields[0].Get<uint32>()][Fields[1].Get<uint32>()] = Fields[2].Get<int32>();

        } while (Result->NextRow());
    }

    LOG_INFO("server.worldserver", ">> 重要缓存数据读取完毕");
}

class StartedWorldScript : public WorldScript
{
public:
    StartedWorldScript() : WorldScript("StartedWorldScript") { ReLuaCacheTimer = 0; }

    void OnStartup() override
    {
        LOG_INFO("server.worldserver", ">> 模拟器由AzerothCore源码编译 仅仅用于作者个人研究之用");
        LOG_INFO("server.worldserver", ">> 如果有侵犯到其他相关机构的权益 请立即通知本人停止研究");
        LOG_INFO("server.worldserver", ">> 如果使用模拟器进行商业运营或其他违法违规活动的 与作者本人无关");
        LOG_INFO("server.worldserver", ">> 请使用 Ctrl+C 正确退出控制台 请使用 Ctrl+C 正确退出控制台");
        LOG_INFO("server.worldserver", ">> 如果不正确退出可能会出现某些严重问题 如果不正确退出可能会出现某些严重问题");
        LOG_INFO("server.worldserver", ">> 小赛核心，个人爱好研究所用，禁止外传，如果侵权请联系QQ569416517删除");
        LOG_INFO("server.worldserver", "\n");

        if (sHelper->GetBoolean(10002))
        {
            if (!sLFGMgr->IsTesting())
                sLFGMgr->ToggleTesting();
        }
    }

    void OnUpdate(uint32 Diff) override
    {
        if (!sHelper->GetBoolean(555))
            return;

        ReLuaCacheTimer += Diff;
        if (ReLuaCacheTimer >= 8 * MINUTE * IN_MILLISECONDS)
        {
            ReLuaCacheTimer = 0;
            sHelper->AutoResetLuaNowTime();
        }
    }

    void OnBeforeWorldInitialized() override
    {
        sHelper->WorldDoAllLoad();

        // 加载转生配置表
        sReincarnationMgr->LoadReincarnationTemplate();
        sBeastCardMgr->LoadConfig();

        // 加载聊天屏蔽和GCD功能
        sChatFilterMgr->Load();
        sSpellCooldownMgr->Load();
        sAntiGCDHackMgr->Load();

        //sWorld->SetPlayerAmountLimit(8);

        LOG_INFO("server.worldserver", "\n");

        LOG_INFO("server.loading", "功能表 >> 加载 [__控制总表] 总计 {} 条数据", GlobalConfigStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_文本提示表] 总计 {} 条数据", GtsStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [__模版需求总表] 总计 {} 条数据", NeedTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [__模版奖励总表] 总计 {} 条数据", RewardTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [__模版坐标] 总计 {} 条数据", PosMap.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [__模版技能光环] 总计 {} 条数据", AuraVect.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [__模版生物属性表] 总计 {} 条数据", AtCreatureStatTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品背包银行获得属性表] 总计 {} 条数据", ItemInStatStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_功能传送表] 总计 {} 条数据", FunctionStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_幸运抽奖表] 总计 {} 条数据", LuckTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品使用命令表] 总计 {} 条数据", ItemCommandStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品升级随机线表] 总计 {} 条数据", ItemUpRandomStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品升级选择线表] 总计 {} 条数据", ItemUpChoiceTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_双甲物品表] 总计 {} 条数据", SoulItemConfigStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品召唤表] 总计 {} 条数据", SummonStructStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品购买或出售表] 总计 {} 条数据", ItemVsInfoStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品随机附魔控制表] 总计 {} 条数据", ItemEnchStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品随机附魔组表] 加载完毕");
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品随机附魔指定表] 总计 {} 条数据", EnchResetAtItemStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_兑换码奖励表] 总计 {} 条数据", CdkeyStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_一键回收表] 总计 {} 条数据", ItemRyStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_一键回收组] 总计 {} 条数据", ItemRyGroupStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品解绑表] 总计 {} 条数据", ItemClearBindStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品获取限制表] 总计 {} 条数据", AtAddItemLimitStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品装备时需求表] 总计 {} 条数据", ItemEquipStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_ID幻化表] 总计 {} 条数据", IDTransmogStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品名字额外描述表] 总计 {} 条数据", ItemSubNameStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_卡牌物品控制表] 总计 {} 条数据", ItemCardsStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_卡牌效果控制表] 总计 {} 条数据", CardsGpStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品脚本消耗表] 总计 {} 条数据", ItemScriptOnDeleteStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_会员等级配置表] 总计 {} 条数据", VipStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_玩家被玩家击杀掉落表] 总计 {} 条数据", PVPKStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_玩家击杀玩家奖励表] 总计 {} 条数据", PVPMapEventStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_天赋技能需求表] 总计 {} 条数据", CustomTalentTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_玩家职业属性控制表] 总计 {} 条数据", ClassConfigStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [__模版玩家属性表] 总计 {} 条数据", ClassTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_军衔] 总计 {} 条数据", HRStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_斗气配置表] 总计 {} 条数据", StatPtConfigStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_斗气等级经验表] 总计 {} 条数据", StatPtRankStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_斗气奖励表] 总计 {} 条数据", StatPtRewStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_玩家光环杀怪掉落表] 总计 {} 条数据", AuraLootItemStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_进入地图或区域表] 总计 {} 条数据", OnMapAreaStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_生物伤害排行榜表] 总计 {} 条数据", CreDmgRankTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_生物控制组表] 总计 {} 条数据", OneCreGroupStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_地图或区域墓地复活表] 总计 {} 条数据", DieMapGravStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_挑战副本配置表] 总计 {} 条数据", MazChallengeStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_生物自定义智能表] 总计 {} 条数据", CreatureScriptVect.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_副本闯关] 总计 {} 条数据", StageStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_副本天梯配置] 总计 {} 条数据", NtRankStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_副本闯关怪物组] 加载完毕");
        LOG_INFO("server.loading", "功能表 >> 加载 [_副本天梯怪物组] 加载完毕");
        LOG_INFO("server.loading", "功能表 >> 加载 [_生物击杀特殊配置表] 总计 {} 条数据", OKillCreStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_生物击杀特殊随机坐标组表] 总计 {} 条数据", RmSumStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_通告_击杀生物表] 总计 {} 条数据", KillCreNoticeStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_通告_击杀玩家表] 总计 {} 条数据", KillPlayerNoticeStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_通告_物品掉落表] 总计 {} 条数据", LootItemNoticeStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_技能光环控制表] 总计 {} 条数据", OnAsStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_技能施法完成gm命令表] 总计 {} 条数据", SpellCastStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_技能特殊吸血控制表] 总计 {} 条数据", BusXsAuraStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_进入副本控制表] 总计 {} 条数据", EnterFBStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_副本重置] 总计 {} 条数据", ResetFBStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_综合商人表] 总计 {} 条数据", VendorStructStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_签到奖励表] 总计 {} 条数据", SignRewardStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_在线商城出售表] 总计 {} 条数据", ItemShopStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_战场控制表] 总计 {} 条数据", BGTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_装备栏强化] 总计 {} 条数据", EqSlotTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_装备符文配置表] 总计 {} 条数据", AtRunesTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_装备符文宝石表] 总计 {} 条数据", ItemRunesTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_装备符文技能组合表] 总计 {} 条数据", SpGroupTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_装备符文开槽表] 总计 {} 条数据", ItemASlotRunesTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_1V1竞技场配置表] 总计 {} 条数据", ArenaSoloConfigStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_每日活跃度配置表] 总计 {} 条数据", ActivityStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_每日活跃度奖励表] 总计 {} 条数据", RewActivityStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_装备栏拓展] 总计 {} 条数据", EqAuraStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_地图玩家光环表] 总计 {} 条数据", EnMapPlayerStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_玩家法宝技能表] 总计 {} 条数据", ItemMagicStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_玩家法宝类型激活表] 总计 {} 条数据", TypeMagicStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_传奇十二生肖配置表] 总计 {} 条数据", ZodiacStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_传奇十二生肖激活表] 总计 {} 条数据", ZodiacCenterStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品附魔提取转移表] 总计 {} 条数据", ItemEnchTaskStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品普通升级合成表] 总计 {} 条数据", MUpStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品属性奖励表] 总计 {} 条数据", ZsStatTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_物品玩家使用次数表] 总计 {} 条数据", UseItemLimitStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_比武大会] 完毕");
        LOG_INFO("server.loading", "功能表 >> 加载 [_地图区域综合控制表] 总计 {} 条数据", AreaFuncTempStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [_动作触发功能表] 总计 {} 条数据", sPlayerAction->GetActionCount());

        LOG_INFO("server.loading", "功能表 >> 加载 [zs_图鉴_物品_配置项目] 总计 {} 条数据", PicItemGuideConfigStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [zs_图鉴_物品_页面关联] 总计 {} 条数据", PicItemGuidePageMultStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [zs_图鉴_物品_设置羁绊] 总计 {} 条数据", PicItemGuideBondsStore.size());

        LOG_INFO("server.loading", "功能表 >> 加载 [zs_图鉴_生物_配置项目] 总计 {} 条数据", PicCreatureGuideConfigStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [zs_图鉴_生物_页面关联] 总计 {} 条数据", PicCreatureGuidePageMultStore.size());
        LOG_INFO("server.loading", "功能表 >> 加载 [zs_图鉴_生物_设置羁绊] 总计 {} 条数据", PicCreatureGuideBondsStore.size());

        //记录 授权验证
        if (sHelper->InCheckFunc(FuncPet))
        {
            LOG_INFO("server.worldserver", "\n");
            LOG_INFO("server.loading", "--------------------授权定制功能--------------------");
            LOG_INFO("server.worldserver", "\n");
            LOG_INFO("server.loading", "功能表 >> 加载 [_战宠基础配置表] 总计 {} 条数据", CPetTempStore.size());
            LOG_INFO("server.loading", "功能表 >> 加载 [_战宠经验奖励表] 总计 {} 条数据", CPetGasStore.size());
            LOG_INFO("server.loading", "功能表 >> 加载 [_战宠蛋使用配置表] 总计 {} 条数据", CPetUseStore.size());
            LOG_INFO("server.loading", "功能表 >> 加载 [_战宠蛋阶级组表] 总计 {} 条数据", CPetGroupRankStore.size());
            LOG_INFO("server.worldserver", "\n");
            LOG_INFO("server.loading", "--------------------授权定制功能--------------------");
            LOG_INFO("server.worldserver", "\n");
        }

        sHelper->DoAsLoadDataCacheStore();

        LOG_INFO("server.worldserver", "\n");
    }

private:
    uint32 ReLuaCacheTimer;
};

class CustomDamageScript : public UnitScript
{
public:
    CustomDamageScript() : UnitScript("CustomDamageScript")
    {

    }

    static AsInt64 GetItemCombatAttribute(Player const* player, uint32 type)
    {
        return player ? player->GetAsStatBonus(static_cast<ItemModType>(type)) : AsInt64Zero();
    }

    static void ApplyItemCombatBonus(AsInt64& amount, Player const* player, uint32 percentType, uint32 fixedType)
    {
        if (!player || amount <= 0)
            return;
        AsInt64 percent = GetItemCombatAttribute(player, percentType);
        AsInt64 fixed = GetItemCombatAttribute(player, fixedType);
        if (percent > 0)
            amount += amount * percent / 100;
        if (fixed > 0)
            amount += fixed;
    }

    static void ApplyItemCombatReduction(AsInt64& amount, Player const* player, uint32 percentType, uint32 fixedType)
    {
        if (!player || amount <= 0)
            return;
        AsInt64 percent = GetItemCombatAttribute(player, percentType);
        AsInt64 fixed = GetItemCombatAttribute(player, fixedType);
        if (percent > 100)
            percent = 100;
        if (percent > 0)
            amount -= amount * percent / 100;
        if (fixed > 0)
            amount = fixed >= amount ? AsInt64(1) : amount - fixed;
    }

    static void ApplyItemCombatFinish(AsInt64& amount, Player* attacker, Unit* victim, bool isPvp)
    {
        if (!attacker || !victim || amount <= 0)
            return;

        AsInt64 trueDamage = GetItemCombatAttribute(attacker, isPvp ? 222 : 322);
        if (trueDamage > 0)
            amount += isPvp ? amount * trueDamage / 100 : trueDamage;

        uint32 fixedCutType = isPvp ? 227 : 327;
        uint32 fixedChanceType = isPvp ? 228 : 328;
        uint32 percentCutType = isPvp ? 229 : 329;
        uint32 percentChanceType = isPvp ? 230 : 330;
        uint32 fixedChance = AsInt64ToUInt32Clamp(GetItemCombatAttribute(attacker, fixedChanceType), 100);
        uint32 percentChance = AsInt64ToUInt32Clamp(GetItemCombatAttribute(attacker, percentChanceType), 100);
        if (fixedChance && roll_chance_i(fixedChance))
            amount += GetItemCombatAttribute(attacker, fixedCutType);
        if (percentChance && roll_chance_i(percentChance))
            amount += victim->GetAsMaxHealth() * GetItemCombatAttribute(attacker, percentCutType) / 100;
    }

    static void ApplyItemCombatLeech(AsInt64 const& amount, Player* attacker, Unit* victim, bool isPvp)
    {
        if (!attacker || !victim || amount <= 0)
            return;

        AsInt64 effectiveAmount = amount;
        AsInt64 victimHealth = victim->GetAsTotalHealth();
        if (victimHealth > 0 && effectiveAmount > victimHealth)
            effectiveAmount = victimHealth;

        AsInt64 leech = GetItemCombatAttribute(attacker, isPvp ? 213 : 312);
        AsInt64 leechPercent = GetItemCombatAttribute(attacker, isPvp ? 212 : 313);
        if (leechPercent > 0)
            leech += effectiveAmount * leechPercent / 100;

        if (leech <= 0)
            return;

        AsInt64 missingHealth = attacker->GetAsMaxHealth() - attacker->GetAsHealth();
        if (missingHealth <= 0)
            return;
        if (leech > missingHealth)
            leech = missingHealth;

        attacker->ModifyAsHealth(leech);
    }

    static bool IsPvpHealingContext(Unit* healer, Unit* receiver)
    {
        Player* sourcePlayer = healer ? healer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        Player* targetPlayer = receiver ? receiver->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        return sourcePlayer && targetPlayer &&
            ((sourcePlayer->InBattleground() && targetPlayer->InBattleground() &&
                sourcePlayer->GetBattleground() == targetPlayer->GetBattleground()) ||
                sourcePlayer->duel);
    }

    static void ApplyItemCombatReflection(Unit* attacker, Player* defender, AsInt64 const& incoming, bool isPvp, bool isMagic)
    {
        static thread_local bool reflecting = false;
        if (reflecting || !attacker || !defender || attacker == defender || incoming <= 0 || !attacker->IsAlive())
            return;

        uint32 percentType;
        uint32 fixedType;
        if (isPvp)
        {
            percentType = isMagic ? 220 : 218;
            fixedType = isMagic ? 221 : 219;
        }
        else
        {
            fixedType = isMagic ? 320 : 318;
            percentType = isMagic ? 321 : 319;
        }

        AsInt64 reflected = GetItemCombatAttribute(defender, fixedType);
        AsInt64 percent = GetItemCombatAttribute(defender, percentType);
        if (percent > 0)
            reflected += incoming * percent / 100;
        if (reflected <= 0)
            return;

        reflecting = true;
        Unit::DealAsDamage(defender, attacker, reflected, nullptr, DIRECT_DAMAGE,
            isMagic ? SPELL_SCHOOL_MASK_ARCANE : SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
        reflecting = false;
    }

    /*void OnDamage(Unit* Attacker, Unit* Victim, uint32& Damage) override  //奥山战场中玩家对旗帜造成伤害时，伤害为0
    {
        if (!Victim || !Attacker || !Damage || !Victim->IsInWorld() || !Attacker->IsInWorld())
            return;

        if (Attacker->GetTypeId() == TYPEID_PLAYER)
        {
            Player* player = Attacker->ToPlayer();
            if (player && player->GetBattlegroundTypeId() == BATTLEGROUND_AV && sHelper->GetBoolean(22000) && (Victim->GetEntry() == 11948 || Victim->GetEntry() == 11946))
            {
                Damage = 0;
            }
        }
    }*/

    void OnDamageWpMinMax(Unit* Attacker, float& MinDmg, float& MaxDmg) override
    {
        if (!Attacker || !Attacker->IsInWorld())
            return;

        if (Attacker->GetTypeId() == TYPEID_PLAYER)
            return;

        Creature* Me = Attacker->ToCreature();
        if (!Me)
            return;

        if (!sAsCreature->IsInCustom(Me))
            return;

        auto [Min, Max] = sAsCreature->AutoDmgCreatureMinAndMax(Me);
        if (Min > 0.0f && Max > 0.0f)
        {
            MinDmg = Min;
            MaxDmg = Max;
        }
    }

    void OnDamageMeleeCritNow(Unit* Attacker, Unit* Victim, uint32& Damage) override
    {
        if (!Victim || !Attacker || !Damage || !Victim->IsInWorld() || !Attacker->IsInWorld())
            return;

        if (Player* Aplayer = Attacker->ToPlayer())
        {
            AsInt64 RealDamage = Attacker->PeekAsDamageOverride(Victim, Damage);
            float CritMod = 1.0f;
            if (Victim->GetTypeId() != TYPEID_PLAYER)  //玩家对单位 表示PVE
            {
                CritMod = sAuraSpellMgr->AutoPVEMeleeCritAuraMod(Aplayer);
                ApplyItemCombatBonus(RealDamage, Aplayer, 317, 316);
            }
            else if (Victim->GetTypeId() == TYPEID_PLAYER)  //玩家对玩家 表示PVP
            {
                CritMod = sAuraSpellMgr->AutoPVPMeleeCritAuraMod(Aplayer);
                ApplyItemCombatBonus(RealDamage, Aplayer, 216, 217);
                ApplyItemCombatReduction(RealDamage, Victim->ToPlayer(), 226, 225);
            }

            ScaleAsDamagePct(RealDamage, CritMod);
            ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);
        }
    }

    void OnDamageSpellCritEnd(Unit const* Attacker, Unit const* Victim, uint32 CastId, int32& Damage) override
    {
        if (!Victim || !Attacker || !Damage || !Victim->IsInWorld() || !Attacker->IsInWorld())
            return;

        if (Attacker->GetTypeId() != TYPEID_PLAYER)
            return;

        Unit* RealAttacker = const_cast<Unit*>(Attacker);
        Unit* RealVictim = const_cast<Unit*>(Victim);
        AsInt64 RealDamage = RealAttacker->PeekAsDamageOverride(RealVictim, uint32(std::max(0, Damage)));
        float CritMod = 1.0f;
        if (Victim->GetTypeId() != TYPEID_PLAYER)  //玩家对单位 表示PVE
        {
            CritMod = sAuraSpellMgr->AutoPVESpellCritAuraMod(Attacker);
            ApplyItemCombatBonus(RealDamage, Attacker->ToPlayer(), 315, 314);
        }
        else if (Victim->GetTypeId() == TYPEID_PLAYER)  //玩家对玩家 表示PVP
        {
            CritMod = sAuraSpellMgr->AutoPVPSpellCritAuraMod(Attacker);
            ApplyItemCombatBonus(RealDamage, Attacker->ToPlayer(), 214, 215);
            ApplyItemCombatReduction(RealDamage, Victim->ToPlayer(), 224, 223);
        }

        ScaleAsDamagePct(RealDamage, CritMod);
        ApplyAsDamageOverride(RealAttacker, RealVictim, RealDamage, Damage);
    }

    void OnDamageMeleeNow(Unit* Attacker, Unit* Victim, uint32& Damage) override
    {
        if (!Victim || !Attacker || !Damage || !Victim->IsInWorld() || !Attacker->IsInWorld())
            return;

        AsInt64 RealDamage = Attacker->PeekAsDamageOverride(Victim, Damage);
        LogCustomDamageStage("melee_start", Attacker, Victim, Damage, RealDamage);

        if (Attacker->GetTypeId() == TYPEID_PLAYER)
        {
            if (Player* Aplayer = Attacker->ToPlayer())
            {
                if (Victim->GetTypeId() != TYPEID_PLAYER)  //玩家对单位 表示PVE
                {
                    if (Aplayer->Player_ay[NULL].PVEWL_Aura > 0)
                        AddAsDamagePct(RealDamage, Aplayer->Player_ay[NULL].PVEWL_Aura);

                    ScaleAsDamagePct(RealDamage, sCustomClass->GetMPVEModForClass(Aplayer));
                    sAuraSpellMgr->AutoPVEDmg(Aplayer, RealDamage);
                    ApplyItemCombatBonus(RealDamage, Aplayer, 301, 300);
                    ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);
                    LogCustomDamageStage("melee_after_player_pve", Attacker, Victim, Damage, RealDamage);
                }
                else if (Victim->GetTypeId() == TYPEID_PLAYER)  //玩家对玩家 表示PVP
                {
                    ScaleAsDamagePct(RealDamage, sCustomClass->GetMPVPModForClass(Aplayer));
                    sAuraSpellMgr->AutoPVPDmg(Aplayer, RealDamage);
                    ApplyItemCombatBonus(RealDamage, Aplayer, 200, 201);

                    if (Player* Tplayer = Victim->ToPlayer())
                    {
                        ApplyAsDamageRet(RealDamage, sCustomClass->GetPVPRetModForClass(Tplayer));     //PVP免伤
                        sAuraSpellMgr->AutoPVPRet(Tplayer, RealDamage);     //PVP免伤
                        ApplyItemCombatReduction(RealDamage, Tplayer, 206, 207);
                    }

                    ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);
                    LogCustomDamageStage("melee_after_player_pvp", Attacker, Victim, Damage, RealDamage);
                }
            }
        }
        else if (Attacker->GetTypeId() != TYPEID_PLAYER)           //生物伤害
        {
            if (Creature* Me = Attacker->ToCreature())
            {
                RealDamage = Attacker->PeekAsDamageOverride(Victim, Damage);
                if ((Me->IsPet() || Me->IsGuardian() || Me->IsHunterPet()) &&
                    (RealDamage <= Damage || IsLegacyDamageLimitValue(RealDamage)))
                {
                    AsInt64 rebuiltDamage = Attacker->BuildAsWeaponDamageOverride(Victim, Damage, BASE_ATTACK, false, true);
                    if (rebuiltDamage > RealDamage)
                    {
                        RealDamage = rebuiltDamage;
                        ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);
                    }
                }
                ScaleAsDamagePct(RealDamage, sAsCreature->GetMeleeMod(Me));
                ScaleAsDamagePct(RealDamage, sAsCreature->GetMazMeleeMod(Me));
                if (Player* Tplayer = Victim->ToPlayer())
                {
                    ApplyAsDamageRet(RealDamage, sCustomClass->GetPVERetModForClass(Tplayer));             //PVE免伤
                    sAuraSpellMgr->AutoPVERet(Tplayer, RealDamage);     //PVE免伤
                    ApplyItemCombatReduction(RealDamage, Tplayer, 307, 306);
                }

                AsInt64 DmgNow = sAsCreature->GetAsDmgNow(Me);
                if (DmgNow > 0 && !IsLegacyDamageLimitValue(DmgNow))
                    RealDamage = DmgNow;

                DmgNow = sAsCreature->GetAsMazDmgNow(Me);
                if (DmgNow > 0 && !IsLegacyDamageLimitValue(DmgNow))
                    RealDamage = DmgNow;

                ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);

                float KtHp = sAsCreature->GetKtHpMod(Me);
                if (KtHp > 0.0f && Victim->IsAlive())
                {
                    SpellInfo const* KInfo = sSpellMgr->GetSpellInfo(sHelper->GetUInt(3201));
                    if (KInfo)
                    {
                        AsInt64 AsDmg = ScaleAsInt64Value(Victim->GetAsMaxHealth(), KtHp);
                        uint32 Dmg = AsInt64ToUInt32Clamp(AsDmg, uint32(std::numeric_limits<int32>::max()));
                        DealSupplementalSpellDamagePreserveOverride(Attacker, Victim, KInfo, AsDmg, Dmg);
                    }
                }

                KtHp = sAsCreature->GetMazKtHpMod(Me);
                if (KtHp > 0.0f && Victim->IsAlive())
                {
                    SpellInfo const* KInfo = sSpellMgr->GetSpellInfo(sHelper->GetUInt(3201));
                    if (KInfo)
                    {
                        AsInt64 AsDmg = ScaleAsInt64Value(Victim->GetAsMaxHealth(), KtHp);
                        uint32 Dmg = AsInt64ToUInt32Clamp(AsDmg, uint32(std::numeric_limits<int32>::max()));
                        DealSupplementalSpellDamagePreserveOverride(Attacker, Victim, KInfo, AsDmg, Dmg);
                    }
                }
            }
        }

        if (Victim->GetTypeId() != TYPEID_PLAYER)          //生物免伤
        {
            if (Creature* Me = Victim->ToCreature())
            {
                RealDamage = Attacker->PeekAsDamageOverride(Victim, Damage);
                ApplyAsDamageRet(RealDamage, sAsCreature->GetRetMod(Me));
                ApplyAsDamageRet(RealDamage, sAsCreature->GetMazRetMod(Me));

                AsInt64 TakenNow = sAsCreature->GetAsDmgTakenNow(Me);
                if (TakenNow > 0 && !IsLegacyDamageLimitValue(TakenNow))
                    RealDamage = TakenNow;

                TakenNow = sAsCreature->GetAsMazDmgTakenNow(Me);
                if (TakenNow > 0 && !IsLegacyDamageLimitValue(TakenNow))
                    RealDamage = TakenNow;

                ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);
                LogCustomDamageStage("melee_after_creature_ret", Attacker, Victim, Damage, RealDamage);
            }
        }

        if (Player* Aplayer = Attacker->ToPlayer())
        {
            RealDamage = Attacker->PeekAsDamageOverride(Victim, Damage);
            ApplyItemCombatFinish(RealDamage, Aplayer, Victim, Victim->GetTypeId() == TYPEID_PLAYER);
            ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);
            ApplyPlayerDamageLimit(Aplayer, Victim, Damage);
            RealDamage = Attacker->PeekAsDamageOverride(Victim, Damage);
            ApplyItemCombatLeech(RealDamage, Aplayer, Victim, Victim->GetTypeId() == TYPEID_PLAYER);
            LogCustomDamageStage("melee_after_limit", Attacker, Victim, Damage, RealDamage);

            AsInt64 GMDmg = sAuraSpellMgr->AutoAsGMDmgAuraMod(Aplayer);
            uint32 CompatGMDmg = AsInt64ToUInt32Clamp(GMDmg, uint32(std::numeric_limits<int32>::max()));
            SpellInfo const* KInfo = sSpellMgr->GetSpellInfo(sHelper->GetUInt(3202));
            if (GMDmg > 0 && CompatGMDmg && KInfo)
            {
                DealSupplementalSpellDamagePreserveOverride(Attacker, Victim, KInfo, GMDmg, CompatGMDmg);
            }
        }

        sAuraSpellMgr->HealXsOnDamage(Attacker, Victim, RealDamage);

        sAuraSpellMgr->BusHealXsOnDamage(Attacker, Victim, RealDamage);

        Player* player = Attacker->GetCharmerOrOwnerPlayerOrPlayerItself();

        if (player && player->InEvent())
            player->UpdateEventDamage(Victim, Attacker->PeekAsDamageOverride(Victim, Damage));

        if (player && Victim->ToPlayer() && sGuildBattleMgr->InStart)
            sGuildBattleMgr->GuildUpdateEventDamage(player, Victim->ToPlayer(), Attacker->PeekAsDamageOverride(Victim, Damage));

        if (player && Victim->ToPlayer() && sAsPlayerBattleMgr->Started)
            sAsPlayerBattleMgr->AsPlayerUpdateEventDamage(player, Victim->ToPlayer(), Attacker->PeekAsDamageOverride(Victim, Damage));

        if (Player* defender = Victim->ToPlayer())
            ApplyItemCombatReflection(Attacker, defender, Attacker->PeekAsDamageOverride(Victim, Damage),
                Attacker->GetTypeId() == TYPEID_PLAYER, false);

    }

    void OnDamageSpellBefore(Unit* Attacker, Unit* Victim, uint32 CastId, int32& Damage) override
    {
        if (!Victim || !Attacker || !Damage || !Victim->IsInWorld() || !Attacker->IsInWorld())
            return;

        AsInt64 RealDamage = Attacker->PeekAsDamageOverride(Victim, uint32(std::max(0, Damage)));

        if (Attacker->GetTypeId() == TYPEID_PLAYER)
        {
            if (Player* Aplayer = Attacker->ToPlayer())
            {
                if (Victim->GetTypeId() != TYPEID_PLAYER)  //玩家对单位 表示PVE
                {
                    sAuraSpellMgr->AutoPVEDmgBefore(Aplayer, RealDamage, CastId);
                    ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);
                }
            }
        }
        if (Damage < 0)
            Damage = 0;
    }

    void OnDamageSpellEnd(Unit* Attacker, Unit* Victim, uint32 CastId, uint32& Damage) override
    {
        if (!Victim || !Attacker || !Damage || !Victim->IsInWorld() || !Attacker->IsInWorld())
            return;

        AsInt64 RealDamage = Attacker->PeekAsDamageOverride(Victim, Damage);
        LogCustomDamageStage("spell_start", Attacker, Victim, Damage, RealDamage, CastId);

        if (Attacker->GetTypeId() == TYPEID_PLAYER)
        {
            if (Player* Aplayer = Attacker->ToPlayer())
            {
                if (Victim->GetTypeId() != TYPEID_PLAYER)  //玩家对单位 表示PVE
                {
                    if (Aplayer->Player_ay[NULL].PVEFS_Aura > 0)
                        AddAsDamagePct(RealDamage, Aplayer->Player_ay[NULL].PVEFS_Aura);

                    ScaleAsDamagePct(RealDamage, sCustomClass->GetSPVEModForClass(Aplayer));
                    sAuraSpellMgr->AutoPVEDmg(Aplayer, RealDamage, CastId);
                    ScaleAsDamagePct(RealDamage, sAuraSpellMgr->AutoPVEDmgMod(Aplayer, CastId));
                    ApplyItemCombatBonus(RealDamage, Aplayer, 303, 302);
                }
                else if (Victim->GetTypeId() == TYPEID_PLAYER)  //玩家对玩家 表示PVP
                {
                    ScaleAsDamagePct(RealDamage, sCustomClass->GetSPVPModForClass(Aplayer));
                    sAuraSpellMgr->AutoPVPDmg(Aplayer, RealDamage, CastId);
                    ScaleAsDamagePct(RealDamage, sAuraSpellMgr->AutoPVPDmgMod(Aplayer, CastId));
                    ApplyItemCombatBonus(RealDamage, Aplayer, 202, 203);

                    if (Player* Tplayer = Victim->ToPlayer())
                    {
                        ApplyAsDamageRet(RealDamage, sCustomClass->GetPVPRetModForClass(Tplayer));     //PVP免伤
                        sAuraSpellMgr->AutoPVPRet(Tplayer, RealDamage);     //PVP免伤
                        ApplyItemCombatReduction(RealDamage, Tplayer, 208, 209);
                    }
                }

                ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);

                sAuraSpellMgr->HookAutoAddDmg(Aplayer, CastId, RealDamage);
                ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);
                LogCustomDamageStage("spell_after_player_mod", Attacker, Victim, Damage, RealDamage, CastId);
            }
        }
        else if (Attacker->GetTypeId() != TYPEID_PLAYER)           //生物伤害
        {
            if (Creature* Me = Attacker->ToCreature())
            {
                RealDamage = Attacker->PeekAsDamageOverride(Victim, Damage);
                ScaleAsDamagePct(RealDamage, sAsCreature->GetSpellMod(Me));
                ScaleAsDamagePct(RealDamage, sAsCreature->GetMazSpellMod(Me));
                AsInt64 DmgNow = sAsCreature->GetAsDmgNow(Me);
                if (DmgNow > 0 && !IsLegacyDamageLimitValue(DmgNow))
                    RealDamage = DmgNow;

                DmgNow = sAsCreature->GetAsMazDmgNow(Me);
                if (DmgNow > 0 && !IsLegacyDamageLimitValue(DmgNow))
                    RealDamage = DmgNow;
                float KtHp = sAsCreature->GetKtHpMod(Me);
                if (KtHp > 0.0f && Victim->IsAlive())
                {
                    SpellInfo const* KInfo = sSpellMgr->GetSpellInfo(sHelper->GetUInt(3201));
                    if (KInfo)
                    {
                        AsInt64 AsDmg = ScaleAsInt64Value(Victim->GetAsMaxHealth(), KtHp);
                        uint32 Dmg = AsInt64ToUInt32Clamp(AsDmg, uint32(std::numeric_limits<int32>::max()));
                        DealSupplementalSpellDamagePreserveOverride(Attacker, Victim, KInfo, AsDmg, Dmg);
                    }
                }

                KtHp = sAsCreature->GetMazKtHpMod(Me);
                if (KtHp > 0.0f && Victim->IsAlive())
                {
                    SpellInfo const* KInfo = sSpellMgr->GetSpellInfo(sHelper->GetUInt(3201));
                    if (KInfo)
                    {
                        AsInt64 AsDmg = ScaleAsInt64Value(Victim->GetAsMaxHealth(), KtHp);
                        uint32 Dmg = AsInt64ToUInt32Clamp(AsDmg, uint32(std::numeric_limits<int32>::max()));
                        DealSupplementalSpellDamagePreserveOverride(Attacker, Victim, KInfo, AsDmg, Dmg);
                    }
                }
                if (Player* Tplayer = DmgNow <= 0 ? Victim->ToPlayer() : nullptr)
                {
                    ApplyAsDamageRet(RealDamage, sCustomClass->GetPVERetModForClass(Tplayer));             //PVE免伤
                    sAuraSpellMgr->AutoPVERet(Tplayer, RealDamage);     //PVE免伤
                    ApplyItemCombatReduction(RealDamage, Tplayer, 309, 308);
                }

                ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);
            }
        }

        if (Victim->GetTypeId() != TYPEID_PLAYER)          //生物免伤
        {
            if (Creature* Me = Victim->ToCreature())
            {
                RealDamage = Attacker->PeekAsDamageOverride(Victim, Damage);
                ApplyAsDamageRet(RealDamage, sAsCreature->GetRetMod(Me));
                ApplyAsDamageRet(RealDamage, sAsCreature->GetMazRetMod(Me));

                AsInt64 TakenNow = sAsCreature->GetAsDmgTakenNow(Me);
                if (TakenNow > 0 && !IsLegacyDamageLimitValue(TakenNow))
                    RealDamage = TakenNow;

                TakenNow = sAsCreature->GetAsMazDmgTakenNow(Me);
                if (TakenNow > 0 && !IsLegacyDamageLimitValue(TakenNow))
                    RealDamage = TakenNow;

                ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);
                LogCustomDamageStage("spell_after_creature_ret", Attacker, Victim, Damage, RealDamage, CastId);
            }
        }

        RealDamage = Attacker->PeekAsDamageOverride(Victim, Damage);
        ScaleAsDamagePct(RealDamage, sAuraSpellMgr->GetXhDmgMod(CastId));
        ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);

        AsInt64 DNow = sAuraSpellMgr->GetAsXhNowDmgMod(CastId);
        if (DNow > 0)
        {
            RealDamage = DNow;
            ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);
        }

        if (Player* Aplayer = Attacker->ToPlayer())
        {
            RealDamage = Attacker->PeekAsDamageOverride(Victim, Damage);
            ApplyItemCombatFinish(RealDamage, Aplayer, Victim, Victim->GetTypeId() == TYPEID_PLAYER);
            ApplyAsDamageOverride(Attacker, Victim, RealDamage, Damage);
            ApplyPlayerDamageLimit(Aplayer, Victim, Damage);
            RealDamage = Attacker->PeekAsDamageOverride(Victim, Damage);
            ApplyItemCombatLeech(RealDamage, Aplayer, Victim, Victim->GetTypeId() == TYPEID_PLAYER);
            LogCustomDamageStage("spell_after_limit", Attacker, Victim, Damage, RealDamage, CastId);

            AsInt64 GMDmg = sAuraSpellMgr->AutoAsGMDmgAuraMod(Aplayer);
            uint32 CompatGMDmg = AsInt64ToUInt32Clamp(GMDmg, uint32(std::numeric_limits<int32>::max()));
            SpellInfo const* KInfo = sSpellMgr->GetSpellInfo(sHelper->GetUInt(3202));
            if (GMDmg > 0 && CompatGMDmg && KInfo)
            {
                DealSupplementalSpellDamagePreserveOverride(Attacker, Victim, KInfo, GMDmg, CompatGMDmg);
            }
        }

        sAuraSpellMgr->HealXsOnDamage(Attacker, Victim, RealDamage, CastId);

        sAuraSpellMgr->BusHealXsOnDamage(Attacker, Victim, RealDamage, CastId);

        Player* player = Attacker->GetCharmerOrOwnerPlayerOrPlayerItself();

        if (player && player->InEvent())
            player->UpdateEventDamage(Victim, Attacker->PeekAsDamageOverride(Victim, Damage));

        if (player && Victim->ToPlayer() && sGuildBattleMgr->InStart)
            sGuildBattleMgr->GuildUpdateEventDamage(player, Victim->ToPlayer(), Attacker->PeekAsDamageOverride(Victim, Damage));

        if (player && Victim->ToPlayer() && sAsPlayerBattleMgr->Started)
            sAsPlayerBattleMgr->AsPlayerUpdateEventDamage(player, Victim->ToPlayer(), Attacker->PeekAsDamageOverride(Victim, Damage));

        if (Player* defender = Victim->ToPlayer())
            ApplyItemCombatReflection(Attacker, defender, Attacker->PeekAsDamageOverride(Victim, Damage),
                Attacker->GetTypeId() == TYPEID_PLAYER, true);

    }

    void OnDamageMeleeFinal(Unit* Attacker, Unit* Victim, AsInt64 const& Damage, bool Critical) override
    {
        if (!Victim || !Attacker || Damage <= 0 || !Victim->IsInWorld() || !Attacker->IsInWorld())
            return;

        Player* player = Attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
        AsInt64 finalDamage = Damage;
        if (player)
        {
            finalDamage = GetOwnedUnitDisplayDamage(player, Attacker, Damage);
            LogCustomDamageStage("melee_final_display", Attacker, Victim, AsInt64ToUInt32Clamp(Damage), finalDamage);
            SendPlayerDisplayDamage(player, finalDamage, Critical, SPELL_SCHOOL_MASK_NORMAL, 0);

            if (Victim->ToCreature())
                sPlayerModMgr->DmgRankOnCreatureDamageTaken(Victim->ToCreature(), player, finalDamage);
        }

        if (Player* victimPlayer = Victim->ToPlayer())
        {
            if (victimPlayer != player)
                SendPlayerIncomingDamage(victimPlayer, Attacker, finalDamage, SPELL_SCHOOL_MASK_NORMAL, 0);
        }
    }

    void OnDamageSpellFinal(Unit* Attacker, Unit* Victim, uint32 CastId, AsInt64 const& Damage, bool Critical) override
    {
        if (!Victim || !Attacker || Damage <= 0 || !Victim->IsInWorld() || !Attacker->IsInWorld())
            return;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(CastId);
        uint32 schoolMask = spellInfo ? uint32(spellInfo->GetSchoolMask()) : uint32(SPELL_SCHOOL_MASK_NORMAL);
        Player* player = Attacker->GetCharmerOrOwnerPlayerOrPlayerItself();
        AsInt64 finalDamage = Damage;
        if (player)
        {
            finalDamage = GetOwnedUnitDisplayDamage(player, Attacker, Damage);
            LogCustomDamageStage("spell_final_display", Attacker, Victim, AsInt64ToUInt32Clamp(Damage), finalDamage, CastId);
            SendPlayerDisplayDamage(player, finalDamage, Critical, schoolMask, CastId);

            if (Victim->ToCreature())
                sPlayerModMgr->DmgRankOnCreatureDamageTaken(Victim->ToCreature(), player, finalDamage);
        }

        if (Player* victimPlayer = Victim->ToPlayer())
        {
            if (victimPlayer != player)
                SendPlayerIncomingDamage(victimPlayer, Attacker, finalDamage, schoolMask, CastId);
        }
    }

    void ModifyHealRecieved(Unit* Healer, Unit* Reciever, uint32 CastId, uint32& AddHealth) override
    {
        if (!Healer || !Reciever || !AddHealth || !Healer->IsInWorld() || !Reciever->IsInWorld())
            return;

        AsInt64 RealHeal = Healer->PeekAsHealOverride(Reciever, AddHealth);

        if (Player* Aplayer = Healer->ToPlayer())
        {
            RealHeal = ScaleAsInt64Value(RealHeal, sCustomClass->GetHeModForClass(Aplayer));

            if (!IsPvpHealingContext(Healer, Reciever))
            {
                RealHeal = ScaleAsInt64Value(RealHeal, sAuraSpellMgr->AutoPVEHeMod(Aplayer, CastId));
                ApplyItemCombatBonus(RealHeal, Aplayer, 305, 304);
            }
            else
            {
                RealHeal = ScaleAsInt64Value(RealHeal, sAuraSpellMgr->AutoPVPHeMod(Aplayer, CastId));
                ApplyItemCombatBonus(RealHeal, Aplayer, 204, 205);
            }
        }

        if (Player* targetPlayer = Reciever->ToPlayer())
        {
            if (IsPvpHealingContext(Healer, Reciever))
                ApplyItemCombatBonus(RealHeal, targetPlayer, 210, 211);
            else
                ApplyItemCombatBonus(RealHeal, targetPlayer, 311, 310);
        }

        RealHeal = ScaleAsInt64Value(RealHeal, sAuraSpellMgr->GetXhHealMod(CastId));

        if (Player* Aplayer = Healer->ToPlayer())
        {
            AsInt64 Max = sCustomClass->GetAsHeMax(Aplayer);
            if (IsLegacyDamageLimitValue(Max))
                Max = AsInt64Zero();

            AsInt64 Now = sCustomClass->GetAsHeNow(Aplayer);
            if (IsLegacyDamageLimitValue(Now))
                Now = AsInt64Zero();

            if (Now > 0)
            {
                Max = Now;
                RealHeal = Now;
            }

            if (Max > 0)
                RealHeal = RealHeal > Max ? Max : RealHeal;

            AddHealth = AsInt64ToUInt32Clamp(RealHeal, uint32(std::numeric_limits<int32>::max()));
            Healer->SetAsHealOverride(Reciever->GetGUID(), RealHeal);

            if (Aplayer && Aplayer->InEvent())
                Aplayer->UpdateEventHeal(Reciever, RealHeal);

            if (Aplayer && Reciever->ToPlayer() && sGuildBattleMgr->InStart)
                sGuildBattleMgr->GuildUpdateEventHeal(Aplayer, Reciever->ToPlayer(), RealHeal);

            if (Aplayer && Reciever->ToPlayer() && sAsPlayerBattleMgr->Started)
                sAsPlayerBattleMgr->AsPlayerUpdateEventHeal(Aplayer, Reciever->ToPlayer(), RealHeal);

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(CastId);
            uint32 schoolMask = spellInfo ? uint32(spellInfo->GetSchoolMask()) : uint32(SPELL_SCHOOL_MASK_HOLY);
            SendPlayerDisplayHeal(Aplayer, RealHeal, schoolMask, CastId);
        }
        else
        {
            AddHealth = AsInt64ToUInt32Clamp(RealHeal, uint32(std::numeric_limits<int32>::max()));
            Healer->SetAsHealOverride(Reciever->GetGUID(), RealHeal);
        }
    }
};

class GItemLootOnScript : public GlobalScript
{
public:
    GItemLootOnScript() : GlobalScript("GItemLootOnScript") {}

    bool OnItemRoll(Player const* player, LootStoreItem const* StoreItem, float& Chance, Loot& OnLoot, LootStore const& Store) override
    {
        float SChance = Chance;
        float AsRate = player->VLootRate + (player->VLootKn * SChance) + sAuraSpellMgr->GetLootOddsMod(const_cast<Player*>(player));
        AsInt64 itemLootChance = player->GetAsStatBonus(static_cast<ItemModType>(401));
        AsRate += float(AsInt64ToInt32Clamp(itemLootChance, 1000000));
        Chance += AsRate;
        sAuraSpellMgr->AutoLootOddsKn(const_cast<Player*>(player), Chance);

        if (sHelper->GetBoolean(9600) && player->GetSession()->GetSecurity() > SEC_PLAYER)
            const_cast<Player*>(player)->SendSysMsg("[调试]: %s 原始掉率: %.2f 最终掉率: %.2f", sHelper->GetItemLink(StoreItem->itemid).c_str(), SChance, Chance);

        return true;
    }

    void OnAfterRefCount(Player const* player, LootStoreItem*, Loot&, bool, uint16, uint32& maxcount, LootStore const&) override
    {
        if (!player || !maxcount)
            return;

        uint32 chance = AsInt64ToUInt32Clamp(player->GetAsStatBonus(static_cast<ItemModType>(403)), 100);
        uint32 multiplier = AsInt64ToUInt32Clamp(player->GetAsStatBonus(static_cast<ItemModType>(402)), 1000);
        if (chance && multiplier > 1 && roll_chance_i(chance))
            maxcount = uint32(std::min<uint64>(uint64(maxcount) * multiplier, std::numeric_limits<uint32>::max()));
    }
};

void AddSC_HelperScripts()
{
    new StartedWorldScript();
    new CustomDamageScript();
    new GItemLootOnScript();
}
