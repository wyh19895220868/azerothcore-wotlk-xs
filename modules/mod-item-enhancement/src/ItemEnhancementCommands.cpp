#include "AsInt256.h"
#include "StringConvert.h"
#include "ItemEnhancement.h"
#include "ItemEnhancementMgr.h"
#include "../../../src/server/scripts/Custom/_Helper/Helper.h"
#include "ChatCommandHelpers.h"
#include "StringConvert.h"

#include "../../mod-item-sets/src/ItemSets.h"

#include <vector>

// 使用线程安全的 tokenize 函数
using Acore::Impl::ChatCommands::tokenize;

#define MODULE_ITEM_IDENTIFICATION_SYSTEM
#include "mod-item-identification-system/src/ItemIdentificationSystem.h"
#include "ModuleFormat.h"

using namespace Acore::ChatCommands;

namespace
{
bool HasNeedTemplate(uint32 requirementId)
{
    return !requirementId || NeedTempStore.find(requirementId) != NeedTempStore.end();
}

bool CheckNeedTemplate(Player* player, uint32 requirementId, bool showMessages)
{
    if (!requirementId)
        return true;

    if (!HasNeedTemplate(requirementId))
    {
        if (showMessages && player)
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("需求模板%s不存在，请检查 __模版需求总表。", requirementId).c_str());
        return false;
    }

    return showMessages ? sHelper->NotDeleteCheckNeed(player, requirementId) : sHelper->ShowCheckNeed(player, requirementId);
}

bool ConsumeNeedTemplate(Player* player, uint32 requirementId, std::string const& info)
{
    if (!requirementId)
        return true;

    if (!HasNeedTemplate(requirementId))
    {
        if (player)
            ChatHandler(player->GetSession()).PSendSysMessage("%s", ModuleOldFormat("需求模板%s不存在，请检查 __模版需求总表。", requirementId).c_str());
        return false;
    }

    return sHelper->CheckNeed(player, requirementId, info);
}
}

// 帮助信息
std::string GetHelpInfo()
{
    std::string help = "物品强化系统命令:\n";
    help += ".强化 帮助 - 显示帮助信息\n";
    help += ".强化 界面 - 打开物品强化界面\n";
    help += ".强化 强化 [装备栏位] - 强化指定栏位的装备\n";
    help += ".强化 信息 [装备栏位] - 查看指定栏位装备的强化信息\n";

    help += ".强化 更新UI <物品ID|装备栏位> - 发送物品强化数据到客户端UI\n";
    help += ".强化 同步guid <guid> - 强制同步指定GUID的装备链接数据\n";
    help += ".强化 检查guid <guid> - 检查指定GUID的装备链接同步状态\n";
    help += ".强化 同步背包 - 同步背包中所有强化物品的GUID\n";
    help += ".强化 测试需求 [需求ID] - 测试指定需求ID的检查和消耗\n";
    help += ".强化 清理随机属性 [确认] - 清除数据库中的随机属性ID以解决GUID冲突（GM命令）\n";
    help += ".强化 重载 - 重新加载物品强化系统配置和数据（GM命令）\n";
    return help;
}

// 获取装备栏位的物品
Item* GetItemBySlot(Player* player, uint8 slot)
{
    if (!player)
        return nullptr;

    // 检查栏位是否有效
    if (slot >= EQUIPMENT_SLOT_END)
        return nullptr;

    // 获取栏位上的物品
    return player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
}

// 这些函数已在 ItemEnhancement.cpp 中定义

// 命令处理类
class ItemEnhancement_CommandScript : public CommandScript
{
public:
    ItemEnhancement_CommandScript() : CommandScript("ItemEnhancement_CommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable enhancementCommandTable =
        {
            { "帮助",     HandleHelpCommand,     SEC_PLAYER,     Console::No },
            { "强化",     HandleEnhanceCommand,  SEC_PLAYER,     Console::No },
            { "物品",     HandleItemEnhanceCommand, SEC_PLAYER,  Console::No },
            { "信息",     HandleInfoCommand,     SEC_PLAYER,     Console::No },
            { "界面",     HandleOpenUICommand,   SEC_PLAYER,     Console::No },
            { "查询",     HandleQueryCommand,    SEC_PLAYER,     Console::No },
            { "更新UI",   HandleUpdateUICommand, SEC_PLAYER,     Console::No },
            { "同步guid", HandleSyncGuidCommand, SEC_PLAYER,     Console::No },
            { "检查guid", HandleCheckGuidCommand, SEC_PLAYER,     Console::No },
            { "同步背包", HandleSyncBagCommand,  SEC_PLAYER,     Console::No },
            { "调试",     HandleDebugCommand,    SEC_GAMEMASTER, Console::No },  // 【审计修复】调试命令改为GM权限
            { "测试需求", HandleTestRequirementCommand, SEC_GAMEMASTER, Console::No },  // 【审计修复】测试需求改为GM权限
            { "需求状态", HandleRequirementStatusCommand, SEC_PLAYER, Console::No },
            { "清理随机属性", HandleClearRandomPropertiesCommand, SEC_GAMEMASTER, Console::No },
            { "重载",     HandleReloadCommand,   SEC_GAMEMASTER, Console::Yes }
        };

        static ChatCommandTable commandTable =
        {
            { "强化", enhancementCommandTable }
        };

        return commandTable;
    }

    static bool HandleHelpCommand(ChatHandler* handler, const char* /*args*/)
    {
        if (!ItemEnhancementEnabled)
        {
           // handler->SendSysMessage("物品强化系统当前已禁用");
            return true;
        }

        handler->SendSysMessage(GetHelpInfo().c_str());
        return true;
    }

    static bool HandleEnhanceCommand(ChatHandler* handler, const char* args)
    {
        if (!ItemEnhancementEnabled)
        {
           // handler->SendSysMessage("物品强化系统当前已禁用");
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 【修复】使用线程安全的 tokenize 替代 strtok
        if (!args || !*args)
        {
            handler->SendSysMessage("用法: .强化 强化 [装备栏位]");
            handler->SendSysMessage("装备栏位: 0=头部, 1=项链, 2=肩部, 3=衬衣, 4=胸甲, 5=腰带, 6=腿部, 7=靴子, 8=护腕, 9=手套, 10=戒指1, 11=戒指2, 12=饰品1, 13=饰品2, 14=背部, 15=主手, 16=副手, 17=远程, 18=战袍");
            return true;
        }

        std::string_view argsView(args);
        auto [slotToken, rest] = tokenize(argsView);

        if (slotToken.empty())
        {
            handler->SendSysMessage("用法: .强化 强化 [装备栏位]");
            return true;
        }

        auto slotOpt = Acore::StringTo<uint8>(slotToken);
        if (!slotOpt)
        {
            handler->SendSysMessage("无效的装备栏位");
            return true;
        }

        uint8 slot = *slotOpt;
        Item* item = GetItemBySlot(player, slot);
        if (!item)
        {
           // handler->SendSysMessage("指定栏位没有装备物品");
            return true;
        }

        // 【安全修复】统一走 Mgr 路径：内含需求模板校验+消耗，禁止绕过消耗强化
        if (sItemEnhancementMgr->EnhanceItem(player, item))
        {
            handler->SendSysMessage("物品强化成功");
        }
        else
        {
            handler->SendSysMessage("物品强化失败");
        }

        return true;
    }

    static bool HandleItemEnhanceCommand(ChatHandler* handler, const char* args)
    {
        if (!ItemEnhancementEnabled)
        {
           // handler->SendSysMessage("物品强化系统当前已禁用");
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 【修复】使用线程安全的 tokenize 替代 strtok
        if (!args || !*args)
        {
            handler->SendSysMessage("用法: .强化 物品 [物品ID] [物品guid]");
            return true;
        }

        std::string_view argsView(args);
        auto [itemIdToken, rest1] = tokenize(argsView);
        auto [guidToken, rest2] = tokenize(rest1);

        if (itemIdToken.empty())
        {
            handler->SendSysMessage("用法: .强化 物品 [物品ID] [物品guid]");
            return true;
        }

        auto itemIdOpt = Acore::StringTo<uint32>(itemIdToken);
        if (!itemIdOpt || *itemIdOpt == 0)
        {
            handler->SendSysMessage("无效的物品ID");
            return true;
        }

        uint32 itemId = *itemIdOpt;
        uint32 itemGuid = 0;
        if (!guidToken.empty())
        {
            auto guidOpt = Acore::StringTo<uint32>(guidToken);
            if (guidOpt)
                itemGuid = *guidOpt;
        }

        // 添加调试信息
        // handler->PSendSysMessage("开始查找物品: ID={}, GUID={}", itemId, itemGuid);

        // 根据guid查找物品，如果没有guid则按ID查找
        Item* item = nullptr;
        if (itemGuid > 0)
        {
            // handler->PSendSysMessage("尝试按GUID查找物品: {}", itemGuid);
            item = FindPlayerItemByGuid(player, itemGuid);
            if (!item)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("未找到GUID为 %s 的物品", itemGuid).c_str());
                return true;
            }
            else
            {
                // handler->PSendSysMessage("按GUID查找成功");
                // 验证物品ID是否匹配
                if (item->GetEntry() != itemId)
                {
                    // handler->PSendSysMessage("物品ID不匹配：期望 {}, 实际 {}", itemId, item->GetEntry());
                    return true;
                }
            }
        }
        else
        {
            // handler->PSendSysMessage("GUID为0，直接按ID查找: {}", itemId);
            // 如果没有guid，按ID查找（可能找到多个，取第一个）
            item = FindPlayerItem(player, itemId);
            if (!item)
            {
                // handler->PSendSysMessage("按ID查找失败，物品不存在: {}", itemId);
                return true;
            }
            else
            {
                // handler->PSendSysMessage("按ID查找成功，物品GUID: {}", item->GetGUID().GetCounter());
            }
        }

        // 检查物品是否可以强化
        if (!CanEnhanceItem(item))
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("物品 %s 无法强化", item->GetTemplate()->Name1.c_str()).c_str());
            return true;
        }

        // 获取当前强化等级和下一级强化模板
        // 【安全修复】模板查找、需求校验与消耗、成功率判定统一在 Mgr::EnhanceItem 内完成，
        // 命令层不再单独查模板/消耗需求（避免双重扣费与绕过路径）

        // 执行强化
        if (sItemEnhancementMgr->EnhanceItem(player, item))
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("物品 %s 强化成功", item->GetTemplate()->Name1.c_str()).c_str());
        }
        else
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("物品 %s 强化失败", item->GetTemplate()->Name1.c_str()).c_str());
        }

        return true;
    }

    static bool HandleInfoCommand(ChatHandler* handler, const char* args)
    {
        if (!ItemEnhancementEnabled)
        {
           // handler->SendSysMessage("物品强化系统当前已禁用");
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 【修复】使用线程安全的 tokenize 替代 strtok
        if (!args || !*args)
        {
            handler->SendSysMessage("用法: .强化 信息 [装备栏位]");
            handler->SendSysMessage("装备栏位: 0=头部, 1=项链, 2=肩部, 3=衬衣, 4=胸甲, 5=腰带, 6=腿部, 7=靴子, 8=护腕, 9=手套, 10=戒指1, 11=戒指2, 12=饰品1, 13=饰品2, 14=背部, 15=主手, 16=副手, 17=远程, 18=战袍");
            return true;
        }

        std::string_view argsView(args);
        auto [slotToken, rest] = tokenize(argsView);

        if (slotToken.empty())
        {
            handler->SendSysMessage("用法: .强化 信息 [装备栏位]");
            return true;
        }

        auto slotOpt = Acore::StringTo<uint8>(slotToken);
        if (!slotOpt)
        {
            handler->SendSysMessage("无效的装备栏位");
            return true;
        }

        uint8 slot = *slotOpt;
        Item* item = GetItemBySlot(player, slot);
        if (!item)
        {
           // handler->SendSysMessage("指定栏位没有装备物品");
            return true;
        }

        ShowEnhancementInfo(player, item);
        return true;
    }

    // 处理打开UI界面命令: .强化 界面
    static bool HandleOpenUICommand(ChatHandler* handler, const char* /*args*/)
    {
        if (!ItemEnhancementEnabled)
        {
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 发送打开UI界面的消息给客户端
        // 客户端收到此消息后会调用 ItemEnhancement_ToggleMainFrame() 打开界面
        std::string message = "ITEMENHANCE|OPEN_UI";
        sItemEnhancementMgr->SendAddonMessage(player, message);

        return true;
    }


    static bool HandleTestRequirementCommand(ChatHandler* handler, const char* args)
    {
        if (!ItemEnhancementEnabled)
        {
           // handler->SendSysMessage("物品强化系统当前已禁用");
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 【修复】使用线程安全的 tokenize 替代 strtok
        if (!args || !*args)
        {
            handler->SendSysMessage("用法: .强化 测试需求 [需求ID]");
            return true;
        }

        std::string_view argsView(args);
        auto [requirementIdToken, rest] = tokenize(argsView);

        if (requirementIdToken.empty())
        {
            handler->SendSysMessage("用法: .强化 测试需求 [需求ID]");
            return true;
        }

        auto requirementIdOpt = Acore::StringTo<uint32>(requirementIdToken);
        if (!requirementIdOpt || *requirementIdOpt == 0)
        {
            handler->SendSysMessage("无效的需求ID");
            return true;
        }

        uint32 requirementId = *requirementIdOpt;
        handler->PSendSysMessage("%s", ModuleOldFormat("测试需求ID: %s", requirementId).c_str());

        if (!HasNeedTemplate(requirementId))
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("需求模板%s不存在，请检查 __模版需求总表。", requirementId).c_str());
            return true;
        }

        bool requirementMet = CheckNeedTemplate(player, requirementId, true);
        if (requirementMet)
        {
            handler->SendSysMessage("需求检查结果: 满足条件");
            handler->SendSysMessage("测试消耗需求...");
            handler->SendSysMessage(ConsumeNeedTemplate(player, requirementId, "物品强化测试需求消耗") ? "需求消耗成功" : "需求消耗失败");
        }
        else
        {
            handler->SendSysMessage("需求检查结果: 不满足条件");
        }

        return true;
    }

    static bool HandleRequirementStatusCommand(ChatHandler* handler, const char* /*args*/)
    {
        handler->SendSysMessage("=== 需求系统状态检查 ===");
        handler->SendSysMessage("需求系统: 使用旧核心 Helper::__模版需求总表");
        handler->PSendSysMessage("%s", ModuleOldFormat("已加载需求模板数量: %s", NeedTempStore.size()).c_str());
        handler->SendSysMessage("=== 检查完成 ===");
        return true;
    }

    static bool HandleDebugCommand(ChatHandler* handler, const char* /*args*/)
    {
        if (!ItemEnhancementEnabled)
        {
           // handler->SendSysMessage("物品强化系统当前已禁用");
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 调试命令：显示强化模板加载情况
        handler->SendSysMessage("=== 强化模板调试信息 ===");
        handler->PSendSysMessage("%s", ModuleOldFormat("已加载模板数量: %s", sItemEnhancementMgr->GetTemplateCount()).c_str());

        // 显示组0的前10个等级配置
        for (uint32 level = 1; level <= 10; ++level)
        {
            EnhancementTemplate const* tmpl = sItemEnhancementMgr->GetEnhancementTemplateByGroupAndLevel(0, level);
            if (tmpl)
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("组0等级%s: ID=%s, 成功率=%s%%, 属性值1=%s, 属性值2=%s", level, tmpl->id, tmpl->chance, Acore::ToString(tmpl->statValue1), Acore::ToString(tmpl->statValue2)).c_str());
            }
            else
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("组0等级%s: 未找到配置", level).c_str());
            }
        }
        return true;
    }

    static bool HandleUpdateUICommand(ChatHandler* handler, const char* args)
    {
        if (!ItemEnhancementEnabled)
        {
           // handler->SendSysMessage("物品强化系统当前已禁用");
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 解析参数 - 可以是物品ID或装备栏位，也可以是"物品ID guid [targetPlayerGuid]"
        if (!args || !*args)
        {
            handler->SendSysMessage("用法: .强化 更新UI <物品ID|装备栏位> [guid] [targetPlayerGuid]");
            handler->SendSysMessage("装备栏位: 0=头部, 1=项链, 2=肩部, 3=衬衣, 4=胸甲, 5=腰带, 6=腿部, 7=靴子, 8=护腕, 9=手套, 10=戒指1, 11=戒指2, 12=饰品1, 13=饰品2, 14=背部, 15=主手, 16=副手, 17=远程, 18=战袍");
            return true;
        }

        // 【修复】使用 tokenize 替代 istringstream + atoi
        std::string_view argsView(args);
        auto [firstToken, rest1] = tokenize(argsView);
        auto [secondToken, rest2] = tokenize(rest1);
        auto [thirdToken, rest3] = tokenize(rest2);

        auto itemIdOrSlotOpt = Acore::StringTo<uint32>(firstToken);
        if (!itemIdOrSlotOpt)
        {
            handler->SendSysMessage("无效的参数");
            return true;
        }

        uint32 itemIdOrSlot = *itemIdOrSlotOpt;
        uint32 requestedGuid = 0;
        if (!secondToken.empty())
        {
            auto guidOpt = Acore::StringTo<uint32>(secondToken);
            if (guidOpt)
                requestedGuid = *guidOpt;
        }
        Item* item = nullptr;

        // 如果参数小于19，认为是装备栏位
        if (itemIdOrSlot < EQUIPMENT_SLOT_END)
        {
            item = GetItemBySlot(player, (uint8)itemIdOrSlot);
            if (!item)
            {
               // handler->SendSysMessage("指定栏位没有装备物品");
                return true;
            }
        }
        else
        {
            // 如果提供了guid，优先通过guid查找
            if (requestedGuid > 0)
            {
                item = FindPlayerItemByGuid(player, requestedGuid);
                if (!item)
                {
                    // 尝试通过装备链接中的GUID查找物品
                  //  handler->PSendSysMessage("强化系统：直接查找GUID {} 失败，尝试通过装备链接GUID查找", requestedGuid);

                    // 遍历玩家所有装备，查找装备链接中包含该GUID的物品
                    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                    {
                        Item* equippedItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                        if (equippedItem)
                        {
                            int32 storedGuid = equippedItem->GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID);
                            if (storedGuid == static_cast<int32>(requestedGuid))
                            {
                                item = equippedItem;
                                uint32 actualGuid = item->GetGUID().GetCounter();
                               // handler->PSendSysMessage("强化系统：通过装备链接GUID找到物品，实际GUID: {}", actualGuid);
                                break;
                            }
                        }
                    }

                    // 如果还没找到，检查主背包
                    if (!item)
                    {
                        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
                        {
                            Item* bagItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                            if (bagItem)
                            {
                                int32 storedGuid = bagItem->GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID);
                                if (storedGuid == static_cast<int32>(requestedGuid))
                                {
                                    item = bagItem;
                                    uint32 actualGuid = item->GetGUID().GetCounter();
                                   // handler->PSendSysMessage("强化系统：在主背包中通过装备链接GUID找到物品，实际GUID: {}", actualGuid);
                                    break;
                                }
                            }
                        }
                    }

                    // 如果还没找到，检查其他背包
                    if (!item)
                    {
                        for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
                        {
                            Bag* pBag = player->GetBagByPos(bag);
                            if (pBag)
                            {
                                for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
                                {
                                    Item* bagItem = pBag->GetItemByPos(slot);
                                    if (bagItem)
                                    {
                                        int32 storedGuid = bagItem->GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID);
                                        if (storedGuid == static_cast<int32>(requestedGuid))
                                        {
                                            item = bagItem;
                                            uint32 actualGuid = item->GetGUID().GetCounter();
                                           // handler->PSendSysMessage("强化系统：在背包{}中通过装备链接GUID找到物品，实际GUID: {}", bag, actualGuid);
                                            break;
                                        }
                                    }
                                }
                                if (item) break;
                            }
                        }
                    }

                    if (!item)
                    {
                        // 如果在当前玩家找不到物品，直接从数据库查询（查看其他玩家装备）
                        EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(requestedGuid);

                        // 构建消息数据
                        std::string message = "ITEMENHANCE|UI_UPDATE|";
                        message += Acore::ToString(itemIdOrSlot) + "|";
                        message += Acore::ToString(requestedGuid) + "|";

                        if (record)
                        {
                            message += Acore::ToString(record->level) + "|";
                            message += record->statValues;
                        }
                        else
                        {
                            // 没有强化记录，发送默认值
                            message += "0|";
                        }

                        sItemEnhancementMgr->SendAddonMessage(player, message);

                        return true;
                    }
                }
                // 验证物品ID是否匹配
                if (item->GetEntry() != itemIdOrSlot)
                {
                    handler->PSendSysMessage("%s", ModuleOldFormat("物品ID不匹配: 期望 %s, 实际 %s", itemIdOrSlot, item->GetEntry()).c_str());
                    return true;
                }
            }
            else
            {
                // 否则认为是物品ID，在玩家背包中查找
                item = FindPlayerItem(player, itemIdOrSlot);
                if (!item)
                {
                    handler->PSendSysMessage("%s", ModuleOldFormat("未找到物品ID: %s", itemIdOrSlot).c_str());
                    return true;
                }
            }
        }

        // 使用现有的发送机制发送物品强化数据给客户端
        uint32 itemId = item->GetEntry();
        uint32 itemGuid = item->GetGUID().GetCounter();

        // 【已废弃】不再将GUID写入randomPropertyId字段，避免覆盖随机属性
        // 强化信息完全通过 物品强化_记录 表关联，无需同步GUID到装备链接

        // 获取强化记录
        EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);

        // 构建消息数据，使用与SendLoginCompleteMessage相同的格式
        std::string message = "ITEMENHANCE|UI_UPDATE|";
        message += Acore::ToString(itemId) + "|";
        message += Acore::ToString(itemGuid) + "|";

        if (record)
        {
            message += Acore::ToString(record->level) + "|";
            message += record->statValues;
        }
        else
        {
            // 没有强化记录，发送默认值
            message += "0|";
        }

        sItemEnhancementMgr->SendAddonMessage(player, message);

        // 获取装备的基础属性信息
        ItemTemplate const* itemTemplate = item->GetTemplate();
        std::string baseStats = "";

        // 收集装备的基础属性
        if (itemTemplate->Class == ITEM_CLASS_WEAPON)
        {
            // 武器属性
            if (itemTemplate->Damage[0].DamageMin > 0 || itemTemplate->Damage[0].DamageMax > 0)
            {
                baseStats += "伤害:" + Acore::ToString((itemTemplate->Damage[0].DamageMin + itemTemplate->Damage[0].DamageMax) / 2) + " ";
            }
        }
        else if (itemTemplate->Class == ITEM_CLASS_ARMOR)
        {
            // 护甲属性
            if (itemTemplate->AsArmor > 0)
            {
                baseStats += "护甲:" + Acore::ToString(itemTemplate->AsArmor) + " ";
            }
        }

        // 添加属性加成
        for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (itemTemplate->ItemStat[i].ItemStatType != 0 && itemTemplate->AsItemStatValue[i] != 0)
            {
                std::string statName = "";
                switch (itemTemplate->ItemStat[i].ItemStatType)
                {
                    case ITEM_MOD_STRENGTH: statName = "力量"; break;
                    case ITEM_MOD_AGILITY: statName = "敏捷"; break;
                    case ITEM_MOD_STAMINA: statName = "耐力"; break;
                    case ITEM_MOD_INTELLECT: statName = "智力"; break;
                    case ITEM_MOD_SPIRIT: statName = "精神"; break;
                    default: statName = "属性" + Acore::ToString(itemTemplate->ItemStat[i].ItemStatType); break;
                }
                baseStats += statName + ":" + Acore::ToString(itemTemplate->AsItemStatValue[i]) + " ";
            }
        }

        if (!baseStats.empty())
        {
            std::string baseStatsMessage = "ITEMENHANCE|BASE_STATS|";
            baseStatsMessage += Acore::ToString(itemId) + "|";
            baseStatsMessage += Acore::ToString(itemGuid) + "|";
            baseStatsMessage += baseStats;
            sItemEnhancementMgr->SendAddonMessage(player, baseStatsMessage);
        }

#ifdef MODULE_ITEM_IDENTIFICATION_SYSTEM
        // 同步通知统一物品提示框插件，使用批量 ALL_MODULE_DATA 数据刷新该物品的所有模块属性
        if (sItemIdentificationSystem)
            sItemIdentificationSystem->SendAllModuleDataAddon(player, itemId, itemGuid);
#endif

       // handler->PSendSysMessage("已发送物品 {} 的强化数据到客户端UI", item->GetTemplate()->Name1.c_str());
        return true;
    }

    static bool HandleSyncGuidCommand(ChatHandler* handler, const char* args)
    {
        if (!ItemEnhancementEnabled)
        {
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 【已废弃】不再需要同步GUID到randomPropertyId字段
        // 强化信息完全通过 物品强化_记录 表关联
        handler->SendSysMessage("强化系统：GUID同步功能已废弃，强化信息通过数据库记录关联");
        return true;
    }

    static bool HandleCheckGuidCommand(ChatHandler* handler, const char* args)
    {
        if (!ItemEnhancementEnabled)
        {
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 【重构】不再检查randomPropertyId字段，改为检查强化记录
        handler->SendSysMessage("=== 强化系统装备检查 ===");

        uint32 totalEquipment = 0;
        uint32 enhancedEquipment = 0;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            totalEquipment++;
            uint32 itemGuid = item->GetGUID().GetCounter();

            EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(itemGuid);

            if (record && record->level > 0)
            {
                enhancedEquipment++;
                handler->PSendSysMessage("%s", ModuleOldFormat("槽位 %s - %s (ID:%s) GUID:%s 强化等级:+%s", slot, item->GetTemplate()->Name1, item->GetEntry(), itemGuid, record->level).c_str());
                handler->PSendSysMessage("%s", ModuleOldFormat("  └─ 属性: '%s'", record->statValues).c_str());
            }
        }

        handler->PSendSysMessage("%s", ModuleOldFormat("检查完成: 总装备%s件, 强化装备%s件", totalEquipment, enhancedEquipment).c_str());
        return true;
    }

    static bool HandleSyncBagCommand(ChatHandler* handler, const char* /*args*/)
    {
        if (!ItemEnhancementEnabled)
        {
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 【已废弃】不再需要同步GUID到randomPropertyId字段
        // 强化信息完全通过 物品强化_记录 表关联
        handler->SendSysMessage("强化系统：背包GUID同步功能已废弃，强化信息通过数据库记录关联");
        return true;
    }

    static bool HandleClearRandomPropertiesCommand(ChatHandler* handler, const char* args)
    {
        if (!ItemEnhancementEnabled)
        {
            handler->SendSysMessage("物品强化系统当前已禁用");
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 解析参数
        std::string argStr = args ? args : "";
        bool confirmAction = (argStr == "确认");

        if (!confirmAction)
        {
            handler->SendSysMessage("=== 清理随机属性ID警告 ===");
            handler->SendSysMessage("此操作将清除数据库中所有装备的随机属性ID。");
            handler->SendSysMessage("【警告】这会导致所有装备的随机后缀/随机附魔永久丢失！");
            handler->SendSysMessage("");
            handler->SendSysMessage("【重要说明】强化系统已不再使用randomPropertyId字段存储GUID，");
            handler->SendSysMessage("因此通常不需要执行此命令。如果确实需要清理，请谨慎操作。");
            handler->SendSysMessage("");
            handler->SendSysMessage("执行前会自动备份现有数据到 item_instance_random_backup 表");
            handler->SendSysMessage("如果确认执行，请使用: .强化 清理随机属性 确认");
            return true;
        }

        // 执行清理操作
        handler->SendSysMessage("开始清理随机属性ID...");

        try
        {
            // 1. 创建备份表 - 【修复】使用INT而不是SMALLINT，避免数据溢出
            CharacterDatabase.Execute("CREATE TABLE IF NOT EXISTS item_instance_random_backup ("
                                    "guid INT UNSIGNED NOT NULL, "
                                    "randomPropertyId INT NOT NULL, "
                                    "backup_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                                    "PRIMARY KEY (guid))");

            // 2. 备份现有数据
            CharacterDatabase.Execute("INSERT IGNORE INTO item_instance_random_backup (guid, randomPropertyId) "
                                    "SELECT guid, randomPropertyId FROM item_instance WHERE randomPropertyId != 0");

            // 3. 统计要清理的数量
            QueryResult result = CharacterDatabase.Query("SELECT COUNT(*) as count FROM item_instance WHERE randomPropertyId != 0");
            uint32 affectedCount = 0;
            if (result)
            {
                Field* fields = result->Fetch();
                affectedCount = fields[0].Get<uint32>();
            }

            // 4. 清除随机属性ID
            CharacterDatabase.Execute("UPDATE item_instance SET randomPropertyId = 0 WHERE randomPropertyId != 0");

            // 5. 验证清理结果
            QueryResult verifyResult = CharacterDatabase.Query("SELECT COUNT(*) as count FROM item_instance WHERE randomPropertyId != 0");
            uint32 remainingCount = 0;
            if (verifyResult)
            {
                Field* fields = verifyResult->Fetch();
                remainingCount = fields[0].Get<uint32>();
            }

            handler->PSendSysMessage("清理完成！");
            handler->PSendSysMessage("%s", ModuleOldFormat("已备份 %s 条记录到 item_instance_random_backup 表", affectedCount).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("已清理 %s 条随机属性记录", affectedCount).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("剩余随机属性记录: %s", remainingCount).c_str());

            if (remainingCount == 0)
            {
                handler->SendSysMessage("✓ 所有随机属性ID已成功清除");
                handler->SendSysMessage("建议重启服务器或重新登录以确保更改生效");
            }
            else
            {
                handler->SendSysMessage(" 仍有部分随机属性ID未清除，请检查数据库");
            }
        }
        catch (const std::exception& e)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("清理过程中发生错误: %s", e.what()).c_str());
            handler->SendSysMessage("请检查数据库连接和权限");
            return true;
        }

        return true;
    }

    static bool HandleQueryCommand(ChatHandler* handler, const char* args)
    {
        if (!ItemEnhancementEnabled)
        {
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 解析参数: itemID guid bag slot equip rpId
        if (!args || !*args)
        {
            handler->SendSysMessage("用法: .强化 查询 <物品ID> <GUID> <bag> <slot> <equip> <rpId>");
            return true;
        }

        std::istringstream iss(args);
        uint32 itemID = 0;
        uint32 guid = 0;
        int32 bag = -1;
        int32 slot = -1;
        int32 equip = 0;
        int32 rpId = 0;

        iss >> itemID >> guid >> bag >> slot >> equip >> rpId;

        if (itemID == 0 || guid == 0)
        {
            handler->SendSysMessage("无效的物品ID或GUID");
            return true;
        }

        // 查询强化记录
        EnhancementRecord const* record = sItemEnhancementMgr->GetEnhancementRecord(guid);

        if (!record || record->level == 0)
        {
            // 没有强化数据，返回空响应
            return true;
        }

        // 构建属性字符串
        std::ostringstream attrs;
        bool first = true;

        // 解析 statValues 字符串 (格式: "type value,type value,...")
        std::istringstream statStream(record->statValues);
        std::string statPair;

        while (std::getline(statStream, statPair, ','))
        {
            std::istringstream pairStream(statPair);
            uint32 statType = 0;
            AsInt64 statValue = 0;

            pairStream >> statType >> statValue;

            if (statType > 0 && statValue != 0)
            {
                if (!first) attrs << ",";
                attrs << statType << " " << Acore::ToString(statValue);
                first = false;
            }
        }

        // 发送紧凑格式消息：ITEM_ENHANCEMENT:itemID:guid:level:attrs
        std::ostringstream response;
        response << "ITEM_ENHANCEMENT:" << itemID
                 << ":" << guid
                 << ":" << record->level
                 << ":" << attrs.str();

        handler->PSendSysMessage(response.str().c_str());

        return true;
    }

    static bool HandleReloadCommand(ChatHandler* handler, const char* /*args*/)
    {
        // 【修复】同时刷新全局旧接口和ItemEnhancementMgr的配置/模板
        // 先刷新Manager的配置和模板
        if (sItemEnhancementMgr)
        {
            sItemEnhancementMgr->LoadConfig();
            sItemEnhancementMgr->LoadEnhancementTemplates();
        }

        // 再刷新全局旧接口（保持兼容性）
        LoadItemEnhancementConfig();
        LoadEnhancementTemplates();
        LoadEnhancementRecords();

        handler->SendSysMessage("物品强化系统配置和数据已重新加载");
        return true;
    }
};

// 注册脚本
void AddSC_ItemEnhancementCommands()
{
    new ItemEnhancement_CommandScript();
}
