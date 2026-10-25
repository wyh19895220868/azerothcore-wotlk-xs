#pragma execution_character_set("utf-8")
#include "ReincarnationMgr.h"
#include "Common.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "World.h"
#include "Chat.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "Creature.h"
#include "GossipDef.h"
#include "AI/ScriptedAI/ScriptedGossip.h"
#include <Custom/_Helper/Helper.h>

using namespace Acore::ChatCommands;

ReincarnationTemplateMap ReincarnationTemplateStore;

void ReincarnationMgr::LoadReincarnationTemplate(bool isReload)
{
    ReincarnationTemplateStore.clear();
    uint32 count = 0;

    QueryResult result = WorldDatabase.Query("SELECT 转生等级, 需求模版ID, 奖励模版ID, 对话文本 FROM __转生配置表");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 level = fields[0].Get<uint32>();
            ReincarnationTemplate temp;
            temp.reqId = fields[1].Get<uint32>();
            temp.rewId = fields[2].Get<uint32>();
            temp.gossipText = fields[3].Get<std::string>();
            ReincarnationTemplateStore.insert(std::make_pair(level, temp));
            count++;
        } while (result->NextRow());
    }

    if (!isReload)
        LOG_INFO("server.loading", "功能表 >> 加载 [转生配置表] 总计 {} 条数据", count);
}

void ReincarnationMgr::Reload()
{
    LOG_INFO("server.loading", ">> 重载 转生配置表...");
    LoadReincarnationTemplate(true);
    LOG_INFO("server.loading", ">> 重载 转生配置表 生效");
}

uint32 ReincarnationMgr::GetMaxLevel()
{
    uint32 max = 0;

    if (ReincarnationTemplateStore.empty())
        return max;

    for (auto i = ReincarnationTemplateStore.begin(); i != ReincarnationTemplateStore.end(); i++)
        if (max < i->first)
            max = i->first;

    return max;
}

std::string ReincarnationMgr::GetGossipText(Player* player)
{
    for (auto i = ReincarnationTemplateStore.begin(); i != ReincarnationTemplateStore.end(); i++)
        if (player->reincarnationLv == i->first)
            return i->second.gossipText;

    return "";
}

void ReincarnationMgr::AddGossip(Player* player, Creature* creature)
{
    // 清空之前的Gossip菜单
    ClearGossipMenuFor(player);

    if (!ReincarnationTemplateStore.empty())
    {
        if (player->reincarnationLv >= GetMaxLevel())
        {
            // 已达到最高转生等级
            std::string gossipText = GetGossipText(player);
            if (!gossipText.empty())
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, gossipText.c_str(), 0, 1);
        }
        else
        {
            // 显示当前转生等级信息
            std::string gossipText = GetGossipText(player);
            if (!gossipText.empty())
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, gossipText.c_str(), 0, 1);

            uint32 rewId = 0;
            uint32 reqId = 0;
            GetParams(player, rewId, reqId);

            std::string nextText = "进行转生 (当前转生等级: " + std::to_string(player->reincarnationLv) + ")";

            // 获取需求提示文本
            std::string needText = sHelper->GetNeedGts(reqId);
            if (!needText.empty())
                nextText += "\n" + needText;

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, nextText.c_str(), 0, 2);
        }
    }

    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
}

void ReincarnationMgr::GetParams(Player* player, uint32& rewId, uint32& reqId)
{
    for (auto i = ReincarnationTemplateStore.begin(); i != ReincarnationTemplateStore.end(); i++)
        if (player->reincarnationLv + 1 == i->first)
        {
            rewId = i->second.rewId;
            reqId = i->second.reqId;
        }
}

void ReincarnationMgr::Save(Player* player)
{
    CharacterDatabase.Execute("UPDATE characters SET reincarnationLv = {} WHERE guid = {}", player->reincarnationLv, player->GetGUID().GetCounter());
}

void ReincarnationMgr::Load(Player* player)
{
    if (QueryResult result = CharacterDatabase.Query("SELECT reincarnationLv FROM characters WHERE guid = {}", player->GetGUID().GetCounter()))
        player->reincarnationLv = result->Fetch()[0].Get<uint32>();
    else
        player->reincarnationLv = 0;
}

bool ReincarnationMgr::TryReincarnate(Player* player)
{
    if (!player)
        return false;

    if (ReincarnationTemplateStore.empty())
    {
        player->SendSysMsg("转生配置尚未加载，请联系管理员。");
        return false;
    }

    if (player->reincarnationLv >= GetMaxLevel())
    {
        player->SendSysMsg("你已经达到最高转生等级 [%u]。", GetMaxLevel());
        return false;
    }

    uint32 rewId = 0;
    uint32 reqId = 0;
    GetParams(player, rewId, reqId);
    if (ReincarnationTemplateStore.find(player->reincarnationLv + 1) == ReincarnationTemplateStore.end())
    {
        player->SendSysMsg("下一转配置不存在，请联系管理员检查 __转生配置表。");
        return false;
    }

    // 继续使用核心统一模板，确保NPC、物品、物体和UI入口的扣除/奖励完全一致。
    if (!sHelper->CheckNeed(player, reqId, "转生需求", 1))
        return false;

    sHelper->DoTheReward(player, rewId, "转生奖励", 1);
    player->reincarnationLv++;
    Save(player);

    std::string announce = "|cFFFF0000[转生公告]|r|cff00FF00玩家 |r" + std::string(player->GetName()) +
        "|cff00FF00 成功完成转生！当前转生等级：|r|cFFFF0000" + std::to_string(player->reincarnationLv) + "|r";
    sWorld->SendServerMessage(SERVER_MSG_STRING, announce);
    player->CastSpell(player, 47292, true);
    return true;
}

void ReincarnationMgr::DoAction(Player* player, Creature* creature, uint32 action)
{
    if (action == 1)
    {
        // 返回主菜单
        AddGossip(player, creature);
    }
    else if (action == 2)
    {
        if (TryReincarnate(player))
        {
            CloseGossipMenuFor(player);
        }
        else
        {
            // 需求不满足，返回主菜单
            AddGossip(player, creature);
        }
    }
}

// UI按钮使用的玩家级命令。实际执行仍走 TryReincarnate，不允许绕过需求模板。
class reincarnation_ui_commandscript : public CommandScript
{
public:
    reincarnation_ui_commandscript() : CommandScript("reincarnation_ui_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "reincui", HandleReincarnationUiCommand, SEC_PLAYER, Console::No },
        };
        return commandTable;
    }

    static bool HandleReincarnationUiCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler && handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        sReincarnationMgr->TryReincarnate(player);
        return true;
    }
};

// NPC脚本
class npc_reincarnation : public CreatureScript
{
public:
    npc_reincarnation() : CreatureScript("npc_reincarnation") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        sReincarnationMgr->AddGossip(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        sReincarnationMgr->DoAction(player, creature, action);
        return true;
    }
};

// 玩家脚本 - 登录时加载转生数据
class ReincarnationPlayerScript : public PlayerScript
{
public:
    ReincarnationPlayerScript() : PlayerScript("ReincarnationPlayerScript") { }

    void OnLogin(Player* player) override
    {
        sReincarnationMgr->Load(player);
    }

    void OnPlayerSave(Player* player) override
    {
        sReincarnationMgr->Save(player);
    }
};

// 世界脚本 - 服务器启动完成后的处理
class ReincarnationWorldScript : public WorldScript
{
public:
    ReincarnationWorldScript() : WorldScript("ReincarnationWorldScript") { }

    // 配置已在 World::SetInitialWorldSettings() 中加载
    // 此处不需要重复加载
};

void AddSC_ReincarnationMgr()
{
    new npc_reincarnation();
    new reincarnation_ui_commandscript();
    new ReincarnationPlayerScript();
    new ReincarnationWorldScript();
}
