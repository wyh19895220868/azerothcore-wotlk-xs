
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
#include "Pet.h"
#include "Bag.h"
#include "Guild.h"
#include <CharacterCache.h>
#include <algorithm>
#include "AsItemMgr.h"
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/ItemFuncMgr/ItemFuncMgr.h>
#include <Custom/AddonActive/DoAddon.h>
#include <Mails/Mail.h>
#include <Custom/PlayerModMgr/PlayerModMgr.h>
#include <Custom/EnterFBMgr/EnterFBMgr.h>
#include <Custom/ClassConfig/ClassMod.h>
#include "Cell.h"
#include "CellImpl.h"
#include <GridNotifiersImpl.h>
#include <Time/GameTime.h>
#include <Loot\LootMgr.h>
#include <Custom\ItemRandomAtMgr\ItemRandomAtMgr.h>
#include <Custom\ItemRunesMgr\ItemRunesMgr.h>
#include <Custom\AuraSpellMgr\AuraSpellMgr.h>
#include <Custom\CreatureMod\AsCreature.h>
#include "NpcBeastmaster.h"
#pragma execution_character_set("utf-8")

class ItemFunction : public ItemScript
{
public:
    ItemFunction() : ItemScript("ItemFunction") {}

    void ShowItemActionGossipHello(Player* player, Item* OnItem)
    {
        ClearGossipMenuFor(player);

        uint32 UseItemId = OnItem->GetEntry();

        auto Itr = FunctionStore.find(UseItemId);
        if (Itr == FunctionStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(1002));
            CloseGossipMenuFor(player);
            return;
        }

        if (sHelper->GetBoolean(2020))
        {
            uint32 VipRank = player->GetVipRank();
            uint64 Token = player->GetToken();
            uint64 AsToken = player->GetAsToken();
            uint32 Mobi = player->GetTokenMB(); //新增魔币
            uint64 AsMobi = player->GetAsTokenMB();//新增魔币
            uint64 Paodian = player->GetPaodian();//新增泡点
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6601), VipRank, Token, AsToken, Mobi, AsMobi, Paodian), 0, 0);//新增魔币 新增泡点
        }

        uint32 FactionId = player->GetTeamId() == TEAM_ALLIANCE ? 1 : 2;
        for (int i = 0; i < FunctionStore.count(UseItemId); i++, Itr++)
        {
            //开始出现的首页 即 小于 0
            if (Itr->second.MenuId < 0 && (Itr->second.FactionId == 0 || FactionId == Itr->second.FactionId))
            {
                // 检查显示需求模版，如果不满足则不显示该菜单项（使用ShowCheckNeed不发送提示）
                if (Itr->second.ShowNeedTempId > 0 && !sHelper->ShowCheckNeed(player, Itr->second.ShowNeedTempId))
                    continue;

                if (Itr->second.NTempId > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Itr->second.Ttitle, 0, Itr->second.Num, sHelper->GetNeedGts(Itr->second.NTempId), 0, false);
                else
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Itr->second.Ttitle, 0, Itr->second.Num);
            }
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, OnItem->GetGUID());
    }

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        ShowItemActionGossipHello(player, CastItem);

        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        uint32 FactionId = player->GetTeamId() == TEAM_ALLIANCE ? 1 : 2;
        if (!Sender && !Action)
        {
            ShowItemActionGossipHello(player, OnItem);
            return;
        }

        uint32 MenuId = GossipGetMenuId(player);
        if (MenuId == 8888 && Sender)
        {
            uint32 VNtempId = sPlayerModMgr->GetVipNTempId(Sender);
            if (sHelper->CheckNeed(player, VNtempId, "多功能宝石提升会员消耗"))
            {
                player->SetVipRank(Sender);
                player->CastSpell(player, TxLevelSpell, false);
                VNtempId = sPlayerModMgr->GetVipRTempId(Sender);
                sHelper->DoTheReward(player, VNtempId, "多功能宝石提升会员奖励");
            }

            ShowItemActionGossipHello(player, OnItem);
            return;
        }

        if (MenuId == 7777 && Sender && Action)
        {
            if (Sender > C_NOT_FB)
            {
                if (player->GetGroup())
                {
                    player->SendSpecialMsg(sHelper->GetGts(6602));
                    ShowItemActionGossipHello(player, OnItem);
                    return;
                }

                auto Its = ResetFBStore.find(Action);
                if (Its != ResetFBStore.end())
                {
                    uint32 NowCount = player->GetPlayerReMapStoreCount(Sender, Its->second.Diff);
                    uint32 ACount = sEnterFBMgr->GetCount(Sender, Its->second.Diff);
                    if (ACount > 0 && NowCount >= ACount)
                    {
                        ShowItemActionGossipHello(player, OnItem);
                        player->SendSysMsg(sHelper->GetGts(1813), ACount);
                        return;
                    }

                    if (sHelper->CheckNeed(player, Its->second.NTempId, "多功能宝石重置副本消耗"))
                    {
                        if (MapEntry const* MEntry = sMapStore.LookupEntry(Sender))
                        {
                            if (Its->second.Diff < 0)
                                player->CustomUnbinds(Sender);
                            else
                                player->CustomUnbind(Sender, Difficulty(Its->second.Diff));

                            if (ACount > 0)
                                player->UpdatePlayerReMapStore(Sender, Its->second.Diff, NowCount + 1);

                            player->SendSysMsg(sHelper->GetGts(1812), MEntry->name[LOCALE_zhCN], Its->second.Diff);
                        }
                    }
                }

                ShowItemActionGossipHello(player, OnItem);
                return;
            }
            else
            {
                for (auto It = ResetFBStore.begin(); It != ResetFBStore.end(); ++It)
                {
                    if (It->second.Menu == Sender)
                    {
                        std::string Gts = sHelper->GetNeedGts(It->second.NTempId);
                        if (It->second.MapId > C_NOT_FB && It->second.NTempId)
                            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->second.Gossip, It->second.MapId, It->second.Num, Acore::StringFormat(sHelper->GetGts(6603), sHelper->GetMapName(It->second.MapId)) + Gts, 0, false);
                        else
                            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->second.Gossip, It->second.MapId, It->second.Num);
                    }
                }

                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                GossipSetMenuId(player, 7777);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, OnItem->GetGUID());
                return;
            }
        }

        if (MenuId == 6666 && Sender)
        {
            uint32 AccountId = player->GetSession()->GetAccountId();
            LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_ITEM_NUM);
            Stmt->SetData(0, AccountId);
            Stmt->SetData(1, Sender);
            PreparedQueryResult Result = LoginDatabase.Query(Stmt);
            if (!Result)
            {
                player->SendSysMsg(sHelper->GetGts(2502));
            }
            else
            {
                uint64 Count = (*Result)[0].Get<uint64>();
                uint32 Bp = uint32(sHelper->GetInt(1502));
                Stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN_ITEM);
                Stmt->SetData(0, 1);
                Stmt->SetData(1, AccountId);
                Stmt->SetData(2, Sender);
                LoginDatabase.Execute(Stmt);
                player->AddItem(uint32(sHelper->GetInt(1501)), Count * Bp, "充值兑换获得");
                player->SendSysMsg(sHelper->GetGts(2503));
            }

            ShowItemActionGossipHello(player, OnItem);
            return;
        }

        auto Itr = FunctionMenuStore.find(Action);
        if (Itr == FunctionMenuStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(1002));
            CloseGossipMenuFor(player);
            return;
        }

        if (Itr->second.AMenuId == 0)
        {
            if (sHelper->CheckNeed(player, Itr->second.NTempId, "多功能宝石传送消耗"))
            {
                if (sHelper->GetBoolean(2000) && sHelper->GetUInt(2001) > 0)
                {
                    if (Itr->second.PosId > 0)
                    {
                        player->TelePos = Itr->second.PosId;
                        player->CastSpell(player, sHelper->GetUInt(2001), false);
                    }
                    else
                    {
                        player->TeleToPos(Itr->second.PosId);
                    }
                }
                else
                {
                    player->TeleToPos(Itr->second.PosId);
                }

                player->DoCommands(Itr->second.Cmd);
                CloseGossipMenuFor(player);
                return;
            }

            ShowItemActionGossipHello(player, OnItem);
            return;
        }
        else if (Itr->second.AMenuId > 0)
        {
            for (auto const& Temp : FunctionMenuStore)
            {
                if (Temp.second.MenuId == Itr->second.AMenuId && Temp.second.OEntry == Itr->second.OEntry && (Temp.second.FactionId == 0 || FactionId == Temp.second.FactionId))
                {
                    // 检查显示需求模版，如果不满足则不显示该菜单项（使用ShowCheckNeed不发送提示）
                    if (Temp.second.ShowNeedTempId > 0 && !sHelper->ShowCheckNeed(player, Temp.second.ShowNeedTempId))
                        continue;

                    if (Temp.second.NTempId > 0)
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, Temp.second.Ttitle, 0, Temp.second.Num, sHelper->GetNeedGts(Temp.second.NTempId), 0, false);
                    else
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, Temp.second.Ttitle, 0, Temp.second.Num);
                }
            }
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, OnItem->GetGUID());
            return;
        }
        else if (Itr->second.AMenuId < -1)
        {
            if (Itr->second.AMenuId == DEF_13 && player->GetGroup())
            {
                player->SendSpecialMsg(sHelper->GetGts(6602));
                ShowItemActionGossipHello(player, OnItem);
                return;
            }

            if (!sHelper->CheckNeed(player, Itr->second.NTempId, "多功能宝石消耗"))
            {
                ShowItemActionGossipHello(player, OnItem);
                return;
            }

            switch (Itr->second.AMenuId)
            {
            case DEF_2:
            {
                player->GetSession()->SendShowBank(player->GetGUID());
                CloseGossipMenuFor(player);
                return;
            }
            break;
            case DEF_3:
            {
                player->DurabilityRepairAll(false, 0, false);
                player->SendSysMsg(sHelper->GetGts(1003));
                ShowItemActionGossipHello(player, OnItem);
                return;
            }
            break;
            case DEF_4:
            {
                player->UpdateSkillsToMaxSkillsForLevel();
                player->SendSysMsg(sHelper->GetGts(1004));
                ShowItemActionGossipHello(player, OnItem);
                return;
            }
            break;
            case DEF_5:
            {
                if (player->IsAlive())
                {
                    AsInt64 killDamage = player->GetAsHealth();
                    if (killDamage <= 0)
                        killDamage = AsInt64(player->GetHealth());
                    player->SetAsDamageOverride(player->GetGUID(), killDamage);
                    Unit::DealAsDamage(player, player, player->GetAsHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
                }

                CloseGossipMenuFor(player);
                return;
            }
            break;
            case DEF_6:
            {
                uint32 Delay = uint32(sHelper->GetInt(1003));
                if (sHelper->GetBoolean(1002) && Delay > 0)
                {
                    uint32 RTempId = uint32(sHelper->GetInt(1004));
                    uint64 DelayTime = (uint64)Delay * MINUTE * IN_MILLISECONDS;

                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6605), 0, Itr->second.Num);
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6606), Delay, sHelper->GetDoTimeString(player->OnlineRewardTime), sHelper->GetDoTimeString(DelayTime - player->OnlineRewardTime), sHelper->GetRewardGts(RTempId)), 0, Itr->second.Num);

                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, OnItem->GetGUID());
                    return;
                }
                else
                {
                    player->SendSysMsg(sHelper->GetGts(1041));
                    ShowItemActionGossipHello(player, OnItem);
                    return;
                }
            }
            break;
            case DEF_7:
            {
                uint32 MeVipRank = player->GetVipRank();
                if (MeVipRank >= sPlayerModMgr->GetVipMaxRank())
                {
                    player->SendSysMsg(sHelper->GetGts(1801));
                    ShowItemActionGossipHello(player, OnItem);
                    return;
                }

                uint32 NextRank = MeVipRank + 1;
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6607), MeVipRank), 0, Itr->second.Num);
                std::string Oss;
                if (MeVipRank > 0)
                {
                    Oss.append(Acore::StringFormat(sHelper->GetGts(6608), sPlayerModMgr->GetVipMenuStr(MeVipRank)));
                }
                else
                {
                    Oss.append(sHelper->GetGts(6609));
                }

                Oss.append(Acore::StringFormat(sHelper->GetGts(6610), sPlayerModMgr->GetVipMenuStr(NextRank)));

                AddGossipItemFor(player, GOSSIP_ICON_TALK, Oss, 0, Itr->second.Num);

                uint32 VNtempId = sPlayerModMgr->GetVipNTempId(NextRank);
                if (VNtempId)
                {
                    std::string Gts = sHelper->GetNeedGts(VNtempId);
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6611), NextRank, Itr->second.Num, Gts, 0, false);
                }
                else
                {
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6611), NextRank, Itr->second.Num);
                }
                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                GossipSetMenuId(player, 8888);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, OnItem->GetGUID());
                return;
            }
            break;
            case DEF_8:
            {
                uint32 Delay = uint32(sHelper->GetUInt(1035));
                if (Delay)
                {
                    time_t Current = GameTime::GetGameTime().count();
                    if (player->ClearCombatDelay < Current)
                    {
                        player->ClearCombatDelay = Current + Delay;
                        sHelper->DoClearInCombat(player);
                        player->SendSysMsg(sHelper->GetGts(6612));
                    }
                    else
                    {
                        uint32 CheckTime = player->ClearCombatDelay - Current;
                        player->SendSysMsg(sHelper->GetGts(6613), CheckTime);
                    }
                }
                else
                {
                    sHelper->DoClearInCombat(player);
                    player->SendSysMsg(sHelper->GetGts(6614));
                }
                ShowItemActionGossipHello(player, OnItem);
                return;
            }
            break;
            case DEF_9:
            {
                for (auto It = ResetFBStore.begin(); It != ResetFBStore.end(); ++It)
                {
                    if (It->second.Menu == 1)
                    {
                        std::string Gts = sHelper->GetNeedGts(It->second.NTempId);
                        if (It->second.MapId > C_NOT_FB && It->second.NTempId)
                            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->second.Gossip, It->second.MapId, It->second.Num, Acore::StringFormat(sHelper->GetGts(6603), sHelper->GetMapName(It->second.MapId)) + Gts, 0, false);
                        else
                            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->second.Gossip, It->second.MapId, It->second.Num);
                    }
                }

                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                GossipSetMenuId(player, 7777);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, OnItem->GetGUID());
                return;
            }
            break;
            case DEF_10:
            {
                uint32 AccountId = player->GetSession()->GetAccountId();
                LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_ITEM);
                Stmt->SetData(0, AccountId);
                Stmt->SetData(1, 0);
                PreparedQueryResult Result = LoginDatabase.Query(Stmt);
                if (!Result)
                {
                    player->SendSysMsg(sHelper->GetGts(2501));
                    ShowItemActionGossipHello(player, OnItem);
                    return;
                }
                else
                {
                    uint32 Bp = uint32(sHelper->GetInt(1502));
                    std::string RItemIdLink = sHelper->GetItemLink(uint32(sHelper->GetInt(1501)));
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6615), RItemIdLink), 0, Itr->second.Num);

                    uint32 Index = 0;
                    do
                    {
                        Field* Fields = Result->Fetch();
                        uint32 Num = Fields[0].Get<uint32>();
                        uint64 Count = Fields[1].Get<uint64>();
                        uint64 Time = Fields[2].Get<uint64>();
                        Index++;
                        if (Index > 24)
                            continue;

                        std::string TimeStr = Acore::Time::TimeToTimestampStr(Seconds(static_cast<int64>(Time)));
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6616), Index, Count, Count * Bp, TimeStr), Num, Itr->second.Num);

                    } while (Result->NextRow());

                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                    GossipSetMenuId(player, 6666);
                    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, OnItem->GetGUID());
                }
            }
            break;
            case DEF_11:
            {
                uint32 KItemId = sHelper->GetUInt32(2101);
                uint32 KCount = sHelper->GetUInt32(2102);
                uint32 AuraId = sHelper->GetUInt32(2103);
                if (!KItemId && !KCount && !AuraId)
                {
                    ShowItemActionGossipHello(player, OnItem);
                    return;
                }
                if (player->GetTempAt(AT2001) == AuraId && player->HasAura(AuraId))
                {
                    player->SendSysMsg(sHelper->GetGts(3101));
                }
                else
                {
                    if (!player->HasItemCount(KItemId, KCount))
                    {
                        player->SendSysMsg(sHelper->GetGts(1011), sHelper->GetItemLink(KItemId).c_str(), KCount);
                    }
                    else
                    {
                        player->DestroyItemCount(KItemId, KCount, true);
                        player->AddAura(AuraId, player);
                        player->UpdateTempAt(AT2001, AuraId);
                        player->SendSysMsg(sHelper->GetGts(3102));
                    }
                }

                ShowItemActionGossipHello(player, OnItem);
                return;
            }
            break;
            case DEF_12:
            {
                uint32 VipRank = player->GetVipRank();
                uint64 Token = player->GetToken();
                uint64 AsToken = player->GetAsToken();
                player->SendSysMsg(sHelper->GetGts(1054), sHelper->GetNameLinkColor(player).c_str(), VipRank);
                player->SendSysMsg(sHelper->GetGts(1055), sHelper->GetNameLinkColor(player).c_str(), Token, AsToken);
                ShowItemActionGossipHello(player, OnItem);
                return;
            }
            break;
            case DEF_13:
            {
                player->CustomUnbindAll();
                player->SendSysMsg(sHelper->GetGts(6617));
                ShowItemActionGossipHello(player, OnItem);
                return;
            }
            break;
            }
        }
        else
        {
            ShowItemActionGossipHello(player, OnItem);
            return;
        }
    }
};

class NpcFunction : public CreatureScript
{
public:
    NpcFunction() : CreatureScript("NpcFunction") { }

    void ShowParentGossipHello(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        uint32 NpcId = creature->GetEntry();

        auto Itr = FunctionStore.find(NpcId);
        if (Itr == FunctionStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(1002));
            CloseGossipMenuFor(player);
            return;
        }

        if (sHelper->GetBoolean(2020))
        {
            uint32 VipRank = player->GetVipRank();
            uint64 Token = player->GetToken();
            uint64 AsToken = player->GetAsToken();
            uint32 Mobi = player->GetTokenMB();
            uint64 AsMobi = player->GetAsTokenMB();
            uint64 Paodian = player->GetPaodian();//新增泡点
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6601), VipRank, Token, AsToken, Mobi, AsMobi, Paodian), 0, 0);
        }

        uint32 FactionId = player->GetTeamId() == TEAM_ALLIANCE ? 1 : 2;
        for (int i = 0; i < FunctionStore.count(NpcId); i++, Itr++)
        {
            //开始出现的首页 即 小于 0
            if (Itr->second.MenuId < 0 && (Itr->second.FactionId == 0 || FactionId == Itr->second.FactionId))
            {
                // 检查显示需求模版，如果不满足则不显示该菜单项（使用ShowCheckNeed不发送提示）
                if (Itr->second.ShowNeedTempId > 0 && !sHelper->ShowCheckNeed(player, Itr->second.ShowNeedTempId))
                    continue;

                if (Itr->second.NTempId > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Itr->second.Ttitle, 0, Itr->second.Num, sHelper->GetNeedGts(Itr->second.NTempId), 0, false);
                else
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Itr->second.Ttitle, 0, Itr->second.Num);
            }
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (player->IsInCombat())
        {
            player->SendSysMsg(sHelper->GetGts(1001));
            return true;
        }

        ShowParentGossipHello(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        uint32 FactionId = player->GetTeamId() == TEAM_ALLIANCE ? 1 : 2;
        if (!Sender && !Action)
        {
            ShowParentGossipHello(player, creature);
            return true;
        }

        uint32 MenuId = GossipGetMenuId(player);
        if (MenuId == 8888 && Sender)
        {
            uint32 VNtempId = sPlayerModMgr->GetVipNTempId(Sender);
            if (sHelper->CheckNeed(player, VNtempId, "NPC菜单会员功能消耗"))
            {
                player->SetVipRank(Sender);
                player->CastSpell(player, TxLevelSpell, false);
                VNtempId = sPlayerModMgr->GetVipRTempId(Sender);
                sHelper->DoTheReward(player, VNtempId, "NPC菜单提升会员奖励");
            }

            ShowParentGossipHello(player, creature);
            return true;
        }

        if (MenuId == 7777 && Sender && Action)
        {
            if (Sender > C_NOT_FB)
            {
                if (player->GetGroup())
                {
                    player->SendSpecialMsg(sHelper->GetGts(6602));
                    ShowParentGossipHello(player, creature);
                    return true;
                }

                auto Its = ResetFBStore.find(Action);
                if (Its != ResetFBStore.end())
                {
                    uint32 NowCount = player->GetPlayerReMapStoreCount(Sender, Its->second.Diff);
                    uint32 ACount = sEnterFBMgr->GetCount(Sender, Its->second.Diff);
                    if (ACount > 0 && NowCount >= ACount)
                    {
                        ShowParentGossipHello(player, creature);
                        player->SendSysMsg(sHelper->GetGts(1813), ACount);
                        return true;
                    }

                    if (sHelper->CheckNeed(player, Its->second.NTempId, "NPC菜单重置副本消耗"))
                    {
                        if (MapEntry const* MEntry = sMapStore.LookupEntry(Sender))
                        {
                            if (Its->second.Diff < 0)
                                player->CustomUnbinds(Sender);
                            else
                                player->CustomUnbind(Sender, Difficulty(Its->second.Diff));

                            if (ACount > 0)
                                player->UpdatePlayerReMapStore(Sender, Its->second.Diff, NowCount + 1);

                            player->SendSysMsg(sHelper->GetGts(1812), MEntry->name[LOCALE_zhCN], Its->second.Diff);
                        }
                    }
                }

                ShowParentGossipHello(player, creature);
                return true;
            }
            else
            {
                for (auto It = ResetFBStore.begin(); It != ResetFBStore.end(); ++It)
                {
                    if (It->second.Menu == Sender)
                    {
                        std::string Gts = sHelper->GetNeedGts(It->second.NTempId);
                        if (It->second.MapId > C_NOT_FB && It->second.NTempId)
                            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->second.Gossip, It->second.MapId, It->second.Num, Acore::StringFormat(sHelper->GetGts(6603), sHelper->GetMapName(It->second.MapId)) + Gts, 0, false);
                        else
                            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->second.Gossip, It->second.MapId, It->second.Num);
                    }
                }

                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                GossipSetMenuId(player, 7777);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
                return true;
            }
        }

        if (MenuId == 6666 && Sender)
        {
            uint32 AccountId = player->GetSession()->GetAccountId();
            LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_ITEM_NUM);
            Stmt->SetData(0, AccountId);
            Stmt->SetData(1, Sender);
            PreparedQueryResult Result = LoginDatabase.Query(Stmt);
            if (!Result)
            {
                player->SendSysMsg(sHelper->GetGts(2502));
            }
            else
            {
                uint64 Count = (*Result)[0].Get<uint64>();
                uint32 Bp = uint32(sHelper->GetInt(1502));
                Stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN_ITEM);
                Stmt->SetData(0, 1);
                Stmt->SetData(1, AccountId);
                Stmt->SetData(2, Sender);
                LoginDatabase.Execute(Stmt);
                player->AddItem(uint32(sHelper->GetInt(1501)), Count * Bp, "充值兑换获得");
                player->SendSysMsg(sHelper->GetGts(2503));
            }

            ShowParentGossipHello(player, creature);
            return true;
        }

        auto Itr = FunctionMenuStore.find(Action);
        if (Itr == FunctionMenuStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(1002));
            CloseGossipMenuFor(player);
            return true;
        }

        if (Itr->second.AMenuId == 0)
        {
            if (sHelper->CheckNeed(player, Itr->second.NTempId, "NPC菜单传送消耗"))
            {
                player->TeleToPos(Itr->second.PosId);
                player->DoCommands(Itr->second.Cmd);
                CloseGossipMenuFor(player);
                return true;
            }

            ShowParentGossipHello(player, creature);
            return true;
        }
        else if (Itr->second.AMenuId > 0)
        {
            for (auto const& Temp : FunctionMenuStore)
            {
                if (Temp.second.MenuId == Itr->second.AMenuId && Temp.second.OEntry == Itr->second.OEntry && (Temp.second.FactionId == 0 || FactionId == Temp.second.FactionId))
                {
                    // 检查显示需求模版，如果不满足则不显示该菜单项（使用ShowCheckNeed不发送提示）
                    if (Temp.second.ShowNeedTempId > 0 && !sHelper->ShowCheckNeed(player, Temp.second.ShowNeedTempId))
                        continue;

                    if (Temp.second.NTempId > 0)
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, Temp.second.Ttitle, 0, Temp.second.Num, sHelper->GetNeedGts(Temp.second.NTempId), 0, false);
                    else
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, Temp.second.Ttitle, 0, Temp.second.Num);
                }
            }
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            return true;
        }
        else if (Itr->second.AMenuId < -1)
        {
            if (Itr->second.AMenuId == DEF_13 && player->GetGroup())
            {
                player->SendSpecialMsg(sHelper->GetGts(6602));
                ShowParentGossipHello(player, creature);
                return true;
            }

            if (!sHelper->CheckNeed(player, Itr->second.NTempId, "NPC菜单消耗"))
            {
                ShowParentGossipHello(player, creature);
                return true;
            }

            switch (Itr->second.AMenuId)
            {
            case DEF_2:
            {
                player->GetSession()->SendShowBank(player->GetGUID());
                CloseGossipMenuFor(player);
                return true;
            }
            break;
            case DEF_3:
            {
                player->DurabilityRepairAll(false, 0, false);
                player->SendSysMsg(sHelper->GetGts(1003));
                ShowParentGossipHello(player, creature);
                return true;
            }
            break;
            case DEF_4:
            {
                player->UpdateSkillsToMaxSkillsForLevel();
                player->SendSysMsg(sHelper->GetGts(1004));
                ShowParentGossipHello(player, creature);
                return true;
            }
            break;
            case DEF_5:
            {
                if (player->IsAlive())
                {
                    AsInt64 killDamage = player->GetAsHealth();
                    if (killDamage <= 0)
                        killDamage = AsInt64(player->GetHealth());
                    player->SetAsDamageOverride(player->GetGUID(), killDamage);
                    Unit::DealAsDamage(player, player, player->GetAsHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
                }

                CloseGossipMenuFor(player);
                return true;
            }
            break;
            case DEF_6:
            {
                uint32 Delay = uint32(sHelper->GetInt(1003));
                if (sHelper->GetBoolean(1002) && Delay > 0)
                {
                    uint32 RTempId = uint32(sHelper->GetInt(1004));
                    uint64 DelayTime = (uint64)Delay * MINUTE * IN_MILLISECONDS;

                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6605), 0, Itr->second.Num);
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6606), Delay, sHelper->GetDoTimeString(player->OnlineRewardTime), sHelper->GetDoTimeString(DelayTime - player->OnlineRewardTime), sHelper->GetRewardGts(RTempId)), 0, Itr->second.Num);

                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
                    return true;
                }
                else
                {
                    player->SendSysMsg(sHelper->GetGts(1041));
                    ShowParentGossipHello(player, creature);
                    return true;
                }
            }
            break;
            case DEF_7:
            {
                uint32 MeVipRank = player->GetVipRank();
                if (MeVipRank >= sPlayerModMgr->GetVipMaxRank())
                {
                    player->SendSysMsg(sHelper->GetGts(1801));
                    ShowParentGossipHello(player, creature);
                    return true;
                }

                uint32 NextRank = MeVipRank + 1;
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6607), MeVipRank), 0, Itr->second.Num);
                std::string Oss;
                if (MeVipRank > 0)
                {
                    Oss.append(Acore::StringFormat(sHelper->GetGts(6608), sPlayerModMgr->GetVipMenuStr(MeVipRank)));
                }
                else
                {
                    Oss.append(sHelper->GetGts(6609));
                }

                Oss.append(Acore::StringFormat(sHelper->GetGts(6610), sPlayerModMgr->GetVipMenuStr(NextRank)));

                AddGossipItemFor(player, GOSSIP_ICON_TALK, Oss, 0, Itr->second.Num);

                uint32 VNtempId = sPlayerModMgr->GetVipNTempId(NextRank);
                if (VNtempId)
                {
                    std::string Gts = sHelper->GetNeedGts(VNtempId);
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6611), NextRank, Itr->second.Num, Gts, 0, false);
                }
                else
                {
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6611), NextRank, Itr->second.Num);
                }
                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                GossipSetMenuId(player, 8888);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
                return true;
            }
            break;
            case DEF_8:
            {
                uint32 Delay = uint32(sHelper->GetUInt(1035));
                if (Delay)
                {
                    time_t Current = time(NULL);
                    if (player->ClearCombatDelay < Current)
                    {
                        player->ClearCombatDelay = Current + Delay;
                        sHelper->DoClearInCombat(player);
                        player->SendSysMsg(sHelper->GetGts(6612));
                    }
                    else
                    {
                        uint32 CheckTime = player->ClearCombatDelay - Current;
                        player->SendSysMsg(sHelper->GetGts(6613), CheckTime);
                    }
                }
                else
                {
                    sHelper->DoClearInCombat(player);
                    player->SendSysMsg(sHelper->GetGts(6614));
                }
                ShowParentGossipHello(player, creature);
                return true;
            }
            break;
            case DEF_9:
            {
                for (auto It = ResetFBStore.begin(); It != ResetFBStore.end(); ++It)
                {
                    if (It->second.Menu == 1)
                    {
                        std::string Gts = sHelper->GetNeedGts(It->second.NTempId);
                        if (It->second.MapId > C_NOT_FB)
                            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->second.Gossip, It->second.MapId, It->second.Num, Acore::StringFormat(sHelper->GetGts(6603), sHelper->GetMapName(It->second.MapId)) + Gts, 0, false);
                        else
                            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->second.Gossip, It->second.MapId, It->second.Num);
                    }
                }

                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                GossipSetMenuId(player, 7777);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
                return true;
            }
            break;
            case DEF_10:
            {
                uint32 AccountId = player->GetSession()->GetAccountId();
                LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_ITEM);
                Stmt->SetData(0, AccountId);
                Stmt->SetData(1, 0);
                PreparedQueryResult Result = LoginDatabase.Query(Stmt);
                if (!Result)
                {
                    player->SendSysMsg(sHelper->GetGts(2501));
                    ShowParentGossipHello(player, creature);
                    return true;
                }
                else
                {
                    uint32 Bp = uint32(sHelper->GetInt(1502));
                    std::string RItemIdLink = sHelper->GetItemLink(uint32(sHelper->GetInt(1501)));
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6615), RItemIdLink), 0, Itr->second.Num);

                    uint32 Index = 0;
                    do
                    {
                        Field* Fields = Result->Fetch();
                        uint32 Num = Fields[0].Get<uint32>();
                        uint64 Count = Fields[1].Get<uint64>();
                        uint64 Time = Fields[2].Get<uint64>();
                        Index++;
                        if (Index > 24)
                            continue;

                        std::string TimeStr = Acore::Time::TimeToTimestampStr(Seconds(static_cast<int64>(Time)));
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6616), Index, Count, Count * Bp, TimeStr), Num, Itr->second.Num);

                    } while (Result->NextRow());

                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                    GossipSetMenuId(player, 6666);
                    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
                    return true;
                }
            }
            break;
            case DEF_11:
            {
                uint32 KItemId = sHelper->GetUInt32(2101);
                uint32 KCount = sHelper->GetUInt32(2102);
                uint32 AuraId = sHelper->GetUInt32(2103);
                if (!KItemId && !KCount && !AuraId)
                {
                    ShowParentGossipHello(player, creature);
                    return true;
                }
                if (player->GetTempAt(AT2001) == AuraId && player->HasAura(AuraId))
                {
                    player->SendSysMsg(sHelper->GetGts(3101));
                }
                else
                {
                    if (!player->HasItemCount(KItemId, KCount))
                    {
                        player->SendSysMsg(sHelper->GetGts(1011), sHelper->GetItemLink(KItemId).c_str(), KCount);
                    }
                    else
                    {
                        player->DestroyItemCount(KItemId, KCount, true);
                        player->AddAura(AuraId, player);
                        player->UpdateTempAt(AT2001, AuraId);
                        player->SendSysMsg(sHelper->GetGts(3102));
                    }
                }

                ShowParentGossipHello(player, creature);
                return true;
            }
            break;
            case DEF_12:
            {
                uint32 VipRank = player->GetVipRank();
                uint64 Token = player->GetToken();
                uint64 AsToken = player->GetAsToken();
                player->SendSysMsg(sHelper->GetGts(1054), sHelper->GetNameLinkColor(player).c_str(), VipRank);
                player->SendSysMsg(sHelper->GetGts(1055), sHelper->GetNameLinkColor(player).c_str(), Token, AsToken);
                ShowParentGossipHello(player, creature);
                return true;
            }
            break;
            case DEF_13:
            {
                player->CustomUnbindAll();
                player->SendSysMsg(sHelper->GetGts(6617));
                ShowParentGossipHello(player, creature);
                return true;
            }
            break;
            }
        }
        else
        {
            ShowParentGossipHello(player, creature);
            return true;
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

class GameObjectFunction : public GameObjectScript
{
public:
    GameObjectFunction() : GameObjectScript("GameObjectFunction") { }

    void ShowParentGossipHello(Player* player, GameObject* Object)
    {
        ClearGossipMenuFor(player);

        if (Object->ToGameObject()->GetGoType() == GAMEOBJECT_TYPE_QUESTGIVER)
            player->PrepareQuestMenu(Object->GetGUID());

        uint32 ObjectId = Object->GetEntry();

        auto Itr = FunctionStore.find(ObjectId);
        if (Itr == FunctionStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(1002));
            CloseGossipMenuFor(player);
            return;
        }

        if (sHelper->GetBoolean(2020))
        {
            uint32 VipRank = player->GetVipRank();
            uint64 Token = player->GetToken();
            uint64 AsToken = player->GetAsToken();
            uint32 Mobi = player->GetTokenMB();
            uint64 AsMobi = player->GetAsTokenMB();
            uint64 Paodian = player->GetPaodian();//新增泡点
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6601), VipRank, Token, AsToken, Mobi, AsMobi, Paodian), 0, 0);
        }

        uint32 FactionId = player->GetTeamId() == TEAM_ALLIANCE ? 1 : 2;
        for (int i = 0; i < FunctionStore.count(ObjectId); i++, Itr++)
        {
            //开始出现的首页 即 小于 0
            if (Itr->second.MenuId < 0 && (Itr->second.FactionId == 0 || FactionId == Itr->second.FactionId))
            {
                if (Itr->second.NTempId > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Itr->second.Ttitle, 0, Itr->second.Num, sHelper->GetNeedGts(Itr->second.NTempId), 0, false);
                else
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Itr->second.Ttitle, 0, Itr->second.Num);
            }
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, Object->GetGUID());
    }

    bool OnGossipHello(Player* player, GameObject* Object) override
    {
        if (player->IsInCombat())
        {
            player->SendSysMsg(sHelper->GetGts(1001));
            return true;
        }

        ShowParentGossipHello(player, Object);
        return true;
    }

    bool OnGossipSelect(Player* player, GameObject* Object, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        uint32 FactionId = player->GetTeamId() == TEAM_ALLIANCE ? 1 : 2;
        if (!Sender && !Action)
        {
            ShowParentGossipHello(player, Object);
            return true;
        }

        uint32 MenuId = GossipGetMenuId(player);
        if (MenuId == 8888 && Sender)
        {
            uint32 VNtempId = sPlayerModMgr->GetVipNTempId(Sender);
            if (sHelper->CheckNeed(player, VNtempId, "物体菜单会员功能消耗"))
            {
                player->SetVipRank(Sender);
                player->CastSpell(player, TxLevelSpell, false);
                VNtempId = sPlayerModMgr->GetVipRTempId(Sender);
                sHelper->DoTheReward(player, VNtempId, "物体菜单提升会员");
            }

            ShowParentGossipHello(player, Object);
            return true;
        }

        if (MenuId == 7777 && Sender && Action)
        {
            if (Sender > C_NOT_FB)
            {
                if (player->GetGroup())
                {
                    player->SendSpecialMsg(sHelper->GetGts(6602));
                    ShowParentGossipHello(player, Object);
                    return true;
                }

                auto Its = ResetFBStore.find(Action);
                if (Its != ResetFBStore.end())
                {
                    uint32 NowCount = player->GetPlayerReMapStoreCount(Sender, Its->second.Diff);
                    uint32 ACount = sEnterFBMgr->GetCount(Sender, Its->second.Diff);
                    if (ACount > 0 && NowCount >= ACount)
                    {
                        ShowParentGossipHello(player, Object);
                        player->SendSysMsg(sHelper->GetGts(1813), ACount);
                        return true;
                    }

                    if (sHelper->CheckNeed(player, Its->second.NTempId, "物体菜单传送消耗"))
                    {
                        if (MapEntry const* MEntry = sMapStore.LookupEntry(Sender))
                        {
                            if (Its->second.Diff < 0)
                                player->CustomUnbinds(Sender);
                            else
                                player->CustomUnbind(Sender, Difficulty(Its->second.Diff));

                            if (ACount > 0)
                                player->UpdatePlayerReMapStore(Sender, Its->second.Diff, NowCount + 1);

                            player->SendSysMsg(sHelper->GetGts(1812), MEntry->name[LOCALE_zhCN], Its->second.Diff);
                        }
                    }
                }

                ShowParentGossipHello(player, Object);
                return true;
            }
            else
            {
                for (auto It = ResetFBStore.begin(); It != ResetFBStore.end(); ++It)
                {
                    if (It->second.Menu == Sender)
                    {
                        std::string Gts = sHelper->GetNeedGts(It->second.NTempId);
                        if (It->second.MapId > C_NOT_FB)
                            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->second.Gossip, It->second.MapId, It->second.Num, Acore::StringFormat(sHelper->GetGts(6603), sHelper->GetMapName(It->second.MapId)) + Gts, 0, false);
                        else
                            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->second.Gossip, It->second.MapId, It->second.Num);
                    }
                }

                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                GossipSetMenuId(player, 7777);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, Object->GetGUID());
                return true;
            }
        }

        if (MenuId == 6666 && Sender)
        {
            uint32 AccountId = player->GetSession()->GetAccountId();
            LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_ITEM_NUM);
            Stmt->SetData(0, AccountId);
            Stmt->SetData(1, Sender);
            PreparedQueryResult Result = LoginDatabase.Query(Stmt);
            if (!Result)
            {
                player->SendSysMsg(sHelper->GetGts(2502));
            }
            else
            {
                uint64 Count = (*Result)[0].Get<uint64>();
                uint32 Bp = uint32(sHelper->GetInt(1502));
                Stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN_ITEM);
                Stmt->SetData(0, 1);
                Stmt->SetData(1, AccountId);
                Stmt->SetData(2, Sender);
                LoginDatabase.Execute(Stmt);
                player->AddItem(uint32(sHelper->GetInt(1501)), Count * Bp, "充值兑换获得");
                player->SendSysMsg(sHelper->GetGts(2503));
            }

            ShowParentGossipHello(player, Object);
            return true;
        }

        auto Itr = FunctionMenuStore.find(Action);
        if (Itr == FunctionMenuStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(1002));
            CloseGossipMenuFor(player);
            return true;
        }

        if (Itr->second.AMenuId == 0)
        {
            if (sHelper->CheckNeed(player, Itr->second.NTempId, "物体菜单传送消耗"))
            {
                player->TeleToPos(Itr->second.PosId);
                player->DoCommands(Itr->second.Cmd);
                CloseGossipMenuFor(player);
                return true;
            }

            ShowParentGossipHello(player, Object);
            return true;
        }
        else if (Itr->second.AMenuId > 0)
        {
            for (auto const& Temp : FunctionMenuStore)
            {
                if (Temp.second.MenuId == Itr->second.AMenuId && Temp.second.OEntry == Itr->second.OEntry && (Temp.second.FactionId == 0 || FactionId == Temp.second.FactionId))
                {
                    // 检查显示需求模版，如果不满足则不显示该菜单项（使用ShowCheckNeed不发送提示）
                    if (Temp.second.ShowNeedTempId > 0 && !sHelper->ShowCheckNeed(player, Temp.second.ShowNeedTempId))
                        continue;

                    if (Temp.second.NTempId > 0)
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, Temp.second.Ttitle, 0, Temp.second.Num, sHelper->GetNeedGts(Temp.second.NTempId), 0, false);
                    else
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, Temp.second.Ttitle, 0, Temp.second.Num);
                }
            }
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, Object->GetGUID());
            return true;
        }
        else if (Itr->second.AMenuId < -1)
        {
            if (Itr->second.AMenuId == DEF_13 && player->GetGroup())
            {
                player->SendSpecialMsg(sHelper->GetGts(6602));
                ShowParentGossipHello(player, Object);
                return true;
            }

            if (!sHelper->CheckNeed(player, Itr->second.NTempId, "物体菜单功能消耗"))
            {
                ShowParentGossipHello(player, Object);
                return true;
            }

            switch (Itr->second.AMenuId)
            {
            case DEF_2:
            {
                player->GetSession()->SendShowBank(player->GetGUID());
                CloseGossipMenuFor(player);
                return true;
            }
            break;
            case DEF_3:
            {
                player->DurabilityRepairAll(false, 0, false);
                player->SendSysMsg(sHelper->GetGts(1003));
                ShowParentGossipHello(player, Object);
                return true;
            }
            break;
            case DEF_4:
            {
                player->UpdateSkillsToMaxSkillsForLevel();
                player->SendSysMsg(sHelper->GetGts(1004));
                ShowParentGossipHello(player, Object);
                return true;
            }
            break;
            case DEF_5:
            {
                if (player->IsAlive())
                {
                    AsInt64 killDamage = player->GetAsHealth();
                    if (killDamage <= 0)
                        killDamage = AsInt64(player->GetHealth());
                    player->SetAsDamageOverride(player->GetGUID(), killDamage);
                    Unit::DealAsDamage(player, player, player->GetAsHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
                }

                CloseGossipMenuFor(player);
                return true;
            }
            break;
            case DEF_6:
            {
                uint32 Delay = uint32(sHelper->GetInt(1003));
                if (sHelper->GetBoolean(1002) && Delay > 0)
                {
                    uint32 RTempId = uint32(sHelper->GetInt(1004));
                    uint64 DelayTime = (uint64)Delay * MINUTE * IN_MILLISECONDS;

                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6605), 0, Itr->second.Num);
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6606), Delay, sHelper->GetDoTimeString(player->OnlineRewardTime), sHelper->GetDoTimeString(DelayTime - player->OnlineRewardTime), sHelper->GetRewardGts(RTempId)), 0, Itr->second.Num);

                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, Object->GetGUID());
                    return true;
                }
                else
                {
                    player->SendSysMsg(sHelper->GetGts(1041));
                    ShowParentGossipHello(player, Object);
                    return true;
                }
            }
            break;
            case DEF_7:
            {
                uint32 MeVipRank = player->GetVipRank();
                if (MeVipRank >= sPlayerModMgr->GetVipMaxRank())
                {
                    player->SendSysMsg(sHelper->GetGts(1801));
                    ShowParentGossipHello(player, Object);
                    return true;
                }

                uint32 NextRank = MeVipRank + 1;
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6607), MeVipRank), 0, Itr->second.Num);
                std::string Oss;
                if (MeVipRank > 0)
                {
                    Oss.append(Acore::StringFormat(sHelper->GetGts(6608), sPlayerModMgr->GetVipMenuStr(MeVipRank)));
                }
                else
                {
                    Oss.append(sHelper->GetGts(6609));
                }

                Oss.append(Acore::StringFormat(sHelper->GetGts(6610), sPlayerModMgr->GetVipMenuStr(NextRank)));

                AddGossipItemFor(player, GOSSIP_ICON_TALK, Oss, 0, Itr->second.Num);

                uint32 VNtempId = sPlayerModMgr->GetVipNTempId(NextRank);
                if (VNtempId)
                {
                    std::string Gts = sHelper->GetNeedGts(VNtempId);
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6611), NextRank, Itr->second.Num, Gts, 0, false);
                }
                else
                {
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6611), NextRank, Itr->second.Num);
                }
                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                GossipSetMenuId(player, 8888);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, Object->GetGUID());
                return true;
            }
            break;
            case DEF_8:
            {
                uint32 Delay = uint32(sHelper->GetUInt(1035));
                if (Delay)
                {
                    time_t Current = time(NULL);
                    if (player->ClearCombatDelay < Current)
                    {
                        player->ClearCombatDelay = Current + Delay;
                        sHelper->DoClearInCombat(player);
                        player->SendSysMsg(sHelper->GetGts(6612));
                    }
                    else
                    {
                        uint32 CheckTime = player->ClearCombatDelay - Current;
                        player->SendSysMsg(sHelper->GetGts(6613), CheckTime);
                    }
                }
                else
                {
                    sHelper->DoClearInCombat(player);
                    player->SendSysMsg(sHelper->GetGts(6614));
                }
                ShowParentGossipHello(player, Object);
                return true;
            }
            break;
            case DEF_9:
            {
                for (auto It = ResetFBStore.begin(); It != ResetFBStore.end(); ++It)
                {
                    if (It->second.Menu == 1)
                    {
                        std::string Gts = sHelper->GetNeedGts(It->second.NTempId);
                        if (It->second.MapId > C_NOT_FB && It->second.NTempId)
                            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->second.Gossip, It->second.MapId, It->second.Num, Acore::StringFormat(sHelper->GetGts(6603), sHelper->GetMapName(It->second.MapId)) + Gts, 0, false);
                        else
                            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->second.Gossip, It->second.MapId, It->second.Num);
                    }
                }

                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                GossipSetMenuId(player, 7777);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, Object->GetGUID());
                return true;
            }
            break;
            case DEF_10:
            {
                uint32 AccountId = player->GetSession()->GetAccountId();
                LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_ITEM);
                Stmt->SetData(0, AccountId);
                Stmt->SetData(1, 0);
                PreparedQueryResult Result = LoginDatabase.Query(Stmt);
                if (!Result)
                {
                    player->SendSysMsg(sHelper->GetGts(2501));
                    ShowParentGossipHello(player, Object);
                    return true;
                }
                else
                {
                    uint32 Bp = uint32(sHelper->GetInt(1502));
                    std::string RItemIdLink = sHelper->GetItemLink(uint32(sHelper->GetInt(1501)));
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6615), RItemIdLink), 0, Itr->second.Num);

                    uint32 Index = 0;
                    do
                    {
                        Field* Fields = Result->Fetch();
                        uint32 Num = Fields[0].Get<uint32>();
                        uint64 Count = Fields[1].Get<uint64>();
                        uint64 Time = Fields[2].Get<uint64>();
                        Index++;
                        if (Index > 24)
                            continue;

                        std::string TimeStr = Acore::Time::TimeToTimestampStr(Seconds(static_cast<int64>(Time)));
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6616), Index, Count, Count * Bp, TimeStr), Num, Itr->second.Num);

                    } while (Result->NextRow());

                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6604), 0, 0);
                    GossipSetMenuId(player, 6666);
                    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, Object->GetGUID());
                    return true;
                }
            }
            break;
            case DEF_11:
            {
                uint32 KItemId = sHelper->GetUInt32(2101);
                uint32 KCount = sHelper->GetUInt32(2102);
                uint32 AuraId = sHelper->GetUInt32(2103);
                if (!KItemId && !KCount && !AuraId)
                {
                    ShowParentGossipHello(player, Object);
                    return true;
                }
                if (player->GetTempAt(AT2001) == AuraId && player->HasAura(AuraId))
                {
                    player->SendSysMsg(sHelper->GetGts(3101));
                }
                else
                {
                    if (!player->HasItemCount(KItemId, KCount))
                    {
                        player->SendSysMsg(sHelper->GetGts(1011), sHelper->GetItemLink(KItemId).c_str(), KCount);
                    }
                    else
                    {
                        player->DestroyItemCount(KItemId, KCount, true);
                        player->AddAura(AuraId, player);
                        player->UpdateTempAt(AT2001, AuraId);
                        player->SendSysMsg(sHelper->GetGts(3102));
                    }
                }

                ShowParentGossipHello(player, Object);
                return true;
            }
            break;
            case DEF_12:
            {
                uint32 VipRank = player->GetVipRank();
                uint64 Token = player->GetToken();
                uint64 AsToken = player->GetAsToken();
                player->SendSysMsg(sHelper->GetGts(1054), sHelper->GetNameLinkColor(player).c_str(), VipRank);
                player->SendSysMsg(sHelper->GetGts(1055), sHelper->GetNameLinkColor(player).c_str(), Token, AsToken);
                ShowParentGossipHello(player, Object);
                return true;
            }
            break;
            case DEF_13:
            {
                player->CustomUnbindAll();
                player->SendSysMsg(sHelper->GetGts(6617));
                ShowParentGossipHello(player, Object);
                return true;
            }
            break;
            }
        }
        else
        {
            ShowParentGossipHello(player, Object);
            return true;
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

class ItemTargetEnch : public ItemScript
{
public:
    ItemTargetEnch() : ItemScript("ItemTargetEnch") {}

    void OnGossipTargetEnch(Player* player, Item* OnItem, uint32 ItemGuid)
    {
        ClearGossipMenuFor(player);
        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid)))
        {
            uint32 TEnch1 = TargetItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_0);
            uint32 TEnch2 = TargetItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_1);
            uint32 TEnch3 = TargetItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_2);
            uint32 TEnch4 = TargetItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_3);
            uint32 TEnch5 = TargetItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_4);

            bool NotTEnch = (TEnch1 == 0
                && TEnch2 == 0
                && TEnch3 == 0
                && TEnch4 == 0
                && TEnch5 == 0
                );

            if (NotTEnch)
            {
                player->SendSysMsg(sHelper->GetGts(1150));
                return;
            }

            auto Itr = ItemEnchStore.find(TargetItem->GetEntry());
            if (Itr == ItemEnchStore.end())
                return;

            if (Itr->second.Rts < 0)
            {
                player->SendSysMsg(sHelper->GetGts(1303));
                return;
            }

            if (Itr->second.Rts > 0 && int32(TargetItem->ItemRtsCount) > Itr->second.Rts)
            {
                player->SendSysMsg(sHelper->GetGts(1301));
                CloseGossipMenuFor(player);
                return;
            }

            if (Itr->second.Rts > 0)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6619), TargetItem->ItemRtsCount, Itr->second.Rts), 0, ItemGuid);
            else if (Itr->second.Rts == 0)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Acore::StringFormat(sHelper->GetGts(6620), TargetItem->ItemRtsCount), 0, ItemGuid);

            uint32 NTempId = sItemFuncMgr->GetCustomEnchNTempId(TargetItem->GetEntry());
            if (NTempId)
            {
                std::string Gts = sHelper->GetNeedGts(NTempId);
                if (TEnch1 > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetEnchName(TEnch1), PROP_ENCHANTMENT_SLOT_0, ItemGuid, Gts, 0, false);
                if (TEnch2 > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetEnchName(TEnch2), PROP_ENCHANTMENT_SLOT_1, ItemGuid, Gts, 0, false);
                if (TEnch3 > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetEnchName(TEnch3), PROP_ENCHANTMENT_SLOT_2, ItemGuid, Gts, 0, false);
                if (TEnch4 > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetEnchName(TEnch4), PROP_ENCHANTMENT_SLOT_3, ItemGuid, Gts, 0, false);
                if (TEnch5 > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetEnchName(TEnch5), PROP_ENCHANTMENT_SLOT_4, ItemGuid, Gts, 0, false);
            }
            else
            {
                if (TEnch1 > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetEnchName(TEnch1), PROP_ENCHANTMENT_SLOT_0, ItemGuid);
                if (TEnch2 > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetEnchName(TEnch2), PROP_ENCHANTMENT_SLOT_1, ItemGuid);
                if (TEnch3 > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetEnchName(TEnch3), PROP_ENCHANTMENT_SLOT_2, ItemGuid);
                if (TEnch4 > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetEnchName(TEnch4), PROP_ENCHANTMENT_SLOT_3, ItemGuid);
                if (TEnch5 > 0)
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetEnchName(TEnch5), PROP_ENCHANTMENT_SLOT_4, ItemGuid);
            }

            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, OnItem->GetGUID());
        }
    }

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        uint32 FuncItemId = CastItem->GetEntry();
        uint32 TargetItemId = TargetItem->GetEntry();
        uint32 ItemGuid = TargetItem->GetGUIDLow();

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        auto Itr = ItemEnchStore.find(TargetItemId);
        if (Itr == ItemEnchStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(1303));
            return false;
        }

        OnGossipTargetEnch(player, CastItem, ItemGuid);
        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        if (Sender == 0)
        {
            OnGossipTargetEnch(player, OnItem, Action);
            return;
        }

        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, Action)))
        {
            auto Itr = ItemEnchStore.find(TargetItem->GetEntry());
            if (Itr == ItemEnchStore.end())
            {
                OnGossipTargetEnch(player, OnItem, Action);
                return;
            }

            if (Itr->second.Rts < 0)
            {
                player->SendSysMsg(sHelper->GetGts(1303));
                CloseGossipMenuFor(player);
                return;
            }

            if (Itr->second.Rts > 0 && int32(TargetItem->ItemRtsCount) >= Itr->second.Rts)
            {
                player->SendAsMsg(sHelper->GetGts(1301));
                CloseGossipMenuFor(player);
                return;
            }

            uint32 NTempId = sItemFuncMgr->GetCustomEnchNTempId(TargetItem->GetEntry());
            if (!sHelper->CheckNeed(player, NTempId, "随机附魔洗练消耗"))
            {
                CloseGossipMenuFor(player);
                return;
            }

            TargetItem->ItemRtsCount++;

            sItemFuncMgr->DoUseCustomEnchOne(TargetItem, EnchantmentSlot(Sender));
            player->SendSysMsg(sHelper->GetGts(1302));

            sItemFuncMgr->UpdateEnchRtsDB(player, TargetItem);
            OnGossipTargetEnch(player, OnItem, Action);
            return;
        }

        CloseGossipMenuFor(player);
    }
};

class ItemUseVipRank : public ItemScript
{
public:
    ItemUseVipRank() : ItemScript("ItemUseVipRank") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (!CastItem)
            return true;

        uint32 FuncItemId = CastItem->GetEntry();
        uint32 TarRank = CastItem->GetTemplate()->ItemLevel;
        uint32 NowRank = player->GetVipRank();

        if (!TarRank)
            return false;

        if (!(TarRank > NowRank))
            return false;

        if (!sHelper->CheckNeed(player, sPlayerModMgr->GetVipNTempId(TarRank), "物品使用获得会员消耗"))
            return false;

        player->SetVipRank(TarRank);
        uint32 VNtempId = sPlayerModMgr->GetVipRTempId(TarRank);
        sHelper->DoTheReward(player, VNtempId, "提升会员奖励");

        if (NowRank < TarRank)
            player->CastSpell(player, TxLevelSpell, false);

        auto Iter = ItemScriptOnDeleteStore.find(FuncItemId);
        if (Iter != ItemScriptOnDeleteStore.end())
        {
            if (Iter->second > 0)
            {
                player->DestroyItemCount(FuncItemId, 1, true);
                return true;
            }
        }

        return false;
    }
};

class ItemUseVipRankOne : public ItemScript
{
public:
    ItemUseVipRankOne() : ItemScript("ItemUseVipRankOne") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (!CastItem)
            return true;

        uint32 FuncItemId = CastItem->GetEntry();
        uint32 AddRank = player->GetVipRank() + 1;

        uint32 MaxRank = sPlayerModMgr->GetVipMaxRank();
        if (AddRank > MaxRank)
            return false;

        if (!sHelper->CheckNeed(player, sPlayerModMgr->GetVipNTempId(AddRank), "物品使用获得会员消耗"))
            return false;

        player->SetVipRank(AddRank);
        uint32 VNtempId = sPlayerModMgr->GetVipRTempId(AddRank);
        sHelper->DoTheReward(player, VNtempId, "物品使用获得VIP奖励");
        player->CastSpell(player, TxLevelSpell, false);

        auto Iter = ItemScriptOnDeleteStore.find(FuncItemId);
        if (Iter != ItemScriptOnDeleteStore.end())
        {
            if (Iter->second > 0)
            {
                player->DestroyItemCount(FuncItemId, 1, true);
                return true;
            }
        }

        return false;
    }
};

class NpcCdKey : public CreatureScript
{
public:
    NpcCdKey() : CreatureScript("NpcCdKey") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6621), 555, 555);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6622), 666, 666, "", 0, true);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        if (Sender == 555 && Action == 555)
        {
            OnGossipHello(player, creature);
            return true;
        }

        OnGossipHello(player, creature);
        return true;
    }

    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 Sender, uint32 Action, const char* Code) override
    {
        ClearGossipMenuFor(player);
        if (Sender == 666 && Action == 666 && Code && *Code != '\0')
        {
            if (CdkeyStore.empty())
            {
                player->SendSysMsg(sHelper->GetGts(1110));
                OnGossipHello(player, creature);
                return true;
            }

            time_t Current = GameTime::GetGameTime().count();
            if (Current < player->ErrorTime)
            {
                player->SendSysMsg(sHelper->GetGts(1088), player->ErrorTime - Current);
                OnGossipHello(player, creature);
                return true;
            }

            auto Itr = CdkeyStore.find(std::string(Code));
            if (Itr == CdkeyStore.end())
            {
                player->SendSysMsg(sHelper->GetGts(1090));

                player->ErrorCount++;
                if (player->ErrorCount >= 3)
                {
                    player->ErrorCount = 0;
                    player->ErrorTime = Current + 30;
                }

                OnGossipHello(player, creature);
                return true;
            }

            player->ErrorCount = 0;
            player->ErrorTime = Current;
            sHelper->DoTheReward(player, Itr->second, "兑换码奖励");
            std::string escapedCode(Code);
            LoginDatabase.EscapeString(escapedCode);
            LoginDatabase.Execute("DELETE FROM _兑换码奖励表 WHERE 兑换码 = '{}'", escapedCode);
            CdkeyStore.erase(Itr);
            player->SendSysMsg(sHelper->GetGts(1091));
            CloseGossipMenuFor(player);
            return true;
        }

        player->SendSysMsg(sHelper->GetGts(1090));
        OnGossipHello(player, creature);
        return true;
    }
};

class ItemUseAoeLoot : public ItemScript
{
public:
    ItemUseAoeLoot() : ItemScript("ItemUseAoeLoot") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (!CastItem)
            return true;

        if (player->GetGroup())
        {
            player->SendSysMsg(sHelper->GetGts(6623));
            return false;
        }

        if (!sHelper->CanItemLootBag(player))
        {
            player->SendSysMsg(sHelper->GetGts(6624));
            return false;
        }

        uint32 FuncItemId = CastItem->GetEntry();

        float Range = sHelper->GetFloat(1049);
        std::list<Creature*> DeathList;
        player->GetDeadCreatureListInGrid(DeathList, Range);

        bool IsLootNow = false;
        for (auto Itr = DeathList.begin(); Itr != DeathList.end(); ++Itr)
        {
            Creature* Death = *Itr;
            if (Death->IsAlive())
                continue;
            Player* Looter = Death->GetLootRecipient();
            if (!Looter)
                continue;
            if (Looter->GetGUID() != player->GetGUID())
                continue;
            Loot* AtLoot = &Death->loot;
            AtLoot->NotifyMoneyRemoved();
            player->ModifyMoney(AtLoot->gold);
            player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, AtLoot->gold);
            WorldPacket Packet(SMSG_LOOT_MONEY_NOTIFY, 8 + 1);
            Packet << uint64(AtLoot->gold);
            Packet << uint8(1);
            player->GetSession()->SendPacket(&Packet);
            AtLoot->gold = 0;

            uint32 MaxSlot = AtLoot->GetMaxSlotInLootFor(player);

            for (uint32 i = 0; i < MaxSlot; ++i)
            {
                if (LootItem* LItem = AtLoot->LootItemInSlot(i, player))
                {
                    if (LItem->is_looted)
                        continue;
                    if (!LItem->AllowedForPlayer(player, AtLoot->sourceWorldObjectGUID))
                        continue;
                    if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(LItem->itemid))
                        if ((proto->Flags & ITEM_FLAG_MULTI_DROP) && proto->IsCurrencyToken())
                            continue;

                    IsLootNow = true;
                    if (player->AddItem(LItem->itemid, LItem->count))
                    {
                        player->SendNotifyLootItemRemoved(0);
                        player->SendLootRelease(player->GetLootGUID());
                    }
                }
            }

            if (!AtLoot->empty())
            {
                Death->AllLootRemovedFromCorpse();
                Death->RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                AtLoot->clear();

                if (Death->HasUnitFlag(UNIT_FLAG_SKINNABLE))
                    Death->RemoveUnitFlag(UNIT_FLAG_SKINNABLE);
            }
            else
            {
                Death->RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                Death->AllLootRemovedFromCorpse();
            }
        }

        if (!IsLootNow)
            player->SendSysMsg(sHelper->GetGts(1094));
        else
            player->SendSysMsg(sHelper->GetGts(1095));

        auto Iter = ItemScriptOnDeleteStore.find(FuncItemId);
        if (Iter != ItemScriptOnDeleteStore.end())
        {
            if (Iter->second > 0)
            {
                player->DestroyItemCount(FuncItemId, 1, true);
                return true;
            }
        }

        return false;
    }
};

class NpcSoulItem : public CreatureScript
{
public:
    NpcSoulItem() : CreatureScript("NpcSoulItem") { }

    void ShowSoulItemGossipHello(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        for (uint8 Slot = SOUL_SLOT_START; Slot < SOUL_SLOT_END; ++Slot)
        {
            uint32 Entry = sItemFuncMgr->GetSoulItemEntry(player->GetGUIDLow(), Slot);
            std::string Icon = Entry ? sHelper->GetItemIcon(Entry, 30, 30, -18, 0) : sItemFuncMgr->GetSoulSlotIcon(Slot, 30, 30, -18, 0);
            std::string Link = Entry ? " " + sHelper->GetItemLink(Entry) : "";
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Icon + sItemFuncMgr->GetSoulSlotName(Slot) + Link, 0, Slot + GOSSIP_OPTION_GOSSIP);
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6626), 0, 0, sHelper->GetGts(6625), 0, false);
        GossipSetMenuId(player, 1001);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    void ShowSoulItemTheItemsGossipHello(Player* player, Creature* creature, uint8 Slot)
    {
        uint32 Entry = sItemFuncMgr->GetSoulItemEntry(player->GetGUIDLow(), Slot - GOSSIP_OPTION_GOSSIP);

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6627), 0, Slot);

        for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            Item* NewItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (!NewItem)
                continue;
            if (!sItemFuncMgr->CanEquipSoul(player, NewItem, Slot - GOSSIP_OPTION_GOSSIP))
                continue;

            uint32 SoulNTempId = sItemFuncMgr->GetSoulNTempId(NewItem->GetEntry());
            if (SoulNTempId)
            {
                std::ostringstream Oss;
                if (!Entry)
                {
                    Oss << sHelper->GetGts(6628);
                }
                else
                {
                    Oss << Acore::StringFormat(sHelper->GetGts(6629), sHelper->GetItemLink(Entry));
                    Oss << sHelper->GetItemLink(NewItem->GetEntry()) << "\n\n";
                }

                Oss << sHelper->GetNeedGts(SoulNTempId);
                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetItemIcon(NewItem->GetEntry(), 30, 30, -18, 0) + sHelper->GetItemLink(NewItem->GetEntry()), NewItem->GetGUIDLow(), Slot, Oss.str().c_str(), 0, false);
            }
            else
            {
                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetItemIcon(NewItem->GetEntry(), 30, 30, -18, 0) + sHelper->GetItemLink(NewItem->GetEntry()), NewItem->GetGUIDLow(), Slot);
            }
        }

        for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            Bag* MeBag = player->GetBagByPos(i);
            if (!MeBag)
                continue;
            for (uint32 j = 0; j < MeBag->GetBagSize(); ++j)
            {
                Item* NewItem = player->GetItemByPos(i, j);
                if (!NewItem)
                    continue;
                if (!sItemFuncMgr->CanEquipSoul(player, NewItem, Slot - GOSSIP_OPTION_GOSSIP))
                    continue;

                uint32 SoulNTempId = sItemFuncMgr->GetSoulNTempId(NewItem->GetEntry());
                if (SoulNTempId)
                {
                    std::ostringstream Ass;
                    if (!Entry)
                    {
                        Ass << sHelper->GetGts(6630);
                    }
                    else
                    {
                        Ass << Acore::StringFormat(sHelper->GetGts(6629), sHelper->GetItemLink(Entry));
                        Ass << sHelper->GetItemLink(NewItem->GetEntry()) << "\n\n";
                    }

                    Ass << sHelper->GetNeedGts(SoulNTempId);
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetItemIcon(NewItem->GetEntry(), 30, 30, -18, 0) + sHelper->GetItemLink(NewItem->GetEntry()), NewItem->GetGUIDLow(), Slot, Ass.str().c_str(), 0, false);
                }
                else
                {
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetItemIcon(NewItem->GetEntry(), 30, 30, -18, 0) + sHelper->GetItemLink(NewItem->GetEntry()), NewItem->GetGUIDLow(), Slot);
                }
            }
        }

        if (Entry)
        {
            AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6633), 0, Slot, Acore::StringFormat(sHelper->GetGts(6632), sHelper->GetItemLink(Entry)), 0, false);
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6634), 0, 0);
        GossipSetMenuId(player, 1002);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ShowSoulItemGossipHello(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        if (!Sender && !Action)
        {
            ShowSoulItemGossipHello(player, creature);
            return true;
        }

        if (Action >= SOUL_SLOT_MAX)
        {
            CloseGossipMenuFor(player);
            return true;
        }

        uint32 MenuId = GossipGetMenuId(player);
        if (!Sender)
        {
            if (MenuId == 1001)
            {
                ShowSoulItemTheItemsGossipHello(player, creature, Action);
                return true;
            }
            else
            {
                uint32 Entry = sItemFuncMgr->GetSoulItemEntry(player->GetGUIDLow(), Action - GOSSIP_OPTION_GOSSIP);
                if (Entry)
                {
                    uint32 OGuidLow = sItemFuncMgr->GetSoulItemGuid(player->GetGUIDLow(), Action - GOSSIP_OPTION_GOSSIP);

                    if (Item* OItem = sItemFuncMgr->GetEquipSoulItem(OGuidLow))
                    {

                        OItem->SetOwnerGUID(player->GetGUID());
                        sItemFuncMgr->ApplySoulItemStatsAndSpells(player, OItem, sItemFuncMgr->GetOriginalSlot(Action - GOSSIP_OPTION_GOSSIP), false);
                        CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();
                        ItemPosCountVec Dest;
                        uint8 Msg = player->CanStoreItem(NULL_BAG, NULL_SLOT, Dest, OItem);
                        if (Msg == EQUIP_ERR_OK)
                        {

                            player->MoveItemToInventory(Dest, OItem, true, true);
                            player->SaveInventoryAndGoldToDB(Trans);
                        }
                        else
                        {
                            MailDraft("双甲补发", "由于你背包空间不足\n补发你的").AddItem(OItem).SendMailTo(Trans, player, MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
                        }

                        sItemFuncMgr->RemoveSoulItemData(OGuidLow);
                        sItemFuncMgr->RemoveSoulItemDB(OGuidLow);

                        CharacterDatabase.CommitTransaction(Trans);

                        player->SendSysMsg(sHelper->GetGts(1070));
                    }

                    ShowSoulItemGossipHello(player, creature);
                    return true;
                }
            }
        }
        else
        {
            if (MenuId == 1002)
            {
                ObjectGuid Guid = ObjectGuid(HighGuid::Item, Sender);
                if (Item* NewItem = player->GetItemByGuid(Guid))
                {
                    if (sHelper->CheckNeed(player, sItemFuncMgr->GetSoulNTempId(NewItem->GetEntry()), "双甲装备消耗"))
                    {
                        sItemFuncMgr->UpdateSoulItem(player, NewItem, Action - GOSSIP_OPTION_GOSSIP);
                        player->SendSysMsg(sHelper->GetGts(1071));
                        ShowSoulItemGossipHello(player, creature);
                        return true;
                    }
                }
            }
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

class NpcNewCharInfo : public CreatureScript
{
public:
    NpcNewCharInfo() : CreatureScript("NpcNewCharInfo") { }

    void ShowItemPlayerInfoGossipHello(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        std::string Menu1 = sHelper->GetStrConfig(9731);
        std::string Menu2 = sHelper->GetStrConfig(9732);
        std::string Menu3 = sHelper->GetStrConfig(9733);
        std::string Menu4 = sHelper->GetStrConfig(9734);
        std::string Menu5 = sHelper->GetStrConfig(9735);

        if (!Menu1.empty())
        {
            uint32 NTempId = sHelper->GetUInt(1027);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu1, 0, 1, sHelper->GetGts(6635) + sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu1, 0, 1);
        }
        if (!Menu2.empty())
        {
            uint32 NTempId = sHelper->GetUInt(1028);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu2, 0, 2, sHelper->GetGts(6636) + sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu2, 0, 2);
        }
        if (!Menu3.empty())
        {
            uint32 NTempId = sHelper->GetUInt(1029);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu3, 0, 3, sHelper->GetGts(6637) + sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu3, 0, 3);
        }
        if (!Menu4.empty())
        {
            uint32 NTempId = sHelper->GetUInt(1030);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu4, 0, 4, sHelper->GetGts(6638) + sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu4, 0, 4);
        }
        if (!Menu5.empty())
        {
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu5, 0, 5);
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ShowItemPlayerInfoGossipHello(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        if (!Sender && !Action)
        {
            ShowItemPlayerInfoGossipHello(player, creature);
            return true;
        }

        if (!Sender)
        {
            if (Action < 5)
            {
                if (Action == 1)
                {
                    if (sHelper->CheckNeed(player, sHelper->GetUInt(1027), "改变阵营消耗"))
                    {
                        player->SetAtLoginFlag(AT_LOGIN_CHANGE_FACTION);
                        player->SendSysMsg(sHelper->GetGts(1096));
                        if (Guild* MeGuild = player->GetGuild())
                            MeGuild->HandleLeaveMember(player->GetSession());

                        player->GetSession()->LogoutPlayer(true);
                    }
                }
                else if (Action == 2)
                {
                    if (sHelper->CheckNeed(player, sHelper->GetUInt(1028), "改变种族消耗"))
                    {
                        player->SetAtLoginFlag(AT_LOGIN_CHANGE_RACE);
                        player->SendSysMsg(sHelper->GetGts(1097));
                        if (Guild* MeGuild = player->GetGuild())
                            MeGuild->HandleLeaveMember(player->GetSession());

                        player->GetSession()->LogoutPlayer(true);
                    }
                }
                else if (Action == 3)
                {
                    if (sHelper->CheckNeed(player, sHelper->GetUInt(1029), "改变名字消耗"))
                    {
                        player->SetAtLoginFlag(AT_LOGIN_RENAME);
                        player->SendSysMsg(sHelper->GetGts(1098));
                        player->GetSession()->LogoutPlayer(true);
                    }
                }
                else if (Action == 4)
                {
                    if (sHelper->CheckNeed(player, sHelper->GetUInt(1030), "改变外观消耗"))
                    {
                        player->SetAtLoginFlag(AT_LOGIN_CUSTOMIZE);
                        player->SendSysMsg(sHelper->GetGts(1099));
                        if (Guild* MeGuild = player->GetGuild())
                            MeGuild->HandleLeaveMember(player->GetSession());

                        player->GetSession()->LogoutPlayer(true);
                    }
                }
            }
            else
            {
                ClearGossipMenuFor(player);
                uint8 Race = player->GetRace();
                uint8 Class = player->GetClass();
                std::string Gts = sHelper->GetGts(6639) + sHelper->GetNeedGts(sHelper->GetUInt(1031)) + sHelper->GetGts(6640);

                for (uint32 Cs = CLASS_WARRIOR; Cs <= CLASS_DRUID; Cs++)
                {
                    if (Cs == 10 || Class == Cs)
                        continue;

                    if (!sObjectMgr->GetPlayerInfo(Race, Cs))
                        continue;

                    if (sHelper->GetUInt(1031))
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGossipClassString(Cs), Cs, Cs, Gts, 0, false);
                    else
                        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGossipClassString(Cs), Cs, Cs);
                }

                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6634), 0, 0);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
                return true;
            }
        }
        else
        {
            if (Sender)
            {
                uint32 ClassMask = sWorld->getIntConfig(CONFIG_CHARACTER_CREATING_DISABLED_CLASSMASK);
                if ((1 << (Sender - 1)) & ClassMask)
                {
                    player->SendSysMsg(sHelper->GetGts(6641));
                    CloseGossipMenuFor(player);
                    return true;
                }

                if (!sHelper->CheckNeed(player, sHelper->GetUInt(1031), "转职消耗"))
                {
                    CloseGossipMenuFor(player);
                    return true;
                }

                CloseGossipMenuFor(player);
                sHelper->SetClassToNew(player, Sender);
            }
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

class ItemUseTalent : public ItemScript
{
public:
    ItemUseTalent() : ItemScript("ItemUseTalent") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (!CastItem)
            return true;

        uint32 FuncItemId = CastItem->GetEntry();
        uint32 TarRank = CastItem->GetTemplate()->ItemLevel;
        if (!TarRank)
            return false;

        uint32 MeCount = player->GetTempAtUInt32(AT02);
        player->UpdateTempAt(AT02, TarRank + MeCount);
        player->InitTalentForLevel();
        player->SendSysMsg(sHelper->GetGts(1401), TarRank);

        auto Iter = ItemScriptOnDeleteStore.find(FuncItemId);
        if (Iter != ItemScriptOnDeleteStore.end())
        {
            if (Iter->second > 0)
            {
                player->DestroyItemCount(FuncItemId, 1, true);
                return true;
            }
        }

        return false;
    }
};

class ItemUseProfessions : public ItemScript
{
public:
    ItemUseProfessions() : ItemScript("ItemUseProfessions") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (!CastItem)
            return true;

        uint32 FuncItemId = CastItem->GetEntry();
        uint32 TarRank = CastItem->GetTemplate()->ItemLevel;
        if (!TarRank)
            return false;

        uint32 MeCount = player->GetTempAtUInt32(AT03);
        player->UpdateTempAt(AT03, TarRank + MeCount);
        uint32 Points = player->GetFreePrimaryProfessionPoints();
        player->SetFreePrimaryProfessions(TarRank + Points);
        player->SendSysMsg(sHelper->GetGts(1402), TarRank);

        auto Iter = ItemScriptOnDeleteStore.find(FuncItemId);
        if (Iter != ItemScriptOnDeleteStore.end())
        {
            if (Iter->second > 0)
            {
                player->DestroyItemCount(FuncItemId, 1, true);
                return true;
            }
        }

        return false;
    }
};

class ItemTaskSocket : public ItemScript
{
public:
    ItemTaskSocket() : ItemScript("ItemTaskSocket") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        uint32 FuncItemId = CastItem->GetEntry();
        uint32 TargetItemId = TargetItem->GetEntry();
        uint32 ItemGuid = TargetItem->GetGUIDLow();

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        uint32 S1 = TargetItem->GetEnchantmentId(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT));
        uint32 S2 = TargetItem->GetEnchantmentId(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT_2));
        uint32 S3 = TargetItem->GetEnchantmentId(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT_3));
        if (!S1 && !S2 && !S3)
        {
            player->SendSysMsg(sHelper->GetGts(1083));
            return false;
        }

        uint32 NTempId = sHelper->GetUInt(1026);
        if (NTempId)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "", 0, ItemGuid, sHelper->GetGts(6642) + sHelper->GetNeedGts(NTempId), 0, false);
        else
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "", 0, ItemGuid);

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, CastItem->GetGUID());
        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, Action)))
        {
            uint32 FuncItemId = OnItem->GetEntry();
            uint32 NTempId = sHelper->GetUInt(1026);
            if (sHelper->CheckNeed(player, NTempId, "宝石拆卸消耗"))
            {
                for (uint32 ESlot = SOCK_ENCHANTMENT_SLOT; ESlot < SOCK_ENCHANTMENT_SLOT + 3; ++ESlot)
                {
                    uint32 EnchantId = TargetItem->GetEnchantmentId(EnchantmentSlot(ESlot));
                    if (!EnchantId)
                        continue;
                    SpellItemEnchantmentEntry const* EnchantEntry = sSpellItemEnchantmentStore.LookupEntry(EnchantId);
                    if (!EnchantEntry)
                        continue;

                    uint32 GemId = EnchantEntry->GemID;

                    if (sItemFuncMgr->InItemNotTask(GemId))
                        continue;

                    if (GemId)
                    {
                        ItemTemplate const* GemProto = sObjectMgr->GetItemTemplate(GemId);
                        if (GemProto)
                        {
                            player->AddItem(GemId, 1, "宝石拆卸获得");
                            player->ApplyEnchantment(TargetItem, EnchantmentSlot(ESlot), false);
                            TargetItem->ClearEnchantment(EnchantmentSlot(ESlot));
                        }
                    }
                }

                player->DestroyItemCount(FuncItemId, 1, true);
                player->SendSysMsg(sHelper->GetGts(1084));
                player->SetCanModifyStats(true);
                player->UpdateAllStats();
            }
        }

        CloseGossipMenuFor(player);
    }
};

class ItemTargetUpGuid : public ItemScript
{
public:
    ItemTargetUpGuid() : ItemScript("ItemTargetUpGuid") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        uint32 FuncItemId = CastItem->GetEntry();
        uint32 TargetItemId = TargetItem->GetEntry();

        if (!sItemRandomAtMgr->IsInItemSuperUp(TargetItemId))
        {
            player->SendSysMsg(sHelper->GetGts(1169));
            return false;
        }

        uint32 ItemGuid = TargetItem->GetGUIDLow();
        uint32 NextLevel = sItemRandomAtMgr->GetNextULevel(ItemGuid, TargetItemId);
        if (!NextLevel)
        {
            player->SendSysMsg(sHelper->GetGts(1168));
            return false;
        }

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        if (TargetItem->GetCount() > 1)
        {
            player->SendSysMsg(sHelper->GetGts(1060));
            return false;
        }

        if (TargetItem->NowCount)
        {
            player->SendSysMsg(sHelper->GetGts(5801));
            return false;
        }

        ClearGossipMenuFor(player);

        uint32 NTempId = sItemRandomAtMgr->GetUNTempId(TargetItemId, NextLevel);
        bool OnSave = sItemRandomAtMgr->GetUOnSave(TargetItemId, NextLevel);
        float UXy = sItemRandomAtMgr->GetUChance(TargetItemId, NextLevel);

        uint32 XyItem = sItemRandomAtMgr->GetSuperXyItem(TargetItemId, NextLevel);
        uint32 XyCount = sItemRandomAtMgr->GetSuperXyCount(TargetItemId, NextLevel);
        uint32 XyOdds = sItemRandomAtMgr->GetSuperXyOdds(TargetItemId, NextLevel);
        uint32 FBItem = sItemRandomAtMgr->GetSuperFBItem(TargetItemId, NextLevel);
        bool Flag = sItemRandomAtMgr->GetSuperFBFlag(TargetItemId, NextLevel);

        uint32 AtCount = player->GetItemCount(XyItem);
        AtCount = AtCount > XyCount ? XyCount : AtCount;
        std::string GtsXyOdds = sHelper->ToExString(XyOdds * AtCount);

        std::ostringstream Ass;
        Ass << Acore::StringFormat(sHelper->GetGts(6643), sHelper->GetNeedGts(NTempId), sHelper->ToExString(UXy), GtsXyOdds);
        Ass << Acore::StringFormat(sHelper->GetGts(7801), sHelper->GetItemLink(XyItem), AtCount, sHelper->ToExString(XyOdds));
        if (OnSave)
            Ass << sHelper->GetGts(6644);
        else
            Ass << sHelper->GetGts(6645);

        float DXy = sItemRandomAtMgr->GetDChance(TargetItemId, NextLevel);
        uint32 DLevel = sItemRandomAtMgr->GetDLevel(TargetItemId, NextLevel);
        if (UXy < 100.0f && DXy > 0.0f && DLevel > 0)
        {
            Ass << Acore::StringFormat(sHelper->GetGts(6646), sHelper->ToExString(DXy), DLevel);
        }

        uint32 NewId = sItemRandomAtMgr->GetNewId(TargetItemId, NextLevel);
        if (NewId)
        {
            Ass << sHelper->GetGts(6647);
            Ass << Acore::StringFormat(sHelper->GetGts(6648), sHelper->GetItemLink(NewId));
        }

        if (Flag && FBItem)
            Ass << Acore::StringFormat(sHelper->GetGts(7802), sHelper->GetItemLink(FBItem));
        else
            Ass << Acore::StringFormat(sHelper->GetGts(7803));

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "", 0, ItemGuid, Ass.str(), 0, false);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, CastItem->GetGUID());

        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, Action)))
        {
            uint32 ItemId = TargetItem->GetEntry();
            uint32 NextLevel = sItemRandomAtMgr->GetNextULevel(Action, ItemId);
            if (NextLevel)
            {
                uint32 NTempId = sItemRandomAtMgr->GetUNTempId(ItemId, NextLevel);
                if (sHelper->CheckNeed(player, NTempId, "超级随机升级"))
                {
                    bool OnReset = sItemRandomAtMgr->GetUOnSave(ItemId, NextLevel);
                    uint32 XyItem = sItemRandomAtMgr->GetSuperXyItem(ItemId, NextLevel);
                    uint32 XyCount = sItemRandomAtMgr->GetSuperXyCount(ItemId, NextLevel);
                    uint32 XyOdds = sItemRandomAtMgr->GetSuperXyOdds(ItemId, NextLevel);
                    uint32 FBItem = sItemRandomAtMgr->GetSuperFBItem(ItemId, NextLevel);
                    bool Flag = sItemRandomAtMgr->GetSuperFBFlag(ItemId, NextLevel);

                    uint32 AtCount = player->GetItemCount(XyItem);
                    AtCount = AtCount > XyCount ? XyCount : AtCount;

                    if (XyItem > 0)
                        player->DestroyItemCount(XyItem, AtCount, true);

                    float Xy = sItemRandomAtMgr->GetUChance(ItemId, NextLevel);
                    if (roll_chance_f(Xy + XyOdds * AtCount))
                    {
                        uint32 NewId = sItemRandomAtMgr->GetNewId(ItemId, NextLevel);
                        if (NewId)
                        {
                            if (OnReset)
                                player->AddItemSaveRandomSock(TargetItem, NewId);
                            else
                                player->AddItem(NewId, 1, "超级随机升级获得");
                        }
                        else
                        {
                            sItemRandomAtMgr->DoUpPowerItemAt(TargetItem, NextLevel, !OnReset);
                            player->SendSysMsg(sHelper->GetGts(1166));
                        }
                    }
                    else
                    {
                        float DXy = sItemRandomAtMgr->GetDChance(ItemId, NextLevel);
                        uint32 DLevel = sItemRandomAtMgr->GetDLevel(ItemId, NextLevel);
                        if (DXy > 0.0f && roll_chance_f(DXy) && DLevel > 0)
                        {
                            uint32 AtLevel = sItemRandomAtMgr->GetNowULevel(Action) - DLevel;
                            sItemRandomAtMgr->DoUpPowerItemAt(TargetItem, AtLevel, !OnReset);
                            player->SendSysMsg(sHelper->GetGts(1167), AtLevel);
                        }
                        else
                        {
                            if (Flag)
                            {
                                if (FBItem > 0)
                                {
                                    if (!player->HasItemCount(FBItem))
                                    {
                                        player->DestroyItem(TargetItem->GetBagSlot(), TargetItem->GetSlot(), true);

                                        player->SendSysMsg(sHelper->GetGts(7804));
                                    }
                                    else
                                    {
                                        player->SendSysMsg(sHelper->GetGts(7805));
                                    }

                                    player->DestroyItemCount(FBItem, 1, true);
                                }
                                else
                                {
                                    player->SendSysMsg(sHelper->GetGts(1121));
                                }
                            }
                            else
                            {
                                player->SendSysMsg(sHelper->GetGts(1121));
                            }
                        }
                    }
                }
            }
        }

        CloseGossipMenuFor(player);
    }
};

class ItemTargetUpGuidLv : public ItemScript
{
public:
    ItemTargetUpGuidLv() : ItemScript("ItemTargetUpGuidLv") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        uint32 UseItemId = CastItem->GetEntry();
        uint32 TargetItemId = TargetItem->GetEntry();
        uint32 ItemGuid = TargetItem->GetGUIDLow();

        if (!sItemRandomAtMgr->IsInItemSuperUp(TargetItemId))
        {
            player->SendSysMsg(sHelper->GetGts(1169));
            return false;
        }

        if (!sItemRandomAtMgr->InUpDo(UseItemId, TargetItemId))
        {
            player->SendSysMsg(sHelper->GetGts(1169));
            return false;
        }

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        if (TargetItem->GetCount() > 1)
        {
            player->SendSysMsg(sHelper->GetGts(1060));
            return false;
        }

        uint32 NextLevel = sItemRandomAtMgr->GetNextULevel(ItemGuid, TargetItemId);
        if (!NextLevel)
        {
            player->SendSysMsg(sHelper->GetGts(1168));
            return false;
        }

        if (TargetItem->NowCount)
        {
            player->SendSysMsg(sHelper->GetGts(5801));
            return false;
        }

        float Xy = sItemRandomAtMgr->GetOChance(UseItemId, TargetItemId);
        uint32 MaxRank = sItemRandomAtMgr->GetMaxULevel(TargetItemId);
        uint32 TarRank = sItemRandomAtMgr->GetInUpRank(UseItemId, TargetItemId);
        TarRank = TarRank > MaxRank ? MaxRank : TarRank;
        if (TarRank)
        {
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "", TarRank, ItemGuid, Acore::StringFormat(sHelper->GetGts(6649), TarRank, sHelper->ToExString(Xy)), 0, false);
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, CastItem->GetGUID());
        }

        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, Action)))
        {
            uint32 FuncItemId = OnItem->GetEntry();
            uint32 TargetItemId = TargetItem->GetEntry();
            float Xy = sItemRandomAtMgr->GetOChance(FuncItemId, TargetItemId);
            if (roll_chance_f(Xy))
            {
                bool OnReset = sItemRandomAtMgr->GetUOnSave(TargetItemId, Sender);
                sItemRandomAtMgr->DoUpPowerItemAt(TargetItem, Sender, !OnReset);
                player->SendSysMsg(sHelper->GetGts(1166));
            }
            else
            {
                player->SendSysMsg(sHelper->GetGts(3601), Sender);
            }

            player->DestroyItemCount(FuncItemId, 1, true);
        }

        CloseGossipMenuFor(player);
    }
};

class ItemTargetUpGuidReset : public ItemScript
{
public:
    ItemTargetUpGuidReset() : ItemScript("ItemTargetUpGuidReset") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        uint32 UseItemId = CastItem->GetEntry();
        uint32 TargetItemId = TargetItem->GetEntry();
        uint32 ItemGuid = TargetItem->GetGUIDLow();

        if (!sItemRandomAtMgr->IsGenerateItemAt(TargetItemId))
        {
            player->SendSysMsg(sHelper->GetGts(3701));
            return false;
        }

        if (!sItemRandomAtMgr->IntResetDo(UseItemId, TargetItemId))
        {
            player->SendSysMsg(sHelper->GetGts(3701));
            return false;
        }

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        if (TargetItem->GetCount() > 1)
        {
            player->SendSysMsg(sHelper->GetGts(1060));
            return false;
        }

        std::string Gts = sHelper->GetNeedGts(sItemRandomAtMgr->GetResetNTempId(UseItemId, TargetItemId));
        std::string UChance = sHelper->ToExString(sItemRandomAtMgr->GetResetOChance(UseItemId, TargetItemId));
        std::string OnStr = sItemRandomAtMgr->InResetOnSave(UseItemId, TargetItemId) ? sHelper->GetGts(6650) : sHelper->GetGts(6651);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "", UseItemId, ItemGuid, Acore::StringFormat(sHelper->GetGts(6652), UChance, Gts, OnStr), 0, false);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, CastItem->GetGUID());

        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, Action)))
        {
            uint32 FuncItemId = OnItem->GetEntry();
            uint32 TargetItemId = TargetItem->GetEntry();

            if (!sItemRandomAtMgr->IntResetDo(FuncItemId, TargetItemId))
            {
                CloseGossipMenuFor(player);
                return;
            }

            uint32 TempId = sItemRandomAtMgr->GetResetNTempId(FuncItemId, TargetItemId);
            float UChance = sItemRandomAtMgr->GetResetOChance(FuncItemId, TargetItemId);
            bool OnSave = sItemRandomAtMgr->InResetOnSave(FuncItemId, TargetItemId);
            bool OnAtUpSave = sItemRandomAtMgr->InResetOnAtUpSave(FuncItemId, TargetItemId);
            uint32 GroupTempId = sItemRandomAtMgr->GetResetSpGroupId(FuncItemId, TargetItemId);

            if (roll_chance_f(UChance))
            {
                if (sItemRandomAtMgr->DoResetItemAt(TargetItem, TempId, GroupTempId, !OnSave, !OnAtUpSave))
                {
                    player->SendSysMsg(sHelper->GetGts(3702));
                    player->DestroyItemCount(FuncItemId, 1, true);
                }
            }
            else
            {
                player->SendSysMsg(sHelper->GetGts(3703));
            }
        }

        CloseGossipMenuFor(player);
    }
};

class DoItemRecovery : public ItemScript
{
public:
    DoItemRecovery() : ItemScript("DoItemRecovery") {}

    bool OnRecoveryGossipHello(Player* player, Item* CastItem)
    {
        ClearGossipMenuFor(player);

        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6653), 0, 0);
        for (auto Itr = ItemRyGroupStore.begin(); Itr != ItemRyGroupStore.end(); Itr++)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Itr->second, 0, Itr->first, sHelper->GetGts(6654), 0, false);

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, CastItem->GetGUID());
        return false;
    }

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (!CastItem)
            return false;

        OnRecoveryGossipHello(player, CastItem);

        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        if (!Sender && !Action)
        {
            OnRecoveryGossipHello(player, OnItem);
            return;
        }

        if (Action)
        {
            if (sItemFuncMgr->IsNotFuncRecovery())
            {
                player->SendSysMsg(sHelper->GetGts(1403));
                CloseGossipMenuFor(player);
                return;
            }

            if (!sItemFuncMgr->IsHasRecovery(player, Action))
            {
                player->SendSysMsg(sHelper->GetGts(1404));
                CloseGossipMenuFor(player);
                return;
            }

            sItemFuncMgr->DoItemRecovery(player, Action);
            player->SendSysMsg(sHelper->GetGts(1405));
        }

        CloseGossipMenuFor(player);
    }
};

class NpcItemRecovery : public CreatureScript
{
public:
    NpcItemRecovery() : CreatureScript("NpcItemRecovery") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6653), 0, 0);
        for (auto Itr = ItemRyGroupStore.begin(); Itr != ItemRyGroupStore.end(); Itr++)
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Itr->second, 0, Itr->first, sHelper->GetGts(6654), 0, false);

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

        if (Action)
        {
            if (sItemFuncMgr->IsNotFuncRecovery())
            {
                player->SendSysMsg(sHelper->GetGts(1403));
                CloseGossipMenuFor(player);
                return true;
            }

            if (!sItemFuncMgr->IsHasRecovery(player, Action))
            {
                player->SendSysMsg(sHelper->GetGts(1404));
                CloseGossipMenuFor(player);
                return true;
            }

            sItemFuncMgr->DoItemRecovery(player, Action);
            player->SendSysMsg(sHelper->GetGts(1405));
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

class ItemBindCr : public ItemScript
{
public:
    ItemBindCr() : ItemScript("ItemBindCr") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        uint32 TargetItemId = TargetItem->GetEntry();
        uint32 ItemGuid = TargetItem->GetGUIDLow();

        auto Itr = ItemClearBindStore.find(TargetItemId);
        if (Itr == ItemClearBindStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(1601));
            return false;
        }

        if (!TargetItem->IsSoulBound())
        {
            player->SendSysMsg(sHelper->GetGts(1603));
            return false;
        }

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        if (TargetItem->UnBinded)
        {
            player->SendSysMsg(sHelper->GetGts(1604));
            return false;
        }

        auto It = AtRunesTempStore.find(TargetItemId);
        if (!sHelper->GetBoolean(9101) && It != AtRunesTempStore.end() && TargetItem->NowCount > 0)
        {
            player->SendSysMsg(sHelper->GetGts(7450));
            return false;
        }

        if (!Itr->second)
        {
            TargetItem->ClearEnchantment(PERM_ENCHANTMENT_SLOT);
            TargetItem->UnBinded = true;
            TargetItem->SetBinding(false);
            TargetItem->UpdateTempKt(KT02, 1);
            player->SendSysMsg(sHelper->GetGts(1602));
            return false;
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "", 0, ItemGuid, sHelper->GetGts(6655) + sHelper->GetNeedGts(Itr->second, TargetItem->GetCount()), 0, false);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, CastItem->GetGUID());

        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, Action)))
        {
            uint32 TargetItemId = TargetItem->GetEntry();
            auto Itr = ItemClearBindStore.find(TargetItemId);
            if (Itr == ItemClearBindStore.end())
            {
                CloseGossipMenuFor(player);
                return;
            }

            auto It = AtRunesTempStore.find(TargetItemId);
            if (!sHelper->GetBoolean(9101) && It != AtRunesTempStore.end() && TargetItem->NowCount > 0)
            {
                CloseGossipMenuFor(player);
                return;
            }

            if (sHelper->CheckNeed(player, Itr->second, "物品解绑消耗", TargetItem->GetCount()))
            {
                TargetItem->ClearEnchantment(PERM_ENCHANTMENT_SLOT);
                TargetItem->UnBinded = true;
                TargetItem->SetBinding(false);
                TargetItem->UpdateTempKt(KT02, 1);
                player->SendSysMsg(sHelper->GetGts(1602));
            }
        }

        CloseGossipMenuFor(player);
    }
};

class NpcHRTitle : public CreatureScript
{
public:
    NpcHRTitle() : CreatureScript("NpcHRTitle") { }

    void ShowGossipHello(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (player->GetTeamId() == TEAM_ALLIANCE)
        {
            for (auto Itr = HRStore.begin(); Itr != HRStore.end(); ++Itr)
            {
                if (Itr->first < 15)
                {
                    std::string NGts = sHelper->GetNeedGts(Itr->second.NTempId);
                    std::string RGts = sHelper->GetRewardGts(Itr->second.RTempId);
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Itr->second.CTitle, 0, Itr->first, Acore::StringFormat(sHelper->GetGts(6656), NGts) + RGts, 0, false);
                }
            }
        }
        else
        {
            for (auto Itr = HRStore.begin(); Itr != HRStore.end(); ++Itr)
            {
                if (Itr->first > 14)
                {
                    std::string NGts = sHelper->GetNeedGts(Itr->second.NTempId);
                    std::string RGts = sHelper->GetRewardGts(Itr->second.RTempId);
                    AddGossipItemFor(player, GOSSIP_ICON_TALK, Itr->second.CTitle, 0, Itr->first, Acore::StringFormat(sHelper->GetGts(6656), NGts) + RGts, 0, false);
                }
            }
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ShowGossipHello(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        if (!Sender && !Action)
        {
            ShowGossipHello(player, creature);
            return true;
        }

        if (Action)
        {
            uint32 NextTiteId = sCustomClass->GetHRTiteId(player) + 1;
            if (NextTiteId != Action)
            {
                player->SendSysMsg(sHelper->GetGts(1803));
                ShowGossipHello(player, creature);
                return true;
            }

            CharTitlesEntry const* TInfo = sCharTitlesStore.LookupEntry(Action);
            if (!TInfo)
            {
                ShowGossipHello(player, creature);
                return true;
            }

            uint32 NTempId = sCustomClass->GetHRNTempId(Action);
            if (sHelper->CheckNeed(player, NTempId, "提升军衔消耗"))
            {
                uint32 RTempId = sCustomClass->GetHRRTempId(Action);
                sHelper->DoTheReward(player, RTempId, "提升军衔奖励");
                player->SetTitle(TInfo);
                ShowGossipHello(player, creature);
                return true;
            }
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

class ItemUseTaskGlyph : public ItemScript
{
public:
    ItemUseTaskGlyph() : ItemScript("ItemUseTaskGlyph") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (!CastItem)
            return true;

        uint32 FuncItemId = CastItem->GetEntry();
        bool On = false;
        for (uint32 Slot = 0; Slot < MAX_GLYPH_SLOT_INDEX; ++Slot)
        {
            if (uint32 Glyph = player->GetGlyph(Slot))
            {
                if (GlyphPropertiesEntry const* GpEntry = sGlyphPropertiesStore.LookupEntry(Glyph))
                {
                    On = true;
                    player->RemoveAurasDueToSpell(GpEntry->SpellId);
                    player->SetGlyph(Slot, 0, true);
                    player->SendTalentsInfoData(false);

                    if (uint32 ItemId = sHelper->GetItemIdByGlyph(GpEntry->Id))
                        player->AddItem(ItemId, 1, "雕文拆除获得");
                }
            }
        }

        if (!On)
            return false;

        player->SendSysMsg(sHelper->GetGts(1804));
        auto Iter = ItemScriptOnDeleteStore.find(FuncItemId);
        if (Iter != ItemScriptOnDeleteStore.end())
        {
            if (Iter->second > 0)
            {
                player->DestroyItemCount(FuncItemId, 1, true);
                return true;
            }
        }

        return false;
    }
};

class DoUseStatRankXp : public ItemScript
{
public:
    DoUseStatRankXp() : ItemScript("DoUseStatRankXp") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (!CastItem)
            return true;

        uint32 FuncItemId = CastItem->GetEntry();
        auto Itr = StatPtRewStore.find(FuncItemId);
        if (Itr == StatPtRewStore.end())
            return true;

        bool On1 = false;
        uint32 TarRank = Itr->second.ToRank;
        uint32 Max = sCustomClass->GetStatMaxRank();
        uint32 NowRank = AsInt64ToUInt32Clamp(player->TempAtStore[AT11].Value);
        if (TarRank)
        {
            if (TarRank > NowRank && NowRank < Max)
            {
                TarRank = (TarRank > Max) ? Max : TarRank;
                player->TempAtStore[AT11] += (TarRank - NowRank);
                player->TempAtStore[AT13] += sCustomClass->GetStatOldNewRewCount(NowRank, AsInt64ToUInt32Clamp(player->TempAtStore[AT11].Value));

                player->CastSpell(player, TxLevelSpell, false);
                sCustomClass->SendUpdatePlayerStatData(player);
                player->SendSysMsg(sHelper->GetGts(2001), TarRank);
                On1 = true;
            }
        }

        bool On2 = false;
        AsInt64 Xp = Itr->second.RewXp;
        AsInt64 NXp = sAuraSpellMgr->GetStatNowHook(player, Xp);
        if (NXp)
        {
            AsInt64 NextXp = sCustomClass->GetStatNeedXp(AsInt64ToUInt32Clamp(player->TempAtStore[AT11].Value));
            if (!NextXp)
            {
                player->SendSysMsg(sHelper->GetGts(2003));
            }
            else
            {
                AsInt64 NewXp = player->TempAtStore[AT12].Value + NXp;
                while (NewXp >= NextXp && NextXp)
                {
                    NewXp -= NextXp;

                    if (NextXp)
                    {
                        player->TempAtStore[AT11] += 1;
                        uint32 newRank = AsInt64ToUInt32Clamp(player->TempAtStore[AT11].Value);
                        player->TempAtStore[AT13] += sCustomClass->GetStatRewCount(newRank);
                        player->CastSpell(player, TxLevelSpell, false);
                        player->SendSysMsg(sHelper->GetGts(2001), newRank);
                    }

                    NextXp = sCustomClass->GetStatNeedXp(AsInt64ToUInt32Clamp(player->TempAtStore[AT11].Value));
                }

                player->TempAtStore[AT12] = NewXp;
                sCustomClass->SendUpdatePlayerStatData(player);
                player->SendSysMsg("[温馨提示]: 获得斗气经验 %s", AsInt64ToString(NXp).c_str());
                On2 = true;
            }
        }

        bool On3 = false;
        bool AddRank = Itr->second.AddRank;
        if (AddRank)
        {
            uint32 ARank = NowRank + 1;
            if (NowRank < Max)
            {
                ARank = (ARank > Max) ? Max : ARank;
                player->TempAtStore[AT11] = ARank;
                player->TempAtStore[AT13] += sCustomClass->GetStatRewCount(ARank);
                player->CastSpell(player, TxLevelSpell, false);
                sCustomClass->SendUpdatePlayerStatData(player);
                player->SendSysMsg(sHelper->GetGts(2001), ARank);
                On3 = true;
            }
        }

        if (On1 || On2 || On3)
        {
            auto Iter = ItemScriptOnDeleteStore.find(FuncItemId);
            if (Iter != ItemScriptOnDeleteStore.end())
            {
                if (Iter->second > 0)
                {
                    player->DestroyItemCount(FuncItemId, 1, true);
                    return true;
                }
            }
        }

        return false;
    }
};

class ItemChoiceUp : public ItemScript
{
public:
    ItemChoiceUp() : ItemScript("ItemChoiceUp") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        if (!sHelper->GetBoolean(1404))
            return false;

        uint32 TarItemId = TargetItem->GetEntry();
        uint32 TarGuidLow = TargetItem->GetGUIDLow();

        auto Itr = ItemUpChoiceTempStore.find(TarItemId);
        if (Itr == ItemUpChoiceTempStore.end())
        {
            sDoAddon->SendPacketToClient(player, "NotUpChoice");
            return false;
        }

        sDoAddon->SendPacketToClient(player, "AsItemChoiceUp", std::to_string(TarGuidLow) + "#" + std::to_string(TarItemId) + "#");
        return false;
    }
};

class ItemRandomUp : public ItemScript
{
public:
    ItemRandomUp() : ItemScript("ItemRandomUp") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        if (!sHelper->GetBoolean(1405))
            return false;

        uint32 TarItemId = TargetItem->GetEntry();
        uint32 TarGuidLow = TargetItem->GetGUIDLow();

        auto Itr = ItemUpRandomStore.find(TarItemId);
        if (Itr == ItemUpRandomStore.end())
        {
            sDoAddon->SendPacketToClient(player, "NotUpRandom");
            return false;
        }

        sDoAddon->SendPacketToClient(player, "AsItemRandomUp", std::to_string(TarGuidLow) + "#" + std::to_string(TarItemId) + "#");
        return false;
    }
};

class ItemResetEnch : public ItemScript
{
public:
    ItemResetEnch() : ItemScript("ItemResetEnch") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return true;

        uint32 FuncItemId = CastItem->GetEntry();
        uint32 TarItemId = TargetItem->GetEntry();
        uint32 TarGuidLow = TargetItem->GetGUIDLow();

        auto Itr = ItemEnchStore.find(TarItemId);
        if (Itr == ItemEnchStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(1303));
            return false;
        }

        if (!sHelper->GetBoolean(3901) && int32(TargetItem->ItemRtsCount) >= Itr->second.Rts)
        {
            player->SendSysMsg(sHelper->GetGts(1301));
            return false;
        }

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        bool Zt = false;
        if (!player->GetEnchLock(TarGuidLow, PROP_ENCHANTMENT_SLOT_0))
        {
            if (roll_chance_f(Itr->second.EnchSlot1))
                sItemFuncMgr->DoRandomEnch(TargetItem, Itr->second.EnchGroup1, PROP_ENCHANTMENT_SLOT_0);
            else
                TargetItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_0);

            Zt = true;
        }

        if (!player->GetEnchLock(TarGuidLow, PROP_ENCHANTMENT_SLOT_1))
        {
            if (roll_chance_f(Itr->second.EnchSlot2))
                sItemFuncMgr->DoRandomEnch(TargetItem, Itr->second.EnchGroup2, PROP_ENCHANTMENT_SLOT_1);
            else
                TargetItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_1);

            Zt = true;
        }

        if (!player->GetEnchLock(TarGuidLow, PROP_ENCHANTMENT_SLOT_2))
        {
            if (roll_chance_f(Itr->second.EnchSlot3))
                sItemFuncMgr->DoRandomEnch(TargetItem, Itr->second.EnchGroup3, PROP_ENCHANTMENT_SLOT_2);
            else
                TargetItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_2);

            Zt = true;
        }

        if (!player->GetEnchLock(TarGuidLow, PROP_ENCHANTMENT_SLOT_3))
        {
            if (roll_chance_f(Itr->second.EnchSlot4))
                sItemFuncMgr->DoRandomEnch(TargetItem, Itr->second.EnchGroup4, PROP_ENCHANTMENT_SLOT_3);
            else
                TargetItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_3);

            Zt = true;
        }

        if (!player->GetEnchLock(TarGuidLow, PROP_ENCHANTMENT_SLOT_4))
        {
            if (roll_chance_f(Itr->second.EnchSlot5))
                sItemFuncMgr->DoRandomEnch(TargetItem, Itr->second.EnchGroup5, PROP_ENCHANTMENT_SLOT_4);
            else
                TargetItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_4);

            Zt = true;
        }

        if (!Zt)
        {
            player->SendSysMsg(sHelper->GetGts(3002));
            return false;
        }

        EnchantTemp Temp;
        Temp.EnchantId1 = TargetItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0));
        Temp.EnchantId2 = TargetItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_1));
        Temp.EnchantId3 = TargetItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_2));
        Temp.EnchantId4 = TargetItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_3));
        Temp.EnchantId5 = TargetItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_4));
        EnchantAtStore[TargetItem->GetGUIDLow()] = Temp;

        std::string SendMsg;
        SendMsg.clear();
        for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
        {
            uint32 EnchantId = TargetItem->GetEnchantmentId(EnchantmentSlot(ESlot));
            SendMsg.append(std::to_string(EnchantId) + "#");
        }

        if (!SendMsg.empty())
        {
            sDoAddon->SendPacketToClient(player, "AutoEnchant", std::to_string(TargetItem->GetGUIDLow()) + "$" + SendMsg);
            SendMsg.clear();
        }

        if (sHelper->GetBoolean(2202))
        {
            for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
                player->UpdateEnchLock(TarGuidLow, ESlot, false);
        }

        if (Itr->second.Rts > 0)
            TargetItem->ItemRtsCount++;

        sItemFuncMgr->UpdateEnchRtsDB(player, TargetItem);
        player->SendSysMsg(sHelper->GetGts(3001));

        auto Iter = ItemScriptOnDeleteStore.find(FuncItemId);
        if (Iter != ItemScriptOnDeleteStore.end())
        {
            if (Iter->second > 0)
            {
                player->DestroyItemCount(FuncItemId, 1, true);
                return true;
            }
        }

        return false;
    }
};

class ItemLockEnch : public ItemScript
{
public:
    ItemLockEnch() : ItemScript("ItemLockEnch") {}

    void OnGossipEnchLock(Player* player, Item* OnItem, uint32 ItemGuid)
    {
        ClearGossipMenuFor(player);
        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid)))
        {
            AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetGts(6657), 0, ItemGuid);

            for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
            {
                uint32 EnchId = TargetItem->GetEnchantmentId(EnchantmentSlot(ESlot));
                if (!EnchId)
                    continue;

                bool Lock = player->GetEnchLock(ItemGuid, ESlot);
                std::string TInfo = Lock ? sHelper->GetGts(6658) : sHelper->GetGts(6659);
                std::string SInfo = Lock ? sHelper->GetGts(6660) : sHelper->GetGts(6661);
                std::string NeedGts = Lock ? SInfo + sHelper->GetNeedGts(sHelper->GetUInt(2062)) : SInfo + sHelper->GetNeedGts(sHelper->GetUInt(2061));
                AddGossipItemFor(player, GOSSIP_ICON_TALK, sHelper->GetEnchName(EnchId) + TInfo, ESlot, ItemGuid, NeedGts, 0, false);
            }

            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, OnItem->GetGUID());
        }
    }

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        uint32 FuncItemId = CastItem->GetEntry();
        uint32 TargetItemId = TargetItem->GetEntry();
        uint32 ItemGuid = TargetItem->GetGUIDLow();

        auto Itr = ItemEnchStore.find(TargetItemId);
        if (Itr == ItemEnchStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(3000));
            return false;
        }

        uint32 TEnch1 = TargetItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_0);
        uint32 TEnch2 = TargetItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_1);
        uint32 TEnch3 = TargetItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_2);
        uint32 TEnch4 = TargetItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_3);
        uint32 TEnch5 = TargetItem->GetEnchantmentId(PROP_ENCHANTMENT_SLOT_4);

        bool NotTEnch = (TEnch1 == 0
            && TEnch2 == 0
            && TEnch3 == 0
            && TEnch4 == 0
            && TEnch5 == 0
            );

        if (NotTEnch)
        {
            player->SendSysMsg(sHelper->GetGts(1150));
            return false;
        }

        OnGossipEnchLock(player, CastItem, ItemGuid);
        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);
        if (Sender == 0)
        {
            OnGossipEnchLock(player, OnItem, Action);
            return;
        }

        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, Action)))
        {
            uint32 ItemGuid = TargetItem->GetGUIDLow();
            bool Lock = player->GetEnchLock(ItemGuid, Sender);
            uint32 NTempId = Lock ? sHelper->GetUInt(2062) : sHelper->GetUInt(2061);
            if (sHelper->CheckNeed(player, NTempId, "随机附魔锁定消耗"))
            {
                player->UpdateEnchLock(ItemGuid, Sender, !Lock);
                player->SendSysMsg(sHelper->GetGts(Lock ? 3004 : 3003));
            }
        }

        OnGossipEnchLock(player, OnItem, Action);
    }
};

class UseResetAtItem : public ItemScript
{
public:
    UseResetAtItem() : ItemScript("UseResetAtItem") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return true;

        uint32 OnItemId = CastItem->GetEntry();
        uint32 TarItemId = TargetItem->GetEntry();
        if (!sItemFuncMgr->IsInAtEnchItem(OnItemId, TarItemId))
        {
            player->SendSysMsg(sHelper->GetGts(3301));
            return false;
        }

        auto Itr = ItemEnchStore.find(TarItemId);
        if (Itr == ItemEnchStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(1303));
            return false;
        }

        if (!sHelper->GetBoolean(3901) && int32(TargetItem->ItemRtsCount) >= Itr->second.Rts)
        {
            player->SendSysMsg(sHelper->GetGts(1301));
            return false;
        }

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        if (!sItemFuncMgr->DoAtEnchItem(player, TargetItem, OnItemId))
        {
            player->SendSysMsg(sHelper->GetGts(3302));
            return false;
        }

        if (Itr->second.Rts > 0)
            TargetItem->ItemRtsCount++;

        sItemFuncMgr->UpdateEnchRtsDB(player, TargetItem);
        player->SendSysMsg(sHelper->GetGts(3303));

        auto Iter = ItemScriptOnDeleteStore.find(OnItemId);
        if (Iter != ItemScriptOnDeleteStore.end())
        {
            if (Iter->second > 0)
            {
                player->DestroyItemCount(OnItemId, 1, true);
                return true;
            }
        }

        return false;
    }
};

class UseSumGroupOrRaid : public ItemScript
{
public:
    UseSumGroupOrRaid() : ItemScript("UseSumGroupOrRaid") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (!CastItem)
            return true;

        uint32 FuncItemId = CastItem->GetEntry();
        if (player->InBattleground())
        {
            player->SendSysMsg(sHelper->GetGts(6662));
            return false;
        }

        Group* Gp = player->GetGroup();
        if (!Gp)
        {
            player->SendSysMsg(sHelper->GetGts(6663));
            return false;
        }

        for (GroupReference* Itr = Gp->GetFirstMember(); Itr != nullptr; Itr = Itr->next())
        {
            if (Player* Member = Itr->GetSource())
            {
                CloseGossipMenuFor(Member);
                if (player->GetGUIDLow() != Member->GetGUIDLow())
                    Member->DoSummonRequestFrom(player);
            }
        }

        auto Iter = ItemScriptOnDeleteStore.find(FuncItemId);
        if (Iter != ItemScriptOnDeleteStore.end())
        {
            if (Iter->second > 0)
            {
                player->DestroyItemCount(FuncItemId, 1, true);
                return true;
            }
        }

        player->SendSysMsg(sHelper->GetGts(6664));
        return false;
    }
};

class ItemTargetRunes : public ItemScript
{
public:
    ItemTargetRunes() : ItemScript("ItemTargetRunes") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        uint32 FuncItemId = CastItem->GetEntry();
        uint32 TargetItemId = TargetItem->GetEntry();

        auto Itr = AtRunesTempStore.find(TargetItemId);
        if (Itr == AtRunesTempStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(3801));
            return false;
        }

        auto It = ItemRunesTempStore.find(FuncItemId);
        if (It == ItemRunesTempStore.end())
            return false;;

        if (!TargetItem->RunesCount)
            return false;

        uint8 NowNum = sHelper->GetNowNum(TargetItem);
        if (NowNum > (MAX_RUNES_COUNT - 1))
            return false;

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        if (TargetItem->GetCount() > 1)
        {
            player->SendSysMsg(sHelper->GetGts(1060));
            return false;
        }

        if (sHelper->GetBoolean(16030) && sItemRunesMgr->HasPlayerRunesItemIdAlls(player, FuncItemId))
        {
            player->SendSysMsg(sHelper->GetGts(3911));
            return false;
        }

        uint32 ItemGuid = TargetItem->GetGUIDLow();
        ClearGossipMenuFor(player);
        std::string Gts = sHelper->GetNeedGts(It->second.NTempId1);

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "", NowNum, ItemGuid, Acore::StringFormat(sHelper->GetGts(6665), sHelper->GetItemLink(FuncItemId)) + Gts, 0, false);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, CastItem->GetGUID());
        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        if (Sender > (MAX_RUNES_COUNT - 1))
        {
            CloseGossipMenuFor(player);
            return;
        }

        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, Action)))
        {
            uint32 UseItemId = OnItem->GetEntry();
            uint32 TargetItemId = TargetItem->GetEntry();

            if (sHelper->GetBoolean(16030) && sItemRunesMgr->HasPlayerRunesItemIdAlls(player, UseItemId))
            {
                player->SendSysMsg(sHelper->GetGts(3911));
                return;
            }

            if (sItemRunesMgr->DoAddRunes(TargetItem, UseItemId, Sender, player))
            {
                if (sHelper->GetBoolean(9100) && !TargetItem->IsSoulBound())
                {
                    TargetItem->ClearEnchantment(PERM_ENCHANTMENT_SLOT);
                    TargetItem->UnBinded = false;
                    TargetItem->SetBinding(true);
                }

                player->SendSysMsg(sHelper->GetGts(3802));
                player->DestroyItemCount(UseItemId, 1, true);
            }
            else
            {
                player->SendSysMsg(sHelper->GetGts(3803));
            }
        }

        CloseGossipMenuFor(player);
    }
};

class ItemTaskRunes : public ItemScript
{
public:
    ItemTaskRunes() : ItemScript("ItemTaskRunes") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        uint32 FuncItemId = CastItem->GetEntry();
        uint32 TargetItemId = TargetItem->GetEntry();

        auto Itr = AtRunesTempStore.find(TargetItemId);
        if (Itr == AtRunesTempStore.end())
            return false;

        if (!TargetItem->NowCount)
            return false;

        if (!TargetItem->RunesCount)
            return false;

        bool In = false;
        for (uint8 N = 0; N < TargetItem->RunesCount; ++N)
            if (TargetItem->ResItems[N] != 0)
                In = true;

        if (!In)
            return false;

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        if (TargetItem->GetCount() > 1)
        {
            player->SendSysMsg(sHelper->GetGts(1060));
            return false;
        }

        uint32 ItemGuid = TargetItem->GetGUIDLow();
        ClearGossipMenuFor(player);

        uint8 LastNum = sHelper->GetLastNum(TargetItem);
        if (LastNum > (MAX_RUNES_COUNT - 1))
            return false;

        uint32 DelNtempId = sItemRunesMgr->GetDelNtempId(TargetItem->ResItems[LastNum]);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "", LastNum, ItemGuid, Acore::StringFormat(sHelper->GetGts(6666), sHelper->GetItemLink(TargetItem->ResItems[LastNum]), sHelper->GetNeedGts(DelNtempId)), 0, false);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, CastItem->GetGUID());
        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        if (Sender > (MAX_RUNES_COUNT - 1))
        {
            CloseGossipMenuFor(player);
            return;
        }

        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, Action)))
        {
            uint32 BackItem = TargetItem->ResItems[Sender];
            if (!BackItem)
            {
                CloseGossipMenuFor(player);
                return;
            }

            uint32 TargetItemId = TargetItem->GetEntry();
            if (sItemRunesMgr->DoDeleteRunes(TargetItem, BackItem, Sender, player))
            {
                player->SendSysMsg(sHelper->GetGts(3804));
                Item* ItemOld = player->AddItemByIdOne(BackItem, "符文拆除获得");
                if (ItemOld && sItemRunesMgr->IsTaskBind(BackItem))
                {
                    ItemOld->UnBinded = false;
                    ItemOld->SetBinding(true);
                    ItemOld->UpdateTempKt(KT03, 1);
                }
            }
            else
            {
                player->SendSysMsg(sHelper->GetGts(3805));
            }
        }

        CloseGossipMenuFor(player);
    }
};

class AsItemTaskRunes : public ItemScript
{
public:
    AsItemTaskRunes() : ItemScript("AsItemTaskRunes") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return true;

        uint32 TargetItemId = TargetItem->GetEntry();

        auto Itr = AtRunesTempStore.find(TargetItemId);
        if (Itr == AtRunesTempStore.end())
            return true;;

        if (!TargetItem->NowCount)
            return true;

        if (!TargetItem->RunesCount)
            return true;

        bool In = false;
        for (uint8 N = 0; N < TargetItem->RunesCount; ++N)
            if (TargetItem->ResItems[N] != 0)
                In = true;

        if (!In)
            return true;

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return true;
        }

        if (TargetItem->GetCount() > 1)
        {
            player->SendSysMsg(sHelper->GetGts(1060));
            return true;
        }

        uint32 ItemGuid = TargetItem->GetGUIDLow();
        ClearGossipMenuFor(player);

        if (sItemRunesMgr->DoDeleteAsRunes(TargetItem, player))
        {
            player->SendSysMsg(sHelper->GetGts(3804));
            return false;
        }

        player->SendSysMsg(sHelper->GetGts(3805));
        return true;
    }
};

class ItemAddSlotRunes : public ItemScript
{
public:
    ItemAddSlotRunes() : ItemScript("ItemAddSlotRunes") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        Item* TargetItem = Targets.GetItemTarget();
        if (!CastItem || !TargetItem)
            return false;

        uint32 FuncItemId = CastItem->GetEntry();
        uint32 TargetItemId = TargetItem->GetEntry();

        if (TargetItem->RunesCount >= MAX_RUNES_COUNT)
        {
            player->SendSysMsg(sHelper->GetGts(3903));
            return false;
        }

        auto Itr = ItemASlotRunesTempStore.find(TargetItemId);
        if (Itr == ItemASlotRunesTempStore.end())
            return false;

        if (TargetItem->IsEquipped())
        {
            player->SendSysMsg(sHelper->GetGts(1085));
            return false;
        }

        if (TargetItem->GetCount() > 1)
        {
            player->SendSysMsg(sHelper->GetGts(1060));
            return false;
        }

        uint32 ItemGuid = TargetItem->GetGUIDLow();
        ClearGossipMenuFor(player);

        std::string Gts = sHelper->GetNeedGts(Itr->second.NTempId);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "", 0, ItemGuid, Acore::StringFormat(sHelper->GetGts(6667), sHelper->ToExString(Itr->second.Chance)) + Gts, 0, false);

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, CastItem->GetGUID());
        return false;
    }

    void OnGossipSelect(Player* player, Item* OnItem, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        if (Item* TargetItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, Action)))
        {
            uint32 TargetItemId = TargetItem->GetEntry();
            uint32 UseItemId = OnItem->GetEntry();

            auto Itr = ItemASlotRunesTempStore.find(TargetItemId);
            if (Itr == ItemASlotRunesTempStore.end())
            {
                CloseGossipMenuFor(player);
                return;
            }

            if (sItemRunesMgr->DoAddSlotRunes(TargetItem, player))
            {
                player->SendSysMsg(sHelper->GetGts(3901));
                player->DestroyItemCount(UseItemId, 1, true);
            }
        }

        CloseGossipMenuFor(player);
    }
};

class NpcRandomUp : public CreatureScript
{
public:
    NpcRandomUp() : CreatureScript("NpcRandomUp") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (!sHelper->GetBoolean(1405))
            return true;

        sDoAddon->SendPacketToClient(player, "OpenRandomUpUI");
        CloseGossipMenuFor(player);
        return true;
    }
};

class DoResetChallenge : public ItemScript
{
public:
    DoResetChallenge() : ItemScript("DoResetChallenge") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (!CastItem)
            return true;

        uint32 FuncItemId = CastItem->GetEntry();
        auto Iter = MazChallengeItemStore.find(FuncItemId);
        if (Iter == MazChallengeItemStore.end())
            return false;

        uint32 TarReMap = Iter->second;
        uint32 GuidLow = player->GetGUIDLow();
        for (auto Itr = ChallengeDataStore.begin(); Itr != ChallengeDataStore.end(); Itr++)
            if (Itr->MapId == TarReMap && Itr->GuidLow == GuidLow)
                Itr->Bind = false;

        player->CustomUnbinds(TarReMap);
        player->SendSysMsg(sHelper->GetGts(4201), sHelper->GetMapName(TarReMap));

        auto Itar = ItemScriptOnDeleteStore.find(FuncItemId);
        if (Itar != ItemScriptOnDeleteStore.end())
        {
            if (Itar->second > 0)
            {
                player->DestroyItemCount(FuncItemId, 1, true);
                return true;
            }
        }
        return false;
    }
};

class ItemUseDelBuff : public ItemScript
{
public:
    ItemUseDelBuff() : ItemScript("ItemUseDelBuff") {}

    bool OnUse(Player* player, Item* CastItem, SpellCastTargets const& Targets) override
    {
        ClearGossipMenuFor(player);

        if (!CastItem)
            return true;

        uint32 FuncItemId = CastItem->GetEntry();
        Unit* Cre = player->GetSelectedUnit();
        if (Cre && Cre->ToCreature())
        {
            if (Cre->ToCreature()->HasAura(92000))
            {
                Cre->ToCreature()->RemoveAurasDueToSpell(92000);
                player->DestroyItemCount(FuncItemId, 1, true);
                return true;
            }
        }

        return false;
    }
};

class BeastMaster_CreatureScript : public CreatureScript
{
public:
    BeastMaster_CreatureScript() : CreatureScript("NpcBeastMaster") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        sNpcBeastMaster->ShowMainMenu(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        sNpcBeastMaster->GossipSelect(player, creature, action);
        return true;
    }
};

class BeastMaster_PlayerScript : public PlayerScript
{
public:
    BeastMaster_PlayerScript() : PlayerScript("BeastMaster_PlayerScript") { }

    void OnBeforeUpdate(Player* player, uint32 p_time) override
    {
        sNpcBeastMaster->PlayerUpdate(player, p_time);
    }

    void OnBeforeLoadPetFromDB(Player* /*player*/, uint32& /*petentry*/, uint32& /*petnumber*/, bool& /*current*/, bool& forceLoadFromDB) override
    {
        forceLoadFromDB = true;
    }

    void OnBeforeGuardianInitStatsForLevel(Player* /*player*/, Guardian* /*guardian*/, CreatureTemplate const* cinfo, PetType& petType) override
    {
        if (cinfo->IsTameable(true))
        {
            petType = HUNTER_PET;
        }
    }
};

void AddSC_AsItemMgrScripts()
{
    new ItemFunction();
    new NpcFunction();
    new GameObjectFunction();
    new ItemTargetEnch();
    new ItemUseVipRankOne();
    new ItemUseVipRank();

    new NpcCdKey();
    new ItemUseAoeLoot();

    new NpcSoulItem();
    new NpcNewCharInfo();

    new ItemUseTalent();
    new ItemUseProfessions();
    new ItemTaskSocket();
    new DoItemRecovery();
    new NpcItemRecovery();
    new ItemBindCr();
    new NpcHRTitle();
    new ItemUseTaskGlyph();

    new DoUseStatRankXp();

    new ItemChoiceUp();
    new ItemRandomUp();

    new ItemResetEnch();
    new ItemLockEnch();

    new UseResetAtItem();

    new UseSumGroupOrRaid();

    new ItemTargetUpGuid();
    new ItemTargetUpGuidLv();
    new ItemTargetUpGuidReset();

    new ItemTargetRunes();
    new ItemTaskRunes();
    new AsItemTaskRunes();
    new ItemAddSlotRunes();

    new NpcRandomUp();

    new DoResetChallenge();
    new ItemUseDelBuff();

    new BeastMaster_CreatureScript();
    new BeastMaster_PlayerScript();


}
