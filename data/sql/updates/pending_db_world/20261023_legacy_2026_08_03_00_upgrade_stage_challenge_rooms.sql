-- 将旧的“手动点击版天梯”升级为章节制副本闯关。
-- 每章固定5关，第5关为首领关；怪物组和旧奖励物体字段继续复用。

CREATE TABLE IF NOT EXISTS `_副本闯关章节` (
  `章节编号` int(10) unsigned NOT NULL,
  `章节名称` varchar(64) NOT NULL DEFAULT '',
  `地图编号` int(10) unsigned NOT NULL COMMENT '必须为副本地图；同一章节的5关使用此地图实例',
  `入口坐标编号` int(10) unsigned NOT NULL COMMENT '对应__模版坐标，地图必须等于地图编号',
  `出口坐标编号` int(10) unsigned NOT NULL COMMENT '对应__模版坐标，可指向副本外地图',
  `章节奖励模版` int(10) unsigned NOT NULL DEFAULT '0',
  `下一章节编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0=最后章节',
  `默认传送门物体编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'gameobject_template.ScriptName必须为StageChallengePortal',
  `启用` tinyint(3) unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`章节编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

SET @column_exists := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='_副本闯关' AND COLUMN_NAME='章节编号');
SET @ddl := IF(@column_exists=0, 'ALTER TABLE `_副本闯关` ADD COLUMN `章节编号` int(10) unsigned NOT NULL DEFAULT 1 COMMENT ''对应_副本闯关章节'' AFTER `关卡序号`', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @column_exists := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='_副本闯关' AND COLUMN_NAME='章节内序号');
SET @ddl := IF(@column_exists=0, 'ALTER TABLE `_副本闯关` ADD COLUMN `章节内序号` tinyint(3) unsigned NOT NULL DEFAULT 1 COMMENT ''每章固定1-5，第5关为首领关'' AFTER `章节编号`', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @column_exists := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='_副本闯关' AND COLUMN_NAME='关卡坐标编号');
SET @ddl := IF(@column_exists=0, 'ALTER TABLE `_副本闯关` ADD COLUMN `关卡坐标编号` int(10) unsigned NOT NULL DEFAULT 0 COMMENT ''对应__模版坐标，必须位于章节地图'' AFTER `菜单文本`', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @column_exists := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='_副本闯关' AND COLUMN_NAME='通关传送门编号');
SET @ddl := IF(@column_exists=0, 'ALTER TABLE `_副本闯关` ADD COLUMN `通关传送门编号` int(10) unsigned NOT NULL DEFAULT 0 COMMENT ''0=使用章节默认传送门；ScriptName必须为StageChallengePortal'' AFTER `关卡坐标编号`', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @column_exists := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='_副本闯关' AND COLUMN_NAME='首领关');
SET @ddl := IF(@column_exists=0, 'ALTER TABLE `_副本闯关` ADD COLUMN `首领关` tinyint(3) unsigned NOT NULL DEFAULT 0 COMMENT ''章节第5关必须为1，其余关必须为0'' AFTER `通关传送门编号`', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @column_exists := (SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='_副本闯关' AND COLUMN_NAME='通关奖励模版');
SET @ddl := IF(@column_exists=0, 'ALTER TABLE `_副本闯关` ADD COLUMN `通关奖励模版` int(10) unsigned NOT NULL DEFAULT 0 COMMENT ''角色首次推进该关时发放'' AFTER `首领关`', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 按旧关卡序号自动分组，便于迁移已有数据；章节地图和坐标仍需按实际地图填写。
UPDATE `_副本闯关`
SET `章节编号` = FLOOR((`关卡序号` - 1) / 5) + 1,
    `章节内序号` = MOD(`关卡序号` - 1, 5) + 1,
    `首领关` = IF(MOD(`关卡序号` - 1, 5) = 4, 1, 0);

SET @index_exists := (SELECT COUNT(*) FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='_副本闯关' AND INDEX_NAME='uk_章节关卡');
SET @ddl := IF(@index_exists=0, 'ALTER TABLE `_副本闯关` ADD UNIQUE KEY `uk_章节关卡` (`章节编号`,`章节内序号`)', 'SELECT 1');
PREPARE stmt FROM @ddl; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- 配置完成后执行一次，确保所有传送门物体调用新脚本。
UPDATE `gameobject_template`
SET `ScriptName` = 'StageChallengePortal'
WHERE `entry` IN (
  SELECT `传送门编号` FROM (
    SELECT `通关传送门编号` AS `传送门编号` FROM `_副本闯关` WHERE `通关传送门编号` > 0
    UNION
    SELECT `默认传送门物体编号` FROM `_副本闯关章节` WHERE `默认传送门物体编号` > 0
  ) AS `stage_portals`
);

-- 章节配置示例（坐标编号、地图、奖励和物体编号必须替换成实际值）：
-- INSERT INTO `_副本闯关章节`
-- (`章节编号`,`章节名称`,`地图编号`,`入口坐标编号`,`出口坐标编号`,`章节奖励模版`,`下一章节编号`,`默认传送门物体编号`,`启用`)
-- VALUES (1,'第一章',33,50001,50099,10001,2,190000,1);
