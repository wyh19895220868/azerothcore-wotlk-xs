#pragma execution_character_set("utf-8")
#include "SpellCooldownMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "DatabaseEnv.h"
#include "Log.h"

void SpellCooldownMgr::Load()
{
    _gcdWhitelist.clear();

    // 从配置表加载无GCD检查白名单
    QueryResult result = WorldDatabase.Query(
        "SELECT 功能值 FROM __控制总表 WHERE 序号 = 22001");

    if (result)
    {
        Field* fields = result->Fetch();
        std::string spellList = fields[0].Get<std::string>();

        // 解析逗号分隔的法术ID列表
        size_t start = 0;
        size_t end = spellList.find(',');
        while (end != std::string::npos)
        {
            std::string spellIdStr = spellList.substr(start, end - start);
            if (!spellIdStr.empty())
            {
                _gcdWhitelist.insert(uint32(atoi(spellIdStr.c_str())));
            }
            start = end + 1;
            end = spellList.find(',', start);
        }
        // 处理最后一个
        std::string lastSpellIdStr = spellList.substr(start);
        if (!lastSpellIdStr.empty())
        {
            _gcdWhitelist.insert(uint32(atoi(lastSpellIdStr.c_str())));
        }
    }

    LOG_INFO("server.worldserver", ">> 加载无GCD检查白名单完成，共 {} 条", _gcdWhitelist.size());
}

void SpellCooldownMgr::Reload()
{
    Load();
}

bool SpellCooldownMgr::IsInGCDWhitelist(uint32 spellId)
{
    return _gcdWhitelist.find(spellId) != _gcdWhitelist.end();
}

// 脚本注册
class SpellCooldownPlayerScript : public PlayerScript
{
public:
    SpellCooldownPlayerScript() : PlayerScript("SpellCooldownPlayerScript") {}

    void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!spell)
            return;

        // 如果法术在白名单中，可以在这里处理特殊逻辑
        // 这个白名单主要用于其他系统查询
        uint32 spellId = spell->GetSpellInfo()->Id;
        if (sSpellCooldownMgr->IsInGCDWhitelist(spellId))
        {
            // 法术在白名单中，可以添加特殊处理
        }
    }
};

void AddSC_SpellCooldownMgr()
{
    new SpellCooldownPlayerScript();
}
