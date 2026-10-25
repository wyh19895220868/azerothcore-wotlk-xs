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
#include "ScriptMgr.h"
#include "SpellScript.h"
#include "PoolMgr.h"
#include "Config.h"
#include "WorldSession.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"
#include "Pet.h"
#include "Object.h"
#include <CharacterCache.h>
#include <algorithm>
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include <Time\GameTime.h>
#include "OneLifeMgr.h"
#pragma execution_character_set("utf-8")

void OneLifeMgr::OnPlayerDieNow(Player* player)
{
    uint32 AType = sHelper->GetUInt32(15008);
    if (AType == 1)
    {
        player->UpdateTempAt(AT6001, 1);
        player->SendSysMsg(sHelper->GetGts(9100));
        sHelper->SendSerMessage(sHelper->GetGts(9104), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetMapName(player->GetMapId()));
    }
    else if (AType == 2)
    {
        if (player->GetTempAt(AT6000) == 1)
        {
            player->UpdateTempAt(AT6001, 1);
            player->SendSysMsg(sHelper->GetGts(9100));
            sHelper->SendSerMessage(sHelper->GetGts(9104), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetMapName(player->GetMapId()));
        }
    }
}

class OnPlayerOneLife : public PlayerScript
{
public:
    OnPlayerOneLife() : PlayerScript("OnPlayerOneLife") {  }

    void OnLogin(Player* player) override
    {
        if (player->GetTempAt(AT6000) == 1 && player->GetTempAt(AT6001) == 1)
        {
            if (player->IsAlive())
            {
                AsInt64 killDamage = player->GetAsHealth();
                if (killDamage <= 0)
                    killDamage = AsInt64(player->GetHealth());
                player->SetAsDamageOverride(player->GetGUID(), killDamage);
                Unit::DealAsDamage(player, player, player->GetAsHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
            }
            else
                player->SendSysMsg(sHelper->GetGts(9100));
        }

        if (player->IsAlive() && player->GetTempAt(AT6000) == 1 && player->GetTempAt(AT6001) != 1)
        {
            std::string OnData = sHelper->GetStrConfig(15009);
            if (!OnData.empty())
            {
                Tokenizer AuraData(OnData, '#');
                for (auto const& Temp : AuraData)
                {
                    if (!player->HasAura(atoi(Temp)))
                        player->AddAura(atoi(Temp), player);
                }
            }
        }
    }

    bool CanPlayerResurrect(Player* player) override
    {
        if (player->GetTempAt(AT6000) == 1 && player->GetTempAt(AT6001) == 1)
        {
            player->SendSysMsg(sHelper->GetGts(9100));
            return false;
        }

        return true;
    }

    void OnPlayerResurrect(Player* player, float restore_percent, bool& applySickness) override
    {
        if (sHelper->GetUInt32(15008))
        {
            std::string OnData = sHelper->GetStrConfig(15009);
            if (!OnData.empty())
            {
                Tokenizer AuraData(OnData, '#');
                for (auto const& Temp : AuraData)
                {
                    if (player->HasAura(atoi(Temp)))
                        player->RemoveAurasDueToSpell(atoi(Temp));
                }
            }
        }
    }
};

class NpcOneLife : public CreatureScript
{
public:
    NpcOneLife() : CreatureScript("NpcOneLife") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        uint32 AType = sHelper->GetUInt32(15008);
        if (AType != 2)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "\n这里是一命模式服务员\n\n", 0, 0);
        if (player->GetTempAt(AT6001) == 0)
        {
            if (player->GetTempAt(AT6000) == 1)
            {
                CloseGossipMenuFor(player);
                player->SendSysMsg(sHelper->GetGts(9101));
                return true;
            }

            std::string OnData = sHelper->GetStrConfig(15009);
            if (!OnData.empty())
            {
                player->SendSysMsg("--------------------------------------------------");

                Tokenizer AuraData(OnData, '#');
                for (auto const& Temp : AuraData)
                    player->SendSysMsg("[一命模式]: 增益效果 %s", sHelper->GetAtSpellName(atoi(Temp)).c_str());

                player->SendSysMsg("--------------------------------------------------");
            }

            AddGossipItemFor(player, GOSSIP_ICON_TALK, "选择一命模式\n\n", 88, 1, std::string("\n\n|cFF00FF00选择此模式 如果你不幸死亡\n将无法再次复活\n\n为此我们给与你一些增益效果奖励\n聊天窗口查看增益|r\n\n"), 0, false);
        }
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "我被锁定了 需要解除一命模式复活\n\n", 88, 2);
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());

        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        if (Sender == 0 && Action == 0)
        {
            OnGossipHello(player, creature);
            return true;
        }

        ClearGossipMenuFor(player);

        if (Sender != 88)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        if (Action == 1)
        {
            uint32 AType = sHelper->GetUInt32(15008);
            if (AType != 2)
            {
                CloseGossipMenuFor(player);
                return true;
            }

            if (player->GetTempAt(AT6000) == 1)
            {
                CloseGossipMenuFor(player);
                player->SendSysMsg(sHelper->GetGts(9101));
                return true;
            }

            player->UpdateTempAt(AT6000, 1);
            std::string OnData = sHelper->GetStrConfig(15009);
            if (!OnData.empty())
            {
                Tokenizer AuraData(OnData, '#');
                for (auto const& Temp : AuraData)
                {
                    if (!player->HasAura(atoi(Temp)))
                        player->AddAura(atoi(Temp), player);
                }
            }
            player->SendSysMsg(sHelper->GetGts(9102));
        }
        else if (Action == 2)
        {
            uint32 AType = sHelper->GetUInt32(15008);
            if (AType != 2)
            {
                CloseGossipMenuFor(player);
                return true;
            }

            if (player->GetTempAt(AT6001) == 0)
            {
                CloseGossipMenuFor(player);
                return true;
            }

            uint32 NTempId = sHelper->GetUInt32(15010);
            if (sHelper->CheckNeed(player, NTempId, "一命模式复活消耗"))
            {
                player->UpdateTempAt(AT6000, 0);
                player->UpdateTempAt(AT6001, 0);
                player->ResurrectPlayer(1.0f);
                player->SpawnCorpseBones();
                player->SendSysMsg(sHelper->GetGts(9103));
            }
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

void AddSC_PlayerOneLifeScripts()
{
    new OnPlayerOneLife();
    new NpcOneLife();
}
