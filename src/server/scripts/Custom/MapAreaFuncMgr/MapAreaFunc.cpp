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
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include "MapAreaFunc.h"
#include <Maps\MapMgr.h>
#pragma execution_character_set("utf-8")

AreaFuncTempMap AreaFuncTempStore;

void MapAreaFunc::LoadMapAreaFuncData()
{
    QueryResult Result = WorldDatabase.Query("SELECT 地图或区域编号, 设置属性, 允许进入, 允许决斗, 允许组队, 允许陆地坐骑, 允许飞行坐骑, 联盟墓地坐标模版, 部落墓地坐标模版, 禁止技能施放, 强制光环无效, 进入提示内容 FROM _地图区域综合控制表;");
    if (!Result)
        return;

    do
    {
        AreaFuncTemp St;
        Field* Fields = Result->Fetch();
        int32 MapOrArea = Fields[0].Get<int32>();
        std::string_view TypeStr = Fields[1].Get<std::string_view>();
        const char* StrEnum = TypeStr.data();
        if (StrEnum && *StrEnum != '\0')
        {
            if (strcmp(StrEnum, "不作改变") == 0)
                St.FuncEnum = NullPtr;
            else if (strcmp(StrEnum, "安全地区") == 0)
                St.FuncEnum = Sanctuary;
            else if (strcmp(StrEnum, "全体攻击") == 0)
                St.FuncEnum = AllsAttck;
            else if (strcmp(StrEnum, "组队攻击") == 0)
                St.FuncEnum = GroupAttck;
            else if (strcmp(StrEnum, "公会攻击") == 0)
                St.FuncEnum = GuildAttck;
        }

        St.AllowIn = Fields[2].Get<bool>();
        St.AllowDuel = Fields[3].Get<bool>();
        St.AllowGroup = Fields[4].Get<bool>();
        St.AllowMount = Fields[5].Get<bool>();
        St.AllowFlyMount = Fields[6].Get<bool>();

        St.AGraveyardTempId = Fields[7].Get<uint32>();
        St.HGraveyardTempId = Fields[8].Get<uint32>();

        St.DisSpells.clear();
        std::string StrSpells = Fields[9].Get<std::string>();
        if (!StrSpells.empty())
        {
            Tokenizer SpellsData(StrSpells, '#');
            for (auto const& Temp : SpellsData)
                if (sSpellMgr->GetSpellInfo(uint32(atoi(Temp))))
                    St.DisSpells.insert(uint32(atoi(Temp)));
        }

        St.DisAuras.clear();
        std::string StrAuras = Fields[10].Get<std::string>();
        if (!StrAuras.empty())
        {
            Tokenizer AurasData(StrAuras, '#');
            for (auto const& Temp : AurasData)
                if (sSpellMgr->GetSpellInfo(uint32(atoi(Temp))))
                    St.DisAuras.insert(uint32(atoi(Temp)));
        }

        St.InMsg = Fields[11].Get<std::string>();

        AreaFuncTempStore.insert(std::make_pair(MapOrArea, St));

    } while (Result->NextRow());
}

bool MapAreaFunc::AllowGroupHandler(Player* player)
{
    bool Allow = true;
    int32 NowMapId = (int32)player->GetMapId();
    auto Itr = AreaFuncTempStore.find(NowMapId);
    if (Itr != AreaFuncTempStore.end())
    {
        if (!Itr->second.AllowGroup)
            Allow = false;

        if (Itr->second.FuncEnum == AllsAttck || Itr->second.FuncEnum == GuildAttck)
            Allow = false;
    }

    int32 NowAreaId = -(int32)player->GetAreaId();
    Itr = AreaFuncTempStore.find(NowAreaId);
    if (Itr != AreaFuncTempStore.end())
    {
        if (!Itr->second.AllowGroup)
            Allow = false;

        if (Itr->second.FuncEnum == AllsAttck || Itr->second.FuncEnum == GuildAttck)
            Allow = false;
    }

    return Allow;
}

bool MapAreaFunc::AllowDuelHandler(Player* player)
{
    bool Allow = true;
    int32 NowMapId = (int32)player->GetMapId();
    auto Itr = AreaFuncTempStore.find(NowMapId);
    if (Itr != AreaFuncTempStore.end())
    {
        if (!Itr->second.AllowDuel)
            Allow = false;
    }

    int32 NowAreaId = -(int32)player->GetAreaId();
    Itr = AreaFuncTempStore.find(NowAreaId);
    if (Itr != AreaFuncTempStore.end())
    {
        if (!Itr->second.AllowDuel)
            Allow = false;
    }

    return Allow;
}

bool MapAreaFunc::AllowMountHandler(Player* player)
{
    bool Allow = true;
    int32 NowMapId = (int32)player->GetMapId();
    auto Itr = AreaFuncTempStore.find(NowMapId);
    if (Itr != AreaFuncTempStore.end())
    {
        if (!Itr->second.AllowMount)
            Allow = false;
    }

    int32 NowAreaId = -(int32)player->GetAreaId();
    Itr = AreaFuncTempStore.find(NowAreaId);
    if (Itr != AreaFuncTempStore.end())
    {
        if (!Itr->second.AllowMount)
            Allow = false;
    }

    return Allow;
}

bool MapAreaFunc::AllowFlyMountHandler(Player* player)
{
    bool Allow = true;
    int32 NowMapId = (int32)player->GetMapId();
    auto Itr = AreaFuncTempStore.find(NowMapId);
    if (Itr != AreaFuncTempStore.end())
    {
        if (!Itr->second.AllowFlyMount)
            Allow = false;
    }

    int32 NowAreaId = -(int32)player->GetAreaId();
    Itr = AreaFuncTempStore.find(NowAreaId);
    if (Itr != AreaFuncTempStore.end())
    {
        if (!Itr->second.AllowFlyMount)
            Allow = false;
    }

    return Allow;
}

bool MapAreaFunc::AllowCastSpellIdHandler(Unit* Caster, uint32 SpellId)
{
    bool Allow = true;
    int32 NowMapId = (int32)Caster->GetMapId();
    auto Itr = AreaFuncTempStore.find(NowMapId);
    if (Itr != AreaFuncTempStore.end())
    {
        if (!Itr->second.DisSpells.empty())
        {
            auto It = Itr->second.DisSpells.find(SpellId);
            if (It != Itr->second.DisSpells.end())
                Allow = false;
        }
    }

    int32 NowAreaId = -(int32)Caster->GetAreaId();
    Itr = AreaFuncTempStore.find(NowAreaId);
    if (Itr != AreaFuncTempStore.end())
    {
        if (!Itr->second.DisSpells.empty())
        {
            auto It = Itr->second.DisSpells.find(SpellId);
            if (It != Itr->second.DisSpells.end())
                Allow = false;
        }
    }

    return Allow;
}

bool MapAreaFunc::AllowAuraIdHandler(Player* player, uint32 AuraId)
{
    bool Allow = true;
    int32 NowMapId = (int32)player->GetMapId();
    auto Itr = AreaFuncTempStore.find(NowMapId);
    if (Itr != AreaFuncTempStore.end())
    {
        if (!Itr->second.DisAuras.empty())
        {
            auto It = Itr->second.DisAuras.find(AuraId);
            if (It != Itr->second.DisAuras.end())
                Allow = false;
        }
    }

    int32 NowAreaId = -(int32)player->GetAreaId();
    Itr = AreaFuncTempStore.find(NowAreaId);
    if (Itr != AreaFuncTempStore.end())
    {
        if (!Itr->second.DisAuras.empty())
        {
            auto It = Itr->second.DisAuras.find(AuraId);
            if (It != Itr->second.DisAuras.end())
                Allow = false;
        }
    }

    return Allow;
}

bool MapAreaFunc::IsDisAuraFunc(Player* player)
{
    bool Dis = true;
    int32 NowMapId = (int32)player->GetMapId();
    auto Itr = AreaFuncTempStore.find(NowMapId);
    if (Itr != AreaFuncTempStore.end())
        if (Itr->second.DisAuras.empty())
            Dis = false;

    int32 NowAreaId = -(int32)player->GetAreaId();
    Itr = AreaFuncTempStore.find(NowAreaId);
    if (Itr != AreaFuncTempStore.end())
        if (Itr->second.DisAuras.empty())
            Dis = false;

    return Dis;
}

class OnPlayerMapAreaFunc : public PlayerScript
{
public:
    OnPlayerMapAreaFunc() : PlayerScript("OnPlayerMapAreaFunc") {  }

    void OnMapChanged(Player* player) override
    {
        if (IsXsPlayerbot(player))
            return;

        int32 NowMapId = (int32)player->GetMapId();
        auto Itr = AreaFuncTempStore.find(NowMapId);
        if (Itr == AreaFuncTempStore.end())
        {
            //player->AsSetFactionForRace(player->GetRace());
            return;
        }

        if (!Itr->second.AllowIn)
        {
            player->SendSysMsg(sHelper->GetGts(9903));
            player->SetCanTeleport(true);
            player->TeleportToEntryPoint();
            return;
        }

        if (!Itr->second.AllowGroup)
        {
            //不能组队
            if (player->GetGroup())
                player->RemoveFromGroup();
        }

        if (!Itr->second.InMsg.empty())
            player->SendSysMsg(Itr->second.InMsg.c_str());

        if (Itr->second.FuncEnum == NullPtr)
        {
            //player->AsSetFactionForRace(player->GetRace());
            return;
        }

        switch (Itr->second.FuncEnum)
        {
        case Sanctuary:
        {
            player->pvpInfo.IsInFFAPvPArea = false;
            player->UpdateFFAPvPState(false);
            player->SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
            player->pvpInfo.IsInNoPvPArea = true;

            player->CombatStopWithPets();
        }
        break;
        case AllsAttck:
        {
            //不能组队
            if (player->GetGroup())
                player->RemoveFromGroup();

            player->pvpInfo.IsInFFAPvPArea = true;
            player->UpdateFFAPvPState(false);
            player->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
            player->pvpInfo.IsInNoPvPArea = false;
        }
        break;
        case GroupAttck:
        {
            player->pvpInfo.IsInFFAPvPArea = true;
            player->UpdateFFAPvPState(false);
            player->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
            player->pvpInfo.IsInNoPvPArea = false;

            player->UpdatePvPState();
        }
        break;
        case GuildAttck:
        {

        }
        break;
        }
    }

    void OnUpdateAreaEnd(Player* player, uint32 newArea) override
    {
        if (IsXsPlayerbot(player))
            return;

        int32 NowMapId = (int32)player->GetMapId();
        auto Iter = AreaFuncTempStore.find(NowMapId);
        if (Iter != AreaFuncTempStore.end())
        {
            if (!Iter->second.AllowIn)
            {
                player->SendSysMsg(sHelper->GetGts(9903));
                player->SetCanTeleport(true);
                player->TeleportToEntryPoint();
                return;
            }

            if (!Iter->second.AllowGroup)
            {
                //不能组队
                if (player->GetGroup())
                    player->RemoveFromGroup();
            }

            if (!Iter->second.InMsg.empty())
                player->SendSysMsg(Iter->second.InMsg.c_str());

            if (Iter->second.FuncEnum == NullPtr)
            {
                //player->AsSetFactionForRace(player->GetRace());
                return;
            }

            switch (Iter->second.FuncEnum)
            {
            case Sanctuary:
            {
                player->pvpInfo.IsInFFAPvPArea = false;
                player->UpdateFFAPvPState(false);
                player->SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
                player->pvpInfo.IsInNoPvPArea = true;

                player->CombatStopWithPets();
            }
            break;
            case AllsAttck:
            {
                //不能组队
                if (player->GetGroup())
                    player->RemoveFromGroup();

                player->pvpInfo.IsInFFAPvPArea = true;
                player->UpdateFFAPvPState(false);
                player->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
                player->pvpInfo.IsInNoPvPArea = false;
            }
            break;
            case GroupAttck:
            {
                player->pvpInfo.IsInFFAPvPArea = true;
                player->UpdateFFAPvPState(false);
                player->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
                player->pvpInfo.IsInNoPvPArea = false;

                player->UpdatePvPState();
            }
            break;
            case GuildAttck:
            {

            }
            break;
            }

            return;
        }

        int32 NowAreaId = -(int32)newArea;
        auto Itr = AreaFuncTempStore.find(NowAreaId);
        if (Itr == AreaFuncTempStore.end())
        {
            //player->AsSetFactionForRace(player->GetRace());
            return;
        }

        if (!Itr->second.AllowIn)
        {
            player->SendSysMsg(sHelper->GetGts(9903));
            player->SetCanTeleport(true);
            player->TeleportToEntryPoint();
            return;
        }

        if (!Itr->second.AllowGroup)
        {
            //不能组队
            if (player->GetGroup())
                player->RemoveFromGroup();
        }

        if (!Itr->second.InMsg.empty())
            player->SendSysMsg(Itr->second.InMsg.c_str());

        if (Itr->second.FuncEnum == NullPtr)
        {
            //player->AsSetFactionForRace(player->GetRace());
            return;
        }

        switch (Itr->second.FuncEnum)
        {
        case Sanctuary:
        {
            player->pvpInfo.IsInFFAPvPArea = false;
            player->UpdateFFAPvPState(false);
            player->SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
            player->pvpInfo.IsInNoPvPArea = true;

            player->CombatStopWithPets();
        }
        break;
        case AllsAttck:
        {
            if (player->GetGroup())
                player->RemoveFromGroup();

            player->pvpInfo.IsInFFAPvPArea = true;
            player->UpdateFFAPvPState(false);
            player->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
            player->pvpInfo.IsInNoPvPArea = false;
        }
        break;
        case GroupAttck:
        {
            player->pvpInfo.IsInFFAPvPArea = true;
            player->UpdateFFAPvPState(false);
            player->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SANCTUARY);
            player->pvpInfo.IsInNoPvPArea = false;

            player->UpdatePvPState();
        }
        break;
        case GuildAttck:
        {

        }
        break;
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
        auto Itr = AreaFuncTempStore.find(AreaId);
        if (Itr != AreaFuncTempStore.end())
        {
            if (player->GetTeamId() == TEAM_ALLIANCE)
            {
                if (Itr->second.AGraveyardTempId)
                {
                    player->SpawnCorpseBones();
                    player->TeleToPos(Itr->second.AGraveyardTempId);
                    return false;
                }
            }
            else if (player->GetTeamId() == TEAM_HORDE)
            {
                if (Itr->second.HGraveyardTempId)
                {
                    player->SpawnCorpseBones();
                    player->TeleToPos(Itr->second.HGraveyardTempId);
                    return false;
                }
            }
        }

        Itr = AreaFuncTempStore.find(MapId);
        if (Itr != AreaFuncTempStore.end())
        {
            if (player->GetTeamId() == TEAM_ALLIANCE)
            {
                if (Itr->second.AGraveyardTempId)
                {
                    player->SpawnCorpseBones();
                    player->TeleToPos(Itr->second.AGraveyardTempId);
                    return false;
                }
            }
            else if (player->GetTeamId() == TEAM_HORDE)
            {
                if (Itr->second.HGraveyardTempId)
                {
                    player->SpawnCorpseBones();
                    player->TeleToPos(Itr->second.HGraveyardTempId);
                    return false;
                }
            }
        }

        return true;
    }
};


void AddSC_CustomMapAreaFuncScript()
{
    new OnPlayerMapAreaFunc();
}
