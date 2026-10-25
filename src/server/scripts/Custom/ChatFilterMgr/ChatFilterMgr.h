#ifndef _CHATFILTERMGR_H
#define _CHATFILTERMGR_H

#include "Common.h"
#include <vector>
#include <string>

class ChatFilterMgr
{
public:
    static ChatFilterMgr* Instance()
    {
        static ChatFilterMgr Instance;
        return &Instance;
    }

    void Load();
    void Reload();

    // 过滤消息，返回true表示消息正常，false表示消息被拦截
    bool FilterMessage(std::string& msg);

private:
    std::vector<std::string> _dirtyWordVector;
};

#define sChatFilterMgr ChatFilterMgr::Instance()

#endif // !_CHATFILTERMGR_H
