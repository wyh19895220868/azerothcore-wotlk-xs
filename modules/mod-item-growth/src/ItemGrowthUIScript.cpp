#include "ItemGrowthMgr.h"
#include "Bag.h"
#include "Chat.h"
#include "Language.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "StringConvert.h"
#include <limits>

#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr char ITEM_GROWTH_UI_PREFIX[] = "IGRO";

std::vector<std::string> Split(std::string const& text, char delimiter)
{
    std::vector<std::string> parts;
    std::string current;
    std::istringstream stream(text);
    while (std::getline(stream, current, delimiter))
        parts.push_back(current);
    return parts;
}

uint32 ToUInt32(std::string const& value)
{
    return Acore::StringTo<uint32>(value).value_or(0);
}

int32 ToInt32(std::string const& value, int32 fallback = 0)
{
    return Acore::StringTo<int32>(value).value_or(fallback);
}

void SendGrowthUIMessage(Player* player, std::string const& payload)
{
    if (!player || payload.size() > std::numeric_limits<uint16>::max())
        return;

    // Native clients also receive the same payload through
    // SMSG_NATIVE_UI_SNAPSHOT kind=4 (IGRO_DATA) and read it with
    // NativeUIGrowthData(). The addon message is kept for older clients.
    if (player->GetSession() && player->GetSession()->HasNativeUI())
    {
        WorldPacket packet(SMSG_NATIVE_UI_SNAPSHOT);
        packet << uint16(1);
        packet << uint8(4);
        packet << uint64(player->GetGUID().GetRawValue());
        packet << uint16(1);

        constexpr char key[] = "IGRO_DATA";
        packet << uint8(sizeof(key) - 1);
        packet.append(key, sizeof(key) - 1);
        packet << uint8(1);
        packet << uint16(payload.size());
        packet.append(payload.data(), payload.size());
        player->SendDirectMessage(&packet);
    }

    std::string fullMessage = std::string(ITEM_GROWTH_UI_PREFIX) + "\t" + payload;
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
    player->SendDirectMessage(&packet);
}

Item* FindItemByUiLocation(Player* player, uint32 itemId, int32 bag, int32 slot, bool equipped)
{
    if (!player || itemId == 0)
        return nullptr;

    auto matches = [itemId](Item* item) -> bool
    {
        return item && item->GetEntry() == itemId;
    };

    if (equipped)
    {
        int32 serverSlot = slot > 0 ? slot - 1 : slot;
        if (serverSlot >= EQUIPMENT_SLOT_START && serverSlot < EQUIPMENT_SLOT_END)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, static_cast<uint8>(serverSlot));
            if (matches(item))
                return item;
        }
    }

    if (bag >= 0 && slot > 0)
    {
        if (bag == 0)
        {
            uint8 clientSlot = static_cast<uint8>(slot - 1);
            if (clientSlot < (INVENTORY_SLOT_ITEM_END - INVENTORY_SLOT_ITEM_START))
            {
                uint8 serverSlot = INVENTORY_SLOT_ITEM_START + clientSlot;
                Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
                if (matches(item))
                    return item;
            }

            uint8 serverSlot = static_cast<uint8>(slot - 1);
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, serverSlot);
            if (matches(item))
                return item;
        }
        else if (bag >= 1 && bag <= 4)
        {
            uint8 bagPos = INVENTORY_SLOT_BAG_START + static_cast<uint8>(bag - 1);
            if (Bag* bagObject = player->GetBagByPos(bagPos))
            {
                uint32 innerSlot = static_cast<uint32>(slot - 1);
                if (innerSlot < bagObject->GetBagSize())
                {
                    Item* item = bagObject->GetItemByPos(innerSlot);
                    if (matches(item))
                        return item;
                }
            }
        }
    }

    for (uint8 invSlot = EQUIPMENT_SLOT_START; invSlot < EQUIPMENT_SLOT_END; ++invSlot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, invSlot);
        if (matches(item))
            return item;
    }

    for (uint8 invSlot = INVENTORY_SLOT_ITEM_START; invSlot < INVENTORY_SLOT_ITEM_END; ++invSlot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, invSlot);
        if (matches(item))
            return item;
    }

    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        Bag* bagObject = player->GetBagByPos(bagSlot);
        if (!bagObject)
            continue;

        for (uint32 innerSlot = 0; innerSlot < bagObject->GetBagSize(); ++innerSlot)
        {
            Item* item = bagObject->GetItemByPos(innerSlot);
            if (matches(item))
                return item;
        }
    }

    return nullptr;
}

void SendGrowthData(Player* player, Item* item, char const* status)
{
    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    ItemGrowthPlayerRecord* record = sItemGrowthMgr->GetPlayerItemRecord(itemGuid);
    bool canGrow = sItemGrowthMgr->CanItemGrow(item);

    uint32 level = 0;
    uint32 exp = 0;
    uint32 requiredExp = 0;
    std::string attrs;
    std::string percentAttrs;

    if (record)
    {
        sItemGrowthMgr->UpdateRequiredExp(record);
        level = record->currentLevel;
        exp = record->currentExp;
        requiredExp = record->requiredExp;
        attrs = record->growthAttributes;
        percentAttrs = record->percentAttributes;
    }

    std::ostringstream response;
    response << "DATA|" << item->GetEntry()
             << "|" << itemGuid
             << "|" << (canGrow ? 1 : 0)
             << "|" << level
             << "|" << exp
             << "|" << requiredExp
             << "|" << (status ? status : "")
             << "|" << attrs
             << "|" << percentAttrs;

    SendGrowthUIMessage(player, response.str());
}

class ItemGrowthUIScript : public PlayerScript
{
public:
    ItemGrowthUIScript() : PlayerScript("ItemGrowthUIScript") { }

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != ITEM_GROWTH_UI_PREFIX)
            return;

        std::vector<std::string> parts = Split(msg.substr(tabPos + 1), '|');
        if (parts.size() < 5)
        {
            SendGrowthUIMessage(player, "RESULT|0|参数错误");
            return;
        }

        std::string const& command = parts[0];
        uint32 itemId = ToUInt32(parts[1]);
        int32 bag = ToInt32(parts[2], -1);
        int32 slot = ToInt32(parts[3], -1);
        bool equipped = ToUInt32(parts[4]) != 0;

        Item* item = FindItemByUiLocation(player, itemId, bag, slot, equipped);
        if (!item)
        {
            SendGrowthUIMessage(player, "RESULT|0|没有找到该装备");
            return;
        }

        if (command == "QUERY")
        {
            SendGrowthData(player, item, "查询完成");
            return;
        }

        if (command == "UPGRADE")
        {
            bool success = sItemGrowthMgr->UpgradeItem(player, item);
            SendGrowthUIMessage(player, std::string("RESULT|") + (success ? "1|成长成功" : "0|成长失败"));
            SendGrowthData(player, item, success ? "成长完成" : "成长失败");
            return;
        }

        SendGrowthUIMessage(player, "RESULT|0|未知命令");
    }
};
}

void AddSC_ItemGrowthUIScript()
{
    new ItemGrowthUIScript();
}
