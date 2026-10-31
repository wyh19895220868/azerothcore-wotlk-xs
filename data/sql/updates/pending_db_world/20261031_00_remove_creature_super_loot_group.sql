-- 移除废弃的超级掉落组：整表删除，并清掉属性模板上的四个超级掉落字段。
DROP TABLE IF EXISTS `__生物超级掉落组表`;

SET @super_loot_columns := (
    SELECT GROUP_CONCAT(CONCAT('DROP COLUMN `', COLUMN_NAME, '`') ORDER BY ORDINAL_POSITION SEPARATOR ', ')
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
        AND TABLE_NAME = '__模版生物属性表'
        AND COLUMN_NAME IN ('超级掉落组号', '超级掉落几率', '超级掉落抽取数量Min', '超级掉落抽取数量Max')
);

SET @drop_super_loot := IF(@super_loot_columns IS NULL, 'DO 0', CONCAT('ALTER TABLE `__模版生物属性表` ', @super_loot_columns));

PREPARE drop_super_loot FROM @drop_super_loot;
EXECUTE drop_super_loot;
DEALLOCATE PREPARE drop_super_loot;
