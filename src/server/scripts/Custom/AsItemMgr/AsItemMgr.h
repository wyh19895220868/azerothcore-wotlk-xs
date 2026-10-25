
#ifndef _ASITEMMGR_H
#define _ASITEMMGR_H

#define DEF_2 -2                        //个人银行
#define DEF_3 -3                        //装备修理
#define DEF_4 -4                        //武器熟练
#define DEF_5 -5                        //自杀解卡
#define DEF_6 -6                        //泡点
#define DEF_7 -7                        //会员提升
#define DEF_8 -8                        //卡战
#define DEF_9 -9                        //副本重置
#define DEF_10 -10                        //充值领取
#define DEF_11 -11                        //狂暴之力
#define DEF_12 -12                        //会员积分查询
#define DEF_13 -13                        //重置所有副本
#define DEF_14 -14                        //赞助等级自动化给会员

#define C_NOT_FB 30                        //不属于副本

class AsItemMgr
{
public:
    static AsItemMgr* Instance()
    {
        static AsItemMgr Instance;
        return &Instance;
    }

private:

};
#define sAsItemMgr AsItemMgr::Instance()

#endif //!
