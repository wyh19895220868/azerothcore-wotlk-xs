-- Keep the NAz ObjectMgr field layout available after the core migration to `id` and `creature_multispawn`.
-- `id1` is a virtual alias of the canonical base entry; `id2` and `id3` cache the first two variants.
--
-- NOTE (2026-10-20): this file was applied partially on a fresh install (the ALTER + UPDATE went
-- through, the CREATE TRIGGER failed on a binlog/SUPER error), so it is now written to be
-- re-runnable: the ALTER is guarded, the triggers are dropped before being created.

SET @naz_has_id1 := (
    SELECT COUNT(*) FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'creature' AND COLUMN_NAME = 'id1'
);

SET @naz_alter := IF(@naz_has_id1 = 0,
    'ALTER TABLE `creature`
       ADD COLUMN `id1` int unsigned GENERATED ALWAYS AS (`id`) VIRTUAL AFTER `id`,
       ADD COLUMN `id2` int unsigned NOT NULL DEFAULT 0 AFTER `id1`,
       ADD COLUMN `id3` int unsigned NOT NULL DEFAULT 0 AFTER `id2`,
       ADD KEY `idx_naz_compat_creature_id1` (`id1`)',
    'DO 0');

PREPARE naz_stmt FROM @naz_alter;
EXECUTE naz_stmt;
DEALLOCATE PREPARE naz_stmt;

UPDATE `creature`
SET `id2` = COALESCE((
      SELECT `entry` FROM `creature_multispawn` AS `cms`
      WHERE `cms`.`spawnId` = `creature`.`guid`
      ORDER BY `cms`.`entry` LIMIT 1 OFFSET 0
    ), 0),
    `id3` = COALESCE((
      SELECT `entry` FROM `creature_multispawn` AS `cms`
      WHERE `cms`.`spawnId` = `creature`.`guid`
      ORDER BY `cms`.`entry` LIMIT 1 OFFSET 1
    ), 0)
WHERE `id` IN (SELECT `entry` FROM `creature_template`)
  AND `guid` IN (SELECT `spawnId` FROM `creature_multispawn`);

DROP TRIGGER IF EXISTS `naz_compat_creature_multispawn_after_insert`;

CREATE TRIGGER `naz_compat_creature_multispawn_after_insert`
AFTER INSERT ON `creature_multispawn`
FOR EACH ROW
UPDATE `creature`
SET `id3` = CASE
      WHEN `id2` = 0 THEN 0
      WHEN NEW.`entry` < `id2` THEN `id2`
      WHEN `id3` = 0 OR NEW.`entry` < `id3` THEN NEW.`entry`
      ELSE `id3`
    END,
    `id2` = CASE
      WHEN `id2` = 0 OR NEW.`entry` < `id2` THEN NEW.`entry`
      ELSE `id2`
    END
WHERE `guid` = NEW.`spawnId`
  AND `id` IN (SELECT `entry` FROM `creature_template`);

DROP TRIGGER IF EXISTS `naz_compat_creature_multispawn_after_delete`;

CREATE TRIGGER `naz_compat_creature_multispawn_after_delete`
AFTER DELETE ON `creature_multispawn`
FOR EACH ROW
UPDATE `creature`
SET `id2` = CASE WHEN `id2` = OLD.`entry` THEN `id3` ELSE `id2` END,
    `id3` = CASE WHEN `id2` = `id3` OR `id3` = OLD.`entry` THEN 0 ELSE `id3` END
WHERE `guid` = OLD.`spawnId`
  AND `id` IN (SELECT `entry` FROM `creature_template`);
