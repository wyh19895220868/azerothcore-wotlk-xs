#ifndef ITEM_ATTRIBUTES_DISPLAY_H
#define ITEM_ATTRIBUTES_DISPLAY_H

#include "ItemAttributesLoader.h"
#include "ItemAttributesEffects.h"
#include "Player.h"
#include "Item.h"

// 物品属性显示处理器
class ItemAttributesDisplay
{
public:
    static ItemAttributesDisplay* instance();

    // 初始化显示处理器
    void Initialize();

    // 【根本性修复】检查是否已初始化
    bool IsInitialized() const { return _isInitialized; }

    // 添加物品属性到提示信息
    void AddItemAttributesToTooltip(Player* player, Item* item, std::vector<std::string>& lines);

    // 生成物品属性描述
    std::string GenerateItemAttributesDescription(Item* item);

    // 生成物品属性颜色代码
    std::string GetAttributeColorCode(ItemAttributeTemplate const* attributeTemplate);

private:
    ItemAttributesDisplay() : _isInitialized(false) {}
    ~ItemAttributesDisplay() = default;

    // 【根本性修复】初始化状态标志
    bool _isInitialized;
};

#define sItemAttributesDisplay ItemAttributesDisplay::instance()

#endif // ITEM_ATTRIBUTES_DISPLAY_H
