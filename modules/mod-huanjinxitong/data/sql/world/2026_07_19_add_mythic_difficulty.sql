-- 为地图生物属性控制增加秘境等级筛选；可重复执行。

DELIMITER $$

DROP PROCEDURE IF EXISTS `增加地图生物秘境难度`$$
CREATE PROCEDURE `增加地图生物秘境难度`()
BEGIN
    IF EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = '_模板_地图生物控制'
    ) AND NOT EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = '_模板_地图生物控制'
          AND COLUMN_NAME = '秘境难度'
    ) THEN
        ALTER TABLE `_模板_地图生物控制`
            ADD COLUMN `秘境难度` varchar(255) NOT NULL DEFAULT '0'
            COMMENT '秘境等级(0/空/全部=所有秘境等级，多个等级用逗号隔开)'
            AFTER `难度`;
    END IF;
END$$

CALL `增加地图生物秘境难度`()$$
DROP PROCEDURE `增加地图生物秘境难度`$$

DELIMITER ;
