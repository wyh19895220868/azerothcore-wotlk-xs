-- ============================================================
-- 活动公告卡片表（2026-10-23）
-- 客户端：Interface\AddOns\____Function\OnGameEventShow.lua（“活动”按钮）
-- 服务端：src/server/scripts/Custom/AddonActive/DoAddon.cpp
-- 作用：全背景图 + 大标题 + 底部栏的活动卡片。
-- 说明：
--   事件ID 关联 _事件.事件ID；填 0 表示不关联事件。
--   需求 关联 __模版需求总表.模版编号；0 表示无需求。
--   奖励 关联 __模版奖励总表.模版编号；0 表示无奖励。
--   触发GM命令 可填 GM 命令（多条用 # 分隔）；填写后优先执行命令，不再自动加入事件。
-- ============================================================

CREATE TABLE IF NOT EXISTS `_活动_公告` (
  `注释` varchar(255) NOT NULL DEFAULT '' COMMENT '仅作备注',
  `ID` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '公告卡片编号',
  `事件ID` int(11) NOT NULL DEFAULT '0' COMMENT '关联 _事件.事件ID；0=不关联事件',
  `活动名称` varchar(128) NOT NULL DEFAULT '' COMMENT '卡片大标题；留空用 _事件.事件名称',
  `活动介绍` varchar(1024) NOT NULL DEFAULT '' COMMENT '鼠标提示里的活动介绍',
  `活动背景图` varchar(255) NOT NULL DEFAULT '' COMMENT '整卡背景图路径，例如 Interface\\WorldMap\\ArathiBasin1',
  `按钮名称` varchar(32) NOT NULL DEFAULT '传送' COMMENT '右下角按钮文字',
  `需求` int(11) NOT NULL DEFAULT '0' COMMENT '关联 __模版需求总表.模版编号；0=无需求',
  `奖励` int(11) NOT NULL DEFAULT '0' COMMENT '关联 __模版奖励总表.模版编号；0=无奖励',
  `触发GM命令` varchar(512) NOT NULL DEFAULT '' COMMENT '多条用 # 分隔；填写后优先执行命令',
  PRIMARY KEY (`ID`),
  KEY `idx_event` (`事件ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='活动公告卡片配置';

-- 示例（需要时取消注释并改成自己的事件/模板）：
-- INSERT INTO `_活动_公告`
--   (`注释`, `事件ID`, `活动名称`, `活动介绍`, `活动背景图`, `按钮名称`, `需求`, `奖励`, `触发GM命令`)
-- VALUES
--   ('阿拉希盆地', 101, '阿拉希盆地（12点）', '战场活动，点击传送进入。', 'Interface\\WorldMap\\ArathiBasin1', '传送', 0, 0, '');
