-- Store exact 256-bit decimal values; preserve existing defaults and column comments.

ALTER TABLE `item_template`
    MODIFY COLUMN `stat_value1` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value2` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value3` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value4` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value5` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value6` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value7` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value8` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value9` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value10` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `dmg_min1` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `dmg_max1` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `dmg_min2` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `dmg_max2` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `armor` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `creature`
    MODIFY COLUMN `curhealth` varchar(80) NOT NULL DEFAULT '1',
    MODIFY COLUMN `curmana` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `__模版生物属性表`
    MODIFY COLUMN `固定生命值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `法力值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `受到的伤害固定值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `造成的伤害固定值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `真实伤害值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `物理伤害Min` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `物理伤害Max` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `技能法术伤害Min` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `技能法术伤害Max` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `攻击强度值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `法术强度值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `护甲` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_模板_地图生物控制`
    MODIFY COLUMN `战力门槛` varchar(80) NOT NULL DEFAULT '0' COMMENT '0=不限制,>0=最低战力并作为动态难度计算基准';

ALTER TABLE `_模板_物品成长`
    MODIFY COLUMN `属性值1` varchar(80) NOT NULL DEFAULT '0' COMMENT '随机属性最小值',
    MODIFY COLUMN `属性值2` varchar(80) NOT NULL DEFAULT '0' COMMENT '随机属性最大值',
    MODIFY COLUMN `属性百分比1` varchar(80) NOT NULL DEFAULT '0' COMMENT '随机属性最小百分比',
    MODIFY COLUMN `属性百分比2` varchar(80) NOT NULL DEFAULT '0' COMMENT '随机属性最大百分比',
    MODIFY COLUMN `达到该成长等级后的奖励属性` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `达到该成长等级后的奖励属性百分比` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_模板_物品强化`
    MODIFY COLUMN `属性值1` varchar(80) NOT NULL DEFAULT '0' COMMENT '随机属性最小值',
    MODIFY COLUMN `属性值2` varchar(80) NOT NULL DEFAULT '0' COMMENT '随机属性最大值',
    MODIFY COLUMN `属性百分比1` varchar(80) NOT NULL DEFAULT '0' COMMENT '随机属性最小百分比',
    MODIFY COLUMN `属性百分比2` varchar(80) NOT NULL DEFAULT '0' COMMENT '随机属性最大百分比',
    MODIFY COLUMN `达到该强化等级后的奖励属性` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `达到该强化等级后的奖励属性百分比` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_模板_物品鉴定`
    MODIFY COLUMN `成长属性最小属性值` varchar(80) NOT NULL DEFAULT '0' COMMENT '成长属性单条最小属性值',
    MODIFY COLUMN `成长属性最大属性值` varchar(80) NOT NULL DEFAULT '0' COMMENT '成长属性单条最大属性值',
    MODIFY COLUMN `强化属性最小属性值` varchar(80) NOT NULL DEFAULT '0' COMMENT '强化属性单条最小属性值',
    MODIFY COLUMN `强化属性最大属性值` varchar(80) NOT NULL DEFAULT '0' COMMENT '强化属性单条最大属性值',
    MODIFY COLUMN `基础最小属性值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `基础最大属性值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `追加属性最小值` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `追加属性最大值` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `_模板_物品属性`
    MODIFY COLUMN `属性最小百分比` varchar(80) NOT NULL DEFAULT '100' COMMENT '属性值的最小百分比',
    MODIFY COLUMN `属性最大百分比` varchar(80) NOT NULL DEFAULT '100' COMMENT '属性值的最大百分比';
