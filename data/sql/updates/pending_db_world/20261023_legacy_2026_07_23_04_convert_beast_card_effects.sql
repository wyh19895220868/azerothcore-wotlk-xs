-- 神兽品质与神兽羁绊统一改用物品符文式效果配置。
-- 效果类型：0无额外效果，1固定属性，2技能固定加成，3百分比属性，4技能百分比加成。
-- 属性效果值支持多项，格式：属性ID 数值,属性ID 数值，例如 4 50,7 100。
-- 技能效果值使用：99 数值。技能ID为0时对全部技能生效，否则只对指定技能生效。

ALTER TABLE `_神兽翻牌品质配置`
  DROP COLUMN `属性倍率`,
  ADD COLUMN `技能ID` int unsigned NOT NULL DEFAULT '0'
    COMMENT '效果类型2/4的目标技能；0表示全部技能' AFTER `升级需求模板ID`,
  ADD COLUMN `效果类型` tinyint unsigned NOT NULL DEFAULT '0'
    COMMENT '0无 1固定属性 2技能固定 3百分比属性 4技能百分比' AFTER `技能ID`,
  ADD COLUMN `效果值` varchar(640) NOT NULL DEFAULT ''
    COMMENT '多属性格式4 50,7 100；技能格式99 10' AFTER `效果类型`,
  ADD COLUMN `效果描述` varchar(640) NOT NULL DEFAULT ''
    COMMENT '客户端品质效果区域显示内容；空值时服务端自动生成' AFTER `效果值`;

ALTER TABLE `_神兽翻牌羁绊配置`
  DROP COLUMN `羁绊效果`,
  ADD COLUMN `技能ID` int unsigned NOT NULL DEFAULT '0'
    COMMENT '效果类型2/4的目标技能；0表示全部技能' AFTER `激活数量`,
  ADD COLUMN `效果类型` tinyint unsigned NOT NULL DEFAULT '0'
    COMMENT '0无 1固定属性 2技能固定 3百分比属性 4技能百分比' AFTER `技能ID`,
  ADD COLUMN `效果值` varchar(640) NOT NULL DEFAULT ''
    COMMENT '多属性格式4 50,7 100；技能格式99 10' AFTER `效果类型`,
  CHANGE COLUMN `效果说明` `效果描述` varchar(640) NOT NULL DEFAULT ''
    COMMENT '客户端羁绊面板显示内容；空值时服务端自动生成';

-- 旧羁绊光环列表已删除，先关闭尚未重新填写新效果的羁绊，避免显示已激活但没有实际效果。
UPDATE `_神兽翻牌羁绊配置`
SET `是否启用`=0
WHERE `效果类型`=0 AND `效果值`='';

-- 品质配置示例：真龙S品质增加50力量和100耐力。
-- UPDATE `_神兽翻牌品质配置`
-- SET `技能ID`=0, `效果类型`=1, `效果值`='4 50,7 100', `效果描述`='力量 +50\n耐力 +100'
-- WHERE `卡牌编号`=1 AND `品质等级`=4;

-- 品质技能示例：真龙SS品质使火球术固定增加1000点伤害/治疗。
-- UPDATE `_神兽翻牌品质配置`
-- SET `技能ID`=133, `效果类型`=2, `效果值`='99 1000', `效果描述`='火球术伤害和治疗 +1000'
-- WHERE `卡牌编号`=1 AND `品质等级`=5;

-- 羁绊配置示例：真龙、凤凰、鲲鹏任意激活2张，增加80力量、120耐力。
-- INSERT INTO `_神兽翻牌羁绊配置`
--   (`羁绊编号`, `羁绊名称`, `卡牌组合`, `激活数量`, `技能ID`, `效果类型`, `效果值`, `效果描述`, `是否启用`, `排序`)
-- VALUES
--   (1, '龙凤呈祥', '1#2#3', 2, 0, 1, '4 80,7 120', '力量 +80\n耐力 +120', 1, 1)
-- ON DUPLICATE KEY UPDATE
--   `羁绊名称`=VALUES(`羁绊名称`), `卡牌组合`=VALUES(`卡牌组合`), `激活数量`=VALUES(`激活数量`),
--   `技能ID`=VALUES(`技能ID`), `效果类型`=VALUES(`效果类型`), `效果值`=VALUES(`效果值`),
--   `效果描述`=VALUES(`效果描述`), `是否启用`=VALUES(`是否启用`), `排序`=VALUES(`排序`);
