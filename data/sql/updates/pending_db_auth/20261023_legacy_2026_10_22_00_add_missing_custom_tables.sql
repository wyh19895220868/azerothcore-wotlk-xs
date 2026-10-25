-- 335XS: tables that existed only in the live database (not in any source .sql or C++).
-- Generated 2026-10-22 from SHOW CREATE TABLE. UTF-8 (no BOM), utf8mb4 session.
SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

CREATE TABLE IF NOT EXISTS `_兑换码奖励表` (
  `兑换码` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '兑换码',
  `奖励模版` int(11) NOT NULL COMMENT '奖励模版',
  PRIMARY KEY (`兑换码`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=gbk ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_每日上限` (
  `账号编号` int(10) unsigned NOT NULL COMMENT '账号编号',
  `物品编号` int(10) unsigned NOT NULL COMMENT '物品编号',
  `日当前数量` int(10) unsigned NOT NULL COMMENT '日当前数量',
  PRIMARY KEY (`账号编号`,`物品编号`) USING BTREE,
  KEY `Owner` (`日当前数量`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_永久上限` (
  `账号编号` int(10) unsigned NOT NULL COMMENT '账号编号',
  `物品编号` int(10) unsigned NOT NULL COMMENT '物品编号',
  `永当前数量` int(10) unsigned NOT NULL COMMENT '永当前数量',
  PRIMARY KEY (`账号编号`,`物品编号`) USING BTREE,
  KEY `Owner` (`永当前数量`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_账号签到` (
  `账号编号` int(11) unsigned NOT NULL COMMENT '账号编号',
  `日期` int(11) unsigned NOT NULL COMMENT '账号签到的日期',
  `签到时间` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '签到时间',
  PRIMARY KEY (`账号编号`,`日期`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_账号签到存档` (
  `账号编号` int(11) unsigned NOT NULL COMMENT '账号编号',
  `连签` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '连签',
  `累签` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '累签',
  PRIMARY KEY (`账号编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `动作账号唯一` (
  `账号编号` int(10) unsigned NOT NULL COMMENT '账号编号',
  `动作` int(10) unsigned NOT NULL COMMENT '动作',
  `变量` int(10) unsigned NOT NULL COMMENT '动作账号唯一的变量',
  PRIMARY KEY (`账号编号`,`动作`) USING BTREE,
  KEY `Owner` (`变量`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

SET FOREIGN_KEY_CHECKS = 1;
