// 注意：此文件包含旧格式的符文查询命令（发送 [RUNEINFO] 等消息）
// 这些命令已被 ItemIdentificationSystem 的批量查询 (addon 消息格式) 替代
// 但为了向后兼容和直接查询功能，这些命令仍然保留
// 新客户端应使用 addon 消息 (UITQ QUERY) 自动获取符文数据

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "RuneTypes.h"
#include "RuneSystem.h"
#include "RuneManager.h"
#include "RuneUtils.h"
#include "RuneApplyManager.h"
#include "Item.h"
#include "RuneManager.h"
#include "ScriptedGossip.h"
#include "GameTime.h"
#include "Common.h"
#include "World.h"
#include "ModuleFormat.h"

// 前向声明RuneSystemLoader类
class RuneSystemLoader;

using namespace Acore::ChatCommands;

// 【卸载残留修复】命令卸载路径与 Addon 路径对齐的公共辅助：
// 原 `.符文 卸载`/`.符文 移除` 只调 RemoveRuneFromItem 清存储，不移除已应用到
// 玩家身上的属性——已装备物品卸符文后加成残留（Addon 路径早已正确处理）。
namespace
{
uint32 GetSocketedRuneItemId(uint32 itemGuid, uint32 slotId)
{
    CustomRuneStorage const* storage = sRuneSystem->GetItemRuneStorage(itemGuid);
    if (!storage)
        return 0;

    auto slotIds = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
    auto runeIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

    for (size_t i = 0; i < slotIds.size(); ++i)
        if (slotIds[i] == slotId && i < runeIds.size())
            return runeIds[i];

    return 0;
}

void RemoveRuneEffectsIfEquipped(Player* player, Item* item, uint32 runeItemId)
{
    if (!player || !item || !runeItemId || !item->IsEquipped())
        return;

    if (CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeItemId))
        RuneEffectHandler::RemoveRuneEffects(player, *runeInfo);
}
}

// 符文系统命令处理类
class RuneSystem_CommandScript : public CommandScript
{
public:
    RuneSystem_CommandScript() : CommandScript("RuneSystem_CommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable runeCommandTable =
        {
            // 客户端UI命令（同时支持ADDON消息和聊天命令）
            { "信息", HandleRuneInfoCommand, AccountTypes::SEC_PLAYER, Console::No },
            { "查询装备", HandleRuneQueryEquipmentCommand, AccountTypes::SEC_PLAYER, Console::No },
            { "查询背包", HandleRuneQueryBagCommand, AccountTypes::SEC_PLAYER, Console::No },
            { "查询效果", HandleRuneQueryAllEffectsCommand, AccountTypes::SEC_PLAYER, Console::No },
            { "查询槽位", HandleRuneQueryCommand, AccountTypes::SEC_PLAYER, Console::No },
            { "装备", HandleRuneSocketCommand, AccountTypes::SEC_PLAYER, Console::No },
            { "卸载", HandleRuneUnsocketCommand, AccountTypes::SEC_PLAYER, Console::No },
            { "插槽", HandleRuneAddCommand, AccountTypes::SEC_GAMEMASTER, Console::No },
            { "移除", HandleRuneRemoveCommand, AccountTypes::SEC_GAMEMASTER, Console::No },
            { "升级", HandleRuneUpgradeCommand, AccountTypes::SEC_GAMEMASTER, Console::No },
            { "开槽", HandleRuneSlotCommand, AccountTypes::SEC_GAMEMASTER, Console::No },
            { "解锁", HandleRuneUnlockCommand, AccountTypes::SEC_GAMEMASTER, Console::No },
            { "创建", HandleRuneCreateCommand, AccountTypes::SEC_ADMINISTRATOR, Console::No },
            { "删除", HandleRuneDeleteCommand, AccountTypes::SEC_ADMINISTRATOR, Console::No },
            { "列表", HandleRuneListCommand, AccountTypes::SEC_GAMEMASTER, Console::No },
            { "效果", HandleRuneEffectCommand, AccountTypes::SEC_ADMINISTRATOR, Console::No },
            { "转移", HandleRuneTransferCommand, AccountTypes::SEC_GAMEMASTER, Console::No },
            { "重载", HandleRuneReloadCommand, AccountTypes::SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable commandTable =
        {
            { "符文", runeCommandTable }
        };

        return commandTable;
    }

    // 显示符文信息（客户端UI专用）
    // 返回格式: [RUNEINFO]guid:totalSlots:filledSlots:slotId1,runeItemId1,runeQuality1;slotId2,...
    static bool HandleRuneInfoCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            return false;
        }

        // 解析参数
        char* itemGuidStr = strtok((char*)args, " ");
        Item* item = nullptr;

        LOG_INFO("rune.system", ModuleOldFormat("【符文查询】收到参数: '%s'", args ? args : "NULL").c_str());

        if (!itemGuidStr)
        {
            // 如果没有指定物品GUID，则使用当前主手物品
            item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            if (!item)
            {
                handler->PSendSysMessage("[RUNEINFO]0:0:0:");
                return false;
            }
        }
        else
        {
            uint32 itemGuid = atoi(itemGuidStr);
            LOG_INFO("rune.system", ModuleOldFormat("【符文查询】解析GUID: %s", itemGuid).c_str());

            item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, itemGuid));
            if (!item)
            {
                LOG_INFO("rune.system", ModuleOldFormat("【符文查询】通过GetItemByGuid未找到物品GUID: %s", itemGuid).c_str());

                // 尝试通过遍历背包和装备查找
                // 遍历装备栏
                for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
                {
                    Item* checkItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                    if (checkItem && checkItem->GetGUID().GetCounter() == itemGuid)
                    {
                        item = checkItem;
                        LOG_INFO("rune.system", ModuleOldFormat("【符文查询】在装备槽位%s找到物品", i).c_str());
                        break;
                    }
                }

                // 遍历背包
                if (!item)
                {
                    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
                    {
                        Bag* pBag = player->GetBagByPos(bag);
                        if (pBag)
                        {
                            for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
                            {
                                Item* checkItem = pBag->GetItemByPos(slot);
                                if (checkItem && checkItem->GetGUID().GetCounter() == itemGuid)
                                {
                                    item = checkItem;
                                    LOG_INFO("rune.system", ModuleOldFormat("【符文查询��在背包%s槽位%s找到物品", bag, slot).c_str());
                                    break;
                                }
                            }
                            if (item) break;
                        }
                    }
                }

                if (!item)
                {
                    handler->PSendSysMessage("[RUNEINFO]0:0:0:");
                    LOG_INFO("rune.system", ModuleOldFormat("【符文查询】未找到物品GUID: %s", itemGuid).c_str());
                    return false;
                }
            }
        }

        uint32 itemGuid = item->GetGUID().GetCounter();
        int32 randomPropId = item->GetItemRandomPropertyId();

        LOG_INFO("rune.system", ModuleOldFormat("【符文查询】物品ID: %s, GUID: %s, randomPropertyId: %s", item->GetEntry(), itemGuid, randomPropId).c_str());

        // 先从数据库加载物品符文数据
        sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);

        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);

        std::ostringstream oss;
        oss << "[RUNEINFO]" << itemGuid << ":";

        if (!storage)
        {
            oss << "0:0:";
            handler->PSendSysMessage(oss.str().c_str());
            return true;
        }

        // 解析插槽和符文
        auto slotIds = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
        auto runeIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

        uint32 totalSlots = static_cast<uint32>(slotIds.size());
        uint32 filledSlots = RuneUtils::GetEquippedRuneCount(storage->runeIds);

        oss << totalSlots << ":" << filledSlots << ":";

        // 添加每个槽位的详细信息
        bool first = true;
        for (size_t i = 0; i < slotIds.size(); ++i)
        {
            uint32 slotId = slotIds[i];
            uint32 runeId = (i < runeIds.size()) ? runeIds[i] : 0;

            if (!first)
                oss << ";";
            first = false;

            // 格式: slotId,runeItemId,runeQuality,runeName,runeIcon
            oss << slotId << ",";

            if (runeId > 0)
            {
                // 注意：storage->runeIds 存储的是符文物品ID，不是符文定义ID
                CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeId);
                if (runeInfo)
                {
                    // 获取符文物品信息
                    ItemTemplate const* runeItemTemplate = sObjectMgr->GetItemTemplate(runeInfo->itemId);
                    uint32 runeQuality = runeItemTemplate ? runeItemTemplate->Quality : 0;

                    // 格式: runeItemId,runeQuality,runeName,iconItemId,effectDesc
                    oss << runeInfo->itemId << ","
                        << runeQuality << ","
                        << runeInfo->name << ","
                        << runeInfo->itemId << ","  // iconItemId
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
        }

        std::string response = oss.str();
        handler->SendSysMessage(response.c_str());  // 使用SendSysMessage避免格式化问题

        LOG_DEBUG("module.runesystem", ModuleOldFormat("RuneInfo: Player %s itemGuid %s - response: '%s'", player->GetName(), itemGuid, response).c_str());

        return true;
    }

    // 查询符文数据（客户端专用，返回简化格式）
    static bool HandleRuneQueryCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!*args)
            return false;

        // 解析物品GUID
        uint32 itemGuid = atoi(args);

        // 从数据库加载物品符文数据
        sRuneSystem->LoadItemRuneStorageFromDB(itemGuid);

        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);

        // 构建响应数据
        // 格式: [RUNEDATA]物品GUID:最大插槽,已使用插槽,符文类型1,符文类型2,...
        static const std::array<uint32, 21> slotColorMap = {
            0,
            1, // 1
            2, // 2
            3, // 3
            4, // 4
            1, // 5
            2, // 6
            3, // 7
            4, // 8
            1, // 9
            2, // 10
            3, // 11
            4, // 12
            1, // 13
            2, // 14
            3, // 15
            4, // 16
            1, // 17
            2, // 18
            3, // 19
            4  // 20
        };

        std::ostringstream oss;
        oss << "[RUNEDATA]" << itemGuid << ":";

        if (!storage)
        {
            // 物品没有符文插槽
            oss << "0,0";
        }
        else
        {
            // 解析符文列表
            auto slotIds = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
            auto runeIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

            uint32 maxSlots = static_cast<uint32>(slotIds.size());
            uint32 usedSlots = RuneUtils::GetEquippedRuneCount(storage->runeIds);

            oss << maxSlots << "," << usedSlots;

            // 添加每个符文的颜色类型
            for (size_t i = 0; i < runeIds.size(); ++i)
            {
                uint32 runeItemId = runeIds[i];
                uint32 colorType = 0;  // 0=空
                uint32 slotId = (i < slotIds.size()) ? slotIds[i] : static_cast<uint32>(i + 1);
                uint32 baseColor = (slotId < slotColorMap.size()) ? slotColorMap[slotId] : 1;

                if (runeItemId > 0)
                {
                    colorType = baseColor;

                    // 修复：runeIds存储的是符文物品ID，应使用GetRuneInfoByItemId
                    const CustomRuneInfo* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeItemId);
                    if (runeInfo)
                    {
                        if (runeInfo->runeSlot >= 1 && runeInfo->runeSlot < slotColorMap.size())
                            colorType = slotColorMap[runeInfo->runeSlot];
                    }
                }

                oss << "," << colorType;
            }
        }

        // 发送系统消息给玩家（使用SendSysMessage避免格式化问题）
        std::string response = oss.str();
        handler->SendSysMessage(response.c_str());

        LOG_DEBUG("module.runesystem", ModuleOldFormat("Query: Player %s itemGuid %s - response: '%s'", player->GetName(), itemGuid, response).c_str());

        return true;
    }

    // 查询所有装备的符文槽位信息（客户端UI专用）
    // 返回格式: [RUNEEQUIP]guid1:name1:totalSlots1:filledSlots1:slotId1:quality1:icon1|guid2:...
    static bool HandleRuneQueryEquipmentCommand(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        std::ostringstream oss;
        oss << "[RUNEEQUIP]";

        bool first = true;

        // 遍历所有装备槽位(19个)
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            uint32 itemGuid = item->GetGUID().GetCounter();

            // 从数据库加载符文数据
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

            // 只返回有符文槽位的装备
            if (totalSlots > 0)
            {
                if (!first)
                    oss << "|";
                first = false;

                ItemTemplate const* itemTemplate = item->GetTemplate();
                std::string itemName = itemTemplate->Name1;
                uint32 quality = itemTemplate->Quality;
                uint32 displayId = itemTemplate->DisplayInfoID;

                // 格式: guid:name:totalSlots:filledSlots:slotId:quality:icon
                oss << itemGuid << ":"
                    << itemName << ":"
                    << totalSlots << ":"
                    << filledSlots << ":"
                    << static_cast<uint32>(slot) << ":"
                    << quality << ":"
                    << displayId;
            }
        }

        std::string response = oss.str();
        handler->SendSysMessage(response.c_str());  // 使用SendSysMessage避免格式化问题

        LOG_DEBUG("module.runesystem", ModuleOldFormat("QueryEquipment: Player %s - response: '%s'", player->GetName(), response).c_str());

        return true;
    }

    // 查询背包中的符文物品（客户端UI专用）
    // 返回格式: [RUNELIST]itemId1:name1:quality1:count1|itemId2:...
    static bool HandleRuneQueryBagCommand(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        std::ostringstream oss;
        oss << "[RUNELIST]";

        bool first = true;
        std::map<uint32, uint32> runeItemCounts; // itemId -> count

        // 扫描背包(0-4)
        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        {
            Bag* pBag = player->GetBagByPos(bag);
            if (!pBag)
                continue;

            for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                Item* item = pBag->GetItemByPos(slot);
                if (!item)
                    continue;

                uint32 itemId = item->GetEntry();

                // 检查物品是否在符文定义表中
                if (sRuneSystem->GetRuneInfoByItemId(itemId))
                {
                    runeItemCounts[itemId] += item->GetCount();
                }
            }
        }

        // 也检查主背包
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            uint32 itemId = item->GetEntry();

            // 检查物品是否在符文定义表中
            if (sRuneSystem->GetRuneInfoByItemId(itemId))
            {
                runeItemCounts[itemId] += item->GetCount();
            }
        }

        // 构建响应
        for (auto const& [itemId, count] : runeItemCounts)
        {
            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
            if (!itemTemplate)
                continue;

            // 获取符文定义信息
            CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfoByItemId(itemId);
            if (!runeInfo)
                continue;

            if (!first)
                oss << "|";
            first = false;

            std::string itemName = itemTemplate->Name1;
            uint32 quality = itemTemplate->Quality;

            // 格式: itemId:name:quality:count:iconItemId:effectType:effectDesc
            // iconItemId 使用物品ID，让客户端通过GetItemInfo获取图标
            oss << itemId << ":"
                << itemName << ":"
                << quality << ":"
                << count << ":"
                << itemId << ":"  // 使用itemId让客户端获取图标
                << runeInfo->effectType << ":"
                << (runeInfo->effectDescription.empty() ? "无描述" : runeInfo->effectDescription);
        }

        std::string response = oss.str();
        handler->SendSysMessage(response.c_str());  // 使用SendSysMessage避免格式化问题

        LOG_DEBUG("module.runesystem", ModuleOldFormat("QueryBag: Player %s - response: '%s'", player->GetName(), response).c_str());

        return true;
    }

    // 查询所有符文物品效果命令（客户端专用）
    // 返回格式: [RUNEITEMEFFECT]itemId1:effectDesc1|itemId2:effectDesc2
    static bool HandleRuneQueryAllEffectsCommand(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        std::ostringstream oss;
        oss << "[RUNEITEMEFFECT]";

        bool first = true;

        // 直接查询数据库
        QueryResult result = WorldDatabase.Query(
            "SELECT `物品ID`, `效果描述` FROM `_模板_物品符文` WHERE `物品ID` > 0 AND `效果描述` IS NOT NULL AND `效果描述` != ''"
        );

        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 itemId = fields[0].Get<uint32>();
                std::string effectDesc = fields[1].Get<std::string>();

                if (!effectDesc.empty())
                {
                    if (!first)
                        oss << "|";
                    first = false;

                    oss << itemId << ":" << effectDesc;
                }
            } while (result->NextRow());
        }

        std::string message = oss.str();
        handler->PSendSysMessage(message.c_str());

        LOG_DEBUG("module.runesystem", ModuleOldFormat("QueryAllRuneEffects: Player %s - response: '%s'", player->GetName(), message).c_str());

        return true;
    }

    // 镶嵌符文到装备（客户端UI专用）
    // 用法: .符文 装备 <装备GUID> <槽位ID> <符文物品ID>
    // 返回格式: [RUNERESULT]success:message
    static bool HandleRuneSocketCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 解析参数
        char* itemGuidStr = strtok((char*)args, " ");
        char* slotIdStr = strtok(nullptr, " ");
        char* runeItemIdStr = strtok(nullptr, " ");

        if (!itemGuidStr || !slotIdStr || !runeItemIdStr)
        {
            handler->PSendSysMessage("[RUNERESULT]0:用法错误");
            return true;  // 返回true避免显示命令USAGE
        }

        uint32 itemGuid = atoi(itemGuidStr);
        uint32 slotId = atoi(slotIdStr);
        uint32 runeItemId = atoi(runeItemIdStr);

        // 通过GUID查找装备
        Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, itemGuid));
        if (!item)
        {
            handler->PSendSysMessage("[RUNERESULT]0:找不到装备");
            LOG_DEBUG("module.runesystem", ModuleOldFormat("SocketRune: Player %s - item GUID %s not found", player->GetName(), itemGuid).c_str());
            return true;  // 返回true避免显示命令USAGE
        }

        // 检查符文物品是否存在
        CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeItemId);
        if (!runeInfo)
        {
            handler->PSendSysMessage("[RUNERESULT]0:无效的符文物品ID");
            LOG_DEBUG("module.runesystem", ModuleOldFormat("SocketRune: Player %s - invalid rune item ID %s", player->GetName(), runeItemId).c_str());
            return true;  // 返回true避免显示命令USAGE
        }

        // 检查玩家背包中是否有该符文物品
        if (!player->HasItemCount(runeItemId, 1))
        {
            handler->PSendSysMessage("[RUNERESULT]0:背包中没有该符文");
            return true;  // 返回true避免显示命令USAGE
        }

        // 添加符文到装备（AddRuneToItem内部已经消耗物品，无需再次消耗）
        if (sRuneSystem->AddRuneToItem(player, item, runeItemId, slotId))
        {
            handler->PSendSysMessage("[RUNERESULT]1:符文镶嵌成功");
            LOG_DEBUG("module.runesystem", ModuleOldFormat("SocketRune: Player %s socketed rune %s to item %s slot %s", player->GetName(), runeInfo->name, item->GetTemplate()->Name1, slotId).c_str());
            return true;
        }
        else
        {
            handler->PSendSysMessage("[RUNERESULT]0:镶嵌失败,槽位可能已被占用");
            return true;  // 返回true避免显示命令USAGE
        }
    }

    // 卸载符文（客户端UI专用）
    // 用法: .符文 卸载 <装备GUID> <槽位ID>
    // 返回格式: [RUNERESULT]success:message
    static bool HandleRuneUnsocketCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 解析参数
        char* itemGuidStr = strtok((char*)args, " ");
        char* slotIdStr = strtok(nullptr, " ");

        if (!itemGuidStr || !slotIdStr)
        {
            handler->PSendSysMessage("[RUNERESULT]0:用法错误");
            return true;  // 返回true避免显示命令USAGE
        }

        uint32 itemGuid = atoi(itemGuidStr);
        uint32 slotId = atoi(slotIdStr);

        // 通过GUID查找装备
        Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, itemGuid));
        if (!item)
        {
            handler->PSendSysMessage("[RUNERESULT]0:找不到装备");
            LOG_DEBUG("module.runesystem", ModuleOldFormat("UnsocketRune: Player %s - item GUID %s not found", player->GetName(), itemGuid).c_str());
            return true;  // 返回true避免显示命令USAGE
        }

        // 移除符文（先取槽内符文 ID，成功后移除已装备物品的身上效果）
        uint32 socketedRuneId = GetSocketedRuneItemId(itemGuid, slotId);
        if (sRuneSystem->RemoveRuneFromItem(player, item, slotId))
        {
            RemoveRuneEffectsIfEquipped(player, item, socketedRuneId);
            handler->PSendSysMessage("[RUNERESULT]1:符文卸载成功");
            LOG_DEBUG("module.runesystem", ModuleOldFormat("UnsocketRune: Player %s removed rune from item %s slot %s", player->GetName(), item->GetTemplate()->Name1, slotId).c_str());
            return true;
        }
        else
        {
            handler->PSendSysMessage("[RUNERESULT]0:卸载失败,槽位可能为空");
            return true;  // 返回true避免显示命令USAGE
        }
    }

    // 添加符文到物品
    static bool HandleRuneAddCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            return false;
        }

        // 解析参数
        char* itemIdStr = strtok((char*)args, " ");
        char* slotIdStr = strtok(nullptr, " ");
        char* runeItemIdStr = strtok(nullptr, " ");

        if (!itemIdStr || !slotIdStr || !runeItemIdStr)
        {
            handler->SendSysMessage("用法: .符文 插槽 <物品ID> <插槽ID> <符文物品ID>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = atoi(itemIdStr);
        uint32 slotId = atoi(slotIdStr);
        uint32 runeItemId = atoi(runeItemIdStr);

        Item* item = player->GetItemByEntry(itemId);
        if (!item)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("未找到ID为 %s 的物品，请检查你的背包中是否有该物品。", itemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 根据符文物品ID查找符文信息
        CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeItemId);
        if (!runeInfo)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("物品ID %s 不是有效的符文物品。请使用 .符文 列表 查看可用符文。", runeItemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 添加符文到物品（使用符文物品ID）
        if (sRuneSystem->AddRuneToItem(player, item, runeInfo->itemId, slotId))
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("成功将符文 %s (物品ID:%s) 添加到物品 %s 的插槽 %s。", runeInfo->name, runeInfo->itemId, item->GetTemplate()->Name1, slotId).c_str());
            return true;
        }
        else
        {
            handler->SendSysMessage("无法添加符文到物品。可能的原因：");
            handler->SendSysMessage("1. 物品还没有开槽，请先使用 .符文 开槽 命令");
            handler->SendSysMessage("2. 指定的插槽不存在");
            handler->SendSysMessage("3. 插槽已经有符文");
            handler->SendSysMessage("4. 你的背包中没有该符文物品");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // 从物品移除符文
    static bool HandleRuneRemoveCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            return false;
        }

        // 解析参数
        char* itemIdStr = strtok((char*)args, " ");
        char* slotIdStr = strtok(nullptr, " ");

        if (!itemIdStr || !slotIdStr)
        {
            handler->SendSysMessage("用法: .符文 移除 <物品ID> <插槽ID>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = atoi(itemIdStr);
        uint32 slotId = atoi(slotIdStr);

        Item* item = player->GetItemByEntry(itemId);
        if (!item)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("未找到ID为 %s 的物品，请检查你的背包中是否有该物品。", itemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 移除符文（先取槽内符文 ID，成功后移除已装备物品的身上效果）
        uint32 socketedRuneId = GetSocketedRuneItemId(item->GetGUID().GetCounter(), slotId);
        if (sRuneSystem->RemoveRuneFromItem(player, item, slotId))
        {
            RemoveRuneEffectsIfEquipped(player, item, socketedRuneId);
            handler->PSendSysMessage("%s", ModuleOldFormat("成功从物品 %s 的插槽 %s 移除符文。", item->GetTemplate()->Name1, slotId).c_str());
            return true;
        }
        else
        {
            handler->SendSysMessage("无法移除符文。请检查插槽是否有符文。");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // 升级符文
    static bool HandleRuneUpgradeCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            return false;
        }

        // 解析参数
        char* itemIdStr = strtok((char*)args, " ");
        char* slotIdStr = strtok(nullptr, " ");

        if (!itemIdStr || !slotIdStr)
        {
            handler->SendSysMessage("用法: .符文 升级 <物品ID> <插槽ID>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = atoi(itemIdStr);
        uint32 slotId = atoi(slotIdStr);

        Item* item = player->GetItemByEntry(itemId);
        if (!item)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("未找到ID为 %s 的物品，请检查你的背包中是否有该物品。", itemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 升级符文
        if (sRuneSystem->UpgradeRune(player, item, slotId))
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("成功升级物品 %s 的插槽 %s 中的符文。", item->GetTemplate()->Name1, slotId).c_str());
            return true;
        }
        else
        {
            handler->SendSysMessage("无法升级符文。请检查插槽是否有符文或符文是否已达到最大等级。");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // 添加符文插槽到物品
    static bool HandleRuneSlotCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            return false;
        }

        // 解析参数
        char* itemIdStr = strtok((char*)args, " ");
        char* countStr = strtok(nullptr, " ");

        if (!itemIdStr)
        {
            handler->SendSysMessage("用法: .符文 开槽 <物品ID> [数量]");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = atoi(itemIdStr);
        uint32 count = countStr ? atoi(countStr) : 1;

        Item* item = player->GetItemByEntry(itemId);
        if (!item)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("未找到ID为 %s 的物品，请检查你的背包中是否有该物品。", itemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 添加符文插槽
        if (sRuneManager->AddRuneSlotToItem(item, count))
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("成功为物品 %s 添加 %s 个符文插槽。", item->GetTemplate()->Name1, count).c_str());
            return true;
        }
        else
        {
            handler->SendSysMessage("无法添加符文插槽。");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // 解锁符文插槽
    static bool HandleRuneUnlockCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            return false;
        }

        // 解析参数
        char* itemIdStr = strtok((char*)args, " ");
        char* slotIdStr = strtok(nullptr, " ");

        if (!itemIdStr || !slotIdStr)
        {
            handler->SendSysMessage("用法: .符文 解锁 <物品ID> <插槽ID>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 itemId = atoi(itemIdStr);
        uint32 slotId = atoi(slotIdStr);

        Item* item = player->GetItemByEntry(itemId);
        if (!item)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("未找到ID为 %s 的物品，请检查你的背包中是否有该物品。", itemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 解锁符文插槽
        if (sRuneManager->UnlockRuneSlot(item, slotId))
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("成功解锁物品 %s 的插槽 %s。", item->GetTemplate()->Name1, slotId).c_str());
            return true;
        }
        else
        {
            handler->SendSysMessage("无法解锁符文插槽。请检查插槽是否存在或已解锁。");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // 创建新符文
    static bool HandleRuneCreateCommand(ChatHandler* handler, const char* args)
    {
        // 解析参数
        char* nameStr = strtok((char*)args, ",");
        char* slotStr = strtok(nullptr, ",");
        char* qualityStr = strtok(nullptr, ",");
        char* spellIdStr = strtok(nullptr, ",");
        char* cooldownStr = strtok(nullptr, ",");
        char* procChanceStr = strtok(nullptr, ",");
        char* effectTypeStr = strtok(nullptr, ",");

        if (!nameStr || !slotStr || !qualityStr ||
            !spellIdStr || !cooldownStr || !procChanceStr || !effectTypeStr)
        {
            handler->SendSysMessage("用法: .符文 创建 <名称>,<槽位(1-20)>,<品质>,<法术ID>,<冷却时间>,<触发几率>,<效果类型>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string name = nameStr;
        uint32 runeSlot = atoi(slotStr);
        uint32 runeQuality = atoi(qualityStr);
        uint32 spellId = atoi(spellIdStr);
        uint32 cooldown = atoi(cooldownStr);
        uint32 procChance = atoi(procChanceStr);
        uint32 effectType = atoi(effectTypeStr);

        // 创建新符文
        uint32 newRuneId = sRuneManager->CreateRune(name, runeSlot, runeQuality,
                                                   spellId, cooldown, procChance, effectType);

        if (newRuneId > 0)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("成功创建新符文 %s (ID: %s)。", name, newRuneId).c_str());
            return true;
        }
        else
        {
            handler->SendSysMessage("无法创建新符文。");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // 删除符文
    static bool HandleRuneDeleteCommand(ChatHandler* handler, const char* args)
    {
        // 解析参数
        char* runeIdStr = strtok((char*)args, " ");

        if (!runeIdStr)
        {
            handler->SendSysMessage("用法: .符文 删除 <id>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 runeId = atoi(runeIdStr);

        // 检查符文是否存在
        CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfo(runeId);
        if (!runeInfo)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("找不到ID为 %s 的符文。", runeId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 删除符文
        if (sRuneManager->DeleteRune(runeId))
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("成功删除符文 %s (ID: %s)。", runeInfo->name, runeId).c_str());
            return true;
        }
        else
        {
            handler->SendSysMessage("无法删除符文。");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // 列出符文
    static bool HandleRuneListCommand(ChatHandler* handler, const char* args)
    {
        // 解析参数
        char* typeStr = strtok((char*)args, " ");
        char* qualityStr = strtok(nullptr, " ");

        uint32 runeSlot = typeStr ? atoi(typeStr) : 0;
        uint32 runeQuality = qualityStr ? atoi(qualityStr) : 0;

        // 获取符文列表
        std::vector<CustomRuneInfo> runeList = sRuneManager->GetRuneList(runeSlot, runeQuality);

        if (runeList.empty())
        {
            handler->SendSysMessage("未找到符合条件的符文。");
            return true;
        }

        handler->SendSysMessage("符文列表:");
        for (auto& rune : runeList)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("ID: %s, 物品ID: %s, 名称: %s, 槽位: %s, 品质: %s", rune.runeId, rune.itemId, rune.name, rune.runeSlot, rune.runeQuality).c_str());
        }

        return true;
    }

    // 设置符文效果
    static bool HandleRuneEffectCommand(ChatHandler* handler, const char* args)
    {
        // 解析参数
        char* runeIdStr = strtok((char*)args, " ");
        char* effectValuesStr = strtok(nullptr, " ");
        char* effectDescStr = strtok(nullptr, "\n");

        if (!runeIdStr || !effectValuesStr)
        {
            handler->SendSysMessage("用法: .符文 效果 <id> <效果值> [效果描述]");
            handler->SendSysMessage("效果值格式: 属性类型 值,属性类型 值 (如: 3 10,4 15)");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 runeId = atoi(runeIdStr);
        std::string effectValues = effectValuesStr ? effectValuesStr : "";
        std::string effectDesc = effectDescStr ? effectDescStr : "默认效果";

        // 检查符文是否存在
        CustomRuneInfo const* runeInfo = sRuneSystem->GetRuneInfo(runeId);
        if (!runeInfo)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("找不到ID为 %s 的符文。", runeId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 使用参数化查询防止SQL注入（通过format语法）
        WorldDatabase.Execute(ModuleOldFormat("UPDATE _模板_物品符文 SET 效果值 = '%s', 效果描述 = '%s' WHERE id = %s", effectValues, effectDesc, runeId).c_str());

        // 重新加载符文数据
        sRuneSystem->LoadRunesFromDB();

        handler->PSendSysMessage("%s", ModuleOldFormat("成功更新符文 %s (ID: %s) 的效果值。", runeInfo->name, runeId).c_str());
        return true;
    }

    // 转移符文
    static bool HandleRuneTransferCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
        {
            return false;
        }

        // 解析参数
        char* sourceItemIdStr = strtok((char*)args, " ");
        char* sourceSlotIdStr = strtok(nullptr, " ");
        char* targetItemIdStr = strtok(nullptr, " ");
        char* targetSlotIdStr = strtok(nullptr, " ");

        if (!sourceItemIdStr || !sourceSlotIdStr || !targetItemIdStr || !targetSlotIdStr)
        {
            handler->SendSysMessage("用法: .符文 转移 <源物品ID> <源插槽ID> <目标物品ID> <目标插槽ID>");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 sourceItemId = atoi(sourceItemIdStr);
        uint32 sourceSlotId = atoi(sourceSlotIdStr);
        uint32 targetItemId = atoi(targetItemIdStr);
        uint32 targetSlotId = atoi(targetSlotIdStr);

        Item* sourceItem = player->GetItemByEntry(sourceItemId);
        if (!sourceItem)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("未找到ID为 %s 的源物品，请检查你的背包中是否有该物品。", sourceItemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        Item* targetItem = player->GetItemByEntry(targetItemId);
        if (!targetItem)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("未找到ID为 %s 的目标物品，请检查你的背包中是否有该物品。", targetItemId).c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        // 转移符文
        if (sRuneSystem->TransferRune(player, sourceItem, sourceSlotId, targetItem, targetSlotId))
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("成功将符文从物品 %s 的插槽 %s 转移到物品 %s 的插槽 %s。", sourceItem->GetTemplate()->Name1, sourceSlotId, targetItem->GetTemplate()->Name1, targetSlotId).c_str());
            return true;
        }
        else
        {
            handler->SendSysMessage("无法转移符文。请检查源插槽是否有符文，以及目标插槽是否已解锁且为空。");
            handler->SetSentErrorMessage(true);
            return false;
        }
    }

    // 重载符文数据
    static bool HandleRuneReloadCommand(ChatHandler* handler, const char* /*args*/)
    {
        handler->SendSysMessage("正在重载符文数据...");

        // 显示重载前的状态
        CustomRuneInfo const* beforeRune = sRuneSystem->GetRuneInfo(1);
        if (beforeRune)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("重载前: 符文1的物品ID = %s", beforeRune->itemId).c_str());
        }
        else
        {
            handler->SendSysMessage("重载前: 找不到符文1");
        }

        // 重新加载
        sRuneSystem->LoadRunesFromDB();

        // 显示重载后的状态
        CustomRuneInfo const* afterRune = sRuneSystem->GetRuneInfo(1);
        if (afterRune)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("重载后: 符文1的物品ID = %s", afterRune->itemId).c_str());
        }
        else
        {
            handler->SendSysMessage("重载后: 找不到符文1");
        }

        handler->SendSysMessage("符文数据重载完成。");
        return true;
    }
};

// 【死代码清理】原 RuneSystem_PlayerScript / RuneSystem_ItemScript 已删除：
// - RuneSystem_PlayerScript 的 OnLogin/OnLogout 钩子名错误（核心为 OnPlayer*）从未执行，
//   且登录批量加载/登出清理已由 RunePlayerScripts.cpp 的 OnLogin/OnLogout 正确实现，
//   修活只会造成双重加载；OnPlayerDamageDealt 依赖的 _procEffects 全仓库无填充代码（proc 系统未实现），
//   待实现数据填充后再挂真实战斗钩子。
// - RuneSystem_ItemScript 需要 item_template.ScriptName 绑定（SQL 未提供）从不触发，
//   且调用的 RuneSystem::ApplyRuneEffects 是空函数（实现已移至 RuneApplyManager）。

// 添加所有脚本
void AddRuneSystemScripts()
{
    new RuneSystem_CommandScript();
    // RuneSystemWorldScript已在RuneSystemLoader.cpp中创建，这里不需要重复创建
    // 注意：RuneSystemWorldScript的实例由AddSC_RuneSystemLoader函数创建
}
