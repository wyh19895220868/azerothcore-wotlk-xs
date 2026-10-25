
#include "Transmogrification.h"
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
#include "Bag.h"
#include "Pet.h"
#include "ReputationMgr.h"
#include <CharacterCache.h>
#include <AI/ScriptedAI/ScriptedGossip.h>
#pragma execution_character_set("utf-8")

#define sT  sTransmogrification

class NpcTransmogrifier : public CreatureScript
{
public:
    NpcTransmogrifier() : CreatureScript("NpcTransmogrifier") { }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        WorldSession* session = player->GetSession();
        if (sT->GetEnableTransmogInfo())
        {
            std::ostringstream Oss;
            Oss << "装备幻化系统将更改玩家装备的外观\n";
            Oss << "而不会影响玩家正常的装备属性\n";
            Oss << "玩家幻化将收取一定的费用\n";
            Oss << "此费用不会退还给玩家\n";
            Oss << "玩家可以点击 更新菜单\n";
            Oss << "来查看最新的可幻化的装备\n";
            Oss << "虽然幻化功能比较强大\n";
            Oss << "并不是所有的物品都可以幻化\n";
            Oss << "目前可以幻化的物品: \n";
            Oss << "目前只有装备和武器可以幻化\n";
            Oss << "枪械 弓箭 弩和鱼竿都不能被幻化\n";
            Oss << "你必须装备这两种物品才可以被幻化\n";
            Oss << "幻化后的装备只能存在于自己的背包里\n";
            Oss << "邮寄 存公会银行 交易 会让幻化效果失效\n";
            Oss << "你不想使用此幻化效果的话\n";
            Oss << "可以在幻化商人那里将幻化效果取消\n";
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/Icons/INV_Misc_Book_11:30:30:-18:0|t 如何使用管理幻化方案", EQUIPMENT_SLOT_END + 9, 0, Oss.str(), 0, false);
        }
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            if (const char* slotName = sT->GetSlotName(slot, session))
            {
                Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                uint32 entry = newItem ? sT->GetFakeEntry(newItem->GetGUIDHigh()) : 0;
                std::string icon = entry ? sT->GetItemIcon(entry, 30, 30, -18, 0) : sT->GetSlotIcon(slot, 30, 30, -18, 0);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, icon + std::string(slotName), EQUIPMENT_SLOT_END, slot);
            }
        }
#ifdef PRESETS
        if (sT->GetEnableSets())
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/RAIDFRAME/UI-RAIDFRAME-MAINASSIST:30:30:-18:0|t 管理幻化", EQUIPMENT_SLOT_END + 4, 0);
#endif
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/Icons/INV_Enchant_Disenchant:30:30:-18:0|t 清除所有幻化", EQUIPMENT_SLOT_END + 2, 0, "你确定要清除所有幻化效果么?", 0, false);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/PaperDollInfoFrame/UI-GearManager-Undo:30:30:-18:0|t 更新菜单", EQUIPMENT_SLOT_END + 1, 0);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action)
    {
        player->PlayerTalkClass->ClearMenus();
        WorldSession* session = player->GetSession();
        switch (sender)
        {
            case EQUIPMENT_SLOT_END:
                ShowTransmogItems(player, creature, action);
                break;
            case EQUIPMENT_SLOT_END + 1:
                OnGossipHello(player, creature);
                break;
            case EQUIPMENT_SLOT_END + 2:
            {
                bool removed = false;
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                {
                    if (Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    {
                        if (!sT->GetFakeEntry(newItem->GetGUIDHigh()))
                            continue;
                        sT->DeleteFakeEntry(player, slot, newItem, &trans);
                        removed = true;
                    }
                }
                if (removed)
                {
                    session->SendAreaTriggerMessage("幻化效果已成功去除!");
                    CharacterDatabase.CommitTransaction(trans);
                }
                else
                    session->SendNotification("没有可以幻化的物品!");
                OnGossipHello(player, creature);
            } break;
            case EQUIPMENT_SLOT_END + 3:
            {
                if (Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, action))
                {
                    if (sT->GetFakeEntry(newItem->GetGUIDHigh()))
                    {
                        sT->DeleteFakeEntry(player, action, newItem);
                        session->SendAreaTriggerMessage("幻化效果已成功去除!");
                    }
                    else
                        session->SendNotification("没有可以幻化的物品!");
                }
                OnGossipSelect(player, creature, EQUIPMENT_SLOT_END, action);
            } break;
    #ifdef PRESETS
            case EQUIPMENT_SLOT_END + 4: // Presets menu
            {
                if (!sT->GetEnableSets())
                {
                    OnGossipHello(player, creature);
                    return true;
                }
                if (sT->GetEnableSetInfo())
                {
                    std::ostringstream Oss;
                    Oss << "你可以保存自己的幻化套装\n";
                    Oss << "如果想要保存幻化设置\n";
                    Oss << "你必须要有已经幻化好的装备\n";
                    Oss << "然后你可以在 幻化管理\n";
                    Oss << "保存你的幻化设置\n";
                    Oss << "菜单会显示你的所有已经幻化装备\n";
                    Oss << "你可以看到保存设置\n";
                    Oss << "点击 保存设置 来保存你的幻化效果\n";
                    Oss << "你可以在菜单内选择 载入幻化设置\n";
                    Oss << "如果你的设置里有一个已经幻化的装备\n";
                    Oss << "如果使用新的设置 旧的幻化效果教会消失\n";
                    Oss << "注意 多次幻化时 幻化效果取最新\n";
                    Oss << "如果需要删除已经保存的设置\n";
                    Oss << "可以选择 删除幻化设置\n";
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/Icons/INV_Misc_Book_11:30:30:-18:0|t 如何使用管理幻化方案", EQUIPMENT_SLOT_END + 10, 0, Oss.str(), 0, false);
                }
                for (Transmogrification::presetIdMap::const_iterator it = sT->presetByName[player->GetGUIDHigh()].begin(); it != sT->presetByName[player->GetGUIDHigh()].end(); ++it)
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/Icons/INV_Misc_Statue_02:30:30:-18:0|t" + it->second, EQUIPMENT_SLOT_END + 6, it->first);

                if (sT->presetByName[player->GetGUIDHigh()].size() < sT->GetMaxSets())
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/GuildBankFrame/UI-GuildBankFrame-NewTab:30:30:-18:0|t 保存幻化方案", EQUIPMENT_SLOT_END + 8, 0);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/Icons/Ability_Spy:30:30:-18:0|t 返回主菜单", EQUIPMENT_SLOT_END + 1, 0);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            } break;
            case EQUIPMENT_SLOT_END + 5:
            {
                if (!sT->GetEnableSets())
                {
                    OnGossipHello(player, creature);
                    return true;
                }

                for (Transmogrification::slotMap::const_iterator it = sT->presetById[player->GetGUIDHigh()][action].begin(); it != sT->presetById[player->GetGUIDHigh()][action].end(); ++it)
                {
                    if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, it->first))
                        sT->PresetTransmog(player, item, it->second, it->first);
                }
                OnGossipSelect(player, creature, EQUIPMENT_SLOT_END + 6, action);
            } break;
            case EQUIPMENT_SLOT_END + 6: // view preset
            {
                if (!sT->GetEnableSets())
                {
                    OnGossipHello(player, creature);
                    return true;
                }
                // action = presetID
                for (Transmogrification::slotMap::const_iterator it = sT->presetById[player->GetGUIDHigh()][action].begin(); it != sT->presetById[player->GetGUIDHigh()][action].end(); ++it)
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, sT->GetItemIcon(it->second, 30, 30, -18, 0) + sT->GetItemLink(it->second, session), sender, action);

                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/Icons/INV_Misc_Statue_02:30:30:-18:0|t 开始进行幻化", EQUIPMENT_SLOT_END + 5, action, "幻化后幻化的装备源将变成绑定的.\n你确定要这么做么?\n\n" + sT->presetByName[player->GetGUIDHigh()][action], 0, false);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/PaperDollInfoFrame/UI-GearManager-LeaveItem-Opaque:30:30:-18:0|t 删除幻化效果", EQUIPMENT_SLOT_END + 7, action, "你确定要删除幻化 " + sT->presetByName[player->GetGUIDHigh()][action] + "?", 0, false);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/Icons/Ability_Spy:30:30:-18:0|t 返回主菜单", EQUIPMENT_SLOT_END + 4, 0);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            } break;
            case EQUIPMENT_SLOT_END + 7: // Delete preset
            {
                if (!sT->GetEnableSets())
                {
                    OnGossipHello(player, creature);
                    return true;
                }
                // action = presetID
                CharacterDatabase.Execute("DELETE FROM _幻化保存 WHERE Owner = {} AND PresetID = {}", player->GetGUIDLow(), action);
                sT->presetById[player->GetGUIDHigh()][action].clear();
                sT->presetById[player->GetGUIDHigh()].erase(action);
                sT->presetByName[player->GetGUIDHigh()].erase(action);

                OnGossipSelect(player, creature, EQUIPMENT_SLOT_END + 4, 0);
            } break;
            case EQUIPMENT_SLOT_END + 8: // Save preset
            {
                if (!sT->GetEnableSets() || sT->presetByName[player->GetGUIDHigh()].size() >= sT->GetMaxSets())
                {
                    OnGossipHello(player, creature);
                    return true;
                }
                uint32 cost = 0;
                bool canSave = false;
                for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                {
                    if (!sT->GetSlotName(slot, session))
                        continue;
                    if (Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    {
                        uint32 entry = sT->GetFakeEntry(newItem->GetGUIDHigh());
                        if (!entry)
                            continue;
                        const ItemTemplate* temp = sObjectMgr->GetItemTemplate(entry);
                        if (!temp)
                            continue;
                        if (!sT->SuitableForTransmogrification(player, temp)) // no need to check?
                            continue;
                        cost += sT->GetSpecialPrice(temp);
                        canSave = true;
                        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, sT->GetItemIcon(entry, 30, 30, -18, 0) + sT->GetItemLink(entry, session), EQUIPMENT_SLOT_END + 8, 0);
                    }
                }
                if (canSave)
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/GuildBankFrame/UI-GuildBankFrame-NewTab:30:30:-18:0|t 保存幻化方案", 0, 0, "建立方案名称", cost*sT->GetSetCostModifier() + sT->GetSetCopperCost(), true);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/PaperDollInfoFrame/UI-GearManager-Undo:30:30:-18:0|t 更新菜单", sender, action);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/Icons/Ability_Spy:30:30:-18:0|tBack..", EQUIPMENT_SLOT_END + 4, 0);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            } break;
            case EQUIPMENT_SLOT_END + 10: // Set info
            {
                OnGossipSelect(player, creature, EQUIPMENT_SLOT_END + 4, 0);
            } break;
    #endif
            case EQUIPMENT_SLOT_END + 9: // Transmog info
            {
                OnGossipSelect(player, creature, EQUIPMENT_SLOT_END + 1, 0);
            } break;
            default: // Transmogrify
            {
                if (!sender && !action)
                {
                    OnGossipHello(player, creature);
                    return true;
                }
                // sender = slot, action = display
                sT->Transmogrify(player, ObjectGuid(HighGuid::Item, action).GetRawValue(), sender);

                CloseGossipMenuFor(player);
            } break;
        }
        return true;
    }

#ifdef PRESETS
    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action, const char* code)
    {
        player->PlayerTalkClass->ClearMenus();
        if (sender || action)
            return true;
        if (!sT->GetEnableSets())
        {
            OnGossipHello(player, creature);
            return true;
        }
        std::string name(code);
        if (name.find('"') != std::string::npos || name.find('\\') != std::string::npos)
            player->GetSession()->SendNotification("输入了无效的名称!");
        else
        {
            for (uint8 presetID = 0; presetID < sT->GetMaxSets(); ++presetID)
            {
                if (sT->presetByName[player->GetGUIDHigh()].find(presetID) != sT->presetByName[player->GetGUIDHigh()].end())
                    continue;

                int32 cost = 0;
                std::map<uint8, uint32> items;
                for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                {
                    if (!sT->GetSlotName(slot, player->GetSession()))
                        continue;
                    if (Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    {
                        uint32 entry = sT->GetFakeEntry(newItem->GetGUIDHigh());
                        if (!entry)
                            continue;
                        const ItemTemplate* temp = sObjectMgr->GetItemTemplate(entry);
                        if (!temp)
                            continue;
                        if (!sT->SuitableForTransmogrification(player, temp))
                            continue;
                        cost += sT->GetSpecialPrice(temp);
                        items[slot] = entry;
                    }
                }
                if (items.empty())
                    break;
                cost *= sT->GetSetCostModifier();
                cost += sT->GetSetCopperCost();
                if (!player->HasEnoughMoney(cost))
                {
                    player->GetSession()->SendNotification("你没有足够的金币使用幻化功能!");
                    break;
                }

                std::ostringstream ss;
                for (std::map<uint8, uint32>::iterator it = items.begin(); it != items.end(); ++it)
                {
                    ss << uint32(it->first) << ' ' << it->second << ' ';
                    sT->presetById[player->GetGUIDHigh()][presetID][it->first] = it->second;
                }
                sT->presetByName[player->GetGUIDHigh()][presetID] = name;
                // name is raw client input from the gossip code box. The check above
                // only rejects '"' and '\\', so a single quote used to break out of
                // the '{}' literal below (SQL injection). Escape a copy for the DB and
                // keep the original for display.
                std::string escapedName = name;
                CharacterDatabase.EscapeString(escapedName);
                CharacterDatabase.Execute("REPLACE INTO _幻化保存 (Owner, PresetID, SetName, SetData) VALUES ({}, {}, '{}', '{}')", player->GetGUIDLow(), uint32(presetID), escapedName, ss.str());
                if (cost)
                    player->ModifyMoney(-cost);
                break;
            }
        }
        CloseGossipMenuFor(player);
        return true;
    }
#endif

    void ShowTransmogItems(Player* player, Creature* creature, uint8 slot)
    {
        WorldSession* session = player->GetSession();
        Item* oldItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (oldItem)
        {
            uint32 limit = 0;
            uint32 price = sT->GetSpecialPrice(oldItem->GetTemplate());
            price *= sT->GetScaledCostModifier();
            price += sT->GetCopperCost();
            std::ostringstream ss;
            ss << std::endl;
            if (sT->GetRequireToken())
                ss << std::endl << std::endl << sT->GetTokenAmount() << " x " << sT->GetItemLink(sT->GetTokenEntry(), session);

            for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
            {
                if (limit > MAX_OPTIONS)
                    break;
                Item* newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                if (!newItem)
                    continue;
                if (!sT->CanTransmogrifyItemWithItem(player, oldItem->GetTemplate(), newItem->GetTemplate()))
                    continue;
                if (sT->GetFakeEntry(oldItem->GetGUIDHigh()) == newItem->GetEntry())
                    continue;
                ++limit;
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, sT->GetItemIcon(newItem->GetEntry(), 30, 30, -18, 0) + sT->GetItemLink(newItem, session), slot, newItem->GetGUIDLow(), "幻化后幻化的装备源将变成绑定的.\n你确定要这么做么?\n\n" + sT->GetItemIcon(newItem->GetEntry(), 40, 40, -15, -10) + sT->GetItemLink(newItem, session) + ss.str(), price, false);
            }

            for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            {
                Bag* bag = player->GetBagByPos(i);
                if (!bag)
                    continue;
                for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                {
                    if (limit > MAX_OPTIONS)
                        break;
                    Item* newItem = player->GetItemByPos(i, j);
                    if (!newItem)
                        continue;
                    if (!sT->CanTransmogrifyItemWithItem(player, oldItem->GetTemplate(), newItem->GetTemplate()))
                        continue;
                    if (sT->GetFakeEntry(oldItem->GetGUIDHigh()) == newItem->GetEntry())
                        continue;
                    ++limit;
                    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, sT->GetItemIcon(newItem->GetEntry(), 30, 30, -18, 0) + sT->GetItemLink(newItem, session), slot, newItem->GetGUIDLow(), "幻化后幻化的装备源将变成绑定的.\n你确定要这么做么?\n\n" + sT->GetItemIcon(newItem->GetEntry(), 40, 40, -15, -10) + sT->GetItemLink(newItem, session) + ss.str(), price, false);
                }
            }
        }

        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/Icons/INV_Enchant_Disenchant:30:30:-18:0|t 删除幻化效果", EQUIPMENT_SLOT_END + 3, slot, " 你确定要清除此位置的幻化么?", 0, false);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/PaperDollInfoFrame/UI-GearManager-Undo:30:30:-18:0|t 更新菜单", EQUIPMENT_SLOT_END, slot);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/Icons/Ability_Spy:30:30:-18:0|t 返回主菜单", EQUIPMENT_SLOT_END + 1, 0);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }
};

class PS_Transmogrification : public PlayerScript
{
public:
    PS_Transmogrification() : PlayerScript("Player_Transmogrify") { }

    void OnAfterSetVisibleItemSlot(Player* player, uint8 slot, Item *item) override
    {
        if (!item)
            return;

        if (uint32 entry = sT->GetFakeEntry(item->GetGUIDHigh()))
            player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (slot * 2), entry);
    }

    void OnAfterMoveItemFromInventory(Player* player, Item* it, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        sT->DeleteFakeFromDB(it->GetGUIDLow());
    }

    void OnLogin(Player* player) override
    {
        uint64 playerGUID = player->GetGUIDHigh();
        sT->entryMap.erase(playerGUID);
        QueryResult result = CharacterDatabase.Query("SELECT GUID, FakeEntry FROM _幻化 WHERE Owner = {}", player->GetGUIDLow());
        if (result)
        {
            do
            {
                uint64 itemGUID = ObjectGuid(HighGuid::Item, (*result)[0].Get<uint32>()).GetRawValue();
                uint32 fakeEntry = (*result)[1].Get<uint32>();
                if (sObjectMgr->GetItemTemplate(fakeEntry))
                {
                    sT->dataMap[itemGUID] = playerGUID;
                    sT->entryMap[playerGUID][itemGUID] = fakeEntry;
                }
            } while (result->NextRow());

            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            {
                if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    player->SetVisibleItemSlot(slot, item);
            }
        }

#ifdef PRESETS
        if (sT->GetEnableSets())
            sT->LoadPlayerSets(playerGUID);
#endif
    }

    void OnLogout(Player* player) override
    {
        uint64 pGUID = player->GetGUIDHigh();
        for (Transmogrification::transmogData::const_iterator it = sT->entryMap[pGUID].begin(); it != sT->entryMap[pGUID].end(); ++it)
            sT->dataMap.erase(it->first);
        sT->entryMap.erase(pGUID);

#ifdef PRESETS
        if (sT->GetEnableSets())
            sT->UnloadPlayerSets(pGUID);
#endif
    }
};

class WS_Transmogrification : public WorldScript
{
public:
    WS_Transmogrification() : WorldScript("WS_Transmogrification") { }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload)
            sT->LoadConfig(reload);
    }

    void OnStartup() override
    {
        sT->LoadConfig(false);
        CharacterDatabase.Execute("DELETE FROM _幻化 WHERE NOT EXISTS (SELECT 1 FROM item_instance WHERE item_instance.guid = _幻化.GUID)");

#ifdef PRESETS
        CharacterDatabase.Execute("DELETE FROM _幻化保存 WHERE NOT EXISTS(SELECT 1 FROM characters WHERE characters.guid = _幻化保存.Owner)");
#endif
    }
};

class global_transmog_script : public GlobalScript {
    public:
        global_transmog_script() : GlobalScript("global_transmog_script") { }

        void OnItemDelFromDB(CharacterDatabaseTransaction trans, uint32 itemGuid) override
        {
            sT->DeleteFakeFromDB(itemGuid, &trans);
        }

        void OnMirrorImageDisplayItem(const Item *item, uint32 &display) override
        {
            if (uint32 entry = sTransmogrification->GetFakeEntry(item->GetGUID().GetRawValue()))
                display=uint32(sObjectMgr->GetItemTemplate(entry)->DisplayInfoID);
        }
};

void AddSC_TransmogScript()
{
    new global_transmog_script();
    new NpcTransmogrifier();
    new PS_Transmogrification();
    new WS_Transmogrification();
}
