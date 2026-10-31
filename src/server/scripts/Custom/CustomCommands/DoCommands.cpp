
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
#include "DBCStores.h"
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "PoolMgr.h"
#include "Config.h"
#include "WorldSession.h"
#include "Player.h"
#include "Item.h"
#include "Pet.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <tchar.h>
#include <fstream>
#include <iostream>
#include <limits>

#include <Custom/_Helper/Helper.h>
#include "../../../scripts/Custom/AsItemMgr/AsItemMgr.h"
#include <Custom/ItemFuncMgr/ItemFuncMgr.h>
#include <Custom/PlayerModMgr/PlayerModMgr.h>
#include <Custom/ClassConfig/ClassMod.h>
#include <Custom/AddonActive/DoAddon.h>
#include <Custom/EventMgr/EventMgr.h>
#include <Custom/CreatureMod/AsCreature.h>
#include "../../../../../modules/mod-huanjinxitong/src/HuanJingSystem.h"
#include <Custom/NoticeMgr/NoticeMgr.h>
#include <Custom/AuraSpellMgr/AuraSpellMgr.h>
#include <Custom/EnterFBMgr/EnterFBMgr.h>
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/MultiNpcMgr/MultiNpcMgr.h>
#include <Custom/ItemShopMgr/ItemShopMgr.h>
#include <Custom\ItemRandomAtMgr\ItemRandomAtMgr.h>
#include <Custom\ItemRunesMgr\ItemRunesMgr.h>
#include <Loot\LootMgr.h>
#include <Custom\BGAsTeam\BGAs.h>
#include <Custom/ArenaSoloMgr/ArenaSoloMgr.h>
#include <Custom/ItemMagicMgr/ItemMagicMgr.h>
#include <Custom/ItemUpMgr/ItemUpMgr.h>
#include <Custom/ItemPlayerStatMod/ItemPlayerStatMod.h>
#include <Custom/GuildBattleMgr/GuildBattleMgr.h>
#include <Custom/DBCMgr/DBCMgr.h>
#include <Custom/AsItemMgr/NpcBeastmaster.h>
#include <Custom/CombatPetMgr/CombatPetMgr.h>
#include <Custom/AthenaEventMgr/AthenaEventMgr.h>
#include <Custom/CharPvpTop/CharPvpTop.h>
#include <Custom/MapAreaFuncMgr/MapAreaFunc.h>
#include <Custom/PicGuideMgr/PicGuideMgr.h>
#include <Custom/PlayerAction/PlayerAction.h>
#include <Custom/CustomVendorMgr/CustomVendorMgr.h>
#include <Custom/ReincarnationMgr/ReincarnationMgr.h>
#include <Custom/BeastCardMgr/BeastCardMgr.h>
#include <Custom/StarVeinMgr/StarVeinMgr.h>
#include <Custom/ChatFilterMgr/ChatFilterMgr.h>
#include <Custom/SpellCooldownMgr/SpellCooldownMgr.h>
#include <Custom/AntiGCDHackMgr/AntiGCDHackMgr.h>
#pragma execution_character_set("utf-8")

namespace
{
AsInt64 ScaleCommandAsValue(AsInt64 const& value, float modifier)
{
    if (value <= 0)
        return AsInt64Zero();

    int64 scale = int64(modifier * 10000.0f + (modifier >= 0.0f ? 0.5f : -0.5f));
    return (value * scale) / 10000;
}

AsInt64 GetCommandCombatPetAsMaxHp(uint32 playerLevel, CreatureTemplate const* cInfo, uint32 creEntry, uint32 rank)
{
    AsInt64 maxHp = AsInt64(1);
    if (!cInfo)
        return maxHp;

    if (PetLevelInfo const* petInfo = sObjectMgr->GetPetLevelInfo(creEntry, playerLevel))
        maxHp = petInfo->asHealth > 0 ? petInfo->asHealth : AsInt64(petInfo->health);
    else if (CreatureBaseStats const* baseStats = sObjectMgr->GetCreatureBaseStats(playerLevel, cInfo->unit_class))
    {
        maxHp = baseStats->AsBaseHealth[cInfo->expansion] > 0 ? baseStats->AsBaseHealth[cInfo->expansion] : AsInt64(baseStats->BaseHealth[cInfo->expansion]);
        maxHp = ScaleCommandAsValue(maxHp, std::min(1.0f, cInfo->ModHealth));
    }

    if (uint32 stTempId = sCombatPetMgr->GetStTempIdForCombatPetRank(creEntry, rank))
    {
        auto itrStat = AtCreatureStatTempStore.find(stTempId);
        if (itrStat != AtCreatureStatTempStore.end())
        {
            if (!AsInt64IsZero(itrStat->second.NowHp))
                maxHp = itrStat->second.NowHp;
            else
                maxHp = ScaleCommandAsValue(maxHp, itrStat->second.HealthMultiplier);
        }
    }

    return maxHp > 0 ? maxHp : AsInt64(1);
}
}

using namespace Acore::ChatCommands;

class Master_CommandScript : public CommandScript
{
public:
    Master_CommandScript() : CommandScript("Master_CommandScript") {}

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> DoCreSetInfoCommandTable =
        {
            { "name", HandleCreNameCommand, SEC_ADMINISTRATOR, Console::No },
            { "scale",   HandleCreScaleCommand, SEC_ADMINISTRATOR, Console::No },
            { "display", HandleCreDisplayCommand,SEC_ADMINISTRATOR, Console::No },
        };

        static std::vector<ChatCommand> DoReloadCommandTable =
        {
            { "0",   HandleDoReloadGlobalConfigCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "1",   HandleDoReloadGlobalGtsCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "2",   HandleDoReloadAsTempCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "3",   HandleDoReloadItemFuncCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "4",   HandleDoReloadClassModCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "6",   HandleDoReloadDmgRankCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "7",   HandleDoReloadAsCreatureCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "8",   HandleDoReloadNoticeMgrCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "10",   HandleDoReloadOnHealByCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "11",   HandleDoReloadEnterFBMgrCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "12",   HandleDoReloadMultiNpcMgrCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "13",   HandleDoReloadSignCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "14",   HandleDoReloadShopCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "15",   HandleDoReloadBGAsCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "16",   HandleDoReloadSoloDuelCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "17",   HandleDoReloadItemRandomAtMgrCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "18",   HandleDoReloadItemRunesMgrCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "19",   HandleDoReloadPVPKModCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "20",   HandleDoReloadSecretCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "21",   HandleDoReloadCustomPetCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "22",   HandleDoReloadBusMgrCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "23",   HandleDoReloadArenaSoloCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "24",   HandleDoReloadAuraSpellMgrCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "25",   HandleDoReloadEnMapAuraDataCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "28",   HandleDoReloadAyThroughCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "26",   HandleDoReloadItemMagicCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "27", HandleItemCtUpMgrCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "29", HandleItemPlayerStatCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "31", HandleGuildBattleCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "32",   HandleDoReloadItemZodiacCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "33",   HandleDoReloadBeastMasterCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "34",   HandleDoReloadTalentInfoModCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "35",   HandleDoReloadCombatPetCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "36",   HandleDoReloadAthenaEventCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "37",   HandleDoReloadCharPvpTopCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "38",   HandleDoReloadMapAreaFuncCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "39",   HandleDoReloadPicGuideMgrCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "40",   HandleDoReloadPlayerActionCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "41",   HandleDoReloadCustomVendorCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "42",   HandleDoReloadReincarnationCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "43",   HandleDoReloadChatFilterCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "44",   HandleDoReloadGCDWhitelistCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "45",   HandleDoReloadAntiGCDHackCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "46",   HandleDoReloadModulesCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "47",   HandleDoReloadBeastCardCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "49",   HandleDoReloadStarVeinCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "xmyh", HandleDoReloadStarVeinCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "星脉衍化", HandleDoReloadStarVeinCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "模块", HandleDoReloadModulesCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "module", HandleDoReloadModulesCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "modules", HandleDoReloadModulesCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "all", HandleDoReloadAllCommand, SEC_ADMINISTRATOR, Console::Yes },
        { "",     HandleDoReloadAllCommand, SEC_ADMINISTRATOR, Console::Yes },
        };

        static std::vector<ChatCommand> CommandTable =
        {
            { "delcombatpet",     HandleDelCombatPetAsCommand, SEC_CONSOLE, Console::No },
            { "toitemdbc",     HandleDBCGenerateCommand, SEC_CONSOLE, Console::Yes },
            { "resetlua",     HandleResetLuaCommand, SEC_CONSOLE, Console::Yes },
            { "datalua",     HandleAutoDataLuaCommand, SEC_CONSOLE, Console::Yes },
            { "resethm",     HandleResetHmCommand, SEC_ADMINISTRATOR, Console::No },
            { "vendor",     HandlePlayerVendorCommand, SEC_ADMINISTRATOR, Console::No },
            { "killone",     HandlePlayerKillOneCommand, SEC_ADMINISTRATOR, Console::No },
            { "killgroup",     HandlePlayerKillGroupCommand, SEC_ADMINISTRATOR, Console::No },
            { "recreid",     HandleResetCreEntryCommand, SEC_ADMINISTRATOR, Console::No },
            { "autosumboss",     HandleAutoSumBossCommand, SEC_ADMINISTRATOR, Console::No },
            { "autosum",     HandleAutoSumCommand, SEC_ADMINISTRATOR, Console::No },
            { "autoloot",     HandleAutoLootCommand, SEC_ADMINISTRATOR, Console::No },
            { "reitemall",     HandleReloadItemAllCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "rewtemp",     HandleDoRewtempCommand, SEC_ADMINISTRATOR, Console::No },
            { "onann",     HandleDoOnAnnCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "totele",     HandleDoCustomTeleCommand, SEC_ADMINISTRATOR, Console::No },
            { "dospell",     HandleDoSpellCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "posadd",     HandleAddPosCommand, SEC_ADMINISTRATOR, Console::No },
            { "cgpass",     HandleCgPassCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "statrank",     HandleStatRankCommand, SEC_ADMINISTRATOR, Console::No },
            { "statxp",     HandleXpStatCommand, SEC_ADMINISTRATOR, Console::No },
            { "asum",     HandleASumCommand, SEC_ADMINISTRATOR, Console::No },
            { "recre",     HandleReloadCreEntryCommand, SEC_ADMINISTRATOR, Console::No },
            { "reitem",     HandleReloadItemEntryCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "repas",     HandleRepAsCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "sj",     HandleWChatCommand, SEC_PLAYER, Console::No },
            { "vip",     HandleVipApplyCommand, SEC_ADMINISTRATOR, Console::No },
            { "jf",     HandleJFApplyCommand, SEC_ADMINISTRATOR, Console::No },
            { "jfas",     HandleJFAsApplyCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "mb",     HandleMBApplyCommand, SEC_ADMINISTRATOR, Console::No },
            { "mbas",     HandleMBAsApplyCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "pd",     HandlePDApplyCommand, SEC_ADMINISTRATOR, Console::No },
            { "as",     HandleAsInfoCommand, SEC_ADMINISTRATOR, Console::No },
            { "asdmg",     HandleAsDamageCommand, SEC_ADMINISTRATOR, Console::No },
            { "_set",         DoCreSetInfoCommandTable },
        { "dore",         DoReloadCommandTable },
        { "totz",     HandleTotzCommand, SEC_ADMINISTRATOR, Console::No },
        { "奖励",     HandleRewardCommand, SEC_ADMINISTRATOR, Console::No },
        { "随机",     HandleRandomCommand, SEC_ADMINISTRATOR, Console::No },
        };

        return CommandTable;
    }

    // 335XS: deal a real int64 damage value to the selected unit. Used to verify
    // the native 64-bit combat text channel with numbers above the 32-bit limit.
    static bool HandleAsDamageCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        if (!args || !*args)
        {
            handler->SendSysMessage("Usage: .asdmg <int64 amount>");
            return false;
        }

        Unit* target = handler->getSelectedUnit();

        if (!target)
        {
            handler->SendSysMessage("No target selected.");
            return false;
        }

        char* text = strtok((char*)args, " ");

        if (!text)
            return false;

        AsInt64 damage = AsInt64FromString(std::string(text));

        if (damage <= 0)
        {
            handler->SendSysMessage("Invalid amount (must be a positive int64).");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();

        Unit::DealAsDamage(player, target, damage, nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);

        handler->PSendSysMessage("asdmg: %s -> %s (%s)",
            AsInt64ToString(damage).c_str(), target->GetName().c_str(), target->GetGUID().ToString().c_str());

        return true;
    }

    static bool HandleTotzCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
        {
            handler->SendSysMessage("语法: .totz 坐标模板ID 挑战等级(或秘境等级)");
            return false;
        }

        // 解析参数
        char* posIdStr = strtok((char*)args, " ");
        char* levelStr = strtok(nullptr, " ");

        if (!posIdStr || !levelStr)
        {
            handler->SendSysMessage("语法: .totz 坐标模板ID 挑战等级(或秘境等级)");
            return false;
        }

        uint32 posId = uint32(atoi(posIdStr));
        uint32 challengeLevel = uint32(atoi(levelStr));

        Player* player = handler->GetSession()->GetPlayer();

        // 获取坐标模板数据
        auto it = PosMap.find(posId);
        if (it == PosMap.end())
        {
            handler->PSendSysMessage("坐标模板 %u 不存在", posId);
            return false;
        }

        uint32 targetMapId = it->second.MapId;

        MapEntry const* targetMapEntry = sMapStore.LookupEntry(targetMapId);
        if (!targetMapEntry || !targetMapEntry->IsDungeon())
        {
            handler->PSendSysMessage("目标地图 %u 不是有效副本地图", targetMapId);
            return false;
        }

        bool isChallengeMap = sAsCreature->IsChallengeMap(targetMapId);
        if (isChallengeMap)
        {
            uint32 nTempId = sAsCreature->GetChallengeNTempId(targetMapId, challengeLevel);
            if (nTempId > 0 && !sHelper->CheckNeed(player, nTempId, "GM命令进入挑战副本"))
            {
                return false;
            }
        }

        // 实例地图会在 AddPlayerToMap 时保存该等级，挑战与秘境共用同一份 MapRank。
        player->SetMapRank(challengeLevel);
        player->OnNowFB = true;  // 标记为已确认进入，避免撞门拦截
        player->UpdateTempAt(AT8001, isChallengeMap ? 1 : 0);

        // 执行传送
        player->SetCanTeleport(true);
        player->TeleportTo(it->second.MapId, it->second.X, it->second.Y, it->second.Z, it->second.O);

        // 传送提示已禁用，如需恢复请取消下面两行注释
        // std::string mapName = sHelper->GetMapName(targetMapId);
        // handler->PSendSysMessage("已传送至挑战副本 %s 难度 %u", mapName.c_str(), challengeLevel);

        return true;
    }

    static bool HandleRewardCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
        {
            handler->SendSysMessage("语法: .奖励 模板ID [数量]");
            return false;
        }

        // 使用 Tokenizer 解析参数
        Tokenizer tokens(args, ' ');
        if (tokens.size() < 1)
        {
            handler->SendSysMessage("语法: .奖励 模板ID [数量]");
            return false;
        }

        uint32 tempId = atoi(tokens[0]);
        uint32 count = 1;

        if (tokens.size() >= 2)
            count = atoi(tokens[1]);

        if (tempId == 0)
        {
            handler->SendSysMessage("模板ID必须大于0");
            return false;
        }

        if (count == 0)
            count = 1;

        if (count > 100)
        {
            handler->SendSysMessage("数量必须在 1-100 之间");
            return false;
        }

        Player* player = handler->GetSession()->GetPlayer();
        Player* target = handler->getSelectedPlayer();
        if (!target)
            target = player;

        // 使用 Mod 参数一次性发放多次奖励，避免数据库异步问题
        sHelper->DoTheReward(target, tempId, "GM命令.奖励", count);

        handler->PSendSysMessage("已向 %s 发放奖励模板 %u x %u 次",
            target->GetName().c_str(), tempId, count);

        return true;
    }

    static bool HandleRandomCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
        {
            handler->SendSysMessage("语法: .随机 .命令1*几率#.命令2*几率#.命令3 或 .随机 .命令1#.命令2#.命令3");
            return false;
        }

        std::string input(args);
        std::vector<std::pair<std::string, uint32>> commands; // <命令, 权重>
        uint32 totalWeight = 0;

        // 手动按#分割多个命令（保留命令中的空格）
        size_t start = 0;
        size_t sepPos = 0;
        while ((sepPos = input.find('#', start)) != std::string::npos)
        {
            std::string cmdStr = input.substr(start, sepPos - start);

            // 解析权重
            uint32 weight = 0; // 默认权重0（表示未指定，后续平均分配）
            size_t starPos = cmdStr.find_last_of('*');
            if (starPos != std::string::npos)
            {
                // 检查*后面是否是数字
                bool isNumber = true;
                for (size_t i = starPos + 1; i < cmdStr.length(); i++)
                {
                    if (!isdigit(cmdStr[i]))
                    {
                        isNumber = false;
                        break;
                    }
                }

                if (isNumber && starPos + 1 < cmdStr.length())
                {
                    weight = atoi(cmdStr.substr(starPos + 1).c_str());
                    cmdStr = cmdStr.substr(0, starPos);
                }
            }

            // 去除前后空格
            size_t trimStart = cmdStr.find_first_not_of(" ");
            size_t trimEnd = cmdStr.find_last_not_of(" ");
            if (trimStart != std::string::npos && trimEnd != std::string::npos)
                cmdStr = cmdStr.substr(trimStart, trimEnd - trimStart + 1);

            if (!cmdStr.empty())
            {
                commands.push_back({ cmdStr, weight });
                // 只有指定了权重的才加到totalWeight
                if (weight > 0)
                    totalWeight += weight;
            }

            start = sepPos + 1;
        }

        // 处理最后一个命令
        if (start < input.length())
        {
            std::string cmdStr = input.substr(start);

            uint32 weight = 0;
            size_t starPos = cmdStr.find_last_of('*');
            if (starPos != std::string::npos)
            {
                bool isNumber = true;
                for (size_t i = starPos + 1; i < cmdStr.length(); i++)
                {
                    if (!isdigit(cmdStr[i]))
                    {
                        isNumber = false;
                        break;
                    }
                }

                if (isNumber && starPos + 1 < cmdStr.length())
                {
                    weight = atoi(cmdStr.substr(starPos + 1).c_str());
                    cmdStr = cmdStr.substr(0, starPos);
                }
            }

            size_t trimStart = cmdStr.find_first_not_of(" ");
            size_t trimEnd = cmdStr.find_last_not_of(" ");
            if (trimStart != std::string::npos && trimEnd != std::string::npos)
                cmdStr = cmdStr.substr(trimStart, trimEnd - trimStart + 1);

            if (!cmdStr.empty())
            {
                commands.push_back({ cmdStr, weight });
                // 只有指定了权重的才加到totalWeight
                if (weight > 0)
                    totalWeight += weight;
            }
        }

        if (commands.empty())
        {
            handler->SendSysMessage("没有有效的命令");
            return false;
        }

        // 检查是否有命令没有指定权重
        // 如果没有指定权重的命令，平均分配权重
        uint32 unspecifiedCount = 0;
        for (auto& [cmd, weight] : commands)
        {
            if (weight == 0)
                unspecifiedCount++;
        }

        if (unspecifiedCount > 0)
        {
            // 有命令没有指定权重，平均分配
            uint32 avgWeight = 100; // 默认每个命令权重100
            totalWeight = 0;
            for (auto& [cmd, weight] : commands)
            {
                if (weight == 0)
                    weight = avgWeight;
                totalWeight += weight;
            }
        }

        // 随机选择一个命令
        uint32 randomVal = urand(1, totalWeight);
        uint32 currentWeight = 0;
        std::string selectedCmd;
        uint32 selectedWeight = 0;

        for (auto& [cmd, weight] : commands)
        {
            currentWeight += weight;
            if (randomVal <= currentWeight)
            {
                selectedCmd = cmd;
                selectedWeight = weight;
                break;
            }
        }

        // 保存原始命令用于显示
        std::string displayCmd = selectedCmd;

        // 执行选中的命令 - 通用GM命令执行
        // 去掉命令前的 "." 前缀
        if (!selectedCmd.empty() && selectedCmd[0] == '.')
            selectedCmd = selectedCmd.substr(1);

        // 显示结果
        float chancePct = (float)selectedWeight / totalWeight * 100.0f;
        handler->PSendSysMessage("随机结果 [%.1f%%]: %s", chancePct, displayCmd.c_str());

        // 分割命令名和参数
        size_t spacePos = selectedCmd.find(' ');
        std::string cmdName = (spacePos != std::string::npos) ? selectedCmd.substr(0, spacePos) : selectedCmd;
        std::string cmdArgs = (spacePos != std::string::npos) ? selectedCmd.substr(spacePos + 1) : "";

        // 去除参数前后空格
        if (!cmdArgs.empty())
        {
            size_t argsStart = cmdArgs.find_first_not_of(" ");
            size_t argsEnd = cmdArgs.find_last_not_of(" ");
            if (argsStart != std::string::npos && argsEnd != std::string::npos)
                cmdArgs = cmdArgs.substr(argsStart, argsEnd - argsStart + 1);
            else
                cmdArgs = "";
        }

        // 使用 ChatHandler 执行命令
        bool executed = false;

        // 尝试查找并执行命令
        if (handler->GetSession())
        {
            // 构建完整命令字符串（需要以 . 开头才能被ParseCommands识别）
            std::string fullCmd = "." + cmdName;
            if (!cmdArgs.empty())
                fullCmd += " " + cmdArgs;

            // 直接通过 ChatHandler 执行
            executed = handler->ParseCommands(fullCmd.c_str());
        }

        if (!executed)
        {
            // 如果解析执行失败，尝试直接调用已知的命令处理函数
            if (cmdName == "奖励")
            {
                // 解析 .奖励 参数
                Tokenizer rewardTokens(cmdArgs.c_str(), ' ');
                uint32 tempId = 0;
                uint32 count = 1;

                if (rewardTokens.size() >= 1)
                    tempId = atoi(rewardTokens[0]);

                if (rewardTokens.size() >= 2)
                    count = atoi(rewardTokens[1]);

                if (tempId > 0)
                {
                    if (count == 0)
                        count = 1;

                    Player* player = handler->GetSession()->GetPlayer();
                    Player* target = handler->getSelectedPlayer();
                    if (!target)
                        target = player;

                    // 使用 Mod 参数一次性发放多次奖励，避免数据库异步问题
                    sHelper->DoTheReward(target, tempId, "GM命令.随机", count);

                    handler->PSendSysMessage("已执行 .奖励 %u %u", tempId, count);
                    executed = true;
                }
            }
            else if (cmdName == "jf")
            {
                // 解析 .jf 参数
                if (!cmdArgs.empty())
                {
                    int64 count = int64(atoll(cmdArgs.c_str()));
                    if (count != 0)
                    {
                        Player* player = handler->GetSession()->GetPlayer();
                        Player* target = handler->getSelectedPlayer();
                        if (!target)
                            target = player;

                        uint32 AccountId = target->GetSession()->GetAccountId();
                        int64 OldToken = int64(target->GetToken());
                        int64 NowToken = OldToken + count;
                        NowToken = NowToken > 0 ? NowToken : 0;

                        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN);
                        Utmt->SetData(0, NowToken);
                        Utmt->SetData(1, AccountId);
                        LoginDatabase.Execute(Utmt);

                        sHelper->LogPlayerTokenNow(player->GetGUID(), count, "GM:jf");

                        std::string action = (count < 0) ? "减少" : "增加";
                        uint32 changeCount = (count > 0) ? count : (OldToken < abs(count)) ? OldToken : abs(count);
                        target->SendSysMsg(sHelper->GetGts(1007), action.c_str(), changeCount, NowToken);
                        if (player->GetGUID() != target->GetGUID())
                            player->SendSysMsg(sHelper->GetGts(1050), action.c_str(), sHelper->GetNameLinkColor(target).c_str(), changeCount, NowToken);

                        handler->PSendSysMessage("已执行 .jf %lld", count);
                        executed = true;
                    }
                }
            }
            else if (cmdName == "mb")
            {
                // 解析 .mb 参数
                if (!cmdArgs.empty())
                {
                    int64 count = int64(atoll(cmdArgs.c_str()));
                    if (count != 0)
                    {
                        Player* player = handler->GetSession()->GetPlayer();
                        Player* target = handler->getSelectedPlayer();
                        if (!target)
                            target = player;

                        uint32 AccountId = target->GetSession()->GetAccountId();
                        int64 OldToken = int64(target->GetTokenMB());
                        int64 NowToken = OldToken + count;
                        NowToken = NowToken > 0 ? NowToken : 0;

                        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN_MB);
                        Utmt->SetData(0, NowToken);
                        Utmt->SetData(1, AccountId);
                        LoginDatabase.Execute(Utmt);

                        sHelper->LogPlayerTokenNow(player->GetGUID(), count, "[魔币]GM:mb");

                        std::string action = (count < 0) ? "减少" : "增加";
                        uint32 changeCount = (count > 0) ? count : (OldToken < abs(count)) ? OldToken : abs(count);
                        target->SendSysMsg("[温馨提示]: 您%s了 |cFFFF6699%u|r 魔币 当前魔币: |cFFFF6699%u|r", action.c_str(), changeCount, NowToken);
                        if (player->GetGUID() != target->GetGUID())
                            player->SendSysMsg("[GM频道]: 您使用命令 %s了 %s 的 |cFFFF6699%u|r 魔币 当前魔币 |cFFFF6699%u|r", action.c_str(), sHelper->GetNameLinkColor(target).c_str(), changeCount, NowToken);

                        handler->PSendSysMessage("已执行 .mb %lld", count);
                        executed = true;
                    }
                }
            }
            else if (cmdName == "statxp")
            {
                // 解析 .statxp 参数
                if (!cmdArgs.empty())
                {
                    AsInt64 xp = AsInt64FromString(cmdArgs);
                    if (xp > 0)
                    {
                        Player* player = handler->GetSession()->GetPlayer();
                        Player* target = handler->getSelectedPlayer();
                        if (!target)
                            target = player;

                        AsInt64 NextXp = sCustomClass->GetStatNeedXp(AsInt64ToUInt32Clamp(target->TempAtStore[AT11].Value));
                        if (NextXp)
                        {
                            AsInt64 NXp = sAuraSpellMgr->GetStatNowHook(target, xp);
                            AsInt64 NewXp = target->TempAtStore[AT12].Value + NXp;
                            while (NewXp >= NextXp && NextXp)
                            {
                                NewXp -= NextXp;
                                if (NextXp)
                                {
                                    target->TempAtStore[AT11] += 1;
                                    target->CastSpell(target, TxLevelSpell, false);
                                    target->TempAtStore[AT13] += sCustomClass->GetStatRewCount(AsInt64ToUInt32Clamp(target->TempAtStore[AT11].Value));
                                }
                                NextXp = sCustomClass->GetStatNeedXp(AsInt64ToUInt32Clamp(target->TempAtStore[AT11].Value));
                            }
                            target->TempAtStore[AT12] = NewXp;
                            sCustomClass->SendUpdatePlayerStatData(target);
                            target->SendSysMsg("[温馨提示]: 获得斗气经验 %s", AsInt64ToString(NXp).c_str());
                            target->SendSysMsg(sHelper->GetGts(2001), AsInt64ToUInt32Clamp(target->TempAtStore[AT11].Value));
                            handler->PSendSysMessage("已执行 .statxp %s", AsInt64ToString(xp).c_str());
                            executed = true;
                        }
                    }
                }
            }
            else if (cmdName == "totele")
            {
                // 解析 .totele 参数
                if (!cmdArgs.empty())
                {
                    uint32 teleId = uint32(atoi(cmdArgs.c_str()));
                    if (teleId > 0)
                    {
                        Player* player = handler->GetSession()->GetPlayer();
                        player->TeleToPos(teleId);
                        handler->PSendSysMessage("已执行 .totele %u", teleId);
                        executed = true;
                    }
                }
            }
            else if (cmdName == "autosum")
            {
                // 解析 .autosum 参数
                if (!cmdArgs.empty())
                {
                    uint32 creEntry = uint32(atoi(cmdArgs.c_str()));
                    if (creEntry > 0)
                    {
                        Player* player = handler->GetSession()->GetPlayer();
                        CreatureTemplate const* CInfo = sObjectMgr->GetCreatureTemplate(creEntry);
                        if (CInfo)
                        {
                            Creature* Cre = player->SummonCreature(creEntry, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation(), TEMPSUMMON_MANUAL_DESPAWN);
                            if (Cre)
                            {
                                Cre->SetOwnerGUID(player->GetGUID());
                                Cre->SetFaction(player->GetFaction());
                                Cre->GetMotionMaster()->MoveFollow(player, 2.0f, M_PI);
                                handler->PSendSysMessage("已执行 .autosum %u", creEntry);
                                executed = true;
                            }
                        }
                    }
                }
            }
            else if (cmdName == "killgroup")
            {
                // 解析 .killgroup 参数
                if (!cmdArgs.empty())
                {
                    uint32 tempId = uint32(atoi(cmdArgs.c_str()));
                    if (tempId > 0)
                    {
                        Player* player = handler->GetSession()->GetPlayer();
                        Group* Gp = player->GetGroup();
                        if (Gp)
                        {
                            for (GroupReference* Itr = Gp->GetFirstMember(); Itr != nullptr; Itr = Itr->next())
                            {
                                Player* Member = Itr->GetSource();
                                if (Member && Member->IsInWorld() && player->IsInMap(Member))
                                    sHelper->DoTheReward(Member, tempId, "GM命令.随机 killgroup");
                            }
                        }
                        else
                            sHelper->DoTheReward(player, tempId, "GM命令.随机 killgroup");

                        handler->PSendSysMessage("已执行 .killgroup %u", tempId);
                        executed = true;
                    }
                }
            }
            else if (cmdName == "resethm")
            {
                // .resethm 不需要参数
                Player* player = handler->GetSession()->GetPlayer();
                player->SetFullHealth();
                player->SetPower(POWER_MANA, player->GetMaxPower(POWER_MANA));
                handler->PSendSysMessage("已执行 .resethm");
                executed = true;
            }
            else if (cmdName == "pd")
            {
                // 解析 .pd 参数
                if (!cmdArgs.empty())
                {
                    int64 count = int64(atoll(cmdArgs.c_str()));
                    if (count != 0)
                    {
                        Player* player = handler->GetSession()->GetPlayer();
                        Player* target = handler->getSelectedPlayer();
                        if (!target)
                            target = player;

                        uint64 OldPaodian = target->GetPaodian();
                        target->AddPaodian(count, "GM命令.随机");
                        uint64 NowPaodian = target->GetPaodian();

                        std::string action = (count < 0) ? "减少" : "增加";
                        uint64 changeCount = (count > 0) ? count : (OldPaodian < uint64(llabs(count))) ? OldPaodian : uint64(llabs(count));

                        if (player->GetGUID() != target->GetGUID())
                            player->SendSysMsg("[GM频道]: 您使用命令 %s了 %s 的 |cFFFF6699%llu|r 泡点 当前泡点 |cFFFF6699%llu|r", action.c_str(), sHelper->GetNameLinkColor(target).c_str(), changeCount, NowPaodian);

                        handler->PSendSysMessage("已执行 .pd %lld", count);
                        executed = true;
                    }
                }
            }
            else if (cmdName == "add")
            {
                // .add 命令 - 尝试通过 ParseCommands 执行
                // ParseCommands 需要以 . 开头的完整命令
                std::string fullCmd = ".add " + cmdArgs;
                executed = handler->ParseCommands(fullCmd.c_str());
                if (executed)
                    handler->PSendSysMessage("已执行 .add %s", cmdArgs.c_str());
            }
            else if (cmdName == "levelup")
            {
                // .levelup 命令 - 尝试通过 ParseCommands 执行
                std::string fullCmd = ".levelup " + cmdArgs;
                executed = handler->ParseCommands(fullCmd.c_str());
                if (executed)
                    handler->PSendSysMessage("已执行 .levelup %s", cmdArgs.c_str());
            }
            else if (cmdName == "aura")
            {
                // .aura 命令 - 尝试通过 ParseCommands 执行
                std::string fullCmd = ".aura " + cmdArgs;
                executed = handler->ParseCommands(fullCmd.c_str());
                if (executed)
                    handler->PSendSysMessage("已执行 .aura %s", cmdArgs.c_str());
            }
        }

        if (!executed)
        {
            handler->PSendSysMessage("命令执行失败: %s", selectedCmd.c_str());
        }

        return true;
    }

    static bool HandleDelCombatPetAsCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        Player* player = handler->GetSession()->GetPlayer();

        uint32 CreateIndex = player->GetTempAtUInt32(AT7002);
        if (CreateIndex)
        {
            auto Itep = player->CombatPetStStore.find(CreateIndex);
            if (Itep != player->CombatPetStStore.end())
            {
                if (Itep->second.CreEntry)
                {
                    CreatureTemplate const* CInfo = sObjectMgr->GetCreatureTemplate(Itep->second.CreEntry);
                    uint32 PlayerLevel = player->GetLevel();
                    AsInt64 MaxHp = GetCommandCombatPetAsMaxHp(PlayerLevel, CInfo, Itep->second.CreEntry, Itep->second.Rank);

                    for (auto Itrc = CPetTempStore.begin(); Itrc != CPetTempStore.end(); ++Itrc)
                    {
                        if (Itrc->CreId == Itep->second.CreEntry && Itrc->Rank == Itep->second.Rank)
                        {
                            AsInt64 AddHp = ScaleCommandAsValue(MaxHp, Itrc->FitMod);
                            int32 AddStat1 = int32(Itrc->Stat1 * Itrc->FitMod);
                            int32 AddStat2 = int32(Itrc->Stat2 * Itrc->FitMod);
                            int32 AddStat3 = int32(Itrc->Stat3 * Itrc->FitMod);
                            int32 AddStat4 = int32(Itrc->Stat4 * Itrc->FitMod);

                            player->DoApplyStat(ITEM_MOD_HEALTH, AddHp, false);
                            player->DoApplyStat(ITEM_MOD_STRENGTH, AddStat1, false);
                            player->DoApplyStat(ITEM_MOD_AGILITY, AddStat2, false);
                            player->DoApplyStat(ITEM_MOD_INTELLECT, AddStat3, false);
                            player->DoApplyStat(ITEM_MOD_SPIRIT, AddStat4, false);

                            for (auto It = Itrc->FitAuras.begin(); It != Itrc->FitAuras.end(); ++It)
                                if (sSpellMgr->GetSpellInfo(*It) && player->HasAura(*It))
                                        player->RemoveAurasDueToSpell(*It);
                        }
                    }
                }
            }
        }

        Creature* OldCre = player->GetWorldSoltIndexCombatPet();
        if (OldCre)
        {
            OldCre->CombatStop();
            OldCre->ToTempSummon()->UnSummon();
        }
        player->UpdateTempAt(AT7001, 0);
        player->UpdateTempAt(AT7002, 0);
        uint32 GuidLow = player->GetGUIDLow();
        CharacterDatabase.Execute("UPDATE  _玩家战宠存档 SET 战宠编号 = '0', 战宠阶级 = '0', 战宠经验 = '0' WHERE 玩家标识 =  '{}'", GuidLow);
        handler->GetSession()->LogoutPlayer(true);
        return false;
    }

    static bool HandleDBCGenerateCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        std::string DataPath = sConfigMgr->GetOption<std::string>("DataDir", "./");
        if (DataPath.empty() || (DataPath.at(DataPath.length() - 1) != '/' && DataPath.at(DataPath.length() - 1) != '\\'))
            DataPath.push_back('/');

        std::vector<std::vector<int32>> DBCData;
        if (QueryResult Result = WorldDatabase.Query("SELECT entry, class, subclass, SoundOverrideSubclass, Material, displayid, InventoryType, sheath FROM item_template ORDER BY entry ASC;"))
        {
            do
            {
                std::vector<int32> RecordData;
                Field* fields = Result->Fetch();

                for (uint8 i = 0; i < 8; i++)
                    RecordData.push_back(fields[i].Get<int32>());

                DBCData.push_back(RecordData);

            } while (Result->NextRow());
        }

        int32 WBDC = 0x43424457;
        int32 RecordCount = DBCData.size();
        int32 FieldCount = 8;
        int32 RecordSize = 32;

        std::string OutFilePath = DataPath + "dbc/Item.dbc";
        std::ofstream FStream(OutFilePath.c_str(), std::ios::binary);
        if (!FStream.is_open())
        {
            LOG_ERROR("server.worldserver", ">> 未知错误 无法写入文件: {}", OutFilePath.c_str());
            return false;
        }

        LOG_INFO("server.worldserver", ">> 正在写入数据到: {}", OutFilePath.c_str());

        FStream.write((char*)&WBDC, sizeof(int32));
        FStream.write((char*)&RecordCount, sizeof(int32));
        FStream.write((char*)&FieldCount, sizeof(int32));
        FStream.write((char*)&RecordSize, sizeof(int32));
        FStream.write("\0", sizeof(int32));

        for (int32 i = 0; i < RecordCount; i++)
        {
            std::vector<int32> DataList = DBCData.at(i);
            for (int32 j = 0; j < FieldCount; j++)
            {
                int32 FieldValue = DataList[j];
                FStream.write((char*)&FieldValue, sizeof(int32));
            }
        }

        FStream.close();
        LOG_INFO("server.worldserver", ">> 写入数据完毕 自动覆盖到: {}", OutFilePath.c_str());
        return false;
    }

    static bool HandleResetLuaCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        sHelper->AutoResetLuaNowTime();
        LOG_INFO("server.loading", ">> 重载 所有玩家插件缓存成功");
        handler->SendGlobalGMSysMessage("[GM频道]: 所有玩家插件缓存成功");
        return false;
    }

    static bool HandleAutoDataLuaCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        TCHAR LuaFolder[MAX_PATH];
        GetModuleFileName(0, LuaFolder, MAX_PATH);
        TCHAR* AppExe = _tcsrchr(LuaFolder, '\\');
        if (!AppExe)
        {
            LOG_INFO("server.worldserver", ">> 转换路径失败");
            return false;
        }

        AppExe[0] = '\0';
        ++AppExe;

        TCHAR LuaFolderPath[MAX_PATH];
        sprintf(LuaFolderPath, "%s\\%s", LuaFolder, _T("__ClientLuaData"));

        if (!CreateDirectoryA(LuaFolderPath, nullptr))
        {
            if (GetLastError() != ERROR_ALREADY_EXISTS)
            {
                LOG_INFO("server.worldserver", ">> 创建 {} 文件夹失败", LuaFolderPath);
                return false;
            }
        }

        LOG_INFO("server.worldserver", ">> 正在生成Lua数据文件中......");

        {
            char FilePath[MAX_PATH];
            snprintf(FilePath, MAX_PATH, _T("%s\\SpellItemEnchantmentData.lua"), LuaFolderPath);
            FILE* LuaFile = fopen(FilePath, "w");
            fprintf(LuaFile, _T("\n--Data\nDream_EnchantmentDes = {\n"));
            for (auto Itr = sSpellItemEnchantmentStore.begin(); Itr != sSpellItemEnchantmentStore.end(); Itr++)
                fprintf(LuaFile, _T("    [\"%u\"] = \"%s\",\n"), Itr->ID, Itr->description[LOCALE_zhCN]);

            fprintf(LuaFile, _T("};"));
            fflush(LuaFile);
            fclose(LuaFile);
            LOG_INFO("server.worldserver", ">> Lua数据文件 SpellItemEnchantmentData.lua 生成完毕");
        }

        {
            char FilePath[MAX_PATH];
            snprintf(FilePath, MAX_PATH, _T("%s\\ItemSubNameData.lua"), LuaFolderPath);
            FILE* LuaFile = fopen(FilePath, "w");
            fprintf(LuaFile, _T("\n--Data\nDream_ItemSubNameData = {\n"));
            for (auto Itr = ItemSubNameStore.begin(); Itr != ItemSubNameStore.end(); Itr++)
                fprintf(LuaFile, _T("    [%u] = \"%s\",\n"), Itr->first, Itr->second.c_str());

            fprintf(LuaFile, _T("};"));
            fflush(LuaFile);
            fclose(LuaFile);
            LOG_INFO("server.worldserver", ">> Lua数据文件 ItemSubNameData.lua 生成完毕");
        }

        {
            char FilePath[MAX_PATH];
            snprintf(FilePath, MAX_PATH, _T("%s\\ItemVendorInfo.lua"), LuaFolderPath);
            FILE* LuaFile = fopen(FilePath, "w");
            fprintf(LuaFile, _T("\n--Data\nDream_ItemSellEntryGtsData = {\n"));
            for (auto Itr = ItemVsInfoStore.begin(); Itr != ItemVsInfoStore.end(); Itr++)
            {
                if (Itr->ItemId < 0)
                    fprintf(LuaFile, _T("    {%u, %u, %s,\n    },\n"), abs(Itr->ItemId), Itr->VendorId, sHelper->GetRewardLuaGts(Itr->RTempId).c_str());
            }

            fprintf(LuaFile, _T("};"));
            fflush(LuaFile);
            fclose(LuaFile);
            LOG_INFO("server.worldserver", ">> Lua数据文件 ItemVendorInfo.lua 生成完毕");
        }

        {
            char FilePath[MAX_PATH];
            snprintf(FilePath, MAX_PATH, _T("%s\\ItemBuyInfo.lua"), LuaFolderPath);
            FILE* LuaFile = fopen(FilePath, "w");
            fprintf(LuaFile, _T("\n--Data\nDream_ItemBuyGtsData = {\n"));
            for (auto Itr = ItemVsInfoStore.begin(); Itr != ItemVsInfoStore.end(); Itr++)
            {
                if (Itr->ItemId > 0)
                    fprintf(LuaFile, _T("    {%u, %u, %s,\n    },\n"), abs(Itr->ItemId), Itr->VendorId, sHelper->GetNeedLuaGts(Itr->NTempId).c_str());
            }

            fprintf(LuaFile, _T("};"));
            fflush(LuaFile);
            fclose(LuaFile);
            LOG_INFO("server.worldserver", ">> Lua数据文件 ItemBuyInfo.lua 生成完毕");
        }

        {
            char FilePath[MAX_PATH];
            snprintf(FilePath, MAX_PATH, _T("%s\\ItemSellGuidInfo.lua"), LuaFolderPath);
            FILE* LuaFile = fopen(FilePath, "w");
            fprintf(LuaFile, _T("\n--Data\nDream_ItemSellGuidGtsData = {\n"));
            for (auto Itr = SuperUpSellStore.begin(); Itr != SuperUpSellStore.end(); ++Itr)
            {
                fprintf(LuaFile, _T("    {%u, %u, %s,\n    },\n"), Itr->ItemId, Itr->Level, sHelper->GetRewardLuaGts(Itr->RTempId).c_str());
            }

            fprintf(LuaFile, _T("};"));
            fflush(LuaFile);
            fclose(LuaFile);
            LOG_INFO("server.worldserver", ">> Lua数据文件 ItemSellGuidInfo.lua 生成完毕");
        }

        LOG_INFO("server.worldserver", ">> 所有数据文件生成完毕 在当前目录 __ClientLuaData 文件夹内");
        LOG_INFO("server.worldserver", ">> 将所有生成后的文件放入客户端插件 ____Function 目录下替换");

        return false;
    }

    static bool HandleResetHmCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        Player* player = handler->GetSession()->GetPlayer();
        player->SetFullHealth();
        player->SetPower(POWER_MANA, player->GetMaxPower(POWER_MANA));

        return false;
    }

    static bool HandlePlayerVendorCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        char* Vendor = strtok((char*)args, " ");
        if (!Vendor)
            return false;

        uint32 VendorId = atoi(Vendor);
        Player* player = handler->GetSession()->GetPlayer();
        ClearGossipMenuFor(player);
        player->GetSession()->SendPlayerNewListInventory(player->GetGUID(), VendorId);

        return false;
    }

    static bool HandlePlayerKillOneCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        char* RChar = strtok((char*)args, " ");
        if (!RChar)
            return false;

        uint32 RTempId = atoi(RChar);

        Player* player = handler->GetSession()->GetPlayer();
        sHelper->DoTheReward(player, RTempId, "命令 killone");

        return false;
    }

    static bool HandlePlayerKillGroupCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        char* RChar = strtok((char*)args, " ");
        if (!RChar)
            return false;

        uint32 RTempId = atoi(RChar);

        Player* player = handler->GetSession()->GetPlayer();
        Group* Gp = player->GetGroup();
        if (Gp)
        {
            for (GroupReference* Itr = Gp->GetFirstMember(); Itr != nullptr; Itr = Itr->next())
            {
                Player* Member = Itr->GetSource();
                if (Member && Member->IsInWorld() && player->IsInMap(Member))
                {
                    sHelper->DoTheReward(Member, RTempId, "命令 killgroup");
                }
            }
        }
        else
        {
            sHelper->DoTheReward(player, RTempId, "命令 killgroup");
        }

        return false;
    }

    static bool HandleResetCreEntryCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        char* CreChar = strtok((char*)args, " ");
        if (!CreChar)
            return false;

        uint32 CreEntry = atoi(CreChar);
        sObjectMgr->UpdateCreTempForEntry(CreEntry);
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 单个生物 完成");
        return false;
    }

    static bool HandleAutoSumBossCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        char* CreChar = strtok((char*)args, " ");
        if (!CreChar)
            return false;

        uint32 CreEntry = atoi(CreChar);
        CreatureTemplate const* CInfo = sObjectMgr->GetCreatureTemplate(CreEntry);
        if (!CInfo)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Creature* Cre = player->SummonCreature(CreEntry, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation(), TEMPSUMMON_MANUAL_DESPAWN);
        if (!Cre)
            return false;

        if (Cre->GetAI())
            Cre->GetAI()->AttackStart(player);

        return false;
    }

    static bool HandleAutoSumCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        char* CreChar = strtok((char*)args, " ");
        if (!CreChar)
            return false;

        uint32 CreEntry = atoi(CreChar);
        CreatureTemplate const* CInfo = sObjectMgr->GetCreatureTemplate(CreEntry);
        if (!CInfo)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Creature* Cre = player->SummonCreature(CreEntry, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation(), TEMPSUMMON_MANUAL_DESPAWN);
        if (!Cre)
            return false;

        Cre->SetOwnerGUID(player->GetGUID());
        Cre->SetFaction(player->GetFaction());
        Cre->GetMotionMaster()->MoveFollow(player, 2.0f, M_PI);
        return false;
    }

    static bool HandleCreNameCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        Creature* Cre = handler->getSelectedCreature();
        if (!Cre)
        {
            handler->SendSysMessage(LANG_SELECT_CREATURE);
            return false;
        }

        CreatureTemplate* CInfo = const_cast<CreatureTemplate*>(Cre->GetCreatureTemplate());
        if (!CInfo)
        {
            handler->SendSysMessage(LANG_SELECT_CREATURE);
            return false;
        }

        char* NameStr = strtok((char*)args, " ");
        if (!NameStr)
            return false;

        if (strcmp(Cre->GetCreatureTemplate()->Name.c_str(), NameStr) == 0)
            return false;

        CInfo->Name = NameStr;
        uint32 Entry = Cre->GetEntry();
        // Admin-only, but a name containing a quote (e.g. O'Neil) broke the
        // statement and left memory and DB out of sync. Escape for the DB.
        std::string EscapedName = NameStr;
        WorldDatabase.EscapeString(EscapedName);
        WorldDatabase.Execute("UPDATE creature_template SET name = '{}' WHERE (entry = {});", EscapedName, Entry);

        SessionMap const& Sessions = sWorld->GetAllSessions();
        for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
        {
            if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
                continue;

            WorldPacket Packet;
            Packet << CInfo->Entry;
            Packet << Cre->GetGUID();
            Itr->second->HandleCreatureQueryOpcode(Packet);
        }

        handler->SendGlobalGMSysMessage(Acore::StringFormat("[GM频道]: 当前生物名字修改存档完毕 新名字 -> %s", NameStr).c_str());
        return false;
    }

    static bool HandleCreScaleCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        Creature* Cre = handler->getSelectedCreature();
        if (!Cre)
        {
            handler->SendSysMessage(LANG_SELECT_CREATURE);
            return false;
        }

        CreatureTemplate* CInfo = const_cast<CreatureTemplate*>(Cre->GetCreatureTemplate());
        if (!CInfo)
        {
            handler->SendSysMessage(LANG_SELECT_CREATURE);
            return false;
        }

        char* ScaleStr = strtok((char*)args, " ");
        if (!ScaleStr)
            return false;

        float Scale = atof(ScaleStr);
        if (CInfo->scale == Scale)
            return false;

        uint32 Entry = Cre->GetEntry();
        WorldDatabase.Execute("UPDATE creature_template SET scale = {} WHERE (entry = {});", Scale, Entry);

        CInfo->scale = Scale;
        Cre->SetObjectScale(Scale);

        handler->SendGlobalGMSysMessage(Acore::StringFormat("[GM频道]: 当前生物体型大小修改存档完毕 新体型大小 -> %.1f", Scale).c_str());
        return false;
    }

    static bool HandleCreDisplayCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        Creature* Cre = handler->getSelectedCreature();
        if (!Cre)
        {
            handler->SendSysMessage(LANG_SELECT_CREATURE);
            return false;
        }

        CreatureTemplate* CInfo = const_cast<CreatureTemplate*>(Cre->GetCreatureTemplate());
        if (!CInfo)
        {
            handler->SendSysMessage(LANG_SELECT_CREATURE);
            return false;
        }

        char* DisplayStr = strtok((char*)args, " ");
        if (!DisplayStr)
            return false;

        uint32 DisplayId = uint32(atoi(DisplayStr));
        if (CInfo->Modelid1 == DisplayId)
            return false;

        CInfo->Modelid1 = DisplayId;
        Cre->SetDisplayId(DisplayId);

        uint32 Entry = Cre->GetEntry();
        WorldDatabase.Execute("UPDATE creature_template SET modelid1 = {} WHERE (entry = {});", DisplayId, Entry);

        handler->SendGlobalGMSysMessage(Acore::StringFormat("[GM频道]: 当前生物模型编号修改存档完毕 新模型编号 -> %u", DisplayId).c_str());
        return false;
    }

    static bool HandleDoReloadArenaSoloCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sArenaSoloMgr->LoadArenaSoloConfig();

        LOG_INFO("server.loading", ">> 重载 _1V1竞技场配置表");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 _1V1竞技场配置表");
        return false;
    }

    static bool HandleDoReloadAuraSpellMgrCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sAuraSpellMgr->LoadAuraSpellMgr();

        LOG_INFO("server.loading", ">> 重载 _技能光环控制表");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 _技能光环控制表");
        return false;
    }

    static bool HandleDoReloadBeastMasterCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sNpcBeastMaster->LoadSystem();

        LOG_INFO("server.loading", ">> 重载 __全职业宠物配置表");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 __全职业宠物配置表");
        return false;
    }

    static bool HandleDoReloadItemZodiacCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sItemMagicMgr->LoadItemZodiacData();

        LOG_INFO("server.loading", ">> 重载 _传奇十二生肖配置表");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 _传奇十二生肖配置表");
        return false;
    }

    static bool HandleGuildBattleCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sGuildBattleMgr->LoadGuildBattle();
        sAsPlayerBattleMgr->LoadAsPlayerBattle();

        LOG_INFO("server.loading", ">> 重载 __公会战配置表");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 __公会战配置表");
        LOG_INFO("server.loading", ">> 重载 __全服战配置表");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 __全服战配置表");
        return false;
    }

    static bool HandleItemPlayerStatCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sItemPlayerStatMod->LoadItemPlayerStatMod();

        LOG_INFO("server.loading", ">> 重载 _物品属性奖励表");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 _物品属性奖励表");
        return false;
    }

    static bool HandleItemCtUpMgrCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sItemUpMgr->LoadItemUpData();

        LOG_INFO("server.loading", ">> 重载 _物品普通升级合成表");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 _物品普通升级合成表");
        return false;
    }

    static bool HandleDoReloadItemMagicCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sItemMagicMgr->LoadItemMagicData();

        LOG_INFO("server.loading", ">> 重载 _玩家法宝光环表");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 _玩家法宝光环表");
        return false;
    }

    static bool HandleDoReloadEnMapAuraDataCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sPlayerModMgr->LoadEnMapAuraData();

        LOG_INFO("server.loading", ">> 重载 _地图玩家光环表");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 _地图玩家光环表");
        return false;
    }

    static bool HandleDoReloadBusMgrCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool HandleDoReloadCustomPetCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool HandleDoReloadSecretCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);
        //sSecretPlaceMgr->LoadSecretPlace();
        //LOG_INFO("server.loading", ">> 重载 秘境相关");
        //handler->SendGlobalGMSysMessage("[GM频道]: 重载秘境相关完成");
        return false;
    }

    static bool HandleReloadItemAllCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);
        sObjectMgr->UpdateItemTempForAll();
        LOG_INFO("server.loading", ">> 重载 所有物品完成 此命令慎用");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载所有物品完成 此命令慎用");
        return false;
    }

    static bool HandleDoRewtempCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        char* IdStr = strtok((char*)args, " ");
        if (!IdStr)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
            playerTarget = player;

        uint32 Id = uint32(atoi(IdStr));
        sHelper->DoTheReward(playerTarget, Id, "GM发放奖励");
        return false;
    }

    static bool HandleAutoLootCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        char* TypeStr = strtok((char*)args, " ");
        char* LootIdStr = strtok(nullptr, " ");
        if (!TypeStr || !LootIdStr)
            return false;

        LootStore& Store = LootTemplates_Creature;
        uint32 Typ = uint32(atoi(TypeStr));
        switch (Typ)
        {
        case 1:
            Store = LootTemplates_Creature;
            break;
        case 2:
            Store = LootTemplates_Fishing;
            break;
        case 3:
            Store = LootTemplates_Gameobject;
            break;
        case 4:
            Store = LootTemplates_Item;
            break;
        case 5:
            Store = LootTemplates_Mail;
            break;
        case 6:
            Store = LootTemplates_Milling;
            break;
        case 7:
            Store = LootTemplates_Pickpocketing;
            break;
        case 8:
            Store = LootTemplates_Reference;
            break;
        case 9:
            Store = LootTemplates_Skinning;
            break;
        case 10:
            Store = LootTemplates_Disenchant;
            break;
        case 11:
            Store = LootTemplates_Prospecting;
            break;
        case 12:
            Store = LootTemplates_Spell;
            break;
        default:
            Store = LootTemplates_Creature;
            break;
        }

        uint32 LootId = uint32(atoi(LootIdStr));
        sPlayerModMgr->DoAutoStoreLoot(handler->GetSession()->GetPlayer(), LootId, Store);

        return false;
    }

    static bool HandleDoOnAnnCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        char* IdStr = strtok((char*)args, " ");
        char* AnnText = strtok(nullptr, " ");
        if (!IdStr || !AnnText)
            return false;

        std::string Msg = "[服务器通告]: " + std::string(AnnText);

        uint32 Id = uint32(atoi(IdStr));
        switch (Id)
        {
        case 1:
            sHelper->SendSerMessage(Msg.c_str());
            break;
        case 2:
            sHelper->SendSerNewMessage(Msg.c_str());
            break;
        case 3:
            sHelper->SendSerSpecialMessage(Msg.c_str());
            break;
        case 4:
            sHelper->SendSerAllMessage(Msg.c_str());
            break;
        default:
            sHelper->SendSerMessage(Msg.c_str());
            break;
        }
        return false;
    }

    static bool HandleDoCustomTeleCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        char* Tele = strtok((char*)args, " ");
        if (!Tele)
            return false;
        uint32 TeleId = uint32(atoi(Tele));
        handler->GetSession()->GetPlayer()->TeleToPos(TeleId);
        return false;
    }

    static bool HandleDoReloadPVPKModCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        sPlayerModMgr->LoadPVPKMod();
        handler->SendGlobalGMSysMessage("重载 玩家被玩家击杀掉落 生效");
        return false;
    }

    static bool HandleDoReloadPlayerActionCommand(ChatHandler* handler, char const* args)
         {
        handler->SetSentErrorMessage(true);
        sPlayerAction->LoadPlayerAction();
        handler->SendGlobalGMSysMessage("重载 _动作触发功能表 生效");
        return false;
        }

    static bool HandleDoReloadMapAreaFuncCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        sMapAreaFunc->LoadMapAreaFuncData();
        handler->SendGlobalGMSysMessage("重载 地图区域综合控制 生效");
        return false;
    }
    static bool HandleDoReloadPicGuideMgrCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        sPicGuideMgr->LoadPicGuideData();
        handler->SendGlobalGMSysMessage("重载 图鉴功能 生效");
        return false;
    }

    static bool HandleDoReloadCharPvpTopCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        sCharPvpTop->LoadTopSys();
        handler->SendGlobalGMSysMessage("重载 比武大会 生效");
        return false;
    }

    static bool HandleDoReloadAthenaEventCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        sAthenaEventMgr->LoadAthenaEventData();
        handler->SendGlobalGMSysMessage("重载 雅典娜活动 生效");
        return false;
    }

    static bool HandleDoReloadCombatPetCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        sCombatPetMgr->LoadCombatPetData();
        handler->SendGlobalGMSysMessage("重载 战宠功能表 生效");
        return false;
    }

    static bool HandleDoReloadTalentInfoModCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        sPlayerModMgr->LoadTalentInfoMod();
        handler->SendGlobalGMSysMessage("重载 天赋技能需求表 生效");
        return false;
    }

    static bool HandleDoReloadItemRunesMgrCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        sItemRunesMgr->LoadItemRunes();
        handler->SendGlobalGMSysMessage("重载 符文相关 生效");
        return false;
    }

    static bool HandleDoReloadItemRandomAtMgrCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        sItemRandomAtMgr->LoadItemRandomAt();
        LOG_INFO("server.loading", ">> 重载 装备超级随机相关 生效");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 装备超级随机相关 生效");
        return false;
    }

    static bool HandleDoReloadSoloDuelCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        sCharPvpTop->LoadTopSys();
        LOG_INFO("server.loading", ">> 重载 _比武大会配置表 生效");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 _比武大会配置表 生效");
        return false;
    }

    static bool HandleDoSpellCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        sSpellMgr->LoadSpellInfoCorrections();
        handler->SendGlobalGMSysMessage("重载 技能光环相关模块 生效");
        return false;
    }

    static bool HandleAddPosCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        Player* player = handler->GetSession()->GetPlayer();
        if (!player->IsInWorld())
            return false;

        uint32 PosId = 1;
        if (QueryResult Result = WorldDatabase.Query("SELECT max(坐标编号) FROM __模版坐标;"))
            PosId = Result->Fetch()[0].Get<uint32>() + 1;

        std::string escapedComment = args ? args : "";
        WorldDatabase.EscapeString(escapedComment);
        WorldDatabase.DirectExecute("INSERT INTO __模版坐标(坐标编号, 地图编号, X坐标, Y坐标, Z坐标, O坐标, 备注) "
            "VALUES ({}, {}, {}, {}, {}, {}, '{}')", PosId, player->GetMapId(), player->GetPositionX(),
            player->GetPositionY(), player->GetPositionZ(), player->GetOrientation(), escapedComment);

        PosTemplate St;
        St.MapId = player->GetMapId();
        St.X = player->GetPositionX();
        St.Y = player->GetPositionY();
        St.Z = player->GetPositionZ();
        St.O = player->GetOrientation();
        PosMap[PosId] = St;

        player->SendSysMsg("__模版坐标 新增编号 %u 备注: %s", PosId, args);
        return false;
    }

    static bool HandleCgPassCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        char* Input = strtok((char*)args, " ");
        char* newPassword = strtok(nullptr, " ");
        if (!Input || !newPassword)
            return false;

        uint32 Account = uint32(atoi(Input));

        AccountOpResult result = AccountMgr::ChangePassword(Account, newPassword);
        switch (result)
        {
        case AccountOpResult::AOR_OK:
            handler->SendSysMessage(LANG_COMMAND_PASSWORD);
            break;
        case AccountOpResult::AOR_PASS_TOO_LONG:
            handler->SendSysMessage(LANG_PASSWORD_TOO_LONG);
            break;
        default:
            handler->SendSysMessage(LANG_COMMAND_NOTCHANGEPASSWORD);
            break;
        }

        return false;
    }

    static bool HandleStatRankCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
            playerTarget = player;

        char* Input = strtok((char*)args, " ");
        uint32 Rank = uint32(atoi(Input));
        uint32 NowRank = AsInt64ToUInt32Clamp(playerTarget->TempAtStore[AT11].Value);

        if (Rank <= NowRank)
        {
            player->SendSysMsg(sHelper->GetGts(2007));
            return false;
        }

        uint32 Max = sCustomClass->GetStatMaxRank();
        Rank = (Rank > Max) ? Max : Rank;
        playerTarget->TempAtStore[AT11] += (Rank - NowRank);
        playerTarget->TempAtStore[AT13] += sCustomClass->GetStatOldNewRewCount(NowRank, AsInt64ToUInt32Clamp(playerTarget->TempAtStore[AT11].Value));
        playerTarget->CastSpell(playerTarget, TxLevelSpell, false);

        sCustomClass->SendUpdatePlayerStatData(playerTarget);
        playerTarget->SendSysMsg(sHelper->GetGts(2001), Rank);

        return false;
    }

    static bool HandleXpStatCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
            playerTarget = player;

        AsInt64 NextXp = sCustomClass->GetStatNeedXp(AsInt64ToUInt32Clamp(playerTarget->TempAtStore[AT11].Value));
        if (!NextXp)
        {
            playerTarget->SendSysMsg(sHelper->GetGts(2006));
            return false;
        }

        char* Input = strtok((char*)args, " ");
        AsInt64 Xp = AsInt64FromString(Input);
        AsInt64 NXp = sAuraSpellMgr->GetStatNowHook(player, Xp);
        AsInt64 NewXp = playerTarget->TempAtStore[AT12].Value + NXp;
        while (NewXp >= NextXp && NextXp)
        {
            NewXp -= NextXp;

            if (NextXp)
            {
                playerTarget->TempAtStore[AT11] += 1;
                playerTarget->CastSpell(playerTarget, TxLevelSpell, false);
                playerTarget->TempAtStore[AT13] += sCustomClass->GetStatRewCount(AsInt64ToUInt32Clamp(playerTarget->TempAtStore[AT11].Value));
            }

            NextXp = sCustomClass->GetStatNeedXp(AsInt64ToUInt32Clamp(playerTarget->TempAtStore[AT11].Value));
        }

        playerTarget->TempAtStore[AT12] = NewXp;

        sCustomClass->SendUpdatePlayerStatData(playerTarget);

        playerTarget->SendSysMsg("[温馨提示]: 获得斗气经验 %s", AsInt64ToString(NXp).c_str());
        playerTarget->SendSysMsg(sHelper->GetGts(2001), AsInt64ToUInt32Clamp(playerTarget->TempAtStore[AT11].Value));

        return false;
    }

    static bool HandleASumCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        Player* Summoner = handler->GetSession()->GetPlayer();
        char* Input = strtok((char*)args, " ");
        int32 Id = int32(atoi(Input));
        switch (Id)
        {
        case 1:
        {
            SessionMap const& Sessions = sWorld->GetAllSessions();
            for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
            {
                if (!Itr->second)
                    continue;

                Player* player = Itr->second->GetPlayer();
                if (player && player->IsInWorld() && player->GetTeamId() == TEAM_ALLIANCE && player->GetGUID() != Summoner->GetGUID())
                    player->DoSummonRequestFrom(Summoner);
            }

            Summoner->SendSysMsg("[GM频道]: 所有联盟玩家已被召唤");
        }
        break;
        case 2:
        {
            SessionMap const& Sessions = sWorld->GetAllSessions();
            for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
            {
                if (!Itr->second)
                    continue;

                Player* player = Itr->second->GetPlayer();
                if (player && player->IsInWorld() && player->GetTeamId() == TEAM_HORDE && player->GetGUID() != Summoner->GetGUID())
                    player->DoSummonRequestFrom(Summoner);
            }

            Summoner->SendSysMsg("[GM频道]: 所有部落玩家已被召唤");
        }
        break;
        case 3:
        {
            if (Group* Gp = Summoner->GetGroup())
            {
                for (GroupReference* Itr = Gp->GetFirstMember(); Itr != nullptr; Itr = Itr->next())
                {
                    Player* Member = Itr->GetSource();
                    if (Member && Member->IsInWorld() && Member->GetGUID() != Summoner->GetGUID())
                        Member->DoSummonRequestFrom(Summoner);
                }
            }

            Summoner->SendSysMsg("[GM频道]: 队伍成员玩家已被召唤");
        }
        break;
        case 4:
        {
            SessionMap const& Sessions = sWorld->GetAllSessions();
            for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
            {
                if (!Itr->second)
                    continue;

                Player* player = Itr->second->GetPlayer();
                if (player && player->IsInWorld() && player->GetGUID() != Summoner->GetGUID())
                    player->DoSummonRequestFrom(Summoner);
            }

            Summoner->SendSysMsg("[GM频道]: 所有世界玩家已被召唤");
        }
        break;
        }

        return false;
    }

    static bool HandleReloadCreEntryCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        Creature* Cre = handler->getSelectedCreature();
        if (!Cre)
        {
            handler->SendSysMessage(LANG_SELECT_CREATURE);
            return false;
        }

        sHelper->UpdateCreTempForEntry(Cre);
        handler->SendGlobalGMSysMessage(Acore::StringFormat("[GM频道]: 重载生物 %u 完成", Cre->GetEntry()).c_str());
        return false;
    }

    static bool HandleReloadItemEntryCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);
        if (!*args)
            return false;

        char* Input = strtok((char*)args, " ");
        if (!Input)
            return false;

        errno = 0;
        char* End = nullptr;
        unsigned long long ParsedEntry = std::strtoull(Input, &End, 10);
        if (errno == ERANGE || End == Input || *End != '\0' || ParsedEntry == 0 ||
            ParsedEntry > std::numeric_limits<uint32>::max())
        {
            handler->SendGlobalGMSysMessage("[GM频道]: 物品编号无效，必须是 1 到 4294967295 的整数");
            return false;
        }

        uint32 Entry = static_cast<uint32>(ParsedEntry);
        if (!sObjectMgr->UpdateItemTempForEntry(Entry))
        {
            handler->SendGlobalGMSysMessage("[GM频道]: 数据库不存在此物品 无法重载");
            return false;
        }

        handler->SendGlobalGMSysMessage(Acore::StringFormat("[GM频道]: 重载物品 %u 完成", Entry).c_str());
        return false;
    }

    static bool HandleRepAsCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        if (!*args)
        {
            handler->SendGlobalGMSysMessage("[GM频道]: 没有输入奖励模板编号");
            return false;
        }

        char* Input = strtok((char*)args, " ");
        if (!Input)
        {
            handler->SendGlobalGMSysMessage("[GM频道]: 没有输入奖励模板编号");
            return false;
        }

        int32 RTempId = int32(atoi(Input));
        if (RTempId <= 0)
        {
            handler->SendGlobalGMSysMessage("[GM频道]: 没有输入奖励模板编号");
            return false;
        }

        std::map<std::string, Player*> MCheck;

        SessionMap const& Sessions = sWorld->GetAllSessions();
        for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
        {
            if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
                continue;

            auto It = MCheck.find(Itr->second->GetRemoteAddress());
            if (It == MCheck.end())
                MCheck.insert(std::make_pair(Itr->second->GetRemoteAddress(), Itr->second->GetPlayer()));
        }

        for (auto Itr = MCheck.begin(); Itr != MCheck.end(); Itr++)
        {
            Player* player = Itr->second;
            if (player && player->IsInWorld())
            {
                player->SendSysMsg("[系统]: 恭喜你获得全服发放的奖励");
                sHelper->DoTheReward(player, RTempId, "GM全服奖励");
            }
        }

        MCheck.clear();

        handler->SendGlobalGMSysMessage("[GM频道]: >>>>> 成功发放奖励完毕 <<<<<");
        return false;
    }

    static bool HandleWChatCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        bool OnChat = sHelper->GetBoolean(1012);
        uint32 NTempId = uint32(sHelper->GetUInt(1014));
        uint32 Delay = uint32(sHelper->GetUInt(1013));

        if (!player->CanSpeak())
            return false;

        if (!OnChat)
        {
            handler->PSendSysMessage(sHelper->GetGts(1002));
            return false;
        }

        if (Delay > 0)
        {
            time_t Current = time(NULL);
            if (player->WChatDelay < Current)
            {
                player->WChatDelay = Current + Delay;
                if (sHelper->CheckNeed(player, NTempId, "世界聊天消耗"))
                    sHelper->SendSerMessage(sHelper->GetGts(1066), sHelper->GetWorldChatPlayerChatInfo(player).c_str(), args);
            }
            else
            {
                uint32 CheckTime = player->WChatDelay - Current;
                handler->PSendSysMessage(sHelper->GetGts(1067), CheckTime <= 0 ? 1 : CheckTime);
            }
        }
        else
        {
            if (sHelper->CheckNeed(player, NTempId, "世界聊天消耗"))
                sHelper->SendSerMessage(sHelper->GetGts(1066), sHelper->GetWorldChatPlayerChatInfo(player).c_str(), args);
        }

        return false;
    }

    static bool HandleAsInfoCommand(ChatHandler* handler/*, char const* args*/)
    {
        handler->SetSentErrorMessage(true);

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
            playerTarget = player;

        uint32 VipRank = playerTarget->GetVipRank();
        uint64 Token = playerTarget->GetToken();
        uint64 AsToken = playerTarget->GetAsToken();
        uint32 NsRank = playerTarget->GetTempAtUInt32(AT4001);
        player->SendSysMsg(sHelper->GetGts(1054), sHelper->GetNameLinkColor(playerTarget).c_str(), VipRank);
        player->SendSysMsg(sHelper->GetGts(1055), sHelper->GetNameLinkColor(playerTarget).c_str(), Token, AsToken);
        player->SendSysMsg(sHelper->GetGts(1056), sHelper->GetNameLinkColor(playerTarget).c_str(), NsRank);
        return false;
    }

    static bool HandleJFApplyCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
            playerTarget = player;

        char* Input = strtok((char*)args, " ");
        int64 Count = int64(atoll(Input));
        if (Count == 0)
            return false;

        uint32 AccountId = playerTarget->GetSession()->GetAccountId();
        int64 OldToken = int64(playerTarget->GetToken());
        int64 NowToken = OldToken + Count;
        NowToken = NowToken > 0 ? NowToken : 0;

        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN);
        Utmt->SetData(0, NowToken);
        Utmt->SetData(1, AccountId);
        LoginDatabase.Execute(Utmt);

        sHelper->LogPlayerTokenNow(player->GetGUID(), Count, "GM:jf");

        playerTarget->SendSysMsg(sHelper->GetGts(1007), (Count < 0) ? "减少" : "增加", (Count > 0) ? Count : (OldToken < abs(Count)) ? OldToken : abs(Count), NowToken);
        if (player->GetGUID() != playerTarget->GetGUID())
            player->SendSysMsg(sHelper->GetGts(1050), (Count < 0) ? "减少" : "增加", sHelper->GetNameLinkColor(playerTarget).c_str(), (Count > 0) ? Count : (OldToken < abs(Count)) ? OldToken : abs(Count), NowToken);

        return false;
    }

    /// <summary>
    /// 魔币
    /// </summary>
    /// <param name="handler"></param>
    /// <param name="args"></param>
    /// <returns></returns>
    static bool HandleMBApplyCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
            playerTarget = player;

        char* Input = strtok((char*)args, " ");
        int64 Count = int64(atoll(Input));
        if (Count == 0)
            return false;

        uint32 AccountId = playerTarget->GetSession()->GetAccountId();
        int64 OldToken = int64(playerTarget->GetTokenMB());
        int64 NowToken = OldToken + Count;
        NowToken = NowToken > 0 ? NowToken : 0;

        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN_MB);
        Utmt->SetData(0, NowToken);
        Utmt->SetData(1, AccountId);
        LoginDatabase.Execute(Utmt);

        sHelper->LogPlayerTokenNow(player->GetGUID(), Count, "[魔币]GM:mb");

        std::string action = (Count < 0) ? "减少" : "增加";
        uint32 changeCount = (Count > 0) ? Count : (OldToken < abs(Count)) ? OldToken : abs(Count);
        playerTarget->SendSysMsg("[温馨提示]: 您%s了 |cFFFF6699%u|r 魔币 当前魔币: |cFFFF6699%u|r", action.c_str(), changeCount, NowToken);
        if (player->GetGUID() != playerTarget->GetGUID())
            player->SendSysMsg("[GM频道]: 您使用命令 %s了 %s 的 |cFFFF6699%u|r 魔币 当前魔币 |cFFFF6699%u|r", action.c_str(), sHelper->GetNameLinkColor(playerTarget).c_str(), changeCount, NowToken);

        return false;
    }

    /// <summary>
    /// 泡点命令
    /// </summary>
    static bool HandlePDApplyCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
            playerTarget = player;

        char* Input = strtok((char*)args, " ");
        int64 Count = int64(atoll(Input));
        if (Count == 0)
            return false;

        uint64 OldPaodian = playerTarget->GetPaodian();
        playerTarget->AddPaodian(Count, "GM命令");
        uint64 NowPaodian = playerTarget->GetPaodian();

        std::string action = (Count < 0) ? "减少" : "增加";
        uint64 changeCount = (Count > 0) ? Count : (OldPaodian < uint64(llabs(Count))) ? OldPaodian : uint64(llabs(Count));

        if (player->GetGUID() != playerTarget->GetGUID())
            player->SendSysMsg("[GM频道]: 您使用命令 %s了 %s 的 |cFFFF6699%llu|r 泡点 当前泡点 |cFFFF6699%llu|r", action.c_str(), sHelper->GetNameLinkColor(playerTarget).c_str(), changeCount, NowPaodian);

        return false;
    }

    /// <summary>
    /// 魔币累计
    /// </summary>
    /// <param name="handler"></param>
    /// <param name="args"></param>
    /// <returns></returns>
    static bool HandleMBAsApplyCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
            playerTarget = player;

        char* Input = strtok((char*)args, " ");
        int64 Count = int64(atoll(Input));
        if (Count == 0)
            return false;

        uint32 AccountId = playerTarget->GetSession()->GetAccountId();
        int64 OldToken = int64(playerTarget->GetTokenMB());
        int64 NowToken = OldToken + Count;
        NowToken = NowToken > 0 ? NowToken : 0;

        int64 OldAsToken = int64(playerTarget->GetAsTokenMB());
        int64 NowAsToken = OldAsToken + Count;
        NowAsToken = NowAsToken > 0 ? NowAsToken : 0;

        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN_ASMB);
        Utmt->SetData(0, NowToken);
        Utmt->SetData(1, NowAsToken);
        Utmt->SetData(2, AccountId);
        LoginDatabase.Execute(Utmt);

        sHelper->LogPlayerTokenNow(player->GetGUID(), Count, "[魔币]GM:mbas");

        playerTarget->SendSysMsg("[温馨提示]: 您%s了 |cFFFF6699%u|r 魔币 当前魔币: |cFFFF6699%u|r 累计魔币 %u", (Count < 0) ? "减少" : "增加", (Count > 0) ? Count : (OldToken < abs(Count)) ? OldToken : abs(Count), NowToken, NowAsToken);
        if (player->GetGUID() != playerTarget->GetGUID())
            player->SendSysMsg("[GM频道]: 您使用命令 %s了 %s 的 |cFFFF6699%u|r 魔币 当前魔币 |cFFFF6699%u|r 累计魔币 %u", (Count < 0) ? "减少" : "增加", sHelper->GetNameLinkColor(playerTarget).c_str(), (Count > 0) ? Count : (OldToken < abs(Count)) ? OldToken : abs(Count), NowToken, NowAsToken);

        return false;
    }

    static bool HandleJFAsApplyCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
            playerTarget = player;

        char* Input = strtok((char*)args, " ");
        int64 Count = int64(atoll(Input));
        if (Count == 0)
            return false;

        uint32 AccountId = playerTarget->GetSession()->GetAccountId();
        int64 OldToken = int64(playerTarget->GetToken());
        int64 NowToken = OldToken + Count;
        NowToken = NowToken > 0 ? NowToken : 0;

        int64 OldAsToken = int64(playerTarget->GetAsToken());
        int64 NowAsToken = OldAsToken + Count;
        NowAsToken = NowAsToken > 0 ? NowAsToken : 0;

        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN_AS);
        Utmt->SetData(0, NowToken);
        Utmt->SetData(1, NowAsToken);
        Utmt->SetData(2, AccountId);
        LoginDatabase.Execute(Utmt);

        sHelper->LogPlayerTokenNow(player->GetGUID(), Count, "GM:jfas");

        playerTarget->SendSysMsg("[温馨提示]: 您%s了 |cFFFF6699%u|r 积分 当前积分: |cFFFF6699%u|r 累计积分 %u", (Count < 0) ? "减少" : "增加", (Count > 0) ? Count : (OldToken < abs(Count)) ? OldToken : abs(Count), NowToken, NowAsToken);
        if (player->GetGUID() != playerTarget->GetGUID())
            player->SendSysMsg("[GM频道]: 您使用命令 %s了 %s 的 |cFFFF6699%u|r 积分 当前积分 |cFFFF6699%u|r 累计积分 %u", (Count < 0) ? "减少" : "增加", sHelper->GetNameLinkColor(playerTarget).c_str(), (Count > 0) ? Count : (OldToken < abs(Count)) ? OldToken : abs(Count), NowToken, NowAsToken);

        return false;
    }

    static bool HandleVipApplyCommand(ChatHandler* handler, char const* args)
    {
        handler->SetSentErrorMessage(true);

        if (!*args)
            return false;

        Player* player = handler->GetSession()->GetPlayer();
        Player* playerTarget = handler->getSelectedPlayer();
        if (!playerTarget)
            playerTarget = player;

        char* Input = strtok((char*)args, " ");
        int32 Value = int32(atoi(Input));
        if (Value < 0 || Value == playerTarget->VipRank)
            return false;

        playerTarget->SendSysMsg(sHelper->GetGts(1008), (uint32(Value) > playerTarget->VipRank) ? "提升" : "降低", uint32(Value));
        if (player->GetGUID() != playerTarget->GetGUID())
            player->SendSysMsg(sHelper->GetGts(1051), sHelper->GetNameLinkColor(playerTarget).c_str(), (uint32(Value) > playerTarget->VipRank) ? "提升" : "降低", uint32(Value));

        if (sHelper->GetBoolean(13901) && uint32(Value) > playerTarget->VipRank)
            sHelper->SendSerSpecialMessage(sHelper->GetGts(7100), sHelper->GetNameLinkColor(playerTarget).c_str(), uint32(Value));

        playerTarget->VipRank = uint32(Value);
        playerTarget->VLootRate = sPlayerModMgr->GetVipLootOdds(playerTarget->VipRank);
        playerTarget->VLootKn = sPlayerModMgr->GetVipLootKn(playerTarget->VipRank);
        playerTarget->CastSpell(playerTarget, TxLevelSpell, false);

        uint32 AccountId = playerTarget->GetSession()->GetAccountId();
        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_VIP_RANK);
        Utmt->SetData(0, playerTarget->VipRank);
        Utmt->SetData(1, AccountId);
        LoginDatabase.Execute(Utmt);

        ObjectGuid Guid = playerTarget->GetGUID();
        std::string Prefix = sPlayerModMgr->GetVipPixStr(playerTarget->VipRank);
        sCharacterCache->DoUpdateNamePrefix(Guid, Prefix);
        sPlayerModMgr->UpdateNameQuery(playerTarget);

        playerTarget->SetCanModifyStats(true);
        playerTarget->UpdateAllStats();

        return false;
    }

    static void ExecuteModuleReloadCommand(ChatHandler* handler, char const* command)
    {
        if (!handler || !command || !*command)
            return;

        handler->ParseCommands(command);
    }

    static bool HandleDoReloadModulesCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);
        handler->SendSysMessage("开始重载 modules 自定义模块...");

        ExecuteModuleReloadCommand(handler, ".物品属性 重载");
        ExecuteModuleReloadCommand(handler, ".强化 重载");
        ExecuteModuleReloadCommand(handler, ".物品成长 重载");
        ExecuteModuleReloadCommand(handler, ".物品套装 重载");
        ExecuteModuleReloadCommand(handler, ".物品技能 重载");
        ExecuteModuleReloadCommand(handler, ".符文 重载");
        ExecuteModuleReloadCommand(handler, ".鉴定 重载");
        ExecuteModuleReloadCommand(handler, ".地图生物 重载");
        ExecuteModuleReloadCommand(handler, ".地图控制 重载");
        ExecuteModuleReloadCommand(handler, ".reload customloot");
        ExecuteModuleReloadCommand(handler, ".自动拾取 重载");
        ExecuteModuleReloadCommand(handler, ".reload eluna");

        handler->SendSysMessage("modules 自定义模块重载命令已执行");
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool HandleDoReloadAllCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        HandleDoReloadGlobalConfigCommand(handler, "");
        HandleDoReloadEnMapAuraDataCommand(handler, "");

        HandleDoReloadPlayerActionCommand(handler, "");
        HandleDoReloadPicGuideMgrCommand(handler, "");
        HandleDoReloadMapAreaFuncCommand(handler, "");
        HandleDoReloadCharPvpTopCommand(handler, "");
        HandleDoReloadAthenaEventCommand(handler, "");
        HandleDoReloadCombatPetCommand(handler, "");
        HandleDoReloadBeastMasterCommand(handler, "");
        HandleDoReloadItemZodiacCommand(handler, "");
        HandleGuildBattleCommand(handler, "");
        HandleItemPlayerStatCommand(handler, "");
        HandleItemCtUpMgrCommand(handler, "");
        HandleDoReloadItemMagicCommand(handler, "");
        HandleDoReloadAuraSpellMgrCommand(handler, "");
        HandleDoReloadAyThroughCommand(handler, "");
        HandleDoReloadArenaSoloCommand(handler, "");
        HandleDoReloadBusMgrCommand(handler, "");
        HandleDoReloadCustomPetCommand(handler, "");
        HandleDoReloadSecretCommand(handler, "");
        HandleDoReloadPVPKModCommand(handler, "");
        HandleDoReloadTalentInfoModCommand(handler, "");
        HandleDoReloadItemRunesMgrCommand(handler, "");
        HandleDoReloadItemRandomAtMgrCommand(handler, "");
        HandleDoReloadGlobalGtsCommand(handler, "");
        HandleDoReloadAsTempCommand(handler, "");
        HandleDoReloadItemFuncCommand(handler, "");
        HandleDoReloadClassModCommand(handler, "");
        HandleDoReloadDmgRankCommand(handler, "");
        HandleDoReloadAsCreatureCommand(handler, "");
        HandleDoReloadNoticeMgrCommand(handler, "");
        HandleDoReloadOnHealByCommand(handler, "");
        HandleDoReloadEnterFBMgrCommand(handler, "");
        HandleDoReloadMultiNpcMgrCommand(handler, "");
        HandleDoReloadSignCommand(handler, "");
        HandleDoReloadShopCommand(handler, "");
        HandleDoReloadBGAsCommand(handler, "");
        HandleDoReloadCustomVendorCommand(handler, "");
        HandleDoReloadReincarnationCommand(handler, "");
        HandleDoReloadBeastCardCommand(handler, "");
        HandleDoReloadChatFilterCommand(handler, "");
        HandleDoReloadGCDWhitelistCommand(handler, "");
        HandleDoReloadAntiGCDHackCommand(handler, "");
        HandleDoReloadStarVeinCommand(handler, "");
        HandleDoReloadModulesCommand(handler, "");

        return false;
    }

    static bool HandleDoReloadGlobalConfigCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sHelper->LoadGlobalConfig();
        LOG_INFO("server.loading", ">> 重载 __控制总表 生效");
        handler->SendGlobalGMSysMessage("重载 __控制总表 生效");

        int32 HouseType = sHelper->GetInt(1005);
        uint32 TokenItem = sHelper->GetUInt(1018);
        uint32 CDays = uint32(sHelper->GetUInt(1051));
        uint32 TDays = uint32(sHelper->GetUInt(1052));
        SessionMap const& Sessions = sWorld->GetAllSessions();
        for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
        {
            if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
                continue;

            sDoAddon->SendPacketToClient(Itr->second->GetPlayer(), "ResetUI");
            sDoAddon->SendPacketToClient(Itr->second->GetPlayer(), "AuctionHouseInfo", std::to_string(HouseType) + "#" + sHelper->GetItemLink(TokenItem));
            sDoAddon->SendPacketToClient(Itr->second->GetPlayer(), "ConfigSignData", std::to_string(CDays) + "$" + std::to_string(TDays) + "$");
            sDoAddon->SendPacketToClient(Itr->second->GetPlayer(), "OnPlayerAliveDead", std::to_string(sHelper->GetUInt(3001)));
            bool customHealthEnabled = sHelper->GetBoolean(17000) || sHelper->GetBoolean(22010);
            sDoAddon->SendPacketToClient(Itr->second->GetPlayer(), "AutoOpenCustomTargetHp", std::to_string(customHealthEnabled ? 1 : 0));

            sItemShopMgr->SendTabNames(Itr->second->GetPlayer());
        }

        if (sHelper->GetBoolean(10002))
        {
            if (!sLFGMgr->IsTesting())
                sLFGMgr->ToggleTesting();
        }

        return false;
    }

    static bool HandleDoReloadGlobalGtsCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sHelper->LoadGlobalGts();
        LOG_INFO("server.loading", ">> 重载 _文本提示表 生效");
        handler->SendGlobalGMSysMessage("重载 _文本提示表 生效");
        return false;
    }

    static bool HandleDoReloadItemFuncCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sItemFuncMgr->LoadItemFunc();

        SessionMap const& Sessions = sWorld->GetAllSessions();
        for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
        {
            if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
                continue;

            sItemFuncMgr->DoNewLuckDataTo(Itr->second->GetPlayer());
        }

        LOG_INFO("server.loading", ">> 重载 物品相关功能 生效");
        handler->SendGlobalGMSysMessage("重载 物品相关功能 生效");

        return false;
    }

    static bool HandleDoReloadAsTempCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sHelper->LoadAsTemp();
        // Attribute templates are copied into the resolved map-control config,
        // so those per-creature copies must be dropped together with the reload.
        sHuanJingSystem->InvalidateResolvedCreatureConfigs();
        LOG_INFO("server.loading", ">> 重载 模版总表 生效");
        handler->SendGlobalGMSysMessage("重载 模版总表 生效");
        return false;
    }

    static bool HandleDoReloadClassModCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sCustomClass->LoadCustomClass();
        LOG_INFO("server.loading", ">> 重载 _玩家职业属性调整表 生效");
        handler->SendGlobalGMSysMessage("重载 _玩家职业属性调整表 生效");

        SessionMap const& Sessions = sWorld->GetAllSessions();
        for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
        {
            if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
                continue;

            sCustomClass->SendConfigStatData(Itr->second->GetPlayer());
        }

        return false;
    }

    static bool HandleDoReloadDmgRankCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sPlayerModMgr->LoadDmgRank();
        LOG_INFO("server.loading", ">> 重载 _生物伤害排行榜表 生效");
        handler->SendGlobalGMSysMessage("重载 _生物伤害排行榜表 生效");
        return false;
    }

    static bool HandleDoReloadAsCreatureCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sAsCreature->LoadAsCreatureStore();
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        uint32 refreshedCount = sHuanJingSystem->ReloadMapCreatureConfigsAndRefresh(player);
        LOG_INFO("server.loading", ">> 重载 生物属性相关 生效");
        handler->PSendSysMessage("重载 生物属性相关生效，已刷新 %u 个附近非战斗生物", refreshedCount);
        return false;
    }

    static bool HandleDoReloadNoticeMgrCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sNoticeMgr->LoadNoticeMgr();
        LOG_INFO("server.loading", ">> 重载 通告相关 生效");
        handler->SendGlobalGMSysMessage("重载 通告相关 生效");
        return false;
    }

    static bool HandleDoReloadOnHealByCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sAuraSpellMgr->LoadCustomSpellMgr();
        LOG_INFO("server.loading", ">> 重载 技能光环相关模块 生效");
        handler->SendGlobalGMSysMessage("重载 _技能光环吸血表 生效");
        return false;
    }

    static bool HandleDoReloadAyThroughCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sAuraSpellMgr->LoadAuraSpellMgr();
        LOG_INFO("server.loading", ">> 重载 技能光环控制相关 生效");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 技能光环控制相关 生效");
        return false;
    }

    static bool HandleDoReloadEnterFBMgrCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sEnterFBMgr->LoadEnterFBMgr();
        LOG_INFO("server.loading", ">> 重载 _进入副本控制表 生效");
        handler->SendGlobalGMSysMessage("重载 _进入副本控制表 生效");
        return false;
    }

    static bool HandleDoReloadMultiNpcMgrCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sMultiNpcMgr->LoadMultiVendorStore();
        LOG_INFO("server.loading", ">> 重载 _综合商人表 生效");
        handler->SendGlobalGMSysMessage("重载 _综合商人表 生效");
        return false;
    }

    static bool HandleDoReloadSignCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sPlayerModMgr->LoadSignMod();
        LOG_INFO("server.loading", ">> 重载 _签到奖励表 生效");
        handler->SendGlobalGMSysMessage("重载 _签到奖励表 生效");

        time_t NowTime = time(NULL);
        tm ATm;
        localtime_r(&NowTime, &ATm);
        uint32 NowYear = ATm.tm_year + 1900;
        uint32 NowMonth = ATm.tm_mon + 1;
        uint32 Days = sHelper->GetDaysInMonth(NowYear, NowMonth);

        uint32 CDays = uint32(sHelper->GetUInt(1051));
        uint32 TDays = uint32(sHelper->GetUInt(1052));

        SessionMap const& Sessions = sWorld->GetAllSessions();
        for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
        {
            if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
                continue;

            sDoAddon->SendPacketToClient(Itr->second->GetPlayer(), "ConfigSignData", std::to_string(CDays) + "$" + std::to_string(TDays) + "$");

            for (auto It = SignRewardStore.begin(); It != SignRewardStore.end(); It++)
            {
                sDoAddon->SendPacketToClient(Itr->second->GetPlayer(), "RewardSignData", std::to_string(It->first) + "$"
                    + std::to_string(It->second.ItemId1) + "#"
                    + std::to_string(It->second.Count1) + "#"
                    + std::to_string(It->second.ItemId2) + "#"
                    + std::to_string(It->second.Count2) + "#"
                    + std::to_string(It->second.ItemId3) + "#"
                    + std::to_string(It->second.Count3) + "#"
                    + std::to_string(It->second.ItemId4) + "#"
                    + std::to_string(It->second.Count4) + "#"
                    + std::to_string(It->second.ItemId5) + "#"
                    + std::to_string(It->second.Count5) + "#"
                    + std::to_string(It->second.ItemId6) + "#"
                    + std::to_string(It->second.Count6) + "#"
                );
            }
        }

        return false;
    }

    static bool HandleDoReloadShopCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sItemShopMgr->LoadItemShop();
        LOG_INFO("server.loading", ">> 重载 _在线商城出售表 生效");
        handler->SendGlobalGMSysMessage("重载 _在线商城出售表 生效");

        SessionMap const& Sessions = sWorld->GetAllSessions();
        for (SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
        {
            if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
                continue;

            sDoAddon->SendPacketToClient(Itr->second->GetPlayer(), "CloseItemShopFrame");
        }

        return false;
    }

    static bool HandleDoReloadBGAsCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);
        sBGAs->LoadBGConfig();
        LOG_INFO("server.loading", ">> 重载 _战场控制表 生效");
        handler->SendGlobalGMSysMessage("重载 _战场控制表 生效");

        return false;
    }

    static bool HandleDoReloadCustomVendorCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sCustomVendorMgr->LoadCustomVendorStore();
        LOG_INFO("server.loading", ">> 重载 _生物自定义售卖表 生效");
        handler->SendGlobalGMSysMessage("重载 _生物自定义售卖表 生效");

        return false;
    }

    static bool HandleDoReloadReincarnationCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sReincarnationMgr->Reload();

        return false;
    }

    static bool HandleDoReloadBeastCardCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);
        sBeastCardMgr->Reload();
        return false;
    }

    static bool HandleDoReloadChatFilterCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sChatFilterMgr->Reload();
        LOG_INFO("server.loading", ">> 重载 __聊天敏感词列表 生效");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 __聊天敏感词列表 生效");

        return false;
    }

    static bool HandleDoReloadGCDWhitelistCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sSpellCooldownMgr->Reload();
        LOG_INFO("server.loading", ">> 重载 无GCD检查白名单 生效");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 无GCD检查白名单 生效");

        return false;
    }

    static bool HandleDoReloadAntiGCDHackCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sAntiGCDHackMgr->Reload();
        LOG_INFO("server.loading", ">> 重载 反GCD工具配置 生效");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 反GCD工具配置 生效");

        return false;
    }

    static bool HandleDoReloadStarVeinCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->SetSentErrorMessage(true);

        sStarVeinMgr->Reload();
        LOG_INFO("server.loading", ">> 重载 [_星脉衍化节点配置] 生效");
        handler->SendGlobalGMSysMessage("[GM频道]: 重载 [_星脉衍化节点配置] 生效");

        return false;
    }
};

void AddSC_CustomCmdScript()
{
    new Master_CommandScript();
}
