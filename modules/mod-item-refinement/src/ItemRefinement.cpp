#include "Bag.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Item.h"
#include "ItemIdentificationSystem.h"
#include "ModuleFormat.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include "../../../src/server/scripts/Custom/ItemFuncMgr/ItemFuncMgr.h"

#include <array>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr char REFINEMENT_PREFIX[] = "IREF";

struct RefinementConfig
{
    uint32 itemId = 0;
    uint32 identificationGroup = 0;
    uint32 requirementId = 0;
    std::set<uint8> categories;
};

std::map<uint32, RefinementConfig> Configs;
std::map<uint32, RefinementConfig> GroupConfigs;
std::set<uint32> AmbiguousGroupConfigs;

enum class OfficialEnchantRefinementState
{
    NotConfigured,
    Ready,
    Invalid
};

bool SameRefinementRules(RefinementConfig const& left, RefinementConfig const& right)
{
    return left.requirementId == right.requirementId && left.categories == right.categories;
}

RefinementConfig const* ResolveConfig(Item* item, bool* usedGroupFallback = nullptr, std::string* outErrorMsg = nullptr)
{
    if (usedGroupFallback)
        *usedGroupFallback = false;
    if (!item)
        return nullptr;

    auto exactItr = Configs.find(item->GetEntry());
    if (exactItr != Configs.end())
        return &exactItr->second;

    uint32 groupId = sItemIdentificationSystem->GetIdentificationGroupForItem(item->GetGUID().GetCounter());

    // 【BUG修复】groupId为0表示物品未鉴定，给出明确提示而非静默返回nullptr
    if (!groupId)
    {
        if (outErrorMsg)
            *outErrorMsg = "该物品尚未鉴定，请先鉴定后再进行洗炼";
        return nullptr;
    }

    // AmbiguousGroupConfigs中的组ID表示配置存在冲突，也给出明确提示
    if (AmbiguousGroupConfigs.find(groupId) != AmbiguousGroupConfigs.end())
    {
        if (outErrorMsg)
            *outErrorMsg = "该物品的洗炼配置存在冲突，请联系管理员";
        return nullptr;
    }

    auto groupItr = GroupConfigs.find(groupId);
    if (groupItr == GroupConfigs.end())
    {
        if (outErrorMsg)
            *outErrorMsg = "该物品没有洗炼配置";
        return nullptr;
    }

    if (usedGroupFallback)
        *usedGroupFallback = true;
    return &groupItr->second;
}

std::vector<std::string> Split(std::string const& text, char separator)
{
    std::vector<std::string> result;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, separator))
        result.push_back(part);
    return result;
}

uint32 ToUInt(std::string const& value)
{
    try { return static_cast<uint32>(std::stoul(value)); }
    catch (...) { return 0; }
}

void Send(Player* player, std::string const& payload)
{
    std::string message = std::string(REFINEMENT_PREFIX) + "\t" + payload;
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, message, 0);
    player->SendDirectMessage(&packet);
}

std::string BuildEnchantValue(Item* item)
{
    if (!item)
        return {};

    std::ostringstream value;
    bool first = true;
    for (uint8 slot = 0; slot < 5; ++slot)
    {
        uint32 enchantId = item->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + slot));
        if (!enchantId)
            continue;
        if (!first)
            value << ',';
        value << enchantId;
        first = false;
    }
    return value.str();
}

uint32 GetOfficialEnchantGroup(ItemEnchTemp const& config, uint8 slot)
{
    switch (slot)
    {
        case 0: return config.EnchGroup1;
        case 1: return config.EnchGroup2;
        case 2: return config.EnchGroup3;
        case 3: return config.EnchGroup4;
        case 4: return config.EnchGroup5;
        default: return 0;
    }
}

OfficialEnchantRefinementState ValidateOfficialEnchantRefinement(Item* item, std::string& error)
{
    if (!item)
    {
        error = "洗炼物品不存在";
        return OfficialEnchantRefinementState::Invalid;
    }

    auto configItr = ItemEnchStore.find(item->GetEntry());
    if (configItr == ItemEnchStore.end())
        return OfficialEnchantRefinementState::NotConfigured;

    ItemEnchTemp const& config = configItr->second;
    if (config.Rts < 0)
    {
        error = "该物品的官方附魔禁止洗炼";
        return OfficialEnchantRefinementState::Invalid;
    }

    if (config.Rts > 0 && int32(item->ItemRtsCount) >= config.Rts)
    {
        error = "该物品的官方附魔洗炼次数已用完";
        return OfficialEnchantRefinementState::Invalid;
    }

    bool hasRefinableEnchant = false;
    for (uint8 slot = 0; slot < 5; ++slot)
    {
        EnchantmentSlot enchantmentSlot = EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + slot);
        if (!item->GetEnchantmentId(enchantmentSlot))
            continue;

        uint32 groupId = GetOfficialEnchantGroup(config, slot);
        auto groupItr = EnchGroupStore.find(groupId);
        if (!groupId || groupItr == EnchGroupStore.end() || groupItr->second.empty())
        {
            error = "官方附魔槽位没有配置可用的随机附魔组";
            return OfficialEnchantRefinementState::Invalid;
        }
        hasRefinableEnchant = true;
    }

    if (!hasRefinableEnchant)
    {
        error = "该装备当前没有可洗炼的官方附魔";
        return OfficialEnchantRefinementState::Invalid;
    }

    return OfficialEnchantRefinementState::Ready;
}

bool RerollOfficialEnchants(Player* player, Item* item, uint32 requirementId, std::string& error)
{
    if (ValidateOfficialEnchantRefinement(item, error) != OfficialEnchantRefinementState::Ready)
        return false;

    if (item->ItemRtsCount == std::numeric_limits<uint32>::max())
    {
        error = "洗炼次数已达到上限，未扣费";
        return false;
    }
    uint32 const guid = item->GetGUIDLow();
    auto readEnchants = [guid]()
    {
        auto* statement = CharacterDatabase.GetPreparedStatement(CHAR_REFINE_ITEM_ENCHANT_SELECT);
        statement->SetData(0, guid);
        return CharacterDatabase.Query(statement);
    };
    auto* count = CharacterDatabase.GetPreparedStatement(CHAR_REFINE_RTS_COUNT);
    count->SetData(0, guid);
    PreparedQueryResult oldCount = CharacterDatabase.Query(count);
    PreparedQueryResult oldItem = readEnchants();
    if (!oldItem || !oldCount)
    {
        error = "无法读取原附魔存档，未扣费";
        return false;
    }
    std::string const oldStoredEnchants = oldItem->Fetch()[0].Get<std::string>();
    bool const hadCount = oldCount->Fetch()[0].Get<uint64>() != 0;
    uint32 oldStoredEntry = 0;
    uint32 oldStoredCount = 0;
    if (hadCount)
    {
        auto* statement = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_ENCH_RTS);
        statement->SetData(0, guid);
        PreparedQueryResult stored = CharacterDatabase.Query(statement);
        if (!stored)
        {
            error = "无法读取原洗炼次数，未扣费";
            return false;
        }
        oldStoredEntry = stored->Fetch()[1].Get<uint32>();
        oldStoredCount = stored->Fetch()[2].Get<uint32>();
    }
    std::array<std::array<uint32, 3>, 5> oldEnchants{};
    for (uint8 index = 0; index < oldEnchants.size(); ++index)
    {
        EnchantmentSlot slot = EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + index);
        oldEnchants[index] = {item->GetEnchantmentId(slot), item->GetEnchantmentDuration(slot),
            item->GetEnchantmentCharges(slot)};
    }
    auto restoreMemory = [&]
    {
        for (uint8 index = 0; index < oldEnchants.size(); ++index)
            item->SetEnchantment(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + index),
                oldEnchants[index][0], oldEnchants[index][1], oldEnchants[index][2]);
        EnchantAtStore[guid] = {oldEnchants[0][0], oldEnchants[1][0], oldEnchants[2][0],
            oldEnchants[3][0], oldEnchants[4][0]};
        item->SetState(ITEM_CHANGED, player);
    };
    auto restore = [&]
    {
        auto transaction = CharacterDatabase.BeginTransaction();
        auto* state = CharacterDatabase.GetPreparedStatement(CHAR_REFINE_ITEM_ENCHANT_UPDATE);
        state->SetArguments(oldStoredEnchants, guid);
        transaction->Append(state);
        if (hadCount)
        {
            auto* statement = CharacterDatabase.GetPreparedStatement(CHAR_REP_ITEM_ENCH_RTS);
            statement->SetArguments(guid, oldStoredEntry, oldStoredCount);
            transaction->Append(statement);
        }
        else
        {
            auto* statement = CharacterDatabase.GetPreparedStatement(CHAR_REFINE_RTS_DELETE);
            statement->SetData(0, guid);
            transaction->Append(statement);
        }
        CharacterDatabase.DirectCommitTransaction(transaction);
        restoreMemory();
        PreparedQueryResult restored = readEnchants();
        auto* statement = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_ENCH_RTS);
        statement->SetData(0, guid);
        PreparedQueryResult restoredCount = CharacterDatabase.Query(statement);
        auto* countStatement = CharacterDatabase.GetPreparedStatement(CHAR_REFINE_RTS_COUNT);
        countStatement->SetData(0, guid);
        PreparedQueryResult restoredRowCount = CharacterDatabase.Query(countStatement);
        bool const countMatches = hadCount ? restoredCount &&
            restoredCount->Fetch()[1].Get<uint32>() == oldStoredEntry &&
            restoredCount->Fetch()[2].Get<uint32>() == oldStoredCount :
            restoredRowCount && restoredRowCount->Fetch()[0].Get<uint64>() == 0;
        if (!restored || restored->Fetch()[0].Get<std::string>() != oldStoredEnchants || !countMatches)
        {
            error = "洗炼失败且存档恢复未确认，未扣费，请联系管理员";
            LOG_ERROR("module.itemrefinement", "物品 {} 官方附魔洗炼恢复未确认", guid);
        }
        return false;
    };

    // 先生成并验证，不切换身上的附魔效果，也不增加洗炼次数。
    for (uint8 index = 0; index < oldEnchants.size(); ++index)
    {
        if (!oldEnchants[index][0])
            continue;
        EnchantmentSlot slot = EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + index);
        item->ClearEnchantment(slot);
        sItemFuncMgr->DoUseCustomEnchOneNoPacket(item, slot);
        if (!item->GetEnchantmentId(slot) ||
            !sSpellItemEnchantmentStore.LookupEntry(item->GetEnchantmentId(slot)))
        {
            restoreMemory();
            error = "随机附魔组未生成有效附魔，未扣费";
            return false;
        }
    }
    std::ostringstream serialized;
    for (uint8 slot = 0; slot < MAX_ENCHANTMENT_SLOT; ++slot)
        serialized << item->GetEnchantmentId(EnchantmentSlot(slot)) << ' '
            << item->GetEnchantmentDuration(EnchantmentSlot(slot)) << ' '
            << item->GetEnchantmentCharges(EnchantmentSlot(slot)) << ' ';
    uint32 const nextCount = item->ItemRtsCount + 1;
    auto transaction = CharacterDatabase.BeginTransaction();
    auto* state = CharacterDatabase.GetPreparedStatement(CHAR_REFINE_ITEM_ENCHANT_UPDATE);
    state->SetArguments(serialized.str(), guid);
    transaction->Append(state);
    auto* counter = CharacterDatabase.GetPreparedStatement(CHAR_REP_ITEM_ENCH_RTS);
    counter->SetArguments(guid, item->GetEntry(), nextCount);
    transaction->Append(counter);
    CharacterDatabase.DirectCommitTransaction(transaction);
    PreparedQueryResult saved = readEnchants();
    auto* verify = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_ENCH_RTS);
    verify->SetData(0, guid);
    PreparedQueryResult savedCount = CharacterDatabase.Query(verify);
    if (!saved || saved->Fetch()[0].Get<std::string>() != serialized.str() ||
        !savedCount || savedCount->Fetch()[2].Get<uint32>() != nextCount)
    {
        error = "附魔结果保存失败，未扣费";
        return restore();
    }
    if (!sItemIdentificationSystem->ConsumeOperationRequirements(player, item, requirementId))
    {
        error = "洗炼需求无效、合计不足或包含目标装备，未扣费";
        return restore();
    }

    if (item->IsEquipped())
        for (uint8 index = 0; index < oldEnchants.size(); ++index)
        {
            EnchantmentSlot slot = EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0 + index);
            uint32 enchant = item->GetEnchantmentId(slot);
            uint32 duration = item->GetEnchantmentDuration(slot);
            uint32 charges = item->GetEnchantmentCharges(slot);
            item->SetEnchantment(slot, oldEnchants[index][0], oldEnchants[index][1], oldEnchants[index][2]);
            player->ApplyEnchantment(item, slot, false);
            item->SetEnchantment(slot, enchant, duration, charges);
            player->ApplyEnchantment(item, slot, true);
        }
    item->ItemRtsCount = nextCount;
    auto const& config = ItemEnchStore.at(item->GetEntry());
    ItemEnchAtCountStore[guid] = std::to_string(nextCount) + "#" + std::to_string(config.Rts);
    sItemFuncMgr->SendUpdateEnchAtCount(player, guid, nextCount, config.Rts);
    item->SetState(ITEM_CHANGED, player);
    player->UpdateAllStats();
    error.clear();
    return true;
}

void LoadConfigs()
{
    Configs.clear();
    GroupConfigs.clear();
    AmbiguousGroupConfigs.clear();
    QueryResult result = WorldDatabase.Query("SELECT `物品id`, `_物品_鉴定_组`, `洗炼需求`, `洗炼类目限制` FROM `_模板_物品洗炼`");
    if (!result)
    {
        LOG_WARN("module.itemrefinement", ">> _模板_物品洗炼 表为空或不存在");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        RefinementConfig config;
        config.itemId = fields[0].Get<uint32>();
        config.identificationGroup = fields[1].Get<uint32>();
        config.requirementId = fields[2].Get<uint32>();
        std::string categoryText = fields[3].Get<std::string>();
        std::replace(categoryText.begin(), categoryText.end(), '#', ',');
        for (std::string const& value : Split(categoryText, ','))
        {
            uint32 category = ToUInt(value);
            if (category >= 1 && category <= 4)
                config.categories.insert(static_cast<uint8>(category));
            else if (category != 0)
                LOG_ERROR("module.itemrefinement", ModuleOldFormat("物品 %s 配置了无效洗炼类目 %s", config.itemId, category).c_str());
        }

        if (!config.identificationGroup)
        {
            bool removedIdentificationCategory = false;
            for (uint8 category = 1; category <= 3; ++category)
                removedIdentificationCategory = config.categories.erase(category) > 0 || removedIdentificationCategory;

            if (removedIdentificationCategory)
                LOG_WARN("module.itemrefinement", ModuleOldFormat(
                    "物品 %s 未配置鉴定组，已忽略基础属性、追加属性和追加技能洗炼，仅可使用官方附魔洗炼",
                    config.itemId).c_str());
        }

        if (!config.itemId || config.categories.empty())
        {
            LOG_ERROR("module.itemrefinement", ModuleOldFormat("跳过无效洗炼配置：物品=%s 鉴定组=%s 类目数=%s", config.itemId, config.identificationGroup, config.categories.size()).c_str());
            continue;
        }
        Configs[config.itemId] = config;

        if (!config.identificationGroup)
            continue;

        if (AmbiguousGroupConfigs.find(config.identificationGroup) != AmbiguousGroupConfigs.end())
            continue;

        auto groupItr = GroupConfigs.find(config.identificationGroup);
        if (groupItr == GroupConfigs.end())
        {
            GroupConfigs.emplace(config.identificationGroup, config);
        }
        else if (!SameRefinementRules(groupItr->second, config))
        {
            AmbiguousGroupConfigs.insert(config.identificationGroup);
            GroupConfigs.erase(groupItr);
            LOG_WARN("module.itemrefinement", ModuleOldFormat(
                "鉴定组 %s 存在不同的洗炼需求或类目，仅保留物品精确配置，不启用同组兜底",
                config.identificationGroup).c_str());
        }
    } while (result->NextRow());

    LOG_INFO("server.loading", ModuleOldFormat("→_模板_物品洗炼 加载完成，共 %s 条", Configs.size()).c_str());
}

Item* FindItem(Player* player, uint32 itemId, uint32 itemGuid, int32 bag, int32 slot, bool equipped)
{
    if (!player || !itemId)
        return nullptr;

    auto matches = [itemId](Item* item) { return item && item->GetEntry() == itemId; };

    if (equipped)
    {
        int32 serverSlot = slot > 0 ? slot - 1 : slot;
        if (serverSlot >= EQUIPMENT_SLOT_START && serverSlot < EQUIPMENT_SLOT_END)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, static_cast<uint8>(serverSlot)); matches(item))
                return item;
    }

    if (bag >= 0 && slot > 0)
    {
        if (bag == 0)
        {
            uint8 clientSlot = static_cast<uint8>(slot - 1);
            if (clientSlot < (INVENTORY_SLOT_ITEM_END - INVENTORY_SLOT_ITEM_START))
                if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, INVENTORY_SLOT_ITEM_START + clientSlot); matches(item))
                    return item;

            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, static_cast<uint8>(slot - 1)); matches(item))
                return item;
        }
        else if (bag >= 1 && bag <= 4)
        {
            uint8 bagPos = INVENTORY_SLOT_BAG_START + static_cast<uint8>(bag - 1);
            if (Bag* bagObject = player->GetBagByPos(bagPos))
            {
                uint32 innerSlot = static_cast<uint32>(slot - 1);
                if (innerSlot < bagObject->GetBagSize())
                    if (Item* item = bagObject->GetItemByPos(innerSlot); matches(item))
                        return item;
            }
        }
    }

    // The item may have moved after the UI query. Fall back to the GUID only
    // when it still belongs to this player and has the expected entry.
    if (itemGuid)
        if (Item* item = player->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(itemGuid)); matches(item))
            return item;

    return nullptr;
}

void SendConfig(Player* player, Item* item, char const* status = "")
{
    bool usedGroupFallback = false;
    RefinementConfig const* config = ResolveConfig(item, &usedGroupFallback);
    if (!config)
    {
        Send(player, "DATA|0|0||该物品没有洗炼配置");
        return;
    }

    std::string categories;
    for (uint8 category : config->categories)
    {
        if (!categories.empty()) categories += ",";
        categories += std::to_string(category);
    }

    std::string materials;
    auto needItr = NeedTempStore.find(config->requirementId);
    if (needItr != NeedTempStore.end())
    {
        for (size_t index = 0; index < MNC; ++index)
        {
            uint32 materialId = needItr->second.ItemId[index];
            uint32 materialCount = needItr->second.Count[index];
            if (!materialId || !materialCount)
                continue;
            if (!materials.empty())
                materials += "~";
            materials += std::to_string(materialId) + "^" + std::to_string(materialCount) + "^"
                + (player->HasItemCount(materialId, materialCount, false) ? "1" : "0");
        }
    }
    std::string resolvedStatus = status;
    if (usedGroupFallback)
        resolvedStatus = resolvedStatus.empty() ? "已按同鉴定组开放洗炼" : resolvedStatus + "（按同鉴定组）";
    Send(player, "DATA|1|" + std::to_string(config->requirementId) + "|" + categories + "|" + resolvedStatus + "|" + materials + "|" + BuildEnchantValue(item));
}

class RefinementWorldScript : public WorldScript
{
public:
    RefinementWorldScript() : WorldScript("RefinementWorldScript") { }
    void OnStartup() override { LoadConfigs(); }
};

class RefinementCommandScript : public CommandScript
{
public:
    RefinementCommandScript() : CommandScript("RefinementCommandScript") { }

    std::vector<Acore::ChatCommands::ChatCommandBuilder> GetCommands() const override
    {
        using namespace Acore::ChatCommands;

        static ChatCommandTable reloadSubCommands =
        {
            { "item_refinement", HandleReloadCommand, SEC_ADMINISTRATOR, Console::Yes },
        };
        static ChatCommandTable commandTable =
        {
            { "reload", reloadSubCommands },
        };
        return commandTable;
    }

private:
    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        LoadConfigs();
        sItemIdentificationSystem->LoadConfig(true);
        handler->PSendSysMessage("物品洗炼与物品鉴定模板已在线重载，洗炼配置数量：%u", static_cast<uint32>(Configs.size()));
        return true;
    }
};

class RefinementPlayerScript : public PlayerScript
{
public:
    RefinementPlayerScript() : PlayerScript("RefinementPlayerScript") { }

    void OnChat(Player* player, uint32 type, uint32 language, std::string& message, Player*) override
    {
        if (!player || type != CHAT_MSG_WHISPER || language != LANG_ADDON)
            return;
        size_t tab = message.find('\t');
        if (tab == std::string::npos || message.substr(0, tab) != REFINEMENT_PREFIX)
            return;
        std::vector<std::string> parts = Split(message.substr(tab + 1), '|');
        if (parts.size() < 5)
            return;

        uint32 itemId = ToUInt(parts[1]);
        int32 bag = static_cast<int32>(ToUInt(parts[2]));
        int32 slot = static_cast<int32>(ToUInt(parts[3]));
        bool equipped = ToUInt(parts[4]) != 0;
        uint32 itemGuid = parts.size() > 5 ? ToUInt(parts[5]) : 0;
        Item* item = FindItem(player, itemId, itemGuid, bag, slot, equipped);
        if (!item)
        {
            Send(player, "RESULT|0|没有找到该装备");
            return;
        }

        if (parts[0] == "QUERY")
        {
            SendConfig(player, item, "查询完成");
            return;
        }
        if (parts[0] != "REFINE" || parts.size() < 7)
            return;

        uint8 category = static_cast<uint8>(ToUInt(parts[6]));
        RefinementConfig const* resolvedConfig = ResolveConfig(item);
        if (!resolvedConfig || resolvedConfig->categories.find(category) == resolvedConfig->categories.end())
        {
            Send(player, "RESULT|0|该洗炼类目未开放");
            return;
        }

        RefinementConfig const& config = *resolvedConfig;
        OfficialEnchantRefinementState officialEnchantState = OfficialEnchantRefinementState::NotConfigured;
        std::string officialEnchantError;
        if (category == 4)
        {
            officialEnchantState = ValidateOfficialEnchantRefinement(item, officialEnchantError);
            if (officialEnchantState == OfficialEnchantRefinementState::Invalid)
            {
                Send(player, "RESULT|0|" + officialEnchantError);
                return;
            }

            if (officialEnchantState == OfficialEnchantRefinementState::NotConfigured && !config.identificationGroup)
            {
                Send(player, "RESULT|0|该物品未配置官方随机附魔，且洗炼配置没有填写鉴定组");
                return;
            }
        }

        // 模板洗炼必须保留原鉴定档次，配置失效时在扣除材料前拒绝。
        if (category != 4 || officialEnchantState != OfficialEnchantRefinementState::Ready)
        {
            uint32 originalGroup = sItemIdentificationSystem->GetIdentificationGroupForItem(item->GetGUIDLow());
            if (!originalGroup || originalGroup != config.identificationGroup)
            {
                Send(player, "RESULT|0|原鉴定模板不存在或洗炼鉴定组不匹配，请联系管理员");
                return;
            }
        }

        if (config.requirementId && !sHelper->NotDeleteCheckNeed(player, config.requirementId))
        {
            Send(player, "RESULT|0|不满足洗炼需求");
            return;
        }

        ItemIdentificationSystem::AllModuleData beforeData =
            sItemIdentificationSystem->QueryAllModuleData(itemId, item->GetGUID().GetCounter());
        std::string beforeValue;
        if (category == 1)
            beforeValue = beforeData.baseAttributes;
        else if (category == 2)
            beforeValue = beforeData.additionalAttributes;
        else if (category == 3)
            beforeValue = beforeData.skillsData;
        else if (category == 4)
            beforeValue = BuildEnchantValue(item);

        std::string error;
        bool refined = false;
        if (category == 4 && officialEnchantState == OfficialEnchantRefinementState::Ready)
            refined = RerollOfficialEnchants(player, item, config.requirementId, error);
        else
            refined = sItemIdentificationSystem->RefineItemCategory(
                player, item, config.identificationGroup, category, error, config.requirementId);

        if (!refined)
        {
            Send(player, "RESULT|0|" + (error.empty() ? std::string("洗炼失败") : error));
            return;
        }
        ItemIdentificationSystem::AllModuleData afterData = sItemIdentificationSystem->QueryAllModuleData(itemId, item->GetGUID().GetCounter());
        std::string afterValue;
        if (category == 1)
        {
            afterValue = afterData.baseAttributes;
        }
        else if (category == 2)
        {
            afterValue = afterData.additionalAttributes;
        }
        else if (category == 3)
        {
            afterValue = afterData.skillsData;
        }
        else if (category == 4)
        {
            afterValue = BuildEnchantValue(item);
        }
        std::replace(beforeValue.begin(), beforeValue.end(), '|', '/');
        std::replace(afterValue.begin(), afterValue.end(), '|', '/');
        Send(player, "RESULT|1|洗炼成功|" + std::to_string(category) + "|" + beforeValue + "|" + afterValue);
        SendConfig(player, item, "洗炼完成");
    }
};
}

void AddSC_ItemRefinementScripts()
{
    new RefinementWorldScript();
    new RefinementPlayerScript();
    new RefinementCommandScript();
}
