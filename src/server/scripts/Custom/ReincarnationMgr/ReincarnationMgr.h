#ifndef _REINCARNATIONMGR_H
#define _REINCARNATIONMGR_H

#include "Common.h"
#include <unordered_map>
#include <string>

class Player;
class Creature;

struct ReincarnationTemplate
{
    uint32 reqId;           // 需求模板ID
    uint32 rewId;           // 奖励模板ID
    std::string gossipText; // 对话文本
};

typedef std::unordered_map<uint32, ReincarnationTemplate> ReincarnationTemplateMap;
extern ReincarnationTemplateMap ReincarnationTemplateStore;

class ReincarnationMgr
{
public:
    static ReincarnationMgr* Instance()
    {
        static ReincarnationMgr Instance;
        return &Instance;
    }

    void LoadReincarnationTemplate(bool isReload = false);
    uint32 GetMaxLevel();
    void GetParams(Player* player, uint32& rewId, uint32& reqId);
    std::string GetGossipText(Player* player);
    void AddGossip(Player* player, Creature* creature);
    void DoAction(Player* player, Creature* creature, uint32 action);
    bool TryReincarnate(Player* player);
    void Save(Player* player);
    void Load(Player* player);

    // 重载功能
    void Reload();

private:

};

#define sReincarnationMgr ReincarnationMgr::Instance()

#endif
