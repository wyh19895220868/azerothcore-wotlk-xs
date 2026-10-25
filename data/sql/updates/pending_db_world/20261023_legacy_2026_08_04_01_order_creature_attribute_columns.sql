-- 调整 __模版生物属性表 的最终显示顺序；可在重复字段清理前后重复执行。
DELIMITER $$

DROP PROCEDURE IF EXISTS `order_creature_attribute_columns`$$
CREATE PROCEDURE `order_creature_attribute_columns`()
BEGIN
    IF EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表'
    ) AND EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '鉴定物品等级'
    ) THEN
        ALTER TABLE `__模版生物属性表`
            MODIFY COLUMN `模版编号` int(11) NOT NULL DEFAULT '0' FIRST,
            MODIFY COLUMN `备注` varchar(255) NOT NULL DEFAULT '' AFTER `模版编号`,
            MODIFY COLUMN `生物等级` int(11) NOT NULL DEFAULT '80' AFTER `备注`,
            MODIFY COLUMN `生物等级范围最小` int(11) NOT NULL DEFAULT '-1' COMMENT '-1=使用生物等级字段' AFTER `生物等级`,
            MODIFY COLUMN `生物等级范围最大` int(11) NOT NULL DEFAULT '-1' COMMENT '-1=使用生物等级字段' AFTER `生物等级范围最小`,
            MODIFY COLUMN `血条数量` int(11) NOT NULL DEFAULT '1' COMMENT '1=原始 以此类推' AFTER `生物等级范围最大`,
            MODIFY COLUMN `固定生命值` decimal(65,0) NOT NULL DEFAULT '0' AFTER `血条数量`,
            MODIFY COLUMN `血量倍率` double unsigned NOT NULL DEFAULT '1' AFTER `固定生命值`,
            MODIFY COLUMN `法力倍率` double unsigned NOT NULL DEFAULT '1' AFTER `血量倍率`,
            MODIFY COLUMN `法力值` decimal(65,0) unsigned NOT NULL DEFAULT '0' AFTER `法力倍率`,
            MODIFY COLUMN `受到的伤害固定值` decimal(65,0) NOT NULL DEFAULT '0' AFTER `法力值`,
            MODIFY COLUMN `伤害减免百分比` bigint(20) NOT NULL DEFAULT '0' AFTER `受到的伤害固定值`,
            MODIFY COLUMN `切割比例` float NOT NULL DEFAULT '0' AFTER `伤害减免百分比`,
            MODIFY COLUMN `造成的伤害固定值` decimal(65,0) NOT NULL DEFAULT '0' AFTER `切割比例`,
            MODIFY COLUMN `真实伤害值` decimal(65,0) unsigned NOT NULL DEFAULT '0' AFTER `造成的伤害固定值`,
            MODIFY COLUMN `物理伤害Min` decimal(65,0) NOT NULL DEFAULT '0' AFTER `真实伤害值`,
            MODIFY COLUMN `物理伤害Max` decimal(65,0) NOT NULL DEFAULT '0' AFTER `物理伤害Min`,
            MODIFY COLUMN `最小伤害倍率` double unsigned NOT NULL DEFAULT '1' AFTER `物理伤害Max`,
            MODIFY COLUMN `最大伤害倍率` double unsigned NOT NULL DEFAULT '1' AFTER `最小伤害倍率`,
            MODIFY COLUMN `技能法术伤害Min` decimal(65,0) NOT NULL DEFAULT '0' AFTER `最大伤害倍率`,
            MODIFY COLUMN `技能法术伤害Max` decimal(65,0) NOT NULL DEFAULT '0' AFTER `技能法术伤害Min`,
            MODIFY COLUMN `技能法术比例` float NOT NULL DEFAULT '1' AFTER `技能法术伤害Max`,
            MODIFY COLUMN `攻击强度倍率` double unsigned NOT NULL DEFAULT '1' AFTER `技能法术比例`,
            MODIFY COLUMN `攻击强度值` decimal(65,0) unsigned NOT NULL DEFAULT '0' AFTER `攻击强度倍率`,
            MODIFY COLUMN `法术强度倍率` double unsigned NOT NULL DEFAULT '1' AFTER `攻击强度值`,
            MODIFY COLUMN `法术强度值` decimal(65,0) unsigned NOT NULL DEFAULT '0' AFTER `法术强度倍率`,
            MODIFY COLUMN `护甲` decimal(65,0) NOT NULL DEFAULT '0' AFTER `法术强度值`,
            MODIFY COLUMN `护甲倍率` double unsigned NOT NULL DEFAULT '1' AFTER `护甲`,
            MODIFY COLUMN `神圣抗性` int(11) NOT NULL DEFAULT '0' AFTER `护甲倍率`,
            MODIFY COLUMN `火焰抗性` int(11) NOT NULL DEFAULT '0' AFTER `神圣抗性`,
            MODIFY COLUMN `火抗倍率` double unsigned NOT NULL DEFAULT '1' AFTER `火焰抗性`,
            MODIFY COLUMN `自然抗性` int(11) NOT NULL DEFAULT '0' AFTER `火抗倍率`,
            MODIFY COLUMN `自然抗倍率` double unsigned NOT NULL DEFAULT '1' AFTER `自然抗性`,
            MODIFY COLUMN `冰霜抗性` int(11) NOT NULL DEFAULT '0' AFTER `自然抗倍率`,
            MODIFY COLUMN `冰霜抗倍率` double unsigned NOT NULL DEFAULT '1' AFTER `冰霜抗性`,
            MODIFY COLUMN `暗影抗性` int(11) NOT NULL DEFAULT '0' AFTER `冰霜抗倍率`,
            MODIFY COLUMN `暗影抗倍率` double unsigned NOT NULL DEFAULT '1' AFTER `暗影抗性`,
            MODIFY COLUMN `奥术抗性` int(11) NOT NULL DEFAULT '0' AFTER `暗影抗倍率`,
            MODIFY COLUMN `奥术抗倍率` double unsigned NOT NULL DEFAULT '1' AFTER `奥术抗性`,
            MODIFY COLUMN `攻击速度倍率` float(10,2) unsigned NOT NULL DEFAULT '1.00' AFTER `奥术抗倍率`,
            MODIFY COLUMN `攻击速度值` int(10) unsigned NOT NULL DEFAULT '0' AFTER `攻击速度倍率`,
            MODIFY COLUMN `施法速度倍率` float(10,2) unsigned NOT NULL DEFAULT '1.00' AFTER `攻击速度值`,
            MODIFY COLUMN `施法速度值` int(10) unsigned NOT NULL DEFAULT '0' AFTER `施法速度倍率`,
            MODIFY COLUMN `移动速度倍率` float(10,2) unsigned NOT NULL DEFAULT '1.00' AFTER `施法速度值`,
            MODIFY COLUMN `移动速度值` int(10) unsigned NOT NULL DEFAULT '0' AFTER `移动速度倍率`,
            MODIFY COLUMN `命中率加成` float(10,2) NOT NULL DEFAULT '0.00' AFTER `移动速度值`,
            MODIFY COLUMN `护甲穿透值` int(10) unsigned NOT NULL DEFAULT '0' AFTER `命中率加成`,
            MODIFY COLUMN `护甲穿透百分比` float(10,2) unsigned NOT NULL DEFAULT '0.00' AFTER `护甲穿透值`,
            MODIFY COLUMN `格挡穿透值` int(10) unsigned NOT NULL DEFAULT '0' AFTER `护甲穿透百分比`,
            MODIFY COLUMN `格挡穿透百分比` float(10,2) unsigned NOT NULL DEFAULT '0.00' AFTER `格挡穿透值`,
            MODIFY COLUMN `生物光环组` int(11) NOT NULL DEFAULT '0' COMMENT '__模版技能光环' AFTER `格挡穿透百分比`,
            MODIFY COLUMN `生物光环几率` float NOT NULL DEFAULT '100' AFTER `生物光环组`,
            MODIFY COLUMN `生物光环数量` int(11) NOT NULL DEFAULT '0' AFTER `生物光环几率`,
            MODIFY COLUMN `生物固定光环` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开' AFTER `生物光环数量`,
            MODIFY COLUMN `官方掉落` tinyint(3) unsigned NOT NULL DEFAULT '1' AFTER `生物固定光环`,
            MODIFY COLUMN `额外掉落编号` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开' AFTER `官方掉落`,
            MODIFY COLUMN `超级掉落组号` int(11) NOT NULL DEFAULT '0' COMMENT '对应 __生物超级掉落组表' AFTER `额外掉落编号`,
            MODIFY COLUMN `超级掉落几率` float NOT NULL DEFAULT '100' COMMENT '这个几率决定是否掉落' AFTER `超级掉落组号`,
            MODIFY COLUMN `超级掉落抽取数量Min` int(11) NOT NULL DEFAULT '1' COMMENT '组里面按组那边的几率出来的多少个物品' AFTER `超级掉落几率`,
            MODIFY COLUMN `超级掉落抽取数量Max` int(11) NOT NULL DEFAULT '1' COMMENT '组里面按组那边的几率出来的多少个物品' AFTER `超级掉落抽取数量Min`,
            MODIFY COLUMN `装备属性倍率` float(10,2) unsigned NOT NULL DEFAULT '1.00' AFTER `超级掉落抽取数量Max`,
            MODIFY COLUMN `装备属性值` int(10) unsigned NOT NULL DEFAULT '0' AFTER `装备属性倍率`,
            MODIFY COLUMN `装备属性倍率模式` tinyint(4) NOT NULL DEFAULT '0' AFTER `装备属性值`,
            MODIFY COLUMN `鉴定组` int(10) unsigned NOT NULL DEFAULT '0' AFTER `装备属性倍率模式`,
            MODIFY COLUMN `鉴定品质` tinyint(3) unsigned NOT NULL DEFAULT '0' AFTER `鉴定组`,
            MODIFY COLUMN `鉴定物品等级` int(10) unsigned NOT NULL DEFAULT '0' AFTER `鉴定品质`;
    END IF;
END$$

CALL `order_creature_attribute_columns`()$$
DROP PROCEDURE `order_creature_attribute_columns`$$

DELIMITER ;
