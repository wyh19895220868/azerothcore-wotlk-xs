
#ifndef _CLASSMOD_H
#define _CLASSMOD_H

#include "../_Helper/AsInt64.h"

typedef std::unordered_map<uint32, uint32> ClassConfigMap;
extern ClassConfigMap ClassConfigStore;

struct ClassTemp
{
    float BlockNow;                    //固定格挡
    float BlockMax;                    //格挡上限
    float BlockMod;                   //格挡倍率
    float ParryNow;                    //固定招架
    float ParryMax;                    //招架上限
    float ParryMod;                    //招架倍率
    float DodgeNow;                    //固定闪避
    float DodgeMax;                    //闪避上限
    float DodgeMod;                    //闪避倍率
    float CritNow;                    //固定暴击
    float CritMax;                    //暴击上限
    float CritMod;                    //暴击倍率
    int32 RxNow;                    //固定韧性
    int32 RxMax;                    //韧性上限
    float RxMod;                    //韧性倍率
    int32 ArrNow;                    //固定护甲
    int32 ArrMax;                    //护甲上限
    float ArrMod;                    //护甲倍率
    int32 PreNow;                    //固定精准
    int32 PreMax;                    //精准上限
    float PreMod;                    //精准倍率
    AsInt64 HpNow;                    //固定血量
    AsInt64 HpMin;                    //血量下限
    AsInt64 HpMax;                    //血量上限
    float HpMod;                    //血量倍率
    int32 JsNow;                    //固定急速
    int32 JsMax;                    //急速上限
    float JsMod;                    //急速倍率
    AsInt64 DmgNow;                    //固定伤害
    AsInt64 DmgMax;                    //伤害上限
    float MPVEMod;                    //PVE物理伤害倍率
    float SPVEMod;                    //PVE技能伤害倍率
    float MPVPMod;                    //PVP物理伤害倍率
    float SPVPMod;                    //PVP技能伤害倍率
    float PVERet;                    //PVE免伤率
    float PVPRet;                    //PVP免伤率
    AsInt64 HeNow;                    //固定治疗
    AsInt64 HeMax;                    //治疗上限
    float HeMod;                    //治疗倍率
    AsInt64 ManaNow;                    //固定蓝量
    AsInt64 ManaMin;                    //血量下限
    AsInt64 ManaMax;                    //血量上限
    float ManaMod;                    //血量倍率
    AsInt64 St1Now;                    //固定力量
    AsInt64 St1Min;                    //力量下限
    AsInt64 St2Now;                    //固定敏捷
    AsInt64 St2Min;                    //敏捷下限
    AsInt64 St3Now;                    //固定耐力
    AsInt64 St3Min;                    //耐力下限
    AsInt64 St4Now;                    //固定智力
    AsInt64 St4Min;                    //智力下限
    AsInt64 St5Now;                    //固定精神
    AsInt64 St5Min;                    //精神下限
    AsInt64 MinWpNow;                    //固定最小武器伤害
    AsInt64 MaxWpNow;                    //固定最大武器伤害
    AsInt64 ApNow;                    //固定攻强强度
    AsInt64 SpNow;                    //固定法术强度
    AsInt64 EpNow;                    //固定治疗效果
    float St4SPMod;                    //智力加法术强度倍率
    float St4HEMod;                    //智力加治疗效果倍率
    float St5SPMod;                    //精神加法术强度倍率
    float St5HEMod;                    //精神加治疗效果倍率
    float PVEDot;                    //PVE的Dot
    float PVPDot;                    //PVP的Dot
};

typedef std::unordered_map<uint32, ClassTemp> ClassTempMap;
extern ClassTempMap ClassTempStore;

struct HRTemp
{
    uint32 NTempId;                    //需求模版
    uint32 RTempId;                    //奖励模版
    std::string CTitle;
};

typedef std::unordered_map<uint32, HRTemp> HRTempMap;
extern HRTempMap HRStore;

typedef std::unordered_map<uint32, AsInt64> StatPtConfigMap;
extern StatPtConfigMap StatPtConfigStore;

struct StatPtRankTemp
{
    AsInt64 NeedXp;                    //需要经验
    AsInt64 RewCount;                    //奖励点数
};
typedef std::map<uint32, StatPtRankTemp> StatPtRankMap;
extern StatPtRankMap StatPtRankStore;

struct StatPtRewTemp
{
    uint32 ToRank;                    //直达等级
    AsInt64 RewXp;                    //奖励经验
    bool AddRank;                    //是否加1级
};
typedef std::unordered_map<uint32, StatPtRewTemp> StatPtRewMap;
extern StatPtRewMap StatPtRewStore;

struct OnMapAreaTemp
{
    int32 MapOrArea;
    uint32 Class;
    uint32 TempId;
};
typedef std::vector<OnMapAreaTemp> OnMapAreaTempVect;
extern OnMapAreaTempVect OnMapAreaStore;

class CustomClass
{
public:
    static CustomClass* Instance()
    {
        static CustomClass Instance;
        return &Instance;
    }

    void LoadCustomClass();

    float GetMPVEModForClass(Player* player);
    float GetSPVEModForClass(Player* player);
    float GetMPVPModForClass(Player* player);
    float GetSPVPModForClass(Player* player);
    float GetPVERetModForClass(Player* player);
    float GetPVPRetModForClass(Player* player);
    float GetHeModForClass(Player* player);

    float GetBlockNow(Player* player);
    float GetBlockMax(Player* player);
    float GetBlockMod(Player* player);

    float GetParryNow(Player* player);
    float GetParryMax(Player* player);
    float GetParryMod(Player* player);

    float GetDodgeNow(Player* player);
    float GetDodgeMax(Player* player);
    float GetDodgeMod(Player* player);

    float GetCritNow(Player* player);
    float GetCritMax(Player* player);
    float GetCritMod(Player* player);

    int32 GetRxNow(Player* player);
    int32 GetRxMax(Player* player);
    float GetRxMod(Player* player);

    int32 GetArrNow(Player* player);
    int32 GetArrMax(Player* player);
    float GetArrMod(Player* player);

    int32 GetPreNow(Player* player);
    int32 GetPreMax(Player* player);
    float GetPreMod(Player* player);

    uint32 GetHpNow(Player* player);
    AsInt64 GetAsHpNow(Player* player);
    uint32 GetHpMin(Player* player);
    AsInt64 GetAsHpMin(Player* player);
    uint32 GetHpMax(Player* player);
    AsInt64 GetAsHpMax(Player* player);
    float GetHpMod(Player* player);

    int32 GetJsNow(Player* player);
    int32 GetJsMax(Player* player);
    float GetJsMod(Player* player);

    uint32 GetDmgNow(Player* player);
    AsInt64 GetAsDmgNow(Player* player);
    uint32 GetDmgMax(Player* player);
    AsInt64 GetAsDmgMax(Player* player);

    uint32 GetHeNow(Player* player);
    AsInt64 GetAsHeNow(Player* player);
    uint32 GetHeMax(Player* player);
    AsInt64 GetAsHeMax(Player* player);

    uint32 GetManaNow(Player* player);
    AsInt64 GetAsManaNow(Player* player);
    uint32 GetManaMin(Player* player);
    AsInt64 GetAsManaMin(Player* player);
    uint32 GetManaMax(Player* player);
    AsInt64 GetAsManaMax(Player* player);
    float GetManaMod(Player* player);

    int32 GetSt1Now(Player* player);
    int32 GetSt1Min(Player* player);
    int32 GetSt2Now(Player* player);
    int32 GetSt2Min(Player* player);
    int32 GetSt3Now(Player* player);
    int32 GetSt3Min(Player* player);
    int32 GetSt4Now(Player* player);
    int32 GetSt4Min(Player* player);
    int32 GetSt5Now(Player* player);
    int32 GetSt5Min(Player* player);
    AsInt64 GetAsSt1Now(Player* player);
    AsInt64 GetAsSt1Min(Player* player);
    AsInt64 GetAsSt2Now(Player* player);
    AsInt64 GetAsSt2Min(Player* player);
    AsInt64 GetAsSt3Now(Player* player);
    AsInt64 GetAsSt3Min(Player* player);
    AsInt64 GetAsSt4Now(Player* player);
    AsInt64 GetAsSt4Min(Player* player);
    AsInt64 GetAsSt5Now(Player* player);
    AsInt64 GetAsSt5Min(Player* player);

    int32 GetMinWpNow(Player* player);
    int32 GetMaxWpNow(Player* player);
    AsInt64 GetAsMinWpNow(Player* player);
    AsInt64 GetAsMaxWpNow(Player* player);

    int32 GetApNow(Player* player);
    int32 GetSpNow(Player* player);
    int32 GetEpNow(Player* player);
    AsInt64 GetAsApNow(Player* player);
    AsInt64 GetAsSpNow(Player* player);
    AsInt64 GetAsEpNow(Player* player);

    void OnPlayerMapChanged(Player* player);
    void OnPlayerUpdateZone(Player* player, uint32 newZone, uint32 newArea);

    uint32 GetHRNTempId(uint32 TitleId);
    uint32 GetHRRTempId(uint32 TitleId);
    uint32 GetHRTiteId(Player* player);

    uint32 AotoTempAt(uint32 Try, bool Src);
    uint32 GetStatConfigCount(uint32 Num);
    AsInt64 GetStatConfigValue(uint32 Num);
    void ApplyPlayerStat(Player* player, uint32 Try, AsInt64 const& Value, bool Apply);
    void SendUpdatePlayerStatData(Player* player);
    bool IsCheckReset(Player* player);
    AsInt64 GetStatConfigMax(uint32 Try);
    uint32 GetStatMaxRank();
    AsInt64 GetStatNeedXp(uint32 Rank);
    AsInt64 GetStatNextRewCount(uint32 Rank);

    AsInt64 GetStatOldNewRewCount(uint32 ORank, uint32 NRank);
    AsInt64 GetStatRewCount(uint32 Rank);
    AsInt64 GetStatAllsCount(uint32 NowRank);

    void SendConfigStatData(Player* player);
    uint32 GetStatTempId(int32 MapId, int32 AreaId, uint32 Class, uint32 VipRank);
    bool InNowMapOrArea(Player* player);

    float GetSt4SPMod(Player* player);
    float GetSt4HEMod(Player* player);
    float GetSt5SPMod(Player* player);
    float GetSt5HEMod(Player* player);
    float GetPVEDotMod(Player* player);
    float GetPVPDotMod(Player* player);

    void OnHookSpellDamageDot(Player* player, Unit* victim, float &Dmg);

private:

};
#define sCustomClass CustomClass::Instance()

#endif
