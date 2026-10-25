#include "RuneSystem.h"
#include "RuneApplyManager.h"
#include "RuneUtils.h"
#include "Bag.h"
#include "Chat.h"
#include "ItemTemplate.h"
#include "Language.h"
#include "Player.h"
#include "ScriptMgr.h"

#define MODULE_ITEM_IDENTIFICATION_SYSTEM
#include "mod-item-identification-system/src/ItemIdentificationSystem.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr char RUNE_UI_PREFIX[] = "IRUN";
constexpr uint32 RUNE_UI_MAX_SLOTS = 20;

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
    try
    {
        return static_cast<uint32>(std::stoul(value));
    }
    catch (...)
    {
        return 0;
    }
}

int32 ToInt32(std::string const& value, int32 fallback = 0)
{
    try
    {
        return static_cast<int32>(std::stol(value));
    }
    catch (...)
    {
        return fallback;
    }
}

std::string CleanField(std::string text)
{
    for (char& ch : text)
    {
        if (ch == '|' || ch == '~' || ch == '^' || ch == '\t' || ch == '\r' || ch == '\n')
            ch = ' ';
    }
    return text;
}

void SendRuneUIMessage(Player* player, std::string const& payload)
{
    if (!player)
        return;

    std::string fullMessage = std::string(RUNE_UI_PREFIX) + "\t" + payload;
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

            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, static_cast<uint8>(slot - 1));
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

void RefreshTooltipData(Player* player, Item* item)
{
    if (!player || !item || !sItemIdentificationSystem)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    sItemIdentificationSystem->ClearItemCache(item->GetEntry(), itemGuid);
    sItemIdentificationSystem->SendAllModuleDataAddon(player, item->GetEntry(), itemGuid);
}

void SendRuneData(Player* player, Item* item, char const* status)
{
    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);
    CustomRuneStorage const* storage = sRuneSystem->GetItemRuneStorage(itemGuid);

    std::vector<uint32> slotIds;
    std::vector<uint32> runeIds;
    if (storage)
    {
        slotIds = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
        runeIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);
    }

    if (slotIds.size() > RUNE_UI_MAX_SLOTS)
        slotIds.resize(RUNE_UI_MAX_SLOTS);

    uint32 filledSlots = 0;
    std::ostringstream slotStream;
    for (size_t i = 0; i < slotIds.size(); ++i)
    {
        uint32 slotId = slotIds[i];
        uint32 runeItemId = (i < runeIds.size()) ? runeIds[i] : 0;

        if (i > 0)
            slotStream << "~";

        std::string runeName;
        std::string runeDesc;
        uint32 runeQuality = 0;
        if (runeItemId > 0)
        {
            ++filledSlots;
            if (CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeItemId))
            {
                runeName = runeInfo->name;
                runeQuality = runeInfo->runeQuality;
                runeDesc = runeInfo->effectDescription;
            }
        }

        slotStream << slotId << "^"
                   << runeItemId << "^"
                   << CleanField(runeName) << "^"
                   << runeQuality << "^"
                   << CleanField(runeDesc);
    }

    std::ostringstream response;
    response << "DATA|" << item->GetEntry()
             << "|" << itemGuid
             << "|" << slotIds.size()
             << "|" << filledSlots
             << "|" << (status ? status : "")
             << "|" << slotStream.str();

    SendRuneUIMessage(player, response.str());
}

void SendBagRuneData(Player* player)
{
    if (!player)
        return;

    std::map<uint32, uint32> counts;

    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            if (sRuneSystem->GetRuneInfoByItemId(item->GetEntry()))
                counts[item->GetEntry()] += item->GetCount();
        }
    }

    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        Bag* bag = player->GetBagByPos(bagSlot);
        if (!bag)
            continue;

        for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
        {
            if (Item* item = bag->GetItemByPos(slot))
            {
                if (sRuneSystem->GetRuneInfoByItemId(item->GetEntry()))
                    counts[item->GetEntry()] += item->GetCount();
            }
        }
    }

    std::ostringstream list;
    bool first = true;
    for (auto const& pair : counts)
    {
        CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfoByItemId(pair.first);
        if (!runeInfo)
            continue;

        if (!first)
            list << "~";
        first = false;

        list << pair.first << "^"
             << CleanField(runeInfo->name) << "^"
             << runeInfo->runeQuality << "^"
             << pair.second << "^"
             << runeInfo->runeSlot << "^"
             << CleanField(runeInfo->effectDescription);
    }

    SendRuneUIMessage(player, std::string("BAG|") + list.str());
}

class RuneUIScript : public PlayerScript
{
public:
    RuneUIScript() : PlayerScript("RuneUIScript") { }

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        if (msg.substr(0, tabPos) != RUNE_UI_PREFIX)
            return;

        std::vector<std::string> parts = Split(msg.substr(tabPos + 1), '|');
        if (parts.empty())
            return;

        std::string const& command = parts[0];
        if (command == "BAG")
        {
            SendBagRuneData(player);
            return;
        }

        if (parts.size() < 5)
        {
            SendRuneUIMessage(player, "RESULT|0|参数错误");
            return;
        }

        uint32 itemId = ToUInt32(parts[1]);
        int32 bag = ToInt32(parts[2], -1);
        int32 slot = ToInt32(parts[3], -1);
        bool equipped = ToUInt32(parts[4]) != 0;
        Item* item = FindItemByUiLocation(player, itemId, bag, slot, equipped);

        if (!item)
        {
            SendRuneUIMessage(player, "RESULT|0|没有找到该装备");
            return;
        }

        if (command == "QUERY")
        {
            SendRuneData(player, item, "查询完成");
            SendBagRuneData(player);
            return;
        }

        if (command == "SOCKET")
        {
            if (parts.size() < 7)
            {
                SendRuneUIMessage(player, "RESULT|0|参数错误");
                return;
            }

            uint32 slotId = ToUInt32(parts[5]);
            uint32 runeItemId = ToUInt32(parts[6]);
            if (slotId == 0 || runeItemId == 0)
            {
                SendRuneUIMessage(player, "RESULT|0|请选择符文槽和符文");
                return;
            }

            bool success = sRuneSystem->AddRuneToItem(player, item, runeItemId, slotId);
            SendRuneUIMessage(player, std::string("RESULT|") + (success ? "1|符文镶嵌成功" : "0|符文镶嵌失败"));
            SendRuneData(player, item, success ? "符文镶嵌完成" : "符文镶嵌失败");
            SendBagRuneData(player);
            if (success)
                RefreshTooltipData(player, item);
            return;
        }

        if (command == "UNSOCKET")
        {
            if (parts.size() < 6)
            {
                SendRuneUIMessage(player, "RESULT|0|参数错误");
                return;
            }

            uint32 slotId = ToUInt32(parts[5]);
            bool success = sRuneSystem->RemoveRuneFromItem(player, item, slotId);
            SendRuneUIMessage(player, std::string("RESULT|") + (success ? "1|符文已移除" : "0|符文移除失败"));
            SendRuneData(player, item, success ? "符文已移除" : "符文移除失败");
            if (success)
                RefreshTooltipData(player, item);
            return;
        }

        SendRuneUIMessage(player, "RESULT|0|未知命令");
    }
};
}

void AddSC_rune_ui_script()
{
    new RuneUIScript();
}
