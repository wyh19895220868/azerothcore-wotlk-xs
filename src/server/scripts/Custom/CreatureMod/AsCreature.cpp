#include "Custom/_Helper/XsPlayerbotCompat.h"
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
#include <limits>
#include <unordered_set>
#include <AI/ScriptedAI/ScriptedCreature.h>
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include "AsCreature.h"
#include "AICreatureScripts.h"
#include <Custom/EnterFBMgr/EnterFBMgr.h>
#include <Custom\AuraSpellMgr\AuraSpellMgr.h>
#include <Custom/CombatPetMgr/CombatPetMgr.h>
#include "../../../../../modules/mod-huanjinxitong/src/HuanJingSystem.h"
#pragma execution_character_set("utf-8")

CreatureOneVect CreatureStructStore;
OneCreGroupVect OneCreGroupStore;
CreatureStructVect MazCreatureStructStore;
MazChallengeVect MazChallengeStore;
MazChallengeItemMap MazChallengeItemStore;
ChallengeDataVect ChallengeDataStore;
StageTemplateMap StageStore;
StageChapterTemplateMap StageChapterStore;
StageGroupMap StageGroupStore;
NtRankTempMap NtRankStore;
AsPlayerNtRankMap AsPlayerNtRankStore;
NtRankGroupMap NtRankGroupStore;
OKillCreMap OKillCreStore;
RmSumTempVect RmSumStore;
DieMapGravMap DieMapGravStore;

namespace
{
using MazChallengeDefinitionKey = std::pair<uint32, uint32>;
std::map<MazChallengeDefinitionKey, MazChallenge> MazChallengeDefinitionStore;

bool IsStagePositionValid(uint32 PositionId, uint32 MapId);
uint32 GetStagePortalEntry(StageTemplate const& Stage, StageChapterTemplate const& Chapter);

bool HasSameChallengeMetadata(MazChallenge const& left, MazChallenge const& right)
{
    return left.GossipText == right.GossipText &&
        left.NTempId == right.NTempId &&
        left.ReItem == right.ReItem &&
        left.AutoReset == right.AutoReset;
}
}

void AsCreature::LoadAsCreatureStore()
{
    CreatureStructStore.clear();

    OneCreGroupStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 组号, 属性模版编号, 模型编号, 模型大小, 武器1, 武器2, 武器3, 几率 FROM _生物控制组表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            OneCreGroupTemp St;
            St.GroupId = Fields[0].Get<uint32>();
            St.StTempId = Fields[1].Get<uint32>();
            St.ModeId = Fields[2].Get<uint32>();
            St.Scale = Fields[3].Get<float>();
            St.EqWp1 = Fields[4].Get<uint32>();
            St.EqWp2 = Fields[5].Get<uint32>();
            St.EqWp3 = Fields[6].Get<uint32>();
            St.Chance = Fields[7].Get<float>();

            OneCreGroupStore.emplace_back(St);
        } while (Result->NextRow());
    }

    DieMapGravStore.clear();
    Result = WorldDatabase.Query("SELECT 地图或区域, 阵营, 墓地坐标区间Min, 墓地坐标区间Max, 复活后血量蓝量比例 FROM _地图或区域墓地复活表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            int32 MapOrArea = Fields[0].Get<int32>();
            DieMapGravTemp St;
            St.Faction = Fields[1].Get<uint32>();
            St.PosMin = Fields[2].Get<uint32>();
            St.PosMax = Fields[3].Get<uint32>();
            St.HMPct = Fields[4].Get<float>();

            if (St.PosMin > St.PosMax)
                St.PosMin = St.PosMax;

            DieMapGravStore.insert(std::make_pair(MapOrArea, St));

        } while (Result->NextRow());
    }

    MazChallengeStore.clear();
    MazChallengeItemStore.clear();
    MazChallengeDefinitionStore.clear();
    Result = WorldDatabase.Query("SELECT 副本地图编号, 难度, 生物类型, 生物属性模版, 菜单文字, 进入该难度需求模版, 重置的物品, 出本重置 FROM _挑战副本配置表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            MazChallenge St;
            St.MapId = Fields[0].Get<uint32>();
            St.Diff = Fields[1].Get<uint32>();
            St.Flag = Fields[2].Get<bool>();
            St.CreTempId = Fields[3].Get<uint32>();
            St.GossipText = Fields[4].Get<std::string>();
            St.NTempId = Fields[5].Get<uint32>();
            St.ReItem = Fields[6].Get<uint32>();
            St.AutoReset = Fields[7].Get<bool>();

            MazChallengeStore.emplace_back(St);

            MazChallengeDefinitionKey key = std::make_pair(St.MapId, St.Diff);
            auto definitionItr = MazChallengeDefinitionStore.find(key);
            if (definitionItr == MazChallengeDefinitionStore.end())
            {
                MazChallengeDefinitionStore.emplace(key, St);
            }
            else
            {
                if (!HasSameChallengeMetadata(definitionItr->second, St))
                {
                    LOG_ERROR("server.loading",
                        "[_挑战副本配置表] 地图 {} 难度 {} 的普通怪/BOSS行基础配置不一致；新旧模式都会统一使用生物类型=0行的菜单、需求和重置配置",
                        St.MapId, St.Diff);
                }

                // 生物类型=0行作为该地图层数的唯一基础定义，避免查询结果顺序影响菜单和需求。
                if (definitionItr->second.Flag && !St.Flag)
                    definitionItr->second = St;
            }

        } while (Result->NextRow());
    }

    if (!MazChallengeStore.empty())
    {
        std::sort(MazChallengeStore.begin(), MazChallengeStore.end(), [](MazChallenge const& left, MazChallenge const& right)
        {
            if (left.MapId != right.MapId)
                return left.MapId < right.MapId;
            if (left.Diff != right.Diff)
                return left.Diff < right.Diff;
            return left.Flag < right.Flag;
        });
    }

    for (auto const& definitionPair : MazChallengeDefinitionStore)
    {
        MazChallenge const& definition = definitionPair.second;
        if (!definition.ReItem)
            continue;

        auto itemItr = MazChallengeItemStore.find(definition.ReItem);
        if (itemItr == MazChallengeItemStore.end())
            MazChallengeItemStore.emplace(definition.ReItem, definition.MapId);
        else if (itemItr->second != definition.MapId)
            LOG_ERROR("server.loading", "[_挑战副本配置表] 重置物品 {} 同时绑定地图 {} 和 {}，保留首次绑定", definition.ReItem, itemItr->second, definition.MapId);
    }

    CreatureScriptVect.clear();
    Result = WorldDatabase.Query("SELECT 生物编号, 事件类型, 事件阶段, 延迟时间, 最小重复时间, 最大重复时间, 动作类型, 动作参数1, 动作参数2, 动作参数3 FROM _生物自定义智能表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            CreatureScriptTemplate St;
            St.Entry = Fields[0].Get<uint32>();

            const char* ETypeStr = Fields[1].Get<std::string_view>().data();
            if (strcmp("进入战斗", ETypeStr) == 0)
                St.EventType = EVENT_TYPE_COMBAT;
            else if (strcmp("杀死玩家", ETypeStr) == 0)
                St.EventType = EVENT_TYPE_KILL;
            else if (strcmp("怪物死亡", ETypeStr) == 0)
                St.EventType = EVENT_TYPE_DIED;
            else if (strcmp("血量低于90%", ETypeStr) == 0)
                St.EventType = EVENT_TYPE_HP_90;
            else if (strcmp("血量低于75%", ETypeStr) == 0)
                St.EventType = EVENT_TYPE_HP_75;
            else if (strcmp("血量低于50%", ETypeStr) == 0)
                St.EventType = EVENT_TYPE_HP_50;
            else if (strcmp("血量低于25%", ETypeStr) == 0)
                St.EventType = EVENT_TYPE_HP_25;
            else if (strcmp("血量低于10%", ETypeStr) == 0)
                St.EventType = EVENT_TYPE_HP_10;
            else if (strcmp("重置AI", ETypeStr) == 0)
                St.EventType = EVENT_TYPE_RESET;
            else
                St.EventType = EVENT_TYPE_NONE;

            St.EventPhase = Fields[2].Get<uint32>();
            St.DelayTime = Fields[3].Get<uint32>() * IN_MILLISECONDS;
            St.RepeatMinTime = Fields[4].Get<uint32>() * IN_MILLISECONDS;
            St.RepeatMaxTime = Fields[5].Get<uint32>() * IN_MILLISECONDS;

            const char* ATypeStr = Fields[6].Get<std::string_view>().data();
            if (strcmp("释放技能", ATypeStr) == 0)
                St.ActionType = ACTION_TYPE_CAST_SPELL;
            else if (strcmp("激活或使失活", ATypeStr) == 0)
                St.ActionType = ACTION_TYPE_ACTIVE_OR_DEACTIVE_OBJECT;
            else if (strcmp("设定阶段", ATypeStr) == 0)
                St.ActionType = ACTION_TYPE_ALT_PHASE;
            else if (strcmp("说话", ATypeStr) == 0)
                St.ActionType = ACTION_TYPE_TALK;
            else if (strcmp("召唤", ATypeStr) == 0)
                St.ActionType = ACTION_TYPE_SUMMON;
            else if (strcmp("转换", ATypeStr) == 0)
                St.ActionType = ACTION_TYPE_ALT_GOB;
            else if (strcmp("移动范围", ATypeStr) == 0)
                St.ActionType = ACTION_TYPE_MOVE_RANDOM;
            else
                St.ActionType = ACTION_TYPE_NONE;

            St.ActionParam1 = Fields[7].Get<std::string>();
            St.ActionParam2 = Fields[8].Get<int32>();
            St.ActionParam3 = Fields[9].Get<int32>();

            CreatureScriptVect.push_back(St);
        } while (Result->NextRow());
    }

    StageChapterStore.clear();
    QueryResult StageChapterTable = WorldDatabase.Query("SHOW TABLES LIKE '_副本闯关章节';");
    if (StageChapterTable)
    {
        Result = WorldDatabase.Query("SELECT 章节编号, 章节名称, 地图编号, 入口坐标编号, 出口坐标编号, 章节奖励模版, 下一章节编号, 默认传送门物体编号, 启用 FROM _副本闯关章节 ORDER BY 章节编号;");
        if (Result)
        {
            do
            {
                Field* Fields = Result->Fetch();
                StageChapterTemplate Chapter;
                Chapter.ChapterId = Fields[0].Get<uint32>();
                Chapter.Name = Fields[1].Get<std::string>();
                Chapter.MapId = Fields[2].Get<uint32>();
                Chapter.EntryPositionId = Fields[3].Get<uint32>();
                Chapter.ExitPositionId = Fields[4].Get<uint32>();
                Chapter.RewardTempId = Fields[5].Get<uint32>();
                Chapter.NextChapterId = Fields[6].Get<uint32>();
                Chapter.DefaultPortalGameobjectId = Fields[7].Get<uint32>();
                Chapter.Enabled = Fields[8].Get<bool>();
                StageChapterStore.emplace(Chapter.ChapterId, Chapter);
            } while (Result->NextRow());
        }
    }

    StageStore.clear();
    QueryResult StageChapterColumn = WorldDatabase.Query("SHOW COLUMNS FROM `_副本闯关` LIKE '章节编号';");
    QueryResult StagePositionColumn = WorldDatabase.Query("SHOW COLUMNS FROM `_副本闯关` LIKE '关卡坐标编号';");
    bool HasFullStageColumns = bool(StageChapterColumn) && bool(StagePositionColumn);
    if (HasFullStageColumns)
        Result = WorldDatabase.Query("SELECT 关卡序号, 章节编号, 章节内序号, 召唤生物组, 通关召唤物体编号, 菜单文本, 关卡坐标编号, 通关传送门编号, 首领关, 通关奖励模版 FROM _副本闯关 ORDER BY 章节编号, 章节内序号;");
    else
        Result = WorldDatabase.Query("SELECT 关卡序号, 召唤生物组, 通关召唤物体编号, 菜单文本 FROM _副本闯关 ORDER BY 关卡序号;");

    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 StageRank = Fields[0].Get<uint32>();
            StageTemplate St;
            if (HasFullStageColumns)
            {
                St.ChapterId = Fields[1].Get<uint32>();
                St.ChapterStage = Fields[2].Get<uint32>();
                St.SummonsGroupId = Fields[3].Get<uint32>();
                St.RewardGameobjectId = Fields[4].Get<uint32>();
                St.Description = Fields[5].Get<std::string>();
                St.PositionId = Fields[6].Get<uint32>();
                St.PortalGameobjectId = Fields[7].Get<uint32>();
                St.IsBossStage = Fields[8].Get<bool>();
                St.RewardTempId = Fields[9].Get<uint32>();
            }
            else
            {
                St.ChapterId = ((StageRank - 1) / 5) + 1;
                St.ChapterStage = ((StageRank - 1) % 5) + 1;
                St.SummonsGroupId = Fields[1].Get<uint32>();
                St.RewardGameobjectId = Fields[2].Get<uint32>();
                St.Description = Fields[3].Get<std::string>();
                St.IsBossStage = St.ChapterStage == 5;
            }
            StageStore.insert(std::make_pair(StageRank, St));
        } while (Result->NextRow());
    }

    if (!StageChapterStore.empty() && !HasFullStageColumns)
        LOG_ERROR("server.worldserver", "副本闯关章节表已经存在，但 _副本闯关 缺少章节扩展字段，请执行最新 world 数据库升级脚本。");

    NtRankStore.clear();
    Result = WorldDatabase.Query("SELECT 层数, 召唤生物组, 奖励物品礼包, 菜单文本, 奖励模版 FROM _副本天梯配置;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Rank = Fields[0].Get<uint32>();
            NtRankTemp St;
            St.SGroupId = Fields[1].Get<uint32>();
            St.RItemId = Fields[2].Get<uint32>();
            St.Des = Fields[3].Get<std::string>();
            St.RewardTempId = Fields[4].Get<uint32>();
            NtRankStore.insert(std::make_pair(Rank, St));
        } while (Result->NextRow());
    }

    StageGroupStore.clear();
    Result = WorldDatabase.Query("SELECT 怪物组, 生物编号, 数量, 坐标X, 坐标Y, 坐标Z, 坐标O FROM _副本闯关怪物组;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            StageGroupTemp St;
            St.GroupId = Fields[0].Get<uint32>();
            St.CreId = Fields[1].Get<uint32>();
            St.Count = Fields[2].Get<uint32>();
            St.X = Fields[3].Get<float>();
            St.Y = Fields[4].Get<float>();
            St.Z = Fields[5].Get<float>();
            St.O = Fields[6].Get<float>();
            StageGroupStore[St.GroupId].push_back(St);
        } while (Result->NextRow());
    }

    uint32 StagePortalScriptId = sObjectMgr->GetScriptId("StageChallengePortal");
    for (auto const& ChapterPair : StageChapterStore)
    {
        StageChapterTemplate const& Chapter = ChapterPair.second;
        if (!Chapter.Enabled)
            continue;

        MapEntry const* MapInfo = sMapStore.LookupEntry(Chapter.MapId);
        if (!MapInfo || !MapInfo->IsDungeon())
            LOG_ERROR("server.worldserver", "副本闯关章节 {} 的地图 {} 不是有效的副本地图。", Chapter.ChapterId, Chapter.MapId);

        if (!IsStagePositionValid(Chapter.EntryPositionId, Chapter.MapId))
            LOG_ERROR("server.worldserver", "副本闯关章节 {} 的入口坐标 {} 不存在或地图不一致。", Chapter.ChapterId, Chapter.EntryPositionId);
        if (PosMap.find(Chapter.ExitPositionId) == PosMap.end())
            LOG_ERROR("server.worldserver", "副本闯关章节 {} 的出口坐标 {} 不存在。", Chapter.ChapterId, Chapter.ExitPositionId);

        uint32 StageCount = 0;
        uint32 StageMask = 0;
        for (auto const& StagePair : StageStore)
        {
            StageTemplate const& Stage = StagePair.second;
            if (Stage.ChapterId != Chapter.ChapterId)
                continue;

            ++StageCount;
            if (Stage.ChapterStage >= 1 && Stage.ChapterStage <= 5)
                StageMask |= 1u << (Stage.ChapterStage - 1);
            if (!Stage.SummonsGroupId || StageGroupStore.find(Stage.SummonsGroupId) == StageGroupStore.end())
                LOG_ERROR("server.worldserver", "副本闯关第 {} 关怪物组 {} 不存在。", StagePair.first, Stage.SummonsGroupId);
            if (!IsStagePositionValid(Stage.PositionId, Chapter.MapId))
                LOG_ERROR("server.worldserver", "副本闯关第 {} 关坐标 {} 不存在或不属于章节地图 {}。", StagePair.first, Stage.PositionId, Chapter.MapId);
            if ((Stage.ChapterStage == 5) != Stage.IsBossStage)
                LOG_ERROR("server.worldserver", "副本闯关第 {} 关的首领标记错误：章节第 5 关必须且只能标记为首领关。", StagePair.first);

            uint32 PortalEntry = GetStagePortalEntry(Stage, Chapter);
            GameObjectTemplate const* PortalTemplate = sObjectMgr->GetGameObjectTemplate(PortalEntry);
            if (!PortalEntry || !PortalTemplate)
                LOG_ERROR("server.worldserver", "副本闯关第 {} 关传送门物体 {} 不存在。", StagePair.first, PortalEntry);
            else if (PortalTemplate->ScriptId != StagePortalScriptId)
                LOG_ERROR("server.worldserver", "副本闯关传送门物体 {} 的 ScriptName 必须设置为 StageChallengePortal。", PortalEntry);
        }

        if (StageCount != 5 || StageMask != 0x1F)
            LOG_ERROR("server.worldserver", "副本闯关章节 {} 必须配置且只能配置章节内序号 1-5，当前关卡数 {}，序号掩码 {}。", Chapter.ChapterId, StageCount, StageMask);
    }

    NtRankGroupStore.clear();
    Result = WorldDatabase.Query("SELECT 怪物组, 生物编号, 数量, 坐标X, 坐标Y, 坐标Z, 坐标O FROM _副本天梯怪物组;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            NtRankGroupTemp St;
            St.GroupId = Fields[0].Get<uint32>();
            St.CreId = Fields[1].Get<uint32>();
            St.Count = Fields[2].Get<uint32>();
            St.X = Fields[3].Get<float>();
            St.Y = Fields[4].Get<float>();
            St.Z = Fields[5].Get<float>();
            St.O = Fields[6].Get<float>();
            NtRankGroupStore[St.GroupId].push_back(St);
        } while (Result->NextRow());
    }

    OKillCreStore.clear();
    Result = WorldDatabase.Query("SELECT 生物编号, 固定命令组, 随机命令组, 随机召唤对象组, 奖励模版, 奖励几率, 随机召唤生物坐标组, 随机召唤数量, 随机召唤几率 FROM _生物击杀特殊配置表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 CreId = Fields[0].Get<uint32>();
            OKillCreTemp St;
            St.NCmd = Fields[1].Get<std::string>();
            St.RCmd = Fields[2].Get<std::string>();
            St.CreOrOb = Fields[3].Get<std::string>();
            St.RTempId = Fields[4].Get<uint32>();
            St.Chance = Fields[5].Get<float>();
            St.RmGroupId = Fields[6].Get<uint32>();
            St.RmCount = Fields[7].Get<uint32>();
            St.RmChance = Fields[8].Get<float>();

            OKillCreStore.insert(std::make_pair(CreId, St));
        } while (Result->NextRow());
    }


    RmSumStore.clear();
    Result = WorldDatabase.Query("SELECT 坐标组编号, 生物编号, 模版坐标编号, 持续时间 FROM _生物击杀特殊随机坐标组表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 CreId = Fields[0].Get<uint32>();
            RmSumTemp St;
            St.GroupId = Fields[0].Get<uint32>();
            St.RmCreId = Fields[1].Get<uint32>();
            St.RmPosId = Fields[2].Get<uint32>();
            St.TimeFn = Fields[3].Get<uint32>();

            RmSumStore.push_back(St);
        } while (Result->NextRow());
    }

    Result = CharacterDatabase.Query("SELECT 玩家标识, 变量 FROM ______玩家 WHERE 序号 = {}", uint32(AT2002));
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            ObjectGuid Guid = ObjectGuid(HighGuid::Player, Fields[0].Get<uint32>());
            uint32 Rank = Fields[1].Get<uint32>();
            AsPlayerNtRankStore[Guid] = Rank;
        } while (Result->NextRow());
    }
}

void AsCreature::UpdateAsPlayerNtRank(ObjectGuid Guid, uint32 Rank)
{
    AsPlayerNtRankStore[Guid] = Rank;
}

void AsCreature::DoNtRankDaySendReward()
{
    QueryResult Result = CharacterDatabase.Query("SELECT 玩家标识, 变量 FROM ______玩家 WHERE 序号 = {} ORDER BY 变量 DESC", uint32(AT2002));
    if (Result)
    {
        uint32 Nun = 0;
        do
        {
            Field* Fields = Result->Fetch();
            ObjectGuid Guid = ObjectGuid(HighGuid::Player, Fields[0].Get<uint32>());
            uint32 Rank = Fields[1].Get<uint32>();
            Nun++;
            std::string Body = "你的总层数 " + std::to_string(Rank) + " 排名第 " + std::to_string(Nun) + " 名";
            sHelper->SendMailToGuid(Guid, "每日天梯排行奖励", Body, GetNtRItemId(Rank - 1), 1);

        } while (Result->NextRow());
    }
}

bool AsCreature::IsNotFor(Creature* Me)
{
    return ((Me->GetFaction() == 35 || Me->IsPet() || Me->IsHunterPet() || Me->IsCivilian() || Me->IsControlledByPlayer() || Me->IsTotem() || Me->GetEntry() == 1964) && !sCombatPetMgr->IsCombatPetData(Me));
}

bool AsCreature::IsOneCreStore(Creature* Me)
{
    bool In = false;
    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        if (Itr->CreId == Me->GetEntry())
            In = true;
    }
    return In;
}

bool AsCreature::IsLFGCustomBoss(Creature* Me)
{
    return 	(Me->IsDungeonBoss() || (Me->GetCreatureTemplate()->rank >= 3 && Me->GetCreatureTemplate()->type_flags == 108));
}

bool AsCreature::IsCustomBoss(Creature* Me)
{
    return 	(Me->IsDungeonBoss() || (Me->GetCreatureTemplate()->rank >= 3 && Me->GetCreatureTemplate()->type_flags == 108));
}

void AsCreature::DoLevelMod(Creature* Me, uint8& MeLevel)
{
    if (IsNotFor(Me))
        return;

    if (sCombatPetMgr->IsCombatPetData(Me))
    {
        MeLevel = sHelper->GetCreLevelByTempId(Me->StTempId);
        return;
    }

    if (Me->StTempId > 0 && sHelper->GetCreLevelByTempId(Me->StTempId))
    {
        MeLevel = sHelper->GetCreLevelByTempId(Me->StTempId);
        return;
    }

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId && sHelper->GetCreLevelByTempId(Me->StTempId))
            MeLevel = sHelper->GetCreLevelByTempId(Me->StTempId);
    }
}

float AsCreature::GetHpMod(Creature* Me)
{
    if (IsNotFor(Me))
        return 1.0f;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return sHelper->GetCreHpByTempId(Me->StTempId);

    if (Me->StTempId > 0)
        return sHelper->GetCreHpByTempId(Me->StTempId);

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId)
            return sHelper->GetCreHpByTempId(Me->StTempId);
    }

    return 1.0f;
}

float AsCreature::GetManaMod(Creature* Me)
{
    if (IsNotFor(Me))
        return 1.0f;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return sHelper->GetCreManaByTempId(Me->StTempId);

    if (Me->StTempId > 0)
        return sHelper->GetCreManaByTempId(Me->StTempId);

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId)
            return sHelper->GetCreManaByTempId(Me->StTempId);
    }

    return 1.0f;
}

float AsCreature::GetMeleeMod(Creature* Me)
{
    if (IsNotFor(Me))
        return 1.0f;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return sHelper->GetCreMeleeByTempId(Me->StTempId);

    if (Me->StTempId > 0)
        return sHelper->GetCreMeleeByTempId(Me->StTempId);

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId)
            return sHelper->GetCreMeleeByTempId(Me->StTempId);
    }

    return 1.0f;
}

float AsCreature::GetSpellMod(Creature* Me)
{
    if (IsNotFor(Me))
        return 1.0f;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return sHelper->GetCreSpellByTempId(Me->StTempId);

    if (Me->StTempId > 0)
        return sHelper->GetCreSpellByTempId(Me->StTempId);

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId)
            return sHelper->GetCreSpellByTempId(Me->StTempId);
    }

    return 1.0f;
}

uint32 AsCreature::GetCastSpeedValue(Creature* Me)
{
    if (IsNotFor(Me) || Me->StTempId == 0)
        return 0;

    auto itr = AtCreatureStatTempStore.find(Me->StTempId);
    return itr != AtCreatureStatTempStore.end() ? itr->second.CastSpeedValue : 0;
}

uint32 AsCreature::GetArmorPenetration(Creature* Me)
{
    if (IsNotFor(Me) || Me->StTempId == 0)
        return 0;

    auto itr = AtCreatureStatTempStore.find(Me->StTempId);
    return itr != AtCreatureStatTempStore.end() ? itr->second.ArmorPenetration : 0;
}

float AsCreature::GetArmorPenetrationPct(Creature* Me)
{
    if (IsNotFor(Me) || Me->StTempId == 0)
        return 0.0f;

    auto itr = AtCreatureStatTempStore.find(Me->StTempId);
    return itr != AtCreatureStatTempStore.end() ? itr->second.ArmorPenetrationPct : 0.0f;
}

uint32 AsCreature::GetBlockPenetration(Creature* Me)
{
    if (IsNotFor(Me) || Me->StTempId == 0)
        return 0;

    auto itr = AtCreatureStatTempStore.find(Me->StTempId);
    return itr != AtCreatureStatTempStore.end() ? itr->second.BlockPenetration : 0;
}

float AsCreature::GetBlockPenetrationPct(Creature* Me)
{
    if (IsNotFor(Me) || Me->StTempId == 0)
        return 0.0f;

    auto itr = AtCreatureStatTempStore.find(Me->StTempId);
    return itr != AtCreatureStatTempStore.end() ? itr->second.BlockPenetrationPct : 0.0f;
}

float AsCreature::GetRetMod(Creature* Me)
{
    if (IsNotFor(Me))
        return 0.0f;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return sHelper->GetCreRetByTempId(Me->StTempId);

    if (Me->StTempId > 0)
        return sHelper->GetCreRetByTempId(Me->StTempId);

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId)
            return sHelper->GetCreRetByTempId(Me->StTempId);
    }

    return 0.0f;
}

std::string AsCreature::GetOneLootIds(Creature* Me)
{
    if (IsNotFor(Me))
        return "";

    if (sCombatPetMgr->IsCombatPetData(Me))
        return sHelper->GetCrePLootIdsByTempId(Me->StTempId);

    if (Me->StTempId > 0)
        return sHelper->GetCrePLootIdsByTempId(Me->StTempId);

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId)
            return sHelper->GetCrePLootIdsByTempId(Me->StTempId);
    }

    return "";
}

uint32 AsCreature::GetHpNow(Creature* Me)
{
    return AsInt64ToUInt32Clamp(GetAsHpNow(Me), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 AsCreature::GetAsHpNow(Creature* Me)
{
    if (IsNotFor(Me))
        return 0;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return sHelper->GetAsCreNowHpByTempId(Me->StTempId);

    if (Me->StTempId > 0)
        return sHelper->GetAsCreNowHpByTempId(Me->StTempId);

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId)
            return sHelper->GetAsCreNowHpByTempId(Me->StTempId);
    }

    return sHelper->GetAsBigCreatureTemplateHp(Me->GetEntry());
}

uint32 AsCreature::GetRespawntime(Creature* Me)
{
    HuanJingCreatureConfig const* config = sHuanJingSystem->GetMapCreatureConfig(Me, nullptr);
    return config ? config->respawnTime : 0;
}

uint32 AsCreature::GetDmgTakenNow(Creature* Me)
{
    return AsInt64ToUInt32Clamp(GetAsDmgTakenNow(Me), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 AsCreature::GetAsDmgTakenNow(Creature* Me)
{
    if (IsNotFor(Me))
        return 0;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return sHelper->GetAsCreDmgTakenNowByTempId(Me->StTempId);

    if (Me->StTempId > 0)
        return sHelper->GetAsCreDmgTakenNowByTempId(Me->StTempId);

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId)
            return sHelper->GetAsCreDmgTakenNowByTempId(Me->StTempId);
    }

    return sHelper->GetAsBigCreatureTemplateDmgTakenNow(Me->GetEntry());
}

uint32 AsCreature::GetDmgNow(Creature* Me)
{
    return AsInt64ToUInt32Clamp(GetAsDmgNow(Me), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 AsCreature::GetAsDmgNow(Creature* Me)
{
    if (IsNotFor(Me))
        return 0;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return sHelper->GetAsCreDmgNowByTempId(Me->StTempId);

    if (Me->StTempId > 0)
        return sHelper->GetAsCreDmgNowByTempId(Me->StTempId);

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId)
            return sHelper->GetAsCreDmgNowByTempId(Me->StTempId);
    }

    return sHelper->GetAsBigCreatureTemplateDmgNow(Me->GetEntry());
}

float AsCreature::GetKtHpMod(Creature* Me)
{
    if (IsNotFor(Me))
        return 0.0f;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return sHelper->GetCreKtHpByTempId(Me->StTempId);

    if (Me->StTempId > 0)
        return sHelper->GetCreKtHpByTempId(Me->StTempId);

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId)
            return sHelper->GetCreKtHpByTempId(Me->StTempId);
    }

    return 0.0f;
}

void AsCreature::CreAddAuras(Creature* Me)
{
    if (IsNotFor(Me))
        return;

    for (auto It = Me->TopAuras.begin(); It != Me->TopAuras.end(); It++)
    {
        if (Me->HasAura(*It))
            Me->RemoveAurasDueToSpell(*It);
    }

    Me->TopAuras.clear();

    auto addTemplateAuras = [Me]()
    {
        if (roll_chance_f(sHelper->GetCreAuraChanceByTempId(Me->StTempId)))
        {
            std::vector<uint32> auraIds;
            for (AuraTemplate const& aura : AuraVect)
                if (aura.GroupId == sHelper->GetCreAuraGroupByTempId(Me->StTempId))
                    auraIds.push_back(aura.AuraId);

            Acore::Containers::AtRandomResize(auraIds, sHelper->GetCreAuraCountByTempId(Me->StTempId));
            for (uint32 auraId : auraIds)
            {
                if (!Me->HasAura(auraId))
                    Me->AddAura(auraId, Me);
                Me->TopAuras.insert(auraId);
            }
        }

        for (uint32 auraId : sHelper->GetCreAtAurasByTempId(Me->StTempId))
        {
            if (!Me->HasAura(auraId))
                Me->AddAura(auraId, Me);
            Me->TopAuras.insert(auraId);
        }
    };

    // 新地图控制会直接把本只生物选中的模板写入 StTempId。
    if (Me->StTempId > 0)
    {
        addTemplateAuras();
        return;
    }

    if (sCombatPetMgr->IsCombatPetData(Me))
    {
        if (roll_chance_f(sHelper->GetCreAuraChanceByTempId(Me->StTempId)))
        {
            std::vector<uint32> DoVect;
            DoVect.clear();
            for (auto It = AuraVect.begin(); It != AuraVect.end(); ++It)
            {
                if (It->GroupId == sHelper->GetCreAuraGroupByTempId(Me->StTempId))
                    DoVect.push_back(It->AuraId);
            }

            Acore::Containers::AtRandomResize(DoVect, sHelper->GetCreAuraCountByTempId(Me->StTempId));
            for (auto const& AuraId : DoVect)
            {
                if (!Me->HasAura(AuraId))
                    Me->AddAura(AuraId, Me);

                Me->TopAuras.insert(AuraId);
            }
        }

        std::set<uint32> Temps = sHelper->GetCreAtAurasByTempId(Me->StTempId);
        for (auto const& AuraId : Temps)
        {
            if (!Me->HasAura(AuraId))
                Me->AddAura(AuraId, Me);

            Me->TopAuras.insert(AuraId);
        }

        return;
    }

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId)
        {
            if (roll_chance_f(sHelper->GetCreAuraChanceByTempId(Me->StTempId)))
            {
                std::vector<uint32> DoVect;
                DoVect.clear();
                for (auto It = AuraVect.begin(); It != AuraVect.end(); ++It)
                {
                    if (It->GroupId == sHelper->GetCreAuraGroupByTempId(Me->StTempId))
                        DoVect.push_back(It->AuraId);
                }

                Acore::Containers::AtRandomResize(DoVect, sHelper->GetCreAuraCountByTempId(Me->StTempId));
                for (auto const& AuraId : DoVect)
                {
                    if (!Me->HasAura(AuraId))
                        Me->AddAura(AuraId, Me);

                    Me->TopAuras.insert(AuraId);
                }
            }

            std::set<uint32> Temps = sHelper->GetCreAtAurasByTempId(Me->StTempId);
            for (auto const& AuraId : Temps)
            {
                if (!Me->HasAura(AuraId))
                    Me->AddAura(AuraId, Me);

                Me->TopAuras.insert(AuraId);
            }
        }
    }
}

void AsCreature::AutoRespawnGts(Creature* Me, bool Select)
{
    (void)Select;

    if (sHelper->OnDungeon(Me->GetMapId()))
        return;

    HuanJingCreatureConfig const* config = sHuanJingSystem->GetMapCreatureConfig(Me, nullptr);
    if (config && config->noticeMode > 0 && !config->respawnMessage.empty())
    {
        switch (config->noticeMode)
        {
        case 1:
            sHelper->SendSerMessage(config->respawnMessage.c_str(), sHelper->GetCreatureNameByEntry(Me->GetEntry()).c_str());
            break;
        case 2:
            sHelper->SendSerAllMessage(config->respawnMessage.c_str(), sHelper->GetCreatureNameByEntry(Me->GetEntry()).c_str());
            break;
        case 3:
            sHelper->SendSerSpecialMessage(config->respawnMessage.c_str(), sHelper->GetCreatureNameByEntry(Me->GetEntry()).c_str());
            break;
        }
    }

    if (sAsCreature->IsNotFor(Me))
        return;

    CreatureTemplate const* CInfo = Me->GetCreatureTemplate();
    if (!CInfo)
        return;

    uint32 AGroup = 0;
    for (auto Iter = CreatureStructStore.begin(); Iter != CreatureStructStore.end(); Iter++)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Iter->CreId == Me->GetEntry() && Iter->Diff == MapRank)
            AGroup = Iter->SpGroupId;
    }

    if (!AGroup)
        return;

    struct ChanceTemp { uint32 StTempId; uint32 ModeId; float Scale; uint32 EqWp1; uint32 EqWp2; uint32 EqWp3; float Chance; };
    std::vector<ChanceTemp> ChanceList;
    ChanceList.clear();

    for (auto It = OneCreGroupStore.begin(); It != OneCreGroupStore.end(); It++)
    {
        if (It->GroupId == AGroup)
        {
            ChanceTemp St;
            St.StTempId = It->StTempId;
            St.ModeId = It->ModeId;
            St.Scale = It->Scale;
            St.EqWp1 = It->EqWp1;
            St.EqWp2 = It->EqWp2;
            St.EqWp3 = It->EqWp3;
            St.Chance = It->Chance;
            ChanceList.push_back(St);
        }
    }

    if (!ChanceList.empty())
    {
        float RIdx = 0.0f;
        for (auto Itr = ChanceList.begin(); Itr != ChanceList.end(); Itr++)
            RIdx += Itr->Chance;

        for (auto It = ChanceList.begin(); It != ChanceList.end(); It++)
        {
            if (frand(0, RIdx) <= It->Chance)
            {
                Me->StTempId = It->StTempId;

                if (sHelper->GetBoolean(22010) && sHuanJingSystem)
                {
                    Me->MultipleHp = 1;
                    Me->MaxMultipleHp = 1;
                    sHuanJingSystem->EnsureCreatureHealthBarsFromConfig(Me, nullptr, false);
                }
                else
                {
                    Me->MultipleHp = sHelper->GetCreHpCountByTempId(Me->StTempId);
                    Me->MaxMultipleHp = Me->MultipleHp;
                }

                sAsCreature->DoAsAddAuras(Me);
                if (It->ModeId)
                {
                    Me->SetDisplayId(It->ModeId);
                    Me->SetNativeDisplayId(It->ModeId);
                }
                else
                {
                    uint32 DisplayID = ObjectMgr::ChooseDisplayId(CInfo);
                    if (sObjectMgr->GetCreatureModelRandomGender(&DisplayID))
                    {
                        if (DisplayID)
                        {
                            Me->SetDisplayId(DisplayID);
                            Me->SetNativeDisplayId(DisplayID);
                        }
                    }
                }

                if (It->Scale != 0.0f)
                    Me->SetObjectScale(It->Scale);
                else
                    Me->SetObjectScale(CInfo->scale);

                if (It->EqWp1)
                    Me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 0, It->EqWp1);
                if (It->EqWp2)
                    Me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 1, It->EqWp2);
                if (It->EqWp3)
                    Me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 2, It->EqWp3);

                if (Select)
                    Me->SelectLevel();

                break;
            }
            else
                RIdx -= It->Chance;
        }

        ChanceList.clear();
    }
}

void AsCreature::DoMazLevelMod(Creature* Me, uint8& MeLevel)
{
    if (IsNotFor(Me))
        return;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return;

    if (IsOneCreStore(Me))
        return;

    if (!UseLegacyMapCreatureAttributes())
        return;

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreLevelByTempId(Itr->CreTempId))
                MeLevel = sHelper->GetCreLevelByTempId(Itr->CreTempId);
        }

        return;
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreLevelByTempId(Itr->CreTempId))
                MeLevel = sHelper->GetCreLevelByTempId(Itr->CreTempId);
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreLevelByTempId(Itr->CreTempId))
                MeLevel = sHelper->GetCreLevelByTempId(Itr->CreTempId);
        }
    }
}

float AsCreature::GetMazHpMod(Creature* Me)
{
    if (IsNotFor(Me))
        return 1.0f;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return 1.0f;

    if (IsOneCreStore(Me))
        return 1.0f;

    if (!UseLegacyMapCreatureAttributes())
        return 1.0f;

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreHpByTempId(Itr->CreTempId);
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreHpByTempId(Itr->CreTempId);
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreHpByTempId(Itr->CreTempId);
        }
    }

    return 1.0f;
}

float AsCreature::GetMazManaMod(Creature* Me)
{
    if (IsNotFor(Me))
        return 1.0f;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return 1.0f;

    if (IsOneCreStore(Me))
        return 1.0f;

    if (!UseLegacyMapCreatureAttributes())
        return 1.0f;

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreManaByTempId(Itr->CreTempId);
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreManaByTempId(Itr->CreTempId);
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreManaByTempId(Itr->CreTempId);
        }
    }

    return 1.0f;
}

float AsCreature::GetMazMeleeMod(Creature* Me)
{
    if (IsNotFor(Me))
        return 1.0f;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return 1.0f;

    if (IsOneCreStore(Me))
        return 1.0f;

    if (!UseLegacyMapCreatureAttributes())
        return 1.0f;

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreMeleeByTempId(Itr->CreTempId);
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreMeleeByTempId(Itr->CreTempId);
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreMeleeByTempId(Itr->CreTempId);
        }
    }

    return 1.0f;
}

float AsCreature::GetMazSpellMod(Creature* Me)
{
    if (IsNotFor(Me))
        return 1.0f;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return 1.0f;

    if (IsOneCreStore(Me))
        return 1.0f;

    if (!UseLegacyMapCreatureAttributes())
        return 1.0f;

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreSpellByTempId(Itr->CreTempId);
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreSpellByTempId(Itr->CreTempId);
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreSpellByTempId(Itr->CreTempId);
        }
    }

    return 1.0f;
}

float AsCreature::GetMazRetMod(Creature* Me)
{
    if (IsNotFor(Me))
        return 0.0f;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return 0.0f;

    if (IsOneCreStore(Me))
        return 0.0f;

    if (!UseLegacyMapCreatureAttributes())
        return 0.0f;

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreRetByTempId(Itr->CreTempId);
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreRetByTempId(Itr->CreTempId);
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreRetByTempId(Itr->CreTempId);
        }
    }

    return 0.0f;
}

std::pair<float, float> AsCreature::AutoDmgCreatureMinAndMax(Creature* Me)
{
    auto AsResult = AutoAsDmgCreatureMinAndMax(Me);
    return {
        float(AsInt64ToDoubleClamp(AsResult.first, double(std::numeric_limits<int32>::max()))),
        float(AsInt64ToDoubleClamp(AsResult.second, double(std::numeric_limits<int32>::max())))
    };
}

std::pair<AsInt64, AsInt64> AsCreature::AutoAsDmgCreatureMinAndMax(Creature* Me)
{
    std::pair<AsInt64, AsInt64> Result;
    Result.first = AsInt64Zero();
    Result.second = AsInt64Zero();

    if (sCombatPetMgr->IsCombatPetData(Me))
    {
        Result.first = sHelper->GetAsCreDmgWMinByTempId(Me->StTempId);
        Result.second = sHelper->GetAsCreDmgWMaxByTempId(Me->StTempId);
        return Result;
    }

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        if (Itr->CreId == Me->GetEntry() && Me->StTempId)
        {
            Result.first = sHelper->GetAsCreDmgWMinByTempId(Me->StTempId);
            Result.second = sHelper->GetAsCreDmgWMaxByTempId(Me->StTempId);
            return Result;
        }
    }

    if (!UseLegacyMapCreatureAttributes())
    {
        Result.first = sHelper->GetAsBigCreatureTemplateDmgWMin(Me->GetEntry());
        Result.second = sHelper->GetAsBigCreatureTemplateDmgWMax(Me->GetEntry());
        return Result;
    }

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
            {
                Result.first = sHelper->GetAsCreDmgWMinByTempId(Itr->CreTempId);
                Result.second = sHelper->GetAsCreDmgWMaxByTempId(Itr->CreTempId);
                return Result;
            }
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
            {
                Result.first = sHelper->GetAsCreDmgWMinByTempId(Itr->CreTempId);
                Result.second = sHelper->GetAsCreDmgWMaxByTempId(Itr->CreTempId);
                return Result;
            }
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
            {
                Result.first = sHelper->GetAsCreDmgWMinByTempId(Itr->CreTempId);
                Result.second = sHelper->GetAsCreDmgWMaxByTempId(Itr->CreTempId);
                return Result;
            }
        }
    }

    Result.first = sHelper->GetAsBigCreatureTemplateDmgWMin(Me->GetEntry());
    Result.second = sHelper->GetAsBigCreatureTemplateDmgWMax(Me->GetEntry());
    if (Result.first > 0 || Result.second > 0)
        return Result;

    return Result;
}

namespace
{
AsInt64 RandomAsCreatureSpellDamage(AsInt64 minDamage, AsInt64 maxDamage)
{
    if (maxDamage < minDamage)
        maxDamage = minDamage;

    if (minDamage <= 0 && maxDamage <= 0)
        return AsInt64Zero();

    if (minDamage < 0)
        minDamage = AsInt64Zero();

    AsInt64 diff = maxDamage - minDamage;
    if (diff <= 0)
        return minDamage;

    uint32 roll = urand(0, 10000);
    return minDamage + (diff * roll) / 10000;
}
}

int32 AsCreature::AutoDmgSpellBeginCreature(Creature* Me)
{
    return AsInt64ToInt32Clamp(AutoAsDmgSpellBeginCreature(Me));
}

AsInt64 AsCreature::AutoAsDmgSpellBeginCreature(Creature* Me)
{
    if (sCombatPetMgr->IsCombatPetData(Me))
    {
        AsInt64 Min = sHelper->GetAsCreMinDmgSpellByTempId(Me->StTempId);
        AsInt64 Max = sHelper->GetAsCreMaxDmgSpellByTempId(Me->StTempId);
        return RandomAsCreatureSpellDamage(Min, Max);
    }

    if (Me->StTempId > 0)
    {
        AsInt64 Min = sHelper->GetAsCreMinDmgSpellByTempId(Me->StTempId);
        AsInt64 Max = sHelper->GetAsCreMaxDmgSpellByTempId(Me->StTempId);
        return RandomAsCreatureSpellDamage(Min, Max);
    }

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        if (Itr->CreId == Me->GetEntry() && Me->StTempId)
        {
            AsInt64 Min = sHelper->GetAsCreMinDmgSpellByTempId(Me->StTempId);
            AsInt64 Max = sHelper->GetAsCreMaxDmgSpellByTempId(Me->StTempId);
            return RandomAsCreatureSpellDamage(Min, Max);
        }
    }

    if (!UseLegacyMapCreatureAttributes())
    {
        AsInt64 minDamage = sHelper->GetAsBigCreatureTemplateMinSpellDmg(Me->GetEntry());
        AsInt64 maxDamage = sHelper->GetAsBigCreatureTemplateMaxSpellDmg(Me->GetEntry());
        return RandomAsCreatureSpellDamage(minDamage, maxDamage);
    }

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
            {
                AsInt64 Min = sHelper->GetAsCreMinDmgSpellByTempId(Itr->CreTempId);
                AsInt64 Max = sHelper->GetAsCreMaxDmgSpellByTempId(Itr->CreTempId);
                return RandomAsCreatureSpellDamage(Min, Max);
            }
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
            {
                AsInt64 Min = sHelper->GetAsCreMinDmgSpellByTempId(Itr->CreTempId);
                AsInt64 Max = sHelper->GetAsCreMaxDmgSpellByTempId(Itr->CreTempId);
                return RandomAsCreatureSpellDamage(Min, Max);
            }
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
            {
                AsInt64 Min = sHelper->GetAsCreMinDmgSpellByTempId(Itr->CreTempId);
                AsInt64 Max = sHelper->GetAsCreMaxDmgSpellByTempId(Itr->CreTempId);
                return RandomAsCreatureSpellDamage(Min, Max);
            }
        }
    }

    AsInt64 Min = sHelper->GetAsBigCreatureTemplateMinSpellDmg(Me->GetEntry());
    AsInt64 Max = sHelper->GetAsBigCreatureTemplateMaxSpellDmg(Me->GetEntry());
    if (Min > 0 || Max > 0)
    {
        return RandomAsCreatureSpellDamage(Min, Max);
    }

    return AsInt64Zero();
}

bool AsCreature::IsOnEtLoot(Creature* Me)
{
    if (IsNotFor(Me))
        return false;

    if (sCombatPetMgr->IsCombatPetData(Me))
    {
        if (Me->StTempId && sHelper->GetCreOnEtByTempId(Me->StTempId))
            return true;
    }

    if (Me->StTempId > 0 && sHelper->GetCreOnEtByTempId(Me->StTempId))
        return true;

    uint32 MapId = Me->GetMapId();
    bool On = false;

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        if (Itr->CreId == Me->GetEntry() && Me->StTempId && sHelper->GetCreOnEtByTempId(Me->StTempId))
            On = true;
    }

    if (!UseLegacyMapCreatureAttributes())
        return On;

    for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
    {
        if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreOnEtByTempId(Itr->CreTempId))
            On = true;
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreOnEtByTempId(Itr->CreTempId))
                On = true;
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreOnEtByTempId(Itr->CreTempId))
                On = true;
        }
    }

    return On;
}

void AsCreature::AsCustomFillLoot(Creature* Me, Loot* OnLoot, Player* Looter)
{
    uint32 EntryLootId = Me->GetCreatureTemplate()->lootid;
    EntryLootId = IsOnEtLoot(Me) ? 0 : EntryLootId;

    if (EntryLootId)
    {
        OnLoot->FillLoot(EntryLootId, LootTemplates_Creature, Looter, false, false, Me->GetLootMode());
        uint32 LootCount = sAuraSpellMgr->GetLootCountMod(Looter);
        for (uint32 i = 0; i < LootCount; ++i)
            OnLoot->FillLoot(EntryLootId, LootTemplates_Creature, Looter, false, false, Me->GetLootMode());
    }

    // _模板_地图生物控制在加载时已经把属性模板写入 StTempId。
    // 旧地图属性表关闭后 IsInCustom() 会返回 false，不能再用它拦截模板额外掉落。
    if (Me->StTempId > 0 || IsInCustom(Me))
    {
        std::string LootAs = GetOneLootIds(Me);
        if (LootAs.empty())
            LootAs = GetMazLootIds(Me);

        if (!LootAs.empty())
        {
            Tokenizer LootData(LootAs, '#');
            for (auto const& Temp : LootData)
            {
                uint32 LootId = atoi(Temp);
                if (LootId)
                {
                    OnLoot->FillLoot(LootId, LootTemplates_Creature, Looter, false, false, Me->GetLootMode());
                    uint32 LootCount = sAuraSpellMgr->GetLootCountMod(Looter);
                    for (uint32 i = 0; i < LootCount; ++i)
                        OnLoot->FillLoot(LootId, LootTemplates_Creature, Looter, false, false, Me->GetLootMode());
                }
            }
        }

    }
}

uint32 AsCreature::AsCustomAttackTime(Creature* Me)
{
    CreatureTemplate const* CInfo = Me->GetCreatureTemplate();

    if (IsInCustom(Me))
    {
        if (sCombatPetMgr->IsCombatPetData(Me))
            return sHelper->GetCreAttackTimeByTempId(Me->StTempId);

        for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
        {
            if (Itr->CreId == Me->GetEntry() && Me->StTempId && sHelper->GetCreAttackTimeByTempId(Me->StTempId))
                return sHelper->GetCreAttackTimeByTempId(Me->StTempId);
        }

        uint32 MapId = Me->GetMapId();

        if (IsChallengeMap(MapId))
        {
            for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
            {
                if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreAttackTimeByTempId(Itr->CreTempId))
                    return sHelper->GetCreAttackTimeByTempId(Itr->CreTempId);
            }
        }

        for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
        {
            if (Itr->MapOrAreaId >= 0)
            {
                uint32 MapRank = Me->GetMap()->GetDifficulty();
                if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreAttackTimeByTempId(Itr->CreTempId))
                    return sHelper->GetCreAttackTimeByTempId(Itr->CreTempId);
            }
            else if (Itr->MapOrAreaId < 0)
            {
                if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreAttackTimeByTempId(Itr->CreTempId))
                    return sHelper->GetCreAttackTimeByTempId(Itr->CreTempId);
            }
        }
    }

    return CInfo->BaseAttackTime;
}

float AsCreature::AsCustomArmor(Creature* Me)
{
    CreatureTemplate const* CInfo = Me->GetCreatureTemplate();

    if (IsInCustom(Me))
    {
        if (sCombatPetMgr->IsCombatPetData(Me))
        {
            AsInt64 Armor = sHelper->GetCreArmorByTempId(Me->StTempId);
            if (Armor > 0)
                return float(AsInt64ToDoubleClamp(Armor, double(std::numeric_limits<int32>::max())));
        }

        for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
        {
            AsInt64 Armor = Me->StTempId ? sHelper->GetCreArmorByTempId(Me->StTempId) : AsInt64Zero();
            if (Itr->CreId == Me->GetEntry() && Armor > 0)
                return float(AsInt64ToDoubleClamp(Armor, double(std::numeric_limits<int32>::max())));
        }

        uint32 MapId = Me->GetMapId();

        if (IsChallengeMap(MapId))
        {
            for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
            {
                AsInt64 Armor = Itr->CreTempId ? sHelper->GetCreArmorByTempId(Itr->CreTempId) : AsInt64Zero();
                if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Armor > 0)
                    return float(AsInt64ToDoubleClamp(Armor, double(std::numeric_limits<int32>::max())));
            }
        }

        for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
        {
            if (Itr->MapOrAreaId >= 0)
            {
                uint32 MapRank = Me->GetMap()->GetDifficulty();
                AsInt64 Armor = Itr->CreTempId ? sHelper->GetCreArmorByTempId(Itr->CreTempId) : AsInt64Zero();
                if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Armor > 0)
                    return float(AsInt64ToDoubleClamp(Armor, double(std::numeric_limits<int32>::max())));
            }
            else if (Itr->MapOrAreaId < 0)
            {
                AsInt64 Armor = Itr->CreTempId ? sHelper->GetCreArmorByTempId(Itr->CreTempId) : AsInt64Zero();
                if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Armor > 0)
                    return float(AsInt64ToDoubleClamp(Armor, double(std::numeric_limits<int32>::max())));
            }
        }
    }

    AsInt64 Armor = sHelper->GetAsCreatureClassLevelArmor(Me->getLevel(), CInfo->unit_class);
    if (Armor > 0)
        Armor = CInfo->AsModArmor > 0 ? ScaleAsInt64ByScaled(Armor, CInfo->AsModArmor) : CreatureBaseStats::ScaleAsInt64(Armor, CInfo->ModArmor);
    else
    {
        CreatureBaseStats const* AStats = sObjectMgr->GetCreatureBaseStats(Me->getLevel(), CInfo->unit_class);
        Armor = AStats->GenerateAsArmor(CInfo);
    }

    return float(AsInt64ToDoubleClamp(Armor, double(std::numeric_limits<int32>::max())));
}

float AsCreature::AsCustomSchool1(Creature* Me)
{
    CreatureTemplate const* CInfo = Me->GetCreatureTemplate();

    if (IsInCustom(Me))
    {
        if (sCombatPetMgr->IsCombatPetData(Me))
            return float(sHelper->GetCreSchool1ByTempId(Me->StTempId));

        for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
        {
            if (Itr->CreId == Me->GetEntry() && Me->StTempId && sHelper->GetCreSchool1ByTempId(Me->StTempId))
                return float(sHelper->GetCreSchool1ByTempId(Me->StTempId));
        }

        uint32 MapId = Me->GetMapId();

        if (IsChallengeMap(MapId))
        {
            for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
            {
                if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool1ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool1ByTempId(Itr->CreTempId));
            }
        }

        for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
        {
            if (Itr->MapOrAreaId >= 0)
            {
                uint32 MapRank = Me->GetMap()->GetDifficulty();
                if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool1ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool1ByTempId(Itr->CreTempId));
            }
            else if (Itr->MapOrAreaId < 0)
            {
                if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool1ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool1ByTempId(Itr->CreTempId));
            }
        }
    }

    return float(CInfo->resistance[SPELL_SCHOOL_HOLY]);
}

float AsCreature::AsCustomSchool2(Creature* Me)
{
    CreatureTemplate const* CInfo = Me->GetCreatureTemplate();

    if (IsInCustom(Me))
    {
        if (sCombatPetMgr->IsCombatPetData(Me))
            return float(sHelper->GetCreSchool2ByTempId(Me->StTempId));

        for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
        {
            if (Itr->CreId == Me->GetEntry() && Me->StTempId && sHelper->GetCreSchool2ByTempId(Me->StTempId))
                return float(sHelper->GetCreSchool2ByTempId(Me->StTempId));
        }

        uint32 MapId = Me->GetMapId();

        if (IsChallengeMap(MapId))
        {
            for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
            {
                if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool2ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool2ByTempId(Itr->CreTempId));
            }
        }

        for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
        {
            if (Itr->MapOrAreaId >= 0)
            {
                uint32 MapRank = Me->GetMap()->GetDifficulty();
                if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool2ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool2ByTempId(Itr->CreTempId));
            }
            else if (Itr->MapOrAreaId < 0)
            {
                if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool2ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool2ByTempId(Itr->CreTempId));
            }
        }
    }

    return float(CInfo->resistance[SPELL_SCHOOL_FIRE]);
}

float AsCreature::AsCustomSchool3(Creature* Me)
{
    CreatureTemplate const* CInfo = Me->GetCreatureTemplate();

    if (IsInCustom(Me))
    {
        if (sCombatPetMgr->IsCombatPetData(Me))
            return float(sHelper->GetCreSchool3ByTempId(Me->StTempId));

        for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
        {
            if (Itr->CreId == Me->GetEntry() && Me->StTempId && sHelper->GetCreSchool3ByTempId(Me->StTempId))
                return float(sHelper->GetCreSchool3ByTempId(Me->StTempId));
        }

        uint32 MapId = Me->GetMapId();

        if (IsChallengeMap(MapId))
        {
            for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
            {
                if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool3ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool3ByTempId(Itr->CreTempId));
            }
        }

        for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
        {
            if (Itr->MapOrAreaId >= 0)
            {
                uint32 MapRank = Me->GetMap()->GetDifficulty();
                if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool3ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool3ByTempId(Itr->CreTempId));
            }
            else if (Itr->MapOrAreaId < 0)
            {
                if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool3ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool3ByTempId(Itr->CreTempId));
            }
        }
    }

    return float(CInfo->resistance[SPELL_SCHOOL_NATURE]);
}

float AsCreature::AsCustomSchool4(Creature* Me)
{
    CreatureTemplate const* CInfo = Me->GetCreatureTemplate();

    if (IsInCustom(Me))
    {
        if (sCombatPetMgr->IsCombatPetData(Me))
            return float(sHelper->GetCreSchool4ByTempId(Me->StTempId));

        for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
        {
            if (Itr->CreId == Me->GetEntry() && Me->StTempId && sHelper->GetCreSchool4ByTempId(Me->StTempId))
                return float(sHelper->GetCreSchool4ByTempId(Me->StTempId));
        }

        uint32 MapId = Me->GetMapId();

        if (IsChallengeMap(MapId))
        {
            for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
            {
                if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool4ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool4ByTempId(Itr->CreTempId));
            }
        }

        for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
        {
            if (Itr->MapOrAreaId >= 0)
            {
                uint32 MapRank = Me->GetMap()->GetDifficulty();
                if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool4ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool4ByTempId(Itr->CreTempId));
            }
            else if (Itr->MapOrAreaId < 0)
            {
                if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool4ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool4ByTempId(Itr->CreTempId));
            }
        }
    }

    return float(CInfo->resistance[SPELL_SCHOOL_FROST]);
}

float AsCreature::AsCustomSchool5(Creature* Me)
{
    CreatureTemplate const* CInfo = Me->GetCreatureTemplate();

    if (IsInCustom(Me))
    {
        if (sCombatPetMgr->IsCombatPetData(Me))
            return float(sHelper->GetCreSchool5ByTempId(Me->StTempId));

        for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
        {
            if (Itr->CreId == Me->GetEntry() && Me->StTempId && sHelper->GetCreSchool5ByTempId(Me->StTempId))
                return float(sHelper->GetCreSchool5ByTempId(Me->StTempId));
        }

        uint32 MapId = Me->GetMapId();

        if (IsChallengeMap(MapId))
        {
            for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
            {
                if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool5ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool5ByTempId(Itr->CreTempId));
            }
        }

        for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
        {
            if (Itr->MapOrAreaId >= 0)
            {
                uint32 MapRank = Me->GetMap()->GetDifficulty();
                if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool5ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool5ByTempId(Itr->CreTempId));
            }
            else if (Itr->MapOrAreaId < 0)
            {
                if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool5ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool5ByTempId(Itr->CreTempId));
            }
        }
    }

    return float(CInfo->resistance[SPELL_SCHOOL_SHADOW]);
}

float AsCreature::AsCustomSchool6(Creature* Me)
{
    CreatureTemplate const* CInfo = Me->GetCreatureTemplate();

    if (IsInCustom(Me))
    {
        if (sCombatPetMgr->IsCombatPetData(Me))
            return float(sHelper->GetCreSchool6ByTempId(Me->StTempId));

        for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
        {
            if (Itr->CreId == Me->GetEntry() && Me->StTempId && sHelper->GetCreSchool6ByTempId(Me->StTempId))
                return float(sHelper->GetCreSchool6ByTempId(Me->StTempId));
        }

        uint32 MapId = Me->GetMapId();

        if (IsChallengeMap(MapId))
        {
            for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
            {
                if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool6ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool6ByTempId(Itr->CreTempId));
            }
        }

        for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
        {
            if (Itr->MapOrAreaId >= 0)
            {
                uint32 MapRank = Me->GetMap()->GetDifficulty();
                if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool6ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool6ByTempId(Itr->CreTempId));
            }
            else if (Itr->MapOrAreaId < 0)
            {
                if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId && sHelper->GetCreSchool6ByTempId(Itr->CreTempId))
                    return float(sHelper->GetCreSchool6ByTempId(Itr->CreTempId));
            }
        }
    }

    return float(CInfo->resistance[SPELL_SCHOOL_ARCANE]);
}

std::string AsCreature::GetMazLootIds(Creature* Me)
{
    if (IsNotFor(Me))
        return "";

    if (sCombatPetMgr->IsCombatPetData(Me))
        return "";

    if (Me->StTempId > 0)
        return sHelper->GetCrePLootIdsByTempId(Me->StTempId);

    if (IsOneCreStore(Me))
        return "";

    if (!UseLegacyMapCreatureAttributes())
        return "";

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCrePLootIdsByTempId(Itr->CreTempId);
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCrePLootIdsByTempId(Itr->CreTempId);
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCrePLootIdsByTempId(Itr->CreTempId);
        }
    }

    return "";
}

uint32 AsCreature::GetMazHpNow(Creature* Me)
{
    return AsInt64ToUInt32Clamp(GetAsMazHpNow(Me), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 AsCreature::GetAsMazHpNow(Creature* Me)
{
    if (IsNotFor(Me))
        return 0;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return 0;

    if (IsOneCreStore(Me))
        return 0;

    if (!UseLegacyMapCreatureAttributes())
        return 0;

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetAsCreNowHpByTempId(Itr->CreTempId);
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetAsCreNowHpByTempId(Itr->CreTempId);
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetAsCreNowHpByTempId(Itr->CreTempId);
        }
    }

    return 0;
}

uint32 AsCreature::GetMazDmgTakenNow(Creature* Me)
{
    return AsInt64ToUInt32Clamp(GetAsMazDmgTakenNow(Me), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 AsCreature::GetAsMazDmgTakenNow(Creature* Me)
{
    if (IsNotFor(Me))
        return 0;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return 0;

    if (IsOneCreStore(Me))
        return 0;

    if (!UseLegacyMapCreatureAttributes())
        return 0;

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetAsCreDmgTakenNowByTempId(Itr->CreTempId);
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetAsCreDmgTakenNowByTempId(Itr->CreTempId);
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetAsCreDmgTakenNowByTempId(Itr->CreTempId);
        }
    }

    return 0;
}

uint32 AsCreature::GetMazDmgNow(Creature* Me)
{
    return AsInt64ToUInt32Clamp(GetAsMazDmgNow(Me), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 AsCreature::GetAsMazDmgNow(Creature* Me)
{
    if (IsNotFor(Me))
        return 0;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return 0;

    if (IsOneCreStore(Me))
        return 0;

    if (!UseLegacyMapCreatureAttributes())
        return 0;

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetAsCreDmgNowByTempId(Itr->CreTempId);
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetAsCreDmgNowByTempId(Itr->CreTempId);
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetAsCreDmgNowByTempId(Itr->CreTempId);
        }
    }

    return 0;
}

float AsCreature::GetMazKtHpMod(Creature* Me)
{
    if (IsNotFor(Me))
        return 0.0f;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return 0.0f;

    if (IsOneCreStore(Me))
        return 0.0f;

    if (!UseLegacyMapCreatureAttributes())
        return 0.0f;

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
        {
            if (Itr->MapId == MapId && Itr->Diff == Me->GetMap()->GetMapRank() && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreKtHpByTempId(Itr->CreTempId);
        }
    }

    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreKtHpByTempId(Itr->CreTempId);
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
                return sHelper->GetCreKtHpByTempId(Itr->CreTempId);
        }
    }

    return 0.0f;
}

void AsCreature::DoAsAddAuras(Creature* Me)
{
    CreAddAuras(Me);
    MazAddAuras(Me);
    //sSecretPlaceMgr->AddSecretCreCast(Me);
}

void AsCreature::MazAddAuras(Creature* Me)
{
    if (IsNotFor(Me))
        return;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return;

    if (IsOneCreStore(Me))
        return;

    if (!UseLegacyMapCreatureAttributes())
        return;

    uint32 MapId = Me->GetMapId();

    if (IsChallengeMap(MapId))
    {
        ChallengeAddAuras(Me);
        return;
    }

    for (auto It = Me->TopAuras.begin(); It != Me->TopAuras.end(); It++)
    {
        if (Me->HasAura(*It))
            Me->RemoveAurasDueToSpell(*It);
    }
    Me->TopAuras.clear();
    for (auto Itr = MazCreatureStructStore.begin(); Itr != MazCreatureStructStore.end(); ++Itr)
    {
        if (Itr->MapOrAreaId >= 0)
        {
            uint32 MapRank = Me->GetMap()->GetDifficulty();
            if (Itr->MapOrAreaId == MapId && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
            {
                if (roll_chance_f(sHelper->GetCreAuraChanceByTempId(Itr->CreTempId)))
                {
                    std::vector<uint32> DoVect;
                    DoVect.clear();
                    for (auto It = AuraVect.begin(); It != AuraVect.end(); ++It)
                    {
                        if (It->GroupId == sHelper->GetCreAuraGroupByTempId(Itr->CreTempId))
                            DoVect.push_back(It->AuraId);
                    }

                    Acore::Containers::AtRandomResize(DoVect, sHelper->GetCreAuraCountByTempId(Itr->CreTempId));
                    for (auto const& AuraId : DoVect)
                    {
                        if (!Me->HasAura(AuraId))
                            Me->AddAura(AuraId, Me);

                        Me->TopAuras.insert(AuraId);
                    }
                }

                std::set<uint32> Temps = sHelper->GetCreAtAurasByTempId(Itr->CreTempId);
                for (auto const& AuraId : Temps)
                {
                    if (!Me->HasAura(AuraId))
                        Me->AddAura(AuraId, Me);

                    Me->TopAuras.insert(AuraId);
                }
            }
        }
        else if (Itr->MapOrAreaId < 0)
        {
            if (Itr->MapOrAreaId == -int32(Me->GetAreaId()) && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
            {
                if (roll_chance_f(sHelper->GetCreAuraChanceByTempId(Itr->CreTempId)))
                {
                    std::vector<uint32> DoVect;
                    DoVect.clear();
                    for (auto It = AuraVect.begin(); It != AuraVect.end(); ++It)
                    {
                        if (It->GroupId == sHelper->GetCreAuraGroupByTempId(Itr->CreTempId))
                            DoVect.push_back(It->AuraId);
                    }

                    Acore::Containers::AtRandomResize(DoVect, sHelper->GetCreAuraCountByTempId(Itr->CreTempId));
                    for (auto const& AuraId : DoVect)
                    {
                        if (!Me->HasAura(AuraId))
                            Me->AddAura(AuraId, Me);

                        Me->TopAuras.insert(AuraId);
                    }

                    std::set<uint32> Temps = sHelper->GetCreAtAurasByTempId(Itr->CreTempId);
                    for (auto const& AuraId : Temps)
                    {
                        if (!Me->HasAura(AuraId))
                            Me->AddAura(AuraId, Me);

                        Me->TopAuras.insert(AuraId);
                    }
                }
            }
        }
    }
}

bool AsCreature::IsInCustom(Creature* Me)
{
    if (IsNotFor(Me))
        return false;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return true;

    if (!UseLegacyMapCreatureAttributes())
        return false;

    uint32 MapId = Me->GetMapId();
    if (IsChallengeMap(MapId))
        return true;

    bool In = false;

    for (auto Itr = CreatureStructStore.begin(); Itr != CreatureStructStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Itr->CreId == Me->GetEntry() && Itr->Diff == MapRank && Me->StTempId)
            In = true;
    }

    for (auto It = MazCreatureStructStore.begin(); It != MazCreatureStructStore.end(); ++It)
    {
        if (It->MapOrAreaId >= 0)
        {
            if (It->MapOrAreaId == MapId && It->CreTempId)
                In = true;
        }
        else if (It->MapOrAreaId < 0)
        {
            if (It->MapOrAreaId == -int32(Me->GetAreaId()) && It->CreTempId)
                In = true;
        }
    }

    return In;
}

void AsCreature::ChallengeAddAuras(Creature* Me)
{
    if (IsNotFor(Me))
        return;

    if (!UseLegacyMapCreatureAttributes())
        return;

    for (auto It = Me->TopAuras.begin(); It != Me->TopAuras.end(); It++)
    {
        if (Me->HasAura(*It))
            Me->RemoveAurasDueToSpell(*It);
    }

    Me->TopAuras.clear();
    for (auto Itr = MazChallengeStore.begin(); Itr != MazChallengeStore.end(); ++Itr)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank();
        if (Itr->MapId == Me->GetMapId() && Itr->Diff == MapRank && Itr->Flag == IsCustomBoss(Me) && Itr->CreTempId)
        {
            if (roll_chance_f(sHelper->GetCreAuraChanceByTempId(Itr->CreTempId)))
            {
                std::vector<uint32> DoVect;
                DoVect.clear();
                for (auto It = AuraVect.begin(); It != AuraVect.end(); ++It)
                {
                    if (It->GroupId == sHelper->GetCreAuraGroupByTempId(Itr->CreTempId))
                        DoVect.push_back(It->AuraId);
                }

                Acore::Containers::AtRandomResize(DoVect, sHelper->GetCreAuraCountByTempId(Itr->CreTempId));
                for (auto const& AuraId : DoVect)
                {
                    if (!Me->HasAura(AuraId))
                        Me->AddAura(AuraId, Me);

                    Me->TopAuras.insert(AuraId);
                }
            }

            std::set<uint32> Temps = sHelper->GetCreAtAurasByTempId(Itr->CreTempId);
            for (auto const& AuraId : Temps)
            {
                if (!Me->HasAura(AuraId))
                    Me->AddAura(AuraId, Me);

                Me->TopAuras.insert(AuraId);
            }
        }
    }
}

bool AsCreature::UseLegacyMapCreatureAttributes() const
{
    // 地图生物属性已统一由 _模板_地图生物控制 处理。
    return false;
}

MazChallenge const* AsCreature::GetChallengeConfig(uint32 MapId, uint32 Rank) const
{
    auto itr = MazChallengeDefinitionStore.find(std::make_pair(MapId, Rank));
    return itr != MazChallengeDefinitionStore.end() ? &itr->second : nullptr;
}

bool AsCreature::IsChallengeMap(uint32 MapId)
{
    MapEntry const* MEntry = sMapStore.LookupEntry(MapId);
    if (!MEntry)
        return false;

    if (!MEntry->IsDungeon())
        return false;

    auto itr = MazChallengeDefinitionStore.lower_bound(std::make_pair(MapId, 0u));
    return itr != MazChallengeDefinitionStore.end() && itr->first.first == MapId;
}

void AsCreature::DoApplyChallengeLogin(Player* player, uint32 MapId)
{
    uint32 GuidLow = player->GetGUIDLow();
    player->SetDungeonDifficulty(Difficulty(0));
    player->SetRaidDifficulty(Difficulty(0));
    for (auto Itr = ChallengeDataStore.begin(); Itr != ChallengeDataStore.end(); Itr++)
    {
        if (Itr->MapId == MapId && Itr->GuidLow == GuidLow)
            player->SetMapRank(Itr->Level);
    }
}

void AsCreature::DoApplyChallenge(Player* player, uint32 MapId, uint32 Rank, bool showMsg)
{
    uint32 GuidLow = player->GetGUIDLow();
    player->SetDungeonDifficulty(Difficulty(0));
    player->SetRaidDifficulty(Difficulty(0));

    bool Exsist = false;
    for (auto Itr = ChallengeDataStore.begin(); Itr != ChallengeDataStore.end(); Itr++)
    {
        if (Itr->MapId == MapId && Itr->GuidLow == GuidLow && Itr->Level == Rank)
        {
            Itr->Bind = true;
            Exsist = true;
        }
    }

    if (!Exsist)
    {
        ChallengeData Data;
        Data.MapId = MapId;
        Data.GuidLow = GuidLow;
        Data.Level = Rank;
        Data.Bind = true;
        ChallengeDataStore.push_back(Data);
    }

    // 进入挑战模式提示（已临时禁用，如需恢复请取消下面两行注释）
    // if (showMsg)
    //     player->SendAsMsg(sHelper->GetGts(1501), Rank);
}

bool AsCreature::InChallenge(Player* player, uint32 MapId, uint32 Rank)
{
    uint32 GuidLow = player->GetGUIDLow();
    for (auto Itr = ChallengeDataStore.begin(); Itr != ChallengeDataStore.end(); Itr++)
        if (Itr->MapId == MapId && Itr->GuidLow == GuidLow && Itr->Level == Rank)
            return Itr->Bind;

    return false;
}

void AsCreature::ResetChallengeLevel(Player* player, uint32 MapId, uint32 Rank)
{
    uint32 GuidLow = player->GetGUIDLow();
    for (auto Itr = ChallengeDataStore.begin(); Itr != ChallengeDataStore.end(); Itr++)
        if (Itr->MapId == MapId && Itr->GuidLow == GuidLow && Itr->Level == Rank)
            Itr->Bind = false;

    // 重置副本实例，让小怪复活
    player->CustomUnbinds(MapId);
}

void AsCreature::AddGossipForChallenge(Player* player, uint32 TriggerId)
{
    player->ATtriggerId = TriggerId;

    AreaTriggerTeleport const* AreaTr = sObjectMgr->GetAreaTriggerTeleport(TriggerId);
    if (!AreaTr)
        return;

    uint32 TargetMapId = AreaTr->target_mapId;
    if (!IsChallengeMap(TargetMapId))
    {
        player->SetCanTeleport(true);
        player->TeleportTo(AreaTr->target_mapId, AreaTr->target_X, AreaTr->target_Y, AreaTr->target_Z, AreaTr->target_Orientation, TELE_TO_NOT_LEAVE_TRANSPORT);
        return;
    }

    //如果在队伍中 自己不是队长 并且队长没有选择难度时
    if (Group* Gp = player->GetGroup())
    {
        if (Player* Leader = ObjectAccessor::FindPlayer(Gp->GetLeaderGUID()))
        {
            if (Leader->GetGUIDLow() != player->GetGUIDLow())
            {
                if (Leader->GetMapId() == TargetMapId)
                {
                    player->SetMapRank(Leader->GetMap()->GetMapRank());
                    player->SetCanTeleport(true);
                    player->TeleportTo(AreaTr->target_mapId, AreaTr->target_X, AreaTr->target_Y, AreaTr->target_Z, AreaTr->target_Orientation, TELE_TO_NOT_LEAVE_TRANSPORT);
                }
                else
                {
                    if (!InChallenge(player, TargetMapId, Leader->GetMap()->GetMapRank()))
                    {
                        player->SendAsMsg(sHelper->GetGts(1502), sHelper->GetNameLinkColor(Leader).c_str());
                        Leader->SendAsMsg(sHelper->GetGts(1503), sHelper->GetNameLinkColor(player).c_str());
                    }
                }

                return;
            }
        }
    }

    ClearGossipMenuFor(player);
    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6721), TriggerId, 0);

    auto definitionItr = MazChallengeDefinitionStore.lower_bound(std::make_pair(TargetMapId, 0u));
    for (; definitionItr != MazChallengeDefinitionStore.end() && definitionItr->first.first == TargetMapId; ++definitionItr)
    {
        MazChallenge const& challenge = definitionItr->second;
        if (InChallenge(player, challenge.MapId, challenge.Diff))
        {
            player->SetMapRank(challenge.Diff);
            player->SetCanTeleport(true);
            player->TeleportTo(AreaTr->target_mapId, AreaTr->target_X, AreaTr->target_Y, AreaTr->target_Z, AreaTr->target_Orientation, TELE_TO_NOT_LEAVE_TRANSPORT);
            break;
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, challenge.GossipText, TriggerId, challenge.Diff);
    }

    GossipSetMenuId(player, AtChallenge);
    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, player->GetGUID());
}

uint32 AsCreature::GetChallengeNTempId(uint32 MapId, uint32 Rank)
{
    MazChallenge const* challenge = GetChallengeConfig(MapId, Rank);
    return challenge ? challenge->NTempId : 0;
}

uint32 AsCreature::GetSumId(uint32 Stage)
{
    auto Itr = StageStore.find(Stage);
    return Itr != StageStore.end() ? Itr->second.SummonsGroupId : 0;
}

uint32 AsCreature::GetGobId(uint32 Stage)
{
    auto Itr = StageStore.find(Stage);
    return Itr != StageStore.end() ? Itr->second.RewardGameobjectId : 0;
}

uint32 AsCreature::SummonCreatureGroup(Creature* Me, uint32 SumId)
{
    auto Itr = StageGroupStore.find(SumId);
    if (Itr == StageGroupStore.end())
        return 0;

    uint32 SummonedCount = 0;

    for (auto Iter = Itr->second.begin(); Iter != Itr->second.end(); ++Iter)
    {
        Position Pos;
        Pos.Relocate(Iter->X, Iter->Y, Iter->Z, Iter->O);
        for (size_t i = 0; i < Iter->Count; i++)
        {
            if (Creature* Sum = Me->SummonCreature(Iter->CreId, Pos))
            {
                Sum->SelectLevel(false);
                ++SummonedCount;
            }
        }
    }

    return SummonedCount;
}

std::string AsCreature::GetDescription(uint32 Stage)
{
    auto Itr = StageStore.find(Stage);
    return Itr != StageStore.end() ? Itr->second.Description : "空数据";
}

uint32 AsCreature::GetNtSumId(uint32 Rank)
{
    auto Itr = NtRankStore.find(Rank);
    return Itr != NtRankStore.end() ? Itr->second.SGroupId : 0;
}

uint32 AsCreature::GetNtRItemId(uint32 Rank)
{
    auto Itr = NtRankStore.find(Rank);
    return Itr != NtRankStore.end() ? Itr->second.RItemId : 0;
}

uint32 AsCreature::NtSumCreatureGroup(Creature* Me, uint32 NtSumId)
{
    auto Itr = NtRankGroupStore.find(NtSumId);
    if (Itr == NtRankGroupStore.end())
        return 0;

    uint32 SummonedCount = 0;

    for (auto Iter = Itr->second.begin(); Iter != Itr->second.end(); ++Iter)
    {
        Position Pos;
        Pos.Relocate(Iter->X, Iter->Y, Iter->Z, Iter->O);
        for (size_t i = 0; i < Iter->Count; i++)
        {
            if (Creature* Sum = Me->SummonCreature(Iter->CreId, Pos))
            {
                Sum->SelectLevel(false);
                ++SummonedCount;
            }
        }
    }

    return SummonedCount;
}

std::string AsCreature::GetNtDes(uint32 Rank)
{
    auto Itr = NtRankStore.find(Rank);
    return Itr != NtRankStore.end() ? Itr->second.Des : "空数据";
}

void AsCreature::OnOneInitEntry(Creature* Me, uint32& AtModeId, float& AtScale, uint32& AtWp1, uint32& AtWp2, uint32& AtWp3)
{
    if (sAsCreature->IsNotFor(Me))
        return;

    if (sCombatPetMgr->IsCombatPetData(Me))
        return;

    uint32 AGroup = 0;
    for (auto Iter = CreatureStructStore.begin(); Iter != CreatureStructStore.end(); Iter++)
    {
        uint32 MapRank = Me->GetMap()->GetMapRank() > 0 ? Me->GetMap()->GetMapRank() : Me->GetMap()->GetDifficulty();
        if (Iter->CreId == Me->GetEntry() && Iter->Diff == MapRank)
            AGroup = Iter->SpGroupId;
    }

    if (!AGroup)
        return;

    struct ChanceTemp { uint32 StTempId; uint32 ModeId; float Scale; uint32 EqWp1; uint32 EqWp2; uint32 EqWp3; float Chance; };
    std::vector<ChanceTemp> ChanceList;
    ChanceList.clear();

    for (auto It = OneCreGroupStore.begin(); It != OneCreGroupStore.end(); It++)
    {
        if (It->GroupId == AGroup)
        {
            ChanceTemp St;
            St.StTempId = It->StTempId;
            St.ModeId = It->ModeId;
            St.Scale = It->Scale;
            St.EqWp1 = It->EqWp1;
            St.EqWp2 = It->EqWp2;
            St.EqWp3 = It->EqWp3;
            St.Chance = It->Chance;
            ChanceList.push_back(St);
        }
    }

    if (!ChanceList.empty())
    {
        float RIdx = 0.0f;
        for (auto Itr = ChanceList.begin(); Itr != ChanceList.end(); Itr++)
            RIdx += Itr->Chance;

        for (auto It = ChanceList.begin(); It != ChanceList.end(); It++)
        {
            if (frand(0, RIdx) <= It->Chance)
            {
                Me->StTempId = It->StTempId;

                if (sHelper->GetBoolean(22010) && sHuanJingSystem)
                {
                    Me->MultipleHp = 1;
                    Me->MaxMultipleHp = 1;
                    sHuanJingSystem->EnsureCreatureHealthBarsFromConfig(Me, nullptr, false);
                }
                else
                {
                    Me->MultipleHp = sHelper->GetCreHpCountByTempId(Me->StTempId);
                    Me->MaxMultipleHp = Me->MultipleHp;
                }

                AtModeId = It->ModeId;
                AtScale = It->Scale;
                AtWp1 = It->EqWp1;
                AtWp2 = It->EqWp2;
                AtWp3 = It->EqWp3;

                break;
            }
            else
                RIdx -= It->Chance;
        }

        ChanceList.clear();
    }
}

void AsCreature::GetSumTelePos(uint32 PosId, uint32& MapId, float& X, float& Y, float& Z, float& O)
{
    auto It = PosMap.find(PosId);
    if (It != PosMap.end())
    {
        MapId = It->second.MapId;
        X = It->second.X;
        Y = It->second.Y;
        Z = It->second.Z;
        O = It->second.O;
    }
}

void AsCreature::SendUpdateMultipleHp(Player* player, Creature* Killed)
{
    if (!Killed)
        return;

    std::string msg;
    if (!sHuanJingSystem || !sHuanJingSystem->BuildCustomTargetHealthPacket(Killed, msg))
    {
        // The new HuanJing health-bar packet is the only display path while it
        // is enabled; never fall back to the retired legacy payload.
        if (sHelper->GetBoolean(22010))
            return;

        if (Killed->MaxMultipleHp <= 1)
            return;

        AsInt64 barHealth = Killed->GetAsHealth();
        if (barHealth <= 0)
            barHealth = AsInt64(Killed->GetHealth());

        AsInt64 barMax = Killed->GetAsMaxHealth();
        if (barMax <= 0)
            barMax = AsInt64(Killed->GetMaxHealth());

        msg = std::to_string(Killed->GetGUIDLow()) + "#" +
            std::to_string(Killed->MultipleHp) + "#" +
            std::to_string(Killed->MaxMultipleHp) + "#" +
            AsInt64ToString(barHealth) + "#" +
            AsInt64ToString(barMax);
    }

    std::unordered_set<ObjectGuid> recipients;
    auto SendUpdate = [&recipients, &msg](Player* recipient)
    {
        if (!recipient || !recipient->IsInWorld() || !recipients.insert(recipient->GetGUID()).second)
            return;

        sDoAddon->SendPacketToClient(recipient, "CustomTargetHpCount", msg);
    };

    SendUpdate(player);

    for (ThreatReference const* threatRef : Killed->GetThreatMgr().GetUnsortedThreatList())
    {
        Unit* Tar = threatRef->GetVictim();
        if (Tar && Tar->ToPlayer())
            SendUpdate(Tar->ToPlayer());
    }

    // 生物刚刷出时还没有仇恨列表；将初始血条状态发送给附近玩家，
    // 客户端无需等待选中目标或首次受伤才获得多血条数据。
    if (!player)
    {
        if (Map* map = Killed->GetMap())
        {
            for (auto const& playerRef : map->GetPlayers())
            {
                Player* nearbyPlayer = playerRef.GetSource();
                if (nearbyPlayer && Killed->GetDistance(nearbyPlayer) <= 50.0f)
                    SendUpdate(nearbyPlayer);
            }
        }
    }
}

void AsCreature::SendResetMultipleHp(Player* player, uint32 CreGuidLow)
{
    if (player)
        sDoAddon->SendPacketToClient(player, "ResetCustomTargetHp", std::to_string(CreGuidLow));
}

namespace
{
enum StageAIAction : int32
{
    ACTION_STAGE_START = 1
};

enum StageAIGuid : int32
{
    GUID_STAGE_STARTER = 1
};

enum class StageRunState : uint8
{
    Idle,
    Starting,
    Fighting,
    Resetting
};

enum class StageSessionState : uint8
{
    Ready,
    Fighting,
    Cleared,
    ChapterCompleted
};

struct StageChallengeProgress
{
    uint32 HighestUnlockedStage = 1;
    uint32 HighestCompletedChapter = 0;
};

struct StageChallengeSession
{
    uint32 LeaderGuid = 0;
    std::vector<uint32> MemberGuids;
    uint32 ChapterId = 0;
    uint32 CurrentStageId = 0;
    uint32 MapId = 0;
    uint32 InstanceId = 0;
    ObjectGuid ControllerGuid = ObjectGuid::Empty;
    ObjectGuid PortalGuid = ObjectGuid::Empty;
    StageSessionState State = StageSessionState::Ready;
};

constexpr uint32 StageWatchdogInterval = 2 * IN_MILLISECONDS;
constexpr uint32 StageEntranceMemberRange = 100;
constexpr uint32 StagePortalLifetime = HOUR;
constexpr uint32 StageChapterGossipSender = 9100;

std::unordered_map<uint32, StageChallengeProgress> StageProgressStore;
std::unordered_map<uint32, StageChallengeSession> StageSessionStore;
std::unordered_map<uint32, uint32> StageMemberSessionStore;
int8 StageProgressTableState = -1;

uint32 GetDefaultStageId()
{
    uint32 StageId = 0;
    for (auto const& Pair : StageStore)
        if (!StageId || Pair.first < StageId)
            StageId = Pair.first;
    return StageId ? StageId : 1;
}

bool HasStageProgressTable()
{
    if (StageProgressTableState < 0)
        StageProgressTableState = CharacterDatabase.Query("SHOW TABLES LIKE '_副本闯关进度';") ? 1 : 0;
    return StageProgressTableState == 1;
}

StageChallengeProgress& GetStageProgress(uint32 GuidLow)
{
    auto Itr = StageProgressStore.find(GuidLow);
    if (Itr != StageProgressStore.end())
        return Itr->second;

    StageChallengeProgress Progress;
    Progress.HighestUnlockedStage = GetDefaultStageId();
    if (HasStageProgressTable())
    {
        if (QueryResult Result = CharacterDatabase.Query("SELECT 最高已解锁关卡, 最高完成章节 FROM _副本闯关进度 WHERE 角色GUID = {}", GuidLow))
        {
            Field* Fields = Result->Fetch();
            Progress.HighestUnlockedStage = std::max(GetDefaultStageId(), Fields[0].Get<uint32>());
            Progress.HighestCompletedChapter = Fields[1].Get<uint32>();
        }
    }

    return StageProgressStore.emplace(GuidLow, Progress).first->second;
}

void SaveStageProgress(uint32 GuidLow, uint32 HighestUnlockedStage, uint32 HighestCompletedChapter)
{
    StageChallengeProgress& Progress = GetStageProgress(GuidLow);
    Progress.HighestUnlockedStage = std::max(Progress.HighestUnlockedStage, HighestUnlockedStage);
    Progress.HighestCompletedChapter = std::max(Progress.HighestCompletedChapter, HighestCompletedChapter);

    if (HasStageProgressTable())
    {
        CharacterDatabase.Execute(
            "INSERT INTO _副本闯关进度 (角色GUID, 最高已解锁关卡, 最高完成章节) VALUES ({}, {}, {}) "
            "ON DUPLICATE KEY UPDATE 最高已解锁关卡 = GREATEST(最高已解锁关卡, VALUES(最高已解锁关卡)), "
            "最高完成章节 = GREATEST(最高完成章节, VALUES(最高完成章节)), 更新时间 = CURRENT_TIMESTAMP",
            GuidLow, Progress.HighestUnlockedStage, Progress.HighestCompletedChapter);
    }
}

StageTemplate const* GetStageForChapter(uint32 ChapterId, uint32 ChapterStage, uint32* StageId = nullptr)
{
    for (auto const& Pair : StageStore)
    {
        if (Pair.second.ChapterId == ChapterId && Pair.second.ChapterStage == ChapterStage)
        {
            if (StageId)
                *StageId = Pair.first;
            return &Pair.second;
        }
    }
    return nullptr;
}

uint32 GetFirstStageId(uint32 ChapterId)
{
    uint32 StageId = 0;
    GetStageForChapter(ChapterId, 1, &StageId);
    return StageId;
}

StageChallengeSession* FindStageSessionByLeader(uint32 LeaderGuid)
{
    auto Itr = StageSessionStore.find(LeaderGuid);
    return Itr != StageSessionStore.end() ? &Itr->second : nullptr;
}

StageChallengeSession* FindStageSession(Player const* player)
{
    if (!player)
        return nullptr;

    auto MemberItr = StageMemberSessionStore.find(player->GetGUIDLow());
    return MemberItr != StageMemberSessionStore.end() ? FindStageSessionByLeader(MemberItr->second) : nullptr;
}

StageChallengeSession* FindStageSession(Creature const* creature)
{
    if (!creature)
        return nullptr;

    for (auto& Pair : StageSessionStore)
        if (Pair.second.ControllerGuid == creature->GetGUID())
            return &Pair.second;
    return nullptr;
}

void SendStageSessionMessage(StageChallengeSession const& Session, std::string const& Message)
{
    for (uint32 GuidLow : Session.MemberGuids)
        if (Player* Member = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(GuidLow)))
            Member->SendSysMsg("[副本闯关]: %s", Message.c_str());
}

void DestroyStageSession(uint32 LeaderGuid)
{
    auto Itr = StageSessionStore.find(LeaderGuid);
    if (Itr == StageSessionStore.end())
        return;

    for (uint32 GuidLow : Itr->second.MemberGuids)
        StageMemberSessionStore.erase(GuidLow);
    StageSessionStore.erase(Itr);
}

bool IsStageSessionLeader(Player const* player, StageChallengeSession const& Session)
{
    return player && player->GetGUIDLow() == Session.LeaderGuid;
}

uint32 GetStagePortalEntry(StageTemplate const& Stage, StageChapterTemplate const& Chapter)
{
    return Stage.PortalGameobjectId ? Stage.PortalGameobjectId : Chapter.DefaultPortalGameobjectId;
}

bool IsStagePositionValid(uint32 PositionId, uint32 MapId)
{
    auto PositionItr = PosMap.find(PositionId);
    return PositionItr != PosMap.end() && PositionItr->second.MapId == MapId;
}

bool MoveStageSessionToPosition(StageChallengeSession& Session, WorldObject& Context, uint32 PositionId)
{
    auto PositionItr = PosMap.find(PositionId);
    if (PositionItr == PosMap.end() || PositionItr->second.MapId != Context.GetMapId())
        return false;

    PosTemplate const& Position = PositionItr->second;
    for (uint32 GuidLow : Session.MemberGuids)
    {
        Player* Member = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(GuidLow));
        if (Member && Member->IsInWorld() && Member->GetMap() == Context.GetMap())
            Member->NearTeleportTo(Position.X, Position.Y, Position.Z, Position.O, false, false, true);
    }

    if (!Session.ControllerGuid.IsEmpty())
    {
        if (Creature* Controller = ObjectAccessor::GetCreature(Context, Session.ControllerGuid))
        {
            Controller->CombatStop(true);
            Controller->NearTeleportTo(Position.X, Position.Y, Position.Z, Position.O);
        }
    }
    return true;
}

bool TeleportStageSessionToChapter(StageChallengeSession& Session, StageChapterTemplate const& Chapter, WorldObject* Context)
{
    auto PositionItr = PosMap.find(Chapter.EntryPositionId);
    if (PositionItr == PosMap.end() || PositionItr->second.MapId != Chapter.MapId)
        return false;

    uint32 FirstStageId = GetFirstStageId(Chapter.ChapterId);
    if (!FirstStageId)
        return false;

    PosTemplate const& Position = PositionItr->second;
    bool ReuseCurrentInstance = Context && Context->GetMapId() == Chapter.MapId;
    ObjectGuid ExistingControllerGuid = Session.ControllerGuid;
    std::vector<Player*> Members;
    Player* Leader = nullptr;
    for (uint32 GuidLow : Session.MemberGuids)
    {
        if (Player* Member = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(GuidLow)))
        {
            if (GuidLow == Session.LeaderGuid)
                Leader = Member;
            else
                Members.push_back(Member);
        }
    }

    auto TeleportMember = [&](Player* Member)
    {
        if (!Member)
            return;
        if (ReuseCurrentInstance && Member->GetMap() == Context->GetMap())
            Member->NearTeleportTo(Position.X, Position.Y, Position.Z, Position.O, false, false, true);
        else
            Member->TeleportTo(Chapter.MapId, Position.X, Position.Y, Position.Z, Position.O);
    };

    TeleportMember(Leader);
    for (Player* Member : Members)
        TeleportMember(Member);

    if (ReuseCurrentInstance && !ExistingControllerGuid.IsEmpty())
    {
        if (Creature* Controller = ObjectAccessor::GetCreature(*Context, ExistingControllerGuid))
        {
            Controller->CombatStop(true);
            Controller->NearTeleportTo(Position.X, Position.Y, Position.Z, Position.O);
            Controller->AtStage = FirstStageId;
        }
    }

    Session.ChapterId = Chapter.ChapterId;
    Session.CurrentStageId = FirstStageId;
    Session.MapId = Chapter.MapId;
    Session.InstanceId = ReuseCurrentInstance ? Context->GetInstanceId() : 0;
    Session.ControllerGuid = ReuseCurrentInstance ? ExistingControllerGuid : ObjectGuid::Empty;
    Session.PortalGuid = ObjectGuid::Empty;
    Session.State = StageSessionState::Ready;
    return true;
}

bool CollectStageMembers(Player* Leader, uint32 FirstStageId, std::vector<uint32>& Members)
{
    if (Group* Group = Leader->GetGroup())
    {
        if (!Group->IsLeader(Leader->GetGUID()))
        {
            Leader->SendSysMsg("[副本闯关]: 只有队长可以创建章节副本。");
            return false;
        }

        for (GroupReference* Itr = Group->GetFirstMember(); Itr != nullptr; Itr = Itr->next())
        {
            Player* Member = Itr->GetSource();
            if (!Member || !Member->IsInWorld())
                continue;

            if (!Member->IsAlive() || Member->GetMap() != Leader->GetMap() || !Leader->IsWithinDistInMap(Member, float(StageEntranceMemberRange)))
            {
                Leader->SendSysMsg("[副本闯关]: 队员 %s 必须存活并在入口 NPC 的 100 码范围内。", Member->GetName().c_str());
                return false;
            }
            if (FindStageSession(Member))
            {
                Leader->SendSysMsg("[副本闯关]: 队员 %s 已绑定其他闯关进度。", Member->GetName().c_str());
                return false;
            }
            if (GetStageProgress(Member->GetGUIDLow()).HighestUnlockedStage < FirstStageId)
            {
                Leader->SendSysMsg("[副本闯关]: 队员 %s 尚未解锁该章节。", Member->GetName().c_str());
                return false;
            }
            Members.push_back(Member->GetGUIDLow());
        }
    }
    else
    {
        if (FindStageSession(Leader))
        {
            Leader->SendSysMsg("[副本闯关]: 你已经绑定了一个闯关进度。");
            return false;
        }
        Members.push_back(Leader->GetGUIDLow());
    }
    return !Members.empty();
}
}

void AsCreature::HandleStageChallengeInstanceReset(Player* player, bool isRaid)
{
    if (!player)
        return;

    StageChallengeSession* Session = FindStageSession(player);
    if (!Session)
        return;

    MapEntry const* MapInfo = sMapStore.LookupEntry(Session->MapId);
    if (!MapInfo || MapInfo->IsRaid() != isRaid)
        return;

    for (uint32 GuidLow : Session->MemberGuids)
    {
        Player* Member = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(GuidLow));
        if (!Member || !Member->IsInWorld() || Member->GetMapId() != Session->MapId)
            continue;

        if (!Session->InstanceId || Member->GetInstanceId() == Session->InstanceId)
        {
            player->SendSysMsg("[副本闯关]: 队伍仍有成员在章节副本内，本次章节没有重置。请全员离开后由队长再次重置副本。");
            return;
        }
    }

    bool InstanceStillBound = false;
    for (uint32 GuidLow : Session->MemberGuids)
    {
        ObjectGuid MemberGuid = ObjectGuid::Create<HighGuid::Player>(GuidLow);
        for (uint8 DifficultyId = 0; DifficultyId < MAX_DIFFICULTY; ++DifficultyId)
        {
            BoundInstancesMap const& BoundInstances = sInstanceSaveMgr->PlayerGetBoundInstances(MemberGuid, Difficulty(DifficultyId));
            auto BoundItr = BoundInstances.find(Session->MapId);
            if (BoundItr == BoundInstances.end() || !BoundItr->second.save)
                continue;

            if (!Session->InstanceId || BoundItr->second.save->GetInstanceId() == Session->InstanceId)
            {
                InstanceStillBound = true;
                break;
            }
        }

        if (InstanceStillBound)
            break;
    }

    if (InstanceStillBound)
    {
        player->SendSysMsg("[副本闯关]: 章节副本未能重置，本次章节运行状态已保留。");
        return;
    }

    uint32 LeaderGuid = Session->LeaderGuid;
    SendStageSessionMessage(*Session, "队长已重置章节副本，本次章节运行已结束；下次进入该章节时从第一关开始，永久解锁进度和已领取奖励不会回退。");
    DestroyStageSession(LeaderGuid);
}

class LegacyStageCreature : public CreatureScript
{
public:
    LegacyStageCreature() : CreatureScript("LegacyStageCreature") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        Map* TarMap = creature->GetMap();
        if (!TarMap->IsDungeon())
        {
            player->SendSysMsg(sHelper->GetGts(6722));
            return false;
        }

        uint32 Eventid = uint32(sHelper->GetUInt(1204));
        if (Eventid && !sGameEventMgr->IsActiveEvent(Eventid))
        {
            player->SendSysMsg(sHelper->GetGts(6723));
            return false;
        }

        if (creature->SummonsClear)
        {
            auto StageItr = StageStore.find(creature->AtStage);
            if (StageItr == StageStore.end())
            {
                player->SendSysMsg(sHelper->GetGts(6724));
                return false;
            }

            if (StageItr->second.SummonsGroupId == 0)
            {
                player->SendSysMsg("[副本闯关]: 第 %u 关没有配置有效的怪物组。", creature->AtStage);
                return false;
            }

            std::ostringstream Oss;
            if (creature->AtStage == 1)
                Oss << "开始第一关";
            else
                Oss << "进入下一关（第" << creature->AtStage << "关）";

            AddGossipItemFor(player, GOSSIP_ICON_TALK, Oss.str(), creature->AtStage, GOSSIP_ACTION_INFO_DEF);
            AddGossipItemFor(player, GOSSIP_ICON_TALK, sAsCreature->GetDescription(creature->AtStage), creature->AtStage, GOSSIP_ACTION_INFO_DEF + 1);
        }
        else
        {
            player->SendSysMsg(sHelper->GetGts(6726));
            return false;
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        if (Action == GOSSIP_ACTION_INFO_DEF + 1)
        {
            OnGossipHello(player, creature);
            return true;
        }

        if (!creature->SummonsClear)
        {
            player->SendSysMsg(sHelper->GetGts(6726));
            CloseGossipMenuFor(player);
            return true;
        }

        creature->AI()->SetGUID(player->GetGUID(), GUID_STAGE_STARTER);
        creature->AI()->DoAction(ACTION_STAGE_START);
        CloseGossipMenuFor(player);
        return true;
    }

    struct StageCreatureAI : public ScriptedAI
    {
        StageCreatureAI(Creature* creature) : ScriptedAI(creature), Summons(me), EntrancePosition(creature->GetHomePosition()) {}

        SummonList Summons;

        void SetGUID(ObjectGuid const& Guid, int32 Id) override
        {
            if (Id == GUID_STAGE_STARTER)
                StarterGuid = Guid;
        }

        void DoAction(int32 Action) override
        {
            if (Action != ACTION_STAGE_START || RunState != StageRunState::Idle || !me->SummonsClear)
                return;

            Player* player = GetStarter();
            if (!IsPlayerInThisInstance(player) || !player->IsAlive())
            {
                StarterGuid = ObjectGuid::Empty;
                return;
            }

            StartStage(player);
        }

        void JustSummoned(Creature* summon) override
        {
            Summons.Summon(summon);

            sHelper->DoZoneInCombatForPlayer(summon);
        }

        void Reset() override
        {
            ResetChallenge(false);
        }

        void SummonedCreatureDespawn(Creature* Summoned) override
        {
            uint32 PreviousCount = uint32(Summons.size());
            Summons.Despawn(Summoned);

            if (RunState == StageRunState::Fighting && Summons.size() < PreviousCount)
                AbortStage(GetStarter(), "怪物异常消失，本关已中止，可以重新点击挑战。", true);
        }

        void SummonedCreatureDies(Creature* summon, Unit* killer)  override
        {
            Position RewardPosition = summon->GetPosition();
            Summons.Despawn(summon);

            if (RunState != StageRunState::Fighting)
                return;

            std::ostringstream Oss;
            Oss << sHelper->GetGts(6727) << me->AtStage;

            if (Summons.empty())
            {
                uint32 GEntry = sAsCreature->GetGobId(me->AtStage);
                if (sObjectMgr->GetGameObjectTemplate(GEntry))
                {
                    if (killer)
                        RewardPosition.Relocate(killer->GetPositionX(), killer->GetPositionY(), killer->GetPositionZ(), killer->GetOrientation());

                    me->SummonGameObject(GEntry, RewardPosition.GetPositionX(), RewardPosition.GetPositionY(), RewardPosition.GetPositionZ(), RewardPosition.GetOrientation(), 0, 0, 0, 0, 0);
                }

                me->AtStage++;
                me->SummonsClear = true;
                RunState = StageRunState::Idle;
                StarterGuid = ObjectGuid::Empty;

                Oss << sHelper->GetGts(6728);
                if (StageStore.find(me->AtStage) != StageStore.end())
                    Oss << " 点击闯关NPC进入第" << me->AtStage << "关。";
                else
                    Oss << " 已完成全部闯关。";
            }
            else
            {
                Oss << Acore::StringFormat(sHelper->GetGts(6729), Summons.size());
            }

            me->Say(Oss.str().c_str(), LANG_UNIVERSAL);
        }

        void UpdateAI(uint32 Diff) override
        {
            if (RunState != StageRunState::Fighting)
                return;

            if (WatchdogTimer > Diff)
            {
                WatchdogTimer -= Diff;
                return;
            }

            WatchdogTimer = StageWatchdogInterval;

            if (!HasLivingPlayers())
            {
                ResetChallenge(true);
                return;
            }

            uint32 PreviousCount = uint32(Summons.size());
            Summons.RemoveNotExisting();
            if (Summons.size() < PreviousCount)
                AbortStage(GetStarter(), "检测到怪物状态异常，本关已中止，可以重新点击挑战。", true);
        }

    private:
        Player* GetStarter() const
        {
            if (StarterGuid == ObjectGuid::Empty)
                return nullptr;

            return ObjectAccessor::GetPlayer(*me, StarterGuid);
        }

        bool IsPlayerInThisInstance(Player* player) const
        {
            return player && player->IsInWorld() && player->GetMap() == me->GetMap();
        }

        bool HasLivingPlayers() const
        {
            for (auto const& PlayerRef : me->GetMap()->GetPlayers())
            {
                Player* player = PlayerRef.GetSource();
                if (player && player->IsInWorld() && player->IsAlive() && !player->IsGameMaster())
                    return true;
            }

            return false;
        }

        bool MoveToStageRoom(Player* starter, StageTemplate const& Stage)
        {
            if (Stage.PositionId == 0)
                return true;

            auto PositionItr = PosMap.find(Stage.PositionId);
            if (PositionItr == PosMap.end())
            {
                starter->SendSysMsg("[副本闯关]: 第 %u 关的坐标编号 %u 不存在。", me->AtStage, Stage.PositionId);
                LOG_ERROR("server.worldserver", "副本闯关第 {} 关坐标编号 {} 不存在，NPC {}，地图 {}",
                    me->AtStage, Stage.PositionId, me->GetEntry(), me->GetMapId());
                return false;
            }

            PosTemplate const& StagePosition = PositionItr->second;
            if (StagePosition.MapId != me->GetMapId())
            {
                starter->SendSysMsg("[副本闯关]: 第 %u 关坐标必须与闯关NPC位于同一地图。", me->AtStage);
                LOG_ERROR("server.worldserver", "副本闯关第 {} 关坐标 {} 指向地图 {}，但闯关NPC {} 位于地图 {}",
                    me->AtStage, Stage.PositionId, StagePosition.MapId, me->GetEntry(), me->GetMapId());
                return false;
            }

            if (Group* group = starter->GetGroup())
            {
                for (GroupReference* Itr = group->GetFirstMember(); Itr != nullptr; Itr = Itr->next())
                {
                    Player* Member = Itr->GetSource();
                    if (Member && Member->IsInWorld() && Member->IsAlive() && me->IsInMap(Member))
                        Member->NearTeleportTo(StagePosition.X, StagePosition.Y, StagePosition.Z, StagePosition.O, false, false, true);
                }
            }
            else
                starter->NearTeleportTo(StagePosition.X, StagePosition.Y, StagePosition.Z, StagePosition.O, false, false, true);

            me->CombatStop(true);
            me->NearTeleportTo(StagePosition.X, StagePosition.Y, StagePosition.Z, StagePosition.O);
            return true;
        }

        void StartStage(Player* player)
        {
            auto StageItr = StageStore.find(me->AtStage);
            if (StageItr == StageStore.end() || StageItr->second.SummonsGroupId == 0)
            {
                player->SendSysMsg("[副本闯关]: 当前关卡配置无效，请联系管理员。");
                StarterGuid = ObjectGuid::Empty;
                return;
            }

            RunState = StageRunState::Starting;
            me->SummonsClear = false;

            if (!MoveToStageRoom(player, StageItr->second))
            {
                AbortStage(player, "进入关卡房间失败，请联系管理员检查坐标配置。", false);
                return;
            }

            uint32 SummonedCount = sAsCreature->SummonCreatureGroup(me, StageItr->second.SummonsGroupId);
            if (SummonedCount == 0)
            {
                LOG_ERROR("server.worldserver", "副本闯关第 {} 关怪物组 {} 未召唤出任何生物，NPC {}，地图 {}",
                    me->AtStage, StageItr->second.SummonsGroupId, me->GetEntry(), me->GetMapId());
                AbortStage(player, "本关怪物召唤失败，请联系管理员检查怪物组配置。", true);
                return;
            }

            RunState = StageRunState::Fighting;
            WatchdogTimer = StageWatchdogInterval;
            player->SendSysMsg("[副本闯关]: 第 %u 关开始，本关怪物数量：%u。", me->AtStage, SummonedCount);
        }

        void AbortStage(Player* player, char const* Message, bool DespawnSummons)
        {
            RunState = StageRunState::Resetting;
            if (DespawnSummons)
                Summons.DespawnAll();
            else
                Summons.clear();

            me->SummonsClear = true;
            RunState = StageRunState::Idle;
            WatchdogTimer = StageWatchdogInterval;

            if (player && Message)
                player->SendSysMsg("[副本闯关]: %s", Message);

            StarterGuid = ObjectGuid::Empty;
        }

        void ResetChallenge(bool ReturnToEntrance)
        {
            RunState = StageRunState::Resetting;
            Summons.DespawnAll();
            me->SummonsClear = true;
            me->AtStage = 1;
            StarterGuid = ObjectGuid::Empty;
            WatchdogTimer = StageWatchdogInterval;

            if (ReturnToEntrance)
                me->NearTeleportTo(EntrancePosition.GetPositionX(), EntrancePosition.GetPositionY(), EntrancePosition.GetPositionZ(), EntrancePosition.GetOrientation());

            RunState = StageRunState::Idle;
        }

        ObjectGuid StarterGuid = ObjectGuid::Empty;
        Position EntrancePosition;
        uint32 WatchdogTimer = StageWatchdogInterval;
        StageRunState RunState = StageRunState::Idle;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new StageCreatureAI(creature);
    }
};

class StageCreature : public CreatureScript
{
public:
    StageCreature() : CreatureScript("StageCreature") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);
        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        uint32 EventId = uint32(sHelper->GetUInt(1204));
        if (EventId && !sGameEventMgr->IsActiveEvent(EventId))
        {
            player->SendSysMsg(sHelper->GetGts(6723));
            return false;
        }

        if (!creature->GetMap()->IsDungeon())
            return ShowChapterMenu(player, creature);

        StageChallengeSession* Session = FindStageSession(player);
        if (!Session)
        {
            player->SendSysMsg("[副本闯关]: 当前实例没有绑定你的队伍，请从入口 NPC 选择章节进入。");
            return false;
        }

        if (Session->MapId != creature->GetMapId() || (Session->InstanceId && Session->InstanceId != creature->GetInstanceId()))
        {
            player->SendSysMsg("[副本闯关]: 你所在的实例与队伍闯关实例不一致。");
            return false;
        }

        if (!IsStageSessionLeader(player, *Session))
        {
            player->SendSysMsg("[副本闯关]: 当前进度由队长控制，队员不能开始或推进关卡。");
            return false;
        }

        if (Session->InstanceId == 0)
            Session->InstanceId = creature->GetInstanceId();
        Session->ControllerGuid = creature->GetGUID();
        creature->AtStage = Session->CurrentStageId;

        auto StageItr = StageStore.find(Session->CurrentStageId);
        if (StageItr == StageStore.end() || StageItr->second.ChapterId != Session->ChapterId)
        {
            player->SendSysMsg("[副本闯关]: 当前关卡配置无效，请联系管理员。");
            return false;
        }

        if (Session->State == StageSessionState::Ready && creature->SummonsClear)
        {
            std::ostringstream Text;
            Text << "开始第 " << Session->CurrentStageId << " 关";
            if (StageItr->second.IsBossStage)
                Text << "（首领关）";
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, Text.str(), GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, StageItr->second.Description, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        }
        else if (Session->State == StageSessionState::Fighting)
            player->SendSysMsg(sHelper->GetGts(6726));
        else
            player->SendSysMsg("[副本闯关]: 本关已经完成，请使用通关传送门推进。");

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        if (Sender == StageChapterGossipSender)
        {
            StartChapter(player, Action);
            CloseGossipMenuFor(player);
            return true;
        }

        if (Action == GOSSIP_ACTION_INFO_DEF + 1)
        {
            OnGossipHello(player, creature);
            return true;
        }

        StageChallengeSession* Session = FindStageSession(player);
        if (!Session || !IsStageSessionLeader(player, *Session) || Session->State != StageSessionState::Ready || !creature->SummonsClear)
        {
            player->SendSysMsg(sHelper->GetGts(6726));
            CloseGossipMenuFor(player);
            return true;
        }

        creature->AI()->SetGUID(player->GetGUID(), GUID_STAGE_STARTER);
        creature->AI()->DoAction(ACTION_STAGE_START);
        CloseGossipMenuFor(player);
        return true;
    }

    struct StageCreatureAI : public ScriptedAI
    {
        StageCreatureAI(Creature* creature) : ScriptedAI(creature), Summons(me) { }

        SummonList Summons;

        void SetGUID(ObjectGuid const& Guid, int32 Id) override
        {
            if (Id == GUID_STAGE_STARTER)
                StarterGuid = Guid;
        }

        void DoAction(int32 Action) override
        {
            if (Action != ACTION_STAGE_START || RunState != StageRunState::Idle || !me->SummonsClear)
                return;

            Player* player = GetStarter();
            StageChallengeSession* Session = FindStageSession(player);
            if (!player || !player->IsAlive() || player->GetMap() != me->GetMap() || !Session ||
                !IsStageSessionLeader(player, *Session) || Session->State != StageSessionState::Ready)
            {
                StarterGuid = ObjectGuid::Empty;
                return;
            }
            StartStage(player, *Session);
        }

        void JustSummoned(Creature* summon) override
        {
            Summons.Summon(summon);
            sHelper->DoZoneInCombatForPlayer(summon);
        }

        void Reset() override
        {
            ResetStage(false);
        }

        void SummonedCreatureDespawn(Creature* Summoned) override
        {
            uint32 PreviousCount = uint32(Summons.size());
            Summons.Despawn(Summoned);
            if (RunState == StageRunState::Fighting && Summons.size() < PreviousCount)
                AbortStage("怪物异常消失，本关已中止，可以重新点击挑战。", true);
        }

        void SummonedCreatureDies(Creature* summon, Unit* killer) override
        {
            Position RewardPosition = summon->GetPosition();
            Summons.Despawn(summon);
            if (RunState != StageRunState::Fighting)
                return;

            StageChallengeSession* Session = FindStageSession(me);
            if (!Session || Session->State != StageSessionState::Fighting)
                return;

            std::ostringstream Text;
            Text << sHelper->GetGts(6727) << Session->CurrentStageId;
            if (Summons.empty())
            {
                CompleteStage(*Session, RewardPosition, killer);
                me->SummonsClear = true;
                RunState = StageRunState::Idle;
                StarterGuid = ObjectGuid::Empty;
                Text << sHelper->GetGts(6728) << " 请使用通关传送门推进。";
            }
            else
                Text << Acore::StringFormat(sHelper->GetGts(6729), Summons.size());
            me->Say(Text.str().c_str(), LANG_UNIVERSAL);
        }

        void UpdateAI(uint32 Diff) override
        {
            if (RunState != StageRunState::Fighting)
                return;

            if (WatchdogTimer > Diff)
            {
                WatchdogTimer -= Diff;
                return;
            }
            WatchdogTimer = StageWatchdogInterval;

            StageChallengeSession* Session = FindStageSession(me);
            if (!Session || !HasLivingPlayers(*Session))
            {
                ResetStage(true);
                return;
            }

            uint32 PreviousCount = uint32(Summons.size());
            Summons.RemoveNotExisting();
            if (Summons.size() < PreviousCount)
                AbortStage("检测到怪物状态异常，本关已中止，可以重新点击挑战。", true);
        }

    private:
        Player* GetStarter() const
        {
            return StarterGuid.IsEmpty() ? nullptr : ObjectAccessor::GetPlayer(*me, StarterGuid);
        }

        bool HasLivingPlayers(StageChallengeSession const& Session) const
        {
            for (uint32 GuidLow : Session.MemberGuids)
            {
                Player* Member = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(GuidLow));
                if (Member && Member->IsAlive() && Member->GetMap() == me->GetMap())
                    return true;
            }
            return false;
        }

        void StartStage(Player* player, StageChallengeSession& Session)
        {
            auto StageItr = StageStore.find(Session.CurrentStageId);
            if (StageItr == StageStore.end() || StageItr->second.ChapterId != Session.ChapterId || !StageItr->second.SummonsGroupId)
            {
                player->SendSysMsg("[副本闯关]: 当前关卡没有有效的怪物组配置。");
                StarterGuid = ObjectGuid::Empty;
                return;
            }

            StageTemplate const& Stage = StageItr->second;
            RunState = StageRunState::Starting;
            me->SummonsClear = false;
            Session.ControllerGuid = me->GetGUID();
            Session.InstanceId = me->GetInstanceId();
            me->AtStage = Session.CurrentStageId;

            if (Stage.PositionId && !MoveStageSessionToPosition(Session, *me, Stage.PositionId))
            {
                AbortStage("进入关卡房间失败，请检查坐标配置。", false);
                return;
            }

            uint32 SummonedCount = sAsCreature->SummonCreatureGroup(me, Stage.SummonsGroupId);
            if (!SummonedCount)
            {
                LOG_ERROR("server.worldserver", "副本闯关第 {} 关怪物组 {} 未召唤出任何生物，NPC {}，地图 {}",
                    Session.CurrentStageId, Stage.SummonsGroupId, me->GetEntry(), me->GetMapId());
                AbortStage("本关怪物召唤失败，请检查怪物组配置。", true);
                return;
            }

            Session.State = StageSessionState::Fighting;
            RunState = StageRunState::Fighting;
            WatchdogTimer = StageWatchdogInterval;
            SendStageSessionMessage(Session, Acore::StringFormat("第 {} 关开始，本关怪物数量：{}。", Session.CurrentStageId, SummonedCount));
        }

        void CompleteStage(StageChallengeSession& Session, Position RewardPosition, Unit* killer)
        {
            auto StageItr = StageStore.find(Session.CurrentStageId);
            auto ChapterItr = StageChapterStore.find(Session.ChapterId);
            if (StageItr == StageStore.end() || ChapterItr == StageChapterStore.end())
                return;

            StageTemplate const& Stage = StageItr->second;
            StageChapterTemplate const& Chapter = ChapterItr->second;
            if (killer)
                RewardPosition.Relocate(killer->GetPositionX(), killer->GetPositionY(), killer->GetPositionZ(), killer->GetOrientation());

            if (Stage.RewardGameobjectId && sObjectMgr->GetGameObjectTemplate(Stage.RewardGameobjectId))
                me->SummonGameObject(Stage.RewardGameobjectId, RewardPosition.GetPositionX(), RewardPosition.GetPositionY(), RewardPosition.GetPositionZ(), RewardPosition.GetOrientation(), 0, 0, 0, 0, StagePortalLifetime);

            uint32 NextStageId = 0;
            GetStageForChapter(Session.ChapterId, Stage.ChapterStage + 1, &NextStageId);
            uint32 NextChapterStageId = Chapter.NextChapterId ? GetFirstStageId(Chapter.NextChapterId) : 0;
            bool ChapterComplete = Stage.IsBossStage || Stage.ChapterStage >= 5 || !NextStageId;

            for (uint32 GuidLow : Session.MemberGuids)
            {
                StageChallengeProgress Progress = GetStageProgress(GuidLow);
                bool NewlyCompleted = ChapterComplete
                    ? Progress.HighestCompletedChapter < Session.ChapterId
                    : Progress.HighestUnlockedStage <= Session.CurrentStageId;
                if (NewlyCompleted)
                {
                    if (Player* Member = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(GuidLow)))
                    {
                        if (Stage.RewardTempId)
                            sHelper->DoTheReward(Member, Stage.RewardTempId, "副本闯关关卡奖励");
                        if (ChapterComplete && Chapter.RewardTempId)
                            sHelper->DoTheReward(Member, Chapter.RewardTempId, "副本闯关章节奖励");
                    }
                }

                uint32 UnlockStageId = ChapterComplete ? (NextChapterStageId ? NextChapterStageId : Session.CurrentStageId) : NextStageId;
                SaveStageProgress(GuidLow, UnlockStageId, ChapterComplete ? Session.ChapterId : Progress.HighestCompletedChapter);
            }

            Session.State = ChapterComplete ? StageSessionState::ChapterCompleted : StageSessionState::Cleared;
            uint32 PortalEntry = GetStagePortalEntry(Stage, Chapter);
            if (PortalEntry && sObjectMgr->GetGameObjectTemplate(PortalEntry))
            {
                if (GameObject* Portal = me->SummonGameObject(PortalEntry, RewardPosition.GetPositionX(), RewardPosition.GetPositionY(), RewardPosition.GetPositionZ(), RewardPosition.GetOrientation(), 0, 0, 0, 0, StagePortalLifetime))
                    Session.PortalGuid = Portal->GetGUID();
            }
            else
            {
                LOG_ERROR("server.worldserver", "副本闯关第 {} 关没有有效的传送门物体配置。", Session.CurrentStageId);
                SendStageSessionMessage(Session, "本关传送门配置无效，请联系管理员。");
            }

            if (ChapterComplete)
                SendStageSessionMessage(Session, "本章节首领已击败，章节奖励已按个人首次进度结算。使用传送门进入下一章节或离开。");
            else
                SendStageSessionMessage(Session, "本关已完成，使用传送门进入下一房间；到达后由队长点击关卡 NPC 开始下一关。");
        }

        void AbortStage(std::string const& Message, bool DespawnSummons)
        {
            RunState = StageRunState::Resetting;
            if (DespawnSummons)
                Summons.DespawnAll();
            else
                Summons.clear();
            me->SummonsClear = true;
            RunState = StageRunState::Idle;
            WatchdogTimer = StageWatchdogInterval;
            StarterGuid = ObjectGuid::Empty;

            if (StageChallengeSession* Session = FindStageSession(me))
            {
                Session->State = StageSessionState::Ready;
                SendStageSessionMessage(*Session, Message);
            }
        }

        void ResetStage(bool Wiped)
        {
            RunState = StageRunState::Resetting;
            Summons.DespawnAll();
            me->SummonsClear = true;
            StarterGuid = ObjectGuid::Empty;
            WatchdogTimer = StageWatchdogInterval;
            if (StageChallengeSession* Session = FindStageSession(me))
            {
                if (Session->State == StageSessionState::Fighting)
                    Session->State = StageSessionState::Ready;
                if (Wiped)
                    SendStageSessionMessage(*Session, "队伍已全部倒下，本关已重置，可以重新点击挑战。");
            }
            RunState = StageRunState::Idle;
        }

        ObjectGuid StarterGuid = ObjectGuid::Empty;
        uint32 WatchdogTimer = StageWatchdogInterval;
        StageRunState RunState = StageRunState::Idle;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new StageCreatureAI(creature);
    }

private:
    bool ShowChapterMenu(Player* player, Creature* creature)
    {
        if (StageChapterStore.empty())
        {
            player->SendSysMsg("[副本闯关]: 没有可用的章节配置，请执行数据库升级并配置章节。");
            return false;
        }
        if (!HasStageProgressTable())
        {
            player->SendSysMsg("[副本闯关]: 缺少角色进度表，请执行 characters 数据库升级脚本。");
            return false;
        }

        StageChallengeProgress const& Progress = GetStageProgress(player->GetGUIDLow());
        bool HasChapter = false;
        for (auto const& Pair : StageChapterStore)
        {
            StageChapterTemplate const& Chapter = Pair.second;
            uint32 FirstStageId = GetFirstStageId(Chapter.ChapterId);
            if (!Chapter.Enabled || !FirstStageId || Progress.HighestUnlockedStage < FirstStageId)
                continue;

            std::ostringstream Text;
            Text << "进入章节：" << Chapter.Name << "（第" << FirstStageId << "关起）";
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, Text.str(), StageChapterGossipSender, Chapter.ChapterId);
            HasChapter = true;
        }

        if (!HasChapter)
        {
            player->SendSysMsg("[副本闯关]: 当前没有已解锁的章节。");
            return false;
        }
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    void StartChapter(Player* player, uint32 ChapterId)
    {
        auto ChapterItr = StageChapterStore.find(ChapterId);
        if (ChapterItr == StageChapterStore.end() || !ChapterItr->second.Enabled)
        {
            player->SendSysMsg("[副本闯关]: 章节配置不存在或未启用。");
            return;
        }
        if (FindStageSession(player))
        {
            player->SendSysMsg("[副本闯关]: 你已经在一个闯关队伍中。");
            return;
        }

        uint32 FirstStageId = GetFirstStageId(ChapterId);
        std::vector<uint32> Members;
        if (!FirstStageId || !CollectStageMembers(player, FirstStageId, Members))
            return;
        if (!IsStagePositionValid(ChapterItr->second.EntryPositionId, ChapterItr->second.MapId))
        {
            player->SendSysMsg("[副本闯关]: 章节入口坐标配置无效。");
            return;
        }

        MapEntry const* MapInfo = sMapStore.LookupEntry(ChapterItr->second.MapId);
        if (!MapInfo || !MapInfo->IsDungeon())
        {
            player->SendSysMsg("[副本闯关]: 章节地图必须是副本地图。");
            return;
        }

        StageChallengeSession Session;
        Session.LeaderGuid = player->GetGUIDLow();
        Session.MemberGuids = Members;
        Session.ChapterId = ChapterId;
        Session.CurrentStageId = FirstStageId;
        Session.MapId = ChapterItr->second.MapId;
        StageSessionStore.emplace(Session.LeaderGuid, Session);
        for (uint32 GuidLow : Members)
            StageMemberSessionStore[GuidLow] = Session.LeaderGuid;

        StageChallengeSession* CreatedSession = FindStageSessionByLeader(Session.LeaderGuid);
        if (!CreatedSession || !TeleportStageSessionToChapter(*CreatedSession, ChapterItr->second, nullptr))
        {
            DestroyStageSession(Session.LeaderGuid);
            player->SendSysMsg("[副本闯关]: 创建章节副本失败，请检查入口坐标和关卡配置。");
            return;
        }
        SendStageSessionMessage(*CreatedSession, "章节副本已创建。进入后由队长点击关卡 NPC 开始挑战。");
    }
};

class StageChallengePortal : public GameObjectScript
{
public:
    StageChallengePortal() : GameObjectScript("StageChallengePortal") { }

    bool OnGossipHello(Player* player, GameObject* gameobject) override
    {
        ClearGossipMenuFor(player);
        StageChallengeSession* Session = FindStageSession(player);
        if (!Session || Session->PortalGuid != gameobject->GetGUID() || Session->MapId != gameobject->GetMapId() ||
            (Session->InstanceId && Session->InstanceId != gameobject->GetInstanceId()))
        {
            player->SendSysMsg("[副本闯关]: 这个传送门不属于你的队伍。");
            return true;
        }
        if (!IsStageSessionLeader(player, *Session))
        {
            player->SendSysMsg("[副本闯关]: 只有队长可以使用通关传送门推进队伍。");
            return true;
        }

        if (Session->State == StageSessionState::Cleared)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "进入下一关房间", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
        else if (Session->State == StageSessionState::ChapterCompleted)
        {
            auto ChapterItr = StageChapterStore.find(Session->ChapterId);
            if (ChapterItr != StageChapterStore.end() && ChapterItr->second.NextChapterId && StageChapterStore.count(ChapterItr->second.NextChapterId))
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "进入下一章节", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "离开章节副本", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
        }
        else
        {
            player->SendSysMsg("[副本闯关]: 当前不能使用传送门。");
            return true;
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, gameobject->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, GameObject* gameobject, uint32 /*Sender*/, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        StageChallengeSession* Session = FindStageSession(player);
        if (!Session || !IsStageSessionLeader(player, *Session) || Session->PortalGuid != gameobject->GetGUID())
        {
            CloseGossipMenuFor(player);
            return true;
        }

        uint32 LeaderGuid = Session->LeaderGuid;
        if (Action == GOSSIP_ACTION_INFO_DEF && Session->State == StageSessionState::Cleared)
        {
            auto StageItr = StageStore.find(Session->CurrentStageId);
            uint32 NextStageId = 0;
            StageTemplate const* NextStage = StageItr == StageStore.end() ? nullptr :
                GetStageForChapter(Session->ChapterId, StageItr->second.ChapterStage + 1, &NextStageId);
            if (!NextStage || !NextStageId || !NextStage->PositionId || !MoveStageSessionToPosition(*Session, *gameobject, NextStage->PositionId))
            {
                player->SendSysMsg("[副本闯关]: 下一关房间坐标无效。");
                CloseGossipMenuFor(player);
                return true;
            }

            Session->CurrentStageId = NextStageId;
            Session->PortalGuid = ObjectGuid::Empty;
            Session->State = StageSessionState::Ready;
            if (Creature* Controller = ObjectAccessor::GetCreature(*gameobject, Session->ControllerGuid))
                Controller->AtStage = NextStageId;
            SendStageSessionMessage(*Session, "已进入下一关房间，队长可以点击关卡 NPC 开始挑战。");
            gameobject->Delete();
        }
        else if (Action == GOSSIP_ACTION_INFO_DEF + 1 && Session->State == StageSessionState::ChapterCompleted)
        {
            auto ChapterItr = StageChapterStore.find(Session->ChapterId);
            auto NextChapterItr = ChapterItr == StageChapterStore.end() ? StageChapterStore.end() :
                StageChapterStore.find(ChapterItr->second.NextChapterId);
            if (NextChapterItr == StageChapterStore.end() || !NextChapterItr->second.Enabled ||
                !TeleportStageSessionToChapter(*Session, NextChapterItr->second, gameobject))
            {
                player->SendSysMsg("[副本闯关]: 下一章节配置无效。");
                CloseGossipMenuFor(player);
                return true;
            }

            SendStageSessionMessage(*Session, "已进入下一章节，找到关卡 NPC 后由队长开始第一关。");
            gameobject->Delete();
        }
        else if (Action == GOSSIP_ACTION_INFO_DEF + 2 && Session->State == StageSessionState::ChapterCompleted)
        {
            auto ChapterItr = StageChapterStore.find(Session->ChapterId);
            auto PositionItr = ChapterItr == StageChapterStore.end() ? PosMap.end() : PosMap.find(ChapterItr->second.ExitPositionId);
            if (PositionItr == PosMap.end())
            {
                player->SendSysMsg("[副本闯关]: 章节出口坐标无效。");
                CloseGossipMenuFor(player);
                return true;
            }

            std::vector<uint32> Members = Session->MemberGuids;
            PosTemplate Position = PositionItr->second;
            DestroyStageSession(LeaderGuid);
            for (uint32 GuidLow : Members)
            {
                if (Player* Member = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(GuidLow)))
                    Member->TeleportTo(Position.MapId, Position.X, Position.Y, Position.Z, Position.O);
            }
            gameobject->Delete();
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

class StageOrNtPlayerScript : public PlayerScript
{
public:
    StageOrNtPlayerScript() : PlayerScript("StageOrNtPlayerScript") {}

    void OnLogout(Player* player) override
    {
        StageChallengeSession* Session = FindStageSession(player);
        if (!Session || Session->LeaderGuid != player->GetGUIDLow())
            return;

        uint32 LeaderGuid = Session->LeaderGuid;
        SendStageSessionMessage(*Session, "队长已经离线，本次运行中的章节进度已结束；已解锁关卡不会丢失。");
        DestroyStageSession(LeaderGuid);
    }

    void OnMapChanged(Player* player) override
    {
        if (Map* TarMap = player->GetMap())
        {
            if (!TarMap->IsDungeon())
                return;

            uint32 InCount = TarMap->GetPlayersCountExceptGMs();
            uint32 PCount = uint32(sHelper->GetUInt(1203));
            uint32 NowMapId = player->GetMapId();
            if (NowMapId == uint32(sHelper->GetUInt(1202)) && InCount > PCount)
            {
                player->RepopAtGraveyard();
                player->SendSysMsg(sHelper->GetGts(6731), PCount);
                return;
            }

            if (NowMapId == uint32(sHelper->GetUInt(2601)) && InCount > 1)
            {
                player->RepopAtGraveyard();
                player->SendSysMsg(sHelper->GetGts(6732));
                return;
            }
        }
    }
};

namespace
{
enum NtRankAIData : uint32
{
    DATA_NT_RANK_STAGE = 1
};

enum NtRankAIGuid : int32
{
    GUID_NT_RANK_CHALLENGER = 1
};

enum NtRankAIAction : int32
{
    ACTION_NT_RANK_START = 1
};

enum NtRankAIEvent : uint32
{
    EVENT_NT_RANK_NEXT_STAGE = 1
};

enum class NtRankRunState : uint8
{
    Idle,
    Fighting,
    Transition
};

constexpr uint32 NtRankNextStageDelay = 3 * IN_MILLISECONDS;
constexpr uint32 NtRankWatchdogInterval = 2 * IN_MILLISECONDS;
}

class NtRankCreature : public CreatureScript
{
public:
    NtRankCreature() : CreatureScript("NtRankCreature") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        Map* TarMap = creature->GetMap();
        if (!TarMap->IsDungeon())
        {
            player->SendSysMsg(sHelper->GetGts(6733));
            return false;
        }

        uint32 NowNtStage = player->GetTempAtUInt32(AT2002);
        if (creature->NtSumClear)
        {
            auto NtItr = NtRankStore.find(NowNtStage);
            if (NtItr == NtRankStore.end())
            {
                player->SendSysMsg(sHelper->GetGts(6734));
                return false;
            }

            if (NtItr->second.SGroupId == 0)
            {
                player->SendSysMsg(sHelper->GetGts(6740));
                return false;
            }

            std::ostringstream Oss;
            Oss << sHelper->GetGts(6735);

            AddGossipItemFor(player, GOSSIP_ICON_TALK, Oss.str(), NowNtStage, GOSSIP_ACTION_INFO_DEF + 1);
            AddGossipItemFor(player, GOSSIP_ICON_TALK, sAsCreature->GetNtDes(NowNtStage), NowNtStage, GOSSIP_ACTION_INFO_DEF);
        }
        else
        {
            player->SendSysMsg(sHelper->GetGts(6736));
            return false;
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        if (Action == GOSSIP_ACTION_INFO_DEF + 1)
        {
            OnGossipHello(player, creature);
            return true;
        }

        if (!creature->NtSumClear)
        {
            player->SendSysMsg(sHelper->GetGts(6736));
            CloseGossipMenuFor(player);
            return true;
        }

        uint32 NowNtStage = player->GetTempAtUInt32(AT2002);
        uint32 SumId = sAsCreature->GetNtSumId(NowNtStage);
        if (SumId == 0)
        {
            player->SendSysMsg(sHelper->GetGts(6740));
            CloseGossipMenuFor(player);
            return true;
        }

        creature->AI()->SetGUID(player->GetGUID(), GUID_NT_RANK_CHALLENGER);
        creature->AI()->SetData(DATA_NT_RANK_STAGE, NowNtStage);
        creature->AI()->DoAction(ACTION_NT_RANK_START);
        CloseGossipMenuFor(player);
        return true;
    }

    struct NtRankCreatureAI : public ScriptedAI
    {
        NtRankCreatureAI(Creature* creature) : ScriptedAI(creature), Summons(me) { }

        SummonList Summons;

        void SetGUID(ObjectGuid const& Guid, int32 Id) override
        {
            if (Id == GUID_NT_RANK_CHALLENGER)
                ChallengerGuid = Guid;
        }

        void SetData(uint32 Type, uint32 Data) override
        {
            if (Type == DATA_NT_RANK_STAGE)
                ActiveStage = Data;
        }

        void DoAction(int32 Action) override
        {
            if (Action != ACTION_NT_RANK_START || RunState != NtRankRunState::Idle)
                return;

            Player* player = GetChallenger();
            if (!IsChallengerInThisInstance(player) || !player->IsAlive())
            {
                ResetChallenge();
                return;
            }

            StartStage(player);
        }

        void JustSummoned(Creature* summon) override
        {
            summon->GetMotionMaster()->MoveRandom(8.0f);
            Summons.Summon(summon);

            sHelper->DoZoneInCombatForPlayer(summon);
        }

        void Reset() override
        {
            ResetChallenge();
        }

        void SummonedCreatureDespawn(Creature* Summoned) override
        {
            uint32 PreviousCount = uint32(Summons.size());
            Summons.Despawn(Summoned);

            if (RunState == NtRankRunState::Fighting && Summons.size() < PreviousCount)
                AbortChallenge(GetChallenger(), "怪物异常消失，本轮挑战已重置，请重新点击挑战。");
        }

        void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
        {
            Summons.Despawn(summon);

            if (RunState != NtRankRunState::Fighting)
                return;

            Player* player = GetChallenger();
            if (!IsChallengerInThisInstance(player))
            {
                AbortChallenge(nullptr, nullptr);
                return;
            }

            if (!Summons.empty())
            {
                player->SendSysMsg(sHelper->GetGts(6739), uint32(Summons.size()));
                return;
            }

            CompleteStage(player);
        }

        void UpdateAI(uint32 Diff) override
        {
            Events.Update(Diff);
            while (uint32 EventId = Events.ExecuteEvent())
            {
                if (EventId == EVENT_NT_RANK_NEXT_STAGE)
                {
                    Player* player = GetChallenger();
                    if (!IsChallengerInThisInstance(player) || !player->IsAlive())
                    {
                        FinishChallenge(player, "自动挑战已停止，复活或返回副本后可继续挑战。");
                        continue;
                    }

                    StartStage(player);
                }
            }

            if (RunState != NtRankRunState::Fighting)
                return;

            if (WatchdogTimer > Diff)
            {
                WatchdogTimer -= Diff;
                return;
            }

            WatchdogTimer = NtRankWatchdogInterval;

            Player* player = GetChallenger();
            if (!IsChallengerInThisInstance(player) || !player->IsAlive())
            {
                AbortChallenge(player, "挑战者已死亡或离开副本，本轮挑战已重置。");
                return;
            }

            uint32 PreviousCount = uint32(Summons.size());
            Summons.RemoveNotExisting();
            if (Summons.size() < PreviousCount)
                AbortChallenge(player, "检测到怪物状态异常，本轮挑战已重置，请重新点击挑战。");
        }

    private:
        Player* GetChallenger() const
        {
            if (ChallengerGuid == ObjectGuid::Empty)
                return nullptr;

            return ObjectAccessor::GetPlayer(*me, ChallengerGuid);
        }

        bool IsChallengerInThisInstance(Player* player) const
        {
            return player && player->IsInWorld() && player->GetMap() == me->GetMap();
        }

        void StartStage(Player* player)
        {
            auto NtItr = NtRankStore.find(ActiveStage);
            if (NtItr == NtRankStore.end() || NtItr->second.SGroupId == 0)
            {
                FinishChallenge(player, "已完成全部天梯挑战。");
                return;
            }

            RunState = NtRankRunState::Fighting;
            me->NtSumClear = false;
            WatchdogTimer = NtRankWatchdogInterval;

            uint32 SummonedCount = sAsCreature->NtSumCreatureGroup(me, NtItr->second.SGroupId);
            if (SummonedCount == 0)
            {
                LOG_ERROR("server.worldserver", "副本天梯第 {} 层怪物组 {} 未召唤出任何生物，NPC {}，地图 {}",
                    ActiveStage, NtItr->second.SGroupId, me->GetEntry(), me->GetMapId());
                AbortChallenge(player, "本层怪物召唤失败，本轮挑战已重置，请联系管理员检查天梯配置。");
                return;
            }

            player->SendSysMsg("[副本天梯]: 第 %u 层挑战开始，本层怪物数量：%u。", ActiveStage, SummonedCount);
        }

        void CompleteStage(Player* player)
        {
            RunState = NtRankRunState::Transition;
            me->NtSumClear = false;

            uint32 CompletedStage = ActiveStage;
            uint32 NextStage = CompletedStage + 1;
            player->UpdateTempAt(AT2002, NextStage);
            sAsCreature->UpdateAsPlayerNtRank(player->GetGUID(), NextStage);

            auto NtItr = NtRankStore.find(CompletedStage);
            if (NtItr != NtRankStore.end() && NtItr->second.RewardTempId > 0)
                sHelper->DoTheReward(player, NtItr->second.RewardTempId, "副本天梯通关奖励");

            std::ostringstream Oss;
            Oss << sHelper->GetGts(6737) << CompletedStage << sHelper->GetGts(6738);
            player->SendSysMsg(Oss.str().c_str());

            auto NextItr = NtRankStore.find(NextStage);
            if (NextItr == NtRankStore.end())
            {
                FinishChallenge(player, "已完成全部天梯挑战。");
                return;
            }

            if (NextItr->second.SGroupId == 0)
            {
                LOG_ERROR("server.worldserver", "副本天梯第 {} 层未配置有效的召唤生物组", NextStage);
                FinishChallenge(player, "下一层配置无效，自动挑战已停止。");
                return;
            }

            if (!player->IsAlive())
            {
                FinishChallenge(player, "本层已通关，自动挑战已暂停，复活后可继续挑战。");
                return;
            }

            ActiveStage = NextStage;
            player->SendSysMsg("[副本天梯]: %u 秒后自动进入第 %u 层。", NtRankNextStageDelay / IN_MILLISECONDS, ActiveStage);
            Events.ScheduleEvent(EVENT_NT_RANK_NEXT_STAGE, Milliseconds(NtRankNextStageDelay));
        }

        void AbortChallenge(Player* player, char const* Message)
        {
            RunState = NtRankRunState::Idle;
            me->NtSumClear = true;
            Events.Reset();
            Summons.DespawnAll();

            if (player && Message)
                player->SendSysMsg("[副本天梯]: %s", Message);

            ChallengerGuid = ObjectGuid::Empty;
            ActiveStage = 0;
            WatchdogTimer = NtRankWatchdogInterval;
        }

        void FinishChallenge(Player* player, char const* Message)
        {
            RunState = NtRankRunState::Idle;
            me->NtSumClear = true;
            Events.Reset();

            if (player && Message)
                player->SendSysMsg("[副本天梯]: %s", Message);

            ChallengerGuid = ObjectGuid::Empty;
            ActiveStage = 0;
            WatchdogTimer = NtRankWatchdogInterval;
        }

        void ResetChallenge()
        {
            RunState = NtRankRunState::Idle;
            me->NtSumClear = true;
            Events.Reset();
            Summons.DespawnAll();
            ChallengerGuid = ObjectGuid::Empty;
            ActiveStage = 0;
            WatchdogTimer = NtRankWatchdogInterval;
        }

        EventMap Events;
        ObjectGuid ChallengerGuid = ObjectGuid::Empty;
        uint32 ActiveStage = 0;
        uint32 WatchdogTimer = NtRankWatchdogInterval;
        NtRankRunState RunState = NtRankRunState::Idle;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new NtRankCreatureAI(creature);
    }
};

class KillCreForPlayerScript : public PlayerScript
{
public:
    KillCreForPlayerScript() : PlayerScript("KillCreForPlayerScript") {  }

    void OnCreatureKill(Player* player, Creature* Killed) override
    {
        auto Itr = OKillCreStore.find(Killed->GetEntry());
        if (Itr == OKillCreStore.end())
            return;

        if (!Itr->second.NCmd.empty())
        {
            Tokenizer Data(Itr->second.NCmd, '#');
            for (auto const& Cmd : Data)
                player->DoCommands(Cmd);
        }

        if (!Itr->second.RCmd.empty())
        {
            Tokenizer Data(Itr->second.RCmd, '#');
            if (!Data.IsEmpty())
                player->DoCommands(Data[urand(0, Data.size() - 1)]);
        }

        if (!Itr->second.CreOrOb.empty())
        {
            Tokenizer AtData(Itr->second.CreOrOb, '|');
            if (!AtData.IsEmpty())
            {
                std::string Groups = std::string(AtData[urand(0, AtData.size() - 1)]);
                Tokenizer Data(Groups, '#');
                if (!Data.IsEmpty())
                {
                    int32 CreOrOb = atoi(Data[0]);
                    uint32 Time = uint32(atoi(Data[1]));
                    float Odds = atof(Data[2]);
                    if (CreOrOb > 0)
                    {
                        if (roll_chance_f(Odds))
                            player->SummonCreature(CreOrOb, player->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, Time * IN_MILLISECONDS);
                    }
                    else if (CreOrOb < 0)
                    {
                        if (roll_chance_f(Odds))
                            player->SummonGameObject(-CreOrOb, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation(), 0, 0, 0, 0, Time);
                    }
                }
            }
        }

        if (roll_chance_f(Itr->second.Chance))
            sHelper->DoTheReward(player, Itr->second.RTempId, "击杀怪物奖励");

        if (Itr->second.RmGroupId && Itr->second.RmCount && Itr->second.RmChance > 0.0f)
        {
            if (roll_chance_f(Itr->second.RmChance))
            {
                struct AtSumTemp { uint32 RmCreId; uint32 TimeFn; uint32 RmPosId;};
                std::vector<AtSumTemp> DoVect;
                for (auto Ite = RmSumStore.begin(); Ite != RmSumStore.end(); ++Ite)
                {
                    if (Ite->GroupId == Itr->second.RmGroupId)
                    {
                        AtSumTemp St;
                        St.RmCreId = Ite->RmCreId;
                        St.TimeFn = Ite->TimeFn;
                        St.RmPosId = Ite->RmPosId;
                        DoVect.push_back(St);
                    }
                }

                Acore::Containers::AtRandomResize(DoVect, Itr->second.RmCount);
                for (auto Its = DoVect.begin(); Its != DoVect.end(); ++Its)
                {
                    uint32 MapId = 0;
                    float X = 0;
                    float Y = 0;
                    float Z = 0;
                    float O = 0;
                    sAsCreature->GetSumTelePos(Its->RmPosId, MapId, X, Y, Z, O);
                    Position Mpos;
                    Mpos.Relocate(X, Y, Z, O);
                    player->SummonCreature(Its->RmCreId, Mpos, TEMPSUMMON_TIMED_DESPAWN, Its->TimeFn * IN_MILLISECONDS);
                }
            }
        }
    }

    bool CanRepopAtGraveyard(Player* player) override
    {
        if (IsXsPlayerbot(player))
            return true;

        if (player->IsAlive())
            return true;

        int32 MapId = int32(player->GetMapId());
        int32 AreaId = -int32(player->GetAreaId());
        uint32 FactionId = player->GetTeamId() == TEAM_ALLIANCE ? 1 : 2;

        auto It = DieMapGravStore.find(AreaId);
        if (It != DieMapGravStore.end())
        {
            for (int i = 0; i < DieMapGravStore.count(AreaId); i++, It++)
            {
                if (It->second.Faction == 0 || It->second.Faction == FactionId)
                {
                    uint32 PosId = urand(It->second.PosMin, It->second.PosMax);
                    player->ResurrectPlayer(It->second.HMPct);
                    player->SpawnCorpseBones();
                    player->TeleToPos(PosId);
                    return false;
                }
            }
        }

        auto Itr = DieMapGravStore.find(MapId);
        if (Itr != DieMapGravStore.end())
        {
            for (int i = 0; i < DieMapGravStore.count(MapId); i++, Itr++)
            {
                if (Itr->second.Faction == 0 || Itr->second.Faction == FactionId)
                {
                    uint32 PosId = urand(Itr->second.PosMin, Itr->second.PosMax);
                    player->ResurrectPlayer(Itr->second.HMPct);
                    player->SpawnCorpseBones();
                    player->TeleToPos(PosId);
                    return false;
                }
            }
        }

        return true;
    }

    void OnMapChanged(Player* player) override
    {
        if (sHelper->GetBoolean(9994))
            LOG_WARN("server", ">> 玩家进入地图调试输出 PlayerGuidLow = {} MapId = {}", player->GetGUIDLow(), player->GetMapId());

        uint32 MapId = player->GetMapId();
        if (sAsCreature->IsChallengeMap(MapId))
        {
            uint32 mapRank = player->GetMapRank();
            // LOG_INFO("server.worldserver", "OnMapChanged: 玩家进入挑战地图 {}，MapRank={}", MapId, mapRank);

            // 只有通过GM命令进入挑战模式时才显示提示（AT8001标记为1）
            bool showChallengeMsg = player->GetTempAt(AT8001) == 1;
            sAsCreature->DoApplyChallenge(player, MapId, mapRank, showChallengeMsg);
            player->SetMapRank(0);

            // 记录是否以挑战模式进入（用于出本时判断是否重置）
            // AT8002 = 1 表示本次是以挑战模式进入的
            // 修复：无论通过GM命令还是撞门进入，只要mapRank > 0就标记为挑战模式
            if (mapRank > 0)
            {
                player->UpdateTempAt(AT8002, 1);
                // LOG_INFO("server.worldserver", "OnMapChanged: 标记为挑战模式进入，AT8002=1，mapRank={}", mapRank);
            }

            // 清除GM命令标记
            if (showChallengeMsg)
                player->UpdateTempAt(AT8001, 0);
        }
        else if (Map* map = player->GetMap(); map && map->IsDungeon() && map->GetMapRank() > 0)
        {
            // 秘境专用副本不进入旧挑战存档流程；实例已保存等级，清理玩家临时传送值。
            player->SetMapRank(0);
        }
    }
};

class ChallengeAutoResetMapScript : public AllMapScript
{
public:
    ChallengeAutoResetMapScript() : AllMapScript("ChallengeAutoResetMapScript") {}

    void OnPlayerLeaveAll(Map* map, Player* player) override
    {
        // LOG_INFO("server.worldserver", "OnPlayerLeaveAll: 玩家 {} 离开地图 {}", player->GetName(), map->GetId());

        // 只处理副本地图
        if (!map->IsDungeon())
        {
            // LOG_INFO("server.worldserver", "OnPlayerLeaveAll: 不是副本地图，跳过");
            return;
        }

        uint32 mapId = map->GetId();
        // LOG_INFO("server.worldserver", "OnPlayerLeaveAll: 副本地图 mapId={}", mapId);

        // 检查是否是挑战副本地图
        if (!sAsCreature->IsChallengeMap(mapId))
        {
            // LOG_INFO("server.worldserver", "OnPlayerLeaveAll: 不是挑战副本地图，跳过");
            return;
        }

        // 检查是否以挑战模式进入（AT8002标记为1）
        bool isChallengeMode = player->GetTempAt(AT8002) == 1;
        // LOG_INFO("server.worldserver", "OnPlayerLeaveAll: 是挑战副本地图，挑战模式标记 AT8002={}", player->GetTempAt(AT8002));

        if (!isChallengeMode)
        {
            // LOG_INFO("server.worldserver", "OnPlayerLeaveAll: 不是以挑战模式进入，跳过重置");
            return;
        }

        // LOG_INFO("server.worldserver", "OnPlayerLeaveAll: 是以挑战模式进入，检查玩家数据");

        // 遍历玩家在该地图的所有挑战数据（Bind=true表示已通关）
        for (auto& data : ChallengeDataStore)
        {
            if (data.MapId == mapId && data.GuidLow == player->GetGUIDLow() && data.Bind)
            {
                uint32 playerRank = data.Level;
                // LOG_INFO("server.worldserver", "OnPlayerLeaveAll: 找到玩家挑战数据，Rank={}", playerRank);

                // 检查该地图难度是否配置了自动重置
                if (MazChallenge const* challenge = sAsCreature->GetChallengeConfig(mapId, playerRank))
                {
                    // LOG_INFO("server.worldserver", "OnPlayerLeaveAll: 找到配置 AutoReset={}", challenge->AutoReset);
                    if (challenge->AutoReset)
                    {
                        // 自动重置该挑战等级
                        sAsCreature->ResetChallengeLevel(player, mapId, playerRank);
                        // LOG_INFO("server.worldserver", "玩家 {} 离开挑战副本 {} 难度 {}，已自动重置",
                        //     player->GetName(), mapId, playerRank);
                    }
                }
            }
        }

        // 清除挑战模式进入标记
        player->UpdateTempAt(AT8002, 0);
        // LOG_INFO("server.worldserver", "OnPlayerLeaveAll: 已清除挑战模式标记 AT8002=0");

        // LOG_INFO("server.worldserver", "OnPlayerLeaveAll: 处理完成");
    }
};

void AddSC_AsCreatureScripts()
{
    new StageCreature();
    new StageChallengePortal();
    new StageOrNtPlayerScript();
    new NtRankCreature();
    new KillCreForPlayerScript();
    new ChallengeAutoResetMapScript();
}
