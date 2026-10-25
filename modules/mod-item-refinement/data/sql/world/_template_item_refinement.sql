CREATE TABLE IF NOT EXISTS `_模板_物品洗炼` (
  `注释` varchar(255) NOT NULL DEFAULT '' COMMENT '备注描述',
  `物品id` int UNSIGNED NOT NULL COMMENT '允许洗炼的物品模板ID；精确配置优先，同鉴定组且规则一致时可作为其它已鉴定装备的兜底配置',
  `_物品_鉴定_组` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '关联 _模板_物品鉴定.组；仅配置类目4并使用_物品随机附魔控制表时允许填0',
  `洗炼需求` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '关联 __模版需求总表.序号',
  `洗炼类目限制` varchar(1000) NOT NULL DEFAULT '' COMMENT '1基础属性,2追加属性,3追加技能,4附魔；类目4优先使用ItemTargetEnch同源的官方附魔配置',
  PRIMARY KEY (`物品id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='物品洗炼配置';
