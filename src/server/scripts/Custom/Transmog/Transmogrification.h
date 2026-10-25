#ifndef DEF_TRANSMOGRIFICATION_H
#define DEF_TRANSMOGRIFICATION_H

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "Define.h"
#include "DatabaseEnv.h"
#include "Language.h"
#include "Config.h"

#define PRESETS
#define MAX_OPTIONS 25

class Item;
class Player;
class WorldSession;
struct ItemTemplate;

class Transmogrification
{
public:
    static Transmogrification* Instance()
    {
        static Transmogrification Instance;
        return &Instance;
    }

    typedef std::unordered_map<uint64, uint64> transmogData;
    typedef std::unordered_map<uint64, transmogData> transmogMap;
    transmogMap entryMap;
    transmogData dataMap;

#ifdef PRESETS
    bool EnableSetInfo;

    typedef std::map<uint8, uint32> slotMap;
    typedef std::map<uint8, slotMap> presetData;
    typedef std::unordered_map<uint64, presetData> presetDataMap;
    presetDataMap presetById;
    typedef std::map<uint8, std::string> presetIdMap;
    typedef std::unordered_map<uint64, presetIdMap> presetNameMap;
    presetNameMap presetByName;

    void PresetTransmog(Player* player, Item* itemTransmogrified, uint32 fakeEntry, uint8 slot);

    bool EnableSets;
    uint8 MaxSets;
    float SetCostModifier;
    int32 SetCopperCost;

    bool GetEnableSets() const;
    uint8 GetMaxSets() const;
    float GetSetCostModifier() const;
    int32 GetSetCopperCost() const;

    void LoadPlayerSets(uint64 pGUID);
    void UnloadPlayerSets(uint64 pGUID);
#endif

    bool EnableTransmogInfo;

    std::set<uint32> Allowed;
    std::set<uint32> NotAllowed;

    float ScaledCostModifier;
    int32 CopperCost;

    bool RequireToken;
    uint32 TokenEntry;
    uint32 TokenAmount;

    bool AllowPoor;
    bool AllowCommon;
    bool AllowUncommon;
    bool AllowRare;
    bool AllowEpic;
    bool AllowLegendary;
    bool AllowArtifact;
    bool AllowHeirloom;

    bool AllowMixedArmorTypes;
    bool AllowMixedWeaponTypes;
    bool AllowFishingPoles;

    bool IgnoreReqRace;
    bool IgnoreReqClass;
    bool IgnoreReqSkill;
    bool IgnoreReqSpell;
    bool IgnoreReqLevel;
    bool IgnoreReqEvent;
    bool IgnoreReqStats;

    bool IsAllowed(uint32 entry) const;
    bool IsNotAllowed(uint32 entry) const;
    bool IsAllowedQuality(uint32 quality) const;
    bool IsRangedWeapon(uint32 Class, uint32 SubClass) const;

    void LoadConfig(bool reload); // thread unsafe

    std::string GetItemIcon(uint32 entry, uint32 width, uint32 height, int x, int y) const;
    std::string GetSlotIcon(uint8 slot, uint32 width, uint32 height, int x, int y) const;
    const char * GetSlotName(uint8 slot, WorldSession* session) const;
    std::string GetItemLink(Item* DoItem, WorldSession* Session) const;
    std::string GetItemLink(uint32 entry, WorldSession* session) const;
    uint32 GetFakeEntry(uint64 itemGUID) const;
    void UpdateItem(Player* player, Item* item) const;
    void DeleteFakeEntry(Player* player, uint8 slot, Item* itemTransmogrified, CharacterDatabaseTransaction* trans = nullptr);
    void SetFakeEntry(Player* player, uint32 newEntry, uint8 slot, Item* itemTransmogrified);

    void Transmogrify(Player* player, uint64 itemGUID, uint8 slot, /*uint32 newEntry, */bool no_cost = false);
    bool CanTransmogrifyItemWithItem(Player* player, ItemTemplate const* destination, ItemTemplate const* source) const;
    bool SuitableForTransmogrification(Player* player, ItemTemplate const* proto) const;

    uint32 GetSpecialPrice(ItemTemplate const* proto) const;

    void DeleteFakeFromDB(uint64 itemGUID, CharacterDatabaseTransaction* trans = nullptr);
    float GetScaledCostModifier() const;
    int32 GetCopperCost() const;

    bool GetRequireToken() const;
    uint32 GetTokenEntry() const;
    uint32 GetTokenAmount() const;

    bool GetAllowMixedArmorTypes() const;
    bool GetAllowMixedWeaponTypes() const;

    // Config
    bool GetEnableTransmogInfo() const;
    bool GetEnableSetInfo() const;

    bool IDTransmogrify(Player* player, ItemTemplate const* Source, uint8 Slot, uint32 NTempId);
    bool IDCustomTransmogrify(Player* player, ItemTemplate const* Source, uint8 Slot, uint32 NTempId);
};
#define sTransmogrification Transmogrification::Instance()

#endif
