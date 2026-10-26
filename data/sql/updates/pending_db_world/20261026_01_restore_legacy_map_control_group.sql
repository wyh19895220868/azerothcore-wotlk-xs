-- Restore the map-control group column required by the current HuanJingSystem module.

DELIMITER $$

DROP PROCEDURE IF EXISTS `restore_legacy_map_control_group`$$
CREATE PROCEDURE `restore_legacy_map_control_group`()
BEGIN
    IF EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = '_模板_地图生物控制'
    ) AND NOT EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = '_模板_地图生物控制'
          AND COLUMN_NAME = '生物控制组'
    ) THEN
        ALTER TABLE `_模板_地图生物控制`
            ADD COLUMN `生物控制组` int unsigned NOT NULL DEFAULT 0
            COMMENT '关联 _生物控制组表.组号；每只生物只随机一次'
            AFTER `秘境难度`;
    END IF;
END$$

CALL `restore_legacy_map_control_group`()$$
DROP PROCEDURE `restore_legacy_map_control_group`$$

DELIMITER ;

UPDATE `_模板_地图生物控制`
SET `生物控制组` = CASE `配置ID`
    WHEN 1 THEN 45
    WHEN 2 THEN 44
    WHEN 3 THEN 1
    WHEN 4 THEN 1
    WHEN 5 THEN 2
    ELSE `生物控制组`
END
WHERE `配置ID` IN (1, 2, 3, 4, 5)
  AND `生物控制组` = 0;
