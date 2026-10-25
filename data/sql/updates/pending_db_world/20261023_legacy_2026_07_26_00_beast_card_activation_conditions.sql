-- 神兽翻牌激活条件支持使用 -1 明确关闭需求或击杀条件。
-- 两项都大于0时，翻牌必须同时满足需求模板和击杀数量。
ALTER TABLE `_神兽翻牌配置`
  MODIFY COLUMN `需求模板ID` int NOT NULL DEFAULT '-1'
    COMMENT '大于0启用需求条件；-1或0表示不使用需求条件',
  MODIFY COLUMN `击杀生物编号` int NOT NULL DEFAULT '-1'
    COMMENT '大于0且需要击杀数量大于0时启用击杀条件；-1或0表示不使用击杀条件';
