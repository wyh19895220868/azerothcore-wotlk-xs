#ifndef XS_PLAYERBOT_COMPAT_H
#define XS_PLAYERBOT_COMPAT_H

#include "Player.h"
#include "WorldSession.h"

// New playerbots use headless sessions; no legacy fake-player runtime is linked.
inline bool IsXsPlayerbot(Player const* player)
{
    return player && player->GetSession() && player->GetSession()->IsHeadless();
}

#endif
