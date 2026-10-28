#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "RuneSystem.h"
#include "RuneManager.h"
#include "RuneUtils.h"
#include "RuneApplyManager.h"
#include "Item.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "ModuleFormat.h"

using namespace Acore::ChatCommands;

// Addon消息前缀
static constexpr char const* RUNE_ADDON_PREFIX = "RuneManagerUI";

class rune_commandscript : public CommandScript
{
public:
    rune_commandscript() : CommandScript("rune_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable runeCommandTable =
        {
            { "openslot",  HandleRuneOpenSlotCommand,   SEC_GAMEMASTER,     Console::No },
            { "equip",     HandleRuneEquipCommand,      SEC_GAMEMASTER,     Console::No },
            { "unequip",   HandleRuneUnequipCommand,    SEC_GAMEMASTER,     Console::No },
            { "info",      HandleRuneInfoCommand,       SEC_PLAYER,         Console::No },
            { "list",      HandleRuneListCommand,       SEC_PLAYER,         Console::No },
            { "query",     HandleRuneQueryCommand,      SEC_PLAYER,         Console::No },
            { "reload",    HandleRuneReloadCommand,     SEC_ADMINISTRATOR,  Console::Yes },
            { "界面",      HandleRuneOpenUICommand,     SEC_PLAYER,         Console::No },
            { "ui",        HandleRuneOpenUICommand,     SEC_PLAYER,         Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "rune", runeCommandTable },
        };

        return commandTable;
    }

    // .符文 信息 <装备GUID>
    static bool HandleRuneInfoCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!*args)
        {
            handler->SendSysMessage("用法: .符文 信息 <装备GUID>");
            return false;
        }

        uint32 itemGuid = atoi(args);
        Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, itemGuid));
        if (!item)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("找不到GUID为 %s 的装备。", itemGuid).c_str());
            return false;
        }

        itemGuid = item->GetGUID().GetCounter();
        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);

        handler->PSendSysMessage("%s", ModuleOldFormat("物品: %s (GUID: %s)", item->GetTemplate()->Name1.c_str(), itemGuid).c_str());

        if (!storage)
        {
            handler->SendSysMessage("此物品没有符文插槽。");
            return true;
        }

        auto slotIds = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
        auto runeIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

        handler->PSendSysMessage("%s", ModuleOldFormat("符文插槽数量: %s", static_cast<uint32>(slotIds.size())).c_str());
        handler->PSendSysMessage("%s", ModuleOldFormat("已装备符文: %s", RuneUtils::GetEquippedRuneCount(storage->runeIds)).c_str());
        handler->SendSysMessage("----------------------------");

        for (size_t i = 0; i < slotIds.size(); ++i)
        {
            uint32 slotId = slotIds[i];
            uint32 runeItemId = (i < runeIds.size()) ? runeIds[i] : 0;

            if (runeItemId > 0)
            {
                // 修复：runeIds存储的是符文物品ID，应使用GetRuneInfoByItemId
                const CustomRuneInfo* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeItemId);
                if (runeInfo)
                {
                    handler->PSendSysMessage("%s", ModuleOldFormat("插槽 %s: %s (品质 %s, 物品ID: %s, 符文记录ID: %s)", slotId, runeInfo->name.c_str(), runeInfo->runeQuality, runeInfo->itemId, runeInfo->runeId).c_str());
                    handler->PSendSysMessage("%s", ModuleOldFormat("  效果: %s", runeInfo->effectDescription.c_str()).c_str());
                }
                else
                {
                    handler->PSendSysMessage("%s", ModuleOldFormat("插槽 %s: 符文物品ID %s (未在符文表中登记)", slotId, runeItemId).c_str());
                }
            }
            else
            {
                handler->PSendSysMessage("%s", ModuleOldFormat("插槽 %s: 空", slotId).c_str());
            }
        }

        return true;
    }

    // .符文 开槽 <装备GUID> <槽位数量>
    static bool HandleRuneOpenSlotCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        char* itemGuidStr = strtok((char*)args, " ");
        char* slotCountStr = strtok(nullptr, " ");

        if (!itemGuidStr || !slotCountStr)
        {
            handler->SendSysMessage("用法: .符文 开槽 <装备GUID> <槽位数量>");
            return false;
        }

        uint32 itemGuid = atoi(itemGuidStr);
        uint32 slotCount = atoi(slotCountStr);

        if (slotCount == 0 || slotCount > 28)
        {
            handler->SendSysMessage("插槽数量必须在 1-28 之间。");
            return false;
        }

        Item* item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, itemGuid));
        if (!item)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("找不到GUID为 %s 的装备。", itemGuid).c_str());
            return false;
        }

        // 检查是否已经有槽位
        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);
        if (storage)
        {
            handler->SendSysMessage("此装备已经开过槽，不能重复开槽。");
            return false;
        }

        if (sRuneManager->AddRuneSlotToItem(item, slotCount))
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("成功为GUID=%s的装备 %s 开启了 %s 个符文槽位。", itemGuid, item->GetTemplate()->Name1.c_str(), slotCount).c_str());
            return true;
        }
        else
        {
            handler->SendSysMessage("开槽失败。");
            return false;
        }
    }

    // .符文 装备 <装备GUID> <插槽编号> <符文物品ID>
    static bool HandleRuneEquipCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        char* itemGuidStr = strtok((char*)args, " ");
        char* slotIdStr = strtok(nullptr, " ");
        char* runeItemIdStr = strtok(nullptr, " ");

        if (!itemGuidStr || !slotIdStr || !runeItemIdStr)
        {
            handler->SendSysMessage("用法: .符文 装备 <装备GUID> <插槽编号> <符文物品ID>");
            return false;
        }

        uint32 itemGuid = atoi(itemGuidStr);
        uint32 slotId = atoi(slotIdStr);
        uint32 runeItemId = atoi(runeItemIdStr);

        if (RuneApplyManager::EquipRuneByItemId(player, itemGuid, slotId, runeItemId))
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("成功将符文物品 %s 装备到GUID=%s装备的插槽 %s", runeItemId, itemGuid, slotId).c_str());
            return true;
        }
        else
        {
            handler->SendSysMessage("装备符文失败。");
            return false;
        }
    }

    // .符文 卸载 <装备GUID> <插槽编号>
    static bool HandleRuneUnequipCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        char* itemGuidStr = strtok((char*)args, " ");
        char* slotIdStr = strtok(nullptr, " ");

        if (!itemGuidStr || !slotIdStr)
        {
            handler->SendSysMessage("用法: .符文 卸载 <装备GUID> <插槽编号>");
            return false;
        }

        uint32 itemGuid = atoi(itemGuidStr);
        uint32 slotId = atoi(slotIdStr);

        if (RuneApplyManager::UnequipRune(player, itemGuid, slotId))
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("成功从GUID=%s装备的插槽 %s 卸载符文。", itemGuid, slotId).c_str());
            return true;
        }
        else
        {
            handler->SendSysMessage("卸载符文失败，插槽可能是空的。");
            return false;
        }
    }

    // .符文 创建 <名称>,<槽位>,<品质>,<法术ID>,<冷却>,<触发几率>,<效果类型>
    static bool HandleRuneCreateCommand(ChatHandler* handler, const char* args)
    {
        char* nameStr = strtok((char*)args, ",");
        char* slotStr = strtok(nullptr, ",");
        char* qualityStr = strtok(nullptr, ",");
        char* spellIdStr = strtok(nullptr, ",");
        char* cooldownStr = strtok(nullptr, ",");
        char* procChanceStr = strtok(nullptr, ",");
        char* effectTypeStr = strtok(nullptr, ",");

        if (!nameStr || !slotStr || !qualityStr || !spellIdStr ||
            !cooldownStr || !procChanceStr || !effectTypeStr)
        {
            handler->SendSysMessage("用法: .符文 创建 <名称>,<槽位>,<品质>,<法术ID>,<冷却>,<触发几率>,<效果类型>");
            handler->SendSysMessage("示例: .符文 创建 力量符文,1,0,0,0,0,1");
            return false;
        }

        std::string name = nameStr;
        uint32 runeSlot = atoi(slotStr);
        uint32 runeQuality = atoi(qualityStr);
        uint32 spellId = atoi(spellIdStr);
        uint32 cooldown = atoi(cooldownStr);
        uint32 procChance = atoi(procChanceStr);
        uint32 effectType = atoi(effectTypeStr);

        uint32 newRuneId = sRuneManager->CreateRune(name, runeSlot, runeQuality,
                                                     spellId, cooldown, procChance, effectType);

        if (newRuneId > 0)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("成功创建符文: %s (ID: %s)", name.c_str(), newRuneId).c_str());
            return true;
        }
        else
        {
            handler->SendSysMessage("创建符文失败。");
            return false;
        }
    }

    // .符文 列表 [槽位] [品质]
    static bool HandleRuneListCommand(ChatHandler* handler, const char* args)
    {
        uint32 runeSlot = 0;
        uint32 runeQuality = 0;

        char* slotStr = strtok((char*)args, " ");
        char* qualityStr = strtok(nullptr, " ");

        if (slotStr)
            runeSlot = atoi(slotStr);
        if (qualityStr)
            runeQuality = atoi(qualityStr);

        auto runes = sRuneManager->GetRuneList(runeSlot, runeQuality);

        if (runes.empty())
        {
            handler->SendSysMessage("没有找到符文。");
            return true;
        }

        handler->PSendSysMessage("%s", ModuleOldFormat("找到 %s 个符文:", static_cast<uint32>(runes.size())).c_str());
        handler->SendSysMessage("----------------------------");

        for (const auto& rune : runes)
        {
            handler->PSendSysMessage("%s", ModuleOldFormat("ID:%s | %s | 槽位:%s | 品质:%s", rune.runeId, rune.name.c_str(), rune.runeSlot, rune.runeQuality).c_str());
            handler->PSendSysMessage("%s", ModuleOldFormat("  效果: %s", rune.effectDescription.c_str()).c_str());
        }

        return true;
    }

    // .rune query <itemID> <guid> <bag> <slot> <equipFlag> <unused>
    static bool HandleRuneQueryCommand(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        // 解析参数
        char* itemIDStr = strtok((char*)args, " ");
        char* guidStr = strtok(nullptr, " ");
        char* bagStr = strtok(nullptr, " ");
        char* slotStr = strtok(nullptr, " ");
        char* equipFlagStr = strtok(nullptr, " ");
        char* unusedStr = strtok(nullptr, " ");

        if (!itemIDStr || !guidStr)
        {
            handler->SendSysMessage("用法: .rune query <itemID> <guid> <bag> <slot> <equipFlag> <unused>");
            return false;
        }

        uint32 itemID = atoi(itemIDStr);
        uint32 guid = atoi(guidStr);
        int32 bag = bagStr ? atoi(bagStr) : -1;
        int32 slot = slotStr ? atoi(slotStr) : -1;
        bool equipFlag = equipFlagStr ? (atoi(equipFlagStr) != 0) : false;

        // 查找物品
        Item* item = nullptr;

        if (guid > 0)
        {
            // 优先使用GUID查找
            item = player->GetItemByGuid(ObjectGuid(HighGuid::Item, guid));
        }
        else if (equipFlag && slot >= 0)
        {
            // 从装备栏查找
            item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        }
        else if (bag >= 0 && slot >= 0)
        {
            // 从背包查找
            item = player->GetItemByPos(bag, slot);
        }

        if (!item || item->GetEntry() != itemID)
        {
            // 返回空数据（静默失败，不显示错误信息）
            return true;
        }

        // 获取物品GUID
        uint32 itemGuid = item->GetGUID().GetCounter();

        // 查询符文槽位信息
        const CustomRuneStorage* storage = sRuneSystem->GetItemRuneStorage(itemGuid);

        if (!storage)
        {
            // 没有符文槽位，返回空数据
            return true;
        }

        // 解析槽位和符文数据
        auto slotIds = RuneUtils::ParseCommaSeparatedUInt32(storage->slotIds);
        auto runeIds = RuneUtils::ParseCommaSeparatedUInt32(storage->runeIds);

        uint32 totalSlots = static_cast<uint32>(slotIds.size());
        uint32 filledSlots = 0;

        // 构建槽位数据字符串
        std::ostringstream slotDataStream;

        for (size_t i = 0; i < slotIds.size(); ++i)
        {
            uint32 slotId = slotIds[i];
            uint32 runeId = (i < runeIds.size()) ? runeIds[i] : 0;

            if (runeId > 0)
            {
                filledSlots++;
                // 注意：runeId 实际上是符文物品ID (itemId)，需要用 GetRuneInfoByItemId 查找
                const CustomRuneInfo* runeInfo = sRuneSystem->GetRuneInfoByItemId(runeId);
                if (runeInfo)
                {
                    // 格式：slotId:runeId:runeName:runeQuality:effectDesc
                    if (i > 0)
                        slotDataStream << ",";
                    slotDataStream << slotId << ":"
                                  << runeId << ":"
                                  << runeInfo->name << ":"
                                  << runeInfo->runeQuality << ":"
                                  << runeInfo->effectDescription;
                }
                else
                {
                    if (i > 0)
                        slotDataStream << ",";
                    slotDataStream << slotId << ":" << runeId << ":未知符文:0:";
                }
            }
            else
            {
                // 空槽位
                if (i > 0)
                    slotDataStream << ",";
                slotDataStream << slotId << ":0:::";
            }
        }

        // 返回格式化的数据：RUNEDATA:itemID:guid:totalSlots:filledSlots:slotData
        std::ostringstream response;
        response << "RUNEDATA:" << itemID << ":" << itemGuid << ":"
                << totalSlots << ":" << filledSlots << ":" << slotDataStream.str();

        handler->SendSysMessage(response.str().c_str());

        return true;
    }

    // .符文 重载
    static bool HandleRuneReloadCommand(ChatHandler* handler, const char* /*args*/)
    {
        handler->SendSysMessage("正在重载符文数据...");

        sRuneSystem->LoadRunesFromDB();

        handler->SendSysMessage("符文数据重载完成。");
        return true;
    }

    // .符文 界面 / .rune ui - 打开UI界面
    static bool HandleRuneOpenUICommand(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        std::string fullMessage = std::string(RUNE_ADDON_PREFIX) + "\tOPEN_UI";
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, fullMessage, 0);
        player->SendDirectMessage(&data);

        return true;
    }
};

void AddSC_rune_commands()
{
    new rune_commandscript();
}
