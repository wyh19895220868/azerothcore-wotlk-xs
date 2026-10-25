-- NAz custom core schema compatibility.
-- Applied by AzerothCore's pending database updater.

ALTER TABLE `item_template`
    MODIFY COLUMN `stat_value1` bigint NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value2` bigint NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value3` bigint NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value4` bigint NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value5` bigint NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value6` bigint NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value7` bigint NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value8` bigint NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value9` bigint NOT NULL DEFAULT '0',
    MODIFY COLUMN `stat_value10` bigint NOT NULL DEFAULT '0',
    MODIFY COLUMN `dmg_min1` bigint unsigned NOT NULL DEFAULT '0',
    MODIFY COLUMN `dmg_max1` bigint unsigned NOT NULL DEFAULT '0',
    MODIFY COLUMN `dmg_min2` bigint unsigned NOT NULL DEFAULT '0',
    MODIFY COLUMN `dmg_max2` bigint unsigned NOT NULL DEFAULT '0',
    MODIFY COLUMN `armor` bigint unsigned NOT NULL DEFAULT '0';
