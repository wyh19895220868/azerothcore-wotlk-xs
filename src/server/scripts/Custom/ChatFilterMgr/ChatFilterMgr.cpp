#pragma execution_character_set("utf-8")
#include "ChatFilterMgr.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ScriptMgr.h"
#include <algorithm>

void ChatFilterMgr::Load()
{
    _dirtyWordVector.clear();

    QueryResult result = WorldDatabase.Query("SELECT 敏感词 FROM __聊天敏感词列表");

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            _dirtyWordVector.push_back(fields[0].Get<std::string>());
        } while (result->NextRow());
    }

    LOG_INFO("server.worldserver", ">> 加载聊天敏感词列表完成，共 {} 条", _dirtyWordVector.size());
}

void ChatFilterMgr::Reload()
{
    Load();
}

bool ChatFilterMgr::FilterMessage(std::string& msg)
{
    for (const std::string& dirtyWord : _dirtyWordVector)
    {
        if (dirtyWord.empty())
            continue;

        size_t pos = msg.find(dirtyWord);
        if (pos != std::string::npos)
        {
            // 将敏感词替换为*
            msg.replace(pos, dirtyWord.length(), dirtyWord.length(), '*');
        }
    }
    return true;
}

// 脚本注册
class ChatFilterPlayerScript : public PlayerScript
{
public:
    ChatFilterPlayerScript() : PlayerScript("ChatFilterPlayerScript") {}

    void OnChat(Player* player, uint32 /*type*/, uint32 /*lang*/, std::string& msg) override
    {
        sChatFilterMgr->FilterMessage(msg);
    }

    void OnChat(Player* player, uint32 /*type*/, uint32 /*lang*/, std::string& msg, Player* /*receiver*/) override
    {
        sChatFilterMgr->FilterMessage(msg);
    }

    void OnBeforeSendChatMessage(Player* /*player*/, uint32& /*type*/, uint32& /*lang*/, std::string& msg) override
    {
        sChatFilterMgr->FilterMessage(msg);
    }
};

void AddSC_ChatFilterMgr()
{
    new ChatFilterPlayerScript();
}
