-- Restore the race/class level stats read by the NAz compatibility loader.
-- The current world base stores class stats and race offsets separately.
-- Death knights have class stats from level 55; use warrior values for levels 1-54,
-- matching the compatibility rows in player_classlevelstats.

CREATE TABLE IF NOT EXISTS `player_levelstats` (
  `race` tinyint unsigned NOT NULL,
  `class` tinyint unsigned NOT NULL,
  `level` tinyint unsigned NOT NULL,
  `str` int unsigned NOT NULL DEFAULT 0,
  `agi` int unsigned NOT NULL DEFAULT 0,
  `sta` int unsigned NOT NULL DEFAULT 0,
  `inte` int unsigned NOT NULL DEFAULT 0,
  `spi` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`race`, `class`, `level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `player_levelstats`;
INSERT INTO `player_levelstats` (`race`, `class`, `level`, `str`, `agi`, `sta`, `inte`, `spi`)
SELECT pci.`race`, pci.`class`, pcl.`level`,
       CAST(pcs.`Strength` AS SIGNED) + prs.`Strength`,
       CAST(pcs.`Agility` AS SIGNED) + prs.`Agility`,
       CAST(pcs.`Stamina` AS SIGNED) + prs.`Stamina`,
       CAST(pcs.`Intellect` AS SIGNED) + prs.`Intellect`,
       CAST(pcs.`Spirit` AS SIGNED) + prs.`Spirit`
FROM `playercreateinfo` AS pci
JOIN `player_race_stats` AS prs ON prs.`Race` = pci.`race`
JOIN `player_classlevelstats` AS pcl ON pcl.`class` = pci.`class`
JOIN `player_class_stats` AS pcs
  ON pcs.`Class` = IF(pci.`class` = 6 AND pcl.`level` < 55, 1, pci.`class`)
 AND pcs.`Level` = pcl.`level`;
