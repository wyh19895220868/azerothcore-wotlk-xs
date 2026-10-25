-- ----------------------------
-- Table structure for 物品属性_数据
-- ----------------------------
DROP TABLE IF EXISTS `物品属性_数据`;
CREATE TABLE `物品属性_数据` (
  `物品ID` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '物品Entry ID',
  `物品GUID` bigint UNSIGNED NOT NULL COMMENT '物品唯一标识符',
  `基础属性` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci COMMENT '基础属性（替换官方属性），格式：属性ID 值,属性ID 值,...（例如：3 10,4 10,5 10）',
  `追加属性` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci COMMENT '追加属性（额外添加），格式：属性ID 值,属性ID 值,...（例如：3 10,4 10,5 10）',
  PRIMARY KEY (`物品GUID`) USING BTREE,
  KEY `idx_item_id` (`物品ID`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '物品属性数据表' ROW_FORMAT = DYNAMIC;
