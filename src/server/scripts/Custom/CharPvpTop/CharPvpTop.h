#ifndef _CHARPVPTOP_H
#define _CHARPVPTOP_H

#include "Define.h"
#include "ObjectGuid.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

class Player;

#define CtTopMc 9999
#define BeginTime 10

struct SoloCustom
{
    uint32 Num;
    ObjectGuid AGuld;
    ObjectGuid BGuld;
};

struct SoloTopSys
{
    uint32 Num;
    uint32 ItemId;
    uint32 AuraId;
};

class CharPvpTop
{
public:
    CharPvpTop();
    ~CharPvpTop();

    static CharPvpTop* Instance()
    {
        static CharPvpTop Instance;
        return &Instance;
    }

    std::unordered_map<uint32, std::string> SoloPvpConfigStore;
    uint32 GetSoloPvpConfig(uint32 Index);

    std::map<uint32, SoloCustom> VCtopCustom;

    void AddTopTeam(uint32 Num, ObjectGuid GuldA, ObjectGuid GuldB)
    {
        SoloCustom St;
        St.Num = Num;
        St.AGuld = GuldA;
        St.BGuld = GuldB;
        VCtopCustom[Num] = St;
    }

    std::vector<ObjectGuid> SoloTopDataVect;
    std::vector<ObjectGuid> SoloTopMaxVect;

    void RollTopTeamNum();
    void UpdateCharTopData();
    void SengPVPgo(uint32 Num);

    void CheckSoloDuel();
    void PVPupdate();
    uint32 GetTopMax() const { return SoloTopMaxVect.size(); }
    void DeleteTopData(ObjectGuid Guid)
    {
        for (auto Iter = SoloTopMaxVect.begin(); Iter != SoloTopMaxVect.end();)
        {
            if (*Iter == Guid)
                Iter = SoloTopMaxVect.erase(Iter);
            else
                Iter++;
        }
    }

    bool IsInSolo(ObjectGuid Guid)
    {
        auto Itr = std::find(SoloTopMaxVect.begin(), SoloTopMaxVect.end(), Guid);
        return Itr != SoloTopMaxVect.end();
    }

    bool TopEvent;
    uint32 PvpCount;
    bool TeleStarted;
    time_t SoloNextTime;

    std::map<uint32, SoloTopSys> VCTopSys;
    const SoloTopSys* FindTopSys(uint32 Num)
    {
        auto Itr = VCTopSys.find(Num);
        return (Itr != VCTopSys.end() ? &Itr->second : nullptr);
    }

    std::vector<uint32> AsSoloAuras;
    void AddTopAuraToAllPlayer();
    void SendAndClearTopAura(Player * player);
    std::map<uint32, ObjectGuid> SoloPlayerTop;
    void LoadTopSys();
    uint32 GetRewItemId(uint32 Num);
    uint32 GetRewAuraId(uint32 Num);

    void SendAsUpdateWorldState(uint32 AtValue);

private:

};
#define sCharPvpTop CharPvpTop::Instance()

#endif
