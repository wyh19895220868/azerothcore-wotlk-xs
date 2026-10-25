-- 回滚 2026-07-18 挑战副本生物控制组相关数据库结构。
-- 可重复执行；同时兼容“生物控制组”和“生物控制组ID”两个字段名。

DELIMITER $$

DROP PROCEDURE IF EXISTS `回滚挑战副本字段`$$
CREATE PROCEDURE `回滚挑战副本字段`()
BEGIN
    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = '_模板_地图生物控制'
          AND COLUMN_NAME = '挑战副本'
    ) THEN
        ALTER TABLE `_模板_地图生物控制` DROP COLUMN `挑战副本`;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = '_模板_地图生物控制'
          AND COLUMN_NAME = '生物控制组'
    ) THEN
        ALTER TABLE `_模板_地图生物控制` DROP COLUMN `生物控制组`;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = '_模板_地图生物控制'
          AND COLUMN_NAME = '随机权重'
    ) THEN
        ALTER TABLE `_模板_地图生物控制` DROP COLUMN `随机权重`;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.STATISTICS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = '_模板_地图生物控制'
          AND INDEX_NAME = '地图区域难度'
    ) THEN
        ALTER TABLE `_模板_地图生物控制` DROP INDEX `地图区域难度`;
    END IF;

    ALTER TABLE `_模板_地图生物控制`
        ADD KEY `地图区域难度` (`地图ID`, `区域ID`, `难度`);

    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = '_挑战副本配置表'
          AND COLUMN_NAME = '生物控制组'
    ) THEN
        ALTER TABLE `_挑战副本配置表` DROP COLUMN `生物控制组`;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE()
          AND TABLE_NAME = '_挑战副本配置表'
          AND COLUMN_NAME = '生物控制组ID'
    ) THEN
        ALTER TABLE `_挑战副本配置表` DROP COLUMN `生物控制组ID`;
    END IF;
END$$

CALL `回滚挑战副本字段`()$$
DROP PROCEDURE `回滚挑战副本字段`$$

DELIMITER ;
