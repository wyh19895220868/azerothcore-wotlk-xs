
DROP TABLE IF EXISTS `_物品技能_数据`;
CREATE TABLE `_物品技能_数据` (
  `物品ID` int UNSIGNED NOT NULL COMMENT '物品Entry ID（item_template.entry）',
  `物品GUID` bigint UNSIGNED NOT NULL COMMENT '物品实例唯一标识（item_instance.guid）',
  `技能模板ID` varchar(255) NOT NULL COMMENT '技能模板ID列表，逗号分隔，如"1,2,3"',
  `技能ID` varchar(255) NOT NULL COMMENT '法术ID列表，逗号分隔，如"133,10,122"',
  `技能触发类型` varchar(255) NOT NULL COMMENT '触发类型列表，逗号分隔，如"0,1,2"（0=使用,1=装备,2=击中,100=学习）',
  `触发几率` varchar(255) NOT NULL DEFAULT '100' COMMENT '触发几率列表，逗号分隔，如"100,50,30"',
  PRIMARY KEY (`物品GUID`) USING BTREE COMMENT '物品GUID为主键，一个物品一行存储所有技能',
  KEY `idx_item_id` (`物品ID`) USING BTREE COMMENT '物品ID索引，用于查询同类物品'
) ENGINE = InnoDB
  CHARACTER SET = utf8mb4
  COLLATE = utf8mb4_general_ci
  COMMENT = '物品技能数据表 - 一个物品GUID一行，多个技能用逗号分隔'
  ROW_FORMAT = DYNAMIC;
