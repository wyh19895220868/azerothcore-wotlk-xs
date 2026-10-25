-- 物品模板独立附魔组。请先在 world 数据库执行本更新，再启动使用新代码的服务器。
-- 首次创建新表时复制旧配置；新表已存在时保留其数据，重复执行不会覆盖修改。
-- 旧表仍供尚未删除的装备超级随机系统使用，本更新不删除旧表。
SET @item_enchant_legacy_exists := (
    SELECT COUNT(*) FROM `information_schema`.`TABLES`
    WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '_装备超级随机附魔组表'
);
SET @item_enchant_create := 'CREATE TABLE IF NOT EXISTS `_模板_物品附魔` (
    `组号` int NOT NULL COMMENT ''关联 _模板_物品鉴定.随机附魔组'',
    `附魔编号` int NOT NULL DEFAULT 0 COMMENT ''SpellItemEnchantment 附魔编号'',
    `生效几率` int NOT NULL DEFAULT 100 COMMENT ''附魔进入候选池的几率，百分比'',
    `备注` varchar(255) NOT NULL DEFAULT '''' COMMENT ''物品模板附魔配置说明'',
    PRIMARY KEY (`组号`, `附魔编号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT=''物品模板附魔组，供模板鉴定及洗炼使用''';
SET @item_enchant_sql := CONCAT(@item_enchant_create, IF(@item_enchant_legacy_exists > 0,
    ' SELECT `组号`, `附魔编号`, `生效几率`, `备注` FROM `_装备超级随机附魔组表`', ''));
PREPARE item_enchant_migration FROM @item_enchant_sql;
EXECUTE item_enchant_migration;
DEALLOCATE PREPARE item_enchant_migration;

-- 更新管理数据库时显示的字段说明；不改变已有的附魔组号。
SET @item_enchant_column_exists := (
    SELECT COUNT(*) FROM `information_schema`.`COLUMNS`
    WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = '_模板_物品鉴定' AND `COLUMN_NAME` = '随机附魔组'
);
SET @item_enchant_sql := IF(@item_enchant_column_exists > 0,
    'ALTER TABLE `_模板_物品鉴定` MODIFY COLUMN `随机附魔组` int UNSIGNED NOT NULL DEFAULT 0
        COMMENT ''关联_模板_物品附魔.组号''', 'DO 0');
PREPARE item_enchant_migration FROM @item_enchant_sql;
EXECUTE item_enchant_migration;
DEALLOCATE PREPARE item_enchant_migration;
