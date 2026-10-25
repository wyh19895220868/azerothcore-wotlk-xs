-- NAz custom core schema compatibility.
-- Applied by AzerothCore's pending database updater.

-- MySQL 8.0.12 does not support ADD COLUMN IF NOT EXISTS. The updater records
-- this file in `updates`, so it is applied once after the column is added.
ALTER TABLE `characters`
    ADD COLUMN `reincarnationLv` int unsigned NOT NULL DEFAULT '0' COMMENT '转生等级' AFTER `level`;

ALTER TABLE `characters`
    MODIFY COLUMN `money` bigint unsigned NOT NULL DEFAULT '0',
    MODIFY COLUMN `health` bigint unsigned NOT NULL DEFAULT '0',
    MODIFY COLUMN `power1` bigint unsigned NOT NULL DEFAULT '0',
    MODIFY COLUMN `power2` bigint unsigned NOT NULL DEFAULT '0',
    MODIFY COLUMN `power3` bigint unsigned NOT NULL DEFAULT '0',
    MODIFY COLUMN `power4` bigint unsigned NOT NULL DEFAULT '0',
    MODIFY COLUMN `power5` bigint unsigned NOT NULL DEFAULT '0',
    MODIFY COLUMN `power6` bigint unsigned NOT NULL DEFAULT '0',
    MODIFY COLUMN `power7` bigint unsigned NOT NULL DEFAULT '0';
