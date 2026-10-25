-- Preserve legacy columns while adding the fields required by mod-item-attributes.
SELECT IF(
    EXISTS (
        SELECT 1
        FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA` = DATABASE()
          AND `TABLE_NAME` = '_模板_物品属性'
          AND `COLUMN_NAME` = '技能模板_组'
    ),
    'SELECT 1',
    'ALTER TABLE `_模板_物品属性` ADD COLUMN `技能模板_组` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `属性计算方式`'
) INTO @naz_item_attr_ddl;
PREPARE naz_item_attr_stmt FROM @naz_item_attr_ddl;
EXECUTE naz_item_attr_stmt;
DEALLOCATE PREPARE naz_item_attr_stmt;

SELECT IF(
    EXISTS (
        SELECT 1
        FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA` = DATABASE()
          AND `TABLE_NAME` = '_模板_物品属性'
          AND `COLUMN_NAME` = '属性品质要求'
    ),
    'SELECT 1',
    'ALTER TABLE `_模板_物品属性` ADD COLUMN `属性品质要求` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `技能模板_组`'
) INTO @naz_item_attr_ddl;
PREPARE naz_item_attr_stmt FROM @naz_item_attr_ddl;
EXECUTE naz_item_attr_stmt;
DEALLOCATE PREPARE naz_item_attr_stmt;

SELECT IF(
    EXISTS (
        SELECT 1
        FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA` = DATABASE()
          AND `TABLE_NAME` = '_模板_物品属性'
          AND `COLUMN_NAME` = '属性等级要求'
    ),
    'SELECT 1',
    'ALTER TABLE `_模板_物品属性` ADD COLUMN `属性等级要求` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `属性品质要求`'
) INTO @naz_item_attr_ddl;
PREPARE naz_item_attr_stmt FROM @naz_item_attr_ddl;
EXECUTE naz_item_attr_stmt;
DEALLOCATE PREPARE naz_item_attr_stmt;

SELECT IF(
    EXISTS (
        SELECT 1
        FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA` = DATABASE()
          AND `TABLE_NAME` = '_模板_物品属性'
          AND `COLUMN_NAME` = '属性职业要求'
    ),
    'SELECT 1',
    'ALTER TABLE `_模板_物品属性` ADD COLUMN `属性职业要求` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `属性等级要求`'
) INTO @naz_item_attr_ddl;
PREPARE naz_item_attr_stmt FROM @naz_item_attr_ddl;
EXECUTE naz_item_attr_stmt;
DEALLOCATE PREPARE naz_item_attr_stmt;

SELECT IF(
    EXISTS (
        SELECT 1
        FROM `information_schema`.`COLUMNS`
        WHERE `TABLE_SCHEMA` = DATABASE()
          AND `TABLE_NAME` = '_模板_物品属性'
          AND `COLUMN_NAME` = '属性颜色'
    ),
    'SELECT 1',
    'ALTER TABLE `_模板_物品属性` ADD COLUMN `属性颜色` VARCHAR(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT ''FFFFFF'' AFTER `属性职业要求`'
) INTO @naz_item_attr_ddl;
PREPARE naz_item_attr_stmt FROM @naz_item_attr_ddl;
EXECUTE naz_item_attr_stmt;
DEALLOCATE PREPARE naz_item_attr_stmt;
