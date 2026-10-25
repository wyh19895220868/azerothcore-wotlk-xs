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
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
#include <string_view>
#include <unordered_set>
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include <Time\GameTime.h>
#include "PlayerAction.h"
#pragma execution_character_set("utf-8")

namespace
{
std::shared_ptr<const PlrActTempMap> PlrActTempStore = std::make_shared<const PlrActTempMap>();

std::shared_ptr<const PlrActTempMap> GetPlayerActionSnapshot()
{
    return std::atomic_load_explicit(&PlrActTempStore, std::memory_order_acquire);
}

thread_local std::unordered_set<uint32> ActivePlayerActionRules;

class PlayerActionRuleScope
{
public:
    explicit PlayerActionRuleScope(uint32 ruleId) : _ruleId(ruleId), _entered(ActivePlayerActionRules.insert(ruleId).second) { }

    ~PlayerActionRuleScope()
    {
        if (_entered)
            ActivePlayerActionRules.erase(_ruleId);
    }

    bool Entered() const { return _entered; }

private:
    uint32 _ruleId;
    bool _entered;
};

std::string_view GetMessageArgumentSignature(ActionTypes actionType)
{
    switch (actionType)
    {
    case PLR_SYS:
    case PLR_REV_WHISPER:
    case MAP_CG:
    case AREA_CG:
    case ZONE_CG:
    case PLR_LOGIN:
    case PLR_LOGOUT:
    case PLR_FIRST_LOGIN:
    case PLR_REVIVE:
    case PLR_REPOP:
    case PLR_LEARN_COMBAT_START:
    case PLR_LEARN_COMBAT_STOP:
    case PLR_RESET_TALENT:
    case PLR_MODE_CG:
    case PLR_EMOTE:
    case PLR_MOVE:
    case PLR_DIE:
        return "s";
    case KILL_CRE:
    case KILL_PLR:
    case CM_QET:
    case CM_ACH:
    case B_KILL_PLR:
    case B_KILL_CRE:
    case MAP_LEAVE:
    case AREA_LEAVE:
    case CAST_SPELL:
    case PLR_EQUIP:
    case PLR_LEARN_SPELL:
    case PLR_UN_LEARN_SPELL:
    case PLR_DUEL_START:
    case PLR_DUEL_STOP:
    case PLR_LEARN_TALENT:
    case PLR_ADD_AURA:
    case PLR_REMOVE_AURA:
    case PLR_MINING:
    case PLR_HERBALISM:
    case PLR_FISHING:
    case CREATURE_MOVE:
    case PLR_USE_ITEM:
    case PLR_ACCEPT_QET:
    case PLR_CANCEL_QET:
        return "ss";
    case PLR_MONEY_CG:
    case PLR_XP:
    case PLR_REPUTATION:
        return "su";
    case PLR_LOOT:
    case PLR_QET_RW_ITEM:
        return "ssu";
    case LEVEL_CG:
    case VIP_RANK_CG:
        return "suu";
    case PLR_DUEL_WIN:
    case PLR_DUEL_LOSER:
        return "sss";
    default:
        return {};
    }
}

bool IsSafeMessageFormat(std::string_view format)
{
    for (size_t i = 0; i < format.size(); ++i)
    {
        if (format[i] != '%')
            continue;

        if (++i >= format.size() || (format[i] != '%' && format[i] != 's' && format[i] != 'u'))
            return false;
    }

    return true;
}

bool IsValidMessageFormat(ActionTypes actionType, std::string_view format)
{
    if (!IsSafeMessageFormat(format))
        return false;

    std::string_view signature = GetMessageArgumentSignature(actionType);
    size_t argumentIndex = 0;

    for (size_t i = 0; i < format.size(); ++i)
    {
        if (format[i] != '%')
            continue;

        if (++i >= format.size())
            return false;

        char specifier = format[i];
        if (specifier == '%')
            continue;

        if ((specifier != 's' && specifier != 'u') || argumentIndex >= signature.size() || signature[argumentIndex] != specifier)
            return false;

        ++argumentIndex;
    }

    return true;
}

ActionTypes GetGatherAction(GameObject const* gameObject)
{
    if (!gameObject)
        return Bng;

    if (gameObject->GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE || gameObject->GetGoType() == GAMEOBJECT_TYPE_FISHINGHOLE)
        return PLR_FISHING;

    LockEntry const* lock = sLockStore.LookupEntry(gameObject->GetGOInfo()->GetLockId());
    if (!lock)
        return Bng;

    for (uint8 i = 0; i < MAX_LOCK_CASE; ++i)
    {
        if (lock->Type[i] != LOCK_KEY_SKILL)
            continue;

        if (lock->Index[i] == LOCKTYPE_MINING)
            return PLR_MINING;
        if (lock->Index[i] == LOCKTYPE_HERBALISM)
            return PLR_HERBALISM;
    }

    return Bng;
}

std::string GetGameObjectName(uint32 entry)
{
    if (GameObjectTemplate const* gameObjectTemplate = sObjectMgr->GetGameObjectTemplate(entry))
        return gameObjectTemplate->name;

    return "游戏物体 " + std::to_string(entry);
}
}

void PlayerAction::LoadPlayerAction()
{
    auto NewStore = std::make_shared<PlrActTempMap>();
    QueryResult Result = WorldDatabase.Query("SELECT 序号, 唯一, 动作, 内容, 触发几率, 触发间隔, 关联事件, 关联地图, 关联区域, 关联地区, 需求模版, 奖励模版, 公告类型, 公告内容 FROM _动作触发功能表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            PlrActTemp St{};
            St.WType = NOT;
            St.AType = Bng;
            St.Num = Fields[0].Get<uint32>();

            std::string_view WTypeStr = Fields[1].Get<std::string_view>();
            const char* WTypeCh = WTypeStr.data();
            if (WTypeCh && *WTypeCh != '\0')
            {
                if (strcmp(WTypeCh, "不限制") == 0)
                    St.WType = NOT;
                else if (strcmp(WTypeCh, "全服唯一") == 0)
                    St.WType = AS_ONE;
                else if (strcmp(WTypeCh, "账号唯一") == 0)
                    St.WType = ACCOUNT_ONE;
                else if (strcmp(WTypeCh, "角色唯一") == 0)
                    St.WType = PLR_ONE;
            }

            std::string_view TypeStr = Fields[2].Get<std::string_view>();
            St.ATypeStr = TypeStr;
            const char* TypeCh = TypeStr.data();
            if (TypeCh && *TypeCh != '\0')
            {
                if (strcmp(TypeCh, "说话") == 0)
                    St.AType = PLR_SYS;
                else if (strcmp(TypeCh, "收到密语") == 0)
                    St.AType = PLR_REV_WHISPER;
                else if (strcmp(TypeCh, "击杀生物") == 0)
                    St.AType = KILL_CRE;
                else if (strcmp(TypeCh, "击杀玩家") == 0)
                    St.AType = KILL_PLR;
                else if (strcmp(TypeCh, "完成任务") == 0)
                    St.AType = CM_QET;
                else if (strcmp(TypeCh, "完成成就") == 0)
                    St.AType = CM_ACH;
                else if (strcmp(TypeCh, "被玩家击杀") == 0)
                    St.AType = B_KILL_PLR;
                else if (strcmp(TypeCh, "被生物击杀") == 0)
                    St.AType = B_KILL_CRE;
                else if (strcmp(TypeCh, "拾取指定物品") == 0)
                    St.AType = PLR_LOOT;
                else if (strcmp(TypeCh, "等级变化") == 0)
                    St.AType = LEVEL_CG;
                else if (strcmp(TypeCh, "切换地图") == 0)
                    St.AType = MAP_CG;
                else if (strcmp(TypeCh, "切换区域") == 0)
                    St.AType = AREA_CG;
                else if (strcmp(TypeCh, "离开地图") == 0)
                    St.AType = MAP_LEAVE;
                else if (strcmp(TypeCh, "切换地区") == 0)
                    St.AType = ZONE_CG;
                else if (strcmp(TypeCh, "离开区域") == 0)
                    St.AType = AREA_LEAVE;
                else if (strcmp(TypeCh, "释放技能") == 0)
                    St.AType = CAST_SPELL;
                else if (strcmp(TypeCh, "登陆") == 0)
                    St.AType = PLR_LOGIN;
                else if (strcmp(TypeCh, "登出") == 0)
                    St.AType = PLR_LOGOUT;
                else if (strcmp(TypeCh, "第一次登陆") == 0)
                    St.AType = PLR_FIRST_LOGIN;
                else if (strcmp(TypeCh, "VIP等级变化") == 0)
                    St.AType = VIP_RANK_CG;
                else if (strcmp(TypeCh, "复活") == 0)
                    St.AType = PLR_REVIVE;
                else if (strcmp(TypeCh, "装备物品") == 0)
                    St.AType = PLR_EQUIP;
                else if (strcmp(TypeCh, "金币变化") == 0)
                    St.AType = PLR_MONEY_CG;
                else if (strcmp(TypeCh, "获得经验") == 0)
                    St.AType = PLR_XP;
                else if (strcmp(TypeCh, "获得声望") == 0)
                    St.AType = PLR_REPUTATION;
                else if (strcmp(TypeCh, "学习技能") == 0)
                    St.AType = PLR_LEARN_SPELL;
                else if (strcmp(TypeCh, "遗忘技能") == 0)
                    St.AType = PLR_UN_LEARN_SPELL;
                else if (strcmp(TypeCh, "决斗开始") == 0)
                    St.AType = PLR_DUEL_START;
                else if (strcmp(TypeCh, "决斗中止") == 0)
                    St.AType = PLR_DUEL_STOP;
                else if (strcmp(TypeCh, "决斗胜利") == 0)
                    St.AType = PLR_DUEL_WIN;
                else if (strcmp(TypeCh, "决斗失败") == 0)
                    St.AType = PLR_DUEL_LOSER;
                else if (strcmp(TypeCh, "释放尸体") == 0)
                    St.AType = PLR_REPOP;
                else if (strcmp(TypeCh, "学习天赋") == 0)
                    St.AType = PLR_LEARN_TALENT;
                else if (strcmp(TypeCh, "进入战斗") == 0)
                    St.AType = PLR_LEARN_COMBAT_START;
                else if (strcmp(TypeCh, "结束战斗") == 0)
                    St.AType = PLR_LEARN_COMBAT_STOP;
                else if (strcmp(TypeCh, "重置天赋") == 0)
                    St.AType = PLR_RESET_TALENT;
                else if (strcmp(TypeCh, "获得光环") == 0)
                    St.AType = PLR_ADD_AURA;
                else if (strcmp(TypeCh, "失去光环") == 0)
                    St.AType = PLR_REMOVE_AURA;
                else if (strcmp(TypeCh, "模型改变") == 0)
                    St.AType = PLR_MODE_CG;
                else if (strcmp(TypeCh, "表情") == 0)
                    St.AType = PLR_EMOTE;
                else if (strcmp(TypeCh, "采矿") == 0)
                    St.AType = PLR_MINING;
                else if (strcmp(TypeCh, "采药") == 0)
                    St.AType = PLR_HERBALISM;
                else if (strcmp(TypeCh, "钓鱼") == 0)
                    St.AType = PLR_FISHING;
                else if (strcmp(TypeCh, "玩家移动") == 0)
                    St.AType = PLR_MOVE;
                else if (strcmp(TypeCh, "生物移动") == 0)
                    St.AType = CREATURE_MOVE;
                else if (strcmp(TypeCh, "使用指定物品") == 0)
                    St.AType = PLR_USE_ITEM;
                else if (strcmp(TypeCh, "接受任务") == 0)
                    St.AType = PLR_ACCEPT_QET;
                else if (strcmp(TypeCh, "放弃任务") == 0)
                    St.AType = PLR_CANCEL_QET;
                else if (strcmp(TypeCh, "玩家死亡") == 0)
                    St.AType = PLR_DIE;
                else if (strcmp(TypeCh, "任务奖励物品") == 0)
                    St.AType = PLR_QET_RW_ITEM;
            }

            St.Contents = Fields[3].Get<std::string>();
            St.ExOdds = Fields[4].Get<float>();
            int64 RawDelay = Fields[5].Get<int64>();
            St.ExDelay = RawDelay > 0 ? static_cast<uint32>(RawDelay) : 0;
            St.GEventId = Fields[6].Get<uint16>();
            St.GMapId = Fields[7].Get<int32>();
            St.GAreaId = Fields[8].Get<int32>();
            St.GZoneId = Fields[9].Get<int32>();
            St.NTempId = Fields[10].Get<uint32>();
            St.RTempId = Fields[11].Get<uint32>();
            St.GtsType = Fields[12].Get<uint32>();
            St.Gts = Fields[13].Get<std::string>();

            constexpr int64 MaxDelaySeconds = std::numeric_limits<uint32>::max() / IN_MILLISECONDS;
            if (RawDelay < 0 || RawDelay > MaxDelaySeconds)
            {
                LOG_ERROR("server.loading", "Skipping action rule {}: delay {} is outside the supported range", St.Num, RawDelay);
                continue;
            }

            if (St.Num == 0 || St.AType == Bng || !std::isfinite(St.ExOdds) || St.ExOdds < 0.0f || St.ExOdds > 100.0f || St.GtsType > 3)
            {
                LOG_ERROR("server.loading", "跳过无效动作触发配置: 序号={}, 动作='{}', 几率={}, 公告类型={}", St.Num, St.ATypeStr, St.ExOdds, St.GtsType);
                continue;
            }
            if ((St.AType == CREATURE_MOVE || St.AType == PLR_USE_ITEM) && St.Contents.empty())
            {
                LOG_ERROR("server.loading", "跳过无效动作触发配置: 序号={}，动作={}必须在内容中填写指定ID", St.Num, St.ATypeStr);
                continue;
            }
            if (St.GtsType != 0 && !St.Gts.empty() && !IsValidMessageFormat(St.AType, St.Gts))
            {
                LOG_ERROR("server.loading", "Skipping action rule {}: unsafe or mismatched announcement format", St.Num);
                continue;
            }
            if (St.RTempId != 0 && St.ExDelay == 0)
            {
                St.ExDelay = 1;
                LOG_WARN("server.loading", "Action rule {} has a reward and zero delay; clamped to 1 second", St.Num);
            }
            if ((St.AType == PLR_MOVE || St.AType == CREATURE_MOVE) && St.ExDelay == 0)
            {
                St.ExDelay = 1;
                LOG_WARN("server.loading", "动作触发配置序号={} 的移动触发间隔不能为0，已按1秒处理", St.Num);
            }

            NewStore->insert(std::make_pair(St.Num, St));

        } while (Result->NextRow());
    }

    std::shared_ptr<const PlrActTempMap> PublishedStore = NewStore;
    std::atomic_store_explicit(&PlrActTempStore, std::move(PublishedStore), std::memory_order_release);
}

uint32 PlayerAction::GetActionCount() const
{
    auto Store = GetPlayerActionSnapshot();
    return Store ? static_cast<uint32>(Store->size()) : 0;
}

bool PlayerAction::CheckContent(std::string Contents, std::string Content)
{
    Tokenizer Datas(Contents, '#');
    if (Datas.IsEmpty())
        return false;

    for (auto const& Info : Datas)
    {
        if (Content == std::string(Info))
            return true;
    }

    return false;
}

bool PlayerAction::CheckContent(std::string Contents, uint32 AsEx)
{
    Tokenizer Datas(Contents, '#');
    if (Datas.IsEmpty())
        return false;

    for (auto const& Info : Datas)
    {
        if (AsEx == uint32(atoi(Info)))
            return true;
    }

    return false;
}

void PlayerAction::SendMessageForGtsType(Player* player, uint32 GtsType, const char* Format, ...)
{
    if (!Format || !*Format)
        return;
    if (!IsSafeMessageFormat(Format))
    {
        LOG_ERROR("server", "Blocked unsafe player-action announcement format");
        return;
    }

    va_list Ap;
    char Buff[1024]{};
    va_start(Ap, Format);
    int Written = vsnprintf(Buff, sizeof(Buff), Format, Ap);
    va_end(Ap);
    if (Written < 0)
        return;

    if (player)
    {
        if (GtsType == 1)
            ChatHandler(player->GetSession()).SendSysMessage(Buff);
    }
    else
    {
        SessionMap const& Sessions = sWorld->GetAllSessions();
        for (auto Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
        {
            if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
                continue;

            if (GtsType == 2)
                ChatHandler(Itr->second->GetPlayer()->GetSession()).SendSysMessage(Buff);
            else if (GtsType == 3)
                Itr->second->GetPlayer()->GetSession()->SendAreaTriggerMessage(Buff);
        }
    }
}

void PlayerAction::DoGtsForAType(Player* player, ActionTypes AType, uint32 GtsType, std::string Gts, Player* ExPlayer, uint32 AsEx1, uint32 AsEx2)
{
    if (!GtsType || Gts.empty() || !sHelper)
        return;

    if (player && !player->IsInWorld()) return;
    if (ExPlayer && !ExPlayer->IsInWorld()) return;

    Player* DoPlayer = (GtsType == 1) ? player : nullptr;
    switch (AType)
    {
    case PLR_SYS:
    case PLR_REV_WHISPER:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case KILL_CRE:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetCreatureNameByEntry(AsEx1).c_str());
        break;
    case KILL_PLR:
        if (player && ExPlayer)
            SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetNameLinkColor(ExPlayer).c_str());
        break;
    case CM_QET:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetQuestName(AsEx1));
        break;
    case CM_ACH:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetAchievementName(AsEx1));
        break;
    case B_KILL_PLR:
        if (player && ExPlayer)
            SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetNameLinkColor(ExPlayer).c_str());
        break;
    case B_KILL_CRE:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetCreatureNameByEntry(AsEx1).c_str());
        break;
    case PLR_LOOT:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetItemLink(AsEx1).c_str(), AsEx2);
        break;
    case LEVEL_CG:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), AsEx1, AsEx2);
        break;
    case MAP_CG:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case AREA_CG:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case MAP_LEAVE:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetMapName(AsEx1));
        break;
    case ZONE_CG:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case AREA_LEAVE:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetAreaName(AsEx1));
        break;
    case CAST_SPELL:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetSpellName(AsEx1));
        break;
    case PLR_LOGIN:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case PLR_LOGOUT:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case PLR_FIRST_LOGIN:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case VIP_RANK_CG:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), AsEx1, AsEx2);
        break;
    case PLR_REVIVE:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case PLR_EQUIP:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetItemLink(AsEx1).c_str());
        break;
    case PLR_MONEY_CG:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), AsEx1);
        break;
    case PLR_XP:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), AsEx1);
        break;
    case PLR_REPUTATION:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), AsEx1);
        break;
    case PLR_LEARN_SPELL:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetSpellName(AsEx1));
        break;
    case PLR_UN_LEARN_SPELL:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetSpellName(AsEx1));
        break;
    case PLR_DUEL_START:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetNameLinkColor(ExPlayer).c_str());
        break;
    case PLR_DUEL_STOP:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetNameLinkColor(ExPlayer).c_str());
        break;
    case PLR_DUEL_WIN:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetNameLinkColor(ExPlayer).c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case PLR_DUEL_LOSER:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetNameLinkColor(ExPlayer).c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case PLR_REPOP:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case PLR_LEARN_TALENT:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetSpellName(AsEx1));
        break;
    case PLR_LEARN_COMBAT_START:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case PLR_LEARN_COMBAT_STOP:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case PLR_RESET_TALENT:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case PLR_ADD_AURA:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetSpellName(AsEx1));
        break;
    case PLR_REMOVE_AURA:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetSpellName(AsEx1));
        break;
    case PLR_MODE_CG:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case PLR_EMOTE:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case PLR_MINING:
    case PLR_HERBALISM:
    case PLR_FISHING:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), GetGameObjectName(AsEx1).c_str());
        break;
    case PLR_MOVE:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case CREATURE_MOVE:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetCreatureNameByEntry(AsEx1).c_str());
        break;
    case PLR_USE_ITEM:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetItemLink(AsEx1).c_str());
        break;
    case PLR_ACCEPT_QET:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetQuestName(AsEx1));
        break;
    case PLR_CANCEL_QET:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetQuestName(AsEx1));
        break;
    case PLR_DIE:
        SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str());
        break;
    case PLR_QET_RW_ITEM:
        if (player)
            SendMessageForGtsType(DoPlayer, GtsType, Gts.c_str(), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetItemLink(AsEx1).c_str(), AsEx2);
        break;
    }
}

void PlayerAction::DoChatExecute(Player* player, ActionTypes AType, Player* ExPlayer, std::string Content)
{
    auto Store = GetPlayerActionSnapshot();
    if (!Store || Store->empty() || !sHelper)
        return;

    if (!player || !player->IsInWorld()) return;
    if (ExPlayer && !ExPlayer->IsInWorld()) return;

    for (auto Itr = Store->begin(); Itr != Store->end(); ++Itr)
    {
        PlayerActionRuleScope RuleScope(Itr->second.Num);
        if (!RuleScope.Entered())
            continue;

        if (Itr->second.AType == AType
            && (Content.empty() || CheckContent(Itr->second.Contents, Content))
            && roll_chance_f(Itr->second.ExOdds)
            && (Itr->second.WType == NOT || (Itr->second.WType == AS_ONE && !IsAsOne(player, Itr->second.Num)) || (Itr->second.WType == ACCOUNT_ONE && !IsAccountOne(player, Itr->second.Num)) || (Itr->second.WType == PLR_ONE && !IsPlayerOne(player, Itr->second.Num)))
            && (Itr->second.GEventId == 0 || sGameEventMgr->IsActiveEvent(Itr->second.GEventId))
            && (Itr->second.GMapId == -1 || player->GetMapId() == Itr->second.GMapId)
            && (Itr->second.GAreaId == -1 || player->GetAreaId() == Itr->second.GAreaId)
            && (Itr->second.GZoneId == -1 || player->GetZoneId() == Itr->second.GZoneId)
            && IsCooldownReady(player, Itr->second.Num)
            && (Itr->second.NTempId == 0 || sHelper->CheckNeed(player, Itr->second.NTempId, ("动作触发-" + Itr->second.ATypeStr + "-需求")))
            )
        {
            SetCooldown(player, Itr->second.Num, Itr->second.ExDelay * IN_MILLISECONDS);
            MarkUnique(player, Itr->second.WType, Itr->second.Num);
            DoGtsForAType(player, Itr->second.AType, Itr->second.GtsType, Itr->second.Gts, ExPlayer, 0, 0);
            sHelper->DoTheReward(player, Itr->second.RTempId, ("动作触发-" + Itr->second.ATypeStr + "-奖励"));
        }
    }
}

bool PlayerAction::HasAction(ActionTypes AType, uint32 AsEx1)
{
    auto Store = GetPlayerActionSnapshot();
    if (!Store)
        return false;

    for (auto const& [ruleId, rule] : *Store)
    {
        if (rule.AType != AType)
            continue;
        if (AsEx1 == 0 || CheckContent(rule.Contents, AsEx1))
            return true;
    }

    return false;
}

bool PlayerAction::HasPlayerMoved(Player* player, uint32 diff)
{
    if (!player)
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    std::lock_guard<std::mutex> lock(_cooldownMutex);
    PlayerActionMovementState& state = _movementMap[playerGuid];
    state.ElapsedMs += diff;
    if (state.ElapsedMs < IN_MILLISECONDS)
        return false;

    state.ElapsedMs = 0;
    float x = player->GetPositionX();
    float y = player->GetPositionY();
    float z = player->GetPositionZ();
    bool moved = state.Initialized && player->GetExactDistSq(state.X, state.Y, state.Z) >= 0.01f;
    state.X = x;
    state.Y = y;
    state.Z = z;
    state.Initialized = true;
    return moved;
}

bool PlayerAction::ShouldTriggerGather(Player* player, ObjectGuid lootGuid)
{
    if (!player || lootGuid.IsEmpty())
        return false;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint64 currentTime = GameTime::GetGameTimeMS().count();
    std::lock_guard<std::mutex> lock(_cooldownMutex);
    PlayerActionGatherState& state = _gatherMap[playerGuid];
    if (state.LootGuid == lootGuid.GetRawValue() && currentTime - state.TriggerTimeMs < 10 * IN_MILLISECONDS)
        return false;

    state.LootGuid = lootGuid.GetRawValue();
    state.TriggerTimeMs = currentTime;
    return true;
}

void PlayerAction::QueueItemUse(Player* player, uint32 itemEntry)
{
    if (!player || !itemEntry || !HasAction(PLR_USE_ITEM, itemEntry))
        return;

    std::lock_guard<std::mutex> lock(_cooldownMutex);
    _itemUseMap[player->GetGUID().GetCounter()] =
        { itemEntry, static_cast<uint64>(GameTime::GetGameTimeMS().count()) };
}

void PlayerAction::ProcessPendingItemUse(Player* player)
{
    if (!player || player->IsNonMeleeSpellCast(false))
        return;

    uint32 itemEntry = 0;
    {
        std::lock_guard<std::mutex> lock(_cooldownMutex);
        auto itr = _itemUseMap.find(player->GetGUID().GetCounter());
        if (itr == _itemUseMap.end() || GameTime::GetGameTimeMS().count() - itr->second.QueuedTimeMs < 100)
            return;

        itemEntry = itr->second.ItemEntry;
        _itemUseMap.erase(itr);
    }

    DoExecute(player, PLR_USE_ITEM, nullptr, itemEntry, 0);
}

void PlayerAction::DoExecute(Player* player, ActionTypes AType, Player* ExPlayer, uint32 AsEx1, uint32 AsEx2)
{
    auto Store = GetPlayerActionSnapshot();
    if (!Store || Store->empty() || !sHelper)
        return;

    if (!player || !player->IsInWorld()) return;
    if (ExPlayer && !ExPlayer->IsInWorld()) return;

    for (auto Itr = Store->begin(); Itr != Store->end(); ++Itr)
    {
        PlayerActionRuleScope RuleScope(Itr->second.Num);
        if (!RuleScope.Entered())
            continue;

        bool At = (AType == KILL_PLR
            || AType == B_KILL_PLR
            || AType == LEVEL_CG
            || AType == MAP_CG
            || AType == AREA_CG
            || AType == MAP_LEAVE
            || AType == ZONE_CG
            || AType == AREA_LEAVE
            || AType == PLR_REVIVE
            || AType == PLR_MONEY_CG
            || AType == PLR_XP
            || AType == PLR_REPUTATION
            || AType == PLR_DUEL_START
            || AType == PLR_DUEL_STOP
            || AType == PLR_DUEL_WIN
            || AType == PLR_DUEL_LOSER
            || AType == PLR_REPOP
            || AType == PLR_LEARN_COMBAT_START
            || AType == PLR_LEARN_COMBAT_STOP
            || AType == PLR_RESET_TALENT
            || AType == PLR_MODE_CG
            || AType == PLR_MOVE
            || AType == PLR_DIE
            );
        bool AllowEmptyContent = AType == PLR_MINING || AType == PLR_HERBALISM || AType == PLR_FISHING;

        if (Itr->second.AType == AType
            && (At || AsEx1 == 0 || (AllowEmptyContent && Itr->second.Contents.empty()) || CheckContent(Itr->second.Contents, AsEx1))
            && roll_chance_f(Itr->second.ExOdds)
            && (Itr->second.WType == NOT || (Itr->second.WType == AS_ONE && !IsAsOne(player, Itr->second.Num)) || (Itr->second.WType == ACCOUNT_ONE && !IsAccountOne(player, Itr->second.Num)) || (Itr->second.WType == PLR_ONE && !IsPlayerOne(player, Itr->second.Num)))
            && (Itr->second.GEventId == 0 || sGameEventMgr->IsActiveEvent(Itr->second.GEventId))
            && (Itr->second.GMapId == -1 || player->GetMapId() == Itr->second.GMapId)
            && (Itr->second.GAreaId == -1 || player->GetAreaId() == Itr->second.GAreaId)
            && (Itr->second.GZoneId == -1 || player->GetZoneId() == Itr->second.GZoneId)
            && IsCooldownReady(player, Itr->second.Num)
            && (Itr->second.NTempId == 0 || sHelper->CheckNeed(player, Itr->second.NTempId, ("动作触发-" + Itr->second.ATypeStr + "-需求")))
            )
        {
            SetCooldown(player, Itr->second.Num, Itr->second.ExDelay * IN_MILLISECONDS);
            MarkUnique(player, Itr->second.WType, Itr->second.Num);
            DoGtsForAType(player, Itr->second.AType, Itr->second.GtsType, Itr->second.Gts, ExPlayer, AsEx1, AsEx2);
            sHelper->DoTheReward(player, Itr->second.RTempId, ("动作触发-" + Itr->second.ATypeStr + "-奖励"));
        }
    }
}

void PlayerAction::DoVipCgExecute(Player* player, ActionTypes AType, Player* ExPlayer, uint32 OldRank, uint32 NewRank)
{
    auto Store = GetPlayerActionSnapshot();
    if (!Store || Store->empty() || !sHelper)
        return;

    if (!player || !player->IsInWorld()) return;
    if (ExPlayer && !ExPlayer->IsInWorld()) return;

    for (auto Itr = Store->begin(); Itr != Store->end(); ++Itr)
    {
        PlayerActionRuleScope RuleScope(Itr->second.Num);
        if (!RuleScope.Entered())
            continue;

        if (Itr->second.AType == AType
            && roll_chance_f(Itr->second.ExOdds)
            && (Itr->second.WType == NOT || (Itr->second.WType == AS_ONE && !IsAsOne(player, Itr->second.Num)) || (Itr->second.WType == ACCOUNT_ONE && !IsAccountOne(player, Itr->second.Num)) || (Itr->second.WType == PLR_ONE && !IsPlayerOne(player, Itr->second.Num)))
            && (Itr->second.GEventId == 0 || sGameEventMgr->IsActiveEvent(Itr->second.GEventId))
            && (Itr->second.GMapId == -1 || player->GetMapId() == Itr->second.GMapId)
            && (Itr->second.GAreaId == -1 || player->GetAreaId() == Itr->second.GAreaId)
            && (Itr->second.GZoneId == -1 || player->GetZoneId() == Itr->second.GZoneId)
            && IsCooldownReady(player, Itr->second.Num)
            && (Itr->second.NTempId == 0 || sHelper->CheckNeed(player, Itr->second.NTempId, ("动作触发-" + Itr->second.ATypeStr + "-需求")))
            )
        {
            SetCooldown(player, Itr->second.Num, Itr->second.ExDelay * IN_MILLISECONDS);
            MarkUnique(player, Itr->second.WType, Itr->second.Num);
            DoGtsForAType(player, Itr->second.AType, Itr->second.GtsType, Itr->second.Gts, ExPlayer, OldRank, NewRank);
            sHelper->DoTheReward(player, Itr->second.RTempId, ("动作触发-" + Itr->second.ATypeStr + "-奖励"));
        }
    }
}

void PlayerAction::CustomVipChanged(Player* player, uint32 OldRank, uint32 NewRank)
{
    DoVipCgExecute(player, VIP_RANK_CG, nullptr, OldRank, NewRank);
}

void PlayerAction::DieNow(Player* player)
{
    sPlayerAction->DoExecute(player, PLR_DIE, nullptr, 0, 0);
}

namespace
{
constexpr uint32 PLAYER_ACTION_UNIQUE_KEY_BASE = 100000;
}

bool PlayerAction::IsAsOne(Player* player, uint32 ruleId)
{
    //查整个
    bool In = false;
    QueryResult Result = CharacterDatabase.Query("SELECT 变量 FROM ______玩家 WHERE 序号={};", PLAYER_ACTION_UNIQUE_KEY_BASE + ruleId);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            int32 Temp = Fields[0].Get<int32>();
            if (Temp == 1)
            {
                In = true;
                break;
            }

        } while (Result->NextRow());
    }
    if (In && player)
        player->SendSysMsg("[提示]: 全服已触发过");

    return In;
}

bool PlayerAction::IsAccountOne(Player* player, uint32 ruleId)
{
    if (!player || !player->GetSession())
        return false;

    uint32 Index = ruleId;
    uint32 AccountId = player->GetSession()->GetAccountId();
    QueryResult Result = LoginDatabase.Query("SELECT 变量 FROM 动作账号唯一 WHERE 账号编号={} AND 动作={};", AccountId, Index);
    if (Result)
    {
        Field* Fields = Result->Fetch();
        if (Fields)
        {
            int32 Temp = Fields[0].Get<int32>();
            bool T = (Temp == 1);
            if (T)
                player->SendSysMsg("[提示]: 该账号已触发过");
            return T;
        }
    }
    return false;
}

bool PlayerAction::IsPlayerOne(Player* player, uint32 ruleId)
{
    if (!player)
        return false;

    uint32 Index = PLAYER_ACTION_UNIQUE_KEY_BASE + ruleId;
    int32 Temp = player->GetTempAtInt32(PlayerTemp(Index));
    bool T = (Temp == 1);
    if (T)
        player->SendSysMsg("[提示]: 该角色已触发过");
    return T;
}

void PlayerAction::MarkUnique(Player* player, WkType type, uint32 ruleId)
{
    if (!player)
        return;
    if (type == AS_ONE)
        CharacterDatabase.DirectExecute("REPLACE INTO `______玩家` (`玩家标识`, `序号`, `变量`) VALUES ({}, {}, 1)", player->GetGUID().GetCounter(), PLAYER_ACTION_UNIQUE_KEY_BASE + ruleId);
    else if (type == PLR_ONE)
        player->UpdateTempAt(PlayerTemp(PLAYER_ACTION_UNIQUE_KEY_BASE + ruleId), 1);
    else if (type == ACCOUNT_ONE && player->GetSession())
        LoginDatabase.DirectExecute("REPLACE INTO 动作账号唯一 (账号编号, 动作, 变量) VALUES ({}, {}, 1)", player->GetSession()->GetAccountId(), ruleId);
}

bool PlayerAction::IsCooldownReady(Player* player, uint32 ruleId)
{
    if (!player)
        return true;

    uint32 playerGuid = player->GetGUID().GetCounter();
    uint64 currentTime = GameTime::GetGameTimeMS().count();

    std::lock_guard<std::mutex> lock(_cooldownMutex);

    auto playerItr = _cooldownMap.find(playerGuid);
    if (playerItr != _cooldownMap.end())
    {
        auto actionItr = playerItr->second.find(ruleId);
        if (actionItr != playerItr->second.end())
        {
            if (currentTime < actionItr->second)
            {
                // 还在冷却中
                uint64 remainingMs = actionItr->second - currentTime;
                // player->SendSysMsg(fmt::format("[提示]: 该动作冷却中，还需 {} 秒", remainingMs / 1000).c_str());
                return false;
            }
        }
    }

    return true;
}

void PlayerAction::SetCooldown(Player* player, uint32 ruleId, uint32 cooldownMs)
{
    if (!player || cooldownMs == 0)
        return;
    std::lock_guard<std::mutex> lock(_cooldownMutex);
    _cooldownMap[player->GetGUID().GetCounter()][ruleId] = GameTime::GetGameTimeMS().count() + cooldownMs;
}

void PlayerAction::CleanupCooldownData(Player* player)
{
    if (!player)
        return;

    uint32 playerGuid = player->GetGUID().GetCounter();
    std::lock_guard<std::mutex> lock(_cooldownMutex);
    _cooldownMap.erase(playerGuid);
    _movementMap.erase(playerGuid);
    _gatherMap.erase(playerGuid);
    _itemUseMap.erase(playerGuid);
}

class ExActionPlayerScript : public PlayerScript
{
public:
    ExActionPlayerScript() : PlayerScript("ExActionPlayerScript") {}

    void OnChat(Player* player, uint32 Type, uint32 Lang, std::string& Msg) override
    {
        if (Lang == LANG_ADDON)
            return;

        if (Type == CHAT_MSG_SAY)
            sPlayerAction->DoChatExecute(player, PLR_SYS, nullptr, Msg);
        else if(Type == CHAT_MSG_EMOTE)
            sPlayerAction->DoChatExecute(player, PLR_EMOTE, nullptr, Msg);
    }

    void OnTextEmote(Player* player, uint32 textEmote, uint32 /*emoteNum*/, ObjectGuid /*guid*/) override
    {
        if (player)
            sPlayerAction->DoExecute(player, PLR_EMOTE, nullptr, textEmote, 0);
    }

    void OnChat(Player* player, uint32 Type, uint32 Lang, std::string& Msg, Player* Receiver) override
    {
        if (Lang == LANG_ADDON || Type != CHAT_MSG_WHISPER || player->GetGUID() == Receiver->GetGUID())
            return;

        sPlayerAction->DoChatExecute(Receiver, PLR_REV_WHISPER, nullptr, Msg);
    }

    void OnCreatureKill(Player* player, Creature* Killed) override
    {
        if (player && Killed)
            sPlayerAction->DoExecute(player, KILL_CRE, nullptr, Killed->GetEntry(), 0);
    }

    void OnPVPKill(Player* Killer, Player* Killed) override
    {
        if (!Killer || !Killed || !Killer->GetSession() || !Killed->GetSession())
            return;

        if (Killer->GetGUID() == Killed->GetGUID())
            return;

        if (strcmp(Killer->GetSession()->GetRemoteAddress().c_str(), Killed->GetSession()->GetRemoteAddress().c_str()) == 0)
            return;

        sPlayerAction->DoExecute(Killer, KILL_PLR, Killed, 0, 0);
        sPlayerAction->DoExecute(Killed, B_KILL_PLR, Killer, 0, 0);
    }

    void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
    {
        if (player && quest)
            sPlayerAction->DoExecute(player, CM_QET, nullptr, quest->GetQuestId(), 0);
    }

    void OnAchiComplete(Player* player, AchievementEntry const* achievement) override
    {
        if (player && achievement)
            sPlayerAction->DoExecute(player, CM_ACH, nullptr, achievement->ID, 0);
    }

    void OnPlayerKilledByCreature(Creature* killer, Player* killed) override
    {
        if (killer && killed)
            sPlayerAction->DoExecute(killed, B_KILL_CRE, nullptr, killer->GetEntry(), 0);
    }

    void OnLootItem(Player* player, Item* item, uint32 count, ObjectGuid lootguid) override
    {
        if (!player || !item || !player->IsInWorld()) return;
        if (count == 0) return;
        if (lootguid.IsGameObject())
        {
            if (GameObject* gameObject = player->GetMap()->GetGameObject(lootguid))
            {
                ActionTypes gatherAction = GetGatherAction(gameObject);
                if (gatherAction != Bng && sPlayerAction->ShouldTriggerGather(player, lootguid))
                    sPlayerAction->DoExecute(player, gatherAction, nullptr, gameObject->GetEntry(), 0);
            }
        }
        sPlayerAction->DoExecute(player, PLR_LOOT, nullptr, item->GetEntry(), count);
    }

    void OnUpdate(Player* player, uint32 diff) override
    {
        sPlayerAction->ProcessPendingItemUse(player);
        if (player && !player->IsBeingTeleported() && sPlayerAction->HasAction(PLR_MOVE) &&
            sPlayerAction->HasPlayerMoved(player, diff))
            sPlayerAction->DoExecute(player, PLR_MOVE, nullptr, 0, 0);
    }

    void OnLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (player)
            sPlayerAction->DoExecute(player, LEVEL_CG, nullptr, oldLevel, player->GetLevel());
    }

    void OnMapChanged(Player* player) override
    {
        if (player)
        {
            sPlayerAction->DoExecute(player, MAP_CG, nullptr, 0, 0);
            if (player->GetLastMap())
                sPlayerAction->DoExecute(player, MAP_LEAVE, nullptr, player->GetLastMap()->GetId(), 0);
        }
    }

    void OnUpdateArea(Player* player, uint32 oldArea, uint32 newArea) override
    {
        if (player)
        {
            sPlayerAction->DoExecute(player, AREA_CG, nullptr, 0, 0);
            sPlayerAction->DoExecute(player, AREA_LEAVE, nullptr, oldArea, 0);
        }
    }

    void OnUpdateZone(Player* player, uint32 newZone, uint32 /*newArea*/) override
    {
        if (player)
            sPlayerAction->DoExecute(player, ZONE_CG, nullptr, newZone, 0);
    }

    void OnSpellCast(Player* player, Spell* spell, bool skipCheck) override
    {
        if (player && spell && spell->m_spellInfo && spell->m_spellInfo->Id)
            sPlayerAction->DoExecute(player, CAST_SPELL, nullptr, spell->m_spellInfo->Id, 0);
    }

    void OnLogin(Player* player) override
    {
        sPlayerAction->DoExecute(player, PLR_LOGIN, nullptr, 0, 0);
    }

    void OnLogout(Player* player) override
    {
        sPlayerAction->DoExecute(player, PLR_LOGOUT, nullptr, 0, 0);
        sPlayerAction->CleanupCooldownData(player);
    }

    void OnFirstLogin(Player* player) override
    {
        sPlayerAction->DoExecute(player, PLR_FIRST_LOGIN, nullptr, 0, 0);
    }

    void OnPlayerResurrect(Player* player, float restore_percent, bool& applySickness) override
    {
        sPlayerAction->DoExecute(player, PLR_REVIVE, nullptr, 0, 0);
    }

    void OnEquip(Player* player, Item* it, uint8 bag, uint8 slot, bool update) override
    {
        if (player && it)
            sPlayerAction->DoExecute(player, PLR_EQUIP, nullptr, it->GetEntry(), 0);
    }

    void OnMoneyChanged(Player* player, int64& amount) override
    {
        sPlayerAction->DoExecute(player, PLR_MONEY_CG, nullptr, amount, 0);
    }

    void OnGiveXP(Player* player, uint32& amount, Unit* victim) override
    {
        if (player)
            sPlayerAction->DoExecute(player, PLR_XP, nullptr, amount, 0);
    }

    bool OnReputationChange(Player* player, uint32 factionID, int32& standing, bool incremental) override
    {
        sPlayerAction->DoExecute(player, PLR_REPUTATION, nullptr, standing, 0);
        return true;
    }

    void OnLearnSpell(Player* player, uint32 spellID) override
    {
        sPlayerAction->DoExecute(player, PLR_LEARN_SPELL, nullptr, spellID, 0);
    }

    void OnForgotSpell(Player* player, uint32 spellID) override
    {
        sPlayerAction->DoExecute(player, PLR_UN_LEARN_SPELL, nullptr, spellID, 0);
    }

    void OnDuelStart(Player* player1, Player* player2) override
    {
        if (player1 && player2)
            sPlayerAction->DoExecute(player1, PLR_DUEL_START, player2, 0, 0);
    }

    void OnDuelEnd(Player* winner, Player* loser, DuelCompleteType type) override
    {
        if (winner && loser)
        {
            sPlayerAction->DoExecute(winner, PLR_DUEL_STOP, loser, 0, 0);
            sPlayerAction->DoExecute(loser, PLR_DUEL_STOP, winner, 0, 0);

            if (type != DUEL_FLED)
            {
                sPlayerAction->DoExecute(winner, PLR_DUEL_WIN, loser, 0, 0);
                sPlayerAction->DoExecute(loser, PLR_DUEL_LOSER, winner, 0, 0);
            }
        }
    }

    void OnPlayerReleasedGhost(Player* player) override
    {
        sPlayerAction->DoExecute(player, PLR_REPOP, nullptr, 0, 0);
    }

    void OnPlayerLearnTalents(Player* player, uint32 talentId, uint32 talentRank, uint32 spellid) override
    {
        sPlayerAction->DoExecute(player, PLR_LEARN_TALENT, nullptr, spellid, 0);
    }

    void OnPlayerEnterCombat(Player* player, Unit* enemy) override
    {
        sPlayerAction->DoExecute(player, PLR_LEARN_COMBAT_START, nullptr, 0, 0);
    }

    void OnPlayerLeaveCombat(Player* player) override
    {
        sPlayerAction->DoExecute(player, PLR_LEARN_COMBAT_STOP, nullptr, 0, 0);
    }

    void OnTalentsReset(Player* player, bool noCost) override
    {
        sPlayerAction->DoExecute(player, PLR_RESET_TALENT, nullptr, 0, 0);
    }

    void OnPlayerAddAura(Player* player, uint32 AuraId) override
    {
        sPlayerAction->DoExecute(player, PLR_ADD_AURA, nullptr, AuraId, 0);
    }

    void OnPlayerAuraRemove(Player* player, uint32 AuraId) override
    {
        sPlayerAction->DoExecute(player, PLR_REMOVE_AURA, nullptr, AuraId, 0);
    }

    void OnPlayerModeChanged(Player* player, uint32 ModeId) override
    {
        sPlayerAction->DoExecute(player, PLR_MODE_CG, nullptr, ModeId, 0);
    }

    void OnPlayerQuestAccept(Player* player, Quest const* quest) override
    {
        sPlayerAction->DoExecute(player, PLR_ACCEPT_QET, nullptr, quest ? quest->GetQuestId() : 0, 0);
    }

    void OnQuestAbandon(Player* player, uint32 questId) override
    {
        sPlayerAction->DoExecute(player, PLR_CANCEL_QET, nullptr, questId, 0);
    }

    void OnQuestRewardItem(Player* player, Item* item, uint32 count) override
    {
        if (player && item)
            sPlayerAction->DoExecute(player, PLR_QET_RW_ITEM, nullptr, item->GetEntry(), count);
    }
};

class ExActionCreatureScript : public AllCreatureScript
{
public:
    ExActionCreatureScript() : AllCreatureScript("ExActionCreatureScript") { }

    void OnAllCreatureUpdate(Creature* creature, uint32 diff) override
    {
        if (!creature || !creature->IsInWorld() || creature->movespline->Finalized() ||
            !sPlayerAction->HasAction(CREATURE_MOVE, creature->GetEntry()))
            return;

        uint64 currentTime = GameTime::GetGameTimeMS().count();
        if (currentTime % IN_MILLISECONDS >= std::min<uint32>(diff, IN_MILLISECONDS))
            return;

        Player* player = creature->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!player)
            player = creature->SelectNearestPlayer(50.0f);
        if (player)
            sPlayerAction->DoExecute(player, CREATURE_MOVE, nullptr, creature->GetEntry(), 0);
    }
};

void AddSC_PlayerActionScripts()
{
    new ExActionPlayerScript();
    new ExActionCreatureScript();
}
