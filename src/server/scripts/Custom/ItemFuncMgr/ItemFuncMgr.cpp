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
#include "ItemFuncMgr.h"
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include <Mails/Mail.h>
#include <Loot/LootMgr.h>
#include "Custom/ItemRandomAtMgr/ItemRandomAtMgr.h"
#include "Custom/ItemRunesMgr/ItemRunesMgr.h"
#include <Custom/PlayerModMgr/PlayerModMgr.h>
#pragma execution_character_set("utf-8")

FunctionMap FunctionStore;
FunctionMenuMap FunctionMenuStore;
ItemInStatVect ItemInStatStore;
LuckTempVect LuckTempStore;
ItemCommandMap ItemCommandStore;
ItemUpRandomMap ItemUpRandomStore;
ItemUpChoiceTempMap ItemUpChoiceTempStore;
SoulItemConfigMap SoulItemConfigStore;
SoulItemMap SoulItemStore;
SummonStructMap SummonStructStore;
ItemVsInfoVect ItemVsInfoStore;
ItemEnchMap ItemEnchStore;
EnchGroupMap EnchGroupStore;
CdkeyMap CdkeyStore;
EnchantAtMap EnchantAtStore;
ItemRyMap ItemRyStore;
ItemRyGroupMap ItemRyGroupStore;
ItemClearBindMap ItemClearBindStore;
AtAddItemLimitMap AtAddItemLimitStore;
UseItemLimitMap UseItemLimitStore;
ItemEquipMap ItemEquipStore;
IDTransmogMap IDTransmogStore;
ItemSubNameMap ItemSubNameStore;
EnchResetAtItemStVect EnchResetAtItemStore;
ItemCardsMap ItemCardsStore;
CardsGpMap CardsGpStore;
ItemScriptOnDeleteMap ItemScriptOnDeleteStore;
ItemEnchTaskMap ItemEnchTaskStore;
NewFmZyVect NewFmZyStore;
ItemEnchAtCountMap ItemEnchAtCountStore;
ItemNotTaskMap ItemNotTaskStore;
EnchLimitMap EnchLimitStore;

ItemFuncMgr::ItemFuncMgr()
{

}

ItemFuncMgr::~ItemFuncMgr()
{
    for (auto Itr = SoulItemStore.begin(); Itr != SoulItemStore.end(); ++Itr)
        if (Itr->second.DoItem)
            delete Itr->second.DoItem;
}

void ItemFuncMgr::LoadItemFunc()
{
    ItemInStatStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 物品编号 FROM _物品背包银行获得属性表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            ItemInStatStore.emplace_back(ItemId);
        } while (Result->NextRow());
    }

    FunctionStore.clear();
    FunctionMenuStore.clear();

    Result = WorldDatabase.Query("SELECT 对象编号, 序号, 菜单编号, 标题, 菜单项, 阵营, 显示需求模版, 需求模版, 坐标编号, GM命令组 FROM _功能传送表 ORDER BY 序号;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            CFunction St;
            St.OEntry = Fields[0].Get<uint32>();
            St.Num = Fields[1].Get<uint32>();
            St.MenuId = Fields[2].Get<int32>();
            St.Ttitle = Fields[3].Get<std::string>();
            St.AMenuId = Fields[4].Get<int32>();
            St.FactionId = Fields[5].Get<uint32>();
            St.ShowNeedTempId = Fields[6].Get<uint32>();
            St.NTempId = Fields[7].Get<uint32>();
            St.PosId = Fields[8].Get<uint32>();
            St.Cmd = Fields[9].Get<std::string>();

            FunctionStore.insert(std::make_pair(St.OEntry, St));
            FunctionMenuStore.insert(std::make_pair(St.Num, St));

        } while (Result->NextRow());
    }

    LuckTempStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 物品数量, 几率, 是否公告提示 FROM _幸运抽奖表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            LuckTemp St;
            St.ItemId = Fields[0].Get<uint32>();
            St.Count = Fields[1].Get<uint32>();
            St.Odds = Fields[2].Get<float>();
            St.Ann = Fields[3].Get<uint32>();

            LuckTempStore.emplace_back(St);

        } while (Result->NextRow());
    }

    ItemCommandStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 执行命令组, 随机命令组, 是否消耗, 需求模板 FROM _物品使用命令表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            ItemCommandTemp St;
            St.Cmd = Fields[1].Get<std::string>();
            St.RCmd = Fields[2].Get<std::string>();
            St.OnDel = Fields[3].Get<uint32>();
            St.NTempId = Fields[4].Get<uint32>();

            ItemCommandStore.insert(std::make_pair(ItemId, St));

        } while (Result->NextRow());
    }

    ItemUpRandomStore.clear();
    Result = WorldDatabase.Query("SELECT 原物品编号, 需求模版, 是否保留附魔宝石, 新物品编号1, 成功率1, 新物品编号2, 成功率2, 新物品编号3, 成功率3, 幸运宝石, 幸运几率, 幸运数量 FROM _物品升级随机线表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            UpRandomTemp St;
            St.TempId = Fields[1].Get<uint32>();
            St.OnSave = Fields[2].Get<uint32>();
            St.NewId1 = Fields[3].Get<uint32>();
            St.Chance1 = Fields[4].Get<float>();
            St.NewId2 = Fields[5].Get<uint32>();
            St.Chance2 = Fields[6].Get<float>();
            St.NewId3 = Fields[7].Get<uint32>();
            St.Chance3 = Fields[8].Get<float>();
            St.XyItem = Fields[9].Get<uint32>();
            St.XyOdds = Fields[10].Get<float>();
            St.XyCount = Fields[11].Get<uint32>();

            if (St.Chance1 < 1.0f)
                St.Chance1 = 1.0f;
            if (St.Chance2 < 1.0f)
                St.Chance2 = 1.0f;
            if (St.Chance3 < 1.0f)
                St.Chance3 = 1.0f;

            if (St.Chance1 == St.Chance2 && St.Chance2 == St.Chance3)
            {
                St.Chance1 -= 1.0f;
                St.Chance3 += 1.0f;
            }

            ItemUpRandomStore.insert(std::make_pair(ItemId, St));

        } while (Result->NextRow());
    }

    ItemUpChoiceTempStore.clear();
    Result = WorldDatabase.Query("SELECT 原物品编号, 需求模版, 是否保留附魔宝石, 新物品编号1, 成功率1, 新物品编号2, 成功率2, 新物品编号3, 成功率3, 幸运宝石, 幸运几率, 幸运数量, 防爆宝石, 是否碎裂 FROM _物品升级选择线表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            UpChoiceTemp St;
            St.TempId = Fields[1].Get<uint32>();
            St.OnSave = Fields[2].Get<uint32>();
            St.NewId1 = Fields[3].Get<uint32>();
            St.Chance1 = Fields[4].Get<float>();
            St.NewId2 = Fields[5].Get<uint32>();
            St.Chance2 = Fields[6].Get<float>();
            St.NewId3 = Fields[7].Get<uint32>();
            St.Chance3 = Fields[8].Get<float>();
            St.XyItem = Fields[9].Get<uint32>();
            St.XyOdds = Fields[10].Get<float>();
            St.XyCount = Fields[11].Get<uint32>();
            St.FBItem = Fields[12].Get<uint32>();
            St.Flag = Fields[13].Get<uint32>();

            ItemUpChoiceTempStore.insert(std::make_pair(ItemId, St));

        } while (Result->NextRow());
    }

    SoulItemConfigStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 需求模版 FROM _双甲物品表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            uint32 NTempId = Fields[1].Get<uint32>();

            SoulItemConfigStore.insert(std::make_pair(ItemId, NTempId));

        } while (Result->NextRow());
    }

    SummonStructStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 需求模版, 召唤编号, 提示语, 持续时间, 是否消耗 FROM _物品召唤表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            SummonStruct St;
            St.NTempId = Fields[1].Get<uint32>();
            St.SEntry = Fields[2].Get<int32>();
            St.Text = Fields[3].Get<std::string>();
            St.DoTime = Fields[4].Get<uint32>();
            St.OnDel = Fields[5].Get<uint32>();

            SummonStructStore.insert(std::make_pair(ItemId, St));

        } while (Result->NextRow());
    }

    ItemVsInfoStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 商人编号, 需求模版, 奖励模版 FROM _物品购买或出售表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            ItemVsInfo St;
            St.ItemId = Fields[0].Get<int32>();
            St.VendorId = Fields[1].Get<uint32>();
            St.NTempId = Fields[2].Get<uint32>();
            St.RTempId = Fields[3].Get<uint32>();

            ItemVsInfoStore.push_back(St);

        } while (Result->NextRow());
    }

    ItemEnchStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 附魔1几率, 附魔1组编号, 附魔2几率, 附魔2组编号, 附魔3几率, 附魔3组编号, 附魔4几率, 附魔4组编号, 附魔5几率, 附魔5组编号, 洗练次数, 洗练需求模版 FROM _物品随机附魔控制表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            int32 ItemId = Fields[0].Get<int32>();
            ItemEnchTemp St;
            St.EnchSlot1 = Fields[1].Get<float>();
            St.EnchGroup1 = Fields[2].Get<uint32>();
            St.EnchSlot2 = Fields[3].Get<float>();
            St.EnchGroup2 = Fields[4].Get<uint32>();
            St.EnchSlot3 = Fields[5].Get<float>();
            St.EnchGroup3 = Fields[6].Get<uint32>();
            St.EnchSlot4 = Fields[7].Get<float>();
            St.EnchGroup4 = Fields[8].Get<uint32>();
            St.EnchSlot5 = Fields[9].Get<float>();
            St.EnchGroup5 = Fields[10].Get<uint32>();
            St.Rts = Fields[11].Get<int32>();
            St.NTempId = Fields[12].Get<uint32>();

            ItemEnchStore.insert(std::make_pair(ItemId, St));

        } while (Result->NextRow());
    }

    EnchGroupStore.clear();
    Result = WorldDatabase.Query("SELECT 组编号, 附魔编号, 几率 FROM _物品随机附魔组表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            EnchGroupTemp St;
            St.EnchGroup = Fields[0].Get<uint32>();
            St.EnchId = Fields[1].Get<uint32>();
            St.Chance = Fields[2].Get<float>();

            EnchGroupStore[St.EnchGroup].push_back(St);

        } while (Result->NextRow());
    }

    EnchResetAtItemStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 装备编号, 附魔1几率, 附魔1组编号, 附魔2几率, 附魔2组编号, 附魔3几率, 附魔3组编号, 附魔4几率, 附魔4组编号, 附魔5几率, 附魔5组编号 FROM _物品随机附魔指定表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            EnchResetAtItemSt St;
            St.UseItem = Fields[0].Get<uint32>();
            St.TarItem = Fields[1].Get<uint32>();
            St.EnchSlot1 = Fields[2].Get<float>();
            St.EnchGroup1 = Fields[3].Get<uint32>();
            St.EnchSlot2 = Fields[4].Get<float>();
            St.EnchGroup2 = Fields[5].Get<uint32>();
            St.EnchSlot3 = Fields[6].Get<float>();
            St.EnchGroup3 = Fields[7].Get<uint32>();
            St.EnchSlot4 = Fields[8].Get<float>();
            St.EnchGroup4 = Fields[9].Get<uint32>();
            St.EnchSlot5 = Fields[10].Get<float>();
            St.EnchGroup5 = Fields[11].Get<uint32>();

            EnchResetAtItemStore.emplace_back(St);

        } while (Result->NextRow());
    }

    CdkeyStore.clear();
    Result = LoginDatabase.Query("SELECT 兑换码, 奖励模版 FROM _兑换码奖励表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            std::string Cdkey = Fields[0].Get<std::string>();
            uint32 RTempId = Fields[1].Get<uint32>();

            CdkeyStore.insert(std::make_pair(Cdkey, RTempId));

        } while (Result->NextRow());
    }

    ItemRyStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 奖励材料1, 奖励数量1, 奖励材料2, 奖励数量2, 奖励材料3, 奖励数量3, 奖励材料4, 奖励数量4, 奖励材料5, 奖励数量5, 积分, 回收组序号 FROM _一键回收表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            ItemRyTemp St;
            St.RwItemId1 = Fields[1].Get<uint32>();
            St.RwCount1 = Fields[2].Get<uint32>();
            St.RwItemId2 = Fields[3].Get<uint32>();
            St.RwCount2 = Fields[4].Get<uint32>();
            St.RwItemId3 = Fields[5].Get<uint32>();
            St.RwCount3 = Fields[6].Get<uint32>();
            St.RwItemId4 = Fields[7].Get<uint32>();
            St.RwCount4 = Fields[8].Get<uint32>();
            St.RwItemId5 = Fields[9].Get<uint32>();
            St.RwCount5 = Fields[10].Get<uint32>();
            St.RwToken = Fields[11].Get<uint32>();
            St.Menu = Fields[12].Get<uint32>();

            ItemRyStore.insert(std::make_pair(ItemId, St));

        } while (Result->NextRow());
    }

    ItemRyGroupStore.clear();
    Result = WorldDatabase.Query("SELECT 序号, 菜单文本 FROM _一键回收组;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Num = Fields[0].Get<uint32>();
            std::string Gossip = Fields[1].Get<std::string>();

            ItemRyGroupStore.insert(std::make_pair(Num, Gossip));

        } while (Result->NextRow());
    }

    ItemClearBindStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 需求模版 FROM _物品解绑表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            uint32 NTempId = Fields[1].Get<uint32>();
            ItemClearBindStore.insert(std::make_pair(ItemId, NTempId));
        } while (Result->NextRow());
    }

    AtAddItemLimitStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 角色每日上限, 角色永久上限, 账号每日上限, 账号永久上限 FROM _物品获取限制表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            AtLimit St;
            St.CharDayLimt = Fields[1].Get<uint32>();
            St.CharFrLimt = Fields[2].Get<uint32>();
            St.AccDayLimt = Fields[3].Get<uint32>();
            St.AccFrLimt = Fields[4].Get<uint32>();

            if (!St.CharDayLimt && !St.CharFrLimt && !St.AccDayLimt && !St.AccFrLimt)
                continue;

            if (St.CharDayLimt == St.CharFrLimt && St.CharFrLimt == St.AccDayLimt && St.AccDayLimt == St.AccFrLimt)
                continue;

            if (St.CharDayLimt > 0 && (St.CharFrLimt > 0 || St.AccDayLimt > 0 || St.AccFrLimt > 0))
                continue;

            if (St.CharFrLimt > 0 && (St.CharDayLimt > 0 || St.AccDayLimt > 0 || St.AccFrLimt > 0))
                continue;

            if (St.AccDayLimt > 0 && (St.CharDayLimt > 0 || St.CharFrLimt > 0 || St.AccFrLimt > 0))
                continue;

            if (St.AccFrLimt > 0 && (St.CharDayLimt > 0 || St.CharFrLimt > 0 || St.AccDayLimt > 0))
                continue;

            AtAddItemLimitStore.insert(std::make_pair(ItemId, St));

        } while (Result->NextRow());
    }

    UseItemLimitStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 每日次数, 永久次数 FROM _物品玩家使用次数表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            UseItemLimit St;
            St.UseDayLimt = Fields[1].Get<uint32>();
            St.UseFrLimt = Fields[2].Get<uint32>();

            if (!St.UseDayLimt && !St.UseFrLimt)
                continue;

            UseItemLimitStore.insert(std::make_pair(ItemId, St));

        } while (Result->NextRow());
    }

    ItemEquipStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 需求模板 FROM _物品装备时需求表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            uint32 NTempId = Fields[1].Get<uint32>();
            ItemEquipStore.insert(std::make_pair(ItemId, NTempId));
        } while (Result->NextRow());
    }

    IDTransmogStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 需求模板 FROM _id幻化表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            uint32 NTempId = Fields[1].Get<uint32>();
            IDTransmogStore.insert(std::make_pair(ItemId, NTempId));
        } while (Result->NextRow());
    }

    ItemSubNameStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 文本描述 FROM _物品名字额外描述表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            std::string SubName = Fields[1].Get<std::string>();
            ItemSubNameStore.insert(std::make_pair(ItemId, SubName));

        } while (Result->NextRow());
    }

    ItemCardsStore.clear();
    Result = WorldDatabase.Query("SELECT 卡牌物品编号, 分组1, 分组2, 分组3, 镶嵌需求模板, 镶嵌是否绑定 FROM _卡牌物品控制表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            ItemCardsTemp St;
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            St.Group1 = Fields[1].Get<uint32>();
            St.Group2 = Fields[2].Get<uint32>();
            St.Group3 = Fields[3].Get<uint32>();
            St.NTempId = Fields[4].Get<uint32>();
            St.Bind = Fields[5].Get<bool>();

            ItemCardsStore.insert(std::make_pair(ItemId, St));

        } while (Result->NextRow());
    }

    CardsGpStore.clear();
    Result = WorldDatabase.Query("SELECT 分组编号, 2件技能效果, 4件技能效果, 6件技能效果, 8件技能效果 FROM _卡牌效果控制表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            CardsGpTemp St;
            Field* Fields = Result->Fetch();
            uint32 GroupId = Fields[0].Get<uint32>();
            St.Aura2 = Fields[1].Get<uint32>();
            St.Aura4 = Fields[2].Get<uint32>();
            St.Aura6 = Fields[3].Get<uint32>();
            St.Aura8 = Fields[4].Get<uint32>();

            CardsGpStore.insert(std::make_pair(GroupId, St));

        } while (Result->NextRow());
    }

    ItemScriptOnDeleteStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 是否消耗 FROM _物品脚本消耗表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            uint32 Count = Fields[1].Get<uint32>();

            ItemScriptOnDeleteStore.insert(std::make_pair(ItemId, Count));

        } while (Result->NextRow());
    }

    ItemEnchTaskStore.clear();
    Result = WorldDatabase.Query("SELECT 物品编号, 需求模版 FROM _物品附魔提取转移表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            uint32 NTempId = Fields[1].Get<uint32>();

            ItemEnchTaskStore.insert(std::make_pair(ItemId, NTempId));

        } while (Result->NextRow());
    }

    NewFmZyStore.clear();
    Result = WorldDatabase.Query("SELECT 装备编号, 附魔位置, 转移到物品编号, 需求模版 FROM _新物品附魔提取转移表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            NewFmZyTemp St;
            St.ItemId = Fields[0].Get<uint32>();
            St.Slot = Fields[1].Get<uint32>();
            St.TarItemId = Fields[2].Get<uint32>();
            St.NTempId = Fields[3].Get<uint32>();

            NewFmZyStore.emplace_back(St);

        } while (Result->NextRow());
    }

    ItemNotTaskStore.clear();
    Result = WorldDatabase.Query("SELECT 宝石物品编号 FROM _宝石不可拆除表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            ItemNotTaskStore.insert(ItemId);
        } while (Result->NextRow());
    }

    EnchLimitStore.clear();
    Result = WorldDatabase.Query("SELECT 附魔编号, 限制数量 FROM _全局随机附魔限制表;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 EnchId = Fields[0].Get<uint32>();
            uint32 MaxCount = Fields[1].Get<uint32>();
            EnchLimitStore.insert(std::make_pair(EnchId, MaxCount));
        } while (Result->NextRow());
    }
}

void ItemFuncMgr::LoadSoulItemStore()
{
    SoulItemStore.clear();
    QueryResult Result = CharacterDatabase.Query("SELECT It.creatorGuid, It.giftCreatorGuid, It.count, It.duration, It.charges, It.flags, It.enchantments, It.randomPropertyId, It.durability, It.playedTime, It.text, Soul.物品标识, Soul.物品编号, Soul.玩家编号, Soul.装备位置, It.itemEntry, It.owner_guid FROM _玩家双甲 Soul JOIN item_instance It ON Soul.物品标识 = It.guid LEFT JOIN character_inventory Inv ON It.guid = Inv.item LEFT JOIN mail_items MailItem ON It.guid = MailItem.item_guid LEFT JOIN guild_bank_item GuildItem ON It.guid = GuildItem.item_guid LEFT JOIN auctionhouse AuctionItem ON It.guid = AuctionItem.itemguid WHERE Inv.item IS NULL AND MailItem.item_guid IS NULL AND GuildItem.item_guid IS NULL AND AuctionItem.itemguid IS NULL ORDER BY Soul.物品标识 DESC;");
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemGuid = Fields[11].Get<uint32>();
            uint32 ItemEntry = Fields[12].Get<uint32>();
            uint32 OwnerGuid = Fields[13].Get<uint32>();
            uint32 Slot = Fields[14].Get<uint32>();
            if (Slot >= SOUL_SLOT_END)
                continue;

            uint32 InstanceEntry = Fields[15].Get<uint32>();
            uint32 InstanceOwner = Fields[16].Get<uint32>();

            if (ItemEntry != InstanceEntry)
            {
                LOG_ERROR("server.loading", "双甲存档物品编号不一致，跳过加载：角色={} 槽位={} 物品标识={} 双甲表编号={} 实例编号={}", OwnerGuid, Slot, ItemGuid, ItemEntry, InstanceEntry);
                continue;
            }

            if (OwnerGuid != InstanceOwner)
            {
                LOG_ERROR("server.loading", "双甲存档物品所有者不一致，跳过加载：角色={} 槽位={} 物品标识={} 实例所有者={}", OwnerGuid, Slot, ItemGuid, InstanceOwner);
                continue;
            }

            if (GetSoulItemGuid(OwnerGuid, Slot))
            {
                LOG_WARN("server.loading", "双甲存档存在重复槽位，保留最新有效记录并跳过旧记录：角色={} 槽位={} 物品标识={}", OwnerGuid, Slot, ItemGuid);
                continue;
            }

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(ItemEntry);
            if (!proto)
                continue;

            Item* SoulItem = new Item();
            if (!SoulItem->LoadFromDB(ItemGuid, ObjectGuid::Create<HighGuid::Player>(OwnerGuid), Fields, ItemEntry))
            {
                delete SoulItem;
                continue;
            }

            SoulItemStruct& Data = SoulItemStore[ItemGuid];
            Data.DoItem = SoulItem;
            Data.ItemEntry = ItemEntry;
            Data.OwnerGuidLow = OwnerGuid;
            Data.Slot = Slot;

        } while (Result->NextRow());
    }
}

bool ItemFuncMgr::IsFunctionItem(std::string ScriptName)
{
    return  (strcmp(ScriptName.c_str(), "ItemFunction") == 0);
}

bool ItemFuncMgr::IsItemInStat(uint32 ItemId)
{
    auto Itr = std::find(ItemInStatStore.begin(), ItemInStatStore.end(), ItemId);
    return (Itr != ItemInStatStore.end());
}

void Player::CustomApplyItemBonuses(Item* pItem, bool apply)
{
    if (!pItem)
        return;

    ItemTemplate const* proto = pItem->GetTemplate();
    if (!proto)
        return;

    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        uint32 statType = 0;
        int32  val = 0;

        if (i >= proto->StatsCount)
            continue;
        statType = proto->ItemStat[i].ItemStatType;
        val = proto->ItemStat[i].ItemStatValue;

        AsInt64 realVal = i < proto->StatsCount ? proto->AsItemStatValue[i] : AsInt64(val);
        if (AsInt64IsZero(realVal))
            continue;

        if (statType < MAX_ITEM_MOD)
        {
            DoApplyStat(ItemModType(statType), realVal, apply);
            continue;
        }

        switch (statType)
        {
        case ITEM_MOD_MANA:
            HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(val), apply);
            break;
        case ITEM_MOD_HEALTH:                           // modify HP
            HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(val), apply);
            break;
        case ITEM_MOD_AGILITY:                          // modify agility
            HandleStatModifier(UNIT_MOD_STAT_AGILITY, BASE_VALUE, float(val), apply);
            ApplyStatBuffMod(STAT_AGILITY, float(val), apply);
            break;
        case ITEM_MOD_STRENGTH:                         //modify strength
            HandleStatModifier(UNIT_MOD_STAT_STRENGTH, BASE_VALUE, float(val), apply);
            ApplyStatBuffMod(STAT_STRENGTH, float(val), apply);
            break;
        case ITEM_MOD_INTELLECT:                        //modify intellect
            HandleStatModifier(UNIT_MOD_STAT_INTELLECT, BASE_VALUE, float(val), apply);
            ApplyStatBuffMod(STAT_INTELLECT, float(val), apply);
            break;
        case ITEM_MOD_SPIRIT:                           //modify spirit
            HandleStatModifier(UNIT_MOD_STAT_SPIRIT, BASE_VALUE, float(val), apply);
            ApplyStatBuffMod(STAT_SPIRIT, float(val), apply);
            break;
        case ITEM_MOD_STAMINA:                          //modify stamina
            HandleStatModifier(UNIT_MOD_STAT_STAMINA, BASE_VALUE, float(val), apply);
            ApplyStatBuffMod(STAT_STAMINA, float(val), apply);
            break;
        case ITEM_MOD_DEFENSE_SKILL_RATING:
            ApplyRatingMod(CR_DEFENSE_SKILL, int32(val), apply);
            break;
        case ITEM_MOD_DODGE_RATING:
            ApplyRatingMod(CR_DODGE, int32(val), apply);
            break;
        case ITEM_MOD_PARRY_RATING:
            ApplyRatingMod(CR_PARRY, int32(val), apply);
            break;
        case ITEM_MOD_BLOCK_RATING:
            ApplyRatingMod(CR_BLOCK, int32(val), apply);
            break;
        case ITEM_MOD_HIT_MELEE_RATING:
            ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
            break;
        case ITEM_MOD_HIT_RANGED_RATING:
            ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_HIT_SPELL_RATING:
            ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_MELEE_RATING:
            ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_RANGED_RATING:
            ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_SPELL_RATING:
            ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
            ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
            break;
        case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
            ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
            ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
            ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
            ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
            ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_HASTE_MELEE_RATING:
            ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
            break;
        case ITEM_MOD_HASTE_RANGED_RATING:
            ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
            break;
        case ITEM_MOD_HASTE_SPELL_RATING:
            ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_HIT_RATING:
            ApplyRatingMod(CR_HIT_MELEE, int32(val), apply);
            ApplyRatingMod(CR_HIT_RANGED, int32(val), apply);
            ApplyRatingMod(CR_HIT_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_RATING:
            ApplyRatingMod(CR_CRIT_MELEE, int32(val), apply);
            ApplyRatingMod(CR_CRIT_RANGED, int32(val), apply);
            ApplyRatingMod(CR_CRIT_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_HIT_TAKEN_RATING:
            ApplyRatingMod(CR_HIT_TAKEN_MELEE, int32(val), apply);
            ApplyRatingMod(CR_HIT_TAKEN_RANGED, int32(val), apply);
            ApplyRatingMod(CR_HIT_TAKEN_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_CRIT_TAKEN_RATING:
        case ITEM_MOD_RESILIENCE_RATING:
            ApplyRatingMod(CR_CRIT_TAKEN_MELEE, int32(val), apply);
            ApplyRatingMod(CR_CRIT_TAKEN_RANGED, int32(val), apply);
            ApplyRatingMod(CR_CRIT_TAKEN_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_HASTE_RATING:
            ApplyRatingMod(CR_HASTE_MELEE, int32(val), apply);
            ApplyRatingMod(CR_HASTE_RANGED, int32(val), apply);
            ApplyRatingMod(CR_HASTE_SPELL, int32(val), apply);
            break;
        case ITEM_MOD_EXPERTISE_RATING:
            ApplyRatingMod(CR_EXPERTISE, int32(val), apply);
            break;
        case ITEM_MOD_ATTACK_POWER:
            HandleStatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(val), apply);
            HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), apply);
            break;
        case ITEM_MOD_RANGED_ATTACK_POWER:
            HandleStatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(val), apply);
            break;
            //            case ITEM_MOD_FERAL_ATTACK_POWER:
            //                ApplyFeralAPBonus(int32(val), apply);
            //                break;
        case ITEM_MOD_MANA_REGENERATION:
            ApplyManaRegenBonus(int32(val), apply);
            break;
        case ITEM_MOD_ARMOR_PENETRATION_RATING:
            ApplyRatingMod(CR_ARMOR_PENETRATION, int32(val), apply);
            break;
        case ITEM_MOD_SPELL_POWER:
            ApplySpellPowerBonus(int32(val), apply);
            break;
        case ITEM_MOD_HEALTH_REGEN:
            ApplyHealthRegenBonus(int32(val), apply);
            break;
        case ITEM_MOD_SPELL_PENETRATION:
            ApplySpellPenetrationBonus(val, apply);
            break;
        case ITEM_MOD_BLOCK_VALUE:
            HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(val), apply);
            break;
            // deprecated item mods
        case ITEM_MOD_SPELL_HEALING_DONE:
        case ITEM_MOD_SPELL_DAMAGE_DONE:
            break;
        }
    }

    uint32 armor = proto->Armor;
    if (armor)
    {
        UnitModifierType modType = TOTAL_VALUE;
        if (proto->Class == ITEM_CLASS_ARMOR)
        {
            switch (proto->SubClass)
            {
            case ITEM_SUBCLASS_ARMOR_CLOTH:
            case ITEM_SUBCLASS_ARMOR_LEATHER:
            case ITEM_SUBCLASS_ARMOR_MAIL:
            case ITEM_SUBCLASS_ARMOR_PLATE:
            case ITEM_SUBCLASS_ARMOR_SHIELD:
                modType = BASE_VALUE;
                break;
            }
        }
        HandleStatModifier(UNIT_MOD_ARMOR, modType, float(armor), apply);
    }

    // Add armor bonus from ArmorDamageModifier if > 0
    if (proto->ArmorDamageModifier > 0 && sScriptMgr->OnPlayerCanArmorDamageModifier(this))
        HandleStatModifier(UNIT_MOD_ARMOR, TOTAL_VALUE, float(proto->ArmorDamageModifier), apply);

    if (proto->Block)
        HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(proto->Block), apply);

    if (proto->HolyRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(proto->HolyRes), apply);

    if (proto->FireRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(proto->FireRes), apply);

    if (proto->NatureRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(proto->NatureRes), apply);

    if (proto->FrostRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(proto->FrostRes), apply);

    if (proto->ShadowRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(proto->ShadowRes), apply);

    if (proto->ArcaneRes)
        HandleStatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(proto->ArcaneRes), apply);

    if (getClass() == CLASS_DRUID)
    {
        int32 dpsMod = 0;
        int32 feral_bonus = 0;
        feral_bonus += proto->getFeralBonus(dpsMod);
        if (feral_bonus)
            ApplyFeralAPBonus(feral_bonus, apply);
    }
}

void Player::ApplyItemInStat(Item* pItem, bool Apply)
{
    if (!pItem)
        return;

    ItemTemplate const* proto = pItem->GetTemplate();
    if (!proto)
        return;

    if (!sItemFuncMgr->IsItemInStat(pItem->GetEntry()))
        return;

    CustomApplyItemBonuses(pItem, Apply);

    sItemRandomAtMgr->OnApplyItemAtStoreStatToHF(this, pItem, Apply);

    ApplyItemEquipSpell(pItem, Apply);

    sItemRandomAtMgr->OnApplyItemAtStoreSpellToHF(this, pItem, Apply);

    CustomApplyEnchantment(pItem, Apply);
}

std::pair<uint32, uint32> ItemFuncMgr::PullItemAndCount()
{
    std::pair<uint32, uint32> Result;
    std::string Data = std::string(sHelper->GetConfig(1001));
    Tokenizer Tokens(Data, '#');
    if (Data.empty() || Data.find("#") == std::string::npos || Tokens.IsEmpty())
    {
        Result.first = 0;
        Result.second = 0;
    }
    else
    {
        Result.first = atoi(Tokens[0]);
        Result.second = atoi(Tokens[1]);
    }

    return Result;
}

std::pair<uint32, uint32> ItemFuncMgr::PullTenItemAndCount()
{
    std::pair<uint32, uint32> Result;
    std::string Data = std::string(sHelper->GetConfig(1001));
    Tokenizer Tokens(Data, '#');
    if (Data.empty() || Data.find("#") == std::string::npos || Tokens.IsEmpty())
    {
        Result.first = 0;
        Result.second = 0;
    }
    else
    {
        Result.first = atoi(Tokens[2]);
        Result.second = atoi(Tokens[3]);
    }

    return Result;
}

bool ItemFuncMgr::IsLuckAnn(uint32 ItemId)
{
    bool In = false;
    for (auto Itr = LuckTempStore.begin(); Itr != LuckTempStore.end(); Itr++)
    {
        if (Itr->ItemId == ItemId && Itr->Ann != 0)
            In = true;
    }
    return In;
}

uint32 ItemFuncMgr::MaxLuck()
{
    return LuckTempStore.size();
}

void ItemFuncMgr::DoNewLuckDataTo(Player* player)
{
    std::string SendMsg;
    SendMsg.clear();

    auto [ItemId, Count] = PullItemAndCount();
    auto [TenItemId, TenCount] = PullTenItemAndCount();

    std::sort(LuckTempStore.begin(), LuckTempStore.end(), [](const LuckTemp& V1, const LuckTemp& V2) {return V1.ItemId < V2.ItemId; });
    for (auto Itr = LuckTempStore.begin(); Itr != LuckTempStore.end(); Itr++)
    {
        SendMsg.append(std::to_string(Itr->ItemId) + "#" + std::to_string(Itr->Count) + "|");
    }

    std::string ItemLink = sHelper->GetItemLink(ItemId);
    std::string TenItemLink = sHelper->GetItemLink(TenItemId);
    if (!SendMsg.empty())
        sDoAddon->SendPacketToClient(player, "DoNewLuckData", ItemLink + "$" + std::to_string(Count) + "$" + TenItemLink + "$" + std::to_string(TenCount) + "$" + SendMsg + "$");

    SendMsg.clear();
}

std::string ItemFuncMgr::GetCommands(uint32 ItemId)
{
    auto Itr = ItemCommandStore.find(ItemId);
    return (Itr != ItemCommandStore.end()) ? Itr->second.Cmd : "";
}

void ItemFuncMgr::DoOnLoginItemInStat(Player* player)
{
    if (IsXsPlayerbot(player))
        return;

    //主包
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; i++)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (IsItemInStat(pItem->GetEntry()))
                player->ApplyItemInStat(pItem, true);

    //额外四包
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
        if (Bag* pBag = player->GetBagByPos(i))
            for (uint32 j = 0; j < pBag->GetBagSize(); j++)
                if (Item* pItem = player->GetItemByPos(i, j))
                    if (IsItemInStat(pItem->GetEntry()))
                        player->ApplyItemInStat(pItem, true);

    //银行
    for (uint8 i = BANK_SLOT_ITEM_START; i < BANK_SLOT_BAG_END; ++i)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (IsItemInStat(pItem->GetEntry()))
                player->ApplyItemInStat(pItem, true);

    //银行包
    for (uint8 i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        if (Bag* pBag = player->GetBagByPos(i))
            for (uint32 j = 0; j < pBag->GetBagSize(); j++)
                if (Item* pItem = player->GetItemByPos(i, j))
                    if (IsItemInStat(pItem->GetEntry()))
                        player->ApplyItemInStat(pItem, true);


    player->SetHealth(player->GetMaxHealth());
}

Item* Player::AddItemSaveRandomSock(Item* OldItem, uint32 ItemId)
{
    if (!OldItem)
        return nullptr;

    uint32 Count = 1;
    uint32 SpaceForCount = 0;
    ItemPosCountVec Dest;
    InventoryResult Msg = CanStoreNewItem(NULL_BAG, NULL_SLOT, Dest, ItemId, Count, &SpaceForCount);
    if (Msg != EQUIP_ERR_OK)
        Count -= SpaceForCount;

    if (Count == 0 || Dest.empty())
        return nullptr;

    Item* OnItem = StoreNewItem(Dest, ItemId, true, "AddItemSaveRandomSock", Item::GenerateItemRandomPropertyId(ItemId));
    if (OnItem)
    {
        SendNewItem(OnItem, Count, true, false);

        for (uint32 EnSlot = PERM_ENCHANTMENT_SLOT; EnSlot < MAX_ENCHANTMENT_SLOT; ++EnSlot)
        {
            if (uint32 EnchantId = OldItem->GetEnchantmentId(EnchantmentSlot(EnSlot)))
            {
                uint32 Duration = OldItem->GetEnchantmentDuration(EnchantmentSlot(EnSlot));
                uint32 Charges = OldItem->GetEnchantmentCharges(EnchantmentSlot(EnSlot));
                OnItem->SetEnchantment(EnchantmentSlot(EnSlot), EnchantId, Duration, Charges, GetGUID());
            }
        }

        EnchantTemp Temp;
        Temp.EnchantId1 = OnItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0));
        Temp.EnchantId2 = OnItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_1));
        Temp.EnchantId3 = OnItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_2));
        Temp.EnchantId4 = OnItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_3));
        Temp.EnchantId5 = OnItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_4));
        EnchantAtStore[OnItem->GetGUIDLow()] = Temp;

        std::string SendMsg;
        SendMsg.clear();
        for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
        {
            uint32 EnchantId = OnItem->GetEnchantmentId(EnchantmentSlot(ESlot));
            SendMsg.append(std::to_string(EnchantId) + "#");
        }

        if (!SendMsg.empty())
        {
            sDoAddon->SendPacketToClient(this, "AutoEnchant", std::to_string(OnItem->GetGUIDLow()) + "$" + SendMsg);
            SendMsg.clear();
        }

        DestroyItem(OldItem->GetBagSlot(), OldItem->GetSlot(), true);

        return OnItem;
    }

    return nullptr;
}

Item* Player::AddNewSuperItemSaveRandomSock(Item* OldItem, uint32 ItemId)
{
    if (!OldItem)
        return nullptr;

    uint32 Count = 1;
    uint32 SpaceForCount = 0;
    ItemPosCountVec Dest;
    InventoryResult Msg = CanStoreNewItem(NULL_BAG, NULL_SLOT, Dest, ItemId, Count, &SpaceForCount);
    if (Msg != EQUIP_ERR_OK)
        Count -= SpaceForCount;

    if (Count == 0 || Dest.empty())
        return nullptr;

    Item* OnItem = StoreNewItem(Dest, ItemId, true, "AddNewSuperItemSaveRandomSock", Item::GenerateItemRandomPropertyId(ItemId));
    if (OnItem)
    {
        SendNewItem(OnItem, Count, true, false);

        for (uint32 EnSlot = PERM_ENCHANTMENT_SLOT; EnSlot < MAX_ENCHANTMENT_SLOT; ++EnSlot)
        {
            if (uint32 EnchantId = OldItem->GetEnchantmentId(EnchantmentSlot(EnSlot)))
            {
                uint32 Duration = OldItem->GetEnchantmentDuration(EnchantmentSlot(EnSlot));
                uint32 Charges = OldItem->GetEnchantmentCharges(EnchantmentSlot(EnSlot));
                OnItem->SetEnchantment(EnchantmentSlot(EnSlot), EnchantId, Duration, Charges, GetGUID());
            }
        }

        EnchantTemp Temp;
        Temp.EnchantId1 = OnItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0));
        Temp.EnchantId2 = OnItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_1));
        Temp.EnchantId3 = OnItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_2));
        Temp.EnchantId4 = OnItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_3));
        Temp.EnchantId5 = OnItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_4));
        EnchantAtStore[OnItem->GetGUIDLow()] = Temp;

        std::string SendMsg;
        SendMsg.clear();
        for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
        {
            uint32 EnchantId = OnItem->GetEnchantmentId(EnchantmentSlot(ESlot));
            SendMsg.append(std::to_string(EnchantId) + "#");
        }

        if (!SendMsg.empty())
        {
            sDoAddon->SendPacketToClient(this, "AutoEnchant", std::to_string(OnItem->GetGUIDLow()) + "$" + SendMsg);
            SendMsg.clear();
        }

        return OnItem;
    }

    return nullptr;
}

bool ItemFuncMgr::IsUpItem(uint32 ItemId)
{
    auto Itr = ItemUpRandomStore.find(ItemId);
    auto It = ItemUpChoiceTempStore.find(ItemId);
    return (Itr != ItemUpRandomStore.end() || It != ItemUpChoiceTempStore.end());
}

bool ItemFuncMgr::IsInItemUpRandom(Item* Tar)
{
    uint32 ItemId = Tar->GetEntry();
    auto Itr = ItemUpRandomStore.find(ItemId);
    return (Itr != ItemUpRandomStore.end());
}

bool ItemFuncMgr::IsInItemUpChoice(Item* Tar)
{
    uint32 ItemId = Tar->GetEntry();
    auto Itr = ItemUpChoiceTempStore.find(ItemId);
    return (Itr != ItemUpChoiceTempStore.end());
}

bool ItemFuncMgr::IsInItemTaskSock(Item* Tar)
{
    uint32 S1 = Tar->GetEnchantmentId(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT));
    uint32 S2 = Tar->GetEnchantmentId(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT_2));
    uint32 S3 = Tar->GetEnchantmentId(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT_3));
    return (S1 || S2 || S3);
}

bool ItemFuncMgr::IsSoulItemConfigStore(Player* player, uint32 ItemId)
{
    auto Itr = SoulItemConfigStore.find(ItemId);
    if (Itr == SoulItemConfigStore.end())
        return false;

    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetEntry() == ItemId)
                return false;

    return true;
}

uint32 ItemFuncMgr::GetSoulNTempId(uint32 ItemId)
{
    auto Itr = SoulItemConfigStore.find(ItemId);
    return Itr != SoulItemConfigStore.end() ? Itr->second : 0;
}

bool ItemFuncMgr::IsSoulItem(uint32 ItemGuidLow)
{
    auto Itr = SoulItemStore.find(ItemGuidLow);
    return Itr != SoulItemStore.end();
}

Item* ItemFuncMgr::GetEquipSoulItem(uint32 ItemGuidLow)
{
    auto Itr = SoulItemStore.find(ItemGuidLow);
    return Itr != SoulItemStore.end() ? Itr->second.DoItem : nullptr;
}

void ItemFuncMgr::ApplySoulItemData(uint32 ItemGuidLow, Item* DoItem, uint32 ItemEntry, uint32 OwnerLow, uint8 Slot)
{
    if (Slot >= SOUL_SLOT_END || !DoItem || DoItem->GetOwnerGUID().GetCounter() != OwnerLow)
        return;

    SoulItemStruct& Data = SoulItemStore[ItemGuidLow];
    Data.DoItem = DoItem;
    Data.ItemEntry = ItemEntry;
    Data.OwnerGuidLow = OwnerLow;
    Data.Slot = Slot;
}

void ItemFuncMgr::RemoveSoulItemData(uint32 ItemGuidLow)
{
    auto Itr = SoulItemStore.find(ItemGuidLow);
    if (Itr != SoulItemStore.end())
        SoulItemStore.erase(Itr);
}

void ItemFuncMgr::SaveEquipSoulItemToDB(CharacterDatabaseTransaction& Trans, uint32 OwnerLow, uint32 ItemEntry, uint32 ItemGuidLow, uint8 Slot)
{
    if (Slot >= SOUL_SLOT_END)
        return;

    Trans->Append("DELETE FROM _玩家双甲 WHERE 玩家编号 = {} AND 装备位置 = {} AND 物品标识 <> {};", OwnerLow, Slot, ItemGuidLow);
    Trans->Append("REPLACE INTO _玩家双甲 (物品标识, 物品编号, 玩家编号, 装备位置) VALUES ({}, {}, {}, {});", ItemGuidLow, ItemEntry, OwnerLow, Slot);
}

void ItemFuncMgr::RemoveSoulItemDB(uint32 ItemGuidLow)
{
    CharacterDatabase.Execute("DELETE FROM _玩家双甲 WHERE (物品标识 = {});", ItemGuidLow);
}

uint32 ItemFuncMgr::GetSoulItemEntry(uint32 OwnerLow, uint8 Slot)
{
    for (auto Itr = SoulItemStore.begin(); Itr != SoulItemStore.end(); ++Itr)
        if (Itr->second.OwnerGuidLow == OwnerLow && Itr->second.Slot == Slot)
            return Itr->second.ItemEntry;

    return 0;
}

uint32 ItemFuncMgr::GetSoulItemGuid(uint32 OwnerLow, uint8 Slot)
{
    for (auto Itr = SoulItemStore.begin(); Itr != SoulItemStore.end(); ++Itr)
        if (Itr->second.OwnerGuidLow == OwnerLow && Itr->second.Slot == Slot)
            return Itr->first;

    return 0;
}

std::pair<AsInt64, AsInt64> ItemFuncMgr::GetSoulWeaponDamageRange(Player* player)
{
    if (!player)
        return { AsInt64Zero(), AsInt64Zero() };

    // The client exposes one soul weapon button as slot 17; its zero-based server slot is 16.
    uint32 itemGuidLow = GetSoulItemGuid(player->GetGUIDLow(), 16);
    Item* item = GetEquipSoulItem(itemGuidLow);
    if (!item || item->IsBroken())
        return { AsInt64Zero(), AsInt64Zero() };

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto || proto->Class != ITEM_CLASS_WEAPON)
        return { AsInt64Zero(), AsInt64Zero() };

    AsInt64 minDamage = AsInt64Zero();
    AsInt64 maxDamage = AsInt64Zero();
    if (sItemRandomAtMgr->IsGenerateItemAt(proto->ItemId))
    {
        minDamage = item->AsMinWDmg > 0 ? item->AsMinWDmg : AsInt64(item->MinWDmg);
        maxDamage = item->AsMaxWDmg > 0 ? item->AsMaxWDmg : AsInt64(item->MaxWDmg);
        if (minDamage <= 0)
            minDamage = proto->AsDamageMin[0] > 0 ? proto->AsDamageMin[0] : AsInt64(uint64(std::max(0.0f, proto->Damage[0].DamageMin)));
        if (maxDamage <= 0)
            maxDamage = proto->AsDamageMax[0] > 0 ? proto->AsDamageMax[0] : AsInt64(uint64(std::max(0.0f, proto->Damage[0].DamageMax)));

        float rankMod = sItemRandomAtMgr->GetModForRank(proto->ItemId, item->RmAtLevel);
        minDamage = ScaleAsInt64ByDouble(minDamage, 1.0 + double(rankMod));
        maxDamage = ScaleAsInt64ByDouble(maxDamage, 1.0 + double(rankMod));
    }
    else
    {
        minDamage = proto->AsDamageMin[0] > 0 ? proto->AsDamageMin[0] : AsInt64(uint64(std::max(0.0f, proto->Damage[0].DamageMin)));
        maxDamage = proto->AsDamageMax[0] > 0 ? proto->AsDamageMax[0] : AsInt64(uint64(std::max(0.0f, proto->Damage[0].DamageMax)));
    }

    float equipmentMod = sPlayerModMgr->GetModPlayerEqSlot(player, EQUIPMENT_SLOT_MAINHAND);
    minDamage = ScaleAsInt64ByDouble(minDamage, 1.0 + double(equipmentMod));
    maxDamage = ScaleAsInt64ByDouble(maxDamage, 1.0 + double(equipmentMod));

    double soulMod = double(GetSoulItemPtData(player, proto->ItemId));
    minDamage = ScaleAsInt64ByDouble(minDamage, soulMod);
    maxDamage = ScaleAsInt64ByDouble(maxDamage, soulMod);

    if (minDamage <= 0)
        minDamage = maxDamage;
    if (maxDamage <= 0)
        maxDamage = minDamage;
    if (maxDamage < minDamage)
        std::swap(minDamage, maxDamage);

    return { minDamage, maxDamage };
}

void ItemFuncMgr::RefreshSoulItemStats(Player* player)
{
    if (!player)
        return;

    player->SetCanModifyStats(true);
    player->UpdateAllStats();
    player->UpdateAttackPowerAndDamage(false);
    sDoAddon->SendPacketToClient(player, "PATTRPANEL", "REFRESH");
}

void ItemFuncMgr::SendClientSoulData(Player* player)
{
    uint32 OwnerLow = player->GetGUIDLow();
    for (auto Itr = SoulItemStore.begin(); Itr != SoulItemStore.end(); ++Itr)
    {
        if (Itr->second.OwnerGuidLow == OwnerLow)
            sDoAddon->SendPacketToClient(player, "LoginAutoSoulItemData", std::to_string(Itr->second.Slot + 1) + "#" + std::to_string(Itr->first) + "#" + sHelper->GetNItemLink(Itr->second.DoItem) + "#");
    }
}

void ItemFuncMgr::SendClientSoulOneData(Player* player, Item* DoItem, uint8 Slot)
{
    uint32 ItemGuidLow = DoItem->GetGUIDLow();
    sDoAddon->SendPacketToClient(player, "AutoSoulItemData", std::to_string(Slot + 1) + "#" + std::to_string(ItemGuidLow) + "#" + sHelper->GetNItemLink(DoItem) + "#");
}

void ItemFuncMgr::SendClientSoulTaskOneData(Player* player, uint8 Slot)
{
    sDoAddon->SendPacketToClient(player, "TaskSoulItemNull", std::to_string(Slot + 1));
}

std::string ItemFuncMgr::GetSoulSlotName(uint8 Slot)
{
    switch (Slot)
    {
    case 0: return  "头部";
    case 1: return  "项链";
    case 2: return  "肩膀";
    case 3: return  "披风";
    case 4: return  "胸甲";
    case 5: return  "衬衣";
    case 6: return  "战袍";
    case 7: return  "手腕";
    case 8: return  "手套";
    case 9: return  "腰带";
    case 10: return  "腿部";
    case 11: return  "鞋子";
    case 12: return  "上戒";
    case 13: return  "下戒";
    case 14: return  "上饰";
    case 15: return  "下饰";
    case 16: return  "武器";
    default: return "";
    }
}

std::string ItemFuncMgr::GetSoulSlotIcon(uint8 Slot, uint32 W, uint32 H, int X, int Y)
{
    std::ostringstream Oss;
    Oss << "|TInterface/PaperDoll/";
    switch (Slot)
    {
    case 0: Oss << "UI-PaperDoll-Slot-Head"; break;
    case 1: Oss << "UI-PaperDoll-Slot-Neck"; break;
    case 2: Oss << "UI-PaperDoll-Slot-Shoulder"; break;
    case 3: Oss << "UI-Backpack-EmptySlot"; break;
    case 4: Oss << "UI-PaperDoll-Slot-Chest"; break;
    case 5: Oss << "UI-PaperDoll-Slot-Shirt"; break;
    case 6: Oss << "UI-PaperDoll-Slot-Tabard"; break;
    case 7: Oss << "UI-PaperDoll-Slot-Wrists"; break;
    case 8: Oss << "UI-PaperDoll-Slot-Hands"; break;
    case 9: Oss << "UI-PaperDoll-Slot-Waist"; break;
    case 10: Oss << "UI-PaperDoll-Slot-Legs"; break;
    case 11: Oss << "UI-PaperDoll-Slot-Feet"; break;
    case 12: Oss << "UI-PaperDoll-Slot-Finger"; break;
    case 13: Oss << "UI-PaperDoll-Slot-RFinger"; break;
    case 14: Oss << "UI-PaperDoll-Slot-REar"; break;
    case 15: Oss << "UI-PaperDoll-Slot-Trinket"; break;
    case 16: Oss << "UI-PaperDoll-Slot-MainHand"; break;

    default: Oss << "UI-Backpack-EmptySlot";
    }
    Oss << ":" << W << ":" << H << ":" << X << ":" << Y << "|t";
    return Oss.str();
}

uint8 ItemFuncMgr::GetOriginalSlot(uint8 CustomSlot)
{
    switch (CustomSlot)
    {
    case 0: return EQUIPMENT_SLOT_HEAD;                           //头部
    case 1: return EQUIPMENT_SLOT_NECK;                           //项链
    case 2: return EQUIPMENT_SLOT_SHOULDERS;                //肩膀
    case 3: return EQUIPMENT_SLOT_BACK;                           //背部
    case 4: return EQUIPMENT_SLOT_CHEST;                         //胸部
    case 5: return EQUIPMENT_SLOT_BODY;                          //衬衣
    case 6: return EQUIPMENT_SLOT_TABARD;                      //战袍
    case 7: return EQUIPMENT_SLOT_WRISTS;                       //手腕
    case 8: return EQUIPMENT_SLOT_HANDS;                       //手套
    case 9: return EQUIPMENT_SLOT_WAIST;                         //腰带
    case 10: return EQUIPMENT_SLOT_LEGS;                         //裤子
    case 11: return EQUIPMENT_SLOT_FEET;                          //鞋子
    case 12: return EQUIPMENT_SLOT_FINGER1;                   //上戒
    case 13: return EQUIPMENT_SLOT_FINGER2;                   //下戒
    case 14: return EQUIPMENT_SLOT_TRINKET1;                  //上饰
    case 15: return EQUIPMENT_SLOT_TRINKET2;                  //下饰
    case 16: return EQUIPMENT_SLOT_MAINHAND;              //武器
    }
    return EQUIPMENT_SLOT_START;
}

bool ItemFuncMgr::CanEquipSoul(Player* player, Item* NewItem, uint8 Slot)
{
    if (!player || !NewItem || Slot >= SOUL_SLOT_END || NewItem->GetOwnerGUID() != player->GetGUID())
        return false;

    if (!IsSoulItemConfigStore(player, NewItem->GetEntry()))
        return false;

    ItemTemplate const* NewTemplate = NewItem->GetTemplate();
    if (!NewTemplate)
        return false;

    if (NewItem->IsBroken() || NewItem->IsEquipped())
        return false;

    if (NewTemplate->Class != ITEM_CLASS_ARMOR && NewTemplate->Class != ITEM_CLASS_WEAPON)
        return false;

    if (NewTemplate->InventoryType == INVTYPE_BAG)
        return false;

    if (!SuitableForEquipSou(player, NewTemplate))
        return false;

    /*if (HasItemTotemCategory(player, NewTemplate))
        return false;*/

    const bool WeaponMask = (
        NewTemplate->InventoryType == INVTYPE_WEAPON
        || NewTemplate->InventoryType == INVTYPE_2HWEAPON
        || NewTemplate->InventoryType == INVTYPE_WEAPONMAINHAND
        || NewTemplate->InventoryType == INVTYPE_WEAPONOFFHAND
        || NewTemplate->InventoryType == INVTYPE_SHIELD
        || NewTemplate->InventoryType == INVTYPE_HOLDABLE
        || NewTemplate->InventoryType == INVTYPE_RANGED
        || NewTemplate->InventoryType == INVTYPE_RANGEDRIGHT
        || NewTemplate->InventoryType == INVTYPE_THROWN
        );

    switch (Slot)
    {
    case 0:
        return (NewTemplate->InventoryType == INVTYPE_HEAD);
        break;
    case 1:
        return (NewTemplate->InventoryType == INVTYPE_NECK);
        break;
    case 2:
        return (NewTemplate->InventoryType == INVTYPE_SHOULDERS);
        break;
    case 3:
        return (NewTemplate->InventoryType == INVTYPE_CLOAK);
        break;
    case 4:
        return (NewTemplate->InventoryType == INVTYPE_CHEST || NewTemplate->InventoryType == INVTYPE_ROBE);
        break;
    case 5:
        return (NewTemplate->InventoryType == INVTYPE_BODY);
        break;
    case 6:
        return (NewTemplate->InventoryType == INVTYPE_TABARD);
        break;
    case 7:
        return (NewTemplate->InventoryType == INVTYPE_WRISTS);
        break;
    case 8:
        return (NewTemplate->InventoryType == INVTYPE_HANDS);
        break;
    case 9:
        return (NewTemplate->InventoryType == INVTYPE_WAIST);
        break;
    case 10:
        return (NewTemplate->InventoryType == INVTYPE_LEGS);
        break;
    case 11:
        return (NewTemplate->InventoryType == INVTYPE_FEET);
        break;
    case 12:
        return (NewTemplate->InventoryType == INVTYPE_FINGER);
        break;
    case 13:
        return (NewTemplate->InventoryType == INVTYPE_FINGER);
        break;
    case 14:
        return (NewTemplate->InventoryType == INVTYPE_TRINKET);
        break;
    case 15:
        return (NewTemplate->InventoryType == INVTYPE_TRINKET);
        break;
    case 16:
        return (WeaponMask);
        break;
    }
    return false;
}

bool ItemFuncMgr::SuitableForEquipSou(Player* player, ItemTemplate const* Proto)
{
    if (Proto->SubClass > 0 && player->GetSkillValue(Proto->GetSkill()) == 0)
        return false;

    if ((Proto->Flags2 & ITEM_FLAG2_FACTION_HORDE) && player->GetTeamId() != TEAM_HORDE)
        return false;

    if ((Proto->Flags2 & ITEM_FLAG2_FACTION_ALLIANCE) && player->GetTeamId() != TEAM_ALLIANCE)
        return false;

    if ((Proto->AllowableClass & player->GetClassMask()) == 0)
        return false;

    if ((Proto->AllowableRace & player->GetRaceMask()) == 0)
        return false;

    if (Proto->RequiredSkill != 0)
    {
        if (player->GetSkillValue(Proto->RequiredSkill) == 0)
            return false;
        else if (player->GetSkillValue(Proto->RequiredSkill) < Proto->RequiredSkillRank)
            return false;
    }

    if (Proto->RequiredSpell != 0 && !player->HasSpell(Proto->RequiredSpell))
        return false;

    if (player->GetLevel() < Proto->RequiredLevel)
        return false;

    return true;
}

void ItemFuncMgr::ApplySoulItemStatsAndSpells(Player* player, Item* DoItem, uint8 Slot, bool Apply)
{
    ItemTemplate const* Proto = DoItem->GetTemplate();
    if (!Proto)
        return;

    if (Apply)
        AddItemsSetItem(player, DoItem);
    else
        RemoveItemsSetItem(player, Proto);

    if (DoItem->HasSocket())
        player->CorrectMetaGemEnchants(Slot, Apply);

    player->_ApplySoulItemBonuses(DoItem, Apply);

    sItemRandomAtMgr->OnApplyItemAtStoreStat(player, DoItem, Slot, Apply);

    player->ApplySoulItemEquipSpell(DoItem, Apply);

    sItemRandomAtMgr->OnApplyItemAtStoreSpell(player, DoItem, Apply);
    sItemRunesMgr->OnApplyItemRunesSpell(player, DoItem, Apply);

    player->ApplySoulEnchantment(DoItem, Apply);
}

void ItemFuncMgr::UpdateSoulItem(Player* player, Item* NewItem, uint8 Slot)
{
    if (!CanEquipSoul(player, NewItem, Slot))
        return;

    if (NewItem)
    {
        uint32 ItemGuidLow = NewItem->GetGUIDLow();
        CharacterDatabaseTransaction Trans = CharacterDatabase.BeginTransaction();
        uint32 OGuidLow = GetSoulItemGuid(player->GetGUIDLow(), Slot);

        if (Item* OItem = GetEquipSoulItem(OGuidLow))
        {
            OItem->SetOwnerGUID(player->GetGUID());
            ApplySoulItemStatsAndSpells(player, OItem, GetOriginalSlot(Slot), false);
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

            RemoveSoulItemData(OGuidLow);
            RemoveSoulItemDB(OGuidLow);
        }

        SaveEquipSoulItemToDB(Trans, player->GetGUIDLow(), NewItem->GetEntry(), NewItem->GetGUIDLow(), Slot);
        ApplySoulItemData(ItemGuidLow, NewItem, NewItem->GetEntry(), player->GetGUIDLow(), Slot);
        ApplySoulItemStatsAndSpells(player, NewItem, GetOriginalSlot(Slot), true);
        player->MoveItemFromInventory(NewItem->GetBagSlot(), NewItem->GetSlot(), true);
        NewItem->SetOwnerGUID(player->GetGUID());
        NewItem->DeleteFromInventoryDB(Trans);
        NewItem->SaveToDB(Trans);
        player->SaveInventoryAndGoldToDB(Trans);
        CharacterDatabase.CommitTransaction(Trans);

        RefreshSoulItemStats(player);
    }
}

void ItemFuncMgr::AutoSoulItemStatsAndSpells(Player* player)
{
    if (IsXsPlayerbot(player))
        return;

    bool hasSoulWeapon = false;
    for (uint8 Index = SOUL_SLOT_START; Index < SOUL_SLOT_END; ++Index)
    {
        uint32 LGuid = GetSoulItemGuid(player->GetGUIDLow(), Index);
        if (!LGuid)
            continue;

        if (Item* OItem = GetEquipSoulItem(LGuid))
        {
            ApplySoulItemStatsAndSpells(player, OItem, GetOriginalSlot(Index), true);
            hasSoulWeapon = hasSoulWeapon || Index == 16;
        }
    }

    if (hasSoulWeapon)
        player->UpdateAttackPowerAndDamage(false);
}

float ItemFuncMgr::GetSoulItemPtData(Player* player, uint32 ItemId)
{
    if (!IsSoulItemConfigStore(player, ItemId))
        return 1.0f;

    return sHelper->GetFloat(13501);
}

bool ItemFuncMgr::HasItemTotemCategory(Player* player, ItemTemplate const* NewTemplate)
{
    bool In = false;
    for (uint8 Index = SOUL_SLOT_START; Index < SOUL_SLOT_END; ++Index)
    {
        uint32 ItemId = GetSoulItemEntry(player->GetGUIDLow(), Index);
        ItemTemplate const* NTemplate = sObjectMgr->GetItemTemplate(ItemId);
        if (NTemplate)
            if (!player->IsTotemCategoryCompatiableWith(NTemplate, NewTemplate->TotemCategory))
                In = true;
    }
    return In;
}

/*仅仅用作物品出售购买*/
void ItemFuncMgr::SendItemVsAccept(Player* player, std::string Text, uint32 SendId)
{
    WorldPacket Packet(SMSG_QUEST_CONFIRM_ACCEPT, (4 + Text.size() + 8));
    Packet << uint32(SendId);
    Packet << Text;
    Packet << player->GetGUID().GetRawValue();
    player->GetSession()->SendPacket(&Packet);
}

bool ItemFuncMgr::IsItemVsInfo(int32 ItemId)
{
    bool In = false;
    for (auto Itr = ItemVsInfoStore.begin(); Itr != ItemVsInfoStore.end(); Itr++)
        if (Itr->ItemId == ItemId)
            In = true;

    return In;
}

uint32 ItemFuncMgr::GetNTempId(int32 ItemId, uint32 VendorId)
{
    struct AtTemp { uint32 VEntry; uint32 TempId; };
    std::vector<AtTemp> AtStore;
    AtStore.clear();

    for (auto Itr = ItemVsInfoStore.begin(); Itr != ItemVsInfoStore.end(); Itr++)
    {
        if (Itr->ItemId == ItemId)
        {
            AtTemp St;
            St.VEntry = Itr->VendorId;
            St.TempId = Itr->NTempId;
            AtStore.push_back(St);
        }
    }

    if (AtStore.empty())
        return 0;

    std::sort(AtStore.begin(), AtStore.end(), [](const AtTemp& V1, const AtTemp& V2) {return V1.VEntry > V2.VEntry; });
    for (auto Itr = AtStore.begin(); Itr != AtStore.end(); Itr++)
    {
        if (Itr->VEntry == VendorId)
            return Itr->TempId;
        else if (Itr->VEntry == 0)
            return Itr->TempId;
    }

    AtStore.clear();
    return 0;
}

uint32 ItemFuncMgr::GetRTempId(int32 ItemId, uint32 VEntry, bool Sort)
{
    struct AtTemp { uint32 VEntry; uint32 TempId; };
    std::vector<AtTemp> AtStore;
    AtStore.clear();

    for (auto Itr = ItemVsInfoStore.begin(); Itr != ItemVsInfoStore.end(); Itr++)
    {
        if (Itr->ItemId == ItemId)
        {
            AtTemp St;
            St.VEntry = Itr->VendorId;
            St.TempId = Itr->RTempId;
            AtStore.push_back(St);
        }
    }

    if (AtStore.empty())
        return 0;

    if (Sort)
        std::sort(AtStore.begin(), AtStore.end(), [](const AtTemp& V1, const AtTemp& V2) {return V1.VEntry > V2.VEntry; });

    for (auto Itr = AtStore.begin(); Itr != AtStore.end(); Itr++)
    {
        if (Itr->VEntry == VEntry)
            return Itr->TempId;
        else if (Itr->VEntry == 0)
            return Itr->TempId;
    }

    AtStore.clear();
    return 0;
}

bool ItemFuncMgr::DoItemVsAccept(Player* player, uint32 SendId)
{
    if (SendId)
        return false;

    uint32 BItem = player->BuyItemId;
    uint32 BCount = player->BuyCount;
    uint32 NTempId = GetNTempId(BItem, player->GetSession()->GetVEntry());

    ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(BItem);
    if (!pProto)
        return true;

    if (pProto->MaxCount > 0)
    {
        if (player->HasItemCount(pProto->ItemId, pProto->MaxCount, true))
        {
            player->GetSession()->SendNotification(sHelper->GetGts(1080));
            return true;
        }
    }

    if (sHelper->CheckNeed(player, NTempId, "商店购买消耗", BCount))
    {
        player->AddItem(BItem, BCount, "商店购买获得");
        return true;
    }

    return true;
}

bool ItemFuncMgr::DoTheSellItem(Player* player, uint32 VEntry, int32 ItemId, uint32 Count)
{
    uint32 RTempId = GetRTempId(ItemId, VEntry, true);
    if (!RTempId)
        return false;

    sHelper->DoTheReward(player, RTempId, "出售物品奖励", Count);
    return true;
}

uint32 ItemFuncMgr::GetCustomEnchNTempId(int32 ItemId)
{
    auto Itr = ItemEnchStore.find(ItemId);
    return Itr != ItemEnchStore.end() ? Itr->second.NTempId : 0;
}

bool ItemFuncMgr::DoUseCustomEnch(Item* NewItem)
{
    auto Itr = ItemEnchStore.find(NewItem->GetEntry());
    if (Itr == ItemEnchStore.end())
        return false;

    if (roll_chance_f(Itr->second.EnchSlot1))
        DoRandomEnch(NewItem, Itr->second.EnchGroup1, PROP_ENCHANTMENT_SLOT_0);
    if (roll_chance_f(Itr->second.EnchSlot2))
        DoRandomEnch(NewItem, Itr->second.EnchGroup2, PROP_ENCHANTMENT_SLOT_1);
    if (roll_chance_f(Itr->second.EnchSlot3))
        DoRandomEnch(NewItem, Itr->second.EnchGroup3, PROP_ENCHANTMENT_SLOT_2);
    if (roll_chance_f(Itr->second.EnchSlot4))
        DoRandomEnch(NewItem, Itr->second.EnchGroup4, PROP_ENCHANTMENT_SLOT_3);
    if (roll_chance_f(Itr->second.EnchSlot5))
        DoRandomEnch(NewItem, Itr->second.EnchGroup5, PROP_ENCHANTMENT_SLOT_4);

    EnchantTemp Temp;
    Temp.EnchantId1 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0));
    Temp.EnchantId2 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_1));
    Temp.EnchantId3 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_2));
    Temp.EnchantId4 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_3));
    Temp.EnchantId5 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_4));
    EnchantAtStore[NewItem->GetGUIDLow()] = Temp;

    std::string SendMsg;
    SendMsg.clear();
    for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
    {
        uint32 EnchantId = NewItem->GetEnchantmentId(EnchantmentSlot(ESlot));
        SendMsg.append(std::to_string(EnchantId) + "#");
    }

    if (!SendMsg.empty() && NewItem->GetOwner())
    {
        sDoAddon->SendPacketToClient(NewItem->GetOwner(), "AutoEnchant", std::to_string(NewItem->GetGUIDLow()) + "$" + SendMsg);
        SendMsg.clear();
    }

    return true;
}

void ItemFuncMgr::DoUseCustomEnchOne(Item* NewItem, EnchantmentSlot EnchSlot)
{
    auto Itr = ItemEnchStore.find(NewItem->GetEntry());
    if (Itr == ItemEnchStore.end())
        return;

    uint32 EnchGroup = Itr->second.EnchGroup1;
    switch (EnchSlot)
    {
    case PROP_ENCHANTMENT_SLOT_0:
        EnchGroup = Itr->second.EnchGroup1;
        break;
    case PROP_ENCHANTMENT_SLOT_1:
        EnchGroup = Itr->second.EnchGroup2;
        break;
    case PROP_ENCHANTMENT_SLOT_2:
        EnchGroup = Itr->second.EnchGroup3;
        break;
    case PROP_ENCHANTMENT_SLOT_3:
        EnchGroup = Itr->second.EnchGroup4;
        break;
    case PROP_ENCHANTMENT_SLOT_4:
        EnchGroup = Itr->second.EnchGroup5;
        break;
    }

    DoRandomEnch(NewItem, EnchGroup, EnchSlot);

    EnchantTemp Temp;
    Temp.EnchantId1 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0));
    Temp.EnchantId2 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_1));
    Temp.EnchantId3 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_2));
    Temp.EnchantId4 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_3));
    Temp.EnchantId5 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_4));
    EnchantAtStore[NewItem->GetGUIDLow()] = Temp;

    std::string SendMsg;
    SendMsg.clear();
    for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
    {
        uint32 EnchantId = NewItem->GetEnchantmentId(EnchantmentSlot(ESlot));
        SendMsg.append(std::to_string(EnchantId) + "#");
    }

    if (!SendMsg.empty() && NewItem->GetOwner())
    {
        sDoAddon->SendPacketToClient(NewItem->GetOwner(), "AutoEnchant", std::to_string(NewItem->GetGUIDLow()) + "$" + SendMsg);
        SendMsg.clear();
    }
}

void ItemFuncMgr::DoRandomEnchSwitchOne(Player* player, Item* NewItem, Item* OldItem, EnchantmentSlot EnchSlot)
{
    if (EnchSlot >= MAX_ENCHANTMENT_SLOT)
        return;
    uint32 Ench = OldItem->GetEnchantmentId(EnchSlot);
    NewItem->SetEnchantment(EnchSlot, Ench, 0, 0);
    OldItem->ClearEnchantment(EnchSlot);

    EnchantTemp Temp1;
    Temp1.EnchantId1 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0));
    Temp1.EnchantId2 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_1));
    Temp1.EnchantId3 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_2));
    Temp1.EnchantId4 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_3));
    Temp1.EnchantId5 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_4));
    EnchantAtStore[NewItem->GetGUIDLow()] = Temp1;

    std::string SendMsg1;
    SendMsg1.clear();
    for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
    {
        uint32 EnchantId = NewItem->GetEnchantmentId(EnchantmentSlot(ESlot));
        SendMsg1.append(std::to_string(EnchantId) + "#");
    }

    if (!SendMsg1.empty())
    {
        sDoAddon->SendPacketToClient(player, "AutoEnchant", std::to_string(NewItem->GetGUIDLow()) + "$" + SendMsg1);
        SendMsg1.clear();
    }

    EnchantTemp Temp2;
    Temp2.EnchantId1 = OldItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0));
    Temp2.EnchantId2 = OldItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_1));
    Temp2.EnchantId3 = OldItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_2));
    Temp2.EnchantId4 = OldItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_3));
    Temp2.EnchantId5 = OldItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_4));
    EnchantAtStore[OldItem->GetGUIDLow()] = Temp2;

    std::string SendMsg2;
    SendMsg2.clear();
    for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
    {
        uint32 EnchantId = OldItem->GetEnchantmentId(EnchantmentSlot(ESlot));
        SendMsg2.append(std::to_string(EnchantId) + "#");
    }

    if (!SendMsg2.empty())
    {
        sDoAddon->SendPacketToClient(player, "AutoEnchant", std::to_string(OldItem->GetGUIDLow()) + "$" + SendMsg2);
        SendMsg2.clear();
    }
}

void ItemFuncMgr::SendUpdateShowEnchInfo1(Player* player, Item* OnItem)
{
    uint8 Num = 0;
    for (uint32 i = PROP_ENCHANTMENT_SLOT_0; i < MAX_ENCHANTMENT_SLOT; ++i)
    {
        Num++;
        uint32 EnchId = OnItem->GetEnchantmentId(EnchantmentSlot(i));
        std::string EnchDes = sHelper->GetEnchName(EnchId);
        sDoAddon->SendPacketToClient(player, "ShowEnchInfo1", std::to_string(Num) + "#" + std::to_string(EnchId) + "#" + EnchDes);
    }
}

void ItemFuncMgr::SendUpdateShowEnchInfo2(Player* player, Item* OnItem)
{
    uint8 Num = 0;
    for (uint32 i = PROP_ENCHANTMENT_SLOT_0; i < MAX_ENCHANTMENT_SLOT; ++i)
    {
        Num++;
        uint32 EnchId = OnItem->GetEnchantmentId(EnchantmentSlot(i));
        std::string EnchDes = sHelper->GetEnchName(EnchId);
        sDoAddon->SendPacketToClient(player, "ShowEnchInfo2", std::to_string(Num) + "#" + std::to_string(EnchId) + "#" + EnchDes);
    }
}

void ItemFuncMgr::DoUseCustomEnchOneNoPacket(Item* NewItem, EnchantmentSlot EnchSlot)
{
    auto Itr = ItemEnchStore.find(NewItem->GetEntry());
    if (Itr == ItemEnchStore.end())
        return;

    uint32 EnchGroup = Itr->second.EnchGroup1;
    switch (EnchSlot)
    {
    case PROP_ENCHANTMENT_SLOT_0:
        EnchGroup = Itr->second.EnchGroup1;
        break;
    case PROP_ENCHANTMENT_SLOT_1:
        EnchGroup = Itr->second.EnchGroup2;
        break;
    case PROP_ENCHANTMENT_SLOT_2:
        EnchGroup = Itr->second.EnchGroup3;
        break;
    case PROP_ENCHANTMENT_SLOT_3:
        EnchGroup = Itr->second.EnchGroup4;
        break;
    case PROP_ENCHANTMENT_SLOT_4:
        EnchGroup = Itr->second.EnchGroup5;
        break;
    }

    DoRandomEnch(NewItem, EnchGroup, EnchSlot);

    EnchantTemp Temp;
    Temp.EnchantId1 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_0));
    Temp.EnchantId2 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_1));
    Temp.EnchantId3 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_2));
    Temp.EnchantId4 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_3));
    Temp.EnchantId5 = NewItem->GetEnchantmentId(EnchantmentSlot(PROP_ENCHANTMENT_SLOT_4));
    EnchantAtStore[NewItem->GetGUIDLow()] = Temp;
}

void ItemFuncMgr::DoRandomEnch(Item* NewItem, uint32 Group, EnchantmentSlot EnchSlot)
{
    auto Iter = EnchGroupStore.find(Group);
    if (Iter == EnchGroupStore.end())
        return;

    float RIdx = 0.0f;
    for (auto It = Iter->second.begin(); It != Iter->second.end(); It++)
        RIdx += It->Chance;

    for (auto It = Iter->second.begin(); It != Iter->second.end(); It++)
    {
        if (frand(0, RIdx) <= It->Chance)
        {
            NewItem->SetEnchantment(EnchSlot, It->EnchId, 0, 0);
            break;
        }
        else
            RIdx -= It->Chance;
    }
}

std::string ItemFuncMgr::GetDesForEnchant(uint32 EnchantId)
{
    if (!EnchantId)
        return "0";

    SpellItemEnchantmentEntry const* EnchantEntry = sSpellItemEnchantmentStore.LookupEntry(EnchantId);
    if (!EnchantEntry)
        return "0";

    return std::string(EnchantEntry->description[LOCALE_zhCN]);
}

void ItemFuncMgr::DeleteItemFuncFromDB(uint32 ItemGuldLow, CharacterDatabaseTransaction* Trans)
{

}

bool ItemFuncMgr::IsNotFuncRecovery()
{
    return ItemRyStore.empty();
}

bool ItemFuncMgr::IsItemRecovery(uint32 ItemId, uint32 Menu)
{
    auto Itr = ItemRyStore.find(ItemId);
    return Itr != ItemRyStore.end() && Itr->second.Menu == Menu;
}

void ItemFuncMgr::DoItemRecovery(Player* player, uint32 Menu)
{
    std::vector<Item*> List;
    List.clear();

    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            List.push_back(pItem);

    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Bag* pBag = player->GetBagByPos(i))
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                if (Item* pItem = pBag->GetItemByPos(j))
                    List.push_back(pItem);

    int64 AddToken = 0;
    for (auto const& Item : List)
    {
        if (!sHelper->GetBoolean(20005) && Item->RmAtLevel > 0)
            continue;

        uint32 ItemId = Item->GetEntry();
        uint32 Count = Item->GetCount();
        if (!IsItemRecovery(ItemId, Menu))
            continue;

        auto Itr = ItemRyStore.find(ItemId);
        if (Itr == ItemRyStore.end())
            continue;

        if (Itr->second.RwItemId1 > 0 && Itr->second.RwCount1 > 0)
            player->AddItem(Itr->second.RwItemId1, Itr->second.RwCount1 * Count, "一键回收获得");
        if (Itr->second.RwItemId2 > 0 && Itr->second.RwCount2 > 0)
            player->AddItem(Itr->second.RwItemId2, Itr->second.RwCount2 * Count, "一键回收获得");
        if (Itr->second.RwItemId3 > 0 && Itr->second.RwCount3 > 0)
            player->AddItem(Itr->second.RwItemId3, Itr->second.RwCount3 * Count, "一键回收获得");
        if (Itr->second.RwItemId4 > 0 && Itr->second.RwCount4 > 0)
            player->AddItem(Itr->second.RwItemId4, Itr->second.RwCount4 * Count, "一键回收获得");
        if (Itr->second.RwItemId5 > 0 && Itr->second.RwCount5 > 0)
            player->AddItem(Itr->second.RwItemId5, Itr->second.RwCount5 * Count, "一键回收获得");
        if (Itr->second.RwToken > 0)
            AddToken += int64(Itr->second.RwToken) * Count;

        player->DestroyItemCount(ItemId, Count, true);
    }

    if (AddToken > 0)
        player->AddToken(AddToken, "一键回收奖励");

    List.clear();
}

bool ItemFuncMgr::IsHasRecovery(Player* player, uint32 Menu)
{
    std::vector<Item*> List;
    List.clear();

    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            List.push_back(pItem);

    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Bag* pBag = player->GetBagByPos(i))
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                if (Item* pItem = pBag->GetItemByPos(j))
                    List.push_back(pItem);

    bool Is = false;
    for (auto const& Item : List)
    {
        uint32 ItemId = Item->GetEntry();
        if (IsItemRecovery(ItemId, Menu))
        {
            Is = true;
            break;
        }
    }

    List.clear();
    return Is;
}

/*加载上限*/
void Player::InitLimitItem()
{
    if (IsXsPlayerbot(this))
        return;

    uint32 GuidLow = GetGUIDLow();
    CharacterDatabasePreparedStatement* CStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_LIMIT);
    CStmt->SetData(0, GuidLow);
    PreparedQueryResult CResult = CharacterDatabase.Query(CStmt);
    if (CResult)
    {
        do
        {
            Field* Fields = CResult->Fetch();
            PlayerDayItemLimitMap[Fields[0].Get<uint32>()] = Fields[1].Get<uint32>();
        } while (CResult->NextRow());
    }

    CStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_FR_LIMIT);
    CStmt->SetData(0, GuidLow);
    CResult = CharacterDatabase.Query(CStmt);
    if (CResult)
    {
        do
        {
            Field* Fields = CResult->Fetch();
            PlayerFrItemLimitMap[Fields[0].Get<uint32>()] = Fields[1].Get<uint32>();
        } while (CResult->NextRow());
    }

    uint32 AccountId = GetSession()->GetAccountId();
    LoginDatabasePreparedStatement* LStmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ITEM_LIMIT);
    LStmt->SetData(0, AccountId);
    PreparedQueryResult LResult = LoginDatabase.Query(LStmt);
    if (LResult)
    {
        do
        {
            Field* Fields = LResult->Fetch();
            PlayerAccDayItemLimitMap[Fields[0].Get<uint32>()] = Fields[1].Get<uint32>();
        } while (LResult->NextRow());
    }

    LStmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ITEM_FR_LIMIT);
    LStmt->SetData(0, AccountId);
    LResult = LoginDatabase.Query(LStmt);
    if (LResult)
    {
        do
        {
            Field* Fields = LResult->Fetch();
            PlayerAccFrItemLimitMap[Fields[0].Get<uint32>()] = Fields[1].Get<uint32>();
        } while (LResult->NextRow());
    }
}

void Player::InitUseLimitItem()
{
    if (IsXsPlayerbot(this))
        return;

    uint32 GuidLow = GetGUIDLow();
    CharacterDatabasePreparedStatement* CStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_USE_ITEM_LIMIT);
    CStmt->SetData(0, GuidLow);
    PreparedQueryResult CResult = CharacterDatabase.Query(CStmt);
    if (CResult)
    {
        do
        {
            Field* Fields = CResult->Fetch();
            DayUseItemLimitMap[Fields[0].Get<uint32>()] = Fields[1].Get<uint32>();
        } while (CResult->NextRow());
    }

    CStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_FR_USE_ITEM_LIMIT);
    CStmt->SetData(0, GuidLow);
    CResult = CharacterDatabase.Query(CStmt);
    if (CResult)
    {
        do
        {
            Field* Fields = CResult->Fetch();
            FrUseItemLimitMap[Fields[0].Get<uint32>()] = Fields[1].Get<uint32>();
        } while (CResult->NextRow());
    }
}

bool ItemFuncMgr::LimitHookResItemUseNow(Player* player, Item* PItem)
{
    if (!PItem)
        return true;

    uint32 ItemId = PItem->GetEntry();
    auto Itr = UseItemLimitStore.find(ItemId);
    if (Itr == UseItemLimitStore.end())
        return true;

    if (Itr->second.UseDayLimt)
    {
        auto DIt = player->DayUseItemLimitMap.find(ItemId);
        if (DIt == player->DayUseItemLimitMap.end())
        {
            player->DayUseItemLimitMap[ItemId] = 1;
        }
        else
        {
            if (player->DayUseItemLimitMap[ItemId] >= Itr->second.UseDayLimt)
            {
                player->SendSysMsg(sHelper->GetGts(7051), Itr->second.UseDayLimt);
                return false;
            }

            ++player->DayUseItemLimitMap[ItemId];
        }

        uint32 DayCount = player->DayUseItemLimitMap[ItemId];
        CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_USE_ITEM_LIMIT);
        Utmt->SetData(0, player->GetGUIDLow());
        Utmt->SetData(1, ItemId);
        Utmt->SetData(2, DayCount);
        CharacterDatabase.Execute(Utmt);

        sDoAddon->SendPacketToClient(player, "AutoItemUseLimitData1", std::to_string(ItemId) + "#" + std::to_string(DayCount) + "#" + std::to_string(Itr->second.UseDayLimt));
    }

    if (Itr->second.UseFrLimt)
    {
        auto FIt = player->FrUseItemLimitMap.find(ItemId);
        if (FIt == player->FrUseItemLimitMap.end())
        {
            player->FrUseItemLimitMap[ItemId] = 1;
        }
        else
        {
            if (player->FrUseItemLimitMap[ItemId] >= Itr->second.UseFrLimt)
            {
                player->SendSysMsg(sHelper->GetGts(7052), Itr->second.UseFrLimt);
                return false;
            }

            ++player->FrUseItemLimitMap[ItemId];
        }

        uint32 FrCount = player->FrUseItemLimitMap[ItemId];
        CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_FR_USE_ITEM_LIMIT);
        Utmt->SetData(0, player->GetGUIDLow());
        Utmt->SetData(1, ItemId);
        Utmt->SetData(2, FrCount);
        CharacterDatabase.Execute(Utmt);

        sDoAddon->SendPacketToClient(player, "AutoItemUseLimitData2", std::to_string(ItemId) + "#" + std::to_string(FrCount) + "#" + std::to_string(Itr->second.UseFrLimt));
    }

    return true;
}

void ItemFuncMgr::ReseDayUsetLimitItem()
{
    CharacterDatabase.Execute(CharacterDatabase.GetPreparedStatement(CHAR_DEL_USE_ITEM_LIMIT));
}

bool ItemFuncMgr::InItemNotTask(uint32 ItemId)
{
    if (ItemNotTaskStore.empty())
        return false;

    auto Itr = ItemNotTaskStore.find(ItemId);
    return Itr != ItemNotTaskStore.end();
}

uint32 ItemFuncMgr::GetEnchMaxCount(uint32 EnchantId)
{
    auto Itr = EnchLimitStore.find(EnchantId);
    return Itr != EnchLimitStore.end() ? Itr->second : 0;
}

bool ItemFuncMgr::InEnchMaxCount(Player* player, bool Apply, uint32 EnchantId)
{
    uint32 MaxCount = GetEnchMaxCount(EnchantId);
    if (!MaxCount)
        return false;

    uint32 Count = 0;
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; i++)
    {
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint8 n = PROP_ENCHANTMENT_SLOT_0; n < MAX_ENCHANTMENT_SLOT; n++)
            {
                uint32 EnchId = pItem->GetEnchantmentId(EnchantmentSlot(n));
                if (EnchId == EnchantId)
                {
                    Count++;
                }
            }
        }
    }

    bool As = (Count > MaxCount);
    if (Apply && As)
        player->SendSysMsg(sHelper->GetGts(9910), sHelper->GetEnchName(EnchantId).c_str(), Count, MaxCount);

    return (Apply && As);
}

uint32 Player::CanStoreDayLimitItem(uint32 Entry, uint32 Count)
{
    auto Itr = AtAddItemLimitStore.find(Entry);
    if (Itr == AtAddItemLimitStore.end())
        return Count;

    if (!Itr->second.CharDayLimt)
        return Count;

    uint32 InsCount = Count;

    uint32 Limit = Itr->second.CharDayLimt;
    uint32 HasCount = 0;

    auto It = PlayerDayItemLimitMap.find(Entry);
    if (It != PlayerDayItemLimitMap.end())
        HasCount = It->second;

    if (HasCount + Count > Limit)
    {
        SendSysMsg(sHelper->GetGts(1701), sHelper->GetItemLink(Entry).c_str());
        InsCount = (Limit - HasCount);
    }

    It = PlayerDayItemLimitMap.find(Entry);
    if (It != PlayerDayItemLimitMap.end())
        It->second += InsCount;
    else
        PlayerDayItemLimitMap[Entry] = HasCount + InsCount;

    sDoAddon->SendPacketToClient(this, "AutoItemLimitData1", std::to_string(Entry) + "#" + std::to_string(HasCount + InsCount) + "#" + std::to_string(Itr->second.CharDayLimt));

    CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_LIMIT);
    Utmt->SetData(0, GetGUIDLow());
    Utmt->SetData(1, Entry);
    Utmt->SetData(2, HasCount + InsCount);
    CharacterDatabase.Execute(Utmt);

    return InsCount;
}

uint32 Player::CanStoreFrLimitItem(uint32 Entry, uint32 Count)
{
    auto Itr = AtAddItemLimitStore.find(Entry);
    if (Itr == AtAddItemLimitStore.end())
        return Count;

    if (!Itr->second.CharFrLimt)
        return Count;

    uint32 InsCount = Count;

    uint32 Limit = Itr->second.CharFrLimt;
    uint32 HasCount = 0;

    auto It = PlayerFrItemLimitMap.find(Entry);
    if (It != PlayerFrItemLimitMap.end())
        HasCount = It->second;

    if (HasCount + Count > Limit)
    {
        SendSysMsg(sHelper->GetGts(1702), sHelper->GetItemLink(Entry).c_str());
        InsCount = (Limit - HasCount);
    }

    It = PlayerFrItemLimitMap.find(Entry);
    if (It != PlayerFrItemLimitMap.end())
        It->second += InsCount;
    else
        PlayerFrItemLimitMap[Entry] = HasCount + InsCount;

    sDoAddon->SendPacketToClient(this, "AutoItemLimitData2", std::to_string(Entry) + "#" + std::to_string(HasCount + InsCount) + "#" + std::to_string(Itr->second.CharFrLimt));

    CharacterDatabasePreparedStatement* Utmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ITEM_FR_LIMIT);
    Utmt->SetData(0, GetGUIDLow());
    Utmt->SetData(1, Entry);
    Utmt->SetData(2, HasCount + InsCount);
    CharacterDatabase.Execute(Utmt);

    return InsCount;
}

uint32 Player::CanStoreAccDayLimitItem(uint32 Entry, uint32 Count)
{
    auto Itr = AtAddItemLimitStore.find(Entry);
    if (Itr == AtAddItemLimitStore.end())
        return Count;

    if (!Itr->second.AccDayLimt)
        return Count;

    uint32 InsCount = Count;

    uint32 Limit = Itr->second.AccDayLimt;
    uint32 HasCount = 0;

    auto It = PlayerAccDayItemLimitMap.find(Entry);
    if (It != PlayerAccDayItemLimitMap.end())
        HasCount = It->second;

    if (HasCount + Count > Limit)
    {
        SendSysMsg(sHelper->GetGts(1703), sHelper->GetItemLink(Entry).c_str());
        InsCount = (Limit - HasCount);
    }

    It = PlayerAccDayItemLimitMap.find(Entry);
    if (It != PlayerAccDayItemLimitMap.end())
        It->second += InsCount;
    else
        PlayerAccDayItemLimitMap[Entry] = HasCount + InsCount;

    sDoAddon->SendPacketToClient(this, "AutoItemLimitData3", std::to_string(Entry) + "#" + std::to_string(HasCount + InsCount) + "#" + std::to_string(Itr->second.AccDayLimt));

    LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ITEM_LIMIT);
    Utmt->SetData(0, GetSession()->GetAccountId());
    Utmt->SetData(1, Entry);
    Utmt->SetData(2, HasCount + InsCount);
    LoginDatabase.Execute(Utmt);

    return InsCount;
}

uint32 Player::CanStoreAccFrLimitItem(uint32 Entry, uint32 Count)
{
    auto Itr = AtAddItemLimitStore.find(Entry);
    if (Itr == AtAddItemLimitStore.end())
        return Count;

    if (!Itr->second.AccFrLimt)
        return Count;

    uint32 InsCount = Count;

    uint32 Limit = Itr->second.AccFrLimt;
    uint32 HasCount = 0;

    auto It = PlayerAccFrItemLimitMap.find(Entry);
    if (It != PlayerAccFrItemLimitMap.end())
        HasCount = It->second;

    if (HasCount + Count > Limit)
    {
        SendSysMsg(sHelper->GetGts(1704), sHelper->GetItemLink(Entry).c_str());
        InsCount = (Limit - HasCount);
    }

    It = PlayerAccFrItemLimitMap.find(Entry);
    if (It != PlayerAccFrItemLimitMap.end())
        It->second += InsCount;
    else
        PlayerAccFrItemLimitMap[Entry] = HasCount + InsCount;

    sDoAddon->SendPacketToClient(this, "AutoItemLimitData4", std::to_string(Entry) + "#" + std::to_string(HasCount + InsCount) + "#" + std::to_string(Itr->second.AccFrLimt));

    LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ITEM_FR_LIMIT);
    Utmt->SetData(0, GetSession()->GetAccountId());
    Utmt->SetData(1, Entry);
    Utmt->SetData(2, HasCount + InsCount);
    LoginDatabase.Execute(Utmt);

    return InsCount;
}

void ItemFuncMgr::ResetLimitItem()
{
    CharacterDatabase.Execute(CharacterDatabase.GetPreparedStatement(CHAR_DEL_ITEM_LIMIT));
    LoginDatabase.Execute(LoginDatabase.GetPreparedStatement(LOGIN_DEL_ITEM_LIMIT));
}

uint32 ItemFuncMgr::GetEquipTempId(uint32 ItemId)
{
    auto Itr = ItemEquipStore.find(ItemId);
    return (Itr != ItemEquipStore.end()) ? Itr->second : 0;
}

uint32 ItemFuncMgr::GetLimitNowCount(Player* player, uint32 ItemId)
{
    auto It = player->PlayerDayItemLimitMap.find(ItemId);
    if (It != player->PlayerDayItemLimitMap.end())
    {
        if (It->second)
            return It->second;
    }

    It = player->PlayerFrItemLimitMap.find(ItemId);
    if (It != player->PlayerFrItemLimitMap.end())
    {
        if (It->second)
            return It->second;
    }

    It = player->PlayerAccDayItemLimitMap.find(ItemId);
    if (It != player->PlayerAccDayItemLimitMap.end())
    {
        if (It->second)
            return It->second;
    }

    It = player->PlayerAccFrItemLimitMap.find(ItemId);
    if (It != player->PlayerAccFrItemLimitMap.end())
    {
        if (It->second)
            return It->second;
    }

    return 0;
}

bool ItemFuncMgr::CheckBuyItemLimit(Player* player, uint32 BuyItemId, uint32 BuyCount)
{
    auto Itr = AtAddItemLimitStore.find(BuyItemId);
    if (Itr == AtAddItemLimitStore.end())
        return true;

    uint32 NowCount = GetLimitNowCount(player, BuyItemId);
    uint32 InCount = NowCount + BuyCount;
    if (Itr->second.CharDayLimt > 0 && InCount > Itr->second.CharDayLimt)
    {
        uint32 DeCount = (NowCount > Itr->second.CharDayLimt) ? 0 : (Itr->second.CharDayLimt - NowCount);
        player->SendSysMsg("[提示]: <角色每日>购买已达限制 你还能购买 %u 个", DeCount);
        return false;
    }

    if (Itr->second.CharFrLimt > 0 && InCount > Itr->second.CharFrLimt)
    {
        uint32 DeCount = (NowCount > Itr->second.CharFrLimt) ? 0 : (Itr->second.CharFrLimt - NowCount);
        player->SendSysMsg("[提示]: <角色永久>购买已达限制 你还能购买 %u 个", DeCount);
        return false;
    }

    if (Itr->second.AccDayLimt > 0 && InCount > Itr->second.AccDayLimt)
    {
        uint32 DeCount = (NowCount > Itr->second.AccDayLimt) ? 0 : (Itr->second.AccDayLimt - NowCount);
        player->SendSysMsg("[提示]: <账号每日>购买已达限制 你还能购买 %u 个", DeCount);
        return false;
    }

    if (Itr->second.AccFrLimt > 0 && InCount > Itr->second.AccFrLimt)
    {
        uint32 DeCount = (NowCount > Itr->second.AccFrLimt) ? 0 : (Itr->second.AccFrLimt - NowCount);
        player->SendSysMsg("[提示]: <账号永久>购买已达限制 你还能购买 %u 个", DeCount);
        return false;
    }

    return true;
}

bool ItemFuncMgr::IsInAtEnchItem(uint32 UseItem, uint32 TarItem)
{
    for (auto Itr = EnchResetAtItemStore.begin(); Itr != EnchResetAtItemStore.end(); Itr++)
    {
        if (Itr->UseItem == UseItem && Itr->TarItem == TarItem)
            return true;
    }

    return false;
}

bool ItemFuncMgr::DoAtEnchItem(Player* player, Item* TargetItem, uint32 UseItem)
{
    uint32 TarItemId = TargetItem->GetEntry();
    uint32 TarGuidLow = TargetItem->GetGUIDLow();
    bool Zt = false;
    for (auto Itr = EnchResetAtItemStore.begin(); Itr != EnchResetAtItemStore.end(); Itr++)
    {
        if (Itr->UseItem == UseItem && Itr->TarItem == TarItemId)
        {
            if (!player->GetEnchLock(TarGuidLow, PROP_ENCHANTMENT_SLOT_0))
            {
                if (roll_chance_f(Itr->EnchSlot1))
                    DoRandomEnch(TargetItem, Itr->EnchGroup1, PROP_ENCHANTMENT_SLOT_0);
                else
                    TargetItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_0);

                Zt = true;
            }

            if (!player->GetEnchLock(TarGuidLow, PROP_ENCHANTMENT_SLOT_1))
            {
                if (roll_chance_f(Itr->EnchSlot2))
                    DoRandomEnch(TargetItem, Itr->EnchGroup2, PROP_ENCHANTMENT_SLOT_1);
                else
                    TargetItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_1);

                Zt = true;
            }

            if (!player->GetEnchLock(TarGuidLow, PROP_ENCHANTMENT_SLOT_2))
            {
                if (roll_chance_f(Itr->EnchSlot3))
                    DoRandomEnch(TargetItem, Itr->EnchGroup3, PROP_ENCHANTMENT_SLOT_2);
                else
                    TargetItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_2);

                Zt = true;
            }

            if (!player->GetEnchLock(TarGuidLow, PROP_ENCHANTMENT_SLOT_3))
            {
                if (roll_chance_f(Itr->EnchSlot4))
                    DoRandomEnch(TargetItem, Itr->EnchGroup4, PROP_ENCHANTMENT_SLOT_3);
                else
                    TargetItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_3);

                Zt = true;
            }

            if (!player->GetEnchLock(TarGuidLow, PROP_ENCHANTMENT_SLOT_4))
            {
                if (roll_chance_f(Itr->EnchSlot5))
                    DoRandomEnch(TargetItem, Itr->EnchGroup5, PROP_ENCHANTMENT_SLOT_4);
                else
                    TargetItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_4);

                Zt = true;
            }
        }
    }

    if (!Zt)
        return false;

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

    return true;
}

bool ItemFuncMgr::CanDoInItemCards(Player* player, uint32 ItemId)
{
    bool IsIn = true;
    for (size_t i = AT3001; i < ATKPMAX; i++)
    {
        uint32 OnItemId = player->TempAtStore[i];
        if (OnItemId == ItemId)
            IsIn = false;
    }

    return IsIn;
}

void ItemFuncMgr::HandlersItemCardsAuraSpellOnPlayer(Player* player, uint32 Num)
{
    player->ItemCardsAuraList[Num].clear();

    std::vector<uint32> GroupList;
    GroupList.clear();
    for (size_t i = AT3001; i < ATKPMAX; i++)
    {
        uint32 OnItemId = player->TempAtStore[i];
        auto Itr = ItemCardsStore.find(OnItemId);
        if (Itr != ItemCardsStore.end())
        {
            auto It = CardsGpStore.find(Itr->second.Group1);
            if (It != CardsGpStore.end())
                GroupList.push_back(Itr->second.Group1);

            It = CardsGpStore.find(Itr->second.Group2);
            if (It != CardsGpStore.end())
                GroupList.push_back(Itr->second.Group2);

            It = CardsGpStore.find(Itr->second.Group3);
            if (It != CardsGpStore.end())
                GroupList.push_back(Itr->second.Group3);
        }
    }

    std::map<uint32, uint32> OneList;
    OneList.clear();
    for (auto Itr = GroupList.begin(); Itr != GroupList.end(); ++Itr)
    {
        auto Count = std::count(GroupList.begin(), GroupList.end(), *Itr);
        if (Count >= Num)
        {
            auto It = OneList.find(*Itr);
            if (It == OneList.end())
                OneList.insert(std::make_pair(*Itr, Count));
        }
    }

    for (auto Itr = OneList.begin(); Itr != OneList.end(); ++Itr)
    {
        auto It = CardsGpStore.find(Itr->first);
        if (It != CardsGpStore.end())
        {
            switch (Num)
            {
            case AuraIndex8:
            {
                player->ItemCardsAuraList[AuraIndex8].push_back(It->second.Aura8);
                for (auto Ite = player->ItemCardsAuraList[AuraIndex8].begin(); Ite != player->ItemCardsAuraList[AuraIndex8].end(); ++Ite)
                {
                    if (!player->HasAura(*Ite))
                        player->AddAura(*Ite, player);
                }
            }
            break;
            case AuraIndex6:
            {
                player->ItemCardsAuraList[AuraIndex6].push_back(It->second.Aura6);
                for (auto Ite = player->ItemCardsAuraList[AuraIndex6].begin(); Ite != player->ItemCardsAuraList[AuraIndex6].end(); ++Ite)
                {
                    if (!player->HasAura(*Ite))
                        player->AddAura(*Ite, player);
                }
            }
            break;
            case AuraIndex4:
            {
                player->ItemCardsAuraList[AuraIndex4].push_back(It->second.Aura4);
                for (auto Ite = player->ItemCardsAuraList[AuraIndex4].begin(); Ite != player->ItemCardsAuraList[AuraIndex4].end(); ++Ite)
                {
                    if (!player->HasAura(*Ite))
                        player->AddAura(*Ite, player);
                }
            }
            break;
            case AuraIndex2:
            {
                player->ItemCardsAuraList[AuraIndex2].push_back(It->second.Aura2);
                for (auto Ite = player->ItemCardsAuraList[AuraIndex2].begin(); Ite != player->ItemCardsAuraList[AuraIndex2].end(); ++Ite)
                {
                    if (!player->HasAura(*Ite))
                        player->AddAura(*Ite, player);
                }
            }
            break;
            }
        }
    }

    GroupList.clear();
    OneList.clear();
}

void ItemFuncMgr::OnLoginCastCardsAuraSpell(Player* player)
{
    if (IsXsPlayerbot(player))
        return;

    OnClearItemCardsAuraSpell(player);
    HandlersItemCardsAuraSpellOnPlayer(player, AuraIndex8);
    HandlersItemCardsAuraSpellOnPlayer(player, AuraIndex6);
    HandlersItemCardsAuraSpellOnPlayer(player, AuraIndex4);
    HandlersItemCardsAuraSpellOnPlayer(player, AuraIndex2);
}

void ItemFuncMgr::OnClearItemCardsAuraSpell(Player* player)
{
    for (size_t i = AT3001; i < ATKPMAX; i++)
    {
        uint32 OnItemId = player->TempAtStore[i];
        auto Itr = ItemCardsStore.find(OnItemId);
        if (Itr != ItemCardsStore.end())
        {
            auto It = CardsGpStore.find(Itr->second.Group1);
            if (It != CardsGpStore.end())
            {
                if (player->HasAura(It->second.Aura2))
                    player->RemoveAurasDueToSpell(It->second.Aura2);
                if (player->HasAura(It->second.Aura4))
                    player->RemoveAurasDueToSpell(It->second.Aura4);
                if (player->HasAura(It->second.Aura6))
                    player->RemoveAurasDueToSpell(It->second.Aura6);
                if (player->HasAura(It->second.Aura8))
                    player->RemoveAurasDueToSpell(It->second.Aura8);
            }

            It = CardsGpStore.find(Itr->second.Group2);
            if (It != CardsGpStore.end())
            {
                if (player->HasAura(It->second.Aura2))
                    player->RemoveAurasDueToSpell(It->second.Aura2);
                if (player->HasAura(It->second.Aura4))
                    player->RemoveAurasDueToSpell(It->second.Aura4);
                if (player->HasAura(It->second.Aura6))
                    player->RemoveAurasDueToSpell(It->second.Aura6);
                if (player->HasAura(It->second.Aura8))
                    player->RemoveAurasDueToSpell(It->second.Aura8);
            }

            It = CardsGpStore.find(Itr->second.Group3);
            if (It != CardsGpStore.end())
            {
                if (player->HasAura(It->second.Aura2))
                    player->RemoveAurasDueToSpell(It->second.Aura2);
                if (player->HasAura(It->second.Aura4))
                    player->RemoveAurasDueToSpell(It->second.Aura4);
                if (player->HasAura(It->second.Aura6))
                    player->RemoveAurasDueToSpell(It->second.Aura6);
                if (player->HasAura(It->second.Aura8))
                    player->RemoveAurasDueToSpell(It->second.Aura8);
            }
        }
    }
}

void ItemFuncMgr::SendUpdateCardsAuraSpellData(Player* player)
{
    std::ostringstream Oss8;
    for (auto Itr = player->ItemCardsAuraList[AuraIndex8].begin(); Itr != player->ItemCardsAuraList[AuraIndex8].end(); ++Itr)
        Oss8 << sHelper->GetSpellDes(*Itr) << "#";

    sDoAddon->SendPacketToClient(player, "AutoMeItemCardsAuras8", Oss8.str());

    std::ostringstream Oss6;
    for (auto Itr = player->ItemCardsAuraList[AuraIndex6].begin(); Itr != player->ItemCardsAuraList[AuraIndex6].end(); ++Itr)
        Oss6 << sHelper->GetSpellDes(*Itr) << "#";

    sDoAddon->SendPacketToClient(player, "AutoMeItemCardsAuras6", Oss6.str());

    std::ostringstream Oss4;
    for (auto Itr = player->ItemCardsAuraList[AuraIndex4].begin(); Itr != player->ItemCardsAuraList[AuraIndex4].end(); ++Itr)
        Oss4 << sHelper->GetSpellDes(*Itr) << "#";

    sDoAddon->SendPacketToClient(player, "AutoMeItemCardsAuras4", Oss4.str());

    std::ostringstream Oss2;
    for (auto Itr = player->ItemCardsAuraList[AuraIndex2].begin(); Itr != player->ItemCardsAuraList[AuraIndex2].end(); ++Itr)
        Oss2 << sHelper->GetSpellDes(*Itr) << "#";

    sDoAddon->SendPacketToClient(player, "AutoMeItemCardsAuras2", Oss2.str());
}

void ItemFuncMgr::SendUpdateEnchAtCount(Player* player, uint32 GuidLow, uint32 RtsCount, int32 AwRts)
{
    sDoAddon->SendPacketToClient(player, "AutoItemEnchAtCount", std::to_string(GuidLow) + "#" + std::to_string(RtsCount) + "#" + std::to_string(AwRts));
}

void ItemFuncMgr::DoCreateRts(Item* DoItem)
{
    if (!DoItem->GetOwner())
        return;

    auto Itr = ItemEnchStore.find(DoItem->GetEntry());
    if (Itr == ItemEnchStore.end())
        return;
    if (Itr->second.Rts < 0)
        return;

    DoItem->ItemRtsCount = 0;

    CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_ITEM_ENCH_RTS);
    Rtmt->SetData(0, DoItem->GetGUIDLow());
    Rtmt->SetData(1, DoItem->GetEntry());
    Rtmt->SetData(2, DoItem->ItemRtsCount);
    CharacterDatabase.Execute(Rtmt);
    ItemEnchAtCountStore[DoItem->GetGUIDLow()] = std::to_string(DoItem->ItemRtsCount) + "#" + std::to_string(Itr->second.Rts);
    SendUpdateEnchAtCount(DoItem->GetOwner(), DoItem->GetGUIDLow(), DoItem->ItemRtsCount, Itr->second.Rts);
}

void ItemFuncMgr::LoadEnchRtsDB(Item* DoItem)
{
    auto Itr = ItemEnchAtCountStore.find(DoItem->GetGUIDLow());
    if (Itr == ItemEnchAtCountStore.end())
        return;

    Tokenizer Data(Itr->second, '#');
    if (Data.IsEmpty())
        return;

    if (Optional<uint32> Count = Acore::StringTo<uint32>(Data[0]))
        DoItem->ItemRtsCount = *Count;
}

int32 ItemFuncMgr::GetAwEnchRts(uint32 ItemId)
{
    auto Itr = ItemEnchStore.find(ItemId);
    return Itr != ItemEnchStore.end() ? Itr->second.Rts : -1;
}

void ItemFuncMgr::UpdateEnchRtsDB(Player* player, Item* DoItem)
{
    CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_ITEM_ENCH_RTS);
    Rtmt->SetData(0, DoItem->GetGUIDLow());
    Rtmt->SetData(1, DoItem->GetEntry());
    Rtmt->SetData(2, DoItem->ItemRtsCount);
    CharacterDatabase.Execute(Rtmt);

    auto Itr = ItemEnchStore.find(DoItem->GetEntry());
    if (Itr == ItemEnchStore.end())
        return;
    if (Itr->second.Rts < 0)
        return;

    ItemEnchAtCountStore[DoItem->GetGUIDLow()] = std::to_string(DoItem->ItemRtsCount) + "#" + std::to_string(Itr->second.Rts);
    SendUpdateEnchAtCount(player, DoItem->GetGUIDLow(), DoItem->ItemRtsCount, Itr->second.Rts);
}

bool ItemFuncMgr::OnItemUseNow(Player* player, Item* PItem)
{
    if (!PItem)
        return true;

    uint32 ItemId = PItem->GetEntry();

    auto It1 = ItemCommandStore.find(ItemId);
    if (It1 != ItemCommandStore.end())
    {
        if (It1->second.NTempId > 0)
        {
            if (!sHelper->CheckNeed(player, It1->second.NTempId, "物品使用命令消耗"))
                return true;
        }

        // 先执行固定命令组
        if (!It1->second.Cmd.empty())
            player->DoCommands(It1->second.Cmd);

        // 再执行随机命令组
        if (!It1->second.RCmd.empty())
        {
            Tokenizer Data(It1->second.RCmd, '#');
            if (!Data.IsEmpty())
                player->DoCommands(Data[urand(0, Data.size() - 1)]);
        }

        if (It1->second.OnDel > 0)
        {
            player->DestroyItemCount(ItemId, 1, true);
            return true;
        }
    }

    auto It2 = SummonStructStore.find(ItemId);
    if (It2 != SummonStructStore.end())
    {
        if (sHelper->CheckNeed(player, It2->second.NTempId, "物品召唤功能消耗"))
        {
            WorldLocation Location = player->GetWorldLocation();
            if (It2->second.SEntry > 0)
            {
                if (Creature* Cre = player->SummonCreature(It2->second.SEntry, Location, TEMPSUMMON_TIMED_DESPAWN, It2->second.DoTime * IN_MILLISECONDS))
                    Cre->Say(It2->second.Text, LANG_UNIVERSAL);
            }
            else if (It2->second.SEntry < 0)
            {
                player->SummonGameObject(abs(It2->second.SEntry), Location.GetPositionX(), Location.GetPositionY(), Location.GetPositionZ(), Location.GetOrientation(), 0, 0, 0, 0, It2->second.DoTime);
            }

            if (It2->second.OnDel > 0)
            {
                player->DestroyItemCount(ItemId, 1, true);
                return true;
            }
        }
    }

    return false;
}

bool ItemFuncMgr::InItemEnchTaskStoreLeft(uint32 ItemId)
{
    bool In = false;
    for (auto Itr = NewFmZyStore.begin(); Itr != NewFmZyStore.end(); Itr++)
    {
        if (Itr->ItemId == ItemId)
            In = true;
    }

    return In;
}

bool ItemFuncMgr::InKtItemEnchTaskStore(uint32 LeftItem, uint32 RightItem)
{
    bool In = false;
    for (auto Itr = NewFmZyStore.begin(); Itr != NewFmZyStore.end(); Itr++)
    {
        if (Itr->ItemId == LeftItem && Itr->TarItemId == RightItem)
            In = true;
    }

    return In;
}

bool ItemFuncMgr::InSlotItemEnchTaskStore(uint32 LeftItem, uint32 RightItem, uint32 AtSlot)
{
    bool In = false;
    for (auto Itr = NewFmZyStore.begin(); Itr != NewFmZyStore.end(); Itr++)
    {
        if (Itr->ItemId == LeftItem && Itr->TarItemId == RightItem && Itr->Slot == AtSlot)
            In = true;
    }

    return In;
}

uint32 ItemFuncMgr::GetTaskNTempId(uint32 LeftItem, uint32 RightItem, uint32 AtSlot)
{
    uint32 NTempId = 0;
    for (auto Itr = NewFmZyStore.begin(); Itr != NewFmZyStore.end(); Itr++)
    {
        if (Itr->ItemId == LeftItem && Itr->TarItemId == RightItem && Itr->Slot == AtSlot)
            NTempId = Itr->NTempId;
    }

    return NTempId;
}

void ItemFuncMgr::NewSendUpdateShowEnchInfo1(Player* player, Item* OnItem)
{
    uint8 Num = 0;
    for (uint32 i = PROP_ENCHANTMENT_SLOT_0; i < MAX_ENCHANTMENT_SLOT; ++i)
    {
        Num++;
        uint32 EnchId = OnItem->GetEnchantmentId(EnchantmentSlot(i));
        std::string EnchDes = sHelper->GetEnchName(EnchId);
        sDoAddon->SendPacketToClient(player, "NewShowEnchInfo1", std::to_string(Num) + "#" + std::to_string(EnchId) + "#" + EnchDes);
    }
}

void ItemFuncMgr::NewSendUpdateShowEnchInfo2(Player* player, Item* OnItem)
{
    uint8 Num = 0;
    for (uint32 i = PROP_ENCHANTMENT_SLOT_0; i < MAX_ENCHANTMENT_SLOT; ++i)
    {
        Num++;
        uint32 EnchId = OnItem->GetEnchantmentId(EnchantmentSlot(i));
        std::string EnchDes = sHelper->GetEnchName(EnchId);
        sDoAddon->SendPacketToClient(player, "NewShowEnchInfo2", std::to_string(Num) + "#" + std::to_string(EnchId) + "#" + EnchDes);
    }
}

class OnItemFuncPlayer : public PlayerScript
{
public:
    OnItemFuncPlayer() : PlayerScript("OnItemFuncPlayer") {  }

    void OnLogin(Player* player) override
    {
        sItemFuncMgr->DoOnLoginItemInStat(player);
        sItemFuncMgr->AutoSoulItemStatsAndSpells(player);
        sItemFuncMgr->OnLoginCastCardsAuraSpell(player);
    }
};

void AddSC_ItemFuncPlayerScript()
{
    new OnItemFuncPlayer();
}
