#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "Item.h"
#include "Log.h"
#include "LootMgr.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "../../../src/server/scripts/Custom/AuraSpellMgr/AuraSpellMgr.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <vector>

namespace
{
enum class CustomLootType : uint8
{
    Creature = 1,
    Item = 2,
    GameObject = 3
};

struct CustomLootEntry
{
    uint32 id = 0;
    uint32 itemId = 0;
    float chance = 0.0f;
    uint32 minCount = 1;
    uint32 maxCount = 1;
    bool allowMultiplier = false;
};

using CustomLootStore = std::unordered_map<uint64, std::vector<CustomLootEntry>>;

uint64 MakeKey(CustomLootType type, uint32 entry)
{
    return (uint64(uint8(type)) << 32) | entry;
}

bool ParseType(std::string const& value, CustomLootType& type)
{
    if (value == "生物")
        type = CustomLootType::Creature;
    else if (value == "物品")
        type = CustomLootType::Item;
    else if (value == "物体")
        type = CustomLootType::GameObject;
    else
        return false;

    return true;
}

class CustomLootMgr
{
public:
    static CustomLootMgr* Instance()
    {
        static CustomLootMgr instance;
        return &instance;
    }

    void Load()
    {
        _store.clear();
        _enabled = sConfigMgr->GetOption<bool>("CustomLoot.Enable", true);
        _debug = sConfigMgr->GetOption<bool>("CustomLoot.Debug", false);
        _maxRollsPerEntry = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("CustomLoot.MaxRollsPerEntry", 20));
        _maxDeliveryStacks = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("CustomLoot.MaxDeliveryStacks", 120));
        _maxCountPerRoll = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("CustomLoot.MaxCountPerRoll", 1000000));

        if (!_enabled)
        {
            LOG_INFO("module.custom_loot", "自定义掉落模块已关闭");
            return;
        }

        QueryResult result = WorldDatabase.Query(
            "SELECT `序号`, `类型`, `来源编号`, `物品编号`, `几率`, "
            "`最小数量`, `最大数量`, `允许多倍` FROM `_生物自定义掉落表` ORDER BY `序号`");

        if (!result)
        {
            LOG_WARN("module.custom_loot", "_生物自定义掉落表不存在或没有配置数据");
            return;
        }

        uint32 loaded = 0;
        do
        {
            Field* fields = result->Fetch();
            CustomLootType type;
            std::string typeText = fields[1].Get<std::string>();
            if (!ParseType(typeText, type))
            {
                LOG_ERROR("module.custom_loot", "自定义掉落序号 {} 的类型 '{}' 无效", fields[0].Get<uint32>(), typeText);
                continue;
            }

            uint32 sourceEntry = fields[2].Get<uint32>();
            CustomLootEntry entry;
            entry.id = fields[0].Get<uint32>();
            entry.itemId = fields[3].Get<uint32>();
            entry.chance = fields[4].Get<float>();
            entry.minCount = fields[5].Get<uint32>();
            entry.maxCount = fields[6].Get<uint32>();
            entry.allowMultiplier = fields[7].Get<std::string>() == "开启";

            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(entry.itemId);

            if (!sourceEntry || !itemTemplate || entry.chance <= 0.0f ||
                entry.chance > 100.0f || !entry.minCount || entry.maxCount < entry.minCount)
            {
                LOG_ERROR("module.custom_loot", "自定义掉落序号 {} 配置无效，已跳过", entry.id);
                continue;
            }

            uint64 safeByStacks = uint64(std::max<uint32>(itemTemplate->GetMaxStackSize(), 1)) * _maxDeliveryStacks;
            uint32 safeMaxCount = static_cast<uint32>(std::min<uint64>(safeByStacks, _maxCountPerRoll));
            if (entry.minCount > safeMaxCount)
            {
                LOG_ERROR("module.custom_loot", "自定义掉落序号 {} 最小数量 {} 超过安全上限 {}，已跳过",
                    entry.id, entry.minCount, safeMaxCount);
                continue;
            }

            if (entry.maxCount > safeMaxCount)
            {
                LOG_WARN("module.custom_loot", "自定义掉落序号 {} 最大数量 {} 超过安全上限 {}，已限制",
                    entry.id, entry.maxCount, safeMaxCount);
                entry.maxCount = safeMaxCount;
            }

            _store[MakeKey(type, sourceEntry)].push_back(entry);
            ++loaded;
        } while (result->NextRow());

        LOG_INFO("server.loading", "_生物自定义掉落表加载完成，共 {} 条", loaded);
    }

    void Generate(CustomLootType type, uint32 sourceEntry, Player* player, Loot* loot, bool groupRules)
    {
        if (!_enabled || !player || !loot)
            return;

        auto itr = _store.find(MakeKey(type, sourceEntry));
        if (itr == _store.end())
            return;

        // Sources without a native loot template still need an owner so the
        // core can count and expose the custom items correctly.
        loot->lootOwnerGUID = player->GetGUID();
        std::unordered_map<uint32, uint32> remainingCountByItem;

        for (CustomLootEntry const& entry : itr->second)
        {
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(entry.itemId);
            if (!itemTemplate)
                continue;

            auto remainingItr = remainingCountByItem.find(entry.itemId);
            if (remainingItr == remainingCountByItem.end())
            {
                uint64 safeByStacks = uint64(std::max<uint32>(itemTemplate->GetMaxStackSize(), 1)) * _maxDeliveryStacks;
                uint32 safeCount = static_cast<uint32>(std::min<uint64>(safeByStacks, _maxCountPerRoll));
                remainingItr = remainingCountByItem.emplace(entry.itemId, safeCount).first;
            }

            uint32& remainingCount = remainingItr->second;
            uint32 rolls = 1;
            if (entry.allowMultiplier)
            {
                uint64 requestedRolls = uint64(rolls) + sAuraSpellMgr->GetLootCountMod(player);
                rolls = static_cast<uint32>(std::min<uint64>(requestedRolls, _maxRollsPerEntry));
            }

            for (uint32 roll = 0; roll < rolls; ++roll)
            {
                if (remainingCount < entry.minCount)
                    break;

                if (!roll_chance_f(entry.chance))
                    continue;

                uint32 count = urand(entry.minCount, std::min(entry.maxCount, remainingCount));
                remainingCount -= count;
                AddResult(player, loot, entry.itemId, count, groupRules);

                if (_debug)
                    LOG_INFO("module.custom_loot", "玩家 {} 触发自定义掉落序号 {}，物品 {} x{}", player->GetName(), entry.id, entry.itemId, count);
            }
        }
    }

private:
    void AddResult(Player* player, Loot* loot, uint32 itemId, uint32 count, bool groupRules)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
            return;

        // Currency tokens are auto-stored by the native loot path before this
        // module hook runs, so custom currency must be delivered directly.
        if (itemTemplate->IsCurrencyToken())
        {
            DeliverOverflow(player, itemId, count);
            return;
        }

        // Repeated rolls of a stackable item can share one visible slot.
        if (itemTemplate->GetMaxStackSize() > 1)
        {
            for (LootItem& existing : loot->items)
            {
                if (existing.itemid != itemId || existing.is_looted || existing.is_blocked ||
                    existing.freeforall || !existing.conditions.empty())
                    continue;

                uint64 combined = uint64(existing.GetCount()) + count;
                existing.CustomCount = uint32(std::min<uint64>(combined, std::numeric_limits<uint32>::max()));
                return;
            }
        }

        if (loot->items.size() < MAX_NR_LOOT_ITEMS)
        {
            size_t oldSize = loot->items.size();
            LootStoreItem lootItem(itemId, 0, 100.0f, false, LOOT_MODE_DEFAULT, 0, 1, 1);
            loot->AddItem(lootItem, player);

            if (loot->items.size() > oldSize)
            {
                // The 3.3.5a packet already carries a uint32 count. Keeping
                // the whole reward in one loot entry produces displays such
                // as 1824 while inventory storage still splits real stacks.
                LootItem& generated = loot->items.back();
                generated.CustomCount = count;

                // FillFFALoot has already run. Treat custom multi-drop items
                // as owner loot and account for them here.
                if (generated.freeforall && generated.AllowedForPlayer(player, loot->sourceWorldObjectGUID))
                {
                    generated.freeforall = false;
                    ++loot->unlootedCount;
                }
                return;
            }
        }

        if (groupRules)
        {
            LOG_ERROR("module.custom_loot", "组队掉落已满 {} 个槽位，物品 {} x{} 未生成，避免绕过队伍分配规则", MAX_NR_LOOT_ITEMS, itemId, count);
            ChatHandler(player->GetSession()).SendSysMessage("自定义掉落种类超过组队拾取窗口上限，请联系管理员调整掉落配置。");
            return;
        }

        DeliverOverflow(player, itemId, count);
    }

    void DeliverOverflow(Player* player, uint32 itemId, uint32 count)
    {
        uint32 noSpaceForCount = 0;
        ItemPosCountVec dest;
        InventoryResult result = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, count, &noSpaceForCount);
        uint32 bagCount = result == EQUIP_ERR_OK ? count : count - noSpaceForCount;

        if (dest.empty())
        {
            bagCount = 0;
            noSpaceForCount = count;
        }

        if (bagCount && !dest.empty())
        {
            if (Item* item = player->StoreNewItem(dest, itemId, true, "CustomLootOverflow"))
                player->SendNewItem(item, bagCount, true, false);
            else
                noSpaceForCount += bagCount;
        }

        if (noSpaceForCount)
            SendByMail(player, itemId, noSpaceForCount);
    }

    void SendByMail(Player* player, uint32 itemId, uint32 count)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        uint32 stackSize = itemTemplate ? std::max<uint32>(itemTemplate->GetMaxStackSize(), 1) : 1;

        while (count)
        {
            CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
            MailDraft draft("自定义掉落补发", "由于拾取窗口或背包空间不足，物品通过邮件补发。");
            bool hasAttachment = false;

            for (uint8 attachment = 0; attachment < MAX_MAIL_ITEMS && count; ++attachment)
            {
                uint32 stackCount = std::min(count, stackSize);
                Item* item = Item::CreateItem(itemId, stackCount, player);
                if (!item)
                    break;

                item->SaveToDB(transaction);
                draft.AddItem(item);
                count -= stackCount;
                hasAttachment = true;
            }

            if (!hasAttachment)
            {
                LOG_ERROR("module.custom_loot", "自定义掉落邮件创建物品 {} 失败，剩余数量 {}", itemId, count);
                break;
            }

            draft.SendMailTo(transaction, MailReceiver(player), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));
            CharacterDatabase.CommitTransaction(transaction);
        }

        ChatHandler(player->GetSession()).SendSysMessage("自定义掉落超出拾取窗口或背包容量，剩余物品已通过邮件补发。");
    }

    CustomLootStore _store;
    bool _enabled = true;
    bool _debug = false;
    uint32 _maxRollsPerEntry = 20;
    uint32 _maxDeliveryStacks = 120;
    uint32 _maxCountPerRoll = 1000000;
};

#define sCustomLootMgr CustomLootMgr::Instance()

class CustomLootGlobalScript : public GlobalScript
{
public:
    CustomLootGlobalScript() : GlobalScript("CustomLootGlobalScript") { }

    void OnAfterCreatureLootGenerated(Player* player, Creature* creature, Loot* loot) override
    {
        if (creature)
            sCustomLootMgr->Generate(CustomLootType::Creature, creature->GetEntry(), player, loot, player && player->GetGroup());
    }
};

class CustomLootPlayerScript : public PlayerScript
{
public:
    CustomLootPlayerScript() : PlayerScript("CustomLootPlayerScript") { }

    void OnAfterGameObjectLootGenerated(Player* player, GameObject* gameObject, Loot* loot) override
    {
        if (gameObject)
        {
            bool groupRules = player && player->GetGroup() && gameObject->GetGOInfo()->type == GAMEOBJECT_TYPE_CHEST &&
                gameObject->GetGOInfo()->chest.groupLootRules;
            sCustomLootMgr->Generate(CustomLootType::GameObject, gameObject->GetEntry(), player, loot, groupRules);
        }
    }

    void OnAfterItemLootGenerated(Player* player, Item* item, Loot* loot) override
    {
        if (item)
            sCustomLootMgr->Generate(CustomLootType::Item, item->GetEntry(), player, loot, false);
    }
};

class CustomLootWorldScript : public WorldScript
{
public:
    CustomLootWorldScript() : WorldScript("CustomLootWorldScript") { }

    void OnStartup() override
    {
        sCustomLootMgr->Load();
    }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload)
            sCustomLootMgr->Load();
    }
};

class CustomLootCommandScript : public CommandScript
{
public:
    CustomLootCommandScript() : CommandScript("CustomLootCommandScript") { }

    Acore::ChatCommands::ChatCommandTable GetCommands() const override
    {
        using namespace Acore::ChatCommands;

        static ChatCommandTable reloadSubcommand =
        {
            { "customloot", HandleReload, SEC_ADMINISTRATOR, Console::Yes }
        };
        static ChatCommandTable customLootSubcommand =
        {
            { "重载", HandleReload, SEC_ADMINISTRATOR, Console::Yes }
        };
        static ChatCommandTable commands =
        {
            { "reload", reloadSubcommand },
            { "自定义掉落", customLootSubcommand }
        };
        return commands;
    }

    static bool HandleReload(ChatHandler* handler, char const*)
    {
        sCustomLootMgr->Load();
        handler->SendSysMessage("_生物自定义掉落表已重新加载。");
        return true;
    }
};
}

void Addmod_custom_lootScripts()
{
    new CustomLootGlobalScript();
    new CustomLootPlayerScript();
    new CustomLootWorldScript();
    new CustomLootCommandScript();
}
