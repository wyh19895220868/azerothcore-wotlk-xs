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
#include "EventMgr.h"
#include <Maps\MapMgr.h>
#pragma execution_character_set("utf-8")

void Player::UpdateEventDamage(Unit* target, AsInt64 const& damage)
{
    if (damage <= 0 || !target || target->GetGUID() == GetGUID() || (target->GetTypeId() == TYPEID_UNIT && target->ToCreature()->IsCivilian()))
        return;

    _EventData.Damage += damage;
}


void Player::UpdateEventHeal(Unit* target, AsInt64 const& heal)
{
    if (heal <= 0 || !target || target->GetTypeId() != TYPEID_PLAYER || target->GetGUID() == GetGUID())
        return;

    _EventData.Heal += heal;
}

void Player::UpdateEventKills(Unit* target)
{
    if (!target || target->GetGUID() == GetGUID())
        return;

    _EventData.Kills++;
    sEvent->UpdateKillInfo(this, true, _EventData.EventId);
}

void Player::UpdateEventKilleds(Unit* target)
{
    if (!target || target->GetGUID() == GetGUID())
        return;

    _EventData.Killeds++;
    sEvent->UpdateKillInfo(this, false, _EventData.EventId);
}

void Player::InitEventData(uint32 EventId, std::string FactionName, uint32 FactionId)
{
    _EventData.EventId = EventId;
    _EventData.FactionName = FactionName;
    _EventData.FactionId = FactionId;
    _EventData.Damage = 0;
    _EventData.Heal = 0;
    _EventData.Kills = 0;
    _EventData.Killeds = 0;

    if (FactionId != 0)
        SetFaction(FactionId);
}

void Player::EventRest()
{
    _EventData.EventId = 0;
    _EventData.FactionName = "";
    _EventData.FactionId = 0;
    _EventData.Damage = 0;
    _EventData.Heal = 0;
    _EventData.Kills = 0;
    _EventData.Killeds = 0;

    AsSetFactionForRace(GetRace());
}

void Player::FixedBGJoin(BattlegroundTypeId BTypeId)
{
    WorldPacket Packet;
    Packet << GetGUID();
    Packet << uint32(BTypeId);
    Packet << uint32(0);
    Packet << uint8(0);
    GetSession()->HandleBattlemasterJoinOpcode(Packet);
}

bool CustomEventMgr::CanRew(Player* pl, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return false;

    EventDataTemplate playerEventData = pl->GetEventData();

    if (playerEventData.Damage >= eventData->DamageForRew ||
        playerEventData.Heal >= eventData->HealForRew ||
        playerEventData.Kills >= eventData->KillsForRew ||
        playerEventData.Killeds >= eventData->KilledForRew)
        return true;

    return false;
}

bool Player::InEvent()
{
    return (_EventData.FactionId != 0);
}

CustomEventMgr::CustomEventMgr()
{
}

// 获取活动数据
EventData* CustomEventMgr::GetEventData(uint32 eventId)
{
    auto itr = _eventDataMap.find(eventId);
    if (itr != _eventDataMap.end())
        return &itr->second;
    return nullptr;
}

void CustomEventMgr::Reset(uint32 eventId)
{
    if (eventId == 0)
    {
        // 重置所有活动
        for (auto itr = _eventDataMap.begin(); itr != _eventDataMap.end(); itr++)
        {
            Reset(itr->first);
        }
        _eventDataMap.clear();
        return;
    }

    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    // 移除所有灵魂医者
    for (auto itr = eventData->HealerMap.begin(); itr != eventData->HealerMap.end(); itr++)
        if (Creature* healer = eventData->Map->GetCreature(itr->second))
            healer->RemoveFromWorld();
    eventData->HealerMap.clear();

    // 重置活动数据
    eventData->EventId = 0;
    eventData->EventName = "";
    eventData->EventType = C_ET_NONE;
    eventData->GroupType = C_GT_RANDOM_PLAYER;
    eventData->RejoinType = C_RT_REDIS_GROUP;
    eventData->NoticeText = "";
    eventData->GossipText = "";
    eventData->MapId = -1;
    eventData->ZoneId = -1;
    eventData->Map = nullptr;
    eventData->Phase = 1;
    eventData->RandomAH = urand(1, 100) > 50 ? false : true;
    eventData->WinRewId = 0;
    eventData->LosRewId = 0;
    eventData->DamageForRew = 0;
    eventData->HealForRew = 0;
    eventData->KillsForRew = 0;
    eventData->KilledForRew = 0;
    eventData->GroupMap.clear();
    eventData->FactionVec.clear();
    eventData->PlayersMap.clear();
    eventData->KillInfoMap.clear();
    eventData->FactionNameMap.clear();
    eventData->StartPosMap.clear();
    eventData->StopPosMap.clear();
    eventData->StopVec.clear();
    eventData->AltVec.clear();
    eventData->AltFactionVec.clear();
    eventData->AltedVec.clear();
    eventData->PhaseFactionVec.clear();
    eventData->PhaseVec.clear();
    eventData->UpdateSafeTimer = 0;
    eventData->UpdateReviveTimer = 0;
    eventData->DelayStartCall = false;
    eventData->DelayStopCall = false;
    eventData->UpdateDelayNotice = 0;
    eventData->WinFaction = 0;
    eventData->WorldStateVec.clear();
    eventData->WorldStateFactionVec.clear();
    eventData->SpawnVec.clear();
    eventData->UpdateSpawnTimer = 0;
}

void CustomEventMgr::InitParams(uint32 eventId)
{
    Reset(eventId);

    if (eventId <= MINEVENTSO)
        return;

    // 创建或获取活动数据
    EventData& eventData = _eventDataMap[eventId];
    eventData.EventId = eventId;

    QueryResult result;

    if (result = WorldDatabase.Query("SELECT 事件名称, 弹窗提示文本, 菜单提示文本, 归属地图ID, 区域ID, 事件类型, 组队类型, 重新加入类型, "
                                                            "胜利奖励模板ID, 失败奖励模板ID, 获得奖励最小伤害值, 获得奖励最小治疗值, 获得奖励最小击杀数, 获得奖励最小被杀数 From _事件 WHERE 事件ID = {}", eventId))
    {
        Field* fields = result->Fetch();
        eventData.EventName = fields[0].Get<std::string>();
        eventData.NoticeText = fields[1].Get<std::string>();
        eventData.GossipText = fields[2].Get<std::string>();

        std::string_view TypeStr = fields[5].Get<std::string_view>();
        const char* type = TypeStr.data();

        if (type && *type != '\0')
        {
            if (strcmp(type, "战场 - 战歌") == 0)
                eventData.EventType = C_ET_WS;
            else if (strcmp(type, "战场 - 阿拉希") == 0)
                eventData.EventType = C_ET_AB;
            else if (strcmp(type, "战场 - 风暴之眼") == 0)
                eventData.EventType = C_ET_EY;
            else if (strcmp(type, "战场 - 远古海滩") == 0)
                eventData.EventType = C_ET_SA;
            else if (strcmp(type, "战场 - 奥山") == 0)
                eventData.EventType = C_ET_AV;
            else if (strcmp(type, "战场 - 征服之岛") == 0)
                eventData.EventType = C_ET_IC;
            else if (strcmp(type, "仅传送") == 0)
                eventData.EventType = C_ET_TELE;
            else if (strcmp(type, "对线") == 0)
                eventData.EventType = C_ET_LINE;
            else if (strcmp(type, "公会") == 0)
                eventData.EventType = C_ET_GUILD;
        }

        if (eventData.EventType < C_ET_TELE)
            return;

    eventData.MapId = fields[3].Get<int32>();
    eventData.ZoneId = fields[4].Get<int32>();

    eventData.Map = sMapMgr->FindMap(eventData.MapId, 0);
    int32 map = GetMapIdByZone(eventData.ZoneId);

    if (!eventData.Map || map < 0)
    {
        LOG_ERROR("server.loading", "功能表 >> [_事件] 配置错误 地图和区域不配置正确 此事件失效");
        return;
    }

    TypeStr = fields[6].Get<std::string_view>();
    type = TypeStr.data();

    if (type && *type !='\0')
    {
        if (strcmp(type, "对线 - 全部阵营1") == 0)
            eventData.GroupType = C_GT_ONE_ALL;
        else if (strcmp(type, "对线 - 联盟阵营1BL阵营2") == 0)
            eventData.GroupType = C_GT_ONE_A;
        else if (strcmp(type, "对线 - 联盟阵营2BL阵营1") == 0)
            eventData.GroupType = C_GT_ONE_H;
        else if (strcmp(type, "对线 - 联盟部落随机") == 0)
            eventData.GroupType = C_GT_RANDOM_A_H;
        else if (strcmp(type, "对线 - 玩家随机") == 0)
            eventData.GroupType = C_GT_RANDOM_PLAYER;
    }

    TypeStr = fields[7].Get<std::string_view>();
    type = TypeStr.data();

    if (type && *type != '\0')
    {
        if (strcmp(type, "重新加入 - 禁止") == 0)
            eventData.RejoinType = C_RT_BANED;
        else if (strcmp(type, "重新加入- 之前队伍") == 0)
            eventData.RejoinType = C_RT_KEEP_GROUP;
        else if (strcmp(type, "重新加入 - 系统分配") == 0)
            eventData.RejoinType = C_RT_REDIS_GROUP;
    }

    eventData.WinRewId		= fields[8].Get<uint32>();
    eventData.LosRewId		= fields[9].Get<uint32>();
    eventData.DamageForRew	= AsInt64FromString(fields[10].Get<std::string>());
    eventData.HealForRew		= AsInt64FromString(fields[11].Get<std::string>());
    eventData.KillsForRew	= fields[12].Get<uint32>();
    eventData.KilledForRew	= fields[13].Get<uint32>();
    }
    else
        return;

    if (result = WorldDatabase.Query("SELECT 队伍ID, 队伍名称, 事件开始传送坐标ID, 事件结束传送坐标ID From _事件_队伍配置 WHERE 事件ID = {}", eventId))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 faction		= fields[0].Get<uint32>();
            eventData.FactionVec.push_back(faction);
            eventData.FactionNameMap[faction]	= fields[1].Get<std::string>();
            eventData.StartPosMap[faction]		= fields[2].Get<uint32>();
            eventData.StopPosMap[faction]		= fields[3].Get<uint32>();
        } while (result->NextRow());
    }

    if (result = WorldDatabase.Query("SELECT 结束类型, 队伍ID, 参数, 变量, 权重值 From _事件_结束条件配置 WHERE 事件ID = {}", eventId))
    {
        do
        {
            Field* fields = result->Fetch();
            EventStopTemplate Temp;

            std::string_view TypeStr = fields[0].Get<std::string_view>();
            const char* type = TypeStr.data();
            if (strcmp(type, "击杀生物 - guid") == 0)
                Temp.StopType = TYPEID_UNIT;
            else if (strcmp(type, "摧毁物体 - guid") == 0)
                Temp.StopType = TYPEID_GAMEOBJECT;
            else if (strcmp(type, "击杀玩家 - 数量") == 0)
                Temp.StopType = TYPEID_PLAYER;
            else if (strcmp(type, "击杀生物 - entry") == 0)
                Temp.StopType = TYPEID_UNIT_ENTRY;

            Temp.Faction	= fields[1].Get<uint32>();
            Temp.Param		= fields[2].Get<uint32>();
            Temp.AtCount = fields[3].Get<uint32>();
            Temp.Weight		= fields[4].Get<uint32>();
            eventData.StopVec.push_back(Temp);
        } while (result->NextRow());
    }

    if (result = WorldDatabase.Query("SELECT 生物或物体GUID, 生物或物体激活后FLAG, 激活时提示文本,""需击杀生物或摧毁物体ID1, 数量1, 需击杀生物或摧毁物体ID2, 数量2, 需击杀生物或摧毁物体ID3, 数量3, 需击杀生物或摧毁物体ID4, 数量4, 需击杀生物或摧毁物体ID5, 数量5 ""From _事件_生物或物体激活条件配置 WHERE 事件ID = {}", eventId))
    {
        do
        {
            Field* fields = result->Fetch();
            EventAltTemplate Temp;

            Temp.GUID		= fields[0].Get<int32>();
            Temp.AltFlag	= fields[1].Get<uint32>();
            Temp.NoticeText = fields[2].Get<std::string>();
            for (size_t i = 0; i < ALT_GUID_MAX; i++)
            {
                Temp.TargetEntry[i] = fields[3 + i * 2].Get<int64>();
                Temp.TargetCount[i] = fields[4 + i * 2].Get<uint32>();
                Temp.TargetKillCount[i] = 0;
            }

            eventData.AltVec.push_back(Temp);
        } while (result->NextRow());
    }

    if (result = WorldDatabase.Query("SELECT 生物或物体GUID, 队伍ID, 击杀玩家数, 玩家被杀数 From _事件_生物或物体激活条件队伍配置 WHERE 事件ID = {}", eventId))
    {
        do
        {
            Field* fields = result->Fetch();
            EventAltFactionTemplate Temp;
            Temp.GUID			= fields[0].Get<int32>();
            Temp.Faction		= fields[1].Get<uint32>();
            Temp.Kills			= fields[2].Get<uint32>();
            Temp.Killeds		= fields[3].Get<uint32>();
            eventData.AltFactionVec.push_back(Temp);
        } while (result->NextRow());
    }

    if (result = WorldDatabase.Query("SELECT 阶段ID, 阶段结束需击杀生物或摧毁物体GUID From _事件_阶段生物或物体配置 WHERE 事件ID = {}", eventId))
    {
        do
        {
            Field* fields = result->Fetch();
            EventPhaseTemplate Temp;
            Temp.Phase		= fields[0].Get<uint8>();
            Temp.StopGUID	= fields[1].Get<int32>();
            eventData.PhaseVec.push_back(Temp);
        } while (result->NextRow());
    }

    if (result = WorldDatabase.Query("SELECT 阶段ID,队伍ID, 墓地坐标ID, 墓地保护区范围, 墓地灵魂医者ID, 阶段结束需击杀玩家数,""阶段结束需玩家被杀数, 阶段结束集体传送坐标ID, 进入墓地保护区提示文本, 阶段结束提示文本 ""From _事件_阶段队伍配置 WHERE 事件ID = {}", eventId))
    {
        do
        {
            Field* fields = result->Fetch();
            EventPhaseFactionTemplate Temp;
            Temp.Phase				= fields[0].Get<uint32>();
            Temp.Faction			= fields[1].Get<uint32>();
            Temp.GraveyardPos		= fields[2].Get<uint32>();
            Temp.GraveyardSafe		= fields[3].Get<uint32>();
            Temp.GraveyardHealer	= fields[4].Get<uint32>();
            Temp.StopPlayerKills	= fields[5].Get<uint32>();
            Temp.StopPlayerKilleds	= fields[6].Get<uint32>();
            Temp.StopTelePos		= fields[7].Get<uint32>();
            Temp.GraveyardSafeNotice	= fields[8].Get<std::string>();
            Temp.StopNotice			= fields[9].Get<std::string>();
            eventData.PhaseFactionVec.push_back(Temp);
        } while (result->NextRow());

        SummonSoulHealer(eventId);
    }

    if (result = WorldDatabase.Query("SELECT 世界状态ID, 世界状态初始值, 更新世界状态需要击杀生物或摧毁物体ID1, 更新世界状态增减值1,""更新世界状态需要击杀生物或摧毁物体ID2, 更新世界状态增减值2, 更新世界状态需要击杀生物或摧毁物体ID3, 更新世界状态增减值3,""世界状态达到该值时提示, 提示文本 From _事件_世界状态配置 WHERE 事件ID = {}" , eventId))
    {
        do
        {
            Field* fields = result->Fetch();
            EventWorldStateTemplate Temp;
            Temp.ID			= fields[0].Get<uint32>();
            Temp.InitValue		= fields[1].Get<int32>();
            for (size_t i = 0; i < WORLDSTATE_ENTRY_MAX; i++)
            {
                Temp.Entry[i] = fields[2 + i * 2].Get<int32>();
                Temp.Step[i] = fields[3 + i * 2].Get<int32>();
            }
            Temp.NoticeValue	= fields[8].Get<int32>();
            Temp.Notice		= fields[9].Get<std::string>();
            eventData.WorldStateVec.push_back(Temp);
        } while (result->NextRow());
    }

    if (result = WorldDatabase.Query("SELECT 世界状态ID, 队伍ID, 击杀或被杀, 更新世界状态增减值, 世界状态达到该值时提示, 提示文本 From _事件_世界状态队伍配置 WHERE 事件ID  = {}", eventId))
    {
        do
        {
            Field* fields = result->Fetch();
            EventWorldStateFactionTemplate Temp;
            Temp.ID			= fields[0].Get<uint32>();
            Temp.Faction		= fields[1].Get<uint32>();
            Temp.Kill		= fields[2].Get<bool>();
            Temp.Step		= fields[3].Get<int32>();
            Temp.NoticeValue	= fields[4].Get<int32>();
            Temp.Notice		= fields[5].Get<std::string>();
            eventData.WorldStateFactionVec.push_back(Temp);
        } while (result->NextRow());
    }

    if (result = WorldDatabase.Query("SELECT 刷新生物或物体ID, 阶段ID, 刷新时间, 刷新位置坐标ID, 移动到坐标ID, 持续时间,""提示时间1, 提示文本1, 提示时间2, 提示文本2, 提示时间3, 提示文本3 From _事件_生物或物体刷新配置 WHERE 事件ID = {}", eventId))
    {
        do
        {
            Field* fields = result->Fetch();
            EventSpawnTemplate Temp;
            Temp.Entry		= fields[0].Get<int32>();
            Temp.Phase		= fields[1].Get<uint32>();
            Temp.SpawnTime	= fields[2].Get<uint32>();
            Temp.PosId		= fields[3].Get<uint32>();
            Temp.MovePosId = fields[4].Get<uint32>();
            Temp.Duration	= fields[5].Get<uint32>();

            for (size_t i = 0; i < MAX_SPAWNNOTICE_COUNT; i++)
            {
                Temp.NoticeTime[i] = fields[6 + i * 2].Get<int32>();
                Temp.SpawnNotice[i] = fields[7 + i * 2].Get<std::string>();
            }

            eventData.SpawnVec.push_back(Temp);
        } while (result->NextRow());
    }
}

void CustomEventMgr::GetTelePos(uint32 posId, uint32& map, float& x, float& y, float& z, float& o)
{
    auto It = PosMap.find(posId);
    if (It != PosMap.end())
    {
        map = It->second.MapId;
        x = It->second.X;
        y = It->second.Y;
        z = It->second.Z;
        o = It->second.O;
    }
}

bool CustomEventMgr::IpCheck(Player* player, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return true;

    uint32 Max = uint32(sHelper->GetUInt(1090));
    if (!Max)
        return true;

    uint32 Count = 0;
    for (auto itr = eventData->PlayersMap.begin(); itr != eventData->PlayersMap.end(); itr++)
        if (Player* pl = ObjectAccessor::FindPlayer(itr->first))
            if (strcmp(pl->GetSession()->GetRemoteAddress().c_str(), player->GetSession()->GetRemoteAddress().c_str()) == 0)
                Count++;

    if (Count > Max)
    {
        std::ostringstream Oss;
        Oss << "加入活动已达到限制 加入失败 " << Count << " / " << Max;
        player->SendSysMsg(Oss.str().c_str());
        return false;
    }
    else
        return true;
}

//战场
bool CustomEventMgr::IsFixedTimeBgActive(BattlegroundTypeId bgTypeId)
{
    if (bgTypeId == BATTLEGROUND_RB)
        return false;

    EventTypes eventType = C_ET_NONE;

    switch (bgTypeId)
    {
    case BATTLEGROUND_AV:
        eventType = C_ET_AV;
        break;
    case BATTLEGROUND_WS:
        eventType = C_ET_WS;
        break;
    case BATTLEGROUND_AB:
        eventType = C_ET_AB;
        break;
    case BATTLEGROUND_EY:
        eventType = C_ET_EY;
        break;
    case BATTLEGROUND_SA:
        eventType = C_ET_SA;
        break;
    case BATTLEGROUND_IC:
        eventType = C_ET_IC;
        break;
    }

    // 遍历所有活动数据，检查是否有匹配的活动类型
    for (auto itr = _eventDataMap.begin(); itr != _eventDataMap.end(); itr++)
    {
        if (itr->second.EventType == eventType)
            return true;
    }

    return false;
}

int32 CustomEventMgr::GetZoneId(uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return -1;

    return eventData->ZoneId;
}

int32 CustomEventMgr::GetMapId(uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return -1;

    return eventData->MapId;
}


void CustomEventMgr::Start(uint32 eventId)
{
    if (eventId <= MINEVENTSO)
        return;

    InitParams(eventId);
    InitWorldState(eventId);

    EventData* eventData = GetEventData(eventId);
    if (!eventData || eventData->EventType == C_ET_NONE)
        return;

    std::ostringstream oss;
    oss << eventData->EventName << "开始";
    sWorld->SendServerMessage(SERVER_MSG_STRING, oss.str().c_str());

    ReviveAll(true);

    if (eventData->EventType < C_ET_LINE)
    {
        StartCall(eventId);
        return;
    }

    eventData->DelayStartCall = true;

    oss.str("");
    oss << eventData->EventName;

    switch (eventData->RejoinType)
    {
    case C_RT_BANED:
        oss << "重新加入活动将被禁止";
        break;
    case C_RT_KEEP_GROUP:
        oss << "重新加入活动不会改变分组";
        break;
    case C_RT_REDIS_GROUP:
        oss << "重新加入活动时 系统会重新分组";
        break;
    }

    sWorld->SendServerMessage(SERVER_MSG_STRING, oss.str().c_str());
}

int32 CustomEventMgr::GetWeightLeft(uint32 faction, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return 0;

    bool exist = false;
    uint32 Weight = 0;

    for (auto itr = eventData->StopVec.begin(); itr != eventData->StopVec.end(); itr++)
    {
        if (faction != itr->Faction)
            continue;

        exist = true;

        if (itr->StopType == TYPEID_PLAYER)
            Weight += itr->Param * itr->Weight;
        else if (itr->Param != 0)
            Weight += itr->Weight;
    }

    if (!exist)
        return 0;

    if (Weight == 0)
        return -1;

    return Weight;
}

void CustomEventMgr::Stop(uint32 eventId)
{
    if (eventId <= MINEVENTSO)
        return;

    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    if (eventData->EventType >= C_ET_WS && eventData->EventType <= C_ET_IC)
    {
        Reset(eventId);
        return;
    }

    if (!Valid(eventId))
        return;

    int32 w = 100000;
    eventData->WinFaction = 0;

    for (auto itr = eventData->FactionVec.begin(); itr != eventData->FactionVec.end(); itr++)
    {
        int32 weight = GetWeightLeft(*itr, eventId);

        if (weight < w)
        {
            w = weight;
            eventData->WinFaction = *itr;
        }
    }

    auto itr = eventData->FactionNameMap.find(eventData->WinFaction);
    if (itr != eventData->FactionNameMap.end())
        SendScreenMessage("[服务器]: 事件 <" + eventData->EventName + "> 队伍<" + itr->second + "> 获胜", 0, false, eventId);

    ReviveAll(false);
    eventData->DelayStopCall = true;
}

void CustomEventMgr::UpdateActive(Object* obj,bool kill, uint32 eventId)
{
    if (eventId == 0)
    {
        // 遍历所有活动数据
        for (auto itr = _eventDataMap.begin(); itr != _eventDataMap.end(); itr++)
        {
            UpdateActive(obj, kill, itr->first);
        }
        return;
    }

    EventData* eventData = GetEventData(eventId);
    if (!eventData || eventData->AltVec.empty())
        return;

    uint32 targetEntry = obj->GetEntry();
    TypeID typeId = obj->GetTypeId();

    switch (typeId)
    {
    case TYPEID_UNIT:
    {
        for (auto itr = eventData->AltVec.begin(); itr != eventData->AltVec.end(); itr++)
            for (size_t i = 0; i < ALT_GUID_MAX; i++)
                if (targetEntry == itr->TargetEntry[i] && itr->TargetCount[i] != itr->TargetKillCount[i])
                {
                    itr->TargetKillCount[i]++;
                    SendScreenMessage("击杀[" + sObjectMgr->GetCreatureTemplate(obj->GetEntry())->Name + "] " + std::to_string(itr->TargetKillCount[i]) + " / " + std::to_string(itr->TargetCount[i]), 0, false, eventId);
                }

        break;
    }
    case TYPEID_GAMEOBJECT:
    {
        for (auto itr = eventData->AltVec.begin(); itr != eventData->AltVec.end(); itr++)
            for (size_t i = 0; i < ALT_GUID_MAX; i++)
                if (targetEntry == -itr->TargetEntry[i] && itr->TargetCount[i] != itr->TargetKillCount[i])
                {
                    itr->TargetKillCount[i]++;
                    SendScreenMessage("摧毁[" + sObjectMgr->GetGameObjectTemplate(obj->GetEntry())->name + "] " + std::to_string(itr->TargetKillCount[i]) + "/" + std::to_string(itr->TargetCount[i]), 0, false, eventId);
                }
        break;
    }
    case TYPEID_PLAYER:
    {
        for (auto itr = eventData->AltFactionVec.begin(); itr != eventData->AltFactionVec.end(); itr++)
        {
            if (itr->Faction == obj->ToPlayer()->GetEventFaction())
            {
                if (itr->Kills > 0 && kill)
                    itr->Kills--;
                if (itr->Killeds > 0 && !kill)
                    itr->Killeds--;
            }
        }
        break;
    }
    }

    for (auto itr = eventData->AltVec.begin(); itr != eventData->AltVec.end(); itr++)
    {
        int32 GUID = itr->GUID;

        if (IsInAltedVec(uint32(GUID), eventId))
            continue;

        bool check1 = true;

        for (size_t i = 0; i < ALT_GUID_MAX; i++)
            if (itr->TargetCount[i] != itr->TargetKillCount[i])
                check1 = false;

        bool check2 = true;

        if (check1)
            for (auto i = eventData->AltFactionVec.begin(); i != eventData->AltFactionVec.end(); i++)
                if (i->GUID == GUID)
                    if (i->Kills != 0 || i->Killeds != 0)
                        check2 = false;

        if (check1 && check2)
        {
            eventData->AltedVec.push_back(GUID);

            if (eventData->Map)
            {
                if (GUID > 0)
                {
                    if (CreatureData const* data = sObjectMgr->GetCreatureData(GUID))
                    {
                        auto CreBounds = eventData->Map->GetCreatureBySpawnIdStore().equal_range(uint32(GUID));
                        if (CreBounds.first != CreBounds.second)
                        {
                            if (Creature* c = CreBounds.first->second)
                            {
                                c->SetUInt32Value(UNIT_FIELD_FLAGS, itr->AltFlag);
                                SendScreenMessage(itr->NoticeText, 0, false, eventId);
                            }
                        }
                    }
                }
                else
                {
                    if (GameObjectData const* data = sObjectMgr->GetGOData(abs(GUID)))
                    {
                        auto GoBounds = eventData->Map->GetGameObjectBySpawnIdStore().equal_range(uint32(abs(GUID)));
                        if (GoBounds.first != GoBounds.second)
                        {
                            if (GameObject* g = GoBounds.first->second)
                            {
                                g->SetUInt32Value(GAMEOBJECT_FLAGS, itr->AltFlag);
                                SendScreenMessage(itr->NoticeText, 0, false, eventId);
                            }
                        }
                    }
                }
            }
        }
    }
}

float CustomEventMgr::GetDistance(Player* player, uint32 pos)
{
    auto It = PosMap.find(pos);
    if (It != PosMap.end())
        return player->GetDistance(It->second.X, It->second.Y, It->second.Z);

    return 1000.0f;
}

void CustomEventMgr::Tele(Player* player, uint32 pos)
{
    auto It = PosMap.find(pos);
    if (It != PosMap.end())
    {
        player->SetCanTeleport(true);
        player->TeleportTo(It->second.MapId, It->second.X, It->second.Y, It->second.Z, It->second.O);
    }
}

uint32 CustomEventMgr::GetPhaseGraveyardPos(uint32 faction, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return 0;

    for (auto itr = eventData->PhaseFactionVec.begin(); itr != eventData->PhaseFactionVec.end(); itr++)
        if (itr->Phase == eventData->Phase && itr->Faction == faction)
            return itr->GraveyardPos;

    return 0;
}

void CustomEventMgr::TeleToGraveyard(Player* player, uint32 eventId)
{
    Tele(player, GetPhaseGraveyardPos(player->GetEventFaction(), eventId));
}

template <class TValue>
void SortMapByValue(std::unordered_map<std::string, TValue>& tMap, std::vector<std::pair<std::string, TValue> >& tVector)
{
    for (typename std::unordered_map<std::string, TValue>::iterator curr = tMap.begin(); curr != tMap.end(); curr++)
        tVector.push_back(std::make_pair(curr->first, curr->second));

    std::sort(tVector.begin(), tVector.end(), [](const std::pair<std::string, TValue>& V1, const std::pair<std::string, TValue>& V2) {return V1.second > V2.second; });
}

void CustomEventMgr::RewAndRest(uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    std::unordered_map<std::string, uint32 >KillsMap;
    std::unordered_map<std::string, uint32 >KilledsMap;
    std::unordered_map<std::string, AsInt64 >DamageMap;
    std::unordered_map<std::string, AsInt64 >HealMap;

    for (auto itr = eventData->PlayersMap.begin(); itr != eventData->PlayersMap.end(); itr++)
    {
        if (Player* pl = ObjectAccessor::FindPlayer(itr->first))
        {
            if (!pl->InEvent())
                continue;

            std::string name = sHelper->GetNameLinkColor(pl);
            KillsMap[name] = pl->GetEventData().Kills;
            KilledsMap[name] = pl->GetEventData().Killeds;
            DamageMap[name] = pl->GetEventData().Damage;
            HealMap[name] = pl->GetEventData().Heal;

            if (eventData->EventType == C_ET_LINE && sHelper->GetBoolean(20001))
            {
                if (pl->GetGroup())
                    pl->RemoveFromGroup();
            }
        }
    }

    std::ostringstream oss;

    std::vector<std::pair<std::string, uint32>> tVector;
    std::vector<std::pair<std::string, AsInt64>> tAsVector;
    uint32 count;

    oss << "|cffFF0000击杀排行|r\n";
    count = 0;
    tVector.clear();
    SortMapByValue(KillsMap, tVector);

    for (uint32 i = 0; i < tVector.size(); i++)
    {
        if (count > 3)
            continue;
        count++;
        oss << "|cFFF49B00" << tVector[i].first << "|r - " << tVector[i].second << "\n";
    }

    oss << "\n\n";
    oss << "|cffFF0000伤害排行|r\n";
    count = 0;
    tAsVector.clear();
    SortMapByValue(DamageMap, tAsVector);

    for (uint32 i = 0; i < tAsVector.size(); i++)
    {
        if (count > 3)
            continue;
        count++;
        oss << "|cFFF49B00" << tAsVector[i].first << "|r - " << AsInt64ToString(tAsVector[i].second) << "\n";
    }

    oss << "\n\n";
    oss << "|cffFF0000治疗排行|r\n";
    count = 0;
    tAsVector.clear();
    SortMapByValue(HealMap, tAsVector);

    for (uint32 i = 0; i < tAsVector.size(); i++)
    {
        if (count > 3)
            continue;
        count++;
        oss << "|cFFF49B00" << tAsVector[i].first << "|r - " << AsInt64ToString(tAsVector[i].second) << "\n";
    }

    oss <<"\n\n";
    for (auto itr = eventData->PlayersMap.begin(); itr != eventData->PlayersMap.end(); itr++)
    {
        if (Player* pl = ObjectAccessor::FindPlayer(itr->first))
        {
            if (pl->InEvent())
            {
                std::ostringstream oss1;
                oss1 << "|cffFF0000你的个人战绩|r" << "\n";
                oss1 << "|cFFF49B00击杀|r - " << pl->GetEventData().Kills << "\n";
                oss1 << "|cFFF49B00被杀|r - " << pl->GetEventData().Killeds << "\n";
                oss1 << "|cFFF49B00伤害|r - " << AsInt64ToString(pl->GetEventData().Damage) << "\n";
                oss1 << "|cFFF49B00治疗|r - " << AsInt64ToString(pl->GetEventData().Heal) << "\n";

                sHelper->PopToPlayerDialog(pl, oss.str() + oss1.str());

                if (CanRew(pl, eventId))
                {
                    if (pl->GetEventFaction() == eventData->WinFaction)
                        sHelper->DoTheReward(pl, eventData->WinRewId, "自定义事件胜利奖励");
                    else
                        sHelper->DoTheReward(pl, eventData->LosRewId, "自定义事件失败奖励");
                }

                auto i = eventData->StopPosMap.find(pl->GetEventFaction());

                if (i != eventData->StopPosMap.end())
                    Tele(pl, i->second);
            }

            pl->EventRest();
        }
    }

    Reset(eventId);
}

void CustomEventMgr::Update(Map* NowMap, uint32 diff)
{
    // 遍历所有活动数据
    for (auto itr = _eventDataMap.begin(); itr != _eventDataMap.end(); itr++)
    {
        EventData* eventData = &itr->second;
        if (!eventData->Map)
            continue;

        if (eventData->EventType < C_ET_TELE)
            continue;

        if (eventData->Map->GetId() != NowMap->GetId())
            continue;

        //延迟召唤
        if (eventData->DelayStartCall)
        {
            eventData->UpdateDelayNotice += diff;

            if (eventData->UpdateDelayNotice > 500)
            {
                eventData->UpdateDelayNotice = 0;
                eventData->DelayStartCall = false;
                StartCall(eventData->EventId);
            }
        }

        //延迟奖励并重置
        if (eventData->DelayStopCall)
        {
            eventData->UpdateDelayNotice += diff;

            if (eventData->UpdateDelayNotice > 500)
            {
                eventData->UpdateDelayNotice = 0;
                eventData->DelayStopCall = false;
                RewAndRest(eventData->EventId);
            }
        }

        UpdateRevive(diff, eventData->EventId);
        UpdateSafe(diff, eventData->EventId);
        UpdateSpawn(diff, eventData->EventId);
    }
}

void CustomEventMgr::UpdateSpawn(uint32 diff, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData || !eventData->Map)
        return;

    eventData->UpdateSpawnTimer += diff;

    if (eventData->UpdateSpawnTimer < IN_MILLISECONDS)
        return;

    eventData->UpdateSpawnTimer = 0;

    for (auto itr = eventData->SpawnVec.begin(); itr != eventData->SpawnVec.end(); itr++)
    {
        if (itr->SpawnTime == 0 || (itr->Phase != 0 && itr->Phase != eventData->Phase))
            continue;

        itr->SpawnTime -= 1;

        for (size_t i = 0; i < MAX_SPAWNNOTICE_COUNT; i++)
            if (itr->SpawnTime == itr->NoticeTime[i])
                SendScreenMessage(itr->SpawnNotice[i], 0, false, eventId);

        if (itr->SpawnTime == 0)
        {
            uint32 map = 0;
            float x = 0;
            float y = 0;
            float z = 0;
            float o = 0;

            GetTelePos(itr->PosId, map, x, y, z, o);

            Position pos;
            pos.Relocate(x, y, z, o);

            uint32 MapId = 0;
            float X = 0;
            float Y = 0;
            float Z = 0;
            float O = 0;
            GetTelePos(itr->MovePosId, MapId, X, Y, Z, O);
            Position Mpos;
            Mpos.Relocate(X, Y, Z, O);

            if (eventData->Map)
            {
                if (itr->Entry > 0)
                {
                    if (Creature* Cre = eventData->Map->SummonCreature(itr->Entry, pos, 0, itr->Duration * IN_MILLISECONDS))
                    {
                        if (itr->MovePosId)
                            Cre->GetMotionMaster()->MoveCharge(X + frand(-2.0f, 2.0f), Y + frand(-2.0f, 2.0f), Z, Cre->GetSpeed(MOVE_RUN));
                    }
                }
                else
                {
                    if (GameObject* GObj = eventData->Map->SummonGameObject(-itr->Entry, pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), pos.GetOrientation(), 0, 0, 0, 0, itr->Duration))
                    {

                    }
                }
            }
        }
    }
}

void CustomEventMgr::UpdateRevive(uint32 diff, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    eventData->UpdateReviveTimer += diff;
    if (eventData->UpdateReviveTimer >= 30000)
    {
        eventData->UpdateReviveTimer = 0;

        for (auto itr = eventData->PlayersMap.begin(); itr != eventData->PlayersMap.end(); itr++)
        {
            if (Player* pl = ObjectAccessor::FindPlayer(itr->first))
            {
                if (!pl->InEvent())
                    continue;

                if (pl->IsAlive())
                    continue;

                if (GetDistance(pl, GetPhaseGraveyardPos(pl->GetEventFaction(), eventId)) < 20.0f)
                {
                    pl->SendSysMsg("%s 拿出你的勇气 继续战斗", eventData->EventName.c_str());
                    pl->CastSpell(pl, 24171, true);
                    pl->ResurrectPlayer(1.0f);
                    pl->CastSpell(pl, 6962, true);
                    pl->CastSpell(pl, 44535, true);
                    pl->GetMap()->ConvertCorpseToBones(pl->GetGUID());
                }
            }
        }
    }
}

void CustomEventMgr::UpdateSafe(uint32 diff, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    eventData->UpdateSafeTimer += diff;

    if (eventData->UpdateSafeTimer >= 1000)
    {
        eventData->UpdateSafeTimer = 0;

        for (auto itr = eventData->PlayersMap.begin(); itr != eventData->PlayersMap.end(); itr++)
        {
            if (Player* pl = ObjectAccessor::FindPlayer(itr->first))
            {
                if (pl->InEvent() && pl->IsAlive())
                {
                    for (auto i = eventData->PhaseFactionVec.begin(); i != eventData->PhaseFactionVec.end(); i++)
                    {
                        if (i->Phase == eventData->Phase)
                        {
                            if (i->Faction != pl->GetEventFaction() && GetDistance(pl, i->GraveyardPos) < i->GraveyardSafe)
                            {
                                TeleToGraveyard(pl, eventId);
                                pl->GetSession()->SendNotification(i->GraveyardSafeNotice.c_str());
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

uint32 CustomEventMgr::GetFactionMembers(uint32 faction, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return 0;

    uint32 count = 0;

    for (auto iter = eventData->PlayersMap.begin(); iter != eventData->PlayersMap.end(); iter++)
        if (faction == iter->second)
            count++;

    return count;
}

void CustomEventMgr::ReviveAll(bool start)
{
    SessionMap const& smap = sWorld->GetAllSessions();
    for (SessionMap::const_iterator iter = smap.begin(); iter != smap.end(); ++iter)
        if (Player* player = iter->second->GetPlayer())
        {
            if (player->IsAlive())
                continue;

            if (start || !start && player->InEvent())
            {
                player->ResurrectPlayer(1.0f);
                player->SpawnCorpseBones();
            }
        }
}

void CustomEventMgr::StartCall(uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    SessionMap const& smap = sWorld->GetAllSessions();
    for (SessionMap::const_iterator iter = smap.begin(); iter != smap.end(); ++iter)
    {
        if (!iter->second)
            continue;

        Player* player = iter->second->GetPlayer();
        if (player && player->IsInWorld())
        {
            sHelper->PopToPlayerDialog(player, eventData->NoticeText, 0, eventData->EventId, AtEvent);
        }
    }
}

void CustomEventMgr::Join(Player* pl, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData || !Valid(eventId))
    {
        pl->SendSysMsg("加入失败 活动已经结束");
        return;
    }

    //超过IP限制
    if (!IpCheck(pl, eventId))
        return;

    //已经在事件中，先结束当前活动再加入新活动
    if (pl->InEvent())
    {
        pl->EventRest();
    }

    uint32 faction = Subgroup(pl, eventId);

    if (faction == 0)
    {
        pl->SendSysMsg("%s 加入失败 无法重新加入", eventData->EventName.c_str());
        return;
    }

    pl->UninviteFromGroup();
    if (pl->GetGroup())
        pl->RemoveFromGroup();

    if (pl->isDead())
    {
        pl->ResurrectPlayer(1.0f);
        pl->SpawnCorpseBones();
    }

    pl->InitEventData(eventData->EventId, eventData->FactionNameMap[faction], faction);
    eventData->PlayersMap[pl->GetGUID()] = faction;

    bool groupExist = false;

    for (auto iter = eventData->GroupMap.begin(); iter != eventData->GroupMap.end(); iter++)
    {
        if (Group* group = sGroupMgr->GetGroupByGUID(iter->first))
        {
            if (!group->IsFull() && iter->second == faction)
            {
                group->AddMember(pl);
                groupExist = true;
                break;
            }
        }
    }

    if (!groupExist)
    {
        Group* group = new Group();
        group->Create(pl);
        group->ConvertToRaid();
        sGroupMgr->AddGroup(group);
        eventData->GroupMap[group->GetGUID().ToGuidLow()] = faction;
    }

    auto itr = eventData->StartPosMap.find(faction);
    if (itr != eventData->StartPosMap.end())
        Tele(pl, itr->second);
}

uint32 CustomEventMgr::Subgroup(Player* pl, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return 0;

    auto iter = eventData->PlayersMap.find(pl->GetGUID());
    if (iter != eventData->PlayersMap.end())
    {
        if (eventData->RejoinType == C_RT_BANED)
            return 0;
        else if (eventData->RejoinType == C_RT_KEEP_GROUP)
            return iter->second;
    }

    if (eventData->EventType == C_ET_LINE)
    {
        switch (eventData->GroupType)
        {
        case C_GT_ONE_ALL:
        {
            return 1;
        }
        break;
        case C_GT_ONE_A:
        {
            if (pl->GetTeamId() == TEAM_ALLIANCE)
                return 1;
            else
                return 2;
        }
        break;
        case C_GT_ONE_H:
        {
            if (pl->GetTeamId() == TEAM_ALLIANCE)
                return 2;
            else
                return 1;
        }
        break;
        case C_GT_RANDOM_A_H:
        {
            if (eventData->RandomAH)
            {
                if (pl->GetTeamId() == TEAM_ALLIANCE)
                    return 1;
                else
                    return 2;
            }
            else
            {
                if (pl->GetTeamId() == TEAM_ALLIANCE)
                    return 2;
                else
                    return 1;
            }
        }
        break;
        case C_GT_RANDOM_PLAYER:
        {
            if (GetFactionMembers(1, eventId) > GetFactionMembers(2, eventId))
                return 2;
            else
                return 1;
        }
        break;
        }
    }
    else if (eventData->EventType == C_ET_GUILD)
        return pl->GetGuildId();

    return 0;
}

void CustomEventMgr::UpdatePhase(Object* obj, bool kill, uint32 eventId)
{
    if (eventId == 0)
    {
        // 遍历所有活动数据
        for (auto itr = _eventDataMap.begin(); itr != _eventDataMap.end(); itr++)
        {
            UpdatePhase(obj, kill, itr->first);
        }
        return;
    }

    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    bool nextphaseexsit = false;

    for (auto itr = eventData->PhaseFactionVec.begin(); itr != eventData->PhaseFactionVec.end(); itr++)
    {
        if (itr->Phase == (eventData->Phase + 1))
        {
            nextphaseexsit = true;
            break;
        }
    }

    if (!nextphaseexsit)
        return;

    uint32 guid = obj->GetGUIDLow();
    TypeID typeId = obj->GetTypeId();

    switch (typeId)
    {
    case TYPEID_UNIT:
    {
        for (auto itr = eventData->PhaseVec.begin(); itr != eventData->PhaseVec.end(); itr++)
        {
            if (itr->Phase == eventData->Phase)
            {
                if (itr->StopGUID == guid && itr->StopGUID > 0)
                    itr->StopGUID = 0;
            }
        }
    }
        break;
    case TYPEID_PLAYER:
    {
        for (auto itr = eventData->PhaseFactionVec.begin(); itr != eventData->PhaseFactionVec.end(); itr++)
        {
            if (itr->Phase == eventData->Phase && obj->ToPlayer()->GetEventFaction() == itr->Faction)
            {
                if (itr->StopPlayerKills > 0 && kill)
                    itr->StopPlayerKills--;

                if (itr->StopPlayerKilleds > 0 && !kill)
                    itr->StopPlayerKilleds--;
            }
        }
    }
        break;
    case TYPEID_GAMEOBJECT:
    {
        for (auto itr = eventData->PhaseVec.begin(); itr != eventData->PhaseVec.end(); itr++)
        {
            if (itr->Phase == eventData->Phase)
            {
                if (-itr->StopGUID == guid && itr->StopGUID < 0)
                    itr->StopGUID = 0;
            }
        }
    }
        break;
    }

    for (auto itr = eventData->PhaseVec.begin(); itr != eventData->PhaseVec.end(); itr++)
        if (itr->Phase == eventData->Phase)
            if (itr->StopGUID != 0)
                return;

    for (auto itr = eventData->PhaseFactionVec.begin(); itr != eventData->PhaseFactionVec.end(); itr++)
        if (itr->Phase == eventData->Phase && itr->StopPlayerKilleds == 0 && itr->StopPlayerKills == 0)
        {
            for (auto playerItr = eventData->PlayersMap.begin(); playerItr != eventData->PlayersMap.end(); playerItr++)
            {
                if (Player* pl = ObjectAccessor::FindPlayer(playerItr->first))
                {
                    if (!pl->InEvent())
                        continue;

                    for (auto i = eventData->PhaseFactionVec.begin(); i != eventData->PhaseFactionVec.end(); i++)
                        if (eventData->Phase == i->Phase && pl->GetEventFaction() == i->Faction)
                        {
                            if (!i->StopNotice.empty())
                            {
                                WorldPacket data;
                                ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, nullptr, nullptr, i->StopNotice);

                                if (WorldSession* ss = pl->GetSession())
                                    ss->SendPacket(&data);
                            }
                            Tele(pl, i->StopTelePos);
                        }
                }
            }

            eventData->Phase++;
            SummonSoulHealer(eventId);
            return;
        }
}

void CustomEventMgr::UpdateWorldState(uint32 ID, uint32 value, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    for (auto itr = eventData->PlayersMap.begin(); itr != eventData->PlayersMap.end(); itr++)
        if (Player* pl = ObjectAccessor::FindPlayer(itr->first))
            pl->SendUpdateWorldState(ID, value);
}


void CustomEventMgr::InitWorldState(uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    for (auto itr = eventData->WorldStateVec.begin(); itr != eventData->WorldStateVec.end(); itr++)
    {
        sWorld->setWorldState(itr->ID, itr->InitValue);

        SessionMap const& smap = sWorld->GetAllSessions();
        for (SessionMap::const_iterator i = smap.begin(); i != smap.end(); ++i)
            if (Player* pl = i->second->GetPlayer())
                pl->SendUpdateWorldState(itr->ID, itr->InitValue);
    }
}

void CustomEventMgr::UpdateWorldState(Object* obj, bool kill, uint32 eventId)
{
    if (eventId == 0)
    {
        // 遍历所有活动数据
        for (auto itr = _eventDataMap.begin(); itr != _eventDataMap.end(); itr++)
        {
            UpdateWorldState(obj, kill, itr->first);
        }
        return;
    }

    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    int64 entry = obj->GetEntry();
    TypeID typeId = obj->GetTypeId();

    switch (typeId)
    {
    case TYPEID_UNIT:
    {
        for (auto itr = eventData->WorldStateVec.begin(); itr != eventData->WorldStateVec.end(); itr++)
        {
            for (size_t i = 0; i < WORLDSTATE_ENTRY_MAX; i++)
            {
                if (entry == itr->Entry[i])
                {
                    if (itr->Step[i] == 0)
                        continue;

                    uint64 value = sWorld->getWorldState(itr->ID);
                    value += itr->Step[i];
                    sWorld->setWorldState(itr->ID, value);
                    UpdateWorldState(itr->ID, value, eventId);
                    if (value == itr->NoticeValue || itr->NoticeValue == 0)
                        SendScreenMessage(itr->Notice, 0, false, eventId);
                }
            }
        }
    }
        break;
    case TYPEID_GAMEOBJECT:
    {
        entry = -entry;

        for (auto itr = eventData->WorldStateVec.begin(); itr != eventData->WorldStateVec.end(); itr++)
        {
            for (size_t i = 0; i < WORLDSTATE_ENTRY_MAX; i++)
            {
                if (entry == itr->Entry[i])
                {
                    if (itr->Step[i] == 0)
                        continue;

                    uint64 value = sWorld->getWorldState(itr->ID);
                    value += itr->Step[i];
                    sWorld->setWorldState(itr->ID, value);
                    UpdateWorldState(itr->ID, value, eventId);
                    if (value == itr->NoticeValue || itr->NoticeValue == 0)
                        SendScreenMessage(itr->Notice, 0, false, eventId);
                }
            }
        }
    }
        break;
    case TYPEID_PLAYER:
    {
        for (auto itr = eventData->WorldStateFactionVec.begin(); itr != eventData->WorldStateFactionVec.end(); itr++)
        {
            if (itr->Faction == obj->ToPlayer()->GetEventFaction() && kill == itr->Kill)
            {
                if (itr->Step == 0)
                    continue;

                uint64 value = sWorld->getWorldState(itr->ID);
                value += itr->Step;
                sWorld->setWorldState(itr->ID, value);
                UpdateWorldState(itr->ID, value, eventId);
                if (value == itr->NoticeValue || itr->NoticeValue == 0)
                    SendScreenMessage(itr->Notice, 0, false, eventId);
            }
        }
    }
        break;
    }
}

void CustomEventMgr::Process(Object* obj, uint32 eventId)
{
    if (eventId == 0)
    {
        // 遍历所有活动数据
        for (auto itr = _eventDataMap.begin(); itr != _eventDataMap.end(); itr++)
        {
            Process(obj, itr->first);
        }
        return;
    }

    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    switch (obj->GetTypeId())
    {
    case TYPEID_UNIT:
        if (obj->ToCreature())
        {
            uint32 guid = obj->ToCreature()->GetSpawnId();
            for (auto itr = eventData->StopVec.begin(); itr != eventData->StopVec.end(); itr++)
            {
                if ((guid == itr->Param && itr->StopType == TYPEID_UNIT) || (obj->GetEntry() == itr->Param && itr->StopType == TYPEID_UNIT_ENTRY))
                {
                    if (itr->AtCount > 0)
                        itr->AtCount--;

                    if (itr->AtCount == 0)
                        itr->Param = 0;
                }
            }
        }
        break;
    case TYPEID_GAMEOBJECT:
        if (obj->ToGameObject())
        {
            uint32 guid = obj->ToGameObject()->GetSpawnId();
            for (auto itr = eventData->StopVec.begin(); itr != eventData->StopVec.end(); itr++)
            {
                if (guid == itr->Param && itr->StopType == TYPEID_GAMEOBJECT)
                {
                    if (itr->AtCount > 0)
                        itr->AtCount--;

                    if (itr->AtCount == 0)
                        itr->Param = 0;
                }
            }
        }
    break;
    case TYPEID_PLAYER:
        for (auto itr = eventData->StopVec.begin(); itr != eventData->StopVec.end(); itr++)
            if (itr->StopType == TYPEID_PLAYER && itr->Faction == obj->ToPlayer()->GetEventFaction())
                if (itr->Param > 0)
                    itr->Param--;
    break;
    }

    for (auto itr = eventData->StopVec.begin(); itr != eventData->StopVec.end(); itr++)
    {
        uint32 faction = itr->Faction;

        bool stop = true;

        for (auto i = eventData->StopVec.begin(); i != eventData->StopVec.end(); i++)
            if (faction == i->Faction)
                if (i->Param != 0)
                {
                    stop = false;
                }

        if (stop)
        {
            sGameEventMgr->StopEvent(eventData->EventId, false, true);
            return;
        }
    }
}

bool CustomEventMgr::DiedTele(Player* player, uint32 eventId)
{
    if (player->InEvent())
    {
        Tele(player, GetPhaseGraveyardPos(player->GetEventFaction(), eventId));
        return true;
    }

    return false;
}

bool CustomEventMgr::SendAreaSpiritHealerQueryOpcode(Player* player, uint32 eventId)
{
    if (!player)
        return false;

    if (player->GetBattleground())
        return false;

    if (!player->InEvent() || player->IsAlive())
        return false;

    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return false;

    auto itr = eventData->HealerMap.find(player->GetEventFaction());
    if (itr != eventData->HealerMap.end())
    {
        WorldPacket data(SMSG_AREA_SPIRIT_HEALER_TIME, 12);
        uint32 time_ = 30000 - eventData->UpdateReviveTimer;
        if (time_ == uint32(-1))
            time_ = 0;
        data << itr->second << time_;
        player->SendDirectMessage(&data);
        return true;
    }

    return false;
}

void CustomEventMgr::SummonSoulHealer(uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData || !eventData->Map)
        return;

    for (auto itr = eventData->HealerMap.begin(); itr != eventData->HealerMap.end(); itr++)
        if (Creature* healer = eventData->Map->GetCreature(itr->second))
            healer->RemoveFromWorld();

    eventData->HealerMap.clear();

    for (auto itr = eventData->PhaseFactionVec.begin(); itr != eventData->PhaseFactionVec.end(); itr++)
    {
        if (itr->Phase != eventData->Phase)
            continue;

        uint32 map = 0;
        float x = 0;
        float y = 0;
        float z = 0;
        float o = 0;

        GetTelePos(itr->GraveyardPos, map, x, y, z, o);

        Position pos;
        pos.Relocate(x, y, z, o);

        if (Creature* healer = eventData->Map->SummonCreature(itr->GraveyardHealer, pos))
        {
            eventData->HealerMap[itr->Faction] = healer->GetGUID();
        }
    }
}

void CustomEventMgr::SendScreenMessage(std::string text, uint32 faction, bool onlydead, uint32 eventId)
{
    if (text.empty())
        return;

    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, nullptr, nullptr, text);

    for (auto itr = eventData->PlayersMap.begin(); itr != eventData->PlayersMap.end(); itr++)
    {
        if (Player* pl = ObjectAccessor::FindPlayer(itr->first))
        {
            if (!pl->InEvent())
                continue;

            if (faction == 0 || faction == pl->GetEventFaction())
            {
                if (pl->IsAlive() && onlydead)
                    continue;

                if (WorldSession* ss = pl->GetSession())
                    pl->GetSession()->SendPacket(&data);
            }
        }
    }
}

bool CustomEventMgr::IsInAltedVec(uint32 GuidLow, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return false;

    auto Itr = std::find(eventData->AltedVec.begin(), eventData->AltedVec.end(), GuidLow);
    return (Itr != eventData->AltedVec.end());
}

void CustomEventMgr::GetKillInfo(uint32 faction, uint32& Kills, uint32& Killeds, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
    {
        Kills = 0;
        Killeds = 0;
        return;
    }

    auto itr = eventData->KillInfoMap.find(faction);
    if (itr != eventData->KillInfoMap.end())
    {
        Kills = itr->second.Kills;
        Killeds = itr->second.Killeds;
    }
    else
    {
        Kills = 0;
        Killeds = 0;
    }
}

void CustomEventMgr::UpdateKillInfo(Player* player, bool kill, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return;

    uint32 faction = player->GetEventFaction();
    auto itr = eventData->KillInfoMap.find(faction);
    if (itr == eventData->KillInfoMap.end())
    {
        EventKillInfoTemplate temp;
        temp.Kills = 0;
        temp.Killeds = 0;
        eventData->KillInfoMap[faction] = temp;
        itr = eventData->KillInfoMap.find(faction);
    }

    if (kill)
        itr->second.Kills++;
    else
        itr->second.Killeds++;
}

GroupTypes CustomEventMgr::GetGType(uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return C_GT_RANDOM_PLAYER;

    return eventData->GroupType;
}

class EventPlayerScript : PlayerScript
{
public:
    EventPlayerScript() : PlayerScript("EventPlayerScript") {}

    void OnCreatureKill(Player* player, Creature* creature) override
    {
        // 遍历所有活动数据，调用相应的方法
        sEvent->UpdateActive(creature, false, 0);
        sEvent->UpdatePhase(creature, false, 0);
        sEvent->UpdateWorldState(creature, false, 0);
        sEvent->Process(creature, 0);
    }

    void OnUseGameObject(Player* player, GameObject* gob) override
    {
        // 遍历所有活动数据，调用相应的方法
        sEvent->UpdateActive(gob, false, 0);
        sEvent->UpdatePhase(gob, false, 0);
        sEvent->UpdateWorldState(gob, false, 0);
        sEvent->Process(gob, 0);
    }

    void OnPVPKill(Player* killer, Player* killed) override
    {
        killer->UpdateEventKills(killed);
        killed->UpdateEventKilleds(killer);

        // 遍历所有活动数据，调用相应的方法
        sEvent->UpdateActive(killer, true, 0);
        sEvent->UpdateActive(killed, false, 0);
        sEvent->UpdatePhase(killer, true, 0);
        sEvent->UpdatePhase(killed, false, 0);
        sEvent->UpdateWorldState(killer, true, 0);
        sEvent->UpdateWorldState(killed, false, 0);
        sEvent->Process(killer, 0);
    }

    void OnUpdateZone(Player* player, uint32 newZone, uint32 newArea) override
    {
        if (!player->InEvent())
            return;

        // 获取玩家所在活动的eventId
        uint32 eventId = player->GetEventData().EventId;
        int32 zone = sEvent->GetZoneId(eventId);
        if (zone == newZone || zone == newArea)
            return;

        player->EventRest();
    }

    void OnMapChanged(Player* player) override
    {
        if (!player->InEvent())
            return;

        // 获取玩家所在活动的eventId
        uint32 eventId = player->GetEventData().EventId;
        int32 Map = sEvent->GetMapId(eventId);
        if (Map == player->GetMapId())
            return;

        player->EventRest();
    }
};

class NpcEventDelay : public CreatureScript
{
public:
    NpcEventDelay() : CreatureScript("NpcEventDelay") { }

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

        bool On = false;
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "事件活动专员", 0, 0);

        uint8 N = 0;
        for (auto Itr = CustomGEventStore.begin(); Itr != CustomGEventStore.end(); Itr++)
        {
            N++;
            if (N > 30)
                continue;

            bool IsActive = sGameEventMgr->IsActiveEvent(Itr->first);

            std::ostringstream Oss;
            Oss << "\n事件活动 >> " << Itr->second;
            if (IsActive)
                Oss << "\n|cFF0000FF[已激活 距离结束 - ";
            else
                Oss << "\n|cFF0000FF[未激活 距离开始 - ";

            Oss << sHelper->SecTimeString(sGameEventMgr->NextCheck(Itr->first));

            if (IsActive)
                Oss << "] 点击进入|r";

            AddGossipItemFor(player, GOSSIP_ICON_TALK, Oss.str(), IsActive ? AtEvent : As, Itr->first);
            On = true;
        }

        if (!On)
        {
            player->SendAsMsg("无活动数据......");
            CloseGossipMenuFor(player);
            return true;
        }

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

        if (Sender != AtEvent)
        {
            OnGossipHello(player, creature);
            return true;
        }

        if (!sGameEventMgr->IsActiveEvent(Action))
        {
            player->SendAsMsg("该活动已经结束");
            return true;
        }

        if (Action <= MINEVENTSO)
        {
            player->SendSysMsg("内部错误 请重新进入即可");
            return true;
        }

        sEvent->StartPlayerInEvent(player, Action);
        return true;
    }
};

class OnMgrGameEventScript : public GameEventScript
{
public:
    OnMgrGameEventScript() : GameEventScript("OnMgrGameEventScript") { }

    void OnStart(uint16 DoEventId) override
    {
        sEvent->Start(DoEventId);
    }

    void OnStop(uint16 DoEventId) override
    {
        sEvent->Stop(DoEventId);
    }
};

bool CustomEventMgr::Valid(uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
        return false;

    return eventData->EventType != C_ET_NONE && eventData->Map != nullptr;
}

void CustomEventMgr::StartPlayerInEvent(Player* player, uint32 eventId)
{
    EventData* eventData = GetEventData(eventId);
    if (!eventData)
    {
        player->SendSysMsg("活动数据不存在");
        return;
    }

    switch (eventData->EventType)
    {
    case C_ET_NONE:
        break;
    case C_ET_WS:
        player->FixedBGJoin(BATTLEGROUND_WS);
        break;
    case C_ET_AB:
        player->FixedBGJoin(BATTLEGROUND_AB);
        break;
    case C_ET_EY:
        player->FixedBGJoin(BATTLEGROUND_EY);
        break;
    case C_ET_SA:
        player->FixedBGJoin(BATTLEGROUND_SA);
        break;
    case C_ET_AV:
        player->FixedBGJoin(BATTLEGROUND_AV);
        break;
    case C_ET_IC:
        player->FixedBGJoin(BATTLEGROUND_IC);
        break;
    case C_ET_TELE:
        Tele(player, eventData->StartPosMap[uint32(player->GetTeamId())]);
        break;
    case C_ET_LINE:
    case C_ET_GUILD:
        Join(player, eventId);
        break;
    }
}

class OnCreKillToCreOrDieScript : public UnitScript
{
public:
    OnCreKillToCreOrDieScript() : UnitScript("OnCreKillToCreOrDieScript") {}

    void OnCreKillToCreOrDie(Unit* Killer, Unit* Killed) override
    {
        // 遍历所有活动数据，调用Process方法
        sEvent->Process(Killed, 0);
    }
};

class CustomEventAllMapScript : public AllMapScript
{
public:
    CustomEventAllMapScript() : AllMapScript("CustomAllMapScript") { }

    void OnMapUpdate(Map* NowMap, uint32 Diff) override
    {
        if (!NowMap)
            return;

        sEvent->Update(NowMap, Diff);
    }
};

void AddSC_CustomEventScript()
{
    new EventPlayerScript();
    new NpcEventDelay();
    new OnMgrGameEventScript();
    new OnCreKillToCreOrDieScript();
    new CustomEventAllMapScript();
}
