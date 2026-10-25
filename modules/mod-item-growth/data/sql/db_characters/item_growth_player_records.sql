-- 玩家物品成长记录表
DROP TABLE IF EXISTS `物品成长_玩家记录`;
CREATE TABLE `物品成长_玩家记录` (
  `id` int NOT NULL AUTO_INCREMENT,
  `玩家GUID` int UNSIGNED NOT NULL,
  `物品GUID` int UNSIGNED NOT NULL,
  `物品模板ID` int UNSIGNED NOT NULL,
  `当前等级` int UNSIGNED NOT NULL DEFAULT 0,
  `当前经验` int UNSIGNED NOT NULL DEFAULT 0,
  `升级经验` int UNSIGNED NOT NULL DEFAULT 100 COMMENT '升级到下一等级所需的经验值',
  `物品属性_模板_组` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '物品所属的属性模板组，用于确定成长路径',
  `成长次数` int UNSIGNED NOT NULL DEFAULT 0,
  `失败次数` int UNSIGNED NOT NULL DEFAULT 0,

  -- 成长属性数据（格式：属性类型 值,属性类型 值）
  `成长属性` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL COMMENT '成长获得的属性加成，格式：7 5,3 10,4 8（属性类型 值,属性类型 值）',
  `百分比属性` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL COMMENT '百分比成长获得的属性加成，格式：0 50,1 30（属性类型 值,属性类型 值）',

  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE INDEX `物品GUID_索引`(`物品GUID`) USING BTREE,
  INDEX `玩家GUID_索引`(`玩家GUID`) USING BTREE,
  INDEX `属性组_索引`(`物品属性_模板_组`) USING BTREE
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '玩家物品成长记录表' ROW_FORMAT = DYNAMIC;
