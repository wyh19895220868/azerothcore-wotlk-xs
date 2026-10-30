#pragma execution_character_set("utf-8")
#include "StarVeinMgr.h"

#include "Chat.h"
#include "Common.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "StringConvert.h"
#include "Time/GameTime.h"

#include <Custom/_Helper/Helper.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace
{
    // 插件固定前缀，StarVein.lua 中 PREFIX 必须与此一致。
    constexpr char const* STAR_VEIN_ADDON_PREFIX = "XMYH";

    std::string const& GetBranchName(uint8 branch)
    {
        static std::string const names[STAR_VEIN_BRANCH_COUNT] =
        {
            "翠绿星脉", "绯紫星脉", "湛蓝星脉", "曜金星脉"
        };
        static std::string const unknown = "星脉";
        if (branch < 1 || branch > STAR_VEIN_BRANCH_COUNT)
            return unknown;
        return names[branch - 1];
    }

    // 插件用 | 分隔字段，并在收包后把所有 %XX 还原成原始字节。
    // 这里只转义分隔符、百分号和换行，中文按 UTF-8 原样发送。
    std::string EncodeAddonField(std::string const& text)
    {
        static char const hex[] = "0123456789ABCDEF";
        std::string encoded;
        encoded.reserve(text.size());
        for (unsigned char byte : text)
        {
            if (byte == '%' || byte == '|' || byte == '\r' || byte == '\n')
            {
                encoded.push_back('%');
                encoded.push_back(hex[(byte >> 4) & 0x0F]);
                encoded.push_back(hex[byte & 0x0F]);
            }
            else
                encoded.push_back(char(byte));
        }
        return encoded;
    }
}

StarVeinNodeMap StarVeinNodeStore;

// branch -> order -> node id，用于判断同一条星脉是否可以从左向右逐个点亮。
static std::array<std::array<uint32, STAR_VEIN_NODE_PER_BRANCH + 1>, STAR_VEIN_BRANCH_COUNT + 1> StarVeinChainStore{};

void StarVeinMgr::LoadConfig(bool isReload)
{
    StarVeinNodeStore.clear();
    for (auto& branch : StarVeinChainStore)
        branch.fill(0);

    QueryResult result = WorldDatabase.Query(
        "SELECT `节点ID`, `星脉`, `顺序`, `前置节点`, `是否共鸣`, `是否启用`, `需求模板`, `技能编号`, "
        "`节点名称`, `效果说明`, `材料来源` FROM `_星脉衍化节点配置` ORDER BY `节点ID`");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            StarVeinNode node;
            node.Id = fields[0].Get<uint32>();
            node.Branch = fields[1].Get<uint8>();
            node.Order = fields[2].Get<uint8>();
            node.ParentId = fields[3].Get<uint32>();
            node.Resonance = fields[4].Get<uint8>() != 0;
            node.Enabled = fields[5].Get<uint8>() != 0;
            node.NeedTempId = fields[6].Get<uint32>();
            node.SpellId = fields[7].Get<uint32>();
            node.Name = fields[8].Get<std::string>();
            node.Description = fields[9].Get<std::string>();
            node.Material = fields[10].Get<std::string>();

            if (!node.Id || node.Id > STAR_VEIN_MAX_NODE_ID)
            {
                LOG_ERROR("server.loading", "星脉衍化节点配置存在无效节点ID {}，有效范围 1-{}", node.Id, STAR_VEIN_MAX_NODE_ID);
                continue;
            }
            if (node.Branch < 1 || node.Branch > STAR_VEIN_BRANCH_COUNT)
            {
                LOG_ERROR("server.loading", "星脉衍化节点 {} 的星脉编号 {} 无效，有效范围 1-{}",
                    node.Id, uint32(node.Branch), uint32(STAR_VEIN_BRANCH_COUNT));
                continue;
            }
            if (node.Order < 1 || node.Order > STAR_VEIN_NODE_PER_BRANCH)
            {
                LOG_ERROR("server.loading", "星脉衍化节点 {} 的顺序 {} 无效，有效范围 1-{}",
                    node.Id, uint32(node.Order), uint32(STAR_VEIN_NODE_PER_BRANCH));
                continue;
            }
            if (uint32 occupied = StarVeinChainStore[node.Branch][node.Order])
            {
                LOG_ERROR("server.loading", "星脉衍化节点 {} 与节点 {} 重复占用星脉 {} 顺序 {}，已跳过",
                    node.Id, occupied, uint32(node.Branch), uint32(node.Order));
                continue;
            }
            StarVeinChainStore[node.Branch][node.Order] = node.Id;

            if (node.Resonance && node.Order != STAR_VEIN_NODE_PER_BRANCH)
                LOG_WARN("server.loading", "星脉衍化节点 {} 标记为共鸣孔，但顺序为 {} 而不是每链第 {} 孔",
                    node.Id, uint32(node.Order), uint32(STAR_VEIN_NODE_PER_BRANCH));
            if (node.SpellId && !sSpellMgr->GetSpellInfo(node.SpellId))
                LOG_WARN("server.loading", "星脉衍化节点 {} 的技能编号 {} 不在法术表中，点亮后不会施加光环",
                    node.Id, node.SpellId);

            if (node.Name.empty())
                node.Name = GetBranchName(node.Branch) + " " + std::to_string(uint32(node.Order));
            if (node.Description.empty())
                node.Description = "待配置";
            if (node.Material.empty())
                node.Material = "待配置";

            StarVeinNodeStore[node.Id] = std::move(node);
        } while (result->NextRow());
    }

    // 第二遍校验前置关系：必须同链、顺序更小，缺失时回退到同链前一孔。
    for (auto& pair : StarVeinNodeStore)
    {
        StarVeinNode& node = pair.second;
        if (node.Order == 1)
        {
            if (node.ParentId)
            {
                LOG_WARN("server.loading", "星脉衍化节点 {} 为同链第1孔，前置节点配置已忽略", node.Id);
                node.ParentId = 0;
            }
            continue;
        }

        uint32 previousId = GetChainNodeId(node.Branch, uint8(node.Order - 1));
        if (!previousId)
            LOG_WARN("server.loading", "星脉衍化节点 {} 缺少同链前一孔（星脉 {} 顺序 {}），该节点会保持未解锁",
                node.Id, uint32(node.Branch), uint32(node.Order - 1));

        if (!node.ParentId)
        {
            node.ParentId = previousId;
            continue;
        }

        auto parentItr = StarVeinNodeStore.find(node.ParentId);
        if (parentItr == StarVeinNodeStore.end())
        {
            LOG_WARN("server.loading", "星脉衍化节点 {} 的前置节点 {} 不存在，改用同链前一孔 {}",
                node.Id, node.ParentId, previousId);
            node.ParentId = previousId;
            continue;
        }

        StarVeinNode const& parent = parentItr->second;
        if (parent.Branch != node.Branch)
        {
            LOG_ERROR("server.loading", "星脉衍化节点 {} 的前置节点 {} 跨链配置无效，改用同链前一孔 {}",
                node.Id, node.ParentId, previousId);
            node.ParentId = previousId;
        }
        else if (parent.Order >= node.Order)
        {
            LOG_ERROR("server.loading", "星脉衍化节点 {} 的前置节点 {} 顺序不小于自身，无法形成前置，改用同链前一孔 {}",
                node.Id, node.ParentId, previousId);
            node.ParentId = previousId;
        }
        else if (parent.Order != node.Order - 1)
            LOG_WARN("server.loading", "星脉衍化节点 {} 的前置节点不是同链前一孔，同链前一孔仍需点亮",
                node.Id);
    }

    uint32 enabledCount = 0;
    for (auto const& pair : StarVeinNodeStore)
        if (pair.second.Enabled)
            ++enabledCount;

    if (!isReload)
    {
        LOG_INFO("server.loading", "功能表 >> 加载 [_星脉衍化节点配置] 总计 {} 条数据，已启用 {} 条",
            StarVeinNodeStore.size(), enabledCount);
    }
}

void StarVeinMgr::Reload()
{
    LOG_INFO("server.loading", ">> 重载 [_星脉衍化节点配置]...");
    LoadConfig(true);
    LOG_INFO("server.loading", ">> 重载 [_星脉衍化节点配置] 生效，在线面板下次查询使用新配置");
}

bool StarVeinMgr::IsEnabled() const
{
    return sHelper->GetBoolean(STAR_VEIN_FEATURE_CONFIG_ID) && !StarVeinNodeStore.empty();
}

uint32 StarVeinMgr::GetChainNodeId(uint8 branch, uint8 order) const
{
    if (branch < 1 || branch > STAR_VEIN_BRANCH_COUNT || order < 1 || order > STAR_VEIN_NODE_PER_BRANCH)
        return 0;
    return StarVeinChainStore[branch][order];
}

void StarVeinMgr::LoadLitNodes(Player* player, std::unordered_set<uint32>& lit) const
{
    lit.clear();
    if (!player)
        return;

    QueryResult result = CharacterDatabase.Query(
        "SELECT `节点ID` FROM `_星脉衍化角色存档` WHERE `角色GUID` = {}",
        player->GetGUID().GetCounter());
    if (!result)
        return;

    do
    {
        uint32 nodeId = result->Fetch()[0].Get<uint32>();
        if (nodeId)
            lit.insert(nodeId);
    } while (result->NextRow());
}

std::unordered_set<uint32> const& StarVeinMgr::GetLitNodes(Player* player)
{
    static std::unordered_set<uint32> const empty;

    if (!player)
        return empty;

    uint32 guid = player->GetGUID().GetCounter();
    auto itr = _litCache.find(guid);
    if (itr != _litCache.end())
        return itr->second;

    std::unordered_set<uint32> lit;
    LoadLitNodes(player, lit);
    return _litCache.emplace(guid, std::move(lit)).first->second;
}

void StarVeinMgr::ClearPlayerCache(Player* player)
{
    if (!player)
        return;

    _litCache.erase(player->GetGUID().GetCounter());
}

uint8 StarVeinMgr::GetNodeStatus(StarVeinNode const& node, std::unordered_set<uint32> const& lit) const
{
    if (lit.find(node.Id) != lit.end())
        return 2;

    if (!node.Enabled)
        return 0;

    // 同一条星脉必须从左向右逐个点亮，前置节点与同链前一孔都必须已点亮。
    if (node.Order > 1)
    {
        uint32 previousId = GetChainNodeId(node.Branch, uint8(node.Order - 1));
        if (!previousId || lit.find(previousId) == lit.end())
            return 0;
    }
    if (node.ParentId && lit.find(node.ParentId) == lit.end())
        return 0;

    return 1;
}

void StarVeinMgr::ApplyPlayerAuras(Player* player)
{
    if (!player)
        return;

    std::unordered_set<uint32> const& lit = GetLitNodes(player);
    if (lit.empty())
        return;

    for (uint32 nodeId : lit)
    {
        auto itr = StarVeinNodeStore.find(nodeId);
        if (itr == StarVeinNodeStore.end() || !itr->second.SpellId)
            continue;

        uint32 spellId = itr->second.SpellId;
        if (!sSpellMgr->GetSpellInfo(spellId) || player->HasAura(spellId))
            continue;

        player->AddAura(spellId, player);
    }
}

void StarVeinMgr::SendFrame(Player* player, std::string const& payload) const
{
    if (!player)
        return;

    std::string full = std::string(STAR_VEIN_ADDON_PREFIX) + "\t" + payload;
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, full, 0);
    player->SendDirectMessage(&data);
}

void StarVeinMgr::SendError(Player* player, std::string const& message) const
{
    // 插件只把 E 帧的第二个字段整段显示，因此这里不能再带分隔符。
    std::string text = message;
    std::replace(text.begin(), text.end(), '|', ' ');
    SendFrame(player, "E|" + text);
}

void StarVeinMgr::SendNodeText(Player* player, uint32 nodeId, char const* key, std::string const& text) const
{
    std::string encoded = EncodeAddonField(text);
    std::string header = "T|" + std::to_string(nodeId) + "|" + key + "|";
    std::size_t const limit = STAR_VEIN_ADDON_CHUNK > header.size() ? STAR_VEIN_ADDON_CHUNK - header.size() : 8;

    if (encoded.empty())
    {
        SendFrame(player, header);
        return;
    }

    std::size_t index = 0;
    while (index < encoded.size())
    {
        // 一个 %XX 转义要么整段放进当前分片，要么整体推到下一片，避免半截转义。
        std::size_t end = index;
        while (end < encoded.size())
        {
            std::size_t token = encoded[end] == '%' ? 3 : 1;
            if (end + token - index > limit)
                break;
            end += token;
        }
        if (end == index)
            end = std::min(encoded.size(), index + (encoded[index] == '%' ? 3 : 1));

        SendFrame(player, header + encoded.substr(index, end - index));
        index = end;
    }
}

void StarVeinMgr::SendState(Player* player)
{
    if (!player)
        return;

    if (!IsEnabled())
    {
        SendError(player, "星脉衍化功能已关闭，请联系管理员。");
        return;
    }

    std::unordered_set<uint32> const& lit = GetLitNodes(player);

    std::vector<StarVeinNode const*> nodes;
    nodes.reserve(StarVeinNodeStore.size());
    for (auto const& pair : StarVeinNodeStore)
        nodes.push_back(&pair.second);

    std::sort(nodes.begin(), nodes.end(), [](StarVeinNode const* left, StarVeinNode const* right)
    {
        if (left->Branch != right->Branch)
            return left->Branch < right->Branch;
        if (left->Order != right->Order)
            return left->Order < right->Order;
        return left->Id < right->Id;
    });

    SendFrame(player, "B");
    for (StarVeinNode const* node : nodes)
    {
        if (!node)
            continue;

        uint8 status = GetNodeStatus(*node, lit);
        std::string frame = "N|" + std::to_string(node->Id) +
            "|" + std::to_string(uint32(node->Branch)) +
            "|" + std::to_string(uint32(node->Order)) +
            "|" + std::to_string(node->ParentId) +
            "|" + (node->Resonance ? "1" : "0") +
            "|" + std::to_string(uint32(status)) +
            "|" + std::to_string(node->SpellId);
        SendFrame(player, frame);

        SendNodeText(player, node->Id, "N", node->Name);
        SendNodeText(player, node->Id, "D", node->Description);
        SendNodeText(player, node->Id, "S", node->Material);

        std::string need = node->NeedTempId ? sHelper->GetNeedGts(node->NeedTempId) : std::string("无需材料");
        if (need.empty())
            need = node->NeedTempId ? "需求模板未配置，请联系管理员。" : "无需材料";
        SendNodeText(player, node->Id, "R", need);
    }
    SendFrame(player, "Z");
}

bool StarVeinMgr::SaveNode(Player* player, StarVeinNode const& node) const
{
    if (!player)
        return false;

    uint32 guid = player->GetGUID().GetCounter();
    CharacterDatabase.DirectExecute(
        "INSERT IGNORE INTO `_星脉衍化角色存档` (`角色GUID`, `节点ID`, `技能编号`, `点亮时间`) VALUES ({}, {}, {}, {})",
        guid, node.Id, node.SpellId, uint32(GameTime::GetGameTime().count()));

    // 写库失败时不能扣材料，回读确认存档真实落库。
    return CharacterDatabase.Query(
        "SELECT 1 FROM `_星脉衍化角色存档` WHERE `角色GUID` = {} AND `节点ID` = {} LIMIT 1",
        guid, node.Id) != nullptr;
}

void StarVeinMgr::ActivateNode(Player* player, uint32 nodeId)
{
    if (!player)
        return;

    if (!IsEnabled())
    {
        SendError(player, "星脉衍化功能已关闭，请联系管理员。");
        return;
    }

    auto nodeItr = StarVeinNodeStore.find(nodeId);
    if (nodeItr == StarVeinNodeStore.end())
    {
        SendError(player, "星脉节点不存在或已被移除。");
        return;
    }

    StarVeinNode const& node = nodeItr->second;
    std::unordered_set<uint32> const& lit = GetLitNodes(player);
    uint8 status = GetNodeStatus(node, lit);

    if (status == 2)
    {
        SendError(player, "该节点已经点亮。");
        return;
    }
    if (status != 1)
    {
        SendError(player, "请先点亮同一条星脉左侧的前置节点。");
        return;
    }

    if (node.NeedTempId && !sHelper->CheckNeed(player, node.NeedTempId, "星脉衍化点亮"))
    {
        // 需求检查函数已经给出缺料提示，这里补发一次状态刷新让面板解除等待。
        SendState(player);
        return;
    }

    if (!SaveNode(player, node))
    {
        LOG_ERROR("server.worldserver", "星脉衍化点亮存档写入失败 player={} node={}",
            player->GetName(), node.Id);
        SendError(player, "点亮失败：存档写入异常，请联系管理员。");
        return;
    }

    _litCache[player->GetGUID().GetCounter()].insert(node.Id);

    if (node.NeedTempId)
        sHelper->DeleteCheckNeed(player, node.NeedTempId, "星脉衍化点亮");

    if (node.SpellId && sSpellMgr->GetSpellInfo(node.SpellId) && !player->HasAura(node.SpellId))
        player->AddAura(node.SpellId, player);

    player->SendSysMsg("星脉衍化：成功点亮【%s】。", node.Name.c_str());
    SendState(player);
}

bool StarVeinMgr::HandleAddonMessage(Player* player, std::string const& message)
{
    if (!player || message.empty())
        return false;

    if (message == "Q")
    {
        SendState(player);
        return true;
    }

    if (message.rfind("A|", 0) == 0)
    {
        auto parsedNodeId = Acore::StringTo<uint32>(message.substr(2));
        if (parsedNodeId)
            ActivateNode(player, *parsedNodeId);
        return true;
    }

    return false;
}

class star_vein_player_script : public PlayerScript
{
public:
    star_vein_player_script() : PlayerScript("star_vein_player_script") { }

    void OnLogin(Player* player) override
    {
        // 登录时补挂已点亮节点的常驻光环，缓存同时在这里建立。
        sStarVeinMgr->ApplyPlayerAuras(player);
    }

    void OnLogout(Player* player) override
    {
        sStarVeinMgr->ClearPlayerCache(player);
    }

    // StarVein 插件用 SendAddonMessage(..., "WHISPER", 自己) 与服务器通讯，
    // 该路径会走到私聊白名单校验，这里按前缀分发 XMYH 请求。
    bool CanPlayerUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        if (type != CHAT_MSG_WHISPER || language != LANG_ADDON || !player || receiver != player)
            return true;

        std::size_t tab = msg.find('\t');
        if (tab == std::string::npos || msg.compare(0, tab, STAR_VEIN_ADDON_PREFIX) != 0)
            return true;

        sStarVeinMgr->HandleAddonMessage(player, msg.substr(tab + 1));
        return true;
    }
};

void AddSC_StarVeinMgr()
{
    new star_vein_player_script();
}
