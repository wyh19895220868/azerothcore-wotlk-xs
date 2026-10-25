DROP TABLE IF EXISTS `_模板_物品强化`;
CREATE TABLE `_模板_物品强化` (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,
  `id` int NOT NULL,
  `组` int UNSIGNED NOT NULL DEFAULT 0,
  `等级` int UNSIGNED NOT NULL DEFAULT 0,
  `几率` float UNSIGNED NOT NULL DEFAULT 0,
  `物品属性_模板_组` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '关联物品属性_模板',
  `平均随机分配` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '属性分配方式（0平均分配 1随机分配）',
  `属性值1` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最小值',
  `属性值2` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最大值',
  `属性百分比1` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最小百分比',
  `属性百分比2` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最大百分比',
  `达到该强化等级后的奖励属性` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `达到该强化等级后的奖励属性百分比` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `物品技能_模板_id_多个逗号隔开` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NULL DEFAULT NULL,

  `强化需求` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '关联旧核心__模版需求总表的序号',
  `失败处理` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '失败后处理（0不变，1等级清0，2摧毁，3失败降级）',
  `失败降级等级` int UNSIGNED NOT NULL DEFAULT 1 COMMENT '仅失败处理=3时生效；0、1、2时忽略',
  `保底失败次数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '0关闭；连续失败达到该次数后本次必定成功',
  `每次失败增加成功率` float UNSIGNED NOT NULL DEFAULT 0 COMMENT '按当前连续失败次数增加成功率',
  `幸运物品` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '背包存在时自动消耗并增加本次成功率',
  `幸运物品数量` int UNSIGNED NOT NULL DEFAULT 0,
  `幸运增加成功率` float UNSIGNED NOT NULL DEFAULT 0,
  `保护物品` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '失败将清零、降级或摧毁时自动消耗并保护装备',
  `保护物品数量` int UNSIGNED NOT NULL DEFAULT 0,

  PRIMARY KEY (`id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='物品强化模板表';

ALTER TABLE `_模板_物品强化`
  MODIFY COLUMN `属性值1` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最小值',
  MODIFY COLUMN `属性值2` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最大值',
  MODIFY COLUMN `属性百分比1` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最小百分比',
  MODIFY COLUMN `属性百分比2` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '随机属性最大百分比',
  MODIFY COLUMN `达到该强化等级后的奖励属性` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  MODIFY COLUMN `达到该强化等级后的奖励属性百分比` BIGINT UNSIGNED NOT NULL DEFAULT 0;

-- 插入0组（官方装备）强化配置数据
INSERT INTO `_模板_物品强化` (
    `注释`, `id`, `组`, `等级`, `几率`, `物品属性_模板_组`, `平均随机分配`,
    `属性值1`, `属性值2`, `属性百分比1`, `属性百分比2`,
    `达到该强化等级后的奖励属性`, `达到该强化等级后的奖励属性百分比`,
    `物品技能_模板_id_多个逗号隔开`, `强化需求`, `失败处理`
) VALUES
-- 1-5级：高成功率，失败不变，每级+2属性
('官方装备+1', 1001, 0, 1, 95.0, 0, 0, 2, 2, 0, 0, 0, 0, '', 0, 0),
('官方装备+2', 1002, 0, 2, 90.0, 0, 0, 2, 2, 0, 0, 0, 0, '', 0, 0),
('官方装备+3', 1003, 0, 3, 85.0, 0, 0, 2, 2, 0, 0, 0, 0, '', 0, 0),
('官方装备+4', 1004, 0, 4, 80.0, 0, 0, 2, 2, 0, 0, 0, 0, '', 0, 0),
('官方装备+5', 1005, 0, 5, 75.0, 0, 0, 2, 2, 0, 0, 0, 0, '', 0, 0),

-- 6-10级：中等成功率，失败清零，每级+2属性
('官方装备+6', 1006, 0, 6, 70.0, 0, 0, 2, 2, 0, 0, 0, 0, '', 0, 1),
('官方装备+7', 1007, 0, 7, 65.0, 0, 0, 2, 2, 0, 0, 0, 0, '', 0, 1),
('官方装备+8', 1008, 0, 8, 60.0, 0, 0, 2, 2, 0, 0, 0, 0, '', 0, 1),
('官方装备+9', 1009, 0, 9, 55.0, 0, 0, 2, 2, 0, 0, 0, 0, '', 0, 1),
('官方装备+10', 1010, 0, 10, 50.0, 0, 0, 2, 2, 0, 0, 0, 0, '', 0, 1),

-- 11-15级：低成功率，失败摧毁，每级+3属性
('官方装备+11', 1011, 0, 11, 45.0, 0, 0, 3, 3, 0, 0, 0, 0, '', 0, 2),
('官方装备+12', 1012, 0, 12, 40.0, 0, 0, 3, 3, 0, 0, 0, 0, '', 0, 2),
('官方装备+13', 1013, 0, 13, 35.0, 0, 0, 3, 3, 0, 0, 0, 0, '', 0, 2),
('官方装备+14', 1014, 0, 14, 30.0, 0, 0, 3, 3, 0, 0, 0, 0, '', 0, 2),
('官方装备+15', 1015, 0, 15, 25.0, 0, 0, 3, 3, 0, 0, 0, 0, '', 0, 2),

-- 16-20级：极低成功率，失败摧毁，每级+4-5属性
('官方装备+16', 1016, 0, 16, 20.0, 0, 0, 4, 4, 0, 0, 0, 0, '', 0, 2),
('官方装备+17', 1017, 0, 17, 15.0, 0, 0, 4, 4, 0, 0, 0, 0, '', 0, 2),
('官方装备+18', 1018, 0, 18, 10.0, 0, 0, 4, 4, 0, 0, 0, 0, '', 0, 2),
('官方装备+19', 1019, 0, 19, 8.0, 0, 0, 5, 5, 0, 0, 0, 0, '', 0, 2),
('官方装备+20', 1020, 0, 20, 5.0, 0, 0, 5, 5, 0, 0, 0, 0, '', 0, 2);
