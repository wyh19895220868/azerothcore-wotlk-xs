#ifndef _STARVEINMGR_H
#define _STARVEINMGR_H

#include "Common.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Player;

// __控制总表 序号：1=允许入口与点亮，0=关闭入口及新操作（已有存档与光环保留）
constexpr uint32 STAR_VEIN_FEATURE_CONFIG_ID = 1426;
constexpr uint8 STAR_VEIN_BRANCH_COUNT = 4;
constexpr uint8 STAR_VEIN_NODE_PER_BRANCH = 7;
constexpr uint32 STAR_VEIN_MAX_NODE_ID = 407;
// 单条插件消息正文上限，留出 "XMYH<TAB>T|节点|键|" 头部余量，避免超过 255 字节被客户端截断。
constexpr std::size_t STAR_VEIN_ADDON_CHUNK = 160;

struct StarVeinNode
{
    uint32 Id = 0;
    uint8 Branch = 1;
    uint8 Order = 1;
    uint32 ParentId = 0;
    bool Resonance = false;
    bool Enabled = false;
    uint32 NeedTempId = 0;
    uint32 SpellId = 0;
    std::string Name;
    std::string Description;
    std::string Material;
};

using StarVeinNodeMap = std::unordered_map<uint32, StarVeinNode>;
extern StarVeinNodeMap StarVeinNodeStore;

class StarVeinMgr
{
public:
    static StarVeinMgr* Instance()
    {
        static StarVeinMgr instance;
        return &instance;
    }

    void LoadConfig(bool isReload = false);
    void Reload();

    bool HandleAddonMessage(Player* player, std::string const& message);
    void SendState(Player* player);
    void ActivateNode(Player* player, uint32 nodeId);
    void ApplyPlayerAuras(Player* player);
    void ClearPlayerCache(Player* player);

private:
    bool IsEnabled() const;
    uint32 GetChainNodeId(uint8 branch, uint8 order) const;
    std::unordered_set<uint32> const& GetLitNodes(Player* player);
    void LoadLitNodes(Player* player, std::unordered_set<uint32>& lit) const;
    uint8 GetNodeStatus(StarVeinNode const& node, std::unordered_set<uint32> const& lit) const;
    bool SaveNode(Player* player, StarVeinNode const& node) const;
    void SendFrame(Player* player, std::string const& payload) const;
    void SendError(Player* player, std::string const& message) const;
    void SendNodeText(Player* player, uint32 nodeId, char const* key, std::string const& text) const;

    // 在线角色已点亮节点缓存，登录时加载，点亮后增量更新，登出清理。
    std::unordered_map<uint32, std::unordered_set<uint32>> _litCache;
};

#define sStarVeinMgr StarVeinMgr::Instance()

#endif
