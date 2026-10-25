CREATE TABLE IF NOT EXISTS `_授权自动拾取` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '唯一ID',
  `玩家账号` varchar(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '玩家注册账号，授权该账号下所有角色',
  `自动拾取` tinyint(1) unsigned NOT NULL DEFAULT 1 COMMENT '1开启 0关闭',
  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE KEY `idx_玩家账号` (`玩家账号`) USING BTREE
) ENGINE=MyISAM AUTO_INCREMENT=1 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='自动拾取账号授权表' ROW_FORMAT=DYNAMIC;
