-- 每个角色按卡牌独立累计击杀。若运营端更换该卡的生物编号，下一次击杀会自动重置旧进度。
CREATE TABLE IF NOT EXISTS `_神兽翻牌击杀进度` (
  `玩家GUID` int unsigned NOT NULL,
  `卡牌编号` int unsigned NOT NULL,
  `生物编号` int unsigned NOT NULL DEFAULT '0',
  `击杀数量` int unsigned NOT NULL DEFAULT '0',
  `更新时间` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`玩家GUID`, `卡牌编号`),
  KEY `idx_生物编号` (`生物编号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='神兽翻牌击杀解锁进度';
