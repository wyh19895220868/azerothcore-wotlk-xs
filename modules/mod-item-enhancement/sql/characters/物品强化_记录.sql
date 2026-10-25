-- 物品强化记录表
-- 如果表已存在，先删除
DROP TABLE IF EXISTS `物品强化_记录`;

-- 创建新表，确保字段类型正确
CREATE TABLE `物品强化_记录` (
  `guid` int UNSIGNED NOT NULL COMMENT '物品GUID',
  `owner_guid` int UNSIGNED NOT NULL COMMENT '所有者GUID',
  `物品模板ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '物品模板ID',
  `强化组` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '强化组号，用于选择对应的强化模板',
  `强化等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前强化等级',
  `强化经验` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前强化经验',
  `属性值` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL COMMENT '强化属性值，格式：属性类型 值,属性类型 值，如：7 10,5 15,3 8',
  `品质等级` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '当前品质等级',
  `品质进阶次数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '品质进阶次数',
  `强化次数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '强化次数',
  `失败次数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '失败次数',
  `最后强化时间` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '最后强化时间',
  PRIMARY KEY (`guid`),
  KEY `idx_owner` (`owner_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='物品强化记录表';
