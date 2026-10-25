
#ifndef _NPC_BEAST_MASTER_H_
#define _NPC_BEAST_MASTER_H_

#include "Common.h"
#include <map>

class Player;
class Creature;

using PetsStore = std::map<std::string, uint32>;

typedef std::map<uint32, std::string> PetCustomMap;
extern PetCustomMap PetCustomStore;

class NpcBeastmaster
{
    NpcBeastmaster() = default;
    ~NpcBeastmaster() = default;

    NpcBeastmaster(NpcBeastmaster const&) = delete;
    NpcBeastmaster(NpcBeastmaster&&) = delete;
    NpcBeastmaster& operator= (NpcBeastmaster const&) = delete;
    NpcBeastmaster& operator= (NpcBeastmaster&&) = delete;

public:
    static NpcBeastmaster* Instance();

    void LoadSystem();
    std::string GetStrConfig(uint32 Index);
    uint32 GetUInt(uint32 Index);
    float GetFloat(uint32 Index);
    bool GetBoolean(uint32 Index);

    void ShowMainMenu(Player* player, Creature* creature);
    void GossipSelect(Player* player, Creature* creature, uint32 action);

    void PlayerUpdate(Player* player, uint32 p_time);

private:
    void CreatePet(Player* player, Creature* creature, uint32 action);
    void AddPetsToGossip(Player* player, PetsStore const& petsStore, uint32 page);
};

#define sNpcBeastMaster NpcBeastmaster::Instance()

#endif // _NPC_BEAST_MASTER_H_
