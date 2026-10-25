-- 将神兽品质从8档扩展到10档；可在已经导入旧品质配置的数据库上执行。
INSERT INTO `_神兽翻牌品质配置`
  (`卡牌编号`, `品质等级`, `是否启用`, `升级需求模板ID`, `属性倍率`)
VALUES
  (0, 8, 1, 0, 360),
  (0, 9, 1, 0, 450)
ON DUPLICATE KEY UPDATE
  `是否启用`=VALUES(`是否启用`),
  `升级需求模板ID`=VALUES(`升级需求模板ID`),
  `属性倍率`=VALUES(`属性倍率`);

ALTER TABLE `_神兽翻牌品质配置`
  MODIFY COLUMN `品质等级` tinyint unsigned NOT NULL
    COMMENT '0D灰 1C白 2B绿 3A蓝 4S紫 5SS橙 6SSR暗金 7SSS红 8SR粉 9UR彩色';
