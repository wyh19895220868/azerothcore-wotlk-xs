-- Restore the legacy creature scale field required by the NAz loader.

SET @naz_scale_exists := (
    SELECT COUNT(*)
    FROM information_schema.columns
    WHERE table_schema = DATABASE()
      AND table_name = 'creature_template'
      AND column_name = 'scale'
);
SET @naz_scale_sql := IF(
    @naz_scale_exists = 0,
    'ALTER TABLE `creature_template` ADD COLUMN `scale` float NOT NULL DEFAULT 1 AFTER `detection_range`',
    'SELECT 1'
);
PREPARE naz_scale_stmt FROM @naz_scale_sql;
EXECUTE naz_scale_stmt;
DEALLOCATE PREPARE naz_scale_stmt;

UPDATE `creature_template` AS ct
LEFT JOIN `creature_template_model` AS ctm
    ON ctm.`CreatureID` = ct.`entry` AND ctm.`Idx` = 0
SET ct.`scale` = COALESCE(ctm.`DisplayScale`, 1);
