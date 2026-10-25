-- 神兽翻牌品质配置。
-- 卡牌编号 0 表示所有神兽牌共用的默认配置；1-72 可覆盖某张牌的指定品质。
-- 品质等级：0 D灰、1 C白、2 B绿、3 A蓝、4 S紫、5 SS橙、6 SSR暗金、7 SSS红、8 SR粉、9 UR彩色。
-- 升级需求模板ID为0时，沿用该神兽牌在 `_神兽翻牌配置`.`需求模板ID` 中的消耗。
-- 属性倍率按激活属性的百分比计算，例如基础属性100、倍率170，实际生效170。
CREATE TABLE IF NOT EXISTS `_神兽翻牌品质配置` (
  `卡牌编号` int unsigned NOT NULL DEFAULT '0' COMMENT '0为全局默认，1-72为单卡覆盖',
  `品质等级` tinyint unsigned NOT NULL COMMENT '0D灰 1C白 2B绿 3A蓝 4S紫 5SS橙 6SSR暗金 7SSS红 8SR粉 9UR彩色',
  `是否启用` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '0关闭该品质升级，1开启',
  `升级需求模板ID` int unsigned NOT NULL DEFAULT '0' COMMENT '0沿用卡牌翻牌需求模板',
  `属性倍率` smallint unsigned NOT NULL DEFAULT '100' COMMENT '相对基础激活属性的百分比倍率',
  PRIMARY KEY (`卡牌编号`, `品质等级`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='神兽翻牌品质升级配置';

-- 兼容已存在的旧版本表结构（CREATE TABLE IF NOT EXISTS 不会补列，缺列时这里补上）。
SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_神兽翻牌品质配置' AND COLUMN_NAME = '是否启用');
SET @s := IF(@c = 0, 'ALTER TABLE `_神兽翻牌品质配置` ADD COLUMN `是否启用` tinyint unsigned NOT NULL DEFAULT ''1'' COMMENT ''0关闭该品质升级，1开启'' AFTER `品质等级`', 'DO 0');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_神兽翻牌品质配置' AND COLUMN_NAME = '升级需求模板ID');
SET @s := IF(@c = 0, 'ALTER TABLE `_神兽翻牌品质配置` ADD COLUMN `升级需求模板ID` int unsigned NOT NULL DEFAULT ''0'' COMMENT ''0沿用卡牌翻牌需求模板'' AFTER `是否启用`', 'DO 0');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_神兽翻牌品质配置' AND COLUMN_NAME = '属性倍率');
SET @s := IF(@c = 0, 'ALTER TABLE `_神兽翻牌品质配置` ADD COLUMN `属性倍率` smallint unsigned NOT NULL DEFAULT ''100'' COMMENT ''相对基础激活属性的百分比倍率'' AFTER `升级需求模板ID`', 'DO 0');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 默认十档品质倍率；INSERT IGNORE 不覆盖运营端已经修改的数值。
INSERT IGNORE INTO `_神兽翻牌品质配置`
  (`卡牌编号`, `品质等级`, `是否启用`, `升级需求模板ID`, `属性倍率`)
VALUES
  (0, 0, 1, 0, 100),
  (0, 1, 1, 0, 110),
  (0, 2, 1, 0, 125),
  (0, 3, 1, 0, 145),
  (0, 4, 1, 0, 170),
  (0, 5, 1, 0, 200),
  (0, 6, 1, 0, 240),
  (0, 7, 1, 0, 300),
  (0, 8, 1, 0, 360),
  (0, 9, 1, 0, 450);

-- 单卡覆盖示例：真龙升级到S品质时使用需求模板30004，并把基础属性放大到200%。
-- INSERT INTO `_神兽翻牌品质配置`
--   (`卡牌编号`, `品质等级`, `是否启用`, `升级需求模板ID`, `属性倍率`)
-- VALUES (1, 4, 1, 30004, 200)
-- ON DUPLICATE KEY UPDATE
--   `是否启用`=VALUES(`是否启用`),
--   `升级需求模板ID`=VALUES(`升级需求模板ID`),
--   `属性倍率`=VALUES(`属性倍率`);
