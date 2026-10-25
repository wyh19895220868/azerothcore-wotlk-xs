#pragma execution_character_set("utf-8")
#include "AntiGCDHackMgr.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "../SpellCooldownMgr/SpellCooldownMgr.h"
#include <chrono>

void AntiGCDHackMgr::Load()
{
    // 从配置表加载基础配置
    QueryResult result = WorldDatabase.Query(
        "SELECT 功能值 FROM __控制总表 WHERE 序号 = 22002");

    if (result)
    {
        std::string value = result->Fetch()[0].Get<std::string>();
        _enable = (value == "1" || value == "YES" || value == "TRUE" || value == "ON" || value == "是");
    }
    else
    {
        _enable = true;
    }

    // 加载每秒最大施法次数
    result = WorldDatabase.Query(
        "SELECT 功能值 FROM __控制总表 WHERE 序号 = 22003");

    if (result)
    {
        _maxCastsPerSecond = uint32(atoi(result->Fetch()[0].Get<std::string>().c_str()));
        if (_maxCastsPerSecond == 0)
            _maxCastsPerSecond = 5;
    }
    else
    {
        _maxCastsPerSecond = 5;  // 默认每秒5次
    }

    // 加载最小施法间隔
    result = WorldDatabase.Query(
        "SELECT 功能值 FROM __控制总表 WHERE 序号 = 22004");

    if (result)
    {
        _minCastInterval = uint32(atoi(result->Fetch()[0].Get<std::string>().c_str()));
        if (_minCastInterval == 0)
            _minCastInterval = 200;
    }
    else
    {
        _minCastInterval = 200;  // 默认200毫秒
    }

    LOG_INFO("server.worldserver", ">> 加载反GCD工具配置完成");
}

void AntiGCDHackMgr::Reload()
{
    Load();
}

bool AntiGCDHackMgr::CheckAndRecordSpellCast(Player* player, uint32 spellId)
{
    if (!_enable || !player)
        return true;

    // 检查是否在GCD白名单中
    if (sSpellCooldownMgr->IsInGCDWhitelist(spellId))
        return true; // 白名单中的法术不检查

    uint32 playerGuid = player->GetGUIDLow();
    uint32 currentTime = GetCurrentTimeMs();

    // 获取玩家施法历史
    auto& history = _playerCastHistory[playerGuid];

    // 清理1秒前的记录
    while (!history.empty() && (currentTime - history.front().CastTime) > 1000)
    {
        history.pop_front();
    }

    // 检查每秒施法次数
    if (history.size() >= _maxCastsPerSecond)
    {
        return false;
    }

    // 检查最小施法间隔
    if (!history.empty())
    {
        uint32 lastCastTime = history.back().CastTime;
        if ((currentTime - lastCastTime) < _minCastInterval)
        {
            return false;
        }
    }

    // 检查通过，记录施法（使用同一时间戳）
    PlayerCastRecord record;
    record.SpellId = spellId;
    record.CastTime = currentTime;
    history.push_back(record);

    return true;
}

uint32 AntiGCDHackMgr::GetCurrentTimeMs()
{
    using namespace std::chrono;
    return static_cast<uint32>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

// 脚本注册
class AntiGCDHackPlayerScript : public PlayerScript
{
public:
    AntiGCDHackPlayerScript() : PlayerScript("AntiGCDHackPlayerScript") {}

    void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!spell)
            return;

        uint32 spellId = spell->GetSpellInfo()->Id;

        // 检查是否是玩家主动施放的法术（排除系统赋予的光环等）
        if (spell->IsTriggered())
            return;

        // 检查施法频率并记录（如果检查通过）
        if (!sAntiGCDHackMgr->CheckAndRecordSpellCast(player, spellId))
        {
            player->GetSession()->SendNotification("施法太频繁");
            // 取消施法
            player->InterruptSpell(CURRENT_GENERIC_SPELL);
            return;
        }
    }
};

void AddSC_AntiGCDHackMgr()
{
    new AntiGCDHackPlayerScript();
}
