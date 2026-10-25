/*
 * Copyright (C) 2010 - 2016 Eluna Lua Engine <http://emudevs.com/>
 * This program is free software licensed under GPL version 3
 * Please see the included DOCS/LICENSE.md for more information
 */

#include "Hooks.h"
#include "HookHelpers.h"
#include "LuaEngine.h"
#include "BindingMap.h"
#include "ElunaEventMgr.h"
#include "ElunaIncludes.h"
#include "ElunaTemplate.h"
#include "ModuleFormat.h"

using namespace Hooks;

#define START_HOOK(EVENT) \
    if (!IsEnabled())\
        return;\
    auto key = EventKey<ServerEvents>(EVENT);\
    if (!ServerEventBindings->HasBindingsFor(key))\
        return;\
    LOCK_ELUNA

#define START_HOOK_WITH_RETVAL(EVENT, RETVAL) \
    if (!IsEnabled())\
        return RETVAL;\
    auto key = EventKey<ServerEvents>(EVENT);\
    if (!ServerEventBindings->HasBindingsFor(key))\
        return RETVAL;\
    LOCK_ELUNA

void Eluna::OnChatAddonMessage(Player* Sender, uint32 DType, const char* HMsg, const char* RMsg, Player* Receiver)
{
    START_HOOK(ADDON_EVENT_ON_MESSAGE);
    Push(Sender);
    Push(DType);
    Push(HMsg); // prefix
    if (RMsg)
        Push(RMsg); // msg
    else
        Push();

    if (Receiver)
        Push(Receiver);
    else
        Push();

    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnTimedEvent(int funcRef, uint32 delay, uint32 calls, WorldObject* obj)
{
    LOCK_ELUNA;
    ASSERT(!event_level);

    // Get function
    lua_rawgeti(L, LUA_REGISTRYINDEX, funcRef);

    // 检查是否成功获取到函数
    if (!lua_isfunction(L, -1))
    {
        ELUNA_LOG_ERROR(ModuleOldFormat("[Eluna]: OnTimedEvent failed - Invalid function reference: %s", funcRef).c_str());
        lua_pop(L, 1); // 移除非函数值
        return;
    }

    // Push parameters
    Push(L, funcRef);
    Push(L, delay);
    Push(L, calls);
    Push(L, obj);

    // Call function
    ExecuteCall(4, 0);

    ASSERT(!event_level);
    InvalidateObjects();
}

void Eluna::OnGameEventStart(uint32 eventid)
{
    START_HOOK(GAME_EVENT_START);
    Push(eventid);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnGameEventStop(uint32 eventid)
{
    START_HOOK(GAME_EVENT_STOP);
    Push(eventid);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnDamageLuaMeleeNow(Unit* Attacker, Unit* Victim, uint32& Damage)
{
    START_HOOK(WORLD_UNIT_MELEE_DAMAGE);
    Push(Attacker);
    Push(Victim);
    Push(Damage);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnDamageLuaSpellEnd(Unit* Attacker, Unit* Victim, uint32 CastId, uint32& Damage)
{
    START_HOOK(WORLD_UNIT_SEPLL_DAMAGE);
    Push(Attacker);
    Push(Victim);
    Push(CastId);
    Push(Damage);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnLuaStateClose()
{
    START_HOOK(ELUNA_EVENT_ON_LUA_STATE_CLOSE);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnLuaStateOpen()
{
    START_HOOK(ELUNA_EVENT_ON_LUA_STATE_OPEN);
    CallAllFunctions(ServerEventBindings, key);
}

// AreaTrigger
bool Eluna::OnAreaTrigger(Player* pPlayer, AreaTriggerEntry const* pTrigger)
{
    START_HOOK_WITH_RETVAL(TRIGGER_EVENT_ON_TRIGGER, false);
    Push(pPlayer);
#ifdef TRINITY
    Push(pTrigger->ID);
#elif AZEROTHCORE
    Push(pTrigger->entry);
#else
    Push(pTrigger->id);

#endif
    return CallAllFunctionsBool(ServerEventBindings, key);
}

// Weather
void Eluna::OnChange(Weather* /*weather*/, uint32 zone, WeatherState state, float grade)
{
    START_HOOK(WEATHER_EVENT_ON_CHANGE);
    Push(zone);
    Push(state);
    Push(grade);
    CallAllFunctions(ServerEventBindings, key);
}

// Auction House
void Eluna::OnAdd(AuctionHouseObject* /*ah*/, AuctionEntry* entry)
{
#ifdef AZEROTHCORE
    Player* owner = eObjectAccessor()FindPlayer(entry->owner);
#else
    Player* owner = eObjectAccessor()FindPlayer(MAKE_NEW_GUID(entry->owner, 0, HIGHGUID_PLAYER));
#endif

#ifdef TRINITY
    Item* item = eAuctionMgr->GetAItem(entry->itemGUIDLow);
    uint32 expiretime = entry->expire_time;
#elif AZEROTHCORE
    Item* item = eAuctionMgr->GetAItem(entry->item_guid);
    uint32 expiretime = entry->expire_time;
#else
    Item* item = eAuctionMgr->GetAItem(entry->itemGuidLow);
    uint32 expiretime = entry->expireTime;
#endif

    if (!owner || !item)
        return;

    START_HOOK(AUCTION_EVENT_ON_ADD);
    Push(entry->Id);
    Push(owner);
    Push(item);
    Push(expiretime);
    Push(entry->buyout);
    Push(entry->startbid);
    Push(entry->bid);
    Push(entry->bidder);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnRemove(AuctionHouseObject* /*ah*/, AuctionEntry* entry)
{
#ifdef AZEROTHCORE
    Player* owner = eObjectAccessor()FindPlayer(entry->owner);
#else
    Player* owner = eObjectAccessor()FindPlayer(MAKE_NEW_GUID(entry->owner, 0, HIGHGUID_PLAYER));
#endif

#ifdef TRINITY
    Item* item = eAuctionMgr->GetAItem(entry->itemGUIDLow);
    uint32 expiretime = entry->expire_time;
#elif AZEROTHCORE
    Item* item = eAuctionMgr->GetAItem(entry->item_guid);
    uint32 expiretime = entry->expire_time;
#else
    Item* item = eAuctionMgr->GetAItem(entry->itemGuidLow);
    uint32 expiretime = entry->expireTime;
#endif


    if (!owner || !item)
        return;

    START_HOOK(AUCTION_EVENT_ON_REMOVE);
    Push(entry->Id);
    Push(owner);
    Push(item);
    Push(expiretime);
    Push(entry->buyout);
    Push(entry->startbid);
    Push(entry->bid);
    Push(entry->bidder);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnSuccessful(AuctionHouseObject* /*ah*/, AuctionEntry* entry)
{
#ifdef AZEROTHCORE
    Player* owner = eObjectAccessor()FindPlayer(entry->owner);
#else
    Player* owner = eObjectAccessor()FindPlayer(MAKE_NEW_GUID(entry->owner, 0, HIGHGUID_PLAYER));
#endif

#ifdef TRINITY
    Item* item = eAuctionMgr->GetAItem(entry->itemGUIDLow);
    uint32 expiretime = entry->expire_time;
#elif AZEROTHCORE
    Item* item = eAuctionMgr->GetAItem(entry->item_guid);
    uint32 expiretime = entry->expire_time;
#else
    Item* item = eAuctionMgr->GetAItem(entry->itemGuidLow);
    uint32 expiretime = entry->expireTime;
#endif


    if (!owner || !item)
        return;

    START_HOOK(AUCTION_EVENT_ON_SUCCESSFUL);
    Push(entry->Id);
    Push(owner);
    Push(item);
    Push(expiretime);
    Push(entry->buyout);
    Push(entry->startbid);
    Push(entry->bid);
    Push(entry->bidder);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnExpire(AuctionHouseObject* /*ah*/, AuctionEntry* entry)
{
#ifdef AZEROTHCORE
    Player* owner = eObjectAccessor()FindPlayer(entry->owner);
#else
    Player* owner = eObjectAccessor()FindPlayer(MAKE_NEW_GUID(entry->owner, 0, HIGHGUID_PLAYER));
#endif

#ifdef TRINITY
    Item* item = eAuctionMgr->GetAItem(entry->itemGUIDLow);
    uint32 expiretime = entry->expire_time;
#elif AZEROTHCORE
    Item* item = eAuctionMgr->GetAItem(entry->item_guid);
    uint32 expiretime = entry->expire_time;
#else
    Item* item = eAuctionMgr->GetAItem(entry->itemGuidLow);
    uint32 expiretime = entry->expireTime;
#endif


    if (!owner || !item)
        return;

    START_HOOK(AUCTION_EVENT_ON_EXPIRE);
    Push(entry->Id);
    Push(owner);
    Push(item);
    Push(expiretime);
    Push(entry->buyout);
    Push(entry->startbid);
    Push(entry->bid);
    Push(entry->bidder);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnOpenStateChange(bool open)
{
    START_HOOK(WORLD_EVENT_ON_OPEN_STATE_CHANGE);
    Push(open);
    CallAllFunctions(ServerEventBindings, key);
}

#ifndef AZEROTHCORE
void Eluna::OnConfigLoad(bool reload)
#else
void Eluna::OnConfigLoad(bool reload, bool isBefore)
#endif
{
    START_HOOK(WORLD_EVENT_ON_CONFIG_LOAD);
    Push(reload);
#ifdef AZEROTHCORE
    Push(isBefore);
#endif
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnShutdownInitiate(ShutdownExitCode code, ShutdownMask mask)
{
    START_HOOK(WORLD_EVENT_ON_SHUTDOWN_INIT);
    Push(code);
    Push(mask);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnShutdownCancel()
{
    START_HOOK(WORLD_EVENT_ON_SHUTDOWN_CANCEL);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnWorldUpdate(uint32 diff)
{
    {
        LOCK_ELUNA;
        if (ShouldReload())
            _ReloadEluna();
    }

    eventMgr->globalProcessor->Update(diff);
    httpManager.HandleHttpResponses();

    START_HOOK(WORLD_EVENT_ON_UPDATE);
    Push(diff);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnStartup()
{
    START_HOOK(WORLD_EVENT_ON_STARTUP);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnShutdown()
{
    START_HOOK(WORLD_EVENT_ON_SHUTDOWN);
    CallAllFunctions(ServerEventBindings, key);
}

/* Map */
void Eluna::OnCreate(Map* map)
{
    START_HOOK(MAP_EVENT_ON_CREATE);
    Push(map);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnDestroy(Map* map)
{
    START_HOOK(MAP_EVENT_ON_DESTROY);
    Push(map);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnPlayerEnter(Map* map, Player* player)
{
    START_HOOK(MAP_EVENT_ON_PLAYER_ENTER);
    Push(map);
    Push(player);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnPlayerLeave(Map* map, Player* player)
{
    START_HOOK(MAP_EVENT_ON_PLAYER_LEAVE);
    Push(map);
    Push(player);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnUpdate(Map* map, uint32 diff)
{
    START_HOOK(MAP_EVENT_ON_UPDATE);
    // enable this for multithread
    // eventMgr->globalProcessor->Update(diff);
    Push(map);
    Push(diff);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnRemove(GameObject* gameobject)
{
    START_HOOK(WORLD_EVENT_ON_DELETE_GAMEOBJECT);
    Push(gameobject);
    CallAllFunctions(ServerEventBindings, key);
}

void Eluna::OnRemove(Creature* creature)
{
    START_HOOK(WORLD_EVENT_ON_DELETE_CREATURE);
    Push(creature);
    CallAllFunctions(ServerEventBindings, key);
}
