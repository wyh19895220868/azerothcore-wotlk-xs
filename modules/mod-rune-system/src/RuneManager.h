#ifndef RUNE_MANAGER_H
#define RUNE_MANAGER_H

#include "RuneTypes.h"
#include "Player.h"
#include "Item.h"
#include "RuneSystem.h"

class RuneManager
{
public:
    static RuneManager* instance();

    // 初始化符文管理器
    void Initialize();

    // 创建符文
    uint32 CreateRune(const std::string& name, uint32 runeSlot, uint32 runeQuality,
                      uint32 spellId, uint32 cooldown, uint32 procChance, uint32 effectType);

    // 修改符文
    bool ModifyRune(uint32 runeId, const std::string& name, uint32 runeSlot, uint32 runeQuality,
                    uint32 spellId, uint32 cooldown, uint32 procChance, uint32 effectType);

    // 删除符文
    bool DeleteRune(uint32 runeId);

    // 为物品添加符文插槽
    bool AddRuneSlotToItem(Item* item, uint32 slotCount = 1);

    // 解锁物品的符文插槽
    bool UnlockRuneSlot(Item* item, uint32 slotId);

    // 获取物品的符文插槽数量
    uint32 GetItemRuneSlotCount(Item* item);

    // 获取物品的已解锁符文插槽数量
    uint32 GetItemUnlockedRuneSlotCount(Item* item);

    // 检查物品是否可以添加符文
    bool CanAddRuneToItem(Item* item);

    // 获取符文列表
    std::vector<CustomRuneInfo> GetRuneList(uint32 runeSlot = 0, uint32 runeQuality = 0);

private:
    RuneManager() {}
    ~RuneManager() {}

    static RuneManager* _instance;
};

#define sRuneManager RuneManager::instance()

#endif // RUNE_MANAGER_H
