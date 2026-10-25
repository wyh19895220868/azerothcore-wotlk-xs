-- 335XS: tables that existed only in the live database (not in any source .sql or C++).
-- Generated 2026-10-22 from SHOW CREATE TABLE. UTF-8 (no BOM), utf8mb4 session.
SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

CREATE TABLE IF NOT EXISTS `______物品` (
  `物品标识` int(11) unsigned NOT NULL COMMENT '物品标识',
  `序号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '物品的序号',
  `变量` int(11) NOT NULL DEFAULT '0' COMMENT '物品的变量',
  PRIMARY KEY (`物品标识`,`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `______玩家` (
  `玩家标识` int(11) unsigned NOT NULL COMMENT '玩家标识',
  `序号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '玩家的序号',
  `变量` bigint(20) NOT NULL DEFAULT '0' COMMENT '玩家的变量',
  PRIMARY KEY (`玩家标识`,`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `___日志_物品记录` (
  `玩家Guid` int(10) unsigned NOT NULL COMMENT '玩家Guid',
  `玩家名字` varchar(255) NOT NULL DEFAULT '' COMMENT '玩家名字',
  `玩家账号ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '玩家账号ID',
  `物品编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '正数增加 负数减少',
  `物品数量` int(10) NOT NULL DEFAULT '0' COMMENT '正数表示增加 负数表示减少',
  `类型说明` varchar(255) NOT NULL DEFAULT '' COMMENT '类型说明',
  `操作时间` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '操作时间',
  PRIMARY KEY (`玩家Guid`,`玩家名字`,`玩家账号ID`,`物品编号`,`物品数量`,`类型说明`,`操作时间`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `___日志_积分记录` (
  `玩家Guid` int(10) unsigned NOT NULL COMMENT '玩家Guid',
  `玩家名字` varchar(255) NOT NULL DEFAULT '' COMMENT '玩家名字',
  `玩家账号ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '玩家账号ID',
  `积分状况` bigint(20) NOT NULL DEFAULT '0' COMMENT '正数表示增加 负数表示减少',
  `类型说明` varchar(255) NOT NULL DEFAULT '' COMMENT '类型说明',
  `操作时间` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '操作时间',
  PRIMARY KEY (`玩家Guid`,`玩家名字`,`玩家账号ID`,`积分状况`,`类型说明`,`操作时间`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_公会战存档` (
  `公会编号` int(11) NOT NULL COMMENT '公会编号',
  `当前胜利次数` int(11) NOT NULL DEFAULT '0' COMMENT '当前胜利次数',
  PRIMARY KEY (`公会编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_副本重置存档` (
  `玩家标识` int(11) unsigned NOT NULL COMMENT '玩家标识',
  `副本地图` int(11) NOT NULL DEFAULT '0' COMMENT '副本地图',
  `副本难度` int(11) NOT NULL DEFAULT '-1' COMMENT '副本难度',
  `次数` int(11) NOT NULL DEFAULT '0' COMMENT '次数',
  PRIMARY KEY (`玩家标识`,`副本地图`,`副本难度`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_副本闯关进度` (
  `角色GUID` int(10) unsigned NOT NULL COMMENT '角色GUID',
  `最高已解锁关卡` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '最高已解锁关卡',
  `最高完成章节` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '最高完成章节',
  `更新时间` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
  PRIMARY KEY (`角色GUID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC COMMENT='章节制副本闯关角色进度';

CREATE TABLE IF NOT EXISTS `_反作弊触发记录` (
  `记录ID` bigint(20) unsigned NOT NULL AUTO_INCREMENT COMMENT '自增主键',
  `玩家GUID` int(10) unsigned NOT NULL COMMENT '玩家角色GUID',
  `玩家名字` varchar(32) NOT NULL COMMENT '玩家角色名',
  `账号ID` int(10) unsigned NOT NULL COMMENT '账号ID',
  `触发时间` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '触发时间',
  `作弊类型` varchar(32) NOT NULL COMMENT '作弊类型:加速/飞行/瞬移/跳跃/传送平面/重力/Z轴/无视控制/水上行走',
  `地图ID` int(10) unsigned NOT NULL COMMENT '触发时的地图ID',
  `区域ID` int(10) unsigned NOT NULL COMMENT '触发时的区域ID',
  `坐标X` float NOT NULL COMMENT '触发时的X坐标',
  `坐标Y` float NOT NULL COMMENT '触发时的Y坐标',
  `坐标Z` float NOT NULL COMMENT '触发时的Z坐标',
  `服务器速度` float NOT NULL COMMENT '服务器计算的正常速度',
  `客户端速度` float NOT NULL COMMENT '客户端上报的速度(加速检测用)',
  `移动距离` float NOT NULL COMMENT '本次移动距离',
  `时间差` int(10) unsigned NOT NULL COMMENT '与上次移动的时间差(ms)',
  `处理结果` varchar(32) NOT NULL COMMENT '处理结果:传送回原点/仅记录/忽略',
  PRIMARY KEY (`记录ID`),
  KEY `idx_player_guid` (`玩家GUID`),
  KEY `idx_account_id` (`账号ID`),
  KEY `idx_trigger_time` (`触发时间`),
  KEY `idx_cheat_type` (`作弊类型`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='反作弊系统触发记录表';

CREATE TABLE IF NOT EXISTS `_图鉴_物品_存档` (
  `玩家标识` int(11) unsigned NOT NULL COMMENT '玩家标识',
  `物品编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '物品编号',
  `页面` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '图鉴_物品_存档的页面',
  PRIMARY KEY (`玩家标识`,`物品编号`,`页面`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_图鉴_生物_存档` (
  `玩家标识` int(11) unsigned NOT NULL COMMENT '玩家标识',
  `生物编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '生物编号',
  `击杀数量` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '击杀数量',
  `页面` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '图鉴_生物_存档的页面',
  PRIMARY KEY (`玩家标识`,`生物编号`,`页面`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_大秘境_待领取奖励` (
  `角色guid` int(10) unsigned NOT NULL COMMENT '玩家角色GUID',
  `玩家名称` varchar(50) NOT NULL COMMENT '玩家角色名称',
  `周期编号` int(11) NOT NULL COMMENT '第几个周期的奖励',
  `上期总评分` decimal(12,2) DEFAULT '0.00' COMMENT '上期总评分',
  `已领取宝箱id` int(11) DEFAULT NULL COMMENT '已领取的宝箱ID（NULL表示未领取）',
  `已领取时间` timestamp NULL DEFAULT NULL COMMENT '领取时间',
  PRIMARY KEY (`角色guid`) USING BTREE,
  KEY `idx_cycle` (`周期编号`) USING BTREE,
  KEY `idx_claimed` (`已领取宝箱id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC COMMENT='大秘境待领取奖励表';

CREATE TABLE IF NOT EXISTS `_大秘境_排行榜快照` (
  `快照id` int(11) NOT NULL AUTO_INCREMENT COMMENT '快照ID',
  `周期编号` int(11) NOT NULL COMMENT '第几个周期（从1开始）',
  `排名` int(11) NOT NULL COMMENT '玩家排名',
  `角色guid` int(10) unsigned NOT NULL COMMENT '玩家角色GUID',
  `玩家名称` varchar(50) NOT NULL COMMENT '玩家角色名称',
  `最终得分` decimal(12,2) DEFAULT '0.00' COMMENT '最终得分',
  PRIMARY KEY (`快照id`) USING BTREE,
  KEY `idx_cycle` (`周期编号`,`排名`) USING BTREE,
  KEY `idx_guid` (`角色guid`) USING BTREE,
  KEY `idx_rank` (`排名`) USING BTREE,
  KEY `idx_cycle_rank` (`周期编号`,`排名`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC COMMENT='大秘境排行榜快照表（历史数据）';

CREATE TABLE IF NOT EXISTS `_大秘境_玩家累积评分` (
  `角色guid` int(10) unsigned NOT NULL COMMENT '玩家角色GUID',
  `玩家名称` varchar(50) NOT NULL COMMENT '玩家角色名称',
  `累积总评分` decimal(12,2) DEFAULT '0.00' COMMENT '累积总评分',
  PRIMARY KEY (`角色guid`) USING BTREE,
  KEY `idx_score` (`累积总评分`) USING BTREE,
  KEY `idx_name` (`玩家名称`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC COMMENT='大秘境玩家累积评分表（当前周期）';

CREATE TABLE IF NOT EXISTS `_属性奖励存档` (
  `玩家标识` int(11) NOT NULL COMMENT '使用类型的',
  `法力值` bigint(20) NOT NULL DEFAULT '0' COMMENT '法力值',
  `生命值` bigint(20) NOT NULL DEFAULT '0' COMMENT '生命值',
  `敏捷` bigint(20) NOT NULL DEFAULT '0' COMMENT '敏捷',
  `力量` bigint(20) NOT NULL DEFAULT '0' COMMENT '力量',
  `智力` bigint(20) NOT NULL DEFAULT '0' COMMENT '智力',
  `精神` bigint(20) NOT NULL DEFAULT '0' COMMENT '精神',
  `耐力` bigint(20) NOT NULL DEFAULT '0' COMMENT '耐力',
  `防御等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '防御等级',
  `躲闪等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '躲闪等级',
  `招架等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '招架等级',
  `格挡等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '格挡等级',
  `近战命中等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '近战命中等级',
  `远程命中等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '远程命中等级',
  `法术命中等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '法术命中等级',
  `近战暴击等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '近战暴击等级',
  `远程暴击等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '远程暴击等级',
  `法术暴击等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '法术暴击等级',
  `近战命中躲闪等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '近战命中躲闪等级',
  `远程命中躲闪等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '远程命中躲闪等级',
  `法术命中躲闪等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '法术命中躲闪等级',
  `近战暴击躲闪等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '近战暴击躲闪等级',
  `远程暴击躲闪等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '远程暴击躲闪等级',
  `法术暴击躲闪等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '法术暴击躲闪等级',
  `近战急速等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '近战急速等级',
  `远程急速等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '远程急速等级',
  `法术急速等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '法术急速等级',
  `命中等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '命中等级',
  `暴击等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '暴击等级',
  `命中躲闪等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '命中躲闪等级',
  `暴击躲闪等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '暴击躲闪等级',
  `韧性等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '韧性等级',
  `急速等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '急速等级',
  `精准等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '精准等级',
  `攻击强度` bigint(20) NOT NULL DEFAULT '0' COMMENT '攻击强度',
  `远程攻击强度` bigint(20) NOT NULL DEFAULT '0' COMMENT '远程攻击强度',
  `形态攻击强度` bigint(20) NOT NULL DEFAULT '0' COMMENT '形态攻击强度',
  `法术治疗效果` bigint(20) NOT NULL DEFAULT '0' COMMENT '法术治疗效果',
  `法术伤害效果` bigint(20) NOT NULL DEFAULT '0' COMMENT '法术伤害效果',
  `每5秒回复法力` bigint(20) NOT NULL DEFAULT '0' COMMENT '每5秒回复法力',
  `护甲穿透等级` bigint(20) NOT NULL DEFAULT '0' COMMENT '护甲穿透等级',
  `法术强度提高` bigint(20) NOT NULL DEFAULT '0' COMMENT '法术强度提高',
  `每5秒回复生命` bigint(20) NOT NULL DEFAULT '0' COMMENT '每5秒回复生命',
  `法术穿透` bigint(20) NOT NULL DEFAULT '0' COMMENT '法术穿透',
  `格挡值提高` bigint(20) NOT NULL DEFAULT '0' COMMENT '格挡值提高',
  PRIMARY KEY (`玩家标识`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_幻化` (
  `GUID` int(11) unsigned NOT NULL COMMENT '全局唯一标识',
  `FakeEntry` int(11) unsigned NOT NULL COMMENT '幻化后的物品模板ID',
  `Owner` int(11) unsigned NOT NULL COMMENT '所属角色GUID',
  PRIMARY KEY (`GUID`) USING BTREE,
  KEY `Owner` (`Owner`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_幻化保存` (
  `Owner` int(10) unsigned NOT NULL COMMENT '所属角色GUID',
  `PresetID` tinyint(3) unsigned NOT NULL COMMENT '幻化预设ID',
  `SetName` text COMMENT '幻化套装名称',
  `SetData` text COMMENT '幻化套装数据',
  PRIMARY KEY (`Owner`,`PresetID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_1';

CREATE TABLE IF NOT EXISTS `_授权自动拾取` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '唯一ID',
  `玩家账号` varchar(50) NOT NULL DEFAULT '' COMMENT '玩家注册账号，授权该账号下所有角色',
  `自动拾取` tinyint(1) unsigned NOT NULL DEFAULT '1' COMMENT '1开启 0关闭',
  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE KEY `idx_玩家账号` (`玩家账号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC COMMENT='自动拾取账号授权表';

CREATE TABLE IF NOT EXISTS `_每日上限` (
  `玩家标识` int(10) unsigned NOT NULL COMMENT '玩家标识',
  `物品编号` int(10) unsigned NOT NULL COMMENT '物品编号',
  `日当前数量` int(10) unsigned NOT NULL COMMENT '日当前数量',
  PRIMARY KEY (`玩家标识`,`物品编号`) USING BTREE,
  KEY `Owner` (`日当前数量`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_每日使用次数` (
  `玩家标识` int(11) NOT NULL COMMENT '玩家标识',
  `物品编号` int(11) NOT NULL DEFAULT '0' COMMENT '物品编号',
  `每日次数` int(11) NOT NULL DEFAULT '0' COMMENT '每日次数',
  PRIMARY KEY (`玩家标识`,`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_比武存档` (
  `玩家标识` int(10) NOT NULL DEFAULT '0' COMMENT '玩家标识',
  `当前排名` int(8) NOT NULL DEFAULT '0' COMMENT '当前排名',
  `上次排名` int(8) NOT NULL DEFAULT '0' COMMENT '上次排名',
  PRIMARY KEY (`玩家标识`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_永久上限` (
  `玩家标识` int(10) unsigned NOT NULL COMMENT '玩家标识',
  `物品编号` int(10) unsigned NOT NULL COMMENT '物品编号',
  `永当前数量` int(10) unsigned NOT NULL COMMENT '永当前数量',
  PRIMARY KEY (`玩家标识`,`物品编号`) USING BTREE,
  KEY `Owner` (`永当前数量`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_永久使用次数` (
  `玩家标识` int(11) NOT NULL COMMENT '玩家标识',
  `物品编号` int(11) NOT NULL DEFAULT '0' COMMENT '物品编号',
  `永久次数` int(11) NOT NULL DEFAULT '0' COMMENT '永久次数',
  PRIMARY KEY (`玩家标识`,`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_法宝存档` (
  `玩家标识` int(11) unsigned NOT NULL COMMENT '玩家标识',
  `物品编号或技能` int(11) NOT NULL DEFAULT '0' COMMENT '物品编号或技能',
  `类型` int(11) NOT NULL DEFAULT '1' COMMENT '法宝存档的类型',
  `序号` int(11) NOT NULL DEFAULT '0' COMMENT '法宝存档的序号',
  `页面` int(11) NOT NULL DEFAULT '1' COMMENT '法宝存档的页面',
  PRIMARY KEY (`玩家标识`,`物品编号或技能`,`序号`,`页面`,`类型`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_活跃度存档1` (
  `玩家标识` int(11) NOT NULL COMMENT '玩家标识',
  `序号` int(11) NOT NULL DEFAULT '0' COMMENT '活跃度存档1的序号',
  `类型` int(11) NOT NULL DEFAULT '0' COMMENT '活跃度存档1的类型',
  `变量1` int(11) NOT NULL DEFAULT '0' COMMENT '变量1',
  `变量2` int(11) NOT NULL DEFAULT '0' COMMENT '变量2',
  PRIMARY KEY (`玩家标识`,`序号`,`类型`,`变量1`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_活跃度存档2` (
  `玩家标识` int(11) NOT NULL COMMENT '玩家标识',
  `活跃段` int(11) NOT NULL DEFAULT '0' COMMENT '活跃段',
  `是否奖励` smallint(1) NOT NULL DEFAULT '0' COMMENT '是否奖励',
  PRIMARY KEY (`玩家标识`,`活跃段`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_游戏模式状态` (
  `角色guid` int(10) unsigned NOT NULL COMMENT '角色guid',
  `降级_启用` tinyint(3) unsigned DEFAULT '0' COMMENT '降级_启用',
  `降级_通关` tinyint(3) unsigned DEFAULT '0' COMMENT '降级_通关',
  `减半_启用` tinyint(3) unsigned DEFAULT '0' COMMENT '减半_启用',
  `减半_通关` tinyint(3) unsigned DEFAULT '0' COMMENT '减半_通关',
  `轮回_次数` int(10) unsigned DEFAULT '0' COMMENT '轮回_次数',
  `孤狼_启用` tinyint(3) unsigned DEFAULT '0' COMMENT '孤狼_启用',
  `金砖_启用` tinyint(3) unsigned DEFAULT '0' COMMENT '金砖_启用',
  `天堂_启用` tinyint(3) unsigned DEFAULT '0' COMMENT '天堂_启用',
  `地狱_启用` tinyint(3) unsigned DEFAULT '0' COMMENT '地狱_启用',
  `地狱_通关` tinyint(3) unsigned DEFAULT '0' COMMENT '地狱_通关',
  PRIMARY KEY (`角色guid`) USING BTREE,
  KEY `idx_hardcore` (`降级_启用`,`降级_通关`) USING BTREE,
  KEY `idx_halfxp` (`减半_启用`,`减半_通关`) USING BTREE,
  KEY `idx_hell` (`地狱_启用`,`地狱_通关`) USING BTREE,
  KEY `idx_lonewolf` (`孤狼_启用`) USING BTREE,
  KEY `idx_heaven` (`天堂_启用`) USING BTREE,
  KEY `idx_goldbrick` (`金砖_启用`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC COMMENT='游戏模式玩家状态表';

CREATE TABLE IF NOT EXISTS `_物品套装_数据` (
  `物品ID` int(10) unsigned NOT NULL COMMENT '物品模板ID',
  `物品GUID` int(10) unsigned NOT NULL COMMENT '物品的唯一GUID',
  `套装ID` int(10) unsigned NOT NULL COMMENT '分配的套装ID',
  PRIMARY KEY (`物品GUID`),
  KEY `idx_set_id` (`套装ID`),
  KEY `idx_item_id` (`物品ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='物品套装分配记录表 - 记录物品和套装的映射关系';

CREATE TABLE IF NOT EXISTS `_物品技能_数据` (
  `物品ID` int(10) unsigned NOT NULL COMMENT '物品Entry ID（item_template.entry）',
  `物品GUID` bigint(20) unsigned NOT NULL COMMENT '物品实例唯一标识（item_instance.guid）',
  `技能模板ID` varchar(255) COLLATE utf8mb4_general_ci NOT NULL COMMENT '技能模板ID列表，逗号分隔，如"1,2,3"',
  `技能ID` varchar(255) COLLATE utf8mb4_general_ci NOT NULL COMMENT '法术ID列表，逗号分隔，如"133,10,122"',
  `技能触发类型` varchar(255) COLLATE utf8mb4_general_ci NOT NULL COMMENT '触发类型列表，逗号分隔，如"0,1,2"（0=使用,1=装备,2=击中,100=学习）',
  `触发几率` varchar(255) COLLATE utf8mb4_general_ci NOT NULL DEFAULT '100' COMMENT '触发几率列表，逗号分隔，如"100,50,30"',
  PRIMARY KEY (`物品GUID`) USING BTREE COMMENT '物品GUID为主键，一个物品一行存储所有技能',
  KEY `idx_item_id` (`物品ID`) USING BTREE COMMENT '物品ID索引，用于查询同类物品'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC COMMENT='物品技能数据表 - 一个物品GUID一行，多个技能用逗号分隔';

CREATE TABLE IF NOT EXISTS `_玩家双甲` (
  `物品标识` int(11) unsigned NOT NULL COMMENT '物品标识',
  `物品编号` int(11) unsigned NOT NULL COMMENT 'Item entry',
  `玩家编号` int(11) unsigned NOT NULL COMMENT '玩家编号',
  `装备位置` int(11) unsigned NOT NULL COMMENT '装备位置',
  PRIMARY KEY (`物品标识`) USING BTREE,
  KEY `Owner` (`玩家编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_玩家战宠存档` (
  `玩家标识` int(11) unsigned NOT NULL COMMENT '玩家标识',
  `战宠位置` int(11) unsigned NOT NULL COMMENT '战宠位置',
  `战宠编号` int(11) unsigned NOT NULL COMMENT '战宠编号',
  `战宠阶级` int(11) unsigned NOT NULL COMMENT '战宠阶级',
  `战宠经验` int(11) unsigned NOT NULL COMMENT '战宠经验',
  PRIMARY KEY (`玩家标识`,`战宠位置`) USING BTREE,
  KEY `Owner` (`战宠阶级`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_神兽翻牌击杀进度` (
  `玩家GUID` int(10) unsigned NOT NULL COMMENT '玩家GUID',
  `卡牌编号` int(10) unsigned NOT NULL COMMENT '卡牌编号',
  `生物编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '生物编号',
  `击杀数量` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '击杀数量',
  `更新时间` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
  PRIMARY KEY (`玩家GUID`,`卡牌编号`),
  KEY `idx_生物编号` (`生物编号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='神兽翻牌击杀解锁进度';

CREATE TABLE IF NOT EXISTS `_神兽翻牌存档` (
  `玩家GUID` int(10) unsigned NOT NULL COMMENT '玩家GUID',
  `卡牌编号` int(10) unsigned NOT NULL COMMENT '卡牌编号',
  `品质等级` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '0D灰 1C白 2B绿 3A蓝 4S紫 5SS橙 6SSR暗金 7SSS红 8SR粉 9UR彩色',
  `翻牌时间` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '翻牌时间',
  PRIMARY KEY (`玩家GUID`,`卡牌编号`),
  KEY `idx_卡牌编号` (`卡牌编号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='神兽翻牌角色存档';

CREATE TABLE IF NOT EXISTS `_秘境钥匙存档` (
  `钥匙标识` int(11) NOT NULL COMMENT '钥匙标识',
  `编号` int(11) NOT NULL DEFAULT '0' COMMENT '秘境钥匙存档的编号',
  `A组` int(11) NOT NULL DEFAULT '0' COMMENT 'A组',
  `固定技能` varchar(255) NOT NULL DEFAULT '' COMMENT '固定技能',
  `B组` int(11) NOT NULL DEFAULT '0' COMMENT 'B组',
  `随机技能` varchar(255) NOT NULL DEFAULT '' COMMENT '随机技能',
  `关联地图` int(11) NOT NULL DEFAULT '-1' COMMENT '关联地图',
  PRIMARY KEY (`钥匙标识`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备特殊成长经验存档` (
  `装备标识` int(11) NOT NULL COMMENT '装备标识',
  `装备编号` int(11) NOT NULL DEFAULT '0' COMMENT '装备编号',
  `成长等级` int(11) NOT NULL DEFAULT '0' COMMENT '成长等级',
  `当前经验` int(11) NOT NULL DEFAULT '0' COMMENT '当前经验',
  PRIMARY KEY (`装备标识`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备符文存档` (
  `装备标识` int(11) NOT NULL COMMENT '装备标识',
  `装备编号` int(11) NOT NULL DEFAULT '0' COMMENT '装备编号',
  `当前插入数量` int(11) NOT NULL DEFAULT '0' COMMENT '当前插入数量',
  `符文插槽数量` int(11) NOT NULL DEFAULT '0' COMMENT '符文插槽数量',
  `符文宝石1` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石1',
  `符文宝石2` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石2',
  `符文宝石3` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石3',
  `符文宝石4` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石4',
  `符文宝石5` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石5',
  `符文宝石6` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石6',
  `符文宝石7` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石7',
  `符文宝石8` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石8',
  `自带技能1` int(11) NOT NULL DEFAULT '0' COMMENT '自带技能1',
  `自带技能2` int(11) NOT NULL DEFAULT '0' COMMENT '自带技能2',
  `自带技能3` int(11) NOT NULL DEFAULT '0' COMMENT '自带技能3',
  `自带技能4` int(11) NOT NULL DEFAULT '0' COMMENT '自带技能4',
  `自带技能5` int(11) NOT NULL DEFAULT '0' COMMENT '自带技能5',
  `自带技能6` int(11) NOT NULL DEFAULT '0' COMMENT '自带技能6',
  `自带技能7` int(11) NOT NULL DEFAULT '0' COMMENT '自带技能7',
  `自带技能8` int(11) NOT NULL DEFAULT '0' COMMENT '自带技能8',
  `组合技名称` varchar(255) NOT NULL DEFAULT '' COMMENT '组合技名称',
  `组合技能列表` varchar(255) NOT NULL DEFAULT '' COMMENT '组合技能列表',
  PRIMARY KEY (`装备标识`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备超级随机存档` (
  `装备标识` int(11) NOT NULL COMMENT '装备标识',
  `装备编号` int(11) NOT NULL DEFAULT '0' COMMENT '装备编号',
  `强化等级` int(11) NOT NULL DEFAULT '0' COMMENT '强化等级',
  `前缀名称` varchar(255) NOT NULL DEFAULT '' COMMENT '前缀名称',
  `后缀名称` varchar(255) NOT NULL DEFAULT '' COMMENT '后缀名称',
  `强化名称` varchar(255) NOT NULL DEFAULT '' COMMENT '强化名称',
  `武器最小伤害` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '武器最小伤害',
  `武器最大伤害` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '武器最大伤害',
  `属性类型1` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型1',
  `属性值1` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值1',
  `属性类型2` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型2',
  `属性值2` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值2',
  `属性类型3` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型3',
  `属性值3` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值3',
  `属性类型4` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型4',
  `属性值4` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值4',
  `属性类型5` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型5',
  `属性值5` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值5',
  `属性类型6` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型6',
  `属性值6` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值6',
  `属性类型7` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型7',
  `属性值7` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值7',
  `属性类型8` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型8',
  `属性值8` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值8',
  `属性类型9` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型9',
  `属性值9` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值9',
  `属性类型10` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型10',
  `属性值10` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值10',
  `属性类型11` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型11',
  `属性值11` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值11',
  `属性类型12` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型12',
  `属性值12` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值12',
  `属性类型13` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型13',
  `属性值13` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值13',
  `属性类型14` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型14',
  `属性值14` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值14',
  `属性类型15` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型15',
  `属性值15` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值15',
  `属性类型16` int(11) NOT NULL DEFAULT '-1' COMMENT '属性类型16',
  `属性值16` bigint(20) NOT NULL DEFAULT '0' COMMENT '属性值16',
  `技能1` int(11) NOT NULL DEFAULT '0' COMMENT '技能1',
  `技能2` int(11) NOT NULL DEFAULT '0' COMMENT '技能2',
  `技能3` int(11) NOT NULL DEFAULT '0' COMMENT '技能3',
  `技能4` int(11) NOT NULL DEFAULT '0' COMMENT '技能4',
  `技能5` int(11) NOT NULL DEFAULT '0' COMMENT '技能5',
  `技能6` int(11) NOT NULL DEFAULT '0' COMMENT '技能6',
  `技能7` int(11) NOT NULL DEFAULT '0' COMMENT '技能7',
  `技能8` int(11) NOT NULL DEFAULT '0' COMMENT '技能8',
  `随机属性1` bigint(20) NOT NULL DEFAULT '1' COMMENT '随机属性1',
  `随机属性2` bigint(20) NOT NULL DEFAULT '1' COMMENT '随机属性2',
  `随机属性3` bigint(20) NOT NULL DEFAULT '1' COMMENT '随机属性3',
  `随机属性4` bigint(20) NOT NULL DEFAULT '1' COMMENT '随机属性4',
  `随机属性5` bigint(20) NOT NULL DEFAULT '1' COMMENT '随机属性5',
  `随机属性6` bigint(20) NOT NULL DEFAULT '1' COMMENT '随机属性6',
  `随机属性7` bigint(20) NOT NULL DEFAULT '1' COMMENT '随机属性7',
  PRIMARY KEY (`装备标识`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_附魔洗练存档` (
  `物品标识` int(11) unsigned NOT NULL COMMENT '物品标识',
  `物品编号` int(11) NOT NULL DEFAULT '0' COMMENT '物品编号',
  `洗练次数` int(11) NOT NULL DEFAULT '0' COMMENT '洗练次数',
  PRIMARY KEY (`物品标识`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `_附魔锁定存档` (
  `玩家标识` int(11) unsigned NOT NULL COMMENT '玩家标识',
  `物品标识` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '物品标识',
  `序号` int(11) NOT NULL DEFAULT '1' COMMENT '附魔锁定存档的序号',
  `锁定` tinyint(3) NOT NULL DEFAULT '0' COMMENT '锁定',
  PRIMARY KEY (`玩家标识`,`物品标识`,`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC COMMENT='6_2';

CREATE TABLE IF NOT EXISTS `待鉴定物品标记` (
  `物品GUID` int(10) unsigned NOT NULL COMMENT '物品GUID（主键）',
  `物品ID` int(10) unsigned NOT NULL COMMENT '物品Entry ID',
  `玩家GUID` int(10) unsigned NOT NULL COMMENT '玩家GUID',
  `幻境倍率` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '拾取时的幻境倍率',
  `幻境倍率模式` tinyint(4) NOT NULL DEFAULT '0' COMMENT '幻境属性模式(-1=关闭,0=乘法,1=加法)',
  `鉴定组ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '预设的鉴定组ID',
  PRIMARY KEY (`物品GUID`),
  KEY `idx_玩家GUID` (`玩家GUID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='待鉴定物品标记表 - 性能优化：拾取时只标记，手动鉴定';

CREATE TABLE IF NOT EXISTS `物品_鉴定记录` (
  `记录ID` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '自增主键',
  `玩家GUID` int(10) unsigned NOT NULL COMMENT '玩家GUID',
  `物品GUID` int(10) unsigned NOT NULL COMMENT '物品GUID',
  `物品ID` int(10) unsigned NOT NULL COMMENT '物品Entry ID',
  `鉴定模板ID` int(10) unsigned NOT NULL COMMENT '使用的鉴定模板ID',
  `是否获得成长` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得成长属性',
  `成长组ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '成长属性组ID',
  `是否获得强化` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得强化属性',
  `强化组ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '强化属性组ID',
  `是否获得基础属性` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得基础随机属性',
  `基础属性数量` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '获得的基础属性数量',
  `基础属性组ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '基础属性组ID',
  `基础属性详情` varchar(500) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '基础属性详情（格式：类型 值,类型 值, 例如：3 10,5 15,）',
  `是否获得追加属性` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得追加随机属性',
  `追加属性数量` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '获得的追加属性数量',
  `追加属性组ID列表` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '追加属性组ID（逗号分隔）',
  `是否获得符文凹槽` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得符文凹槽',
  `符文凹槽数量` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '获得的符文凹槽数量',
  `是否获得技能` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得技能效果',
  `技能组ID列表` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '技能组ID（逗号分隔）',
  `是否获得套装` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否获得套装属性',
  `套装组ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '套装组ID',
  `套装ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '具体套装ID',
  `消耗金币` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '鉴定消耗的金币（铜币）',
  `成功率` int(10) unsigned NOT NULL DEFAULT '100' COMMENT '鉴定时的成功率',
  PRIMARY KEY (`记录ID`),
  UNIQUE KEY `idx_物品GUID` (`物品GUID`),
  KEY `idx_玩家GUID` (`玩家GUID`),
  KEY `idx_物品ID` (`物品ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='物品鉴定记录表';

CREATE TABLE IF NOT EXISTS `物品属性_数据` (
  `物品ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '物品Entry ID',
  `物品GUID` bigint(20) unsigned NOT NULL COMMENT '物品唯一标识符',
  `基础属性` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci COMMENT '基础属性（替换官方属性），格式：属性ID 值,属性ID 值,...（例如：3 10,4 10,5 10）',
  `追加属性` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci COMMENT '追加属性（额外添加），格式：属性ID 值,属性ID 值,...（例如：3 10,4 10,5 10）',
  PRIMARY KEY (`物品GUID`) USING BTREE,
  KEY `idx_item_id` (`物品ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC COMMENT='物品属性数据表';

CREATE TABLE IF NOT EXISTS `物品强化_记录` (
  `guid` int(10) unsigned NOT NULL COMMENT '物品GUID',
  `owner_guid` int(10) unsigned NOT NULL COMMENT '所有者GUID',
  `物品模板ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '物品模板ID',
  `强化组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '强化组号，用于选择对应的强化模板',
  `强化等级` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '当前强化等级',
  `强化经验` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '当前强化经验',
  `属性值` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci COMMENT '强化属性值，格式：属性类型 值,属性类型 值，如：7 10,5 15,3 8',
  `品质等级` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '当前品质等级',
  `品质进阶次数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '品质进阶次数',
  `强化次数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '强化次数',
  `失败次数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '失败次数',
  `最后强化时间` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '最后强化时间',
  PRIMARY KEY (`guid`),
  KEY `idx_owner` (`owner_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='物品强化记录表';

CREATE TABLE IF NOT EXISTS `物品成长_玩家记录` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `玩家GUID` int(10) unsigned NOT NULL,
  `物品GUID` int(10) unsigned NOT NULL,
  `物品模板ID` int(10) unsigned NOT NULL,
  `当前等级` int(10) unsigned NOT NULL DEFAULT '0',
  `当前经验` int(10) unsigned NOT NULL DEFAULT '0',
  `升级经验` int(10) unsigned NOT NULL DEFAULT '100' COMMENT '升级到下一等级所需的经验值',
  `物品属性_模板_组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '物品所属的属性模板组，用于确定成长路径',
  `成长次数` int(10) unsigned NOT NULL DEFAULT '0',
  `失败次数` int(10) unsigned NOT NULL DEFAULT '0',
  `成长属性` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci COMMENT '成长获得的属性加成，格式：7 5,3 10,4 8（属性类型 值,属性类型 值）',
  `百分比属性` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci COMMENT '百分比成长获得的属性加成，格式：0 50,1 30（属性类型 值,属性类型 值）',
  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE KEY `物品GUID_索引` (`物品GUID`) USING BTREE,
  KEY `玩家GUID_索引` (`玩家GUID`) USING BTREE,
  KEY `属性组_索引` (`物品属性_模板_组`) USING BTREE
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC COMMENT='玩家物品成长记录表';

CREATE TABLE IF NOT EXISTS `玩家装备属性增强` (
  `记录ID` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '记录ID',
  `玩家GUID` int(10) unsigned NOT NULL COMMENT '玩家GUID',
  `装备GUID` int(10) unsigned NOT NULL COMMENT '装备GUID',
  `装备ID` int(10) unsigned NOT NULL COMMENT '装备模板ID',
  `属性倍率` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '属性倍率',
  `属性倍率模式` tinyint(4) NOT NULL DEFAULT '0' COMMENT '属性模式(-1=关闭,0=乘法,1=加法)',
  `增强属性数据` text COLLATE utf8mb4_unicode_ci COMMENT '增强后的属性数据，格式：属性类型 原值 增强值,属性类型 原值 增强值',
  `鉴定组ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '使用的鉴定组ID',
  `鉴定属性数据` text COLLATE utf8mb4_unicode_ci COMMENT '鉴定系统的自定义属性数据（JSON格式）',
  PRIMARY KEY (`记录ID`),
  UNIQUE KEY `装备GUID` (`装备GUID`),
  KEY `玩家GUID` (`玩家GUID`),
  KEY `装备ID` (`装备ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='玩家装备属性增强表';

CREATE TABLE IF NOT EXISTS `生物采集_日志` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT COMMENT '日志ID',
  `操作类型` enum('种植','采集','施肥') NOT NULL COMMENT '操作类型',
  `玩家GUID` bigint(20) unsigned NOT NULL COMMENT '操作玩家GUID',
  `玩家名字` varchar(12) NOT NULL COMMENT '操作玩家名字',
  `配置ID` int(10) unsigned NOT NULL COMMENT '关联配置表ID',
  `生物GUID` bigint(20) unsigned DEFAULT NULL COMMENT '生物GUID',
  `种植者GUID` bigint(20) unsigned DEFAULT NULL COMMENT '种植者GUID（采集时有效）',
  `种植者名字` varchar(12) DEFAULT NULL COMMENT '种植者名字（采集时有效）',
  `地图ID` int(10) unsigned NOT NULL COMMENT '地图ID',
  `坐标X` float NOT NULL COMMENT 'X坐标',
  `坐标Y` float NOT NULL COMMENT 'Y坐标',
  `坐标Z` float NOT NULL COMMENT 'Z坐标',
  `获得物品` text COMMENT '获得的物品（JSON格式）',
  `操作时间` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '操作时间',
  PRIMARY KEY (`id`),
  KEY `idx_player` (`玩家GUID`),
  KEY `idx_type` (`操作类型`),
  KEY `idx_time` (`操作时间`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='生物采集操作日志表（Characters库）';

CREATE TABLE IF NOT EXISTS `生物采集_玩家数据` (
  `玩家GUID` bigint(20) unsigned NOT NULL COMMENT '玩家GUID',
  `今日种植次数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '今日种植次数',
  `最后重置日期` date NOT NULL COMMENT '最后重置日期',
  PRIMARY KEY (`玩家GUID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='生物采集玩家数据表（Characters库）';

CREATE TABLE IF NOT EXISTS `生物采集_种植记录` (
  `guid` bigint(20) unsigned NOT NULL COMMENT '生物GUID',
  `配置ID` int(10) unsigned NOT NULL COMMENT '关联配置表ID',
  `种植者GUID` bigint(20) unsigned NOT NULL COMMENT '种植者角色GUID',
  `种植者名字` varchar(12) NOT NULL COMMENT '种植者角色名',
  `地图ID` int(10) unsigned NOT NULL COMMENT '地图ID',
  `坐标X` float NOT NULL COMMENT 'X坐标',
  `坐标Y` float NOT NULL COMMENT 'Y坐标',
  `坐标Z` float NOT NULL COMMENT 'Z坐标',
  `朝向` float NOT NULL COMMENT '朝向',
  `种植时间` bigint(20) unsigned NOT NULL COMMENT '种植时间戳',
  `当前成熟度` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '当前成熟度（0-100）',
  `剩余掉落百分比` float NOT NULL DEFAULT '100' COMMENT '剩余掉落百分比',
  PRIMARY KEY (`guid`),
  KEY `idx_planter` (`种植者GUID`),
  KEY `idx_config` (`配置ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='生物采集种植记录表（Characters库）';

CREATE TABLE IF NOT EXISTS `生物采集_采集冷却` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT COMMENT 'ID',
  `生物GUID` bigint(20) unsigned NOT NULL COMMENT '生物GUID',
  `玩家IP` varchar(45) NOT NULL COMMENT '玩家IP地址',
  `玩家GUID` bigint(20) unsigned NOT NULL COMMENT '玩家GUID',
  `玩家名字` varchar(12) NOT NULL COMMENT '玩家名字',
  `采集时间` bigint(20) unsigned NOT NULL COMMENT '采集时间戳',
  `冷却结束时间` bigint(20) unsigned NOT NULL COMMENT '冷却结束时间戳',
  PRIMARY KEY (`id`),
  KEY `idx_creature_ip` (`生物GUID`,`玩家IP`),
  KEY `idx_cooldown` (`冷却结束时间`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='生物采集冷却表（Characters库）';

CREATE TABLE IF NOT EXISTS `生物采集_野生生物记录` (
  `生物GUID` bigint(20) unsigned NOT NULL COMMENT '野生生物的DBTableGUID',
  `配置ID` int(10) unsigned NOT NULL COMMENT '关联配置表ID',
  `刷新时间` bigint(20) unsigned NOT NULL COMMENT '刷新时间戳（相当于种植时间）',
  `当前成熟度` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '当前成熟度（0-100）',
  PRIMARY KEY (`生物GUID`),
  KEY `idx_config` (`配置ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='野生生物成长记录表（Characters库）';

CREATE TABLE IF NOT EXISTS `符文系统_数据` (
  `ID` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '记录ID',
  `角色GUID` int(10) unsigned NOT NULL COMMENT '角色GUID',
  `物品ID` int(10) unsigned NOT NULL COMMENT '物品模板ID',
  `物品GUID` int(10) unsigned NOT NULL COMMENT '物品实例GUID',
  `插槽数量` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '插槽数量(1-20)',
  `插槽ID` varchar(255) NOT NULL DEFAULT '' COMMENT '插槽ID列表(逗号分隔): 1,2,3,4,5',
  `符文物品ID` varchar(255) NOT NULL DEFAULT '' COMMENT '符文物品ID列表(逗号分隔): 25,26,0,28,0',
  PRIMARY KEY (`ID`),
  UNIQUE KEY `idx_item_guid` (`物品GUID`),
  KEY `idx_character_guid` (`角色GUID`),
  KEY `idx_item_id` (`物品ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='符文系统存储表-装备符文数据';

SET FOREIGN_KEY_CHECKS = 1;
