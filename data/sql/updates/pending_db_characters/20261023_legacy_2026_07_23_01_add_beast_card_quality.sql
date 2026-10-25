-- 为已有神兽翻牌存档增加品质等级；可重复执行。
DELIMITER $$

DROP PROCEDURE IF EXISTS `add_beast_card_quality_column`$$
CREATE PROCEDURE `add_beast_card_quality_column`()
BEGIN
    IF EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = '_神兽翻牌存档'
    ) AND NOT EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = '_神兽翻牌存档'
          AND COLUMN_NAME = '品质等级'
    ) THEN
        ALTER TABLE `_神兽翻牌存档`
            ADD COLUMN `品质等级` tinyint unsigned NOT NULL DEFAULT '0'
            COMMENT '0D灰 1C白 2B绿 3A蓝 4S紫 5SS橙 6SSR暗金 7SSS红 8SR粉 9UR彩色'
            AFTER `卡牌编号`;
    END IF;
END$$

CALL `add_beast_card_quality_column`()$$
DROP PROCEDURE `add_beast_card_quality_column`$$

DELIMITER ;
