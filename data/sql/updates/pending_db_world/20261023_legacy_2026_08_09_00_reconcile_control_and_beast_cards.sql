-- Reconcile control IDs used by the core and restore the fixed 72-card
-- material slots.  Card slots remain disabled until the operator configures
-- their requirements/rewards.

-- The challenge entrance code reads this flag with a default of TRUE when the
-- row is absent.  Keep that behavior explicit in the control table.
INSERT INTO `__控制总表` (`序号`, `功能名称`, `功能值`, `备注`) VALUES
  (22005, '撞门触发挑战副本', '1', '1=启用挑战副本撞门弹窗，0=关闭弹窗并使用官方进入方式')
ON DUPLICATE KEY UPDATE
  `功能名称` = VALUES(`功能名称`),
  `备注` = VALUES(`备注`);

-- InstanceMap::CannotEnter reads this flag to decide whether an encounter in
-- progress blocks entry.  The historical behavior is the safe default (0).
INSERT INTO `__控制总表` (`序号`, `功能名称`, `功能值`, `备注`) VALUES
  (13201, '副本战斗中禁止进入', '0', '1=允许战斗中的副本进入，0=副本有战斗时禁止进入')
ON DUPLICATE KEY UPDATE
  `功能名称` = VALUES(`功能名称`),
  `备注` = VALUES(`备注`);

-- 22011 gates the complete template-item module family, not only the legacy
-- random-equipment switch.  Preserve the operator's current value.
INSERT INTO `__控制总表` (`序号`, `功能名称`, `功能值`, `备注`) VALUES
  (22011, '模板物品系列总开关', '1', '1=启用模板物品系列（属性、技能、套装、成长、强化、鉴定），0=启用装备超级系列')
ON DUPLICATE KEY UPDATE
  `功能名称` = VALUES(`功能名称`),
  `备注` = VALUES(`备注`);

-- Keep the feature-bar entry aligned with FuncType23/BEAST_CARD_FEATURE_CONFIG_ID.
INSERT INTO `__控制总表` (`序号`, `功能名称`, `功能值`, `备注`) VALUES
  (1423, '神兽翻牌', '1', '客户端功能栏第23项；源码常量 BEAST_CARD_FEATURE_CONFIG_ID')
ON DUPLICATE KEY UPDATE
  `功能名称` = VALUES(`功能名称`),
  `备注` = VALUES(`备注`);

-- The client has a fixed 1..72 material set.  INSERT IGNORE keeps any existing
-- 运营配置 untouched and leaves new cards disabled by the table defaults.
INSERT IGNORE INTO `_神兽翻牌配置` (`卡牌编号`) VALUES
  (1),(2),(3),(4),(5),(6),(7),(8),(9),(10),(11),(12),(13),(14),(15),(16),(17),(18),
  (19),(20),(21),(22),(23),(24),(25),(26),(27),(28),(29),(30),(31),(32),(33),(34),(35),(36),
  (37),(38),(39),(40),(41),(42),(43),(44),(45),(46),(47),(48),(49),(50),(51),(52),(53),(54),
  (55),(56),(57),(58),(59),(60),(61),(62),(63),(64),(65),(66),(67),(68),(69),(70),(71),(72);

-- Supply stable fallback names/descriptions without overwriting custom text.
UPDATE `_神兽翻牌配置`
SET `卡牌名称` = CONCAT('神兽牌 ', LPAD(`卡牌编号`, 2, '0'))
WHERE `卡牌名称` = '';

UPDATE `_神兽翻牌配置`
SET `卡牌介绍` = CONCAT('上古神兽「', `卡牌名称`, '」，翻开后永久激活配置属性与品质效果。')
WHERE `卡牌介绍` = '';

-- Global quality rows make the client quality state complete even before
-- per-card quality overrides are added.
INSERT IGNORE INTO `_神兽翻牌品质配置`
  (`卡牌编号`, `品质等级`, `是否启用`, `升级需求模板ID`, `技能ID`, `效果类型`, `效果值`, `效果描述`)
VALUES
  (0,0,1,0,0,0,'',''), (0,1,1,0,0,0,'',''), (0,2,1,0,0,0,'',''),
  (0,3,1,0,0,0,'',''), (0,4,1,0,0,0,'',''), (0,5,1,0,0,0,'',''),
  (0,6,1,0,0,0,'',''), (0,7,1,0,0,0,'',''), (0,8,1,0,0,0,'',''),
  (0,9,1,0,0,0,'','');
