
#ifndef _MAPAREAFUNC_H
#define _MAPAREAFUNC_H

enum AreaFuncEnum
{
    NullPtr,                        //不作改变
    Sanctuary,                   //安全地区
    AllsAttck,                   //全体攻击
    GroupAttck,                   //组队攻击
    GuildAttck,                   //公会攻击
};

struct AreaFuncTemp
{
    AreaFuncEnum FuncEnum;                                                              //设置属性
    bool AllowIn;                                                                             //允许进入
    bool AllowDuel;                                                                             //允许决斗
    bool AllowGroup;                                                                             //允许组队
    bool AllowMount;                                                                             //允许陆地坐骑
    bool AllowFlyMount;                                                                             //允许飞行坐骑
    uint32 AGraveyardTempId;                                                                        //联盟墓地
    uint32 HGraveyardTempId;                                                                        //部落墓地
    std::set<uint32> DisSpells;                                                    //禁止施放技能
    std::set<uint32> DisAuras;                                                    //强制光环无效
    std::string InMsg;                                            //进入提示内容
};
typedef std::unordered_map<int32, AreaFuncTemp> AreaFuncTempMap;
extern AreaFuncTempMap AreaFuncTempStore;

class MapAreaFunc
{
public:
    static MapAreaFunc* Instance()
    {
        static MapAreaFunc Instance;
        return &Instance;
    }

    void LoadMapAreaFuncData();
    bool AllowGroupHandler(Player* player);
    bool AllowDuelHandler(Player* player);
    bool AllowMountHandler(Player* player);
    bool AllowFlyMountHandler(Player* player);
    bool AllowCastSpellIdHandler(Unit* Caster, uint32 SpellId);
    bool AllowAuraIdHandler(Player* player, uint32 AuraId);

    bool IsDisAuraFunc(Player* player);
};
#define sMapAreaFunc MapAreaFunc::Instance()

#endif
