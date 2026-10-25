-- 在 acore_characters 数据库执行。主键确保每个角色的每张神兽牌只能成功翻开一次。
CREATE TABLE IF NOT EXISTS `_神兽翻牌存档` (
  `玩家GUID` int unsigned NOT NULL,
  `卡牌编号` int unsigned NOT NULL,
  `品质等级` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0D灰 1C白 2B绿 3A蓝 4S紫 5SS橙 6SSR暗金 7SSS红 8SR粉 9UR彩色',
  `翻牌时间` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`玩家GUID`, `卡牌编号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='神兽翻牌角色存档';
