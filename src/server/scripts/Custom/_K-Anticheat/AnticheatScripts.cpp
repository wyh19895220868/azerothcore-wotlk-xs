/*
 *MIT License
 *
 *Copyright (c) 2023 Azerothcore
 *
 *Permission is hereby granted, free of charge, to any person obtaining a copy
 *of this software and associated documentation files (the "Software"), to deal
 *in the Software without restriction, including without limitation the rights
 *to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *copies of the Software, and to permit persons to whom the Software is
 *furnished to do so, subject to the following conditions:
 *
 *The above copyright notice and this permission notice shall be included in all
 *copies or substantial portions of the Software.
 *
 *THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *SOFTWARE.
 */

#include "Config.h"
#include "AnticheatMgr.h"
#include "Object.h"
#include "AccountMgr.h"
#include "Chat.h"
#include "Player.h"
#include "Timer.h"
#include "GameTime.h"

Seconds resetTime = 0s;

class AnticheatPlayerScript : public PlayerScript
{
public:
    AnticheatPlayerScript() : PlayerScript("AnticheatPlayerScript") { }

    void OnLogout(Player* player) override
    {
        sAnticheatMgr->HandlePlayerLogout(player);
    }

    void OnLogin(Player* player) override
    {
        sAnticheatMgr->HandlePlayerLogin(player);
    }

    void OnUpdate(Player* player, uint32 diff) override
    {
            sAnticheatMgr->AckUpdate(player, diff);
    }

    void AnticheatSetSkipOnePacketForASH(Player* player, bool apply) override
    {
        sAnticheatMgr->SetSkipNextPacket(player, apply);
    }
};

class AnticheatWorldScript : public WorldScript
{
public:
    AnticheatWorldScript() : WorldScript("AnticheatWorldScript") { }

    void OnUpdate(uint32 /* diff */) override // unusued parameter
    {
        if (GameTime::GetGameTime() > resetTime)
        {
            sAnticheatMgr->ResetDailyReportStates();
            UpdateReportResetTime();
        }
    }

    void UpdateReportResetTime()
    {
        resetTime = Seconds(Acore::Time::GetNextTimeWithDayAndHour(-1, 6));
    }
};

class AnticheatMovementHandlerScript : public MovementHandlerScript
{
public:
    AnticheatMovementHandlerScript() : MovementHandlerScript("AnticheatMovementHandlerScript") { }

    void OnPlayerMove(Player* player, MovementInfo mi, uint32 opcode) override
    {
        sAnticheatMgr->StartHackDetection(player, mi, opcode);
    }
};

void AddSC_AnticheatScripts()
{
    new AnticheatWorldScript();
    new AnticheatPlayerScript();
    new AnticheatMovementHandlerScript();
}
