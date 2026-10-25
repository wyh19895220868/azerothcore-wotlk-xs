#include "AutoLoot_new.h"
#include "AccountMgr.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Config.h"
#include "ModuleFormat.h"
#include "Util.h"
#include "WorldSession.h"
#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>

namespace
{
    bool enableModule = true;
    bool autoLootMoney = true;
    bool autoLootQuestItems = true;
    bool debugMode = false;
    std::unordered_map<uint32, bool> authorizedAccounts;

    void LoadAutoLootSettings();

    uint32 ToClientMoney(uint32 money)
    {
        return money;
    }

    uint32 AddMoney(Player* player, uint32 money)
    {
        uint32 added = 0;
        uint32 remaining = money;

        while (remaining > 0)
        {
            uint32 const chunk = std::min<uint32>(remaining, static_cast<uint32>(std::numeric_limits<int32>::max()));
            if (!player->ModifyMoney(static_cast<int32>(chunk)))
                break;

            added += chunk;
            remaining -= chunk;
        }

        return added;
    }

    std::string NormalizeAccountName(std::string accountName)
    {
        Utf8ToUpperOnlyLatin(accountName);
        return accountName;
    }

    void LoadAuthorizationTable()
    {
        authorizedAccounts.clear();

        QueryResult tableCheck = CharacterDatabase.Query("SHOW TABLES LIKE '\\_授权自动拾取'");
        if (!tableCheck)
        {
            LOG_WARN("module.auto_loot", "自动拾取授权表 _授权自动拾取 不存在，自动拾取不会对任何账号生效");
            return;
        }

        QueryResult result = CharacterDatabase.Query("SELECT `玩家账号`, `自动拾取` FROM `_授权自动拾取`");
        if (!result)
        {
            LOG_WARN("module.auto_loot", "自动拾取授权表为空，自动拾取不会对任何账号生效");
            return;
        }

        uint32 enabledCount = 0;
        uint32 disabledCount = 0;

        do
        {
            Field* fields = result->Fetch();
            std::string accountName = fields[0].Get<std::string>();
            bool enabled = fields[1].Get<uint8>() != 0;

            if (accountName.empty())
                continue;

            accountName = NormalizeAccountName(accountName);
            uint32 accountId = AccountMgr::GetId(accountName);
            if (!accountId)
            {
                LOG_WARN("module.auto_loot", ModuleOldFormat("自动拾取授权表账号不存在，已跳过: %s", accountName).c_str());
                continue;
            }

            authorizedAccounts[accountId] = enabled;

            if (enabled)
                ++enabledCount;
            else
                ++disabledCount;
        } while (result->NextRow());

        LOG_INFO("module.auto_loot", ModuleOldFormat("自动拾取授权表加载完成: 开启账号=%s, 关闭账号=%s", enabledCount, disabledCount).c_str());
    }

    void LoadAutoLootSettings()
    {
        enableModule = sConfigMgr->GetOption<bool>("AutoLoot.Enable", true);
        autoLootMoney = sConfigMgr->GetOption<bool>("AutoLoot.Money", true);
        autoLootQuestItems = sConfigMgr->GetOption<bool>("AutoLoot.QuestItems", true);
        debugMode = sConfigMgr->GetOption<bool>("AutoLoot.Debug", false);
        LoadAuthorizationTable();

        if (debugMode)
        {
            LOG_INFO("module.auto_loot", ModuleOldFormat("自动拾取模块: 已加载配置 - 启用状态: %s, 自动拾取金币: %s, 自动拾取任务物品: %s",
                enableModule ? "启用" : "禁用",
                autoLootMoney ? "启用" : "禁用",
                autoLootQuestItems ? "启用" : "禁用").c_str());
        }
    }

    bool IsAccountAuthorized(Player* player)
    {
        if (!player || !player->GetSession())
            return false;

        uint32 accountId = player->GetSession()->GetAccountId();

        auto itr = authorizedAccounts.find(accountId);
        if (itr == authorizedAccounts.end())
        {
            if (debugMode)
                LOG_INFO("module.auto_loot", ModuleOldFormat("账号ID %s 未授权自动拾取，跳过", accountId).c_str());

            return false;
        }

        if (!itr->second && debugMode)
            LOG_INFO("module.auto_loot", ModuleOldFormat("账号ID %s 自动拾取已关闭，跳过", accountId).c_str());

        return itr->second;
    }

    class ScopedLootGuid
    {
    public:
        ScopedLootGuid(Player* player, ObjectGuid lootGuid) : _player(player), _oldLootGuid(player ? player->GetLootGUID() : ObjectGuid::Empty)
        {
            if (_player)
                _player->SetLootGUID(lootGuid);
        }

        ~ScopedLootGuid()
        {
            if (_player)
                _player->SetLootGUID(_oldLootGuid);
        }

    private:
        Player* _player;
        ObjectGuid _oldLootGuid;
    };
}

AutoLootPlayerScript::AutoLootPlayerScript() : PlayerScript("AutoLootPlayerScript")
{
}

AutoLootModuleLoader::AutoLootModuleLoader() : WorldScript("AutoLootModuleLoader"), _loaded(false), _startTime(0) { }

void AutoLootModuleLoader::OnAfterConfigLoad(bool /*reload*/)
{
    LoadAutoLootSettings();
}

void AutoLootModuleLoader::OnUpdate(uint32 diff)
{
    // 如果已经加载过，或者服务器正在关闭，则不执行延迟加载逻辑
    if (_loaded || sWorld->IsClosed())
        return;

    // 记录开始时间
    if (_startTime == 0)
        _startTime = getMSTime();

    // 检查是否已经过了延迟时间（1秒）
    if (getMSTime() - _startTime >= 1000)
    {
        _loaded = true;

        // 这里可以加载模块所需的数据
        // 示例：加载1条数据
        LOG_INFO("server.loading", "→自动拾取系统√");
    }
}

bool AutoLootPlayerScript::IsDebugEnabled() const
{
    return debugMode;
}

void AutoLootPlayerScript::OnCreatureKill(Player* player, Creature* killed)
{
    // 如果模块被禁用，直接返回
    if (!enableModule)
        return;

    // 确保参数有效
    if (!player || !killed)
        return;

    if (!IsAccountAuthorized(player))
        return;

    if (IsDebugEnabled())
        LOG_INFO("module.auto_loot", ModuleOldFormat("玩家 %s 击杀了生物 %s (ID: %s)",
            player->GetName(),
            killed->GetName(),
            killed->GetEntry()).c_str());

    // 处理战利品
    HandleLoot(player, killed);
}

void AutoLootPlayerScript::HandleLoot(Player* player, Creature* creature)
{
    // 检查玩家是否在生物的战利品接收者列表中
    if (!creature->hasLootRecipient() || !creature->isTappedBy(player))
    {
        if (IsDebugEnabled())
            LOG_INFO("module.auto_loot", ModuleOldFormat("玩家 %s 不是生物 %s 的战利品接收者",
                player->GetName(),
                creature->GetName()).c_str());
        return;
    }

    // 如果玩家在小队/团队中，使用小队/团队战利品处理
    if (player->GetGroup())
    {
        HandleGroupLoot(player, creature);
        return;
    }

    // 处理个人战利品
    if (autoLootMoney)
    {
        HandleMoneyLoot(player, creature);
    }

    HandleItemLoot(player, creature);
}

void AutoLootPlayerScript::HandleMoneyLoot(Player* player, Creature* creature)
{
    Loot* loot = &creature->loot;
    if (!loot)
        return;

    if (loot->gold == 0)
        return;

    uint32 const lootedGold = loot->gold;
    uint32 const addedGold = AddMoney(player, lootedGold);
    if (!addedGold)
        return;

    // 更新成就统计
    player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, ToClientMoney(addedGold));

    if (IsDebugEnabled())
        LOG_INFO("module.auto_loot", ModuleOldFormat("玩家 %s 自动拾取了 %s 铜币", player->GetName(), addedGold).c_str());

    loot->gold = lootedGold > addedGold ? lootedGold - addedGold : 0;
    if (loot->gold == 0)
        loot->NotifyMoneyRemoved();
}

void AutoLootPlayerScript::HandleItemLoot(Player* player, Creature* creature)
{
    Loot* loot = &creature->loot;
    if (!loot)
        return;

    // 获取玩家可以拾取的物品数量
    uint32 maxSlot = loot->GetMaxSlotInLootFor(player);
    ScopedLootGuid lootGuidGuard(player, creature->GetGUID());

    for (uint32 i = 0; i < maxSlot; ++i)
    {
        LootItem* lootItem = loot->LootItemInSlot(i, player);

        // 跳过已拾取的物品
        if (!lootItem || lootItem->is_looted)
            continue;

        // 如果是任务物品且任务物品自动拾取被禁用，则跳过
        if (lootItem->needs_quest && !autoLootQuestItems)
            continue;

        // 检查物品是否唯一且玩家是否已拥有
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(lootItem->itemid);
        if (proto && proto->MaxCount == 1)
        {
            // 检查玩家是否已拥有此唯一物品
            if (player->HasItemCount(lootItem->itemid, 1, true))
            {
                // 玩家已拥有此唯一物品，保留在尸体中让核心维持原生拾取状态
                if (IsDebugEnabled())
                    LOG_INFO("module.auto_loot", ModuleOldFormat("玩家 %s 已拥有唯一物品 %s (ID: %s)，跳过拾取",
                        player->GetName(), proto->Name1, lootItem->itemid).c_str());

                continue;
            }
        }

        // 尝试将物品添加到玩家背包
        InventoryResult inventoryResult = EQUIP_ERR_OK;
        player->StoreLootItem(static_cast<uint8>(i), loot, inventoryResult);
        if (IsDebugEnabled())
            LOG_INFO("module.auto_loot", ModuleOldFormat("玩家 %s 尝试自动拾取物品 %s", player->GetName(), lootItem->itemid).c_str());
    }

    // 如果所有物品都已拾取，移除尸体的可拾取标记
    if (loot->isLooted())
    {
        // 移除可拾取标记
        creature->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
        creature->AllLootRemovedFromCorpse();
        if (IsDebugEnabled())
            LOG_INFO("module.auto_loot", ModuleOldFormat("生物 %s 的所有战利品已被拾取，移除可拾取标记", creature->GetName()).c_str());
    }
}

void AutoLootPlayerScript::HandleGroupLoot(Player* player, Creature* creature)
{
    Group* group = player->GetGroup();
    if (!group)
        return;

    Loot* loot = &creature->loot;
    if (!loot)
        return;

    // 处理金币拾取（平均分配给小队成员）
    if (autoLootMoney && loot->gold > 0)
    {
        std::vector<Player*> playersNear;

        // 收集附近的小队成员
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !member->IsAtGroupRewardDistance(creature))
                continue;

            playersNear.push_back(member);
        }

        if (!playersNear.empty())
        {
            uint32 const lootedGold = loot->gold;
            uint32 const playerCount = static_cast<uint32>(playersNear.size());
            uint32 goldPerPlayer = lootedGold / playerCount;
            uint32 extraCopper = lootedGold % playerCount;
            uint32 totalAddedGold = 0;

            for (Player* member : playersNear)
            {
                uint32 playerGold = goldPerPlayer;

                // 将多余的铜币给第一个玩家
                if (extraCopper > 0 && member == playersNear[0])
                {
                    playerGold += extraCopper;
                }

                uint32 const addedGold = AddMoney(member, playerGold);
                if (!addedGold)
                    continue;

                totalAddedGold += addedGold;
                // 更新成就统计
                member->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, ToClientMoney(addedGold));

                if (IsDebugEnabled())
                    LOG_INFO("module.auto_loot", ModuleOldFormat("小队成员 %s 自动拾取了 %s 铜币", member->GetName(), addedGold).c_str());
            }

            if (totalAddedGold > 0)
            {
                loot->gold = lootedGold > totalAddedGold ? lootedGold - totalAddedGold : 0;
                if (loot->gold == 0)
                    loot->NotifyMoneyRemoved();
            }
        }
    }

    // 物品拾取遵循小队拾取规则，不自动分配
    // 这里我们只处理个人拾取和自由拾取模式
    int lootMethod = group->GetLootMethod();
    // 检查拾取方式
    if (lootMethod == MASTER_LOOT || lootMethod == GROUP_LOOT ||
        lootMethod == NEED_BEFORE_GREED || lootMethod == ROUND_ROBIN)
    {
        if (IsDebugEnabled())
            LOG_INFO("module.auto_loot", "小队使用的是需要手动分配的拾取模式，跳过自动拾取物品");
        return;
    }

    // 处理自由拾取或个人拾取模式
    HandleItemLoot(player, creature);
}

class AutoLootCommandScript : public CommandScript
{
public:
    AutoLootCommandScript() : CommandScript("AutoLootCommandScript") { }

    Acore::ChatCommands::ChatCommandTable GetCommands() const override
    {
        using namespace Acore::ChatCommands;

        static ChatCommandTable autoLootCommandTable =
        {
            { "重载", HandleReloadCommand, SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "自动拾取", autoLootCommandTable },
        };

        return commandTable;
    }

    static bool HandleReloadCommand(ChatHandler* handler, char const* /*args*/)
    {
        LoadAutoLootSettings();

        if (handler)
            handler->SendSysMessage("自动拾取系统配置和授权表重载完成");

        return true;
    }
};

// 添加脚本
void AddAutoLootScripts()
{
    new AutoLootPlayerScript();
    new AutoLootModuleLoader();
    new AutoLootCommandScript();
}
