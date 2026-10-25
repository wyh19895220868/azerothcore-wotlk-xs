
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
#include <Custom/ItemFuncMgr/ItemFuncMgr.h>
#pragma execution_character_set("utf-8")

#define sT  sTransmogrification

class IDTransmog : public CreatureScript
{
public:
    IDTransmog() : CreatureScript("IDTransmog") { }

    bool OnGossipHello(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Misc_Book_11:30:30:-18:0|t 点击指定部位输入ID幻化", 0, 0);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Misc_Book_11:30:30:-18:0|t 删除此部位幻化则输入DELETE", 0, 0);

        for (uint8 Slot = EQUIPMENT_SLOT_START; Slot < EQUIPMENT_SLOT_END; ++Slot)
        {
            if (const char* SlotName = sT->GetSlotName(Slot, player->GetSession()))
            {
                Item* DoItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, Slot);
                uint32 Entry = DoItem ? sT->GetFakeEntry(DoItem->GetGUIDHigh()) : 0;
                std::string Icon = Entry ? sT->GetItemIcon(Entry, 30, 30, -18, 0) : sT->GetSlotIcon(Slot, 30, 30, -18, 0);

                AddGossipItemFor(player, GOSSIP_ICON_TALK, Icon + std::string(SlotName), 0, Slot, "", 0, true);
            }
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Enchant_Disenchant:30:30:-18:0|t 清除所有幻化", 999, 0, "你确定要清除所有幻化效果么?", 0, false);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());

        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action)
    {
        ClearGossipMenuFor(player);
        if (Sender == 0 && Action == 0)
        {
            OnGossipHello(player, creature);
            return true;
        }

        if (Sender == 999 && Action == 0)
        {
            bool Removed = false;
            CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();
            for (uint8 Slot = EQUIPMENT_SLOT_START; Slot < EQUIPMENT_SLOT_END; ++Slot)
            {
                if (Item* DoItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, Slot))
                {
                    if (!sT->GetFakeEntry(DoItem->GetGUIDHigh()))
                        continue;
                    sT->DeleteFakeEntry(player, Slot, DoItem, &Trans);
                    Removed = true;
                }
            }
            if (Removed)
            {
                player->SendSysMsg("所有幻化效果已成功去除");
                CharacterDatabase.CommitTransaction(Trans);
            }

            OnGossipHello(player, creature);
            return true;
        }

        CloseGossipMenuFor(player);
        return true;
    }

    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 Sender, uint32 Action, const char* Code)
    {
        ClearGossipMenuFor(player);

        if (!Code)
        {
            OnGossipHello(player, creature);
            return true;
        }

        if (*Code == '\0')
        {
            OnGossipHello(player, creature);
            return true;
        }

        if (strcmp(Code, "delete") == 0 || strcmp(Code, "DELETE") == 0)
        {
            CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();
            if (Item* DoItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, Action))
            {
                if (sT->GetFakeEntry(DoItem->GetGUIDHigh()))
                {
                    sT->DeleteFakeEntry(player, Action, DoItem, &Trans);
                    player->SendSysMsg("[幻化提示]: 当前幻化效果已成功去除");
                    CharacterDatabase.CommitTransaction(Trans);
                }
            }

            OnGossipHello(player, creature);
            return true;
        }

        std::string CodeStr = Code;
        static const char* Checkname = "1234567890";
        if (!CodeStr.length() || CodeStr.find_first_not_of(Checkname) != std::string::npos)
        {
            player->SendSysMsg("[幻化提示]: 只能是数字......");
            OnGossipHello(player, creature);
            return true;
        }

        uint32 Entry = (uint32)atoi(Code);
        ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(Entry);
        if (!Proto)
        {
            player->SendSysMsg("[幻化提示]: 该物品不存在于 item_template 请重新输入");
            OnGossipHello(player, creature);
            return true;
        }

        if (Proto->Class != ITEM_CLASS_WEAPON && Proto->Class != ITEM_CLASS_ARMOR)
        {
            player->SendSysMsg("[幻化提示]: 该物品不属于武器护甲类型 不能用于幻化");
            OnGossipHello(player, creature);
            return true;
        }

        auto Itr = IDTransmogStore.find(Entry);
        if (Itr == IDTransmogStore.end())
        {
            player->SendSysMsg("[幻化提示]: 该物品不存在于 id幻化表 请重新输入");
            OnGossipHello(player, creature);
            return true;
        }

        if (sT->IDTransmogrify(player, Proto, Action, Itr->second))
        {
            OnGossipHello(player, creature);
            return true;
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

void AddSC_IDTransmogScript()
{
    new IDTransmog();
}
