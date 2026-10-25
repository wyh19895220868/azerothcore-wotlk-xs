/*
 * ItemEnhancementAddonUtil.cpp - Addon 消息处理辅助工具实现
 */

#include "ItemEnhancementAddonUtil.h"
#include "ItemEnhancementMgr.h"
#include "Chat.h"
#include "Log.h"
#include "Bag.h"
#include <algorithm>

namespace AddonUtil
{
    // 调试模式全局开关
    static bool g_debugMode = false;

    // ========================================
    // 字符串处理工具实现
    // ========================================

    std::vector<std::string> SplitString(std::string const& str, char delimiter)
    {
        std::vector<std::string> parts;
        std::stringstream ss(str);
        std::string item;

        while (std::getline(ss, item, delimiter))
        {
            if (!item.empty())
                parts.push_back(item);
        }

        return parts;
    }

    std::string JoinString(std::vector<std::string> const& parts, char delimiter)
    {
        if (parts.empty())
            return "";

        std::ostringstream oss;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            if (i > 0)
                oss << delimiter;
            oss << parts[i];
        }

        return oss.str();
    }

    std::string EscapeString(std::string const& str)
    {
        std::string result;
        result.reserve(str.length());

        for (char c : str)
        {
            switch (c)
            {
                case '\t': result += "\\t"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\\': result += "\\\\"; break;
                case '|': result += "\\|"; break;
                case ':': result += "\\:"; break;
                default: result += c; break;
            }
        }

        return result;
    }

    // ========================================
    // MessageBuilder 实现
    // ========================================

    MessageBuilder::MessageBuilder(std::string const& command)
    {
        m_stream << command;
        m_firstParam = false;
    }

    MessageBuilder& MessageBuilder::AddParam(std::string const& value)
    {
        if (!m_firstParam)
            m_stream << ':';
        else
            m_firstParam = false;

        m_stream << value;
        return *this;
    }

    MessageBuilder& MessageBuilder::AddParam(uint32 value)
    {
        return AddParam(std::to_string(value));
    }

    MessageBuilder& MessageBuilder::AddParam(int32 value)
    {
        return AddParam(std::to_string(value));
    }

    MessageBuilder& MessageBuilder::AddParam(uint64 value)
    {
        return AddParam(std::to_string(value));
    }

    MessageBuilder& MessageBuilder::AddParam(float value)
    {
        return AddParam(std::to_string(value));
    }

    MessageBuilder& MessageBuilder::AddParam(bool value)
    {
        return AddParam(std::string(value ? "1" : "0"));
    }

    MessageBuilder& MessageBuilder::AddKeyValue(std::string const& key, std::string const& value)
    {
        return AddParam(key + "=" + value);
    }

    MessageBuilder& MessageBuilder::AddKeyValue(std::string const& key, uint32 value)
    {
        return AddKeyValue(key, std::to_string(value));
    }

    MessageBuilder& MessageBuilder::AddKeyValue(std::string const& key, int32 value)
    {
        return AddKeyValue(key, std::to_string(value));
    }

    MessageBuilder& MessageBuilder::AddKeyValue(std::string const& key, float value)
    {
        return AddKeyValue(key, std::to_string(value));
    }

    MessageBuilder& MessageBuilder::AddKeyValue(std::string const& key, bool value)
    {
        return AddKeyValue(key, std::string(value ? "1" : "0"));
    }

    std::string MessageBuilder::Build() const
    {
        return m_stream.str();
    }

    void MessageBuilder::Clear()
    {
        m_stream.str("");
        m_stream.clear();
        m_firstParam = true;
    }

    // ========================================
    // BatchResponseBuilder 实现
    // ========================================

    void BatchResponseBuilder::AddItem(std::string const& data)
    {
        if (!data.empty())
            m_items.push_back(data);
    }

    std::string BatchResponseBuilder::Build() const
    {
        return JoinString(m_items, '|');
    }

    void BatchResponseBuilder::Clear()
    {
        m_items.clear();
    }

    // ========================================
    // MessageParser 实现
    // ========================================

    MessageParser::MessageParser(std::string const& message)
    {
        m_params = SplitString(message, ':');
    }

    std::string MessageParser::GetCommand() const
    {
        return m_params.empty() ? "" : m_params[0];
    }

    size_t MessageParser::GetParamCount() const
    {
        return m_params.empty() ? 0 : m_params.size() - 1;
    }

    std::string MessageParser::GetParam(size_t index, std::string const& defaultValue) const
    {
        // index 是从 0 开始的参数索引（不包括命令）
        size_t actualIndex = index + 1;
        return actualIndex < m_params.size() ? m_params[actualIndex] : defaultValue;
    }

    bool MessageParser::HasParam(size_t index) const
    {
        size_t actualIndex = index + 1;
        return actualIndex < m_params.size();
    }

    // ========================================
    // ItemDataSerializer 实现
    // ========================================

    ItemDataSerializer::ItemDataSerializer(Item* item)
        : m_item(item)
    {
    }

    ItemDataSerializer& ItemDataSerializer::AddBasicInfo()
    {
        if (!m_item)
            return *this;

        m_data["ItemId"] = std::to_string(m_item->GetEntry());
        m_data["Guid"] = std::to_string(m_item->GetGUIDLow());
        m_data["Count"] = std::to_string(m_item->GetCount());
        m_data["Quality"] = std::to_string(m_item->GetTemplate()->Quality);

        return *this;
    }

    ItemDataSerializer& ItemDataSerializer::AddEnhancementInfo()
    {
        if (!m_item)
            return *this;

        // 从 ItemEnhancementMgr 获取强化记录
        EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(m_item->GetGUIDLow());

        if (record)
        {
            m_data["EnhLevel"] = std::to_string(record->level);
            m_data["EnhGroup"] = std::to_string(record->group);
            m_data["EnhExp"] = std::to_string(record->enhancementExp);
            m_data["EnhStats"] = record->statValues;
            m_data["QualityLevel"] = std::to_string(record->qualityLevel);
            m_data["EnhCount"] = std::to_string(record->enhancementCount);
        }
        else
        {
            m_data["EnhLevel"] = "0";
            m_data["EnhGroup"] = "0";
        }

        return *this;
    }

    ItemDataSerializer& ItemDataSerializer::AddGrowthInfo()
    {
        if (!m_item)
            return *this;

        // TODO: 获取成长数据
        m_data["GrowthLevel"] = "0";

        return *this;
    }

    ItemDataSerializer& ItemDataSerializer::AddRuneInfo()
    {
        if (!m_item)
            return *this;

        // TODO: 获取符文数据
        m_data["Runes"] = "NONE";

        return *this;
    }

    ItemDataSerializer& ItemDataSerializer::AddEnchantInfo()
    {
        if (!m_item)
            return *this;

        // 获取附魔信息
        for (uint8 i = 0; i < MAX_ENCHANTMENT_SLOT; ++i)
        {
            uint32 enchantId = m_item->GetEnchantmentId(EnchantmentSlot(i));
            if (enchantId > 0)
            {
                std::string key = "Enchant" + std::to_string(i);
                m_data[key] = std::to_string(enchantId);
            }
        }

        return *this;
    }

    ItemDataSerializer& ItemDataSerializer::AddRandomProperties()
    {
        if (!m_item)
            return *this;

        int32 randomPropId = m_item->GetItemRandomPropertyId();
        if (randomPropId != 0)
        {
            m_data["RandomProp"] = std::to_string(randomPropId);
        }

        return *this;
    }

    std::string ItemDataSerializer::Build() const
    {
        MessageBuilder builder;

        for (auto const& pair : m_data)
        {
            builder.AddKeyValue(pair.first, pair.second);
        }

        return builder.Build();
    }

    // ========================================
    // 物品查找工具实现
    // ========================================

    Item* FindItemByGuid(Player* player, uint32 guidLow)
    {
        if (!player || guidLow == 0)
            return nullptr;

        // 搜索装备栏
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (item->GetGUIDLow() == guidLow)
                    return item;
            }
        }

        // 搜索背包
        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (item->GetGUIDLow() == guidLow)
                    return item;
            }
        }

        // 搜索背包格子
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* bag = player->GetBagByPos(i))
            {
                for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                {
                    if (Item* item = bag->GetItemByPos(j))
                    {
                        if (item->GetGUIDLow() == guidLow)
                            return item;
                    }
                }
            }
        }

        return nullptr;
    }

    Item* FindItemByLocation(Player* player, uint32 itemId, int32 bag, int32 slot, bool equipped)
    {
        if (!player)
            return nullptr;

        Item* item = nullptr;

        // 如果是装备位
        if (equipped && slot >= 0 && slot < EQUIPMENT_SLOT_END)
        {
            item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        }
        // 背包中的物品
        else if (bag >= INVENTORY_SLOT_ITEM_START && bag < INVENTORY_SLOT_ITEM_END)
        {
            item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        }
        // 背包格子中的物品
        else if (bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END &&
                slot >= 0 && slot < MAX_BAG_SIZE)
        {
            item = player->GetItemByPos(bag, slot);
        }

        // 验证物品 ID 是否匹配
        if (item && itemId > 0 && item->GetEntry() != itemId)
            return nullptr;

        return item;
    }

    std::vector<uint32> GetAllItemGuids(Player* player)
    {
        std::vector<uint32> guids;

        if (!player)
            return guids;

        // 装备栏
        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                guids.push_back(item->GetGUIDLow());
        }

        // 背包
        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                guids.push_back(item->GetGUIDLow());
        }

        // 背包格子
        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* bag = player->GetBagByPos(i))
            {
                for (uint32 j = 0; j < bag->GetBagSize(); ++j)
                {
                    if (Item* item = bag->GetItemByPos(j))
                        guids.push_back(item->GetGUIDLow());
                }
            }
        }

        return guids;
    }

    // ========================================
    // 消息发送工具实现
    // ========================================

    void SendAddonMessage(Player* player, std::string const& prefix, std::string const& message)
    {
        if (!player || prefix.empty() || message.empty())
            return;

        // 记录日志
        LogMessage(prefix, message, false);

        // 构建完整消息（前缀 + TAB + 内容）
        std::string fullMessage = prefix + "\t" + message;

        // 创建数据包
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_ADDON, LANG_ADDON,
                                    player, player, fullMessage);

        // 发送给玩家
        player->SendDirectMessage(&data);
    }

    void SendError(Player* player, std::string const& prefix, std::string const& error)
    {
        MessageBuilder builder("ERROR");
        builder.AddParam(error);
        SendAddonMessage(player, prefix, builder.Build());
    }

    void SendSuccess(Player* player, std::string const& prefix, std::string const& message)
    {
        MessageBuilder builder("SUCCESS");
        builder.AddParam(message);
        SendAddonMessage(player, prefix, builder.Build());
    }

    // ========================================
    // 调试工具实现
    // ========================================

    void LogDebug(std::string const& message)
    {
        if (g_debugMode)
        {
            LOG_INFO("module", "[AddonUtil] {}", message);
        }
    }

    void LogMessage(std::string const& prefix, std::string const& message, bool incoming)
    {
        if (g_debugMode)
        {
            std::string direction = incoming ? "<<" : ">>";
            LOG_INFO("module", "[AddonUtil] {} [{}] {}", direction, prefix,
                    message.length() > 100 ? message.substr(0, 100) + "..." : message);
        }
    }

    void SetDebugMode(bool enabled)
    {
        g_debugMode = enabled;
        LOG_INFO("module", "[AddonUtil] Debug mode {}", enabled ? "enabled" : "disabled");
    }

    bool IsDebugMode()
    {
        return g_debugMode;
    }

} // namespace AddonUtil
