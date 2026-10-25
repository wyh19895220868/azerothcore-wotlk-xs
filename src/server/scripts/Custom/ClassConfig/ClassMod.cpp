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
#include <Battleground.h>
#include <algorithm>
#include <limits>
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include "ClassMod.h"
#include <Custom\CreatureMod\AsCreature.h>
#include <Custom\PlayerModMgr\PlayerModMgr.h>
#pragma execution_character_set("utf-8")

ClassConfigMap ClassConfigStore;
ClassTempMap ClassTempStore;
HRTempMap HRStore;
StatPtConfigMap StatPtConfigStore;
StatPtRankMap StatPtRankStore;
StatPtRewMap StatPtRewStore;
OnMapAreaTempVect OnMapAreaStore;

namespace
{
bool IsLegacyDamageLimitValue(AsInt64 const& value)
{
    return value == AsInt64(2000000000)
        || value == AsInt64(2100000000)
        || value == AsInt64(std::numeric_limits<int32>::max());
}
}

void CustomClass::LoadCustomClass()
{
    ClassConfigStore.clear();
    uint32 Ct = 0;
    QueryResult Result = WorldDatabase.Query("SELECT 职业编号, 属性模版 FROM _玩家职业属性调整表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ClassId = Fields[0].Get<uint32>();
            uint32 CTempId = Fields[1].Get<uint32>();

            ClassConfigStore.insert(std::make_pair(ClassId, CTempId));
            ++Ct;

        } while (Result->NextRow());
    }

    ClassTempStore.clear();
    Ct = 0;
    Result = WorldDatabase.Query("SELECT 模版编号, 固定格挡, 格挡上限, 格挡倍率, 固定招架, 招架上限, 招架倍率, 固定闪避, 闪避上限, 闪避倍率, 固定暴击, 暴击上限, 暴击倍率,"
                                                       "固定韧性, 韧性上限, 韧性倍率, 固定护甲, 护甲上限, 护甲倍率, 固定精准, 精准上限, 精准倍率, 固定血量, 血量下限, 血量上限, 血量倍率, 固定急速, 急速上限, 急速倍率,"
                                                       "固定伤害, 伤害上限, PVE物理伤害倍率, PVE技能伤害倍率, PVP物理伤害倍率, PVP技能伤害倍率,"
                                                       "PVE免伤率, PVP免伤率, 固定治疗, 治疗上限, 治疗倍率, 固定蓝量, 蓝量下限, 蓝量上限, 蓝量倍率, 固定力量, 力量下限, 固定敏捷, 敏捷下限, 固定耐力, 耐力下限, 固定智力, 智力下限, 固定精神, 精神下限, 固定武器最小伤害, 固定武器最大伤害,"
                                                       "固定攻击强度, 固定法术强度, 固定治疗效果, 智力加法术强度倍率, 智力加治疗效果倍率, 精神加法术强度倍率, 精神加治疗效果倍率, PVEDOT伤害倍率, PVPDOT伤害倍率 "
                                                       "FROM __模版玩家属性表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 TempId = Fields[0].Get<uint32>();
            ClassTemp St;
            St.BlockNow = Fields[1].Get<float>();
            St.BlockMax = Fields[2].Get<float>();
            St.BlockMod = Fields[3].Get<float>();
            St.ParryNow = Fields[4].Get<float>();
            St.ParryMax = Fields[5].Get<float>();
            St.ParryMod = Fields[6].Get<float>();
            St.DodgeNow = Fields[7].Get<float>();
            St.DodgeMax = Fields[8].Get<float>();
            St.DodgeMod = Fields[9].Get<float>();
            St.CritNow = Fields[10].Get<float>();
            St.CritMax = Fields[11].Get<float>();
            St.CritMod = Fields[12].Get<float>();
            St.RxNow = Fields[13].Get<int32>();
            St.RxMax = Fields[14].Get<int32>();
            St.RxMod = Fields[15].Get<float>();
            St.ArrNow = Fields[16].Get<int32>();
            St.ArrMax = Fields[17].Get<int32>();
            St.ArrMod = Fields[18].Get<float>();
            St.PreNow = Fields[19].Get<int32>();
            St.PreMax = Fields[20].Get<int32>();
            St.PreMod = Fields[21].Get<float>();
            St.HpNow = AsInt64FromString(Fields[22].Get<std::string>());
            St.HpMin = AsInt64FromString(Fields[23].Get<std::string>());
            St.HpMax = AsInt64FromString(Fields[24].Get<std::string>());
            St.HpMod = Fields[25].Get<float>();
            St.JsNow = Fields[26].Get<int32>();
            St.JsMax = Fields[27].Get<int32>();
            St.JsMod = Fields[28].Get<float>();
            St.DmgNow = AsInt64FromString(Fields[29].Get<std::string>());
            St.DmgMax = AsInt64FromString(Fields[30].Get<std::string>());
            St.MPVEMod = Fields[31].Get<float>();
            St.SPVEMod = Fields[32].Get<float>();
            St.MPVPMod = Fields[33].Get<float>();
            St.SPVPMod = Fields[34].Get<float>();
            St.PVERet = Fields[35].Get<float>();
            St.PVPRet = Fields[36].Get<float>();
            St.HeNow = AsInt64FromString(Fields[37].Get<std::string>());
            St.HeMax = AsInt64FromString(Fields[38].Get<std::string>());
            St.HeMod = Fields[39].Get<float>();
            St.ManaNow = AsInt64FromString(Fields[40].Get<std::string>());

            St.ManaMin = AsInt64FromString(Fields[41].Get<std::string>());
            St.ManaMax = AsInt64FromString(Fields[42].Get<std::string>());
            St.ManaMod = Fields[43].Get<float>();

            St.St1Now = AsInt64FromString(Fields[44].Get<std::string>());
            St.St1Min = AsInt64FromString(Fields[45].Get<std::string>());
            St.St2Now = AsInt64FromString(Fields[46].Get<std::string>());
            St.St2Min = AsInt64FromString(Fields[47].Get<std::string>());
            St.St3Now = AsInt64FromString(Fields[48].Get<std::string>());
            St.St3Min = AsInt64FromString(Fields[49].Get<std::string>());
            St.St4Now = AsInt64FromString(Fields[50].Get<std::string>());
            St.St4Min = AsInt64FromString(Fields[51].Get<std::string>());
            St.St5Now = AsInt64FromString(Fields[52].Get<std::string>());
            St.St5Min = AsInt64FromString(Fields[53].Get<std::string>());
            St.MinWpNow = AsInt64FromString(Fields[54].Get<std::string>());
            St.MaxWpNow = AsInt64FromString(Fields[55].Get<std::string>());
            St.ApNow = AsInt64FromString(Fields[56].Get<std::string>());
            St.SpNow = AsInt64FromString(Fields[57].Get<std::string>());
            St.EpNow = AsInt64FromString(Fields[58].Get<std::string>());
            St.St4SPMod = Fields[59].Get<float>();
            St.St4HEMod = Fields[60].Get<float>();
            St.St5SPMod = Fields[61].Get<float>();
            St.St5HEMod = Fields[62].Get<float>();
            St.PVEDot = Fields[63].Get<float>();
            St.PVPDot = Fields[64].Get<float>();

            if (St.BlockMod <= 0.0f)
                St.BlockMod = 1.0f;
            if (St.ParryMod <= 0.0f)
                St.ParryMod = 1.0f;
            if (St.DodgeMod <= 0.0f)
                St.DodgeMod = 1.0f;
            if (St.CritMod <= 0.0f)
                St.CritMod = 1.0f;
            if (St.RxMod <= 0.0f)
                St.RxMod = 1.0f;
            if (St.ArrMod <= 0.0f)
                St.ArrMod = 1.0f;
            if (St.PreMod <= 1.0f)
                St.PreMod = 1.0f;
            if (St.HpMod <= 0.0f)
                St.HpMod = 1.0f;
            if (St.JsMod <= 0.0f)
                St.JsMod = 1.0f;
            if (St.MPVEMod <= 0.0f)
                St.MPVEMod = 1.0f;
            if (St.SPVEMod <= 0.0f)
                St.SPVEMod = 1.0f;
            if (St.MPVPMod <= 0.0f)
                St.MPVPMod = 1.0f;
            if (St.SPVPMod <= 0.0f)
                St.SPVPMod = 1.0f;
            if (St.PVERet >= 1.0f)
                St.PVERet = 0.99999f;
            if (St.PVPRet >= 1.0f)
                St.PVPRet = 0.99999f;
            if (St.HeMod <= 0.0f)
                St.HeMod = 1.0f;
            if (St.St4SPMod < 0.0f)
                St.St4SPMod = 0.0f;
            if (St.St4HEMod < 0.0f)
                St.St4HEMod = 0.0f;
            if (St.St5SPMod < 0.0f)
                St.St5SPMod = 0.0f;
            if (St.St5HEMod < 0.0f)
                St.St5HEMod = 0.0f;
            if (St.PVEDot < 0.0f)
                St.PVEDot = 1.0f;
            if (St.PVPDot < 0.0f)
                St.PVPDot = 1.0f;

            ClassTempStore.insert(std::make_pair(TempId, St));
            ++Ct;

        } while (Result->NextRow());
    }

    HRStore.clear();
    Ct = 0;
    Result = WorldDatabase.Query("SELECT 军衔编号, 升级需求模版, 升级奖励模版, 头衔 FROM _军衔;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Id = Fields[0].Get<uint32>();
            HRTemp St;
            St.NTempId = Fields[1].Get<uint32>();
            St.RTempId = Fields[2].Get<uint32>();
            St.CTitle = Fields[3].Get<std::string>();
            HRStore.insert(std::make_pair(Id, St));
            ++Ct;

        } while (Result->NextRow());
    }

    StatPtConfigStore.clear();
    Ct = 0;
    Result = WorldDatabase.Query("SELECT 序号, 功能值 FROM _斗气配置表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Num = Fields[0].Get<uint32>();
            AsInt64 Value = AsInt64FromString(Fields[1].Get<std::string>());
            StatPtConfigStore.insert(std::make_pair(Num, Value));
            ++Ct;

        } while (Result->NextRow());
    }

    StatPtRankStore.clear();
    Ct = 0;
    Result = WorldDatabase.Query("SELECT 斗气等级, 需要经验, 奖励斗气点数 FROM _斗气等级经验表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Rank = Fields[0].Get<uint32>();
            StatPtRankTemp St;
            St.NeedXp = AsInt64FromString(Fields[1].Get<std::string>());
            St.RewCount = AsInt64FromString(Fields[2].Get<std::string>());
            if (!St.RewCount)
                St.RewCount = 1;
            StatPtRankStore.insert(std::make_pair(Rank, St));
            ++Ct;

        } while (Result->NextRow());
    }

    StatPtRewStore.clear();
    Ct = 0;
    Result = WorldDatabase.Query("SELECT 怪物或物品编号, 直达等级, 奖励经验, 启用加1级 FROM _斗气奖励表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ObjId = Fields[0].Get<uint32>();
            StatPtRewTemp St;
            St.ToRank = Fields[1].Get<uint32>();
            St.RewXp = AsInt64FromString(Fields[2].Get<std::string>());
            St.AddRank = Fields[3].Get<bool>();
            StatPtRewStore.insert(std::make_pair(ObjId, St));
            ++Ct;

        } while (Result->NextRow());
    }

    AuraLootItemStore.clear();
    Ct = 0;
    Result = WorldDatabase.Query("SELECT 光环编号, 掉落物品1, 掉落数量1, 掉落物品2, 掉落数量2, 掉落几率 FROM _玩家光环杀怪掉落表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 AuraId = Fields[0].Get<uint32>();
            AuraLootItem St;
            St.ItemId1 = Fields[1].Get<uint32>();
            St.Count1 = Fields[2].Get<uint32>();
            St.ItemId2 = Fields[3].Get<uint32>();
            St.Count2 = Fields[4].Get<uint32>();
            St.Odds = Fields[5].Get<float>();
            AuraLootItemStore.insert(std::make_pair(AuraId, St));
            ++Ct;

        } while (Result->NextRow());
    }

    OnMapAreaStore.clear();
    Ct = 0;
    Result = WorldDatabase.Query("SELECT 地图或区域, 职业, 属性模板 FROM _进入地图或区域表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            OnMapAreaTemp St;
            St.MapOrArea = Fields[0].Get<int32>();
            St.Class = Fields[1].Get<uint32>();
            St.TempId = Fields[2].Get<uint32>();
            OnMapAreaStore.push_back(St);
            ++Ct;

        } while (Result->NextRow());
    }
}

/*PVE物理伤害倍率*/
float CustomClass::GetMPVEModForClass(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.MPVEMod;

    return 1.0f;
}

/*PVE法术伤害倍率*/
float CustomClass::GetSPVEModForClass(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.SPVEMod;

    return 1.0f;
}

/*PVP物理伤害倍率*/
float CustomClass::GetMPVPModForClass(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.MPVPMod;

    return 1.0f;
}

/*PVP法术伤害倍率*/
float CustomClass::GetSPVPModForClass(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.SPVPMod;

    return 1.0f;
}

/*PVE免伤倍率*/
float CustomClass::GetPVERetModForClass(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.PVERet;

    return 0.0f;
}

/*PVP免伤倍率*/
float CustomClass::GetPVPRetModForClass(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.PVPRet;

    return 0.0f;
}

/*治疗倍率*/
float CustomClass::GetHeModForClass(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.HeMod;

    return 1.0f;
}

/*固定格挡*/
float CustomClass::GetBlockNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.BlockNow;

    return 0.0f;
}

/*上限格挡*/
float CustomClass::GetBlockMax(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.BlockMax;

    return 100.0f;
}

/*格挡倍率*/
float CustomClass::GetBlockMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.BlockMod;

    return 1.0f;
}

/*固定招架*/
float CustomClass::GetParryNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.ParryNow;

    return 0.0f;
}

/*上限招架*/
float CustomClass::GetParryMax(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.ParryMax;

    return 100.0f;
}

/*招架倍率*/
float CustomClass::GetParryMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.ParryMod;

    return 1.0f;
}

/*固定躲闪*/
float CustomClass::GetDodgeNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.DodgeNow;

    return 0.0f;
}

/*上限躲闪*/
float CustomClass::GetDodgeMax(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.DodgeMax;

    return 100.0f;
}

/*躲闪倍率*/
float CustomClass::GetDodgeMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.DodgeMod;

    return 1.0f;
}

/*固定暴击*/
float CustomClass::GetCritNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.CritNow;

    return 0.0f;
}

/*上限暴击*/
float CustomClass::GetCritMax(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.CritMax;

    return 100.0f;
}

/*暴击倍率*/
float CustomClass::GetCritMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.CritMod;

    return 1.0f;
}

/*固定韧性*/
int32 CustomClass::GetRxNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.RxNow;

    return 0;
}

/*上限韧性*/
int32 CustomClass::GetRxMax(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.RxMax;

    return 4000;
}

/*韧性倍率*/
float CustomClass::GetRxMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.RxMod;

    return 1.0f;
}

/*固定护甲*/
int32 CustomClass::GetArrNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.ArrNow;

    return 0;
}

/*上限护甲*/
int32 CustomClass::GetArrMax(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.ArrMax;

    return 0;
}

/*护甲倍率*/
float CustomClass::GetArrMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.ArrMod;

    return 1.0f;
}

/*固定精准*/
int32 CustomClass::GetPreNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.PreNow;

    return 0;
}

/*上限精准*/
int32 CustomClass::GetPreMax(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.PreMax;

    return 200;
}

/*精准倍率*/
float CustomClass::GetPreMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.PreMod;

    return 1.0f;
}

/*固定血量*/
uint32 CustomClass::GetHpNow(Player* player)
{
    return AsInt64ToUInt32Clamp(GetAsHpNow(player), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 CustomClass::GetAsHpNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.HpNow;

    return AsInt64Zero();
}

/*下限血量*/
uint32 CustomClass::GetHpMin(Player* player)
{
    return AsInt64ToUInt32Clamp(GetAsHpMin(player), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 CustomClass::GetAsHpMin(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.HpMin;

    return AsInt64Zero();
}

/*上限血量*/
uint32 CustomClass::GetHpMax(Player* player)
{
    return AsInt64ToUInt32Clamp(GetAsHpMax(player), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 CustomClass::GetAsHpMax(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.HpMax;

    return AsInt64Zero();
}

/*血量倍率*/
float CustomClass::GetHpMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.HpMod;

    return 1.0f;
}

/*固定急速*/
int32 CustomClass::GetJsNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.JsNow;

    return 0;
}

/*急速上限*/
int32 CustomClass::GetJsMax(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.JsMax;

    return 0;
}

/*急速倍率*/
float CustomClass::GetJsMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.JsMod;

    return 1.0f;
}

/*固定伤害*/
uint32 CustomClass::GetDmgNow(Player* player)
{
    return AsInt64ToUInt32Clamp(GetAsDmgNow(player), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 CustomClass::GetAsDmgNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
    {
        AsInt64 fixedDamage = It->second.DmgNow;
        if (IsLegacyDamageLimitValue(fixedDamage))
            return AsInt64Zero();

        return fixedDamage;
    }

    return AsInt64Zero();
}

/*上限伤害*/
uint32 CustomClass::GetDmgMax(Player* player)
{
    return AsInt64ToUInt32Clamp(GetAsDmgMax(player), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 CustomClass::GetAsDmgMax(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
    {
        AsInt64 maxDamage = It->second.DmgMax;
        if (IsLegacyDamageLimitValue(maxDamage))
            return AsInt64Zero();

        return maxDamage;
    }

    return AsInt64Zero();
}

/*固定治疗*/
uint32 CustomClass::GetHeNow(Player* player)
{
    return AsInt64ToUInt32Clamp(GetAsHeNow(player), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 CustomClass::GetAsHeNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.HeNow;

    return 0;
}

/*上限治疗*/
uint32 CustomClass::GetHeMax(Player* player)
{
    return AsInt64ToUInt32Clamp(GetAsHeMax(player), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 CustomClass::GetAsHeMax(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.HeMax;

    return CT_MAX_INT;
}

/*固定蓝量*/
uint32 CustomClass::GetManaNow(Player* player)
{
    return AsInt64ToUInt32Clamp(GetAsManaNow(player), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 CustomClass::GetAsManaNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.ManaNow;

    return AsInt64Zero();
}

uint32 CustomClass::GetManaMin(Player* player)
{
    return AsInt64ToUInt32Clamp(GetAsManaMin(player), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 CustomClass::GetAsManaMin(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.ManaMin;

    return AsInt64Zero();
}

/*上限蓝量*/
uint32 CustomClass::GetManaMax(Player* player)
{
    return AsInt64ToUInt32Clamp(GetAsManaMax(player), uint32(std::numeric_limits<int32>::max()));
}

AsInt64 CustomClass::GetAsManaMax(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.ManaMax;

    return AsInt64(CT_MAX_INT);
}

/*蓝量倍率*/
float CustomClass::GetManaMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.ManaMod;

    return 1.0f;
}

/*固定力量*/
int32 CustomClass::GetSt1Now(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsSt1Now(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsSt1Now(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St1Now;

    return 0;
}

/*下限力量*/
int32 CustomClass::GetSt1Min(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsSt1Min(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsSt1Min(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St1Min;

    return 0;
}

/*固定敏捷*/
int32 CustomClass::GetSt2Now(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsSt2Now(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsSt2Now(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St2Now;

    return 0;
}

/*下限敏捷*/
int32 CustomClass::GetSt2Min(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsSt2Min(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsSt2Min(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St2Min;

    return 0;
}

/*固定耐力*/
int32 CustomClass::GetSt3Now(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsSt3Now(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsSt3Now(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St3Now;

    return 0;
}

/*下限耐力*/
int32 CustomClass::GetSt3Min(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsSt3Min(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsSt3Min(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St3Min;

    return 0;
}

/*固定智力*/
int32 CustomClass::GetSt4Now(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsSt4Now(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsSt4Now(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St4Now;

    return 0;
}

/*下限智力*/
int32 CustomClass::GetSt4Min(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsSt4Min(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsSt4Min(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St4Min;

    return 0;
}

/*固定精神*/
int32 CustomClass::GetSt5Now(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsSt5Now(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsSt5Now(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St5Now;

    return 0;
}

/*下限精神*/
int32 CustomClass::GetSt5Min(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsSt5Min(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsSt5Min(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St5Min;

    return 0;
}

/*固定武器伤害*/
int32 CustomClass::GetMinWpNow(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsMinWpNow(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsMinWpNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
    {
        AsInt64 minWeaponDamage = It->second.MinWpNow;
        if (IsLegacyDamageLimitValue(minWeaponDamage))
            return AsInt64Zero();

        return minWeaponDamage;
    }

    return AsInt64Zero();
}

/*上限武器伤害*/
int32 CustomClass::GetMaxWpNow(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsMaxWpNow(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsMaxWpNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
    {
        AsInt64 maxWeaponDamage = It->second.MaxWpNow;
        if (IsLegacyDamageLimitValue(maxWeaponDamage))
            return AsInt64Zero();

        return maxWeaponDamage;
    }

    return AsInt64Zero();
}

/*固定攻强*/
int32 CustomClass::GetApNow(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsApNow(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsApNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
    {
        AsInt64 attackPower = It->second.ApNow;
        if (IsLegacyDamageLimitValue(attackPower))
            return AsInt64Zero();

        return attackPower;
    }

    return 0;
}

/*固定法强*/
int32 CustomClass::GetSpNow(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsSpNow(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsSpNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
    {
        AsInt64 spellPower = It->second.SpNow;
        if (IsLegacyDamageLimitValue(spellPower))
            return AsInt64Zero();

        return spellPower;
    }

    return 0;
}

/*固定治疗*/
int32 CustomClass::GetEpNow(Player* player)
{
    return AsInt64ToInt32Clamp(GetAsEpNow(player), std::numeric_limits<int32>::max());
}

AsInt64 CustomClass::GetAsEpNow(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
    {
        AsInt64 healingPower = It->second.EpNow;
        if (IsLegacyDamageLimitValue(healingPower))
            return AsInt64Zero();

        return healingPower;
    }

    return 0;
}

bool CustomClass::InNowMapOrArea(Player* player)
{
    bool In = false;
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (TempId)
    {
        In = true;
    }
    else
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
        {
            auto It = ClassTempStore.find(Itr->second);
            if (It != ClassTempStore.end())
                In = true;
        }
    }

    return In;
}

void CustomClass::OnPlayerMapChanged(Player* player)
{
    player->SetCanModifyStats(true);
    player->UpdateAllStats();

    uint32 MapId = player->GetMapId();
    player->OnNowFB = sHelper->OnDungeon(MapId);
}

void CustomClass::OnPlayerUpdateZone(Player* player, uint32 newZone, uint32 newArea)
{
    player->SetCanModifyStats(true);
    player->UpdateAllStats();
}

uint32 CustomClass::GetHRNTempId(uint32 TitleId)
{
    auto Itr = HRStore.find(TitleId);
    return Itr != HRStore.end() ? Itr->second.NTempId : 0;
}

uint32 CustomClass::GetHRRTempId(uint32 TitleId)
{
    auto Itr = HRStore.find(TitleId);
    return Itr != HRStore.end() ? Itr->second.RTempId : 0;
}

uint32 CustomClass::GetHRTiteId(Player* player)
{
    if (player->GetTeamId() == TEAM_ALLIANCE)
    {
        if (player->HasTitle(14)) return 14;
        else if (player->HasTitle(13)) return 13;
        else if (player->HasTitle(12)) return 12;
        else if (player->HasTitle(11)) return 11;
        else if (player->HasTitle(10)) return 10;
        else if (player->HasTitle(9)) return 9;
        else if (player->HasTitle(8)) return 8;
        else if (player->HasTitle(7)) return 7;
        else if (player->HasTitle(6)) return 6;
        else if (player->HasTitle(5)) return 5;
        else if (player->HasTitle(4)) return 4;
        else if (player->HasTitle(3)) return 3;
        else if (player->HasTitle(2)) return 2;
        else if (player->HasTitle(1)) return 1;
        else return 0;
    }
    else
    {
        if (player->HasTitle(28)) return 28;
        else if (player->HasTitle(27)) return 27;
        else if (player->HasTitle(26)) return 26;
        else if (player->HasTitle(25)) return 25;
        else if (player->HasTitle(24)) return 24;
        else if (player->HasTitle(23)) return 23;
        else if (player->HasTitle(22)) return 22;
        else if (player->HasTitle(21)) return 21;
        else if (player->HasTitle(20)) return 20;
        else if (player->HasTitle(19)) return 19;
        else if (player->HasTitle(18)) return 18;
        else if (player->HasTitle(17)) return 17;
        else if (player->HasTitle(16)) return 16;
        else if (player->HasTitle(15)) return 15;
        else return 14;
    }
}

uint32 CustomClass::AotoTempAt(uint32 Try, bool Src)
{
    if (Src)
    {
        switch (Try)
        {
        case 4:
            return AT21;
        case 3:
            return AT22;
        case 7:
            return AT23;
        case 5:
            return AT24;
        case 6:
            return AT25;
        case 38:
            return AT26;
        case 45:
            return AT27;
        case 36:
            return AT28;
        case 31:
            return AT29;
        case 32:
            return AT30;
        case 44:
            return AT31;
        case 47:
            return AT32;
        }
    }
    else
    {
        switch (Try)
        {
        case AT21:
            return 4;
        case AT22:
            return 3;
        case AT23:
            return 7;
        case AT24:
            return 5;
        case AT25:
            return 6;
        case AT26:
            return 38;
        case AT27:
            return 45;
        case AT28:
            return 36;
        case AT29:
            return 31;
        case AT30:
            return 32;
        case AT31:
            return 44;
        case AT32:
            return 47;
        }
    }

    return 0;
}

uint32 CustomClass::GetStatConfigCount(uint32 Num)
{
    return AsInt64ToUInt32Clamp(GetStatConfigValue(Num));
}

AsInt64 CustomClass::GetStatConfigValue(uint32 Num)
{
    auto Itr = StatPtConfigStore.find(Num);
    return Itr != StatPtConfigStore.end() ? Itr->second : AsInt64Zero();
}

void CustomClass::ApplyPlayerStat(Player* player, uint32 Try, AsInt64 const& Value, bool Apply)
{
    if (!player || Value == 0)
        return;

    bool updateAllStats = false;
    bool updateAttackPower = false;
    bool updateSpellPower = false;
    bool updateRatings = false;

    auto scaledValue = [&](uint32 configId) -> AsInt64
    {
        return Value * GetStatConfigValue(configId);
    };

    auto scaledCompatValue = [&](uint32 configId) -> int32
    {
        return AsInt64ToInt32Clamp(scaledValue(configId), std::numeric_limits<int32>::max());
    };

    switch (Try)
    {
    case 4:
        player->DoApplyStat(ITEM_MOD_STRENGTH, scaledValue(2001), Apply);
        updateAllStats = true;
        break;
    case 3:
        player->DoApplyStat(ITEM_MOD_AGILITY, scaledValue(2003), Apply);
        updateAllStats = true;
        break;
    case 7:
        player->DoApplyStat(ITEM_MOD_STAMINA, scaledValue(2005), Apply);
        updateAllStats = true;
        break;
    case 5:
        player->DoApplyStat(ITEM_MOD_INTELLECT, scaledValue(2007), Apply);
        updateAllStats = true;
        break;
    case 6:
        player->DoApplyStat(ITEM_MOD_SPIRIT, scaledValue(2009), Apply);
        updateAllStats = true;
        break;
    case 38:
        player->DoApplyStat(ITEM_MOD_ATTACK_POWER, scaledValue(2011), Apply);
        updateAttackPower = true;
        break;
    case 45:
        player->DoApplyStat(ITEM_MOD_SPELL_POWER, scaledValue(2013), Apply);
        updateSpellPower = true;
        break;
    case 36:
        player->ApplyRatingMod(CR_HASTE_MELEE, scaledCompatValue(2015), Apply);
        player->ApplyRatingMod(CR_HASTE_RANGED, scaledCompatValue(2015), Apply);
        player->ApplyRatingMod(CR_HASTE_SPELL, scaledCompatValue(2015), Apply);
        updateRatings = true;
        break;
    case 31:
        player->ApplyRatingMod(CR_HIT_MELEE, scaledCompatValue(2017), Apply);
        player->ApplyRatingMod(CR_HIT_RANGED, scaledCompatValue(2017), Apply);
        player->ApplyRatingMod(CR_HIT_SPELL, scaledCompatValue(2017), Apply);
        updateRatings = true;
        break;
    case 32:
        player->ApplyRatingMod(CR_CRIT_MELEE, scaledCompatValue(2019), Apply);
        player->ApplyRatingMod(CR_CRIT_RANGED, scaledCompatValue(2019), Apply);
        player->ApplyRatingMod(CR_CRIT_SPELL, scaledCompatValue(2019), Apply);
        updateRatings = true;
        break;
    case 44:
        player->ApplyRatingMod(CR_ARMOR_PENETRATION, scaledCompatValue(2021), Apply);
        updateRatings = true;
        break;
    case 47:
        player->ApplySpellPenetrationBonus(scaledCompatValue(2023), Apply);
        updateSpellPower = true;
        break;
    }

    if (updateAllStats)
        player->UpdateAllStats();
    else
    {
        if (updateAttackPower)
        {
            player->UpdateAttackPowerAndDamage(false);
            player->UpdateAttackPowerAndDamage(true);
        }

        if (updateSpellPower)
            player->UpdateSpellDamageAndHealingBonus();

        if (updateRatings)
        {
            player->UpdateAllRatings();
            player->UpdateAllCritPercentages();
            player->UpdateAllSpellCritChances();
            player->UpdateExpertise(BASE_ATTACK);
            player->UpdateExpertise(OFF_ATTACK);
            player->RecalculateRating(CR_ARMOR_PENETRATION);
        }
    }
}

void CustomClass::SendUpdatePlayerStatData(Player* player)
{
    std::string SendMsg;
    SendMsg.clear();
    for (size_t i = AT11; i < ATMAX; i++)
        SendMsg.append(AsInt64ToString(player->TempAtStore[i].Value) + "#");

    uint32 rank = AsInt64ToUInt32Clamp(player->TempAtStore[AT11].Value);
    AsInt64 NeedXp = GetStatNeedXp(rank);
    AsInt64 NextCount = GetStatNextRewCount(rank);
    sDoAddon->SendPacketToClient(player, "MeStatPointData", AsInt64ToString(NextCount) + "#" + AsInt64ToString(NeedXp) + "#" + SendMsg);
    SendMsg.clear();
}

bool CustomClass::IsCheckReset(Player* player)
{
    bool Ret = false;
    for (size_t i = AT21; i < ATMAX; i++)
    {
        if (player->TempAtStore[i].Value > 0)
            Ret = true;
    }
    return Ret;
}

AsInt64 CustomClass::GetStatConfigMax(uint32 Try)
{
    AsInt64 Max = AsInt64Zero();
    switch (Try)
    {
    case 4:
        Max = GetStatConfigValue(2002);
        break;
    case 3:
        Max = GetStatConfigValue(2004);
        break;
    case 7:
        Max = GetStatConfigValue(2006);
        break;
    case 5:
        Max = GetStatConfigValue(2008);
        break;
    case 6:
        Max = GetStatConfigValue(2010);
        break;
    case 38:
        Max = GetStatConfigValue(2012);
        break;
    case 45:
        Max = GetStatConfigValue(2014);
        break;
    case 36:
        Max = GetStatConfigValue(2016);
        break;
    case 31:
        Max = GetStatConfigValue(2018);
        break;
    case 32:
        Max = GetStatConfigValue(2020);
        break;
    case 44:
        Max = GetStatConfigValue(2022);
        break;
    case 47:
        Max = GetStatConfigValue(2024);
        break;
    }

    return Max;
}

uint32 CustomClass::GetStatMaxRank()
{
    if (StatPtRankStore.empty())
        return 0;

    return StatPtRankStore.rbegin()->first;
}

AsInt64 CustomClass::GetStatNeedXp(uint32 Rank)
{
    auto Itr = StatPtRankStore.find(Rank + 1);
    return Itr != StatPtRankStore.end() ? Itr->second.NeedXp : AsInt64Zero();
}

AsInt64 CustomClass::GetStatNextRewCount(uint32 Rank)
{
    auto Itr = StatPtRankStore.find(Rank + 1);
    return Itr != StatPtRankStore.end() ? Itr->second.RewCount : AsInt64Zero();
}

AsInt64 CustomClass::GetStatOldNewRewCount(uint32 ORank, uint32 NRank)
{
    AsInt64 Count = AsInt64Zero();
    for (auto Itr = StatPtRankStore.begin(); Itr != StatPtRankStore.end(); ++Itr)
    {
        if (Itr->first > ORank && Itr->first <= NRank)
            Count += Itr->second.RewCount;
    }
    return Count;
}

AsInt64 CustomClass::GetStatRewCount(uint32 Rank)
{
    auto Itr = StatPtRankStore.find(Rank);
    return Itr != StatPtRankStore.end() ? Itr->second.RewCount : AsInt64(1);
}

AsInt64 CustomClass::GetStatAllsCount(uint32 NowRank)
{
    AsInt64 Count = AsInt64Zero();
    for (auto Itr = StatPtRankStore.begin(); Itr != StatPtRankStore.end(); ++Itr)
    {
        if (Itr->first > NowRank)
            continue;

        Count += Itr->second.RewCount;
    }
    return Count;
}

void CustomClass::SendConfigStatData(Player* player)
{
    std::string SendMsg;
    SendMsg.clear();
    for (size_t i = 2001; i < 2025; i++)
    {
        if (i % 2 == 0)
            continue;

        SendMsg.append(AsInt64ToString(sCustomClass->GetStatConfigValue(i)) + "#");
    }

    sDoAddon->SendPacketToClient(player, "StatPointConfigData", SendMsg);
    SendMsg.clear();
}

uint32 CustomClass::GetStatTempId(int32 MapId, int32 AreaId, uint32 Class, uint32 VipRank)
{
    uint32 STempId = sPlayerModMgr->GetVipSTempId(VipRank);
    if (STempId)
        return STempId;

    for (auto Itr = OnMapAreaStore.begin(); Itr != OnMapAreaStore.end(); ++Itr)
    {
        if ((Itr->MapOrArea == MapId || Itr->MapOrArea == AreaId) && (Itr->Class == 0 || Itr->Class == Class))
            return Itr->TempId;
    }

    return 0;
}

float CustomClass::GetSt4SPMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St4SPMod;

    return 0.0f;
}

float CustomClass::GetSt4HEMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St4HEMod;

    return 0.0f;
}

float CustomClass::GetSt5SPMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St5SPMod;

    return 0.0f;
}

float CustomClass::GetSt5HEMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.St5HEMod;

    return 0.0f;
}

float CustomClass::GetPVEDotMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.PVEDot;

    return 1.0f;
}

float CustomClass::GetPVPDotMod(Player* player)
{
    uint32 Class = player->GetClass();
    uint32 TempId = GetStatTempId((int32)player->GetMapId(), -int32(player->GetAreaId()), Class, player->VipRank);
    if (!TempId)
    {
        auto Itr = ClassConfigStore.find(Class);
        if (Itr != ClassConfigStore.end())
            TempId = Itr->second;
    }

    auto It = ClassTempStore.find(TempId);
    if (It != ClassTempStore.end())
        return It->second.PVPDot;

    return 1.0f;
}

void CustomClass::OnHookSpellDamageDot(Player* Aplayer, Unit* Victim, float& Damage)
{
    if (!Victim || !Aplayer || !Damage || !Victim->IsInWorld() || !Aplayer->IsInWorld())
        return;

    if (Victim->GetTypeId() != TYPEID_PLAYER)
    {
        Damage *= sCustomClass->GetPVEDotMod(Aplayer);
    }
    else if (Victim->GetTypeId() == TYPEID_PLAYER)
    {
        Damage *= sCustomClass->GetPVPDotMod(Aplayer);
    }
}

class OnCustomClassPlayer : public PlayerScript
{
public:
    OnCustomClassPlayer() : PlayerScript("OnCustomClassPlayer") {  }

    void OnMapChanged(Player* player) override
    {
        if (sHelper->GetBoolean(9994))
            LOG_WARN("server", ">> 玩家进入地图调试输出 PlayerGuidLow = {} MapId = {}", player->GetGUIDLow(), player->GetMapId());

        sCustomClass->OnPlayerMapChanged(player);
    }

    void OnUpdateZone(Player* player, uint32 newZone, uint32 newArea) override
    {
        sCustomClass->OnPlayerUpdateZone(player, newZone, newArea);
    }
};

void AddSC_CustomClassScript()
{
    new OnCustomClassPlayer();
}
