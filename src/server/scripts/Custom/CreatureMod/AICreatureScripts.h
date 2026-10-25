
#ifndef _AICREATURE_H
#define _AICREATURE_H

enum CreatureEventTypes
{								//						eventPhase			delayTime			repeatMinTime					repeatMaxTime
    EVENT_TYPE_NONE,
    EVENT_TYPE_COMBAT,			//进入战斗									首次触发延时			最小重复时间						最大重复时间
    EVENT_TYPE_KILL,			//杀死玩家									首次触发延时			最小重复时间						最大重复时间
    EVENT_TYPE_HP_90,			//血量低于90%(直接触发）		-					-					-								-
    EVENT_TYPE_HP_75,			//血量低于75%(直接触发)		-					-					-								-
    EVENT_TYPE_HP_50,			//血量低于50%(直接触发)		-					-					-								-
    EVENT_TYPE_HP_25,			//血量低于25%(直接触发)		-					-					-								-
    EVENT_TYPE_HP_10,			//血量低于10%(直接触发)		-					-					-								-
    EVENT_TYPE_DIED,			//死亡(直接触发)				-					-					-								-
    EVENT_TYPE_RESET,			//重置(直接触发)				-					-					-								-
};

enum CreatureActionTypes
{												//						actionParam1					actionParam2			actionParam3
    ACTION_TYPE_NONE,
    ACTION_TYPE_CAST_SPELL,						//释放技能				SPELL_ID_1,						SPELL_ID_2,				-
    ACTION_TYPE_TALK,							//说话					说话内容								-
    ACTION_TYPE_ACTIVE_OR_DEACTIVE_OBJECT,		//激活					CreatureEntry					GameObjectEntry			-
    ACTION_TYPE_SUMMON,							//召唤生物或物体			CreatureEntry或-	GameObjectEntry	持续时间					坐标ID
    ACTION_TYPE_ALT_PHASE,						//设定阶段				eventPhase							-
    ACTION_TYPE_ALT_GOB,						//打开或关闭物体			GameObjectEntry或-	GameObjectEntry	范围
    ACTION_TYPE_MOVE_RANDOM              //随机移动
};

struct CreatureScriptTemplate
{
    uint32 Entry;
    CreatureEventTypes EventType;
    uint32 EventPhase;
    uint32 DelayTime;
    uint32 RepeatMinTime;
    uint32 RepeatMaxTime;
    CreatureActionTypes ActionType;
    std::string ActionParam1;
    int32 ActionParam2;
    int32 ActionParam3;
};

typedef std::vector<CreatureScriptTemplate> CreatureScriptVe;
extern CreatureScriptVe CreatureScriptVect;

struct ActionTemplate
{
    uint32 EventId;
    CreatureEventTypes EventType;
    uint32 EventPhase;
    uint32 DelayTime;
    uint32 RepeatMinTime;
    uint32 RepeatMaxTime;
    CreatureActionTypes ActionType;
    std::string ActionParam1;
    int32 ActionParam2;
    uint32 ActionParam3;
};

class AICreature
{
public:
    static AICreature* Instance()
    {
        static AICreature Instance;
        return &Instance;
    }

private:

};
#define sAICreature AICreature::Instance()

#endif
