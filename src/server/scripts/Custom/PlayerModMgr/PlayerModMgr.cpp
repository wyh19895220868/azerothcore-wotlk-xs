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
#include "Guild.h"
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
#include "Bag.h"
#include "Item.h"
#include "Pet.h"
#include <CharacterCache.h>
#include <algorithm>
#include "PlayerModMgr.h"
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#include <Custom/AddonActive/DoAddon.h>
#include <Custom/EventMgr/EventMgr.h>
#include <Custom/ClassConfig/ClassMod.h>
#include <Custom/ItemFuncMgr/ItemFuncMgr.h>
#include <Custom/BGAsTeam/BGAs.h>
#include <Custom/NoticeMgr/NoticeMgr.h>
#include <Custom/EnterFBMgr/EnterFBMgr.h>
#include <Time/GameTime.h>
#include <Custom/CreatureMod/AsCreature.h>
#include <Custom/AuraSpellMgr/AuraSpellMgr.h>
#include "PassiveAI.h"
#include <Custom/ItemMagicMgr/ItemMagicMgr.h>
#include <Custom/ItemPlayerStatMod/ItemPlayerStatMod.h>
#include <Custom/ItemRandomAtMgr/ItemRandomAtMgr.h>
#include <Custom/ItemRunesMgr/ItemRunesMgr.h>
#include <Custom/ItemDevelopMgr/ItemDevelopMgr.h>
#include <Custom/GuildBattleMgr/GuildBattleMgr.h>
#include <Custom/AthenaEventMgr/AthenaEventMgr.h>
#include <Custom/MapAreaFuncMgr/MapAreaFunc.h>
#include <Custom/PlayerAction/PlayerAction.h>
#include "../../../../../modules/mod-huanjinxitong/src/HuanJingSystem.h"

#pragma execution_character_set("utf-8")

VipMap VipStore;
CreDmgRankTempMap CreDmgRankTempStore;
SignRewardMap SignRewardStore;
AuraLootItemMap AuraLootItemStore;
EqSlotTempMap EqSlotTempStore;
EqAuraMap EqAuraStore;
PVPKTempMap PVPKStore;
PVPMapEventMap PVPMapEventStore;
EnMapPlayerMap EnMapPlayerStore;

CustomTalentTempMap CustomTalentTempStore;

PlayerModMgr::~PlayerModMgr()
{

}

void PlayerModMgr::SetVipRank(uint32 AccountId, uint32 NowRank)
{
    uint32 OldRank = GetVipRank(AccountId);
    if (NowRank != OldRank)
    {
        uint32 MaxRank = GetVipMaxRank();
        NowRank = NowRank >= MaxRank ? MaxRank : NowRank;
        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_VIP_RANK);
        Utmt->SetData(0, NowRank);
        Utmt->SetData(1, AccountId);
        LoginDatabase.Execute(Utmt);
    }
}

uint32 PlayerModMgr::GetVipRank(uint32 AccountId)
{
    LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_VIP);
    Stmt->SetData(0, AccountId);
    PreparedQueryResult Result = LoginDatabase.Query(Stmt);
    return (Result ? (*Result)[0].Get<uint32>() : 0);
}

void PlayerModMgr::AddToken(ObjectGuid Guld, uint32 AccountId, int64 Count, std::string Info)
{
    if (Count != 0)
    {
        int64 OldToken = int64(GetToken(AccountId));
        int64 NowToken = OldToken + Count;
        NowToken = NowToken > 0 ? NowToken : 0;

        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN);
        Utmt->SetData(0, NowToken);
        Utmt->SetData(1, AccountId);
        LoginDatabase.Execute(Utmt);

        sHelper->LogPlayerTokenNow(Guld, Count, Info);
    }
}

uint64 PlayerModMgr::GetToken(uint32 AccountId)
{
    LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_VIP);
    Stmt->SetData(0, AccountId);
    PreparedQueryResult Result = LoginDatabase.Query(Stmt);
    return (Result ? (*Result)[1].Get<uint64>() : 0);
}

uint64 PlayerModMgr::GetAsToken(uint32 AccountId)
{
    LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_VIP);
    Stmt->SetData(0, AccountId);
    PreparedQueryResult Result = LoginDatabase.Query(Stmt);
    return (Result ? (*Result)[2].Get<uint64>() : 0);
}

void PlayerModMgr::AddTokenAH(ObjectGuid Guld, int64 Count)
{
    Player* player = ObjectAccessor::FindConnectedPlayer(Guld);
    uint32 AccountId = sCharacterCache->GetCharacterAccountIdByGuid(Guld);
    if (player || AccountId)
    {
        int64 OldToken = int64(GetToken(AccountId));
        int64 NowToken = OldToken + Count;
        NowToken = NowToken > 0 ? NowToken : 0;

        if (player)
            player->SendSysMsg(sHelper->GetGts(1049), (Count < 0) ? "减少" : "增加", (Count > 0) ? Count : (OldToken < abs(Count)) ? OldToken : abs(Count), NowToken);

        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN);
        Utmt->SetData(0, NowToken);
        Utmt->SetData(1, AccountId);
        LoginDatabase.Execute(Utmt);

        sHelper->LogPlayerTokenNow(Guld, Count, "拍卖行收入或消费");
    }
}

void Player::AddToken(int64 Count, std::string Info)
{
    if (Count != 0)
    {
        uint32 AccountId = GetSession()->GetAccountId();
        int64 OldToken = int64(GetToken());
        int64 NowToken = OldToken + Count;
        NowToken = NowToken > 0 ? NowToken : 0;

        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_TOKEN);
        Utmt->SetData(0, NowToken);
        Utmt->SetData(1, AccountId);
        LoginDatabase.Execute(Utmt);

        SendSysMsg(sHelper->GetGts(1007), (Count < 0) ? "减少" : "增加", (Count > 0) ? Count : (OldToken < abs(Count)) ? OldToken : abs(Count), NowToken);

        sHelper->LogPlayerTokenNow(GetGUID(), Count, Info);
    }
}

uint64 Player::GetToken()
{
    uint32 AccountId = GetSession()->GetAccountId();
    LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_VIP);
    Stmt->SetData(0, AccountId);
    PreparedQueryResult Result = LoginDatabase.Query(Stmt);
    return (Result ? (*Result)[1].Get<uint64>() : 0);
}

uint64 Player::GetAsToken()
{
    uint32 AccountId = GetSession()->GetAccountId();
    LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_VIP);
    Stmt->SetData(0, AccountId);
    PreparedQueryResult Result = LoginDatabase.Query(Stmt);
    return (Result ? (*Result)[2].Get<uint64>() : 0);
}

void Player::SetVipRank(uint32 Value)
{
    if (Value != VipRank)
    {
        uint32 MaxRank = sPlayerModMgr->GetVipMaxRank();
        Value = Value >= MaxRank ? MaxRank : Value;
        sPlayerAction->CustomVipChanged(this, VipRank, Value);

        SendSysMsg(sHelper->GetGts(1008), (Value > VipRank) ? "提升" : "降低", Value);

        if (sHelper->GetBoolean(13901) && Value > VipRank)
            sHelper->SendSerSpecialMessage(sHelper->GetGts(7100), sHelper->GetNameLinkColor(this).c_str(), Value);

        VipRank = Value;
        VLootRate = sPlayerModMgr->GetVipLootOdds(VipRank);
        VLootKn = sPlayerModMgr->GetVipLootKn(VipRank);

        uint32 AccountId = GetSession()->GetAccountId();
        LoginDatabasePreparedStatement* Utmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_VIP_RANK);
        Utmt->SetData(0, VipRank);
        Utmt->SetData(1, AccountId);
        LoginDatabase.Execute(Utmt);

        sPlayerModMgr->UpdateVipAuras(this, VipRank);

        ObjectGuid Guid = GetGUID();
        std::string Prefix = sPlayerModMgr->GetVipPixStr(VipRank);
        sCharacterCache->DoUpdateNamePrefix(Guid, Prefix);
        sPlayerModMgr->UpdateNameQuery(this);

        InitTalentForLevel();
        SetCanModifyStats(true);
        UpdateAllStats();
    }
}

void Player::DoInitAsRank()
{
    uint32 Account = GetSession()->GetAccountId();
    LoginDatabasePreparedStatement* VStmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_TOKEN_VIP);
    VStmt->SetData(0, Account);
    PreparedQueryResult VResult = LoginDatabase.Query(VStmt);
    if (!VResult)
    {
        LoginDatabasePreparedStatement* VItmt = LoginDatabase.GetPreparedStatement(LOGIN_INS_TOKEN_VIP);
        VItmt->SetData(0, Account);
        VItmt->SetData(1, 0);
        VItmt->SetData(2, 0);
        VItmt->SetData(3, 0);
        VItmt->SetData(4, 0);
        VItmt->SetData(5, 0);
        LoginDatabase.Execute(VItmt);

        VipRank = 0;
        VLootRate = 0.0f;
        VLootKn = 0.0f;
    }
    else
    {
        uint32 Rank = (*VResult)[0].Get<uint32>();
        VipRank = Rank;
        VLootRate = sPlayerModMgr->GetVipLootOdds(VipRank);
        VLootKn = sPlayerModMgr->GetVipLootKn(VipRank);
    }

    ObjectGuid Guid = GetGUID();
    std::string Prefix = sPlayerModMgr->GetVipPixStr(VipRank);
    sCharacterCache->DoUpdateNamePrefix(Guid, Prefix);
    sPlayerModMgr->UpdateNameQuery(this);

    LoadPaodianCache();
}

void PlayerModMgr::LoadPlayerMod()
{
    VipStore.clear();
    uint32 Ct = 0;
    QueryResult Result = WorldDatabase.Query("SELECT 会员等级, 需求模版, 奖励模版, 附加额外属性比例, 增加天赋点数, 附加掉率, 爆率比例, 升级成功率, 附加的光环, 会员玩家模版, 图标字符, 菜单描述, 名字前缀 FROM _会员等级配置表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Level = Fields[0].Get<uint32>();
            VipTemp St;
            St.NTempId = Fields[1].Get<uint32>();
            St.RTempId = Fields[2].Get<uint32>();
            St.StatsMod = Fields[3].Get<float>();
            St.Tp = Fields[4].Get<uint32>();
            St.LootOdds = Fields[5].Get<float>();
            St.LootKn = Fields[6].Get<float>();
            St.UpOdds = Fields[7].Get<float>();
            St.AtAuras = Fields[8].Get<std::string>();
            St.STempId = Fields[9].Get<uint32>();
            St.IconStr = Fields[10].Get<std::string>();
            St.MenuStr = Fields[11].Get<std::string>();
            St.VipPix = Fields[12].Get<std::string>();
            VipStore.insert(std::make_pair(Level, St));
            ++Ct;

        } while (Result->NextRow());
    }
}

void PlayerModMgr::LoadDmgRank()
{
    CreDmgRankTempStore.clear();
    uint32 Ct = 0;
    QueryResult Result = WorldDatabase.Query("SELECT 生物编号, 第1名, 第2名, 第3名, 第4名, 第5名, 第6名, 第7名, 第8名, 第9名, 第10名, 参与, 最低伤害要求, IP地址数量限制 FROM _生物伤害排行榜表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 CreEntry = Fields[0].Get<uint32>();
            CreDmgRankTemp St;
            St.TempId1 = Fields[1].Get<uint32>();
            St.TempId2 = Fields[2].Get<uint32>();
            St.TempId3 = Fields[3].Get<uint32>();
            St.TempId4 = Fields[4].Get<uint32>();
            St.TempId5 = Fields[5].Get<uint32>();
            St.TempId6 = Fields[6].Get<uint32>();
            St.TempId7 = Fields[7].Get<uint32>();
            St.TempId8 = Fields[8].Get<uint32>();
            St.TempId9 = Fields[9].Get<uint32>();
            St.TempId10 = Fields[10].Get<uint32>();
            St.TempIdAs = Fields[11].Get<uint32>();
            St.MinDmg = AsInt64FromString(Fields[12].Get<std::string>());
            St.IPCount = Fields[13].Get<uint32>();
            CreDmgRankTempStore.insert(std::make_pair(CreEntry, St));
            ++Ct;
        } while (Result->NextRow());
    }
}

void PlayerModMgr::LoadPVPKMod()
{
    PVPKStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 物品编号, 概率, 最小数量, 最大数量 FROM _玩家被玩家击杀掉落表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 KItemId = Fields[0].Get<uint32>();
            PVPKTemp St;
            St.Odds = Fields[1].Get<float>();
            St.MinCount = Fields[2].Get<uint32>();
            St.MaxCount = Fields[3].Get<uint32>();
            if (St.MinCount > 250)
                St.MinCount = 250;
            if (St.MaxCount > 250)
                St.MaxCount = 250;

            if (St.MinCount > St.MaxCount)
                St.MinCount = St.MaxCount;

            PVPKStore.insert(std::make_pair(KItemId, St));

        } while (Result->NextRow());
    }

    PVPMapEventStore.clear();
    Result = WorldDatabase.Query("SELECT 绑定事件编号, 地图或区域, 击杀者奖励几率, 击杀者奖励模版, 被杀者奖励几率, 被杀者奖励模版, 同IP判定 FROM _玩家击杀玩家奖励表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint16 EventId = Fields[0].Get<uint16>();
            PVPMapEventTemp St;
            St.MapOrArea = Fields[1].Get<int32>();
            St.KillRewOdds = Fields[2].Get<float>();
            St.KillRewTempId = Fields[3].Get<uint32>();
            St.KilledRewOdds = Fields[4].Get<float>();
            St.KilledRewTempId = Fields[5].Get<uint32>();
            St.OneIp = Fields[6].Get<bool>();

            PVPMapEventStore.insert(std::make_pair(EventId, St));

        } while (Result->NextRow());
    }
}

void PlayerModMgr::LoadTalentInfoMod()
{
    CustomTalentTempStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 天赋技能编号, 需求模版, 需要当前系点数, 需要全部点数, 互斥组 FROM _天赋技能需求表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 SpellId = Fields[0].Get<uint32>();
            CustomTalentTemp St;
            St.NTempId = Fields[1].Get<uint32>();
            St.NeedTabPoints = Fields[2].Get<uint32>();
            St.NeedAsPoints = Fields[3].Get<uint32>();
            std::string Spells = Fields[4].Get<std::string>();
            if (!Spells.empty())
            {
                Tokenizer Datas(Spells, '#');
                for (auto Itr = Datas.begin(); Itr != Datas.end(); ++Itr)
                    St.SpellIdList.insert(uint32(atoi(*Itr)));
            }

            CustomTalentTempStore.insert(std::make_pair(SpellId, St));

        } while (Result->NextRow());
    }
}

uint32 PlayerModMgr::InNotTalentInfo(Player* player, uint32 SpellId)
{
    auto Itr = CustomTalentTempStore.find(SpellId);
    if (Itr == CustomTalentTempStore.end())
        return 0;

    if (Itr->second.SpellIdList.empty())
        return 0;

    const PlayerTalentMap& PTalentMap = player->GetTalentMap();
    for (PlayerTalentMap::const_iterator PIt = PTalentMap.begin(); PIt != PTalentMap.end(); ++PIt)
    {
        auto It = Itr->second.SpellIdList.find(PIt->first);
        if (It != Itr->second.SpellIdList.end())
            return PIt->first;
    }

    return 0;
}

bool PlayerModMgr::HasTalentInfoGts(Player* player, uint32 UseTalentSpellId, uint32 TalentRank)
{
    if (player->GetFreeTalentPoints() == 0)
        return false;

    if (TalentRank >= MAX_TALENT_RANK)
        return false;

    TalentEntry const* TalentInfo = sTalentStore.LookupEntry(UseTalentSpellId);
    if (!TalentInfo)
        return false;

    TalentTabEntry const* TalentTabInfo = sTalentTabStore.LookupEntry(TalentInfo->TalentTab);
    if (!TalentTabInfo)
        return false;

    if ((player->getClassMask() & TalentTabInfo->ClassMask) == 0)
        return false;

    uint32 SpellId = TalentInfo->RankID[TalentRank];
    if (SpellId == 0)
        return false;

    SpellInfo const* SInfo = sSpellMgr->GetSpellInfo(SpellId);
    if (!SInfo)
        return false;

    auto Itt = CustomTalentTempStore.find(SpellId);
    if (Itt == CustomTalentTempStore.end())
        return false;

    if (Itt->second.NTempId == 0 && Itt->second.NeedTabPoints == 0 && Itt->second.NeedAsPoints == 0 && Itt->second.SpellIdList.empty())
        return false;

    return true;
}

void PlayerModMgr::OnKillLootPlayer(Player* Killer, Player* Killed)
{
    if (Killer->GetGUID() == Killed->GetGUID())
        return;

    if (strcmp(Killer->GetSession()->GetRemoteAddress().c_str(), Killed->GetSession()->GetRemoteAddress().c_str()) == 0)
        return;

    if (IsXsPlayerbot(Killed))
        return;

    if (PVPKStore.empty())
        return;

    std::map<uint32, uint32> KList;
    KList.clear();
    //主包
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; i++)
        if (Item* pItem = Killed->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (KList.find(pItem->GetEntry()) == KList.end())
                KList.insert(std::make_pair(pItem->GetEntry(), pItem->GetCount()));

    //额外四包
    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; i++)
        if (Bag* pBag = Killed->GetBagByPos(i))
            for (uint32 j = 0; j < pBag->GetBagSize(); j++)
                if (Item* pItem = Killed->GetItemByPos(i, j))
                    if (KList.find(pItem->GetEntry()) == KList.end())
                        KList.insert(std::make_pair(pItem->GetEntry(), pItem->GetCount()));

    if (KList.empty())
        return;

    std::vector<std::pair<uint32, uint32>> RList;
    RList.clear();

    for (auto Itr = KList.begin(); Itr != KList.end(); Itr++)
    {
        auto It = PVPKStore.find(Itr->first);
        if (It != PVPKStore.end())
        {
            if (roll_chance_f(It->second.Odds))
                RList.push_back(std::make_pair(It->first, urand(It->second.MinCount, It->second.MaxCount)));
        }
    }

    if (RList.empty())
        return;

    if (GameObject* Go = Killed->SummonGameObject(500001, Killed->GetPositionX(), Killed->GetPositionY(), Killed->GetPositionZ(), Killed->GetOrientation(), 0, 0, 0, 0, 2 * MINUTE))
    {
        Go->SetOwnerGUID(ObjectGuid::Empty);
        Loot* OnLoot = &Go->loot;
        OnLoot->clear();

        OnLoot->FillLoot(0, LootTemplates_Gameobject, Killed, false, true, Go->GetLootMode(), Go);

        for (auto Itr = RList.begin(); Itr != RList.end(); Itr++)
        {
            Killed->DestroyItemCount(Itr->first, Itr->second, true);

            Killer->SendSysMsg("你击杀玩家 %s 宝物 %s X %u 被放入箱子中", sHelper->GetNameLinkColor(Killed).c_str(), sHelper->GetItemLink(Itr->first).c_str(), Itr->second);
            Killed->SendSysMsg("你被玩家 %s 击杀损失了 %s X %u 你的宝物被放入箱子中", sHelper->GetNameLinkColor(Killer).c_str(), sHelper->GetItemLink(Itr->first).c_str(), Itr->second);

            LootStoreItem StoreItem = LootStoreItem(Itr->first, 0, 100.0f, false, LOOT_MODE_DEFAULT, 0, Itr->second, Itr->second);
            OnLoot->AddItem(StoreItem);
        }
    }

    KList.clear();
    RList.clear();
}

void PlayerModMgr::LoadSignMod()
{
    SignRewardStore.clear();
    uint32 Ct = 0;
    QueryResult Result = WorldDatabase.Query("SELECT 日期, 物品1, 数量1, 物品2, 数量2, 物品3, 数量3, 物品4, 数量4, 物品5, 数量5, 物品6, 数量6 FROM _签到奖励表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ItemId = Fields[0].Get<uint32>();
            SignTemp St;
            St.ItemId1 = Fields[1].Get<uint32>();
            St.Count1 = Fields[2].Get<uint32>();
            St.ItemId2 = Fields[3].Get<uint32>();
            St.Count2 = Fields[4].Get<uint32>();
            St.ItemId3 = Fields[5].Get<uint32>();
            St.Count3 = Fields[6].Get<uint32>();
            St.ItemId4 = Fields[7].Get<uint32>();
            St.Count4 = Fields[8].Get<uint32>();
            St.ItemId5 = Fields[9].Get<uint32>();
            St.Count5 = Fields[10].Get<uint32>();
            St.ItemId6 = Fields[11].Get<uint32>();
            St.Count6 = Fields[12].Get<uint32>();
            SignRewardStore.insert(std::make_pair(ItemId, St));
            ++Ct;
        } while (Result->NextRow());
    }
}

void PlayerModMgr::CustomKillPlayer(Player* Killer, Player* Killed, uint32 KillRewTempId, float KillRewOdds, uint32 KilledRewTempId, float KilledRewOdds, bool One)
{
    if (KillRewTempId)
    {
        if (roll_chance_f(KillRewOdds))
        {
            if (!One)
                sHelper->DoTheReward(Killer, KillRewTempId, "玩家击杀玩家奖励");
            else if (strcmp(Killer->GetSession()->GetRemoteAddress().c_str(), Killed->GetSession()->GetRemoteAddress().c_str()) != 0)
                sHelper->DoTheReward(Killer, KillRewTempId, "玩家击杀玩家奖励");
        }
    }

    if (KilledRewTempId)
    {
        if (roll_chance_f(KilledRewOdds))
        {
            if (!One)
                sHelper->DoTheReward(Killed, KilledRewTempId, "玩家被杀奖励");
            else if (strcmp(Killer->GetSession()->GetRemoteAddress().c_str(), Killed->GetSession()->GetRemoteAddress().c_str()) != 0)
                sHelper->DoTheReward(Killed, KilledRewTempId, "玩家被杀奖励");
        }
    }
}

void PlayerModMgr::OnKillPlayerReward(Player* Killer, Player* Killed)
{
    if (Killer->GetGUID() == Killed->GetGUID())
        return;

    if (PVPMapEventStore.empty())
        return;

    for (auto Itr = PVPMapEventStore.begin(); Itr != PVPMapEventStore.end(); ++Itr)
    {
        if (!Itr->first)
        {
            if (Itr->second.MapOrArea == -1)
            {
                CustomKillPlayer(Killer, Killed, Itr->second.KillRewTempId, Itr->second.KillRewOdds, Itr->second.KilledRewTempId, Itr->second.KilledRewOdds, Itr->second.OneIp);
            }
            else
            {
                if (Itr->second.MapOrArea > -1)
                {
                    if (int32(Killer->GetMapId()) == Itr->second.MapOrArea)
                    {
                        CustomKillPlayer(Killer, Killed, Itr->second.KillRewTempId, Itr->second.KillRewOdds, Itr->second.KilledRewTempId, Itr->second.KilledRewOdds, Itr->second.OneIp);
                    }
                }
                else
                {
                    if (-(int32)Killer->GetAreaId() == Itr->second.MapOrArea)
                    {
                        CustomKillPlayer(Killer, Killed, Itr->second.KillRewTempId, Itr->second.KillRewOdds, Itr->second.KilledRewTempId, Itr->second.KilledRewOdds, Itr->second.OneIp);
                    }
                }
            }
        }
        else
        {
            if (sGameEventMgr->IsActiveEvent(Itr->first))
            {
                if (Itr->second.MapOrArea == -1)
                {
                    CustomKillPlayer(Killer, Killed, Itr->second.KillRewTempId, Itr->second.KillRewOdds, Itr->second.KilledRewTempId, Itr->second.KilledRewOdds, Itr->second.OneIp);
                }
                else
                {
                    if (Itr->second.MapOrArea > -1)
                    {
                        if (int32(Killer->GetMapId()) == Itr->second.MapOrArea)
                        {
                            CustomKillPlayer(Killer, Killed, Itr->second.KillRewTempId, Itr->second.KillRewOdds, Itr->second.KilledRewTempId, Itr->second.KilledRewOdds, Itr->second.OneIp);
                        }
                    }
                    else
                    {
                        if (-(int32)Killer->GetAreaId() == Itr->second.MapOrArea)
                        {
                            CustomKillPlayer(Killer, Killed, Itr->second.KillRewTempId, Itr->second.KillRewOdds, Itr->second.KilledRewTempId, Itr->second.KilledRewOdds, Itr->second.OneIp);
                        }
                    }
                }
            }
        }
    }
}

void PlayerModMgr::PlayerOnLoginStatPt(Player* player)
{
    if (IsXsPlayerbot(player))
        return;

    for (size_t i = AT21; i < ATMAX; i++)
        sCustomClass->ApplyPlayerStat(player, sCustomClass->AotoTempAt(i, false), player->TempAtStore[i].Value, true);
}

void PlayerModMgr::StatPtOnCreatureKill(Player* player, Creature* Me)
{
    uint32 CreatureId = Me->GetEntry();
    auto Itr = StatPtRewStore.find(CreatureId);
    if (Itr == StatPtRewStore.end())
        return;

    uint32 Max = sCustomClass->GetStatMaxRank();
    Group* Gp = Me->GetLootRecipientGroup();
    uint32 TarRank = Itr->second.ToRank;

    if (TarRank)
    {
        if (Gp)
        {
            uint32 Ptn = uint32(TarRank / float(Gp->GetMembersCount()));
            TarRank = Ptn > 0 ? Ptn : 1;

            for (GroupReference* Itr = Gp->GetFirstMember(); Itr != nullptr; Itr = Itr->next())
            {
                Player* Member = Itr->GetSource();
                if (Member && Member->IsInWorld() && Me->IsInMap(Member))
                {
                    uint32 NowRank = AsInt64ToUInt32Clamp(Member->TempAtStore[AT11].Value);
                    if (TarRank > NowRank && NowRank < Max)
                    {
                        TarRank = (TarRank > Max) ? Max : TarRank;

                        Member->TempAtStore[AT11] += (TarRank - NowRank);
                        Member->TempAtStore[AT13] += sCustomClass->GetStatOldNewRewCount(NowRank, AsInt64ToUInt32Clamp(Member->TempAtStore[AT11].Value));

                        Member->CastSpell(Member, TxLevelSpell, false);
                        sCustomClass->SendUpdatePlayerStatData(Member);
                        Member->SendSysMsg(sHelper->GetGts(2001), TarRank);
                    }
                }
            }
        }
        else
        {
            uint32 NowRank = AsInt64ToUInt32Clamp(player->TempAtStore[AT11].Value);
            if (TarRank > NowRank && NowRank < Max)
            {
                uint32 Max = sCustomClass->GetStatMaxRank();
                TarRank = (TarRank > Max) ? Max : TarRank;
                player->TempAtStore[AT11] += (TarRank - NowRank);
                player->TempAtStore[AT13] += sCustomClass->GetStatOldNewRewCount(NowRank, AsInt64ToUInt32Clamp(player->TempAtStore[AT11].Value));

                player->CastSpell(player, TxLevelSpell, false);
                sCustomClass->SendUpdatePlayerStatData(player);
                player->SendSysMsg(sHelper->GetGts(2001), TarRank);
            }
        }
    }

    AsInt64 Xp = Itr->second.RewXp;
    if (Xp)
    {
        if (Gp)
        {
            AsInt64 Ptn = Xp / Gp->GetMembersCount();
            Xp = Ptn > 0 ? Ptn : AsInt64(1);
            Xp = sHelper->GetBoolean(12999) ? Xp : Itr->second.RewXp;

            for (GroupReference* Itr = Gp->GetFirstMember(); Itr != nullptr; Itr = Itr->next())
            {
                Player* Member = Itr->GetSource();
                if (Member && Member->IsInWorld() && Me->IsInMap(Member))
                {
                    AsInt64 NXp = sAuraSpellMgr->GetStatNowHook(Member, Xp);
                    AsInt64 NextXp = sCustomClass->GetStatNeedXp(AsInt64ToUInt32Clamp(Member->TempAtStore[AT11].Value));
                    AsInt64 NewXp = Member->TempAtStore[AT12].Value + NXp;
                    if (!NextXp)
                    {
                        Member->SendSysMsg(sHelper->GetGts(2003));
                    }
                    else
                    {
                        while (NewXp >= NextXp && NextXp)
                        {
                            NewXp -= NextXp;

                            if (NextXp)
                            {
                                Member->TempAtStore[AT11] += 1;
                                uint32 newRank = AsInt64ToUInt32Clamp(Member->TempAtStore[AT11].Value);
                                Member->TempAtStore[AT13] += sCustomClass->GetStatRewCount(newRank);
                                Member->CastSpell(Member, TxLevelSpell, false);
                                Member->SendSysMsg(sHelper->GetGts(2001), newRank);
                            }

                            NextXp = sCustomClass->GetStatNeedXp(AsInt64ToUInt32Clamp(Member->TempAtStore[AT11].Value));
                        }

                        Member->TempAtStore[AT12] = NewXp;
                        sCustomClass->SendUpdatePlayerStatData(Member);
                        Member->SendSysMsg("[温馨提示]: 获得斗气经验 %s", AsInt64ToString(NXp).c_str());
                    }
                }
            }
        }
        else
        {
            AsInt64 NXp = sAuraSpellMgr->GetStatNowHook(player, Xp);
            AsInt64 NextXp = sCustomClass->GetStatNeedXp(AsInt64ToUInt32Clamp(player->TempAtStore[AT11].Value));
            AsInt64 NewXp = player->TempAtStore[AT12].Value + NXp;
            if (!NextXp)
            {
                player->SendSysMsg(sHelper->GetGts(2003));
            }
            else
            {
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
            }
        }
    }

    bool AddRank = Itr->second.AddRank;
    if (AddRank)
    {
        uint32 Max = sCustomClass->GetStatMaxRank();
        if (Gp)
        {
            for (GroupReference* Itr = Gp->GetFirstMember(); Itr != nullptr; Itr = Itr->next())
            {
                Player* Member = Itr->GetSource();
                if (Member && Member->IsInWorld() && Me->IsInMap(Member))
                {
                    uint32 NowRank = AsInt64ToUInt32Clamp(Member->TempAtStore[AT11].Value);
                    if (NowRank < Max)
                    {
                        uint32 ARank = NowRank + 1;
                        uint32 Max = sCustomClass->GetStatMaxRank();
                        ARank = (ARank > Max) ? Max : ARank;
                        Member->TempAtStore[AT11] = ARank;
                        Member->TempAtStore[AT13] += sCustomClass->GetStatRewCount(ARank);
                        Member->CastSpell(Member, TxLevelSpell, false);
                        sCustomClass->SendUpdatePlayerStatData(Member);
                        Member->SendSysMsg(sHelper->GetGts(2001), ARank);
                    }
                }
            }
        }
        else
        {
            uint32 NowRank = AsInt64ToUInt32Clamp(player->TempAtStore[AT11].Value);
            if (NowRank < Max)
            {
                uint32 ARank = NowRank + 1;
                uint32 Max = sCustomClass->GetStatMaxRank();
                ARank = (ARank > Max) ? Max : ARank;
                player->TempAtStore[AT11] = ARank;
                player->TempAtStore[AT13] += sCustomClass->GetStatRewCount(ARank);
                player->CastSpell(player, TxLevelSpell, false);
                sCustomClass->SendUpdatePlayerStatData(player);
                player->SendSysMsg(sHelper->GetGts(2001), ARank);
            }
        }
    }
}

void PlayerModMgr::AuraLootOnCreatureKill(Player* player, Creature* Me)
{
    if (Me->IsCharmedOwnedByPlayerOrPlayer() || Me->IsControlledByPlayer() || Me->IsHunterPet())
        return;

    bool Ret = false;
    for (auto Itr = AuraLootItemStore.begin(); Itr != AuraLootItemStore.end(); ++Itr)
    {
        if (player->HasAura(Itr->first))
        {
            if (roll_chance_f(Itr->second.Odds))
            {
                if (Itr->second.ItemId1 > 0 && Itr->second.Count1 > 0)
                {
                    Ret = true;
                    player->AddItem(Itr->second.ItemId1, Itr->second.Count1, "光环杀怪获得");
                }
                if (Itr->second.ItemId2 > 0 && Itr->second.Count2 > 0)
                {
                    Ret = true;
                    player->AddItem(Itr->second.ItemId2, Itr->second.Count2, "光环杀怪获得");
                }
            }
        }
    }

    if (Ret)
        player->SendSysMsg(sHelper->GetGts(2601));
}

void PlayerModMgr::OnTeleSpellComplete(Player* player, Spell* DoSpell, bool SkipCheck)
{
    if (sHelper->GetBoolean(2000) && sHelper->GetUInt(2001) > 0)
    {
        if (DoSpell->m_spellInfo && DoSpell->m_spellInfo->Id == sHelper->GetUInt(2001) && player->TelePos)
        {
            player->TeleToPos(player->TelePos);
            player->TelePos = 0;
        }
    }
}

void PlayerModMgr::OnCheckAuraPlayerMapChanged(Player* player)
{
    uint32 AuraId = uint32(sHelper->GetInt(2103));
    bool InBG = (player->InBattleground() || player->InArena());
    if (InBG && sHelper->GetBoolean(2105))
    {
        if (player->HasAura(AuraId))
            player->RemoveAurasDueToSpell(AuraId);
    }
    else
    {
        if (!player->HasAura(AuraId) && player->GetTempAtUInt32(AT2001) == AuraId)
            player->AddAura(AuraId, player);
    }
}

void PlayerModMgr::OnKillAuraPlayer(Player* Killer, Player* Killed)
{
    if (Killer->GetGUID() == Killed->GetGUID())
        return;

    if (strcmp(Killer->GetSession()->GetRemoteAddress().c_str(), Killed->GetSession()->GetRemoteAddress().c_str()) == 0)
        return;

    uint32 KItemId = uint32(sHelper->GetInt(2101));
    uint32 KCount = uint32(sHelper->GetInt(2104));
    if (!KItemId || !KCount)
        return;

    bool InBG = Killer->InBattleground();
    uint32 AuraId = uint32(sHelper->GetInt(2103));
    if (InBG && sHelper->GetBoolean(2105))
        return;

    if (Killed->HasAura(AuraId))
    {
        Killed->RemoveAurasDueToSpell(AuraId);
        Killed->UpdateTempAt(AT2001, 0);
        Killer->AddItem(KItemId, KCount, "狂暴之力击杀获得");
    }
}

void PlayerModMgr::PlayerCastSpellComplete(Player* player, Spell* DoSpell, bool SkipCheck)
{
    uint32 SpellId = DoSpell->GetSpellInfo()->Id;
    auto Itr = SpellCastStore.find(SpellId);
    if (Itr != SpellCastStore.end())
    {
        player->DoCommands(Itr->second);
    }
}

void PlayerModMgr::LoadEqSlotUpConfig()
{
    EqSlotTempStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 序号, 等级, 增加属性百分比, 需求模版 FROM _装备栏强化;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Slot = Fields[0].Get<uint32>();
            EqSlotTemp St;
            St.Rank = Fields[1].Get<uint32>();
            St.Mod = Fields[2].Get<float>();
            St.NTempId = Fields[3].Get<uint32>();
            if (St.Mod < 0.0f)
                St.Mod = 1.0f;

            EqSlotTempStore.insert(std::make_pair(Slot, St));
        } while (Result->NextRow());
    }

    EqAuraStore.clear();
    Result = WorldDatabase.Query("SELECT 全身等级, 套装光环 FROM _装备栏拓展;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 ARank = Fields[0].Get<uint32>();
            std::string Auras = Fields[1].Get<std::string>();
            std::set<uint32> AList;
            if (!Auras.empty())
            {
                Tokenizer Datas(Auras, '#');
                for (auto Itr = Datas.begin(); Itr != Datas.end(); ++Itr)
                    AList.insert(uint32(atoi(*Itr)));
            }

            EqAuraStore.insert(std::make_pair(ARank, AList));
        } while (Result->NextRow());
    }
}

float PlayerModMgr::GetModPlayerEqSlot(Player* player, uint32 Slot)
{
    uint32 SlotRank = player->TempAtStore[Slot + AtCBeg];
    if (!SlotRank)
        return 0.0f;

    uint32 NSlot = AutoEqSlot(Slot, true);
    auto Itr = EqSlotTempStore.find(NSlot);

    if (Itr != EqSlotTempStore.end())
    {
        for (int i = 0; i < EqSlotTempStore.count(NSlot); i++, Itr++)
        {
            if (Itr->second.Rank == SlotRank)
                return Itr->second.Mod / 100.0f;
        }
    }

    return 0.0f;
}

float PlayerModMgr::GetModShowPctSlot(uint32 NowRank, uint32 Slot)
{
    if (!NowRank)
        return 0;

    auto Itr = EqSlotTempStore.find(Slot);

    if (Itr != EqSlotTempStore.end())
    {
        for (int i = 0; i < EqSlotTempStore.count(Slot); i++, Itr++)
        {
            if (Itr->second.Rank == NowRank)
                return Itr->second.Mod;
        }
    }

    return 0;
}

uint32 PlayerModMgr::GetMaxRank(uint32 Slot)
{
    auto Itr = EqSlotTempStore.find(Slot);
    return Itr != EqSlotTempStore.end() ? EqSlotTempStore.count(Slot) : 0;
}

uint32 PlayerModMgr::AutoEqSlot(uint32 Slot, bool Auto)
{
    if (Auto)
    {
        switch (Slot)
        {
        case EQUIPMENT_SLOT_HEAD: return 1;                           //
        case EQUIPMENT_SLOT_NECK: return 2;                           //
        case EQUIPMENT_SLOT_SHOULDERS: return 3;                //
        case EQUIPMENT_SLOT_BODY: return 6;                           //
        case EQUIPMENT_SLOT_CHEST: return 5;                         //
        case EQUIPMENT_SLOT_WAIST: return 10;                          //
        case EQUIPMENT_SLOT_LEGS: return 11;                      //
        case EQUIPMENT_SLOT_FEET: return 12;                       //
        case EQUIPMENT_SLOT_WRISTS: return 8;                       //
        case EQUIPMENT_SLOT_HANDS: return 9;                         //
        case EQUIPMENT_SLOT_FINGER1: return 13;                         //
        case EQUIPMENT_SLOT_FINGER2: return 14;                          //
        case EQUIPMENT_SLOT_TRINKET1: return 15;                   //
        case EQUIPMENT_SLOT_TRINKET2: return 16;                   //
        case EQUIPMENT_SLOT_BACK: return 4;                  //
        case EQUIPMENT_SLOT_MAINHAND: return 17;                  //
        case EQUIPMENT_SLOT_OFFHAND: return 18;              //
        case EQUIPMENT_SLOT_RANGED: return 19;                 //
        case EQUIPMENT_SLOT_TABARD: return 7;                   //
        }
    }
    else
    {
        switch (Slot)
        {
        case 1: return EQUIPMENT_SLOT_HEAD;                           //
        case 2: return EQUIPMENT_SLOT_NECK;                           //
        case 3: return EQUIPMENT_SLOT_SHOULDERS;                //
        case 6: return EQUIPMENT_SLOT_BODY;                           //
        case 5: return EQUIPMENT_SLOT_CHEST;                         //
        case 10: return EQUIPMENT_SLOT_WAIST;                          //
        case 11: return EQUIPMENT_SLOT_LEGS;                      //
        case 12: return EQUIPMENT_SLOT_FEET;                       //
        case 8: return EQUIPMENT_SLOT_WRISTS;                       //
        case 9: return EQUIPMENT_SLOT_HANDS;                         //
        case 13: return EQUIPMENT_SLOT_FINGER1;                         //
        case 14: return EQUIPMENT_SLOT_FINGER2;                          //
        case 15: return EQUIPMENT_SLOT_TRINKET1;                   //
        case 16: return EQUIPMENT_SLOT_TRINKET2;                   //
        case 4: return EQUIPMENT_SLOT_BACK;                  //
        case 17: return EQUIPMENT_SLOT_MAINHAND;                  //
        case 18: return EQUIPMENT_SLOT_OFFHAND;              //
        case 19: return EQUIPMENT_SLOT_RANGED;                 //
        case 7: return EQUIPMENT_SLOT_TABARD;                   //
        }
    }

    return EQUIPMENT_SLOT_START;
}

void PlayerModMgr::SendApplyMeEqSlotData(Player* player)
{
    std::string SendMsg;
    SendMsg.clear();
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; i++)
    {
        uint32 Slot = AutoEqSlot(i, true);
        uint32 SlotRank = player->TempAtStore[i + AtCBeg];
        float Mod = sPlayerModMgr->GetModShowPctSlot(SlotRank, Slot);
        uint32 MaxRank = sPlayerModMgr->GetMaxRank(Slot);
        SendMsg.append(std::to_string(Slot) + "#" + std::to_string(SlotRank) + "#" + std::to_string(MaxRank) + "#" + std::to_string(Mod) + "#$");
    }

    sDoAddon->SendPacketToClient(player, "ApplyMeEqSlotData", SendMsg);
    SendMsg.clear();
}

uint32 PlayerModMgr::GetEqSlotNTempId(uint32 NowRank, uint32 Slot)
{
    uint32 Next = NowRank + 1;
    if (Next > GetMaxRank(Slot))
        return 0;

    auto Itr = EqSlotTempStore.find(Slot);

    if (Itr != EqSlotTempStore.end())
    {
        for (int i = 0; i < EqSlotTempStore.count(Slot); i++, Itr++)
        {
            if (Itr->second.Rank == Next)
                return Itr->second.NTempId;
        }
    }

    return 0;
}

bool PlayerModMgr::IsAsEqRank(Player* player, uint32 Rank)
{
    if (!Rank)
        return true;
    bool On = true;
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; i++)
    {
        uint32 SlotRank = player->TempAtStore[i + AtCBeg];
        if (SlotRank < Rank)
            On = false;
    }
    return On;
}

void PlayerModMgr::CheckEqRankGiveAura(Player* player, uint32 DoRank, bool Login)
{
    if (!DoRank)
        return;
    auto Itr = EqAuraStore.find(DoRank);
    if (Itr == EqAuraStore.end())
        return;
    if (Itr->second.empty())
        return;
    if (!IsAsEqRank(player, DoRank))
        return;

    for (auto It = player->EqAuras.begin(); It != player->EqAuras.end(); It++)
    {
        uint32 Aura = *It;
        if (player->HasAura(Aura))
            player->RemoveAurasDueToSpell(Aura);
    }

    player->EqAuras.clear();
    for (auto It = Itr->second.begin(); It != Itr->second.end(); It++)
    {
        uint32 Aura = *It;
        player->EqAuras.insert(Aura);
        if (!player->HasAura(Aura))
            player->AddAura(Aura, player);

        if (!Login)
            player->SendSysMsg(sHelper->GetGts(6012), Itr->first, sHelper->GetSpellName(Aura));
    }
}

void PlayerModMgr::HandlerEqRankGiveAuraLogin(Player* player)
{
    if (IsXsPlayerbot(player))
        return;

    std::vector<uint32> List;
    for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; i++)
    {
        uint32 SlotRank = player->TempAtStore[i + AtCBeg];
        List.push_back(SlotRank);
    }
    if (List.empty())
        return;

    std::sort(List.begin(), List.end());
    uint32 DoRank = List[0];
    CheckEqRankGiveAura(player, DoRank, true);
}

void PlayerModMgr::DoAutoStoreLoot(Player* player, uint32 loot_id, LootStore const& store)
{
    Loot loot;
    loot.FillLoot(loot_id, store, player, true);

    uint32 max_slot = loot.GetMaxSlotInLootFor(player);
    for (uint32 i = 0; i < max_slot; ++i)
    {
        LootItem const* LItem = &loot.items[i];
        if (!LItem)
            continue;
        if (LItem->is_looted)
            continue;

        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(LItem->itemid))
            if ((proto->Flags & ITEM_FLAG_MULTI_DROP) && proto->IsCurrencyToken())
                continue;

        InventoryResult msg;
        player->StoreLootItem(i, &loot, msg);
    }
}

void PlayerModMgr::OnDKSkipPlayer(Player* player)
{
    if (!sHelper->GetBoolean(5000))
        return;
    if (player->GetAreaId() != 4342)
        return;
    if (player->GetClass() != CLASS_DEATH_KNIGHT)
        return;

    uint8 StartLevel = sConfigMgr->GetOption<int32>("StartHeroicPlayerLevel", 55);
    player->learnSpell(53428);
    player->learnSpell(53441);
    player->learnSpell(53344);
    player->learnSpell(62158);
    player->learnSpell(33391);
    player->learnSpell(54586);
    player->learnSpell(48778);
    player->learnSkillRewardedSpells(776, 375);
    player->learnSkillRewardedSpells(960, 375);
    player->EquipNewItem(EQUIPMENT_SLOT_HEAD, 38661, true);
    player->EquipNewItem(EQUIPMENT_SLOT_WRISTS, 38666, true);
    player->EquipNewItem(EQUIPMENT_SLOT_WAIST, 38668, true);
    player->EquipNewItem(EQUIPMENT_SLOT_HANDS, 38667, true);
    player->EquipNewItem(EQUIPMENT_SLOT_CHEST, 38665, true);
    player->EquipNewItem(EQUIPMENT_SLOT_LEGS, 38669, true);
    player->EquipNewItem(EQUIPMENT_SLOT_SHOULDERS, 38663, true);
    player->EquipNewItem(EQUIPMENT_SLOT_FEET, 38670, true);
    player->EquipNewItem(EQUIPMENT_SLOT_TRINKET1, 38675, true);
    player->EquipNewItem(EQUIPMENT_SLOT_TRINKET2, 38674, true);
    player->EquipNewItem(EQUIPMENT_SLOT_FINGER1, 38671, true);
    player->EquipNewItem(EQUIPMENT_SLOT_FINGER2, 38672, true);
    player->AddItem(39320, 1);
    player->AddItem(38664, 1);
    player->AddItem(39322, 1);
    player->AddItem(38632, 1);
    player->AddItem(6948, 1);
    player->AddItem(38707, 1);
    player->AddItem(40483, 1);

    if (player->GetQuestStatus(12657) == QUEST_STATUS_NONE)
    {
        player->AddQuest(sObjectMgr->GetQuestTemplate(12657), nullptr);
        player->RewardQuest(sObjectMgr->GetQuestTemplate(12657), false, player);
    }

    if (player->GetQuestStatus(12801) == QUEST_STATUS_NONE)
    {
        player->AddQuest(sObjectMgr->GetQuestTemplate(12801), nullptr);
        player->RewardQuest(sObjectMgr->GetQuestTemplate(12801), false, player);
    }

    if (player->GetTeamId() == TEAM_ALLIANCE && player->GetQuestStatus(13188) == QUEST_STATUS_NONE)
        player->AddQuest(sObjectMgr->GetQuestTemplate(13188), nullptr);
    else if (player->GetTeamId() == TEAM_HORDE && player->GetQuestStatus(13189) == QUEST_STATUS_NONE)
        player->AddQuest(sObjectMgr->GetQuestTemplate(13189), nullptr);

    player->GiveLevel(StartLevel);
    player->TeleToPos(sHelper->GetUInt(5001));
    player->SaveToDB(false, false);
}

void PlayerModMgr::OnCheckCanFlyPlayerMapChanged(Player* player)
{
    if (!sMapAreaFunc->AllowMountHandler(player))
    {
        player->RemoveAurasByType(SPELL_AURA_MOUNTED);
    }

    if (!sMapAreaFunc->AllowFlyMountHandler(player))
    {
        player->SetCanFly(false);
        player->RemoveAurasByType(SPELL_AURA_FLY);
        player->RemoveAurasByType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED);
        player->GetMotionMaster()->MoveFall();
    }

    if (!sHelper->CanCustomFly(player))
    {
        player->SetCanFly(false);
        player->RemoveAurasByType(SPELL_AURA_FLY);
        player->RemoveAurasByType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED);
        player->GetMotionMaster()->MoveFall();
    }
}

void PlayerModMgr::LoadEnMapAuraData()
{
    EnMapPlayerStore.clear();
    QueryResult Result = WorldDatabase.Query("SELECT 地图编号, 阵营, 职业掩码, 光环组 FROM _地图玩家光环表;");
    if (!Result)
    {

    }
    else
    {
        do
        {
            Field* Fields = Result->Fetch();
            int32 MapOrAreaId = Fields[0].Get<int32>();
            EnMapTemp Temp;
            Temp.FactionId = Fields[1].Get<uint32>();
            Temp.Class = Fields[2].Get<uint32>();
            std::string Auras = Fields[3].Get<std::string>();
            if (!Auras.empty())
            {
                Tokenizer Datas(Auras, '#');
                for (auto Itr = Datas.begin(); Itr != Datas.end(); ++Itr)
                    Temp.AuraList.insert(uint32(atoi(*Itr)));
            }
            EnMapPlayerStore.insert(std::make_pair(MapOrAreaId, Temp));
        } while (Result->NextRow());
    }
}

void PlayerModMgr::HandlerEnMapAura(Player* player)
{
    for (auto Itr = EnMapPlayerStore.begin(); Itr != EnMapPlayerStore.end(); Itr++)
    {
        for (auto It = Itr->second.AuraList.begin(); It != Itr->second.AuraList.end(); It++)
        {
            uint32 Aura = *It;
            if (player->HasAura(Aura))
                player->RemoveAurasDueToSpell(Aura);
        }
    }

    uint32 FactionId = player->GetTeamId() == TEAM_ALLIANCE ? 1 : 2;
    int32 NowMapId = (int32)player->GetMapId();
    auto Itr = EnMapPlayerStore.find(NowMapId);
    if (Itr != EnMapPlayerStore.end())
    {
        if ((Itr->second.FactionId == 0 || FactionId == Itr->second.FactionId) && (sHelper->IsClassInMask(player->GetClass(), Itr->second.Class)))
        {
            for (auto It = Itr->second.AuraList.begin(); It != Itr->second.AuraList.end(); It++)
            {
                uint32 Aura = *It;
                if (!player->HasAura(Aura))
                    player->AddAura(Aura, player);
            }
        }
    }

    int32 NowAreaId = -(int32)player->GetAreaId();
    Itr = EnMapPlayerStore.find(NowAreaId);
    if (Itr != EnMapPlayerStore.end())
    {
        if ((Itr->second.FactionId == 0 || FactionId == Itr->second.FactionId) && (sHelper->IsClassInMask(player->GetClass(), Itr->second.Class)))
        {
            for (auto It = Itr->second.AuraList.begin(); It != Itr->second.AuraList.end(); It++)
            {
                uint32 Aura = *It;
                if (!player->HasAura(Aura))
                    player->AddAura(Aura, player);
            }
        }
    }
}

void PlayerModMgr::HandlerEnAreaAura(Player* player)
{
    for (auto Itr = EnMapPlayerStore.begin(); Itr != EnMapPlayerStore.end(); Itr++)
    {
        for (auto It = Itr->second.AuraList.begin(); It != Itr->second.AuraList.end(); It++)
        {
            uint32 Aura = *It;
            if (player->HasAura(Aura))
                player->RemoveAurasDueToSpell(Aura);
        }
    }

    uint32 FactionId = player->GetTeamId() == TEAM_ALLIANCE ? 1 : 2;
    int32 NowMapId = (int32)player->GetMapId();
    auto Itr = EnMapPlayerStore.find(NowMapId);
    if (Itr != EnMapPlayerStore.end())
    {
        if ((Itr->second.FactionId == 0 || FactionId == Itr->second.FactionId) && (sHelper->IsClassInMask(player->GetClass(), Itr->second.Class)))
        {
            for (auto It = Itr->second.AuraList.begin(); It != Itr->second.AuraList.end(); It++)
            {
                uint32 Aura = *It;
                if (!player->HasAura(Aura))
                    player->AddAura(Aura, player);
            }
        }
    }

    int32 NowAreaId = -(int32)player->GetAreaId();
    Itr = EnMapPlayerStore.find(NowAreaId);
    if (Itr != EnMapPlayerStore.end())
    {
        if ((Itr->second.FactionId == 0 || FactionId == Itr->second.FactionId) && (sHelper->IsClassInMask(player->GetClass(), Itr->second.Class)))
        {
            for (auto It = Itr->second.AuraList.begin(); It != Itr->second.AuraList.end(); It++)
            {
                uint32 Aura = *It;
                if (!player->HasAura(Aura))
                    player->AddAura(Aura, player);
            }
        }
    }
}

uint32 PlayerModMgr::GetVipNTempId(uint32 Level)
{
    auto Itr = VipStore.find(Level);
    return (Itr != VipStore.end()) ? Itr->second.NTempId : 0;
}

uint32 PlayerModMgr::GetVipRTempId(uint32 Level)
{
    auto Itr = VipStore.find(Level);
    return (Itr != VipStore.end()) ? Itr->second.RTempId : 0;
}

float PlayerModMgr::GetVipStatsMod(uint32 Level)
{
    auto Itr = VipStore.find(Level);
    return (Itr != VipStore.end()) ? Itr->second.StatsMod : 0.0f;
}

uint32 PlayerModMgr::GetVipTpCount(uint32 Level)
{
    auto Itr = VipStore.find(Level);
    return (Itr != VipStore.end()) ? Itr->second.Tp : 0;
}

float PlayerModMgr::GetVipLootOdds(uint32 Level)
{
    auto Itr = VipStore.find(Level);
    return (Itr != VipStore.end()) ? Itr->second.LootOdds : 0.0f;
}

float PlayerModMgr::GetVipLootKn(uint32 Level)
{
    auto Itr = VipStore.find(Level);
    return (Itr != VipStore.end()) ? Itr->second.LootKn : 0.0f;
}

float PlayerModMgr::GetVipUpOdds(uint32 Level)
{
    auto Itr = VipStore.find(Level);
    return (Itr != VipStore.end()) ? Itr->second.UpOdds : 0.0f;
}

std::string PlayerModMgr::GetVipAuras(uint32 Level)
{
    auto Itr = VipStore.find(Level);
    return (Itr != VipStore.end()) ? Itr->second.AtAuras : "";
}

void PlayerModMgr::UpdateVipAuras(Player* player, uint32 NewRank)
{
    if (VipStore.empty())
        return;

    if (IsXsPlayerbot(player))
        return;

    if (!NewRank)
    {
        for (auto Itr = VipStore.begin(); Itr != VipStore.end(); ++Itr)
        {
            if (!Itr->second.AtAuras.empty())
            {
                Tokenizer AuraData(Itr->second.AtAuras, '#');
                for (auto const& Temp : AuraData)
                {
                    uint32 Aura = atoi(Temp);
                    if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(Aura))
                    {
                        if (player->HasAura(Aura))
                            player->RemoveAurasDueToSpell(Aura);
                    }
                }
            }
        }
        return;
    }

    for (auto It = VipStore.begin(); It != VipStore.end(); ++It)
    {
        if (!It->second.AtAuras.empty())
        {
            Tokenizer AuraData(It->second.AtAuras, '#');
            for (auto const& Temp : AuraData)
            {
                uint32 Aura = atoi(Temp);
                if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(Aura))
                {
                    if (player->HasAura(Aura))
                        player->RemoveAurasDueToSpell(Aura);
                }
            }
        }
    }

    auto Iter = VipStore.find(NewRank);
    if (Iter != VipStore.end())
    {
        if (!Iter->second.AtAuras.empty())
        {
            Tokenizer AuraData(Iter->second.AtAuras, '#');
            for (auto const& Temp : AuraData)
            {
                uint32 Aura = atoi(Temp);
                if (SpellInfo const* Info = sSpellMgr->GetSpellInfo(Aura))
                {
                    if (!player->HasAura(Aura))
                        player->AddAura(Aura, player);
                }
            }
        }
    }
}

uint32 PlayerModMgr::GetVipMaxRank()
{
    if (VipStore.empty())
        return 0;

    auto Itr = VipStore.rbegin();
    return Itr->first;
}

uint32 PlayerModMgr::GetVipSTempId(uint32 Level)
{
    auto Itr = VipStore.find(Level);
    return (Itr != VipStore.end()) ? Itr->second.STempId : 0;
}

std::string PlayerModMgr::GetVipMenuStr(uint32 Level)
{
    auto Itr = VipStore.find(Level);
    return (Itr != VipStore.end()) ? Itr->second.MenuStr : "";
}

std::string PlayerModMgr::GetVipIconStr(uint32 Level)
{
    auto Itr = VipStore.find(Level);
    return (Itr != VipStore.end()) ? Itr->second.IconStr : "";
}

std::string PlayerModMgr::GetVipPixStr(uint32 Level)
{
    auto Itr = VipStore.find(Level);
    return (Itr != VipStore.end()) ? Itr->second.VipPix : "";
}

std::string PlayerModMgr::GetPlayerNameOriginal(std::string RevName)
{
    if (RevName.find("-") == std::string::npos)
        return RevName;

    Tokenizer Data(RevName, '-');
    return std::string(Data[Data.size() - 1]);
}

void PlayerModMgr::UpdateNameQuery(Player* player)
{
    ObjectGuid Guid = player->GetGUID();
    SessionMap const& Smap = sWorld->GetAllSessions();
    for (SessionMap::const_iterator Itr = Smap.begin(); Itr != Smap.end(); ++Itr)
    {
        if (Player* SocPlr = Itr->second->GetPlayer())
        {
            SocPlr->GetSession()->SendNameQueryOpcode(Guid);
            if (SocPlr->GetAreaId() == player->GetAreaId())
                sDoAddon->SendPacketToClient(SocPlr, "ResponsePlayerName");
        }
    }
}

void PlayerModMgr::DoOnlineRewardTimeFunc(Player* player, uint32 ptime)
{
    if (!sHelper->GetBoolean(1002))
        return;

    if (IsXsPlayerbot(player))
        return;

    uint64 Delay = sHelper->GetUInt(1003);
    if (!Delay)
        return;

    uint64 DelayTime = Delay * MINUTE * IN_MILLISECONDS;
    player->OnlineRewardTime += ptime;
    if (player->OnlineRewardTime >= DelayTime)
    {
        player->OnlineRewardTime = 0;
        uint32 RTempId = uint32(sHelper->GetInt(1004));
        if (RTempId > 0)
        {
            sHelper->DoTheReward(player, RTempId, "在线泡点奖励");
            player->SendSysMsg(sHelper->GetGts(1040));
        }
    }
}

bool PlayerModMgr::IsInDmgRank(uint32 CreEntry)
{
    auto Itr = CreDmgRankTempStore.find(CreEntry);
    return (Itr != CreDmgRankTempStore.end());
}

AsInt64 PlayerModMgr::GetDmgMin(uint32 CreEntry)
{
    auto Itr = CreDmgRankTempStore.find(CreEntry);
    return (Itr != CreDmgRankTempStore.end()) ? Itr->second.MinDmg : AsInt64Zero();
}

uint32 PlayerModMgr::GetIpCount(uint32 CreEntry)
{
    auto Itr = CreDmgRankTempStore.find(CreEntry);
    return (Itr != CreDmgRankTempStore.end()) ? Itr->second.IPCount : 0;
}

bool PlayerModMgr::IpCheck(Creature* Me, Player* player)
{
    uint32 MaxCount = GetIpCount(Me->GetEntry());
    if (!MaxCount)
        return true;

    uint32 Count = 0;

    for (auto Itr = Me->DmgRankStore.begin(); Itr != Me->DmgRankStore.end(); ++Itr)
        if (Player* pl = ObjectAccessor::FindPlayer(Itr->PlayerGuid))
            if (strcmp(pl->GetSession()->GetRemoteAddress().c_str(), player->GetSession()->GetRemoteAddress().c_str()) == 0)
                Count++;

    return Count <= MaxCount;
}

uint32 PlayerModMgr::GetDmgRankAsTempId(uint32 CreEntry)
{
    auto Itr = CreDmgRankTempStore.find(CreEntry);
    return (Itr != CreDmgRankTempStore.end()) ? Itr->second.TempIdAs : 0;
}

uint32 PlayerModMgr::GetDmgRankTempId(uint32 CreEntry, uint32 Rank)
{
    auto Itr = CreDmgRankTempStore.find(CreEntry);
    if (Itr == CreDmgRankTempStore.end())
        return 0;

    if (Rank > DMGRANKMAX)
        return 0;

    switch (Rank)
    {
    case 1: return Itr->second.TempId1;
    case 2: return Itr->second.TempId2;
    case 3: return Itr->second.TempId3;
    case 4: return Itr->second.TempId4;
    case 5: return Itr->second.TempId5;
    case 6: return Itr->second.TempId6;
    case 7: return Itr->second.TempId7;
    case 8: return Itr->second.TempId8;
    case 9: return Itr->second.TempId9;
    case 10: return Itr->second.TempId10;
    }

    return 0;
}

void PlayerModMgr::AtOnCreatureUpdate(Creature* Me, uint32 Diff)
{
    if (!sHelper->GetBoolean(1407))
        return;

    uint32 CreEntry = Me->GetEntry();
    if (!IsInDmgRank(CreEntry))
        return;
    if (Me->DmgRankStore.empty())
        return;

    Me->DmgRankTime += Diff;
    if (Me->DmgRankTime >= sHelper->GetUInt(9500))
    {
        Me->DmgRankTime = 0;
        SendUpdateDmgRanking(Me);
    }
}

void PlayerModMgr::DmgRankOnCreatureReset(Creature* Me)
{
    uint32 CreEntry = Me->GetEntry();
    if (IsInDmgRank(CreEntry))
    {
        SendResrtDmgRanking(Me);
        Me->DmgRankStore.clear();
    }
}

void PlayerModMgr::DmgRankOnCreatureDied(Creature* Me)
{
    if (!sHelper->GetBoolean(1407))
    {
        Me->DmgRankStore.clear();
        return;
    }

    uint32 CreEntry = Me->GetEntry();
    if (IsInDmgRank(CreEntry))
    {
        AsInt64 Min = GetDmgMin(CreEntry);
        uint32 RankIndex = 0;
        std::sort(Me->DmgRankStore.begin(), Me->DmgRankStore.end(), [](const DmgRankTemp& V1, const DmgRankTemp& V2) { return V1.Dmg > V2.Dmg; });
        for (auto Itr = Me->DmgRankStore.begin(); Itr != Me->DmgRankStore.end(); ++Itr)
        {
            RankIndex++;
            if (Player* player = ObjectAccessor::FindPlayer(Itr->PlayerGuid))
            {
                if (Itr->Dmg >= Min)
                {
                    if (IpCheck(Me, player))
                    {
                        uint32 TempId = GetDmgRankAsTempId(CreEntry);
                        sHelper->DoTheReward(player, TempId, "伤害排行榜参与奖励");
                        TempId = GetDmgRankTempId(CreEntry, RankIndex);
                        sHelper->DoTheReward(player, TempId, "伤害排行榜排名奖励");
                    }
                    else
                    {
                        player->SendSysMsg("你多开违规没有奖励 最多只能 %u 个角色参与", GetIpCount(Me->GetEntry()));
                    }
                }
            }
        }

        Me->DmgRankStore.clear();
    }
}

void PlayerModMgr::SendUpdateDmgRanking(Creature* Me)
{
    uint32 CreEntry = Me->GetEntry();
    if (IsInDmgRank(CreEntry))
    {
        std::string SendMsg;
        SendMsg.clear();
        uint32 RankIndex = 0;
        std::sort(Me->DmgRankStore.begin(), Me->DmgRankStore.end(), [](const DmgRankTemp& V1, const DmgRankTemp& V2) {return V1.Dmg > V2.Dmg; });
        for (auto It = Me->DmgRankStore.begin(); It != Me->DmgRankStore.end(); ++It)
        {
            RankIndex++;
            if (RankIndex <= DMGRANKMAX)
            {
                if (CharacterCacheEntry const* CacheEntry = sCharacterCache->GetCharacterCacheByGuid(It->PlayerGuid))
                {
                    std::string PlayerName = CacheEntry->Name;
                    uint8 Class = CacheEntry->Class;
                    std::string PInfo = sHelper->GetPlayerChatInfo(PlayerName, Class, 18);
                    AsInt64 rankingMaxHealth = Me->GetAsMaxHealth() > 0 ? Me->GetAsMaxHealth() : AsInt64(Me->GetMaxHealth());
                    if (sHuanJingSystem)
                    {
                        if (const HealthBarData* healthBar = sHuanJingSystem->GetHealthBarData(Me))
                        {
                            if (healthBar->totalBars > 1 && healthBar->healthPerBar > 0)
                                rankingMaxHealth = healthBar->healthPerBar * healthBar->totalBars;
                        }
                    }
                    std::string CreMax = AsInt64ToString(rankingMaxHealth);
                    std::string RItemLink = GetRankDmgItemLink(CreEntry, RankIndex);
                    SendMsg.append(CreMax + "#" + PInfo + "#" + AsInt64ToString(It->Dmg) + "#" + RItemLink + "$");
                }
            }
        }

        RankIndex = 0;
        for (auto It = Me->DmgRankStore.begin(); It != Me->DmgRankStore.end(); ++It)
        {
            RankIndex++;
            if (Player* player = ObjectAccessor::FindPlayer(It->PlayerGuid))
            {
                if (player->ShowDmgUI)
                {
                    std::string CreName = sHelper->GetCreatureNameByEntry(CreEntry);
                    std::string OnCount = std::to_string(Me->DmgRankStore.size());
                    sDoAddon->SendPacketToClient(player, "DataTheRanking", CreName + "^" + OnCount + "^" + std::to_string(RankIndex) + "^" + AsInt64ToString(It->Dmg) + "^" + SendMsg);
                }
            }
        }
        SendMsg.clear();
    }
}

void PlayerModMgr::SendResrtDmgRanking(Creature* Me)
{
    for (auto It = Me->DmgRankStore.begin(); It != Me->DmgRankStore.end(); ++It)
    {
        if (Player* player = ObjectAccessor::FindPlayer(It->PlayerGuid))
            sDoAddon->SendPacketToClient(player, "ClearTheRanking");
    }
}

void PlayerModMgr::DmgRankOnCreatureDamageTaken(Creature* Me, Player* player, AsInt64 const& Damage)
{
    if (Damage <= 0 || !Me->IsInWorld() || !player->IsInWorld())
        return;

    if (!sHelper->GetBoolean(1407))
        return;

    uint32 CreEntry = Me->GetEntry();
    if (!IsInDmgRank(CreEntry))
        return;

    // Damage 已是所有职业、光环和生物减伤结算后的最终伤害，排行榜不可再次乘光环倍率。
    AsInt64 const NowDamage = Damage;
    bool IsInStore = false;
    for (auto Itr = Me->DmgRankStore.begin(); Itr != Me->DmgRankStore.end(); ++Itr)
    {
        if (Itr->PlayerGuid == player->GetGUID())
        {
            Itr->Dmg += NowDamage;
            IsInStore = true;
            break;
        }
    }

    if (!IsInStore)
    {
        DmgRankTemp St;
        St.PlayerGuid = player->GetGUID();
        St.Dmg = NowDamage;
        Me->DmgRankStore.emplace_back(St);
    }

    Me->DmgRankTime = 0;
    SendUpdateDmgRanking(Me);
}

std::string PlayerModMgr::GetRankDmgItemLink(uint32 CreEntry, uint32 Rank)
{
    uint32 TempId = GetDmgRankTempId(CreEntry, Rank);
    auto Itr = RewardTempStore.find(TempId);
    if (Itr != RewardTempStore.end())
    {
        std::string Link = sHelper->GetItemLink(Itr->second.ItemId[0]);
        if (!Link.empty())
            return Link;
    }

    return "Not";
}

void PlayerModMgr::PlayerOnLoginJoinGuild(Player* player)
{
    if (IsXsPlayerbot(player))
        return;

    if (player->GetGuildId())
        return;

    if (Guild* DoGuild = sGuildMgr->GetGuildById(player->GetTeamId() == TEAM_ALLIANCE ? uint32(sHelper->GetUInt(1009)) : uint32(sHelper->GetUInt(1010))))
        DoGuild->HandleAcceptMember(player->GetSession());
}

void PlayerModMgr::PlayerOnLoginAsText(Player* player)
{
    if (sHelper->GetBoolean(1101))
        sHelper->SendSerMessage(sHelper->GetConfig(1102), sHelper->GetWorldChatPlayerChatInfo(player).c_str());

    if (IsXsPlayerbot(player))
        return;

    std::string PopText = sHelper->GetConfig(1100);
    if (!PopText.empty())
        sHelper->PopToPlayerDialog(player, PopText);
}

void PlayerModMgr::PlayerOnLogoutAsText(Player* player)
{
    if (sHelper->GetBoolean(1103))
        sHelper->SendSerMessage(sHelper->GetConfig(1104), sHelper->GetWorldChatPlayerChatInfo(player).c_str());
}

void Player::OnLoginSign()
{
    if (IsXsPlayerbot(this))
        return;

    SignStore.clear();
    LoginDatabasePreparedStatement* Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_SIGN);
    Stmt->SetData(0, GetSession()->GetAccountId());
    PreparedQueryResult Result = LoginDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Day = Fields[0].Get<uint32>();
            SignStore.push_back(Day);
        } while (Result->NextRow());
    }

    Stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_LX_SIGN);
    Stmt->SetData(0, GetSession()->GetAccountId());
    Result = LoginDatabase.Query(Stmt);
    if (Result)
    {
        Field* Fields = Result->Fetch();
        SignContinuous = (Fields[0].Get<uint32>() > 0);
        SignTotal = (Fields[1].Get<uint32>() > 0);
    }
    else
    {
        SignContinuous = false;
        SignTotal = false;
    }
}

void Player::OnRepSign(uint32 Day, bool IsRepair)
{
    auto Itr = std::find(SignStore.begin(), SignStore.end(), Day);
    if (Itr != SignStore.end())
    {
        SendSysMsg(sHelper->GetGts(1124));
        return;
    }

    if (IsRepair)
    {
        uint32 NTempId = uint32(sHelper->GetUInt(1050));
        if (!sHelper->CheckNeed(this, NTempId, "签到的补签消耗"))
            return;
    }

    auto It = SignRewardStore.find(Day);
    if (It != SignRewardStore.end())
    {
        if (It->second.ItemId1 > 0 && It->second.Count1 > 0)
            AddItem(It->second.ItemId1, It->second.Count1, "签到奖励获得");
        if (It->second.ItemId2 > 0 && It->second.Count2 > 0)
            AddItem(It->second.ItemId2, It->second.Count2, "签到奖励获得");
        if (It->second.ItemId3 > 0 && It->second.Count3 > 0)
            AddItem(It->second.ItemId3, It->second.Count3, "签到奖励获得");
        if (It->second.ItemId4 > 0 && It->second.Count4 > 0)
            AddItem(It->second.ItemId4, It->second.Count4, "签到奖励获得");
        if (It->second.ItemId5 > 0 && It->second.Count5 > 0)
            AddItem(It->second.ItemId5, It->second.Count5, "签到奖励获得");
        if (It->second.ItemId6 > 0 && It->second.Count6 > 0)
            AddItem(It->second.ItemId6, It->second.Count6, "签到奖励获得");
    }

    LoginDatabasePreparedStatement* Rtmt = LoginDatabase.GetPreparedStatement(LOGIN_REP_SIGN);
    Rtmt->SetData(0, GetSession()->GetAccountId());
    Rtmt->SetData(1, Day);
    LoginDatabase.Execute(Rtmt);

    SignStore.push_back(Day);

    std::vector<uint32> DoList = SignStore;
    uint32 SeriesDays = sHelper->GetSeriesDays(DoList);

    sDoAddon->SendPacketToClient(this, "PlayerSignData", std::to_string(Day) + "$" + std::to_string(SeriesDays) + "$" + std::to_string(DoList.size()) + "$");

    SendSysMsg(sHelper->GetGts(IsRepair ? 1125 : 1123));

    sHelper->SignRewardExtra(this, SeriesDays, DoList.size());
}

void Player::OnLoadDBTempAt()
{
    TempAtStore.clear();

    for (uint32 i = AT01; i < ATMAX; i++)
        TempAtStore[i] = 0;

    for (uint32 i = AT1000; i < ATEND; i++)
        TempAtStore[i] = 0;

    TempAtStore[AT2001] = 0;
    TempAtStore[AT2002] = 1;

    for (uint32 i = AT3001; i < ATKPMAX; i++)
        TempAtStore[i] = 0;

    TempAtStore[AT4001] = 0;
    TempAtStore[AT4002] = 0;
    TempAtStore[AT4003] = 0;

    for (uint32 i = AT5001; i < ATZCMAX; i++)
        TempAtStore[i] = 0;

    TempAtStore[AT6000] = 0;
    TempAtStore[AT6001] = 0;

    TempAtStore[AT7001] = 0;
    TempAtStore[AT7002] = 0;

    for (uint32 i = AT8000; i < ATACTMAX; i++)
    TempAtStore[AT8001] = 0;

    CharacterDatabasePreparedStatement* Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_TEMP_AT);
    Stmt->SetData(0, GetGUIDLow());
    PreparedQueryResult Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            Field* Fields = Result->Fetch();
            uint32 Index = Fields[0].Get<uint32>();
            AsInt64 Temp = AsInt64FromString(Fields[1].Get<std::string>());
            TempAtStore[Index] = Temp;
        } while (Result->NextRow());
    }

    InitLimitItem();
    InitUseLimitItem();
    InitCombatPetPlayerData();
    LoadItemPicGuideCache();
    LoadCrePicGuideCache();
}

void Player::UpdateTempAt(PlayerTemp At, AsInt64 const& Value)
{
    uint32 GUIDLow = GetGUIDLow();
    CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_TEMP_AT);
    Rtmt->SetData(0, GUIDLow);
    Rtmt->SetData(1, uint32(At));
    Rtmt->SetData(2, AsInt64ToString(Value));
    CharacterDatabase.Execute(Rtmt);
    TempAtStore[uint32(At)] = Value;
}

void Player::LogoutSaveStatTempAt()
{
    if (IsXsPlayerbot(this))
        return;

    uint32 GUIDLow = GetGUIDLow();

    for (size_t i = AT11; i < ATMAX; i++)
    {
        CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_TEMP_AT);
        Rtmt->SetData(0, GUIDLow);
        Rtmt->SetData(1, i);
        Rtmt->SetData(2, AsInt64ToString(TempAtStore[i].Value));
        CharacterDatabase.Execute(Rtmt);
    }
}

AsInt64 Player::GetTempAt(PlayerTemp At)
{
    auto Itr = TempAtStore.find(uint32(At));
    return (Itr != TempAtStore.end()) ? Itr->second.Value : AsInt64Zero();
}

uint32 Player::GetTempAtUInt32(PlayerTemp At)
{
    return AsInt64ToUInt32Clamp(GetTempAt(At));
}

int32 Player::GetTempAtInt32(PlayerTemp At)
{
    return AsInt64ToInt32Clamp(GetTempAt(At));
}

void Item::UpdateTempKt(ItemTemp Kt, int32 Value)
{
    uint32 GUIDLow = GetGUIDLow();
    CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_TEMP_KT);
    Rtmt->SetData(0, GUIDLow);
    Rtmt->SetData(1, uint32(Kt));
    Rtmt->SetData(2, Value);
    CharacterDatabase.Execute(Rtmt);
    TempKtStore[uint32(Kt)] = Value;
    ItemTempKtCacheStore[GUIDLow][uint32(Kt)] = Value;
}

int32 Item::GetTempKt(ItemTemp Kt)
{
    auto Itr = TempKtStore.find(uint32(Kt));
    return (Itr != TempKtStore.end()) ? Itr->second : 0;
}

void Player::DoSummonRequestFrom(Player* Summoner, WorldLocation* Location)
{
    if (!Summoner)
        return;

    if (IsSummonAsSpectator())
        return;

    m_summon_expire = GameTime::GetGameTime().count() + MAX_PLAYER_SUMMON_DELAY;

    if (!Location)
        SetSummonPoint(Summoner->GetMapId(), Summoner->GetPositionX(), Summoner->GetPositionY(), Summoner->GetPositionZ());
    else
        SetSummonPoint(Location->GetMapId(), Location->GetPositionX(), Location->GetPositionY(), Location->GetPositionZ());

    WorldPacket data(SMSG_SUMMON_REQUEST, 8 + 4 + 4);
    data << Summoner->GetGUID().GetRawValue();
    data << uint32(Summoner->GetZoneId());
    data << uint32(MAX_PLAYER_SUMMON_DELAY * IN_MILLISECONDS);
    SendDirectMessage(&data);
}

//重置所有副本
void Player::CustomUnbindAll()
{
    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
    {
        BoundInstancesMap const& m_boundInstances = sInstanceSaveMgr->PlayerGetBoundInstances(GetGUID(), Difficulty(i));
        for (BoundInstancesMap::const_iterator itr = m_boundInstances.begin(); itr != m_boundInstances.end();)
        {
            if (itr->first != GetMapId())
            {
                sInstanceSaveMgr->PlayerUnbindInstance(GetGUID(), itr->first, Difficulty(i), true, this);
                itr = m_boundInstances.begin();
            }
            else
                ++itr;
        }
    }
}

//重置单个副本下的所有难度
void Player::CustomUnbinds(uint32 MapId)
{
    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
        sInstanceSaveMgr->PlayerUnbindInstance(GetGUID(), MapId, Difficulty(i), true, this);
}

//重置单个副本按难度
void Player::CustomUnbind(uint32 MapId, Difficulty Diff)
{
    if (Diff < MAX_DIFFICULTY)
        sInstanceSaveMgr->PlayerUnbindInstance(GetGUID(), MapId, Diff, true, this);
}

void Player::LoginLoadEnchLock()
{
    if (IsXsPlayerbot(this))
        return;

    ItemEnchLockVect.clear();

    CharacterDatabasePreparedStatement* Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_ENCH_LOCK);
    Stmt->SetData(0, GetGUIDLow());
    PreparedQueryResult Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            ItemEnchLockTemp St;
            Field* Fields = Result->Fetch();
            St.ItemGuidLow = Fields[0].Get<uint32>();
            St.Num = Fields[1].Get<uint32>();
            St.Lock = Fields[2].Get<bool>();

            ItemEnchLockVect.emplace_back(St);

        } while (Result->NextRow());
    }
}

void Player::LogoutSaveEnchLock()
{
    if (IsXsPlayerbot(this))
        return;

    uint32 GUIDLow = GetGUIDLow();

    for (auto Itr = ItemEnchLockVect.begin(); Itr != ItemEnchLockVect.end(); ++Itr)
    {
        CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_ITEM_ENCH_LOCK);
        Rtmt->SetData(0, GUIDLow);
        Rtmt->SetData(1, Itr->ItemGuidLow);
        Rtmt->SetData(2, Itr->Num);
        Rtmt->SetData(3, Itr->Lock);
        CharacterDatabase.Execute(Rtmt);
    }
}

void Player::UpdateEnchLock(uint32 ItemGuidLow, uint32 Num, bool Lock)
{
    bool In = false;
    for (auto Itr = ItemEnchLockVect.begin(); Itr != ItemEnchLockVect.end(); ++Itr)
    {
        if (Itr->ItemGuidLow == ItemGuidLow && Itr->Num == Num)
        {
            Itr->Lock = Lock;
            In = true;
        }
    }

    if (!In)
    {
        ItemEnchLockTemp St;
        St.ItemGuidLow = ItemGuidLow;
        St.Num = Num;
        St.Lock = Lock;
        ItemEnchLockVect.emplace_back(St);
    }
}

bool Player::GetEnchLock(uint32 ItemGuidLow, uint32 Num)
{
    for (auto Itr = ItemEnchLockVect.begin(); Itr != ItemEnchLockVect.end(); ++Itr)
    {
        if (Itr->ItemGuidLow == ItemGuidLow && Itr->Num == Num)
            return Itr->Lock;
    }

    return false;
}

void Player::LoginLoadPlayerReMapStore()
{
    if (IsXsPlayerbot(this))
        return;

    PlayerReMapStore.clear();

    CharacterDatabasePreparedStatement* Stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_RESET_MAP);
    Stmt->SetData(0, GetGUIDLow());
    PreparedQueryResult Result = CharacterDatabase.Query(Stmt);
    if (Result)
    {
        do
        {
            MapDiffReTemp St;
            Field* Fields = Result->Fetch();
            St.MapId = Fields[0].Get<int32>();
            St.Diff = Fields[1].Get<int32>();
            St.Count = Fields[2].Get<uint32>();

            PlayerReMapStore.emplace_back(St);

        } while (Result->NextRow());
    }
}

void Player::LogoutSavePlayerReMapStore()
{
    if (IsXsPlayerbot(this))
        return;

    uint32 GUIDLow = GetGUIDLow();

    for (auto Itr = PlayerReMapStore.begin(); Itr != PlayerReMapStore.end(); ++Itr)
    {
        CharacterDatabasePreparedStatement* Rtmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_RESET_MAP);
        Rtmt->SetData(0, GUIDLow);
        Rtmt->SetData(1, Itr->MapId);
        Rtmt->SetData(2, Itr->Diff);
        Rtmt->SetData(3, Itr->Count);
        CharacterDatabase.Execute(Rtmt);
    }
}

void Player::UpdatePlayerReMapStore(int32 MapId, int32 Diff, uint32 Count)
{
    bool In = false;
    for (auto Itr = PlayerReMapStore.begin(); Itr != PlayerReMapStore.end(); ++Itr)
    {
        if (Itr->MapId == MapId && Itr->Diff == Diff)
        {
            Itr->Count = Count;
            In = true;
        }
    }

    if (!In)
    {
        MapDiffReTemp St;
        St.MapId = MapId;
        St.Diff = Diff;
        St.Count = Count;
        PlayerReMapStore.emplace_back(St);
    }
}

uint32 Player::GetPlayerReMapStoreCount(int32 MapId, int32 Diff)
{
    for (auto Itr = PlayerReMapStore.begin(); Itr != PlayerReMapStore.end(); ++Itr)
    {
        if (Itr->MapId == MapId && Itr->Diff == Diff)
            return Itr->Count;
    }

    return 0;
}

Item* Player::AddItemByIdOne(uint32 Entry, std::string Info, bool NoRandom)
{
    uint32 Count = 1;
    uint32 NoSpaceForCount = 0;
    ItemPosCountVec Dest;
    InventoryResult Msg = CanStoreNewItem(NULL_BAG, NULL_SLOT, Dest, Entry, Count, &NoSpaceForCount);
    if (Msg != EQUIP_ERR_OK)
        Count -= NoSpaceForCount;

    if (Count == 0 || Dest.empty())
    {
        ChatHandler(GetSession()).PSendSysMessage("背包空间不足或物品唯一限制 无法获得该物品");
        return nullptr;
    }

    Item* NewItem = StoreNewItem(Dest, Entry, true, Info, NoRandom ? Item::GenerateItemRandomPropertyId(Entry) : 0);
    if (NewItem)
        SendNewItem(NewItem, 1, false, false, true);

    return NewItem;
}

Item* Player::GetItemByEntryInBag(uint32 entry)
{
    for (uint8 i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetEntry() == entry)
                return pItem;

    for (uint8 i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Bag* pBag = GetBagByPos(i))
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                if (Item* pItem = pBag->GetItemByPos(j))
                    if (pItem->GetEntry() == entry)
                        return pItem;

    return nullptr;
}

void Player::DoCommands(std::string Commands)
{
    if (Commands.empty())
        return;

    if (!GetSession())
        return;

    Tokenizer Cmds(Commands, '#');
    for (auto const& Cmd : Cmds)
        ChatHandler(GetSession()).CustomParseCommands(Cmd);
}

void Player::TeleToPos(uint32 PosId)
{
    auto It = PosMap.find(PosId);
    if (It != PosMap.end())
    {
        SetCanTeleport(true);
        TeleportTo(It->second.MapId, It->second.X, It->second.Y, It->second.Z, It->second.O);
    }
}

void Player::CustomApplyEnchantment(Item* item, bool apply)
{
    for (uint32 slot = 0; slot < MAX_ENCHANTMENT_SLOT; ++slot)
        CustomApplyEnchantment(item, EnchantmentSlot(slot), apply);
}

void Player::CustomApplyEnchantment(Item* item, EnchantmentSlot slot, bool apply, bool apply_dur, bool ignore_condition)
{
    if (!item || !item->IsEquipped())
        return;

    if (slot >= MAX_ENCHANTMENT_SLOT)
        return;

    uint32 enchant_id = item->GetEnchantmentId(slot);
    if (!enchant_id)
        return;

    SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
    if (!pEnchant)
        return;

    if (!ignore_condition && pEnchant->EnchantmentCondition && !EnchantmentFitsRequirements(pEnchant->EnchantmentCondition, -1))
        return;

    if (pEnchant->requiredLevel > getLevel())
        return;

    if (pEnchant->requiredSkill > 0 && pEnchant->requiredSkillValue > GetSkillValue(pEnchant->requiredSkill))
        return;

    if (!sScriptMgr->OnPlayerCanApplyEnchantment(this, item, slot, apply, apply_dur, ignore_condition))
        return;

    // If we're dealing with a gem inside a prismatic socket we need to check the prismatic socket requirements
    // rather than the gem requirements itself. If the socket has no color it is a prismatic socket.
    if ((slot == SOCK_ENCHANTMENT_SLOT || slot == SOCK_ENCHANTMENT_SLOT_2 || slot == SOCK_ENCHANTMENT_SLOT_3)
        && !item->GetTemplate()->Socket[slot - SOCK_ENCHANTMENT_SLOT].Color)
    {
        // Check if the requirements for the prismatic socket are met before applying the gem stats
        SpellItemEnchantmentEntry const* pPrismaticEnchant = sSpellItemEnchantmentStore.LookupEntry(item->GetEnchantmentId(PRISMATIC_ENCHANTMENT_SLOT));
        if (!pPrismaticEnchant || (pPrismaticEnchant->requiredSkill > 0 && pPrismaticEnchant->requiredSkillValue > GetSkillValue(pPrismaticEnchant->requiredSkill)))
            return;
    }

    if (!item->IsBroken())
    {

        if (sItemFuncMgr->InEnchMaxCount(this, apply, enchant_id))
            return;

        for (int s = 0; s < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++s)
        {
            uint32 enchant_display_type = pEnchant->type[s];
            uint32 enchant_amount = pEnchant->amount[s];
            uint32 enchant_spell_id = pEnchant->spellid[s];

            switch (enchant_display_type)
            {
            case ITEM_ENCHANTMENT_TYPE_NONE:
                break;
            case ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL:
                // processed in Player::CastItemCombatSpell
                break;
            case ITEM_ENCHANTMENT_TYPE_DAMAGE:
                if (item->GetSlot() == EQUIPMENT_SLOT_MAINHAND)
                    HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_VALUE, float(enchant_amount), apply);
                else if (item->GetSlot() == EQUIPMENT_SLOT_OFFHAND)
                    HandleStatModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_VALUE, float(enchant_amount), apply);
                else if (item->GetSlot() == EQUIPMENT_SLOT_RANGED)
                    HandleStatModifier(UNIT_MOD_DAMAGE_RANGED, TOTAL_VALUE, float(enchant_amount), apply);
                break;
            case ITEM_ENCHANTMENT_TYPE_EQUIP_SPELL:
                if (enchant_spell_id)
                {
                    if (apply)
                    {
                        int32 basepoints = 0;
                        // Random Property Exist - try found basepoints for spell (basepoints depends from item suffix factor)
                        if (item->GetItemRandomPropertyId())
                        {
                            ItemRandomSuffixEntry const* item_rand = sItemRandomSuffixStore.LookupEntry(std::abs(item->GetItemRandomPropertyId()));
                            if (item_rand)
                            {
                                // Search enchant_amount
                                for (int k = 0; k < MAX_ITEM_ENCHANTMENT_EFFECTS; ++k)
                                {
                                    if (item_rand->Enchantment[k] == enchant_id)
                                    {
                                        basepoints = int32((item_rand->AllocationPct[k] * item->GetItemSuffixFactor()) / 10000);
                                        break;
                                    }
                                }
                            }
                        }
                        // Cast custom spell vs all equal basepoints got from enchant_amount
                        if (basepoints)
                        {
                            CastCustomSpell(this, enchant_spell_id, &basepoints, &basepoints, &basepoints, true, item);
                        }
                        else
                        {
                            CastSpell(this, enchant_spell_id, true, item);
                        }
                    }
                    else
                        RemoveAurasDueToItemSpell(enchant_spell_id, item->GetGUID());
                }
                break;
            case ITEM_ENCHANTMENT_TYPE_RESISTANCE:
                if (!enchant_amount)
                {
                    ItemRandomSuffixEntry const* item_rand = sItemRandomSuffixStore.LookupEntry(std::abs(item->GetItemRandomPropertyId()));
                    if (item_rand)
                    {
                        for (int k = 0; k < MAX_ITEM_ENCHANTMENT_EFFECTS; ++k)
                        {
                            if (item_rand->Enchantment[k] == enchant_id)
                            {
                                enchant_amount = uint32((item_rand->AllocationPct[k] * item->GetItemSuffixFactor()) / 10000);
                                break;
                            }
                        }
                    }
                }

                HandleStatModifier(UnitMods(UNIT_MOD_RESISTANCE_START + enchant_spell_id), TOTAL_VALUE, float(enchant_amount), apply);
                break;
            case ITEM_ENCHANTMENT_TYPE_STAT:
            {
                if (!enchant_amount)
                {
                    ItemRandomSuffixEntry const* item_rand_suffix = sItemRandomSuffixStore.LookupEntry(std::abs(item->GetItemRandomPropertyId()));
                    if (item_rand_suffix)
                    {
                        for (int k = 0; k < MAX_ITEM_ENCHANTMENT_EFFECTS; ++k)
                        {
                            if (item_rand_suffix->Enchantment[k] == enchant_id)
                            {
                                enchant_amount = uint32((item_rand_suffix->AllocationPct[k] * item->GetItemSuffixFactor()) / 10000);
                                break;
                            }
                        }
                    }
                }

                switch (enchant_spell_id)
                {
                case ITEM_MOD_MANA:
                    HandleStatModifier(UNIT_MOD_MANA, BASE_VALUE, float(enchant_amount), apply);
                    break;
                case ITEM_MOD_HEALTH:
                    HandleStatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(enchant_amount), apply);
                    break;
                case ITEM_MOD_AGILITY:
                    HandleStatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, float(enchant_amount), apply);
                    ApplyStatBuffMod(STAT_AGILITY, (float)enchant_amount, apply);
                    break;
                case ITEM_MOD_STRENGTH:
                    HandleStatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, float(enchant_amount), apply);
                    ApplyStatBuffMod(STAT_STRENGTH, (float)enchant_amount, apply);
                    break;
                case ITEM_MOD_INTELLECT:
                    HandleStatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, float(enchant_amount), apply);
                    ApplyStatBuffMod(STAT_INTELLECT, (float)enchant_amount, apply);
                    break;
                case ITEM_MOD_SPIRIT:
                    HandleStatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, float(enchant_amount), apply);
                    ApplyStatBuffMod(STAT_SPIRIT, (float)enchant_amount, apply);
                    break;
                case ITEM_MOD_STAMINA:
                    HandleStatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, float(enchant_amount), apply);
                    ApplyStatBuffMod(STAT_STAMINA, (float)enchant_amount, apply);
                    break;
                case ITEM_MOD_DEFENSE_SKILL_RATING:
                    ApplyRatingMod(CR_DEFENSE_SKILL, enchant_amount, apply);
                    break;
                case  ITEM_MOD_DODGE_RATING:
                    ApplyRatingMod(CR_DODGE, enchant_amount, apply);
                    break;
                case ITEM_MOD_PARRY_RATING:
                    ApplyRatingMod(CR_PARRY, enchant_amount, apply);
                    break;
                case ITEM_MOD_BLOCK_RATING:
                    ApplyRatingMod(CR_BLOCK, enchant_amount, apply);
                    break;
                case ITEM_MOD_HIT_MELEE_RATING:
                    ApplyRatingMod(CR_HIT_MELEE, enchant_amount, apply);
                    break;
                case ITEM_MOD_HIT_RANGED_RATING:
                    ApplyRatingMod(CR_HIT_RANGED, enchant_amount, apply);
                    break;
                case ITEM_MOD_HIT_SPELL_RATING:
                    ApplyRatingMod(CR_HIT_SPELL, enchant_amount, apply);
                    break;
                case ITEM_MOD_CRIT_MELEE_RATING:
                    ApplyRatingMod(CR_CRIT_MELEE, enchant_amount, apply);
                    break;
                case ITEM_MOD_CRIT_RANGED_RATING:
                    ApplyRatingMod(CR_CRIT_RANGED, enchant_amount, apply);
                    break;
                case ITEM_MOD_CRIT_SPELL_RATING:
                    ApplyRatingMod(CR_CRIT_SPELL, enchant_amount, apply);
                    break;
                case ITEM_MOD_HASTE_RANGED_RATING:
                    ApplyRatingMod(CR_HASTE_RANGED, enchant_amount, apply);
                    break;
                case ITEM_MOD_HASTE_SPELL_RATING:
                    ApplyRatingMod(CR_HASTE_SPELL, enchant_amount, apply);
                    break;
                case ITEM_MOD_HIT_RATING:
                    ApplyRatingMod(CR_HIT_MELEE, enchant_amount, apply);
                    ApplyRatingMod(CR_HIT_RANGED, enchant_amount, apply);
                    ApplyRatingMod(CR_HIT_SPELL, enchant_amount, apply);
                    break;
                case ITEM_MOD_CRIT_RATING:
                    ApplyRatingMod(CR_CRIT_MELEE, enchant_amount, apply);
                    ApplyRatingMod(CR_CRIT_RANGED, enchant_amount, apply);
                    ApplyRatingMod(CR_CRIT_SPELL, enchant_amount, apply);
                    break;
                case ITEM_MOD_RESILIENCE_RATING:
                    ApplyRatingMod(CR_CRIT_TAKEN_MELEE, enchant_amount, apply);
                    ApplyRatingMod(CR_CRIT_TAKEN_RANGED, enchant_amount, apply);
                    ApplyRatingMod(CR_CRIT_TAKEN_SPELL, enchant_amount, apply);
                    break;
                case ITEM_MOD_HASTE_RATING:
                    ApplyRatingMod(CR_HASTE_MELEE, enchant_amount, apply);
                    ApplyRatingMod(CR_HASTE_RANGED, enchant_amount, apply);
                    ApplyRatingMod(CR_HASTE_SPELL, enchant_amount, apply);
                    break;
                case ITEM_MOD_EXPERTISE_RATING:
                    ApplyRatingMod(CR_EXPERTISE, enchant_amount, apply);
                    break;
                case ITEM_MOD_ATTACK_POWER:
                    DoApplyStat(ITEM_MOD_ATTACK_POWER, int32(enchant_amount), apply);
                    break;
                case ITEM_MOD_RANGED_ATTACK_POWER:
                    DoApplyStat(ITEM_MOD_RANGED_ATTACK_POWER, int32(enchant_amount), apply);
                    break;
                case ITEM_MOD_MANA_REGENERATION:
                    ApplyManaRegenBonus(enchant_amount, apply);
                    break;
                case ITEM_MOD_ARMOR_PENETRATION_RATING:
                    ApplyRatingMod(CR_ARMOR_PENETRATION, enchant_amount, apply);
                    break;
                case ITEM_MOD_SPELL_POWER:
                    DoApplyStat(ITEM_MOD_SPELL_POWER, int32(enchant_amount), apply);
                    break;
                case ITEM_MOD_HEALTH_REGEN:
                    ApplyHealthRegenBonus(enchant_amount, apply);
                    break;
                case ITEM_MOD_SPELL_PENETRATION:
                    ApplySpellPenetrationBonus(enchant_amount, apply);
                    break;
                case ITEM_MOD_BLOCK_VALUE:
                    HandleBaseModValue(SHIELD_BLOCK_VALUE, FLAT_MOD, float(enchant_amount), apply);
                    break;
                case ITEM_MOD_SPELL_HEALING_DONE:   // deprecated
                case ITEM_MOD_SPELL_DAMAGE_DONE:    // deprecated
                default:
                    break;
                }
                break;
            }
            case ITEM_ENCHANTMENT_TYPE_TOTEM:           // Shaman Rockbiter Weapon
            {
                if (getClass() == CLASS_SHAMAN)
                {
                    float addValue = 0.0f;
                    if (item->GetSlot() == EQUIPMENT_SLOT_MAINHAND)
                    {
                        addValue = float(enchant_amount * item->GetTemplate()->Delay / 1000.0f);
                        HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_VALUE, addValue, apply);
                    }
                    else if (item->GetSlot() == EQUIPMENT_SLOT_OFFHAND)
                    {
                        addValue = float(enchant_amount * item->GetTemplate()->Delay / 1000.0f);
                        HandleStatModifier(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_VALUE, addValue, apply);
                    }
                }
                break;
            }
            case ITEM_ENCHANTMENT_TYPE_USE_SPELL:
                // processed in Player::CastItemUseSpell
                break;
            case ITEM_ENCHANTMENT_TYPE_PRISMATIC_SOCKET:
                // nothing do..
                break;
            default:
                break;
            }                                               /*switch (enchant_display_type)*/
        }                                                   /*for*/
    }

    // visualize enchantment at player and equipped items
    if (slot == PERM_ENCHANTMENT_SLOT)
        SetUInt16Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (item->GetSlot() * 2), 0, apply ? item->GetEnchantmentId(slot) : 0);

    if (slot == TEMP_ENCHANTMENT_SLOT)
        SetUInt16Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (item->GetSlot() * 2), 1, apply ? item->GetEnchantmentId(slot) : 0);

    if (apply_dur)
    {
        if (apply)
        {
            // set duration
            uint32 duration = item->GetEnchantmentDuration(slot);
            if (duration > 0)
                AddEnchantmentDuration(item, slot, duration);
        }
        else
        {
            // duration == 0 will remove EnchantDuration
            AddEnchantmentDuration(item, slot, 0);
        }
    }
}

class OnPlayerMod : public PlayerScript
{
public:
    OnPlayerMod() : PlayerScript("OnPlayerMod") {  }

    void OnLogin(Player* player) override
    {
        sPlayerModMgr->UpdateVipAuras(player, player->GetVipRank());
        sPlayerModMgr->PlayerOnLoginJoinGuild(player);
        player->OnLoginSign();
        sPlayerModMgr->PlayerOnLoginAsText(player);
        sPlayerModMgr->PlayerOnLoginStatPt(player);
        player->LoginLoadEnchLock();
        player->OnLoginActivity();
        sPlayerModMgr->HandlerEqRankGiveAuraLogin(player);
        player->LoginLoadPlayerReMapStore();
    }

    void OnFirstLogin(Player* player) override
    {
        if (IsXsPlayerbot(player))
            return;

        sHelper->DoTheReward(player, uint32(sHelper->GetUInt(1099)), "首次登录奖励");
        sPlayerModMgr->OnDKSkipPlayer(player);
    }

    void OnLogout(Player* player) override
    {
        if (IsXsPlayerbot(player))
            return;

        sPlayerModMgr->PlayerOnLogoutAsText(player);
        player->LogoutSaveStatTempAt();
        player->LogoutSaveEnchLock();
        player->LogoutSavePlayerReMapStore();
    }

    void OnMapChanged(Player* player) override
    {
        if (sHelper->GetBoolean(9994))
            LOG_WARN("server", ">> 玩家进入地图调试输出 PlayerGuidLow = {} MapId = {}", player->GetGUIDLow(), player->GetMapId());

        sPlayerModMgr->OnCheckAuraPlayerMapChanged(player);
        sPlayerModMgr->OnCheckCanFlyPlayerMapChanged(player);
        sPlayerModMgr->HandlerEnMapAura(player);
    }

    void OnUpdateZone(Player* player, uint32 newZone, uint32 newArea) override
    {
        if (IsXsPlayerbot(player))
            return;

        sPlayerModMgr->OnCheckCanFlyPlayerMapChanged(player);
        sPlayerModMgr->HandlerEnAreaAura(player);
    }

    void OnUpdate(Player* player, uint32 ptime) override
    {
        if (IsXsPlayerbot(player))
            return;

        sPlayerModMgr->DoOnlineRewardTimeFunc(player, ptime);
    }

    void OnCreatureKill(Player* player, Creature* Killed) override
    {
        if (IsXsPlayerbot(player))
            return;

        sPlayerModMgr->StatPtOnCreatureKill(player, Killed);
        sPlayerModMgr->AuraLootOnCreatureKill(player, Killed);
        sPlayerModMgr->DmgRankOnCreatureDied(Killed);
    }

    void OnPVPKill(Player* Killer, Player* Killed) override
    {
        sPlayerModMgr->OnKillPlayerReward(Killer, Killed);
        sPlayerModMgr->OnKillAuraPlayer(Killer, Killed);
        sPlayerModMgr->OnKillLootPlayer(Killer, Killed);
    }

    void OnLootAddItemForCount(Player const* player, uint32& AtCount) override
    {
        if (OnAsStore.empty())
            return;

        uint32 Amount1 = 0;
        uint32 Amount2 = 0;
        for (auto Itr = OnAsStore.begin(); Itr != OnAsStore.end(); ++Itr)
        {
            if (player->HasAura(Itr->first) || player->HasSpell(Itr->first))
            {
                if (Itr->second.LootCountPt == 1.0f)
                    continue;

                uint32 Count = player->GetAtAuraCount(Itr->first);
                for (uint32 N = 0; N < Count; N++)
                {
                    Amount1 += AtCount * Itr->second.LootCountPt;
                    Amount2 += Itr->second.LootCountEx;
                }
            }
        }

        uint32 Add = (Amount1 + Amount2);
        if (Add)
            AtCount = Add;
    }

    void OnSpellCast(Player* player, Spell* DoSpell, bool SkipCheck) override
    {
        sPlayerModMgr->OnTeleSpellComplete(player, DoSpell, SkipCheck);
        sPlayerModMgr->PlayerCastSpellComplete(player, DoSpell, SkipCheck);
    }

    bool CanEquipItem(Player* player, uint8 slot, uint16& dest, Item* pItem, bool swap, bool not_loading) override
    {
        if (sItemFuncMgr->IsItemInStat(pItem->GetEntry()))
            return false;

        uint32 NTempId = sItemFuncMgr->GetEquipTempId(pItem->GetEntry());
        if (!NTempId)
            return true;

        if (!sHelper->CheckNeed(player, NTempId, "装备时消耗"))
            return false;

        return true;
    }

    void OnGossipSelect(Player* player, uint32 MenuId, uint32 Sender, uint32 Action) override
    {
        if (!MenuId)
        {
            CloseGossipMenuFor(player);
            return;
        }

        switch (MenuId)
        {
        case AtEvent:
        {
            if (Action <= MINEVENTSO)
            {
                player->SendSysMsg("内部错误 请重新进入即可");
                return;
            }

            sEvent->StartPlayerInEvent(player, Action);
        }
        break;
        case AtChallenge:
        {
            if (!Action)
            {
                sAsCreature->AddGossipForChallenge(player, Sender);
                return;
            }

            AreaTriggerTeleport const* AreaTr = sObjectMgr->GetAreaTriggerTeleport(Sender);
            if (!AreaTr)
                return;

            uint32 TargetMapId = AreaTr->target_mapId;

            if (!sAsCreature->InChallenge(player, TargetMapId, Action))
            {
                Group* Gp = player->GetGroup();
                if (Gp)
                {
                    bool NowRe = true;
                    for (GroupReference* Itr = Gp->GetFirstMember(); Itr != nullptr; Itr = Itr->next())
                    {
                        if (Player* Member = Itr->GetSource())
                        {
                            if (Member->GetMapId() == TargetMapId)
                            {
                                NowRe = false;
                                Member->SendAsMsg(sHelper->GetGts(1506), Action);
                                if (player->GetGUIDLow() != Member->GetGUIDLow())
                                    player->SendAsMsg(sHelper->GetGts(1507), sHelper->GetNameLinkColor(Member).c_str(), Action);
                            }
                        }
                    }


                    if (NowRe)
                    {
                        for (GroupReference* Itr = Gp->GetFirstMember(); Itr != nullptr; Itr = Itr->next())
                        {
                            if (Player* Member = Itr->GetSource())
                            {
                                //弹框让队友选择进与不进
                                player->SetMapRank(Action);
                                WorldLocation Location = WorldLocation(AreaTr->target_mapId, AreaTr->target_X, AreaTr->target_Y, AreaTr->target_Z, AreaTr->target_Orientation);
                                if (player->GetGUIDLow() != Member->GetGUIDLow())
                                {
                                    Member->SetMapRank(Action);
                                    Member->DoSummonRequestFrom(player, &Location);
                                }
                                else
                                {
                                    Member->SetMapRank(Action);
                                    Member->SetCanTeleport(true);
                                    Member->TeleportTo(AreaTr->target_mapId, AreaTr->target_X, AreaTr->target_Y, AreaTr->target_Z, AreaTr->target_Orientation, TELE_TO_NOT_LEAVE_TRANSPORT);
                                }
                            }
                        }
                    }
                    else
                    {
                        sAsCreature->AddGossipForChallenge(player, Sender);
                    }
                }
                else
                {
                    player->SetMapRank(Action);
                    player->SetCanTeleport(true);
                    player->TeleportTo(AreaTr->target_mapId, AreaTr->target_X, AreaTr->target_Y, AreaTr->target_Z, AreaTr->target_Orientation, TELE_TO_NOT_LEAVE_TRANSPORT);
                }
            }
        }
        break;
        case MapTrigger:
        {
            if (!sHelper->OnDungeon(Action))
            {
                CloseGossipMenuFor(player);
                return;
            }

            if (sAsCreature->IsChallengeMap(Action))
            {
                uint32 MapRank = player->GetMapRank();
                uint32 NTempId = sAsCreature->GetChallengeNTempId(Action, MapRank);
                player->OnNowFB = sHelper->CheckNeed(player, NTempId, "进入挑战副本消耗");
                if (player->OnNowFB)
                {
                    player->UpdateTempAt(AT8001, 1);  // 标记为通过撞门触发进入挑战模式，用于显示提示
                    player->SetCanTeleport(true);
                    player->TeleportTo(player->ATLocation);
                }
                else
                    sAsCreature->AddGossipForChallenge(player, player->ATtriggerId);

                return;
            }

            MapEntry const* MEntry = sMapStore.LookupEntry(Action);
            if (MEntry)
            {
                int32 Difficulty = int32(player->GetDifficulty(MEntry->IsRaid()));
                uint32 NTempId = sEnterFBMgr->GetTempId(Action, Difficulty);
                auto Itr = EnterFBStore.find(Action);
                if (Itr != EnterFBStore.end())
                {
                    player->OnNowFB = sHelper->CheckNeed(player, NTempId, "进入副本消耗");
                    if (player->OnNowFB)
                    {
                        player->SetCanTeleport(true);
                        player->TeleportTo(player->ATLocation);
                    }
                }
            }
        }
        break;
        case GuildBattle:
        {
            uint32 EventId = sGuildBattleMgr->GetUInt(1001);
            if (EventId >= MINEVENTSO)
            {
                player->SendSysMsg("内部错误 请重新进入即可");
                return;
            }

            Guild* DoGuild = player->GetGuild();
            if (!DoGuild)
            {
                player->SendSysMsg("[公会战]: 你没有公会无法使用");
                CloseGossipMenuFor(player);
                return;
            }

            CloseGossipMenuFor(player);

            player->UninviteFromGroup();
            if (player->GetGroup())
                player->RemoveFromGroup();

            sGuildBattleMgr->GuildBattlePlayers.insert(player->GetGUID());

            bool GExist = false;
            for (auto Itr = sGuildBattleMgr->AtGroupMap.begin(); Itr != sGuildBattleMgr->AtGroupMap.end(); Itr++)
            {
                if (Group* Gp = sGroupMgr->GetGroupByGUID(Itr->first))
                {
                    if (!Gp->IsFull() && Itr->second == DoGuild->GetId())
                    {
                        Gp->AddMember(player);
                        GExist = true;
                        break;
                    }
                }
            }

            if (!GExist)
            {
                Group* Gp = new Group();
                Gp->Create(player);
                Gp->ConvertToRaid();
                sGroupMgr->AddGroup(Gp);
                sGuildBattleMgr->AtGroupMap[Gp->GetGUID().ToGuidLow()] = DoGuild->GetId();
            }

            player->TeleToPos(Action);
        }
        break;
        case LearnTab:
        {
            if (sHelper->CheckNeedTalent(player, Sender, Action, "LearnTalent"))
            {
                player->LearnTalent(Sender, Action);
                player->SendTalentsInfoData(false);
            }
        }
        break;
        case AthenaEvent:
        {
            uint32 EventId = sAthenaEventMgr->GetAthenaConfig(101);
            if (EventId >= MINEVENTSO)
            {
                player->SendSysMsg("内部错误");
                return;
            }

            CloseGossipMenuFor(player);

            player->UninviteFromGroup();
            if (player->GetGroup())
                player->RemoveFromGroup();

            bool GExist = false;
            for (auto Itr = sAthenaEventMgr->AthenaEventGroupList.begin(); Itr != sAthenaEventMgr->AthenaEventGroupList.end(); Itr++)
            {
                if (Group* Gp = sGroupMgr->GetGroupByGUID(*Itr))
                {
                    if (!Gp->IsFull())
                    {
                        Gp->AddMember(player);
                        GExist = true;
                        break;
                    }
                }
            }

            if (!GExist)
            {
                Group* Gp = new Group();
                Gp->Create(player);
                Gp->ConvertToRaid();
                sGroupMgr->AddGroup(Gp);
                sAthenaEventMgr->AthenaEventGroupList.insert(Gp->GetGUID().ToGuidLow());
            }

            if (sAthenaEventMgr->PlayerInList.find(player->GetGUID()) == sAthenaEventMgr->PlayerInList.end())
                sAthenaEventMgr->PlayerInList.insert(player->GetGUID());

            player->TeleToPos(Action);
        }
        break;
        case AsPlrBgEvent:
        {
            uint32 EventId = sAsPlayerBattleMgr->GetUInt(5001);
            if (EventId >= MINEVENTSO)
            {
                player->SendSysMsg("内部错误");
                return;
            }

            player->UninviteFromGroup();
            if (player->GetGroup())
                player->RemoveFromGroup();

            FacGroupType FGt = sAsPlayerBattleMgr->AutoFacGroup(player);
            sAsPlayerBattleMgr->SetTempFaction(player, FGt);
            sAsPlayerBattleMgr->AllocationGroupAndTele(player, FGt);
        }
        break;
        }
    }

    void OnStoreNewItem(Player* player, Item* pItem, uint32 Count) override
    {
        player->ApplyItemInStat(pItem, true);
    }
};

void AddSC_PlayerModMgrScripts()
{
    new OnPlayerMod();
}
