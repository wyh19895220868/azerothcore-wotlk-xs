-- Add mythic-level matching to the map creature attribute control table.

DELIMITER $$

DROP PROCEDURE IF EXISTS `add_map_creature_mythic_difficulty`$$
CREATE PROCEDURE `add_map_creature_mythic_difficulty`()
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

CALL `add_map_creature_mythic_difficulty`()$$
DROP PROCEDURE `add_map_creature_mythic_difficulty`$$

DELIMITER ;
