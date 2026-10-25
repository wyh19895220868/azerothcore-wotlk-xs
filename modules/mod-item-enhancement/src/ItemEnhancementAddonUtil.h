#include "AsInt256.h"
#include "StringConvert.h"
/*
 * ItemEnhancementAddonUtil.h - Addon 消息处理辅助工具
 *
 * 提供消息解析、数据序列化、响应构建等辅助功能
 */

#ifndef ITEM_ENHANCEMENT_ADDON_UTIL_H
#define ITEM_ENHANCEMENT_ADDON_UTIL_H

#include "Player.h"
#include "Item.h"
#include <string>
#include <vector>
#include <map>
#include <sstream>

namespace AddonUtil
{
    // ========================================
    // 消息前缀常量
    // ========================================
    namespace Prefix
    {
        constexpr char ENHANCEMENT[] = "IENH";
        constexpr char GROWTH[] = "IGRO";
        constexpr char RUNE[] = "IRUN";
        constexpr char TOOLTIP[] = "UITQ";
        constexpr char MAP_PROGRESS[] = "MPRG";
        constexpr char GUILD_BATTLE[] = "GBAT";
        constexpr char PLAYER_BATTLE[] = "PBAT";
        constexpr char HARVEST[] = "HARV";
    }

    // ========================================
    // 配置常量
    // ========================================
    namespace Config
    {
        constexpr uint32 MAX_BATCH_SIZE = 50;        // 批量查询最大数量
        constexpr uint32 MAX_MESSAGE_LENGTH = 255;   // 消息最大长度
        constexpr uint32 BATCH_COOLDOWN_MS = 1000;   // 批量查询冷却（毫秒）
    }

    // ========================================
    // 字符串处理工具
    // ========================================

    /**
     * @brief 分割字符串
     * @param str 要分割的字符串
     * @param delimiter 分隔符
     * @return std::vector<std::string> 分割后的字符串数组
     */
    std::vector<std::string> SplitString(std::string const& str, char delimiter);

    /**
     * @brief 安全地将字符串转换为数字
     * @tparam T 目标数字类型
     * @param str 字符串
     * @param defaultValue 转换失败时的默认值
     * @return T 转换后的数字
     */
    template<typename T>
    T SafeStringToNumber(std::string const& str, T defaultValue = 0)
    {
        try
        {
            if (str.empty())
                return defaultValue;

            if constexpr (std::is_same_v<T, uint32>)
                return static_cast<uint32>(std::stoul(str));
            else if constexpr (std::is_same_v<T, int32>)
                return static_cast<int32>(std::stoi(str));
            else if constexpr (std::is_same_v<T, uint64>)
                return static_cast<uint64>(std::stoull(str));
            else if constexpr (std::is_same_v<T, AsInt64>)
                return static_cast<AsInt64>(AsInt64FromString(str));
            else if constexpr (std::is_same_v<T, float>)
                return static_cast<float>(std::stof(str));
            else if constexpr (std::is_same_v<T, double>)
                return static_cast<double>(std::stod(str));
            else
                return defaultValue;
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    /**
     * @brief 连接字符串数组
     * @param parts 字符串数组
     * @param delimiter 分隔符
     * @return std::string 连接后的字符串
     */
    std::string JoinString(std::vector<std::string> const& parts, char delimiter);

    /**
     * @brief 转义特殊字符
     * @param str 原始字符串
     * @return std::string 转义后的字符串
     */
    std::string EscapeString(std::string const& str);

    // ========================================
    // 消息构建工具
    // ========================================

    /**
     * @brief 消息构建器类
     */
    class MessageBuilder
    {
    public:
        MessageBuilder() = default;
        explicit MessageBuilder(std::string const& command);

        // 添加参数
        MessageBuilder& AddParam(std::string const& value);
        MessageBuilder& AddParam(uint32 value);
        MessageBuilder& AddParam(int32 value);
        MessageBuilder& AddParam(uint64 value);
        MessageBuilder& AddParam(float value);
        MessageBuilder& AddParam(bool value);

        // 添加键值对
        MessageBuilder& AddKeyValue(std::string const& key, std::string const& value);
        MessageBuilder& AddKeyValue(std::string const& key, uint32 value);
        MessageBuilder& AddKeyValue(std::string const& key, int32 value);
        MessageBuilder& AddKeyValue(std::string const& key, float value);
        MessageBuilder& AddKeyValue(std::string const& key, bool value);

        // 构建消息
        std::string Build() const;

        // 清空
        void Clear();

    private:
        std::ostringstream m_stream;
        bool m_firstParam = true;
    };

    /**
     * @brief 批量响应构建器
     */
    class BatchResponseBuilder
    {
    public:
        BatchResponseBuilder() = default;

        // 添加一个响应项
        void AddItem(std::string const& data);

        // 构建批量响应
        std::string Build() const;

        // 获取项数量
        size_t GetCount() const { return m_items.size(); }

        // 清空
        void Clear();

    private:
        std::vector<std::string> m_items;
    };

    // ========================================
    // 消息解析工具
    // ========================================

    /**
     * @brief 消息解析器类
     */
    class MessageParser
    {
    public:
        explicit MessageParser(std::string const& message);

        // 获取命令
        std::string GetCommand() const;

        // 获取参数数量
        size_t GetParamCount() const;

        // 获取参数（字符串）
        std::string GetParam(size_t index, std::string const& defaultValue = "") const;

        // 获取参数（数字）
        template<typename T>
        T GetParamAs(size_t index, T defaultValue = 0) const
        {
            if (index >= m_params.size())
                return defaultValue;
            return SafeStringToNumber<T>(m_params[index], defaultValue);
        }

        // 检查参数是否存在
        bool HasParam(size_t index) const;

        // 获取所有参数
        std::vector<std::string> const& GetAllParams() const { return m_params; }

    private:
        std::vector<std::string> m_params;
    };

    // ========================================
    // 数据序列化工具
    // ========================================

    /**
     * @brief 物品数据序列化器
     */
    class ItemDataSerializer
    {
    public:
        explicit ItemDataSerializer(Item* item);

        // 添加基本信息
        ItemDataSerializer& AddBasicInfo();

        // 添加强化信息
        ItemDataSerializer& AddEnhancementInfo();

        // 添加成长信息
        ItemDataSerializer& AddGrowthInfo();

        // 添加符文信息
        ItemDataSerializer& AddRuneInfo();

        // 添加附魔信息
        ItemDataSerializer& AddEnchantInfo();

        // 添加随机属性
        ItemDataSerializer& AddRandomProperties();

        // 构建数据字符串
        std::string Build() const;

    private:
        Item* m_item;
        std::map<std::string, std::string> m_data;
    };

    // ========================================
    // 物品查找工具
    // ========================================

    /**
     * @brief 通过 GUID 查找物品
     */
    Item* FindItemByGuid(Player* player, uint32 guidLow);

    /**
     * @brief 通过位置查找物品
     */
    Item* FindItemByLocation(Player* player, uint32 itemId, int32 bag, int32 slot, bool equipped);

    /**
     * @brief 获取玩家所有物品的 GUID 列表
     */
    std::vector<uint32> GetAllItemGuids(Player* player);

    // ========================================
    // 消息发送工具
    // ========================================

    /**
     * @brief 发送 Addon 消息给玩家
     */
    void SendAddonMessage(Player* player, std::string const& prefix, std::string const& message);

    /**
     * @brief 发送错误消息
     */
    void SendError(Player* player, std::string const& prefix, std::string const& error);

    /**
     * @brief 发送成功消息
     */
    void SendSuccess(Player* player, std::string const& prefix, std::string const& message);

    // ========================================
    // 调试工具
    // ========================================

    /**
     * @brief 记录调试日志
     */
    void LogDebug(std::string const& message);

    /**
     * @brief 记录消息日志
     */
    void LogMessage(std::string const& prefix, std::string const& message, bool incoming = true);

    /**
     * @brief 启用/禁用调试模式
     */
    void SetDebugMode(bool enabled);

    /**
     * @brief 检查是否启用调试模式
     */
    bool IsDebugMode();

} // namespace AddonUtil

#endif // ITEM_ENHANCEMENT_ADDON_UTIL_H
