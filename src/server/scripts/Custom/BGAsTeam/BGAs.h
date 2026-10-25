#ifndef _BGAS_H
#define _BGAS_H

#include "Common.h"
#include "Battleground.h"
#include "BattlegroundQueue.h"
#include "Player.h"
#include "../_Helper/AsInt64.h"
#include <unordered_map>

enum FakeMorphs
{
    FAKE_M_FEL_ORC = 21267,
    FAKE_F_ORC = 20316,
    FAKE_M_DWARF = 20317,
    FAKE_M_NIGHT_ELF = 20318,
    FAKE_F_DRAENEI = 20323,
    FAKE_M_BROKEN_DRAENEI = 21105,
    FAKE_M_TROLL = 20321,
    FAKE_M_HUMAN = 19723,
    FAKE_F_HUMAN = 19724,
    FAKE_M_BLOOD_ELF = 20578,
    FAKE_F_BLOOD_ELF = 20579,
    FAKE_F_GNOME = 20320,
    FAKE_M_GNOME = 20580,
    FAKE_F_TAUREN = 20584,
    FAKE_M_TAUREN = 20585
};

enum BGUI
{
    CT_ON_WS = 10000,                //战歌
    CT_ON_AB = 11000,                //阿拉希
    CT_ON_AV = 12000,                //奥山
    CT_ON_EY = 13000,                //风暴之眼
    CT_ON_SA = 14000,                //远古海滩
    CT_ON_IC = 15000,                //征服之岛
};

struct FakePlayer
{
    uint8   FakeRace;
    uint32  FakeMorph;
    TeamId  FakeTeamID;
    uint8   RealRace;
    uint32  RealMorph;
    uint32  RealNativeMorph;
    TeamId  RealTeamID;
};

enum Factions
{
    FACTION_FROSTWOLF_CLAN = 729,
    FACTION_STORMPIKE_GUARD = 730
};

struct BGTemp
{
    AsInt64 Damage;
    AsInt64 Healing;
    uint32 WinTempId;
    uint32 LosTempId;
    uint32 WinKillCount;
    uint32 ResCount;
};
typedef std::unordered_map<uint32, BGTemp> BGTempMap;
extern BGTempMap BGTempStore;

class BGAs
{
public:
    static BGAs* Instance();

    // For config
    bool IsEnableSystem;
    bool IsEnableResetCooldowns;
    bool ShowPlayerName;

    void LoadConfig();

    void LoadBGConfig();

    uint32 GetAllPlayersCountInBG(Battleground* bg);

    TeamId GetLowerTeamIdInBG(Battleground* bg, GroupQueueInfo* groupInfo);

    bool IsPlayerFake(Player* player);
    bool IsPlayingNative(Player* player);

    void ValidatePlayerForBG(Battleground* bg, Player* player, TeamId teamId);
    void SetFakeRaceAndMorph(Player* player);
    void SetFactionForRace(Player* player, uint8 Race);
    void ClearFakePlayer(Player* player);
    void FitPlayerInTeam(Player* player, Battleground* bg);
    void DoForgetPlayersInBG(Player* player, Battleground* bg);
    void SendMessageQueue(BattlegroundQueue* bgQueue, Battleground* bg, PvPDifficultyEntry const* bracketEntry, Player* leader);

    bool FillPlayersToCFBG(BattlegroundQueue* bgqueue, Battleground* bg, BattlegroundBracketId bracket_id);
    bool CheckCrossFactionMatch(BattlegroundQueue* bgqueue, BattlegroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers);

    bool IsCustomBG(BattlegroundTypeId Id);
    bool IsCheckIpToBG(Battleground* BG, Player* player);
    uint32 GetWinKillCount(BattlegroundTypeId Id);
    AsInt64 GetCheckDmg(BattlegroundTypeId Id);
    AsInt64 GetCheckHe(BattlegroundTypeId Id);
    uint32 GetResCount(BattlegroundTypeId Id);

private:
    typedef std::unordered_map<Player*, FakePlayer> FakePlayersContainer;
    typedef std::unordered_map<Player*, ObjectGuid> FakeNamePlayersContainer;
    typedef std::unordered_map<Player*, bool> ForgetBGPlayersContainer;
    typedef std::unordered_map<Player*, bool> ForgetInListPlayersContainer;

    FakePlayersContainer _fakePlayerStore;
    FakeNamePlayersContainer _fakeNamePlayersStore;
    ForgetBGPlayersContainer _forgetBGPlayersStore;
    ForgetInListPlayersContainer _forgetInListPlayersStore;

    void RandomRaceMorph(uint8* race, uint32* morph, TeamId team, uint8 _class, uint8 gender);

    uint8 GetRandomRace(std::initializer_list<uint32> races);
    uint32 GetMorphFromRace(uint8 race, uint8 gender);
};

#define sBGAs BGAs::Instance()

#endif
