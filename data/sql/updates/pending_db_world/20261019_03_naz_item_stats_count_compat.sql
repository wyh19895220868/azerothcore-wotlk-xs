-- Restore the legacy item stat count required by the NAz and playerbot loaders.

SET @naz_stats_count_exists := (
    SELECT COUNT(*)
    FROM information_schema.columns
    WHERE table_schema = DATABASE()
      AND table_name = 'item_template'
      AND column_name = 'StatsCount'
);
SET @naz_stats_count_sql := IF(
    @naz_stats_count_exists = 0,
    'ALTER TABLE `item_template` ADD COLUMN `StatsCount` tinyint unsigned NOT NULL DEFAULT 0 AFTER `ContainerSlots`',
    'SELECT 1'
);
PREPARE naz_stats_count_stmt FROM @naz_stats_count_sql;
EXECUTE naz_stats_count_stmt;
DEALLOCATE PREPARE naz_stats_count_stmt;

UPDATE `item_template`
SET `StatsCount` = CASE
    WHEN `stat_type10` <> 0 OR `stat_value10` <> 0 THEN 10
    WHEN `stat_type9` <> 0 OR `stat_value9` <> 0 THEN 9
    WHEN `stat_type8` <> 0 OR `stat_value8` <> 0 THEN 8
    WHEN `stat_type7` <> 0 OR `stat_value7` <> 0 THEN 7
    WHEN `stat_type6` <> 0 OR `stat_value6` <> 0 THEN 6
    WHEN `stat_type5` <> 0 OR `stat_value5` <> 0 THEN 5
    WHEN `stat_type4` <> 0 OR `stat_value4` <> 0 THEN 4
    WHEN `stat_type3` <> 0 OR `stat_value3` <> 0 THEN 3
    WHEN `stat_type2` <> 0 OR `stat_value2` <> 0 THEN 2
    WHEN `stat_type1` <> 0 OR `stat_value1` <> 0 THEN 1
    ELSE 0
END;
