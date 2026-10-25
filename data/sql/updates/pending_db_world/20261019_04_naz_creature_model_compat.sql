-- Restore the legacy creature model columns required by the NAz loader and hot reload helpers.

SET @naz_modelid1_exists := (
    SELECT COUNT(*)
    FROM information_schema.columns
    WHERE table_schema = DATABASE()
      AND table_name = 'creature_template'
      AND column_name = 'modelid1'
);
SET @naz_modelid1_sql := IF(
    @naz_modelid1_exists = 0,
    'ALTER TABLE `creature_template` ADD COLUMN `modelid1` int unsigned NOT NULL DEFAULT 0 AFTER `KillCredit2`',
    'SELECT 1'
);
PREPARE naz_modelid1_stmt FROM @naz_modelid1_sql;
EXECUTE naz_modelid1_stmt;
DEALLOCATE PREPARE naz_modelid1_stmt;

SET @naz_modelid2_exists := (
    SELECT COUNT(*)
    FROM information_schema.columns
    WHERE table_schema = DATABASE()
      AND table_name = 'creature_template'
      AND column_name = 'modelid2'
);
SET @naz_modelid2_sql := IF(
    @naz_modelid2_exists = 0,
    'ALTER TABLE `creature_template` ADD COLUMN `modelid2` int unsigned NOT NULL DEFAULT 0 AFTER `modelid1`',
    'SELECT 1'
);
PREPARE naz_modelid2_stmt FROM @naz_modelid2_sql;
EXECUTE naz_modelid2_stmt;
DEALLOCATE PREPARE naz_modelid2_stmt;

SET @naz_modelid3_exists := (
    SELECT COUNT(*)
    FROM information_schema.columns
    WHERE table_schema = DATABASE()
      AND table_name = 'creature_template'
      AND column_name = 'modelid3'
);
SET @naz_modelid3_sql := IF(
    @naz_modelid3_exists = 0,
    'ALTER TABLE `creature_template` ADD COLUMN `modelid3` int unsigned NOT NULL DEFAULT 0 AFTER `modelid2`',
    'SELECT 1'
);
PREPARE naz_modelid3_stmt FROM @naz_modelid3_sql;
EXECUTE naz_modelid3_stmt;
DEALLOCATE PREPARE naz_modelid3_stmt;

SET @naz_modelid4_exists := (
    SELECT COUNT(*)
    FROM information_schema.columns
    WHERE table_schema = DATABASE()
      AND table_name = 'creature_template'
      AND column_name = 'modelid4'
);
SET @naz_modelid4_sql := IF(
    @naz_modelid4_exists = 0,
    'ALTER TABLE `creature_template` ADD COLUMN `modelid4` int unsigned NOT NULL DEFAULT 0 AFTER `modelid3`',
    'SELECT 1'
);
PREPARE naz_modelid4_stmt FROM @naz_modelid4_sql;
EXECUTE naz_modelid4_stmt;
DEALLOCATE PREPARE naz_modelid4_stmt;

UPDATE `creature_template` AS ct
LEFT JOIN `creature_template_model` AS ctm1
    ON ctm1.`CreatureID` = ct.`entry` AND ctm1.`Idx` = 0
LEFT JOIN `creature_template_model` AS ctm2
    ON ctm2.`CreatureID` = ct.`entry` AND ctm2.`Idx` = 1
LEFT JOIN `creature_template_model` AS ctm3
    ON ctm3.`CreatureID` = ct.`entry` AND ctm3.`Idx` = 2
LEFT JOIN `creature_template_model` AS ctm4
    ON ctm4.`CreatureID` = ct.`entry` AND ctm4.`Idx` = 3
SET ct.`modelid1` = COALESCE(ctm1.`CreatureDisplayID`, 0),
    ct.`modelid2` = COALESCE(ctm2.`CreatureDisplayID`, 0),
    ct.`modelid3` = COALESCE(ctm3.`CreatureDisplayID`, 0),
    ct.`modelid4` = COALESCE(ctm4.`CreatureDisplayID`, 0);
