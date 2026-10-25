-- 地图表只负责匹配、控制组、刷新策略和动态难度。
-- 数值属性统一配置在 __模版生物属性表，随机关系配置在 _生物控制组表。
CREATE TABLE IF NOT EXISTS `_模板_地图生物控制` (
  `配置ID` int unsigned NOT NULL AUTO_INCREMENT COMMENT '配置ID',
  `生物ID` int NOT NULL DEFAULT -1 COMMENT '生物模板ID(-1=全部)',
  `生物类型` int NOT NULL DEFAULT -1 COMMENT '-1=全部,0=非Boss,1=普通,2=精英,3=Boss',
  `地图ID` int NOT NULL DEFAULT -1 COMMENT '地图ID(-1=全部)',
  `区域ID` int NOT NULL DEFAULT -1 COMMENT '区域ID(-1=全部)',
  `难度` int NOT NULL DEFAULT -1 COMMENT '副本难度(-1=全部)',
  `秘境难度` varchar(255) NOT NULL DEFAULT '0' COMMENT '0/空/全部=全部秘境难度，多个值用逗号分隔',
  `生物控制组` int unsigned NOT NULL DEFAULT 0 COMMENT '关联 _生物控制组表.组号；每只生物只随机一次',
  `提示方式` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '0=不提示,1=聊天,2=聊天和屏幕,3=特殊提示',
  `刷新公告` varchar(255) NOT NULL DEFAULT '[提示]: %s 刷新了' COMMENT '%s 替换为生物名称',
  `刷新时间` int unsigned NOT NULL DEFAULT 0 COMMENT '覆盖尸体移除和重生时间，单位秒；0=官方时间',
  `动态难度开关` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '0=关闭,1=开启',
  `战力门槛` BIGINT unsigned NOT NULL DEFAULT 0 COMMENT '0=不限制；大于0时作为动态难度基准',
  `难度倍率` double unsigned NOT NULL DEFAULT 1 COMMENT '最终倍率=(当前战力/战力门槛)*本值；门槛为0时直接使用本值',
  PRIMARY KEY (`配置ID`),
  KEY `生物模板ID` (`生物ID`),
  KEY `地图ID` (`地图ID`),
  KEY `地图区域难度` (`地图ID`, `区域ID`, `难度`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='地图生物路由与刷新控制表';
