#include "AsInt256.h"
#include "StringConvert.h"
#ifndef ITEM_ATTRIBUTES_GENERATOR_H
#define ITEM_ATTRIBUTES_GENERATOR_H

#include "ItemAttributesLoader.h"
#include "Player.h"
#include "Item.h"
#include <vector>

// 物品属性生成选项
struct ItemAttributeGenerateOptions
{
    uint32 minAttributes;        // 最小属性数量
    uint32 maxAttributes;        // 最大属性数量
    uint32 attributeGroup;       // 属性组（0表示所有组）
    AsInt64 minItemLevel;         // 最小物品等级 / 属性值范围最小值
    AsInt64 maxItemLevel;         // 最大物品等级 / 属性值范围最大值
    bool respectChance;          // 是否考虑属性获取几率
    bool allowDuplicateTypes;    // 是否允许重复属性类型
    bool useValueRangeFilter;    // 是否启用属性值范围过滤
    AttributeCategory category;  // 属性类别（基础或追加）

    ItemAttributeGenerateOptions() : minAttributes(1), maxAttributes(1), attributeGroup(0),
        minItemLevel(0), maxItemLevel(0), respectChance(true), allowDuplicateTypes(false),
        useValueRangeFilter(false), category(AttributeCategory::ADDITIONAL) {}
};

// 物品属性随机生成器
class ItemAttributesGenerator
{
public:
    static ItemAttributesGenerator* instance();

    // 初始化生成器
    void Initialize();

    // 【根本性修复】检查是否已初始化
    bool IsInitialized() const { return _isInitialized; }

    // 随机生成物品属性
    bool GenerateRandomAttributes(Item* item, ItemAttributeGenerateOptions const& options);

    // 根据物品等级生成属性
    bool GenerateAttributesByItemLevel(Item* item);

    // 根据物品品质生成属性
    bool GenerateAttributesByItemQuality(Item* item);

    // 获取适合物品的属性列表
    std::vector<ItemAttributeTemplate const*> GetSuitableAttributes(Item* item, uint32 attributeGroup = 0);

private:
    ItemAttributesGenerator() : _isInitialized(false) {}
    ~ItemAttributesGenerator() = default;

    // 【根本性修复】初始化状态标志
    bool _isInitialized;

    // 检查属性是否适合物品
    bool IsAttributeSuitableForItem(Item* item, ItemAttributeTemplate const* attributeTemplate);
};

#define sItemAttributesGenerator ItemAttributesGenerator::instance()

#endif // ITEM_ATTRIBUTES_GENERATOR_H
