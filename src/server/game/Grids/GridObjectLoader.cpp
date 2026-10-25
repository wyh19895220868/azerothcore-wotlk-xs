/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "GridObjectLoader.h"
#include "CellImpl.h"
#include "Corpse.h"
#include "Creature.h"
#include "DynamicObject.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "ObjectMgr.h"
#include "PoolMgr.h"
#include "Transport.h"
#include "Vehicle.h"

template <class T>
void GridObjectLoader::AddObjectHelper(Map* map, T* obj)
{
    CellCoord cellCoord = Acore::ComputeCellCoord(obj->GetPositionX(), obj->GetPositionY());
    Cell cell(cellCoord);

    map->AddToGrid<T>(obj, cell);
    obj->AddToWorld();
}

void GridObjectLoader::LoadCreatures(CellGuidSet const& guid_set, Map* map)
{
    for (ObjectGuid::LowType const& guid : guid_set)
    {
        CreatureData const* cData = sObjectMgr->GetCreatureData(guid);
        // Skip pool members this map's pool state has not rolled as spawned
        if (cData && sPoolMgr->IsPartOfAPool<Creature>(guid) && !map->GetPoolData().IsSpawnedObject<Creature>(guid))
            continue;

        Creature* obj = new Creature();
        if (!obj->LoadFromDB(guid, map))
        {
            delete obj;
            continue;
        }

        AddObjectHelper<Creature>(map, obj);

        // This fork keeps only "active" objects in the map update list
        // (WorldObject::IsUpdateNeeded() returns isActiveObject()), and the grid
        // loader bypasses Map::AddToMap. Without marking the spawn active here a
        // freshly loaded creature is dropped from the update list on the first
        // recheck: it never ticks, so it never patrols, its AI never runs and its
        // motion generator never starts (it only wakes up when something puts it
        // in combat, which calls setActive(true)).
        obj->setActive(true);

        // Grid load bypasses Map::AddToMap, so seat accessories here.
        if (Vehicle* vehicle = obj->GetVehicleKit())
            vehicle->Reset();

        if (!obj->IsMoveInLineOfSightDisabled() && obj->GetDefaultMovementType() == IDLE_MOTION_TYPE && !obj->isNeedNotify(NOTIFY_VISIBILITY_CHANGED | NOTIFY_AI_RELOCATION))
        {
            if (obj->IsAlive() && !obj->HasUnitState(UNIT_STATE_SIGHTLESS) && obj->HasReactState(REACT_AGGRESSIVE) && !obj->IsImmuneToNPC())
            {
                // call MoveInLineOfSight for nearby grid creatures
                Acore::AIRelocationNotifier notifier(*obj);
                Cell::VisitObjects(obj, notifier, 60.f);
            }
        }
    }
}

void GridObjectLoader::LoadGameObjects(CellGuidSet const& guid_set, Map* map)
{
    for (ObjectGuid::LowType const& guid : guid_set)
    {
        GameObjectData const* data = sObjectMgr->GetGameObjectData(guid);

        // Skip spawns whose spawn group is not active on this map
        if (data && !map->IsSpawnGroupActive(data->spawnGroupId))
            continue;

        // Skip pool members this map's pool state has not rolled as spawned
        if (data && data->poolId && !map->GetPoolData().IsSpawnedObject<GameObject>(guid))
            continue;

        if (data && sObjectMgr->IsGameObjectStaticTransport(data->id))
        {
            StaticTransport* transport = new StaticTransport();

            // Special case for static transports - we are loaded via grids
            // but we do not want to actually be stored in the grid
            if (!transport->LoadGameObjectFromDB(guid, map, true))
                delete transport;
        }
        else
        {
            GameObject* obj = new GameObject();

            if (!obj->LoadFromDB(guid, map))
            {
                delete obj;
                continue;
            }

            AddObjectHelper<GameObject>(map, obj);
        }
    }
}

void GridObjectLoader::LoadAllCellsInGrid()
{
    // 335XS fix: the spawn index (_mapObjectGuidsStore) is keyed by CELL id
    // (Acore::ComputeCellCoord().GetId()), but this loader used the GRID id
    // (y * MAX_NUMBER_OF_GRIDS + x). The two numbering schemes differ, so the
    // lookup always missed and no creature was ever spawned into a grid.
    uint32 loadedGos = 0;
    uint32 loadedCreatures = 0;

    for (uint16 cellX = 0; cellX < MAX_NUMBER_OF_CELLS; ++cellX)
    {
        for (uint16 cellY = 0; cellY < MAX_NUMBER_OF_CELLS; ++cellY)
        {
            CellCoord cellCoord(_grid.GetX() * MAX_NUMBER_OF_CELLS + cellX, _grid.GetY() * MAX_NUMBER_OF_CELLS + cellY);
            CellObjectGuids const& cell_guids = sObjectMgr->GetGridObjectGuids(_map->GetId(), _map->GetSpawnMode(), cellCoord.GetId());

            loadedGos += uint32(cell_guids.gameobjects.size());
            loadedCreatures += uint32(cell_guids.creatures.size());

            LoadGameObjects(cell_guids.gameobjects, _map);
            LoadCreatures(cell_guids.creatures, _map);
        }
    }

    LOG_INFO("visdiag", "grid {} map {} go={} creatures={}", _grid.GetId(), _map->GetId(), loadedGos, loadedCreatures);

    if (std::unordered_set<Corpse*> const* corpses = _map->GetCorpsesInGrid(_grid.GetId()))
    {
        for (Corpse* corpse : *corpses)
        {
            if (corpse->IsInGrid())
                continue;

            AddObjectHelper<Corpse>(_map, corpse);
        }
    }
}

template<class T>
void GridObjectUnloader::Visit(GridRefMgr<T>& m)
{
    while (!m.IsEmpty())
    {
        T* obj = m.getFirst()->GetSource();
        // if option set then object already saved at this moment
        //if (!sWorld->getBoolConfig(CONFIG_SAVE_RESPAWN_TIME_IMMEDIATELY))
        //    obj->SaveRespawnTime();
        //Some creatures may summon other temp summons in CleanupsBeforeDelete()
        //So we need this even after cleaner (maybe we can remove cleaner)
        //Example: Flame Leviathan Turret 33139 is summoned when a creature is deleted
        //TODO: Check if that script has the correct logic. Do we really need to summons something before deleting?
        obj->CleanupsBeforeDelete();

        obj->GetMap()->RemoveObjectFromMapUpdateList(obj);

        ///- object will get delinked from the manager when deleted
        delete obj;
    }
}

template<class T>
void GridObjectCleaner::Visit(GridRefMgr<T>& m)
{
    for (typename GridRefMgr<T>::iterator iter = m.begin(); iter != m.end(); ++iter)
        iter->GetSource()->CleanupsBeforeDelete();
}

template void GridObjectUnloader::Visit(CreatureMapType&);
template void GridObjectUnloader::Visit(GameObjectMapType&);
template void GridObjectUnloader::Visit(DynamicObjectMapType&);

template void GridObjectCleaner::Visit(CreatureMapType&);
template void GridObjectCleaner::Visit<GameObject>(GameObjectMapType&);
template void GridObjectCleaner::Visit<DynamicObject>(DynamicObjectMapType&);
template void GridObjectCleaner::Visit<Corpse>(CorpseMapType&);
