#include "AsInt256.h"
#include "StringConvert.h"
#include "ItemIdentificationSystem.h"

template <typename... Args>
static void ItemIdentificationTraceLog(Args&&...)
{
}
#include "Define.h"
#include <limits>
#include "ScriptMgr.h"
#include "Player.h"
#include "Item.h"
#include "Config.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "StringConvert.h"
#include "Util.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "WorldSession.h"
#include "ScriptedGossip.h"
#include "World.h"
#include "WorldSessionMgr.h"
#include "../../../src/server/scripts/Custom/AddonActive/DoAddon.h"
#include "../../../src/server/scripts/Custom/AddonActive/FrozenCoreModuleBridge.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include "../../../src/server/scripts/Custom/ItemFuncMgr/ItemFuncMgr.h"
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <random>
#include <cstdarg>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <set>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <array>
#include <functional>
#include <optional>

#define MODULE_ITEM_GROWTH
#include "ItemGrowthMgr.h"
#include "ItemGrowthCommands.h"

#define MODULE_ITEM_ENHANCEMENT
#include "ItemEnhancementMgr.h"

#define MODULE_ITEM_ATTRIBUTES
#include "ItemAttributesGenerator.h"
#include "ItemAttributesDBHelper.h"
#include "ItemAttributesLoader.h"
#include "ItemAttributesEffects.h"

#define MODULE_RUNE_SYSTEM
#include "RuneManager.h"

#define MODULE_ITEM_SKILLS
#include "ItemSkillsManager.h"
#include "ItemSkillsDBHelper.h"
#include "ItemSkillsEffects.h"

#define MODULE_ITEM_SETS
#include "ItemSets.h"
#include "ModuleFormat.h"

namespace
{
    constexpr char const* ITEM_IDENTIFICATION_ADDON_PREFIX = "UITQ";
    constexpr uint32 ITEM_REFRESH_DEDUP_MS = 1000;

    struct AddonQueryRateState
    {
        uint32 windowStartMs = 0;
        uint32 requestCount = 0;
        uint32 lastSeenMs = 0;
    };

    std::mutex s_addonQueryRateMutex;
    std::unordered_map<uint32, AddonQueryRateState> s_addonQueryRates;
    std::mutex s_itemRefreshDedupMutex;
    std::unordered_map<uint32, uint32> s_lastQueuedItemRefreshMs;
    std::mutex s_inspectQueryGrantMutex;
    std::unordered_map<uint64, uint32> s_inspectQueryGrants;

    uint64 BuildInspectQueryGrantKey(uint32 playerGuid, uint32 itemGuid)
    {
        return (uint64(playerGuid) << 32) | itemGuid;
    }

    void GrantInspectItemQuery(Player* requester, uint32 itemGuid)
    {
        if (!requester || itemGuid == 0)
            return;

        std::lock_guard<std::mutex> lock(s_inspectQueryGrantMutex);
        s_inspectQueryGrants[BuildInspectQueryGrantKey(requester->GetGUID().GetCounter(), itemGuid)] = getMSTime() + 10000;
    }

    bool HasInspectItemQueryGrant(Player* requester, uint32 itemGuid)
    {
        if (!requester || itemGuid == 0)
            return false;

        uint32 const now = getMSTime();
        std::lock_guard<std::mutex> lock(s_inspectQueryGrantMutex);
        auto itr = s_inspectQueryGrants.find(BuildInspectQueryGrantKey(requester->GetGUID().GetCounter(), itemGuid));
        if (itr == s_inspectQueryGrants.end())
            return false;

        if (now >= itr->second)
        {
            s_inspectQueryGrants.erase(itr);
            return false;
        }

        return true;
    }

    bool AllowItemIdentificationAddonRequest(Player* player, uint32 maxRequestsPerSecond)
    {
        if (!player || maxRequestsPerSecond == 0)
            return false;

        uint32 const now = getMSTime();
        uint32 const playerGuid = player->GetGUID().GetCounter();
        std::lock_guard<std::mutex> lock(s_addonQueryRateMutex);

        AddonQueryRateState& state = s_addonQueryRates[playerGuid];
        if (state.windowStartMs == 0 || now - state.windowStartMs >= IN_MILLISECONDS)
        {
            state.windowStartMs = now;
            state.requestCount = 0;
        }

        state.lastSeenMs = now;
        if (++state.requestCount > maxRequestsPerSecond)
            return false;

        if (s_addonQueryRates.size() > 2048)
        {
            for (auto itr = s_addonQueryRates.begin(); itr != s_addonQueryRates.end();)
            {
                if (now - itr->second.lastSeenMs > 60000)
                    itr = s_addonQueryRates.erase(itr);
                else
                    ++itr;
            }
        }

        return true;
    }

    void SendItemIdentificationAddonMessage(Player* player, std::string const& payload)
    {
        if (!player)
            return;

        // The aggregated tooltip payload is far larger than the addon-message
        // limit. Native clients also get it through kind=5 (UITQ_DATA); the
        // chat packet stays for older clients.
        if (player->GetSession() && player->GetSession()->HasNativeUI()
            && payload.rfind("ALL_MODULE_DATA:", 0) == 0)
        {
            WorldPacket packet(SMSG_NATIVE_UI_SNAPSHOT);
            packet << uint16(1);
            packet << uint8(5);
            packet << uint64(player->GetGUID().GetRawValue());
            packet << uint16(1);

            constexpr char key[] = "UITQ_DATA";
            packet << uint8(sizeof(key) - 1);
            packet.append(key, sizeof(key) - 1);
            packet << uint8(1);

            uint16 length = payload.size() > 65535 ? 65535 : static_cast<uint16>(payload.size());
            packet << length;
            packet.append(payload.data(), length);
            player->SendDirectMessage(&packet);
        }

        std::string fullMessage = std::string(ITEM_IDENTIFICATION_ADDON_PREFIX) + "\t" + payload;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);
    }

    bool HasNeedTemplate(uint32 requirementId)
    {
        return !requirementId || NeedTempStore.find(requirementId) != NeedTempStore.end();
    }

    bool ShowNeedTemplate(Player* player, uint32 requirementId)
    {
        if (!requirementId)
            return true;

        if (!HasNeedTemplate(requirementId))
        {
            if (player)
                ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("需求模板%s不存在，请检查 __模版需求总表。", requirementId).c_str());
            return false;
        }

        return sHelper->NotDeleteCheckNeed(player, requirementId);
    }

    bool PayItemOperation(Player* player, Item* item, std::vector<uint32> const& requirements,
        uint64 reservedGold, bool consume)
    {
        if (!player || !item || item->GetOwnerGUID() != player->GetGUID() || item->IsInTrade())
            return false;

        std::map<uint32, AsInt64> materials;
        AsInt64 money = 0;
        AsInt64 honor = 0;
        AsInt64 arena = 0;
        AsInt64 tokens = 0;
        AsInt64 magicTokens = 0;
        AsInt64 points = 0;
        for (uint32 id : requirements)
        {
            if (!id)
                continue;
            if (!ShowNeedTemplate(player, id))
                return false;
            auto const& need = NeedTempStore.at(id);
            for (size_t index = 0; index < MNC; ++index)
                if (need.ItemId[index] && need.Count[index] > 0)
                {
                    // DestroyItemCount 按物品ID删除，不能保证跳过目标装备。
                    if (need.ItemId[index] == item->GetEntry() ||
                        !sObjectMgr->GetItemTemplate(need.ItemId[index]))
                        return false;
                    materials[need.ItemId[index]] += need.Count[index];
                }
            if (need.Money > 0)
                money += AsInt64(need.Money) * GOLD;
            if (need.Hr > 0)
                honor += need.Hr;
            if (need.An > 0)
                arena += need.An;
            if (need.Token > 0)
                tokens += need.Token;
            if (need.Mobi > 0)
                magicTokens += need.Mobi;
            if (need.Paodian > 0)
                points += need.Paodian;
        }
        for (auto const& [entry, count] : materials)
            if (count > std::numeric_limits<uint32>::max() || player->GetItemCount(entry) < count)
                return false;

        if (money + reservedGold > AsInt64(player->GetMoney()) ||
            money > std::numeric_limits<int64>::max() ||
            honor > AsInt64(player->GetHonorPoints()) || honor > std::numeric_limits<int32>::max() ||
            arena > AsInt64(player->GetArenaPoints()) || arena > std::numeric_limits<int32>::max() ||
            tokens > AsInt64(player->GetToken()) || tokens > std::numeric_limits<int64>::max() ||
            magicTokens > AsInt64(player->GetTokenMB()) || magicTokens > std::numeric_limits<int64>::max() ||
            points > AsInt64(player->GetPaodian()) || points > std::numeric_limits<int64>::max())
            return false;
        if (!consume)
            return true;

        // 同一次同步调用中先检查全部条件及合计消耗，再统一扣除；不逐模板边扣边检查。
        if (money > 0 && !player->DebitMoney(static_cast<uint64>(money)))
            return false;
        for (auto const& [entry, count] : materials)
            player->DestroyItemCount(entry, static_cast<uint32>(count), true);
        if (honor > 0)
            player->ModifyHonorPoints(-static_cast<int32>(honor));
        if (arena > 0)
            player->ModifyArenaPoints(-static_cast<int32>(arena));
        if (tokens > 0)
            player->AddToken(-static_cast<int64>(tokens), "物品鉴定/洗炼消耗");
        if (magicTokens > 0)
            player->AddTokenMB(-static_cast<int64>(magicTokens), "物品鉴定/洗炼消耗");
        if (points > 0)
            player->AddPaodian(-static_cast<int64>(points), "物品鉴定/洗炼消耗");
        return true;
    }

    char NormalizeHuanJingMode(char mode)
    {
        if (mode == '+' || mode == 1)
            return '+';
        if (mode == '-' || mode == 'n')
            return '-';
        return 'x';
    }

    char DbValueToHuanJingMode(int32 value)
    {
        if (value == 1)
            return '+';
        if (value == -1)
            return '-';
        return 'x';
    }

    bool HasHuanJingEffect(uint32 value, char mode)
    {
        mode = NormalizeHuanJingMode(mode);
        if (mode == '-')
            return false;
        return mode == '+' ? value > 0 : value > 1;
    }

    std::string FormatTemplateDouble(double value)
    {
        std::ostringstream oss;
        oss << std::setprecision(15) << value;
        return oss.str();
    }

    std::string BuildTemplateStatsData(uint32 itemID)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemID);
        if (!proto)
            return "";

        std::ostringstream stats;
        bool hasStats = false;

        for (uint32 i = 0; i < proto->StatsCount && i < MAX_ITEM_PROTO_STATS; ++i)
        {
            AsInt64 statValue = proto->AsItemStatValue[i];
            if (!statValue)
                continue;

            if (hasStats)
                stats << ",";

            hasStats = true;
            stats << proto->ItemStat[i].ItemStatType << " " << Acore::ToString(statValue);
        }

        std::ostringstream damage;
        bool hasDamage = false;

        for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            bool hasWideDamage = proto->AsDamageMin[i] != 0 || proto->AsDamageMax[i] != 0;
            if (!hasWideDamage && proto->Damage[i].DamageMin == 0.0 && proto->Damage[i].DamageMax == 0.0)
                continue;

            if (hasDamage)
                damage << ",";

            hasDamage = true;
            if (hasWideDamage)
            {
                damage << Acore::ToString(proto->AsDamageMin[i]) << " "
                       << Acore::ToString(proto->AsDamageMax[i]) << " ";
            }
            else
            {
                damage << FormatTemplateDouble(proto->Damage[i].DamageMin) << " "
                       << FormatTemplateDouble(proto->Damage[i].DamageMax) << " ";
            }
            damage << proto->Damage[i].DamageType;
        }

        if (!hasStats && proto->AsArmor == 0 && !hasDamage)
            return "";

        std::ostringstream data;
        data << "TPL64|" << stats.str() << "|";
        if (proto->AsArmor != 0)
            data << Acore::ToString(proto->AsArmor);
        data << "|" << damage.str();

        return data.str();
    }

    struct ClientItemLocation
    {
        int32 bag = 0;
        uint8 slot = 0;
        bool valid = false;
    };

    ClientItemLocation GetClientItemLocation(Item* item)
    {
        ClientItemLocation location;
        if (!item)
            return location;

        uint8 serverBag = item->GetBagSlot();
        uint8 serverSlot = item->GetSlot();

        if (serverBag == INVENTORY_SLOT_BAG_0)
        {
            if (serverSlot >= EQUIPMENT_SLOT_START && serverSlot < EQUIPMENT_SLOT_END)
            {
                location.bag = 255;
                location.slot = static_cast<uint8>(serverSlot + 1);
                location.valid = true;
            }
            else if (serverSlot >= INVENTORY_SLOT_ITEM_START && serverSlot < INVENTORY_SLOT_ITEM_END)
            {
                location.bag = 0;
                location.slot = static_cast<uint8>(serverSlot - INVENTORY_SLOT_ITEM_START + 1);
                location.valid = true;
            }
        }
        else if (serverBag >= INVENTORY_SLOT_BAG_START && serverBag < INVENTORY_SLOT_BAG_END)
        {
            location.bag = static_cast<int32>(serverBag - INVENTORY_SLOT_BAG_START + 1);
            location.slot = static_cast<uint8>(serverSlot + 1);
            location.valid = true;
        }

        return location;
    }

    Item* FindPlayerItemByGuidForAddon(Player* player, uint32 itemGuid)
    {
        if (!player || itemGuid == 0)
            return nullptr;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (item && item->GetGUID().GetCounter() == itemGuid)
                return item;
        }

        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (item && item->GetGUID().GetCounter() == itemGuid)
                return item;
        }

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        {
            Bag* bag = player->GetBagByPos(bagSlot);
            if (!bag)
                continue;

            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            {
                Item* item = bag->GetItemByPos(slot);
                if (item && item->GetGUID().GetCounter() == itemGuid)
                    return item;
            }
        }

        return nullptr;
    }

    std::unordered_map<uint32, Item*> BuildPlayerItemGuidIndex(Player* player)
    {
        std::unordered_map<uint32, Item*> itemsByGuid;
        if (!player)
            return itemsByGuid;

        itemsByGuid.reserve(128);

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                itemsByGuid[item->GetGUID().GetCounter()] = item;
        }

        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                itemsByGuid[item->GetGUID().GetCounter()] = item;
        }

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        {
            Bag* bag = player->GetBagByPos(bagSlot);
            if (!bag)
                continue;

            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            {
                if (Item* item = bag->GetItemByPos(slot))
                    itemsByGuid[item->GetGUID().GetCounter()] = item;
            }
        }

        return itemsByGuid;
    }

    void QueueAllModuleDataAddonRefresh(Player* player, uint32 itemID, uint32 guid, Milliseconds delay)
    {
        if (!player || itemID == 0 || guid == 0)
            return;

        uint32 const now = getMSTime();
        {
            std::lock_guard<std::mutex> lock(s_itemRefreshDedupMutex);
            auto itr = s_lastQueuedItemRefreshMs.find(guid);
            if (itr != s_lastQueuedItemRefreshMs.end() && now - itr->second < ITEM_REFRESH_DEDUP_MS)
                return;

            s_lastQueuedItemRefreshMs[guid] = now;
            if (s_lastQueuedItemRefreshMs.size() > 4096)
            {
                for (auto sweepItr = s_lastQueuedItemRefreshMs.begin(); sweepItr != s_lastQueuedItemRefreshMs.end();)
                {
                    if (now - sweepItr->second > 10000)
                        sweepItr = s_lastQueuedItemRefreshMs.erase(sweepItr);
                    else
                        ++sweepItr;
                }
            }
        }

        ObjectGuid playerGuid = player->GetGUID();
        player->m_Events.AddEventAtOffset([playerGuid, itemID, guid]()
        {
            Player* player = ObjectAccessor::FindPlayer(playerGuid);
            if (player && player->IsInWorld())
                sItemIdentificationSystem->SendAllModuleDataAddon(player, itemID, guid);
        }, delay);
    }
}

// 前向声明辅助函数
std::vector<uint32> ParseCommaSeparatedNumbers(const std::string& str);
uint32 SelectRandomFromList(const std::vector<uint32>& list);
uint32 GenerateRandomNumber(uint32 min, uint32 max);
uint64 GenerateRandomUInt64(uint64 min, uint64 max);
uint32 GetIdentificationTemplateMatchLevel(Item* item);

// 单例实例
ItemIdentificationSystem* ItemIdentificationSystem::_instance = nullptr;

// 获取单例实例
ItemIdentificationSystem* ItemIdentificationSystem::instance()
{
    if (!_instance)
        _instance = new ItemIdentificationSystem();
    return _instance;
}

// 初始化系统
void ItemIdentificationSystem::Initialize()
{
    PerformanceTimer timer("模块初始化");

    // 加载配置（在服务器完全启动后执行）
    LoadConfig(false);

    if (!_enabled)
    {
        // 禁用状态也是有效状态。把缓存标记为空且已初始化，避免装备加载钩子
        // 对每件物品重复输出“缓存未初始化”错误。
        std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
        _identifiedItemsCache.clear();
        _cacheInitialized = true;
        LOG_INFO("server.loading", "物品鉴定系统: 已禁用");
        return;
    }

    // 初始化已鉴定物品缓存和鉴定模板
    InitializeCache();
    LoadIdentificationTemplates();
}

// 加载配置
void ItemIdentificationSystem::LoadConfig(bool reload)
{
    _enabled = sConfigMgr->GetOption<bool>("ItemIdentificationSystem.Enable", true);
    _enabled = _enabled && sHelper->GetBoolean(22011);
    _baseSuccessRate = sConfigMgr->GetOption<uint32>("ItemIdentificationSystem.BaseSuccessRate", 100);
    _destroyOnFail = sConfigMgr->GetOption<bool>("ItemIdentificationSystem.DestroyOnFail", false);
    _cost = sConfigMgr->GetOption<uint64>("ItemIdentificationSystem.Cost", 10000);
    _enableAnnounce = sConfigMgr->GetOption<bool>("ItemIdentificationSystem.EnableAnnounce", true);
    _debugMode = sConfigMgr->GetOption<bool>("ItemIdentificationSystem.Debug", false);
    _addonQueriesPerSecond = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("ItemIdentificationSystem.AddonQueriesPerSecond", 20));
    _batchCacheMaxEntries = std::max<size_t>(1000, sConfigMgr->GetOption<uint32>("ItemIdentificationSystem.BatchCacheMaxEntries", 50000));

    if (reload)
    {
        if (!_enabled)
        {
            return;
        }

        LoadIdentificationTemplates();
#ifdef MODULE_ITEM_ATTRIBUTES
        sItemAttributesLoaderUnsafe->LoadItemAttributeTemplates();
#endif
        LOG_INFO("server.loading", "物品鉴定系统配置已重新加载");
    }
}

// 鉴定模板结构
struct IdentificationTemplate
{
    uint32 id;
    uint32 group;
    uint32 level;
    uint32 randomChance;
    std::string comment;

    // 模块关联字段
    std::string itemGrowthGroups;
    uint32 growthAttrMinCount;
    uint32 growthAttrMaxCount;
    AsInt64 growthAttrMinValue;
    AsInt64 growthAttrMaxValue;

    std::string itemEnhancementGroups;
    uint32 enhancementAttrMinCount;
    uint32 enhancementAttrMaxCount;
    AsInt64 enhancementAttrMinValue;
    AsInt64 enhancementAttrMaxValue;

    std::string itemAttributesGroups;
    std::string itemAttributesAdditionalGroups;
    std::string itemSkillsGroups;
    std::string runeSystemGroups;
    std::string skillSetGroups;
    uint32 skillSetMinCount;
    uint32 skillSetMaxCount;
    bool skillSetAllowDuplicate;
    uint32 requirementTemplate;

    // 基础属性配置
    uint32 baseAttrMinCount;
    uint32 baseAttrMaxCount;
    AsInt64 baseAttrMinValue;
    AsInt64 baseAttrMaxValue;
    bool baseAttrAllowDuplicate;

    // 追加属性配置
    uint32 additionalAttrMinCount;
    uint32 additionalAttrMaxCount;
    AsInt64 additionalAttrMinValue;
    AsInt64 additionalAttrMaxValue;
    bool additionalAttrAllowDuplicate;

    // 追加技能配置
    uint32 additionalSkillMinCount;
    uint32 additionalSkillMaxCount;
    bool additionalSkillAllowDuplicate;

    // 符文配置
    uint32 runeSlotMinCount;
    uint32 runeSlotMaxCount;

    uint32 announcementTemplate;
    std::string qualityColor;
    std::string itemNamePrefix;
    std::string itemNameSuffix;
    std::string itemNameColors;
    std::string itemBottomDescription;
    uint32 recycleRewardTemplate;
    uint32 sellRewardTemplate;
    uint32 enchantGroup;
    uint32 enchantCount;
    std::string inventoryTypeLimit;
};

// 存储所有鉴定模板
std::map<uint32, IdentificationTemplate> _identificationTemplates;

struct IdentificationEnchantEntry
{
    uint32 enchantId;
    float chance;
};

static std::unordered_map<uint32, std::vector<IdentificationEnchantEntry>> _identificationEnchantGroups;

static void SyncIdentificationEnchantData(Player* player, Item* item)
{
    if (!player || !item)
        return;

    uint32 enchantIds[5] = {};
    std::string payload = Acore::ToString(item->GetGUID().GetCounter()) + "$";
    for (uint8 slot = 0; slot < 5; ++slot)
    {
        enchantIds[slot] = item->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + slot));
        payload += Acore::ToString(enchantIds[slot]) + "#";
    }

    EnchantAtStore[item->GetGUID().GetCounter()] =
        { enchantIds[0], enchantIds[1], enchantIds[2], enchantIds[3], enchantIds[4] };
    sDoAddon->SendPacketToClient(player, "AutoEnchant", payload);
}

namespace
{
    struct ItemEnchantSnapshot
    {
        explicit ItemEnchantSnapshot(Item* item)
        {
            for (uint8 index = 0; index < values.size(); ++index)
            {
                EnchantmentSlot slot = EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + index);
                values[index] = {item->GetEnchantmentId(slot), item->GetEnchantmentDuration(slot),
                    item->GetEnchantmentCharges(slot)};
            }
        }

        void Restore(Player* player, Item* item, bool effectsApplied) const
        {
            for (uint8 index = 0; index < values.size(); ++index)
            {
                EnchantmentSlot slot = EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + index);
                if (effectsApplied && item->IsEquipped())
                    player->ApplyEnchantment(item, slot, false);
                item->SetEnchantment(slot, values[index][0], values[index][1], values[index][2]);
                if (effectsApplied && item->IsEquipped())
                    player->ApplyEnchantment(item, slot, true);
            }
            SyncIdentificationEnchantData(player, item);
        }

        std::array<std::array<uint32, 3>, 5> values{};
    };

    bool VerifyItemAttributes(uint32 itemGuid)
    {
        auto expected = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);
        ItemAttributesDBHelper::EraseCache(itemGuid);
        auto saved = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);
        if (!expected || !saved)
            return false;
        return expected->baseAttributeIds == saved->baseAttributeIds &&
            expected->baseAttributeValues == saved->baseAttributeValues &&
            expected->additionalAttributeIds == saved->additionalAttributeIds &&
            expected->additionalAttributeValues == saved->additionalAttributeValues;
    }

    bool SaveOperationItemState(Item* item)
    {
        std::ostringstream charges;
        for (uint8 index = 0; index < MAX_ITEM_PROTO_SPELLS; ++index)
            charges << item->GetSpellCharges(index) << ' ';
        std::ostringstream enchants;
        for (uint8 index = 0; index < MAX_ENCHANTMENT_SLOT; ++index)
            enchants << item->GetEnchantmentId(EnchantmentSlot(index)) << ' '
                << item->GetEnchantmentDuration(EnchantmentSlot(index)) << ' '
                << item->GetEnchantmentCharges(EnchantmentSlot(index)) << ' ';
        auto* save = CharacterDatabase.GetPreparedStatement(CHAR_IDENT_ITEM_SAVE);
        save->SetArguments(item->GetGUIDLow(), item->GetEntry(), item->GetOwnerGUID().GetCounter(),
            item->GetGuidValue(ITEM_FIELD_CREATOR).GetCounter(),
            item->GetGuidValue(ITEM_FIELD_GIFTCREATOR).GetCounter(), item->GetCount(),
            item->GetUInt32Value(ITEM_FIELD_DURATION), charges.str(), item->GetUInt32Value(ITEM_FIELD_FLAGS),
            enchants.str(), item->GetItemRandomPropertyId(), item->GetUInt32Value(ITEM_FIELD_DURABILITY),
            item->GetUInt32Value(ITEM_FIELD_CREATE_PLAYED_TIME), item->GetText());
        CharacterDatabase.DirectExecute(save);
        auto* verify = CharacterDatabase.GetPreparedStatement(CHAR_IDENT_ITEM_VERIFY);
        verify->SetData(0, item->GetGUIDLow());
        PreparedQueryResult stored = CharacterDatabase.Query(verify);
        return stored && stored->Fetch()[0].Get<uint32>() == item->GetUInt32Value(ITEM_FIELD_FLAGS) &&
            stored->Fetch()[1].Get<std::string>() == enchants.str();
    }

    struct ItemSnapshotStatements
    {
        char const* table;
        CharacterDatabaseStatements count;
        CharacterDatabaseStatements select;
        CharacterDatabaseStatements erase;
        CharacterDatabaseStatements insert;
    };

    ItemSnapshotStatements const SnapshotTables[] =
    {
        {"item_instance", CHAR_IDENT_ITEM_COUNT, CHAR_IDENT_ITEM_SELECT,
            CHAR_IDENT_ITEM_DELETE, CHAR_IDENT_ITEM_INSERT},
        {"物品_鉴定记录", CHAR_IDENT_RECORD_COUNT, CHAR_IDENT_RECORD_SELECT,
            CHAR_IDENT_RECORD_DELETE, CHAR_IDENT_RECORD_INSERT},
        {"物品属性_数据", CHAR_IDENT_ATTRIBUTES_COUNT, CHAR_IDENT_ATTRIBUTES_SELECT,
            CHAR_IDENT_ATTRIBUTES_DELETE, CHAR_IDENT_ATTRIBUTES_INSERT},
        {"_物品技能_数据", CHAR_IDENT_SKILLS_COUNT, CHAR_IDENT_SKILLS_SELECT,
            CHAR_IDENT_SKILLS_DELETE, CHAR_IDENT_SKILLS_INSERT},
        {"物品成长_玩家记录", CHAR_IDENT_GROWTH_COUNT, CHAR_IDENT_GROWTH_SELECT,
            CHAR_IDENT_GROWTH_DELETE, CHAR_IDENT_GROWTH_INSERT},
        {"物品强化_记录", CHAR_IDENT_ENHANCEMENT_COUNT, CHAR_IDENT_ENHANCEMENT_SELECT,
            CHAR_IDENT_ENHANCEMENT_DELETE, CHAR_IDENT_ENHANCEMENT_INSERT},
        {"符文系统_数据", CHAR_IDENT_RUNES_COUNT, CHAR_IDENT_RUNES_SELECT,
            CHAR_IDENT_RUNES_DELETE, CHAR_IDENT_RUNES_INSERT},
        {"_物品套装_数据", CHAR_IDENT_SETS_COUNT, CHAR_IDENT_SETS_SELECT,
            CHAR_IDENT_SETS_DELETE, CHAR_IDENT_SETS_INSERT},
    };

    // 失败补偿只操作此物品的固定实例表；不提供进程崩溃时的跨模块原子性。
    class ItemDataRollback
    {
    public:
        explicit ItemDataRollback(uint32 itemGuid) : _itemGuid(itemGuid) { }

        bool Capture(char const* table)
        {
            auto itr = std::find_if(std::begin(SnapshotTables), std::end(SnapshotTables),
                [table](auto const& entry) { return std::string_view(entry.table) == table; });
            if (itr == std::end(SnapshotTables))
                return false;
            Snapshot snapshot{*itr, {}};
            if (!Read(snapshot))
                return false;
            _snapshots.push_back(std::move(snapshot));
            return true;
        }

        void Arm(std::function<void()> restoreMemory)
        {
            _restoreMemory = std::move(restoreMemory);
            _armed = true;
        }

        void Commit() { _armed = false; }

        ~ItemDataRollback()
        {
            if (!_armed)
                return;
            bool restored = false;
            try
            {
                auto transaction = CharacterDatabase.BeginTransaction();
                for (Snapshot const& snapshot : _snapshots)
                {
                    auto* erase = CharacterDatabase.GetPreparedStatement(snapshot.statements.erase);
                    erase->SetData(0, _itemGuid);
                    transaction->Append(erase);
                    for (auto const& row : snapshot.rows)
                    {
                        auto* insert = CharacterDatabase.GetPreparedStatement(snapshot.statements.insert);
                        for (uint8 index = 0; index < row.size(); ++index)
                        {
                            if (row[index])
                                insert->SetData(index, *row[index]);
                            else
                                insert->SetData(index, nullptr);
                        }
                        transaction->Append(insert);
                    }
                }
                CharacterDatabase.DirectCommitTransaction(transaction);
                restored = true;
                for (Snapshot const& snapshot : _snapshots)
                {
                    Snapshot check{snapshot.statements, {}};
                    if (!Read(check) || check.rows != snapshot.rows)
                    {
                        restored = false;
                        break;
                    }
                }
                _restoreMemory();
            }
            catch (std::exception const& error)
            {
                restored = false;
                LOG_ERROR("module.itemidentification", "物品 {} 恢复发生异常: {}", _itemGuid, error.what());
            }
            if (restored)
                LOG_WARN("module.itemidentification", "物品 {} 操作失败，原数据恢复校验通过", _itemGuid);
            else
                LOG_ERROR("module.itemidentification",
                    "物品 {} 操作失败，数据库恢复未确认，请管理员检查此物品各模块记录", _itemGuid);
        }

    private:
        struct Snapshot
        {
            ItemSnapshotStatements statements;
            std::vector<std::vector<std::optional<std::string>>> rows;
        };

        bool Read(Snapshot& snapshot) const
        {
            auto* countStatement = CharacterDatabase.GetPreparedStatement(snapshot.statements.count);
            countStatement->SetData(0, _itemGuid);
            PreparedQueryResult count = CharacterDatabase.Query(countStatement);
            if (!count)
                return false;
            uint64 rowCount = count->Fetch()[0].Get<uint64>();
            if (!rowCount)
                return true;
            auto* select = CharacterDatabase.GetPreparedStatement(snapshot.statements.select);
            select->SetData(0, _itemGuid);
            PreparedQueryResult rows = CharacterDatabase.Query(select);
            if (!rows || rows->GetRowCount() != rowCount)
                return false;
            do
            {
                std::vector<std::optional<std::string>> row;
                for (uint32 index = 0; index < rows->GetFieldCount(); ++index)
                {
                    Field const& field = rows->Fetch()[index];
                    row.push_back(field.IsNull() ? std::nullopt : std::make_optional(field.Get<std::string>()));
                }
                snapshot.rows.push_back(std::move(row));
            } while (rows->NextRow());
            std::sort(snapshot.rows.begin(), snapshot.rows.end());
            return true;
        }

        uint32 _itemGuid;
        bool _armed = false;
        std::vector<Snapshot> _snapshots;
        std::function<void()> _restoreMemory;
    };
}

static bool RerollIdentificationEnchants(Player* player, Item* item, IdentificationTemplate const& tmpl, std::string& error)
{
    if (!player || !item || !tmpl.enchantGroup || !tmpl.enchantCount)
    {
        error = "鉴定模板未配置随机附魔";
        return false;
    }

    auto enchantItr = _identificationEnchantGroups.find(tmpl.enchantGroup);
    if (enchantItr == _identificationEnchantGroups.end())
    {
        error = "随机附魔组不存在";
        return false;
    }

    std::vector<uint32> candidates;
    for (IdentificationEnchantEntry const& entry : enchantItr->second)
    {
        if (entry.enchantId && sSpellItemEnchantmentStore.LookupEntry(entry.enchantId) &&
            (entry.chance >= 100.0f || roll_chance_f(entry.chance)) &&
            std::find(candidates.begin(), candidates.end(), entry.enchantId) == candidates.end())
            candidates.push_back(entry.enchantId);
    }

    if (candidates.empty())
    {
        error = "随机附魔组没有可用附魔";
        return false;
    }

    std::shuffle(candidates.begin(), candidates.end(), sItemIdentificationSystem->GetRandomGenerator());
    uint32 applyCount = std::min<uint32>(tmpl.enchantCount, static_cast<uint32>(candidates.size()));

    bool equipped = item->IsEquipped();
    for (uint8 slot = 0; slot < 5; ++slot)
    {
        if (equipped)
            player->ApplyEnchantment(item, EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + slot), false);
        item->ClearEnchantment(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + slot));
    }

    for (uint8 slot = 0; slot < applyCount && slot < 5; ++slot)
        item->SetEnchantment(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + slot), candidates[slot], 0, 0);

    if (equipped)
        for (uint8 slot = 0; slot < applyCount && slot < 5; ++slot)
            player->ApplyEnchantment(item, EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + slot), true);

    item->SetState(ITEM_CHANGED, player);
    SyncIdentificationEnchantData(player, item);
    return true;
}

static std::string UrlEncodeAddonField(const std::string& value)
{
    static char const* hex = "0123456789ABCDEF";

    if (value.empty())
        return "";

    std::string encoded;
    encoded.reserve(value.size() * 3);

    for (unsigned char ch : value)
    {
        if ((ch >= '0' && ch <= '9') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~' || ch == ' ')
        {
            encoded.push_back(static_cast<char>(ch));
        }
        else
        {
            encoded.push_back('%');
            encoded.push_back(hex[(ch >> 4) & 0x0F]);
            encoded.push_back(hex[ch & 0x0F]);
        }
    }

    return encoded;
}

static std::string BuildIdentificationDisplayData(const IdentificationTemplate& tmpl)
{
    if (tmpl.level == 0 && tmpl.qualityColor.empty() && tmpl.itemNamePrefix.empty() && tmpl.itemNameSuffix.empty() &&
        tmpl.itemNameColors.empty() && tmpl.itemBottomDescription.empty())
    {
        return "";
    }

    std::ostringstream stream;
    stream << "IDDISP|"
           << UrlEncodeAddonField(tmpl.qualityColor) << "|"
           << UrlEncodeAddonField(tmpl.itemNamePrefix) << "|"
           << UrlEncodeAddonField(tmpl.itemNameSuffix) << "|"
           << UrlEncodeAddonField(tmpl.itemNameColors) << "|"
           << UrlEncodeAddonField(tmpl.itemBottomDescription) << "|"
           << tmpl.level;
    return stream.str();
}

// 从数据库加载鉴定模板
void ItemIdentificationSystem::LoadIdentificationTemplates()
{
    PerformanceTimer timer("加载鉴定模板");

    DebugLog("正在加载物品鉴定模板...");
    _identificationTemplates.clear();

    // 使用明确的字段名而不是SELECT *，避免字段顺序问题
    QueryResult result = WorldDatabase.Query(
        "SELECT `注释`, `id`, `组`, `等级`, `随机几率`, "
        "`物品成长_系统`, `成长属性最小数量`, `成长属性最大数量`, `成长属性最小属性值`, `成长属性最大属性值`, "
        "`物品强化_系统`, `强化属性最小数量`, `强化属性最大数量`, `强化属性最小属性值`, `强化属性最大属性值`, "
        "`物品属性_模板`, "
        "`基础属性最小数量`, `基础属性最大数量`, `基础最小属性值`, `基础最大属性值`, `基础属性允许重复`, "
        "`物品属性_模板_组`, `追加属性最小数量`, `追加属性最大数量`, `追加属性最小值`, `追加属性最大值`, `追加属性允许重复`, "
        "`物品技能_模板_组`, `追加技能最小数量`, `追加技能最大数量`, `追加技能允许重复`, "
        "`需求_模板`, `符文系统_符文`, `符文凹槽最小数量`, `符文凹槽最大数量`, `技能模板_套装_组`, "
        "`套装最小数量`, `套装最大数量`, `套装允许重复`, `公告模板`, "
        "`品质颜色`, `物品名字前缀`, `物品名字后缀`, `物品名字颜色_多个逗号隔开`, `物品底部描述`, "
        "`回收奖励模板`, `售卖奖励模板`, `随机附魔组`, `随机附魔条数`, `装备位置限制` "
        "FROM `_模板_物品鉴定`");

    if (!result)
    {
       // LOG_WARN("server.loading", "物品鉴定系统: 未找到鉴定模板数据");
        return;
    }

    uint32 count = 0;
    std::set<uint32> groups;

    do
    {
        Field* fields = result->Fetch();
        IdentificationTemplate tmpl;

        // 字段顺序与SELECT语句中的顺序完全一致
        // 0:注释, 1:id, 2:组, 3:等级, 4:随机几率,
        // 5:物品成长_系统, 6:成长属性最小数量, 7:成长属性最大数量, 8:成长属性最小属性值, 9:成长属性最大属性值,
        // 10:物品强化_系统, 11:强化属性最小数量, 12:强化属性最大数量, 13:强化属性最小属性值, 14:强化属性最大属性值,
        // 15:物品属性_模板,
        // 16:基础属性最小数量, 17:基础属性最大数量, 18:基础最小属性值, 19:基础最大属性值, 20:基础属性允许重复,
        // 21:物品属性_模板_组, 22:追加属性最小数量, 23:追加属性最大数量, 24:追加属性最小值, 25:追加属性最大值, 26:追加属性允许重复,
        // 27:物品技能_模板_组, 28:追加技能最小数量, 29:追加技能最大数量, 30:追加技能允许重复,
        // 31:需求_模板, 32:符文系统_符文, 33:符文凹槽最小数量, 34:符文凹槽最大数量, 35:技能模板_套装_组,
        // 36:套装最小数量, 37:套装最大数量, 38:套装允许重复, 39:公告模板,
        // 40:品质颜色, 41:物品名字前缀, 42:物品名字后缀, 43:物品名字颜色_多个逗号隔开, 44:物品底部描述

        tmpl.comment = fields[0].Get<std::string>();
        tmpl.id = fields[1].Get<uint32>();
        tmpl.group = fields[2].Get<uint32>();
        tmpl.level = fields[3].Get<uint32>();
        tmpl.randomChance = fields[4].Get<uint32>();

        // 模块关联字段
        tmpl.itemGrowthGroups      = fields[5].Get<std::string>();              // 物品成长_系统
        tmpl.growthAttrMinCount    = fields[6].Get<uint32>();                   // 成长属性最小数量
        tmpl.growthAttrMaxCount    = fields[7].Get<uint32>();                   // 成长属性最大数量
        tmpl.growthAttrMinValue    = AsInt64FromString(fields[8].Get<std::string>());                  // 成长属性最小属性值
        tmpl.growthAttrMaxValue    = AsInt64FromString(fields[9].Get<std::string>());                  // 成长属性最大属性值

        tmpl.itemEnhancementGroups   = fields[10].Get<std::string>();           // 物品强化_系统
        tmpl.enhancementAttrMinCount = fields[11].Get<uint32>();                // 强化属性最小数量
        tmpl.enhancementAttrMaxCount = fields[12].Get<uint32>();                // 强化属性最大数量
        tmpl.enhancementAttrMinValue = AsInt64FromString(fields[13].Get<std::string>());               // 强化属性最小属性值
        tmpl.enhancementAttrMaxValue = AsInt64FromString(fields[14].Get<std::string>());               // 强化属性最大属性值

        tmpl.itemAttributesGroups = fields[15].Get<std::string>();              // 物品属性_模板（基础属性）

        // 基础属性配置
        tmpl.baseAttrMinCount = fields[16].Get<uint32>();                       // 基础属性最小数量
        tmpl.baseAttrMaxCount = fields[17].Get<uint32>();                       // 基础属性最大数量
        tmpl.baseAttrMinValue = AsInt64FromString(fields[18].Get<std::string>());                      // 基础最小属性值
        tmpl.baseAttrMaxValue = AsInt64FromString(fields[19].Get<std::string>());                      // 基础最大属性值
        tmpl.baseAttrAllowDuplicate = fields[20].Get<uint32>() == 0;           // 基础属性允许重复

        // 追加属性配置
        tmpl.itemAttributesAdditionalGroups = fields[21].Get<std::string>();   // 物品属性_模板_组（追加属性）
        tmpl.additionalAttrMinCount = fields[22].Get<uint32>();                // 追加属性最小数量
        tmpl.additionalAttrMaxCount = fields[23].Get<uint32>();                // 追加属性最大数量
        tmpl.additionalAttrMinValue = AsInt64FromString(fields[24].Get<std::string>());               // 追加属性最小值
        tmpl.additionalAttrMaxValue = AsInt64FromString(fields[25].Get<std::string>());               // 追加属性最大值
        tmpl.additionalAttrAllowDuplicate = fields[26].Get<uint32>() == 0;     // 追加属性允许重复

        // 追加技能配置
        tmpl.itemSkillsGroups = fields[27].Get<std::string>();                 // 物品技能_模板_组
        tmpl.additionalSkillMinCount = fields[28].Get<uint32>();               // 追加技能最小数量
        tmpl.additionalSkillMaxCount = fields[29].Get<uint32>();               // 追加技能最大数量
        tmpl.additionalSkillAllowDuplicate = fields[30].Get<uint32>() == 0;    // 追加技能允许重复

        // 其他配置
        tmpl.requirementTemplate = fields[31].Get<uint32>();                   // 需求_模板

        // 符文配置
        tmpl.runeSystemGroups = fields[32].Get<std::string>();                 // 符文系统_符文
        tmpl.runeSlotMinCount = fields[33].Get<uint32>();                      // 符文凹槽最小数量
        tmpl.runeSlotMaxCount = fields[34].Get<uint32>();                      // 符文凹槽最大数量

        // 套装配置
        tmpl.skillSetGroups = fields[35].Get<std::string>();                   // 技能模板_套装_组
        tmpl.skillSetMinCount = fields[36].Get<uint32>();                      // 套装最小数量
        tmpl.skillSetMaxCount = fields[37].Get<uint32>();                      // 套装最大数量
        tmpl.skillSetAllowDuplicate = fields[38].Get<uint32>() == 0;           // 套装允许重复

        // 公告配置
        tmpl.announcementTemplate = fields[39].Get<uint32>();                  // 公告模板

        // 名称显示配置
        tmpl.qualityColor = fields[40].Get<std::string>();                     // 品质颜色
        tmpl.itemNamePrefix = fields[41].Get<std::string>();                   // 物品名字前缀
        tmpl.itemNameSuffix = fields[42].Get<std::string>();                   // 物品名字后缀
        tmpl.itemNameColors = fields[43].Get<std::string>();                   // 物品名字颜色_多个逗号隔开
        tmpl.itemBottomDescription = fields[44].Get<std::string>();            // 物品底部描述
        tmpl.recycleRewardTemplate = fields[45].Get<uint32>();
        tmpl.sellRewardTemplate = fields[46].Get<uint32>();
        tmpl.enchantGroup = fields[47].Get<uint32>();
        tmpl.enchantCount = std::min<uint32>(fields[48].Get<uint32>(), 5);
        tmpl.inventoryTypeLimit = fields[49].Get<std::string>();

        _identificationTemplates[tmpl.id] = tmpl;
        groups.insert(tmpl.group);
        count++;

        if (_debugMode)
        {
            DebugLog(ModuleOldFormat("加载鉴定模板: ID=%s, 组=%s, 等级=%s, 注释='%s'", tmpl.id, tmpl.group, tmpl.level, tmpl.comment).c_str());
        }

    } while (result->NextRow());

    _identificationEnchantGroups.clear();
    auto* enchantStatement = WorldDatabase.GetPreparedStatement(WORLD_SEL_ITEM_IDENTIFICATION_ENCHANTS);
    PreparedQueryResult enchantResult = WorldDatabase.Query(enchantStatement);
    if (enchantResult)
    {
        do
        {
            Field* fields = enchantResult->Fetch();
            _identificationEnchantGroups[fields[0].Get<uint32>()].push_back(
                { fields[1].Get<uint32>(), static_cast<float>(fields[2].Get<int32>()) });
        } while (enchantResult->NextRow());
    }
}

// 检查物品是否可以鉴定
bool ItemIdentificationSystem::CanIdentify(Player* player, Item* item, bool sendError)
{
    if (!_enabled)
    {
        if (sendError)
            ChatHandler(player->GetSession()).PSendSysMessage("物品鉴定系统当前已禁用");
        DebugLog("CanIdentify检查失败: 系统已禁用");
        return false;
    }

    if (!player || !item)
    {
        DebugLog("CanIdentify检查失败: 玩家或物品为空");
        return false;
    }

    DebugLog(ModuleOldFormat("开始检查物品是否可鉴定: 物品ID=%s, 物品GUID=%s", item->GetEntry(), item->GetGUID().GetCounter()).c_str());

    // 检查物品是否已绑定（改为允许绑定物品鉴定）
    // if (item->IsSoulBound())
    // {
    //     LOG_INFO("module", "【鉴定系统】检查失败：物品已绑定");
    //     if (sendError)
    //         ChatHandler(player->GetSession()).SendNotification("已绑定的物品无法鉴定");
    //     return false;
    // }

    // 检查物品是否可装备
    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
    {
        DebugLog("CanIdentify检查失败: 物品模板为空");
        return false;
    }

    if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
    {
        if (sendError)
            ChatHandler(player->GetSession()).PSendSysMessage("只有武器和护甲可以鉴定");
        DebugLog(ModuleOldFormat("CanIdentify检查失败: 物品类型不符 (类型=%s)", proto->Class).c_str());
        return false;
    }

    DebugLog(ModuleOldFormat("物品类型检查通过 (类型: %s, 名称: %s)", proto->Class, proto->Name1).c_str());

    // 检查玩家金币是否足够
    uint64 playerMoney = player->GetMoney();

    if (playerMoney < _cost)
    {
        if (sendError)
            ChatHandler(player->GetSession()).PSendSysMessage("你没有足够的金币进行鉴定");
        DebugLog(ModuleOldFormat("CanIdentify检查失败: 金币不足 (拥有: %s, 需要: %s)", Acore::ToString(playerMoney), _cost).c_str());
        return false;
    }

    DebugLog(ModuleOldFormat("物品可以鉴定: 物品ID=%s, GUID=%s, 玩家金币=%s", item->GetEntry(), item->GetGUID().GetCounter(), Acore::ToString(playerMoney)).c_str());
    return true;
}

// 获取鉴定成功率
uint32 ItemIdentificationSystem::GetSuccessRate(Player* player, Item* item)
{
    // 直接使用配置文件中的基础成功率，不做任何调整
    uint32 successRate = _baseSuccessRate;

    // 【审计修复】移除强制下限1%的限制，允许配置为0%
    // 确保成功率在合理范围内（0-100%）
    if (successRate > 100)
        successRate = 100;

    return successRate;
}

// 鉴定物品（需要指定组ID）
bool ItemIdentificationSystem::IdentifyItem(Player* player, Item* item, uint32 groupId)
{
    if (!CanIdentify(player, item))
        return false;

    // 【修复】先不扣费，等所有校验和操作成功后再扣费
    // 保存当前金币数（用于验证）
    uint64 costToDeduct = _cost;

    // 计算成功率
    uint32 successRate = GetSuccessRate(player, item);

    // ✅ 使用全局随机数生成器
    std::uniform_int_distribution<uint32> dis(1, 100);
    uint32 roll = dis(GetRandomGenerator());
    bool success = roll <= successRate;

    if (success)
    {
        // 鉴定成功
        bool applyResult = ApplyIdentification(player, item, groupId, costToDeduct);

        if (applyResult)
        {
            // 【修复】应用成功后才扣除金币
            if (costToDeduct > static_cast<uint64>(std::numeric_limits<int64>::max()))
                player->SetMoney(player->GetMoney() > costToDeduct ? player->GetMoney() - costToDeduct : 0);
            else
                player->ModifyMoney(-static_cast<int64>(costToDeduct));
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("已扣除 %s 铜币用于鉴定", costToDeduct).c_str());
            ChatHandler(player->GetSession()).PSendSysMessage("物品鉴定成功！");

            // 【审计修复】实现全服公告功能
            if (_enableAnnounce)
            {
                // 获取物品模板和名称
                ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(item->GetEntry());
                if (itemTemplate)
                {
                    std::string itemName = itemTemplate->Name1;
                    std::string playerName = player->GetName();

                    // 构建公告消息
                    std::string announcement = ModuleOldFormat("|cff00ff00【鉴定公告】|r玩家 |cffffffff%s|r 成功鉴定了 |cff1eff00[%s]|r！", playerName, itemName);

                    // 发送全服公告
                    sWorld->SendServerMessage(SERVER_MSG_STRING, announcement);
                }
            }

            return true;
        }
        else
        {
            // 应用鉴定失败，【修复】不扣费
            ChatHandler(player->GetSession()).SendSysMessage("鉴定失败：无法应用鉴定效果（未扣费）");
            return false;
        }
    }
    else
    {
        // 鉴定失败（掷骰失败，这是正常游戏机制，需要扣费）
        if (costToDeduct > static_cast<uint64>(std::numeric_limits<int64>::max()))
            player->SetMoney(player->GetMoney() > costToDeduct ? player->GetMoney() - costToDeduct : 0);
        else
            player->ModifyMoney(-static_cast<int64>(costToDeduct));
        ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("已扣除 %s 铜币用于鉴定", costToDeduct).c_str());
        ChatHandler(player->GetSession()).PSendSysMessage("鉴定失败！");

        // 是否销毁物品
        if (_destroyOnFail)
        {
            player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
            ChatHandler(player->GetSession()).PSendSysMessage("物品已被销毁");
        }

        return false;
    }
}

// 根据组ID随机选择一个鉴定模板（根据几率加权）- 优化版：使用内存数据
uint32 ItemIdentificationSystem::SelectIdentificationTemplate(uint32 groupId, Item* item)
{
    // ✅ 从内存中筛选指定组ID的模板（不查询数据库）
    // 注意：这里不能用物品品质过滤模板等级，否则同品质物品会只剩一个候选模板，随机几率失效。
    std::vector<std::pair<uint32, uint32>> templates; // <模板ID, 几率>
    uint32 totalChance = 0;

    for (const auto& pair : _identificationTemplates)
    {
        const IdentificationTemplate& tmpl = pair.second;
        bool inventoryMatches = tmpl.inventoryTypeLimit.empty();
        if (!inventoryMatches && item && item->GetTemplate())
        {
            std::string limits = tmpl.inventoryTypeLimit;
            std::replace(limits.begin(), limits.end(), '#', ',');
            std::stringstream stream(limits);
            std::string token;
            while (std::getline(stream, token, ','))
            {
                if (!token.empty() && uint32(std::strtoul(token.c_str(), nullptr, 10)) == item->GetTemplate()->InventoryType)
                {
                    inventoryMatches = true;
                    break;
                }
            }
        }

        if (tmpl.group == groupId && inventoryMatches)
        {
            templates.push_back(std::make_pair(tmpl.id, tmpl.randomChance));
            totalChance += tmpl.randomChance;
        }
    }

    if (templates.empty() || totalChance == 0)
    {
        LOG_WARN("module.itemidentification", ModuleOldFormat("[鉴定模板选择] 未找到组ID为 %s 的鉴定模板!", groupId).c_str());
        return 0;
    }

    // ✅ 使用全局随机数生成器（避免重复创建）
    std::uniform_int_distribution<uint32> dis(1, totalChance);
    uint32 roll = dis(GetRandomGenerator());

    uint32 currentChance = 0;
    for (const auto& pair : templates)
    {
        currentChance += pair.second;
        if (roll <= currentChance)
        {
            return pair.first;
        }
    }

    // 如果出现问题，返回第一个模板
    LOG_WARN("module.itemidentification", ModuleOldFormat("[鉴定模板选择] 未命中任何模板，返回第一个: ID=%s", templates[0].first).c_str());
    return templates[0].first;
}

// 应用鉴定结果到物品（需要指定组ID）
bool ItemIdentificationSystem::ApplyIdentification(Player* player, Item* item, uint32 groupId, uint64 reservedGold)
{
    if (!player || !item || item->GetOwnerGUID() != player->GetGUID() || item->IsInTrade())
        return false;

    // 获取选择的模板ID
    if (!_enabled || !sHelper->GetBoolean(22011))
        return false;

    uint32 templateId = SelectIdentificationTemplate(groupId, item);
    if (!templateId || _identificationTemplates.find(templateId) == _identificationTemplates.end())
        return false;

    const IdentificationTemplate& tmpl = _identificationTemplates[templateId];

    ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][开始] 玩家=%s 物品ID=%s GUID=%s 鉴定组=%s 模板=%s 模板组=%s 品质=%s 装等=%s 成长组='%s' 成长数量=%s-%s 成长值=%s-%s 强化组='%s' 强化数量=%s-%s 强化值=%s-%s 基础组='%s' 基础数量=%s-%s 基础值=%s-%s 追加组='%s' 追加数量=%s-%s 追加值=%s-%s",
        player ? player->GetName() : "未知",
        item ? item->GetEntry() : 0,
        item ? item->GetGUID().GetCounter() : 0,
        groupId,
        tmpl.id,
        tmpl.group,
        item && item->GetTemplate() ? item->GetTemplate()->Quality : 0,
        item && item->GetTemplate() ? item->GetTemplate()->ItemLevel : 0,
        tmpl.itemGrowthGroups.c_str(),
        tmpl.growthAttrMinCount,
        tmpl.growthAttrMaxCount,
        Acore::ToString(tmpl.growthAttrMinValue),
        Acore::ToString(tmpl.growthAttrMaxValue),
        tmpl.itemEnhancementGroups.c_str(),
        tmpl.enhancementAttrMinCount,
        tmpl.enhancementAttrMaxCount,
        Acore::ToString(tmpl.enhancementAttrMinValue),
        Acore::ToString(tmpl.enhancementAttrMaxValue),
        tmpl.itemAttributesGroups.c_str(),
        tmpl.baseAttrMinCount,
        tmpl.baseAttrMaxCount,
        Acore::ToString(tmpl.baseAttrMinValue),
        Acore::ToString(tmpl.baseAttrMaxValue),
        tmpl.itemAttributesAdditionalGroups.c_str(),
        tmpl.additionalAttrMinCount,
        tmpl.additionalAttrMaxCount,
        Acore::ToString(tmpl.additionalAttrMinValue),
        Acore::ToString(tmpl.additionalAttrMaxValue)).c_str());


    // 0. 检查物品是否已鉴定（防止重复鉴定）
    uint32 itemPropertySeed = item->GetGUID().GetCounter();
    uint32 itemGuid = itemPropertySeed;  // 使用 PROPERTY_SEED 作为唯一标识
    auto failIdentification = [&](char const* stage)
    {
        LOG_WARN("module.itemidentification", "鉴定未完成: 物品={}, GUID={}, 鉴定组={}, 模板={}, 失败阶段={}",
            item->GetEntry(), itemGuid, groupId, templateId, stage);
        return false;
    };
    if (IsItemIdentified(itemGuid))
    {
        DebugLog(ModuleOldFormat("物品已经被鉴定过了: GUID=%s", itemGuid).c_str());
        ChatHandler(player->GetSession()).SendSysMessage("此物品已经鉴定过了");
        return false;
    }



    // 1. 检查需求条件
    auto reqStart = std::chrono::high_resolution_clock::now();
    if (!CheckRequirements(player, item, tmpl))
    {
        DebugLog("玩家不满足鉴定需求条件");
        return false;
    }



    bool const wasBound = item->IsSoulBound();
    uint32 const oldSeed = item->GetUInt32Value(ITEM_FIELD_PROPERTY_SEED);
    std::optional<ItemGrowthPlayerRecord> oldGrowth;
    if (auto const* growth = sItemGrowthMgr->GetPlayerItemRecord(itemGuid))
        oldGrowth = *growth;
    if (!tmpl.skillSetGroups.empty() && !sItemSetsManager->GetPlayerSetStatus(player->GetGUIDLow()))
        sItemSetsManager->LoadPlayerSetStatus(player);
    uint32 oldSetId = 0;
    if (auto const* status = sItemSetsManager->GetPlayerSetStatus(player->GetGUIDLow()))
        if (auto itr = status->ItemSetMap.find(itemGuid); itr != status->ItemSetMap.end())
            oldSetId = itr->second;
    ItemEnchantSnapshot oldEnchants(item);

    ItemDataRollback rollback(itemGuid);
    if (!rollback.Capture("item_instance") || !rollback.Capture("物品_鉴定记录") || !rollback.Capture("物品属性_数据") ||
        !rollback.Capture("_物品技能_数据") || !rollback.Capture("物品成长_玩家记录") ||
        !rollback.Capture("物品强化_记录") || !rollback.Capture("符文系统_数据") ||
        !rollback.Capture("_物品套装_数据"))
    {
        ChatHandler(player->GetSession()).SendSysMessage("无法读取原装备数据，鉴定已取消，未扣费");
        return failIdentification("读取装备快照");
    }

    if (item->IsEquipped())
        sItemAttributesEffects->RemoveItemAttributeEffects(player, item);
    rollback.Arm([&, this]
    {
        ClearIdentifiedCache(itemGuid);
        ClearItemCache(item->GetEntry(), itemGuid);
        ItemAttributesDBHelper::EraseCache(itemGuid);
        sItemSkillsDBHelper->ClearCache(itemGuid);
        sItemEnhancementMgr->ClearPlayerRecordCache(player->GetGUIDLow());
        if (oldGrowth)
            sItemGrowthMgr->GetPlayerItemRecords()[itemGuid] = *oldGrowth;
        else
            sItemGrowthMgr->GetPlayerItemRecords().erase(itemGuid);
        sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);
        if (auto* status = sItemSetsManager->GetPlayerSetStatus(player->GetGUIDLow()))
        {
            if (oldSetId)
                status->ItemSetMap[itemGuid] = oldSetId;
            else
                status->ItemSetMap.erase(itemGuid);
        }
        oldEnchants.Restore(player, item, false);
        item->SetBinding(wasBound);
        item->SetUInt32Value(ITEM_FIELD_PROPERTY_SEED, oldSeed);
        item->SetState(ITEM_CHANGED, player);
        if (item->IsEquipped())
            sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
        player->UpdateAllStats();
    });

    // 预先绑定物品，使其满足强化系统等模块的需求
    item->SetBinding(true);


    // 创建鉴定记录
    ItemIdentificationRecord record{};
    record.playerGuid = player->GetGUID().GetCounter();
    record.itemGuid = itemGuid;
    record.itemEntry = item->GetEntry();
    record.templateId = templateId;
    record.costGold = reservedGold;
    record.successRate = GetSuccessRate(player, item);

    // 初始化所有标记为false，所有数值字段为0
    record.hasGrowth = false;
    record.growthGroup = 0;
    record.hasEnhancement = false;
    record.enhancementGroup = 0;
    record.hasBaseAttributes = false;
    record.baseAttrCount = 0;
    record.baseAttrGroup = 0;
    record.baseAttrDetails = "";
    record.hasAdditionalAttributes = false;
    record.additionalAttrCount = 0;
    record.additionalAttrGroups = "";
    record.hasRuneSlots = false;
    record.runeSlotCount = 0;
    record.hasSkills = false;
    record.skillGroups = "";
    record.hasSet = false;
    record.setGroup = 0;
    record.setId = 0;



    // 2. 应用物品成长系统

    if (!tmpl.itemGrowthGroups.empty())
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][成长属性] 进入分支: 物品ID=%s GUID=%s 模板=%s 组='%s' 数量=%s-%s 值=%s-%s",
            item->GetEntry(),
            itemGuid,
            tmpl.id,
            tmpl.itemGrowthGroups.c_str(),
            tmpl.growthAttrMinCount,
            tmpl.growthAttrMaxCount,
            Acore::ToString(tmpl.growthAttrMinValue),
            Acore::ToString(tmpl.growthAttrMaxValue)).c_str());
        uint32 appliedGroup = ApplyItemGrowth(player, item, tmpl);

        if (appliedGroup > 0)
        {
            record.hasGrowth = true;
            record.growthGroup = appliedGroup; // 使用实际应用的组号
            ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][成长属性] 成功: 物品ID=%s GUID=%s 模板=%s 应用组=%s", item->GetEntry(), itemGuid, tmpl.id, appliedGroup).c_str());
        }
        else
        {
            ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][成长属性] 失败: 物品ID=%s GUID=%s 模板=%s 组='%s'", item->GetEntry(), itemGuid, tmpl.id, tmpl.itemGrowthGroups.c_str()).c_str());
        }
    }
    else
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][成长属性] 跳过: 物品ID=%s GUID=%s 模板=%s 原因=物品成长_系统为空", item->GetEntry(), itemGuid, tmpl.id).c_str());
    }

    // 3. 应用物品强化系统

    if (!tmpl.itemEnhancementGroups.empty())
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][强化属性] 进入分支: 物品ID=%s GUID=%s 模板=%s 组='%s' 数量=%s-%s 值=%s-%s",
            item->GetEntry(),
            itemGuid,
            tmpl.id,
            tmpl.itemEnhancementGroups.c_str(),
            tmpl.enhancementAttrMinCount,
            tmpl.enhancementAttrMaxCount,
            Acore::ToString(tmpl.enhancementAttrMinValue),
            Acore::ToString(tmpl.enhancementAttrMaxValue)).c_str());
        uint32 appliedGroup = ApplyItemEnhancement(player, item, tmpl);

        if (appliedGroup > 0)
        {
            record.hasEnhancement = true;
            record.enhancementGroup = appliedGroup; // 使用实际应用的组号
            ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][强化属性] 成功: 物品ID=%s GUID=%s 模板=%s 应用组=%s", item->GetEntry(), itemGuid, tmpl.id, appliedGroup).c_str());
        }
        else
        {
            ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][强化属性] 失败: 物品ID=%s GUID=%s 模板=%s 组='%s'", item->GetEntry(), itemGuid, tmpl.id, tmpl.itemEnhancementGroups.c_str()).c_str());
        }
    }
    else
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][强化属性] 跳过: 物品ID=%s GUID=%s 模板=%s 原因=物品强化_系统为空", item->GetEntry(), itemGuid, tmpl.id).c_str());
    }

    // 4. 应用基础属性

    if (!tmpl.itemAttributesGroups.empty() && tmpl.baseAttrMaxCount > 0)
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][基础属性] 进入分支: 物品ID=%s GUID=%s 模板=%s 组='%s' 数量=%s-%s 值=%s-%s 判定模式=%s",
            item->GetEntry(),
            itemGuid,
            tmpl.id,
            tmpl.itemAttributesGroups.c_str(),
            tmpl.baseAttrMinCount,
            tmpl.baseAttrMaxCount,
            Acore::ToString(tmpl.baseAttrMinValue),
            Acore::ToString(tmpl.baseAttrMaxValue),
            "固定值").c_str());
        std::string attrDetails;
        uint32 attrCount = 0;
        uint32 attrGroup = 0;

        // 首次鉴定保留原有清理规则；失败时由快照恢复。
        ItemAttributesDBHelper::ClearItemAttributes(itemGuid);
        if (!ApplyBaseAttributes(player, item, tmpl, attrDetails, attrCount, attrGroup))
            return failIdentification("生成基础属性");

        // 只要attrCount > 0就认为应用成功（即使无法读取详情）
        if (attrCount > 0)
        {
            record.hasBaseAttributes = true;
            record.baseAttrCount = attrCount;
            record.baseAttrGroup = attrGroup;
            record.baseAttrDetails = attrDetails; // 可能为空，但不影响记录
            ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][基础属性] 成功: 物品ID=%s GUID=%s 模板=%s 组=%s 数量=%s 详情='%s'", item->GetEntry(), itemGuid, tmpl.id, attrGroup, attrCount, attrDetails.c_str()).c_str());
        }
        else
        {
            ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][基础属性] 失败: 物品ID=%s GUID=%s 模板=%s 组='%s'", item->GetEntry(), itemGuid, tmpl.id, tmpl.itemAttributesGroups.c_str()).c_str());
        }
    }
    else
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][基础属性] 跳过: 物品ID=%s GUID=%s 模板=%s 原因=%s", item->GetEntry(), itemGuid, tmpl.id, tmpl.itemAttributesGroups.empty() ? "物品属性_模板为空" : "基础属性最大数量为0").c_str());
    }

    // 5. 应用追加属性

    if (!tmpl.itemAttributesAdditionalGroups.empty() && tmpl.additionalAttrMaxCount > 0)
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][追加属性] 进入分支: 物品ID=%s GUID=%s 模板=%s 组='%s' 数量=%s-%s 值=%s-%s 判定模式=%s",
            item->GetEntry(),
            itemGuid,
            tmpl.id,
            tmpl.itemAttributesAdditionalGroups.c_str(),
            tmpl.additionalAttrMinCount,
            tmpl.additionalAttrMaxCount,
            Acore::ToString(tmpl.additionalAttrMinValue),
            Acore::ToString(tmpl.additionalAttrMaxValue),
            "固定值").c_str());
        auto additionalResult = ApplyAdditionalAttributes(player, item, tmpl, record.additionalAttrGroups);
        if (!additionalResult)
            return failIdentification("生成追加属性");
        uint32 appliedAdditionalCount = *additionalResult;
        if (appliedAdditionalCount > 0)
        {
            record.hasAdditionalAttributes = true;
            record.additionalAttrCount = appliedAdditionalCount;
            ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][追加属性] 成功: 物品ID=%s GUID=%s 模板=%s 数量=%s 组='%s'", item->GetEntry(), itemGuid, tmpl.id, appliedAdditionalCount, tmpl.itemAttributesAdditionalGroups.c_str()).c_str());
        }
        else
        {
            ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][追加属性] 失败: 物品ID=%s GUID=%s 模板=%s 组='%s'", item->GetEntry(), itemGuid, tmpl.id, tmpl.itemAttributesAdditionalGroups.c_str()).c_str());
        }

    }
    else
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][追加属性] 跳过: 物品ID=%s GUID=%s 模板=%s 原因=%s", item->GetEntry(), itemGuid, tmpl.id, tmpl.itemAttributesAdditionalGroups.empty() ? "物品属性_模板_组为空" : "追加属性最大数量为0").c_str());
    }

    // 6. 应用追加技能

    if (!tmpl.itemSkillsGroups.empty() && tmpl.additionalSkillMaxCount > 0)
    {
        if (!ApplyAdditionalSkills(player, item, tmpl, record.skillGroups))
            return failIdentification("生成追加技能");
        record.hasSkills = !record.skillGroups.empty();

    }
    else
    {

    }

    // 7. 应用符文系统

    if (!tmpl.runeSystemGroups.empty() || tmpl.runeSlotMaxCount > 0)
    {
        auto slots = ApplyRuneSystem(player, item, tmpl);
        if (!slots)
            return failIdentification("生成符文插槽");
        record.runeSlotCount = *slots;
        record.hasRuneSlots = record.runeSlotCount > 0;

    }
    else
    {

    }

    // 8. 应用技能套装

    record.hasSet = false;
    record.setGroup = 0;
    record.setId = 0;

    if (!tmpl.skillSetGroups.empty())
    {
        auto selectedSetId = ApplySkillSets(player, item, tmpl, record.setGroup);
        if (!selectedSetId)
            return failIdentification("分配套装");
        if (*selectedSetId > 0)
        {
            record.hasSet = true;
            record.setId = *selectedSetId; // 保存实际分配的套装ID，供后续批量查询使用
        }
    }

    // 9. 随机附魔：读取 _模板_物品附魔，按生效几率筛选后无重复抽取。
    if (tmpl.enchantGroup && tmpl.enchantCount)
    {
        auto enchantItr = _identificationEnchantGroups.find(tmpl.enchantGroup);
        if (enchantItr == _identificationEnchantGroups.end())
            return failIdentification("查找随机附魔组");
        if (enchantItr != _identificationEnchantGroups.end())
        {
            std::vector<uint32> candidates;
            for (IdentificationEnchantEntry const& entry : enchantItr->second)
            {
                if (entry.enchantId && sSpellItemEnchantmentStore.LookupEntry(entry.enchantId) &&
                    (entry.chance >= 100.0f || roll_chance_f(entry.chance)) &&
                    std::find(candidates.begin(), candidates.end(), entry.enchantId) == candidates.end())
                    candidates.push_back(entry.enchantId);
            }

            if (candidates.empty())
                return failIdentification("选择随机附魔");
            std::shuffle(candidates.begin(), candidates.end(), GetRandomGenerator());
            uint32 applyCount = std::min<uint32>(tmpl.enchantCount, candidates.size());
            uint32 candidateIndex = 0;
            for (uint8 slot = 0; slot < 5 && candidateIndex < applyCount; ++slot)
            {
                EnchantmentSlot enchantmentSlot = EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + slot);
                if (item->GetEnchantmentId(enchantmentSlot) == 0)
                    item->SetEnchantment(enchantmentSlot, candidates[candidateIndex++], 0, 0);
            }
        }
        SyncIdentificationEnchantData(player, item);
    }

    bool const incomplete = (!tmpl.itemGrowthGroups.empty() && !record.hasGrowth) ||
        (!tmpl.itemEnhancementGroups.empty() && !record.hasEnhancement) ||
        (!tmpl.itemAttributesGroups.empty() && tmpl.baseAttrMaxCount && record.baseAttrCount < tmpl.baseAttrMinCount) ||
        (!tmpl.itemAttributesAdditionalGroups.empty() && tmpl.additionalAttrMaxCount &&
            record.additionalAttrCount < tmpl.additionalAttrMinCount) ||
        (tmpl.runeSlotMaxCount && record.runeSlotCount < tmpl.runeSlotMinCount);
    if (incomplete)
    {
        ChatHandler(player->GetSession()).SendSysMessage("鉴定配置未能完整生成结果，已取消，未扣费");
        return failIdentification("核对成长强化与属性数量");
    }

    if ((record.hasBaseAttributes || record.hasAdditionalAttributes || record.hasEnhancement) &&
        !VerifyItemAttributes(itemGuid))
        return failIdentification("校验属性存档");

    // 10. 更新物品状态
    item->SetState(ITEM_CHANGED, player);

    // 10. 保存鉴定记录到数据库。必须确认落库成功后才允许进入已鉴定缓存，
    // 否则数据库断线时会出现日志成功但悬停/重查没有鉴定数据。
    if (!SaveIdentificationRecord(record) || !SaveOperationItemState(item))
        return failIdentification("保存鉴定结果");

    std::vector<uint32> requirements{tmpl.requirementTemplate};
    if (record.hasSet)
    {
        auto const& setData = sItemSetsManager->GetItemSetData(record.setId);
        if (!setData.empty())
            requirements.push_back(setData.front().RequirementId);
    }
    if (!PayItemOperation(player, item, requirements, reservedGold, true))
    {
        ChatHandler(player->GetSession()).SendSysMessage("鉴定与套装的合计需求不足，已取消，未扣费");
        return failIdentification("核对鉴定与套装消耗");
    }
    rollback.Commit();
    if (item->IsEquipped())
    {
        sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
        for (uint8 index = 0; index < oldEnchants.values.size(); ++index)
            if (!oldEnchants.values[index][0])
                player->ApplyEnchantment(item, EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + index), true);
    }

    ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][完成] 物品ID=%s GUID=%s 模板=%s 成长=%s 成长组=%s 强化=%s 强化组=%s 基础=%s 基础组=%s 基础数量=%s 追加=%s 追加数量=%s 技能=%s 符文=%s 套装=%s 套装ID=%s",
        item->GetEntry(),
        itemGuid,
        tmpl.id,
        record.hasGrowth ? "是" : "否",
        record.growthGroup,
        record.hasEnhancement ? "是" : "否",
        record.enhancementGroup,
        record.hasBaseAttributes ? "是" : "否",
        record.baseAttrGroup,
        record.baseAttrCount,
        record.hasAdditionalAttributes ? "是" : "否",
        record.additionalAttrCount,
        record.hasSkills ? "是" : "否",
        record.hasRuneSlots ? "是" : "否",
        record.hasSet ? "是" : "否",
        record.setId).c_str());

    // 11. 刷新物品显示
    RefreshItem(player, item);

    // 【关键修复】12. 清除批量查询缓存，确保后续查询能获取最新数据
    // 原因：在鉴定过程中，客户端可能已经发送过查询请求，导致缓存中存储了空数据
    // 必须在发送数据前清除缓存，让 SendAllModuleDataAddon 重新从数据库查询
    {
        std::lock_guard<std::mutex> lock(_batchCacheMutex);
        uint64 cacheKey = (static_cast<uint64>(item->GetEntry()) << 32) | itemGuid;
        _batchQueryCache.erase(cacheKey);
    }

    // 13. 鉴定流程完成后延迟推送一次，等待调用方清理待鉴定标记和子模块数据落库。
    QueueAllModuleDataAddonRefresh(player, item->GetEntry(), itemGuid, 2000ms);

    // 13. 套装刷新已优化：移除鉴定时的刷新调用
    // 原因：物品在背包中时套装效果不需要生效，只有装备时才需要刷新
    // 套装系统会在玩家装备物品时（OnEquip）自动调用 RefreshPlayerSetEffects
    // 这样避免了鉴定时 415ms 的无用刷新，性能提升 87%
#ifdef MODULE_ITEM_SETS
    if (sItemSetsManager && !tmpl.skillSetGroups.empty())
    {

    }
#endif

    DebugLog("成功应用鉴定模板到物品，记录已保存");
    return true;
}

// 工具方法：解析逗号分隔的字符串为数字列表
std::vector<uint32> ParseCommaSeparatedNumbers(const std::string& str)
{
    std::vector<uint32> result;
    if (str.empty())
        return result;

    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        if (!item.empty())
        {
            try {
                uint32 num = std::stoul(item);
                result.push_back(num);
            } catch (...) {
            }
        }
    }

    return result;
}

// 工具方法：从列表中随机选择一个元素 - 优化版
uint32 SelectRandomFromList(const std::vector<uint32>& list)
{
    if (list.empty()) return 0;

    // ✅ 使用全局随机数生成器
    std::uniform_int_distribution<size_t> dis(0, list.size() - 1);
    return list[dis(sItemIdentificationSystem->GetRandomGenerator())];
}

// 工具方法：生成指定范围内的随机数 - 优化版
uint32 GenerateRandomNumber(uint32 min, uint32 max)
{
    if (min >= max) return min;

    // ✅ 使用全局随机数生成器
    std::uniform_int_distribution<uint32> dis(min, max);
    return dis(sItemIdentificationSystem->GetRandomGenerator());
}

uint64 GenerateRandomUInt64(uint64 min, uint64 max)
{
    if (min >= max)
        return min;

    uint64 span = max - min;
    uint64 randomValue = 0;
    std::uniform_int_distribution<uint32> dis(0, std::numeric_limits<uint32>::max());

    for (uint8 i = 0; i < 4; ++i)
    {
        randomValue <<= 32;
        randomValue += dis(sItemIdentificationSystem->GetRandomGenerator());
    }

    if (span == std::numeric_limits<uint64>::max())
        return randomValue;

    return min + (randomValue % (span + 1));
}

uint32 GetIdentificationTemplateMatchLevel(Item* item)
{
    if (!item || !item->GetTemplate())
        return 0;

    return item->GetTemplate()->Quality;
}

#ifdef MODULE_ITEM_ATTRIBUTES
std::set<uint32> GetAllowedAttributeTypesByGroups(const std::vector<uint32>& attributeGroups)
{
    std::set<uint32> allowedTypes;

    if (!sItemAttributesLoader)
        return allowedTypes;

    for (uint32 groupId : attributeGroups)
    {
        std::vector<ItemAttributeTemplate const*> templates = sItemAttributesLoader->GetItemAttributeTemplatesByGroup(groupId);
        for (ItemAttributeTemplate const* attributeTemplate : templates)
        {
            if (attributeTemplate)
                allowedTypes.insert(attributeTemplate->attributeType);
        }
    }

    return allowedTypes;
}

uint32 FilterItemAdditionalAttributesByGroups(Item* item, const std::vector<uint32>& attributeGroups)
{
    if (!item || attributeGroups.empty())
        return 0;

    auto data = ItemAttributesDBHelper::LoadItemAttributes(item->GetGUID().GetCounter());
    if (!data)
        return 0;

    std::set<uint32> allowedTypes = GetAllowedAttributeTypesByGroups(attributeGroups);
    if (allowedTypes.empty())
        return static_cast<uint32>(data->additionalAttributeIds.size());

    std::vector<uint32> filteredIds;
    std::vector<AsInt64> filteredValues;
    filteredIds.reserve(data->additionalAttributeIds.size());
    filteredValues.reserve(data->additionalAttributeValues.size());

    bool hasFiltered = false;
    for (size_t i = 0; i < data->additionalAttributeIds.size() && i < data->additionalAttributeValues.size(); ++i)
    {
        uint32 attributeType = data->additionalAttributeIds[i];
        if (allowedTypes.find(attributeType) != allowedTypes.end())
        {
            filteredIds.push_back(attributeType);
            filteredValues.push_back(data->additionalAttributeValues[i]);
        }
        else
        {
            hasFiltered = true;
        }
    }

    if (hasFiltered)
    {
        ItemAttributesDBHelper::SaveAdditionalAttributes(item->GetGUID().GetCounter(), item->GetEntry(), filteredIds, filteredValues);
        std::ostringstream groupStream;
        for (size_t i = 0; i < attributeGroups.size(); ++i)
        {
            if (i > 0)
                groupStream << ",";
            groupStream << attributeGroups[i];
        }

        LOG_WARN("module.itemidentification", ModuleOldFormat("[追加属性修复] 物品GUID=%s 过滤了不属于允许组 [%s] 的追加属性，剩余 %s 条", item->GetGUID().GetCounter(), groupStream.str(), filteredIds.size()).c_str());
    }

    return static_cast<uint32>(filteredIds.size());
}
#endif

// 检查需求条件
bool ItemIdentificationSystem::CheckRequirements(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
    if (tmpl.requirementTemplate == 0)
        return true;

    DebugLog(ModuleOldFormat("检查需求模板ID: %s", tmpl.requirementTemplate).c_str());

    bool meetsRequirements = ShowNeedTemplate(player, tmpl.requirementTemplate);

    if (meetsRequirements)
    {
        DebugLog(ModuleOldFormat("玩家满足需求模板ID: %s", tmpl.requirementTemplate).c_str());
    }
    else
    {
        DebugLog(ModuleOldFormat("玩家不满足需求模板ID: %s", tmpl.requirementTemplate).c_str());
        ChatHandler(player->GetSession()).PSendSysMessage("不满足鉴定条件");
    }

    return meetsRequirements;
}

// 应用物品成长系统（返回实际应用的组号，0表示失败）
uint32 ItemIdentificationSystem::ApplyItemGrowth(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
    std::vector<uint32> growthGroups = ParseCommaSeparatedNumbers(tmpl.itemGrowthGroups);

    if (growthGroups.empty())
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][成长属性] 失败: 物品ID=%s GUID=%s 模板=%s 原因=成长组解析为空 原始='%s'", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id, tmpl.itemGrowthGroups.c_str()).c_str());
        return 0;
    }

#ifdef MODULE_ITEM_GROWTH
    if (!sItemGrowthMgr)
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][成长属性] 失败: 物品ID=%s GUID=%s 模板=%s 原因=sItemGrowthMgr为空", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id).c_str());
        return 0;
    }

    if (!sItemGrowthMgr->IsEnabled())
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][成长属性] 失败: 物品ID=%s GUID=%s 模板=%s 原因=成长系统未启用", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id).c_str());
        return 0;
    }

    // 随机选择一个成长组
    uint32 selectedGroup = SelectRandomFromList(growthGroups);

    if (selectedGroup == 0)
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][成长属性] 失败: 物品ID=%s GUID=%s 模板=%s 原因=随机到0组", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id).c_str());
        return 0;
    }

    ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][成长属性] 调用成长系统: 物品ID=%s GUID=%s 模板=%s 选择组=%s 数量=%s-%s 值=%s-%s",
        item ? item->GetEntry() : 0,
        item ? item->GetGUID().GetCounter() : 0,
        tmpl.id,
        selectedGroup,
        tmpl.growthAttrMinCount,
        tmpl.growthAttrMaxCount,
        Acore::ToString(tmpl.growthAttrMinValue),
        Acore::ToString(tmpl.growthAttrMaxValue)).c_str());

    // 直接调用成长系统API设置物品成长属性，并根据鉴定模板配置约束初始成长属性
    try
    {
        // 使用专为鉴定准备的接口，传入条目数量和属性值范围
        sItemGrowthMgr->SetItemCanGrowForIdentification(
            player,
            item,
            selectedGroup,
            tmpl.growthAttrMinCount,
            tmpl.growthAttrMaxCount,
            AsInt256FromFloating(tmpl.growthAttrMinValue),
            AsInt256FromFloating(tmpl.growthAttrMaxValue));

        // ChatHandler(player->GetSession()).PSendSysMessage("物品获得成长属性（组{}）", selectedGroup);

        auto const* generated = sItemGrowthMgr->GetPlayerItemRecord(item->GetGUIDLow());
        auto* verify = CharacterDatabase.GetPreparedStatement(CHAR_IDENT_GROWTH_VERIFY);
        verify->SetData(0, item->GetGUIDLow());
        PreparedQueryResult saved = CharacterDatabase.Query(verify);
        if (!generated || !saved || saved->Fetch()[0].Get<uint32>() != selectedGroup ||
            saved->Fetch()[1].Get<std::string>() != generated->growthAttributes)
            return 0;
        return selectedGroup;
    }
    catch (...)
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][成长属性] 异常: 物品ID=%s GUID=%s 模板=%s 选择组=%s", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id, selectedGroup).c_str());
        return 0;
    }
#else

    return 0;
#endif
}

// 应用物品强化系统（返回实际应用的组号，0表示失败）
uint32 ItemIdentificationSystem::ApplyItemEnhancement(Player* player, Item* item, const IdentificationTemplate& tmpl)
{
    std::vector<uint32> enhancementGroups = ParseCommaSeparatedNumbers(tmpl.itemEnhancementGroups);

    if (enhancementGroups.empty())
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][强化属性] 失败: 物品ID=%s GUID=%s 模板=%s 原因=强化组解析为空 原始='%s'", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id, tmpl.itemEnhancementGroups.c_str()).c_str());
        return 0;
    }

#ifdef MODULE_ITEM_ENHANCEMENT
    if (!sItemEnhancementMgr)
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][强化属性] 失败: 物品ID=%s GUID=%s 模板=%s 原因=sItemEnhancementMgr为空", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id).c_str());
        return 0;
    }

    if (!sItemEnhancementMgr->IsEnabled())
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][强化属性] 失败: 物品ID=%s GUID=%s 模板=%s 原因=强化系统未启用", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id).c_str());
        return 0;
    }

    // 随机选择一个强化组
    uint32 selectedGroup = SelectRandomFromList(enhancementGroups);

    if (selectedGroup == 0)
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][强化属性] 失败: 物品ID=%s GUID=%s 模板=%s 原因=随机到0组", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id).c_str());
        return 0;
    }

    // 检查物品是否可以强化
    if (!sItemEnhancementMgr->CanEnhanceItem(player, item))
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][强化属性] 失败: 物品ID=%s GUID=%s 模板=%s 选择组=%s 原因=CanEnhanceItem=false", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id, selectedGroup).c_str());
        return 0;
    }

    ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][强化属性] 调用强化系统: 物品ID=%s GUID=%s 模板=%s 选择组=%s 数量=%s-%s 值=%s-%s",
        item ? item->GetEntry() : 0,
        item ? item->GetGUID().GetCounter() : 0,
        tmpl.id,
        selectedGroup,
        tmpl.enhancementAttrMinCount,
        tmpl.enhancementAttrMaxCount,
        Acore::ToString(tmpl.enhancementAttrMinValue),
        Acore::ToString(tmpl.enhancementAttrMaxValue)).c_str());

    try
    {
        // ✅ 使用为鉴定准备的初始化方法，保证产生等级1的强化，并按鉴定模板约束条目数量与数值范围
        if (sItemEnhancementMgr->InitializeEnhancementForIdentification(
                player,
                item,
                selectedGroup,
                tmpl.enhancementAttrMinCount,
                tmpl.enhancementAttrMaxCount,
                tmpl.enhancementAttrMinValue,
                tmpl.enhancementAttrMaxValue))
        {
            // ChatHandler(player->GetSession()).PSendSysMessage("物品获得强化属性（组{}）", selectedGroup);
            auto const* generated = sItemEnhancementMgr->GetEnhancementRecord(item->GetGUIDLow());
            auto* verify = CharacterDatabase.GetPreparedStatement(CHAR_IDENT_ENHANCEMENT_VERIFY);
            verify->SetData(0, item->GetGUIDLow());
            PreparedQueryResult saved = CharacterDatabase.Query(verify);
            if (!generated || !saved || saved->Fetch()[0].Get<uint32>() != selectedGroup ||
                saved->Fetch()[1].Get<std::string>() != generated->statValues)
                return 0;
            return selectedGroup;
        }
        else
        {
            ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][强化属性] 强化系统返回失败: 物品ID=%s GUID=%s 模板=%s 选择组=%s", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id, selectedGroup).c_str());
            return 0;
        }
    }
    catch (...)
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][强化属性] 异常: 物品ID=%s GUID=%s 模板=%s 选择组=%s", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id, selectedGroup).c_str());
        return 0;
    }
#else
    return 0;
#endif
}

// 应用基础属性（替换官方属性显示）
bool ItemIdentificationSystem::ApplyBaseAttributes(Player* player, Item* item,
    IdentificationTemplate const& tmpl, std::string& outAttrDetails, uint32& outAttrCount, uint32& outAttrGroup)
{
    outAttrDetails.clear();
    outAttrCount = 0;
    outAttrGroup = 0;
    if (tmpl.itemAttributesGroups.empty())
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][基础属性] 函数跳过: 物品ID=%s GUID=%s 模板=%s 原因=组为空", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id).c_str());
        return false;
    }

    if (tmpl.baseAttrMaxCount == 0)
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][基础属性] 函数跳过: 物品ID=%s GUID=%s 模板=%s 原因=最大数量为0", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id).c_str());
        return false;
    }

    uint32 attrCount = GenerateRandomNumber(tmpl.baseAttrMinCount, tmpl.baseAttrMaxCount);
    if (attrCount == 0)
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][基础属性] 函数跳过: 物品ID=%s GUID=%s 模板=%s 原因=随机数量为0 范围=%s-%s", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id, tmpl.baseAttrMinCount, tmpl.baseAttrMaxCount).c_str());
        return true;
    }

    std::vector<uint32> attributeGroups = ParseCommaSeparatedNumbers(tmpl.itemAttributesGroups);
    if (attributeGroups.empty())
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][基础属性] 函数失败: 物品ID=%s GUID=%s 模板=%s 原因=组解析为空 原始='%s'", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id, tmpl.itemAttributesGroups.c_str()).c_str());
        return false;
    }

    DebugLog(ModuleOldFormat("应用基础属性，组列表: %s，数量: %s，值范围: %s-%s", tmpl.itemAttributesGroups, attrCount, Acore::ToString(tmpl.baseAttrMinValue), Acore::ToString(tmpl.baseAttrMaxValue)).c_str());

#ifdef MODULE_ITEM_ATTRIBUTES
    if (!sItemAttributesGenerator || !sItemAttributesLoader)
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][基础属性] 函数失败: 物品ID=%s GUID=%s 模板=%s 原因=属性生成器或加载器为空", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id).c_str());
        return false;
    }

    // 【修复】统一使用GetCounter()获取32位GUID，与数据库字段类型一致
    uint64 itemGuid = item->GetGUID().GetCounter();

    // 从配置列表中等概率选择一个组，组0也是有效配置。
    uint32 selectedGroup = SelectRandomFromList(attributeGroups);

    DebugLog("===== 基础属性生成详细信息 =====");
    DebugLog(ModuleOldFormat("配置的组字符串: %s", tmpl.itemAttributesGroups).c_str());
    DebugLog(ModuleOldFormat("解析后的组列表大小: %s", attributeGroups.size()).c_str());
    DebugLog(ModuleOldFormat("选中的组: %s", selectedGroup).c_str());
    DebugLog(ModuleOldFormat("属性数量: %s", attrCount).c_str());
    DebugLog("=================================");

    ItemAttributeGenerateOptions options;
    options.minAttributes = attrCount;        // 最少属性数量
    options.maxAttributes = attrCount;        // 最多属性数量
    options.attributeGroup = selectedGroup;   // 指定属性组
    options.minItemLevel = AsInt256FromFloating(tmpl.baseAttrMinValue);   // 属性值最小值
    options.maxItemLevel = AsInt256FromFloating(tmpl.baseAttrMaxValue);   // 属性值最大值
    options.respectChance = true;             // 考虑属性获取几率
    options.allowDuplicateTypes = tmpl.baseAttrAllowDuplicate;  // 是否允许重复
    options.useValueRangeFilter = true;       // 启用属性值范围过滤
    options.category = AttributeCategory::BASE;  // 设置为基础属性

    // 生成随机属性（这会替换物品的属性显示）
    ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][基础属性] 使用固定值模式: 物品ID=%s GUID=%s 模板=%s 组=%s 数量=%s 值=%s-%s",
        item->GetEntry(),
        item->GetGUID().GetCounter(),
        tmpl.id,
        selectedGroup,
        attrCount,
        Acore::ToString(tmpl.baseAttrMinValue),
        Acore::ToString(tmpl.baseAttrMaxValue)).c_str());
    bool generateResult = sItemAttributesGenerator->GenerateRandomAttributes(item, options);



    if (generateResult)
    {
        // ✅ 优化：使用异步Execute避免死锁，内存缓存已立即更新
        // 如果读取失败，接受属性延迟加载（不影响鉴定成功）

        std::vector<uint32> baseAttributes;
        std::vector<AsInt64> baseValues;

        uint64 itemGuid = item->GetGUID().GetCounter();

        // ✅ 尝试一次读取（不重试，不等待）
        auto data = ItemAttributesDBHelper::LoadItemAttributes(itemGuid);  // 【智能指针修复】
        if (data && !data->baseAttributeIds.empty())
        {
            baseAttributes = data->baseAttributeIds;
            baseValues = data->baseAttributeValues;
            // 【智能指针修复】移除 delete，自动清理
        }
        else
        {
            // 【智能指针修复】移除 delete，自动清理

            // 读取失败也不影响鉴定，玩家重新登录或查询时会加载
            DebugLog("基础属性生成成功，但立即读取失败（属性将在下次查询时加载）");
        }

        if (!baseAttributes.empty() && baseAttributes.size() == baseValues.size())
        {
            // 构建属性详情字符串：格式为 "属性ID 值,属性ID 值,..."
            std::ostringstream oss;
            for (size_t i = 0; i < baseAttributes.size(); ++i)
            {
                oss << baseAttributes[i] << " " << baseValues[i];
                if (i < baseAttributes.size() - 1)
                {
                    oss << ",";
                }
            }
            outAttrDetails = oss.str();
            outAttrCount = baseAttributes.size();
            outAttrGroup = selectedGroup;
        }
        else
        {
            return false;
        }

        DebugLog(ModuleOldFormat("成功应用基础属性组: %s，数量: %s", selectedGroup, attrCount).c_str());
    }
    else
    {
        DebugLog("基础属性应用失败");
        outAttrDetails = "";
        outAttrCount = 0;
        outAttrGroup = 0;
    }
#else
    DebugLog("属性系统模块未编译");
#endif


    return outAttrCount >= tmpl.baseAttrMinCount && outAttrCount > 0;

}

// 应用追加属性
std::optional<uint32> ItemIdentificationSystem::ApplyAdditionalAttributes(
    Player* player, Item* item, IdentificationTemplate const& tmpl, std::string& appliedGroups)
{
    appliedGroups.clear();
    std::set<uint32> selectedGroups;
    std::vector<uint32> attributeGroups = ParseCommaSeparatedNumbers(tmpl.itemAttributesAdditionalGroups);
    if (attributeGroups.empty())
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][追加属性] 函数失败: 物品ID=%s GUID=%s 模板=%s 原因=组解析为空 原始='%s'", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id, tmpl.itemAttributesAdditionalGroups.c_str()).c_str());
        return std::nullopt;
    }

    uint32 attrCount = GenerateRandomNumber(tmpl.additionalAttrMinCount, tmpl.additionalAttrMaxCount);
    if (attrCount == 0)
    {
        ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][追加属性] 函数跳过: 物品ID=%s GUID=%s 模板=%s 原因=随机数量为0 范围=%s-%s", item ? item->GetEntry() : 0, item ? item->GetGUID().GetCounter() : 0, tmpl.id, tmpl.additionalAttrMinCount, tmpl.additionalAttrMaxCount).c_str());
        return 0;
    }

#ifdef MODULE_ITEM_ATTRIBUTES
    if (sItemAttributesGenerator)
    {
        uint32 appliedCount = 0;
        uint64 itemGuid = item->GetGUID().GetCounter();

        if (auto existingData = ItemAttributesDBHelper::LoadItemAttributes(itemGuid))
        {
            if (!existingData->additionalAttributeIds.empty())
            {
                ItemAttributesDBHelper::SaveAdditionalAttributes(itemGuid, item->GetEntry(), {}, {});
            }
        }

        // 判断是单组还是多组模式
        if (attributeGroups.size() == 1)
        {
            // 单组模式：从同一个组生成多个属性
            ItemAttributeGenerateOptions options;
            options.minAttributes = attrCount;  // 使用配置的数量
            options.maxAttributes = attrCount;
            options.attributeGroup = attributeGroups[0];
            options.minItemLevel = AsInt256FromFloating(tmpl.additionalAttrMinValue);
            options.maxItemLevel = AsInt256FromFloating(tmpl.additionalAttrMaxValue);
            options.respectChance = true;
            options.allowDuplicateTypes = tmpl.additionalAttrAllowDuplicate;
            options.useValueRangeFilter = true;
            options.category = AttributeCategory::ADDITIONAL;  // 设置为追加属性

            ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][追加属性] 使用固定值单组模式: 物品ID=%s GUID=%s 模板=%s 组=%s 数量=%s 值=%s-%s",
                item->GetEntry(),
                itemGuid,
                tmpl.id,
                attributeGroups[0],
                attrCount,
                Acore::ToString(tmpl.additionalAttrMinValue),
                Acore::ToString(tmpl.additionalAttrMaxValue)).c_str());
            if (sItemAttributesGenerator->GenerateRandomAttributes(item, options))
            {
                selectedGroups.insert(attributeGroups[0]);
                appliedCount = FilterItemAdditionalAttributesByGroups(item, attributeGroups);
                ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][追加属性] 固定值单组过滤后数量: 物品ID=%s GUID=%s 模板=%s 数量=%s", item->GetEntry(), itemGuid, tmpl.id, appliedCount).c_str());
            }
            else
                return std::nullopt;
        }
        else
        {
            // 多组模式：从多个组中随机选择，总共生成attrCount个属性

            // 为每个属性随机选择一个组
            for (uint32 i = 0; i < attrCount; ++i)
            {
                // 随机选择一个组
                uint32 groupId = SelectRandomFromList(attributeGroups);
                ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][追加属性] 使用固定值多组模式: 物品ID=%s GUID=%s 模板=%s 第%s/%s次 选择组=%s 值=%s-%s",
                    item->GetEntry(),
                    itemGuid,
                    tmpl.id,
                    i + 1,
                    attrCount,
                    groupId,
                    Acore::ToString(tmpl.additionalAttrMinValue),
                    Acore::ToString(tmpl.additionalAttrMaxValue)).c_str());

                ItemAttributeGenerateOptions options;
                options.minAttributes = 1;
                options.maxAttributes = 1;
                options.attributeGroup = groupId;
                options.minItemLevel = AsInt256FromFloating(tmpl.additionalAttrMinValue);
                options.maxItemLevel = AsInt256FromFloating(tmpl.additionalAttrMaxValue);
                options.respectChance = true;
                options.allowDuplicateTypes = tmpl.additionalAttrAllowDuplicate;
                options.useValueRangeFilter = true;
                options.category = AttributeCategory::ADDITIONAL;  // 设置为追加属性

                if (sItemAttributesGenerator->GenerateRandomAttributes(item, options))
                {
                    selectedGroups.insert(groupId);
                    appliedCount++;
                }
                else
                    return std::nullopt;
            }

            appliedCount = FilterItemAdditionalAttributesByGroups(item, attributeGroups);
            ItemIdentificationTraceLog("module.itemidentification", ModuleOldFormat("[鉴定定位][追加属性] 固定值多组过滤后数量: 物品ID=%s GUID=%s 模板=%s 数量=%s", item->GetEntry(), itemGuid, tmpl.id, appliedCount).c_str());
        }

        if (appliedCount > 0)
        {
            // ChatHandler(player->GetSession()).PSendSysMessage("物品获得{}个追加属性", appliedCount);
        }
        else
        {
        }

        if (appliedCount > 0)
            for (uint32 group : selectedGroups)
            {
                if (!appliedGroups.empty())
                    appliedGroups += ',';
                appliedGroups += std::to_string(group);
            }
        return appliedCount;
    }
    else
    {
    }
#else
#endif


    return std::nullopt;
}

// 应用追加技能
bool ItemIdentificationSystem::ApplyAdditionalSkills(Player* /*player*/, Item* item,
    IdentificationTemplate const& tmpl, std::string& appliedGroups)
{
    appliedGroups.clear();
    auto groups = ParseCommaSeparatedNumbers(tmpl.itemSkillsGroups);
    if (groups.empty() || !sItemSkillsManager || !sItemSkillsDBHelper)
        return false;

    uint32 count = GenerateRandomNumber(tmpl.additionalSkillMinCount, tmpl.additionalSkillMaxCount);
    std::set<uint32> expected;
    for (auto const& skill : sItemSkillsDBHelper->GetItemSkillsData(item))
        expected.insert(skill.skillTemplateId);
    for (uint32 index = 0; index < count; ++index)
    {
        uint32 group = SelectRandomFromList(groups);
        auto const* skill = sItemSkillsManager->SelectSkillByWeight(item->GetEntry(), group);
        if (!skill)
            return false;
        expected.insert(skill->id);
        sItemSkillsDBHelper->AddSkillToItem(item, skill->id);
    }

    // AddSkillToItem 不返回写入结果，必须清缓存后核对实际保存的技能模板。
    sItemSkillsDBHelper->ClearCache(item->GetGUIDLow());
    auto saved = sItemSkillsDBHelper->GetItemSkillsData(item);
    std::set<uint32> actual;
    std::set<uint32> actualGroups;
    for (auto const& skill : saved)
    {
        actual.insert(skill.skillTemplateId);
        if (auto const* tmplSkill = sItemSkillsManager->GetSkillTemplate(skill.skillTemplateId))
            actualGroups.insert(tmplSkill->group);
    }
    if (actual != expected || actual.size() < tmpl.additionalSkillMinCount)
        return false;
    for (uint32 group : actualGroups)
    {
        if (!appliedGroups.empty())
            appliedGroups += ',';
        appliedGroups += std::to_string(group);
    }
    return true;
}

// 应用符文系统
std::optional<uint32> ItemIdentificationSystem::ApplyRuneSystem(
    Player* /*player*/, Item* item, IdentificationTemplate const& tmpl)
{
#ifdef MODULE_RUNE_SYSTEM

    if (sRuneManager)
    {
        // 创建符文凹槽
        if (tmpl.runeSlotMaxCount > 0)
        {
            uint32 slotCount = GenerateRandomNumber(tmpl.runeSlotMinCount, tmpl.runeSlotMaxCount);

            if (slotCount > 0)
            {
                DebugLog(ModuleOldFormat("尝试为物品创建%s个符文凹槽", slotCount).c_str());

                if (sRuneManager->AddRuneSlotToItem(item, slotCount))
                {
                    DebugLog(ModuleOldFormat("成功为物品创建%s个符文凹槽", slotCount).c_str());
                    // ChatHandler(player->GetSession()).PSendSysMessage("物品获得{}个符文凹槽", slotCount);
                }
                else
                {
                    DebugLog("创建符文凹槽失败");
                    return std::nullopt;
                }
                if (sRuneManager->GetItemRuneSlotCount(item) != slotCount)
                    return std::nullopt;
            }
        }

        // 记录符文组信息（符文组配置用于玩家自行选择镶嵌的符文）
        std::vector<uint32> runeGroups = ParseCommaSeparatedNumbers(tmpl.runeSystemGroups);
        if (!runeGroups.empty())
        {
            DebugLog(ModuleOldFormat("物品符文组配置: %s（玩家可以使用这些组中的符文进行镶嵌）", tmpl.runeSystemGroups).c_str());
            // 注意：当前符文系统设计中，玩家需要自行镶嵌符文
            // 符文组信息可用于提示玩家可以使用哪些符文
            // ChatHandler(player->GetSession()).PSendSysMessage("物品可以镶嵌符文（推荐符文组: {}）", tmpl.runeSystemGroups);
        }
        return sRuneManager->GetItemRuneSlotCount(item);
    }
    else
    {
        DebugLog("符文系统不可用");
    }
#else
    DebugLog("符文系统模块未编译");
#endif
    return std::nullopt;
}

// 应用技能套装（返回实际分配的套装ID，0表示未分配）
std::optional<uint32> ItemIdentificationSystem::ApplySkillSets(
    Player* player, Item* item, IdentificationTemplate const& tmpl, uint32& appliedGroup)
{
    appliedGroup = 0;
    std::vector<uint32> skillSetGroups = ParseCommaSeparatedNumbers(tmpl.skillSetGroups);
    if (skillSetGroups.empty())
    {
        return std::nullopt;
    }

    uint32 rollMin = tmpl.skillSetMinCount;
    uint32 rollMax = tmpl.skillSetMaxCount;
    if (rollMin == 0 && rollMax == 0)
    {
        rollMin = 1;
        rollMax = 1;
    }

    if (rollMax < rollMin)
        rollMax = rollMin;

    uint32 rollCount = GenerateRandomNumber(rollMin, rollMax);
    if (!rollCount)
        return 0;
    if (!tmpl.skillSetAllowDuplicate && rollCount > skillSetGroups.size())
        rollCount = static_cast<uint32>(skillSetGroups.size());

    std::vector<uint32> candidateGroups;
    std::vector<uint32> availableGroups = skillSetGroups;
    for (uint32 i = 0; i < rollCount && !availableGroups.empty(); ++i)
    {
        uint32 selectedGroup = SelectRandomFromList(availableGroups);
        candidateGroups.push_back(selectedGroup);

        if (!tmpl.skillSetAllowDuplicate)
            availableGroups.erase(std::remove(availableGroups.begin(), availableGroups.end(), selectedGroup), availableGroups.end());
    }

    if (candidateGroups.empty())
        return std::nullopt;

    DebugLog(ModuleOldFormat("应用技能套装候选组数量: %s", candidateGroups.size()).c_str());

#ifdef MODULE_ITEM_SETS

    if (!sItemSetsManager)
    {
        DebugLog("套装系统管理器不可用");
        return std::nullopt;
    }

    if (!sItemSetsConfig || !sItemSetsConfig->IsModuleEnabled())
    {
        DebugLog("套装系统未启用");
        return std::nullopt;
    }

    uint32 itemId = item->GetEntry();
    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 playerGuid = player->GetGUID().GetCounter();

    // 1. 检查该物品是否已经分配过套装
    PlayerSetStatus* status = sItemSetsManager->GetPlayerSetStatus(playerGuid);


    if (status && status->ItemSetMap.find(itemGuid) != status->ItemSetMap.end())
    {
        DebugLog(ModuleOldFormat("物品已经分配过套装: GUID=%s", itemGuid).c_str());
        ChatHandler(player->GetSession()).SendSysMessage("该物品已经分配过套装");
        return std::nullopt;
    }

    // 2. 从候选组中选择套装（考虑玩家需求）。套装模块当前一件物品只保存一个套装ID，
    // 因此这里按候选组顺序尝试，成功后即分配该套装。
    uint32 selectedGroup = 0;
    uint32 selectedSetId = 0;
    for (uint32 groupId : candidateGroups)
    {
        selectedSetId = sItemSetsManager->GetRandomSetForGroup(groupId, player);
        if (selectedSetId > 0)
        {
            selectedGroup = groupId;
            break;
        }

        DebugLog(ModuleOldFormat("从套装组%s中未找到满足需求的套装", groupId).c_str());
    }

    if (selectedSetId == 0)
    {
        DebugLog("所有候选套装组都未找到满足需求的套装");

        // 显示组内套装的需求条件
        for (uint32 groupId : candidateGroups)
            sItemSetsManager->ShowGroupRequirements(player, groupId);
        ChatHandler(player->GetSession()).PSendSysMessage("分配套装失败：不满足需求条件。");
        return std::nullopt;
    }

    // 套装需求与鉴定需求统一在整体保存成功后扣除。
    if (!status)
    {
        sItemSetsManager->LoadPlayerSetStatus(player);
        status = sItemSetsManager->GetPlayerSetStatus(playerGuid);
    }
    if (!status)
        return std::nullopt;

    // 获取最新的状态（如果刚创建会在这里初始化）
    status = sItemSetsManager->GetPlayerSetStatus(playerGuid);
    if (status)
    {
        // 记录物品的套装分配
        status->ItemSetMap[itemGuid] = selectedSetId;
    }

    // 5. 获取套装名称并通知玩家
    std::string setName = sItemSetsManager->GetSetName(selectedSetId);


    // ChatHandler(player->GetSession()).PSendSysMessage("物品已分配到套装: {} (组{}, ID{})",
    //     setName.c_str(), selectedGroup, selectedSetId);

    // 6. 保存到数据库
    auto* assign = CharacterDatabase.GetPreparedStatement(CHAR_IDENT_SET_ASSIGN);
    assign->SetArguments(itemId, itemGuid, selectedSetId);
    CharacterDatabase.DirectExecute(assign);
    auto* verify = CharacterDatabase.GetPreparedStatement(CHAR_IDENT_SET_VERIFY);
    verify->SetData(0, itemGuid);
    PreparedQueryResult savedSet = CharacterDatabase.Query(verify);
    if (!savedSet || savedSet->Fetch()[0].Get<uint32>() != selectedSetId)
        return std::nullopt;



    // 8. 重新计算并应用套装效果
    // ⚠️ 性能优化：注释掉立即刷新，改为在鉴定流程结束后统一刷新
    // 原因：RefreshPlayerSetEffects 单次耗时较长
    // 优化后将在 ApplyIdentification 函数末尾统一调用一次
    // sItemSetsManager->RefreshPlayerSetEffects(player);  // 已优化：延迟到鉴定结束后

    DebugLog(ModuleOldFormat("成功将物品分配到套装组: %s，套装ID: %s", selectedGroup, selectedSetId).c_str());

    appliedGroup = selectedGroup;
    return selectedSetId;
#else
    DebugLog("套装系统模块未编译");
    ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000套装系统未安装|r");
    return std::nullopt;
#endif
}

bool ItemIdentificationSystem::ConsumeOperationRequirements(Player* player, Item* item, uint32 requirementId)
{
    return PayItemOperation(player, item, {requirementId}, 0, true);
}

bool ItemIdentificationSystem::RefineItemCategory(Player* player, Item* item, uint32 groupId, uint8 category,
    std::string& error, uint32 requirementId)
{
    if (!player || !item || item->GetOwnerGUID() != player->GetGUID() || category < 1 || category > 4)
    {
        error = "洗炼参数无效";
        return false;
    }

    uint32 itemGuid = item->GetGUIDLow();
    auto const* original = GetIdentificationRecord(itemGuid);
    if (!original)
    {
        error = "物品尚未鉴定，不能洗炼";
        return false;
    }
    // 保存会清除缓存，因此在本地副本上生成，失败时不能污染原鉴定记录。
    ItemIdentificationRecord record = *original;
    auto templateItr = _identificationTemplates.find(record.templateId);
    if (!record.templateId || templateItr == _identificationTemplates.end())
    {
        error = "原鉴定模板不存在，无法洗炼，请联系管理员";
        return false;
    }
    IdentificationTemplate const& tmpl = templateItr->second;
    if (tmpl.group != groupId)
    {
        error = "洗炼配置与装备原鉴定组不匹配";
        return false;
    }
    if ((category == 1 && (tmpl.itemAttributesGroups.empty() || !tmpl.baseAttrMaxCount)) ||
        (category == 2 && (tmpl.itemAttributesAdditionalGroups.empty() || !tmpl.additionalAttrMaxCount)) ||
        (category == 3 && (tmpl.itemSkillsGroups.empty() || !tmpl.additionalSkillMaxCount)) ||
        (category == 4 && (!tmpl.enchantGroup || !tmpl.enchantCount)))
    {
        error = "原鉴定模板未配置该类目，未扣费";
        return false;
    }
    if (!PayItemOperation(player, item, {requirementId}, 0, false))
    {
        error = "洗炼需求无效、合计不足或包含目标装备，未扣费";
        return false;
    }

    if (category == 3)
        sItemSkillsManager->LoadItemSkillsOnDemand(player, item);
    bool const hadSkills = category == 3 &&
        !sItemSkillsManager->GetItemSkillsFromMemory(player->GetGUIDLow(), itemGuid).empty();
    ItemEnchantSnapshot oldEnchants(item);
    bool const attributes = category == 1 || category == 2;
    bool effectsRemoved = false;
    bool enchantChanged = false;
    ItemDataRollback rollback(itemGuid);
    if (!rollback.Capture("item_instance") || !rollback.Capture("物品_鉴定记录") ||
        (attributes && !rollback.Capture("物品属性_数据")) ||
        (category == 3 && !rollback.Capture("_物品技能_数据")))
    {
        error = "无法读取原装备数据，洗炼已取消，未扣费";
        return false;
    }
    rollback.Arm([&, this]
    {
        ClearIdentifiedCache(itemGuid);
        ClearItemCache(item->GetEntry(), itemGuid);
        ItemAttributesDBHelper::EraseCache(itemGuid);
        sItemSkillsDBHelper->ClearCache(itemGuid);
        if (enchantChanged)
            oldEnchants.Restore(player, item, true);
        if (effectsRemoved)
            sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
        player->UpdateAllStats();
    });

    if (attributes && item->IsEquipped())
    {
        sItemAttributesEffects->RemoveItemAttributeEffects(player, item);
        effectsRemoved = true;
    }
    error = "洗炼结果生成或保存失败，未扣费";
    switch (category)
    {
        case 1:
            ItemAttributesDBHelper::SaveBaseAttributes(itemGuid, item->GetEntry(), {}, {});
            if (!ApplyBaseAttributes(player, item, tmpl,
                    record.baseAttrDetails, record.baseAttrCount, record.baseAttrGroup) ||
                !VerifyItemAttributes(itemGuid))
                return false;
            record.hasBaseAttributes = record.baseAttrCount > 0;
            break;
        case 2:
            ItemAttributesDBHelper::SaveAdditionalAttributes(itemGuid, item->GetEntry(), {}, {});
            {
                auto result = ApplyAdditionalAttributes(player, item, tmpl, record.additionalAttrGroups);
                if (!result)
                    return false;
                record.additionalAttrCount = *result;
            }
            if (record.additionalAttrCount < tmpl.additionalAttrMinCount || !VerifyItemAttributes(itemGuid))
                return false;
            record.hasAdditionalAttributes = record.additionalAttrCount > 0;
            break;
        case 3:
            // 保留旧技能内存及效果，确认成功后再切换。
            sItemSkillsDBHelper->ClearItemSkills(item);
            {
                auto* verify = CharacterDatabase.GetPreparedStatement(CHAR_IDENT_SKILLS_COUNT);
                verify->SetData(0, itemGuid);
                PreparedQueryResult remaining = CharacterDatabase.Query(verify);
                if (!remaining || remaining->Fetch()[0].Get<uint64>() != 0)
                    return false;
            }
            if (!ApplyAdditionalSkills(player, item, tmpl, record.skillGroups))
                return false;
            record.hasSkills = !record.skillGroups.empty();
            break;
        case 4:
            if (!RerollIdentificationEnchants(player, item, tmpl, error))
                return false;
            enchantChanged = true;
            break;
    }

    if (!SaveIdentificationRecord(record) || !SaveOperationItemState(item))
        return false;
    if (!ConsumeOperationRequirements(player, item, requirementId))
    {
        error = "洗炼需求扣除前检查失败，未扣费";
        return false;
    }
    rollback.Commit();
    if (effectsRemoved)
        sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
    if (category == 3)
    {
        if (hadSkills && item->IsEquipped())
            sItemSkillsEffects->RemoveItemSkillEffects(player, item);
        sItemSkillsManager->RefreshItemSkillsInMemory(player->GetGUIDLow(), item);
        if (item->IsEquipped())
            sItemSkillsEffects->ApplyItemSkillEffects(player, item);
    }
    item->SetState(ITEM_CHANGED, player);
    ClearItemCache(item->GetEntry(), itemGuid);
    SendAllModuleDataAddon(player, item->GetEntry(), itemGuid);
    player->UpdateAllStats();
    error.clear();
    return true;
}

uint32 ItemIdentificationSystem::GetIdentificationGroupForItem(uint32 itemGuid)
{
    ItemIdentificationRecord* record = GetIdentificationRecord(itemGuid);
    if (!record)
        return 0;

    auto templateItr = _identificationTemplates.find(record->templateId);
    return templateItr != _identificationTemplates.end() ? templateItr->second.group : 0;
}

// 处理鉴定失败
void ItemIdentificationSystem::HandleFailure(Player* player, Item* item)
{
    if (_destroyOnFail)
    {
        // 销毁物品
        player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        ChatHandler(player->GetSession()).PSendSysMessage("物品在鉴定过程中被摧毁了！");
    }
    else
    {
        // 不销毁物品，可以添加其他失败效果
        // 例如：降低物品耐久度
        uint32 maxDurability = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
        if (maxDurability > 0)
        {
            uint32 currentDurability = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
            uint32 newDurability = currentDurability > maxDurability / 4 ? currentDurability - maxDurability / 4 : 1;
            item->SetUInt32Value(ITEM_FIELD_DURABILITY, newDurability);
            ChatHandler(player->GetSession()).PSendSysMessage("物品在鉴定过程中受到了损伤！");
        }
    }
}

// 发送鉴定公告
void ItemIdentificationSystem::SendAnnouncement(Player* player, Item* item, uint32 identificationId)
{
    if (!_enableAnnounce)
        return;

    // 查询公告模板
    std::string query = "SELECT `公告模板` FROM `_模板_物品鉴定` WHERE `id` = " + Acore::ToString(identificationId);

    QueryResult result = WorldDatabase.Query(query.c_str());
    if (!result)
        return;

    Field* fields = result->Fetch();
    uint32 announceTemplateId = fields[0].Get<uint32>();

    if (announceTemplateId == 0)
        return;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return;

    std::string message = "|cffff0000[物品鉴定系统]|r 恭喜玩家 |cff00ff00" + player->GetName() + "|r 成功鉴定出 ";

    switch (proto->Quality)
    {
        case ITEM_QUALITY_POOR:
            message += "|cff9d9d9d";
            break;
        case ITEM_QUALITY_NORMAL:
            message += "|cffffffff";
            break;
        case ITEM_QUALITY_UNCOMMON:
            message += "|cff1eff00";
            break;
        case ITEM_QUALITY_RARE:
            message += "|cff0070dd";
            break;
        case ITEM_QUALITY_EPIC:
            message += "|cffa335ee";
            break;
        case ITEM_QUALITY_LEGENDARY:
            message += "|cffff8000";
            break;
        default:
            message += "|cffffffff";
            break;
    }

    message += proto->Name1 + "|r！";
    sHelper->SendSerMessage("%s", message.c_str());

    if (_debugMode)
        DebugLog(ModuleOldFormat("通过旧核心广播发送鉴定公告，模板ID: %s", announceTemplateId).c_str());
}

// 调试日志已在头文件中实现为模板函数

// 命令处理类实现
ItemIdentificationCommandScript::ItemIdentificationCommandScript() : CommandScript("ItemIdentificationCommandScript") { }

std::vector<Acore::ChatCommands::ChatCommandBuilder> ItemIdentificationCommandScript::GetCommands() const
{
    using namespace Acore::ChatCommands;

    // 鉴定子命令
    static ChatCommandTable identificationSubCommands =
    {
        // "查询" 命令已删除 - 使用 addon 消息自动查询替代
        { "批量查询", HandleBatchQueryCommand, SEC_PLAYER, Console::No },  // 批量查询命令
        { "性能统计", HandlePerformanceStatsCommand, SEC_ADMINISTRATOR, Console::No },  // 性能监控
        { "重载", HandleReloadCommand, SEC_ADMINISTRATOR, Console::Yes },  // 重载鉴定配置和模板
        // ========== 【新增】手动鉴定相关命令 ==========
        { "手动", HandleManualIdentifyCommand, SEC_PLAYER, Console::No },  // 手动鉴定单个物品
        { "回收", HandleRecycleCommand, SEC_PLAYER, Console::No },
        { "批量回收", HandleBatchRecycleCommand, SEC_PLAYER, Console::No },
        { "待鉴定", HandleListPendingCommand, SEC_PLAYER, Console::No },   // 查询待鉴定列表
        { "批量鉴定", HandleBatchIdentifyCommand, SEC_PLAYER, Console::No }, // 批量鉴定所有
        { "界面", HandleOpenUICommand, SEC_PLAYER, Console::No },          // 打开UI界面
        { "ui", HandleOpenUICommand, SEC_PLAYER, Console::No }             // 打开UI界面
    };

    // 主命令 - 支持直接鉴定和子命令
    static ChatCommandTable commandTable =
    {
        { "鉴定物品", HandleIdentifyCommand, SEC_PLAYER, Console::No },
        { "identifyitem", HandleIdentifyCommand, SEC_PLAYER, Console::No },
        { "鉴定", identificationSubCommands }
    };

    return commandTable;
}

bool ItemIdentificationCommandScript::HandleIdentifyCommand(ChatHandler* handler, const char* args)
{
    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 检查系统是否启用
    if (!sItemIdentificationSystem->_enabled)
    {
        handler->SendSysMessage("物品鉴定系统当前已禁用");
        return true;
    }

    // 获取参数
    if (!*args)
    {
        handler->SendSysMessage("用法: .鉴定物品 <组ID> <物品ID>");
        handler->SendSysMessage("用法: .鉴定物品 查询 [all]");
        handler->SendSysMessage("示例: .鉴定物品 1 25  (使用组1鉴定物品25)");
        handler->SendSysMessage("示例: .鉴定物品 查询 all  (查询所有已鉴定物品)");
        return true;
    }

    // 删除旧的查询子命令 - 现在使用 addon 消息自动查询
    // 旧代码：if (firstArg == "查询") HandleQueryAttributesCommand(...) - 已删除

    // 解析鉴定命令参数: <组ID> <物品ID>
    // 【安全修复】使用std::strtok并增加输入验证
    char* groupIdStr = strtok((char*)args, " ");
    char* itemIdStr = strtok(nullptr, " ");

    if (!groupIdStr || !itemIdStr)
    {
        handler->SendSysMessage("参数不足！用法: .鉴定物品 <组ID> <物品ID>");
        return true;
    }

    // 【安全修复】验证输入是否为有效数字
    char* endPtr = nullptr;
    long groupIdLong = strtol(groupIdStr, &endPtr, 10);
    if (endPtr == groupIdStr || *endPtr != '\0' || groupIdLong < 0 || groupIdLong > UINT32_MAX)
    {
        handler->SendSysMessage("无效的组ID，必须是非负整数");
        return true;
    }

    endPtr = nullptr;
    long itemIdLong = strtol(itemIdStr, &endPtr, 10);
    if (endPtr == itemIdStr || *endPtr != '\0' || itemIdLong <= 0 || itemIdLong > UINT32_MAX)
    {
        handler->SendSysMessage("无效的物品ID，必须是正整数");
        return true;
    }

    uint32 groupId = static_cast<uint32>(groupIdLong);
    uint32 itemId = static_cast<uint32>(itemIdLong);

    // 检查物品模板是否存在
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("物品ID %s 不存在", itemId).c_str());
        return true;
    }

    // 在玩家背包和装备栏中查找该物品
    Item* item = nullptr;

    // 优先查找背包中的物品
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == itemId)
        {
            item = pItem;
            break;
        }
    }

    // 如果背包没找到，查找背包袋子中的物品
    if (!item)
    {
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* pBag = player->GetBagByPos(i))
            {
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                {
                    Item* pItem = pBag->GetItemByPos(j);
                    if (pItem && pItem->GetEntry() == itemId)
                    {
                        item = pItem;
                        break;
                    }
                }
                if (item) break;
            }
        }
    }

    // 如果还没找到，查找装备栏（通常不鉴定已装备的）
    if (!item)
    {
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pItem && pItem->GetEntry() == itemId)
            {
                item = pItem;
                break;
            }
        }
    }

    if (!item)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("您的背包中没有物品ID为 %s 的物品", itemId).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("物品名称: %s", itemTemplate->Name1).c_str());
        return true;
    }

    // 尝试鉴定物品
    handler->PSendSysMessage("%s", ModuleOldFormat("正在鉴定物品: %s [%s]，使用组ID: %s", itemTemplate->Name1, itemId, groupId).c_str());

    if (sItemIdentificationSystem->IdentifyItem(player, item, groupId))
    {
        handler->SendSysMessage("物品鉴定成功！");
    }
    else
    {
        // 错误消息已在IdentifyItem方法中发送
    }

    return true;
}

// HandleQueryAttributesCommand 已删除 - 现在使用 addon 格式的 HandleAddonBatchQuery
// 客户端不再通过命令查询，而是自动发送 addon 消息 (UITQ QUERY) 来获取数据

// HandleQueryCommand 已删除 - 使用 HandleAddonBatchQuery (addon格式) 替代旧的单独查询

// 模块加载器实现
ItemIdentificationSystemModuleLoader::ItemIdentificationSystemModuleLoader() : WorldScript("ItemIdentificationSystemModuleLoader"), _loaded(false), _startTime(0) { }

void ItemIdentificationSystemModuleLoader::OnAfterConfigLoad(bool reload)
{
    // 如果是重新加载配置，才执行加载操作
    // 初始加载将在服务器完全启动后由OnUpdate执行
    if (reload)
    {
        sItemIdentificationSystem->LoadConfig(reload);
    }
}

void ItemIdentificationSystemModuleLoader::OnBeforeWorldInitialized()
{
    // 此时 world 网络监听尚未启动，玩家还无法进入游戏。
    // 启动全量清理放在这里，避免与玩家登录/小退/拾取/保存事务并发。
    CleanupOrphanedItemData();
}

void ItemIdentificationSystemModuleLoader::OnUpdate(uint32 diff)
{
    // 延迟1秒初始化模块，确保服务器完全启动
    if (!_loaded)
    {
        if (_startTime == 0)
            _startTime = getMSTime();
        else if (getMSTime() - _startTime > 1000)
        {
            _loaded = true;

            // 【修复】命令脚本已在 AddItemIdentificationSystemScripts() 中注册，移除重复注册
            // new ItemIdentificationCommandScript();  // 已移除，避免重复注册导致命令执行两次

            // 注册玩家脚本
            new ItemIdentificationPlayerScript();

            // 初始化模块
            sItemIdentificationSystem->Initialize();

            // 显示加载信息
            LOG_INFO("server.loading", "→_模板_物品鉴定 加载完成√");
        }
    }

    // 定期清理过期缓存（每5分钟执行一次）
    static uint32 cacheCleanTimer = 0;
    cacheCleanTimer += diff;

    if (cacheCleanTimer >= 300000)  // 5分钟 = 300000ms
    {
        cacheCleanTimer = 0;
        sItemIdentificationSystem->CleanExpiredCache();
    }

    // ★★★ 定期清理待鉴定物品标记表的孤立数据 ★★★
    // 原因：玩家拾取物品后没有手动鉴定就删除/出售/交易物品，导致待鉴定标记永久保留
    // 规则：每30分钟进入一次“待清理”状态，但只有连续无人在线5分钟才真正执行
    static uint32 pendingCleanTimer = 0;
    static uint32 pendingEmptyTimer = 0;
    static bool pendingCleanupDue = false;
    static bool pendingCleanupDeferredLogged = false;

    if (!pendingCleanupDue)
    {
        pendingCleanTimer += diff;

        if (pendingCleanTimer >= 1800000)  // 30分钟 = 1800000ms
            pendingCleanupDue = true;
    }

    if (pendingCleanupDue)
    {
        uint32 onlinePlayerCount = sWorldSessionMgr->GetActiveSessionCount();
        if (onlinePlayerCount == 0)
        {
            pendingEmptyTimer += diff;
            if (pendingEmptyTimer >= 300000)
            {
                pendingCleanTimer = 0;
                pendingEmptyTimer = 0;
                pendingCleanupDue = false;
                pendingCleanupDeferredLogged = false;
                CleanupPendingIdentificationData();
            }
        }
        else
        {
            pendingEmptyTimer = 0;
            if (!pendingCleanupDeferredLogged)
            {
                pendingCleanupDeferredLogged = true;
                LOG_INFO("module.itemidentification", ModuleOldFormat("[定期清理] 当前仍有 %s 名玩家在线，延后执行待鉴定孤立数据清理。", onlinePlayerCount).c_str());
            }
        }
    }
}

// 启动阶段全量清理孤立的物品数据（服务器启动时执行一次）
void ItemIdentificationSystemModuleLoader::CleanupOrphanedItemData()
{
    LOG_INFO("module.itemidentification", "[启动清理] 开始全量清理孤立的物品数据...");

    auto startTime = std::chrono::high_resolution_clock::now();
    uint32 totalCleaned = 0;

    // 说明：
    // 1) 用 JOIN 代替 NOT IN 子查询，降低锁竞争
    // 2) 启动阶段同步提交，确保 world 网络监听开始前清理已经完成
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // ★★★ 关键修复：清理待鉴定物品标记表的孤立数据 ★★★
    // 原因：玩家拾取物品后没有手动鉴定就删除/出售/交易物品，导致待鉴定标记永久保留
    // 注意：bag=200 是飞升系统虚拟背包，清理时必须视为有效引用，避免误删飞升装备扩展属性
    trans->Append(
        "DELETE p FROM `待鉴定物品标记` p "
        "LEFT JOIN `item_instance` i ON p.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = p.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE r FROM `物品_鉴定记录` r "
        "LEFT JOIN `item_instance` i ON r.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = r.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE a FROM `物品属性_数据` a "
        "LEFT JOIN `item_instance` i ON a.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = a.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE g FROM `物品成长_玩家记录` g "
        "LEFT JOIN `item_instance` i ON g.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = g.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE e FROM `物品强化_记录` e "
        "LEFT JOIN `item_instance` i ON e.`guid` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = e.`guid` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE s FROM `_物品技能_数据` s "
        "LEFT JOIN `item_instance` i ON s.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = s.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE r FROM `符文系统_数据` r "
        "LEFT JOIN `item_instance` i ON r.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = r.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE p FROM `_物品套装_数据` p "
        "LEFT JOIN `item_instance` i ON p.`物品GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = p.`物品GUID` AND ci.`bag` = 200 "
        "WHERE i.`guid` IS NULL AND ci.`item` IS NULL");

    trans->Append(
        "DELETE h FROM `玩家装备属性增强` h "
        "LEFT JOIN `item_instance` i ON h.`装备GUID` = i.`guid` "
        "LEFT JOIN `character_inventory` ci ON ci.`item` = h.`装备GUID` "
        "LEFT JOIN `mail_items` mi ON mi.`item_guid` = h.`装备GUID` "
        "LEFT JOIN `auctionhouse` ah ON ah.`itemguid` = h.`装备GUID` "
        "LEFT JOIN `guild_bank_item` gbi ON gbi.`item_guid` = h.`装备GUID` "
        "LEFT JOIN `item_refund_instance` iri ON iri.`item_guid` = h.`装备GUID` "
        "LEFT JOIN `character_gifts` cg ON cg.`item_guid` = h.`装备GUID` "
        "LEFT JOIN `character_equipmentsets` ces ON h.`装备GUID` IN ("
        "ces.`item0`, ces.`item1`, ces.`item2`, ces.`item3`, ces.`item4`, ces.`item5`, "
        "ces.`item6`, ces.`item7`, ces.`item8`, ces.`item9`, ces.`item10`, ces.`item11`, "
        "ces.`item12`, ces.`item13`, ces.`item14`, ces.`item15`, ces.`item16`, ces.`item17`, "
        "ces.`item18`) "
        // 双保险：item_instance 不存在 + 所有持久化引用表也不存在，才允许删除倍率行。
        // 之前只看 item_instance + bag=200，一旦上游清理误删 item_instance，
        // 这里就会把上架/邮件中装备的倍率属性一起清掉，玩家重启后会反映"装备倍率丢了"。
        "WHERE i.`guid` IS NULL "
        "AND ci.`item` IS NULL "
        "AND mi.`item_guid` IS NULL "
        "AND ah.`itemguid` IS NULL "
        "AND gbi.`item_guid` IS NULL "
        "AND iri.`item_guid` IS NULL "
        "AND cg.`item_guid` IS NULL "
        "AND ces.`setguid` IS NULL");

    CharacterDatabase.DirectCommitTransaction(trans);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    LOG_INFO("module.itemidentification", ModuleOldFormat("[启动清理] 全量孤立数据清理已同步完成，耗时 %s ms", duration).c_str());
}

// ★★★ 定期清理待鉴定物品标记表的孤立数据 ★★★
// 原因：玩家拾取物品后没有手动鉴定就删除/出售/交易物品，导致待鉴定标记永久保留
// 这个函数每30分钟执行一次，确保数据库不会无限膨胀
void ItemIdentificationSystemModuleLoader::CleanupPendingIdentificationData()
{
    LOG_INFO("module.itemidentification", "[定期清理] 开始清理待鉴定物品标记表的孤立数据...");

    auto startTime = std::chrono::high_resolution_clock::now();
    static constexpr uint32 kBatchSize = 5000;
    static constexpr uint32 kMaxBatchesPerRun = 10;

    uint32 totalDeleted = 0;
    bool hasMoreOrphans = false;

    for (uint32 batch = 0; batch < kMaxBatchesPerRun; ++batch)
    {
        std::vector<uint32> orphanGuids;
        orphanGuids.reserve(kBatchSize);

        QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT p.`物品GUID` "
            "FROM `待鉴定物品标记` p "
            "LEFT JOIN `item_instance` i ON p.`物品GUID` = i.`guid` "
            "LEFT JOIN `character_inventory` ci ON ci.`item` = p.`物品GUID` AND ci.`bag` = 200 "
            "WHERE i.`guid` IS NULL AND ci.`item` IS NULL "
            "ORDER BY p.`物品GUID` ASC "
            "LIMIT %s", kBatchSize).c_str());

        if (!result)
        {
            hasMoreOrphans = false;
            break;
        }

        do
        {
            orphanGuids.push_back(result->Fetch()[0].Get<uint32>());
        } while (result->NextRow());

        if (orphanGuids.empty())
        {
            hasMoreOrphans = false;
            break;
        }

        std::ostringstream deleteSql;
        deleteSql << "DELETE FROM `待鉴定物品标记` WHERE `物品GUID` IN (";
        for (size_t i = 0; i < orphanGuids.size(); ++i)
        {
            if (i > 0)
                deleteSql << ",";
            deleteSql << orphanGuids[i];
        }
        deleteSql << ")";

        // 全服无人在线时，使用同步分批删除，避免一次性大事务长期占用表锁。
        CharacterDatabase.DirectExecute(deleteSql.str());
        totalDeleted += static_cast<uint32>(orphanGuids.size());

        if (orphanGuids.size() < kBatchSize)
        {
            hasMoreOrphans = false;
            break;
        }

        hasMoreOrphans = true;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    LOG_INFO("module.itemidentification", ModuleOldFormat("[定期清理] 待鉴定孤立数据分批清理完成，本次删除 %s 条，耗时 %s ms%s", totalDeleted, duration, hasMoreOrphans ? "，仍有剩余孤立数据，等待下一个空闲周期继续清理" : "").c_str());
}

// 【关键修复】服务器关闭时保存数据
void ItemIdentificationSystemModuleLoader::OnShutdownInitiate(ShutdownExitCode /*code*/, ShutdownMask /*mask*/)
{
    LOG_INFO("module.itemidentification", ">> 鉴定系统: 服务器正在关闭，确保所有数据已保存...");

    // 鉴定系统的数据通常在鉴定时立即保存，但这里可以刷新缓存
    // 确保所有待处理的数据库操作完成
    // 由于使用的是异步Execute，这些操作已经在队列中了

    LOG_INFO("module.itemidentification", ">> 鉴定系统: 数据保存完成");
}

void ItemIdentificationSystem::HandleSellReward(Player* player, Item* item, uint32 count)
{
    if (!player || !item || !count)
        return;

    ItemIdentificationRecord* record = GetIdentificationRecord(item->GetGUID().GetCounter());
    if (!record)
        return;

    auto templateItr = _identificationTemplates.find(record->templateId);
    if (templateItr == _identificationTemplates.end() || !templateItr->second.sellRewardTemplate)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    sHelper->DoTheReward(player, templateItr->second.sellRewardTemplate, "鉴定装备售卖奖励", count);
    CharacterDatabase.DirectExecute(ModuleOldFormat(
        "DELETE FROM `物品_鉴定记录` WHERE `物品GUID` = %s", itemGuid).c_str());
    ClearIdentifiedCache(itemGuid);
    ClearItemCache(item->GetEntry(), itemGuid);
}

bool ItemIdentificationSystem::RecycleItem(Player* player, Item* item)
{
    if (!player || !item || item->GetOwnerGUID() != player->GetGUID() || item->IsEquipped())
        return false;

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemIdentificationRecord* record = GetIdentificationRecord(itemGuid);
    if (!record)
        return false;

    auto templateItr = _identificationTemplates.find(record->templateId);
    if (templateItr == _identificationTemplates.end() || !templateItr->second.recycleRewardTemplate)
        return false;

    uint32 rewardTemplate = templateItr->second.recycleRewardTemplate;
    uint32 count = item->GetCount();
    uint8 bag = item->GetBagSlot();
    uint8 slot = item->GetSlot();

    player->DestroyItem(bag, slot, true);
    ClearIdentifiedCache(itemGuid);
    CharacterDatabase.DirectExecute(ModuleOldFormat(
        "DELETE FROM `物品_鉴定记录` WHERE `物品GUID` = %s", itemGuid).c_str());
    sHelper->DoTheReward(player, rewardTemplate, "鉴定装备回收奖励", count);
    return true;
}

uint32 ItemIdentificationSystem::RecycleAllBackpackItems(Player* player, uint32& skippedCount, uint32 identificationLevel)
{
    skippedCount = 0;
    if (!player)
        return 0;

    std::unordered_map<uint32, Item*> itemsByGuid = BuildPlayerItemGuidIndex(player);
    std::vector<uint32> backpackGuids;
    backpackGuids.reserve(itemsByGuid.size());
    for (auto const& pair : itemsByGuid)
    {
        Item* item = pair.second;
        if (item && !item->IsEquipped())
            backpackGuids.push_back(pair.first);
    }

    if (backpackGuids.empty())
        return 0;

    std::ostringstream query;
    query << "SELECT `物品GUID`, `鉴定模板ID` FROM `物品_鉴定记录` WHERE `玩家GUID` = "
          << player->GetGUID().GetCounter() << " AND `物品GUID` IN (";
    for (size_t index = 0; index < backpackGuids.size(); ++index)
    {
        if (index > 0)
            query << ',';
        query << backpackGuids[index];
    }
    query << ')';

    struct RecycleEntry
    {
        uint32 itemGuid;
        uint32 itemEntry;
        uint32 rewardTemplate;
        uint32 count;
        uint8 bag;
        uint8 slot;
    };

    std::vector<RecycleEntry> recycleEntries;
    QueryResult result = CharacterDatabase.Query(query.str().c_str());
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 itemGuid = fields[0].Get<uint32>();
            uint32 templateId = fields[1].Get<uint32>();

            auto itemItr = itemsByGuid.find(itemGuid);
            auto templateItr = _identificationTemplates.find(templateId);
            if (itemItr == itemsByGuid.end() || !itemItr->second || itemItr->second->IsEquipped() ||
                templateItr == _identificationTemplates.end())
            {
                ++skippedCount;
                continue;
            }

            if (identificationLevel > 0 && templateItr->second.level != identificationLevel)
                continue;

            if (!templateItr->second.recycleRewardTemplate)
            {
                ++skippedCount;
                continue;
            }

            Item* item = itemItr->second;
            recycleEntries.push_back({
                itemGuid,
                item->GetEntry(),
                templateItr->second.recycleRewardTemplate,
                item->GetCount(),
                item->GetBagSlot(),
                item->GetSlot()
            });
        } while (result->NextRow());
    }

    if (recycleEntries.empty())
        return 0;

    std::ostringstream deleteQuery;
    deleteQuery << "DELETE FROM `物品_鉴定记录` WHERE `物品GUID` IN (";
    for (size_t index = 0; index < recycleEntries.size(); ++index)
    {
        RecycleEntry const& entry = recycleEntries[index];
        if (index > 0)
            deleteQuery << ',';
        deleteQuery << entry.itemGuid;

        player->DestroyItem(entry.bag, entry.slot, true);
        ClearIdentifiedCache(entry.itemGuid);
        ClearItemCache(entry.itemEntry, entry.itemGuid);
    }
    deleteQuery << ')';
    CharacterDatabase.DirectExecute(deleteQuery.str().c_str());

    for (RecycleEntry const& entry : recycleEntries)
        sHelper->DoTheReward(player, entry.rewardTemplate, "鉴定装备批量回收奖励", entry.count);

    return static_cast<uint32>(recycleEntries.size());
}

// 玩家脚本实现
ItemIdentificationPlayerScript::ItemIdentificationPlayerScript() : PlayerScript("ItemIdentificationPlayerScript") { }

void ItemIdentificationPlayerScript::OnSellItem(Player* player, Item* item, uint32 count)
{
    sItemIdentificationSystem->HandleSellReward(player, item, count);
}

void ItemIdentificationPlayerScript::OnLogin(Player* player)
{
    if (!player || !sItemIdentificationSystem->_enabled)
        return;

    // 【性能优化】禁用登录时的预加载，改为完全按需加载
    // 原因：多人同时登录时，即使异步查询也会导致数据库过载
    // 100个玩家 × 50个物品 × 1次UNION查询 = 5000次并发查询 → 数据库卡死
    //
    // 新策略：客户端通过 addon 消息按需查询，已有24小时缓存，性能足够
    // sItemIdentificationSystem->PreloadPlayerEquipment(player);  // 已禁用

    // 删除旧的登录时主动发送逻辑 - 现在客户端会在需要时通过 addon 消息自动查询
    // 旧代码：收集所有已鉴定物品并通过 SendIdentificationDataToClient 发送 - 已删除
    // 新逻辑：客户端悬停物品时自动发送 UITQ QUERY 请求，服务器通过 HandleAddonBatchQuery 响应

    // 【关键修复】在登录完成后立即执行一次统一的属性刷新
    // 原因：OnEquip 中为了优化性能，使用 isBeingLoaded() 跳过了属性刷新
    // 虽然幻境系统的OnLogin会通过延迟事件刷新，但延迟时间过长可能导致客户端超时
    // 必须在这里立即刷新一次，确保玩家能正常进入游戏
    // 后续幻境系统的延迟刷新会再次更新，不会有冲突
    player->UpdateAllStats();
    player->UpdateAttackPowerAndDamage();
    player->UpdateAttackPowerAndDamage(true);
}

// 鉴定记录管理实现
bool ItemIdentificationSystem::IsItemIdentified(uint32 itemGuid)
{
    if (!_enabled)
        return false;

    std::lock_guard<std::mutex> lock(_identifiedCacheMutex);

    // 模块在世界启动后延迟初始化。初始化前的装备加载属于正常时序，
    // 此时按“未鉴定”处理即可，不能为每件物品输出错误日志。
    if (!_cacheInitialized)
        return false;

    return _identifiedItemsCache.find(itemGuid) != _identifiedItemsCache.end();
}

bool ItemIdentificationSystem::SaveIdentificationRecord(ItemIdentificationRecord record)
{
    // 【审计修复】对字符串字段进行SQL转义，防止SQL注入和语法错误
    std::string escapedBaseAttrDetails = record.baseAttrDetails;
    std::string escapedAdditionalAttrGroups = record.additionalAttrGroups;
    std::string escapedSkillGroups = record.skillGroups;

    CharacterDatabase.EscapeString(escapedBaseAttrDetails);
    CharacterDatabase.EscapeString(escapedAdditionalAttrGroups);
    CharacterDatabase.EscapeString(escapedSkillGroups);

    // 使用 INSERT ... ON DUPLICATE KEY UPDATE 避免重复键错误
    // 【修复】使用 DirectExecute 同步写入数据库
    // 原因：异步写入时，后续的 SendAllModuleDataAddon 查询可能还没有写入完成
    // 导致鉴定后需要重启服务器才能看到数据
    std::string insertSql = ModuleOldFormat("INSERT INTO 物品_鉴定记录 ("
        "玩家GUID, 物品GUID, 物品ID, 鉴定模板ID, "
        "是否获得成长, 成长组ID, "
        "是否获得强化, 强化组ID, "
        "是否获得基础属性, 基础属性数量, 基础属性组ID, 基础属性详情, "
        "是否获得追加属性, 追加属性数量, 追加属性组ID列表, "
        "是否获得符文凹槽, 符文凹槽数量, "
        "是否获得技能, 技能组ID列表, "
        "是否获得套装, 套装组ID, 套装ID, "
        "消耗金币, 成功率"
        ") VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, '%s', %s, %s, '%s', %s, %s, %s, '%s', %s, %s, %s, %s, %s) "
        "ON DUPLICATE KEY UPDATE "
        "玩家GUID = VALUES(玩家GUID), "
        "物品ID = VALUES(物品ID), "
        "鉴定模板ID = VALUES(鉴定模板ID), "
        "是否获得成长 = VALUES(是否获得成长), "
        "成长组ID = VALUES(成长组ID), "
        "是否获得强化 = VALUES(是否获得强化), "
        "强化组ID = VALUES(强化组ID), "
        "是否获得基础属性 = VALUES(是否获得基础属性), "
        "基础属性数量 = VALUES(基础属性数量), "
        "基础属性组ID = VALUES(基础属性组ID), "
        "基础属性详情 = VALUES(基础属性详情), "
        "是否获得追加属性 = VALUES(是否获得追加属性), "
        "追加属性数量 = VALUES(追加属性数量), "
        "追加属性组ID列表 = VALUES(追加属性组ID列表), "
        "是否获得符文凹槽 = VALUES(是否获得符文凹槽), "
        "符文凹槽数量 = VALUES(符文凹槽数量), "
        "是否获得技能 = VALUES(是否获得技能), "
        "技能组ID列表 = VALUES(技能组ID列表), "
        "是否获得套装 = VALUES(是否获得套装), "
        "套装组ID = VALUES(套装组ID), "
        "套装ID = VALUES(套装ID), "
        "消耗金币 = VALUES(消耗金币), "
        "成功率 = VALUES(成功率)", record.playerGuid, record.itemGuid, record.itemEntry, record.templateId, record.hasGrowth ? 1 : 0, record.growthGroup, record.hasEnhancement ? 1 : 0, record.enhancementGroup, record.hasBaseAttributes ? 1 : 0, record.baseAttrCount, record.baseAttrGroup, escapedBaseAttrDetails, record.hasAdditionalAttributes ? 1 : 0, record.additionalAttrCount, escapedAdditionalAttrGroups, record.hasRuneSlots ? 1 : 0, record.runeSlotCount, record.hasSkills ? 1 : 0, escapedSkillGroups, record.hasSet ? 1 : 0, record.setGroup, record.setId, record.costGold, record.successRate);

    CharacterDatabase.DirectExecute(insertSql.c_str());

    // 绕过旧缓存，校验整条结果；旧记录仍存在不能证明本次更新成功。
    {
        std::lock_guard<std::mutex> lock(_identificationRecordCacheMutex);
        _identificationRecordCache.erase(record.itemGuid);
    }
    ItemIdentificationRecord const* stored = GetIdentificationRecord(record.itemGuid);
    if (stored)
        record.id = stored->id;
    if (!stored || *stored != record)
    {
        ClearIdentifiedCache(record.itemGuid);
        LOG_ERROR("module.itemidentification", "物品 {} 鉴定记录写入后校验不一致", record.itemGuid);
        return false;
    }

    // 【审计修复】使用锁保护缓存写入，防止数据竞争
    {
        std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
        _identifiedItemsCache.insert(record.itemGuid);
    }
    {
        std::lock_guard<std::mutex> lock(_identificationRecordCacheMutex);
        _identificationRecordCache[record.itemGuid] = record;
    }

    DebugLog(ModuleOldFormat("保存鉴定记录: 玩家GUID=%s, 物品GUID=%s, 模板ID=%s", record.playerGuid, record.itemGuid, record.templateId).c_str());
    return true;
}

ItemIdentificationRecord* ItemIdentificationSystem::GetIdentificationRecord(uint32 itemGuid)
{
    {
        std::lock_guard<std::mutex> lock(_identificationRecordCacheMutex);
        auto itr = _identificationRecordCache.find(itemGuid);
        if (itr != _identificationRecordCache.end())
            return &itr->second;
    }

    QueryResult result = CharacterDatabase.Query(ModuleOldFormat(
        "SELECT `记录ID`, `玩家GUID`, `物品GUID`, `物品ID`, `鉴定模板ID`, "
        "`是否获得成长`, `成长组ID`, `是否获得强化`, `强化组ID`, "
        "`是否获得基础属性`, `基础属性数量`, `基础属性组ID`, `基础属性详情`, "
        "`是否获得追加属性`, `追加属性数量`, `追加属性组ID列表`, "
        "`是否获得符文凹槽`, `符文凹槽数量`, `是否获得技能`, `技能组ID列表`, "
        "`是否获得套装`, `套装组ID`, `套装ID`, `消耗金币`, `成功率` "
        "FROM `物品_鉴定记录` WHERE `物品GUID` = %s LIMIT 1", itemGuid).c_str());
    if (!result)
        return nullptr;

    Field* fields = result->Fetch();
    ItemIdentificationRecord record{};
    record.id = fields[0].Get<uint32>();
    record.playerGuid = fields[1].Get<uint32>();
    record.itemGuid = fields[2].Get<uint32>();
    record.itemEntry = fields[3].Get<uint32>();
    record.templateId = fields[4].Get<uint32>();
    record.hasGrowth = fields[5].Get<uint8>() != 0;
    record.growthGroup = fields[6].Get<uint32>();
    record.hasEnhancement = fields[7].Get<uint8>() != 0;
    record.enhancementGroup = fields[8].Get<uint32>();
    record.hasBaseAttributes = fields[9].Get<uint8>() != 0;
    record.baseAttrCount = fields[10].Get<uint32>();
    record.baseAttrGroup = fields[11].Get<uint32>();
    record.baseAttrDetails = fields[12].Get<std::string>();
    record.hasAdditionalAttributes = fields[13].Get<uint8>() != 0;
    record.additionalAttrCount = fields[14].Get<uint32>();
    record.additionalAttrGroups = fields[15].Get<std::string>();
    record.hasRuneSlots = fields[16].Get<uint8>() != 0;
    record.runeSlotCount = fields[17].Get<uint32>();
    record.hasSkills = fields[18].Get<uint8>() != 0;
    record.skillGroups = fields[19].Get<std::string>();
    record.hasSet = fields[20].Get<uint8>() != 0;
    record.setGroup = fields[21].Get<uint32>();
    record.setId = fields[22].Get<uint32>();
    record.costGold = fields[23].Get<uint64>();
    record.successRate = fields[24].Get<uint32>();

    std::lock_guard<std::mutex> lock(_identificationRecordCacheMutex);
    auto inserted = _identificationRecordCache.emplace(itemGuid, std::move(record));
    return &inserted.first->second;
}

// SendAllModuleData 和 SendIdentificationDataToClient 已删除
// 现在使用 addon 格式的 HandleAddonBatchQuery 替代旧的单独发送逻辑
// 客户端通过 addon 消息 (UITQ) 接收批量数据，不再使用聊天消息

void ItemIdentificationSystem::RefreshItem(Player* player, Item* item)
{
    if (!player || !item)
        return;

    // 保存物品到数据库
    item->SaveToDB(nullptr);

    // 如果物品已装备
    if (item->IsEquipped())
    {
        // 更新装备槽位显示
        // 刷新物品显示到客户端
        player->SetVisibleItemSlot(item->GetSlot(), item);

        // 只在玩家完全加载且在世界中时才应用属性效果
        // 避免在登录加载阶段调用导致空指针崩溃
#ifdef MODULE_ITEM_ATTRIBUTES
        if (!player->isBeingLoaded() && player->IsInWorld() && player->GetSession())
        {
            if (sItemAttributesEffects)
            {
                // 重新同步该装备的自定义属性效果（先移除再应用一次）
                sItemAttributesEffects->RemoveItemAttributeEffects(player, item);
                sItemAttributesEffects->ApplyItemAttributeEffects(player, item);
            }
        }
#endif

        DebugLog(ModuleOldFormat("刷新已装备物品: GUID=%s, 槽位=%s", item->GetGUID().GetCounter(), item->GetSlot()).c_str());
    }
    else
    {
        // 背包物品只需保存，不需要更新显示
        item->SetState(ITEM_CHANGED, player);

        DebugLog(ModuleOldFormat("刷新背包物品: GUID=%s", item->GetGUID().GetCounter()).c_str());
    }

}

// 物品装备脚本 - 阻止已鉴定物品的官方属性生效
class ItemIdentificationEquipScript : public PlayerScript
{
private:
    // 存储每个玩家每个槽位的已鉴定物品信息
    // Key: PlayerGUID, Value: map<slot, pair<itemGUID, itemEntry>>
    std::map<uint64, std::map<uint8, std::pair<uint64, uint32>>> _identifiedEquippedItems;

public:
    ItemIdentificationEquipScript() : PlayerScript("ItemIdentificationEquipScript") {}

    // 在装备物品后触发：这里只记录装备信息，不再直接修改基础属性
    void OnEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool /*update*/) override
    {
        if (!player || !item)
            return;

        uint32 itemGuid = item->GetGUID().GetCounter();
        if (!sItemIdentificationSystem->IsItemIdentified(itemGuid))
            return;

        uint64 playerGuid = player->GetGUID().GetCounter();
        _identifiedEquippedItems[playerGuid][slot] = std::make_pair(itemGuid, item->GetEntry());
    }

    // 在脱下装备时触发：仅清理记录
    void OnAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* item) override
    {
        if (!player)
            return;

        if (item == nullptr)
        {
            uint64 playerGuid = player->GetGUID().GetCounter();
            auto playerIt = _identifiedEquippedItems.find(playerGuid);
            if (playerIt == _identifiedEquippedItems.end())
                return;

            auto& playerSlots = playerIt->second;
            auto slotIt = playerSlots.find(slot);
            if (slotIt == playerSlots.end())
                return;

            playerSlots.erase(slotIt);
        }
    }

    // 玩家登出时清理数据
    void OnLogout(Player* player) override
    {
        if (!player)
            return;

        uint64 playerGuid = player->GetGUID().GetCounter();
        _identifiedEquippedItems.erase(playerGuid);
    }
};

// ✅ 新增：初始化已鉴定物品缓存
// 【线程安全修复】添加锁保护
void ItemIdentificationSystem::InitializeCache()
{
    PerformanceTimer timer("初始化已鉴定物品缓存");

    // 一次性从数据库加载所有已鉴定物品的GUID
    QueryResult result = CharacterDatabase.Query("SELECT `物品GUID` FROM `物品_鉴定记录`");

    // 【线程安全修复】使用锁保护缓存写入
    std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
    _identifiedItemsCache.clear();

    if (!result)
    {
        _cacheInitialized = true;
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 itemGuid = fields[0].Get<uint32>();
        _identifiedItemsCache.insert(itemGuid);
        count++;
    } while (result->NextRow());

    _cacheInitialized = true;
}

// ✅ 新增：清除指定物品的鉴定缓存
// 【线程安全修复】添加锁保护
void ItemIdentificationSystem::ClearIdentifiedCache(uint32 itemGuid)
{
    {
        std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
        _identifiedItemsCache.erase(itemGuid);
    }
    {
        std::lock_guard<std::mutex> lock(_identificationRecordCacheMutex);
        _identificationRecordCache.erase(itemGuid);
    }
    DebugLog(ModuleOldFormat("清除物品GUID %s 的鉴定缓存", itemGuid).c_str());
}

// ✅ 新增：批量检查物品是否已鉴定（优化版）
// 【线程安全修复】添加锁保护
std::set<uint32> ItemIdentificationSystem::BatchCheckIdentified(const std::vector<uint32>& itemGuids)
{
    PerformanceTimer timer("批量检查已鉴定物品");

    // 【性能优化】如果缓存未初始化，返回空集合而不是同步初始化
    if (!_cacheInitialized)
    {
        DebugLog("批量检查：缓存未初始化，返回空集合");
        return std::set<uint32>();
    }

    std::set<uint32> identifiedSet;

    // 【线程安全修复】加锁读取缓存
    std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
    for (uint32 guid : itemGuids)
    {
        if (_identifiedItemsCache.find(guid) != _identifiedItemsCache.end())
        {
            identifiedSet.insert(guid);
        }
    }

    DebugLog(ModuleOldFormat("批量检查 %s 个物品，找到 %s 个已鉴定", itemGuids.size(), identifiedSet.size()).c_str());
    return identifiedSet;
}

// 物品掉落监控脚本
class ItemIdentificationLootScript : public PlayerScript
{
public:
    ItemIdentificationLootScript() : PlayerScript("ItemIdentificationLootScript") {}

    // 玩家从战利品中获得物品时触发
    void OnLootItem(Player* player, Item* item, uint32 count, ObjectGuid lootguid) override
    {
        if (!player || !item || !sItemIdentificationSystem->_enabled)
            return;

        // 只监控装备类物品
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR))
            return;

        // 【关键修复】物品进入背包后，主动向客户端发送属性数据
        // 这样客户端无需等待小退即可显示自定义属性
        uint32 itemEntry = item->GetEntry();
        uint32 itemGuid = item->GetGUID().GetCounter();
        QueueAllModuleDataAddonRefresh(player, itemEntry, itemGuid, 500ms);
    }

    // 玩家通过其他方式获得物品时触发（创建、任务奖励等）
    void OnStoreNewItem(Player* player, Item* item, uint32 count) override
    {
        if (!player || !item || !sItemIdentificationSystem->_enabled)
            return;

        // 只监控装备类物品
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR))
            return;

        // 【关键修复】物品进入背包后，主动向客户端发送属性数据
        uint32 itemEntry = item->GetEntry();
        uint32 itemGuid = item->GetGUID().GetCounter();
        QueueAllModuleDataAddonRefresh(player, itemEntry, itemGuid, 500ms);
    }
};

// Addon消息处理脚本
class ItemIdentificationAddonScript : public PlayerScript
{
public:
    ItemIdentificationAddonScript() : PlayerScript("ItemIdentificationAddonScript") { }

    // 监听玩家聊天事件，拦截Addon消息
    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        // 只处理Addon消息
        if (type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
        {
            return;
        }

        // 注意：服务器收到的消息格式是 "UITQ<TAB>QUERY:bag:slot:itemID"（新格式）
        // 或 "UITQ<TAB>QUERY:itemID:guid"（旧格式，保持兼容）
        // 需要先移除"UITQ<TAB>"前缀，然后解析参数

        // 查找TAB字符的位置
        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
        {
            return;
        }

        // 提取前缀部分（应该是"UITQ"）
        std::string prefix = msg.substr(0, tabPos);
        if (prefix != "UITQ")
        {
            return;
        }

        if (!AllowItemIdentificationAddonRequest(player, sItemIdentificationSystem->_addonQueriesPerSecond))
            return;

        // 提取消息内容（TAB之后的部分）
        std::string command = msg.substr(tabPos + 1);

        // 处理INSPECT_ITEM_GUID请求
        if (command.find("INSPECT_ITEM_GUID:") == 0)
        {
            HandleInspectItemGuidRequest(player, command.substr(18));  // 去掉"INSPECT_ITEM_GUID:"
            return;
        }

        // ========== 【新增】处理鉴定相关命令 ==========
        // IDENTIFY:<itemGuid> - 鉴定单个物品
        if (command.find("IDENTIFY:") == 0)
        {
            HandleIdentifyRequest(player, command.substr(9));  // 去掉"IDENTIFY:"
            return;
        }

        // IDENTIFY_BATCH - 批量鉴定所有待鉴定物品
        if (command == "IDENTIFY_BATCH")
        {
            HandleBatchIdentifyRequest(player);
            return;
        }

        if (command == "RECYCLE_LEVELS")
        {
            HandleRecycleLevelsRequest(player);
            return;
        }

        if (command == "RECYCLE_BATCH" || command.find("RECYCLE_BATCH:") == 0)
        {
            uint32 identificationLevel = 0;
            if (command.size() > 14)
            {
                try
                {
                    identificationLevel = std::stoul(command.substr(14));
                }
                catch (...)
                {
                    identificationLevel = 0;
                }
            }
            HandleBatchRecycleRequest(player, identificationLevel);
            return;
        }

        // LIST_PENDING - 查询待鉴定物品列表
        if (command == "LIST_PENDING")
        {
            HandleListPendingRequest(player);
            return;
        }

        if (command.find("QUERY_TEMPLATE:") == 0)
        {
            HandleTemplateStatsRequest(player, command.substr(15));
            return;
        }

        // 解析QUERY命令
        if (command.find("QUERY:") != 0)
            return;

        // 提取参数
        std::string params = command.substr(6);  // 去掉"QUERY:"

        // 【修改】新格式：QUERY:bag:slot:itemID（WoW 3.3.5客户端物品链接不包含GUID）
        // bag=255表示装备栏，0-4表示背包，-1/5-11表示银行
        std::vector<std::string> parts;
        std::istringstream stream(params);
        std::string part;
        while (std::getline(stream, part, ':'))
        {
            parts.push_back(part);
        }

        // 兼容旧格式：QUERY:itemID:guid（两个参数）
        // 新格式：QUERY:bag:slot:itemID（三个参数）
        uint32 itemID = 0;
        uint32 guid = 0;
        int32 bag = 255;   // 默认值255表示装备栏（旧格式兼容）
        uint8 slot = 0;

        if (parts.size() == 2)
        {
            // 旧格式：itemID:guid
            try
            {
                itemID = std::stoul(parts[0]);
                guid = std::stoul(parts[1]);
            }
            catch (...)
            {
                return;
            }

            if (itemID == 0 || guid == 0)
                return;

            Item* item = FindPlayerItemByGuidForAddon(player, guid);
            if ((!item || item->GetEntry() != itemID) && !HasInspectItemQueryGrant(player, guid))
                return;
        }
        else if (parts.size() >= 3)
        {
            // 新格式：bag:slot:itemID
            try
            {
                bag = std::stoi(parts[0]);
                int32 parsedSlot = std::stoi(parts[1]);
                itemID = std::stoul(parts[2]);

                if (parsedSlot < 0 || parsedSlot > 255)
                    return;

                slot = static_cast<uint8>(parsedSlot);
            }
            catch (...)
            {
                return;
            }

            if (itemID == 0)
                return;

            // 根据bag和slot获取物品
            Item* item = nullptr;

            if (bag == 255)
            {
                // bag=255表示装备栏
                // 客户端GetInventorySlotInfo返回1-based槽位，需要转换为0-based
                // 例如：HeadSlot客户端返回1，服务器端EQUIPMENT_SLOT_HEAD=0
                uint8 equipSlot = (slot > 0) ? (slot - 1) : slot;
                item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, equipSlot);

                if (sItemIdentificationSystem->_debugMode)
                {
                    LOG_INFO("module.itemidentification", ModuleOldFormat("[QUERY] 装备栏查询: 客户端slot=%s, 服务器equipSlot=%s, itemID=%s", slot, equipSlot, itemID).c_str());
                }
            }
            else if (bag == 0)
            {
                // bag=0是主背包
                // WoW主背包槽位从1开始（客户端），服务器端需要转换
                uint8 bagSlot = (slot > 0) ? (slot - 1) : slot;
                item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START + bagSlot);

                if (sItemIdentificationSystem->_debugMode)
                {
                    LOG_INFO("module.itemidentification", ModuleOldFormat("[QUERY] 主背包查询: 客户端slot=%s, 服务器bagSlot=%s, itemID=%s", slot, INVENTORY_SLOT_ITEM_START + bagSlot, itemID).c_str());
                }
            }
            else if (bag >= 1 && bag <= 4)
            {
                // bag=1-4是额外背包
                // 客户端发送的bag需要转换为服务器的背包槽位
                uint8 serverBagSlot = INVENTORY_SLOT_BAG_START + static_cast<uint8>(bag - 1);
                if (Bag* bagPtr = player->GetBagByPos(serverBagSlot))
                {
                    // 背包内的槽位是0-based
                    if (slot > 0)
                        item = bagPtr->GetItemByPos(slot - 1);
                    else
                        item = bagPtr->GetItemByPos(slot);
                }
            }
            else if (bag == -1)
            {
                // bag=-1是主银行
                uint8 bankSlot = (slot > 0) ? (slot - 1) : slot;
                item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, BANK_SLOT_ITEM_START + bankSlot);

                if (sItemIdentificationSystem->_debugMode)
                {
                    LOG_INFO("module.itemidentification", ModuleOldFormat("[QUERY] 银行主仓查询: 客户端slot=%s, 服务器bankSlot=%s, itemID=%s", slot, BANK_SLOT_ITEM_START + bankSlot, itemID).c_str());
                }
            }
            else if (bag >= 5 && bag <= 11)
            {
                // bag=5-11是银行背包
                uint8 serverBagSlot = BANK_SLOT_BAG_START + static_cast<uint8>(bag - 5);
                if (Bag* bagPtr = player->GetBagByPos(serverBagSlot))
                {
                    if (slot > 0)
                        item = bagPtr->GetItemByPos(slot - 1);
                    else
                        item = bagPtr->GetItemByPos(slot);
                }

                if (sItemIdentificationSystem->_debugMode)
                {
                    LOG_INFO("module.itemidentification", ModuleOldFormat("[QUERY] 银行背包查询: 客户端bag=%s, slot=%s, 服务器bagSlot=%s, itemID=%s", bag, slot, serverBagSlot, itemID).c_str());
                }
            }

            // 验证物品
            if (!item)
            {
                if (sItemIdentificationSystem->_debugMode)
                {
                    LOG_INFO("module.itemidentification", ModuleOldFormat("[QUERY] 未找到物品: bag=%s, slot=%s, itemID=%s", bag, slot, itemID).c_str());
                }
                return;
            }

            // 验证物品ID是否匹配
            if (item->GetEntry() != itemID)
            {
                if (sItemIdentificationSystem->_debugMode)
                {
                    LOG_INFO("module.itemidentification", ModuleOldFormat("[QUERY] 物品ID不匹配: bag=%s, slot=%s, 期望itemID=%s, 实际itemID=%s", bag, slot, itemID, item->GetEntry()).c_str());
                }
                return;
            }

            // 获取物品的真实GUID
            guid = item->GetGUID().GetCounter();

            if (sItemIdentificationSystem->_debugMode)
            {
                LOG_INFO("module.itemidentification", ModuleOldFormat("[QUERY] 通过位置找到物品: bag=%s, slot=%s, itemID=%s, realGUID=%s", bag, slot, itemID, guid).c_str());
            }
        }
        else
        {
            return;  // 参数数量不对
        }

        if (itemID == 0 || guid == 0)
        {
            return;
        }

        // 执行批量查询并通过Addon消息返回结果
        // 【修改】传递bag和slot，用于客户端精确匹配响应
        HandleAddonBatchQuery(player, itemID, guid, bag, slot);
    }

private:
    void HandleTemplateStatsRequest(Player* player, const std::string& params)
    {
        if (!player || !sItemIdentificationSystem || !sItemIdentificationSystem->_enabled)
            return;

        uint32 itemID = 0;
        try
        {
            itemID = std::stoul(params);
        }
        catch (...)
        {
            return;
        }

        if (!itemID)
            return;

        std::string templateStatsData = BuildTemplateStatsData(itemID);
        if (templateStatsData.empty())
            return;

        std::ostringstream response;
        response << "ALL_MODULE_DATA:" << itemID << ":0:"
                 << ":"  // baseAttributes
                 << ":"  // additionalAttributes
                 << ":"  // identificationDisplayData
                 << ":"  // growthData
                 << ":"  // enhancementData
                 << ":"  // skillsData
                 << ":"  // runeData
                 << ":"  // setData
                 << ":"  // huanjingData
                 << templateStatsData;

        SendItemIdentificationAddonMessage(player, response.str());
    }

    // 处理查询其他玩家装备GUID的请求
    // 【安全修复】添加权限校验，防止未授权查看他人装备
    void HandleInspectItemGuidRequest(Player* requester, const std::string& params)
    {
        if (!requester)
            return;

        // 解析参数：playerName:slot:itemID
        std::vector<std::string> parts;
        std::istringstream stream(params);
        std::string part;
        while (std::getline(stream, part, ':'))
        {
            parts.push_back(part);
        }

        if (parts.size() < 3)
            return;

        std::string targetPlayerName = parts[0];
        uint8 slot = 0;
        uint32 itemID = 0;

        try
        {
            slot = static_cast<uint8>(std::stoul(parts[1]));
            itemID = std::stoul(parts[2]);
        }
        catch (...)
        {
            return;
        }

        // 槽位转换：客户端使用1-based，服务端使用0-based
        uint8 serverSlot = (slot > 0) ? (slot - 1) : slot;

        // 查找目标玩家
        Player* targetPlayer = ObjectAccessor::FindPlayerByName(targetPlayerName);
        if (!targetPlayer)
        {
            // 玩家不在线，返回空响应
            std::string response = "INSPECT_ITEM_GUID_RESPONSE:" + targetPlayerName + ":" +
                                   Acore::ToString(slot) + ":" + Acore::ToString(itemID) + ":0";
            SendItemIdentificationAddonMessage(requester, response);
            return;
        }

        // 【安全修复】权限校验：只允许以下情况查看装备信息
        // 1. 查看自己的装备
        // 2. GM权限玩家
        // 3. 目标玩家在附近（30码内）且请求者正在观察状态（模拟inspect行为）
        bool canInspect = false;

        if (requester->GetGUID() == targetPlayer->GetGUID())
        {
            // 查看自己的装备，始终允许
            canInspect = true;
        }
        else if (requester->GetSession()->GetSecurity() >= SEC_GAMEMASTER)
        {
            // GM权限可以查看任何人的装备
            canInspect = true;
        }
        else if (requester->IsWithinDistInMap(targetPlayer, 30.0f))
        {
            // 在30码范围内，允许查看（模拟正常的inspect行为）
            canInspect = true;
        }

        if (!canInspect)
        {
            // 无权限查看，返回空响应
            std::string response = "INSPECT_ITEM_GUID_RESPONSE:" + targetPlayerName + ":" +
                                   Acore::ToString(slot) + ":" + Acore::ToString(itemID) + ":0";
            SendItemIdentificationAddonMessage(requester, response);
            return;
        }

        // 获取目标玩家指定槽位的装备
        Item* item = targetPlayer->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
        uint32 realGuid = 0;

        if (item && item->GetEntry() == itemID)
        {
            realGuid = item->GetGUID().GetCounter();
            GrantInspectItemQuery(requester, realGuid);
        }

        // 发送响应
        std::ostringstream response;
        response << "INSPECT_ITEM_GUID_RESPONSE:" << targetPlayerName << ":"
                 << static_cast<uint32>(slot) << ":" << itemID << ":" << realGuid;

        SendItemIdentificationAddonMessage(requester, response.str());
    }

    // 【修复】辅助函数：在玩家背包和装备中查找指定itemID的物品的真实GUID
    // 返回所有匹配物品的GUID列表（因为玩家可能有多个相同itemID的物品）
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

    // 【修改】添加bag和slot参数，用于客户端精确匹配响应
    void HandleAddonBatchQuery(Player* player, uint32 itemID, uint32 guid, int32 bag = 255, uint8 slot = 0)
    {
        if (!player)
        {
            return;
        }

        if (!sItemIdentificationSystem->_enabled)
        {
            return;
        }



        // 执行批量查询（使用现有的查询逻辑）
        auto queryStart = std::chrono::high_resolution_clock::now();

        // 优先从缓存读取
        uint64 cacheKey = ((uint64)itemID << 32) | guid;

        ItemIdentificationSystem::AllModuleData moduleData;  // 改名避免与WorldPacket data冲突
        bool cacheHit = false;
        uint32 usedGuid = guid;  // 记录最终使用的GUID

        // 【线程安全】加锁读取缓存
        {
            std::lock_guard<std::mutex> lock(sItemIdentificationSystem->_batchCacheMutex);

            auto it = sItemIdentificationSystem->_batchQueryCache.find(cacheKey);

            // 【线程安全修复】移除此处的直接统计更新，改为在函数末尾统一更新

            if (it != sItemIdentificationSystem->_batchQueryCache.end())
            {
                // 检查缓存是否过期
                time_t now = time(nullptr);
                if ((now - it->second.cacheTime) < sItemIdentificationSystem->BATCH_CACHE_EXPIRE_TIME)
                {
                    // 缓存有效，直接使用
                    moduleData = it->second.data;
                    cacheHit = true;
                }
                else
                {
                    // 缓存过期，删除
                    sItemIdentificationSystem->_batchQueryCache.erase(it);
                }
            }
        }

        if (!cacheHit)
        {
            // 缓存未命中，在锁外查询数据库（避免长时间持有锁）
            moduleData = sItemIdentificationSystem->QueryAllModuleData(itemID, guid);

            // 【关键修复】只使用当前GUID的精确数据。
            // 原因：当玩家有多个相同itemID的物品时（一个已鉴定，一个新掉落），
            // 现在只返回当前物品的精确数据，如果没有数据就返回空（让客户端显示"待鉴定"状态）



            // 【线程安全】加锁写入缓存（使用当前物品的GUID作为缓存键）
            sItemIdentificationSystem->StoreBatchQueryCache(cacheKey, moduleData);
        }

        // 构建Addon响应消息
        // 【修改】新格式：ALL_MODULE_DATA:bag:slot:itemID:guid:base:additional:identDisplay:growth:enhancement:skills:rune:set:huanjing:templateStats
        // 添加bag:slot用于客户端精确匹配响应到正确的pending记录
        std::ostringstream response;
        response << "ALL_MODULE_DATA:" << bag << ":" << static_cast<uint32>(slot) << ":" << itemID << ":" << guid << ":"
                 << moduleData.baseAttributes << ":"
                 << moduleData.additionalAttributes << ":"
                 << moduleData.identificationDisplayData << ":"
                 << moduleData.growthData << ":"
                 << moduleData.enhancementData << ":"
                 << moduleData.skillsData << ":"
                 << moduleData.runeData << ":"
                 << moduleData.setData << ":"
                 << moduleData.huanjingData;
        if (!moduleData.pendingIdentifyData.empty())
            response << ":" << moduleData.pendingIdentifyData;
        response << ":" << moduleData.templateStatsData;

        std::string responseStr = response.str();

        SendItemIdentificationAddonMessage(player, responseStr);

        // 计算查询耗时
        auto queryEnd = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(queryEnd - queryStart).count();

        // 【线程安全修复】使用封装方法更新性能统计
        sItemIdentificationSystem->UpdatePerfStats(duration, cacheHit, !cacheHit);

        // 无论是否开启调试，当批量查询耗时较长时输出性能日志（阈值：>= 20ms）
        if (duration >= 20000)
        {
        }

        if (sItemIdentificationSystem->_debugMode)
        {
            LOG_INFO("module.itemidentification", ModuleOldFormat("[Addon响应] 已发送数据: itemID=%s, guid=%s, 消息长度=%s, 缓存命中=%s, 耗时=%sμs", itemID, guid, responseStr.length(), cacheHit, duration).c_str());
        }
    }

    // ========== 【新增】鉴定相关Addon命令处理函数 ==========

    // 辅助函数：发送Addon消息给玩家
    void SendAddonResponse(Player* player, const std::string& response)
    {
        if (!player)
            return;

        SendItemIdentificationAddonMessage(player, response);
    }

    // 辅助函数：在玩家背包中通过GUID查找物品
    Item* FindItemByGuid(Player* player, uint32 itemGuid)
    {
        if (!player)
            return nullptr;

        // 1. 搜索装备栏
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (item && item->GetGUID().GetCounter() == itemGuid)
                return item;
        }

        // 2. 搜索主背包
        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (item && item->GetGUID().GetCounter() == itemGuid)
                return item;
        }

        // 3. 搜索背包袋
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* bag = player->GetBagByPos(i))
            {
                for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                {
                    Item* item = bag->GetItemByPos(j);
                    if (item && item->GetGUID().GetCounter() == itemGuid)
                        return item;
                }
            }
        }

        return nullptr;
    }

    // 处理单个物品鉴定请求: IDENTIFY:<itemGuid>
    void HandleIdentifyRequest(Player* player, const std::string& params)
    {
        if (!player)
            return;

        uint32 itemGuid = 0;
        try
        {
            itemGuid = std::stoul(params);
        }
        catch (...)
        {
            SendAddonResponse(player, "IDENTIFY_RESULT:ERROR:无效的物品GUID格式");
            return;
        }

        if (itemGuid == 0)
        {
            SendAddonResponse(player, "IDENTIFY_RESULT:ERROR:无效的物品GUID");
            return;
        }

        uint32 playerGuid = player->GetGUID().GetCounter();

        // 1. 从数据库查询待鉴定标记
        QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
            "WHERE `物品GUID` = %s AND `玩家GUID` = %s", itemGuid, playerGuid).c_str());

        if (!result)
        {
            SendAddonResponse(player, "IDENTIFY_RESULT:ERROR:此物品不需要鉴定或已鉴定");
            return;
        }

        Field* fields = result->Fetch();
        uint32 itemId = fields[0].Get<uint32>();
        uint32 huanJingMultiplier = fields[1].Get<uint32>();
        char huanJingMode = DbValueToHuanJingMode(fields[2].Get<int32>());
        uint32 identificationGroupId = fields[3].Get<uint32>();

        // 2. 在玩家背包中查找物品
        Item* item = FindItemByGuid(player, itemGuid);
        if (!item)
        {
            // 清理无效的标记
            CharacterDatabase.DirectExecute(ModuleOldFormat("DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = %s", itemGuid).c_str());
            SendAddonResponse(player, "IDENTIFY_RESULT:ERROR:物品不存在或不在背包中");
            return;
        }

        // 验证物品ID是否匹配
        if (item->GetEntry() != itemId)
        {
            CharacterDatabase.DirectExecute(ModuleOldFormat("DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = %s", itemGuid).c_str());
            SendAddonResponse(player, "IDENTIFY_RESULT:ERROR:物品数据不匹配");
            return;
        }

        // 3. 执行鉴定（如果有鉴定组）
        // 【安全修复】改调 IdentifyItem：包含成功率掷骰与金币扣费。
        // 原先直调 ApplyIdentification 使 Addon 路径可免费且 100% 成功鉴定（绕过命令/批量路径的收费机制）
        bool identifySuccess = true;
        if (identificationGroupId > 0)
        {
            identifySuccess = sItemIdentificationSystem->IdentifyItem(player, item, identificationGroupId);
        }

        // 5. 删除待鉴定标记
        // 【修复】仅在鉴定成功时删除标记；掷骰失败/金币不足等可恢复失败保留标记，允许玩家重试
        if (identifySuccess)
        {
            CharacterDatabase.DirectExecute(ModuleOldFormat("DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = %s", itemGuid).c_str());
        }

        // 【关键修复】鉴定成功后清除批量查询缓存，确保下次查询返回新数据
        {
            std::lock_guard<std::mutex> lock(sItemIdentificationSystem->_batchCacheMutex);
            uint64 cacheKey = ((uint64)itemId << 32) | itemGuid;
            sItemIdentificationSystem->_batchQueryCache.erase(cacheKey);
        }

        // 【新增】确保鉴定缓存已更新（防止缓存不同步）
        if (identifySuccess)
        {
            sItemIdentificationSystem->AddToIdentifiedCache(itemGuid);
        }

        // 6. 发送结果
        if (identifySuccess)
        {
            // 发送鉴定成功消息，包含背包位置信息以便客户端正确清理缓存
            ClientItemLocation location = GetClientItemLocation(item);
            int clientBag = location.valid ? location.bag : 255;
            uint8 clientSlot = location.valid ? location.slot : static_cast<uint8>(item->GetSlot() + 1);

            std::ostringstream response;
            response << "IDENTIFY_RESULT:SUCCESS:" << clientBag << ":" << clientSlot << ":" << itemId << ":" << huanJingMultiplier;
            SendAddonResponse(player, response.str());
        }
        else
        {
            SendAddonResponse(player, "IDENTIFY_RESULT:FAIL:鉴定失败");
        }
    }

    // 处理批量鉴定请求: IDENTIFY_BATCH
    void HandleBatchIdentifyRequest(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();

        QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT `物品GUID`, `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
            "WHERE `玩家GUID` = %s ORDER BY `物品GUID` ASC", playerGuid).c_str());

        if (!result)
        {
            SendAddonResponse(player, "IDENTIFY_BATCH_RESULT:0:0:0");
            return;
        }

        uint32 successCount = 0;
        uint32 failCount = 0;
        uint32 skipCount = 0;
        std::vector<uint32> processedGuids;
        std::unordered_map<uint32, Item*> itemsByGuid = BuildPlayerItemGuidIndex(player);

        do
        {
            Field* fields = result->Fetch();
            uint32 itemGuid = fields[0].Get<uint32>();
            uint32 itemId = fields[1].Get<uint32>();
            uint32 huanJingMultiplier = fields[2].Get<uint32>();
            char huanJingMode = DbValueToHuanJingMode(fields[3].Get<int32>());
            uint32 identificationGroupId = fields[4].Get<uint32>();

            // 从本次请求的一次性背包索引中查找物品，避免每个GUID重复扫描背包
            auto itemItr = itemsByGuid.find(itemGuid);
            Item* item = itemItr != itemsByGuid.end() ? itemItr->second : nullptr;
            if (!item || item->GetEntry() != itemId)
            {
                // 物品不存在/不匹配：标记已失效，纳入批量清理
                processedGuids.push_back(itemGuid);
                skipCount++;
                continue;
            }

            // 执行鉴定
            bool success = true;
            if (identificationGroupId > 0)
            {
                success = sItemIdentificationSystem->IdentifyItem(player, item, identificationGroupId);
            }

            if (success)
            {
                successCount++;
                // 【修复】仅成功时删除待鉴定标记；掷骰失败等可恢复失败保留标记允许重试
                processedGuids.push_back(itemGuid);

                // 【关键修复】鉴定成功后清除批量查询缓存
                {
                    std::lock_guard<std::mutex> lock(sItemIdentificationSystem->_batchCacheMutex);
                    uint64 cacheKey = ((uint64)itemId << 32) | itemGuid;
                    sItemIdentificationSystem->_batchQueryCache.erase(cacheKey);
                }

                // 【新增】确保鉴定缓存已更新（防止缓存不同步）
                sItemIdentificationSystem->AddToIdentifiedCache(itemGuid);

                if (identificationGroupId == 0)
                    QueueAllModuleDataAddonRefresh(player, itemId, itemGuid, 2000ms);
            }
            else
            {
                failCount++;
            }

        } while (result->NextRow());

        // 批量删除已处理的标记
        if (!processedGuids.empty())
        {
            std::ostringstream ss;
            ss << "DELETE FROM `待鉴定物品标记` WHERE `物品GUID` IN (";
            for (size_t i = 0; i < processedGuids.size(); ++i)
            {
                if (i > 0) ss << ",";
                ss << processedGuids[i];
            }
            ss << ")";
            CharacterDatabase.DirectExecute(ss.str().c_str());
        }

        // 发送批量鉴定结果
        std::ostringstream response;
        response << "IDENTIFY_BATCH_RESULT:" << successCount << ":" << failCount << ":" << skipCount;
        SendAddonResponse(player, response.str());
    }

    void HandleRecycleLevelsRequest(Player* player)
    {
        if (!player)
            return;

        std::set<uint32> levels;
        for (auto const& pair : _identificationTemplates)
        {
            IdentificationTemplate const& identificationTemplate = pair.second;
            if (identificationTemplate.level > 0 && identificationTemplate.recycleRewardTemplate > 0)
                levels.insert(identificationTemplate.level);
        }

        std::ostringstream response;
        response << "RECYCLE_LEVELS_RESULT:";
        bool first = true;
        for (uint32 level : levels)
        {
            if (!first)
                response << ',';
            response << level;
            first = false;
        }
        SendAddonResponse(player, response.str());
    }

    void HandleBatchRecycleRequest(Player* player, uint32 identificationLevel)
    {
        if (!player)
            return;

        uint32 skippedCount = 0;
        uint32 recycledCount = sItemIdentificationSystem->RecycleAllBackpackItems(player, skippedCount, identificationLevel);

        std::ostringstream response;
        response << "RECYCLE_BATCH_RESULT:" << recycledCount << ':' << skippedCount << ':' << identificationLevel;
        SendAddonResponse(player, response.str());
    }

    // 处理待鉴定列表查询请求: LIST_PENDING
    // 【重要】WoW 3.3.5 Addon消息限制255字节，需要分片发送
    void HandleListPendingRequest(Player* player)
    {
        if (!player)
            return;

        uint32 playerGuid = player->GetGUID().GetCounter();

        QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT `物品GUID`, `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
            "WHERE `玩家GUID` = %s ORDER BY `物品GUID` DESC LIMIT 100", playerGuid).c_str());

        if (!result)
        {
            SendAddonResponse(player, "PENDING_LIST:0:");
            return;
        }

        // 【修改】收集所有物品数据，然后分片发送
        // 格式：bag,slot,itemId,mode,multi,group
        std::vector<std::string> itemEntries;
        uint32 dbCount = 0;
        std::unordered_map<uint32, Item*> itemsByGuid = BuildPlayerItemGuidIndex(player);

        do
        {
            Field* fields = result->Fetch();
            uint32 guid = fields[0].Get<uint32>();
            uint32 itemId = fields[1].Get<uint32>();
            uint32 multiplier = fields[2].Get<uint32>();
            char multiplierMode = DbValueToHuanJingMode(fields[3].Get<int32>());
            uint32 groupId = fields[4].Get<uint32>();
            dbCount++;

            // 检查物品是否还在背包中，并获取位置信息
            auto itemItr = itemsByGuid.find(guid);
            Item* item = itemItr != itemsByGuid.end() ? itemItr->second : nullptr;
            if (item)
            {
                // 获取物品的背包位置
                uint8 serverBag = item->GetBagSlot();
                uint8 slot = item->GetSlot();

                // 【关键】服务器端背包编号转换为客户端编号
                int clientBag = 255;
                int clientSlot = 0;

                if (serverBag == INVENTORY_SLOT_BAG_0)
                {
                    if (slot >= EQUIPMENT_SLOT_START && slot < EQUIPMENT_SLOT_END)
                    {
                        clientBag = 255;
                        clientSlot = static_cast<int>(slot) + 1;
                    }
                    else if (slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END)
                    {
                        clientBag = 0;
                        clientSlot = static_cast<int>(slot - INVENTORY_SLOT_ITEM_START) + 1;
                    }
                    else
                    {
                        continue;
                    }
                }
                else if (serverBag >= INVENTORY_SLOT_BAG_START && serverBag < INVENTORY_SLOT_BAG_END)
                {
                    clientBag = serverBag - INVENTORY_SLOT_BAG_START + 1;
                    clientSlot = static_cast<int>(slot) + 1;
                }
                else
                {
                    continue;
                }

                // 生成物品条目字符串
                std::ostringstream entryStream;
                entryStream << clientBag << "," << clientSlot << "," << itemId << "," << multiplierMode << "," << multiplier << "," << groupId;
                itemEntries.push_back(entryStream.str());
            }
        } while (result->NextRow());

        uint32 totalCount = itemEntries.size();

        // 如果没有有效物品，发送空列表
        if (totalCount == 0)
        {
            SendAddonResponse(player, "PENDING_LIST:0:");
            return;
        }

        // 【分片发送逻辑】
        // 格式变更：PENDING_LIST:总数:当前包序号:总包数:数据
        // 每个包预留约40字节给头部，数据部分约200字节
        const size_t MAX_DATA_SIZE = 180;  // 保守估计，给头部留空间

        std::vector<std::string> packets;
        std::ostringstream currentPacketData;
        bool firstInPacket = true;

        for (size_t i = 0; i < itemEntries.size(); ++i)
        {
            const std::string& entry = itemEntries[i];

            // 检查添加这个条目后是否超过限制
            size_t newSize = currentPacketData.str().length();
            if (!firstInPacket)
                newSize += 1;  // 分号
            newSize += entry.length();

            if (newSize > MAX_DATA_SIZE && !firstInPacket)
            {
                // 当前包已满，保存并开始新包
                packets.push_back(currentPacketData.str());
                currentPacketData.str("");
                currentPacketData.clear();
                firstInPacket = true;
            }

            if (!firstInPacket)
                currentPacketData << ";";
            currentPacketData << entry;
            firstInPacket = false;
        }

        // 保存最后一个包
        if (!currentPacketData.str().empty())
        {
            packets.push_back(currentPacketData.str());
        }

        uint32 totalPackets = packets.size();

        // 发送每个数据包
        for (size_t i = 0; i < packets.size(); ++i)
        {
            std::ostringstream response;
            // 格式: PENDING_LIST:总数:包序号:总包数:数据
            response << "PENDING_LIST:" << totalCount << ":" << (i + 1) << ":" << totalPackets << ":" << packets[i];

            std::string responseStr = response.str();
            SendAddonResponse(player, responseStr);
        }
    }
};

// ============================================================================
// 物品数据清理脚本 - 解决数据库无限膨胀问题
// ============================================================================

// 物品数据清理脚本 - 处理物品删除和角色删除时的数据清理
class ItemIdentificationCleanupScript : public PlayerScript, public AllItemScript
{
public:
    ItemIdentificationCleanupScript() : PlayerScript("ItemIdentificationCleanupScript"), AllItemScript("ItemIdentificationCleanupScript") {}

    // ========== 物品删除时清理数据 ==========
    // 当物品被销毁/删除时触发（出售商店、丢弃、销毁等）
    // 返回 true 表示允许删除，false 表示阻止删除
    bool CanItemRemove(Player* player, Item* item) override
    {
        (void)player;

        if (!item)
            return true;

        uint32 itemGuid = item->GetGUID().GetCounter();

        // 检查是否是已鉴定物品
        if (!sItemIdentificationSystem->IsItemIdentified(itemGuid))
            return true;

        // 飞升系统将装备暂存到 character_inventory.bag=200。
        // 这类物品可能被核心库存保存流程临时判定为不在官方槽位，但并未真正删除。
        if (IsAscensionVirtualBagItem(itemGuid))
            return true;

        // 清理该物品的所有相关数据
        CleanupItemData(itemGuid, item->GetEntry());

        return true;  // 允许删除物品
    }

    // ========== 角色删除时清理数据 ==========
    // 角色从数据库删除时触发（可以在事务中操作）
    void OnDeleteFromDB(CharacterDatabaseTransaction trans, uint32 guid) override
    {
        if (!trans)
            return;

        // 先获取该玩家所有物品的GUID列表（用于清理内存缓存和关联表）
        QueryResult itemsResult = CharacterDatabase.Query(ModuleOldFormat("SELECT `物品GUID` FROM `物品_鉴定记录` WHERE `玩家GUID` = %s", guid).c_str());

        std::vector<uint32> itemGuids;
        if (itemsResult)
        {
            do
            {
                itemGuids.push_back(itemsResult->Fetch()[0].Get<uint32>());
            } while (itemsResult->NextRow());
        }

        // 如果没有该玩家的鉴定记录，尝试从其他表获取物品GUID
        if (itemGuids.empty())
        {
            // 从成长记录获取
            QueryResult growthResult = CharacterDatabase.Query(ModuleOldFormat("SELECT `物品GUID` FROM `物品成长_玩家记录` WHERE `玩家GUID` = %s", guid).c_str());
            if (growthResult)
            {
                do
                {
                    itemGuids.push_back(growthResult->Fetch()[0].Get<uint32>());
                } while (growthResult->NextRow());
            }
        }

        // ========== 在事务中删除所有相关表的数据 ==========

        // 0. 待鉴定物品标记表（有玩家GUID字段）
        trans->Append(ModuleOldFormat("DELETE FROM `待鉴定物品标记` WHERE `玩家GUID` = %s", guid).c_str());

        // 1. 鉴定记录表（有玩家GUID字段）
        trans->Append(ModuleOldFormat("DELETE FROM `物品_鉴定记录` WHERE `玩家GUID` = %s", guid).c_str());

        // 2. 物品成长记录表（有玩家GUID字段）
        trans->Append(ModuleOldFormat("DELETE FROM `物品成长_玩家记录` WHERE `玩家GUID` = %s", guid).c_str());

        // 3. 物品强化记录表（字段名是owner_guid）
        trans->Append(ModuleOldFormat("DELETE FROM `物品强化_记录` WHERE `owner_guid` = %s", guid).c_str());

        // 4. 符文系统数据表（字段名是角色GUID）
        trans->Append(ModuleOldFormat("DELETE FROM `符文系统_数据` WHERE `角色GUID` = %s", guid).c_str());

        // 5. 玩家装备属性增强表（有玩家GUID字段）
        trans->Append(ModuleOldFormat("DELETE FROM `玩家装备属性增强` WHERE `玩家GUID` = %s", guid).c_str());

        // ========== 以下表没有玩家GUID字段，需要通过物品GUID列表删除 ==========
        for (uint32 itemGuid : itemGuids)
        {
            // 6. 物品属性数据表（只有物品GUID）
            trans->Append(ModuleOldFormat("DELETE FROM `物品属性_数据` WHERE `物品GUID` = %s", itemGuid).c_str());

            // 7. 物品技能数据表（只有物品GUID）
            trans->Append(ModuleOldFormat("DELETE FROM `_物品技能_数据` WHERE `物品GUID` = %s", itemGuid).c_str());

            // 8. 玩家套装状态表（只有物品GUID）
            trans->Append(ModuleOldFormat("DELETE FROM `_物品套装_数据` WHERE `物品GUID` = %s", itemGuid).c_str());

            // 清理内存缓存
            sItemIdentificationSystem->ClearIdentifiedCache(itemGuid);
        }

    }

private:
    bool IsAscensionVirtualBagItem(uint32 itemGuid) const
    {
        QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT 1 FROM `character_inventory` WHERE `item` = %s AND `bag` = 200 LIMIT 1", itemGuid).c_str());

        return static_cast<bool>(result);
    }

    // 清理单个物品的所有相关数据
    void CleanupItemData(uint32 itemGuid, uint32 itemEntry)
    {
        // 使用异步执行，避免阻塞主线程
        // 0. 待鉴定物品标记表（物品删除时同步清理）
        CharacterDatabase.DirectExecute(ModuleOldFormat("DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = %s "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = %s AND ci.`bag` = 200)", itemGuid, itemGuid).c_str());

        // 1. 鉴定记录表
        CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `物品_鉴定记录` WHERE `物品GUID` = %s "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = %s AND ci.`bag` = 200)", itemGuid, itemGuid).c_str());

        // 2. 物品属性数据表
        CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `物品属性_数据` WHERE `物品GUID` = %s "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = %s AND ci.`bag` = 200)", itemGuid, itemGuid).c_str());

        // 3. 物品成长记录表
        CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `物品成长_玩家记录` WHERE `物品GUID` = %s "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = %s AND ci.`bag` = 200)", itemGuid, itemGuid).c_str());

        // 4. 物品强化记录表（字段名是guid）
        CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `物品强化_记录` WHERE `guid` = %s "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = %s AND ci.`bag` = 200)", itemGuid, itemGuid).c_str());

        // 5. 物品技能数据表
        CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `_物品技能_数据` WHERE `物品GUID` = %s "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = %s AND ci.`bag` = 200)", itemGuid, itemGuid).c_str());

        // 6. 符文系统数据表
        CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `符文系统_数据` WHERE `物品GUID` = %s "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = %s AND ci.`bag` = 200)", itemGuid, itemGuid).c_str());

        // 7. 玩家套装状态表
        CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `_物品套装_数据` WHERE `物品GUID` = %s "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = %s AND ci.`bag` = 200)", itemGuid, itemGuid).c_str());

        // 9. 玩家装备属性增强表
        CharacterDatabase.Execute(ModuleOldFormat("DELETE FROM `玩家装备属性增强` WHERE `装备GUID` = %s "
            "AND NOT EXISTS (SELECT 1 FROM `character_inventory` ci WHERE ci.`item` = %s AND ci.`bag` = 200)", itemGuid, itemGuid).c_str());

        // 清理内存缓存
        sItemIdentificationSystem->ClearIdentifiedCache(itemGuid);
        sItemIdentificationSystem->ClearItemCache(itemEntry, itemGuid);

        if (sItemIdentificationSystem->_debugMode)
        {
            LOG_DEBUG("module.itemidentification", ModuleOldFormat("[数据清理] 已清理物品数据: GUID=%s, Entry=%s", itemGuid, itemEntry).c_str());
        }
    }
};

// 添加脚本
void AddItemIdentificationSystemScripts()
{
    // 添加命令脚本
    new ItemIdentificationCommandScript();

    // 添加模块加载器
    new ItemIdentificationSystemModuleLoader();

    // 添加装备脚本
    new ItemIdentificationEquipScript();

    // 添加掉落监控脚本
    new ItemIdentificationLootScript();

    // 添加Addon消息处理脚本
    new ItemIdentificationAddonScript();

    // 添加物品数据清理脚本
    new ItemIdentificationCleanupScript();
}

// ============================================================================
// 批量查询优化实现 - 解决客户端查询延迟问题
// ============================================================================

// 批量查询所有模块数据（核心方法）- 【性能优化版】使用UNION合并查询
ItemIdentificationSystem::AllModuleData ItemIdentificationSystem::QueryAllModuleData(uint32 itemID, uint32 guid)
{
    AllModuleData result;
    result.identificationDisplayData = "IDDISP||||||0";
    result.hasData = false;
    result.templateStatsData = BuildTemplateStatsData(itemID);
    if (!result.templateStatsData.empty())
        result.hasData = true;

    // 【修复】不再依赖内存缓存判断是否鉴定，直接查询数据库
    // 原因：内存缓存可能在鉴定后没有正确更新，导致已鉴定物品被判定为未鉴定
    bool isIdentified = IsItemIdentified(guid);

    if (!isIdentified)
    {
        // 即使缓存显示未鉴定，也尝试从数据库查询（修复缓存不同步问题）
        QueryResult checkResult = CharacterDatabase.Query(ModuleOldFormat("SELECT 1 FROM `物品_鉴定记录` WHERE `物品GUID` = %s", guid).c_str());
        if (checkResult)
        {
            isIdentified = true;
            // 【线程安全修复】使用锁保护缓存写入
            {
                std::lock_guard<std::mutex> lock(_identifiedCacheMutex);
                _identifiedItemsCache.insert(guid);
            }
        }
        else
        {
            DebugLog(ModuleOldFormat("[批量查询-优化] 物品未鉴定(仍会继续检查成长/强化/技能等模块): itemID=%s, guid=%s", itemID, guid).c_str());
        }
    }

    // ========== 表存在性缓存（静态变量，只初始化一次）==========
    // 【线程安全修复】添加静态锁保护表存在性缓存
    static std::mutex tableCacheMutex;
    static std::unordered_map<std::string, bool> tableCache;
    static bool cacheInitialized = false;

    // 【线程安全修复】使用锁保护表缓存的初始化和访问
    {
        std::lock_guard<std::mutex> lock(tableCacheMutex);
        if (!cacheInitialized)
        {
            // 一次性检查所有需要的表
            // 【表名修复】技能数据表真名为 `_物品技能_数据`（带前缀，见 mod-item-skills 的
            // SQL 与全部查询）——此前预检写成无前缀，存在性守卫永假，技能数据永不下发
            std::vector<std::string> tablesToCheck = {
                "物品属性_数据", "物品_鉴定记录", "物品成长_玩家记录",
                "物品强化_记录", "_物品技能_数据",
                "符文系统_数据", "_物品套装_数据", "待鉴定物品标记"
            };

            for (const auto& tableName : tablesToCheck)
            {
                QueryResult tableCheckResult = CharacterDatabase.Query(ModuleOldFormat("SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '%s'", tableName).c_str());

                bool exists = (tableCheckResult && tableCheckResult->Fetch()[0].Get<uint32>() > 0);
                tableCache[tableName] = exists;
            }

            cacheInitialized = true;
        }
    }

    // ========== 辅助函数：从缓存检查表是否存在 ==========
    // 注意：tableCache在初始化后只读，所以这里不需要加锁
    // 【修复】静态变量不能通过引用捕获，改为使用指针
    auto TableExists = [](const std::string& tableName) -> bool {
        static std::unordered_map<std::string, bool>* cache = &tableCache;
        auto it = cache->find(tableName);
        return (it != cache->end() && it->second);
    };

    if (!isIdentified && TableExists("待鉴定物品标记"))
    {
        QueryResult pendingIdentifyResult = CharacterDatabase.Query(ModuleOldFormat("SELECT `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` WHERE `物品GUID` = %s AND `物品ID` = %s LIMIT 1", guid, itemID).c_str());

        if (pendingIdentifyResult)
        {
            Field* fields = pendingIdentifyResult->Fetch();
            uint32 multiplier = fields[0].Get<uint32>();
            char multiplierMode = DbValueToHuanJingMode(fields[1].Get<int32>());
            uint32 groupId = fields[2].Get<uint32>();

            std::ostringstream pendingStream;
            pendingStream << "PENDID|" << multiplierMode << "|" << multiplier << "|" << groupId;
            result.pendingIdentifyData = pendingStream.str();
            result.hasData = true;
        }
    }

    // ========== 【性能优化】使用UNION合并多个查询为一个 ==========
    DebugLog(ModuleOldFormat("[批量查询-优化] 开始使用UNION查询: itemID=%s, guid=%s", itemID, guid).c_str());

    try
    {
        // 构建UNION查询（将7个独立查询合并为1个）
        std::ostringstream unionQuery;
        bool hasAnyQuery = false;

        // 1. 鉴定系统数据（基础属性+追加属性）
        // 基础属性优先读物品属性表，旧数据/乱序写入导致为空时回退到鉴定记录详情。
        if (isIdentified && TableExists("物品_鉴定记录"))
        {
            if (TableExists("物品属性_数据"))
            {
                unionQuery << "SELECT 'identification' COLLATE utf8mb4_general_ci as source, "
                           << "CAST(COALESCE(NULLIF(a.`基础属性`, ''), NULLIF(r.`基础属性详情`, ''), '') AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                           << "CAST(IFNULL(a.`追加属性`, '') AS CHAR) COLLATE utf8mb4_general_ci as field2, "
                           << "'' COLLATE utf8mb4_general_ci as field3, "
                           << "'' COLLATE utf8mb4_general_ci as field4, "
                           << "'' COLLATE utf8mb4_general_ci as field5 "
                           << "FROM `物品_鉴定记录` r "
                           << "LEFT JOIN `物品属性_数据` a ON a.`物品GUID` = r.`物品GUID` "
                           << "WHERE r.`物品GUID` = " << guid;
            }
            else
            {
                unionQuery << "SELECT 'identification' COLLATE utf8mb4_general_ci as source, "
                           << "CAST(IFNULL(r.`基础属性详情`, '') AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                           << "'' COLLATE utf8mb4_general_ci as field2, "
                           << "'' COLLATE utf8mb4_general_ci as field3, "
                           << "'' COLLATE utf8mb4_general_ci as field4, "
                           << "'' COLLATE utf8mb4_general_ci as field5 "
                           << "FROM `物品_鉴定记录` r "
                           << "WHERE r.`物品GUID` = " << guid;
            }
            hasAnyQuery = true;
        }
        // 2. 成长系统数据（包含百分比属性，避免额外查询）
        if (TableExists("物品成长_玩家记录"))
        {
            if (hasAnyQuery) unionQuery << " UNION ALL ";
            unionQuery << "SELECT 'growth' COLLATE utf8mb4_general_ci, "
                       << "CAST(`当前等级` AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                       << "CAST(`当前经验` AS CHAR) COLLATE utf8mb4_general_ci as field2, "
                       << "CAST(`升级经验` AS CHAR) COLLATE utf8mb4_general_ci as field3, "
                       << "CAST(`成长属性` AS CHAR) COLLATE utf8mb4_general_ci as field4, "
                       << "CAST(IFNULL(`百分比属性`, '') AS CHAR) COLLATE utf8mb4_general_ci as field5 "
                       << "FROM `物品成长_玩家记录` "
                       << "WHERE `物品GUID` = " << guid;
            hasAnyQuery = true;
        }

        // 3. 强化系统数据
        if (TableExists("物品强化_记录"))
        {
            if (hasAnyQuery) unionQuery << " UNION ALL ";
            unionQuery << "SELECT 'enhancement' COLLATE utf8mb4_general_ci, "
                       << "CAST(`强化等级` AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                       << "CAST(`属性值` AS CHAR) COLLATE utf8mb4_general_ci as field2, "
                       << "'' COLLATE utf8mb4_general_ci as field3, "
                       << "'' COLLATE utf8mb4_general_ci as field4, "
                       << "'' COLLATE utf8mb4_general_ci as field5 "
                       << "FROM `物品强化_记录` "
                       << "WHERE `guid` = " << guid;
            hasAnyQuery = true;
        }

        // 4. 技能系统数据
        if (TableExists("_物品技能_数据"))
        {
            if (hasAnyQuery) unionQuery << " UNION ALL ";
            unionQuery << "SELECT 'skills' COLLATE utf8mb4_general_ci, "
                       << "CAST(`技能ID` AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                       << "'' COLLATE utf8mb4_general_ci as field2, "
                       << "'' COLLATE utf8mb4_general_ci as field3, "
                       << "'' COLLATE utf8mb4_general_ci as field4, "
                       << "'' COLLATE utf8mb4_general_ci as field5 "
                       << "FROM `_物品技能_数据` "
                       << "WHERE `物品GUID` = " << guid;
            hasAnyQuery = true;
        }

        // 5. 符文系统数据
        if (TableExists("符文系统_数据"))
        {
            if (hasAnyQuery) unionQuery << " UNION ALL ";
            unionQuery << "SELECT 'rune' COLLATE utf8mb4_general_ci, "
                       << "CAST(`插槽数量` AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                       << "CAST(`插槽ID` AS CHAR) COLLATE utf8mb4_general_ci as field2, "
                       << "CAST(`符文物品ID` AS CHAR) COLLATE utf8mb4_general_ci as field3, "
                       << "'' COLLATE utf8mb4_general_ci as field4, "
                       << "'' COLLATE utf8mb4_general_ci as field5 "
                       << "FROM `符文系统_数据` "
                       << "WHERE `物品GUID` = " << guid;
            hasAnyQuery = true;
        }

        // 6. 套装系统数据
        if (TableExists("_物品套装_数据") || (isIdentified && TableExists("物品_鉴定记录")))
        {
            if (hasAnyQuery) unionQuery << " UNION ALL ";
            if (TableExists("_物品套装_数据") && isIdentified && TableExists("物品_鉴定记录"))
            {
                unionQuery << "SELECT 'set' COLLATE utf8mb4_general_ci, "
                           << "CAST(COALESCE(NULLIF(s.`套装ID`, 0), NULLIF(r.`套装ID`, 0), 0) AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                           << "'' COLLATE utf8mb4_general_ci as field2, "
                           << "'' COLLATE utf8mb4_general_ci as field3, "
                           << "'' COLLATE utf8mb4_general_ci as field4, "
                           << "'' COLLATE utf8mb4_general_ci as field5 "
                           << "FROM `物品_鉴定记录` r "
                           << "LEFT JOIN `_物品套装_数据` s ON s.`物品GUID` = r.`物品GUID` "
                           << "WHERE r.`物品GUID` = " << guid;
            }
            else if (TableExists("_物品套装_数据"))
            {
                unionQuery << "SELECT 'set' COLLATE utf8mb4_general_ci, "
                           << "CAST(`套装ID` AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                           << "'' COLLATE utf8mb4_general_ci as field2, "
                           << "'' COLLATE utf8mb4_general_ci as field3, "
                           << "'' COLLATE utf8mb4_general_ci as field4, "
                           << "'' COLLATE utf8mb4_general_ci as field5 "
                           << "FROM `_物品套装_数据` "
                           << "WHERE `物品GUID` = " << guid;
            }
            else
            {
                unionQuery << "SELECT 'set' COLLATE utf8mb4_general_ci, "
                           << "CAST(IFNULL(r.`套装ID`, 0) AS CHAR) COLLATE utf8mb4_general_ci as field1, "
                           << "'' COLLATE utf8mb4_general_ci as field2, "
                           << "'' COLLATE utf8mb4_general_ci as field3, "
                           << "'' COLLATE utf8mb4_general_ci as field4, "
                           << "'' COLLATE utf8mb4_general_ci as field5 "
                           << "FROM `物品_鉴定记录` r "
                           << "WHERE r.`物品GUID` = " << guid;
            }
            hasAnyQuery = true;
        }

        // 执行合并查询
        if (hasAnyQuery)
        {
            std::string queryStr = unionQuery.str();

            QueryResult combinedResult = CharacterDatabase.Query(queryStr.c_str());

            if (combinedResult)
            {
                DebugLog("[批量查询-优化] UNION查询成功，开始解析结果");

                do
                {
                    Field* fields = combinedResult->Fetch();
                    std::string source = fields[0].Get<std::string>();

                    if (source == "identification")
                    {
                        std::string baseAttr = fields[1].Get<std::string>();
                        std::string additionalAttr = fields[2].Get<std::string>();

                        if (!baseAttr.empty())
                        {
                            result.baseAttributes = baseAttr;
                            result.hasData = true;
                        }

                        if (!additionalAttr.empty())
                        {
                            result.additionalAttributes = additionalAttr;
                            result.hasData = true;
                        }

                        DebugLog(ModuleOldFormat("[批量查询-优化-鉴定] 基础=[%s], 追加=[%s]", baseAttr, additionalAttr).c_str());
                    }
                    else if (source == "growth")
                    {
                        uint32 level = std::stoul(fields[1].Get<std::string>());
                        uint32 currentExp = std::stoul(fields[2].Get<std::string>());
                        uint32 requiredExp = std::stoul(fields[3].Get<std::string>());
                        std::string attrs = fields[4].Get<std::string>();
                        std::string percentAttrs = fields[5].Get<std::string>();  // 【性能优化】新增百分比属性

                        std::ostringstream growthStream;
                        // 格式：level|curExp|requiredExp|attrs|percentAttrs
                        growthStream << level << "|" << currentExp << "|" << requiredExp << "|" << attrs << "|" << percentAttrs;
                        result.growthData = growthStream.str();
                        result.hasData = true;

                        DebugLog(ModuleOldFormat("[批量查询-优化-成长] level=%s, exp=%s/%s, attrs=[%s], percentAttrs=[%s]", level, currentExp, requiredExp, attrs, percentAttrs).c_str());
                    }
                    else if (source == "enhancement")
                    {
                        std::string level = fields[1].Get<std::string>();
                        std::string attrs = fields[2].Get<std::string>();

                        std::ostringstream enhanceStream;
                        enhanceStream << level << "|" << attrs;
                        result.enhancementData = enhanceStream.str();
                        result.hasData = true;

                        DebugLog(ModuleOldFormat("[批量查询-优化-强化] level=%s, attrs=[%s]", level, attrs).c_str());
                    }
                    else if (source == "skills")
                    {
                        // 继续使用现有逻辑处理技能（需要模板数据）
                        result.hasData = true;
                    }
                    else if (source == "rune")
                    {
                        uint32 slotCount = std::stoul(fields[1].Get<std::string>());
                        std::string slotIds = fields[2].Get<std::string>();
                        std::string runeItemIds = fields[3].Get<std::string>();

                        std::ostringstream runeStream;
                        runeStream << slotCount << "|" << slotIds << "|" << runeItemIds;
                        result.runeData = runeStream.str();
                        result.hasData = true;

                        DebugLog(ModuleOldFormat("[批量查询-优化-符文] slotCount=%s, slotIds=[%s], runeItemIds=[%s]", slotCount, slotIds, runeItemIds).c_str());
                    }
                    else if (source == "set")
                    {
                        uint32 setId = std::stoul(fields[1].Get<std::string>());

                        if (setId > 0)
                        {
                            // 套装数据需要额外查询（因为涉及WorldDatabase）
                            // 保持原有逻辑
                            result.hasData = true;
                            DebugLog(ModuleOldFormat("[批量查询-优化-套装] setId=%s", setId).c_str());

                            // 后续处理套装详情
                            std::string setName;
                            std::string attrsStr;
                            std::string effectsStr;

#if defined(MODULE_ITEM_SETS)
                            if (sItemSetsManager)
                            {
                                const std::vector<ItemSetData>& setDataList = sItemSetsManager->GetItemSetData(setId);

                                if (!setDataList.empty())
                                {
                                    setName = setDataList[0].SetName;

                                    bool firstAttr = true;
                                    bool firstEffect = true;

                                    for (const auto& setData : setDataList)
                                    {
                                        if (!setData.SetAttributes.empty())
                                        {
                                            if (!firstAttr)
                                                attrsStr += ",";
                                            attrsStr += setData.SetAttributes;
                                            firstAttr = false;
                                        }

                                        if (!setData.EffectDesc.empty())
                                        {
                                            if (!firstEffect)
                                                effectsStr += ",";

                                            firstEffect = false;
                                            effectsStr += Acore::ToString(setData.ItemsCount);
                                            effectsStr += "|";
                                            effectsStr += setData.EffectDesc;
                                        }
                                    }
                                }
                            }
#endif

                            std::ostringstream setStream;
                            setStream << setId;

                            if (!setName.empty() || !attrsStr.empty() || !effectsStr.empty())
                            {
                                setStream << ":" << setName << ":" << attrsStr << ":" << effectsStr;
                            }

                            result.setData = setStream.str();
                        }
                    }

                } while (combinedResult->NextRow());

                DebugLog("[批量查询-优化] UNION查询解析完成");
            }
            else
            {
                DebugLog("[批量查询-优化] UNION查询未返回数据");
            }
        }
        else
        {
        }

    }
    catch (const std::exception& e)
    {
        LOG_ERROR("module.itemidentification", ModuleOldFormat("[批量查询-优化] 发生异常: %s", e.what()).c_str());
    }

    // 特殊处理：技能需要额外查询模板数据
    if (TableExists("_物品技能_数据"))
    {
#if defined(MODULE_ITEM_SKILLS)
        std::vector<ItemSkillData> skills = sItemSkillsDBHelper->GetItemSkillsData(static_cast<uint64>(guid));

        if (!skills.empty())
        {
            std::ostringstream skillStream;
            bool first = true;

            for (const auto& skillData : skills)
            {
                ItemSkillTemplate const* skillTemplate = sItemSkillsManager->GetSkillTemplate(skillData.skillTemplateId);
                if (!skillTemplate)
                    continue;

                if (!first)
                    skillStream << ",";

                first = false;
                skillStream << skillData.skillId << "|" << skillTemplate->clientDisplay << "|" << skillTemplate->level;
            }

            std::string skillsStr = skillStream.str();
            if (!skillsStr.empty())
            {
                result.skillsData = skillsStr;
                result.hasData = true;
                DebugLog(ModuleOldFormat("[批量查询-优化-技能] skills=[%s]", result.skillsData).c_str());
            }
        }
#endif
    }

    if (isIdentified && TableExists("物品_鉴定记录"))
    {
        QueryResult templateResult = CharacterDatabase.Query(ModuleOldFormat("SELECT `鉴定模板ID` FROM `物品_鉴定记录` WHERE `物品GUID` = %s LIMIT 1", guid).c_str());

        if (templateResult)
        {
            uint32 templateId = templateResult->Fetch()[0].Get<uint32>();
            auto tmplItr = _identificationTemplates.find(templateId);
            if (tmplItr != _identificationTemplates.end())
            {
                result.identificationDisplayData = BuildIdentificationDisplayData(tmplItr->second);
                if (!result.identificationDisplayData.empty())
                {
                    result.hasData = true;
                    DebugLog(ModuleOldFormat("[批量查询-优化-鉴定显示] templateId=%s, data=[%s]", templateId, result.identificationDisplayData).c_str());
                }
            }
        }
    }

    // 【新增】按物品状态查询幻境系统倍率数据
    {
        bool foundHuanjingData = false;
        int multiplier = 0;
        char multiplierMode = 'x';
        std::string enhancedAttrs;

        if (isIdentified)
        {
            QueryResult huanjingResult = CharacterDatabase.Query(ModuleOldFormat("SELECT `属性倍率`, `属性倍率模式`, `增强属性数据` FROM `玩家装备属性增强` WHERE `装备GUID` = %s AND `装备ID` = %s", guid, itemID).c_str());

            if (huanjingResult)
            {
                Field* fields = huanjingResult->Fetch();
                multiplier = fields[0].Get<int>();
                multiplierMode = DbValueToHuanJingMode(fields[1].Get<int32>());
                enhancedAttrs = fields[2].Get<std::string>();

                if (HasHuanJingEffect(multiplier, multiplierMode))
                {
                    foundHuanjingData = true;
                    DebugLog(ModuleOldFormat("[批量查询-优化-幻境] 从玩家装备属性增强表找到: multiplier=%s, enhancedAttrs=[%s]", multiplier, enhancedAttrs).c_str());
                }
            }
        }
        else
        {
            QueryResult pendingResult = CharacterDatabase.Query(ModuleOldFormat("SELECT `幻境倍率`, `幻境倍率模式` FROM `待鉴定物品标记` WHERE `物品GUID` = %s AND `物品ID` = %s", guid, itemID).c_str());

            if (pendingResult)
            {
                Field* fields = pendingResult->Fetch();
                multiplier = fields[0].Get<int>();
                multiplierMode = DbValueToHuanJingMode(fields[1].Get<int32>());

                if (HasHuanJingEffect(multiplier, multiplierMode))
                {
                    foundHuanjingData = true;
                    enhancedAttrs = "";
                    DebugLog(ModuleOldFormat("[批量查询-优化-幻境] 从待鉴定物品标记表找到: multiplier=%s (待鉴定)", multiplier).c_str());
                }
            }
        }

        // 构建幻境数据字符串
        if (foundHuanjingData && HasHuanJingEffect(multiplier, multiplierMode))
        {
            std::ostringstream huanjingStream;
            huanjingStream << multiplierMode << "," << multiplier;

            // 如果有增强属性数据，添加到结果中
            if (!enhancedAttrs.empty())
            {
                huanjingStream << "|" << enhancedAttrs;
            }

            result.huanjingData = huanjingStream.str();
            result.hasData = true;

            DebugLog(ModuleOldFormat("[批量查询-优化-幻境] 最终结果: huanjingData=[%s]", result.huanjingData).c_str());
        }
    }

    DebugLog(ModuleOldFormat("[批量查询-优化] 完成查询: itemID=%s, guid=%s, 有数据=%s", itemID, guid, result.hasData).c_str());

    return result;
}


// 通过 Addon 消息发送批量数据（ALL_MODULE_DATA:itemID:guid:...）
void ItemIdentificationSystem::SendAllModuleDataAddon(Player* player, uint32 itemID, uint32 guid, bool invalidateCache)
{
    if (!player)
        return;

    if (invalidateCache)
    {
        std::lock_guard<std::mutex> lock(_batchCacheMutex);
        uint64 cacheKey = (static_cast<uint64>(itemID) << 32) | guid;
        _batchQueryCache.erase(cacheKey);
    }

    AllModuleData data = QueryAllModuleData(itemID, guid);
    Item* item = FindPlayerItemByGuidForAddon(player, guid);
    ClientItemLocation location = GetClientItemLocation(item);
    bool hasDisplayData = !data.baseAttributes.empty()
        || !data.additionalAttributes.empty()
        || !data.identificationDisplayData.empty()
        || !data.growthData.empty()
        || !data.enhancementData.empty()
        || !data.skillsData.empty()
        || !data.runeData.empty()
        || !data.setData.empty()
        || !data.huanjingData.empty()
        || !data.pendingIdentifyData.empty();

    if (!hasDisplayData)
    {
        return;
    }

    std::ostringstream response;
    if (location.valid)
        response << "ALL_MODULE_DATA:" << location.bag << ":" << static_cast<uint32>(location.slot) << ":" << itemID << ":" << guid << ":";
    else
        response << "ALL_MODULE_DATA:" << itemID << ":" << guid << ":";

    response
             << data.baseAttributes << ":"
             << data.additionalAttributes << ":"
             << data.identificationDisplayData << ":"
             << data.growthData << ":"
             << data.enhancementData << ":"
             << data.skillsData << ":"
             << data.runeData << ":"
             << data.setData << ":"
             << data.huanjingData;
    if (!data.pendingIdentifyData.empty())
        response << ":" << data.pendingIdentifyData;
        if (player)
    {
        std::string limitSection;
        auto appendLimit = [&limitSection](char const* label, uint32 now, uint32 limit)
        {
            if (!limit)
                return;
            if (!limitSection.empty())
                limitSection += "\n";
            limitSection += std::string("|cFFFFD200") + label + "|r " + std::to_string(now) + " / " + std::to_string(limit);
        };

        auto current = [&player, itemID](auto const& map) -> uint32
        {
            auto it = map.find(itemID);
            return it == map.end() ? 0u : it->second;
        };

        auto limitItr = AtAddItemLimitStore.find(itemID);
        if (limitItr != AtAddItemLimitStore.end())
        {
            appendLimit("角色每日限制", current(player->PlayerDayItemLimitMap), limitItr->second.CharDayLimt);
            appendLimit("角色每周限制", current(player->PlayerFrItemLimitMap), limitItr->second.CharFrLimt);
            appendLimit("账号每日限制", current(player->PlayerAccDayItemLimitMap), limitItr->second.AccDayLimt);
            appendLimit("账号每周限制", current(player->PlayerAccFrItemLimitMap), limitItr->second.AccFrLimt);
        }

        auto useItr = UseItemLimitStore.find(itemID);
        if (useItr != UseItemLimitStore.end())
        {
            appendLimit("每日使用上限", current(player->DayUseItemLimitMap), useItr->second.UseDayLimt);
            appendLimit("每周使用上限", current(player->FrUseItemLimitMap), useItr->second.UseFrLimt);
        }

        if (!limitSection.empty())
            response << ":" << "LIMIT|" << limitSection;
    }
response << ":" << data.templateStatsData;

    std::string responseStr = response.str();

    if (player->GetSession()->HasNativeUI())
        player->GetSession()->SendNativeUITooltipData(itemID, guid, responseStr);
    else
        SendItemIdentificationAddonMessage(player, responseStr);
}

bool FrozenCoreQueryItemModuleData(uint32 itemId, uint32 guidLow, FrozenCoreModuleData& out)
{
    if (itemId == 0 || !sItemIdentificationSystem || !sItemIdentificationSystem->_enabled)
        return false;

    ItemIdentificationSystem::AllModuleData data = sItemIdentificationSystem->QueryAllModuleData(itemId, guidLow);
    out.hasData = data.hasData;
    out.baseAttributes = data.baseAttributes;
    out.additionalAttributes = data.additionalAttributes;
    out.identificationDisplayData = data.identificationDisplayData;
    out.growthData = data.growthData;
    out.enhancementData = data.enhancementData;
    out.skillsData = data.skillsData;
    out.runeData = data.runeData;
    out.setData = data.setData;
    out.huanjingData = data.huanjingData;
    out.pendingIdentifyData = data.pendingIdentifyData;
    out.templateStatsData = data.templateStatsData;
    return true;
}

void FrozenCoreSendItemModuleData(Player* player, uint32 itemId, uint32 guidLow)
{
    if (!player || itemId == 0 || !sItemIdentificationSystem || !sItemIdentificationSystem->_enabled)
        return;

    sItemIdentificationSystem->SendAllModuleDataAddon(player, itemId, guidLow);
}

void FrozenCoreSendItemModuleDataNative(Player* player, uint32 itemId, uint32 guidLow)
{
    if (!player || itemId == 0 || !sItemIdentificationSystem || !sItemIdentificationSystem->_enabled)
        return;

    sItemIdentificationSystem->SendAllModuleDataAddon(player, itemId, guidLow, false);
}

void FrozenCoreSendItemTemplateDataNative(Player* player, uint32 itemId)
{
    if (!player || !itemId || !sItemIdentificationSystem || !sItemIdentificationSystem->_enabled ||
        !sObjectMgr->GetItemTemplate(itemId))
        return;

    std::string templateStats = BuildTemplateStatsData(itemId);
    if (templateStats.empty())
        return;

    std::ostringstream response;
    response << "ALL_MODULE_DATA:" << itemId << ":0";
    for (uint8 index = 0; index < 9; ++index)
        response << ":";
    response << ":" << templateStats;
    player->GetSession()->SendNativeUITooltipData(itemId, 0, response.str());
}

// 批量查询命令处理器（一次性返回所有数据）
void ItemIdentificationSystem::HandleBatchQueryCommand(Player* player, uint32 itemID, uint32 guid)
{
    if (!player)
        return;

    auto queryStart = std::chrono::high_resolution_clock::now();

    // 优先从缓存读取
    uint64 cacheKey = ((uint64)itemID << 32) | guid;

    AllModuleData data;
    bool cacheHit = false;

    // 【线程安全】加锁读取缓存
    {
        std::lock_guard<std::mutex> lock(_batchCacheMutex);

        auto it = _batchQueryCache.find(cacheKey);

        // 更新统计：总查询次数
        _perfStats.totalQueries++;

        if (it != _batchQueryCache.end())
        {
            // 检查缓存是否过期
            time_t now = time(nullptr);
            if ((now - it->second.cacheTime) < BATCH_CACHE_EXPIRE_TIME)
            {
                // 缓存有效，直接使用
                data = it->second.data;
                cacheHit = true;
                _perfStats.cacheHits++;  // 统计：缓存命中
                DebugLog(ModuleOldFormat("[批量查询缓存] 缓存命中: itemID=%s, guid=%s", itemID, guid).c_str());
            }
            else
            {
                // 缓存过期，删除
                _batchQueryCache.erase(it);
                _perfStats.cacheMisses++;  // 统计：缓存未命中
                DebugLog(ModuleOldFormat("[批量查询缓存] 缓存过期: itemID=%s, guid=%s", itemID, guid).c_str());
            }
        }
        else
        {
            _perfStats.cacheMisses++;  // 统计：缓存未命中
        }
    }

    if (!cacheHit)
    {
        // 缓存未命中，在锁外查询数据库（避免长时间持有锁）
        _perfStats.dbQueries++;  // 统计：数据库查询
        data = QueryAllModuleData(itemID, guid);

        StoreBatchQueryCache(cacheKey, data);

        DebugLog(ModuleOldFormat("[批量查询缓存] 新数据已缓存: itemID=%s, guid=%s", itemID, guid).c_str());
    }

    // 新格式消息：ALL_MODULE_DATA:itemID:guid:base:additional:identDisplay:growth:enhancement:skills:rune:set:huanjing:templateStats
    std::ostringstream response;
    response << "ALL_MODULE_DATA:" << itemID << ":" << guid << ":"
             << data.baseAttributes << ":"
             << data.additionalAttributes << ":"
             << data.identificationDisplayData << ":"
             << data.growthData << ":"
             << data.enhancementData << ":"
             << data.skillsData << ":"
             << data.runeData << ":"
             << data.setData << ":"
             << data.huanjingData;
    if (!data.pendingIdentifyData.empty())
        response << ":" << data.pendingIdentifyData;
    response << ":" << data.templateStatsData;

    // 发送前记录完整消息内容
    DebugLog(ModuleOldFormat("[批量查询命令] 准备发送完整消息: [%s]", response.str()).c_str());
    DebugLog("[批量查询命令] 各字段详情:");
    DebugLog(ModuleOldFormat("  - 基础属性: [%s]", data.baseAttributes).c_str());
    DebugLog(ModuleOldFormat("  - 追加属性: [%s]", data.additionalAttributes).c_str());
    DebugLog(ModuleOldFormat("  - 鉴定显示: [%s]", data.identificationDisplayData).c_str());
    DebugLog(ModuleOldFormat("  - 成长数据: [%s]", data.growthData).c_str());
    DebugLog(ModuleOldFormat("  - 强化数据: [%s]", data.enhancementData).c_str());
    DebugLog(ModuleOldFormat("  - 技能数据: [%s]", data.skillsData).c_str());
    DebugLog(ModuleOldFormat("  - 符文数据: [%s]", data.runeData).c_str());
    DebugLog(ModuleOldFormat("  - 套装数据: [%s]", data.setData).c_str());
    DebugLog(ModuleOldFormat("  - 模板属性: [%s]", data.templateStatsData).c_str());

    // 发送到客户端
    ChatHandler(player->GetSession()).PSendSysMessage(response.str().c_str());

    // 计算查询耗时
    auto queryEnd = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(queryEnd - queryStart).count();
    _perfStats.totalQueryTime += duration;
    _perfStats.avgQueryTime = _perfStats.totalQueryTime / _perfStats.totalQueries;

    DebugLog(ModuleOldFormat("[批量查询命令] 已发送数据: itemID=%s, guid=%s, 消息长度=%s, 缓存命中=%s, 耗时=%sμs", itemID, guid, response.str().length(), cacheHit, duration).c_str());
}

// 清理过期缓存
bool ItemIdentificationSystem::StoreBatchQueryCache(uint64 cacheKey, AllModuleData const& data)
{
    std::lock_guard<std::mutex> lock(_batchCacheMutex);
    auto itr = _batchQueryCache.find(cacheKey);
    if (itr != _batchQueryCache.end())
    {
        itr->second.data = data;
        itr->second.cacheTime = time(nullptr);
        return true;
    }

    if (_batchQueryCache.size() >= _batchCacheMaxEntries)
    {
        if (!_batchCacheLimitWarningLogged)
        {
            LOG_WARN("module.itemidentification", "批量查询缓存达到上限 {}，暂停缓存新条目", _batchCacheMaxEntries);
            _batchCacheLimitWarningLogged = true;
        }
        return false;
    }

    BatchQueryCache cache;
    cache.data = data;
    cache.cacheTime = time(nullptr);
    _batchQueryCache.emplace(cacheKey, std::move(cache));
    return true;
}

void ItemIdentificationSystem::CleanExpiredCache()
{
    time_t now = time(nullptr);
    uint32 cleanedAttr = 0;
    uint32 cleanedBatch = 0;

    // 清理属性缓存（不需要互斥锁，只在主线程访问）
    for (auto it = _attrCache.begin(); it != _attrCache.end(); )
    {
        if ((now - it->second.cacheTime) >= ATTR_CACHE_EXPIRE_TIME)
        {
            it = _attrCache.erase(it);
            cleanedAttr++;
        }
        else
        {
            ++it;
        }
    }

    // 【线程安全】清理批量查询缓存（可能被异步线程访问）
    {
        std::lock_guard<std::mutex> lock(_batchCacheMutex);

        for (auto it = _batchQueryCache.begin(); it != _batchQueryCache.end(); )
        {
            if ((now - it->second.cacheTime) >= BATCH_CACHE_EXPIRE_TIME)
            {
                it = _batchQueryCache.erase(it);
                cleanedBatch++;
            }
            else
            {
                ++it;
            }
        }

        if (_batchQueryCache.size() < _batchCacheMaxEntries)
            _batchCacheLimitWarningLogged = false;
    }

    if (cleanedAttr > 0 || cleanedBatch > 0)
    {
        LOG_DEBUG("module.itemidentification", ModuleOldFormat("[缓存清理] 属性缓存: %s, 批量缓存: %s", cleanedAttr, cleanedBatch).c_str());
    }
}

// 批量查询命令处理函数
bool ItemIdentificationCommandScript::HandleBatchQueryCommand(ChatHandler* handler, const char* args)
{
    if (!sItemIdentificationSystem->_enabled)
        return true;

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 解析参数：.鉴定 批量查询 <物品ID> <GUID>
    if (!args || !*args)
    {
        handler->SendSysMessage("用法: .鉴定 批量查询 <物品ID> <GUID>");
        return true;
    }

    std::istringstream iss(args);
    uint32 itemID = 0;
    uint32 guid = 0;

    iss >> itemID >> guid;

    if (itemID == 0 || guid == 0)
    {
        handler->SendSysMessage("无效的物品ID或GUID");
        return true;
    }

    // 执行批量查询
    sItemIdentificationSystem->HandleBatchQueryCommand(player, itemID, guid);

    return true;
}

// ============================================================================
// 高级优化功能实现
// ============================================================================

// 预加载玩家装备数据（登录时调用）- 异步优化版本
void ItemIdentificationSystem::PreloadPlayerEquipment(Player* player)
{
    if (!player || !_enabled)
        return;

    // 保存玩家名称（避免在异步线程中访问Player对象）
    std::string playerName = player->GetName();

    std::vector<std::pair<uint32, uint32>> itemsToPreload;  // <itemID, guid>

    // 【主线程】快速收集所有需要预加载的装备GUID（不涉及数据库查询）
    // 收集所有已鉴定的装备
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item)
        {
            uint32 itemGuid = item->GetGUID().GetCounter();
            if (IsItemIdentified(itemGuid))
            {
                itemsToPreload.push_back(std::make_pair(item->GetEntry(), itemGuid));
            }
        }
    }

    // 收集背包中的已鉴定装备
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item)
        {
            uint32 itemGuid = item->GetGUID().GetCounter();
            if (IsItemIdentified(itemGuid))
            {
                ItemTemplate const* proto = item->GetTemplate();
                if (proto && (proto->Class == ITEM_CLASS_WEAPON || proto->Class == ITEM_CLASS_ARMOR))
                {
                    itemsToPreload.push_back(std::make_pair(item->GetEntry(), itemGuid));
                }
            }
        }
    }

    // 收集背包袋中的已鉴定装备
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* bag = player->GetBagByPos(i))
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                if (item)
                {
                    uint32 itemGuid = item->GetGUID().GetCounter();
                    if (IsItemIdentified(itemGuid))
                    {
                        ItemTemplate const* proto = item->GetTemplate();
                        if (proto && (proto->Class == ITEM_CLASS_WEAPON || proto->Class == ITEM_CLASS_ARMOR))
                        {
                            itemsToPreload.push_back(std::make_pair(item->GetEntry(), itemGuid));
                        }
                    }
                }
            }
        }
    }

    // 如果没有需要预加载的装备，直接返回
    if (itemsToPreload.empty())
    {
        return;
    }

    // 【异步线程】在后台线程中执行数据库查询，不阻塞主线程
    std::thread([this, itemsToPreload, playerName]() {
        auto startTime = std::chrono::high_resolution_clock::now();
        uint32 preloadCount = 0;
        uint32 cacheHitCount = 0;

        // 异步执行数据库查询和缓存
        for (const auto& itemPair : itemsToPreload)
        {
            uint32 itemID = itemPair.first;
            uint32 guid = itemPair.second;
            uint64 cacheKey = ((uint64)itemID << 32) | guid;

            // 使用互斥锁保护缓存访问
            {
                std::lock_guard<std::mutex> lock(_batchCacheMutex);

                // 检查是否已在缓存中
                if (_batchQueryCache.find(cacheKey) != _batchQueryCache.end())
                {
                    cacheHitCount++;
                    continue;
                }
            }

            // 在锁外执行数据库查询（避免长时间持有锁）
            AllModuleData data = QueryAllModuleData(itemID, guid);

                StoreBatchQueryCache(cacheKey, data);

            preloadCount++;
        }

        // 【线程安全修复】使用专用方法更新性能统计
        IncrementPreloadCount(preloadCount);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();



        DebugLog(ModuleOldFormat("[预加载-异步] 玩家=%s, 装备总数=%s, 缓存命中=%s, 查询数=%s, 耗时=%sms", playerName, itemsToPreload.size(), cacheHitCount, preloadCount, duration).c_str());

    }).detach();  // 分离线程，让其在后台执行
}

// 重置性能统计
void ItemIdentificationSystem::ResetPerformanceStats()
{
    _perfStats = PerformanceStats();
    LOG_INFO("module.itemidentification", "[性能统计] 已重置统计数据");
}

// 性能统计命令处理函数
bool ItemIdentificationCommandScript::HandlePerformanceStatsCommand(ChatHandler* handler, const char* args)
{
    if (!sItemIdentificationSystem->_enabled)
    {
        handler->SendSysMessage("物品鉴定系统当前已禁用");
        return true;
    }

    const auto& stats = sItemIdentificationSystem->GetPerformanceStats();

    handler->PSendSysMessage("|cff00ff00========== 鉴定系统性能统计 ==========|r");
    handler->PSendSysMessage("%s", ModuleOldFormat("总查询次数: |cffff8000%s|r", stats.totalQueries).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("缓存命中: |cff00ff00%s|r 次 (%s%%)", stats.cacheHits, stats.GetCacheHitRate()).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("缓存未命中: |cffff0000%s|r 次 (%s%%)", stats.cacheMisses, stats.totalQueries > 0 ? (float)stats.cacheMisses / stats.totalQueries * 100.0f : 0.0f).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("数据库查询: |cffffa500%s|r 次", stats.dbQueries).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("平均查询时间: |cffadd8e6%s|r 毫秒", stats.avgQueryTime / 1000.0).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("总查询时间: |cffadd8e6%s|r 秒", stats.totalQueryTime / 1000000.0).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("预加载次数: |cff00ff00%s|r", stats.preloadCount).c_str());

    // 运行时间
    time_t now = time(nullptr);
    uint32 runningTime = now - stats.startTime;
    uint32 hours = runningTime / 3600;
    uint32 minutes = (runningTime % 3600) / 60;
    uint32 seconds = runningTime % 60;
    handler->PSendSysMessage("%s", ModuleOldFormat("运行时间: |cffffffff%s时%s分%s秒|r", hours, minutes, seconds).c_str());

    // 缓存状态
    handler->PSendSysMessage("|cff00ff00========== 缓存状态 ==========|r");
    handler->PSendSysMessage("%s", ModuleOldFormat("属性缓存大小: |cffadd8e6%s|r", sItemIdentificationSystem->GetAttrCacheSize()).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("批量查询缓存大小: |cffadd8e6%s|r", sItemIdentificationSystem->GetBatchQueryCacheSize()).c_str());
    handler->PSendSysMessage("%s", ModuleOldFormat("已鉴定物品缓存: |cffadd8e6%s|r", sItemIdentificationSystem->GetIdentifiedCacheSize()).c_str());

    // 性能评估
    handler->PSendSysMessage("|cff00ff00========== 性能评估 ==========|r");
    if (stats.GetCacheHitRate() >= 80.0f)
    {
        handler->SendSysMessage("|cff00ff00性能状态: 优秀|r");
    }
    else if (stats.GetCacheHitRate() >= 60.0f)
    {
        handler->SendSysMessage("|cffffa500性能状态: 良好|r");
    }
    else if (stats.GetCacheHitRate() >= 40.0f)
    {
        handler->SendSysMessage("|cffff8000性能状态: 一般，考虑调整缓存策略|r");
    }
    else
    {
        handler->SendSysMessage("|cffff0000性能状态: 较差，需要优化|r");
    }

    if (stats.avgQueryTime < 5000)  // <5ms
    {
        handler->SendSysMessage("|cff00ff00查询速度: 极快|r");
    }
    else if (stats.avgQueryTime < 20000)  // <20ms
    {
        handler->SendSysMessage("|cff00ff00查询速度: 快速|r");
    }
    else if (stats.avgQueryTime < 50000)  // <50ms
    {
        handler->SendSysMessage("|cffffa500查询速度: 正常|r");
    }
    else if (stats.avgQueryTime < 100000)  // <100ms
    {
        handler->SendSysMessage("|cffff8000查询速度: 较慢，考虑优化数据库|r");
    }
    else
    {
        handler->SendSysMessage("|cffff0000查询速度: 很慢，需要立即优化！|r");
    }

    handler->PSendSysMessage("|cff888888提示: 使用 |cffffffff.鉴定 性能统计 reset|r |cff888888重置统计|r");

    // 检查是否需要重置
    if (args && *args)
    {
        std::string arg = args;
        if (arg == "reset")
        {
            sItemIdentificationSystem->ResetPerformanceStats();
            handler->SendSysMessage("|cff00ff00性能统计已重置|r");
        }
    }

    return true;
}

bool ItemIdentificationCommandScript::HandleReloadCommand(ChatHandler* handler, const char* /*args*/)
{
    handler->SendSysMessage("|cffffff00正在重载物品鉴定系统配置和模板...|r");

    sItemIdentificationSystem->LoadConfig(true);

    handler->SendSysMessage("|cff00ff00物品鉴定系统配置和模板重载完成|r");
    return true;
}

// ========== 【新增】手动鉴定相关命令实现 ==========

// 辅助函数：在玩家背包中通过GUID查找物品
static Item* FindItemByGuidInBags(Player* player, uint32 itemGuid)
{
    if (!player)
        return nullptr;

    // 1. 搜索装备栏
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetGUID().GetCounter() == itemGuid)
            return item;
    }

    // 2. 搜索主背包
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (item && item->GetGUID().GetCounter() == itemGuid)
            return item;
    }

    // 3. 搜索背包袋
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* bag = player->GetBagByPos(i))
        {
            for (uint32 j = 0; j < bag->GetBagSize(); ++j)
            {
                Item* item = bag->GetItemByPos(j);
                if (item && item->GetGUID().GetCounter() == itemGuid)
                    return item;
            }
        }
    }

    return nullptr;
}

// 辅助函数：通过背包位置获取物品
// bagSlot: 0=主背包, 1-4=额外背包
// slot: 槽位索引
static Item* GetItemByBagSlot(Player* player, uint8 bagSlot, uint8 slot)
{
    if (!player)
        return nullptr;

    if (bagSlot == 0)
    {
        // 主背包
        return player->GetItemByPos(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START + slot);
    }
    else if (bagSlot >= 1 && bagSlot <= 4)
    {
        // 额外背包 (1-4 对应 INVENTORY_SLOT_BAG_START 到 INVENTORY_SLOT_BAG_END-1)
        Bag* bag = player->GetBagByPos(INVENTORY_SLOT_BAG_START + bagSlot - 1);
        if (bag && slot < bag->GetBagSize())
        {
            return bag->GetItemByPos(slot);
        }
    }

    return nullptr;
}

// 辅助函数：执行单个物品的鉴定逻辑
static bool DoIdentifyItem(Player* player, Item* item, ChatHandler* handler = nullptr)
{
    if (!player || !item)
        return false;

    uint32 itemGuid = item->GetGUID().GetCounter();
    uint32 playerGuid = player->GetGUID().GetCounter();

    // 从数据库查询待鉴定标记
    QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
        "WHERE `物品GUID` = %s AND `玩家GUID` = %s", itemGuid, playerGuid).c_str());

    if (!result)
    {
        if (handler)
            handler->SendSysMessage("|cffff0000此物品不需要鉴定或已鉴定|r");
        return false;
    }

    Field* fields = result->Fetch();
    uint32 itemId = fields[0].Get<uint32>();
    uint32 huanJingMultiplier = fields[1].Get<uint32>();
    char huanJingMode = DbValueToHuanJingMode(fields[2].Get<int32>());
    uint32 identificationGroupId = fields[3].Get<uint32>();

    // 验证物品ID是否匹配
    if (item->GetEntry() != itemId)
    {
        CharacterDatabase.DirectExecute(ModuleOldFormat("DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = %s", itemGuid).c_str());
        if (handler)
            handler->SendSysMessage("|cffff0000物品数据不匹配|r");
        return false;
    }

    // 获取物品名称
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    std::string itemName = itemTemplate ? itemTemplate->Name1 : "未知物品";

    if (handler)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("|cff00ff00开始鉴定物品: |r|cffffffff%s|r", itemName).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("|cff00ff00幻境模式: |r|cffff8000%s%s|r", huanJingMode == '+' ? "+" : (huanJingMode == '-' ? "关闭" : "x"), huanJingMultiplier).c_str());
    }

    // 执行鉴定（如果有鉴定组）
    // 【审计修复】使用 IdentifyItem 进行鉴定，确保成功率和费用扣除逻辑生效
    bool identifySuccess = true;
    if (identificationGroupId > 0)
    {
        identifySuccess = sItemIdentificationSystem->IdentifyItem(player, item, identificationGroupId);
        // IdentifyItem 内部已经发送了成功/失败消息，这里不需要重复发送
    }
    else
    {
        if (handler)
            handler->SendSysMessage("|cffffa500此物品没有配置鉴定组，只应用了幻境倍率|r");
    }

    // 删除待鉴定标记
    // 【修复】仅成功（或无鉴定组仅应用倍率）时删除；掷骰失败等可恢复失败保留标记允许重试
    if (identifySuccess)
    {
        CharacterDatabase.DirectExecute(ModuleOldFormat("DELETE FROM `待鉴定物品标记` WHERE `物品GUID` = %s", itemGuid).c_str());
    }

    if (identificationGroupId == 0)
        QueueAllModuleDataAddonRefresh(player, itemId, itemGuid, 2000ms);

    return identifySuccess;
}

// 手动鉴定命令处理
bool ItemIdentificationCommandScript::HandleRecycleCommand(ChatHandler* handler, const char* args)
{
    Player* player = handler ? handler->GetSession()->GetPlayer() : nullptr;
    if (!player || !args || !*args)
    {
        if (handler)
            handler->SendSysMessage("用法：.鉴定 回收 物品GUID");
        return true;
    }

    uint32 itemGuid = uint32(std::strtoul(args, nullptr, 10));
    Item* item = itemGuid ? player->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(itemGuid)) : nullptr;
    if (!item)
    {
        handler->SendSysMessage("未找到该物品");
        return true;
    }

    if (!sItemIdentificationSystem->RecycleItem(player, item))
    {
        handler->SendSysMessage("该物品没有配置回收奖励，或物品仍在装备栏");
        return true;
    }

    handler->SendSysMessage("装备回收成功，奖励已发放");
    return true;
}

bool ItemIdentificationCommandScript::HandleBatchRecycleCommand(ChatHandler* handler, const char* args)
{
    Player* player = handler ? handler->GetSession()->GetPlayer() : nullptr;
    if (!player)
        return false;

    uint32 identificationLevel = 0;
    if (args && *args)
        identificationLevel = uint32(std::strtoul(args, nullptr, 10));

    uint32 skippedCount = 0;
    uint32 recycledCount = sItemIdentificationSystem->RecycleAllBackpackItems(player, skippedCount, identificationLevel);
    if (identificationLevel > 0)
        handler->PSendSysMessage("鉴定等级 %u 批量回收完成：成功回收 %u 件，跳过 %u 件", identificationLevel, recycledCount, skippedCount);
    else
        handler->PSendSysMessage("全部鉴定等级批量回收完成：成功回收 %u 件，跳过 %u 件", recycledCount, skippedCount);
    return true;
}

// 手动鉴定命令处理
// 用法: .鉴定 手动 [背包号 槽位号] 或 .鉴定 手动 (鉴定鼠标指向的物品)
bool ItemIdentificationCommandScript::HandleManualIdentifyCommand(ChatHandler* handler, const char* args)
{
    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 检查系统是否启用
    if (!sItemIdentificationSystem->_enabled)
    {
        handler->SendSysMessage("|cffff0000物品鉴定系统当前已禁用|r");
        return true;
    }

    // 如果没有参数，显示用法
    if (!args || !*args)
    {
        handler->SendSysMessage("|cff00ff00===== 手动鉴定用法 =====|r");
        handler->SendSysMessage(".鉴定 手动 <背包号> <槽位号>");
        handler->SendSysMessage("  背包号: 0=主背包, 1-4=额外背包");
        handler->SendSysMessage("  槽位号: 从0开始的物品位置");
        handler->SendSysMessage("");
        handler->SendSysMessage("示例: .鉴定 手动 0 5  (鉴定主背包第6格物品)");
        handler->SendSysMessage("示例: .鉴定 手动 1 0  (鉴定第1个额外背包第1格)");
        handler->SendSysMessage("");
        handler->SendSysMessage("|cff888888提示: 使用 .鉴定 待鉴定 查看待鉴定列表|r");
        handler->SendSysMessage("|cff888888提示: 使用 .鉴定 批量鉴定 一键鉴定所有|r");
        return true;
    }

    // 解析参数: 背包号 槽位号
    // 【安全修复】使用std::strtol并增加输入验证
    char* bagStr = strtok((char*)args, " ");
    char* slotStr = strtok(nullptr, " ");

    if (!bagStr || !slotStr)
    {
        handler->SendSysMessage("|cffff0000参数不足！用法: .鉴定 手动 <背包号> <槽位号>|r");
        return true;
    }

    // 【安全修复】验证输入是否为有效数字
    char* endPtr = nullptr;
    long bagLong = strtol(bagStr, &endPtr, 10);
    if (endPtr == bagStr || *endPtr != '\0' || bagLong < 0 || bagLong > 255)
    {
        handler->SendSysMessage("|cffff0000无效的背包号，必须是0-255之间的整数|r");
        return true;
    }

    endPtr = nullptr;
    long slotLong = strtol(slotStr, &endPtr, 10);
    if (endPtr == slotStr || *endPtr != '\0' || slotLong < 0 || slotLong > 255)
    {
        handler->SendSysMessage("|cffff0000无效的槽位号，必须是0-255之间的整数|r");
        return true;
    }

    uint8 bagSlot = static_cast<uint8>(bagLong);
    uint8 slot = static_cast<uint8>(slotLong);

    if (bagSlot > 4)
    {
        handler->SendSysMessage("|cffff0000无效的背包号！0=主背包, 1-4=额外背包|r");
        return true;
    }

    // 获取物品
    Item* item = GetItemByBagSlot(player, bagSlot, slot);
    if (!item)
    {
        handler->PSendSysMessage("%s", ModuleOldFormat("|cffff0000背包%s槽位%s没有物品|r", bagSlot, slot).c_str());
        return true;
    }

    // 执行鉴定
    DoIdentifyItem(player, item, handler);

    return true;
}

// 查询待鉴定物品列表命令处理
// 用法: .鉴定 待鉴定
bool ItemIdentificationCommandScript::HandleListPendingCommand(ChatHandler* handler, const char* /*args*/)
{
    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();

    QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT `物品GUID`, `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
        "WHERE `玩家GUID` = %s ORDER BY `物品GUID` DESC LIMIT 50", playerGuid).c_str());

    if (!result)
    {
        handler->SendSysMessage("|cff00ff00没有待鉴定的物品|r");
        return true;
    }

    handler->SendSysMessage("|cff00ff00===== 待鉴定物品列表 =====|r");

    uint32 count = 0;
    uint32 validCount = 0;
    std::unordered_map<uint32, Item*> itemsByGuid = BuildPlayerItemGuidIndex(player);

    // 遍历结果，同时查找物品在背包中的位置
    do
    {
        Field* fields = result->Fetch();
        uint32 guid = fields[0].Get<uint32>();
        uint32 itemId = fields[1].Get<uint32>();
        uint32 multiplier = fields[2].Get<uint32>();
        char multiplierMode = DbValueToHuanJingMode(fields[3].Get<int32>());

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        std::string itemName = proto ? proto->Name1 : "未知物品";

        // 查找物品在背包中的位置
        bool found = false;
        uint8 foundBag = 0;
        uint8 foundSlot = 0;

        auto itemItr = itemsByGuid.find(guid);
        Item* item = itemItr != itemsByGuid.end() ? itemItr->second : nullptr;
        if (item)
        {
            uint8 serverBag = item->GetBagSlot();
            uint8 serverSlot = item->GetSlot();

            if (serverBag == INVENTORY_SLOT_BAG_0)
            {
                if (serverSlot >= INVENTORY_SLOT_ITEM_START && serverSlot < INVENTORY_SLOT_ITEM_END)
                {
                    found = true;
                    foundBag = 0;
                    foundSlot = serverSlot - INVENTORY_SLOT_ITEM_START;
                }
                else if (serverSlot >= EQUIPMENT_SLOT_START && serverSlot < EQUIPMENT_SLOT_END)
                {
                    found = true;
                    foundBag = 255;
                    foundSlot = serverSlot + 1;
                }
            }
            else if (serverBag >= INVENTORY_SLOT_BAG_START && serverBag < INVENTORY_SLOT_BAG_END)
            {
                found = true;
                foundBag = serverBag - INVENTORY_SLOT_BAG_START + 1;
                foundSlot = serverSlot;
            }
        }

        if (found)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("|cffffffff[%s]|r |cffff8000%s%s|r |cff00ff00背包%s 槽位%s|r", itemName, multiplierMode == '+' ? "+" : (multiplierMode == '-' ? "关闭" : "x"), multiplier, foundBag, foundSlot).c_str());
            validCount++;
        }
        else
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("|cff555555[%s]|r |cffff0000(物品不在背包)|r", itemName).c_str());
        }

        count++;
    } while (result->NextRow());

    handler->SendSysMessage("");
    handler->PSendSysMessage("%s", ModuleOldFormat("|cff00ff00共 %s 个待鉴定，%s 个在背包中|r", count, validCount).c_str());
    handler->SendSysMessage("");
    handler->SendSysMessage("|cff888888鉴定命令: .鉴定 手动 <背包号> <槽位号>|r");
    handler->SendSysMessage("|cff888888一键鉴定: .鉴定 批量鉴定|r");

    return true;
}

// 批量鉴定所有待鉴定物品命令处理
// 用法: .鉴定 批量鉴定
bool ItemIdentificationCommandScript::HandleBatchIdentifyCommand(ChatHandler* handler, const char* /*args*/)
{
    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 检查系统是否启用
    if (!sItemIdentificationSystem->_enabled)
    {
        handler->SendSysMessage("|cffff0000物品鉴定系统当前已禁用|r");
        return true;
    }

    uint32 playerGuid = player->GetGUID().GetCounter();

    QueryResult result = CharacterDatabase.Query(ModuleOldFormat("SELECT `物品GUID`, `物品ID`, `幻境倍率`, `幻境倍率模式`, `鉴定组ID` FROM `待鉴定物品标记` "
        "WHERE `玩家GUID` = %s ORDER BY `物品GUID` ASC", playerGuid).c_str());

    if (!result)
    {
        handler->SendSysMessage("|cff00ff00没有待鉴定的物品|r");
        return true;
    }

    handler->SendSysMessage("|cff00ff00===== 开始批量鉴定 =====|r");

    uint32 successCount = 0;
    uint32 failCount = 0;
    uint32 skipCount = 0;
    std::vector<uint32> processedGuids;
    std::unordered_map<uint32, Item*> itemsByGuid = BuildPlayerItemGuidIndex(player);

    do
    {
        Field* fields = result->Fetch();
        uint32 itemGuid = fields[0].Get<uint32>();
        uint32 itemId = fields[1].Get<uint32>();
        uint32 huanJingMultiplier = fields[2].Get<uint32>();
        char huanJingMode = DbValueToHuanJingMode(fields[3].Get<int32>());
        uint32 identificationGroupId = fields[4].Get<uint32>();

        // 从本次请求的一次性背包索引中查找物品，避免每个GUID重复扫描背包
        auto itemItr = itemsByGuid.find(itemGuid);
        Item* item = itemItr != itemsByGuid.end() ? itemItr->second : nullptr;
        if (!item)
        {
            // 物品不存在：标记已失效，纳入批量清理
            processedGuids.push_back(itemGuid);
            skipCount++;
            continue;
        }

        // 验证物品ID
        if (item->GetEntry() != itemId)
        {
            processedGuids.push_back(itemGuid);
            skipCount++;
            continue;
        }

        // 执行鉴定
        // 【审计修复】使用 IdentifyItem 进行鉴定，确保成功率和费用扣除逻辑生效
        bool success = true;
        if (identificationGroupId > 0)
        {
            success = sItemIdentificationSystem->IdentifyItem(player, item, identificationGroupId);
        }

        if (success)
        {
            successCount++;
            // 【修复】仅成功时删除待鉴定标记；失败保留标记允许重试
            processedGuids.push_back(itemGuid);
            if (identificationGroupId == 0)
                QueueAllModuleDataAddonRefresh(player, itemId, itemGuid, 2000ms);
        }
        else
        {
            failCount++;
        }

    } while (result->NextRow());

    // 批量删除已处理的标记
    if (!processedGuids.empty())
    {
        std::ostringstream ss;
        ss << "DELETE FROM `待鉴定物品标记` WHERE `物品GUID` IN (";
        for (size_t i = 0; i < processedGuids.size(); ++i)
        {
            if (i > 0) ss << ",";
            ss << processedGuids[i];
        }
        ss << ")";
        CharacterDatabase.DirectExecute(ss.str().c_str());
    }

    handler->SendSysMessage("|cff00ff00===== 批量鉴定完成 =====|r");
    handler->PSendSysMessage("%s", ModuleOldFormat("|cff00ff00成功: %s|r, |cffff0000失败: %s|r, |cff888888跳过: %s|r", successCount, failCount, skipCount).c_str());

    return true;
}

// 处理打开UI界面命令: .鉴定 界面 或 .鉴定 ui
bool ItemIdentificationCommandScript::HandleOpenUICommand(ChatHandler* handler, const char* /*args*/)
{
    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return false;

    // 发送打开UI界面的Addon消息
    SendItemIdentificationAddonMessage(player, "OPEN_UI");

    return true;
}
