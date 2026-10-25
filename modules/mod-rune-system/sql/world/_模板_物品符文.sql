-- 符文系统_符文表
-- 配置哪些物品是符文，以及它们的效果
-- 重要说明：
-- 1. id = 自增ID（数据库记录编号）
-- 2. 物品ID = item_template.entry（符文物品的真实ID）
-- 3. 物品ID必须在item_template表中存在！

DROP TABLE IF EXISTS `_模板_物品符文`;
CREATE TABLE `_模板_物品符文` (
  `id` int unsigned NOT NULL AUTO_INCREMENT COMMENT '符文记录ID(自增)',
  `物品ID` int unsigned NOT NULL DEFAULT '0' COMMENT '符文物品ID (item_template.entry)',
  `组` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '符文组别，用于分类管理',
  `名称` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '符文名称',
  `符文槽位` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '符文槽位 (1-20，表示槽位1到槽位20)',
  `符文品质` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '符文品质 (0=普通, 1=优秀, 2=精良, 3=史诗, 4=传说)',
  `法术ID` int unsigned NOT NULL DEFAULT '0' COMMENT '关联法术ID',
  `冷却时间` int unsigned NOT NULL DEFAULT '0' COMMENT '冷却时间(毫秒)',
  `触发几率` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '触发几率(%)',
  `效果类型` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '效果类型 (0=无加成, 1=属性加成, 2=技能加成, 3=百分比属性加成, 4=百分比技能加成)',
  `效果值` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '效果值 (格式: 属性类型 值,属性类型 值 | 技能加成用 99 值, 如: 3 10,4 15=耐力+10力量+15 | 99 10=技能+10)',
  `效果描述` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci COMMENT '效果的详细描述',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='符文系统 - 符文信息表';

-- ----------------------------
-- Records of 符文系统_符文
-- ----------------------------
INSERT INTO `_模板_物品符文` VALUES (1, 61001, 1, '符文1', 1, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (2, 61002, 1, '符文2', 2, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (3, 61003, 1, '符文3', 3, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (4, 61004, 1, '符文4', 4, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (5, 61005, 1, '符文5', 5, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (6, 61006, 1, '符文6', 6, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (7, 61007, 1, '符文7', 7, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (8, 61008, 1, '符文8', 8, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (9, 61009, 1, '符文9', 9, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (10, 61010, 1, '符文10', 10, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (11, 61011, 1, '符文11', 11, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (12, 61012, 1, '符文12', 12, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (13, 61013, 1, '符文13', 13, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (14, 61014, 1, '符文14', 14, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (15, 61015, 1, '符文15', 15, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (16, 61016, 1, '符文16', 16, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (17, 61017, 1, '符文17', 17, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (18, 61018, 1, '符文18', 18, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (19, 61019, 1, '符文19', 19, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');
INSERT INTO `_模板_物品符文` VALUES (20, 61020, 1, '符文20', 20, 0, 133, 1000, 100, 2, '99 1000', '火球术+1000耐力');

SET FOREIGN_KEY_CHECKS = 1;
