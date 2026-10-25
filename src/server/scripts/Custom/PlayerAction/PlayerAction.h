
#ifndef PLAYERACTION_H
#define PLAYERACTION_H

#include <unordered_map>

enum ActionTypes
{
    Bng = 8000,
    PLR_SYS,				                                             //说话
    PLR_REV_WHISPER,                           //收到密语
    KILL_CRE,								               //击杀生物
    KILL_PLR,						                       //击杀玩家
    CM_QET,                           //完成任务
    CM_ACH,                           //完成成就
    B_KILL_PLR,                           //被玩家击杀
    B_KILL_CRE,                           //被生物击杀
    PLR_LOOT,                           //拾取指定物品
    LEVEL_CG,                           //等级变化
    MAP_CG,                           //切换地图
    AREA_CG,                           //切换区域
    MAP_LEAVE,                           //离开地图
    ZONE_CG,                           //切换地区
    AREA_LEAVE,                           //离开区域
    CAST_SPELL,                           //释放技能
    PLR_LOGIN,                           //登陆
    PLR_LOGOUT,                           //登出
    PLR_FIRST_LOGIN,                           //第一次登陆
    VIP_RANK_CG,                           //VIP等级变化
    PLR_REVIVE,                           //复活
    PLR_EQUIP,                           //装备物品
    PLR_MONEY_CG,                           //金币变化
    PLR_XP,                           //获得经验
    PLR_REPUTATION,                           //获得声望
    PLR_LEARN_SPELL,                           //学习技能
    PLR_UN_LEARN_SPELL,                           //遗忘技能
    PLR_DUEL_START,                           //决斗开始
    PLR_DUEL_STOP,                           //决斗中止
    PLR_DUEL_WIN,                           //决斗胜利
    PLR_DUEL_LOSER,                           //决斗失败
    PLR_REPOP,                           //释放尸体
    PLR_LEARN_TALENT,                           //学习天赋
    PLR_LEARN_COMBAT_START,                           //进入战斗
    PLR_LEARN_COMBAT_STOP,                           //结束战斗
    PLR_RESET_TALENT,                           //重置天赋
    PLR_ADD_AURA,                           //获得光环
    PLR_REMOVE_AURA,                           //失去光环
    PLR_MODE_CG,                           //模型改变
    PLR_EMOTE,                           //表情
    PLR_MINING,                           //采矿
    PLR_HERBALISM,                       //采药
    PLR_FISHING,                         //钓鱼
    PLR_MOVE,                            //玩家移动
    CREATURE_MOVE,                       //生物移动
    PLR_USE_ITEM,                        //使用指定物品
    PLR_ACCEPT_QET,                           //接受任务
    PLR_CANCEL_QET,                           //放弃任务
    PLR_DIE,                           //玩家死亡
    PLR_QET_RW_ITEM,                           //任务奖励物品
};

enum WkType
{
    NOT = 1,				                                             //不限制
    AS_ONE,				                                             //全服唯一
    ACCOUNT_ONE,                           //账号唯一
    PLR_ONE,								               //角色唯一
};

struct PlrActTemp
{
    uint32 Num;                           //序号
    WkType WType;                          //唯一
    ActionTypes AType;                          //动作
    std::string ATypeStr;                          //动作的字符串
    std::string Contents;                          //内容
    float ExOdds;                          //触发几率
    uint32 ExDelay;                          //触发间隔
    uint16 GEventId;                          //关联事件
    int32 GMapId;                          //关联地图
    int32 GAreaId;                          //关联区域
    int32 GZoneId;                          //关联地区
    uint32 NTempId;                          //需求模版
    uint32 RTempId;                          //奖励模版
    uint32 GtsType;                          //公告类型
    std::string Gts;                          //公告内容
};
typedef std::unordered_map<uint32, PlrActTemp> PlrActTempMap;

// 玩家动作冷却时间存储: <玩家GUID, <动作类型, 冷却结束时间戳>>
typedef std::unordered_map<uint32, std::unordered_map<uint32, uint64>> PlayerActionCooldownMap;

struct PlayerActionMovementState
{
    uint32 ElapsedMs = 0;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    bool Initialized = false;
};

struct PlayerActionGatherState
{
    uint64 LootGuid = 0;
    uint64 TriggerTimeMs = 0;
};

struct PlayerActionItemUseState
{
    uint32 ItemEntry = 0;
    uint64 QueuedTimeMs = 0;
};

typedef std::unordered_map<uint32, PlayerActionMovementState> PlayerActionMovementMap;
typedef std::unordered_map<uint32, PlayerActionGatherState> PlayerActionGatherMap;
typedef std::unordered_map<uint32, PlayerActionItemUseState> PlayerActionItemUseMap;

class PlayerAction
{
public:
    static PlayerAction* Instance()
    {
        static PlayerAction Instance;
        return &Instance;
    }

    void LoadPlayerAction();
    uint32 GetActionCount() const;
    bool CheckContent(std::string Contents, std::string Content);
    bool CheckContent(std::string Contents, uint32 AsEx);
    void SendMessageForGtsType(Player* player, uint32 GtsType, const char* Format, ...);
    void DoGtsForAType(Player* player, ActionTypes AType, uint32 GtsType, std::string Gts, Player* ExPlayer, uint32 AsEx1, uint32 AsEx2);
    void DoChatExecute(Player* player, ActionTypes AType, Player* ExPlayer, std::string Content);
    void DoExecute(Player* player, ActionTypes AType, Player* ExPlayer, uint32 AsEx1, uint32 AsEx2);
    void DoVipCgExecute(Player* player, ActionTypes AType, Player* ExPlayer, uint32 OldRank, uint32 NewRank);
    bool HasAction(ActionTypes AType, uint32 AsEx1 = 0);
    bool HasPlayerMoved(Player* player, uint32 diff);
    bool ShouldTriggerGather(Player* player, ObjectGuid lootGuid);
    void QueueItemUse(Player* player, uint32 itemEntry);
    void ProcessPendingItemUse(Player* player);
    void CustomVipChanged(Player* player, uint32 OldRank, uint32 NewRank);
    void DieNow(Player* player);

    // 检查并设置冷却时间，返回true表示可以触发（不在冷却中）
    bool IsCooldownReady(Player* player, uint32 ruleId);
    void SetCooldown(Player* player, uint32 ruleId, uint32 cooldownMs);
    // 清理离线玩家的冷却数据
    void CleanupCooldownData(Player* player);

    //是否全服唯一
    bool IsAsOne(Player* player, uint32 ruleId);
    //是否账号唯一
    bool IsAccountOne(Player* player, uint32 ruleId);
    //是否角色唯一
    bool IsPlayerOne(Player* player, uint32 ruleId);
    void MarkUnique(Player* player, WkType type, uint32 ruleId);

private:
    PlayerActionCooldownMap _cooldownMap;
    PlayerActionMovementMap _movementMap;
    PlayerActionGatherMap _gatherMap;
    PlayerActionItemUseMap _itemUseMap;
    std::mutex _cooldownMutex;

};
#define sPlayerAction PlayerAction::Instance()

#endif
