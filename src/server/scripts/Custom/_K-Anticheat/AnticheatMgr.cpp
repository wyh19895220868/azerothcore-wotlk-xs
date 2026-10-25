
#include "AnticheatMgr.h"
#include "Log.h"
#include "MapMgr.h"
#include "Player.h"
#include "Configuration/Config.h"
#include "SpellAuras.h"
#include "DatabaseEnv.h"
#include <Custom/_Helper/Helper.h>
#include <GameTime.h>

#pragma execution_character_set("utf-8")
#include "BanMgr.h"

constexpr auto ALLOWED_ACK_LAG = 2000;

enum Spells : uint32
{
    BLINK = 1953,
    BLINK_COOLDOWN_REDUCTION = 23025,
    GLYPH_OF_BLINK = 56365,
    SHADOWSTEP = 36554,
    FILTHY_TRICKS_RANK_1 = 58414,
    FILTHY_TRICKS_RANK_2 = 58415,
    SHACKLES = 38505,
    LFG_SPELL_DUNGEON_DESERTER = 71041,
    BG_SPELL_DESERTER = 26013,
    SILENCED = 23207,
    RESURRECTION_SICKNESS = 15007,
    SLOWDOWN = 61458
};

AnticheatMgr::AnticheatMgr()
{
    _opackorders =
    {
        { SMSG_FORCE_WALK_SPEED_CHANGE, CMSG_FORCE_WALK_SPEED_CHANGE_ACK },
        { SMSG_FORCE_RUN_SPEED_CHANGE, CMSG_FORCE_RUN_SPEED_CHANGE_ACK },
        { SMSG_FORCE_RUN_BACK_SPEED_CHANGE, CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK },
        { SMSG_FORCE_SWIM_SPEED_CHANGE, CMSG_FORCE_SWIM_SPEED_CHANGE_ACK },
        { SMSG_FORCE_SWIM_BACK_SPEED_CHANGE, CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK },
        { SMSG_FORCE_TURN_RATE_CHANGE, CMSG_FORCE_TURN_RATE_CHANGE_ACK },
        { SMSG_FORCE_PITCH_RATE_CHANGE, CMSG_FORCE_PITCH_RATE_CHANGE_ACK },
        { SMSG_FORCE_FLIGHT_SPEED_CHANGE, CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK },
        { SMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE, CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK },
        { SMSG_FORCE_MOVE_ROOT, CMSG_FORCE_MOVE_ROOT_ACK },
        { SMSG_FORCE_MOVE_UNROOT, CMSG_FORCE_MOVE_UNROOT_ACK },
        { SMSG_MOVE_KNOCK_BACK, CMSG_MOVE_KNOCK_BACK_ACK },
        { SMSG_MOVE_FEATHER_FALL, SMSG_MOVE_NORMAL_FALL, CMSG_MOVE_FEATHER_FALL_ACK },
        { SMSG_MOVE_SET_HOVER, SMSG_MOVE_UNSET_HOVER, CMSG_MOVE_HOVER_ACK },
        { SMSG_MOVE_SET_CAN_FLY, SMSG_MOVE_UNSET_CAN_FLY, CMSG_MOVE_SET_CAN_FLY_ACK },
        { SMSG_MOVE_WATER_WALK, SMSG_MOVE_LAND_WALK, CMSG_MOVE_WATER_WALK_ACK },
        { SMSG_MOVE_SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY, SMSG_MOVE_UNSET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY, CMSG_MOVE_SET_CAN_TRANSITION_BETWEEN_SWIM_AND_FLY_ACK },
        { SMSG_MOVE_GRAVITY_ENABLE, CMSG_MOVE_GRAVITY_ENABLE_ACK },
        { SMSG_MOVE_GRAVITY_DISABLE, CMSG_MOVE_GRAVITY_DISABLE_ACK },
        { SMSG_MOVE_SET_COLLISION_HGT, CMSG_MOVE_SET_COLLISION_HGT_ACK }
    };
}

AnticheatMgr::~AnticheatMgr()
{
    m_Players.clear();
}

bool AnticheatMgr::StartHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode)
{
    if (!sHelper->GetBoolean(2040))
        return false;

    if (player->IsGameMaster())
        return false;

    // 检查是否忽略当前地图
    if (sHelper->IsIgnoreMapCheck(player->GetMapId()))
        return false;

    // 检查是否忽略当前区域（同时检查 AreaId 和 ZoneId）
    // 某些情况下 GetAreaId() 返回子区域ID，GetZoneId() 返回父区域ID
    if (sHelper->IsIgnoreAreaCheck(player->GetAreaId()) || sHelper->IsIgnoreAreaCheck(player->GetZoneId()))
        return false;

    if (sHelper->IsIgnoreAccountId(player->GetSession()->GetAccountId()))
        return false;

    ObjectGuid key = player->GetGUID();

    if (player->IsInFlight() || player->GetTransport())
    {
        m_Players[key].SetLastInformations(movementInfo, opcode, player->GetMapId(), GetPlayerCurrentSpeedRate(player));
        return false;
    }

    if (m_Players[key].GetSkipNextPacket())
    {
        m_Players[key].SetLastInformations(movementInfo, opcode, player->GetMapId(), GetPlayerCurrentSpeedRate(player));
        m_Players[key].SetSkipNextPacket(false);
        return false;
    }

    bool IsCheck = false;

    // 先检测瞬移，如果检测到瞬移，跳过加速检测（瞬移本身就是一种快速移动）
    if (TeleportHackDetection(player, movementInfo))
    {
        IsCheck = true;
        // 瞬移检测到后，跳过加速检测（瞬移包含了快速移动的特征）
    }
    else if (SpeedHackDetection(player, movementInfo))
    {
        IsCheck = true;
    }
    if (FlyHackDetection(player, movementInfo))
        IsCheck = true;
    if (JumpHackDetection(player, movementInfo, opcode))
        IsCheck = true;
    if (TeleportPlaneHackDetection(player, movementInfo, opcode))
        IsCheck = true;
    if (ClimbHackDetection(player, movementInfo, opcode))
        IsCheck = true;
    if (IgnoreControlHackDetection(player, movementInfo, opcode))
        IsCheck = true;
    if (GravityHackDetection(player, movementInfo))
        IsCheck = true;
    if (player->GetLiquidData().Status == LIQUID_MAP_WATER_WALK)
    {
        if (WalkOnWaterHackDetection(player, movementInfo))
            IsCheck = true;
    }
    else
    {
        if (ZAxisHackDetection(player, movementInfo))
            IsCheck = true;
    }
    if (player->GetLiquidData().Status == LIQUID_MAP_UNDER_WATER)
    {
        if (AntiSwimHackDetection(player, movementInfo, opcode))
            IsCheck = true;
    }
    if (AntiKnockBackHackDetection(player, movementInfo))
        IsCheck = true;
    if (NoFallDamageDetection(player, movementInfo))
        IsCheck = true;
    m_Players[key].SetLastInformations(movementInfo, opcode, player->GetMapId(), GetPlayerCurrentSpeedRate(player));
    return IsCheck;
}

const char* AnticheatMgr::GetReportNameFromReportType(ReportTypes reportType)
{
    switch (reportType)
    {
        case SPEED_HACK_REPORT:
            return "加速";
        case FLY_HACK_REPORT:
            return "飞行";
        case WALK_WATER_HACK_REPORT:
            return "水上行走";
        case JUMP_HACK_REPORT:
            return "跳跃";
        case TELEPORT_PLANE_HACK_REPORT:
            return "传送";
        case CLIMB_HACK_REPORT:
            return "攀爬";
        case TELEPORT_HACK_REPORT:
            return "瞬移";
        case IGNORE_CONTROL_REPORT:
            return "忽略地形控制";
        case ZAXIS_HACK_REPORT:
            return "无限攀爬";
        case ANTISWIM_HACK_REPORT:
            return "反游泳";
        case GRAVITY_HACK_REPORT:
            return "反重力";
        case ANTIKNOCK_BACK_HACK_REPORT:
            return "反碰撞";
        case NO_FALL_DAMAGE_HACK_REPORT:
            return "无坠落伤害";
        case OP_ACK_HACK_REPORT:
            return "ACK攻击";
        case COUNTER_MEASURES_REPORT:
            return "反时间差";
        default:
            return "未知";
    }
}

uint32 AnticheatMgr::GetAlertFrequencyConfigFromReportType(ReportTypes reportType)
{
    switch (reportType)
    {
        case SPEED_HACK_REPORT:
            return std::max(1u, 5u);
        case FLY_HACK_REPORT:
            return std::max(1u, 5u);
        case WALK_WATER_HACK_REPORT:
            return std::max(1u, 5u);
        case JUMP_HACK_REPORT:
            return std::max(1u, 5u);
        case TELEPORT_PLANE_HACK_REPORT:
            return std::max(1u, 60u);
        case CLIMB_HACK_REPORT:
            return std::max(1u, 5u);
        case TELEPORT_HACK_REPORT:
            return std::max(1u, 60u);
        case IGNORE_CONTROL_REPORT:
            return std::max(1u, 5u);
        case ZAXIS_HACK_REPORT:
            return std::max(1u, 5u);
        case ANTISWIM_HACK_REPORT:
            return std::max(1u, 5u);
        case GRAVITY_HACK_REPORT:
            return std::max(1u, 5u);
        case ANTIKNOCK_BACK_HACK_REPORT:
            return std::max(1u, 1u);
        case NO_FALL_DAMAGE_HACK_REPORT:
            return std::max(1u, 1u);
        case OP_ACK_HACK_REPORT:
            return std::max(1u, 1u);
        case COUNTER_MEASURES_REPORT:
            return std::max(1u, 60u);
        default:
            return 1;
    }
}

uint32 AnticheatMgr::GetMinimumReportInChatThresholdConfigFromReportType(ReportTypes reportType)
{
    switch (reportType)
    {
        case SPEED_HACK_REPORT:
            return std::max(1u, 50u);
        case FLY_HACK_REPORT:
            return std::max(1u, 50u);
        case WALK_WATER_HACK_REPORT:
            return std::max(1u, 50u);
        case JUMP_HACK_REPORT:
            return std::max(1u, 50u);
        case TELEPORT_PLANE_HACK_REPORT:
            return std::max(1u, 50u);
        case CLIMB_HACK_REPORT:
            return std::max(1u, 50u);
        case TELEPORT_HACK_REPORT:
            return std::max(1u, 1u);
        case IGNORE_CONTROL_REPORT:
            return std::max(1u, 50u);
        case ZAXIS_HACK_REPORT:
            return std::max(1u, 50u);
        case ANTISWIM_HACK_REPORT:
            return std::max(1u, 50u);
        case GRAVITY_HACK_REPORT:
            return std::max(1u, 50u);
        case ANTIKNOCK_BACK_HACK_REPORT:
            return std::max(1u, 50u);
        case NO_FALL_DAMAGE_HACK_REPORT:
            return std::max(1u, 1u);
        case OP_ACK_HACK_REPORT:
            return std::max(1u, 1u);
        case COUNTER_MEASURES_REPORT:
            return std::max(1u, 50u);
        default:
            return 1;
    }
}

uint32 AnticheatMgr::GetMaximumReportInChatThresholdConfigFromReportType(ReportTypes reportType)
{
    switch (reportType)
    {
        case SPEED_HACK_REPORT:
            return std::max(1u, 60u);
        case FLY_HACK_REPORT:
            return std::max(1u, 60u);
        case WALK_WATER_HACK_REPORT:
            return std::max(1u, 60u);
        case JUMP_HACK_REPORT:
            return std::max(1u, 60u);
        case TELEPORT_PLANE_HACK_REPORT:
            return std::max(1u, 60u);
        case CLIMB_HACK_REPORT:
            return std::max(1u, 60u);
        case TELEPORT_HACK_REPORT:
            return std::max(1u, 60u);
        case IGNORE_CONTROL_REPORT:
            return std::max(1u, 60u);
        case ZAXIS_HACK_REPORT:
            return std::max(1u, 60u);
        case ANTISWIM_HACK_REPORT:
            return std::max(1u, 60u);
        case GRAVITY_HACK_REPORT:
            return std::max(1u, 60u);
        case ANTIKNOCK_BACK_HACK_REPORT:
            return std::max(1u, 60u);
        case NO_FALL_DAMAGE_HACK_REPORT:
            return std::max(1u, 60u);
        case OP_ACK_HACK_REPORT:
            return std::max(1u, 60u);
        case COUNTER_MEASURES_REPORT:
            return std::max(1u, 60u);
        default:
            return 80;
    }
}

uint32 AnticheatMgr::GetTeleportSkillCooldownDurationInMS(Player* player) const
{
    switch (player->getClass())
    {
        case CLASS_ROGUE:
            if (player->HasAura(FILTHY_TRICKS_RANK_2))
                return 20000u;
            else if (player->HasAura(FILTHY_TRICKS_RANK_1))
                return 25000u;
            return 30000u;
        case CLASS_MAGE:
            if (player->HasAura(BLINK_COOLDOWN_REDUCTION))
                return 13000u;
            return 15000u;
        default:
            return 0u;
    }
}

float AnticheatMgr::GetTeleportSkillDistanceInYards(Player* player) const
{
    switch (player->getClass())
    {
        case CLASS_ROGUE:
            return 25.0f;
        case CLASS_MAGE:
            if (player->HasAura(GLYPH_OF_BLINK))
                return 25.1f;
            return 20.1f;
        default:
            return 0.0f;
    }
}

float AnticheatMgr::GetPlayerCurrentSpeedRate(Player* player) const
{
    if (player->HasUnitMovementFlag(MOVEMENTFLAG_SWIMMING))
        return player->GetSpeed(MOVE_SWIM);
    else if (player->IsFlying())
        return player->GetSpeed(MOVE_FLIGHT);
    else if (player->HasUnitMovementFlag(MOVEMENTFLAG_WALKING))
        return player->GetSpeed(MOVE_WALK);
    return player->GetSpeed(MOVE_RUN);
}

bool AnticheatMgr::SpeedHackDetection(Player* player, MovementInfo movementInfo)
{
    if (!sHelper->GetBoolean(2051))
        return false;

    ObjectGuid key = player->GetGUID();

    // 如果玩家被标记为可以传送，跳过检测
    if (player->CanTeleport() || player->IsBeingTeleported())
        return false;

    if (m_Players[key].GetLastMapId() != player->GetMapId())
        return false;

    if (m_Players[key].GetLastMovementInfo().HasMovementFlag(MOVEMENTFLAG_ONTRANSPORT))
    {
        switch (player->GetMapId())
        {
            case 369:
            case 607:
            case 582:
            case 584:
            case 586:
            case 587:
            case 588:
            case 589:
            case 590:
            case 591:
            case 592:
            case 593:
            case 594:
            case 596:
            case 610:
            case 612:
            case 613:
            case 614:
            case 620:
            case 621:
            case 622:
            case 623:
            case 641:
            case 642:
            case 647:
            case 672:
            case 673:
            case 712:
            case 713:
            case 718:
                return false;
        }
    }

    float distance2D = movementInfo.pos.GetExactDist2d(&m_Players[key].GetLastMovementInfo().pos);

    if (!distance2D)
        return false;

    uint32 timeDiff = getMSTimeDiff(m_Players[key].GetLastMovementInfo().time, movementInfo.time);

    float speedRate = GetPlayerCurrentSpeedRate(player);
    if (timeDiff <= ALLOWED_ACK_LAG)
        speedRate = std::max(speedRate, m_Players[key].GetLastSpeedRate());

    if (int32(timeDiff) < 0)
    {
        timeDiff = 1;
        BuildReport(player, COUNTER_MEASURES_REPORT, movementInfo);
        return true;
    }

    if (!timeDiff)
    {
        timeDiff = 1;
        BuildReport(player, COUNTER_MEASURES_REPORT, movementInfo);
        return true;
    }

    // 检查各种传送/加速技能，避免误判
    if (player->HasAura(BLINK) || player->HasAura(SHADOWSTEP))
    {
        if (!m_Players[key].GetJustUsedMovementSpell() || timeDiff >= GetTeleportSkillCooldownDurationInMS(player))
        {
            m_Players[key].SetJustUsedMovementSpell(true);
            distance2D = std::max(distance2D - GetTeleportSkillDistanceInYards(player), 0.0f);
        }
    }
    else
    {
        m_Players[key].SetJustUsedMovementSpell(false);
    }

    // 注意：坐骑上也应该检测加速作弊，不再跳过

    float clientSpeedRate = 0.0f;
    if (float floatTimeDiff = float(timeDiff))
        clientSpeedRate = distance2D * 1000.0f / floatTimeDiff;

    float diffspeed = clientSpeedRate - speedRate;
    float assignedspeeddiff = 0.0f;

    // 加速检测条件：
    // 1. 客户端速度 > 服务器速度 * 配置值（默认1.5）
    // 2. 不在下落状态
    // 3. 时间差至少100ms（避免瞬时误差）
    if ((diffspeed >= assignedspeeddiff) && !m_Players[key].GetLastMovementInfo().HasMovementFlag(MOVEMENTFLAG_FALLING))
    {
        // 使用配置值 2058 作为加速检测阈值倍数，默认1.5倍
        float speedThreshold = sHelper->GetFloat(2058);
        if (speedThreshold <= 1.0f)
            speedThreshold = 1.5f;

        // 降低时间差要求到100ms，确保能检测到高速移动
        // 同时增加距离判断：至少移动了一定距离才检测（避免原地抖动）
        if (clientSpeedRate > speedRate * speedThreshold && timeDiff > 100 && distance2D > 1.0f)
        {
            if (!player->CanTeleport())
            {
                if (sHelper->GetBoolean(2048))
                {
                    player->TeleportTo(m_Players[key].GetLastMapId(), m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation());
                }

                BuildReport(player, SPEED_HACK_REPORT, movementInfo);
                m_Players[key].ClearTempReports(SPEED_HACK_REPORT);
                return true;
            }
        }
    }

    return false;
}

bool AnticheatMgr::FlyHackDetection(Player* player, MovementInfo  movementInfo)
{
    if (!sHelper->GetBoolean(2053))
        return false;

    // 放宽飞行光环检测，支持更多类型的飞行光环（包括自定义飞行道具）
    if (player->HasAuraType(SPELL_AURA_FLY) ||
        player->HasAuraType(SPELL_AURA_MOD_INCREASE_VEHICLE_FLIGHT_SPEED) ||
        player->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) ||
        player->HasAuraType(SPELL_AURA_MOD_INCREASE_FLIGHT_SPEED) ||
        player->HasAuraType(SPELL_AURA_MOD_MOUNTED_FLIGHT_SPEED_ALWAYS) ||
        player->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_NOT_STACK))
        return false;

    // 如果在水中或正在使用传送，跳过检测
    if (player->IsInWater() || player->CanTeleport() || player->IsBeingTeleported())
        return false;

    // 检查飞行标志，但增加更多排除条件
    if (!movementInfo.HasMovementFlag(MOVEMENTFLAG_CAN_FLY) && !movementInfo.HasMovementFlag(MOVEMENTFLAG_FLYING))
        return false;

    // 如果正在上升或下降（正常飞行状态），不判定为外挂
    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_ASCENDING | MOVEMENTFLAG_DESCENDING))
        return false;

    // 如果玩家有缓落/漂浮效果，跳过检测
    if (player->HasAuraType(SPELL_AURA_FEATHER_FALL) ||
        player->HasAuraType(SPELL_AURA_HOVER) ||
        player->HasAuraType(SPELL_AURA_SAFE_FALL))
        return false;

    ObjectGuid key = player->GetGUID();

    // 主动模式：取消飞行并传送回地面
    if (sHelper->GetBoolean(2048))
    {
        WorldPacket cheater(12);
        cheater.SetOpcode(SMSG_MOVE_UNSET_CAN_FLY);
        cheater << player->GetPackGUID();
        cheater << uint32(0);
        player->SendMessageToSet(&cheater, true);

        // 传送到地面，避免摔死
        // 使用 player's GetFloorZ 获取更可靠的地面高度
        float floorZ = player->GetFloorZ();
        float posZ = player->GetPositionZ();

        // 如果 floorZ 有效且低于当前位置，传送到地面
        if (floorZ > 0 && floorZ < posZ)
        {
            player->SetCanTeleport(true);
            player->TeleportTo(player->GetMapId(), player->GetPositionX(), player->GetPositionY(), floorZ + 0.5f, player->GetOrientation());
            // 更新位置记录，防止重复检测
            m_Players[key].SetPosition(player->GetPositionX(), player->GetPositionY(), floorZ + 0.5f, player->GetOrientation(), player->GetMapId());
        }
        else
        {
            // 如果无法获取有效地面高度，传送到上一个记录的安全位置
            player->SetCanTeleport(true);
            player->TeleportTo(m_Players[key].GetLastMapId(), m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation());
            // 更新位置记录
            m_Players[key].SetPosition(m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation(), m_Players[key].GetLastMapId());
        }
    }

    BuildReport(player, FLY_HACK_REPORT, movementInfo);
    m_Players[key].ClearTempReports(FLY_HACK_REPORT);
    return true;
}

bool AnticheatMgr::JumpHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode)
{
    if (!sHelper->GetBoolean(2055))
        return false;

    ObjectGuid key = player->GetGUID();

    // 如果玩家被标记为可以传送，跳过检测
    if (player->CanTeleport() || player->IsBeingTeleported())
        return false;

    const float ground_Z = movementInfo.pos.GetPositionZ() - player->GetMapHeight(movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY(), movementInfo.pos.GetPositionZ());

    const bool no_fly_auras = !(player->HasAuraType(SPELL_AURA_FLY) || player->HasAuraType(SPELL_AURA_MOD_INCREASE_VEHICLE_FLIGHT_SPEED)
        || player->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) || player->HasAuraType(SPELL_AURA_MOD_INCREASE_FLIGHT_SPEED)
        || player->HasAuraType(SPELL_AURA_MOD_MOUNTED_FLIGHT_SPEED_ALWAYS));
    const bool no_fly_flags = ((movementInfo.flags & (MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_FLYING)) == 0);
    const bool no_swim_in_water = !player->IsInWater();
    const bool no_swim_above_water = movementInfo.pos.GetPositionZ() - 7.0f >= player->GetMap()->GetWaterLevel(movementInfo.pos.GetPositionX(), movementInfo.pos.GetPositionY());
    const bool no_swim_water = no_swim_in_water && no_swim_above_water;

    if (m_Players[key].GetLastOpcode() == MSG_MOVE_JUMP && opcode == MSG_MOVE_JUMP)
    {
        if (sHelper->GetBoolean(2048))
        {
            player->SetCanTeleport(true);
            player->GetMotionMaster()->MoveFall();
            player->TeleportTo(m_Players[key].GetLastMapId(), m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation());
            // 更新位置记录，防止重复检测
            m_Players[key].SetPosition(m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation(), m_Players[key].GetLastMapId());
        }
        BuildReport(player, JUMP_HACK_REPORT, movementInfo);
        m_Players[key].ClearTempReports(JUMP_HACK_REPORT);
        return true;
    }
    else if (no_fly_auras && no_fly_flags && no_swim_water)
    {
        if (player->GetAreaId())
        {
            switch (player->GetAreaId())
            {
            case 4273:
                return false;
            }
        }

        if (m_Players[key].GetLastOpcode() == MSG_MOVE_JUMP && !player->IsFalling())
            return false;

        if (!movementInfo.pos.GetExactDist2d(&m_Players[key].GetLastMovementInfo().pos))
            return false;

        if (!player->HasUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY) && movementInfo.jump.zspeed < -10.0f)
            return false;

        if (player->HasAuraType(SPELL_AURA_WATER_WALK) || player->HasAuraType(SPELL_AURA_FEATHER_FALL) ||
            player->HasAuraType(SPELL_AURA_SAFE_FALL))
        {
            return false;
        }

        if (ground_Z > 5.0f && movementInfo.pos.GetPositionZ() >= player->GetPositionZ())
        {
            if (sHelper->GetBoolean(2048))
            {
                player->SetCanTeleport(true);
                player->GetMotionMaster()->MoveFall();
                player->TeleportTo(m_Players[key].GetLastMapId(), m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation());
                // 更新位置记录，防止重复检测
                m_Players[key].SetPosition(m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation(), m_Players[key].GetLastMapId());
            }
            BuildReport(player, JUMP_HACK_REPORT, movementInfo);
            m_Players[key].ClearTempReports(JUMP_HACK_REPORT);
            return true;
        }
    }

    return false;
}

bool AnticheatMgr::TeleportPlaneHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode)
{
    if (!sHelper->GetBoolean(2054))
        return false;

    // 如果玩家被标记为可以传送，跳过检测
    if (player->CanTeleport() || player->IsBeingTeleported())
        return false;

    if (player->GetAreaId())
    {
        switch (player->GetAreaId())
        {
            case 4273:
                return false;
        }
    }

    if (player->HasAuraType(SPELL_AURA_WATER_WALK))
        return false;

    if (player->HasAuraType(SPELL_AURA_WATER_BREATHING))
        return false;

    if (player->HasAuraType(SPELL_AURA_GHOST))
        return false;

    ObjectGuid key = player->GetGUID();

    if (!movementInfo.pos.GetExactDist2d(&m_Players[key].GetLastMovementInfo().pos))
        return false;

    if (m_Players[key].GetLastOpcode() == MSG_MOVE_JUMP)
        return false;

    if (opcode == (MSG_MOVE_FALL_LAND))
        return false;

    if (player->GetLiquidData().Status == LIQUID_MAP_ABOVE_WATER)
        return false;

    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_SWIMMING))
        return false;

    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_FLYING))
        return false;

    float pos_z = player->GetPositionZ();
    float ground_Z = player->GetFloorZ();
    float groundZ = player->GetMapHeight(player->GetPositionX(), player->GetPositionY(), MAX_HEIGHT);
    float floorZ = player->GetMapHeight(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());

    if (groundZ == floorZ && (fabs(ground_Z - pos_z) > 2.0f || fabs(ground_Z - pos_z) < -1.0f))
    {
        if (sHelper->GetBoolean(2048))
        {
            player->SetCanTeleport(true);
            player->TeleportTo(m_Players[key].GetLastMapId(), m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation());
            // 更新位置记录，防止重复检测
            m_Players[key].SetPosition(m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation(), m_Players[key].GetLastMapId());
        }

        BuildReport(player, TELEPORT_PLANE_HACK_REPORT, movementInfo);
        m_Players[key].ClearTempReports(TELEPORT_PLANE_HACK_REPORT);
        return true;
    }

    return false;
}

bool AnticheatMgr::ClimbHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode)
{
    if (player->IsInWater() ||
        player->IsFlying() ||
        player->IsFalling())
        return false;

    if (opcode == MSG_MOVE_JUMP)
        return false;

    if (player->HasUnitMovementFlag(MOVEMENTFLAG_FALLING))
        return false;

    Position playerPos = player->GetPosition();

    float diffz = fabs(movementInfo.pos.GetPositionZ() - playerPos.GetPositionZ());
    float tanangle = movementInfo.pos.GetExactDist2d(&playerPos) / diffz;

    if (!player->HasUnitMovementFlag(MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_SWIMMING))
    {
        if (movementInfo.pos.GetPositionZ() > playerPos.GetPositionZ() && diffz > 1.87f && tanangle < 0.57735026919f) // 30 degrees
        {
            BuildReport(player, CLIMB_HACK_REPORT, movementInfo);
            return true;
        }
    }

    return false;
}

bool AnticheatMgr::TeleportHackDetection(Player* player, MovementInfo movementInfo)
{
    if (!sHelper->GetBoolean(2052))
        return false;

    ObjectGuid key = player->GetGUID();

    if (player->HasAuraType(SPELL_AURA_FLY) || player->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) || player->HasAuraType(SPELL_AURA_MOD_INCREASE_FLIGHT_SPEED))
        return false;

    if (m_Players[key].GetLastOpcode() == MSG_DELAY_GHOST_TELEPORT)
        return false;

    // 如果玩家被标记为可以传送，跳过检测
    if (player->CanTeleport() || player->IsBeingTeleported())
    {
        if (player->CanTeleport())
            player->SetCanTeleport(false);
        return false;
    }

    float lastX = m_Players[key].GetLastMovementInfo().pos.GetPositionX();
    float newX = movementInfo.pos.GetPositionX();

    float lastY = m_Players[key].GetLastMovementInfo().pos.GetPositionY();
    float newY = movementInfo.pos.GetPositionY();

    float lastZ = m_Players[key].GetLastMovementInfo().pos.GetPositionZ();

    float xDiff = fabs(lastX - newX);
    float yDiff = fabs(lastY - newY);

    // 注意：坐骑上也应该检测瞬移作弊，不再跳过

    if (m_Players[key].GetLastMovementInfo().HasMovementFlag(MOVEMENTFLAG_ONTRANSPORT))
    {
        switch (player->GetMapId())
        {
            case 369:
            case 607:
            case 582:
            case 584:
            case 586:
            case 587:
            case 588:
            case 589:
            case 590:
            case 591:
            case 592:
            case 593:
            case 594:
            case 596:
            case 610:
            case 612:
            case 613:
            case 614:
            case 620:
            case 621:
            case 622:
            case 623:
            case 641:
            case 642:
            case 647:
            case 672:
            case 673:
            case 712:
            case 713:
            case 718:
                return false;
        }
    }

    // 瞬移检测：两种情况都认为是瞬移
    // 1. 极短时间内的大幅度移动（timeDiff < 100ms）
    // 2. 速度异常高的移动（超过瞬移速度阈值）
    uint32 timeDiff = movementInfo.time - m_Players[key].GetLastMovementInfo().time;

    // 计算实际移动速度（码/秒）
    float actualSpeed = 0.0f;
    if (timeDiff > 0)
    {
        float totalDiff = sqrt(xDiff * xDiff + yDiff * yDiff);
        actualSpeed = totalDiff / (timeDiff / 1000.0f);
    }

    // 瞬移检测条件（使用配置项控制）：
    // - 100ms内移动超过2047配置的距离（设为0关闭此条件），OR
    // - 速度超过2049配置的瞬移速度阈值（设为0关闭此条件）
    // 注意：低于瞬移阈值但高于正常速度的移动会被加速检测捕获
    bool isTeleport = false;
    float teleportDistThreshold = sHelper->GetFloat(2047); // 瞬移距离阈值（码）
    float teleportSpeedThreshold = sHelper->GetFloat(2049); // 瞬移速度阈值（码/秒）

    // 支持设为0关闭检测
    // 距离阈值：设为0或负数表示关闭距离检测，只使用速度检测
    // 速度阈值：设为0或负数表示关闭速度检测，只使用距离检测
    // 两者都设为0表示关闭瞬移检测（所有高速移动都判定为加速）

    bool checkDistance = (teleportDistThreshold > 0);
    bool checkSpeed = (teleportSpeedThreshold > 0);

    // 检测逻辑
    if (checkDistance && timeDiff < 100 && (xDiff >= teleportDistThreshold || yDiff >= teleportDistThreshold))
    {
        isTeleport = true;
    }
    else if (checkSpeed && timeDiff > 0 && actualSpeed > teleportSpeedThreshold)
    {
        isTeleport = true;
    }

    if (isTeleport)
    {
        if (sHelper->GetBoolean(2048))
        {
            // 传送前设置标志，防止重复检测
            player->SetCanTeleport(true);
            player->TeleportTo(m_Players[key].GetLastMapId(), m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation());
            // 更新玩家数据，记录传送后的位置
            m_Players[key].SetPosition(m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation(), m_Players[key].GetLastMapId());
        }

        BuildReport(player, TELEPORT_HACK_REPORT, movementInfo);
        m_Players[key].ClearTempReports(TELEPORT_HACK_REPORT);
        return true;
    }

    return false;
}

bool AnticheatMgr::IgnoreControlHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode)
{
    ObjectGuid key = player->GetGUID();

    if (m_Players[key].GetLastOpcode() == MSG_MOVE_JUMP)
        return false;

    if (opcode == (MSG_MOVE_FALL_LAND))
        return false;

    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_SWIMMING))
        return false;

    uint32 latency = player->GetSession()->GetLatency();
    bool hasBadLatency = latency >= 400;

    if (player->HasAuraType(SPELL_AURA_MOD_ROOT) && !player->GetVehicle() && !hasBadLatency)
    {
        float lastX = m_Players[key].GetLastMovementInfo().pos.GetPositionX();
        float newX = movementInfo.pos.GetPositionX();

        float lastY = m_Players[key].GetLastMovementInfo().pos.GetPositionY();
        float newY = movementInfo.pos.GetPositionY();

        bool unrestricted = newX != lastX || newY != lastY;
        if (unrestricted)
        {
            BuildReport(player, IGNORE_CONTROL_REPORT, movementInfo);
            return true;
        }
    }

    return false;
}

bool AnticheatMgr::GravityHackDetection(Player* player, MovementInfo movementInfo)
{
    if (!sHelper->GetBoolean(2056))
        return false;

    // 如果玩家被标记为可以传送，跳过检测
    if (player->CanTeleport() || player->IsBeingTeleported())
        return false;

    if (player->HasAuraType(SPELL_AURA_FEATHER_FALL))
        return false;

    ObjectGuid key = player->GetGUID();
    if (m_Players[key].GetLastOpcode() == MSG_MOVE_JUMP)
    {
        if (!player->HasUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY) && movementInfo.jump.zspeed < -10.0f)
        {
            if (sHelper->GetBoolean(2048))
            {
                player->SetCanTeleport(true);
                player->GetMotionMaster()->MoveFall();
                player->TeleportTo(m_Players[key].GetLastMapId(), m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation());
                // 更新位置记录，防止重复检测
                m_Players[key].SetPosition(m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation(), m_Players[key].GetLastMapId());
            }
            BuildReport(player, GRAVITY_HACK_REPORT, movementInfo);
            m_Players[key].ClearTempReports(GRAVITY_HACK_REPORT);
            return true;
        }
    }

    return false;
}

bool AnticheatMgr::WalkOnWaterHackDetection(Player* player, MovementInfo movementInfo)
{
    ObjectGuid key = player->GetGUID();

    if (!movementInfo.pos.GetExactDist2d(&m_Players[key].GetLastMovementInfo().pos))
        return false;

    if (player->GetLiquidData().Status == LIQUID_MAP_WATER_WALK && !player->IsFlying())
    {
        if (!m_Players[key].GetLastMovementInfo().HasMovementFlag(MOVEMENTFLAG_WATERWALKING) && !movementInfo.HasMovementFlag(MOVEMENTFLAG_WATERWALKING))
        {
            BuildReport(player, WALK_WATER_HACK_REPORT, movementInfo);
            return true;
        }
    }

    if (player->HasAuraType(SPELL_AURA_GHOST))
        return false;

    if (m_Players[key].GetLastOpcode() == MSG_DELAY_GHOST_TELEPORT)
        return false;

    if (m_Players[key].GetLastMovementInfo().HasMovementFlag(MOVEMENTFLAG_WATERWALKING) && movementInfo.HasMovementFlag(MOVEMENTFLAG_WATERWALKING))
    {
        if (player->HasAuraType(SPELL_AURA_WATER_WALK) || player->HasAuraType(SPELL_AURA_FEATHER_FALL) ||
            player->HasAuraType(SPELL_AURA_SAFE_FALL))
        {
            return false;
        }
    }
    else if (!m_Players[key].GetLastMovementInfo().HasMovementFlag(MOVEMENTFLAG_WATERWALKING) && !movementInfo.HasMovementFlag(MOVEMENTFLAG_WATERWALKING))
    {
        return false;
    }

    BuildReport(player, WALK_WATER_HACK_REPORT, movementInfo);
    return true;
}

bool AnticheatMgr::ZAxisHackDetection(Player* player, MovementInfo movementInfo)
{
    if (!sHelper->GetBoolean(2057))
        return false;

    // 如果玩家被标记为可以传送，跳过检测
    if (player->CanTeleport() || player->IsBeingTeleported())
        return false;

    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_CAN_FLY))
        return false;

    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_FLYING))
        return false;

    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_WATERWALKING) || player->IsInWater() || !player->IsAlive())
        return false;

    switch (player->GetAreaId())
    {
        case 10:
        case 40:
        case 321:
        case 495:
        case 721:
        case 796:
        case 3789:
        case 3847:
        case 4161:
        case 4273:
            return false;
    }

    if (player->GetLiquidData().Status == LIQUID_MAP_ABOVE_WATER)
        return false;

    ObjectGuid key = player->GetGUID();

    if (!movementInfo.pos.GetExactDist2d(&m_Players[key].GetLastMovementInfo().pos))
        return false;

    if (m_Players[key].GetLastMovementInfo().pos.GetPositionZ() == movementInfo.pos.GetPositionZ()
        && player->GetPositionZ() >= player->GetFloorZ() + 2.5f)
    {
        if (sHelper->GetBoolean(2048))
        {
            player->SetCanTeleport(true);
            player->GetMotionMaster()->MoveFall();
            player->TeleportTo(m_Players[key].GetLastMapId(), m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation());
            // 更新位置记录，防止重复检测
            m_Players[key].SetPosition(m_Players[key].GetLastMovementInfo().pos.GetPositionX(), m_Players[key].GetLastMovementInfo().pos.GetPositionY(), m_Players[key].GetLastMovementInfo().pos.GetPositionZ(), player->GetOrientation(), m_Players[key].GetLastMapId());
        }
        BuildReport(player, ZAXIS_HACK_REPORT, movementInfo);
        m_Players[key].ClearTempReports(ZAXIS_HACK_REPORT);
        return true;
    }

    return false;
}

bool AnticheatMgr::AntiSwimHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode)
{
    if (player->GetAreaId())
    {
        switch (player->GetAreaId())
        {
            case 2100:
                return false;
        }
    }

    if (player->GetLiquidData().Status == (LIQUID_MAP_ABOVE_WATER | LIQUID_MAP_WATER_WALK | LIQUID_MAP_IN_WATER))
    {
        return false;
    }

    if (opcode == MSG_MOVE_JUMP)
        return false;

    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_SWIMMING))
        return false;

    if (player->GetLiquidData().Status == LIQUID_MAP_UNDER_WATER && !movementInfo.HasMovementFlag(MOVEMENTFLAG_SWIMMING))
    {
        BuildReport(player, ANTISWIM_HACK_REPORT, movementInfo);
        return true;
    }

    return false;
}

bool AnticheatMgr::AntiKnockBackHackDetection(Player* player, MovementInfo movementInfo)
{
    if (!player->CanKnockback() || player->HasUnitState(UNIT_STATE_ROOT))
        return false;

    ObjectGuid key = player->GetGUID();

    if (movementInfo.pos == m_Players[key].GetLastMovementInfo().pos)
    {
        BuildReport(player, ANTIKNOCK_BACK_HACK_REPORT, movementInfo);
        return true;
    }
    else if (player->CanKnockback())
        player->SetCanKnockback(false);

    return false;
}

bool AnticheatMgr::NoFallDamageDetection(Player* player, MovementInfo movementInfo)
{
    if (player->HasAuraType(SPELL_AURA_GHOST))
        return false;

    if (((player->HasAuraType(SPELL_AURA_WATER_WALK) && player->GetLiquidData().Status == LIQUID_MAP_WATER_WALK && !player->IsFlying())) ||
        player->HasAuraType(SPELL_AURA_FEATHER_FALL) || player->HasAuraType(SPELL_AURA_SAFE_FALL))
    {
        return false;
    }

    ObjectGuid key = player->GetGUID();

    float lastZ = m_Players[key].GetLastMovementInfo().pos.GetPositionZ();
    float newZ = movementInfo.pos.GetPositionZ();
    float zDiff = fabs(lastZ - newZ);
    int32 safe_fall = player->GetTotalAuraModifier(SPELL_AURA_SAFE_FALL);
    float damageperc = 0.018f * (zDiff - safe_fall) - 0.2426f;
    uint32 damage = (uint32)(damageperc * player->GetMaxHealth() * sWorld->getRate(RATE_DAMAGE_FALL));

    if (movementInfo.pos.GetPositionZ() < m_Players[key].GetLastMovementInfo().pos.GetPositionZ() && zDiff > 14.57f)
    {
        if (movementInfo.HasMovementFlag(MOVEMENTFLAG_FALLING) || m_Players[key].GetLastMovementInfo().HasMovementFlag(MOVEMENTFLAG_FALLING))
        {
            if (damage == 0 && !player->IsImmunedToDamageOrSchool(SPELL_SCHOOL_MASK_NORMAL))
            {
                BuildReport(player, NO_FALL_DAMAGE_HACK_REPORT, movementInfo);
                return true;
            }
        }
    }

    return false;
}

void AnticheatMgr::HandlePlayerLogin(Player* player)
{
    m_Players[player->GetGUID()].SetPosition(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation(), player->GetMapId());
}

void AnticheatMgr::HandlePlayerLogout(Player* player)
{
    m_Players.erase(player->GetGUID());
}

void AnticheatMgr::AckUpdate(Player* player, uint32 diff)
{
    if (_updateCheckTimer <= diff)
    {
        DoActions(player);
        _updateCheckTimer = 4000;
    }
    else
    {
        _updateCheckTimer -= diff;
    }
}

void AnticheatMgr::DoActions(Player* player)
{
    auto const now = getMSTime();

    for (auto& order : _opackorders)
    {
        if (order.counter > 0 && order.lastRcvd < order.lastSent && (now - order.lastSent) > ALLOWED_ACK_LAG)
        {
            BuildReport(player, OP_ACK_HACK_REPORT, std::nullopt);
        }
    }
}

void AnticheatMgr::OrderSent(WorldPacket const* data)
{
    for (auto& order : _opackorders)
    {
        if (order.serverOpcode1 == data->GetOpcode() || order.serverOpcode2 == data->GetOpcode())
        {
            order.lastSent = getMSTime();
            ++order.counter;
            break;
        }
    }
}

void AnticheatMgr::CheckForOrderAck(uint32 opcode)
{
    for (auto& order : _opackorders)
    {
        if (order.clientResp == opcode)
        {
            --order.counter;
            break;
        }
    }
}

uint32 AnticheatMgr::GetTotalReports(ObjectGuid guid)
{
    return m_Players[guid].GetTotalReports();
}

float AnticheatMgr::GetAverage(ObjectGuid guid)
{
    return m_Players[guid].GetAverage();
}

uint32 AnticheatMgr::GetTypeReports(ObjectGuid guid, ReportTypes type)
{
    return m_Players[guid].GetTypeReports(type);
}

bool AnticheatMgr::MustCheckTempReports(ReportTypes type)
{
    return type != JUMP_HACK_REPORT
        && type != TELEPORT_HACK_REPORT
        && type != IGNORE_CONTROL_REPORT
        && type != GRAVITY_HACK_REPORT
        && type != ANTIKNOCK_BACK_HACK_REPORT
        && type != NO_FALL_DAMAGE_HACK_REPORT
        && type != OP_ACK_HACK_REPORT
        && type != COUNTER_MEASURES_REPORT;
}

void AnticheatMgr::BuildReport(Player* player, ReportTypes reportType, Optional<MovementInfo> optMovementInfo)
{
    OnReport(player, reportType);
    ObjectGuid key = player->GetGUID();

    if (MustCheckTempReports(reportType))
    {
        uint32 actualTime = getMSTime();

        if (!m_Players[key].GetTempReportsTimer(reportType))
            m_Players[key].SetTempReportsTimer(actualTime, reportType);

        if (getMSTimeDiff(m_Players[key].GetTempReportsTimer(reportType), actualTime) < 3000)
        {
            m_Players[key].SetTempReports(m_Players[key].GetTempReports(reportType) + 1, reportType);
        }
        else
        {
            m_Players[key].SetTempReportsTimer(actualTime, reportType);
            m_Players[key].SetTempReports(1, reportType);
        }
    }

    if (!m_Players[key].GetTotalReports())
        m_Players[key].SetCreationTime(getMSTime());

    m_Players[key].SetTotalReports(m_Players[key].GetTotalReports() + 1);
    m_Players[key].SetTypeReports(reportType, m_Players[key].GetTypeReports(reportType) + 1);

    uint32 diffTime = getMSTimeDiff(m_Players[key].GetCreationTime(), getMSTime()) / IN_MILLISECONDS;

    if (diffTime > 0)
    {
        float average = float(m_Players[key].GetTotalReports()) / float(diffTime);
        m_Players[key].SetAverage(average);
    }

    if (sHelper->GetUInt32(2041) < m_Players[key].GetTotalReports())
    {
        if (!m_Players[key].GetDailyReportState())
        {
            m_Players[key].SetDailyReportState(true);
        }
    }

    BuildAndSendReportToIngameGameMastersAndBan(player, reportType, optMovementInfo);

    // 写入反作弊触发记录表
    SaveAnticheatReportToDB(player, reportType, optMovementInfo);
}

void AnticheatMgr::ResetDailyReportStates()
{
    for (AnticheatPlayersDataMap::iterator it = m_Players.begin(); it != m_Players.end(); ++it)
        m_Players[(*it).first].SetDailyReportState(false);
}

void AnticheatMgr::SetSkipNextPacket(Player* player, bool skip)
{
    if (!player)
        return;

    ObjectGuid key = player->GetGUID();
    m_Players[key].SetSkipNextPacket(skip);
}

void AnticheatMgr::BuildAndSendReportToIngameGameMastersAndBan(Player* player, ReportTypes reportType, Optional<MovementInfo> optMovementInfo)
{
    const char* reportName = GetReportNameFromReportType(reportType);
    ObjectGuid key = player->GetGUID();

    // 所有类型都执行播报和封禁检查
    if (sHelper->GetBoolean(2043))
    {
        if (sHelper->GetBoolean(2045))
            sHelper->SendGMMessage("[反作弊提示]: 玩家 %s 作弊 被系统检测 类型: %s", sHelper->GetNameLinkColor(player).c_str(), reportName);
        else
            sHelper->SendSerMessage("[反作弊提示]: 玩家 %s 作弊 被系统检测 类型: %s", sHelper->GetNameLinkColor(player).c_str(), reportName);
    }

    // 封禁检查（所有类型都执行）
    if (m_Players[key].GetTotalReports() > sHelper->GetUInt32(2041))
    {
        if (sHelper->GetBoolean(2042))
        {
            std::string reportNameStr = std::string(GetReportNameFromReportType(reportType));
            sBan->BanCharacter(player->GetName(), "0s", reportNameStr, "Server");
        }
    }
}

void AnticheatMgr::SaveAnticheatReportToDB(Player* player, ReportTypes reportType, Optional<MovementInfo> optMovementInfo)
{
    if (!player)
        return;

    ObjectGuid key = player->GetGUID();

    // 获取作弊类型名称
    const char* reportName = GetReportNameFromReportType(reportType);
    std::string cheatType = reportName ? reportName : "未知";

    // 获取玩家信息
    uint32 playerGuid = player->GetGUID().GetCounter();
    std::string playerName = player->GetName();
    uint32 accountId = player->GetSession()->GetAccountId();

    // 获取位置信息
    uint32 mapId = player->GetMapId();
    uint32 areaId = player->GetAreaId();
    float posX = player->GetPositionX();
    float posY = player->GetPositionY();
    float posZ = player->GetPositionZ();

    // 获取速度信息
    float serverSpeed = 0.0f;
    float clientSpeed = 0.0f;
    float distance = 0.0f;
    uint32 timeDiff = 0;

    // 先计算距离和时间差
    if (optMovementInfo)
    {
        float lastX = m_Players[key].GetLastMovementInfo().pos.GetPositionX();
        float lastY = m_Players[key].GetLastMovementInfo().pos.GetPositionY();
        distance = sqrt(pow(optMovementInfo->pos.GetPositionX() - lastX, 2) + pow(optMovementInfo->pos.GetPositionY() - lastY, 2));
        timeDiff = getMSTimeDiff(m_Players[key].GetLastMovementInfo().time, optMovementInfo->time);
    }

    // 所有作弊类型都记录速度信息
    serverSpeed = m_Players[key].GetLastSpeedRate();

    // 计算实际客户端速度（根据移动距离和时间）
    if (timeDiff > 0 && distance > 0)
    {
        clientSpeed = distance / (timeDiff / 1000.0f); // 距离/时间(秒) = 码/秒
    }
    else
    {
        clientSpeed = player->GetSpeed(MOVE_RUN);
    }

    // 确定处理结果
    std::string result = "仅记录";
    if (sHelper->GetBoolean(2048))
    {
        if (reportType == TELEPORT_HACK_REPORT || reportType == TELEPORT_PLANE_HACK_REPORT)
            result = "传送回原点";
        else if (reportType == FLY_HACK_REPORT)
            result = "传送回地面";
        else
            result = "传送回原点";
    }

    // 写入数据库
    // SQL字段顺序: 玩家GUID, 玩家名字, 账号ID, 触发时间(NOW()), 作弊类型, 地图ID, 区域ID, 坐标X, 坐标Y, 坐标Z, 服务器速度, 客户端速度, 移动距离, 时间差, 处理结果
    // NOW()自动生成，不占用SetData索引，所以字段索引从0开始: 0-玩家GUID, 1-玩家名字, 2-账号ID, 3-作弊类型, 4-地图ID, ...
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_ANTICHEAT_REPORT);
    stmt->SetData(0, playerGuid);
    stmt->SetData(1, playerName);
    stmt->SetData(2, accountId);
    stmt->SetData(3, cheatType);
    stmt->SetData(4, mapId);
    stmt->SetData(5, areaId);
    stmt->SetData(6, posX);
    stmt->SetData(7, posY);
    stmt->SetData(8, posZ);
    stmt->SetData(9, serverSpeed);
    stmt->SetData(10, clientSpeed);
    stmt->SetData(11, distance);
    stmt->SetData(12, timeDiff);
    stmt->SetData(13, result);
    CharacterDatabase.Execute(stmt);
}
