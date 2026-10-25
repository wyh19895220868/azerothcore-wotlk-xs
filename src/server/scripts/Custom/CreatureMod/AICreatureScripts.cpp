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
#include <AI/ScriptedAI/ScriptedCreature.h>
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include "AICreatureScripts.h"
#pragma execution_character_set("utf-8")

CreatureScriptVe CreatureScriptVect;

enum EventTask
{
    EVENT_CHECK_HEALTH = 1,
};

class CustomCreatureScript : public CreatureScript
{
public:
    CustomCreatureScript() : CreatureScript("CustomCreatureScript") { }

    struct CustomCreatureScriptAI : public ScriptedAI
    {
        CustomCreatureScriptAI(Creature* creature) : ScriptedAI(creature), Summons(me) {}
        SummonList Summons;
        std::vector<ActionTemplate> ActionVect;

        bool Hp_90_Active;
        bool Hp_75_Active;
        bool Hp_50_Active;
        bool Hp_25_Active;
        bool Hp_10_Active;

        void Reset() override
        {
            DoEvents.Reset();
            Summons.DespawnAll();

            Hp_90_Active = false;
            Hp_75_Active = false;
            Hp_50_Active = false;
            Hp_25_Active = false;
            Hp_10_Active = false;

            ActionVect.clear();

            uint32 Count = 2;

            for (std::vector<CreatureScriptTemplate>::iterator Itr = CreatureScriptVect.begin(); Itr != CreatureScriptVect.end(); Itr++)
            {
                if (Itr->Entry == me->GetEntry())
                {
                    ActionTemplate Temp;
                    Temp.EventId = Count;
                    Temp.EventType = Itr->EventType;
                    Temp.EventPhase = Itr->EventPhase;
                    Temp.DelayTime = Itr->DelayTime;
                    Temp.RepeatMinTime = Itr->RepeatMinTime;
                    Temp.RepeatMaxTime = Itr->RepeatMaxTime;
                    Temp.ActionType = Itr->ActionType;
                    Temp.ActionParam1 = Itr->ActionParam1;
                    Temp.ActionParam2 = Itr->ActionParam2;
                    Temp.ActionParam3 = Itr->ActionParam3;
                    ActionVect.push_back(Temp);

                    Count++;
                }
            }

            SetPhase(0);

            TriggerEvent(EVENT_TYPE_RESET, me);
        }


        void AddEvent(CreatureEventTypes EventType)
        {
            for (std::vector<ActionTemplate>::iterator Itr = ActionVect.begin(); Itr != ActionVect.end(); Itr++)
            {
                if (Itr->EventType == EventType)
                    DoEvents.ScheduleEvent(Itr->EventId, Milliseconds(Itr->DelayTime));
            }
        }

        void TriggerEvent(uint32 EventId)
        {
            for (std::vector<ActionTemplate>::iterator Itr = ActionVect.begin(); Itr != ActionVect.end(); Itr++)
            {
                if (Itr->EventId != EventId)
                    continue;

                if (Itr->RepeatMinTime > 0 && Itr->RepeatMaxTime == Itr->RepeatMinTime)
                    DoEvents.ScheduleEvent(Itr->EventId, Milliseconds(Itr->RepeatMinTime));

                if (Itr->RepeatMinTime > 0 && Itr->RepeatMaxTime > Itr->RepeatMinTime)
                    DoEvents.ScheduleEvent(Itr->EventId, Milliseconds(urand(Itr->RepeatMinTime, Itr->RepeatMaxTime)));

                if (Itr->EventPhase == 0 || Itr->EventPhase == GetPhase())
                    Action(Itr->ActionType, Itr->ActionParam1, Itr->ActionParam2, Itr->ActionParam3, me);
            }
        }

        void TriggerEvent(CreatureEventTypes EventType, Unit* TUnit)
        {
            for (std::vector<ActionTemplate>::iterator Itr = ActionVect.begin(); Itr != ActionVect.end(); Itr++)
                if (Itr->EventType == EventType && Itr->EventPhase == GetPhase())
                    Action(Itr->ActionType, Itr->ActionParam1, Itr->ActionParam2, Itr->ActionParam3, TUnit);
        }


        void UpdateAI(uint32 Diff) override
        {
            if (!UpdateVictim())
                return;

            DoEvents.Update(Diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            while (uint32 AtEventId = DoEvents.ExecuteEvent())
            {
                if (AtEventId == EVENT_CHECK_HEALTH)
                {
                    if (!Hp_90_Active && me->HealthBelowPct(90))
                    {
                        Hp_90_Active = true;
                        TriggerEvent(EVENT_TYPE_HP_90, me);
                    }

                    if (!Hp_75_Active && me->HealthBelowPct(75))
                    {
                        Hp_75_Active = true;
                        TriggerEvent(EVENT_TYPE_HP_75, me);
                    }

                    if (!Hp_50_Active && me->HealthBelowPct(50))
                    {
                        Hp_50_Active = true;
                        TriggerEvent(EVENT_TYPE_HP_50, me);
                    }

                    if (!Hp_25_Active && me->HealthBelowPct(25))
                    {
                        Hp_25_Active = true;
                        TriggerEvent(EVENT_TYPE_HP_25, me);
                    }

                    if (!Hp_10_Active && me->HealthBelowPct(10))
                    {
                        Hp_10_Active = true;
                        TriggerEvent(EVENT_TYPE_HP_10, me);
                    }

                    DoEvents.ScheduleEvent(EVENT_CHECK_HEALTH, 1s);
                }

                TriggerEvent(AtEventId);
            }

            DoMeleeAttackIfReady();
        }

        void EnterCombat(Unit* /*Victim*/) override
        {
            AddEvent(EVENT_TYPE_COMBAT);
            DoEvents.ScheduleEvent(EVENT_CHECK_HEALTH, 1s);
        }

        void KilledUnit(Unit* /*Victim*/) override
        {
            AddEvent(EVENT_TYPE_KILL);
        }

        void JustDied(Unit* Killer) override
        {
            TriggerEvent(EVENT_TYPE_DIED, Killer);
            Summons.DespawnAll();
        }

        void JustSummoned(Creature* Summoned) override
        {
            Summoned->GetMotionMaster()->MoveRandom(5.0f);
            Summons.Summon(Summoned);
        }

        void SummonedCreatureDies(Creature* Summoned, Unit* /*Killer*/) override
        {
            Summons.Despawn(Summoned);
        }


        void SummonedCreatureDespawn(Creature* Summoned) override
        {
            Summons.Despawn(Summoned);
        }

        void Action(CreatureActionTypes ActionType, std::string ActionParam1, int32 ActionParam2, int32 ActionParam3, Unit* TUnit)
        {
            switch (ActionType)
            {
            case ACTION_TYPE_CAST_SPELL:
            {
                if (!me->IsAlive())
                    break;

                uint32 Spellid1 = atoi(ActionParam1.c_str());
                uint32 Spellid2 = ActionParam2;

                if (Spellid1 != 0 && Spellid2 != 0)
                    urand(0, 1) == 0 ? DoCast(Spellid1) : DoCast(Spellid2);
                else if (Spellid1 != 0)
                    DoCast(Spellid1);
                else if (Spellid2 != 0)
                    DoCast(Spellid2);
            }
            break;
            case ACTION_TYPE_TALK:
                switch (ActionParam2)
                {
                case 1:
                    me->Say(ActionParam1.c_str(), LANG_UNIVERSAL);
                    break;
                case 2:
                    me->Yell(ActionParam1.c_str(), LANG_UNIVERSAL);
                    break;
                case 3:
                    if (Unit* Victim = me->GetVictim())
                        if (Player* Plr = Victim->ToPlayer())
                            me->Whisper(ActionParam1.c_str(), LANG_UNIVERSAL, Plr);
                    break;
                case 4:
                    me->GetMap()->SendZoneText(me->GetZoneId(), ActionParam1.c_str());
                    break;
                case 5:
                {
                    sHelper->SendSerNewMessage(ActionParam1.c_str());
                }
                break;
                case 6:
                {
                    sHelper->SendSerNewMessage(ActionParam1.c_str());
                    me->GetMap()->SendZoneText(me->GetZoneId(), ActionParam1.c_str());
                }
                break;
                case 7:
                    sHelper->SendScreenMessage(ActionParam1.c_str());
                    break;
                default:
                    me->Say(ActionParam1.c_str(), LANG_UNIVERSAL);
                    break;
                }
                break;
            case ACTION_TYPE_ACTIVE_OR_DEACTIVE_OBJECT:
            {
                int32 Param1 = atoi(ActionParam1.c_str());
                bool IsCreature = (ActionParam2 == 0);
                bool Active = (Param1 > 0);
                if (ActionParam3 < 0)
                {
                    uint32 Entry = abs(Param1);
                    if (IsCreature)
                    {
                        if (Creature* Cre = TUnit->FindNearestCreature(Entry, 500.0f))
                            Cre->RemoveFromWorld();
                    }
                    else
                    {
                        if (GameObject* Go = TUnit->FindNearestGameObject(Entry, 500.0f))
                            Go->RemoveFromWorld();
                    }
                }
                else
                {
                    uint32 GuidLow = abs(Param1);
                    if (IsCreature)
                    {
                        if (CreatureData const* CData = sObjectMgr->GetCreatureData(GuidLow))
                        {
                            auto CreBounds = me->GetMap()->GetCreatureBySpawnIdStore().equal_range(GuidLow);
                            if (CreBounds.first != CreBounds.second)
                            {
                                if (Creature* Cre = CreBounds.first->second)
                                    Active ? Cre->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE) : Cre->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE);
                            }
                        }
                    }
                    else
                    {
                        if (GameObjectData const* CData = sObjectMgr->GetGameObjectData(GuidLow))
                        {
                            auto GoBounds = me->GetMap()->GetGameObjectBySpawnIdStore().equal_range(GuidLow);
                            if (GoBounds.first != GoBounds.second)
                            {
                                if (GameObject* Go = GoBounds.first->second)
                                    Active ? Go->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE) : Go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE);
                            }
                        }
                    }
                }
            }

            break;
            case ACTION_TYPE_SUMMON:
            {
                int32 Entry = atoi(ActionParam1.c_str());
                Position Pos = me->GetPosition();
                if (TUnit)
                {
                    if (Entry > 0)
                        TUnit->SummonCreature(Entry, Pos, TEMPSUMMON_TIMED_DESPAWN, ActionParam2 * IN_MILLISECONDS);
                    else
                        TUnit->SummonGameObject(abs(Entry), Pos.GetPositionX(), Pos.GetPositionY(), Pos.GetPositionZ(), Pos.GetOrientation(), 0, 0, 0, 0, ActionParam2);
                }
            }
            break;
            case ACTION_TYPE_ALT_GOB:
                if (GameObject* Go = me->FindNearestGameObject(abs(atoi(ActionParam1.c_str())), ActionParam2))
                    Go->SetGoState(GO_STATE_ACTIVE);
                break;
            case ACTION_TYPE_ALT_PHASE:
                SetPhase(atoi(ActionParam1.c_str()));
                break;
            case ACTION_TYPE_MOVE_RANDOM:
                me->GetMotionMaster()->MoveRandom(atoi(ActionParam1.c_str()));
                break;
            default:
                break;
            }
        }

        void SetPhase(uint32 Phase) { DoPhase = Phase; }
        uint32 GetPhase() { return DoPhase; }

    private:
        EventMap DoEvents;
        uint32 DoPhase;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new CustomCreatureScriptAI(creature);
    }
};

void AddSC_CustomCreatureScript()
{
    new CustomCreatureScript();
}
