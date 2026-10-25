-- 神兽翻牌扩展：卡牌名称/介绍、击杀解锁条件、翻开后永久属性。
SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_神兽翻牌配置' AND COLUMN_NAME = '卡牌名称');
SET @s := IF(@c = 0, 'ALTER TABLE `_神兽翻牌配置` ADD COLUMN `卡牌名称` varchar(64) NOT NULL DEFAULT '''' AFTER `卡牌编号`', 'DO 0');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_神兽翻牌配置' AND COLUMN_NAME = '卡牌介绍');
SET @s := IF(@c = 0, 'ALTER TABLE `_神兽翻牌配置` ADD COLUMN `卡牌介绍` varchar(640) NOT NULL DEFAULT '''' AFTER `卡牌名称`', 'DO 0');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_神兽翻牌配置' AND COLUMN_NAME = '击杀生物编号');
SET @s := IF(@c = 0, 'ALTER TABLE `_神兽翻牌配置` ADD COLUMN `击杀生物编号` int unsigned NOT NULL DEFAULT ''0'' AFTER `奖励模板ID`', 'DO 0');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_神兽翻牌配置' AND COLUMN_NAME = '需要击杀数量');
SET @s := IF(@c = 0, 'ALTER TABLE `_神兽翻牌配置` ADD COLUMN `需要击杀数量` int unsigned NOT NULL DEFAULT ''0'' AFTER `击杀生物编号`', 'DO 0');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_神兽翻牌配置' AND COLUMN_NAME = '激活后奖励属性编号');
SET @s := IF(@c = 0, 'ALTER TABLE `_神兽翻牌配置` ADD COLUMN `激活后奖励属性编号` smallint unsigned NOT NULL DEFAULT ''1'' AFTER `需要击杀数量`', 'DO 0');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_神兽翻牌配置' AND COLUMN_NAME = '激活后奖励属性数值');
SET @s := IF(@c = 0, 'ALTER TABLE `_神兽翻牌配置` ADD COLUMN `激活后奖励属性数值` int NOT NULL DEFAULT ''0'' AFTER `激活后奖励属性编号`', 'DO 0');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @c := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_神兽翻牌配置' AND COLUMN_NAME = '属性模式');
SET @s := IF(@c = 0, 'ALTER TABLE `_神兽翻牌配置` ADD COLUMN `属性模式` tinyint unsigned NOT NULL DEFAULT ''0'' COMMENT ''0固定值，1百分比'' AFTER `激活后奖励属性数值`', 'DO 0');
PREPARE stmt FROM @s; EXECUTE stmt; DEALLOCATE PREPARE stmt;



-- 名称与客户端 1-72 号卡面一一对应。介绍可由运营端继续修改。
UPDATE `_神兽翻牌配置`
SET `卡牌名称` = CASE `卡牌编号`
  WHEN 1 THEN '真龙' WHEN 2 THEN '凤凰' WHEN 3 THEN '鲲鹏' WHEN 4 THEN '金乌'
  WHEN 5 THEN '雷泽' WHEN 6 THEN '九凤' WHEN 7 THEN '应龙' WHEN 8 THEN '计蒙'
  WHEN 9 THEN '烛照' WHEN 10 THEN '蛊雕' WHEN 11 THEN '幽荧' WHEN 12 THEN '毕方'
  WHEN 13 THEN '黄龙' WHEN 14 THEN '腾蛇' WHEN 15 THEN '当康' WHEN 16 THEN '麒麟'
  WHEN 17 THEN '独角兽' WHEN 18 THEN '混沌' WHEN 19 THEN '飞廉' WHEN 20 THEN '天狗'
  WHEN 21 THEN '鬼车' WHEN 22 THEN '重明鸟' WHEN 23 THEN '化蛇' WHEN 24 THEN '英招'
  WHEN 25 THEN '祸斗' WHEN 26 THEN '帝江' WHEN 27 THEN '穷奇' WHEN 28 THEN '狻猊'
  WHEN 29 THEN '虚耗' WHEN 30 THEN '白泽' WHEN 31 THEN '负屃' WHEN 32 THEN '螭吻'
  WHEN 33 THEN '霸下' WHEN 34 THEN '蒲牢' WHEN 35 THEN '相繇' WHEN 36 THEN '文鳐'
  WHEN 37 THEN '角瑞' WHEN 38 THEN '赤鱬' WHEN 39 THEN '长右' WHEN 40 THEN '朱厌'
  WHEN 41 THEN '猾褢' WHEN 42 THEN '猰貐' WHEN 43 THEN '翼如' WHEN 44 THEN '虎蛟'
  WHEN 45 THEN '虫遗' WHEN 46 THEN '狌狌' WHEN 47 THEN '鹿蜀' WHEN 48 THEN '豪彘'
  WHEN 49 THEN '羬羊' WHEN 50 THEN '鹦鹉' WHEN 51 THEN '数斯' WHEN 52 THEN '鸾鸟'
  WHEN 53 THEN '胜遇' WHEN 54 THEN '鵸鵌' WHEN 55 THEN '蠃鱼' WHEN 56 THEN '勾陈'
  WHEN 57 THEN '相柳' WHEN 58 THEN '天马' WHEN 59 THEN '比翼鸟' WHEN 60 THEN '鸣蛇'
  WHEN 61 THEN '乘黄' WHEN 62 THEN '兽齿' WHEN 63 THEN '则嬴' WHEN 64 THEN '腓腓'
  WHEN 65 THEN '蜚' WHEN 66 THEN '庆忌' WHEN 67 THEN '诸犍' WHEN 68 THEN '狡猊'
  WHEN 69 THEN '夫诸' WHEN 70 THEN '尚付' WHEN 71 THEN '耳鼠' WHEN 72 THEN '犀渠'
  ELSE CONCAT('神兽牌 ', LPAD(`卡牌编号`, 2, '0'))
END;

UPDATE `_神兽翻牌配置`
SET `卡牌介绍` = CONCAT('上古神兽「', `卡牌名称`, '」，翻开后永久激活配置属性。')
WHERE `卡牌介绍` = '';

-- 百分比模式支持属性编号：0法力、1生命、3敏捷、4力量、5智力、6精神、7耐力、38攻击强度、39远程攻击强度。
-- 其他属性编号请使用固定值模式。击杀生物编号或需要击杀数量为0时，不设置击杀门槛。
-- 示例：击杀 100 只生物 12345 后允许翻牌，翻开后永久增加 5% 生命。
-- UPDATE `_神兽翻牌配置`
-- SET `是否启用`=1, `击杀生物编号`=12345, `需要击杀数量`=100,
--     `激活后奖励属性编号`=1, `激活后奖励属性数值`=5, `属性模式`=1
-- WHERE `卡牌编号`=1;
