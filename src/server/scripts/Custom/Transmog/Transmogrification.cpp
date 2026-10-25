#include "Transmogrification.h"
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
#include "ReputationMgr.h"
#include <CharacterCache.h>
#include <AI/ScriptedAI/ScriptedGossip.h>
#include <Custom/_Helper/Helper.h>
#pragma execution_character_set("utf-8")

#ifdef PRESETS
void Transmogrification::PresetTransmog(Player* player, Item* itemTransmogrified, uint32 fakeEntry, uint8 slot)
{
    if (!EnableSets)
        return;
    if (!player || !itemTransmogrified)
        return;
    if (slot >= EQUIPMENT_SLOT_END)
        return;
    if (!CanTransmogrifyItemWithItem(player, itemTransmogrified->GetTemplate(), sObjectMgr->GetItemTemplate(fakeEntry)))
        return;

    // [AZTH] Custom
    if (GetFakeEntry(itemTransmogrified->GetGUID().GetRawValue()))
        DeleteFakeEntry(player, slot, itemTransmogrified);

    SetFakeEntry(player, fakeEntry, slot, itemTransmogrified);


    itemTransmogrified->UpdatePlayedTime(player);

    itemTransmogrified->SetOwnerGUID(player->GetGUID());
    itemTransmogrified->SetNotRefundable(player);
    itemTransmogrified->ClearSoulboundTradeable(player);
}

void Transmogrification::LoadPlayerSets(uint64 pGUID)
{
    for (presetData::iterator it = presetById[pGUID].begin(); it != presetById[pGUID].end(); ++it)
        it->second.clear();

    presetById[pGUID].clear();

    presetByName[pGUID].clear();

    QueryResult result = CharacterDatabase.Query("SELECT PresetID, SetName, SetData FROM _幻化保存 WHERE Owner = {}", ObjectGuid(pGUID).GetCounter());
    if (result)
    {
        do
        {
            uint8 PresetID = (*result)[0].Get<uint8>();
            std::string SetName = (*result)[1].Get<std::string>();
            std::istringstream SetData((*result)[2].Get<std::string>());
            while (SetData.good())
            {
                uint32 slot;
                uint32 entry;
                SetData >> slot >> entry;
                if (SetData.fail())
                    break;
                if (slot >= EQUIPMENT_SLOT_END)
                    continue;
                if (sObjectMgr->GetItemTemplate(entry))
                    presetById[pGUID][PresetID][slot] = entry;
            }

            if (!presetById[pGUID][PresetID].empty())
            {
                presetByName[pGUID][PresetID] = SetName;
            }
            else
            {
                presetById[pGUID].erase(PresetID);
                CharacterDatabase.Execute("DELETE FROM _幻化保存 WHERE Owner = {} AND PresetID = {}", ObjectGuid(pGUID).GetCounter(), PresetID);
            }
        } while (result->NextRow());
    }
}

bool Transmogrification::GetEnableSets() const
{
    return EnableSets;
}
uint8 Transmogrification::GetMaxSets() const
{
    return MaxSets;
}
float Transmogrification::GetSetCostModifier() const
{
    return SetCostModifier;
}
int32 Transmogrification::GetSetCopperCost() const
{
    return SetCopperCost;
}

void Transmogrification::UnloadPlayerSets(uint64 pGUID)
{
    for (presetData::iterator it = presetById[pGUID].begin(); it != presetById[pGUID].end(); ++it)
        it->second.clear();
    presetById[pGUID].clear();

    presetByName[pGUID].clear();
}
#endif

const char* Transmogrification::GetSlotName(uint8 slot, WorldSession* /*session*/) const
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD: return  "头部";
        case EQUIPMENT_SLOT_SHOULDERS: return  "肩膀";
        case EQUIPMENT_SLOT_BODY: return  "衬衣";
        case EQUIPMENT_SLOT_CHEST: return  "胸甲";
        case EQUIPMENT_SLOT_WAIST: return  "腰带";
        case EQUIPMENT_SLOT_LEGS: return  "腿部";
        case EQUIPMENT_SLOT_FEET: return  "鞋子";
        case EQUIPMENT_SLOT_WRISTS: return  "手腕";
        case EQUIPMENT_SLOT_HANDS: return  "手套";
        case EQUIPMENT_SLOT_BACK: return  "披风";
        case EQUIPMENT_SLOT_MAINHAND: return  "主手武器";
        case EQUIPMENT_SLOT_OFFHAND: return  "副手武器";
        case EQUIPMENT_SLOT_RANGED: return  "远程武器";
        case EQUIPMENT_SLOT_TABARD: return  "战袍";
        default: return NULL;
    }
}

std::string Transmogrification::GetItemIcon(uint32 entry, uint32 width, uint32 height, int x, int y) const
{
    std::ostringstream ss;
    ss << "|TInterface";
    const ItemTemplate* temp = sObjectMgr->GetItemTemplate(entry);
    const ItemDisplayInfoEntry* dispInfo = NULL;
    if (temp)
    {
        dispInfo = sItemDisplayInfoStore.LookupEntry(temp->DisplayInfoID);
        if (dispInfo)
            ss << "/ICONS/" << dispInfo->inventoryIcon;
    }
    if (!dispInfo)
        ss << "/InventoryItems/WoWUnknownItem01";
    ss << ":" << width << ":" << height << ":" << x << ":" << y << "|t";
    return ss.str();
}

std::string Transmogrification::GetSlotIcon(uint8 slot, uint32 width, uint32 height, int x, int y) const
{
    std::ostringstream ss;
    ss << "|TInterface/PaperDoll/";
    switch (slot)
    {
        case EQUIPMENT_SLOT_HEAD: ss << "UI-PaperDoll-Slot-Head"; break;
        case EQUIPMENT_SLOT_SHOULDERS: ss << "UI-PaperDoll-Slot-Shoulder"; break;
        case EQUIPMENT_SLOT_BODY: ss << "UI-PaperDoll-Slot-Shirt"; break;
        case EQUIPMENT_SLOT_CHEST: ss << "UI-PaperDoll-Slot-Chest"; break;
        case EQUIPMENT_SLOT_WAIST: ss << "UI-PaperDoll-Slot-Waist"; break;
        case EQUIPMENT_SLOT_LEGS: ss << "UI-PaperDoll-Slot-Legs"; break;
        case EQUIPMENT_SLOT_FEET: ss << "UI-PaperDoll-Slot-Feet"; break;
        case EQUIPMENT_SLOT_WRISTS: ss << "UI-PaperDoll-Slot-Wrists"; break;
        case EQUIPMENT_SLOT_HANDS: ss << "UI-PaperDoll-Slot-Hands"; break;
        case EQUIPMENT_SLOT_BACK: ss << "UI-PaperDoll-Slot-Chest"; break;
        case EQUIPMENT_SLOT_MAINHAND: ss << "UI-PaperDoll-Slot-MainHand"; break;
        case EQUIPMENT_SLOT_OFFHAND: ss << "UI-PaperDoll-Slot-SecondaryHand"; break;
        case EQUIPMENT_SLOT_RANGED: ss << "UI-PaperDoll-Slot-Ranged"; break;
        case EQUIPMENT_SLOT_TABARD: ss << "UI-PaperDoll-Slot-Tabard"; break;
        default: ss << "UI-Backpack-EmptySlot";
    }
    ss << ":" << width << ":" << height << ":" << x << ":" << y << "|t";
    return ss.str();
}

std::string Transmogrification::GetItemLink(Item* DoItem, WorldSession* Session) const
{
    if (!Session)
        return "";

    LocaleConstant Index = Session->GetSessionDbLocaleIndex();

    if (!DoItem)
        return "";

    if (!DoItem->GetOwner())
        return "";

    const ItemTemplate* Temp = DoItem->GetTemplate();
    if (!Temp)
        return "";

    std::string Name = Temp->Name1;

    if (ItemLocale const* Locale = sObjectMgr->GetItemLocale(Temp->ItemId))
        ObjectMgr::GetLocaleString(Locale->Name, Index, Name);

    if (int32 ItemRandPropId = DoItem->GetItemRandomPropertyId())
    {
        std::string DoName = "";
        if (ItemRandPropId < 0)
        {
            const ItemRandomSuffixEntry* SuffixEntry = sItemRandomSuffixStore.LookupEntry(-DoItem->GetItemRandomPropertyId());
            if (SuffixEntry)
                DoName = SuffixEntry->Name[LOCALE_zhCN];
        }
        else
        {
            const ItemRandomPropertiesEntry* PropertiesEntry = sItemRandomPropertiesStore.LookupEntry(DoItem->GetItemRandomPropertyId());
            if (PropertiesEntry)
                DoName = PropertiesEntry->Name[LOCALE_zhCN];
        }

        if (!DoName.empty())
            Name += DoName;
    }

    std::ostringstream Oss;
    Oss << "|c" << std::hex << ItemQualityColors[Temp->Quality] << std::dec
        << "|Hitem:" << Temp->ItemId << ":" <<
        DoItem->GetEnchantmentId(PERM_ENCHANTMENT_SLOT) << ":" <<
        DoItem->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT) << ":" <<
        DoItem->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_2) << ":" <<
        DoItem->GetEnchantmentId(SOCK_ENCHANTMENT_SLOT_3) << ":" <<
        DoItem->GetEnchantmentId(BONUS_ENCHANTMENT_SLOT) << ":" <<
        DoItem->GetItemRandomPropertyId() << ":" << DoItem->GetItemSuffixFactor() << ":" <<
        (uint32)DoItem->GetOwner()->GetLevel() << "|h[" << Name << "]|h|r";

    return Oss.str();
}

std::string Transmogrification::GetItemLink(uint32 entry, WorldSession* session) const
{
    const ItemTemplate* temp = sObjectMgr->GetItemTemplate(entry);
    LocaleConstant loc_idx = session->GetSessionDbLocaleIndex();
    std::string name = temp->Name1;
    if (ItemLocale const* il = sObjectMgr->GetItemLocale(entry))
        ObjectMgr::GetLocaleString(il->Name, loc_idx, name);

    std::ostringstream oss;
    oss << "|c" << std::hex << ItemQualityColors[temp->Quality] << std::dec <<
        "|Hitem:" << entry << ":0:0:0:0:0:0:0:0:0|h[" << name << "]|h|r";

    return oss.str();
}

uint32 Transmogrification::GetFakeEntry(uint64 itemGUID) const
{
    transmogData::const_iterator itr = dataMap.find(itemGUID);
    if (itr == dataMap.end()) return 0;
    transmogMap::const_iterator itr2 = entryMap.find(itr->second);
    if (itr2 == entryMap.end()) return 0;
    transmogData::const_iterator itr3 = itr2->second.find(itemGUID);
    if (itr3 == itr2->second.end()) return 0;
    return itr3->second;
}

void Transmogrification::UpdateItem(Player* player, Item* item) const
{
    if (item->IsEquipped())
    {
        player->SetVisibleItemSlot(item->GetSlot(), item);
        if (player->IsInWorld())
            item->SendUpdateToPlayer(player);
    }
}

void Transmogrification::DeleteFakeEntry(Player* player, uint8 /*slot*/, Item* itemTransmogrified, CharacterDatabaseTransaction* trans)
{
    DeleteFakeFromDB(itemTransmogrified->GetGUID().GetRawValue(), trans);
    UpdateItem(player, itemTransmogrified);
}

void Transmogrification::SetFakeEntry(Player* player, uint32 newEntry, uint8 /*slot*/, Item* itemTransmogrified)
{
    uint64 itemGUID = itemTransmogrified->GetGUID().GetRawValue();
    entryMap[player->GetGUID().GetRawValue()][itemGUID] = newEntry;
    dataMap[itemGUID] = player->GetGUID().GetRawValue();
    CharacterDatabase.Execute("REPLACE INTO _幻化 (GUID, FakeEntry, Owner) VALUES ({}, {}, {})", ObjectGuid(itemGUID).GetCounter(), newEntry, player->GetGUIDLow());
    UpdateItem(player, itemTransmogrified);
}

void Transmogrification::Transmogrify(Player* player, uint64 itemGUID, uint8 slot, /*uint32 newEntry, */bool no_cost)
{
    int32 cost = 0;
    if (slot >= EQUIPMENT_SLOT_END)
    {
        player->SendSysMsg("[幻化提示]: 装备栏是空的!!");
        return;
    }

    Item* itemTransmogrifier = NULL;
    if (itemGUID)
    {
        itemTransmogrifier = player->GetItemByGuid(ObjectGuid(itemGUID));
        if (!itemTransmogrifier)
        {
            player->SendSysMsg("[幻化提示]: 所要幻化的装备不存在!");
            return;
        }
    }

    // transmogrified item
    Item* itemTransmogrified = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!itemTransmogrified)
    {
        player->SendSysMsg("[幻化提示]: 幻化的装备不存在!");
        return;
    }

    if (!itemTransmogrifier)
    {
        DeleteFakeEntry(player, slot, itemTransmogrified);
    }
    else
    {
        if (!CanTransmogrifyItemWithItem(player, itemTransmogrified->GetTemplate(), itemTransmogrifier->GetTemplate()))
        {
            player->SendSysMsg("[幻化提示]: 所选择的的装备是无效的!");
            return;
        }

        if (!no_cost)
        {
            if (RequireToken)
            {
                if (player->HasItemCount(TokenEntry, TokenAmount))
                {
                    player->DestroyItemCount(TokenEntry, TokenAmount, true);
                }
                else
                {
                    player->SendSysMsg("[幻化提示]: 你没有足够的材料使用幻化功能!");
                    return;
                }
            }

            cost = GetSpecialPrice(itemTransmogrified->GetTemplate());
            cost *= ScaledCostModifier;
            cost += CopperCost;

            if (cost)
            {
                if (cost >= 0)
                {
                    if (!player->HasEnoughMoney(cost))
                    {
                        player->SendSysMsg("[幻化提示]: 你没有足够的金币使用幻化功能!");
                        return;
                    }
                    player->ModifyMoney(-cost, false);
                }
            }
        }

        SetFakeEntry(player, itemTransmogrifier->GetEntry(), slot, itemTransmogrified);

        itemTransmogrified->UpdatePlayedTime(player);

        itemTransmogrified->SetOwnerGUID(player->GetGUID());
        itemTransmogrified->SetNotRefundable(player);
        itemTransmogrified->ClearSoulboundTradeable(player);

        if (itemTransmogrifier->GetTemplate()->Bonding == BIND_WHEN_EQUIPED || itemTransmogrifier->GetTemplate()->Bonding == BIND_WHEN_USE)
        {
            itemTransmogrifier->UnBinded = false;
            itemTransmogrifier->SetBinding(true);
        }

        itemTransmogrifier->SetOwnerGUID(player->GetGUID());
        itemTransmogrifier->SetNotRefundable(player);
        itemTransmogrifier->ClearSoulboundTradeable(player);
    }
    player->ModifyMoney(-1 * cost, false);
    player->SendSysMsg("[幻化提示]: 装备幻化成功!");
}

bool Transmogrification::CanTransmogrifyItemWithItem(Player* player, ItemTemplate const* target, ItemTemplate const* source) const
{

    if (!target || !source)
    {
        return false;
    }

    if (source->ItemId == target->ItemId)
    {
        return false;
    }

    if (source->DisplayInfoID == target->DisplayInfoID)
    {
        return false;
    }

    if (source->Class != target->Class)
    {
        return false;
    }

    if (source->InventoryType == INVTYPE_BAG ||
        source->InventoryType == INVTYPE_RELIC ||
        source->InventoryType == INVTYPE_FINGER ||
        source->InventoryType == INVTYPE_TRINKET ||
        source->InventoryType == INVTYPE_AMMO ||
        source->InventoryType == INVTYPE_QUIVER)
    {
        return false;
    }

    if (target->InventoryType == INVTYPE_BAG ||
        target->InventoryType == INVTYPE_RELIC ||
        target->InventoryType == INVTYPE_FINGER ||
        target->InventoryType == INVTYPE_TRINKET ||
        target->InventoryType == INVTYPE_AMMO ||
        target->InventoryType == INVTYPE_QUIVER)
    {
        return false;
    }

    if (!SuitableForTransmogrification(player, target) || !SuitableForTransmogrification(player, source))
    {
        return false;
    }

    if (IsRangedWeapon(source->Class, source->SubClass) != IsRangedWeapon(target->Class, target->SubClass))
    {
        return false;
    }

    if (source->SubClass != target->SubClass && !IsRangedWeapon(target->Class, target->SubClass))
    {
        if (source->Class == ITEM_CLASS_ARMOR && !AllowMixedArmorTypes)
        {
            return false;
        }
        if (source->Class == ITEM_CLASS_WEAPON && !AllowMixedWeaponTypes)
        {
            return false;
        }
    }

    if (source->InventoryType != target->InventoryType)
    {
        if (source->Class == ITEM_CLASS_WEAPON && !(IsRangedWeapon(target->Class, target->SubClass) ||
            (
                (target->InventoryType == INVTYPE_WEAPON || target->InventoryType == INVTYPE_2HWEAPON || target->InventoryType == INVTYPE_WEAPONMAINHAND || target->InventoryType == INVTYPE_WEAPONOFFHAND)
                && (source->InventoryType == INVTYPE_WEAPON || source->InventoryType == INVTYPE_2HWEAPON || source->InventoryType == INVTYPE_WEAPONMAINHAND || source->InventoryType == INVTYPE_WEAPONOFFHAND)
            )
        ))
        {
            return false;
        }
        if (source->Class == ITEM_CLASS_ARMOR &&
            !((source->InventoryType == INVTYPE_CHEST || source->InventoryType == INVTYPE_ROBE) &&
                (target->InventoryType == INVTYPE_CHEST || target->InventoryType == INVTYPE_ROBE)))
        {
            return false;
        }
    }

    return true;
}

bool Transmogrification::SuitableForTransmogrification(Player* player, ItemTemplate const* proto) const
{
    if (!player || !proto)
    {
        return false;
    }

    if (proto->Class != ITEM_CLASS_ARMOR &&
        proto->Class != ITEM_CLASS_WEAPON)
    {
        return false;
    }

    if (IsAllowed(proto->ItemId))
    {
        return true;
    }

    if (IsNotAllowed(proto->ItemId))
    {
        return false;
    }

    if (!AllowFishingPoles && proto->Class == ITEM_CLASS_WEAPON && proto->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE)
    {
        return false;
    }

    if (!IsAllowedQuality(proto->Quality))
    {
        return false;
    }

    if (!IgnoreReqClass && (proto->AllowableClass & player->GetClassMask()) == 0)
    {
        return false;
    }

    if (!IgnoreReqRace && (proto->AllowableRace & player->GetClassMask()) == 0)
    {
        return false;
    }

    if (!IgnoreReqSkill && proto->RequiredSkill != 0)
    {
        if (player->GetSkillValue(proto->RequiredSkill) == 0)
        {
            return false;
        }
        else if (player->GetSkillValue(proto->RequiredSkill) < proto->RequiredSkillRank)
        {
            return false;
        }
    }

    if (!IgnoreReqSpell && proto->RequiredSpell != 0 && !player->HasSpell(proto->RequiredSpell))
    {
        return false;
    }

    if (!IgnoreReqLevel && player->GetLevel() < proto->RequiredLevel)
    {
        return false;
    }

    if (!IgnoreReqEvent && proto->HolidayId && !IsHolidayActive((HolidayIds)proto->HolidayId))
    {
        return false;
    }

    if (!IgnoreReqStats)
    {
        if (!proto->RandomProperty && !proto->RandomSuffix && proto->StatsCount > 0)
        {
            bool found = false;
            for (uint8 i = 0; i < proto->StatsCount; ++i)
            {
                if (proto->ItemStat[i].ItemStatValue != 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                return false;
            }
        }
    }

    return true;
}

uint32 Transmogrification::GetSpecialPrice(ItemTemplate const* proto) const
{
    uint32 cost = proto->SellPrice < 10000 ? 10000 : proto->SellPrice;
    return cost;
}
bool Transmogrification::IsRangedWeapon(uint32 Class, uint32 SubClass) const
{
    return Class == ITEM_CLASS_WEAPON && (
        SubClass == ITEM_SUBCLASS_WEAPON_BOW ||
        SubClass == ITEM_SUBCLASS_WEAPON_GUN ||
        SubClass == ITEM_SUBCLASS_WEAPON_CROSSBOW);
}

bool Transmogrification::IsAllowed(uint32 entry) const
{
    return Allowed.find(entry) != Allowed.end();
}

bool Transmogrification::IsNotAllowed(uint32 entry) const
{
    return NotAllowed.find(entry) != NotAllowed.end();
}

bool Transmogrification::IsAllowedQuality(uint32 quality) const
{
    switch (quality)
    {
        case ITEM_QUALITY_POOR: return AllowPoor;
        case ITEM_QUALITY_NORMAL: return AllowCommon;
        case ITEM_QUALITY_UNCOMMON: return AllowUncommon;
        case ITEM_QUALITY_RARE: return AllowRare;
        case ITEM_QUALITY_EPIC: return AllowEpic;
        case ITEM_QUALITY_LEGENDARY: return AllowLegendary;
        case ITEM_QUALITY_ARTIFACT: return AllowArtifact;
        case ITEM_QUALITY_HEIRLOOM: return AllowHeirloom;
        default: return false;
    }
}

void Transmogrification::LoadConfig(bool reload)
{
#ifdef PRESETS
    EnableSetInfo = sConfigMgr->GetBoolDefault("Transmogrification.EnableSetInfo", true);
    EnableSets = sConfigMgr->GetBoolDefault("Transmogrification.EnableSets", true);
    MaxSets = (uint8)sConfigMgr->GetIntDefault("Transmogrification.MaxSets", 10);
    SetCostModifier = sConfigMgr->GetFloatDefault("Transmogrification.SetCostModifier", 3.0f);
    SetCopperCost = sConfigMgr->GetIntDefault("Transmogrification.SetCopperCost", 0);

    if (MaxSets > MAX_OPTIONS)
        MaxSets = MAX_OPTIONS;

    if (reload)
    {
        SessionMap const& sessions = sWorld->GetAllSessions();
        for (SessionMap::const_iterator it = sessions.begin(); it != sessions.end(); ++it)
        {
            if (Player* player = it->second->GetPlayer())
            {
                UnloadPlayerSets(player->GetGUID().GetRawValue());
                if (GetEnableSets())
                    LoadPlayerSets(player->GetGUID().GetRawValue());
            }
        }
    }
#endif

    EnableTransmogInfo = sConfigMgr->GetBoolDefault("Transmogrification.EnableTransmogInfo", true);

    std::istringstream issAllowed(sConfigMgr->GetStringDefault("Transmogrification.Allowed", ""));
    std::istringstream issNotAllowed(sConfigMgr->GetStringDefault("Transmogrification.NotAllowed", ""));
    while (issAllowed.good())
    {
        uint32 entry;
        issAllowed >> entry;
        if (issAllowed.fail())
            break;
        Allowed.insert(entry);
    }
    while (issNotAllowed.good())
    {
        uint32 entry;
        issNotAllowed >> entry;
        if (issNotAllowed.fail())
            break;
        NotAllowed.insert(entry);
    }

    ScaledCostModifier = sConfigMgr->GetFloatDefault("Transmogrification.ScaledCostModifier", 1.0f);
    CopperCost = sConfigMgr->GetIntDefault("Transmogrification.CopperCost", 0);

    RequireToken = sConfigMgr->GetBoolDefault("Transmogrification.RequireToken", false);
    TokenEntry = uint32(sConfigMgr->GetIntDefault("Transmogrification.TokenEntry", 49426));
    TokenAmount = uint32(sConfigMgr->GetIntDefault("Transmogrification.TokenAmount", 1));

    AllowPoor = sConfigMgr->GetBoolDefault("Transmogrification.AllowPoor", false);
    AllowCommon = sConfigMgr->GetBoolDefault("Transmogrification.AllowCommon", false);
    AllowUncommon = sConfigMgr->GetBoolDefault("Transmogrification.AllowUncommon", true);
    AllowRare = sConfigMgr->GetBoolDefault("Transmogrification.AllowRare", true);
    AllowEpic = sConfigMgr->GetBoolDefault("Transmogrification.AllowEpic", true);
    AllowLegendary = sConfigMgr->GetBoolDefault("Transmogrification.AllowLegendary", false);
    AllowArtifact = sConfigMgr->GetBoolDefault("Transmogrification.AllowArtifact", false);
    AllowHeirloom = sConfigMgr->GetBoolDefault("Transmogrification.AllowHeirloom", true);

    AllowMixedArmorTypes = sConfigMgr->GetBoolDefault("Transmogrification.AllowMixedArmorTypes", false);
    AllowMixedWeaponTypes = sConfigMgr->GetBoolDefault("Transmogrification.AllowMixedWeaponTypes", false);
    AllowFishingPoles = sConfigMgr->GetBoolDefault("Transmogrification.AllowFishingPoles", false);

    IgnoreReqRace = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqRace", false);
    IgnoreReqClass = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqClass", false);
    IgnoreReqSkill = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqSkill", false);
    IgnoreReqSpell = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqSpell", false);
    IgnoreReqLevel = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqLevel", false);
    IgnoreReqEvent = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqEvent", false);
    IgnoreReqStats = sConfigMgr->GetBoolDefault("Transmogrification.IgnoreReqStats", false);

    if (!sObjectMgr->GetItemTemplate(TokenEntry))
    {
        TokenEntry = 49426;
    }
}

void Transmogrification::DeleteFakeFromDB(uint64 itemGUID, CharacterDatabaseTransaction* trans)
{
    if (dataMap.find(itemGUID) != dataMap.end())
    {
        if (entryMap.find(dataMap[itemGUID]) != entryMap.end())
            entryMap[dataMap[itemGUID]].erase(itemGUID);
        dataMap.erase(itemGUID);
    }
    if (trans)
        (*trans)->Append("DELETE FROM _幻化 WHERE GUID = {}", ObjectGuid(itemGUID).GetCounter());
    else
        CharacterDatabase.Execute("DELETE FROM _幻化 WHERE GUID = {}", ObjectGuid(itemGUID).GetCounter());
}

bool Transmogrification::GetEnableTransmogInfo() const
{
    return EnableTransmogInfo;
}

bool Transmogrification::GetEnableSetInfo() const
{
    return EnableSetInfo;
}

bool Transmogrification::IDTransmogrify(Player* player, ItemTemplate const* Source, uint8 Slot, uint32 NTempId)
{
    if (Slot >= EQUIPMENT_SLOT_END)
    {
        player->SendSysMsg("[幻化提示]: 装备栏是空的......");
        return false;
    }

    Item* itemTransmogrified = player->GetItemByPos(INVENTORY_SLOT_BAG_0, Slot);
    if (!itemTransmogrified)
    {
        player->SendSysMsg("[幻化提示]: 被幻化的装备不存在......");
        return false;
    }

    if (!itemTransmogrified->GetTemplate())
    {
        player->SendSysMsg("[幻化提示]: 被幻化的装备不存在......");
        return false;
    }

    if (!CanTransmogrifyItemWithItem(player, itemTransmogrified->GetTemplate(), Source))
    {
        player->SendSysMsg("[幻化提示]: 所选的装备是不能幻化的......");
        return false;
    }

    if (!sHelper->CheckNeed(player, NTempId, "幻化功能消耗"))
    {
        return false;
    }

    SetFakeEntry(player, Source->ItemId, Slot, itemTransmogrified);

    itemTransmogrified->UpdatePlayedTime(player);

    itemTransmogrified->SetOwnerGUID(player->GetGUID());
    itemTransmogrified->SetNotRefundable(player);
    itemTransmogrified->ClearSoulboundTradeable(player);

    player->SendSysMsg("[幻化提示]: 装备幻化成功");
    return true;
}

bool Transmogrification::IDCustomTransmogrify(Player* player, ItemTemplate const* Source, uint8 Slot, uint32 NTempId)
{
    if (Slot >= EQUIPMENT_SLOT_END)
    {
        player->SendSysMsg("[幻化提示]: 装备栏是空的......");
        return false;
    }

    Item* itemTransmogrified = player->GetItemByPos(INVENTORY_SLOT_BAG_0, Slot);
    if (!itemTransmogrified)
    {
        player->SendSysMsg("[幻化提示]: 被幻化的装备不存在......");
        return false;
    }

    if (!itemTransmogrified->GetTemplate())
    {
        player->SendSysMsg("[幻化提示]: 被幻化的装备不存在......");
        return false;
    }

    if (!CanTransmogrifyItemWithItem(player, itemTransmogrified->GetTemplate(), Source))
    {
        player->SendSysMsg("[幻化提示]: 所选的装备是不能幻化的......");
        return false;
    }

    if (sHelper->GetBoolean(6002) && !sHelper->CheckItemInBag(player, Source->ItemId))
    {
        player->SendSysMsg("[幻化提示]: 背包内必须存在改幻化对象");
        return false;
    }

    if (!sHelper->CheckNeed(player, NTempId, "幻化功能消耗"))
    {
        return false;
    }

    SetFakeEntry(player, Source->ItemId, Slot, itemTransmogrified);

    itemTransmogrified->UpdatePlayedTime(player);

    itemTransmogrified->SetOwnerGUID(player->GetGUID());
    itemTransmogrified->SetNotRefundable(player);
    itemTransmogrified->ClearSoulboundTradeable(player);

    player->SendSysMsg("[幻化提示]: 装备幻化成功");
    return true;
}

float Transmogrification::GetScaledCostModifier() const
{
    return ScaledCostModifier;
}
int32 Transmogrification::GetCopperCost() const
{
    return CopperCost;
}
bool Transmogrification::GetRequireToken() const
{
    return RequireToken;
}
uint32 Transmogrification::GetTokenEntry() const
{
    return TokenEntry;
}
uint32 Transmogrification::GetTokenAmount() const
{
    return TokenAmount;
}
bool Transmogrification::GetAllowMixedArmorTypes() const
{
    return AllowMixedArmorTypes;
};
bool Transmogrification::GetAllowMixedWeaponTypes() const
{
    return AllowMixedWeaponTypes;
};
