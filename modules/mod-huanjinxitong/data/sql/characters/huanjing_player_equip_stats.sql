-- 2. 玩家装备属性增强表
DROP TABLE IF EXISTS `玩家装备属性增强`;
CREATE TABLE `玩家装备属性增强` (
  `记录ID` int unsigned NOT NULL AUTO_INCREMENT COMMENT '记录ID',
  `玩家GUID` int unsigned NOT NULL COMMENT '玩家GUID',
  `装备GUID` int unsigned NOT NULL COMMENT '装备GUID',
  `装备ID` int unsigned NOT NULL COMMENT '装备模板ID',
  `属性倍率` int unsigned NOT NULL DEFAULT 1 COMMENT '属性倍率',
  `属性倍率模式` tinyint NOT NULL DEFAULT 0 COMMENT '属性模式(-1=关闭,0=乘法,1=加法)',
  `增强属性数据` text DEFAULT NULL COMMENT '增强后的属性数据，格式：属性类型 原值 增强值,属性类型 原值 增强值',
  `鉴定组ID` int unsigned NOT NULL DEFAULT 0 COMMENT '使用的鉴定组ID',
  `鉴定属性数据` text DEFAULT NULL COMMENT '鉴定系统的自定义属性数据（JSON格式）',

  PRIMARY KEY (`记录ID`),
  UNIQUE KEY `装备GUID` (`装备GUID`),
  KEY `玩家GUID` (`玩家GUID`),
  KEY `装备ID` (`装备ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='玩家装备属性增强表';
