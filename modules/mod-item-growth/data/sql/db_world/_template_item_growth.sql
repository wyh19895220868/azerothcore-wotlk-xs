DROP TABLE IF EXISTS `_模板_物品成长`;
CREATE TABLE `_模板_物品成长` (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `id` int NOT NULL,
  `需求模板` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '关联需求模板的ID，0表示无需求限制，大于0时需要满足对应需求模板才能随机到该组',
  `组` int UNSIGNED NOT NULL DEFAULT 1,
  `等级` int UNSIGNED NOT NULL DEFAULT 0,
  `几率` int UNSIGNED NOT NULL DEFAULT 100 COMMENT '该组的随机几率权重，数值越大被随机到的概率越高',
  `成长经验` int UNSIGNED NOT NULL DEFAULT 0,
  `物品属性_模板_组` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '关联mod-item-attributes模块的属性组ID，0表示不关联任何属性组，大于0时会在物品升级时自动应用对应属性组的随机属性',
  `平均随机分配` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '属性分配方式：0=平均分配（所有属性都加固定值），1=随机分配（在最小值和最大值之间随机分配）',
  `属性值1` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最小值',
  `属性值2` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最大值',
  `属性百分比1` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最小百分比',
  `属性百分比2` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最大百分比',
  `达到该成长等级后的奖励属性` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `达到该成长等级后的奖励属性百分比` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `物品技能_模板id_多个逗号隔开` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL COMMENT '【已弃用】保留字段，不再使用',

  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE KEY `uk_组_等级` (`组`, `等级`) COMMENT '【审计修复】确保每个组内等级唯一，避免重复配置导致的不确定行为'
) ENGINE = MyISAM CHARACTER SET = utf8mb4 COLLATE = utf8mb4_general_ci COMMENT = '物品成长模板' ROW_FORMAT = DYNAMIC;

ALTER TABLE `_模板_物品成长`
  MODIFY COLUMN `属性值1` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最小值',
  MODIFY COLUMN `属性值2` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最大值',
  MODIFY COLUMN `属性百分比1` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最小百分比',
  MODIFY COLUMN `属性百分比2` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最大百分比',
  MODIFY COLUMN `达到该成长等级后的奖励属性` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  MODIFY COLUMN `达到该成长等级后的奖励属性百分比` BIGINT UNSIGNED NOT NULL DEFAULT 0;
