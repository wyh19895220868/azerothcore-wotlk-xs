#ifndef _SPELLCOOLDOWNMGR_H
#define _SPELLCOOLDOWNMGR_H

#include "Common.h"
#include <unordered_set>

class SpellCooldownMgr
{
public:
    static SpellCooldownMgr* Instance()
    {
        static SpellCooldownMgr Instance;
        return &Instance;
    }

    void Load();
    void Reload();

    // 检查法术是否在GCD白名单中
    bool IsInGCDWhitelist(uint32 spellId);

private:
    std::unordered_set<uint32> _gcdWhitelist;
};

#define sSpellCooldownMgr SpellCooldownMgr::Instance()

#endif // !_SPELLCOOLDOWNMGR_H
