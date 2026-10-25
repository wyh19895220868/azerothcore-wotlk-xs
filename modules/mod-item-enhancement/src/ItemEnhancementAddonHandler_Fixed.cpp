/*
 * ItemEnhancementAddonHandler.cpp - Addon 消息处理器（修复版）
 *
 * 这个文件是修复后的版本，已连接到实际的 ItemEnhancementMgr API
 *
 * 使用方法：
 * 1. 备份原文件： copy ItemEnhancementAddonHandler.cpp ItemEnhancementAddonHandler.cpp.backup
 * 2. 用这个文件替换： copy ItemEnhancementAddonHandler_Fixed.cpp ItemEnhancementAddonHandler.cpp
 * 3. 重新编译
 */

#include "ItemEnhancementMgr.h"
#include "ItemEnhancementAddonUtil.h"
#include "Chat.h"
#include "Player.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "ScriptMgr.h"
#include <sstream>
#include <vector>
#include <map>

/**
 * @brief 装备强化 Addon 消息处理脚本
 */
class ItemEnhancementAddonHandler : public PlayerScript
{
public:
    ItemEnhancementAddonHandler() : PlayerScript("ItemEnhancementAddonHandler") {}

    /**
     * @brief 处理玩家聊天消息（包括 ADDON 消息）
     */
    void OnChat(Player* player, uint32 type, uint32 /*lang*/, std::string& msg) override
    {
        // 只处理 ADDON 类型消息
        if (type != CHAT_MSG_ADDON)
            return;

        // 解析消息格式：PREFIX\tCONTENT
        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        std::string prefix = msg.substr(0, tabPos);
        std::string content = msg.substr(tabPos + 1);

        // 记录日志
        AddonUtil::LogMessage(prefix, content, true);

        // 根据前缀路由到对应处理函数
        if (prefix == AddonUtil::Prefix::ENHANCEMENT)
            HandleEnhancementMessage(player, content);
        else if (prefix == AddonUtil::Prefix::GROWTH)
            HandleGrowthMessage(player, content);
        else if (prefix == AddonUtil::Prefix::RUNE)
            HandleRuneMessage(player, content);
        else if (prefix == AddonUtil::Prefix::TOOLTIP)
            HandleTooltipQueryMessage(player, content);
    }

private:
    /**
     * @brief 处理装备强化消息 (IENH)
     */
    void HandleEnhancementMessage(Player* player, std::string const& message)
    {
        AddonUtil::MessageParser parser(message);
        std::string command = parser.GetCommand();

        if (command == "QUERY")
        {
            // 查询装备强化信息
            uint32 itemId = parser.GetParamAs<uint32>(0);
            uint32 guidLow = parser.GetParamAs<uint32>(1);
            int32 bag = parser.GetParamAs<int32>(2, -1);
            int32 slot = parser.GetParamAs<int32>(3, -1);
            bool equipped = parser.HasParam(4) && parser.GetParam(4) == "1";

            // 查找物品
            Item* item = nullptr;
            if (guidLow > 0)
                item = AddonUtil::FindItemByGuid(player, guidLow);
            else if (bag >= 0 && slot >= 0)
                item = AddonUtil::FindItemByLocation(player, itemId, bag, slot, equipped);

            if (item)
            {
                SendEnhancementInfo(player, item);
            }
            else
            {
                // 物品未找到
                AddonUtil::SendError(player, AddonUtil::Prefix::ENHANCEMENT, "ITEM_NOT_FOUND");
            }
        }
        else if (command == "ENHANCE")
        {
            // 执行强化操作
            uint32 itemId = parser.GetParamAs<uint32>(0);
            uint32 guidLow = parser.GetParamAs<uint32>(1);
            uint32 group = parser.GetParamAs<uint32>(2, 0);

            Item* item = AddonUtil::FindItemByGuid(player, guidLow);
            if (!item)
            {
                AddonUtil::SendError(player, AddonUtil::Prefix::ENHANCEMENT, "ITEM_NOT_FOUND");
                return;
            }

            // 调用强化管理器
            if (sItemEnhancementMgr->EnhanceItem(player, item, group))
            {
                // 强化成功，发送更新信息
                SendEnhancementInfo(player, item);
                AddonUtil::SendSuccess(player, AddonUtil::Prefix::ENHANCEMENT, "ENHANCE_SUCCESS");
            }
            else
            {
                AddonUtil::SendError(player, AddonUtil::Prefix::ENHANCEMENT, "ENHANCE_FAILED");
            }
        }
        else if (command == "RESET")
        {
            // 重置强化
            uint32 guidLow = parser.GetParamAs<uint32>(0);
            Item* item = AddonUtil::FindItemByGuid(player, guidLow);

            if (!item)
            {
                AddonUtil::SendError(player, AddonUtil::Prefix::ENHANCEMENT, "ITEM_NOT_FOUND");
                return;
            }

            // 删除强化记录
            sItemEnhancementMgr->DeleteEnhancementRecord(item->GetGUIDLow());
            sItemEnhancementMgr->RemoveEnhancementEffects(player, item);

            AddonUtil::SendSuccess(player, AddonUtil::Prefix::ENHANCEMENT, "RESET_SUCCESS");
            SendEnhancementInfo(player, item);
        }
    }

    /**
     * @brief 处理装备成长消息 (IGRO)
     */
    void HandleGrowthMessage(Player* player, std::string const& message)
    {
        AddonUtil::MessageParser parser(message);
        std::string command = parser.GetCommand();

        if (command == "QUERY")
        {
            uint32 guidLow = parser.GetParamAs<uint32>(0);
            Item* item = AddonUtil::FindItemByGuid(player, guidLow);

            if (item)
            {
                // TODO: 如果有专门的成长系统，调用相应的管理器
                // 目前返回基本信息
                AddonUtil::MessageBuilder builder("DATA");
                builder.AddParam(item->GetEntry())
                       .AddParam(guidLow)
                       .AddKeyValue("GrowthLevel", 0);

                AddonUtil::SendAddonMessage(player, AddonUtil::Prefix::GROWTH, builder.Build());
            }
            else
            {
                AddonUtil::SendError(player, AddonUtil::Prefix::GROWTH, "ITEM_NOT_FOUND");
            }
        }
    }

    /**
     * @brief 处理符文消息 (IRUN)
     */
    void HandleRuneMessage(Player* player, std::string const& message)
    {
        AddonUtil::MessageParser parser(message);
        std::string command = parser.GetCommand();

        if (command == "QUERY")
        {
            uint32 guidLow = parser.GetParamAs<uint32>(0);
            Item* item = AddonUtil::FindItemByGuid(player, guidLow);

            if (item)
            {
                // TODO: 如果有专门的符文系统，调用相应的管理器
                AddonUtil::MessageBuilder builder("DATA");
                builder.AddParam(item->GetEntry())
                       .AddParam(guidLow)
                       .AddKeyValue("Runes", "NONE");

                AddonUtil::SendAddonMessage(player, AddonUtil::Prefix::RUNE, builder.Build());
            }
            else
            {
                AddonUtil::SendError(player, AddonUtil::Prefix::RUNE, "ITEM_NOT_FOUND");
            }
        }
    }

    /**
     * @brief 处理 Tooltip 查询消息 (UITQ)
     */
    void HandleTooltipQueryMessage(Player* player, std::string const& message)
    {
        AddonUtil::MessageParser parser(message);
        std::string command = parser.GetCommand();

        if (command == "QUERY")
        {
            // 单个查询
            uint32 itemId = parser.GetParamAs<uint32>(0);
            uint32 guidLow = parser.GetParamAs<uint32>(1);

            SendTooltipData(player, itemId, guidLow);
        }
        else if (command == "BATCH_QUERY")
        {
            // 批量查询
            if (parser.GetParamCount() < 1)
                return;

            std::string batchData = parser.GetParam(0);
            HandleBatchTooltipQuery(player, batchData);
        }
    }

    /**
     * @brief 处理批量 Tooltip 查询
     */
    void HandleBatchTooltipQuery(Player* player, std::string const& batchData)
    {
        // 分割批量数据
        auto items = AddonUtil::SplitString(batchData, '|');

        // 限制查询数量
        uint32 queryCount = std::min<uint32>(items.size(), AddonUtil::Config::MAX_BATCH_SIZE);

        AddonUtil::BatchResponseBuilder builder;

        for (uint32 i = 0; i < queryCount; ++i)
        {
            // 解析每个物品
            auto itemParts = AddonUtil::SplitString(items[i], ':');
            if (itemParts.size() < 2)
                continue;

            uint32 itemId = AddonUtil::SafeStringToNumber<uint32>(itemParts[0]);
            uint32 guidLow = AddonUtil::SafeStringToNumber<uint32>(itemParts[1]);

            // 查询物品数据
            Item* item = AddonUtil::FindItemByGuid(player, guidLow);
            if (item)
            {
                std::string itemData = BuildTooltipDataString(item);
                builder.AddItem(itemData);
            }
            else
            {
                // 物品未找到
                std::string notFound = std::to_string(itemId) + ":" +
                                      std::to_string(guidLow) + ":NOT_FOUND";
                builder.AddItem(notFound);
            }
        }

        // 发送批量响应
        std::string response = "BATCH_RESPONSE:" + builder.Build();
        AddonUtil::SendAddonMessage(player, AddonUtil::Prefix::TOOLTIP, response);
    }

    /**
     * @brief 发送装备强化信息
     */
    void SendEnhancementInfo(Player* player, Item* item)
    {
        if (!player || !item)
            return;

        // 从 ItemEnhancementMgr 获取强化记录
        EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(item->GetGUIDLow());

        AddonUtil::MessageBuilder builder("DATA");
        builder.AddParam(item->GetEntry())
               .AddParam(item->GetGUIDLow());

        if (record)
        {
            // 有强化数据
            builder.AddKeyValue("EnhLevel", record->level)
                   .AddKeyValue("EnhGroup", record->group)
                   .AddKeyValue("EnhStats", record->statValues);
        }
        else
        {
            // 没有强化数据
            builder.AddKeyValue("EnhLevel", 0)
                   .AddKeyValue("EnhGroup", 0)
                   .AddKeyValue("EnhStats", "NONE");
        }

        AddonUtil::SendAddonMessage(player, AddonUtil::Prefix::ENHANCEMENT, builder.Build());
    }

    /**
     * @brief 发送 Tooltip 数据
     */
    void SendTooltipData(Player* player, uint32 itemId, uint32 guidLow)
    {
        if (!player)
            return;

        Item* item = AddonUtil::FindItemByGuid(player, guidLow);
        if (!item)
        {
            AddonUtil::MessageBuilder builder("DATA");
            builder.AddParam(0).AddParam(0).AddParam("NOT_FOUND");
            AddonUtil::SendAddonMessage(player, AddonUtil::Prefix::TOOLTIP, builder.Build());
            return;
        }

        std::string data = "DATA:" + BuildTooltipDataString(item);
        AddonUtil::SendAddonMessage(player, AddonUtil::Prefix::TOOLTIP, data);
    }

    /**
     * @brief 构建 Tooltip 数据字符串
     */
    std::string BuildTooltipDataString(Item* item)
    {
        if (!item)
            return "0:0:ERROR";

        // 使用序列化器构建数据
        AddonUtil::ItemDataSerializer serializer(item);

        std::string data = serializer
            .AddBasicInfo()
            .AddEnhancementInfo()
            .AddEnchantInfo()
            .AddRandomProperties()
            .Build();

        return data;
    }
};

/**
 * @brief 注册脚本
 */
void AddSC_ItemEnhancementAddonHandler()
{
    new ItemEnhancementAddonHandler();
}
