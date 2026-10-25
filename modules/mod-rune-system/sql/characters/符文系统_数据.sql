
DROP TABLE IF EXISTS `符文系统_数据`;
CREATE TABLE `符文系统_数据` (
  `ID` int unsigned NOT NULL AUTO_INCREMENT COMMENT '记录ID',
  `角色GUID` int unsigned NOT NULL COMMENT '角色GUID',
  `物品ID` int unsigned NOT NULL COMMENT '物品模板ID',
  `物品GUID` int unsigned NOT NULL COMMENT '物品实例GUID',
  `插槽数量` tinyint unsigned NOT NULL DEFAULT 0 COMMENT '插槽数量(1-20)',
  `插槽ID` varchar(255) NOT NULL DEFAULT '' COMMENT '插槽ID列表(逗号分隔): 1,2,3,4,5',
  `符文物品ID` varchar(255) NOT NULL DEFAULT '' COMMENT '符文物品ID列表(逗号分隔): 25,26,0,28,0',
  PRIMARY KEY (`ID`),
  UNIQUE KEY `idx_item_guid` (`物品GUID`),
  KEY `idx_character_guid` (`角色GUID`),
  KEY `idx_item_id` (`物品ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='符文系统存储表-装备符文数据';
