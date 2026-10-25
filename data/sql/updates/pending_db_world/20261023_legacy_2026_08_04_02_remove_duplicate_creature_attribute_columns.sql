-- 将旧生物属性字段迁移到统一的新字段后删除，避免同一功能重复配置。
DELIMITER $$

DROP PROCEDURE IF EXISTS `remove_duplicate_creature_attribute_columns`$$
CREATE PROCEDURE `remove_duplicate_creature_attribute_columns`()
BEGIN
    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '血量比例'
    ) AND EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '血量倍率'
    ) THEN
        UPDATE `__模版生物属性表`
        SET `血量倍率` = IF(`血量倍率` = 1 AND `血量比例` <> 1, `血量比例`, `血量倍率`),
            `法力倍率` = IF(`法力倍率` = 1 AND `蓝量比例` <> 1, `蓝量比例`, `法力倍率`),
            `最小伤害倍率` = IF(`最小伤害倍率` = 1 AND `物理伤害比例` <> 1, `物理伤害比例`, `最小伤害倍率`),
            `最大伤害倍率` = IF(`最大伤害倍率` = 1 AND `物理伤害比例` <> 1, `物理伤害比例`, `最大伤害倍率`),
            `伤害减免百分比` = IF(`伤害减免百分比` = 0 AND `免伤比例` > 0, ROUND(LEAST(`免伤比例`, 1) * 100), `伤害减免百分比`),
            `攻击速度值` = IF(`攻击速度值` = 0, GREATEST(`攻速`, 0), `攻击速度值`),
            `官方掉落` = IF(`是否覆盖原掉落` <> 0, 0, `官方掉落`);
    END IF;

    IF EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '血量比例') THEN
        ALTER TABLE `__模版生物属性表` DROP COLUMN `血量比例`;
    END IF;
    IF EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '蓝量比例') THEN
        ALTER TABLE `__模版生物属性表` DROP COLUMN `蓝量比例`;
    END IF;
    IF EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '物理伤害比例') THEN
        ALTER TABLE `__模版生物属性表` DROP COLUMN `物理伤害比例`;
    END IF;
    IF EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '免伤比例') THEN
        ALTER TABLE `__模版生物属性表` DROP COLUMN `免伤比例`;
    END IF;
    IF EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '攻速') THEN
        ALTER TABLE `__模版生物属性表` DROP COLUMN `攻速`;
    END IF;
    IF EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '是否覆盖原掉落') THEN
        ALTER TABLE `__模版生物属性表` DROP COLUMN `是否覆盖原掉落`;
    END IF;
END$$

CALL `remove_duplicate_creature_attribute_columns`()$$
DROP PROCEDURE `remove_duplicate_creature_attribute_columns`$$

DELIMITER ;
