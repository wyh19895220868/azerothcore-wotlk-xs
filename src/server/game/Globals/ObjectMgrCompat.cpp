/*
 * Compatibility implementations for ObjectMgr APIs used by this source tree.
 * These loaders mirror the matching AzerothCore implementation and remain in a
 * separate translation unit so the existing ObjectMgr.cpp stays isolated.
 */

#include "ObjectMgr.h"
#include "Containers.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Log.h"
#include "Player.h"
#include "PoolMgr.h"
#include "SpellMgr.h"
#include "StringConvert.h"
#include "Tokenize.h"
#include "World.h"
#include <boost/algorithm/string.hpp>

bool ObjectMgr::LoadModuleStrings()
{
    uint32 oldMSTime = getMSTime();

    _moduleStringStore.clear(); // for reload case
    QueryResult result = WorldDatabase.Query("SELECT module, id, string FROM module_string");
    if (!result)
    {
        LOG_WARN("server.loading", ">> Loaded 0 module strings. DB table `module_string` is empty.");
        LOG_INFO("server.loading", " ");
        return false;
    }

    do
    {
        Field* fields = result->Fetch();

        std::string module = fields[0].Get<std::string>();
        uint32 id          = fields[1].Get<uint32>();

        std::pair<std::string, uint32> pairKey = std::make_pair(module, id);
        ModuleString& data = _moduleStringStore[pairKey];

        AddLocaleString(fields[2].Get<std::string>(), LOCALE_enUS, data.Content);
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} Module Strings in {} ms", _moduleStringStore.size(), GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");

    return true;
}

bool ObjectMgr::LoadModuleStringsLocale()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT module, id, locale, string FROM module_string_locale");
    if (!result)
    {
        LOG_WARN("server.loading", ">> Loaded 0 module strings locale. DB table `module_string_locale` is empty.");
        LOG_INFO("server.loading", " ");
        return false;
    }

    uint32 localeCount = 0;
    do
    {
        Field* fields = result->Fetch();

        std::string module = fields[0].Get<std::string>();
        uint32 id          = fields[1].Get<uint32>();

        std::pair<std::string, uint32> pairKey = std::make_pair(module, id);
        ModuleString& data = _moduleStringStore[pairKey];

        ModuleStringContainer::iterator ms = _moduleStringStore.find(pairKey);
        if (ms == _moduleStringStore.end())
        {
            LOG_ERROR("sql.sql", "ModuleString (Module: {} Id: {}) found in table `module_string_locale` but does not exist in `module_string`. Skipped!", module, id);
            continue;
        }

        LocaleConstant locale = GetLocaleByName(fields[2].Get<std::string>());
        if (locale == LOCALE_enUS)
            continue;

        AddLocaleString(fields[3].Get<std::string>(), locale, data.Content);
        localeCount++;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} Module Strings Locales in {} ms", localeCount, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");

    return true;
}

void ObjectMgr::LoadPlayerTotemModels()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT TotemID, RaceID, ModelID from player_totem_model");

    if (!result)
    {
        LOG_INFO("server.loading", ">> Loaded 0 player totem model records. DB table `player_totem_model` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        SummonSlot totemSlot = SummonSlot(fields[0].Get<uint8>());
        uint8 race = fields[1].Get<uint8>();
        uint32 displayId = fields[2].Get<uint32>();

        if (totemSlot < SUMMON_SLOT_TOTEM_FIRE || totemSlot >= MAX_TOTEM_SLOT)
        {
            LOG_ERROR("sql.sql", "Wrong TotemSlot {} in `player_totem_model` table, skipped.", totemSlot);
            continue;
        }

        ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race);
        if (!raceEntry)
        {
            LOG_ERROR("sql.sql", "Race {} defined in `player_totem_model` does not exists, skipped.", uint32(race));
            continue;
        }

        CreatureDisplayInfoEntry const* displayEntry = sCreatureDisplayInfoStore.LookupEntry(displayId);
        if (!displayEntry)
        {
            LOG_ERROR("sql.sql", "TotemSlot: {} defined in `player_totem_model` has non-existing model ({}), skipped.", totemSlot, displayId);
            continue;
        }

        _playerTotemModel[std::make_pair(totemSlot, Races(race))] = displayId;
        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} player totem model records in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

uint32 ObjectMgr::GetModelForTotem(SummonSlot totemSlot, Races race) const
{
    auto itr = _playerTotemModel.find(std::make_pair(totemSlot, race));
    if (itr != _playerTotemModel.end())
        return itr->second;

    LOG_ERROR("misc", "TotemSlot {} with RaceID ({}) have no totem model data defined, set to default model.", totemSlot, race);
    return 0;
}

void ObjectMgr::LoadPlayerShapeshiftModels()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT ShapeshiftID, RaceID, CustomizationID, GenderID, ModelID from player_shapeshift_model");

    if (!result)
    {
        LOG_INFO("server.loading", ">> Loaded 0 player shapeshift model records. DB table `player_shapeshift_model` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        ShapeshiftForm shapeshiftForm = ShapeshiftForm(fields[0].Get<uint8>());
        uint8 race = fields[1].Get<uint8>();
        uint8 customizationID = fields[2].Get<uint8>();
        uint8 genderID = Gender(fields[3].Get<uint8>());
        uint32 modelId = fields[4].Get<uint32>();

        ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race);
        if (!raceEntry)
        {
            LOG_ERROR("sql.sql", "Race {} defined in `player_shapeshift_model` does not exists, skipped.", uint32(race));
            continue;
        }

        CreatureDisplayInfoEntry const* displayEntry = sCreatureDisplayInfoStore.LookupEntry(modelId);
        if (!displayEntry)
        {
            LOG_ERROR("sql.sql", "ShapeshiftForm: {}, Race: {} defined in `player_shapeshift_model` has non-existing model ({}), skipped.", shapeshiftForm, race, modelId);
            continue;
        }

        _playerShapeshiftModel[std::make_tuple(shapeshiftForm, race, customizationID, genderID)] = modelId;
        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} player totem model records in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

uint32 ObjectMgr::GetModelForShapeshift(ShapeshiftForm form, Player* player) const
{
    uint8 customizationID;

    if (player->GetTeamId() == TEAM_ALLIANCE)
        customizationID = player->GetByteValue(PLAYER_BYTES, 3); // Use Hair Color
    else
        customizationID = player->GetByteValue(PLAYER_BYTES, 0); // Use Skin Color

    // getGender() tracks the active display model; real gender lives in PLAYER_BYTES_3
    uint8 gender = player->GetByteValue(PLAYER_BYTES_3, PLAYER_BYTES_3_OFFSET_GENDER);

    auto itr = _playerShapeshiftModel.find(std::make_tuple(form, player->getRace(), customizationID, gender));
    if (itr != _playerShapeshiftModel.end())
        return itr->second; // Explicit combination

    itr = _playerShapeshiftModel.find(std::make_tuple(form, player->getRace(), customizationID, GENDER_NONE));
    if (itr != _playerShapeshiftModel.end())
        return itr->second; // Combination applied to both genders

    itr = _playerShapeshiftModel.find(std::make_tuple(form, player->getRace(), 255, gender));
    if (itr != _playerShapeshiftModel.end())
        return itr->second; // Default gender-dependent model

    itr = _playerShapeshiftModel.find(std::make_tuple(form, player->getRace(), 255, GENDER_NONE));
    if (itr != _playerShapeshiftModel.end())
        return itr->second; // Last resort

    LOG_DEBUG("entities.player", "ShapeshiftForm {} with RaceID ({}) have no shapeshift model data defined, using fallback data.", form, player->getRace());
    return 0;
}

void ObjectMgr::LoadPetNamesLocales()
{
    uint32 oldMSTime = getMSTime();

    //                                                  0     1      2    3
    QueryResult result = WorldDatabase.Query("SELECT Locale, Word, Entry, Half FROM pet_name_generation_locale");

    if (!result)
    {
        LOG_WARN("server.loading", ">> Loaded 0 pet name locales parts. DB table `pet_name_generation_locale` is empty!");
        LOG_INFO("server.loading", " ");
        return;
    }

    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();
        LocaleConstant locale = GetLocaleByName(fields[0].Get<std::string>());
        std::string word = fields[1].Get<std::string>();

        uint32 entry = fields[2].Get<uint32>();
        bool half = fields[3].Get<bool>();
        std::pair<uint32, LocaleConstant> pairkey = std::make_pair(entry, locale);
        if (half)
        {
            _petHalfLocaleName1[pairkey].push_back(word);
        }
        else
        {
            _petHalfLocaleName0[pairkey].push_back(word);
        }
        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} Pet Name Locales Parts in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

void ObjectMgr::LoadCreatureCustomIDs()
{
    // Hack for modules
    std::string stringCreatureIds = sConfigMgr->GetOption<std::string>("Creatures.CustomIDs", "190010,55005,999991,25462,98888,601014,34567,34568");
    std::vector<std::string_view> CustomCreatures = Acore::Tokenize(stringCreatureIds, ',', false);

    for (auto& itr : CustomCreatures)
    {
        _creatureCustomIDsStore.push_back(Acore::StringTo<uint32>(itr).value());
    }
}

void ObjectMgr::LoadSpawnGroupTemplates()
{
    uint32 oldMSTime = getMSTime();

    _spawnGroupDataStore.clear();

    //                                               0        1          2
    QueryResult result = WorldDatabase.Query("SELECT groupId, groupName, groupFlags FROM spawn_group_template");

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 groupId = fields[0].Get<uint32>();
            SpawnGroupTemplateData& group = _spawnGroupDataStore[groupId];
            group.groupId = groupId;
            group.name = fields[1].Get<std::string>();
            group.mapId = SPAWNGROUP_MAP_UNSET;
            uint32 flags = fields[2].Get<uint32>();
            if (flags & ~uint32(SPAWNGROUP_FLAG_ALL))
            {
                flags &= uint32(SPAWNGROUP_FLAG_ALL);
                LOG_ERROR("sql.sql", "Invalid spawn group flag {} on group ID {} ({}), reduced to valid flags {}.",
                    fields[2].Get<uint32>(), groupId, group.name, flags);
            }
            if ((flags & SPAWNGROUP_FLAG_SYSTEM) && (flags & SPAWNGROUP_FLAG_MANUAL_SPAWN))
            {
                flags &= ~SPAWNGROUP_FLAG_MANUAL_SPAWN;
                LOG_ERROR("sql.sql", "System spawn group {} ({}) has invalid manual spawn flag. Ignored.", groupId, group.name);
            }
            group.flags = SpawnGroupFlags(flags);
        } while (result->NextRow());
    }

    if (_spawnGroupDataStore.find(0) == _spawnGroupDataStore.end())
    {
        LOG_ERROR("sql.sql", "Default spawn group (index 0) is missing from DB! Manually inserted.");
        SpawnGroupTemplateData& data = _spawnGroupDataStore[0];
        data.groupId = 0;
        data.name = "Default Group";
        data.mapId = SPAWNGROUP_MAP_UNSET;
        data.flags = SpawnGroupFlags(SPAWNGROUP_FLAG_SYSTEM);
    }
    if (_spawnGroupDataStore.find(1) == _spawnGroupDataStore.end())
    {
        LOG_ERROR("sql.sql", "Default legacy spawn group (index 1) is missing from DB! Manually inserted.");
        SpawnGroupTemplateData& data = _spawnGroupDataStore[1];
        data.groupId = 1;
        data.name = "Legacy Group";
        data.mapId = SPAWNGROUP_MAP_UNSET;
        data.flags = SpawnGroupFlags(SPAWNGROUP_FLAG_SYSTEM | SPAWNGROUP_FLAG_COMPATIBILITY_MODE);
    }

    LOG_INFO("server.loading", ">> Loaded {} spawn group templates in {} ms", _spawnGroupDataStore.size(), GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

void ObjectMgr::LoadCreatureSparring()
{
    uint32 oldMSTime = getMSTime();

    QueryResult result = WorldDatabase.Query("SELECT GUID, SparringPCT FROM creature_sparring");

    if (!result)
    {
        LOG_WARN("server.loading", ">> Loaded 0 sparring data. DB table `creature_sparring` is empty.");
        LOG_INFO("server.loading", " ");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        ObjectGuid::LowType spawnId     = fields[0].Get<uint32>();
        float sparringHealthPct         = fields[1].Get<float>();

        if (!GetCreatureData(spawnId))
        {
            LOG_ERROR("sql.sql", "Entry {} has a record in `creature_sparring` but doesn't exist in `creatures` table");
            continue;
        }

        _creatureSparringStore[spawnId].push_back(sparringHealthPct);

        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} sparring data in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

void ObjectMgr::LoadGameObjectSummons()
{
    uint32 oldMSTime = getMSTime();

    _goSummonDataStore.clear();

    //                                                    0           1             2        3      4           5           6           7            8          9          10         11           12
    QueryResult result = WorldDatabase.Query("SELECT summonerId, summonerType, groupId, entry, position_x, position_y, position_z, orientation, rotation0, rotation1, rotation2, rotation3, respawnTime FROM gameobject_summon_groups");

    if (!result)
    {
        LOG_WARN("server.loading", ">> Loaded 0 gameobject summons. DB table `gameobject_summon_groups` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 summonerId               = fields[0].Get<uint32>();
        SummonerType summonerType       = SummonerType(fields[1].Get<uint8>());
        uint8 group                     = fields[2].Get<uint8>();

        switch (summonerType)
        {
            case SUMMONER_TYPE_CREATURE:
                if (!GetCreatureTemplate(summonerId))
                {
                    LOG_ERROR("sql.sql", "Table `gameobject_summon_groups` has summoner with non existing entry {} for creature summoner type, skipped.", summonerId);
                    continue;
                }
                break;
            case SUMMONER_TYPE_GAMEOBJECT:
                if (!GetGameObjectTemplate(summonerId))
                {
                    LOG_ERROR("sql.sql", "Table `gameobject_summon_groups` has summoner with non existing entry {} for gameobject summoner type, skipped.", summonerId);
                    continue;
                }
                break;
            case SUMMONER_TYPE_MAP:
                if (!sMapStore.LookupEntry(summonerId))
                {
                    LOG_ERROR("sql.sql", "Table `gameobject_summon_groups` has summoner with non existing entry {} for map summoner type, skipped.", summonerId);
                    continue;
                }
                break;
            default:
                LOG_ERROR("sql.sql", "Table `gameobject_summon_groups` has unhandled summoner type {} for summoner {}, skipped.", summonerType, summonerId);
                continue;
        }

        GameObjectSummonData data;
        data.entry                      = fields[3].Get<uint32>();

        if (!GetGameObjectTemplate(data.entry))
        {
            LOG_ERROR("sql.sql", "Table `gameobject_summon_groups` has gameobject in group [Summoner ID: {}, Summoner Type: {}, Group ID: {}] with non existing gameobject entry {}, skipped.", summonerId, summonerType, group, data.entry);
            continue;
        }

        float posX                      = fields[4].Get<float>();
        float posY                      = fields[5].Get<float>();
        float posZ                      = fields[6].Get<float>();
        float orientation               = fields[7].Get<float>();

        data.pos.Relocate(posX, posY, posZ, orientation);

        data.rot                        = G3D::Quat(fields[8].Get<float>(), fields[9].Get<float>(), fields[10].Get<float>(), fields[11].Get<float>());
        data.respawnTime                = fields[12].Get<uint32>();

        TempSummonGroupKey key(summonerId, summonerType, group);
        _goSummonDataStore[key].push_back(data);

        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} Gameobject Summons in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

void ObjectMgr::LoadSpawnGroups()
{
    uint32 oldMSTime = getMSTime();

    // Reset prior state for hot-reload support
    // Preserve the forced legacy group for spawns on transport maps (set in LoadCreatures/LoadGameobjects).
    _spawnGroupMapStore.clear();
    for (auto& [id, data] : _creatureDataStore)
        data.spawnGroupId = _transportMaps.count(data.mapid) ? 1 : 0;
    for (auto& [id, data] : _gameObjectDataStore)
        data.spawnGroupId = _transportMaps.count(data.mapid) ? 1 : 0;

    //                                               0        1          2
    QueryResult result = WorldDatabase.Query("SELECT groupId, spawnType, spawnId FROM spawn_group");

    if (!result)
    {
        LOG_INFO("server.loading", ">> Loaded 0 spawn group members. DB table `spawn_group` is empty.");
        LOG_INFO("server.loading", " ");
        return;
    }

    uint32 numMembers = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 groupId = fields[0].Get<uint32>();
        uint32 type = fields[1].Get<uint8>();
        if (type >= SPAWN_TYPE_MAX)
        {
            LOG_ERROR("sql.sql", "Spawn data with invalid type {} listed for spawn group {}. Skipped.", type, groupId);
            continue;
        }
        SpawnObjectType spawnType = SpawnObjectType(type);
        ObjectGuid::LowType spawnId = fields[2].Get<uint32>();

        SpawnData const* data = GetSpawnData(spawnType, spawnId);
        if (!data)
        {
            LOG_ERROR("sql.sql", "Spawn data with ID ({},{}) not found, but is listed as a member of spawn group {}!",
                uint32(spawnType), spawnId, groupId);
            continue;
        }
        if (data->spawnGroupId)
        {
            LOG_ERROR("sql.sql", "Spawn with ID ({},{}) is listed as a member of spawn group {}, but is already a member of spawn group {}. Skipping.",
                uint32(spawnType), spawnId, groupId, data->spawnGroupId);
            continue;
        }

        auto it = _spawnGroupDataStore.find(groupId);
        if (it == _spawnGroupDataStore.end())
        {
            LOG_ERROR("sql.sql", "Spawn group {} assigned to spawn ID ({},{}), but group is not found!", groupId, uint32(spawnType), spawnId);
            continue;
        }

        SpawnGroupTemplateData& groupTemplate = it->second;
        if (groupTemplate.mapId == SPAWNGROUP_MAP_UNSET)
            groupTemplate.mapId = data->mapid;
        else if (groupTemplate.mapId != data->mapid && !(groupTemplate.flags & SPAWNGROUP_FLAG_SYSTEM))
        {
            LOG_ERROR("sql.sql", "Spawn group {} has map ID {}, but spawn ({},{}) has map id {} - spawn NOT added to group!",
                groupId, groupTemplate.mapId, uint32(spawnType), spawnId, data->mapid);
            continue;
        }

        // Warn if spawn is also in a pool (non-system groups and pools are mutually exclusive)
        if (!(groupTemplate.flags & SPAWNGROUP_FLAG_SYSTEM))
        {
            uint32 poolId = 0;
            if (spawnType == SPAWN_TYPE_CREATURE)
                poolId = sPoolMgr->IsPartOfAPool<Creature>(spawnId);
            else if (spawnType == SPAWN_TYPE_GAMEOBJECT)
                poolId = sPoolMgr->IsPartOfAPool<GameObject>(spawnId);

            if (poolId)
                LOG_WARN("sql.sql", "Spawn ({},{}) is a member of spawn group {} and also part of pool {}. This may cause issues!",
                    uint32(spawnType), spawnId, groupId, poolId);
        }

        const_cast<SpawnData*>(data)->spawnGroupId = groupId;
        if (!(groupTemplate.flags & SPAWNGROUP_FLAG_SYSTEM))
            _spawnGroupMapStore.emplace(groupId, data);
        ++numMembers;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} spawn group members in {} ms", numMembers, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

void ObjectMgr::LoadVehicleSeatAddon()
{
    uint32 oldMSTime = getMSTime();

    _vehicleSeatAddonStore.clear();                           // needed for reload case

    uint32 count = 0;

    //                                                0            1                  2             3             4             5             6
    QueryResult result = WorldDatabase.Query("SELECT `SeatEntry`, `SeatOrientation`, `ExitParamX`, `ExitParamY`, `ExitParamZ`, `ExitParamO`, `ExitParamValue` FROM `vehicle_seat_addon`");

    if (!result)
    {
        LOG_ERROR("server.loading", ">> Loaded 0 vehicle seat addons. DB table `vehicle_seat_addon` is empty.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        uint32 seatID = fields[0].Get<uint32>();
        float orientation = fields[1].Get<float>();
        float exitX = fields[2].Get<float>();
        float exitY = fields[3].Get<float>();
        float exitZ = fields[4].Get<float>();
        float exitO = fields[5].Get<float>();
        uint8 exitParam = fields[6].Get<uint8>();

        if (!sVehicleSeatStore.LookupEntry(seatID))
        {
            LOG_ERROR("sql.sql", "Table `vehicle_seat_addon`: SeatID: {} does not exist in VehicleSeat.dbc. Skipping entry.", seatID);
            continue;
        }

        // Sanitizing values
        if (orientation > float(M_PI * 2))
        {
            LOG_ERROR("sql.sql", "Table `vehicle_seat_addon`: SeatID: {} is using invalid angle offset value ({}). Set Value to 0.", seatID, orientation);
            orientation = 0.0f;
        }

        if (exitParam >= AsUnderlyingType(VehicleExitParameters::VehicleExitParamMax))
        {
            LOG_ERROR("sql.sql", "Table `vehicle_seat_addon`: SeatID: {} is using invalid exit parameter value ({}). Setting to 0 (none).", seatID, exitParam);
            continue;
        }

        _vehicleSeatAddonStore[seatID] = VehicleSeatAddon(orientation, exitX, exitY, exitZ, exitO, exitParam);

        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} Vehicle Seat Addon entries in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadReservedPlayerNamesDB()
{
    uint32 oldMSTime = getMSTime();

    _reservedNamesStore.clear();                                // need for reload case

    QueryResult result = CharacterDatabase.Query("SELECT name FROM reserved_name");

    if (!result)
    {
        LOG_WARN("server.loading", ">> Loaded 0 reserved names. DB table `reserved_name` is empty!");
        return;
    }

    uint32 count = 0;

    Field* fields;
    do
    {
        fields = result->Fetch();
        std::string name = fields[0].Get<std::string>();

        std::wstring wstr;
        if (!Utf8toWStr (name, wstr))
        {
            LOG_ERROR("sql.sql", "Table `reserved_name` have invalid name: {}", name);
            continue;
        }

        wstrToLower(wstr);

        _reservedNamesStore.insert(wstr);
        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} reserved names from DB in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadReservedPlayerNamesDBC()
{
    if (!sWorld->getBoolConfig(CONFIG_STRICT_NAMES_RESERVED))
    {
        LOG_WARN("server.loading", ">> Loaded 0 reserved names from DBC. Config option disabled.");
        return;
    }

    uint32 oldMSTime = getMSTime();

    uint32 count = 0;

    for (NamesReservedEntry const* reservedStore : sNamesReservedStore)
    {
        std::wstring wstr;

        Utf8toWStr(reservedStore->Pattern, wstr);

        // DBC does not have clean entries, remove the junk.
        boost::algorithm::replace_all(wstr, "\\<", "");
        boost::algorithm::replace_all(wstr, "\\>", "");

        _reservedNamesStore.insert(wstr);
        count++;
    }

    LOG_INFO("server.loading", ">> Loaded {} reserved names from DBC in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

void ObjectMgr::LoadProfanityNamesFromDB()
{
    uint32 oldMSTime = getMSTime();

    _profanityNamesStore.clear();                                // need for reload case

    QueryResult result = CharacterDatabase.Query("SELECT name FROM profanity_name");

    if (!result)
    {
        LOG_WARN("server.loading", ">> Loaded 0 profanity names. DB table `profanity_name` is empty!");
        return;
    }

    uint32 count = 0;

    Field* fields;
    do
    {
        fields = result->Fetch();
        std::string name = fields[0].Get<std::string>();

        std::wstring wstr;
        if (!Utf8toWStr (name, wstr))
        {
            LOG_ERROR("sql.sql", "Table `profanity_name` have invalid name: {}", name);
            continue;
        }

        wstrToLower(wstr);

        _profanityNamesStore.insert(wstr);
        ++count;
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> Loaded {} profanity names from DB in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadProfanityNamesFromDBC()
{
    if (!sWorld->getBoolConfig(CONFIG_STRICT_NAMES_PROFANITY))
    {
        LOG_WARN("server.loading", ">> Loaded 0 profanity names from DBC. Config option disabled.");
        return;
    }

    uint32 oldMSTime = getMSTime();

    uint32 count = 0;

    for (NamesProfanityEntry const* profanityStore : sNamesProfanityStore)
    {
        std::wstring wstr;

        Utf8toWStr(profanityStore->Pattern, wstr);

        // DBC does not have clean entries, remove the junk.
        boost::algorithm::replace_all(wstr, "\\<", "");
        boost::algorithm::replace_all(wstr, "\\>", "");

        _profanityNamesStore.insert(wstr);
        count++;
    }

    LOG_INFO("server.loading", ">> Loaded {} profanity names from DBC in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

void ObjectMgr::LoadChatFilter()
{
    uint32 oldMSTime = getMSTime();

    _chatFilterAutomaton.reset();                               // need for reload case

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAT_FILTER);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    if (!result)
    {
        LOG_WARN("server.loading", ">> Loaded 0 chat filter words. DB table `chat_filter` is empty!");
        LOG_INFO("server.loading", " ");
        return;
    }

    auto automaton = std::make_unique<Acore::AhoCorasick<wchar_t>>();
    uint32 count = 0;

    do
    {
        Field* fields = result->Fetch();
        std::string word = fields[1].Get<std::string>();

        if (word.empty())
            continue;

        std::wstring wstr;
        if (!Utf8toWStr(word, wstr))
        {
            LOG_ERROR("sql.sql", "Table `chat_filter` has invalid word: {}", word);
            continue;
        }

        wstrToLower(wstr);

        if (wstr.empty())
            continue;

        automaton->Insert(wstr);
        ++count;
    } while (result->NextRow());

    if (count > 0)
    {
        automaton->Build();
        _chatFilterAutomaton = std::move(automaton);
    }

    LOG_INFO("server.loading", ">> Loaded {} chat filter words in {} ms", count, GetMSTimeDiffToNow(oldMSTime));
    LOG_INFO("server.loading", " ");
}

void ObjectMgr::LoadTrainers()
{
    uint32 oldMSTime = getMSTime();

    // For reload case
    _trainers.clear();
    _classTrainers.clear();

    std::unordered_map<int32, std::vector<Trainer::Spell>> spellsByTrainer;
    if (QueryResult trainerSpellsResult = WorldDatabase.Query("SELECT TrainerId, SpellId, MoneyCost, ReqSkillLine, ReqSkillRank, ReqAbility1, ReqAbility2, ReqAbility3, ReqLevel FROM trainer_spell"))
    {
        do
        {
            Field* fields = trainerSpellsResult->Fetch();

            Trainer::Spell spell;
            uint32 trainerId = fields[0].Get<uint32>();
            spell.SpellId = fields[1].Get<uint32>();
            spell.MoneyCost = fields[2].Get<uint32>();
            spell.ReqSkillLine = fields[3].Get<uint32>();
            spell.ReqSkillRank = fields[4].Get<uint32>();
            spell.ReqAbility[0] = fields[5].Get<uint32>();
            spell.ReqAbility[1] = fields[6].Get<uint32>();
            spell.ReqAbility[2] = fields[7].Get<uint32>();
            spell.ReqLevel = fields[8].Get<uint8>();

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spell.SpellId);
            if (!spellInfo)
            {
                LOG_ERROR("sql.sql", "Table `trainer_spell` references non-existing spell (SpellId: {}) for TrainerId {}, ignoring", spell.SpellId, trainerId);
                continue;
            }

            if (GetTalentSpellCost(spell.SpellId))
            {
                LOG_ERROR("sql.sql", "Table `trainer_spell` references non-existing spell (SpellId: {}) which is a talent, for TrainerId {}, ignoring", spell.SpellId, trainerId);
                continue;
            }

            if (spell.ReqSkillLine && !sSkillLineStore.LookupEntry(spell.ReqSkillLine))
            {
                LOG_ERROR("sql.sql", "Table `trainer_spell` references non-existing skill (ReqSkillLine: {}) for TrainerId {} and SpellId {}, ignoring",
                    spell.ReqSkillLine, spell.SpellId, trainerId);
                continue;
            }

            bool allReqValid = true;
            for (std::size_t i = 0; i < spell.ReqAbility.size(); ++i)
            {
                uint32 requiredSpell = spell.ReqAbility[i];
                if (requiredSpell && !sSpellMgr->GetSpellInfo(requiredSpell))
                {
                    LOG_ERROR("sql.sql", "Table `trainer_spell` references non-existing spell (ReqAbility {} : {}) for TrainerId {} and SpellId {}, ignoring",
                        i + 1, requiredSpell, trainerId, spell.SpellId);
                    allReqValid = false;
                }
            }

            if (!allReqValid)
                continue;

            spellsByTrainer[trainerId].push_back(spell);
        } while (trainerSpellsResult->NextRow());
    }

    if (QueryResult trainersResult = WorldDatabase.Query("SELECT Id, Type, Requirement, Greeting FROM trainer"))
    {
        do
        {
            Field* fields = trainersResult->Fetch();

            uint32 trainerId = fields[0].Get<uint32>();
            Trainer::Type trainerType = Trainer::Type(fields[1].Get<uint8>());
            uint32 requirement = fields[2].Get<uint32>();
            std::string greeting = fields[3].Get<std::string>();
            std::vector<Trainer::Spell> spells;
            auto spellsItr = spellsByTrainer.find(trainerId);
            if (spellsItr != spellsByTrainer.end())
            {
                spells = std::move(spellsItr->second);
                spellsByTrainer.erase(spellsItr);
            }

            auto [it, isNew] = _trainers.emplace(std::piecewise_construct, std::forward_as_tuple(trainerId), std::forward_as_tuple(trainerId, trainerType, requirement, std::move(greeting), std::move(spells)));
            ASSERT(isNew);
            if (trainerType == Trainer::Type::Class)
            {
                if (!requirement || requirement >= MAX_CLASSES)
                    LOG_ERROR("sql.sql", "Table `trainer` has invalid class requirement for trainer {}, ignoring");
                else
                {
                    uint8 classId = static_cast<uint8>(requirement);
                    _classTrainers[classId].push_back(&it->second);
                }
            }
        } while (trainersResult->NextRow());
    }

    for (auto const& unusedSpells : spellsByTrainer)
    {
        for (Trainer::Spell const& unusedSpell : unusedSpells.second)
        {
            LOG_ERROR("sql.sql", "Table `trainer_spell` references non-existing trainer (TrainerId: {}) for SpellId {}, ignoring", unusedSpells.first, unusedSpell.SpellId);
        }
    }

    if (QueryResult trainerLocalesResult = WorldDatabase.Query("SELECT Id, locale, Greeting_lang FROM trainer_locale"))
    {
        do
        {
            Field* fields = trainerLocalesResult->Fetch();
            uint32 trainerId = fields[0].Get<uint32>();
            std::string localeName = fields[1].Get<std::string>();

            LocaleConstant locale = GetLocaleByName(localeName);
            if (locale == LOCALE_enUS)
                continue;

            if (Trainer::Trainer* trainer = Acore::Containers::MapGetValuePtr(_trainers, trainerId))
                trainer->AddGreetingLocale(locale, fields[2].Get<std::string>());
            else
                LOG_ERROR("sql.sql", "Table `trainer_locale` references non-existing trainer (TrainerId: {}) for locale %s, ignoring",
                    trainerId, localeName.c_str());
        } while (trainerLocalesResult->NextRow());
    }

    LOG_INFO("server.loading", ">> Loaded {} Trainers in {} ms", _trainers.size(), GetMSTimeDiffToNow(oldMSTime));
}

void ObjectMgr::LoadCreatureDefaultTrainers()
{
    uint32 oldMSTime = getMSTime();

    _creatureDefaultTrainers.clear();

    if (QueryResult result = WorldDatabase.Query("SELECT CreatureId, TrainerId FROM creature_default_trainer"))
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 creatureId = fields[0].Get<uint32>();
            uint32 trainerId = fields[1].Get<uint32>();

            if (!GetCreatureTemplate(creatureId))
            {
                LOG_ERROR("sql.sql", "Table `creature_default_trainer` references non-existing creature template (CreatureId: %u), ignoring", creatureId);
                continue;
            }

            _creatureDefaultTrainers[creatureId] = trainerId;

        } while (result->NextRow());
    }

    LOG_INFO("server.loading", ">> Loaded {} default trainers in {} ms", _creatureDefaultTrainers.size(), GetMSTimeDiffToNow(oldMSTime));
}

SpawnData const* ObjectMgr::GetSpawnData(SpawnObjectType type, ObjectGuid::LowType spawnId) const
{
    switch (type)
    {
        case SPAWN_TYPE_CREATURE:
            return GetCreatureData(spawnId);
        case SPAWN_TYPE_GAMEOBJECT:
            return GetGameObjectData(spawnId);
        default:
            return nullptr;
    }
}

std::string const* ObjectMgr::GetModuleString(std::string module, uint32 id, LocaleConstant locale) const
{
    ModuleString const* moduleString = GetModuleString(module, id);
    if (moduleString && !moduleString->Content.empty())
    {
        if (moduleString->Content.size() > size_t(locale) && !moduleString->Content[locale].empty())
            return &moduleString->Content[locale];

        return &moduleString->Content[DEFAULT_LOCALE];
    }

    LOG_ERROR("sql.sql", "Module string module {} id {} not found in DB.", module, id);
    return (std::string*)"error";
}

Trainer::Trainer* ObjectMgr::GetTrainer(uint32 creatureId)
{
    auto itr = _creatureDefaultTrainers.find(creatureId);
    if (itr != _creatureDefaultTrainers.end())
        return Acore::Containers::MapGetValuePtr(_trainers, itr->second);

    return nullptr;
}
