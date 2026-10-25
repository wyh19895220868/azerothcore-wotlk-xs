
#ifndef _ItemPlayerStatMod_H
#define _ItemPlayerStatMod_H

#include "../_Helper/AsInt64.h"

struct ZsStatTemp
{
    bool OnDel;                    //是否消耗
    AsInt64 S00;                    //法力值
    AsInt64 S01;                    //生命值
    AsInt64 S03;                   //敏捷
    AsInt64 S04;                    //力量
    AsInt64 S05;                    //智力
    AsInt64 S06;                    //精神
    AsInt64 S07;                    //耐力
    AsInt64 S12;                    //防御等级
    AsInt64 S13;                    //躲闪等级
    AsInt64 S14;                    //招架等级
    AsInt64 S15;                    //格挡等级
    AsInt64 S16;                    //近战命中等级
    AsInt64 S17;                    //远程命中等级
    AsInt64 S18;                    //法术命中等级
    AsInt64 S19;                    //近战暴击等级
    AsInt64 S20;                    //远程暴击等级
    AsInt64 S21;                    //法术暴击等级
    AsInt64 S22;                    //近战命中躲闪等级
    AsInt64 S23;                    //远程命中躲闪等级
    AsInt64 S24;                    //法术命中躲闪等级
    AsInt64 S25;                    //近战暴击躲闪等级
    AsInt64 S26;                    //远程暴击躲闪等级
    AsInt64 S27;                    //法术暴击躲闪等级
    AsInt64 S28;                    //近战急速等级
    AsInt64 S29;                    //远程急速等级
    AsInt64 S30;                    //法术急速等级
    AsInt64 S31;                    //命中等级
    AsInt64 S32;                    //暴击等级
    AsInt64 S33;                    //命中躲闪等级
    AsInt64 S34;                    //暴击躲闪等级
    AsInt64 S35;                    //韧性等级
    AsInt64 S36;                    //急速等级
    AsInt64 S37;                    //精准等级
    AsInt64 S38;                    //攻击强度
    AsInt64 S39;                    //远程攻击强度
    AsInt64 S40;                    //形态攻击强度
    AsInt64 S41;                    //法术治疗效果
    AsInt64 S42;                    //法术伤害效果
    AsInt64 S43;                    //每5秒回复法力
    AsInt64 S44;                    //护甲穿透等级
    AsInt64 S45;                    //法术强度提高
    AsInt64 S46;                    //每5秒回复生命
    AsInt64 S47;                    //法术穿透
    AsInt64 S48;                    //格挡值提高
};

typedef std::unordered_map<uint32, ZsStatTemp> ZsStatTempMap;
extern ZsStatTempMap ZsStatTempStore;

class ItemPlayerStatMod
{
public:
    static ItemPlayerStatMod* Instance()
    {
        static ItemPlayerStatMod Instance;
        return &Instance;
    }

    void LoadItemPlayerStatMod();
    bool OnItemUseNow(Player* player, Item* PItem);
    void OnPlayerLoginLoadStat(Player* player);

private:

};
#define sItemPlayerStatMod ItemPlayerStatMod::Instance()

#endif
