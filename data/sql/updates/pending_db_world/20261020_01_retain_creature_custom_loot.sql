-- Keep the creature custom-loot table and remove the standalone duplicate.
CREATE TABLE IF NOT EXISTS `_生物自定义掉落表` (
  `序号` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '_生物自定义掉落表的序号',
  `类型` enum('生物','物品','物体') NOT NULL COMMENT '掉落来源类型',
  `来源编号` int(10) unsigned NOT NULL COMMENT '生物、物品或物体编号',
  `物品编号` int(10) unsigned NOT NULL COMMENT '掉落的物品编号',
  `几率` float NOT NULL DEFAULT '100' COMMENT '掉落几率0-100',
  `最小数量` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '最小数量',
  `最大数量` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '最大数量',
  `允许多倍` enum('关闭','开启') NOT NULL DEFAULT '关闭' COMMENT '允许多倍',
  PRIMARY KEY (`序号`),
  KEY `来源索引` (`类型`,`来源编号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='生物自定义掉落';

DROP TABLE IF EXISTS `自定义掉落表`;
