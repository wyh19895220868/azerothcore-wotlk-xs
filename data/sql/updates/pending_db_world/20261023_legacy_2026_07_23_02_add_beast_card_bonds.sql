-- 神兽翻牌羁绊，机制与图鉴羁绊一致：满足激活数量后永久补挂配置光环。
-- 卡牌组合和羁绊效果都使用 # 分隔。
CREATE TABLE IF NOT EXISTS `_神兽翻牌羁绊配置` (
  `羁绊编号` int unsigned NOT NULL COMMENT '羁绊唯一编号',
  `羁绊名称` varchar(64) NOT NULL DEFAULT '' COMMENT '客户端显示名称',
  `卡牌组合` varchar(640) NOT NULL DEFAULT '' COMMENT '卡牌编号以#分隔，例如1#2#3',
  `激活数量` int unsigned NOT NULL DEFAULT '0' COMMENT '组合中需要翻开的卡牌数量，0表示全部',
  `羁绊效果` varchar(640) NOT NULL DEFAULT '' COMMENT '光环ID以#分隔，机制同图鉴羁绊',
  `效果说明` varchar(640) NOT NULL DEFAULT '' COMMENT '客户端羁绊面板显示内容',
  `是否启用` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '0关闭，1开启',
  `排序` int unsigned NOT NULL DEFAULT '0' COMMENT '客户端显示顺序',
  `备注` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`羁绊编号`),
  KEY `idx_启用排序` (`是否启用`, `排序`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='神兽翻牌羁绊配置';

-- 示例：真龙、凤凰、鲲鹏中任意翻开2张，激活90003和90004两个光环。
-- INSERT INTO `_神兽翻牌羁绊配置`
--   (`羁绊编号`, `羁绊名称`, `卡牌组合`, `激活数量`, `羁绊效果`, `效果说明`, `是否启用`, `排序`)
-- VALUES
--   (1, '龙凤呈祥', '1#2#3', 2, '90003#90004', '生命值提高5%\n攻击强度提高5%', 1, 1)
-- ON DUPLICATE KEY UPDATE
--   `羁绊名称`=VALUES(`羁绊名称`),
--   `卡牌组合`=VALUES(`卡牌组合`),
--   `激活数量`=VALUES(`激活数量`),
--   `羁绊效果`=VALUES(`羁绊效果`),
--   `效果说明`=VALUES(`效果说明`),
--   `是否启用`=VALUES(`是否启用`),
--   `排序`=VALUES(`排序`);
