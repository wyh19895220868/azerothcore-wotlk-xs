-- 256位数值补充迁移：把实际按 AsInt256 读取的属性字段保存为十进制文本。
-- 标识、数量、等级、时间和倍率字段保持原有整数或浮点类型。

ALTER TABLE `pet_levelstats`
    MODIFY COLUMN `hp` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `mana` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `armor` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `str` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `agi` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `sta` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `inte` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `spi` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `min_dmg` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `max_dmg` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `player_classlevelstats`
    MODIFY COLUMN `basehp` varchar(80) NOT NULL DEFAULT '1',
    MODIFY COLUMN `basemana` varchar(80) NOT NULL DEFAULT '1';

ALTER TABLE `creature_classlevelstats`
    MODIFY COLUMN `basehp0` varchar(80) NOT NULL DEFAULT '1',
    MODIFY COLUMN `basehp1` varchar(80) NOT NULL DEFAULT '1',
    MODIFY COLUMN `basehp2` varchar(80) NOT NULL DEFAULT '1',
    MODIFY COLUMN `basemana` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `basearmor` varchar(80) NOT NULL DEFAULT '1',
    MODIFY COLUMN `attackpower` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `rangedattackpower` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `damage_base` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `damage_exp1` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `damage_exp2` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `__模版玩家属性表`
    MODIFY COLUMN `固定护甲` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `护甲上限` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定血量` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `血量下限` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `血量上限` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定伤害` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `伤害上限` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定治疗` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `治疗上限` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定蓝量` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `蓝量下限` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `蓝量上限` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定力量` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `力量下限` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定敏捷` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `敏捷下限` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定耐力` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `耐力下限` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定智力` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `智力下限` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定精神` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `精神下限` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定武器最小伤害` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定武器最大伤害` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定攻击强度` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定法术强度` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `固定治疗效果` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_事件`
    MODIFY COLUMN `获得奖励最小伤害值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `获得奖励最小治疗值` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_战场控制表`
    MODIFY COLUMN `需要伤害量` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `需要治疗量` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_技能`
    MODIFY COLUMN `固定伤害` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_技能光环控制表`
    MODIFY COLUMN `附带真实伤害值` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_技能加强关联控制表`
    MODIFY COLUMN `全局伤害加值` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_技能特殊吸血控制表`
    MODIFY COLUMN `PVE固定吸血` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `PVP固定吸血` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_生物伤害排行榜表`
    MODIFY COLUMN `最低伤害要求` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_装备超级随机属性倍率`
    MODIFY COLUMN `最小倍率` varchar(80) NOT NULL DEFAULT '1',
    MODIFY COLUMN `最大倍率` varchar(80) NOT NULL DEFAULT '1';

ALTER TABLE `_装备超级随机属性组表`
    MODIFY COLUMN `最小值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `最大值` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_装备超级随机模板总表`
    MODIFY COLUMN `武器最小伤害Min` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `武器最小伤害Max` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `武器最大伤害Min` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `武器最大伤害Max` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_物品属性奖励表`
    MODIFY COLUMN `法力值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `生命值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `敏捷` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `力量` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `智力` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `精神` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `耐力` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `防御等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `躲闪等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `招架等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `格挡等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `近战命中等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `远程命中等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `法术命中等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `近战暴击等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `远程暴击等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `法术暴击等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `近战命中躲闪等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `远程命中躲闪等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `法术命中躲闪等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `近战暴击躲闪等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `远程暴击躲闪等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `法术暴击躲闪等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `近战急速等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `远程急速等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `法术急速等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `命中等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `暴击等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `命中躲闪等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `暴击躲闪等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `韧性等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `急速等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `精准等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `攻击强度` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `远程攻击强度` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `形态攻击强度` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `法术治疗效果` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `法术伤害效果` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `每5秒回复法力` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `护甲穿透等级` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `法术强度提高` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `每5秒回复生命` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `法术穿透` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `格挡值提高` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_斗气配置表`
    MODIFY COLUMN `功能值` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_斗气等级经验表`
    MODIFY COLUMN `需要经验` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `奖励斗气点数` varchar(80) NOT NULL DEFAULT '1';

ALTER TABLE `_斗气奖励表`
    MODIFY COLUMN `奖励经验` varchar(80) NOT NULL DEFAULT '0';
