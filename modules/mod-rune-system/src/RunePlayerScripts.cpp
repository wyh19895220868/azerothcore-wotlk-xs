#include "ScriptMgr.h"
#include "Player.h"
#include "RuneSystem.h"
#include "RuneApplyManager.h"
#include "Item.h"
#include "Chat.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "RuneUtils.h"
#include "ObjectMgr.h"
#include "World.h"
#include <sstream>
#include <vector>
#include "ModuleFormat.h"

// ADDON消息前缀（与客户端保持一致）
const std::string ADDON_PREFIX = "RUNESYSTEM";
const std::string ADDON_PREFIX_RUNESLOT = "RUNESLOT";

// ============================================================
// UTF-8字符串长度说明
// ============================================================
// 重要：中文字符在UTF-8编码中占3个字节！
// 计算字符串前缀长度时必须按字节数计算，不能按字符数计算
//
// 示例：
// "符文查询装备" = 6个中文字符 × 3字节 = 18字节
// "符文查询背包" = 6个中文字符 × 3字节 = 18字节
// "符文查询槽位 " = 6个中文字符 × 3字节 + 1空格 = 19字节
// "符文查询效果" = 6个中文字符 × 3字节 = 18字节
// "符文信息 " = 4个中文字符 × 3字节 + 1空格 = 13字节
// "符文装备 " = 4个中文字符 × 3字节 + 1空格 = 13字节
// "符文卸载 " = 4个中文字符 × 3字节 + 1空格 = 13字节
//
// 使用 substr() 时必须使用正确的字节数，否则会导致越界崩溃！
// ============================================================

// ============================================================
// 辅助函数：通过ADDON消息发送响应给客户端
// 使用正确的ADDON消息格式（隐藏式通信，不显示在聊天框）
// 支持自动分批发送大消息
// ============================================================
void SendAddonResponse(Player* player, const std::string& message)
{
    if (!player || !player->GetSession())
        return;

    const size_t MAX_ADDON_MESSAGE_SIZE = 250; // 留5字节余量
    std::string prefix = ADDON_PREFIX + std::string("\t");
    size_t prefixLen = prefix.length();

    // 如果消息加上前缀后小于限制，直接发送
    if (prefixLen + message.length() <= MAX_ADDON_MESSAGE_SIZE)
    {
        // 直接发送消息内容，使用系统消息格式
        WorldPacket data(SMSG_MESSAGECHAT);
        data << uint8(CHAT_MSG_SYSTEM);
        data << int32(LANG_UNIVERSAL);
        data << player->GetGUID();
        data << uint32(0);
        data << uint32(message.length() + 1);
        data << message;
        data << uint8(0);

        player->GetSession()->SendPacket(&data);
        return;
    }

    // 消息过长，需要分批发送
    // 检查消息类型，只有 [RUNEINFO] 支持分批
    if (message.find("[RUNEINFO]") == 0)
    {
        // 提取消息头部信息：[RUNEINFO]guid:totalSlots:filledSlots:
        size_t firstColon = message.find(':', 10); // 跳过 [RUNEINFO]
        size_t secondColon = message.find(':', firstColon + 1);
        size_t thirdColon = message.find(':', secondColon + 1);

        if (thirdColon != std::string::npos)
        {
            std::string header = message.substr(0, thirdColon + 1); // 包含最后的冒号
            std::string slotData = message.substr(thirdColon + 1);

            // 计算实际可用空间：255 - 前缀(11) - 消息标记 - 安全余量(15)
            const size_t ADDON_PREFIX_SIZE = 11; // "RUNESYSTEM\t"
            const size_t SAFETY_MARGIN = 15; // 增加安全余量
            const size_t RUNEINFO_MORE_SIZE = 16; // "[RUNEINFO_MORE]"

            size_t firstBatchMaxSize = 255 - ADDON_PREFIX_SIZE - header.length() - SAFETY_MARGIN;
            size_t moreBatchMaxSize = 255 - ADDON_PREFIX_SIZE - RUNEINFO_MORE_SIZE - SAFETY_MARGIN;

            // 按槽位分批（每个槽位以分号分隔）
            std::vector<std::string> slots;
            size_t pos = 0;
            while (pos < slotData.length())
            {
                size_t semicolonPos = slotData.find(';', pos);
                if (semicolonPos == std::string::npos)
                {
                    // 最后一个槽位（没有分号）
                    if (pos < slotData.length())
                    {
                        slots.push_back(slotData.substr(pos));
                    }
                    break;
                }
                else
                {
                    slots.push_back(slotData.substr(pos, semicolonPos - pos + 1)); // 包含分号
                    pos = semicolonPos + 1;
                }
            }

            // 准备批次
            std::vector<std::string> batches;
            std::string currentBatch;

            for (size_t i = 0; i < slots.size(); ++i)
            {
                std::string testBatch = currentBatch + slots[i];
                size_t maxSize = batches.empty() ? firstBatchMaxSize : moreBatchMaxSize;

                if (testBatch.length() > maxSize && !currentBatch.empty())
                {
                    // 当前批次已满，保存并开始新批次
                    batches.push_back(currentBatch);
                    currentBatch = slots[i];
                }
                else
                {
                    currentBatch = testBatch;
                }
            }

            // 保存最后一批
            if (!currentBatch.empty())
            {
                batches.push_back(currentBatch);
            }

            // 发送所有批次
            for (size_t i = 0; i < batches.size(); ++i)
            {
                std::string batchMessage;
                if (i == 0)
                {
                    batchMessage = header + batches[i];
                }
                else
                {
                    batchMessage = "[RUNEINFO_MORE]" + batches[i];
                }

                // 直接发送消息内容，使用系统消息格式
                // 验证消息长度
                if (batchMessage.length() > 255)
                {
                    LOG_ERROR("module.runesystem", ModuleOldFormat("SendAddonResponse: Batch %s exceeds 255 bytes (%s bytes)! Truncating...", i + 1, batchMessage.length()).c_str());
                    // 截断到安全长度
                    batchMessage = batchMessage.substr(0, 250);
                }

                WorldPacket data(SMSG_MESSAGECHAT);
                data << uint8(CHAT_MSG_SYSTEM);
                data << int32(LANG_UNIVERSAL);
                data << player->GetGUID();
                data << uint32(0);
                data << uint32(batchMessage.length() + 1);
                data << batchMessage;
                data << uint8(0);

                player->GetSession()->SendPacket(&data);

                // 计算这批包含多少个槽位
                int slotCount = std::count(batches[i].begin(), batches[i].end(), ';');
                if (!batches[i].empty() && batches[i].back() != ';')
                {
                    slotCount++; // 最后一个槽位可能没有分号
                }

            }

            return;
        }
    }

    // 其他类型的消息，截断发送并警告
    LOG_ERROR("module.runesystem", ModuleOldFormat("SendAddonResponse: Message too long (%s bytes) and cannot be split, truncating", message.length()).c_str());

    std::string truncated = message.substr(0, 250);

    WorldPacket data(SMSG_MESSAGECHAT);
    data << uint8(CHAT_MSG_SYSTEM);
    data << int32(LANG_UNIVERSAL);
    data << player->GetGUID();
    data << uint32(0);
    data << uint32(truncated.length() + 1);
    data << truncated;
    data << uint8(0);

    player->GetSession()->SendPacket(&data);
}

// ============================================================
// 辅助函数：发送RUNESLOT响应给客户端
// 使用系统消息格式
// ============================================================
void SendRuneSlotResponse(Player* player, const std::string& message)
{
    if (!player || !player->GetSession())
        return;

    // 直接发送消息内容，使用系统消息格式
    WorldPacket data(SMSG_MESSAGECHAT);
    data << uint8(CHAT_MSG_SYSTEM);
    data << int32(LANG_UNIVERSAL);
    data << player->GetGUID();
    data << uint32(0);
    data << uint32(message.length() + 1);
    data << message;
    data << uint8(0);

    player->GetSession()->SendPacket(&data);
}

// ============================================================
// 辅助函数：处理RUNESLOT前缀的命令（符文插槽提示框插件）
// ============================================================
void HandleRuneSlotCommand(Player* player, const std::string& command)
{
    if (!player)
    {
        LOG_ERROR("module.runesystem", "HandleRuneSlotCommand: player is NULL!");
        return;
    }

    if (command.find("符文查询槽位 ") == 0)
    {
        // 查询物品符文槽位信息 - 返回格式: [RUNEDATA]itemGuid:totalSlots,filledSlots;slotData
        // slotData格式: slotId,runeItemId,runeType,runeName,runeEffectDesc;...
        // "符文查询槽位 " = 19字节（6个中文字符×3字节 + 1个空格）
        const size_t prefixLen = 19;

        if (command.length() <= prefixLen)
        {
            LOG_ERROR("module.runesystem", ModuleOldFormat("HandleRuneSlotCommand: Command too short! Length = %s, expected > %s", command.length(), prefixLen).c_str());
            SendRuneSlotResponse(player, "[RUNEDATA]0:0,0;");
            return;
        }

        std::string itemGuidStr = command.substr(prefixLen); // 去掉 "符文查询槽位 "

        // 安全地转换字符串为数字
        uint32 itemGuid = 0;
        try
        {
            itemGuid = std::stoul(itemGuidStr);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("module.runesystem", ModuleOldFormat("HandleRuneSlotCommand: Failed to parse itemGuid '%s': %s", itemGuidStr, e.what()).c_str());
            SendRuneSlotResponse(player, "[RUNEDATA]0:0,0;");
            return;
        }

        // 【安全修复】guid 所有权校验：原实现按客户端提供的任意 itemGuid 查库并回发数据，
        // 可被用于枚举他人物品符文信息 + 按任意 guid 触发同步查库
        if (!player->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(itemGuid)))
        {
            SendRuneSlotResponse(player, "[RUNEDATA]0:0,0;");
            return;
        }

        // 内存即权威：登录时已批量加载，仅缓存未命中时回源查库
        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);
        if (!storage)
        {
            sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);
            storage = sRuneSystem->GetItemRuneStorage(itemGuid);
        }

        std::ostringstream oss;
        oss << "[RUNEDATA]" << itemGuid << ":";

        if (!storage)
        {
            oss << "0,0";
        }
        else
        {
            auto slotIds = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
            auto runeIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

            uint32 totalSlots = static_cast<uint32>(slotIds.size());
            uint32 filledSlots = 0;

            // 计算已填充数量
            for (size_t i = 0; i < runeIds.size(); ++i)
            {
                if (runeIds[i] > 0)
                {
                    filledSlots++;
                }
            }

            oss << totalSlots << "," << filledSlots;

            // 添加所有槽位的符文类型信息（包括空槽位）
            // 格式：totalSlots,filledSlots,type1,type2,...,type20
            // type为0表示空槽位，1-4表示不同颜色的符文
            for (size_t i = 0; i < slotIds.size(); ++i)
            {
                uint32 runeType = 0;  // 默认为空

                if (i < runeIds.size() && runeIds[i] > 0)
                {
                    // 有符文，获取符文类型
                    const CustomRuneInfo* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeIds[i]);
                    if (runeInfo && runeInfo->runeSlot >= 1 && runeInfo->runeSlot <= 4)
                    {
                        runeType = runeInfo->runeSlot;  // 使用符文的槽位类型（1-4）
                    }
                    else
                    {
                        runeType = 1;  // 默认类型1
                    }
                }

                oss << "," << runeType;
            }
        }

        SendRuneSlotResponse(player, oss.str());
    }
    else if (command.find("符文查询效果") == 0)
    {
        // 查询所有符文物品效果 - 数据启动时已预构建为分批消息（[RUNEITEMEFFECT]，单批 ≤200 字节），
        // 直接读缓存回发，不再每次请求对 _模板_物品符文 全表同步查询
        auto const& cachedBatches = sRuneSystem->GetCachedRuneEffectMessages();

        if (cachedBatches.empty())
        {
            // 没有数据，发送空响应
            SendRuneSlotResponse(player, "[RUNEITEMEFFECT]");
            return;
        }

        for (std::string const& batch : cachedBatches)
            SendRuneSlotResponse(player, batch);
    }
}

// ============================================================
// 辅助函数：处理客户端发来的ADDON命令
// ============================================================
void HandleRuneAddonCommand(Player* player, const std::string& command)
{
    if (!player)
        return;

    ChatHandler handler(player->GetSession());

    // 解析命令（格式：符文查询装备 或 符文 查询装备）
    // 统一处理，支持两种格式
    std::string normalizedCmd = command;

    // 如果以 "符文 " 开头（带空格），转换为无空格格式
    if (normalizedCmd.find("符文 ") == 0)
    {
        normalizedCmd = "符文" + normalizedCmd.substr(3); // 去掉"符文 "，替换为"符文"
    }

    if (normalizedCmd.find("符文查询装备") == 0)
    {
        // 查询装备符文 - 返回格式: [RUNEEQUIP]guid1:name1:totalSlots1:filledSlots1...
        // 支持分批发送
        std::vector<std::string> equipmentRecords;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            uint32 itemGuid = item->GetGUID().GetCounter();
            sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);
            const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);

            uint32 totalSlots = 0;
            uint32 filledSlots = 0;

            if (storage)
            {
                auto slotIds = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
                totalSlots = static_cast<uint32>(slotIds.size());
                filledSlots = RuneUtils::GetEquippedRuneCount(storage->runeIds);
            }

            if (totalSlots > 0)
            {
                ItemTemplate const* itemTemplate = item->GetTemplate();
                std::ostringstream recordOss;
                recordOss << itemGuid << ":"
                         << itemTemplate->Name1 << ":"
                         << totalSlots << ":"
                         << filledSlots << ":"
                         << static_cast<uint32>(slot) << ":"
                         << itemTemplate->Quality << ":"
                         << itemTemplate->DisplayInfoID;

                equipmentRecords.push_back(recordOss.str());
            }
        }

        // 分批发送装备记录
        if (equipmentRecords.empty())
        {
            SendAddonResponse(player, "[RUNEEQUIP]");
        }
        else
        {
            // 计算实际可用空间：255 - 前缀(11) - 消息标记(17) - 安全余量(10) = 217
            const size_t MAX_MESSAGE_SIZE = 217;
            std::vector<std::string> batches;
            std::string currentBatch;

            for (size_t i = 0; i < equipmentRecords.size(); ++i)
            {
                std::string separator = currentBatch.empty() ? "" : "|";
                std::string testBatch = currentBatch + separator + equipmentRecords[i];

                // 检查是否超过限制
                if (testBatch.length() > MAX_MESSAGE_SIZE && !currentBatch.empty())
                {
                    // 保存当前批次
                    batches.push_back(currentBatch);

                    // 开始新批次
                    currentBatch = equipmentRecords[i];
                }
                else
                {
                    // 添加到当前批次
                    currentBatch = testBatch;
                }
            }

            // 保存最后一批
            if (!currentBatch.empty())
            {
                batches.push_back(currentBatch);
            }

            // 发送所有批次
            for (size_t i = 0; i < batches.size(); ++i)
            {
                std::string message;
                if (i == 0)
                {
                    message = "[RUNEEQUIP]" + batches[i];
                }
                else
                {
                    message = "[RUNEEQUIP_MORE]" + batches[i];
                }

                SendAddonResponse(player, message);
            }

        }
    }
    else if (normalizedCmd.find("符文查询背包") == 0)
    {
        // 查询背包符文 - 返回格式: [RUNELIST]itemId1:name1:quality1:count1...
        // 支持分批发送，避免超过255字节限制
        std::map<uint32, uint32> runeItemCounts;

        // 扫描背包
        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        {
            Bag* pBag = player->GetBagByPos(bag);
            if (!pBag) continue;

            for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* item = pBag->GetItemByPos(slot);
                if (!item) continue;

                uint32 itemId = item->GetEntry();
                if (sRuneSystem->GetRuneInfoByItemId(itemId))
                    runeItemCounts[itemId] += item->GetCount();
            }
        }

        // 主背包
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item) continue;

            uint32 itemId = item->GetEntry();
            if (sRuneSystem->GetRuneInfoByItemId(itemId))
                runeItemCounts[itemId] += item->GetCount();
        }

        // 构建响应（分批发送）
        std::ostringstream oss;
        oss << "[RUNELIST]";
        bool first = true;
        int batchCount = 0;
        const int MAX_BATCH_SIZE = 220; // 留出安全余量

        for (auto const& [itemId, count] : runeItemCounts)
        {
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            if (!itemTemplate) continue;

            CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfoByItemId(itemId);
            if (!runeInfo) continue;

            // 构建单条记录
            std::ostringstream tempOss;
            if (!first) tempOss << "|";
            tempOss << itemId << ":"
                    << itemTemplate->Name1 << ":"
                    << itemTemplate->Quality << ":"
                    << count << ":"
                    << itemId << ":"
                    << runeInfo->effectType << ":"
                    << (runeInfo->effectDescription.empty() ? "无描述" : runeInfo->effectDescription);

            std::string tempStr = tempOss.str();

            // 检查是否会超过限制
            if (oss.str().length() + tempStr.length() > MAX_BATCH_SIZE)
            {
                // 发送当前批次
                SendAddonResponse(player, oss.str());

                // 重置为新批次（后续批次使用 _MORE 标记）
                oss.str("");
                oss.clear();
                oss << "[RUNELIST_MORE]";  // 后续批次使用不同的标记
                first = true;

                // 添加当前记录（不需要分隔符）
                oss << itemId << ":"
                    << itemTemplate->Name1 << ":"
                    << itemTemplate->Quality << ":"
                    << count << ":"
                    << itemId << ":"
                    << runeInfo->effectType << ":"
                    << (runeInfo->effectDescription.empty() ? "无描述" : runeInfo->effectDescription);
                first = false;
            }
            else
            {
                // 添加到当前批次
                oss << tempStr;
                first = false;
            }
        }

        // 发送最后一批
        std::string finalMessage = oss.str();
        if (finalMessage != "[RUNELIST]" && finalMessage != "[RUNELIST_MORE]")
        {
            SendAddonResponse(player, finalMessage);
        }

    }
    else if (normalizedCmd.find("符文信息 ") == 0)
    {
        // 查询物品详细信息 - 返回格式: [RUNEINFO]guid:totalSlots:filledSlots:slotData
        // "符文信息 " = 13字节（4个中文字符×3字节 + 1个空格）
        const size_t prefixLen = 13;

        if (normalizedCmd.length() <= prefixLen)
        {
            SendAddonResponse(player, "[RUNEINFO]0:0:0:");
            return;
        }

        std::string itemGuidStr = normalizedCmd.substr(prefixLen);

        uint32 itemGuid = 0;
        try
        {
            itemGuid = std::stoul(itemGuidStr);
        }
        catch (const std::exception& e)
        {
            SendAddonResponse(player, "[RUNEINFO]0:0:0:");
            return;
        }

        Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, itemGuid));
        if (!item)
        {
            SendAddonResponse(player, "[RUNEINFO]0:0:0:");
            return;
        }

        sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);
        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);

        std::ostringstream oss;
        oss << "[RUNEINFO]" << itemGuid << ":";

        if (!storage)
        {
            oss << "0:0:";
            SendAddonResponse(player, oss.str());
            return;
        }

        auto slotIds = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
        auto runeIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

        uint32 totalSlots = static_cast<uint32>(slotIds.size());
        uint32 filledSlots = RuneUtils::GetEquippedRuneCount(storage->runeIds);

        oss << totalSlots << ":" << filledSlots << ":";

        // 每个槽位数据都以分号结尾，方便分批发送
        for (size_t i = 0; i < slotIds.size(); ++i)
        {
            uint32 slotId = slotIds[i];
            uint32 runeId = (i < runeIds.size()) ? runeIds[i] : 0;

            oss << slotId << ",";

            if (runeId > 0)
            {
                CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeId);
                if (runeInfo)
                {
                    ItemTemplate const* runeItemTemplate = sObjectMgr->GetItemTemplate(runeInfo->itemId);
                    uint32 runeQuality = runeItemTemplate ? runeItemTemplate->Quality : 0;

                    oss << runeInfo->itemId << ","
                        << runeQuality << ","
                        << runeInfo->name << ","
                        << runeInfo->itemId << ","
                        << (runeInfo->effectDescription.empty() ? "" : runeInfo->effectDescription);
                }
                else
                {
                    oss << "0,0,未知符文,0,";
                }
            }
            else
            {
                oss << "0,0,,0,";
            }

            // 每个槽位后面都加分号，包括最后一个
            oss << ";";
        }

        SendAddonResponse(player, oss.str());
    }
    else if (normalizedCmd.find("符文装备 ") == 0)
    {
        // 镶嵌符文 - 解析参数：符文装备 itemGuid slotId runeItemId
        // "符文装备 " = 13字节（4个中文字符×3字节 + 1个空格）
        const size_t prefixLen = 13;

        if (normalizedCmd.length() <= prefixLen)
        {
            SendAddonResponse(player, "[RUNERESULT]0:命令格式错误");
            return;
        }

        std::string params = normalizedCmd.substr(prefixLen);

        std::istringstream iss(params);
        uint32 itemGuid = 0, slotId = 0, runeItemId = 0;

        if (!(iss >> itemGuid >> slotId >> runeItemId))
        {
            SendAddonResponse(player, "[RUNERESULT]0:参数解析失败");
            return;
        }

        Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, itemGuid));
        if (!item)
        {
            SendAddonResponse(player, "[RUNERESULT]0:找不到装备");
            return;
        }

        CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeItemId);
        if (!runeInfo)
        {
            SendAddonResponse(player, "[RUNERESULT]0:无效的符文物品ID");
            return;
        }

        if (!player->HasItemCount(runeItemId, 1))
        {
            SendAddonResponse(player, "[RUNERESULT]0:背包中没有该符文");
            return;
        }

        // 检查物品是否已装备
        bool isEquipped = item->IsEquipped();

        // 添加符文到装备（AddRuneToItem内部已经消耗物品，无需再次消耗）
        if (sRuneSystem->AddRuneToItem(player, item, runeItemId, slotId))
        {
            // 如果物品已装备，立即应用新镶嵌的符文效果
            if (isEquipped)
            {
                // 重新加载物品符文数据
                sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);

                // 只应用新镶嵌的符文效果
                const CustomRuneInfo* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeItemId);
                if (runeInfo)
                {
                    RuneEffectHandler::ApplyRuneEffects(player, *runeInfo);
                    LOG_INFO("rune.player", ModuleOldFormat("玩家 %s 在已装备的物品 %s 上镶嵌符文 %s，立即应用效果", player->GetGUID().GetCounter(), itemGuid, runeItemId).c_str());
                }
            }

            SendAddonResponse(player, "[RUNERESULT]1:符文镶嵌成功");
        }
        else
        {
            SendAddonResponse(player, "[RUNERESULT]0:镶嵌失败,槽位可能已被占用");
        }
    }
    else if (normalizedCmd.find("符文卸载 ") == 0)
    {
        // 卸载符文 - 解析参数：符文卸载 itemGuid slotId
        // "符文卸载 " = 13字节（4个中文字符×3字节 + 1个空格）
        const size_t prefixLen = 13;

        if (normalizedCmd.length() <= prefixLen)
        {
            SendAddonResponse(player, "[RUNERESULT]0:命令格式错误");
            return;
        }

        std::string params = normalizedCmd.substr(prefixLen);

        std::istringstream iss(params);
        uint32 itemGuid = 0, slotId = 0;

        if (!(iss >> itemGuid >> slotId))
        {
            SendAddonResponse(player, "[RUNERESULT]0:参数解析失败");
            return;
        }

        Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, itemGuid));
        if (!item)
        {
            SendAddonResponse(player, "[RUNERESULT]0:找不到装备");
            return;
        }

        // 检查物品是否已装备
        bool isEquipped = item->IsEquipped();

        // 如果物品已装备，先获取要卸载的符文信息，以便移除效果
        uint32 runeIdToRemove = 0;
        if (isEquipped)
        {
            const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);
            if (storage)
            {
                auto slotIds = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
                auto runeIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

                for (size_t i = 0; i < slotIds.size(); ++i)
                {
                    if (slotIds[i] == slotId && i < runeIds.size())
                    {
                        runeIdToRemove = runeIds[i];
                        break;
                    }
                }
            }
        }

        if (sRuneSystem->RemoveRuneFromItem(player, item, slotId))
        {
            // 如果物品已装备且有符文被卸载，移除该符文的效果
            if (isEquipped && runeIdToRemove > 0)
            {
                const CustomRuneInfo* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeIdToRemove);
                if (runeInfo)
                {
                    RuneEffectHandler::RemoveRuneEffects(player, *runeInfo);
                    LOG_INFO("rune.player", ModuleOldFormat("玩家 %s 从已装备的物品 %s 卸载符文 %s，移除效果", player->GetGUID().GetCounter(), itemGuid, runeIdToRemove).c_str());
                }
            }

            SendAddonResponse(player, "[RUNERESULT]1:符文卸载成功");
        }
        else
        {
            SendAddonResponse(player, "[RUNERESULT]0:卸载失败,槽位可能为空");
        }
    }
    else if (normalizedCmd.find("符文查询槽位 ") == 0)
    {
        // 查询物品符文槽位信息 - 返回格式: [RUNEDATA]itemGuid:totalSlots,filledSlots,runeTypes...
        // "符文查询槽位 " = 19字节（6个中文字符×3字节 + 1个空格）
        const size_t prefixLen = 19;

        if (normalizedCmd.length() <= prefixLen)
        {
            SendAddonResponse(player, "[RUNEDATA]0:0,0");
            return;
        }

        std::string itemGuidStr = normalizedCmd.substr(prefixLen); // 去掉 "符文查询槽位 "

        // 安全地转换字符串为数字
        uint32 itemGuid = 0;
        try
        {
            itemGuid = std::stoul(itemGuidStr);
        }
        catch (const std::exception& e)
        {
            SendAddonResponse(player, "[RUNEDATA]0:0,0");
            return;
        }

        // 【安全修复】guid 所有权校验（同 HandleRuneSlotCommand）
        if (!player->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(itemGuid)))
        {
            SendAddonResponse(player, "[RUNEDATA]0:0,0");
            return;
        }

        // 直接使用itemGuid加载数据（不需要查找Item对象）
        sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);
        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);

        std::ostringstream oss;
        oss << "[RUNEDATA]" << itemGuid << ":";

        if (!storage)
        {
            oss << "0,0";
        }
        else
        {
            auto slotIds = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
            auto runeIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

            uint32 totalSlots = static_cast<uint32>(slotIds.size());
            uint32 filledSlots = 0;

            // 计算已填充数量
            for (size_t i = 0; i < runeIds.size(); ++i)
            {
                if (runeIds[i] > 0)
                {
                    filledSlots++;
                }
            }

            oss << totalSlots << "," << filledSlots;

            // 添加所有槽位的符文类型信息（包括空槽位）
            for (size_t i = 0; i < slotIds.size(); ++i)
            {
                uint32 runeType = 0;  // 默认为空

                if (i < runeIds.size() && runeIds[i] > 0)
                {
                    // 有符文，获取符文类型
                    const CustomRuneInfo* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeIds[i]);
                    if (runeInfo && runeInfo->runeSlot >= 1 && runeInfo->runeSlot <= 4)
                    {
                        runeType = runeInfo->runeSlot;
                    }
                    else
                    {
                        runeType = 1;  // 默认类型1
                    }
                }

                oss << "," << runeType;
            }

        }

        SendAddonResponse(player, oss.str());
    }
    else if (normalizedCmd.find("符文查询效果") == 0)
    {
        // 查询所有符文物品效果 - 分批发送，避免超过255字节限制
        QueryResult result = WorldDatabase.Query(
            "SELECT `物品ID`, `效果类型`, `效果描述` FROM `_模板_物品符文`"
        );

        if (result)
        {
            // 收集所有符文效果记录
            std::vector<std::string> effectRecords;

            do
            {
                Field* fields = result->Fetch();
                uint32 itemId = fields[0].Get<uint32>();
                std::string effectDescription = fields[2].Get<std::string>();

                std::ostringstream recordOss;
                recordOss << itemId << ":" << effectDescription;
                effectRecords.push_back(recordOss.str());

            } while (result->NextRow());

            // 分批发送
            const size_t MAX_MESSAGE_SIZE = 217; // 255 - 11(前缀) - 17(标记) - 10(余量)
            std::vector<std::string> batches;
            std::string currentBatch;

            for (size_t i = 0; i < effectRecords.size(); ++i)
            {
                std::string separator = currentBatch.empty() ? "" : "|";
                std::string testBatch = currentBatch + separator + effectRecords[i];

                if (testBatch.length() > MAX_MESSAGE_SIZE && !currentBatch.empty())
                {
                    batches.push_back(currentBatch);
                    currentBatch = effectRecords[i];
                }
                else
                {
                    currentBatch = testBatch;
                }
            }

            if (!currentBatch.empty())
            {
                batches.push_back(currentBatch);
            }

            // 发送所有批次
            for (size_t i = 0; i < batches.size(); ++i)
            {
                std::string message;
                if (i == 0)
                {
                    message = "[RUNEITEMEFFECT]" + batches[i];
                }
                else
                {
                    message = "[RUNEITEMEFFECT_MORE]" + batches[i];
                }

                SendAddonResponse(player, message);
            }
        }
        else
        {
            SendAddonResponse(player, "[RUNEITEMEFFECT]");
        }
    }
    else
    {
        SendAddonResponse(player, "[RUNERESULT]0:未知命令");
    }

    LOG_DEBUG("module.runesystem", ModuleOldFormat("HandleRuneAddonCommand finished for player %s", player->GetName()).c_str());
}

// 玩家事件处理脚本
class RunePlayerScript : public PlayerScript
{
public:
    RunePlayerScript() : PlayerScript("RunePlayerScript") { }

    // 玩家登录时
    void OnLogin(Player* player) override
    {
        auto loginStart = std::chrono::high_resolution_clock::now();

        if (!player)
            return;

        uint64 playerGuid = player->GetGUID().GetCounter();

        // 加载玩家的符文数据
        auto step1Start = std::chrono::high_resolution_clock::now();
        sRuneSystem->LoadRuneStorageFromDB(playerGuid);
        auto step1End = std::chrono::high_resolution_clock::now();
        auto step1Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step1End - step1Start).count();

        // 标记玩家刚登录（这个标记会在登录完成后的第一次装备事件中被移除）
        playersJustLoggedIn.insert(playerGuid);

        // 初始化玩家的装备记录
        auto step2Start = std::chrono::high_resolution_clock::now();
        playerEquippedItems[playerGuid].clear();

        // 记录当前已装备的物品
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (item)
            {
                uint32 itemGuid = item->GetGUID().GetCounter();
                playerEquippedItems[playerGuid][slot] = itemGuid;
            }
        }
        auto step2End = std::chrono::high_resolution_clock::now();
        auto step2Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step2End - step2Start).count();

        // 应用所有已装备物品的符文效果
        auto step3Start = std::chrono::high_resolution_clock::now();
        RuneApplyManager::ApplyAllPlayerRunes(player);
        auto step3End = std::chrono::high_resolution_clock::now();
        auto step3Duration = std::chrono::duration_cast<std::chrono::milliseconds>(step3End - step3Start).count();

        // 【性能优化】移除登录时的数据库查询，改为使用全局缓存
        // 原代码：每个玩家登录都执行一次全表查询 "SELECT * FROM _模板_物品符文"
        // 问题：100人登录 = 100次全表查询 → 数据库卡死
        //
        // 新策略：使用 RuneSystem_PlayerScript::OnLogin 中的缓存版本
        // 该版本会延迟2秒调用 sRuneSystem->SendRuneEffectsToPlayer(player)
        // 直接使用预加载的全局缓存，无需查询数据库
        //
        // 注意：不要在此处重复发送符文效果数据，RuneSystem_PlayerScript 已经处理
        //
        // 旧代码已禁用（共64行代码）- 保留注释供参考
        /*
        QueryResult result = WorldDatabase.Query(
            "SELECT `物品ID`, `效果类型`, `效果描述` FROM `_模板_物品符文`"
        );

        if (result)
        {
            std::ostringstream oss;
            oss << "[RUNEITEMEFFECT]";
            bool first = true;
            int batchCount = 0;
            const int MAX_BATCH_SIZE = 220;

            do
            {
                Field* fields = result->Fetch();
                uint32 itemId = fields[0].Get<uint32>();
                uint32 effectType = fields[1].Get<uint32>();
                std::string effectDescription = fields[2].Get<std::string>();

                std::ostringstream tempOss;
                if (!first) tempOss << "|";
                tempOss << itemId << ":" << effectDescription;

                std::string tempStr = tempOss.str();

                if (oss.str().length() + tempStr.length() > MAX_BATCH_SIZE)
                {
                    SendRuneSlotResponse(player, oss.str());
                    LOG_DEBUG("module.runesystem", "OnLogin: Sent batch {} with {} bytes",
                              ++batchCount, oss.str().length());

                    oss.str("");
                    oss.clear();
                    oss << "[RUNEITEMEFFECT]";
                    first = true;

                    oss << itemId << ":" << effectDescription;
                    first = false;
                }
                else
                {
                    oss << tempStr;
                    first = false;
                }

            } while (result->NextRow());

            if (oss.str() != "[RUNEITEMEFFECT]")
            {
                SendRuneSlotResponse(player, oss.str());
                LOG_DEBUG("module.runesystem", "OnLogin: Sent final batch {} with {} bytes",
                          ++batchCount, oss.str().length());
            }

            LOG_DEBUG("module.runesystem", "OnLogin: Sent {} batches of rune effect data to player {}",
                      batchCount, player->GetName());
        }
        */

        auto loginEnd = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loginEnd - loginStart).count();
    }

    // 玩家登出时
    void OnLogout(Player* player) override
    {
        if (!player)
            return;

        uint64 playerGuid = player->GetGUID().GetCounter();

        // 移除所有符文效果
        RuneApplyManager::RemoveAllPlayerRunes(player);

        // 清理玩家的装备记录
        playerEquippedItems.erase(playerGuid);
        RuneCombat::InvalidateSpellBonusCache(static_cast<uint32>(playerGuid));

        // 清理登录标记（以防万一）
        playersJustLoggedIn.erase(playerGuid);

        LOG_DEBUG("rune.player", ModuleOldFormat("玩家 %s 登出，已移除符文效果", playerGuid).c_str());
    }

    // 存储每个玩家每个装备槽位当前装备的物品GUID
    std::map<uint64, std::map<uint8, uint32>> playerEquippedItems;

    // 标记玩家是否刚登录（避免重复应用符文效果）
    std::set<uint64> playersJustLoggedIn;

    // 玩家装备物品时
    void OnEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override
    {
        if (!player || !item)
            return;

        uint64 playerGuid = player->GetGUID().GetCounter();
        uint32 itemGuid = item->GetGUID().GetCounter();
        uint32 itemEntry = item->GetEntry();

        // 检查玩家是否已初始化装备记录
        // 如果没有初始化，说明是登录过程中的装备事件，应该跳过
        if (playerEquippedItems.find(playerGuid) == playerEquippedItems.end())
        {
            return;
        }

        // 如果玩家刚登录，跳过处理（OnLogin已经处理过了）
        // 但是要移除登录标记，确保后续的装备事件能正常处理
        if (playersJustLoggedIn.find(playerGuid) != playersJustLoggedIn.end())
        {
            playersJustLoggedIn.erase(playerGuid);
            return;
        }

        // 检查该槽位是否之前有装备物品
        auto& slots = playerEquippedItems[playerGuid];
        if (slots.find(slot) != slots.end())
        {
            uint32 oldItemGuid = slots[slot];

            // 如果是同一个物品，跳过处理（避免重复应用）
            if (oldItemGuid == itemGuid)
            {
                return;
            }

            // 移除旧物品的符文效果
            RuneApplyManager::RemoveItemRunes(player, oldItemGuid);
        }

        // 加载新物品的符文数据
        sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);

        // 检查物品是否有符文
        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);
        if (!storage)
        {
            // 即使没有符文，也要记录这个物品
            playerEquippedItems[playerGuid][slot] = itemGuid;
            return;
        }

        // 应用物品的符文效果
        RuneApplyManager::ApplyItemRunes(player, itemGuid);

        // 记录当前槽位装备的物品
        playerEquippedItems[playerGuid][slot] = itemGuid;
    }

    // 玩家设置可见物品槽位时（包括装备和卸载）
    void OnAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* item) override
    {
        if (!player)
            return;

        // 检查是否是装备槽位
        if (slot < EQUIPMENT_SLOT_START || slot >= EQUIPMENT_SLOT_END)
            return;

        uint64 playerGuid = player->GetGUID().GetCounter();

        // 只处理卸载装备的情况（item为nullptr）
        if (item == nullptr)
        {
            // 从记录中查找之前在这个槽位的物品
            if (playerEquippedItems.find(playerGuid) != playerEquippedItems.end())
            {
                auto& slots = playerEquippedItems[playerGuid];
                if (slots.find(slot) != slots.end())
                {
                    uint32 oldItemGuid = slots[slot];

                    // 移除旧物品的符文效果
                    RuneApplyManager::RemoveItemRunes(player, oldItemGuid);

                    // 从记录中移除
                    slots.erase(slot);
                }
            }
        }
    }

    // 注意：拾取/制造产生的物品是全新 GUID，数据库中不可能存在其符文记录，
    // 因此不再挂 OnLootItem/OnCreateItem 去查库（此前每拾取一件物品就同步查询一次）。
    // 已有符文的物品在登录时通过 LoadRuneStorageFromDB 一次性加载。

    // 玩家删除时
    void OnDeleteFromDB(CharacterDatabaseTransaction /*trans*/, uint32 /*guid*/) override
    {
        // 清理角色的符文数据可以在这里处理
        // 但通常由数据库的级联删除处理
    }

    // 防止重复处理的消息缓存
    std::unordered_map<uint64, std::pair<std::string, uint32>> lastProcessedMessages;
    const uint32 MESSAGE_CACHE_TIMEOUT = 1000; // 1秒超时

    // 监听聊天消息（用于ADDON消息通道）
    // 注意：必须使用带receiver参数的签名，才能正确接收ADDON消息
    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        // 防止重复处理相同消息
        uint64 playerGuid = player->GetGUID().GetCounter();
        uint32 currentTime = getMSTime();

        auto it = lastProcessedMessages.find(playerGuid);
        if (it != lastProcessedMessages.end())
        {
            // 检查是否是相同消息且在超时时间内
            if (it->second.first == msg && (currentTime - it->second.second) < MESSAGE_CACHE_TIMEOUT)
            {
                LOG_DEBUG("module.runesystem", ModuleOldFormat("OnChat: 忽略重复消息 - Player: %s, Message: '%s'", player->GetName(), msg).c_str());
                return;
            }
        }

        // 更新缓存
        lastProcessedMessages[playerGuid] = std::make_pair(msg, currentTime);

        // 清理过期缓存
        for (auto iter = lastProcessedMessages.begin(); iter != lastProcessedMessages.end();)
        {
            if (currentTime - iter->second.second > MESSAGE_CACHE_TIMEOUT)
            {
                iter = lastProcessedMessages.erase(iter);
            }
            else
            {
                ++iter;
            }
        }

        // 先处理GUILD频道的ADDON消息
        if (type == CHAT_MSG_GUILD && lang == LANG_ADDON)
        {
            std::string command;
            if (msg.find(ADDON_PREFIX) == 0)
            {
                size_t prefixLen = ADDON_PREFIX.length();
                if (prefixLen < msg.length())
                {
                    char separator = msg[prefixLen];
                    if (separator == '\t' || separator == ':' || separator == '\0' || separator == ' ')
                    {
                        command = msg.substr(prefixLen + 1);
                    }
                    else
                    {
                        command = msg.substr(prefixLen);
                    }
                }

                if (!command.empty())
                {
                    HandleRuneAddonCommand(player, command);
                }
            }
            else if (msg.find(ADDON_PREFIX_RUNESLOT) == 0)
            {
                size_t prefixLen = ADDON_PREFIX_RUNESLOT.length();
                if (prefixLen < msg.length())
                {
                    char separator = msg[prefixLen];
                    if (separator == '\t' || separator == ':' || separator == '\0' || separator == ' ')
                    {
                        command = msg.substr(prefixLen + 1);
                    }
                    else
                    {
                        command = msg.substr(prefixLen);
                    }
                }

                if (!command.empty())
                {
                    HandleRuneSlotCommand(player, command);
                }
            }

            return;
        }

        if (!player)
        {
            LOG_ERROR("module.runesystem", "OnChat: player is NULL!");
            return;
        }

        // 只处理ADDON消息类型（WHISPER + LANG_ADDON）
        if (type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
        {
            return;
        }

        if (msg.find(ADDON_PREFIX) == 0)
        {
            std::string command;
            size_t prefixLen = ADDON_PREFIX.length();

            if (prefixLen < msg.length())
            {
                char separator = msg[prefixLen];
                if (separator == '\t' || separator == ':' || separator == '\0' || separator == ' ')
                {
                    command = msg.substr(prefixLen + 1);
                }
                else
                {
                    command = msg.substr(prefixLen);
                }
            }

            if (!command.empty())
            {
                HandleRuneAddonCommand(player, command);
            }
        }
        else if (msg.find(ADDON_PREFIX_RUNESLOT) == 0)
        {
            std::string command;
            size_t prefixLen = ADDON_PREFIX_RUNESLOT.length();

            if (prefixLen < msg.length())
            {
                char separator = msg[prefixLen];
                if (separator == '\t' || separator == ':' || separator == '\0' || separator == ' ')
                {
                    command = msg.substr(prefixLen + 1);
                }
                else
                {
                    command = msg.substr(prefixLen);
                }
            }

            if (!command.empty())
            {
                HandleRuneSlotCommand(player, command);
            }
        }
        else
        {
            LOG_DEBUG("module.runesystem", ModuleOldFormat("OnChat: Player %s - ADDON message but no matching prefix! Message: '%s'", player->GetName(), msg).c_str());
        }
    }
};

// 物品脚本 - 处理物品相关事件
class RuneItemScript : public ItemScript
{
public:
    RuneItemScript() : ItemScript("RuneItemScript") { }

    // 物品被使用时
    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        if (!player || !item)
            return false;

        // 检查是否是符文物品
        // 这里可以添加使用符文相关的逻辑

        return false;
    }

    // 物品被摧毁时
    bool OnRemove(Player* player, Item* item) override
    {
        if (!player || !item)
            return false;

        uint32 itemGuid = item->GetGUID().GetCounter();

        // 如果物品被摧毁，移除其符文效果
        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);
        if (storage && item->IsEquipped())
        {
            RuneApplyManager::RemoveItemRunes(player, itemGuid);
        }

        return false;
    }
};

// AllItemScript - 如果需要全局物品钩子可以在这里添加
// 注意：AllItemScript只有特定的事件钩子，不包含OnItemCreate
// OnCreateItem事件在PlayerScript中已经处理

void AddSC_rune_player_scripts()
{
    new RunePlayerScript();
}
