#ifndef MODULE_ITEM_GROWTH_COMMANDS_H
#define MODULE_ITEM_GROWTH_COMMANDS_H

#include "ScriptMgr.h"
#include "Chat.h"
#include "Player.h"
#include "ItemGrowthMgr.h"
#include "ChatCommand.h"

using namespace Acore::ChatCommands;

class ItemGrowthCommands : public CommandScript
{
public:
    ItemGrowthCommands();

    ChatCommandTable GetCommands() const override;

private:
    static bool HandleItemGrowthCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthInfoCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthUpgradeCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthAttributesCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthReloadCommand(ChatHandler* handler, char const* args);

    // 测试命令
    static bool HandleItemGrowthTestCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthAddExpCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthSetLevelCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthResetCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthListCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthStatsCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthSetGrowableCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthSetGroupCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthRandomGroupCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthAttributeInfoCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthDebugTemplatesCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthViewOriginalStatsCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthForceApplyCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthCheckGuidCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthVerifyAttributesCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthTestAuraCommand(ChatHandler* handler, char const* args);
    static bool HandleItemGrowthQueryCommand(ChatHandler* handler, char const* args);

    // 辅助函数：发送成长属性更新通知给客户端
    static void SendGrowthUpdateToClient(ChatHandler* handler, Player* player, Item* item);
};

#endif // MODULE_ITEM_GROWTH_COMMANDS_H
