#ifndef _ANTIGCDHACKMGR_H
#define _ANTIGCDHACKMGR_H

#include "Common.h"
#include <unordered_map>
#include <deque>

class Player;

// 玩家施法记录
struct PlayerCastRecord
{
    uint32 SpellId;
    uint32 CastTime;  // 施法时间戳(毫秒)
};

class AntiGCDHackMgr
{
public:
    static AntiGCDHackMgr* Instance()
    {
        static AntiGCDHackMgr Instance;
        return &Instance;
    }

    void Load();
    void Reload();

    // 检查施法是否合法，如果合法则自动记录（返回true表示检查通过）
    bool CheckAndRecordSpellCast(Player* player, uint32 spellId);

private:
    bool _enable;
    uint32 _maxCastsPerSecond;  // 每秒最大施法次数
    uint32 _minCastInterval;    // 最小施法间隔(毫秒)

    // 玩家施法历史
    std::unordered_map<uint32, std::deque<PlayerCastRecord>> _playerCastHistory;

    uint32 GetCurrentTimeMs();
};

#define sAntiGCDHackMgr AntiGCDHackMgr::Instance()

#endif // !_ANTIGCDHACKMGR_H
