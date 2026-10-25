#ifndef FROZENCORE_MODULE_BRIDGE_H
#define FROZENCORE_MODULE_BRIDGE_H

#include "Define.h"
#include <string>

class Player;

struct FrozenCoreModuleData
{
    bool hasData = false;
    std::string baseAttributes;
    std::string additionalAttributes;
    std::string identificationDisplayData;
    std::string growthData;
    std::string enhancementData;
    std::string skillsData;
    std::string runeData;
    std::string setData;
    std::string huanjingData;
    std::string pendingIdentifyData;
    std::string templateStatsData;
};

bool FrozenCoreQueryItemModuleData(uint32 itemId, uint32 guidLow, FrozenCoreModuleData& out);
void FrozenCoreSendItemModuleData(Player* player, uint32 itemId, uint32 guidLow);

#endif
