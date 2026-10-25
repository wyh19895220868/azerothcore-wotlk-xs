/*
 * 物品检查GUID查询处理器
 * 用于处理客户端查询其他玩家装备GUID的请求
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Item.h"
#include "Chat.h"
#include "ObjectAccessor.h"
#include "ItemEnhancementMgr.h"
#include <sstream>
#include <vector>

// Addon消息处理脚本 - 处理INSPECT_ITEM_GUID查询
class ItemInspectGuidAddonScript : public PlayerScript
{
public:
    ItemInspectGuidAddonScript() : PlayerScript("ItemInspectGuidAddonScript") { }

    // 监听玩家聊天事件，拦截Addon消息
    void OnChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
    {
        // 只处理Addon消息
        if (type != CHAT_MSG_WHISPER || lang != LANG_ADDON)
            return;

        // 查找TAB字符的位置
        size_t tabPos = msg.find('\t');
        if (tabPos == std::string::npos)
            return;

        // 提取前缀部分（应该是"UITQ"）
        std::string prefix = msg.substr(0, tabPos);
        if (prefix != "UITQ")
            return;

        // 提取消息内容（TAB之后的部分）
        std::string command = msg.substr(tabPos + 1);

        // 只处理INSPECT_ITEM_GUID命令
        if (command.find("INSPECT_ITEM_GUID:") != 0)
            return;

        HandleInspectItemGuidQuery(player, command);
    }

private:
    // 分割字符串的辅助函数
    std::vector<std::string> SplitString(const std::string& str, char delimiter)
    {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;

        while (std::getline(ss, token, delimiter))
        {
            tokens.push_back(token);
        }

        return tokens;
    }

    void HandleInspectItemGuidQuery(Player* player, const std::string& command)
    {
        if (!player)
            return;

        // 解析命令: INSPECT_ITEM_GUID:playerName:slot:itemID
        std::vector<std::string> parts = SplitString(command, ':');

        // 需要至少4个部分: INSPECT_ITEM_GUID, playerName, slot, itemID
        if (parts.size() < 4)
            return;

        std::string playerName = parts[1];
        uint32 slot = 0;
        uint32 itemID = 0;

        try
        {
            slot = std::stoul(parts[2]);
            itemID = std::stoul(parts[3]);
        }
        catch (...)
        {
            return;  // 解析失败
        }

        // 验证槽位范围
        if (slot >= EQUIPMENT_SLOT_END)
            return;

        // 查找目标玩家
        Player* targetPlayer = ObjectAccessor::FindPlayerByName(playerName);
        if (!targetPlayer)
            return;

        // 【安全修复】添加权限和可见性校验
        // 1. 检查是否是查询自己（允许）
        bool isSelf = (player->GetGUID() == targetPlayer->GetGUID());

        if (!isSelf)
        {
            // 2. 检查目标玩家是否在同一地图
            if (player->GetMapId() != targetPlayer->GetMapId())
                return;

            // 3. 检查距离限制（最大100码，与观察装备的默认距离相近）
            float distance = player->GetDistance(targetPlayer);
            if (distance > 100.0f)
                return;

            // 4. 检查目标玩家是否可见（非隐身、非GM隐身状态）
            if (!player->CanSeeOrDetect(targetPlayer, false, true))
                return;

            // 5. 检查是否能观察目标（非敌对阵营等）
            // 允许观察同阵营或中立玩家的装备
            if (player->IsHostileTo(targetPlayer))
                return;
        }

        // 获取目标玩家指定槽位的装备
        Item* item = targetPlayer->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            return;

        // 验证物品ID是否匹配
        if (item->GetEntry() != itemID)
            return;

        // 获取物品的实例GUID（低位部分）
        uint32 itemGuid = item->GetGUID().GetCounter();

        // 构建响应消息: INSPECT_ITEM_GUID_RESPONSE:playerName:slot:itemID:guid
        std::ostringstream response;
        response << "INSPECT_ITEM_GUID_RESPONSE:" << playerName
                 << ":" << slot << ":" << itemID << ":" << itemGuid;

        // 通过Addon消息发送响应
        if (sItemEnhancementMgr)
            sItemEnhancementMgr->SendAddonMessage(player, response.str());
    }
};

// 注册脚本
void AddSC_ItemInspectGuidHandler()
{
    new ItemInspectGuidAddonScript();
}
