-- 物品套装数据表 - 优化版本
-- 如果表已存在，先删除重建
DROP TABLE IF EXISTS `_物品套装_数据`;

-- 创建优化的套装分配记录表
-- 物品GUID本身就是唯一的，不需要玩家GUID作为主键
CREATE TABLE `_物品套装_数据` (
  `物品ID` int(10) unsigned NOT NULL COMMENT '物品模板ID',
  `物品GUID` int(10) unsigned NOT NULL COMMENT '物品的唯一GUID',
  `套装ID` int(10) unsigned NOT NULL COMMENT '分配的套装ID',
  PRIMARY KEY (`物品GUID`),
  KEY `idx_set_id` (`套装ID`),
  KEY `idx_item_id` (`物品ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='物品套装分配记录表 - 记录物品和套装的映射关系';
