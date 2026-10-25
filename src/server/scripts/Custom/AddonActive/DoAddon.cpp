#include "StringConvert.h"

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
#include "Item.h"
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
#include <algorithm>
#include <limits>
#include "Pet.h"
#include "Bag.h"
#include "DoAddon.h"
#include "DBCStores.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"
#include <Custom/_Helper/Helper.h>
#include <Custom/ItemFuncMgr/ItemFuncMgr.h>
#include <Custom/PlayerModMgr/PlayerModMgr.h>
#include <Time/GameTime.h>
#include <Custom/ItemShopMgr/ItemShopMgr.h>
#include <Custom/CustomVendorMgr/CustomVendorMgr.h>
#include <Custom/ClassConfig/ClassMod.h>
#include <Custom/CreatureMod/AsCreature.h>
#include <Cache/CharacterCache.h>
#include <Custom\ItemRandomAtMgr\ItemRandomAtMgr.h>
#include <Custom\ItemRunesMgr\ItemRunesMgr.h>
#include <Custom\Transmog\Transmogrification.h>
#include <Custom/EventMgr/EventMgr.h>
#include <Custom/GuildBattleMgr/GuildBattleMgr.h>
#include <Custom/PlayerActivityMgr/PlayerActivityMgr.h>
#include <Custom/ItemMagicMgr/ItemMagicMgr.h>
#include <Custom/ItemUpMgr/ItemUpMgr.h>
#include <Custom/ItemDevelopMgr/ItemDevelopMgr.h>
#include <Custom/CombatPetMgr/CombatPetMgr.h>
#include <Custom/PicGuideMgr/PicGuideMgr.h>
#include <Custom/BeastCardMgr/BeastCardMgr.h>
#include "FrozenCoreModuleBridge.h"
#include "../../../../../modules/mod-huanjinxitong/src/HuanJingSystem.h"
#include <cstdlib>
#include <iostream>
#include <sstream>

#pragma execution_character_set("utf-8")

namespace
{
ItemAtCacheTemp BuildRandomItemCacheFromLiveItem(Item* item)
{
    ItemAtCacheTemp temp;
    temp.ItemId = item->GetEntry();
    temp.ULevel = item->RmAtLevel;
    temp.Prefix = item->Prefix;
    temp.Suffix = item->Suffix;
    temp.LevelStr = item->LevelStr;
    temp.MinWpDmg = item->AsMinWDmg;
    temp.MaxWpDmg = item->AsMaxWDmg;

    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
    {
        temp.AtStatStore[i].first = item->RmAtStatStore[i].first;
        temp.AtStatStore[i].second = item->RmAtStatStore[i].second;
    }

    for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
        temp.AtSpell[i] = item->RmAtSpell[i];

    for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
        temp._AtSpell[i] = item->_Attribute[i] > 0 ? item->_Attribute[i] : AsInt64(1);

    temp.Mod = sItemRandomAtMgr->GetModForRank(temp.ItemId, temp.ULevel);
    return temp;
}

bool HasLiveRandomItemData(Item* item)
{
    if (!item || !sItemRandomAtMgr->IsGenerateItemAt(item->GetEntry()))
        return false;

    auto hasCustomText = [](std::string const& value)
    {
        return !value.empty() && value != "nullptr";
    };

    if (item->RmAtLevel > 0 || hasCustomText(item->Prefix) || hasCustomText(item->Suffix) || hasCustomText(item->LevelStr))
        return true;

    if (!AsInt64IsZero(item->AsMinWDmg) || !AsInt64IsZero(item->AsMaxWDmg))
        return true;

    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
        if (item->RmAtStatStore[i].first >= 0 && !AsInt64IsZero(item->RmAtStatStore[i].second))
            return true;

    for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
        if (item->RmAtSpell[i])
            return true;

    for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
        if (item->_Attribute[i] > AsInt64(1))
            return true;

    return false;
}

bool SendRandomItemGuidData(Player* player, uint32 guidLow, ItemAtCacheTemp const& data)
{
    if (!player)
        return false;

    std::string sendMsg;
    std::string dps = sHelper->GetWpDPSString(data.MinWpDmg, data.MaxWpDmg, data.ItemId);
    sendMsg.append(std::to_string(guidLow) + "$" + std::to_string(data.ULevel) + "$" + data.Prefix + "$" + data.Suffix + "$" + data.LevelStr + "$");
    sendMsg.append(AsInt64ToString(data.MinWpDmg) + "$" + AsInt64ToString(data.MaxWpDmg) + "$" + dps + "$");

    auto statStore = data.AtStatStore;
    std::sort(statStore.begin(), statStore.end(), [](auto const& x, auto const& y) { return (x.first != -1 && y.first != -1 && x.first < y.first); });

    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
        sendMsg.append(std::to_string(statStore[i].first) + "#");

    sendMsg.append("$");

    for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
        sendMsg.append(AsInt64ToString(statStore[i].second) + "#");

    sendMsg.append("$");

    for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
        sendMsg.append(std::to_string(data.AtSpell[i]) + "#");

    sendMsg.append("$");
    sendMsg.append(std::to_string(data.Mod) + "$");

    std::string attrisStr;
    for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
    {
        AsInt64 multiplier = data._AtSpell[i] > 0 ? data._AtSpell[i] : AsInt64(1);
        sendMsg.append(AsInt64ToString(multiplier) + "#");
        attrisStr.append(AsInt64ToString(multiplier) + "#");
    }

    sendMsg.append("$");
    sendMsg.append(attrisStr);
    sendMsg.append("$");
    sendMsg.append(sItemRandomAtMgr->IsGenerateItemAt(data.ItemId) ? "1" : "0");
    sendMsg.append("$");
    sendMsg.append(std::to_string(data.ItemId));

    sDoAddon->SendPacketToClient(player, "ClientAtGuidData", sendMsg);
    return true;
}

template <typename T>
void FrozenCoreAppend(std::string& signature, T const& value)
{
    signature.append(std::to_string(value));
    signature.push_back('|');
}

void FrozenCoreAppend(std::string& signature, std::string const& value)
{
    signature.append(value);
    signature.push_back('|');
}

uint32 GetFrozenCoreItemId(Player* player, uint32 guidLow, uint32 requestedItemId)
{
    if (requestedItemId > 0)
        return requestedItemId;

    if (player)
    {
        if (Item* liveItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, guidLow)))
            return liveItem->GetEntry();
    }

    auto randomItr = ItemAtCacheStore.find(guidLow);
    if (randomItr != ItemAtCacheStore.end())
        return randomItr->second.ItemId;

    auto runeItr = ItemRunesCacheStore.find(guidLow);
    if (runeItr != ItemRunesCacheStore.end())
        return runeItr->second.ItemId;

    auto developItr = ItemDevelopCacheStore.find(guidLow);
    if (developItr != ItemDevelopCacheStore.end())
        return developItr->second.ItemId;

    return 0;
}

void AppendFrozenCoreModuleSignature(std::string& signature, Player* player, uint32 guidLow, uint32 requestedItemId)
{
    uint32 itemId = GetFrozenCoreItemId(player, guidLow, requestedItemId);
    FrozenCoreAppend(signature, itemId);
    if (itemId == 0)
    {
        FrozenCoreAppend(signature, 0);
        return;
    }

    FrozenCoreModuleData moduleData;
    if (!FrozenCoreQueryItemModuleData(itemId, guidLow, moduleData))
    {
        FrozenCoreAppend(signature, 0);
        return;
    }

    FrozenCoreAppend(signature, moduleData.hasData ? 1 : 0);
    FrozenCoreAppend(signature, moduleData.baseAttributes);
    FrozenCoreAppend(signature, moduleData.additionalAttributes);
    FrozenCoreAppend(signature, moduleData.identificationDisplayData);
    FrozenCoreAppend(signature, moduleData.growthData);
    FrozenCoreAppend(signature, moduleData.enhancementData);
    FrozenCoreAppend(signature, moduleData.skillsData);
    FrozenCoreAppend(signature, moduleData.runeData);
    FrozenCoreAppend(signature, moduleData.setData);
    FrozenCoreAppend(signature, moduleData.huanjingData);
    FrozenCoreAppend(signature, moduleData.pendingIdentifyData);
    FrozenCoreAppend(signature, moduleData.templateStatsData);
}

std::string BuildFrozenCoreServerSignature(Player* player, uint32 guidLow, uint32 requestedItemId)
{
    std::string signature;
    FrozenCoreAppend(signature, GetFrozenCoreItemId(player, guidLow, requestedItemId));

    ItemAtCacheTemp randomData;
    bool hasRandomData = false;
    if (player)
    {
        if (Item* liveItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, guidLow)))
        {
            hasRandomData = HasLiveRandomItemData(liveItem);
            if (hasRandomData)
                randomData = BuildRandomItemCacheFromLiveItem(liveItem);
        }
    }

    if (!hasRandomData)
    {
        auto randomItr = ItemAtCacheStore.find(guidLow);
        if (randomItr != ItemAtCacheStore.end())
        {
            hasRandomData = true;
            randomData = randomItr->second;
        }
    }

    FrozenCoreAppend(signature, hasRandomData ? 1 : 0);
    if (hasRandomData)
    {
        FrozenCoreAppend(signature, randomData.ItemId);
        FrozenCoreAppend(signature, randomData.ULevel);
        FrozenCoreAppend(signature, randomData.Prefix);
        FrozenCoreAppend(signature, randomData.Suffix);
        FrozenCoreAppend(signature, randomData.LevelStr);
        FrozenCoreAppend(signature, AsInt64ToString(randomData.MinWpDmg));
        FrozenCoreAppend(signature, AsInt64ToString(randomData.MaxWpDmg));
        FrozenCoreAppend(signature, randomData.Mod);
        for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
        {
            FrozenCoreAppend(signature, randomData.AtStatStore[i].first);
            FrozenCoreAppend(signature, AsInt64ToString(randomData.AtStatStore[i].second));
        }
        for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
            FrozenCoreAppend(signature, randomData.AtSpell[i]);
        for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
            FrozenCoreAppend(signature, AsInt64ToString(randomData._AtSpell[i]));
    }

    auto enchantItr = EnchantAtStore.find(guidLow);
    FrozenCoreAppend(signature, enchantItr != EnchantAtStore.end() ? 1 : 0);
    if (enchantItr != EnchantAtStore.end())
    {
        FrozenCoreAppend(signature, enchantItr->second.EnchantId1);
        FrozenCoreAppend(signature, enchantItr->second.EnchantId2);
        FrozenCoreAppend(signature, enchantItr->second.EnchantId3);
        FrozenCoreAppend(signature, enchantItr->second.EnchantId4);
        FrozenCoreAppend(signature, enchantItr->second.EnchantId5);
    }

    auto enchantCountItr = ItemEnchAtCountStore.find(guidLow);
    FrozenCoreAppend(signature, enchantCountItr != ItemEnchAtCountStore.end() ? enchantCountItr->second : "");

    auto runeItr = ItemRunesCacheStore.find(guidLow);
    FrozenCoreAppend(signature, runeItr != ItemRunesCacheStore.end() ? 1 : 0);
    if (runeItr != ItemRunesCacheStore.end())
    {
        // Invalidate snapshots created before ClientAtRunesData carried
        // server-resolved rune name/display/icon metadata.
        FrozenCoreAppend(signature, std::string("RuneMetaV1"));
        FrozenCoreAppend(signature, runeItr->second.ItemId);
        FrozenCoreAppend(signature, runeItr->second.NowCount);
        FrozenCoreAppend(signature, runeItr->second.RunesCount);
        for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        {
            FrozenCoreAppend(signature, runeItr->second.ResItems[i]);
            FrozenCoreAppend(signature, runeItr->second.OSpells[i]);
        }
        FrozenCoreAppend(signature, runeItr->second.GSpellName);
        FrozenCoreAppend(signature, runeItr->second.GSpellListStr);
    }

    auto developItr = ItemDevelopCacheStore.find(guidLow);
    FrozenCoreAppend(signature, developItr != ItemDevelopCacheStore.end() ? 1 : 0);
    if (developItr != ItemDevelopCacheStore.end())
    {
        FrozenCoreAppend(signature, developItr->second.ItemId);
        FrozenCoreAppend(signature, developItr->second.Rank);
        FrozenCoreAppend(signature, developItr->second.NowXp);
    }

    AppendFrozenCoreModuleSignature(signature, player, guidLow, requestedItemId);
    return signature;
}

uint64 MakeFrozenCoreCacheId(uint32 guidLow, std::string const& signature)
{
    // Keep IDs in the upper half of Lua's exact integer range. This avoids
    // colliding with old low sequential IDs while remaining exact in WoW Lua.
    uint64 hash = 1469598103934665603ULL;
    auto appendByte = [&hash](uint8 byte)
    {
        hash ^= uint64(byte);
        hash *= 1099511628211ULL;
    };

    for (uint8 shift = 0; shift < 32; shift += 8)
        appendByte(uint8((guidLow >> shift) & 0xFF));
    for (unsigned char byte : signature)
        appendByte(byte);

    hash = (hash & 0x000FFFFFFFFFFFFFULL) | 0x0010000000000000ULL;
    return hash;
}

uint64 GetFrozenCoreServerCacheId(Player* player, uint32 guidLow, uint32 requestedItemId)
{
    return MakeFrozenCoreCacheId(guidLow, BuildFrozenCoreServerSignature(player, guidLow, requestedItemId));
}

bool FrozenCoreSuppressVersion = false;

bool TryGetFrozenCorePayloadGuid(std::string const& packetName, std::string const& payload, uint32& guidLow)
{
    if (packetName != "ClientAtGuidData" && packetName != "AutoEnchant" &&
        packetName != "SoulItemEnchant" && packetName != "ClientAtRunesData" &&
        packetName != "AutoItemEnchAtCount" && packetName != "ClientDevelopData")
        return false;

    std::size_t delimiter = payload.find(packetName == "AutoItemEnchAtCount" || packetName == "ClientDevelopData" ? '#' : '$');
    std::string guidPart = payload.substr(0, delimiter);
    if (guidPart.empty())
        return false;

    auto parsedGuid = Acore::StringTo<uint32>(guidPart);
    if (!parsedGuid || !*parsedGuid)
        return false;
    guidLow = *parsedGuid;
    return true;
}
}

namespace
{
std::string EncodeRunesPayloadField(std::string const& value)
{
    static char const Hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());
    for (unsigned char byte : value)
    {
        if (byte == '%' || byte == '$' || byte == '#' || byte == '~' || byte == '\r' || byte == '\n')
        {
            encoded.push_back('%');
            encoded.push_back(Hex[(byte >> 4) & 0x0F]);
            encoded.push_back(Hex[byte & 0x0F]);
        }
        else
            encoded.push_back(char(byte));
    }
    return encoded;
}
}

namespace
{
    struct ActivityNoticeRow
    {
        uint32 Id = 0;
        uint32 EventId = 0;
        uint32 NeedId = 0;
        uint32 RewardId = 0;
        std::string Name;
        std::string Description;
        std::string Background;
        std::string Button;
        std::string NeedText;
        std::string RewardText;
        std::string GmCommand;
        std::string Type;
        uint32 Remain = 0;
        bool Active = false;
        bool CanClick = false;
    };

    std::string SanitizeActivityNoticeField(std::string Value)
    {
        for (char& Ch : Value)
        {
            if (Ch == '^' || Ch == '\t' || Ch == '\r' || Ch == '\n')
                Ch = ' ';
        }
        return Value;
    }

    std::string ShortActivityEventType(std::string Type)
    {
        std::size_t Pos = Type.find(" - ");
        if (Pos == std::string::npos)
            Pos = Type.find("-");
        if (Pos != std::string::npos)
            Type = Type.substr(0, Pos);

        while (!Type.empty() && (Type.back() == ' ' || Type.back() == '\t'))
            Type.pop_back();

        if (Type == "仅传送")
            Type = "传送";
        return Type;
    }

    // Returns false only when the configuration table is missing. An existing
    // but empty table is valid and intentionally shows the empty state.
    bool LoadActivityNoticeRows(std::vector<ActivityNoticeRow>& Rows)
    {
        Rows.clear();

        uint32 Count = 0;
        {
            QueryResult CountResult = WorldDatabase.Query("SELECT COUNT(*) FROM `_活动_公告`");
            if (!CountResult)
                return false;

            Count = CountResult->Fetch()[0].Get<uint32>();
        }
        if (Count == 0)
            return true;

        {
            QueryResult Result = WorldDatabase.Query(
                "SELECT a.`ID`, a.`事件ID`, a.`活动名称`, a.`活动介绍`, a.`活动背景图`, "
                "a.`按钮名称`, a.`需求`, a.`奖励`, a.`触发GM命令`, "
                "IFNULL(e.`事件名称`, ''), IFNULL(e.`事件类型`, ''), IFNULL(e.`弹窗提示文本`, ''), IFNULL(e.`事件ID`, 0) "
                "FROM `_活动_公告` a "
                "LEFT JOIN `_事件` e ON e.`事件ID` = a.`事件ID` "
                "ORDER BY a.`ID` ASC");
            if (!Result)
                return true;

            do
            {
                Field* Fields = Result->Fetch();
                ActivityNoticeRow Row;
                Row.Id = Fields[0].Get<uint32>();
                Row.EventId = Fields[1].Get<uint32>();
                Row.Name = SanitizeActivityNoticeField(Fields[2].Get<std::string>());
                Row.Description = SanitizeActivityNoticeField(Fields[3].Get<std::string>());
                Row.Background = SanitizeActivityNoticeField(Fields[4].Get<std::string>());
                Row.Button = SanitizeActivityNoticeField(Fields[5].Get<std::string>());
                Row.NeedId = Fields[6].Get<uint32>();
                Row.RewardId = Fields[7].Get<uint32>();
                Row.GmCommand = SanitizeActivityNoticeField(Fields[8].Get<std::string>());

                std::string EventName = SanitizeActivityNoticeField(Fields[9].Get<std::string>());
                std::string EventType = ShortActivityEventType(Fields[10].Get<std::string>());
                std::string EventPopup = SanitizeActivityNoticeField(Fields[11].Get<std::string>());
                bool EventConfigured = Fields[12].Get<uint32>() != 0;

                if (Row.Name.empty())
                    Row.Name = EventName;
                if (Row.Name.empty())
                    Row.Name = "活动 " + std::to_string(Row.Id);
                if (Row.Description.empty())
                    Row.Description = EventPopup;
                if (Row.Button.empty())
                    Row.Button = "传送";

                Row.Type = EventType;
                Row.Active = Row.EventId > MINEVENTSO && sGameEventMgr->IsActiveEvent(uint16(Row.EventId));
                Row.Remain = Row.EventId > MINEVENTSO ? sGameEventMgr->NextCheck(uint16(Row.EventId)) : 0;
                Row.CanClick = !Row.GmCommand.empty() || Row.EventId == 0 || (EventConfigured && Row.Active);

                Rows.push_back(Row);
            } while (Result->NextRow());
        }

        // Resolve requirement/reward text only after the SELECT result has been
        // released. The helper can run its own database queries.
        for (ActivityNoticeRow& Row : Rows)
        {
            if (Row.NeedId)
                Row.NeedText = SanitizeActivityNoticeField(sHelper->GetNeedGts(Row.NeedId));
            if (Row.RewardId)
                Row.RewardText = SanitizeActivityNoticeField(sHelper->GetRewardGts(Row.RewardId));
        }

        std::stable_sort(Rows.begin(), Rows.end(), [](ActivityNoticeRow const& A, ActivityNoticeRow const& B)
        {
            return A.Active && !B.Active;
        });

        return true;
    }

    // Returns false when the table is missing so the caller can keep the old
    // game_event listing behavior as a safe fallback.
    bool SendActivityNotices(Player* Player)
    {
        std::vector<ActivityNoticeRow> Rows;
        if (!LoadActivityNoticeRows(Rows))
            return false;

        sDoAddon->SendPacketToClient(Player, "AutoEventSizeNow", std::to_string(Rows.size()));

        uint16 Num = 0;
        for (ActivityNoticeRow const& Row : Rows)
        {
            ++Num;
            std::string Payload = std::to_string(Num) + "^" +
                std::to_string(Row.Id) + "^" +
                std::to_string(Row.EventId) + "^" +
                Row.Name + "^" +
                Row.Description + "^" +
                Row.Background + "^" +
                Row.Button + "^" +
                Row.NeedText + "^" +
                Row.RewardText + "^" +
                (Row.Active ? "1" : "0") + "^" +
                std::to_string(Row.Remain) + "^" +
                Row.Type + "^" +
                (Row.CanClick ? "1" : "0");
            sDoAddon->SendPacketToClient(Player, "AutoEventDataNow", Payload);
        }

        return true;
    }

    void HandleActivityNoticeClick(Player* Player, uint32 NoticeId)
    {
        if (!NoticeId || !Player)
            return;

        uint32 EventId = 0;
        uint32 NeedId = 0;
        uint32 RewardId = 0;
        std::string GmCommand;
        std::string Name;
        bool EventConfigured = false;
        {
            QueryResult Result = WorldDatabase.Query(
                "SELECT a.`事件ID`, a.`需求`, a.`奖励`, a.`触发GM命令`, a.`活动名称`, IFNULL(e.`事件ID`, 0) "
                "FROM `_活动_公告` a LEFT JOIN `_事件` e ON e.`事件ID` = a.`事件ID` "
                "WHERE a.`ID` = " + std::to_string(NoticeId));
            if (!Result)
            {
                Player->SendSysMsg("活动公告不存在或已被删除");
                return;
            }

            Field* Fields = Result->Fetch();
            EventId = Fields[0].Get<uint32>();
            NeedId = Fields[1].Get<uint32>();
            RewardId = Fields[2].Get<uint32>();
            GmCommand = Fields[3].Get<std::string>();
            Name = Fields[4].Get<std::string>();
            EventConfigured = Fields[5].Get<uint32>() != 0;
        }

        bool HasCommand = !GmCommand.empty();
        bool HasEvent = EventId > MINEVENTSO;

        if (!HasCommand && HasEvent)
        {
            if (!EventConfigured)
            {
                Player->SendSysMsg("活动尚未配置完成，无法进入");
                return;
            }
            if (!sGameEventMgr->IsActiveEvent(uint16(EventId)))
            {
                Player->SendSysMsg("活动尚未开始");
                return;
            }
        }

        if (NeedId && !sHelper->CheckNeed(Player, NeedId, "活动公告需求"))
            return;

        if (RewardId)
            sHelper->DoTheReward(Player, RewardId, "活动公告奖励");

        if (HasCommand)
            Player->DoCommands(GmCommand);
        else if (HasEvent)
        {
            if (!sEvent->GetEventData(EventId))
            {
                Player->SendSysMsg("活动数据尚未加载，请稍后再试");
                return;
            }
            sEvent->StartPlayerInEvent(Player, EventId);
        }

        if (!Name.empty())
            Player->SendSysMsg("活动公告：%s", Name.c_str());
    }
}

std::string BuildRunesDataPayload(uint32 guidLow, ItemRunesCacheTemp const& data)
{
    std::string payload = std::to_string(guidLow) + "$" + std::to_string(data.NowCount) + "$" + std::to_string(data.RunesCount) + "$";

    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        payload.append(std::to_string(data.ResItems[i]) + "#");

    payload.append("$");
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
        payload.append(std::to_string(data.OSpells[i]) + "#");

    payload.append("$");
    payload.append(data.GSpellName.empty() ? "nullptr$" : EncodeRunesPayloadField(data.GSpellName) + "$");
    payload.append(data.GSpellListStr.empty() ? "nullptr$" : EncodeRunesPayloadField(data.GSpellListStr) + "$");
    payload.append(std::to_string(data.ItemId) + "$");

    // Metadata is resolved on the server from item_template and ItemDisplayInfo.dbc.
    // The client can therefore render rune names/icons without GetItemInfo requests.
    for (uint8 i = 0; i < MAX_RUNES_COUNT; ++i)
    {
        uint32 runeItemId = data.ResItems[i];
        uint32 displayId = 0;
        std::string name;
        std::string icon;
        if (runeItemId > 0)
        {
            if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(runeItemId))
            {
                displayId = proto->DisplayInfoID;
                name = proto->Name1;
                if (ItemDisplayInfoEntry const* display = sItemDisplayInfoStore.LookupEntry(displayId))
                    if (display->inventoryIcon)
                        icon = display->inventoryIcon;
            }
        }

        if (i > 0)
            payload.push_back('#');
        payload.append(std::to_string(runeItemId));
        payload.push_back('~');
        payload.append(std::to_string(displayId));
        payload.push_back('~');
        payload.append(EncodeRunesPayloadField(name));
        payload.push_back('~');
        payload.append(EncodeRunesPayloadField(icon));
    }

    return payload;
}

bool DoAddon::IsPacket(std::string RevPacket, std::string Packet)
{
    return strcmp(RevPacket.c_str(), Packet.c_str()) == 0;
}

void DoAddon::SendPacketToClient(Player* Sender, std::string HMsg, std::string SMsg)
{
    if (!Sender)
        return;

    if (!FrozenCoreSuppressVersion)
    {
        uint32 guidLow = 0;
        if (TryGetFrozenCorePayloadGuid(HMsg, SMsg, guidLow))
        {
            uint64 cacheId = GetFrozenCoreServerCacheId(Sender, guidLow, 0);
            SendPacketToClient(Sender, "FrozenCoreCacheVersion", std::to_string(guidLow) + "$" + std::to_string(cacheId));
        }
    }

    std::string Full = HMsg + "\t" + SMsg;

    WorldPacket PacketData(SMSG_MESSAGECHAT, 100);
    PacketData << uint8(CHAT_MSG_WHISPER);
    PacketData << int32(LANG_ADDON);
    PacketData << uint64(Sender->GetGUID().GetRawValue());
    PacketData << uint32(0);
    PacketData << uint64(Sender->GetGUID().GetRawValue());
    PacketData << uint32(Full.length() + 1);
    PacketData << Full;
    PacketData << uint8(0);
    Sender->GetSession()->SendPacket(&PacketData);
}

void DoAddon::OnAddonMessage(Player* player, const char* HMsg, const char* RevMsg)
{
    if (!player || !HMsg || !RevMsg)
        return;

    std::string RHMsg = HMsg;
    std::string RMsg = RevMsg;

    if (sBeastCardMgr->HandleAddonMessage(player, RHMsg, RMsg))
        return;

    if (IsPacket(RHMsg, "GetGuildBattleUI"))
    {
        sGuildBattleMgr->SendGuildBattleUI(player);
    }
    else if (IsPacket(RHMsg, "GetAsPlayerBattleUI"))
    {
        sAsPlayerBattleMgr->SendAsPlayerBattleUI(player);
    }
    else if (IsPacket(RHMsg, "TooltipDebug"))
    {
        std::string logMsg = "[插件Tooltip定位] 玩家=";
        logMsg += player ? player->GetName() : "nullptr";
        logMsg += " ";
        logMsg += RMsg;
        LOG_INFO("server.worldserver", "{}", logMsg);
    }
    else if (IsPacket(RHMsg, "GetAuctionHouseInfo"))
    {
        int32 HouseType = sHelper->GetInt(1005);
        uint32 TokenItem = sHelper->GetUInt(1018);
        SendPacketToClient(player, "AuctionHouseInfo", std::to_string(HouseType) + "#" + sHelper->GetItemLink(TokenItem));
    }
    else if (IsPacket(RHMsg, "FrozenCoreCacheCheck"))
    {
        // Request format: guidLow$serverCacheId.  A hit returns only a
        // small acknowledgement; a miss falls through to the existing payload
        // sender so all current addon fields remain wire-compatible.
        std::string requestMsg = RevMsg ? RevMsg : "";
        std::vector<std::string> parts;
        std::size_t start = 0;
        while (start <= requestMsg.size())
        {
            std::size_t end = requestMsg.find('$', start);
            parts.push_back(requestMsg.substr(start, end == std::string::npos ? std::string::npos : end - start));
            if (end == std::string::npos)
                break;
            start = end + 1;
        }

        if (parts.size() < 2 || !player)
            return;

        auto parsedGuid = Acore::StringTo<uint32>(parts[0]);
        // Legacy Lua clients may stringify a cache ID in scientific notation.
        // Treat it as a cache miss so normal item data still reaches those clients.
        uint64 clientCacheId = Acore::StringTo<uint64>(parts[1]).value_or(0);
        auto parsedItemId = parts.size() >= 3 ? Acore::StringTo<uint32>(parts[2]) : Optional<uint32>(0);
        if (!parsedGuid || !*parsedGuid || !parsedItemId)
            return;
        uint32 guidLow = *parsedGuid;
        uint32 requestedItemId = *parsedItemId;

        uint32 itemId = GetFrozenCoreItemId(player, guidLow, requestedItemId);
        uint64 serverCacheId = GetFrozenCoreServerCacheId(player, guidLow, itemId);
        auto cacheIdText = std::to_string(guidLow) + "$" + std::to_string(serverCacheId);
        if (clientCacheId != 0 && clientCacheId == serverCacheId)
        {
            SendPacketToClient(player, "FrozenCoreCacheHit", cacheIdText);
            return;
        }

        // Publish only the new cache ID first.  The following legacy packets
        // carry the changed fields; FrozenCore stores them under this ID and
        // keeps every older ID linked to the same GUID.
        SendPacketToClient(player, "FrozenCoreCacheVersion", cacheIdText);
        std::string legacyRequest = std::to_string(guidLow) + "$" + std::to_string(itemId);
        FrozenCoreSuppressVersion = true;
        OnAddonMessage(player, "GetGuidLowStoreData", legacyRequest.c_str());
        if (itemId > 0)
            FrozenCoreSendItemModuleData(player, itemId, guidLow);
        FrozenCoreSuppressVersion = false;
    }
    else if (IsPacket(RHMsg, "GetGuidLowStoreData"))
    {
        std::string requestMsg = RevMsg ? RevMsg : "";
        std::string guidPart = requestMsg;
        uint32 RequestedItemId = 0;
        if (std::size_t splitPos = requestMsg.find('$'); splitPos != std::string::npos)
        {
            guidPart = requestMsg.substr(0, splitPos);
            auto parsedItemId = Acore::StringTo<uint32>(requestMsg.substr(splitPos + 1));
            if (!parsedItemId)
                return;
            RequestedItemId = *parsedItemId;
        }

        auto parsedGuid = Acore::StringTo<uint32>(guidPart);
        if (!parsedGuid || !*parsedGuid)
            return;
        uint32 ItemGuidLow = *parsedGuid;
        bool SentGuidPayload = false;

        {
            auto Itr = EnchantAtStore.find(ItemGuidLow);
            if (Itr != EnchantAtStore.end())
            {
                std::string SendMsg = std::to_string(Itr->second.EnchantId1) + "#" + std::to_string(Itr->second.EnchantId2) + "#" + std::to_string(Itr->second.EnchantId3) + "#" + std::to_string(Itr->second.EnchantId4) + "#" + std::to_string(Itr->second.EnchantId5) + "#";
                SendPacketToClient(player, "AutoEnchant", std::to_string(Itr->first) + "$" + SendMsg);
                SentGuidPayload = true;
            }
        }

        {
            for (uint8 Index = SOUL_SLOT_START; Index < SOUL_SLOT_END; ++Index)
            {
                uint32 LGuid = sItemFuncMgr->GetSoulItemGuid(player->GetGUIDLow(), Index);
                if (!LGuid || LGuid != ItemGuidLow)
                    continue;
                if (Item* OItem = sItemFuncMgr->GetEquipSoulItem(LGuid))
                {
                    if (RequestedItemId && OItem->GetEntry() != RequestedItemId)
                        continue;

                    {
                        std::string SendMsg;
                        for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
                        {
                            uint32 EnchantId = OItem->GetEnchantmentId(EnchantmentSlot(ESlot));
                            SendMsg.append(std::to_string(EnchantId) + "#");
                        }

                        if (!SendMsg.empty())
                        {
                            bool const isSoulMainHand = sItemFuncMgr->GetOriginalSlot(Index) == EQUIPMENT_SLOT_MAINHAND;
                            char const* packetName = isSoulMainHand ? "SoulItemEnchant" : "AutoEnchant";
                            std::string packetData = std::to_string(LGuid) + "$";
                            if (isSoulMainHand)
                                packetData.append(std::to_string(OItem->GetEntry()) + "$");
                            packetData.append(SendMsg);
                            SendPacketToClient(player, packetName, packetData);
                            SentGuidPayload = true;
                        }
                    }

                    {
                        auto Itr = ItemAtCacheStore.find(LGuid);
                        if (Itr != ItemAtCacheStore.end())
                        {
                            std::string SendMsg;
                            float Mod = sItemRandomAtMgr->GetModForRank(Itr->second.ItemId, Itr->second.ULevel);
                            std::string DPS = sHelper->GetWpDPSString(Itr->second.MinWpDmg, Itr->second.MaxWpDmg, Itr->second.ItemId);
                            SendMsg.append(std::to_string(Itr->first) + "$" + std::to_string(Itr->second.ULevel) + "$" + Itr->second.Prefix + "$" + Itr->second.Suffix + "$" + Itr->second.LevelStr + "$");
                            SendMsg.append(AsInt64ToString(Itr->second.MinWpDmg) + "$" + AsInt64ToString(Itr->second.MaxWpDmg) + "$" + DPS + "$");

                            std::sort(Itr->second.AtStatStore.begin(), Itr->second.AtStatStore.end(), [](auto const& X, auto const& Y) { return (X.first != -1 && Y.first != -1 && X.first < Y.first); });

                            for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
                                SendMsg.append(std::to_string(Itr->second.AtStatStore[i].first) + "#");

                            SendMsg.append("$");

                            for (uint8 i = 0; i < MAX_AT_A_COUNT; ++i)
                                SendMsg.append(AsInt64ToString(Itr->second.AtStatStore[i].second) + "#");

                            SendMsg.append("$");

                            for (uint8 i = 0; i < MAX_AT_B_COUNT; ++i)
                                SendMsg.append(std::to_string(Itr->second.AtSpell[i]) + "#");

                            SendMsg.append("$");

                            SendMsg.append(std::to_string(Mod) + "$");

                            //------------   随机属性倍率   ------------
                            std::string AttrisStr;
                            for (uint8 i = 0; i < MAX_SX_B_COUNT; ++i)
                            {
                                AsInt64 multiplier = Itr->second._AtSpell[i] > 0 ? Itr->second._AtSpell[i] : AsInt64(1);
                                SendMsg.append(AsInt64ToString(multiplier) + "#");
                                AttrisStr.append(AsInt64ToString(multiplier) + "#");
                            }
                            //------------   随机属性倍率   ------------

                            SendMsg.append("$");
                            SendMsg.append(AttrisStr);
                            SendMsg.append("$");
                            SendMsg.append(sItemRandomAtMgr->IsGenerateItemAt(Itr->second.ItemId) ? "1" : "0");
                            SendMsg.append("$");
                            SendMsg.append(std::to_string(Itr->second.ItemId));

                            SendPacketToClient(player, "ClientAtGuidData", SendMsg);
                        }
                    }

                    {
                        auto Itr = ItemRunesCacheStore.find(LGuid);
                        if (Itr != ItemRunesCacheStore.end())
                            SendPacketToClient(player, "ClientAtRunesData", BuildRunesDataPayload(Itr->first, Itr->second));
                    }

                    {
                        auto Itr = ItemEnchAtCountStore.find(LGuid);
                        if (Itr != ItemEnchAtCountStore.end())
                        {
                            SendPacketToClient(player, "AutoItemEnchAtCount", std::to_string(Itr->first) + "#" + Itr->second);
                        }
                    }

                    {
                        auto Itr = ItemDevelopCacheStore.find(LGuid);
                        if (Itr != ItemDevelopCacheStore.end())
                        {
                            if (sItemDevelopMgr->IsInItemDevelop(Itr->second.ItemId))
                            {
                                uint32 NextRankXp = sItemDevelopMgr->GetDevelopNextRankXp(Itr->second.ItemId, Itr->second.Rank + 1);
                                std::string SendMsg;
                                SendMsg.append(std::to_string(Itr->first) + "#" + std::to_string(Itr->second.Rank) + "#" + std::to_string(Itr->second.NowXp) + "#" + std::to_string(NextRankXp));
                                SendPacketToClient(player, "ClientDevelopData", SendMsg);
                            }
                        }
                    }
                }
            }
        }

        {
            bool CheckedLiveItem = false;
            if (player)
            {
                Item* LiveItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuidLow));
                if (LiveItem)
                {
                    CheckedLiveItem = true;
                    if (!RequestedItemId || LiveItem->GetEntry() == RequestedItemId)
                    {
                        bool HasLiveData = HasLiveRandomItemData(LiveItem);
                        ItemAtCacheTemp LiveCache = BuildRandomItemCacheFromLiveItem(LiveItem);

                        if (HasLiveData)
                        {
                            ItemAtCacheStore[ItemGuidLow] = LiveCache;
                            SentGuidPayload = SendRandomItemGuidData(player, ItemGuidLow, LiveCache) || SentGuidPayload;
                        }
                        else
                        {
                            ItemAtCacheStore.erase(ItemGuidLow);
                        }
                    }
                }
            }

            if (!CheckedLiveItem)
            {
                auto Itr = ItemAtCacheStore.find(ItemGuidLow);
                if (Itr != ItemAtCacheStore.end() && (!RequestedItemId || Itr->second.ItemId == RequestedItemId))
                {
                    SentGuidPayload = SendRandomItemGuidData(player, ItemGuidLow, Itr->second) || SentGuidPayload;
                }
            }
        }

        {
            auto Itr = ItemRunesCacheStore.find(ItemGuidLow);
            if (Itr != ItemRunesCacheStore.end())
            {
                SendPacketToClient(player, "ClientAtRunesData", BuildRunesDataPayload(Itr->first, Itr->second));
                SentGuidPayload = true;
            }
        }

        {
            auto Itr = ItemEnchAtCountStore.find(ItemGuidLow);
            if (Itr != ItemEnchAtCountStore.end())
            {
                SendPacketToClient(player, "AutoItemEnchAtCount", std::to_string(Itr->first) + "#" + Itr->second);
                SentGuidPayload = true;
            }
        }

        {
            auto Itr = ItemDevelopCacheStore.find(ItemGuidLow);
            if (Itr != ItemDevelopCacheStore.end())
            {
                if (sItemDevelopMgr->IsInItemDevelop(Itr->second.ItemId))
                {
                    uint32 NextRankXp = sItemDevelopMgr->GetDevelopNextRankXp(Itr->second.ItemId, Itr->second.Rank + 1);
                    std::string SendMsg;
                    SendMsg.append(std::to_string(Itr->first) + "#" + std::to_string(Itr->second.Rank) + "#" + std::to_string(Itr->second.NowXp) + "#" + std::to_string(NextRankXp));
                    SendPacketToClient(player, "ClientDevelopData", SendMsg);
                    SentGuidPayload = true;
                }
            }
        }

        if (!SentGuidPayload)
        {
            SendPacketToClient(player, "ClientGuidNoData", std::to_string(ItemGuidLow));
        }
    }
    else if (IsPacket(RHMsg, "GetEntryIdStoreData"))
    {
        auto parsedItemId = Acore::StringTo<uint32>(RevMsg);
        if (!parsedItemId)
            return;
        uint32 ItemId = *parsedItemId;

        {
            std::string TypeStore;
            std::string ValueStore;
            std::string DamageMinStore;
            std::string DamageMaxStore;
            std::string DpsStore;

            if (ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(ItemId))
            {
                for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS && i < Proto->StatsCount; ++i)
                {
                    uint64 Key = (uint64(ItemId) << 8) | i;
                    if (BigItemTemplateStatStore.find(Key) != BigItemTemplateStatStore.end())
                        continue;

                    uint32 StatType = Proto->ItemStat[i].ItemStatType;
                    AsInt64 const& StatValue = Proto->AsItemStatValue[i];
                    if (StatType >= MAX_ITEM_MOD || StatValue <= AsInt64(CT_MAX_INT))
                        continue;

                    TypeStore.append(std::to_string(StatType) + "#");
                    ValueStore.append(AsInt64ToString(StatValue) + "#");
                }

                AsInt64 DamageMin = AsInt64Zero();
                AsInt64 DamageMax = AsInt64Zero();
                for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
                {
                    DamageMin += Proto->AsDamageMin[i];
                    DamageMax += Proto->AsDamageMax[i];
                }

                if (DamageMin > AsInt64(CT_MAX_INT) || DamageMax > AsInt64(CT_MAX_INT))
                {
                    DamageMinStore = AsInt64ToString(DamageMin);
                    DamageMaxStore = AsInt64ToString(DamageMax);
                    if (Proto->Delay)
                    {
                        AsInt64 Dps10 = (DamageMin + DamageMax) * AsInt64(5000) / AsInt64(Proto->Delay);
                        AsInt64 DpsInteger = Dps10 / AsInt64(10);
                        AsInt64 DpsDecimal = Dps10 % AsInt64(10);
                        DpsStore = AsInt64ToString(DpsInteger) + "." + AsInt64ToString(DpsDecimal);
                    }
                    else
                        DpsStore = "0";
                }
            }

            for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
            {
                uint64 Key = (uint64(ItemId) << 8) | i;
                auto Itr = BigItemTemplateStatStore.find(Key);
                if (Itr == BigItemTemplateStatStore.end() || Itr->second.StatType >= MAX_ITEM_MOD || AsInt64IsZero(Itr->second.StatValue))
                    continue;

                TypeStore.append(std::to_string(Itr->second.StatType) + "#");
                ValueStore.append(AsInt64ToString(Itr->second.StatValue) + "#");
            }

            std::string SendTypeStore = TypeStore.empty() ? "nullptr" : TypeStore;
            std::string SendValueStore = ValueStore.empty() ? "nullptr" : ValueStore;
            std::string SendDamageMinStore = DamageMinStore.empty() ? "nullptr" : DamageMinStore;
            std::string SendDamageMaxStore = DamageMaxStore.empty() ? "nullptr" : DamageMaxStore;
            std::string SendDpsStore = DpsStore.empty() ? "nullptr" : DpsStore;
            SendPacketToClient(player, "AutoBigItemStatData", std::to_string(ItemId) + "$" + SendTypeStore + "$" + SendValueStore);
            SendPacketToClient(player, "AutoBigItemWeaponData", std::to_string(ItemId) + "$" + SendDamageMinStore + "$" + SendDamageMaxStore + "$" + SendDpsStore);
        }

        {
            auto Itr = AtAddItemLimitStore.find(ItemId);
            if (Itr != AtAddItemLimitStore.end())
            {
                if (Itr->second.CharDayLimt)
                {
                    uint32 Now = 0;
                    auto It = player->PlayerDayItemLimitMap.find(Itr->first);
                    if (It != player->PlayerDayItemLimitMap.end())
                        Now = It->second;

                    SendPacketToClient(player, "AutoItemLimitData1", std::to_string(Itr->first) + "#" + std::to_string(Now) + "#" + std::to_string(Itr->second.CharDayLimt));
                }

                if (Itr->second.CharFrLimt)
                {
                    uint32 Now = 0;
                    auto It = player->PlayerFrItemLimitMap.find(Itr->first);
                    if (It != player->PlayerFrItemLimitMap.end())
                        Now = It->second;

                    SendPacketToClient(player, "AutoItemLimitData2", std::to_string(Itr->first) + "#" + std::to_string(Now) + "#" + std::to_string(Itr->second.CharFrLimt));
                }

                if (Itr->second.AccDayLimt)
                {
                    uint32 Now = 0;
                    auto It = player->PlayerAccDayItemLimitMap.find(Itr->first);
                    if (It != player->PlayerAccDayItemLimitMap.end())
                        Now = It->second;

                    SendPacketToClient(player, "AutoItemLimitData3", std::to_string(Itr->first) + "#" + std::to_string(Now) + "#" + std::to_string(Itr->second.AccDayLimt));
                }

                if (Itr->second.AccFrLimt)
                {
                    uint32 Now = 0;
                    auto It = player->PlayerAccFrItemLimitMap.find(Itr->first);
                    if (It != player->PlayerAccFrItemLimitMap.end())
                        Now = It->second;

                    SendPacketToClient(player, "AutoItemLimitData4", std::to_string(Itr->first) + "#" + std::to_string(Now) + "#" + std::to_string(Itr->second.AccFrLimt));
                }

            }
        }

        {
            auto Itr = UseItemLimitStore.find(ItemId);
            if (Itr != UseItemLimitStore.end())
            {
                if (Itr->second.UseDayLimt)
                {
                    uint32 Now = 0;
                    auto It = player->DayUseItemLimitMap.find(Itr->first);
                    if (It != player->DayUseItemLimitMap.end())
                        Now = It->second;

                    SendPacketToClient(player, "AutoItemUseLimitData1", std::to_string(Itr->first) + "#" + std::to_string(Now) + "#" + std::to_string(Itr->second.UseDayLimt));
                }

                if (Itr->second.UseFrLimt)
                {
                    uint32 Now = 0;
                    auto It = player->FrUseItemLimitMap.find(Itr->first);
                    if (It != player->FrUseItemLimitMap.end())
                        Now = It->second;

                    SendPacketToClient(player, "AutoItemUseLimitData2", std::to_string(Itr->first) + "#" + std::to_string(Now) + "#" + std::to_string(Itr->second.UseFrLimt));
                }
            }
        }
    }
    else if (IsPacket(RHMsg, "GetSignData"))
    {
        if (!sHelper->GetBoolean(1402))
            return;

        time_t NowTime = time(NULL);
        tm ATm;
        localtime_r(&NowTime, &ATm);
        uint32 NowYear = ATm.tm_year + 1900;
        uint32 NowMonth = ATm.tm_mon + 1;
        uint32 Days = sHelper->GetDaysInMonth(NowYear, NowMonth);

        uint32 CDays = uint32(sHelper->GetUInt(1051));
        uint32 TDays = uint32(sHelper->GetUInt(1052));
        SendPacketToClient(player, "ConfigSignData", std::to_string(CDays) + "$" + std::to_string(TDays) + "$");

        std::vector<uint32> DoList = player->SignStore;
        uint32 SeriesDays = sHelper->GetSeriesDays(DoList);
        for (auto Itr = DoList.begin(); Itr != DoList.end(); Itr++)
        {
            SendPacketToClient(player, "PlayerSignData", std::to_string(*Itr) + "$" + std::to_string(SeriesDays) + "$" + std::to_string(DoList.size()) + "$");
        }
        for (auto It = SignRewardStore.begin(); It != SignRewardStore.end(); It++)
        {
            SendPacketToClient(player, "RewardSignData", std::to_string(It->first) + "$"
                + std::to_string(It->second.ItemId1) + "#"
                + std::to_string(It->second.Count1) + "#"
                + std::to_string(It->second.ItemId2) + "#"
                + std::to_string(It->second.Count2) + "#"
                + std::to_string(It->second.ItemId3) + "#"
                + std::to_string(It->second.Count3) + "#"
                + std::to_string(It->second.ItemId4) + "#"
                + std::to_string(It->second.Count4) + "#"
                + std::to_string(It->second.ItemId5) + "#"
                + std::to_string(It->second.Count5) + "#"
                + std::to_string(It->second.ItemId6) + "#"
                + std::to_string(It->second.Count6) + "#"
            );
        }
    }
    else if (IsPacket(RHMsg, "OnStartSign"))
    {
        if (!sHelper->GetBoolean(1402))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 3)
            return;

        auto parsedDataYear = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedDataYear)
            return;
        uint32 DataYear = *parsedDataYear;
        auto parsedDataMonth = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedDataMonth)
            return;
        uint32 DataMonth = *parsedDataMonth;
        auto parsedUiDay = Acore::StringTo<uint32>(Datas[2]);
        if (!parsedUiDay)
            return;
        uint32 UiDay = *parsedUiDay;

        time_t NowTime = time(NULL);
        tm ATm;
        localtime_r(&NowTime, &ATm);

        uint32 NowYear = ATm.tm_year + 1900;
        uint32 NowMonth = ATm.tm_mon + 1;
        uint32 NowDay = ATm.tm_mday;

        if (NowYear == DataYear && NowMonth == DataMonth && UiDay <= NowDay)
        {
            bool IsRepair = (UiDay < NowDay);
            player->OnRepSign(UiDay, IsRepair);
        }
        else
        {
            player->SendSysMsg(sHelper->GetGts(1122));
        }
    }
    else if (IsPacket(RHMsg, "ReqRepair"))
    {
        if (!sHelper->GetBoolean(1402))
            return;
        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 3)
            return;

        auto parsedDataYear = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedDataYear)
            return;
        uint32 DataYear = *parsedDataYear;
        auto parsedDataMonth = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedDataMonth)
            return;
        uint32 DataMonth = *parsedDataMonth;
        auto parsedUiDay = Acore::StringTo<uint32>(Datas[2]);
        if (!parsedUiDay)
            return;
        uint32 UiDay = *parsedUiDay;

        uint32 NTempId = uint32(sHelper->GetUInt(1050));
        if (!NTempId)
        {
            time_t NowTime = time(NULL);
            tm ATm;
            localtime_r(&NowTime, &ATm);

            uint32 NowYear = ATm.tm_year + 1900;
            uint32 NowMonth = ATm.tm_mon + 1;
            uint32 NowDay = ATm.tm_mday;

            if (NowYear == DataYear && NowMonth == DataMonth && UiDay <= NowDay)
            {
                bool IsRepair = (UiDay < NowDay);
                player->OnRepSign(UiDay, IsRepair);
            }
            else
            {
                player->SendSysMsg(sHelper->GetGts(1122));
            }
        }
        else
        {
            std::string Gts = sHelper->GetNeedGts(NTempId);
            SendPacketToClient(player, "SendRepair", RMsg + "$" + Gts);
        }
    }
    else if (IsPacket(RHMsg, "GetAliveData"))
    {
        SendPacketToClient(player, "OnPlayerAliveDead", std::to_string(sHelper->GetUInt(3001)));

        uint32 AllowCount = uint32(sHelper->GetUInt(1060));
        uint32 MeCount = player->GetTempAtUInt32(AT01);
        uint32 Count = (AllowCount > MeCount) ? (AllowCount - MeCount) : 0;
        SendPacketToClient(player, "PlayerAliveData", std::to_string(Count));
    }
    else if (IsPacket(RHMsg, "ReviveReset"))
    {
        uint32 NTempId = uint32(sHelper->GetUInt(1061));
        if (!sHelper->CheckNeed(player, NTempId, "死亡复活功能消耗"))
            return;

        player->ResurrectPlayer(1.0f);
        player->SpawnCorpseBones();
    }
    else if (IsPacket(RHMsg, "ReqReviveMe"))
    {
        uint32 NTempId = uint32(sHelper->GetUInt(1061));
        if (!NTempId)
        {
            player->ResurrectPlayer(1.0f);
            player->SpawnCorpseBones();
        }
        else
        {
            std::string Gts = sHelper->GetNeedGts(NTempId);
            SendPacketToClient(player, "SendReviveMe", Gts);
        }
    }
    else if (IsPacket(RHMsg, "ReviveRequest"))
    {
        uint32 AllowCount = uint32(sHelper->GetUInt(1060));
        uint32 MeCount = player->GetTempAtUInt32(AT01);
        if (MeCount >= AllowCount)
        {
            player->SendSysMsg(sHelper->GetGts(1130));
            return;
        }

        player->UpdateTempAt(AT01, MeCount + 1);
        MeCount = player->GetTempAtUInt32(AT01);

        uint32 Count = (AllowCount > MeCount) ? (AllowCount - MeCount) : 0;

        player->ResurrectPlayer(1.0f);
        player->SpawnCorpseBones();
        SendPacketToClient(player, "PlayerAliveData", std::to_string(Count));
    }
    else if (IsPacket(RHMsg, "GetShopTabName"))
    {
        sItemShopMgr->SendTabNames(player);
    }
    else if (IsPacket(RHMsg, "GetShopTabPage"))
    {
        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 2)
            return;

        auto parsedTab = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedTab)
            return;
        uint32 Tab = *parsedTab;
        auto parsedPage = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedPage)
            return;
        uint32 Page = *parsedPage;
        sItemShopMgr->SendDataForPage(player, Tab, Page);
    }
    else if (IsPacket(RHMsg, "RequestBuyNeedInfo"))
    {
        auto parsedItemId = Acore::StringTo<uint32>(RevMsg);
        if (!parsedItemId)
            return;
        uint32 ItemId = *parsedItemId;
        sItemShopMgr->SendNTempGtsForPage(player, ItemId);
    }
    else if (IsPacket(RHMsg, "AcceptBuyShopItem"))
    {
        if (!sHelper->GetBoolean(1403))
            return;
        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 2)
            return;

        auto parsedItemId = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedItemId)
            return;
        uint32 ItemId = *parsedItemId;
        auto parsedBuyCount = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedBuyCount)
            return;
        uint32 BuyCount = *parsedBuyCount;
        if (!BuyCount || BuyCount > 255)
            return;
        uint32 NTempId = sItemShopMgr->GetNTempId(ItemId);
        if (!sItemShopMgr->IsBuyItem(player, ItemId))
            return;
        if (!sItemFuncMgr->CheckBuyItemLimit(player, ItemId, BuyCount))
            return;
        if (!sHelper->NotDeleteCheckNeed(player, NTempId, BuyCount))
            return;
        if (player->AddItem(ItemId, BuyCount, "在线商城购买获得"))
            sHelper->DeleteCheckNeed(player, NTempId, "在线商城消耗", BuyCount);
    }
    else if (IsPacket(RHMsg, "SearchShop"))
    {
        sItemShopMgr->SendDataForSearch(player, RMsg);
    }
    else if (IsPacket(RHMsg, "ChoiceItemSrcIn"))
    {
        if (!sHelper->GetBoolean(1404))
            return;
        auto parsedItemGuid = Acore::StringTo<uint32>(RevMsg);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        if (Item* SrcItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid)))
        {
            uint32 ItemId = SrcItem->GetEntry();
            auto Itr = ItemUpChoiceTempStore.find(ItemId);
            if (Itr != ItemUpChoiceTempStore.end())
            {
                uint32 XyCount = player->GetItemCount(Itr->second.XyItem);
                XyCount = XyCount > Itr->second.XyCount ? Itr->second.XyCount : XyCount;
                std::ostringstream Ass;
                Ass << ItemId << "#";
                Ass << Itr->second.OnSave << "#";
                Ass << Itr->second.Flag << "#";

                if (Itr->second.NewId1 > 0)
                    Ass << sHelper->GetItemLink(Itr->second.NewId1) << "#";
                else
                    Ass << "Not#";

                Ass << sHelper->ToExString(Itr->second.Chance1) << "#";

                if (Itr->second.NewId2 > 0)
                    Ass << sHelper->GetItemLink(Itr->second.NewId2) << "#";
                else
                    Ass << "Not#";

                Ass << sHelper->ToExString(Itr->second.Chance2) << "#";

                if (Itr->second.NewId3 > 0)
                    Ass << sHelper->GetItemLink(Itr->second.NewId3) << "#";
                else
                    Ass << "Not#";

                Ass << sHelper->ToExString(Itr->second.Chance3) << "#";

                Ass << sHelper->ToExString(Itr->second.XyOdds * XyCount) << "#";

                auto It = NeedTempStore.find(Itr->second.TempId);
                if (It != NeedTempStore.end())
                {
                    std::string SendMsg;
                    SendMsg.clear();

                    for (size_t i = 0; i < 5; i++)
                        SendMsg.append(sHelper->GetItemLink(It->second.ItemId[i]) + "$" + std::to_string(It->second.Count[i]) + "$");

                    Ass << SendMsg << "#";
                }

                SendPacketToClient(player, "ChoiceItemInfo", Ass.str());
            }
            else
            {
                SendPacketToClient(player, "NotUpChoice");
            }
        }
    }
    else if (IsPacket(RHMsg, "ChoiceItemStartUp"))
    {
        if (!sHelper->GetBoolean(1404))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 2)
            return;

        auto parsedItemGuid = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        auto parsedSender = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedSender)
            return;
        uint32 Sender = *parsedSender;
        if (Item* SrcItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid)))
        {
            if (SrcItem->IsBag() && SrcItem->ToBag() && !SrcItem->ToBag()->IsEmpty())
            {
                player->SendSysMsg(sHelper->GetGts(6669));
                return;
            }

            if (SrcItem->NowCount)
            {
                player->SendAsMsg(sHelper->GetGts(5801));
                return;
            }

            uint32 ItemId = SrcItem->GetEntry();

            auto Itr = ItemUpChoiceTempStore.find(ItemId);
            if (Itr != ItemUpChoiceTempStore.end())
            {
                uint32 NewItemId = 0;
                float NowChance = 0.0f;
                if (Sender == 1)
                {
                    NewItemId = Itr->second.NewId1;
                    NowChance = Itr->second.Chance1;
                }
                else if (Sender == 2)
                {
                    NewItemId = Itr->second.NewId2;
                    NowChance = Itr->second.Chance2;
                }
                else if (Sender == 3)
                {
                    NewItemId = Itr->second.NewId3;
                    NowChance = Itr->second.Chance3;
                }

                if (NewItemId)
                {
                    if (!sHelper->IsStoreNewBag(player, NewItemId))
                        return;

                    if (sHelper->CheckNeed(player, Itr->second.TempId, "选择升级功能消耗"))
                    {
                        uint32 XyCount = player->GetItemCount(Itr->second.XyItem);
                        XyCount = XyCount > Itr->second.XyCount ? Itr->second.XyCount : XyCount;

                        if (roll_chance_f(NowChance + Itr->second.XyOdds * XyCount))
                        {
                            if (Itr->second.XyItem > 0)
                                player->DestroyItemCount(Itr->second.XyItem, XyCount, true);
                            if (Itr->second.FBItem > 0)
                                player->DestroyItemCount(Itr->second.FBItem, 1, true);

                            if (Itr->second.OnSave > 0)
                                player->AddItemSaveRandomSock(SrcItem, NewItemId);
                            else
                                player->AddItem(NewItemId, 1, "选择升级功能获得");

                            player->DestroyItem(SrcItem->GetBagSlot(), SrcItem->GetSlot(), true);
                            player->SendAsMsg(sHelper->GetGts(1061), sHelper->GetItemLink(NewItemId).c_str());
                        }
                        else
                        {
                            if (Itr->second.FBItem > 0)
                            {
                                if (!player->HasItemCount(Itr->second.FBItem))
                                    player->DestroyItem(SrcItem->GetBagSlot(), SrcItem->GetSlot(), true);

                                player->DestroyItemCount(Itr->second.FBItem, 1, true);
                            }

                            player->SendAsMsg(sHelper->GetGts(1062));
                        }
                    }
                }
            }
        }
    }
    else if (IsPacket(RHMsg, "RandomItemSrcIn"))
    {
        if (!sHelper->GetBoolean(1405))
            return;
        auto parsedItemGuid = Acore::StringTo<uint32>(RevMsg);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        if (Item* SrcItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid)))
        {
            uint32 ItemId = SrcItem->GetEntry();
            auto Itr = ItemUpRandomStore.find(ItemId);
            if (Itr != ItemUpRandomStore.end())
            {
                uint32 XyCount = player->GetItemCount(Itr->second.XyItem);
                XyCount = XyCount > Itr->second.XyCount ? Itr->second.XyCount : XyCount;
                std::ostringstream Ass;
                Ass << ItemId << "#";
                Ass << Itr->second.OnSave << "#";

                if (Itr->second.NewId1 > 0)
                    Ass << sHelper->GetItemLink(Itr->second.NewId1) << "#";
                else
                    Ass << "Not#";

                Ass << sHelper->ToExString(Itr->second.Chance1) << "#";

                if (Itr->second.NewId2 > 0)
                    Ass << sHelper->GetItemLink(Itr->second.NewId2) << "#";
                else
                    Ass << "Not#";

                Ass << sHelper->ToExString(Itr->second.Chance2) << "#";

                if (Itr->second.NewId3 > 0)
                    Ass << sHelper->GetItemLink(Itr->second.NewId3) << "#";
                else
                    Ass << "Not#";

                Ass << sHelper->ToExString(Itr->second.Chance3) << "#";

                Ass << sHelper->ToExString(Itr->second.XyOdds * XyCount) << "#";

                auto It = NeedTempStore.find(Itr->second.TempId);
                if (It != NeedTempStore.end())
                {
                    std::string SendMsg;
                    SendMsg.clear();

                    for (size_t i = 0; i < 5; i++)
                        SendMsg.append(sHelper->GetItemLink(It->second.ItemId[i]) + "$" + std::to_string(It->second.Count[i]) + "$");

                    Ass << SendMsg << "#";
                }

                SendPacketToClient(player, "RandomItemInfo", Ass.str());
            }
            else
            {
                SendPacketToClient(player, "NotUpRandom");
            }
        }
    }
    else if (IsPacket(RHMsg, "RandomItemStartUp"))
    {
        if (!sHelper->GetBoolean(1405))
            return;
        auto parsedItemGuid = Acore::StringTo<uint32>(RevMsg);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        if (Item* SrcItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid)))
        {
            if (SrcItem->IsBag() && SrcItem->ToBag() && !SrcItem->ToBag()->IsEmpty())
            {
                player->SendSysMsg(sHelper->GetGts(6669));
                return;
            }

            if (SrcItem->NowCount)
            {
                player->SendAsMsg(sHelper->GetGts(5801));
                return;
            }

            uint32 ItemId = SrcItem->GetEntry();

            auto Itr = ItemUpRandomStore.find(ItemId);
            if (Itr != ItemUpRandomStore.end())
            {
                uint32 XyCount = player->GetItemCount(Itr->second.XyItem);
                XyCount = XyCount > Itr->second.XyCount ? Itr->second.XyCount : XyCount;

                float Chance1 = Itr->second.Chance1 + float(Itr->second.XyOdds * XyCount);
                float Chance2 = Itr->second.Chance2 + float(Itr->second.XyOdds * XyCount);
                float Chance3 = Itr->second.Chance3 + float(Itr->second.XyOdds * XyCount);
                std::vector<float> Rs = { Chance1, Chance2, Chance3 };
                float NowChance = 0.0f;
                float RIdx = 0.0f;
                for (auto Itr = Rs.begin(); Itr != Rs.end(); Itr++)
                    RIdx += *Itr;

                for (auto It = Rs.begin(); It != Rs.end(); It++)
                {
                    if (frand(0, RIdx) <= *It)
                    {
                        NowChance = *It;
                        break;
                    }
                    else
                        RIdx -= *It;
                }

                uint32 NewItemId = 0;
                if (NowChance == Chance1)
                    NewItemId = Itr->second.NewId1;
                else if (NowChance == Chance2)
                    NewItemId = Itr->second.NewId2;
                else if (NowChance == Chance3)
                    NewItemId = Itr->second.NewId3;

                if (NewItemId)
                {
                    if (!sHelper->IsStoreNewBag(player, NewItemId))
                        return;

                    if (sHelper->CheckNeed(player, Itr->second.TempId, "随机升级功能消耗"))
                    {
                        if (Itr->second.XyItem > 0)
                            player->DestroyItemCount(Itr->second.XyItem, XyCount, true);

                        if (Itr->second.OnSave > 0)
                            player->AddItemSaveRandomSock(SrcItem, NewItemId);
                        else
                            player->AddItem(NewItemId, 1, "随机升级功能获得");

                        player->DestroyItem(SrcItem->GetBagSlot(), SrcItem->GetSlot(), true);
                        player->SendAsMsg(sHelper->GetGts(1061), sHelper->GetItemLink(NewItemId).c_str());
                    }
                }
            }
        }
    }
    else if (IsPacket(RHMsg, "AcceptBuyInfoItem"))
    {
        uint32 BItem = player->BuyItemId;
        uint32 BCount = player->BuyCount;
        BCount = BCount > 255 ? 255 : BCount;
        uint32 VEntry = player->GetSession()->GetVEntry();
        uint32 NTempId = sItemFuncMgr->GetNTempId(BItem, VEntry);

        if (!NTempId)
            NTempId = sCustomVendorMgr->GetCustomVendorNTempId(VEntry, BItem);

        ItemTemplate const* pProto = sObjectMgr->GetItemTemplate(BItem);
        if (!pProto)
            return;

        if (pProto->MaxCount > 0)
        {
            if (player->HasItemCount(pProto->ItemId, pProto->MaxCount, true))
            {
                player->GetSession()->SendNotification(sHelper->GetGts(1080));
                return;
            }
        }

        // Check custom vendor stock
        if (sCustomVendorMgr->IsCustomVendorItem(VEntry, BItem))
        {
            CustomVendorItem const* cvItem = sCustomVendorMgr->GetCustomVendorItem(VEntry, BItem);
            if (cvItem && cvItem->maxcount > 0)
            {
                uint32 currentCount = sCustomVendorMgr->GetCustomVendorCurrentCount(VEntry, BItem, cvItem->maxcount, cvItem->incrtime);
                if (currentCount < pProto->BuyCount * BCount)
                {
                    player->SendSysMsg("该物品库存不足，剩余数量：%u", currentCount);
                    return;
                }
            }
        }

        // Check NTempId requirements first
        if (!sHelper->NotDeleteCheckNeed(player, NTempId, BCount))
            return;

        // All checks passed, give item
        if (player->AddItem(BItem, BCount, "商人商店购买获得"))
        {
            // Deduct NTempId costs
            sHelper->DeleteCheckNeed(player, NTempId, "商人商店购买消耗", BCount);

            // Deduct custom vendor stock
            if (sCustomVendorMgr->IsCustomVendorItem(VEntry, BItem))
            {
                CustomVendorItem const* cvItem = sCustomVendorMgr->GetCustomVendorItem(VEntry, BItem);
                if (cvItem && cvItem->maxcount > 0)
                {
                    sCustomVendorMgr->UpdateCustomVendorCurrentCount(VEntry, BItem, cvItem->maxcount, cvItem->incrtime, pProto->BuyCount * BCount);
                    // 刷新商人列表，让客户端实时显示新库存
                    Creature* vendorCreature = player->GetCustomNpcWith(player->GetTarget());
                    if (!vendorCreature)
                        vendorCreature = player->GetNPCIfCanInteractWith(player->GetTarget(), UNIT_NPC_FLAG_VENDOR);
                    if (vendorCreature)
                        player->GetSession()->SendCustomListInventory(vendorCreature->GetGUID(), VEntry);
                }
            }
        }
    }
    else if (IsPacket(RHMsg, "GetMeStatPointData"))
    {
        if (!sHelper->GetBoolean(1406) && !sHelper->GetBoolean(1416))
            return;
        sCustomClass->SendUpdatePlayerStatData(player);
    }
    else if (IsPacket(RHMsg, "GetStatPointConfigData"))
    {
        if (!sHelper->GetBoolean(1406) && !sHelper->GetBoolean(1416))
            return;
        sCustomClass->SendConfigStatData(player);
    }
    else if (IsPacket(RHMsg, "StatPointDoReset"))
    {
        if (!sHelper->GetBoolean(1406) && !sHelper->GetBoolean(1416))
            return;
        if (sCustomClass->IsCheckReset(player))
        {
            uint32 NTempId = sCustomClass->GetStatConfigCount(2025);
            if (!NTempId)
            {
                player->TempAtStore[AT13] = sCustomClass->GetStatAllsCount(AsInt64ToUInt32Clamp(player->TempAtStore[AT11].Value));
                for (size_t i = AT21; i < ATMAX; i++)
                {
                    uint32 Try = sCustomClass->AotoTempAt(i, false);
                    sCustomClass->ApplyPlayerStat(player, Try, player->TempAtStore[i].Value, false);
                    player->TempAtStore[i] = 0;
                }
                sCustomClass->SendUpdatePlayerStatData(player);
                player->SendSysMsg(sHelper->GetGts(1903));
            }
            else
            {
                SendPacketToClient(player, "ResetStatPtInfo", sHelper->GetNeedGts(NTempId));
            }
        }
        else
        {
            player->SendSysMsg(sHelper->GetGts(1904));
        }
    }
    else if (IsPacket(RHMsg, "AcceptResetStatPt"))
    {
        if (!sHelper->GetBoolean(1406) && !sHelper->GetBoolean(1416))
            return;
        uint32 NTempId = sCustomClass->GetStatConfigCount(2025);
        if (sHelper->CheckNeed(player, NTempId, "重置斗气消耗"))
        {
            player->TempAtStore[AT13] = sCustomClass->GetStatAllsCount(AsInt64ToUInt32Clamp(player->TempAtStore[AT11].Value));
            for (size_t i = AT21; i < ATMAX; i++)
            {
                uint32 Try = sCustomClass->AotoTempAt(i, false);
                sCustomClass->ApplyPlayerStat(player, Try, player->TempAtStore[i].Value, false);
                player->TempAtStore[i] = 0;
            }
            sCustomClass->SendUpdatePlayerStatData(player);
            player->SendSysMsg(sHelper->GetGts(1903));
        }
    }
    else if (IsPacket(RHMsg, "AcceptStatIncrease"))
    {
        if (!sHelper->GetBoolean(1406) && !sHelper->GetBoolean(1416))
            return;
        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 2)
            return;

        auto parsedTry = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedTry)
            return;
        uint32 Try = *parsedTry;
        AsInt64 Count;
        if (!TryParseAsInt256(Datas[1], Count))
            return;
        if (Count == 0)
            return;

        if (!sHelper->GetBoolean(13601) && Count < 0)
        {
            player->SendSysMsg(sHelper->GetGts(7020));
            return;
        }

        uint32 At = sCustomClass->AotoTempAt(Try, true);
        if (!At)
            return;
        if (Count < 0)
        {
            AsInt64 refundCount = -Count;
            if (player->TempAtStore[At].Value <= 0 || player->TempAtStore[At].Value < refundCount)
            {
                player->SendSysMsg(sHelper->GetGts(1901));
                return;
            }
        }
        else
        {
            AsInt64 spendCount = Count;
            if (player->TempAtStore[AT13].Value <= 0 || player->TempAtStore[AT13].Value < spendCount)
            {
                player->SendSysMsg(sHelper->GetGts(1902));
                return;
            }

            AsInt64 MaxTry = sCustomClass->GetStatConfigMax(Try);
            if (MaxTry != 0 && boost::multiprecision::int512_t(player->TempAtStore[At].Value) + spendCount > MaxTry)
            {
                player->SendSysMsg("[温馨提示]: 已达到斗气加点上限 %s", AsInt64ToString(MaxTry).c_str());
                return;
            }
        }

        boost::multiprecision::int512_t nextAttribute = boost::multiprecision::int512_t(player->TempAtStore[At].Value) + Count;
        boost::multiprecision::int512_t nextPoints = boost::multiprecision::int512_t(player->TempAtStore[AT13].Value) - Count;
        if (nextAttribute < 0 || nextPoints < 0 || nextAttribute > std::numeric_limits<AsInt64>::max() ||
            nextPoints > std::numeric_limits<AsInt64>::max())
            return;
        sCustomClass->ApplyPlayerStat(player, Try, Count, true);
        player->TempAtStore[At] = static_cast<AsInt64>(nextAttribute);
        player->TempAtStore[AT13] = static_cast<AsInt64>(nextPoints);

        sCustomClass->SendUpdatePlayerStatData(player);
        // Refresh the 256-bit character panel immediately after stat-point changes.
        SendPacketToClient(player, "PATTRPANEL", "REFRESH");
    }
    else if (IsPacket(RHMsg, "GetNewLuckData"))
    {
        if (!sHelper->GetBoolean(1401))
            return;
        sItemFuncMgr->DoNewLuckDataTo(player);
    }
    else if (IsPacket(RHMsg, "NewLuckCheckNeed"))
    {
        if (!sHelper->GetBoolean(1401))
            return;

        if (player->InLucking)
        {
            player->SendAsMsg(sHelper->GetGts(2301));
            return;
        }

        auto [ItemId, Count] = sItemFuncMgr->PullItemAndCount();
        std::string ItemLink = sHelper->GetItemLink(ItemId);

        auto parsedType = Acore::StringTo<uint8>(RevMsg);
        if (!parsedType)
            return;
        uint8 Type = *parsedType;
        if (!Type)
            return;

        Type = Type > 10 ? 10 : Type;

        if (!player->HasItemCount(ItemId, Type * Count))
        {
            player->SendAsMsg(sHelper->GetGts(1011), ItemLink.c_str(), Type * Count);
            player->InLucking = false;
            player->IsTen = false;
            player->DoType = 0;
            return;
        }

        player->InLucking = true;
        player->IsTen = (Type == 10);
        player->DestroyItemCount(ItemId, Type * Count, true);
        SendPacketToClient(player, "NewLuckCheckNeedNow");
        player->DoType = Type;
    }
    else if (IsPacket(RHMsg, "NewLuckPlayBegin"))
    {
        if (!sHelper->GetBoolean(1401))
            return;

        if (!player->InLucking && !player->DoType)
        {
            player->DoType = 0;
            player->IsTen = false;
            player->InLucking = false;
            return;
        }

        float RIdx = 0.0f;
        for (size_t i = 0; i < LuckTempStore.size(); i++)
            RIdx += LuckTempStore[i].Odds;

        for (size_t i = 0; i < LuckTempStore.size(); i++)
        {
            if (frand(0, RIdx) <= LuckTempStore[i].Odds)
            {
                player->DoRandomIndex = i;
                player->LuckItemId = LuckTempStore[i].ItemId;
                player->LuckNowCount = LuckTempStore[i].Count;
                break;
            }
            else
                RIdx -= LuckTempStore[i].Odds;
        }

        player->InLucking = true;
        SendPacketToClient(player, "NewStartLuckNow", std::to_string(player->DoRandomIndex) + "#" + std::to_string(player->DoType) + "#");
    }
    else if (IsPacket(RHMsg, "CheckLuckIndex"))
    {
        if (!sHelper->GetBoolean(1401))
            return;

        if (!player->InLucking && !player->DoType)
            return;

        auto parsedIndex = Acore::StringTo<uint32>(RevMsg);
        if (!parsedIndex)
            return;
        uint32 Index = *parsedIndex;
        if (Index == player->DoRandomIndex)
        {
            player->AddItem(player->LuckItemId, player->LuckNowCount, "抽奖获得");
            player->InLucking = false;
            player->DoType--;

            if (!player->DoType && player->IsTen)
            {
                auto [TenItemId, TenCount] = sItemFuncMgr->PullTenItemAndCount();
                if (TenItemId && TenCount)
                    player->AddItem(TenItemId, TenCount, "十连抽额外获得");

                player->IsTen = false;
            }

            if (sHelper->GetBoolean(1800) && sItemFuncMgr->IsLuckAnn(player->LuckItemId))
            {
                //滚动消息内容
                std::string ScrollMsg = "|cFFFFFF00玩家|r "
                    + sHelper->GetNameLinkColor(player) + " |cFFFFFF00抽到|r "
                    + sHelper->GetItemLink(player->LuckItemId) + " |cFFFFFF00X|r |cFFFFFF00"
                    + std::to_string(player->LuckNowCount) + "|r";

                //遍历所有玩家 更新界面
                WorldSessionMgr::SessionMap const& Sessions = sWorldSessionMgr->GetAllSessions();
                for (WorldSessionMgr::SessionMap::const_iterator Itr = Sessions.begin(); Itr != Sessions.end(); ++Itr)
                {
                    if (!Itr->second || !Itr->second->GetPlayer() || !Itr->second->GetPlayer()->IsInWorld())
                        continue;

                    Itr->second->GetPlayer()->SendAsMsg(sHelper->GetGts(1039), sHelper->GetNameLinkColor(player).c_str(), sHelper->GetItemLink(player->LuckItemId).c_str(), player->LuckNowCount);

                    //界面更新消息
                    SendPacketToClient(Itr->second->GetPlayer(), "NewLuckUpdateScrollMsg", ScrollMsg);
                }
            }

            SendPacketToClient(player, "NewLuckCheckNeedNow");
        }
    }
    else if (IsPacket(RHMsg, "GetMeEqSlotData"))
    {
        if (!sHelper->GetBoolean(1408))
            return;
        sPlayerModMgr->SendApplyMeEqSlotData(player);
    }
    else if (IsPacket(RHMsg, "EqPlayerInfoSlot"))
    {
        if (!sHelper->GetBoolean(1408))
            return;

        auto parsedSlot = Acore::StringTo<uint32>(RevMsg);
        if (!parsedSlot)
            return;
        uint32 Slot = *parsedSlot;
        if (Slot < 1 || Slot > EQUIPMENT_SLOT_END)
            return;

        uint32 OSlot = sPlayerModMgr->AutoEqSlot(Slot, false);

        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, OSlot))
        {
            if (pItem->IsEquipped())
            {
                player->SendSysMsg(sHelper->GetGts(3202));
                return;
            }
        }

        uint32 SlotRank = player->TempAtStore[OSlot + AtCBeg];
        uint32 MaxRank = sPlayerModMgr->GetMaxRank(Slot);
        if (SlotRank >= MaxRank)
        {
            player->SendSysMsg(sHelper->GetGts(3201));
            return;
        }

        uint32 NTempId = sPlayerModMgr->GetEqSlotNTempId(SlotRank, Slot);
        if (NTempId)
        {
            std::string GtNeed = sHelper->GetNeedGts(NTempId);
            SendPacketToClient(player, "NowEqNeedInfo", RMsg + "#" + GtNeed + "#");
        }
        else
        {
            if (Slot < 1 || Slot > EQUIPMENT_SLOT_END)
            return;

        uint32 OSlot = sPlayerModMgr->AutoEqSlot(Slot, false);
            uint32 SlotRank = player->TempAtStore[OSlot + AtCBeg];
            uint32 MaxRank = sPlayerModMgr->GetMaxRank(Slot);
            if (SlotRank >= MaxRank)
            {
                player->SendSysMsg(sHelper->GetGts(3201));
                return;
            }

            uint32 NTempId = sPlayerModMgr->GetEqSlotNTempId(SlotRank, Slot);
            if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, OSlot))
            {
                if (pItem->IsEquipped())
                {
                    player->SendSysMsg(sHelper->GetGts(3202));
                    return;
                }
            }

            uint32 NowAt = player->TempAtStore[OSlot + AtCBeg] + 1;
            player->UpdateTempAt(PlayerTemp(OSlot + AtCBeg), NowAt);
            player->SendSysMsg(sHelper->GetGts(3203));
            sPlayerModMgr->SendApplyMeEqSlotData(player);
            sPlayerModMgr->CheckEqRankGiveAura(player, NowAt);
        }
    }
    else if (IsPacket(RHMsg, "AcceptEqNeedInfo"))
    {
        if (!sHelper->GetBoolean(1408))
            return;
        auto parsedSlot = Acore::StringTo<uint32>(RevMsg);
        if (!parsedSlot)
            return;
        uint32 Slot = *parsedSlot;

        if (Slot < 1 || Slot > EQUIPMENT_SLOT_END)
            return;

        uint32 OSlot = sPlayerModMgr->AutoEqSlot(Slot, false);
        uint32 SlotRank = player->TempAtStore[OSlot + AtCBeg];
        uint32 MaxRank = sPlayerModMgr->GetMaxRank(Slot);
        if (SlotRank >= MaxRank)
        {
            player->SendSysMsg(sHelper->GetGts(3201));
            return;
        }

        uint32 NTempId = sPlayerModMgr->GetEqSlotNTempId(SlotRank, Slot);
        if (Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, OSlot))
        {
            if (pItem->IsEquipped())
            {
                player->SendSysMsg(sHelper->GetGts(3202));
                return;
            }
        }

        if (sHelper->CheckNeed(player, NTempId, "装备栏强化消耗"))
        {
            uint32 NowAt = player->TempAtStore[OSlot + AtCBeg] + 1;
            player->UpdateTempAt(PlayerTemp(OSlot + AtCBeg), NowAt);
            player->SendSysMsg(sHelper->GetGts(3203));
            sPlayerModMgr->SendApplyMeEqSlotData(player);
            sPlayerModMgr->CheckEqRankGiveAura(player, NowAt);
        }
    }
    else if (IsPacket(RHMsg, "GetPutNtRankFrame"))
    {
        if (!sHelper->GetBoolean(1409))
            return;

        typedef std::pair<std::string, uint32> TempPairs;
        std::vector<TempPairs> AtStore;
        for (auto Itr = AsPlayerNtRankStore.begin(); Itr != AsPlayerNtRankStore.end(); Itr++)
        {
            if (Itr->second < 2)
                continue;

            if (CharacterCacheEntry const* CacheEntry = sCharacterCache->GetCharacterCacheByGuid(Itr->first))
            {
                std::string PInfo = sHelper->GetPlayerChatInfo(CacheEntry->Name, CacheEntry->Class, 18);
                uint32 Rank = (Itr->second - 1);
                AtStore.push_back(std::make_pair(PInfo, Rank));
            }
        }

        if (AtStore.empty())
        {
            //player->SendSysMsg("服务端目前还没有天梯排名数据");
            return;
        }

        std::string SendMsg;
        std::sort(AtStore.begin(), AtStore.end(), [](const TempPairs& V1, const TempPairs& V2) {return V1.second > V2.second; });
        for (size_t N = 0; N < AtStore.size(); N++)
        {
            if (N < 10)
            {
                SendMsg.append(AtStore[N].first + "#" + std::to_string(AtStore[N].second) + "#" + sHelper->GetItemLink(sAsCreature->GetNtRItemId(AtStore[N].second)) + "#$");
            }
        }

        SendPacketToClient(player, "AutoPutNtRankFrame", SendMsg);
    }
    else if (IsPacket(RHMsg, "GetMeItemCards"))
    {
        if (!sHelper->GetBoolean(1410))
            return;

        std::string SendMsg;
        SendMsg.clear();
        for (size_t i = AT3001; i < ATKPMAX; i++)
            SendMsg.append(AsInt64ToString(player->TempAtStore[i].Value) + "#");

        SendPacketToClient(player, "AutoMeItemCards", SendMsg);
        SendMsg.clear();

        sItemFuncMgr->SendUpdateCardsAuraSpellData(player);
    }
    else if (IsPacket(RHMsg, "GetCardsNeedInfo"))
    {
        if (!sHelper->GetBoolean(1410))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 2)
            return;

        auto parsedItemId = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedItemId)
            return;
        uint32 ItemId = *parsedItemId;
        auto parsedNum = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedNum)
            return;
        uint32 Num = *parsedNum;

        if (!player->HasItemCount(ItemId))
            return;

        if (Num > 8)
            return;

        auto Itr = ItemCardsStore.find(ItemId);
        if (Itr == ItemCardsStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(3401));
            SendPacketToClient(player, "NotCardsForItemId", std::to_string(Num));
            return;
        }

        if (Itr->second.NTempId)
        {
            std::string GtNeed = sHelper->GetNeedGts(Itr->second.NTempId);
            SendPacketToClient(player, "NowCardsNeedInfo", std::to_string(ItemId) + "#" + GtNeed + "#" + std::to_string(Num) + "#");
        }
        else
        {
            if (!sItemFuncMgr->CanDoInItemCards(player, ItemId))
            {
                player->SendSysMsg(sHelper->GetGts(3404));
                return;
            }

            uint32 Index = ATCMAX + Num;
            uint32 OldItem = player->GetTempAtUInt32(PlayerTemp(Index));
            if (OldItem)
            {
                Item* ItemOld = player->AddItemByIdOne(OldItem, "卡牌功能返回获得");
                if (ItemOld && Itr->second.Bind)
                {
                    ItemOld->UnBinded = false;
                    ItemOld->SetBinding(true);
                    ItemOld->UpdateTempKt(KT03, 1);
                }
            }

            player->UpdateTempAt(PlayerTemp(Index), ItemId);
            player->DestroyItemCount(ItemId, 1, true);
            SendPacketToClient(player, "YesCardsForItemId", std::to_string(ItemId) + "#" + std::to_string(Num) + "#");
            player->SendSysMsg(sHelper->GetGts(3402));

            sItemFuncMgr->OnLoginCastCardsAuraSpell(player);
            sItemFuncMgr->SendUpdateCardsAuraSpellData(player);
        }
    }
    else if (IsPacket(RHMsg, "AcceptItemCards"))
    {
        if (!sHelper->GetBoolean(1410))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 2)
            return;

        auto parsedItemId = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedItemId)
            return;
        uint32 ItemId = *parsedItemId;
        auto parsedNum = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedNum)
            return;
        uint32 Num = *parsedNum;

        if (!player->HasItemCount(ItemId))
            return;

        if (Num > 8)
            return;

        auto Itr = ItemCardsStore.find(ItemId);
        if (Itr == ItemCardsStore.end())
        {
            player->SendSysMsg(sHelper->GetGts(3401));
            SendPacketToClient(player, "NotCardsForItemId", std::to_string(Num));
            return;
        }

        if (!sItemFuncMgr->CanDoInItemCards(player, ItemId))
        {
            player->SendSysMsg(sHelper->GetGts(3404));
            return;
        }

        if (sHelper->CheckNeed(player, Itr->second.NTempId, "卡牌功能消耗"))
        {
            uint32 Index = ATCMAX + Num;
            uint32 OldItem = player->GetTempAtUInt32(PlayerTemp(Index));
            if (OldItem)
            {
                Item* ItemOld = player->AddItemByIdOne(OldItem, "卡牌功能返回获得");
                if (ItemOld && Itr->second.Bind)
                {
                    ItemOld->UnBinded = false;
                    ItemOld->SetBinding(true);
                    ItemOld->UpdateTempKt(KT03, 1);
                }
            }

            player->UpdateTempAt(PlayerTemp(Index), ItemId);
            player->DestroyItemCount(ItemId, 1, true);
            SendPacketToClient(player, "YesCardsForItemId", std::to_string(ItemId) + "#" + std::to_string(Num) + "#");
            player->SendSysMsg(sHelper->GetGts(3402));

            sItemFuncMgr->OnLoginCastCardsAuraSpell(player);
            sItemFuncMgr->SendUpdateCardsAuraSpellData(player);
        }
    }
    else if (IsPacket(RHMsg, "TaskItemCards"))
    {
        if (!sHelper->GetBoolean(1410))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 2)
            return;

        auto parsedItemId = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedItemId)
            return;
        uint32 ItemId = *parsedItemId;
        auto parsedNum = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedNum)
            return;
        uint32 Num = *parsedNum;

        auto Itr = ItemCardsStore.find(ItemId);
        if (Itr == ItemCardsStore.end())
            return;

        if (Num > 8)
            return;

        uint32 Index = ATCMAX + Num;
        uint32 OldItem = player->GetTempAtUInt32(PlayerTemp(Index));
        if (!OldItem)
            return;

        if (OldItem == ItemId)
        {
            Item* ItemOld = player->AddItemByIdOne(OldItem, "卡牌功能返回获得");
            if (ItemOld && Itr->second.Bind)
            {
                ItemOld->UnBinded = false;
                ItemOld->SetBinding(true);
                ItemOld->UpdateTempKt(KT03, 1);
            }
        }

        player->UpdateTempAt(PlayerTemp(Index), 0);

        sItemFuncMgr->OnLoginCastCardsAuraSpell(player);
        sItemFuncMgr->SendUpdateCardsAuraSpellData(player);
        player->SendSysMsg(sHelper->GetGts(3403));
    }
    else if (IsPacket(RHMsg, "NowShowDmgUI"))
    {
        player->ShowDmgUI = !player->ShowDmgUI;

        if (!sHelper->GetBoolean(3102))
            player->ShowDmgUI = true;

        player->SendSysMsg(player->ShowDmgUI ? sHelper->GetGts(6670) : sHelper->GetGts(6671));

        SendPacketToClient(player, "NowShowDmgUI", player->ShowDmgUI ? "1" : "0");
    }
    else if (IsPacket(RHMsg, "DressTransItemInfo"))
    {
        if (!sHelper->GetBoolean(6000))
        {
            player->SendSysMsg(sHelper->GetGts(6672));
            return;
        }

        auto parsedItemId = Acore::StringTo<uint32>(RevMsg);
        if (!parsedItemId)
            return;
        uint32 ItemId = *parsedItemId;
        if (sHelper->GetUInt(6001))
        {
            std::string GtNeed = sHelper->GetNeedGts(sHelper->GetUInt(6001));
            SendPacketToClient(player, "NowDressTransItemNeedInfo", std::to_string(ItemId) + "#" + GtNeed + "#");
        }
        else
        {
            ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(ItemId);
            if (!Proto)
            {
                player->SendSysMsg(sHelper->GetGts(6673));
                return;
            }

            uint8 Slot = sHelper->GetDressItemSlot(Proto);
            if (Slot == EQUIPMENT_SLOT_END)
            {
                player->SendSysMsg(sHelper->GetGts(6674));
                return;
            }

            if (!sTransmogrification->IDCustomTransmogrify(player, Proto, Slot, sHelper->GetUInt(6001)))
                return;
        }
    }
    else if (IsPacket(RHMsg, "AcceptDressItemOne"))
    {
        if (!sHelper->GetBoolean(6000))
        {
            player->SendSysMsg(sHelper->GetGts(6672));
            return;
        }

        auto parsedItemId = Acore::StringTo<uint32>(RevMsg);
        if (!parsedItemId)
            return;
        uint32 ItemId = *parsedItemId;
        ItemTemplate const* Proto = sObjectMgr->GetItemTemplate(ItemId);
        if (!Proto)
        {
            player->SendSysMsg(sHelper->GetGts(6673));
            return;
        }

        uint8 Slot = sHelper->GetDressItemSlot(Proto);
        if (Slot == EQUIPMENT_SLOT_END)
        {
            player->SendSysMsg(sHelper->GetGts(6674));
            return;
        }

        if (!sTransmogrification->IDCustomTransmogrify(player, Proto, Slot, sHelper->GetUInt(6001)))
            return;
    }
    else if (IsPacket(RHMsg, "GetEventSizeNow"))
    {
        if (!sHelper->GetBoolean(1411))
            return;

        std::vector<ActivityNoticeRow> Rows;
        if (LoadActivityNoticeRows(Rows))
            SendPacketToClient(player, "AutoEventSizeNow", std::to_string(Rows.size()));
        else
            SendPacketToClient(player, "AutoEventSizeNow", "0");
    }
    else if (IsPacket(RHMsg, "GetEventDataNow"))
    {
        if (!sHelper->GetBoolean(1411))
            return;

        if (!SendActivityNotices(player))
            SendPacketToClient(player, "AutoEventSizeNow", "0");
    }
    else if (IsPacket(RHMsg, "ActivityNoticeClick"))
    {
        if (!sHelper->GetBoolean(1411))
            return;

        auto noticeId = Acore::StringTo<uint32>(RevMsg);
        if (!noticeId)
            return;
        HandleActivityNoticeClick(player, *noticeId);
    }
    else if (IsPacket(RHMsg, "PlayerGoEventNow"))
    {
        if (!sHelper->GetBoolean(1411))
            return;

        auto parsedEventId = Acore::StringTo<uint16>(RevMsg);
        if (!parsedEventId)
            return;
        uint16 EventId = *parsedEventId;
        if (EventId <= MINEVENTSO)
        {
            player->SendSysMsg(sHelper->GetGts(6675));
            return;
        }

        if (!sGameEventMgr->IsActiveEvent(EventId))
        {
            player->SendSysMsg(sHelper->GetGts(6676));
            return;
        }

        sEvent->StartPlayerInEvent(player, EventId);
    }
    else if (IsPacket(RHMsg, "GetPlayerActivityData"))
    {
        if (!sHelper->GetBoolean(1412))
            return;
        sActivityMgr->SendPlayerActivityData(player);
    }
    else if (IsPacket(RHMsg, "GetActivityStoreData"))
    {
        if (!sHelper->GetBoolean(1412))
            return;
        sActivityMgr->SendPacketActivityData(player);
    }
    else if (IsPacket(RHMsg, "NowRewPlayerActivity"))
    {
        if (!sHelper->GetBoolean(1412))
            return;
        auto parsedPopValue = Acore::StringTo<uint32>(RevMsg);
        if (!parsedPopValue)
            return;
        uint32 PopValue = *parsedPopValue;
        if (!PopValue)
            return;
        auto Itr = RewActivityStore.find(PopValue);
        if (Itr == RewActivityStore.end())
            return;
        if (!sActivityMgr->IsOnRewPopValue(player, PopValue))
        {
            player->SendSysMsg(sHelper->GetGts(6001));
            return;
        }
        sActivityMgr->SetOnRewPopValueAndUpdateDB(player, PopValue);
        sActivityMgr->UpdatePlayerOnePopActivity(player);
        sHelper->DoTheReward(player, Itr->second, "活跃度阶段奖励");
    }
    else if (IsPacket(RHMsg, "InItemMagicQuery"))
    {
        if (!sHelper->GetBoolean(1414))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 3)
            return;

        auto parsedItemGuid = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        auto parsedAtGroup = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedAtGroup)
            return;
        uint32 AtGroup = *parsedAtGroup;
        auto parsedNum = Acore::StringTo<uint32>(Datas[2]);
        if (!parsedNum)
            return;
        uint32 Num = *parsedNum;
        if (!Num)
            return;

        if (AtGroup < 1 || AtGroup == 6 || AtGroup > 7)
            return;

        if ((AtGroup == 1 || AtGroup == 2 || AtGroup == 3 || AtGroup == 4 || AtGroup == 5) && Num > 5)
            return;

        if (AtGroup == 7  && Num > 3)
            return;

        Item* OnItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid));
        if (!OnItem)
        {
            SendPacketToClient(player, "NoFuncItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num));
            player->SendSysMsg(sHelper->GetGts(6102));
            return;
        }

        MagicTypes OType1 = sItemMagicMgr->AutoForClientGroupId(AtGroup, Num);
        if (OType1 == M_NULL)
            return;

        MagicTypes OType2 = sItemMagicMgr->GetOType(OnItem->GetEntry());
        if (OType2 == M_NULL)
            return;

        if (OType1 != OType2)
        {
            player->SendSysMsg(sHelper->GetGts(6151), sItemMagicMgr->GetTStr(OnItem->GetEntry()).c_str());
            SendPacketToClient(player, "NoFuncItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num));
            return;
        }

        uint32 Max = sItemMagicMgr->GetMagicItemMax(OnItem->GetEntry());
        uint32 PMax = sItemMagicMgr->GetPlayerActCountForOType(player, OnItem->GetEntry());
        if (Max && PMax >= Max)
        {
            player->SendSysMsg(sHelper->GetGts(6152), Max);
            SendPacketToClient(player, "NoFuncItemMagic", std::to_string(AtGroup) + "#" + std::to_string(Num));
            return;
        }

        sItemMagicMgr->UpdateMagicOnPlayerHandle(player, OType1, AtGroup, OnItem->GetEntry(), Num, true);
    }
    else if (IsPacket(RHMsg, "TaskItemMagic"))
    {
        if (!sHelper->GetBoolean(1414))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 3)
            return;

        auto parsedAtGroup = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedAtGroup)
            return;
        uint32 AtGroup = *parsedAtGroup;
        auto parsedNum = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedNum)
            return;
        uint32 Num = *parsedNum;
        auto parsedItemId = Acore::StringTo<uint32>(Datas[2]);
        if (!parsedItemId)
            return;
        uint32 ItemId = *parsedItemId;

        if (!Num)
            return;

        if (AtGroup < 1 || AtGroup == 6 || AtGroup > 7)
            return;

        if ((AtGroup == 1 || AtGroup == 2 || AtGroup == 3 || AtGroup == 4 || AtGroup == 5) && Num > 5)
            return;

        if (AtGroup == 7 && Num > 3)
            return;

        MagicTypes OType1 = sItemMagicMgr->AutoForClientGroupId(AtGroup, Num);
        if (OType1 == M_NULL)
            return;

        MagicTypes OType2 = sItemMagicMgr->GetOType(ItemId);
        if (OType2 == M_NULL)
            return;

        sItemMagicMgr->UpdateMagicOnPlayerHandle(player, OType1, AtGroup, ItemId, Num, false);
    }
    else if (IsPacket(RHMsg, "InPlayerMagicQuery"))
    {
        if (!sHelper->GetBoolean(1414))
            return;

        if (ItemMagicStore.empty())
            return;

        for (uint8 i = 0; i < 5; i++)
        {
            uint32 ItemId = player->MagicGroup1[i];
            if (ItemId)
            {
                uint32 SpellId = sItemMagicMgr->GetMagicAuraId(ItemId);
                SendPacketToClient(player, "ItemMagicGroup1", std::to_string(i + 1) + "#" + std::to_string(SpellId) + "#" + std::to_string(ItemId));
            }
        }

        for (uint8 i = 0; i < 5; i++)
        {
            uint32 ItemId = player->MagicGroup2[i];
            if (ItemId)
            {
                uint32 SpellId = sItemMagicMgr->GetMagicAuraId(ItemId);
                SendPacketToClient(player, "ItemMagicGroup2", std::to_string(i + 1) + "#" + std::to_string(SpellId) + "#" + std::to_string(ItemId));
            }
        }

        for (uint8 i = 0; i < 5; i++)
        {
            uint32 ItemId = player->MagicGroup3[i];
            if (ItemId)
            {
                uint32 SpellId = sItemMagicMgr->GetMagicAuraId(ItemId);
                SendPacketToClient(player, "ItemMagicGroup3", std::to_string(i + 1) + "#" + std::to_string(SpellId) + "#" + std::to_string(ItemId));
            }
        }

        for (uint8 i = 0; i < 5; i++)
        {
            uint32 ItemId = player->MagicGroup4[i];
            if (ItemId)
            {
                uint32 SpellId = sItemMagicMgr->GetMagicAuraId(ItemId);
                SendPacketToClient(player, "ItemMagicGroup4", std::to_string(i + 1) + "#" + std::to_string(SpellId) + "#" + std::to_string(ItemId));
            }
        }

        for (uint8 i = 0; i < 5; i++)
        {
            uint32 ItemId = player->MagicGroup5[i];
            if (ItemId)
            {
                uint32 SpellId = sItemMagicMgr->GetMagicAuraId(ItemId);
                SendPacketToClient(player, "ItemMagicGroup5", std::to_string(i + 1) + "#" + std::to_string(SpellId) + "#" + std::to_string(ItemId));
            }
        }

        for (uint8 i = 0; i < 5; i++)
        {
            uint32 AuraId = player->MagicActivate[i];
            if (AuraId)
                SendPacketToClient(player, "ItemMagicGroup6", std::to_string(i + 1) + "#" + std::to_string(AuraId));
        }

        for (uint8 i = 0; i < 3; i++)
        {
            uint32 ItemId = player->MagicCentre[i];
            if (ItemId)
            {
                uint32 SpellId = sItemMagicMgr->GetMagicAuraId(ItemId);
                SendPacketToClient(player, "ItemMagicCentre", std::to_string(i + 1) + "#" + std::to_string(SpellId) + "#" + std::to_string(ItemId));
            }
        }
    }
    else if (IsPacket(RHMsg, "ItemEnchTaskCheck1"))
    {
        if (!sHelper->GetBoolean(1415))
            return;
        auto parsedItemGuid = Acore::StringTo<uint32>(RevMsg);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        if (Item* OnItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid)))
        {
            uint32 ItemId = OnItem->GetEntry();
            auto Itr = ItemEnchTaskStore.find(ItemId);
            if (Itr == ItemEnchTaskStore.end())
            {
                SendPacketToClient(player, "NoFuncItemEnch1");
                player->SendSysMsg(sHelper->GetGts(6303));
                return;
            }

            if (player->InEnchButton[1] && ItemGuid == player->InEnchButton[1])
            {
                SendPacketToClient(player, "NoFuncItemEnch1");
                player->SendSysMsg(sHelper->GetGts(6302));
                return;
            }

            if (!sHelper->ItemHasEnch(OnItem))
            {
                SendPacketToClient(player, "NoFuncItemEnch1");
                player->SendSysMsg(sHelper->GetGts(6301));
                return;
            }

            player->InEnchButton[0] = ItemGuid;
            std::string Gts = sHelper->GetNeedGts(Itr->second);
            SendPacketToClient(player, "YesEnchTaskCheck1", std::to_string(ItemGuid) + "#" + std::to_string(ItemId) + "#" + Gts);

            uint8 Num = 0;
            for (uint32 i = PROP_ENCHANTMENT_SLOT_0; i < MAX_ENCHANTMENT_SLOT; ++i)
            {
                Num++;
                uint32 EnchId = OnItem->GetEnchantmentId(EnchantmentSlot(i));
                std::string EnchDes = sHelper->GetEnchName(EnchId);
                SendPacketToClient(player, "ShowEnchInfo1", std::to_string(Num) + "#" + std::to_string(EnchId) + "#" + EnchDes);
            }
        }
    }
    else if (IsPacket(RHMsg, "ItemEnchTaskCheck2"))
    {
        if (!sHelper->GetBoolean(1415))
            return;
        auto parsedItemGuid = Acore::StringTo<uint32>(RevMsg);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        if (Item* OnItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid)))
        {
            uint32 ItemId = OnItem->GetEntry();
            auto Itr = ItemEnchTaskStore.find(ItemId);
            if (Itr == ItemEnchTaskStore.end())
            {
                SendPacketToClient(player, "NoFuncItemEnch2");
                player->SendSysMsg(sHelper->GetGts(6303));
                return;
            }

            if (player->InEnchButton[0] && ItemGuid == player->InEnchButton[0])
            {
                SendPacketToClient(player, "NoFuncItemEnch2");
                player->SendSysMsg(sHelper->GetGts(6302));
                return;
            }

            player->InEnchButton[1] = ItemGuid;
            SendPacketToClient(player, "YesEnchTaskCheck2", std::to_string(ItemGuid) + "#" + std::to_string(ItemId));
            uint8 Num = 0;
            for (uint32 i = PROP_ENCHANTMENT_SLOT_0; i < MAX_ENCHANTMENT_SLOT; ++i)
            {
                Num++;
                uint32 EnchId = OnItem->GetEnchantmentId(EnchantmentSlot(i));
                std::string EnchDes = sHelper->GetEnchName(EnchId);
                SendPacketToClient(player, "ShowEnchInfo2", std::to_string(Num) + "#" + std::to_string(EnchId) + "#" + EnchDes);
            }
        }
    }
    else if (IsPacket(RHMsg, "ItemEnchInitNow"))
    {
        if (!sHelper->GetBoolean(1415))
            return;
        player->InEnchButton[0] = 0;
        player->InEnchButton[1] = 0;
    }
    else if (IsPacket(RHMsg, "DoItemEnchTaskInfo"))
    {
        if (!sHelper->GetBoolean(1415))
            return;
        if (player->InEnchButton[0] && player->InEnchButton[1])
        {
            Item* Item1 = player->GetItemByGuid(ObjectGuid(HighGuid::Item, player->InEnchButton[0]));
            Item* Item2 = player->GetItemByGuid(ObjectGuid(HighGuid::Item, player->InEnchButton[1]));
            if (Item1 && Item2)
            {
                if (Item1->IsEquipped())
                {
                    player->NewInEnchButton[0] = 0;
                    SendPacketToClient(player, "NoFuncItemEnch1");
                    player->SendSysMsg(sHelper->GetGts(1085));
                    return;
                }

                if (Item2->IsEquipped())
                {
                    player->NewInEnchButton[1] = 0;
                    SendPacketToClient(player, "NoFuncItemEnch2");
                    player->SendSysMsg(sHelper->GetGts(1085));
                    return;
                }

                uint32 ItemId1 = Item1->GetEntry();
                uint32 ItemId2 = Item2->GetEntry();
                auto Itr1 = ItemEnchTaskStore.find(ItemId1);
                if (Itr1 == ItemEnchTaskStore.end())
                {
                    player->NewInEnchButton[1] = 0;
                    SendPacketToClient(player, "NoFuncItemEnch1");
                    player->SendSysMsg(sHelper->GetGts(6303));
                    return;
                }

                auto Itr2 = ItemEnchTaskStore.find(ItemId2);
                if (Itr2 == ItemEnchTaskStore.end())
                {
                    SendPacketToClient(player, "NoFuncItemEnch2");
                    player->SendSysMsg(sHelper->GetGts(6303));
                    return;
                }

                if (!sHelper->ItemHasEnch(Item1))
                {
                    player->InEnchButton[0] = 0;
                    SendPacketToClient(player, "NoFuncItemEnch1");
                    player->SendSysMsg(sHelper->GetGts(6301));
                    return;
                }

                if (!sHelper->CheckNeed(player, Itr1->second, "附魔提取|转移消耗"))
                    return;

                auto enchantSlot = Acore::StringTo<uint32>(RevMsg);
                if (!enchantSlot || *enchantSlot >= MAX_ENCHANTMENT_SLOT)
                    return;
                EnchantmentSlot Slot = EnchantmentSlot(*enchantSlot);
                sItemFuncMgr->DoRandomEnchSwitchOne(player, Item2, Item1, Slot);
                sItemFuncMgr->SendUpdateShowEnchInfo1(player, Item1);
                sItemFuncMgr->SendUpdateShowEnchInfo2(player, Item2);
            }
        }
    }
    else if (IsPacket(RHMsg, "GetMeSoulItemData"))
    {
        if (!sHelper->GetBoolean(1416))
            return;
        sItemFuncMgr->SendClientSoulData(player);
    }
    else if (IsPacket(RHMsg, "InItemSoulQuery"))
    {
        if (!sHelper->GetBoolean(1416))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 2)
            return;

        uint32 OwnerLow = player->GetGUIDLow();
        auto itemGuid = Acore::StringTo<uint32>(Datas[0]);
        auto number = Acore::StringTo<uint32>(Datas[1]);
        if (!itemGuid || !*itemGuid || !number || *number < 1 || *number > SOUL_SLOT_END)
            return;

        uint32 ItemGuid = *itemGuid;
        uint32 Num = *number;
        uint8 Slot = uint8(Num - 1);
        uint32 OGuidLow = sItemFuncMgr->GetSoulItemGuid(player->GetGUIDLow(), Slot);
        if (Item* OnItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid)))
        {
            if (!sItemFuncMgr->CanEquipSoul(player, OnItem, Slot))
            {
                if (!OGuidLow)
                    SendPacketToClient(player, "NoItemSoul", std::string(Datas[1]));

                player->SendSysMsg(sHelper->GetGts(6501));
                return;
            }

            uint32 SoulNTempId = sItemFuncMgr->GetSoulNTempId(OnItem->GetEntry());
            if (SoulNTempId)
            {
                std::ostringstream Oss;
                uint32 Entry = sItemFuncMgr->GetSoulItemEntry(OwnerLow, Slot);
                if (!Entry)
                {
                    Oss << sHelper->GetGts(6628);
                }
                else
                {
                    Oss << Acore::StringFormat(sHelper->GetGts(6629), sHelper->GetItemLink(Entry));
                    Oss << sHelper->GetItemLink(OnItem->GetEntry()) << "\n\n";
                }

                Oss << sHelper->GetNeedGts(SoulNTempId);
                SendPacketToClient(player, "NeedInfoItemSoul", std::string(Datas[0]) + "#" + std::string(Datas[1]) + "#" + Oss.str());
            }
            else
            {
                uint32 Entry = sItemFuncMgr->GetSoulItemEntry(player->GetGUIDLow(), Slot);
                if (Entry)
                {
                    uint32 OGuidLow = sItemFuncMgr->GetSoulItemGuid(player->GetGUIDLow(), Slot);
                    if (Item* OItem = sItemFuncMgr->GetEquipSoulItem(OGuidLow))
                    {
                        OItem->SetOwnerGUID(player->GetGUID());
                        sItemFuncMgr->ApplySoulItemStatsAndSpells(player, OItem, sItemFuncMgr->GetOriginalSlot(Slot), false);
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
                        sItemFuncMgr->RefreshSoulItemStats(player);
                        player->SendSysMsg(sHelper->GetGts(1070));
                    }
                }

                sItemFuncMgr->UpdateSoulItem(player, OnItem, Slot);
                player->SendSysMsg(sHelper->GetGts(1071));
                sItemFuncMgr->SendClientSoulOneData(player, OnItem, Slot);
            }
        }
        else
        {
            if (!OGuidLow)
                SendPacketToClient(player, "NoItemSoul", std::string(Datas[1]));
        }
    }
    else if (IsPacket(RHMsg, "AcceptItemSoulStartNow"))
    {
        if (!sHelper->GetBoolean(1416))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 2)
            return;

        uint32 OwnerLow = player->GetGUIDLow();
        auto itemGuid = Acore::StringTo<uint32>(Datas[0]);
        auto number = Acore::StringTo<uint32>(Datas[1]);
        if (!itemGuid || !*itemGuid || !number || *number < 1 || *number > SOUL_SLOT_END)
            return;

        uint32 ItemGuid = *itemGuid;
        uint32 Num = *number;
        uint8 Slot = uint8(Num - 1);
        uint32 OGuidLow = sItemFuncMgr->GetSoulItemGuid(player->GetGUIDLow(), Slot);
        if (Item* OnItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid)))
        {
            if (!sItemFuncMgr->CanEquipSoul(player, OnItem, Slot))
            {
                if (!OGuidLow)
                    SendPacketToClient(player, "NoItemSoul", std::string(Datas[1]));
                player->SendSysMsg(sHelper->GetGts(6501));
                return;
            }

            uint32 Entry = sItemFuncMgr->GetSoulItemEntry(player->GetGUIDLow(), Slot);
            if (Entry)
            {
                uint32 OGuidLow = sItemFuncMgr->GetSoulItemGuid(player->GetGUIDLow(), Slot);
                if (Item* OItem = sItemFuncMgr->GetEquipSoulItem(OGuidLow))
                {
                    OItem->SetOwnerGUID(player->GetGUID());
                    sItemFuncMgr->ApplySoulItemStatsAndSpells(player, OItem, sItemFuncMgr->GetOriginalSlot(Slot), false);
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
                    sItemFuncMgr->RefreshSoulItemStats(player);
                    player->SendSysMsg(sHelper->GetGts(1070));
                }
            }

            if (sHelper->CheckNeed(player, sItemFuncMgr->GetSoulNTempId(OnItem->GetEntry()), "双甲装备消耗"))
            {
                sItemFuncMgr->UpdateSoulItem(player, OnItem, Slot);
                player->SendSysMsg(sHelper->GetGts(1071));
            }

            sItemFuncMgr->SendClientSoulOneData(player, OnItem, Slot);
        }
    }
    else if (IsPacket(RHMsg, "TaskItemSoul"))
    {
        if (!sHelper->GetBoolean(1416))
            return;

        auto number = Acore::StringTo<uint32>(RevMsg);
        if (!number || *number < 1 || *number > SOUL_SLOT_END)
            return;

        uint32 Num = *number;
        uint8 Slot = uint8(Num - 1);
        uint32 Entry = sItemFuncMgr->GetSoulItemEntry(player->GetGUIDLow(), Slot);
        if (Entry)
        {
            uint32 OGuidLow = sItemFuncMgr->GetSoulItemGuid(player->GetGUIDLow(), Slot);
            if (Item* OItem = sItemFuncMgr->GetEquipSoulItem(OGuidLow))
            {
                OItem->SetOwnerGUID(player->GetGUID());
                sItemFuncMgr->ApplySoulItemStatsAndSpells(player, OItem, sItemFuncMgr->GetOriginalSlot(Slot), false);
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
                sItemFuncMgr->RefreshSoulItemStats(player);
                player->SendSysMsg(sHelper->GetGts(1070));
            }
        }

        sItemFuncMgr->SendClientSoulTaskOneData(player, Slot);
    }
    else if (IsPacket(RHMsg, "NewItemEnchTaskCheck1"))
    {
        if (!sHelper->GetBoolean(1417))
            return;
        auto parsedItemGuid = Acore::StringTo<uint32>(RevMsg);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        if (Item* OnItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid)))
        {
            uint32 ItemId = OnItem->GetEntry();
            if (!sItemFuncMgr->InItemEnchTaskStoreLeft(ItemId))
            {
                SendPacketToClient(player, "NewNoFuncItemEnch1");
                player->SendSysMsg(sHelper->GetGts(6313));
                return;
            }

            if (player->NewInEnchButton[1] && ItemGuid == player->NewInEnchButton[1])
            {
                SendPacketToClient(player, "NewNoFuncItemEnch1");
                player->SendSysMsg(sHelper->GetGts(6312));
                return;
            }

            if (!sHelper->ItemHasEnch(OnItem))
            {
                SendPacketToClient(player, "NewNoFuncItemEnch1");
                player->SendSysMsg(sHelper->GetGts(6311));
                return;
            }

            player->NewInEnchButton[0] = ItemGuid;
            SendPacketToClient(player, "NewYesEnchTaskCheck1", std::to_string(ItemGuid) + "#" + std::to_string(ItemId));

            uint8 Num = 0;
            for (uint32 i = PROP_ENCHANTMENT_SLOT_0; i < MAX_ENCHANTMENT_SLOT; ++i)
            {
                Num++;
                uint32 EnchId = OnItem->GetEnchantmentId(EnchantmentSlot(i));
                std::string EnchDes = sHelper->GetEnchName(EnchId);
                SendPacketToClient(player, "NewShowEnchInfo1", std::to_string(Num) + "#" + std::to_string(EnchId) + "#" + EnchDes);
            }
        }
    }
    else if (IsPacket(RHMsg, "NewItemEnchTaskCheck2"))
    {
        if (!sHelper->GetBoolean(1417))
            return;
        auto parsedItemGuid = Acore::StringTo<uint32>(RevMsg);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        if (Item* OnItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid)))
        {
            if (player->NewInEnchButton[0] && ItemGuid == player->NewInEnchButton[0])
            {
                SendPacketToClient(player, "NewNoFuncItemEnch2");
                player->SendSysMsg(sHelper->GetGts(6312));
                return;
            }

            uint32 ItemId = OnItem->GetEntry();
            Item* LeftItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, player->NewInEnchButton[0]));
            if (LeftItem && !sItemFuncMgr->InKtItemEnchTaskStore(LeftItem->GetEntry(), ItemId))
            {
                SendPacketToClient(player, "NewNoFuncItemEnch2");
                player->SendSysMsg(sHelper->GetGts(6313));
                return;
            }

            player->NewInEnchButton[1] = ItemGuid;
            SendPacketToClient(player, "NewYesEnchTaskCheck2", std::to_string(ItemGuid) + "#" + std::to_string(ItemId));
            uint8 Num = 0;
            for (uint32 i = PROP_ENCHANTMENT_SLOT_0; i < MAX_ENCHANTMENT_SLOT; ++i)
            {
                Num++;
                uint32 EnchId = OnItem->GetEnchantmentId(EnchantmentSlot(i));
                std::string EnchDes = sHelper->GetEnchName(EnchId);
                SendPacketToClient(player, "NewShowEnchInfo2", std::to_string(Num) + "#" + std::to_string(EnchId) + "#" + EnchDes);
            }
        }
    }
    else if (IsPacket(RHMsg, "NewItemEnchInitNow"))
    {
        if (!sHelper->GetBoolean(1417))
            return;
        player->NewInEnchButton[0] = 0;
        player->NewInEnchButton[1] = 0;
    }
    else if (IsPacket(RHMsg, "NewDoItemEnchTaskInfo"))
    {
        if (!sHelper->GetBoolean(1417))
            return;
        auto parsedAtSlot = Acore::StringTo<uint32>(RevMsg);
        if (!parsedAtSlot)
            return;
        uint32 AtSlot = *parsedAtSlot;
        if (AtSlot >= MAX_ENCHANTMENT_SLOT - PRISMATIC_ENCHANTMENT_SLOT)
            return;
        EnchantmentSlot Slot = EnchantmentSlot(AtSlot + PRISMATIC_ENCHANTMENT_SLOT);
        if (player->NewInEnchButton[0] && player->NewInEnchButton[1])
        {
            Item* Item1 = player->GetItemByGuid(ObjectGuid(HighGuid::Item, player->NewInEnchButton[0]));
            Item* Item2 = player->GetItemByGuid(ObjectGuid(HighGuid::Item, player->NewInEnchButton[1]));
            if (Item1 && Item2)
            {
                uint32 ItemId1 = Item1->GetEntry();
                uint32 ItemId2 = Item2->GetEntry();
                if (!sItemFuncMgr->InSlotItemEnchTaskStore(ItemId1, ItemId2, AtSlot))
                {
                    player->NewInEnchButton[1] = 0;
                    SendPacketToClient(player, "NewNoFuncItemEnch2");
                    player->SendSysMsg(sHelper->GetGts(6314));
                    return;
                }

                if (!sHelper->ItemHasEnch(Item1))
                {
                    player->NewInEnchButton[0] = 0;
                    SendPacketToClient(player, "NewNoFuncItemEnch1");
                    player->SendSysMsg(sHelper->GetGts(6311));
                    return;
                }

                uint32 NTempId = sItemFuncMgr->GetTaskNTempId(ItemId1, ItemId2, AtSlot);
                std::string Gts = sHelper->GetNeedGts(NTempId);
                SendPacketToClient(player, "NewDoItemEnchTaskNeedGts", std::string(RevMsg) + "#" + Gts);
            }
        }
    }
    else if (IsPacket(RHMsg, "NewYesDoItemEnchTask"))
    {
        if (!sHelper->GetBoolean(1417))
            return;
        auto parsedAtSlot = Acore::StringTo<uint32>(RevMsg);
        if (!parsedAtSlot)
            return;
        uint32 AtSlot = *parsedAtSlot;
        if (AtSlot >= MAX_ENCHANTMENT_SLOT - PRISMATIC_ENCHANTMENT_SLOT)
            return;
        EnchantmentSlot Slot = EnchantmentSlot(AtSlot + PRISMATIC_ENCHANTMENT_SLOT);
        if (player->NewInEnchButton[0] && player->NewInEnchButton[1])
        {
            Item* Item1 = player->GetItemByGuid(ObjectGuid(HighGuid::Item, player->NewInEnchButton[0]));
            Item* Item2 = player->GetItemByGuid(ObjectGuid(HighGuid::Item, player->NewInEnchButton[1]));
            if (Item1 && Item2)
            {
                uint32 ItemId1 = Item1->GetEntry();
                uint32 ItemId2 = Item2->GetEntry();

                if (Item1->IsEquipped())
                {
                    player->NewInEnchButton[0] = 0;
                    SendPacketToClient(player, "NewNoFuncItemEnch1");
                    player->SendSysMsg(sHelper->GetGts(1085));
                    return;
                }

                if (Item2->IsEquipped())
                {
                    player->NewInEnchButton[1] = 0;
                    SendPacketToClient(player, "NewNoFuncItemEnch2");
                    player->SendSysMsg(sHelper->GetGts(1085));
                    return;
                }

                if (!sItemFuncMgr->InSlotItemEnchTaskStore(ItemId1, ItemId2, AtSlot))
                {
                    player->NewInEnchButton[1] = 0;
                    SendPacketToClient(player, "NewNoFuncItemEnch2");
                    player->SendSysMsg(sHelper->GetGts(6314));
                    return;
                }

                if (!sHelper->ItemHasEnch(Item1))
                {
                    player->NewInEnchButton[0] = 0;
                    SendPacketToClient(player, "NewNoFuncItemEnch1");
                    player->SendSysMsg(sHelper->GetGts(6311));
                    return;
                }

                uint32 NTempId = sItemFuncMgr->GetTaskNTempId(ItemId1, ItemId2, AtSlot);
                if (!sHelper->CheckNeed(player, NTempId, "附魔提取|转移消耗"))
                    return;

                sItemFuncMgr->DoRandomEnchSwitchOne(player, Item2, Item1, Slot);
                sItemFuncMgr->NewSendUpdateShowEnchInfo1(player, Item1);
                sItemFuncMgr->NewSendUpdateShowEnchInfo2(player, Item2);

                SendPacketToClient(player, "NewNoFuncItemEnch1");
                player->DestroyItem(Item1->GetBagSlot(), Item1->GetSlot(), true);
            }
        }
    }
    else if (IsPacket(RHMsg, "GetMeZodiacItemData"))
    {
        if (!sHelper->GetBoolean(1418))
            return;

        std::string SendMsg;
        for (uint32 i = AT5001; i < ATZCMAX; i++)
            SendMsg.append(AsInt64ToString(player->TempAtStore[i].Value) + "#");

        if (!SendMsg.empty())
            SendPacketToClient(player, "AutoZodiacItemData", SendMsg);
    }
    else if (IsPacket(RHMsg, "InItemZodiacQuery"))
    {
        if (!sHelper->GetBoolean(1418))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 2)
            return;

        uint32 OwnerLow = player->GetGUIDLow();
        auto parsedItemGuid = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        auto parsedNum = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedNum)
            return;
        uint32 Num = *parsedNum;

        if (Num < 1 || Num > 12)
            return;

        Item* OnItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid));
        if (!OnItem)
        {
            SendPacketToClient(player, "NotItemZodiac", std::to_string(Num));
            player->SendSysMsg(sHelper->GetGts(8001));
            return;
        }

        uint32 ItemId = OnItem->GetEntry();
        uint32 Slot = sItemMagicMgr->GetZodiacSlot(ItemId);
        if (Slot != Num)
        {
            SendPacketToClient(player, "NotItemZodiac", std::to_string(Num));
            player->SendSysMsg(sHelper->GetGts(8001));
            return;
        }

        if (!sItemMagicMgr->InZodiacStoreData(ItemId))
        {
            SendPacketToClient(player, "NotItemZodiac", std::to_string(Num));
            player->SendSysMsg(sHelper->GetGts(8001));
            return;
        }

        uint32 OldAuraId = player->GetTempAtUInt32(PlayerTemp(Num + AT5000));
        if (OldAuraId)
        {
            player->SendSysMsg(sHelper->GetGts(8004));
            return;
        }

        uint32 TempId = sItemMagicMgr->GetZodiacTempId(ItemId, true);
        if (TempId > 0)
        {
            SendPacketToClient(player, "NeedInfoItemZodiac", std::string(Datas[0]) + "#" + std::string(Datas[1]) + "#" + sHelper->GetNeedGts(TempId));
        }
        else
        {
            uint32 AuraId = sItemMagicMgr->GetZodiacAuraId(ItemId);
            if (AuraId)
            {
                if (!player->HasAura(AuraId))
                    player->AddAura(AuraId, player);

                SendPacketToClient(player, "UpdateZodiacItemData", std::to_string(Num) + "#" + std::to_string(AuraId));
                player->UpdateTempAt(PlayerTemp(Num + AT5000), AuraId);
                player->DestroyItem(OnItem->GetBagSlot(), OnItem->GetSlot(), true);
                player->SendSysMsg(sHelper->GetGts(8002), sHelper->GetSpellName(AuraId));
            }

            sItemMagicMgr->CheckUpdateCenterAsZodiacPlayer(player, true);
        }
    }
    else if (IsPacket(RHMsg, "AddItemZodiacNow"))
    {
        if (!sHelper->GetBoolean(1418))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 2)
            return;

        auto parsedItemGuid = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        auto parsedNum = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedNum)
            return;
        uint32 Num = *parsedNum;

        if (Num < 1 || Num > 12)
            return;

        Item* OnItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid));
        if (!OnItem)
        {
            SendPacketToClient(player, "NotItemZodiac", std::to_string(Num));
            player->SendSysMsg(sHelper->GetGts(8001));
            return;
        }

        uint32 ItemId = OnItem->GetEntry();
        uint32 Slot = sItemMagicMgr->GetZodiacSlot(ItemId);
        if (Slot != Num)
        {
            SendPacketToClient(player, "NotItemZodiac", std::to_string(Num));
            player->SendSysMsg(sHelper->GetGts(8001));
            return;
        }

        if (!sItemMagicMgr->InZodiacStoreData(ItemId))
        {
            SendPacketToClient(player, "NotItemZodiac", std::to_string(Num));
            player->SendSysMsg(sHelper->GetGts(8001));
            return;
        }

        uint32 OldAuraId = player->GetTempAtUInt32(PlayerTemp(Num + AT5000));
        if (OldAuraId)
        {
            player->SendSysMsg(sHelper->GetGts(8004));
            return;
        }

        uint32 TempId = sItemMagicMgr->GetZodiacTempId(ItemId, true);
        if (TempId > 0)
        {
            if (sHelper->CheckNeed(player, TempId, "插入生肖物品消耗"))
            {
                uint32 AuraId = sItemMagicMgr->GetZodiacAuraId(ItemId);
                if (AuraId)
                {
                    if (!player->HasAura(AuraId))
                        player->AddAura(AuraId, player);

                    SendPacketToClient(player, "UpdateZodiacItemData", std::to_string(Num) + "#" + std::to_string(AuraId));
                    player->UpdateTempAt(PlayerTemp(Num + AT5000), AuraId);
                    player->DestroyItem(OnItem->GetBagSlot(), OnItem->GetSlot(), true);
                    player->SendSysMsg(sHelper->GetGts(8002), sHelper->GetSpellName(AuraId));
                }

                sItemMagicMgr->CheckUpdateCenterAsZodiacPlayer(player, true);
            }
        }
    }
    else if (IsPacket(RHMsg, "OutItemZodiacQuery"))
    {
        if (!sHelper->GetBoolean(1418))
            return;

        auto parsedNum = Acore::StringTo<uint32>(RevMsg);
        if (!parsedNum)
            return;
        uint32 Num = *parsedNum;

        if (Num < 1 || Num > 12)
            return;

        uint32 OldAuraId = player->GetTempAtUInt32(PlayerTemp(Num + AT5000));
        if (!OldAuraId)
            return;

        uint32 ItemId = sItemMagicMgr->GetZodiacItemId(OldAuraId);
        if (!ItemId)
            return;

        uint32 TempId = sItemMagicMgr->GetZodiacTempId(ItemId, false);
        if (TempId > 0)
        {
            SendPacketToClient(player, "OutNeedInfoItemZodiac", std::string(RevMsg) + "#" + sHelper->GetNeedGts(TempId));
        }
        else
        {
            if (player->HasAura(OldAuraId))
                player->RemoveAurasDueToSpell(OldAuraId);

            player->AddItem(ItemId, 1, "取出生肖物品");
            SendPacketToClient(player, "UpdateZodiacItemData", std::to_string(Num) + "#0");

            sItemMagicMgr->CheckUpdateCenterAsZodiacPlayer(player, false);

            player->UpdateTempAt(PlayerTemp(Num + AT5000), 0);
        }
    }
    else if (IsPacket(RHMsg, "TaskItemZodiacNow"))
    {
        if (!sHelper->GetBoolean(1418))
            return;

        auto parsedNum = Acore::StringTo<uint32>(RevMsg);
        if (!parsedNum)
            return;
        uint32 Num = *parsedNum;

        if (Num < 1 || Num > 12)
            return;

        uint32 OldAuraId = player->GetTempAtUInt32(PlayerTemp(Num + AT5000));
        if (!OldAuraId)
            return;

        uint32 ItemId = sItemMagicMgr->GetZodiacItemId(OldAuraId);
        if (!ItemId)
            return;

        uint32 TempId = sItemMagicMgr->GetZodiacTempId(ItemId, false);
        if (TempId > 0)
        {
            if (sHelper->CheckNeed(player, TempId, "取出生肖物品消耗"))
            {
                if (player->HasAura(OldAuraId))
                    player->RemoveAurasDueToSpell(OldAuraId);

                player->AddItem(ItemId, 1, "取出生肖物品");
                SendPacketToClient(player, "UpdateZodiacItemData", std::to_string(Num) + "#0");
                sItemMagicMgr->CheckUpdateCenterAsZodiacPlayer(player, false);

                player->UpdateTempAt(PlayerTemp(Num + AT5000), 0);
            }
        }
    }
    else if (IsPacket(RHMsg, "GetCombatPetPlayerData"))
    {
        //记录 授权验证
        if (!sHelper->InCheckFunc(FuncPet))
            return;

        player->CharmSpellInitialize();
        sCombatPetMgr->AutoFillCombatPetPlayerDataAll(player);
    }
    else if (IsPacket(RHMsg, "FitCombatPetNow"))
    {
        //记录 授权验证
        if (!sHelper->InCheckFunc(FuncPet))
        {
            //LOG_ERROR("server.worldserver", ">> 没有授权 无法使用该功能");
            player->SendSysMsg("[提示]: 没有授权 无法使用该功能");
            return;
        }

        if (!sHelper->GetBoolean(18005) && player->IsInCombat())
        {
            player->SendSysMsg(sHelper->GetGts(1001));
            return;
        }

        uint32 Delay = 3;
        time_t Current = GameTime::GetGameTime().count();
        if (player->SumPetDelay1 < Current)
        {
            player->SumPetDelay1 = Current + Delay;
            auto parsedAddonIndex = Acore::StringTo<uint32>(RevMsg);
            if (!parsedAddonIndex)
                return;
            uint32 AddonIndex = *parsedAddonIndex;
            sCombatPetMgr->UpdateFitCombatPetNow(player, AddonIndex);
        }
        else
        {
            uint32 CheckTime = player->SumPetDelay1 - Current;
            player->SendSysMsg(sHelper->GetGts(9514), CheckTime);
        }
    }
    else if (IsPacket(RHMsg, "ApartCombatPetNow"))
    {
        //记录 授权验证
        if (!sHelper->InCheckFunc(FuncPet))
        {
            //LOG_ERROR("server.worldserver", ">> 没有授权 无法使用该功能");
            player->SendSysMsg("[提示]: 没有授权 无法使用该功能");
            return;
        }

        uint32 Delay = 3;
        time_t Current = GameTime::GetGameTime().count();
        if (player->SumPetDelay2 < Current)
        {
            player->SumPetDelay2 = Current + Delay;
            auto parsedAddonIndex = Acore::StringTo<uint32>(RevMsg);
            if (!parsedAddonIndex)
                return;
            uint32 AddonIndex = *parsedAddonIndex;
            sCombatPetMgr->ApartSummonComCurrentbatPet(player, AddonIndex);
        }
        else
        {
            uint32 CheckTime = player->SumPetDelay2 - Current;
            player->SendSysMsg(sHelper->GetGts(9514), CheckTime);
        }
    }
    else if (IsPacket(RHMsg, "AddonToggleSummon"))
    {
        //记录 授权验证
        if (!sHelper->InCheckFunc(FuncPet))
        {
            //LOG_ERROR("server.worldserver", ">> 没有授权 无法使用该功能");
            player->SendSysMsg("[提示]: 没有授权 无法使用该功能");
            return;
        }

        if (!sHelper->GetBoolean(18005) && player->IsInCombat())
        {
            player->SendSysMsg(sHelper->GetGts(1001));
            return;
        }

        uint32 CreateIndex = player->GetTempAtUInt32(AT7002);
        if (CreateIndex && !sHelper->GetBoolean(18004))
        {
            player->SendSysMsg(sHelper->GetGts(9521));
            return;
        }

        auto parsedAddonIndex = Acore::StringTo<uint32>(RevMsg);

        if (!parsedAddonIndex)

            return;

        uint32 AddonIndex = *parsedAddonIndex;
        sCombatPetMgr->AddonToggleSummonComCurrentbatPet(player, AddonIndex);
    }
    else if (IsPacket(RHMsg, "AddonDismissCritter"))
    {
        //记录 授权验证
        if (!sHelper->InCheckFunc(FuncPet))
        {
            //LOG_ERROR("server.worldserver", ">> 没有授权 无法使用该功能");
            player->SendSysMsg("[提示]: 没有授权 无法使用该功能");
            return;
        }

        auto parsedAddonIndex = Acore::StringTo<uint32>(RevMsg);
        if (!parsedAddonIndex)
            return;
        uint32 AddonIndex = *parsedAddonIndex;
        sCombatPetMgr->ActiveHandleDismissCritter(player, AddonIndex);
    }
    else if (IsPacket(RHMsg, "AcceptAddonDeleteDB"))
    {
        //记录 授权验证
        if (!sHelper->InCheckFunc(FuncPet))
        {
            //LOG_ERROR("server.worldserver", ">> 没有授权 无法使用该功能");
            player->SendSysMsg("[提示]: 没有授权 无法使用该功能");
            return;
        }

        auto parsedAddonIndex = Acore::StringTo<uint32>(RevMsg);
        if (!parsedAddonIndex)
            return;
        uint32 AddonIndex = *parsedAddonIndex;
        sCombatPetMgr->ActiveDeleteDB(player, AddonIndex);
    }
    else if (IsPacket(RHMsg, "GetOpenCustomTargetHp"))
    {
        bool customHealthEnabled = sHelper->GetBoolean(17000) || sHelper->GetBoolean(22010);
        SendPacketToClient(player, "AutoOpenCustomTargetHp", std::to_string(customHealthEnabled ? 1 : 0));
    }
    else if (IsPacket(RHMsg, "QueryCustomTargetHp"))
    {
        if (!sHelper->GetBoolean(17000) && !sHelper->GetBoolean(22010))
            return;

        auto parsedCreGuidLow = Acore::StringTo<uint32>(RevMsg);
        if (!parsedCreGuidLow)
            return;
        uint32 CreGuidLow = *parsedCreGuidLow;
        if (!CreGuidLow)
            return;

        Unit* SelectedUnit = player->GetSelectedUnit();
        if (!SelectedUnit)
            return;

        Creature* SelectedCreature = SelectedUnit->ToCreature();
        if (!SelectedCreature)
            return;

        if (SelectedCreature->GetGUIDLow() != CreGuidLow)
            return;

        if (!player->IsValidAttackTarget(SelectedUnit))
            return;

        std::string huanJingHealthPayload;
        if (sHuanJingSystem && sHuanJingSystem->BuildCustomTargetHealthPacket(SelectedCreature, huanJingHealthPayload))
        {
            SendPacketToClient(player, "CustomTargetHpCount", huanJingHealthPayload);
            return;
        }

        uint32 currentBars = SelectedCreature->MultipleHp;
        uint32 maxBars = SelectedCreature->MaxMultipleHp;
        if (sHelper->GetBoolean(22010) || maxBars < 2)
        {
            // An explicit single-bar reply completes the target query. Silence
            // cannot distinguish a single bar from a pending multi-bar snapshot.
            // Never restore legacy multi-bar counts when HuanJing owns the bars.
            currentBars = SelectedCreature->IsAlive() ? 1 : 0;
            maxBars = 1;
        }

        AsInt64 barHealth = SelectedCreature->GetAsHealth();
        if (barHealth <= 0)
            barHealth = AsInt64(SelectedCreature->GetHealth());

        AsInt64 barMax = SelectedCreature->GetAsMaxHealth();
        if (barMax <= 0)
            barMax = AsInt64(SelectedCreature->GetMaxHealth());

        SendPacketToClient(player, "CustomTargetHpCount",
            std::to_string(CreGuidLow) + "#" +
            std::to_string(currentBars) + "#" +
            std::to_string(maxBars) + "#" +
            AsInt64ToString(barHealth) + "#" +
            AsInt64ToString(barMax));
    }
    else if (IsPacket(RHMsg, "TaskEnchSpellCheck"))
    {
        if (!sHelper->GetBoolean(1420))
            return;
        auto parsedItemGuid = Acore::StringTo<uint32>(RevMsg);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        Item* OnItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid));
        if (!OnItem)
        {
            SendPacketToClient(player, "NoFuncItemTaskEnchSpell");
            return;
        }

        auto Itr = ItemEnchStore.find(OnItem->GetEntry());
        if (Itr == ItemEnchStore.end())
        {
            SendPacketToClient(player, "NoFuncItemTaskEnchSpell");
            player->SendSysMsg(sHelper->GetGts(6315));
            return;
        }

        if (!sHelper->ItemHasEnch(OnItem))
        {
            SendPacketToClient(player, "NoFuncItemTaskEnchSpell");
            player->SendSysMsg(sHelper->GetGts(6311));
            return;
        }

        std::string SendMsg;
        for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
        {
            uint32 EnchantId = OnItem->GetEnchantmentId(EnchantmentSlot(ESlot));
            SendMsg.append(std::to_string(EnchantId) + "#");
        }

        if (!SendMsg.empty())
        {
            SendPacketToClient(player, "TaskEnchSpellCheckYes", SendMsg);
            SendMsg.clear();
        }

        uint32 HItemId = sHelper->GetUInt32(20011);
        uint32 HCount = player->GetItemCount(HItemId);
        SendPacketToClient(player, "TaskEnchSpellCheckItemCount", std::to_string(HCount));
    }
    else if (IsPacket(RHMsg, "TaskEnchSpellUpdateNow"))
    {
        if (!sHelper->GetBoolean(1420))
            return;
        uint32 HItemId = sHelper->GetUInt32(20011);
        if (!player->HasItemCount(HItemId))
        {
            player->SendSysMsg(sHelper->GetGts(1011), sHelper->GetItemLink(HItemId).c_str(), 1);
            return;
        }

        if (RMsg.find("^") == std::string::npos)
            return;
        if (RMsg.find("$") == std::string::npos)
            return;
        if (RMsg.find("#") == std::string::npos)
            return;

        Tokenizer Datas(RMsg, '^');
        if (Datas.IsEmpty() || Datas.size() < 2)
            return;

        auto parsedItemGuid = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedItemGuid)
            return;
        uint32 ItemGuid = *parsedItemGuid;
        Item* OnItem = player->GetItemByGuid(ObjectGuid(HighGuid::Item, ItemGuid));
        if (!OnItem)
            return;

        auto Itr = ItemEnchStore.find(OnItem->GetEntry());
        if (Itr == ItemEnchStore.end())
            return;

        Tokenizer Args(Datas[0], '$');
        if (Args.IsEmpty() || Args.size() > 5)
            return;

        std::vector<EnchantmentSlot> slotsToReroll;
        for (uint8 i = 0; i < Args.size(); i++)
        {
            Tokenizer Arg(Args[i], '#');
            if (Arg.size() != 2)
                return;
            auto slot = Acore::StringTo<uint32>(Arg[0]);
            auto keep = Acore::StringTo<uint32>(Arg[1]);
            if (!slot || !keep || *slot >= MAX_ENCHANTMENT_SLOT - PRISMATIC_ENCHANTMENT_SLOT || *keep > 1)
                return;
            EnchantmentSlot slotToReroll = EnchantmentSlot(*slot + PRISMATIC_ENCHANTMENT_SLOT);
            if (*keep != 1 && std::find(slotsToReroll.begin(), slotsToReroll.end(), slotToReroll) == slotsToReroll.end())
                slotsToReroll.push_back(slotToReroll);
        }
        for (EnchantmentSlot slot : slotsToReroll)
            sItemFuncMgr->DoUseCustomEnchOneNoPacket(OnItem, slot);

        std::string SendMsg1;
        for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
        {
            uint32 EnchantId = OnItem->GetEnchantmentId(EnchantmentSlot(ESlot));
            SendMsg1.append(std::to_string(EnchantId) + "#");
        }

        if (!SendMsg1.empty() && OnItem->GetOwner())
        {
            sDoAddon->SendPacketToClient(OnItem->GetOwner(), "AutoEnchant", std::to_string(OnItem->GetGUIDLow()) + "$" + SendMsg1);
            SendMsg1.clear();
        }

        std::string SendMsg2;
        for (uint32 ESlot = PROP_ENCHANTMENT_SLOT_0; ESlot < MAX_ENCHANTMENT_SLOT; ++ESlot)
        {
            uint32 EnchantId = OnItem->GetEnchantmentId(EnchantmentSlot(ESlot));
            SendMsg2.append(std::to_string(EnchantId) + "#");
        }

        if (!SendMsg2.empty())
        {
            SendPacketToClient(player, "TaskEnchSpellCheckYesNow", SendMsg2);
            SendMsg2.clear();
        }

        player->DestroyItemCount(HItemId, 1, true);
        uint32 HCount = player->GetItemCount(HItemId);
        SendPacketToClient(player, "TaskEnchSpellCheckItemCount", std::to_string(HCount));
    }
    else if (IsPacket(RHMsg, "TaskEnchSpellNeedInfo"))
    {
        if (!sHelper->GetBoolean(1420))
            return;
        uint32 HItemId = sHelper->GetUInt32(20011);
        uint32 HCount = player->GetItemCount(HItemId);
        SendPacketToClient(player, "TaskEnchSpellNeedInfo", std::to_string(HItemId) + "#" + std::to_string(HCount));
    }
    else if (IsPacket(RHMsg, "GetItemPicGuideSubBtnCount"))
    {
        if (!sHelper->GetBoolean(1421))
            return;

        SendPacketToClient(player, "ItemPicGuideSubBtnCount", std::to_string(PicItemGuideConfigStore.size()) + "#" + std::to_string(PicCreatureGuideConfigStore.size()));
    }
    else if (IsPacket(RHMsg, "GetItemPicGuideSubBtnsData"))
    {
        if (!sHelper->GetBoolean(1421))
            return;

        uint32 Index = 0;
        std::string SendMsg;
        for (auto Itr = PicItemGuideConfigStore.begin(); Itr != PicItemGuideConfigStore.end(); Itr++)
        {
            Index++;
            SendMsg.append(std::to_string(Index) + "#" + Itr->second.SubName + "#" + Itr->second.SubIcon + "#" + std::to_string(Itr->second.SubPageId) + "$");
        }
        SendPacketToClient(player, "PicGuideItemConfig", SendMsg);
    }
    else if (IsPacket(RHMsg, "GetPicGuideItemPageCount"))
    {
        if (!sHelper->GetBoolean(1421))
            return;

        auto parsedSubPageId = Acore::StringTo<uint32>(RevMsg);
        if (!parsedSubPageId)
            return;
        uint32 SubPageId = *parsedSubPageId;
        if (SubPageId < 1)
            return;

        std::string SendMsg;
        uint32 ActCount = player->ActItemPicGuideBondsCount(SubPageId);
        auto TotalCount = PicItemGuidePageMultStore.count(SubPageId);
        SendMsg.append(std::to_string(ActCount) + "#" + std::to_string(TotalCount) + "&");
        uint32 BondsId = sPicGuideMgr->GetItemBondsIdFromSubPage(SubPageId);
        auto BRange = PicItemGuideBondsStore.equal_range(BondsId);
        for (auto Itr = BRange.first; Itr != BRange.second; Itr++)
        {
            SendMsg.append(std::to_string(Itr->second.BondsCount) + "#" + Itr->second.Comment + "$");
        }
        SendPacketToClient(player, "PicGuideItemPageCount", SendMsg);
    }
    else if (IsPacket(RHMsg, "GetItemPicGuideSubListData"))
    {
        if (!sHelper->GetBoolean(1421))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 3)
            return;

        auto parsedSubPageId = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedSubPageId)
            return;
        uint32 SubPageId = *parsedSubPageId;
        auto parsedPageIndex = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedPageIndex)
            return;
        uint32 PageIndex = *parsedPageIndex;
        auto parsedPageSize = Acore::StringTo<uint32>(Datas[2]);
        if (!parsedPageSize)
            return;
        uint32 PageSize = *parsedPageSize;
        if (PageIndex < 1 || PageSize < 1 || PageSize > 100)
            return;

        std::vector<PicItemGuidePage> SendDatas;
        auto Range = PicItemGuidePageMultStore.equal_range(SubPageId);
        for (auto Itr = Range.first; Itr != Range.second; Itr++)
            SendDatas.push_back(Itr->second);

        if (SendDatas.empty())
            return;

        int TotalCount = SendDatas.size();
        uint64 startOffset = uint64(PageIndex - 1) * PageSize;
        if (startOffset >= SendDatas.size())
            return;
        int StartIndex = int(startOffset);
        int EndIndex = int(std::min<uint64>(startOffset + PageSize, SendDatas.size()));
        auto ItrFilte = SendDatas.begin();
        std::advance(ItrFilte, StartIndex);
        uint32 Index = 0;
        std::string SendMsg;
        for (int i = StartIndex; i < EndIndex && ItrFilte != SendDatas.end(); ++i, ++ItrFilte)
        {
            Index++;
            bool IsAct = player->IsActItemPicGuide(ItrFilte->ItemId, ItrFilte->PageId);
            SendMsg.append(std::to_string(Index) + "#" + std::to_string(ItrFilte->ItemId) + "#" + std::to_string(ItrFilte->IsActStat) + "#" + std::to_string(ItrFilte->IsActSpell) + "#" + std::to_string(IsAct) + "$");
        }
        SendPacketToClient(player, "PicGuideItemPageMult", SendMsg);
    }
    else if (IsPacket(RHMsg, "ActivateItemPicGuide"))
    {
        if (!sHelper->GetBoolean(1421))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 3)
            return;

        auto parsedSubPageId = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedSubPageId)
            return;
        uint32 SubPageId = *parsedSubPageId;
        auto parsedItemId = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedItemId)
            return;
        uint32 ItemId = *parsedItemId;
        if (!player->HasItemCount(ItemId))
        {
            player->SendSysMsg(sHelper->GetGts(11000));
            return;
        }
        bool IsAct = player->IsActItemPicGuide(ItemId, SubPageId);
        if (IsAct)
            return;

        PicItemGuidePage* St = sPicGuideMgr->GetItemPageData(ItemId, SubPageId);
        if (!St)
            return;

        player->CastSpell(player, TxLevelSpell, false);
        player->UpdateItemPicGuideCache(ItemId, SubPageId);
        player->SendSysMsg(sHelper->GetGts(11001), sHelper->GetItemLink(ItemId).c_str());
        player->ApplyItemStatsAndEquipSpells(ItemId, St->IsActStat, St->IsActSpell);
        if (St->IsDelect)
            player->DestroyItemCount(ItemId, 1, true);

        sHelper->DoTheReward(player, St->RTempId, "图鉴物品激活奖励");
        SendPacketToClient(player, "ActivateItemPicGuideYes", RMsg);
        player->UpdateItemClickBondsAuraId(SubPageId, false);
    }
    else if (IsPacket(RHMsg, "GetCreaturePicGuideSubBtnsData"))
    {

        {
            WorldSessionMgr::SessionMap const& sessions = sWorldSessionMgr->GetAllSessions();
            for (WorldSessionMgr::SessionMap::const_iterator itr = sessions.begin(); itr != sessions.end(); ++itr)
            {
                if (!itr->second)
                    continue;

                Player* _player = itr->second->GetPlayer();
                if (!_player || !_player->IsInWorld())
                    continue;

                std::ostringstream ss;
                uint32 playerGuid = _player->GetGUID().GetCounter();
                ss << playerGuid << "#" << _player->m_HeahTcut << "#2";
                sDoAddon->SendPacketToClient(player, "haoThroughLimitHp", ss.str().c_str());
            }
        }

        if (!sHelper->GetBoolean(1421))
            return;

        uint32 Index = 0;
        std::string SendMsg;
        for (auto Itr = PicCreatureGuideConfigStore.begin(); Itr != PicCreatureGuideConfigStore.end(); Itr++)
        {
            Index++;
            SendMsg.append(std::to_string(Index) + "#" + Itr->second.SubName + "#" + Itr->second.SubIcon + "#" + std::to_string(Itr->second.SubPageId) + "$");
        }
        SendPacketToClient(player, "PicGuideCreatureConfig", SendMsg);
    }
    else if (IsPacket(RHMsg, "GetPicGuideCreaturePageCount"))
    {
        if (!sHelper->GetBoolean(1421))
            return;

        auto parsedSubPageId = Acore::StringTo<uint32>(RevMsg);
        if (!parsedSubPageId)
            return;
        uint32 SubPageId = *parsedSubPageId;
        if (SubPageId < 1)
            return;

        uint32 ActCount = 0;
        int TotalCount = PicCreatureGuidePageMultStore.count(SubPageId);
        auto Range = PicCreatureGuidePageMultStore.equal_range(SubPageId);
        for (auto Itr = Range.first; Itr != Range.second; Itr++)
        {
            if (player->IsActCreaturePicGuide(Itr->second.CreId, Itr->second.GetMaxKillCount(), Itr->second.PageId))
                ActCount++;
        }
        std::string SendMsg;
        SendMsg.append(std::to_string(ActCount) + "#" + std::to_string(TotalCount) + "&");
        uint32 BondsId = sPicGuideMgr->GetCreatureBondsIdFromSubPage(SubPageId);
        auto BRange = PicCreatureGuideBondsStore.equal_range(BondsId);
        for (auto Itr = BRange.first; Itr != BRange.second; Itr++)
        {
            SendMsg.append(std::to_string(Itr->second.BondsCount) + "#" + Itr->second.Comment + "$");
        }
        SendPacketToClient(player, "PicGuideCreaturePageCount", SendMsg);
    }
    else if (IsPacket(RHMsg, "GetCreaturePicGuideSubListData"))
    {
        if (!sHelper->GetBoolean(1421))
            return;

        if (RMsg.find("#") == std::string::npos)
            return;
        Tokenizer Datas(RMsg, '#');
        if (Datas.IsEmpty() || Datas.size() < 3)
            return;

        auto parsedSubPageId = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedSubPageId)
            return;
        uint32 SubPageId = *parsedSubPageId;
        auto parsedPageIndex = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedPageIndex)
            return;
        uint32 PageIndex = *parsedPageIndex;
        auto parsedPageSize = Acore::StringTo<uint32>(Datas[2]);
        if (!parsedPageSize)
            return;
        uint32 PageSize = *parsedPageSize;
        if (PageIndex < 1 || PageSize < 1 || PageSize > 100)
            return;

        std::vector<PicCreatureGuidePage> SendDatas;
        auto Range = PicCreatureGuidePageMultStore.equal_range(SubPageId);
        for (auto Itr = Range.first; Itr != Range.second; Itr++)
            SendDatas.push_back(Itr->second);

        if (SendDatas.empty())
            return;

        int TotalCount = SendDatas.size();
        uint64 startOffset = uint64(PageIndex - 1) * PageSize;
        if (startOffset >= SendDatas.size())
            return;
        int StartIndex = int(startOffset);
        int EndIndex = int(std::min<uint64>(startOffset + PageSize, SendDatas.size()));
        auto ItrFilte = SendDatas.begin();
        std::advance(ItrFilte, StartIndex);
        uint32 Index = 0;
        std::string SendMsg;
        for (int i = StartIndex; i < EndIndex && ItrFilte != SendDatas.end(); ++i, ++ItrFilte)
        {
            Index++;
            uint32 pKCount = player->GetCrePicGuideKillCount(ItrFilte->CreId, SubPageId);
            CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(ItrFilte->CreId);
            uint32 modelId1 = creatureTemplate ? creatureTemplate->Modelid1 : 0;
            uint32 modelId2 = creatureTemplate ? creatureTemplate->Modelid2 : 0;
            uint32 modelId3 = creatureTemplate ? creatureTemplate->Modelid3 : 0;
            uint32 modelId4 = creatureTemplate ? creatureTemplate->Modelid4 : 0;
            if (ItrFilte->KillRewards.empty())
                continue;
            auto const& LastReward = ItrFilte->KillRewards.back();
            std::string KillRewards;
            for (auto const& Reward : ItrFilte->KillRewards)
            {
                if (!KillRewards.empty())
                    KillRewards.append("~");
                KillRewards.append(std::to_string(Reward.Level) + ":" + std::to_string(Reward.StageKillCount) + ":" + std::to_string(Reward.StatType) + ":" + std::to_string(Reward.StatValue));
            }
            SendMsg.append(std::to_string(Index) + "#" + std::to_string(ItrFilte->CreId) + "#" + sHelper->GetCreatureNameByEntry(ItrFilte->CreId) + "#" + std::to_string(ItrFilte->Diff) + "#" + std::to_string(LastReward.KillCount) + "#" + std::to_string(LastReward.StatType) + "#" + std::to_string(LastReward.StatValue) + "#" + std::to_string(pKCount) + "#" + std::to_string(modelId1) + "#" + std::to_string(modelId2) + "#" + std::to_string(modelId3) + "#" + std::to_string(modelId4) + "#" + KillRewards + "$");
        }
        SendPacketToClient(player, "PicGuideCreaturePageMult", SendMsg);
    }
    else if (IsPacket(RHMsg, "CachePicGuideCreatureModel"))
    {
        if (!sHelper->GetBoolean(1421))
            return;

        auto parsedcreatureId = Acore::StringTo<uint32>(RevMsg);
        if (!parsedcreatureId)
            return;
        uint32 creatureId = *parsedcreatureId;
        CreatureTemplate const* creatureTemplate = sObjectMgr->GetCreatureTemplate(creatureId);
        if (!creatureTemplate)
            return;

        player->GetSession()->SendPacket(&creatureTemplate->queryData);
        SendPacketToClient(player, "PicGuideCreatureModelCached", std::to_string(creatureId) + "#" + std::to_string(creatureTemplate->GetFirstValidModelId()));
    }
    else if (IsPacket(RHMsg, "TipFunction"))
    {
        std::string SendMsg;
        SendMsg.clear();

        for (uint32 i = FuncIndex + 1; i < FuncIndex + AddonMaxIndex; i++)
        {
            if (sHelper->GetBoolean(i))
                SendMsg.append(std::to_string(i - FuncIndex) + "#");
        }

        SendPacketToClient(player, "BackTipFunction", SendMsg);
        SendMsg.clear();
    }
    else if (IsPacket(RHMsg, "AY_PlayerHealth"))
    {
        Tokenizer Datas(RMsg, '#');
        auto parsedplayerGuid = Acore::StringTo<uint32>(Datas[0]);
        if (!parsedplayerGuid)
            return;
        uint32 playerGuid = *parsedplayerGuid;
        auto parsedSubPageId1 = Acore::StringTo<uint32>(Datas[1]);
        if (!parsedSubPageId1)
            return;
        uint32 SubPageId1 = *parsedSubPageId1;
        auto parsedSubPageId2 = Acore::StringTo<uint32>(Datas[2]);
        if (!parsedSubPageId2)
            return;
        uint32 SubPageId2 = *parsedSubPageId2;

        {
            WorldSessionMgr::SessionMap const& sessions = sWorldSessionMgr->GetAllSessions();
            for (WorldSessionMgr::SessionMap::const_iterator itr = sessions.begin(); itr != sessions.end(); ++itr)
            {
                if (!itr->second)
                    continue;

                Player* _player = itr->second->GetPlayer();
                if (!_player || !_player->IsInWorld())
                    continue;

                uint32 _playerGuid = _player->GetGUID().GetCounter();
                if (playerGuid == _playerGuid)
                {
                    std::ostringstream ss;
                    ss << playerGuid << "#" << _player->m_HeahTcut << "#4";
                    sDoAddon->SendPacketToClient(player, "ThroughLimitHp", ss.str().c_str());
                }
            }
        }
    }
}
