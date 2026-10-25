-- Restore legacy creature_template fields still read by the NAz compatibility loader.

SET @naz_trainer_type_sql := IF(
    EXISTS (
        SELECT 1
        FROM information_schema.columns
        WHERE table_schema = DATABASE()
          AND table_name = 'creature_template'
          AND column_name = 'trainer_type'
    ),
    'SELECT 1',
    'ALTER TABLE `creature_template` ADD COLUMN `trainer_type` tinyint unsigned NOT NULL DEFAULT 0 AFTER `family`'
);
PREPARE naz_trainer_type_stmt FROM @naz_trainer_type_sql;
EXECUTE naz_trainer_type_stmt;
DEALLOCATE PREPARE naz_trainer_type_stmt;

SET @naz_trainer_spell_sql := IF(
    EXISTS (
        SELECT 1
        FROM information_schema.columns
        WHERE table_schema = DATABASE()
          AND table_name = 'creature_template'
          AND column_name = 'trainer_spell'
    ),
    'SELECT 1',
    'ALTER TABLE `creature_template` ADD COLUMN `trainer_spell` int unsigned NOT NULL DEFAULT 0 AFTER `trainer_type`'
);
PREPARE naz_trainer_spell_stmt FROM @naz_trainer_spell_sql;
EXECUTE naz_trainer_spell_stmt;
DEALLOCATE PREPARE naz_trainer_spell_stmt;

SET @naz_trainer_class_sql := IF(
    EXISTS (
        SELECT 1
        FROM information_schema.columns
        WHERE table_schema = DATABASE()
          AND table_name = 'creature_template'
          AND column_name = 'trainer_class'
    ),
    'SELECT 1',
    'ALTER TABLE `creature_template` ADD COLUMN `trainer_class` tinyint unsigned NOT NULL DEFAULT 0 AFTER `trainer_spell`'
);
PREPARE naz_trainer_class_stmt FROM @naz_trainer_class_sql;
EXECUTE naz_trainer_class_stmt;
DEALLOCATE PREPARE naz_trainer_class_stmt;

SET @naz_trainer_race_sql := IF(
    EXISTS (
        SELECT 1
        FROM information_schema.columns
        WHERE table_schema = DATABASE()
          AND table_name = 'creature_template'
          AND column_name = 'trainer_race'
    ),
    'SELECT 1',
    'ALTER TABLE `creature_template` ADD COLUMN `trainer_race` tinyint unsigned NOT NULL DEFAULT 0 AFTER `trainer_class`'
);
PREPARE naz_trainer_race_stmt FROM @naz_trainer_race_sql;
EXECUTE naz_trainer_race_stmt;
DEALLOCATE PREPARE naz_trainer_race_stmt;

SET @naz_mechanic_immune_mask_sql := IF(
    EXISTS (
        SELECT 1
        FROM information_schema.columns
        WHERE table_schema = DATABASE()
          AND table_name = 'creature_template'
          AND column_name = 'mechanic_immune_mask'
    ),
    'SELECT 1',
    'ALTER TABLE `creature_template` ADD COLUMN `mechanic_immune_mask` int unsigned NOT NULL DEFAULT 0 AFTER `RegenHealth`'
);
PREPARE naz_mechanic_immune_mask_stmt FROM @naz_mechanic_immune_mask_sql;
EXECUTE naz_mechanic_immune_mask_stmt;
DEALLOCATE PREPARE naz_mechanic_immune_mask_stmt;

SET @naz_spell_school_immune_mask_sql := IF(
    EXISTS (
        SELECT 1
        FROM information_schema.columns
        WHERE table_schema = DATABASE()
          AND table_name = 'creature_template'
          AND column_name = 'spell_school_immune_mask'
    ),
    'SELECT 1',
    'ALTER TABLE `creature_template` ADD COLUMN `spell_school_immune_mask` int unsigned NOT NULL DEFAULT 0 AFTER `mechanic_immune_mask`'
);
PREPARE naz_spell_school_immune_mask_stmt FROM @naz_spell_school_immune_mask_sql;
EXECUTE naz_spell_school_immune_mask_stmt;
DEALLOCATE PREPARE naz_spell_school_immune_mask_stmt;
