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
#include <CharacterCache.h>
#include <algorithm>
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include "AthenaEventMgr.h"
#include "ScriptedCreature.h"
#include <Time\GameTime.h>
#include <Custom/EventMgr/EventMgr.h>
#include <MapMgr.h>
#pragma execution_character_set("utf-8")

CreAthenaTempMap CreAthenaTempStore;
CreAthenaConfigMap CreAthenaConfigStore;

void AthenaEventMgr::LoadAthenaEventData()
{
    CreAthenaTempStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 阶段, 生物编号, 移动速度, 刷新波数, 刷新间隔 FROM _守卫雅典娜功能表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Stage = Fields[0].Get<uint32>();
            CreAthenaTemp St;
            St.CreEntry = Fields[1].Get<uint32>();
            St.MoveSpeed = Fields[2].Get<float>();
            St.OnRspCount = Fields[3].Get<uint32>();
            St.RspInterval = Fields[4].Get<uint32>();

            CreAthenaTempStore.insert(std::make_pair(Stage, St));
        } while (Result->NextRow());
    }

    CreAthenaConfigStore.clear();
    Result = WorldDatabase.Query("SELECT 序号, 配置值 FROM _守卫雅典娜配置表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Id = Fields[0].Get<uint32>();
            std::string Value = Fields[1].Get<std::string>();
            CreAthenaConfigStore.insert(std::make_pair(Id, Value));
        } while (Result->NextRow());
    }
}

uint32 AthenaEventMgr::GetAthenaConfig(uint32 Index)
{
    auto Itr = CreAthenaConfigStore.find(Index);
    if (Itr != CreAthenaConfigStore.end())
        return uint32(atoi(Itr->second.c_str()));

    return 0;
}

uint32 AthenaEventMgr::GetAthenaEntryCountAs()
{
    uint32 Mod = 0;
    for (int8 Index = 0; Index < 8; Index++)
    {
        uint32 PosTempId = GetAthenaConfig(111 + Index);
        if (PosTempId)
            Mod++;
    }

    uint32 An = 0;
    for (auto Itr = CreAthenaTempStore.begin(); Itr != CreAthenaTempStore.end(); Itr++)
        An += Itr->second.OnRspCount;

    return uint32(Mod * An);
}

bool AthenaEventMgr::InAthenaEntry(uint32 Entry)
{
    bool In = false;
    for (auto Itr = CreAthenaTempStore.begin(); Itr != CreAthenaTempStore.end(); Itr++)
    {
        if (Itr->second.CreEntry == Entry)
        {
            In = true;
            break;
        }
    }

    return In;
}

void AthenaEventMgr::AtOnCreatureUpdate(Creature* Me, uint32 Diff)
{

}

class OnAthenaCreatrure : public CreatureScript
{
public:
    OnAthenaCreatrure() : CreatureScript("NpcAthenaEventScript") {  }

    struct OnAthenaCreatrureAI : public ScriptedAI
    {
        OnAthenaCreatrureAI(Creature* creature) : ScriptedAI(creature), Summons(me) { }

        SummonList Summons;

        uint32 MultipleIndex = 1;
        uint32 RspCount = 0;
        uint32 SumTime = 0;
        uint32 AsCount = 0;

        void AutoSummons()
        {
            if (!AsCount)
                return;
            auto Itr = CreAthenaTempStore.find(MultipleIndex);
            if (Itr == CreAthenaTempStore.end())
                return;

            if (MultipleIndex == 1 && RspCount == 1)
            {
                uint32 BossId = sAthenaEventMgr->GetAthenaConfig(102);
                uint32 PosTempId = sAthenaEventMgr->GetAthenaConfig(103);
                if (BossId && PosTempId)
                {
                    uint32 map = 0;
                    float x = 0;
                    float y = 0;
                    float z = 0;
                    float o = 0;
                    sEvent->GetTelePos(PosTempId, map, x, y, z, o);
                    Position Pos;
                    Pos.Relocate(x, y, z, o);
                    if (Creature* BCre = me->SummonCreature(BossId, Pos))
                    {
                        BCre->SetFaction(1775);
                        BCre->SelectLevel(false);
                    }
                }
            }

            for (int8 Index = 0; Index < 8; Index++)
            {
                uint32 PosTempId = sAthenaEventMgr->GetAthenaConfig(111 + Index);
                if (PosTempId)
                {
                    uint32 map = 0;
                    float x = 0;
                    float y = 0;
                    float z = 0;
                    float o = 0;
                    sEvent->GetTelePos(PosTempId, map, x, y, z, o);
                    Position Pos;
                    Pos.Relocate(x, y, z, o);
                    if (Creature* Cre = me->SummonCreature(Itr->second.CreEntry, Pos))
                    {
                        Cre->SelectLevel(false);
                        Cre->GetMotionMaster()->MoveCharge(me->GetPositionX() + frand(-2.0f, 2.0f), me->GetPositionY() + frand(-2.0f, 2.0f), me->GetPositionZ(), Itr->second.MoveSpeed);
                    }
                }
            }

            std::ostringstream Nss;
            Nss << "第 [" << MultipleIndex << "] 阶段 第 [" << RspCount << "] 波 怪物来袭";
            me->Say(Nss.str().c_str(), LANG_UNIVERSAL);
        }

        void JustDied(Unit* /*killer*/) override
        {
            MultipleIndex = 1;
            RspCount = 0;
            SumTime = 0;
            Summons.DespawnAll();
        }

        void JustSummoned(Creature* Summoned) override
        {
            Summons.Summon(Summoned);

            sAthenaEventMgr->SumCreList.insert(Summoned->GetGUID());
            sAthenaEventMgr->SumMapId = Summoned->GetMapId();
            sAthenaEventMgr->SumInstanceId = Summoned->GetInstanceId();
        }

        void SummonedCreatureDies(Creature* Summoned, Unit* /*Killer*/) override
        {
            Summons.Despawn(Summoned);

            sAthenaEventMgr->SumCreList.erase(Summoned->GetGUID());

            if (Summoned->GetEntry() == sAthenaEventMgr->GetAthenaConfig(102))
            {
                sAthenaEventMgr->KillAs = false;
                sGameEventMgr->StopEvent(sAthenaEventMgr->GetAthenaConfig(101), false, true);
                me->Say("守护雅典娜失败了 活动结束", LANG_UNIVERSAL);
                return;
            }

            AsCount--;

            if (AsCount > 0)
                me->Yell(std::string("剩余怪物数量: ") + std::to_string(AsCount), LANG_UNIVERSAL);

            if (AsCount == 0)
            {
                for (auto Iter = sAthenaEventMgr->PlayerInList.begin(); Iter != sAthenaEventMgr->PlayerInList.end(); Iter++)
                    if (Player* player = ObjectAccessor::FindPlayer(*Iter))
                        if (player->GetAreaId() == me->GetAreaId())
                            sHelper->DoTheReward(player, sAthenaEventMgr->GetAthenaConfig(120), "雅典娜奖励");

                sAthenaEventMgr->KillAs = true;
                sGameEventMgr->StopEvent(sAthenaEventMgr->GetAthenaConfig(101), false, true);
                sHelper->SendSerSpecialMessage("[雅典娜活动]: 所有怪物被击杀 守护雅典娜成功");
            }
        }


        void SummonedCreatureDespawn(Creature* Summoned) override
        {
            Summons.Despawn(Summoned);

            // No KillAs early-out here: KillAs is true for the whole running event
            // (set in Reset), so the old guard skipped this erase and left entries
            // pointing at freed summons. OnStop iterates a local copy, so erasing
            // here can no longer invalidate its iterator.
            sAthenaEventMgr->SumCreList.erase(Summoned->GetGUID());
        }

        void Reset() override
        {
            sAthenaEventMgr->AreaId = me->GetAreaId();
            sAthenaEventMgr->KillAs = true;
            MultipleIndex = 1;
            RspCount = 0;
            SumTime = 0;
            AsCount = sAthenaEventMgr->GetAthenaEntryCountAs();
            me->Yell(std::string("总计怪物数量: ") + std::to_string(AsCount), LANG_UNIVERSAL);
            Summons.DespawnAll();
        }

        void UpdateAI(uint32 diff) override
        {
            if (!sGameEventMgr->IsActiveEvent(sAthenaEventMgr->GetAthenaConfig(101)))
                return;

            auto Itr = CreAthenaTempStore.find(MultipleIndex);
            if (Itr != CreAthenaTempStore.end())
            {
                SumTime += diff;
                if (SumTime >= Itr->second.RspInterval * IN_MILLISECONDS)
                {
                    SumTime = 0;
                    RspCount++;
                    AutoSummons();
                    if (RspCount >= Itr->second.OnRspCount)
                    {
                        MultipleIndex++;
                        RspCount = 0;
                    }
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new OnAthenaCreatrureAI(creature);
    }
};

class OnAthenaGameEventScript : public GameEventScript
{
public:
    OnAthenaGameEventScript() : GameEventScript("OnAthenaGameEventScript") { }

    void OnStart(uint16 DoEventId) override
    {
        if (!sAthenaEventMgr->GetAthenaEntryCountAs())
            return;

        if (DoEventId == sAthenaEventMgr->GetAthenaConfig(101))
        {
            sAthenaEventMgr->PlayerInList.clear();
            sAthenaEventMgr->AthenaEventGroupList.clear();
            sAthenaEventMgr->SumCreList.clear();
            SessionMap const& smap = sWorld->GetAllSessions();
            for (SessionMap::const_iterator iter = smap.begin(); iter != smap.end(); ++iter)
            {
                if (!iter->second)
                    continue;

                Player* player = iter->second->GetPlayer();
                if (player && player->IsInWorld())
                {
                    sHelper->PopToPlayerDialog(player, "\n\n雅典娜活动开始了\n\n由此进入或指定NPC对话进入才能获取奖励\n\n", 0, sAthenaEventMgr->GetAthenaConfig(104), AthenaEvent);
                }
            }
        }
    }

    void OnStop(uint16 DoEventId) override
    {
        if (DoEventId == sAthenaEventMgr->GetAthenaConfig(101))
        {
            if (sHelper->GetBoolean(20001))
            {
                for (auto Itr = sAthenaEventMgr->AthenaEventGroupList.begin(); Itr != sAthenaEventMgr->AthenaEventGroupList.end(); Itr++)
                    if (Group* Gp = sGroupMgr->GetGroupByGUID(*Itr))
                        Gp->Disband();
            }

            sAthenaEventMgr->AthenaEventGroupList.clear();

            if (sAthenaEventMgr->KillAs && sAthenaEventMgr->AreaId)
            {
                for (auto Iter = sAthenaEventMgr->PlayerInList.begin(); Iter != sAthenaEventMgr->PlayerInList.end(); Iter++)
                {
                    if (Player* player = ObjectAccessor::FindPlayer(*Iter))
                    {
                        if (player->GetAreaId() == sAthenaEventMgr->AreaId)
                        {
                            sHelper->DoTheReward(player, sAthenaEventMgr->GetAthenaConfig(120), "雅典娜奖励");
                            player->SendSysMsg("[雅典娜活动]: 活动结束 雅典娜存活 守护雅典娜成功");
                        }
                    }
                }
            }

            sAthenaEventMgr->PlayerInList.clear();

            // Take ownership of the set first: removal callbacks may erase from the
            // global set while we iterate, and the set must not outlive the event.
            GuidSet summons;
            summons.swap(sAthenaEventMgr->SumCreList);

            // Resolve each GUID at use time; a summon freed by grid unload simply
            // fails the lookup instead of being dereferenced.
            if (Map* map = sMapMgr->FindMap(sAthenaEventMgr->SumMapId, sAthenaEventMgr->SumInstanceId))
            {
                for (ObjectGuid const& guid : summons)
                {
                    if (Creature* summon = map->GetCreature(guid))
                        if (summon->IsInWorld())
                            summon->AddObjectToRemoveList();
                }
            }
        }
    }
};

class OnNpcStartAthenaCreatrureScript : public CreatureScript
{
public:
    OnNpcStartAthenaCreatrureScript() : CreatureScript("OnNpcStartAthena") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (player->IsInCombat())
        {
            player->SendAsMsg(sHelper->GetGts(1001));
            return true;
        }

        if (!sGameEventMgr->IsActiveEvent(sAthenaEventMgr->GetAthenaConfig(101)))
        {
            return true;
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "点击此处进入 [守护雅典娜]", 0, 0);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "点击此处进入 [守护雅典娜]", 0, 1);

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        if (!Sender && !Action)
        {
            OnGossipHello(player, creature);
            return true;
        }

        uint32 EventId = sAthenaEventMgr->GetAthenaConfig(101);
        if (EventId >= MINEVENTSO)
        {
            player->SendSysMsg("内部错误");
            return true;
        }

        player->UninviteFromGroup();
        if (player->GetGroup())
            player->RemoveFromGroup();

        bool GExist = false;
        for (auto Itr = sAthenaEventMgr->AthenaEventGroupList.begin(); Itr != sAthenaEventMgr->AthenaEventGroupList.end(); Itr++)
        {
            if (Group* Gp = sGroupMgr->GetGroupByGUID(*Itr))
            {
                if (!Gp->IsFull())
                {
                    Gp->AddMember(player);
                    GExist = true;
                    break;
                }
            }
        }

        if (!GExist)
        {
            Group* Gp = new Group();
            Gp->Create(player);
            Gp->ConvertToRaid();
            sGroupMgr->AddGroup(Gp);
            sAthenaEventMgr->AthenaEventGroupList.insert(Gp->GetGUID().ToGuidLow());
        }

        if (sAthenaEventMgr->PlayerInList.find(player->GetGUID()) == sAthenaEventMgr->PlayerInList.end())
            sAthenaEventMgr->PlayerInList.insert(player->GetGUID());

        player->TeleToPos(sAthenaEventMgr->GetAthenaConfig(104));

        return true;
    }
};

void AddSC_AthenaCreatrureEventScripts()
{
    new OnAthenaCreatrure();
    new OnAthenaGameEventScript();
    new OnNpcStartAthenaCreatrureScript();
}
