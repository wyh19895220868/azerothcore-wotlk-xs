#include "ItemEnhancementMgr.h"
#include "Bag.h"
#include "Chat.h"
#include "Language.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr char ITEM_ENHANCEMENT_UI_PREFIX[] = "IENH";

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

void SendEnhancementUIMessage(Player* player, std::string const& payload)
{
    if (!player)
        return;

    std::string fullMessage = std::string(ITEM_ENHANCEMENT_UI_PREFIX) + "\t" + payload;
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

void SendEnhancementData(Player* player, Item* item, char const* status)
{
    if (!player || !item)
        return;

    uint32 itemGuid = item->GetGUID().GetCounter();
    EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);
    uint32 level = record ? record->level : 0;
    uint32 group = record ? record->group : 0;
    uint32 failureCount = record ? record->failureCount : 0;
    std::string attrs = record ? record->statValues : "";
    bool canEnhance = sItemEnhancementMgr->CanEnhanceItem(player, item);
    EnhancementTemplate const* nextTemplate = sItemEnhancementMgr->GetEnhancementTemplateByGroupAndLevel(group, level + 1);
    EnhancementTemplate const* ruleTemplate = nextTemplate;
    bool maxLevelReached = nextTemplate == nullptr && level > 0;
    if (!ruleTemplate && level > 0)
        ruleTemplate = sItemEnhancementMgr->GetEnhancementTemplateByGroupAndLevel(group, level);
    canEnhance = canEnhance && nextTemplate != nullptr;

    float successRate = 0.0f;
    uint32 pityCount = 0;
    float pityBonus = 0.0f;
    uint32 luckItemId = 0;
    uint32 luckItemCount = 0;
    float luckBonus = 0.0f;
    uint32 protectionItemId = 0;
    uint32 protectionItemCount = 0;
    uint32 failureType = 0;
    uint32 failureDropLevels = 0;
    if (ruleTemplate)
    {
        pityCount = ruleTemplate->pityFailureCount;
        pityBonus = ruleTemplate->pityChancePerFailure;
        luckItemId = ruleTemplate->luckItemId;
        luckItemCount = ruleTemplate->luckItemCount;
        luckBonus = ruleTemplate->luckChanceBonus;
        protectionItemId = ruleTemplate->protectionItemId;
        protectionItemCount = ruleTemplate->protectionItemCount;
        failureType = ruleTemplate->failureType;
        failureDropLevels = ruleTemplate->failureDropLevels;
    }
    if (nextTemplate)
    {
        successRate = nextTemplate->chance * sItemEnhancementMgr->GetSuccessRateRate();
        successRate += static_cast<float>(failureCount) * nextTemplate->pityChancePerFailure;
        if (luckItemId && luckItemCount && player->HasItemCount(luckItemId, luckItemCount, false))
            successRate += luckBonus;
        if (pityCount && failureCount >= pityCount)
            successRate = 100.0f;
        successRate = std::min(100.0f, std::max(0.0f, successRate));
    }

    std::ostringstream response;
    response << "DATA|" << item->GetEntry()
             << "|" << itemGuid
             << "|" << level
             << "|" << (canEnhance ? 1 : 0)
             << "|" << (status ? status : "")
             << "|" << attrs
             << "|" << successRate
             << "|" << failureCount
             << "|" << pityCount
             << "|" << pityBonus
             << "|" << luckItemId
             << "|" << luckItemCount
             << "|" << luckBonus
             << "|" << (luckItemId && luckItemCount && player->HasItemCount(luckItemId, luckItemCount, false) ? 1 : 0)
             << "|" << protectionItemId
             << "|" << protectionItemCount
             << "|" << (protectionItemId && protectionItemCount && player->HasItemCount(protectionItemId, protectionItemCount, false) ? 1 : 0)
             << "|" << failureType
             << "|" << failureDropLevels
             << "|" << (maxLevelReached ? 1 : 0);

    SendEnhancementUIMessage(player, response.str());
}

class ItemEnhancementUIScript : public PlayerScript
{
public:
    ItemEnhancementUIScript() : PlayerScript("ItemEnhancementUIScript") { }

    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        if (!player || type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        if (prefix != ITEM_ENHANCEMENT_UI_PREFIX)
            return;

        std::vector<std::string> parts = Split(msg.substr(tabPos + 1), '|');
        if (parts.size() < 5)
        {
            SendEnhancementUIMessage(player, "RESULT|0|参数错误");
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
            SendEnhancementUIMessage(player, "RESULT|0|没有找到该装备");
            return;
        }

        if (command == "QUERY")
        {
            SendEnhancementData(player, item, "查询完成");
            return;
        }

        if (command == "ENHANCE")
        {
            bool success = sItemEnhancementMgr->EnhanceItem(player, item);
            SendEnhancementUIMessage(player, std::string("RESULT|") + (success ? "1|强化成功" : "0|强化失败"));
            SendEnhancementData(player, item, success ? "强化完成" : "强化失败");
            return;
        }

        SendEnhancementUIMessage(player, "RESULT|0|未知命令");
    }
};
}

void AddSC_ItemEnhancementUIScript()
{
    new ItemEnhancementUIScript();
}
