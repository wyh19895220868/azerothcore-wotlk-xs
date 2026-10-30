-- 星脉衍化：角色库存档
-- 与星界符文存档独立；每条记录代表该角色已点亮的一个星脉节点

CREATE TABLE IF NOT EXISTS `_星脉衍化角色存档` (
  `角色GUID` int unsigned NOT NULL DEFAULT '0' COMMENT '角色GUID',
  `节点ID` int NOT NULL DEFAULT '0' COMMENT '已点亮的节点ID',
  `技能编号` int NOT NULL DEFAULT '0' COMMENT '点亮时施加的常驻光环技能编号',
  `点亮时间` int unsigned NOT NULL DEFAULT '0' COMMENT '点亮服务器时间戳',
  PRIMARY KEY (`角色GUID`, `节点ID`),
  KEY `idx_starvein_node` (`节点ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='星脉衍化角色存档';
