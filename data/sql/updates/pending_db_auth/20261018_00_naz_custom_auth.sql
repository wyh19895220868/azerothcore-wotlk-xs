-- NAz custom schema migrated into the AzerothCore pending update system.
-- Wide decimal values are stored as 64-bit BIGINT; client-facing strings remain decimal text.
CREATE TABLE IF NOT EXISTS `动作账号唯一` (
  `账号编号` int(10) unsigned NOT NULL COMMENT '账号编号',
  `动作` int(10) unsigned NOT NULL COMMENT '动作',
  `变量` int(10) unsigned NOT NULL COMMENT '动作账号唯一的变量',
  PRIMARY KEY (`账号编号`,`动作`) USING BTREE,
  KEY `Owner` (`变量`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_兑换码奖励表` (
  `兑换码` varchar(255) CHARACTER SET utf8 NOT NULL DEFAULT '' COMMENT '兑换码',
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

CREATE TABLE IF NOT EXISTS `_account_vip` (
  `AccountId` int(11) unsigned NOT NULL COMMENT '账号ID',
  `Rank` int(11) NOT NULL DEFAULT '0' COMMENT '会员等级',
  `NowJF` bigint(20) NOT NULL DEFAULT '0' COMMENT '当前积分',
  `AsJF` bigint(20) NOT NULL DEFAULT '0' COMMENT '累计积分',
  `泡点` bigint(20) NOT NULL DEFAULT '0' COMMENT '泡点',
  `魔币` bigint(20) DEFAULT NULL COMMENT '魔币',
  `累计魔币` bigint(20) DEFAULT NULL COMMENT '累计魔币',
  PRIMARY KEY (`AccountId`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_account_zz` (
  `Num` int(11) unsigned NOT NULL AUTO_INCREMENT COMMENT '记录序号',
  `AccountId` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '账号ID',
  `Count` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '数量',
  `OnTask` smallint(1) unsigned NOT NULL DEFAULT '0' COMMENT '任务状态',
  `OnTime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '记录时间',
  PRIMARY KEY (`Num`,`AccountId`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `custom_schema_sync_control` (
  `id` tinyint(3) unsigned NOT NULL DEFAULT '1' COMMENT '记录ID',
  `enabled` tinyint(3) unsigned NOT NULL DEFAULT '1' COMMENT '是否启用',
  `database_mask` tinyint(3) unsigned NOT NULL DEFAULT '7' COMMENT '启用同步的数据库掩码',
  `stop_on_error` tinyint(3) unsigned NOT NULL DEFAULT '1' COMMENT '遇到错误时是否停止',
  `check_column_types` tinyint(3) unsigned NOT NULL DEFAULT '1' COMMENT '是否检查字段定义差异',
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Custom schema synchronization options';

CREATE TABLE IF NOT EXISTS `custom_schema_sync_history` (
  `batch_token` varchar(96) NOT NULL COMMENT '结构同步批次标识',
  `database_name` varchar(32) NOT NULL COMMENT '数据库逻辑名称',
  `schema_hash` char(40) NOT NULL DEFAULT '' COMMENT '结构文件哈希',
  `git_revision` varchar(64) NOT NULL DEFAULT '' COMMENT 'Git版本标识',
  `started_at` datetime NOT NULL COMMENT '同步开始时间',
  `finished_at` datetime DEFAULT NULL COMMENT '同步完成时间',
  `status` varchar(16) NOT NULL COMMENT '状态',
  `tables_created` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '已创建数据表数量',
  `columns_added` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '已新增字段数量',
  `indexes_added` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '已新增索引数量',
  `warnings` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '警告数量',
  `error_message` text COMMENT '错误信息',
  PRIMARY KEY (`batch_token`),
  KEY `idx_database_time` (`database_name`,`started_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Custom schema synchronization history';

CREATE TABLE IF NOT EXISTS `custom_schema_sync_state` (
  `database_name` varchar(32) NOT NULL COMMENT '数据库逻辑名称',
  `schema_hash` char(40) NOT NULL DEFAULT '' COMMENT '结构文件哈希',
  `last_attempt_at` datetime DEFAULT NULL COMMENT '最后尝试同步时间',
  `last_success_at` datetime DEFAULT NULL COMMENT '最后成功同步时间',
  `last_status` varchar(16) NOT NULL DEFAULT '' COMMENT '最后一次同步状态',
  `last_error` text COMMENT '最后一次错误信息',
  `tables_created` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '已创建数据表数量',
  `columns_added` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '已新增字段数量',
  `indexes_added` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '已新增索引数量',
  `warnings` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '警告数量',
  PRIMARY KEY (`database_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Latest custom schema synchronization state';
