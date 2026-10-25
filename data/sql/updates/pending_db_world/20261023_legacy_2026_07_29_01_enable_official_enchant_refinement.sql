ALTER TABLE `_模板_物品洗炼`
  MODIFY COLUMN `_物品_鉴定_组` int UNSIGNED NOT NULL DEFAULT 0
    COMMENT '关联 _模板_物品鉴定.组；仅配置类目4并使用_物品随机附魔控制表时允许填0',
  MODIFY COLUMN `洗炼类目限制` varchar(1000) NOT NULL DEFAULT ''
    COMMENT '1基础属性,2追加属性,3追加技能,4附魔；类目4优先使用ItemTargetEnch同源的官方附魔配置';
