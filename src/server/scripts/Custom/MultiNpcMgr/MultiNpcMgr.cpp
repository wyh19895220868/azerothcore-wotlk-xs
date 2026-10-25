#include "MoneyMath.h"
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
#include "MultiNpcMgr.h"
#include <Custom/EventMgr/EventMgr.h>
#include <Custom/ItemFuncMgr/ItemFuncMgr.h>
#include <Custom/CustomVendorMgr/CustomVendorMgr.h>
#pragma execution_character_set("utf-8")

VendorStructMap VendorStructStore;

void MultiNpcMgr::LoadMultiVendorStore()
{
    uint32 Ct = 0;
    VendorStructStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 生物编号, 商人编号, 排序, 显示的菜单图标, 阵营, 职业掩码, 显示需求模版 FROM _综合商人表;");
    if (Result)
    {
        std::multimap<uint32, uint32> MaxMdlist;
        MaxMdlist.clear();

        do
        {
            Field* Fields = Result->Fetch();
            VendorStruct St;
            St.NpcEntry = Fields[0].Get<uint32>();
            St.VTEntry = Fields[1].Get<uint32>();
            St.Num = Fields[2].Get<uint32>();
            St.Gossip = Fields[3].Get<std::string>();
            St.Tean = Fields[4].Get<uint32>();
            St.Class = Fields[5].Get<uint32>();
            St.ShowNeedTempId = Fields[6].Get<uint32>();

            MaxMdlist.insert({ St.NpcEntry, St.NpcEntry });

            if (MaxMdlist.count(St.NpcEntry) >= 30)
            {
                continue;
            }

            VendorStructStore.insert(std::make_pair(St.NpcEntry, St));
            ++Ct;

        } while (Result->NextRow());
    }
}

class MultiTrainerNpc : public CreatureScript
{
public:
    MultiTrainerNpc() : CreatureScript("MultiTrainerNpc") { }

    void SendShowSkill(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Mining.blp:24:24:-20:0|t 采矿训练", 101, 2001);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Herbalism.blp:24:24:-20:0|t 草药训练", 101, 2002);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Alchemy.blp:24:24:-20:0|t 炼金训练", 101, 2003);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Engraving.blp:24:24:-20:0|t 附魔训练", 101, 2004);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_BlackSmithing.blp:24:24:-20:0|t 锻造训练", 101, 2005);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Tailoring.blp:24:24:-20:0|t 裁缝训练", 101, 2006);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Engineering.blp:24:24:-20:0|t 工程训练", 101, 2007);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Misc_Gem_01.blp:24:24:-20:0|t 珠宝训练", 101, 2008);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_LeatherWorking.blp:24:24:-20:0|t 制皮训练", 101, 2009);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Misc_Pelt_Wolf_01.blp:24:24:-20:0|t 剥皮训练", 101, 2010);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Inscription_Tradeskill01.blp:24:24:-20:0|t 铭文训练", 101, 2011);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Misc_Food_15.blp:24:24:-20:0|t 烹饪训练", 101, 2012);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Fishing.blp:24:24:-20:0|t 钓鱼训练", 101, 2013);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Spell_Holy_SealOfSacrifice.blp:24:24:-20:0|t 急救训练", 101, 2014);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/ICONS/achievement_pvp_o_12.blp:30:30:-20:0|t →|cff0000ff返回首页|r", 0, 0);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    const char* GetProStr(uint32 ProId)
    {
        switch (ProId)
        {
        case SKILL_MINING:
            return "采矿";
        case SKILL_HERBALISM:
            return "草药";
        case SKILL_ALCHEMY:
            return "炼金";
        case SKILL_ENCHANTING:
            return "附魔";
        case SKILL_BLACKSMITHING:
            return "锻造";
        case SKILL_TAILORING:
            return "裁缝";
        case SKILL_ENGINEERING:
            return "工程";
        case SKILL_JEWELCRAFTING:
            return "珠宝";
        case SKILL_LEATHERWORKING:
            return "制皮";
        case SKILL_SKINNING:
            return "剥皮";
        case SKILL_INSCRIPTION:
            return "铭文";
        case SKILL_COOKING:
            return "烹饪";
        case SKILL_FISHING:
            return "钓鱼";
        case SKILL_FIRST_AID:
            return "急救";
        default:
            return "未知";
        }
    }

    void SendShowMaxPro(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/ICONS/achievement_pvp_o_12.blp:30:30:-20:0|t →|cff0000ff 只显示已经学会的或未满级的|r", 9999, 9999);

        if (player->HasSkill(SKILL_MINING) && player->GetSkillValue(SKILL_MINING) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9001);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Mining.blp:24:24:-20:0|t 秒升满级 [采矿]", SKILL_MINING, 9001, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Mining.blp:24:24:-20:0|t 秒升满级 [采矿]", SKILL_MINING, 9001);
        }
        if (player->HasSkill(SKILL_HERBALISM) && player->GetSkillValue(SKILL_HERBALISM) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9002);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Herbalism.blp:24:24:-20:0|t 秒升满级 [草药]", SKILL_HERBALISM, 9002, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Herbalism.blp:24:24:-20:0|t 秒升满级 [草药]", SKILL_HERBALISM, 9002);
        }
        if (player->HasSkill(SKILL_ALCHEMY) && player->GetSkillValue(SKILL_ALCHEMY) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9003);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Alchemy.blp:24:24:-20:0|t 秒升满级 [炼金]", SKILL_ALCHEMY, 9003, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Alchemy.blp:24:24:-20:0|t 秒升满级 [炼金]", SKILL_ALCHEMY, 9003);
        }
        if (player->HasSkill(SKILL_ENCHANTING) && player->GetSkillValue(SKILL_ENCHANTING) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9004);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Engraving.blp:24:24:-20:0|t 秒升满级 [附魔]", SKILL_ENCHANTING, 9004, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Engraving.blp:24:24:-20:0|t 秒升满级 [附魔]", SKILL_ENCHANTING, 9004);
        }
        if (player->HasSkill(SKILL_BLACKSMITHING) && player->GetSkillValue(SKILL_BLACKSMITHING) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9005);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_BlackSmithing.blp:24:24:-20:0|t 秒升满级 [锻造]", SKILL_BLACKSMITHING, 9005, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_BlackSmithing.blp:24:24:-20:0|t 秒升满级 [锻造]", SKILL_BLACKSMITHING, 9005);
        }
        if (player->HasSkill(SKILL_TAILORING) && player->GetSkillValue(SKILL_TAILORING) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9006);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Tailoring.blp:24:24:-20:0|t 秒升满级 [裁缝]", SKILL_TAILORING, 9006, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Tailoring.blp:24:24:-20:0|t 秒升满级 [裁缝]", SKILL_TAILORING, 9006);
        }
        if (player->HasSkill(SKILL_ENGINEERING) && player->GetSkillValue(SKILL_ENGINEERING) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9007);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Engineering.blp:24:24:-20:0|t 秒升满级 [工程]", SKILL_ENGINEERING, 9007, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Engineering.blp:24:24:-20:0|t 秒升满级 [工程]", SKILL_ENGINEERING, 9007);
        }
        if (player->HasSkill(SKILL_JEWELCRAFTING) && player->GetSkillValue(SKILL_JEWELCRAFTING) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9008);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Misc_Gem_01.blp:24:24:-20:0|t 秒升满级 [珠宝]", SKILL_JEWELCRAFTING, 9008, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Misc_Gem_01.blp:24:24:-20:0|t 秒升满级 [珠宝]", SKILL_JEWELCRAFTING, 9008);
        }
        if (player->HasSkill(SKILL_LEATHERWORKING) && player->GetSkillValue(SKILL_LEATHERWORKING) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9009);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_LeatherWorking.blp:24:24:-20:0|t 秒升满级 [制皮]", SKILL_LEATHERWORKING, 9009, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_LeatherWorking.blp:24:24:-20:0|t 秒升满级 [制皮]", SKILL_LEATHERWORKING, 9009);
        }
        if (player->HasSkill(SKILL_SKINNING) && player->GetSkillValue(SKILL_SKINNING) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9010);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Misc_Pelt_Wolf_01.blp:24:24:-20:0|t 秒升满级 [剥皮]", SKILL_SKINNING, 9010, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Misc_Pelt_Wolf_01.blp:24:24:-20:0|t 秒升满级 [剥皮]", SKILL_SKINNING, 9010);
        }
        if (player->HasSkill(SKILL_INSCRIPTION) && player->GetSkillValue(SKILL_INSCRIPTION) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9011);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Inscription_Tradeskill01.blp:24:24:-20:0|t 秒升满级 [铭文]", SKILL_INSCRIPTION, 9011, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Inscription_Tradeskill01.blp:24:24:-20:0|t 秒升满级 [铭文]", SKILL_INSCRIPTION, 9011);
        }
        if (player->HasSkill(SKILL_COOKING) && player->GetSkillValue(SKILL_COOKING) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9012);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Misc_Food_15.blp:24:24:-20:0|t 秒升满级 [烹饪]", SKILL_COOKING, 9012, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/INV_Misc_Food_15.blp:24:24:-20:0|t 秒升满级 [烹饪]", SKILL_COOKING, 9012);
        }
        if (player->HasSkill(SKILL_FISHING) && player->GetSkillValue(SKILL_FISHING) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9013);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Fishing.blp:24:24:-20:0|t 秒升满级 [钓鱼]", SKILL_FISHING, 9013, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Trade_Fishing.blp:24:24:-20:0|t 秒升满级 [钓鱼]", SKILL_FISHING, 9013);
        }
        if (player->HasSkill(SKILL_FIRST_AID) && player->GetSkillValue(SKILL_FIRST_AID) < 450)
        {
            uint32 NTempId = sHelper->GetUInt(9014);
            if (NTempId)
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Spell_Holy_SealOfSacrifice.blp:24:24:-20:0|t 秒升满级 [急救]", SKILL_FIRST_AID, 9014, sHelper->GetNeedGts(NTempId), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/Icons/Spell_Holy_SealOfSacrifice.blp:24:24:-20:0|t 秒升满级 [急救]", SKILL_FIRST_AID, 9014);
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "|TInterface/ICONS/achievement_pvp_o_12.blp:30:30:-20:0|t →|cff0000ff 返回首页|r", 0, 0);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (player->IsInCombat())
        {
            player->SendAsMsg(sHelper->GetGts(1001));
            return true;
        }

        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        std::string Menu1 = sHelper->GetStrConfig(9711);
        std::string Menu2 = sHelper->GetStrConfig(9712);
        std::string Menu3 = sHelper->GetStrConfig(9713);
        std::string Menu4 = sHelper->GetStrConfig(9714);
        std::string Menu5 = sHelper->GetStrConfig(9715);
        std::string Menu6 = sHelper->GetStrConfig(9716);
        std::string Menu7 = sHelper->GetStrConfig(9717);
        std::string Menu8 = sHelper->GetStrConfig(9718);
        std::string Menu9 = sHelper->GetStrConfig(9719);

        if (!Menu1.empty())
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu1, 100, 1001);
        if (!Menu2.empty())
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu2, 100, 1002);
        if (!Menu3.empty())
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu3, 100, 1003);
        if (!Menu4.empty())
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu4, 100, 1004);

        if (!Menu5.empty())
        {
            if (sHelper->GetUInt(9702))
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu5, 100, 1005, "\n重置角色天赋\n" + sHelper->GetNeedGts(sHelper->GetUInt(9702)), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu5, 100, 1005);
        }

        if (!Menu6.empty())
        {
            if (sHelper->GetUInt(9701))
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu6, 100, 1006, "\n开通双天赋\n" + sHelper->GetNeedGts(sHelper->GetUInt(9701)), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu6, 100, 1006);
        }

        if (!Menu7.empty())
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu7, 100, 1007);

        if (!Menu8.empty())
        {
            if (sHelper->GetUInt(9703))
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu8, 100, 1008, "\n提升技能熟练度\n" + sHelper->GetNeedGts(sHelper->GetUInt(9703)), 0, false);
            else
                AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu8, 100, 1008);
        }

        if (sHelper->GetBoolean(9000) && !Menu9.empty())
            AddGossipItemFor(player, GOSSIP_ICON_TALK, Menu9, 100, 1009);

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

        if (Action > 9000)
        {
            if (Sender == 9999)
            {
                SendShowMaxPro(player, creature);
                return true;
            }

            if (!sHelper->GetBoolean(9000))
            {
                CloseGossipMenuFor(player);
                return true;
            }

            if (!player->HasSkill(Sender))
            {
                player->SendSysMsg("你没有掌握 [%s] 专业 无法使用", GetProStr(Sender));
                SendShowMaxPro(player, creature);
                return true;
            }

            if (player->GetSkillValue(Sender) >= 450)
            {
                player->SendSysMsg("此专业已经满级 你无需秒升此专业");
                SendShowMaxPro(player, creature);
                return true;
            }

            if (sHelper->CheckNeed(player, sHelper->GetUInt(Action), "秒升专业消耗"))
            {
                bool HasSkill = player->GetSkillValue(Sender);
                player->SetSkill(Sender, HasSkill ? player->GetSkillStep(Sender) : 1, 450, 450);
                player->SendSysMsg("祝贺你 [%s] 专业秒升满级成功", GetProStr(Sender));
            }

            SendShowMaxPro(player, creature);
            return true;
        }

        if (Sender == 100)
        {
            if (Action == 1001)
            {
                uint32 NpcId = 0;
                switch (player->GetClass())
                {
                case CLASS_WARRIOR: NpcId = 51001; break;
                case CLASS_PALADIN:    NpcId = 51002; break;
                case CLASS_HUNTER:    NpcId = 51003; break;
                case CLASS_ROGUE:    NpcId = 51004; break;
                case CLASS_PRIEST:    NpcId = 51005; break;
                case CLASS_DEATH_KNIGHT:    NpcId = 51006; break;
                case CLASS_SHAMAN:    NpcId = 51007; break;
                case CLASS_MAGE:    NpcId = 51008; break;
                case CLASS_WARLOCK:    NpcId = 51009; break;
                case CLASS_DRUID:    NpcId = 51011; break;
                }
                player->GetSession()->SendNewTrainerList(creature, NpcId);
                return true;
            }
            else if (Action == 1002)
            {
                player->GetSession()->SendNewTrainerList(creature, 51801);
                return true;
            }
            else if (Action == 1003)
            {
                player->GetSession()->SendNewTrainerList(creature, 51802);
                return true;
            }
            else if (Action == 1004)
            {
                SendShowSkill(player, creature);
                return true;
            }
            else if (Action == 1005)
            {
                if (sHelper->CheckNeed(player, sHelper->GetUInt(9702), "重置天赋消耗"))
                {
                    if (player->resetTalents(true))
                    {
                        player->InitTalentForLevel();
                        player->SendTalentsInfoData(false);
                        player->SendSysMsg(sHelper->GetGts(1072));
                    }
                }

                OnGossipHello(player, creature);
                return true;
            }
            else if (Action == 1006)
            {
                if (player->GetSpecsCount() > 1 || player->GetLevel() < sWorld->getIntConfig(CONFIG_MIN_DUALSPEC_LEVEL))
                {
                    player->SendSysMsg(sHelper->GetGts(1073));
                    OnGossipHello(player, creature);
                    return true;
                }

                if (!sHelper->GetBoolean(20002))
                {
                    player->SendSysMsg(sHelper->GetGts(9801));
                    CloseGossipMenuFor(player);
                    return true;
                }

                if (sHelper->CheckNeed(player, sHelper->GetUInt(9701), "开通双天赋消耗"))
                {
                    player->CastSpell(player, 63680, true, nullptr, nullptr, player->GetGUID());
                    player->CastSpell(player, 63624, true, nullptr, nullptr, player->GetGUID());
                    player->SendSysMsg(sHelper->GetGts(1074));
                }

                OnGossipHello(player, creature);
                return true;
            }
            else if (Action == 1007)
            {
                Pet* pet = player->GetPet();
                if (!pet || !pet->IsInWorld())
                {
                    player->SendSysMsg(sHelper->GetGts(1076));
                    OnGossipHello(player, creature);
                    return true;
                }

                player->ResetPetTalents();
                player->SendSysMsg(sHelper->GetGts(1077));
                OnGossipHello(player, creature);
                return true;
            }
            else if (Action == 1008)
            {
                if (sHelper->CheckNeed(player, sHelper->GetUInt(9703), "提升武器熟练度消耗"))
                {
                    player->UpdateSkillsToMaxSkillsForLevel();
                    player->SendSysMsg(sHelper->GetGts(1004));
                }

                OnGossipHello(player, creature);
                return true;
            }
            else if (Action == 1009)
            {
                SendShowMaxPro(player, creature);
                return true;
            }
        }
        else if (Sender == 101)
        {
            uint32 NpcSkill = 0;
            switch (Action)
            {
            case 2001:NpcSkill = 52001; break;//采矿
            case 2002:NpcSkill = 52002; break;//草药
            case 2003:NpcSkill = 52003; break;//炼金
            case 2004:NpcSkill = 52004; break;//附魔
            case 2005:NpcSkill = 52005; break;//锻造
            case 2006:NpcSkill = 52006; break;//裁缝
            case 2007:NpcSkill = 52007; break;//工程
            case 2008:NpcSkill = 52008; break;//珠宝
            case 2009:NpcSkill = 52009; break;//制皮
            case 2010:NpcSkill = 52010; break;//剥皮
            case 2011:NpcSkill = 52011; break;//铭文
            case 2012:NpcSkill = 52012; break;//烹饪
            case 2013:NpcSkill = 52013; break;//钓鱼
            case 2014:NpcSkill = 52014; break;//急救
            }
            player->GetSession()->SendNewTrainerList(creature, NpcSkill);
            return true;
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

Creature* Player::GetCustomNpcWith(ObjectGuid const& Guid)
{
    if (!Guid)
        return nullptr;

    if (!IsInWorld())
        return nullptr;

    if (IsInFlight())
        return nullptr;

    Creature* creature = ObjectAccessor::GetCreatureOrPetOrVehicle(*this, Guid);
    if (!creature)
        return nullptr;

    if (!IsAlive() && !(creature->GetCreatureTemplate()->type_flags & CREATURE_TYPE_FLAG_VISIBLE_TO_GHOSTS))
        return nullptr;

    if (!creature->IsAlive() && !(creature->GetCreatureTemplate()->type_flags & CREATURE_TYPE_FLAG_INTERACT_WHILE_DEAD))
        return nullptr;

    if (creature->GetCharmerGUID())
        return nullptr;

    if (creature->GetReactionTo(this) <= REP_UNFRIENDLY)
        return nullptr;

    if (!creature->IsWithinDistInMap(this, creature->GetCombatReach() + 4.0f))
        return nullptr;

    return creature;
}

bool Player::BuyItemFromVendorSlotToNew(ObjectGuid Vendorguid, uint32 Vendorslot, uint32 ItemId, uint8 Count, uint8 Bag, uint8 Slot)
{
    if (Count < 1) Count = 1;

    if (Slot > MAX_BAG_SIZE && Slot != NULL_SLOT)
        return false;

    if (!IsAlive())
        return false;

    ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(ItemId);
    if (!pProto)
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, nullptr, ItemId, 0);
        return false;
    }

    if (!(pProto->AllowableClass & GetClassMask()) && pProto->Bonding == BIND_WHEN_PICKED_UP && !IsGameMaster())
    {
        SendBuyError(BUY_ERR_CANT_FIND_ITEM, nullptr, ItemId, 0);
        return false;
    }

    if (!IsGameMaster() && ((pProto->Flags2 & ITEM_FLAG2_FACTION_HORDE && GetTeamId(true) == TEAM_ALLIANCE) || (pProto->Flags2 == ITEM_FLAG2_FACTION_ALLIANCE && GetTeamId(true) == TEAM_HORDE)))
        return false;

    Creature* creature = nullptr;
    if (!Vendorguid.IsPlayer())
    {
        creature = GetCustomNpcWith(Vendorguid);
        if (!creature)
        {
            SendBuyError(BUY_ERR_DISTANCE_TOO_FAR, nullptr, ItemId, 0);
            return false;
        }
    }

    uint32 vendorEntry = GetSession()->GetVEntry();

    CustomVendorItemVect const* customItems = sCustomVendorMgr->GetCustomVendorItemList(vendorEntry);
    bool isCustomVendor = (customItems && !customItems->empty());
    CustomVendorItem const* customItem = nullptr;
    VendorItem const* crItem = nullptr;

    if (isCustomVendor)
    {
        if (Vendorslot >= customItems->size())
        {
            SendBuyError(BUY_ERR_CANT_FIND_ITEM, creature, ItemId, 0);
            return false;
        }

        customItem = &(*customItems)[Vendorslot];
        if (!customItem || customItem->item != ItemId)
        {
            SendBuyError(BUY_ERR_CANT_FIND_ITEM, creature, ItemId, 0);
            return false;
        }

        if (customItem->maxcount > 0)
        {
            if (sCustomVendorMgr->GetCustomVendorCurrentCount(vendorEntry, customItem->item, customItem->maxcount, customItem->incrtime) < pProto->BuyCount * Count)
            {
                SendBuyError(BUY_ERR_ITEM_ALREADY_SOLD, creature, ItemId, 0);
                return false;
            }
        }
    }
    else
    {
        VendorItemData const* vItems = sObjectMgr->GetNpcVendorItemList(vendorEntry);
        if (!vItems || vItems->Empty())
        {
            SendBuyError(BUY_ERR_CANT_FIND_ITEM, creature, ItemId, 0);
            return false;
        }

        if (Vendorslot >= vItems->GetItemCount())
        {
            SendBuyError(BUY_ERR_CANT_FIND_ITEM, creature, ItemId, 0);
            return false;
        }

        crItem = vItems->GetItem(Vendorslot);
        if (!crItem || crItem->item != ItemId)
        {
            SendBuyError(BUY_ERR_CANT_FIND_ITEM, creature, ItemId, 0);
            return false;
        }

        if (crItem->maxcount != 0)
        {
            if (creature && creature->GetVendorItemCurrentCount(crItem) < pProto->BuyCount * Count)
            {
                SendBuyError(BUY_ERR_ITEM_ALREADY_SOLD, creature, ItemId, 0);
                return false;
            }
        }
    }

    if (pProto->RequiredReputationFaction && (uint32(GetReputationRank(pProto->RequiredReputationFaction)) < pProto->RequiredReputationRank))
    {
        SendBuyError(BUY_ERR_REPUTATION_REQUIRE, creature, ItemId, 0);
        return false;
    }

    uint32 extendedCost = isCustomVendor ? customItem->ExtendedCost : crItem->ExtendedCost;

    if (extendedCost)
    {
        ItemExtendedCostEntry const* iece = sItemExtendedCostStore.LookupEntry(extendedCost);
        if (!iece)
        {
            //TC_LOG_ERROR("entities.player", "Player::BuyItemFromVendorSlot: Item %u has wrong ExtendedCost field value %u", pProto->ItemId, extendedCost);
            return false;
        }

        if (GetHonorPoints() < (iece->reqhonorpoints * Count))
        {
            SendEquipError(EQUIP_ERR_NOT_ENOUGH_HONOR_POINTS, nullptr, nullptr);
            return false;
        }

        if (GetArenaPoints() < (iece->reqarenapoints * Count))
        {
            SendEquipError(EQUIP_ERR_NOT_ENOUGH_ARENA_POINTS, nullptr, nullptr);
            return false;
        }

        for (uint8 i = 0; i < MAX_ITEM_EXTENDED_COST_REQUIREMENTS; ++i)
        {
            if (iece->reqitem[i] && !HasItemCount(iece->reqitem[i], (iece->reqitemcount[i] * Count)))
            {
                SendEquipError(EQUIP_ERR_VENDOR_MISSING_TURNINS, nullptr, nullptr);
                return false;
            }
        }

        if (GetMaxPersonalArenaRatingRequirement(iece->reqarenaslot) < iece->reqpersonalarenarating)
        {
            SendEquipError(EQUIP_ERR_CANT_EQUIP_RANK, nullptr, nullptr);
            return false;
        }
    }

    uint64 price = 0;
    bool needGold = isCustomVendor ? (extendedCost == 0) : crItem->IsGoldRequired(pProto);

    if (needGold && pProto->BuyPrice > 0)
    {
        uint64 maxCount = MAX_MONEY_AMOUNT / pProto->BuyPrice;
        if ((uint32)Count > maxCount)
        {
            /*TC_LOG_ERROR("entities.player.cheat", "Player::BuyItemFromVendorSlot: Player '%s' (%s) tried to buy item (ItemID: %u, Count: %u), causing overflow",
                GetName().c_str(), GetGUID().ToString().c_str(), pProto->ItemId, (uint32)Count);*/
            Count = (uint8)maxCount;
        }
        price = pProto->BuyPrice * Count;

        if (creature)
            price = MoneyMath::Scale(price, GetReputationPriceDiscount(creature));

        if (!HasEnoughMoney(price))
        {
            SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, creature, ItemId, 0);
            return false;
        }
    }

    VendorItem* tempCrItem = nullptr;
    if (!isCustomVendor)
    {
        VendorItemData const* vItems = sObjectMgr->GetNpcVendorItemList(vendorEntry);
        if (vItems)
            tempCrItem = vItems->GetItem(Vendorslot);
    }

    if ((Bag == NULL_BAG && Slot == NULL_SLOT) || IsInventoryPos(Bag, Slot))
    {
        if (!_StoreOrEquipNewItem(Vendorslot, ItemId, Count, Bag, Slot, price, pProto, creature, tempCrItem, true))
            return false;
    }
    else if (IsEquipmentPos(Bag, Slot))
    {
        if (pProto->BuyCount * Count != 1)
        {
            SendEquipError(EQUIP_ERR_ITEM_CANT_BE_EQUIPPED, nullptr, nullptr);
            return false;
        }
        if (!_StoreOrEquipNewItem(Vendorslot, ItemId, Count, Bag, Slot, price, pProto, creature, tempCrItem, false))
            return false;
    }
    else
    {
        SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, nullptr, nullptr);
        return false;
    }

    if (customItem && customItem->maxcount > 0)
    {
        sCustomVendorMgr->UpdateCustomVendorCurrentCount(vendorEntry, customItem->item, customItem->maxcount, customItem->incrtime, pProto->BuyCount * Count);
        // 刷新商人列表，让客户端实时显示新库存
        if (creature)
            GetSession()->SendCustomListInventory(creature->GetGUID(), vendorEntry);
    }

    if (tempCrItem)
        return tempCrItem->maxcount != 0;

    return customItem && customItem->maxcount != 0;
}

void WorldSession::SendNewTrainerList(Creature* Npc, uint32 NowNpcId)
{
    SetTEntry(NowNpcId);

    if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    if (!Npc->IsWithinDistInMap(GetPlayer(), Npc->GetCombatReach() + 4.0f))
        return;

    TrainerSpellData const* trainer_spells = sObjectMgr->GetNpcTrainerSpells(NowNpcId);
    if (!trainer_spells)
        return;

    std::string strTitle = GetAcoreString(LANG_NPC_TAINER_HELLO);
    WorldPacket data(SMSG_TRAINER_LIST, 8 + 4 + 4 + trainer_spells->spellList.size() * 38 + strTitle.size() + 1);
    data << Npc->GetGUID();
    data << uint32(trainer_spells->trainerType);

    size_t count_pos = data.wpos();
    data << uint32(trainer_spells->spellList.size());

    float fDiscountMod = _player->GetReputationPriceDiscount(Npc);
    bool can_learn_primary_prof = GetPlayer()->GetFreePrimaryProfessionPoints() > 0;

    uint32 count = 0;
    for (TrainerSpellMap::const_iterator itr = trainer_spells->spellList.begin(); itr != trainer_spells->spellList.end(); ++itr)
    {
        TrainerSpell const* tSpell = &itr->second;

        bool valid = true;
        bool primary_prof_first_rank = false;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (!tSpell->learnedSpell[i])
                continue;
            if (!_player->IsSpellFitByClassAndRace(tSpell->learnedSpell[i]))
            {
                valid = false;
                break;
            }
            SpellInfo const* learnedSpellInfo = sSpellMgr->GetSpellInfo(tSpell->learnedSpell[i]);
            if (learnedSpellInfo && learnedSpellInfo->IsPrimaryProfessionFirstRank())
                primary_prof_first_rank = true;
        }

        if (!valid)
            continue;

        if (tSpell->reqSpell && !_player->HasSpell(tSpell->reqSpell))
        {
            continue;
        }

        TrainerSpellState state = _player->GetTrainerSpellState(tSpell);

        data << uint32(tSpell->spell);                      // learned spell (or cast-spell in profession case)
        data << uint8(state == TRAINER_SPELL_GREEN_DISABLED ? TRAINER_SPELL_GREEN : state);
        data << uint32(floor(tSpell->spellCost * fDiscountMod));

        data << uint32(primary_prof_first_rank && can_learn_primary_prof ? 1 : 0);
        // primary prof. learn confirmation dialog
        data << uint32(primary_prof_first_rank ? 1 : 0);    // must be equal prev. field to have learn button in enabled state
        data << uint8(tSpell->reqLevel);
        data << uint32(tSpell->reqSkill);
        data << uint32(tSpell->reqSkillValue);
        //prev + req or req + 0
        uint8 maxReq = 0;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (!tSpell->learnedSpell[i])
                continue;
            if (uint32 prevSpellId = sSpellMgr->GetPrevSpellInChain(tSpell->learnedSpell[i]))
            {
                data << uint32(prevSpellId);
                ++maxReq;
            }
            if (maxReq == 3)
                break;
            auto spellsRequired = sSpellMgr->GetSpellsRequiredForSpellBounds(tSpell->learnedSpell[i]);
            for (auto itr2 = spellsRequired.begin(); itr2 != spellsRequired.end() && maxReq < 3; ++itr2)
            {
                data << uint32(itr2->second);
                ++maxReq;
            }
            if (maxReq == 3)
                break;
        }
        while (maxReq < 3)
        {
            data << uint32(0);
            ++maxReq;
        }

        ++count;
    }

    data << strTitle;

    data.put<uint32>(count_pos, count);
    SendPacket(&data);
}

void WorldSession::SendNewListInventory(ObjectGuid Guid, uint32 VendorId)
{
    Creature* vendor = GetPlayer()->GetCustomNpcWith(Guid);
    if (!vendor)
        return;

    if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    if (uint32 pause = vendor->GetMovementTemplate().GetInteractionPauseTimer())
        vendor->PauseMovement(pause);

    vendor->SetHomePosition(vendor->GetPosition());

    SetVEntry(VendorId);
    SetCurrentVendor(VendorId);

    // 优先检查 _生物自定义售卖表
    CustomVendorItemVect const* customItems = sCustomVendorMgr->GetCustomVendorItemList(VendorId);
    if (customItems && !customItems->empty())
    {
        uint8 itemCount = customItems->size();
        uint8 count = 0;

        WorldPacket data(SMSG_LIST_INVENTORY, 8 + 1 + itemCount * 8 * 4);
        data << Guid;

        size_t countPos = data.wpos();
        data << uint8(count);

        float discountMod = _player->GetReputationPriceDiscount(vendor);

        for (uint8 slot = 0; slot < itemCount; ++slot)
        {
            CustomVendorItem const& item = (*customItems)[slot];
            if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(item.item))
            {
                if (!(itemTemplate->AllowableClass & _player->getClassMask()) && itemTemplate->Bonding == BIND_WHEN_PICKED_UP && !_player->IsGameMaster())
                    continue;

                if (!_player->IsGameMaster() && ((itemTemplate->Flags2 & ITEM_FLAGS_EXTRA_HORDE_ONLY && _player->GetTeamId() == TEAM_ALLIANCE) || (itemTemplate->Flags2 & ITEM_FLAGS_EXTRA_ALLIANCE_ONLY && _player->GetTeamId() == TEAM_HORDE)))
                    continue;

                uint32 leftInStock = sCustomVendorMgr->GetCustomVendorCurrentCount(VendorId, item.item, item.maxcount, item.incrtime);
                if (!_player->IsGameMaster() && !leftInStock)
                    continue;

                uint32 NTempId = item.NTempId;
                uint64 price;
                uint32 extendedCost;
                if (NTempId)
                {
                    price = 0;
                    extendedCost = 0;
                }
                else
                {
                    price = item.ExtendedCost ? 0 : MoneyMath::Scale(itemTemplate->BuyPrice, discountMod);
                    extendedCost = item.ExtendedCost;
                }

                data << uint32(slot + 1);
                data << uint32(item.item);
                data << uint32(itemTemplate->DisplayInfoID);
                data << int32(leftInStock);
                data << uint32(price);
                data << uint32(itemTemplate->MaxDurability);
                data << uint32(itemTemplate->BuyCount);
                data << uint32(extendedCost);

                if (++count >= MAX_VENDOR_ITEMS)
                    break;
            }
        }

        if (count == 0)
        {
            data << uint8(0);
            SendPacket(&data);
            return;
        }

        data.put<uint8>(countPos, count);
        SendPacket(&data);
        return;
    }

    VendorItemData const* items = VendorId ? sObjectMgr->GetNpcVendorItemList(VendorId) : vendor->GetVendorItems();
    if (!items)
    {
        WorldPacket data(SMSG_LIST_INVENTORY, 8 + 1 + 1);
        data << Guid;
        data << uint8(0);                                   // count == 0, next will be error code
        data << uint8(0);                                   // "Vendor has no inventory"
        SendPacket(&data);
        return;
    }

    uint8 itemCount = items->GetItemCount();
    uint8 count = 0;

    WorldPacket data(SMSG_LIST_INVENTORY, 8 + 1 + itemCount * 8 * 4);
    data << Guid;

    size_t countPos = data.wpos();
    data << uint8(count);

    for (uint8 slot = 0; slot < itemCount; ++slot)
    {
        if (VendorItem const* item = items->GetItem(slot))
        {
            if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(item->item))
            {
                if (!(itemTemplate->AllowableClass & _player->getClassMask()) && itemTemplate->Bonding == BIND_WHEN_PICKED_UP && !_player->IsGameMaster())
                    continue;

                if (!_player->IsGameMaster() && ((itemTemplate->Flags2 & ITEM_FLAGS_EXTRA_HORDE_ONLY && _player->GetTeamId() == TEAM_ALLIANCE) || (itemTemplate->Flags2 & ITEM_FLAGS_EXTRA_ALLIANCE_ONLY && _player->GetTeamId() == TEAM_HORDE)))
                    continue;

                uint32 leftInStock = 0xFFFFFFFF;
                if (!_player->IsGameMaster() && !leftInStock)
                    continue;

                bool In = sItemFuncMgr->IsItemVsInfo(item->item);
                uint64 price = item->IsGoldRequired(itemTemplate) ? itemTemplate->BuyPrice : 0;
                price = In ? 0 : price;

                data << uint32(slot + 1);       // client expects counting to start at 1
                data << uint32(item->item);
                data << uint32(itemTemplate->DisplayInfoID);
                data << int32(leftInStock);
                data << uint32(price);
                data << uint32(itemTemplate->MaxDurability);
                data << uint32(itemTemplate->BuyCount);
                data << uint32(In ? 0 : item->ExtendedCost);

                if (++count >= MAX_VENDOR_ITEMS)
                    break;
            }
        }
    }

    if (count == 0)
    {
        data << uint8(0);
        SendPacket(&data);
        return;
    }

    data.put<uint8>(countPos, count);
    SendPacket(&data);
}

void WorldSession::SendPlayerNewListInventory(ObjectGuid PlayerGuid, uint32 VendorId)
{
    if (!VendorId)
        return;

    if (!PlayerGuid.IsPlayer())
        return;

    if (!GetPlayer()->IsAlive())
        return;

    if (GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    SetVEntry(VendorId);
    SetCurrentVendor(VendorId);

    // 优先检查 _生物自定义售卖表
    CustomVendorItemVect const* customItems = sCustomVendorMgr->GetCustomVendorItemList(VendorId);
    if (customItems && !customItems->empty())
    {
        uint8 itemCount = customItems->size();
        uint8 count = 0;

        WorldPacket data(SMSG_LIST_INVENTORY, 8 + 1 + itemCount * 8 * 4);
        data << PlayerGuid;

        size_t countPos = data.wpos();
        data << uint8(count);

        for (uint8 slot = 0; slot < itemCount; ++slot)
        {
            CustomVendorItem const& item = (*customItems)[slot];
            if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(item.item))
            {
                if (!(itemTemplate->AllowableClass & _player->getClassMask()) && itemTemplate->Bonding == BIND_WHEN_PICKED_UP && !_player->IsGameMaster())
                    continue;

                if (!_player->IsGameMaster() && ((itemTemplate->Flags2 & ITEM_FLAGS_EXTRA_HORDE_ONLY && _player->GetTeamId() == TEAM_ALLIANCE) || (itemTemplate->Flags2 & ITEM_FLAGS_EXTRA_ALLIANCE_ONLY && _player->GetTeamId() == TEAM_HORDE)))
                    continue;

                uint32 leftInStock = sCustomVendorMgr->GetCustomVendorCurrentCount(VendorId, item.item, item.maxcount, item.incrtime);
                if (!_player->IsGameMaster() && !leftInStock)
                    continue;

                uint32 NTempId = item.NTempId;
                uint64 price;
                uint32 extendedCost;
                if (NTempId)
                {
                    price = 0;
                    extendedCost = 0;
                }
                else
                {
                    price = item.ExtendedCost ? 0 : itemTemplate->BuyPrice;
                    extendedCost = item.ExtendedCost;
                }

                data << uint32(slot + 1);
                data << uint32(item.item);
                data << uint32(itemTemplate->DisplayInfoID);
                data << int32(leftInStock);
                data << uint32(price);
                data << uint32(itemTemplate->MaxDurability);
                data << uint32(itemTemplate->BuyCount);
                data << uint32(extendedCost);

                if (++count >= MAX_VENDOR_ITEMS)
                    break;
            }
        }

        if (count == 0)
        {
            data << uint8(0);
            SendPacket(&data);
            return;
        }

        data.put<uint8>(countPos, count);
        SendPacket(&data);
        return;
    }

    VendorItemData const* items = sObjectMgr->GetNpcVendorItemList(VendorId);
    if (!items)
    {
        WorldPacket data(SMSG_LIST_INVENTORY, 8 + 1 + 1);
        data << PlayerGuid;
        data << uint8(0);
        data << uint8(0);
        SendPacket(&data);
        return;
    }

    uint8 itemCount = items->GetItemCount();
    uint8 count = 0;

    WorldPacket data(SMSG_LIST_INVENTORY, 8 + 1 + itemCount * 8 * 4);
    data << PlayerGuid;

    size_t countPos = data.wpos();
    data << uint8(count);

    for (uint8 slot = 0; slot < itemCount; ++slot)
    {
        if (VendorItem const* item = items->GetItem(slot))
        {
            if (ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(item->item))
            {
                if (!(itemTemplate->AllowableClass & _player->getClassMask()) && itemTemplate->Bonding == BIND_WHEN_PICKED_UP && !_player->IsGameMaster())
                    continue;

                if (!_player->IsGameMaster() && ((itemTemplate->Flags2 & ITEM_FLAGS_EXTRA_HORDE_ONLY && _player->GetTeamId() == TEAM_ALLIANCE) || (itemTemplate->Flags2 & ITEM_FLAGS_EXTRA_ALLIANCE_ONLY && _player->GetTeamId() == TEAM_HORDE)))
                    continue;

                uint32 leftInStock = 0xFFFFFFFF;
                if (!_player->IsGameMaster() && !leftInStock)
                    continue;

                bool In = sItemFuncMgr->IsItemVsInfo(item->item);
                uint64 price = item->IsGoldRequired(itemTemplate) ? itemTemplate->BuyPrice : 0;
                price = In ? 0 : price;

                data << uint32(slot + 1);       // client expects counting to start at 1
                data << uint32(item->item);
                data << uint32(itemTemplate->DisplayInfoID);
                data << int32(leftInStock);
                data << uint32(price);
                data << uint32(itemTemplate->MaxDurability);
                data << uint32(itemTemplate->BuyCount);
                data << uint32(In ? 0 : item->ExtendedCost);

                if (++count >= MAX_VENDOR_ITEMS)
                    break;
            }
        }
    }

    if (count == 0)
    {
        data << uint8(0);
        SendPacket(&data);
        return;
    }

    data.put<uint8>(countPos, count);
    SendPacket(&data);
}

class MultiVendorNpc : public CreatureScript
{
public:
    MultiVendorNpc() : CreatureScript("MultiVendorNpc") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (player->IsInCombat())
        {
            player->SendAsMsg(sHelper->GetGts(1001));
            return false;
        }

        auto Itr = VendorStructStore.find(creature->GetEntry());
        if (Itr == VendorStructStore.end())
            return true;

        std::vector<VendorNum> NumVect;
        uint32 FactionId = player->GetTeamId() == TEAM_ALLIANCE ? 1 : 2;
        for (int k = 0; k < VendorStructStore.count(creature->GetEntry()); k++, Itr++)
        {
            if ((Itr->second.Tean == 0 || FactionId == Itr->second.Tean) && sHelper->IsClassInMask(player->GetClass(), Itr->second.Class))
            {
                // 检查显示需求模版，如果不满足则不显示该菜单项（使用ShowCheckNeed不发送提示）
                if (Itr->second.ShowNeedTempId > 0 && !sHelper->ShowCheckNeed(player, Itr->second.ShowNeedTempId))
                    continue;

                VendorNum St;
                St.Gossip = Itr->second.Gossip;
                St.VTEntry = Itr->second.VTEntry;
                St.Num = Itr->second.Num;
                NumVect.push_back(St);
            }
        }

        if (NumVect.empty())
            return true;

        std::sort(NumVect.begin(), NumVect.end(), [](const VendorNum& V1, const VendorNum& V2) {return V1.Num < V2.Num; });
        for (auto It = NumVect.begin(); It != NumVect.end(); It++)
        {
            AddGossipItemFor(player, GOSSIP_ICON_TALK, It->Gossip, 0, It->VTEntry);
        }

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());

        NumVect.clear();
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 Sender, uint32 Action) override
    {
        ClearGossipMenuFor(player);

        player->GetSession()->SendNewListInventory(creature->GetGUID(), Action);

        return true;
    }

};

void AddSC_MultiNpcMgrScripts()
{
    new MultiTrainerNpc();
    new MultiVendorNpc();
}
