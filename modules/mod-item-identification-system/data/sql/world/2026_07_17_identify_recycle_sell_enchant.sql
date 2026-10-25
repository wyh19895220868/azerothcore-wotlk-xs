-- 2026-10-20: 改为可重复执行（原文件直接 ALTER，字段已存在时报 ERROR 1060 Duplicate column name）
-- 五个字段可能已由 `_模板_物品鉴定.sql` 的 CREATE TABLE 或之前一次执行创建过。
SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_物品鉴定' AND COLUMN_NAME = '回收奖励模板');
SET @s := IF(@c = 0, 'ALTER TABLE `_模板_物品鉴定` ADD COLUMN `回收奖励模板` int UNSIGNED NOT NULL DEFAULT 0 COMMENT ''关联__模版奖励总表，主动回收该实例时发放''', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_物品鉴定' AND COLUMN_NAME = '售卖奖励模板');
SET @s := IF(@c = 0, 'ALTER TABLE `_模板_物品鉴定` ADD COLUMN `售卖奖励模板` int UNSIGNED NOT NULL DEFAULT 0 COMMENT ''关联__模版奖励总表，成功售给商人后发放''', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_物品鉴定' AND COLUMN_NAME = '随机附魔组');
SET @s := IF(@c = 0, 'ALTER TABLE `_模板_物品鉴定` ADD COLUMN `随机附魔组` int UNSIGNED NOT NULL DEFAULT 0 COMMENT ''关联_装备超级随机附魔组表.组号''', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_物品鉴定' AND COLUMN_NAME = '随机附魔条数');
SET @s := IF(@c = 0, 'ALTER TABLE `_模板_物品鉴定` ADD COLUMN `随机附魔条数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT ''鉴定时最多抽取的附魔数量，最大5''', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;

SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_物品鉴定' AND COLUMN_NAME = '装备位置限制');
SET @s := IF(@c = 0, 'ALTER TABLE `_模板_物品鉴定` ADD COLUMN `装备位置限制` varchar(255) NOT NULL DEFAULT '''' COMMENT ''item_template.InventoryType，多个用逗号或#分隔，空值为全部''', 'DO 0');
PREPARE st FROM @s; EXECUTE st; DEALLOCATE PREPARE st;
