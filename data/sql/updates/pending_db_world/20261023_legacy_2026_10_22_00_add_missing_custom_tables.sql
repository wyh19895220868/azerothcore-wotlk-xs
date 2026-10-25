-- 335XS: tables that existed only in the live database (not in any source .sql or C++).
-- Generated 2026-10-22 from SHOW CREATE TABLE. UTF-8 (no BOM), utf8mb4 session.
SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

CREATE TABLE IF NOT EXISTS `_1v1竞技场配置表` (
  `序号` int(11) NOT NULL COMMENT '1v1竞技场配置表的序号',
  `配置说明` varchar(255) NOT NULL DEFAULT '' COMMENT '1v1竞技场配置表的配置说明',
  `配置值` varchar(255) NOT NULL DEFAULT '' COMMENT '1v1竞技场配置表的配置值',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '1v1竞技场配置表的备注',
  PRIMARY KEY (`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `___命令表` (
  `命令` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '命令',
  `备注说明` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '备注说明',
  PRIMARY KEY (`命令`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=gbk ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `__全服对战配置表` (
  `序号` int(11) NOT NULL COMMENT '全服对战配置表的序号',
  `配置说明` varchar(255) NOT NULL DEFAULT '' COMMENT '全服对战配置表的配置说明',
  `配置值` varchar(255) NOT NULL DEFAULT '' COMMENT '全服对战配置表的配置值',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '全服对战配置表的备注',
  PRIMARY KEY (`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `__全职业宠物配置表` (
  `序号` int(11) NOT NULL COMMENT '全职业宠物配置表的序号',
  `配置说明` varchar(255) NOT NULL DEFAULT '' COMMENT '全职业宠物配置表的配置说明',
  `配置值` varchar(255) NOT NULL DEFAULT '' COMMENT '全职业宠物配置表的配置值',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '全职业宠物配置表的备注',
  PRIMARY KEY (`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `__公会战配置表` (
  `序号` int(11) NOT NULL COMMENT '公会战配置表的序号',
  `配置说明` varchar(255) NOT NULL DEFAULT '' COMMENT '公会战配置表的配置说明',
  `配置值` varchar(255) NOT NULL DEFAULT '' COMMENT '公会战配置表的配置值',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '公会战配置表的备注',
  PRIMARY KEY (`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `__控制总表` (
  `序号` int(11) NOT NULL COMMENT '序号(不能改动)',
  `功能名称` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '功能的名称',
  `功能值` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL COMMENT '功能的属性值(开关或详细值)',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '控制总表的备注',
  PRIMARY KEY (`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=gbk ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `__模版坐标` (
  `坐标编号` int(11) NOT NULL DEFAULT '0' COMMENT '坐标编号',
  `地图编号` int(11) NOT NULL DEFAULT '0' COMMENT '地图编号',
  `X坐标` float NOT NULL DEFAULT '0' COMMENT 'X坐标',
  `Y坐标` float NOT NULL DEFAULT '0' COMMENT 'Y坐标',
  `Z坐标` float NOT NULL DEFAULT '0' COMMENT 'Z坐标',
  `O坐标` float NOT NULL DEFAULT '0' COMMENT 'O坐标',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '模版坐标的备注',
  PRIMARY KEY (`坐标编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=COMPACT;

CREATE TABLE IF NOT EXISTS `__模版奖励总表` (
  `模版编号` int(11) NOT NULL DEFAULT '0' COMMENT '模版编号',
  `奖励材料1` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料1',
  `奖励数量1` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量1',
  `奖励材料2` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料2',
  `奖励数量2` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量2',
  `奖励材料3` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料3',
  `奖励数量3` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量3',
  `奖励材料4` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料4',
  `奖励数量4` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量4',
  `奖励材料5` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料5',
  `奖励数量5` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量5',
  `奖励材料6` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料6',
  `奖励数量6` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量6',
  `奖励材料7` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料7',
  `奖励数量7` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量7',
  `奖励材料8` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料8',
  `奖励数量8` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量8',
  `设置会员等级` int(11) NOT NULL DEFAULT '0' COMMENT '设置会员等级',
  `奖励积分` bigint(20) NOT NULL DEFAULT '0' COMMENT '奖励积分',
  `奖励魔币` bigint(20) NOT NULL COMMENT '奖励魔币',
  `奖励泡点` bigint(20) NOT NULL DEFAULT '0' COMMENT '奖励泡点',
  `奖励人物经验` int(11) NOT NULL DEFAULT '0' COMMENT '奖励人物经验',
  `奖励荣誉点数` int(11) NOT NULL DEFAULT '0' COMMENT '奖励荣誉点数',
  `奖励竞技场点数` int(11) NOT NULL DEFAULT '0' COMMENT '奖励竞技场点数',
  `奖励金币` int(11) NOT NULL DEFAULT '0' COMMENT '奖励金币',
  `奖励光环组` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `完成成就组` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `完成任务组` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `GM命令组` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '备注',
  PRIMARY KEY (`模版编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `__模版技能光环` (
  `组号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '技能组',
  `技能编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '技能编号',
  `备注` varchar(128) DEFAULT NULL COMMENT '备注',
  PRIMARY KEY (`组号`,`技能编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `__模版玩家属性表` (
  `模版编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '模版编号',
  `固定格挡` float unsigned NOT NULL DEFAULT '0' COMMENT '固定格挡',
  `格挡上限` float unsigned NOT NULL DEFAULT '35' COMMENT '格挡上限',
  `格挡倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '格挡倍率',
  `固定招架` float unsigned NOT NULL DEFAULT '0' COMMENT '固定招架',
  `招架上限` float unsigned NOT NULL DEFAULT '35' COMMENT '招架上限',
  `招架倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '招架倍率',
  `固定闪避` float unsigned NOT NULL DEFAULT '0' COMMENT '固定闪避',
  `闪避上限` float unsigned NOT NULL DEFAULT '35' COMMENT '闪避上限',
  `闪避倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '闪避倍率',
  `固定暴击` float unsigned NOT NULL DEFAULT '0' COMMENT '固定暴击',
  `暴击上限` float unsigned NOT NULL DEFAULT '35' COMMENT '暴击上限',
  `暴击倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '暴击倍率',
  `固定韧性` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '固定韧性',
  `韧性上限` int(11) unsigned NOT NULL DEFAULT '4000' COMMENT '韧性上限',
  `韧性倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '韧性倍率',
  `固定护甲` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定护甲',
  `护甲上限` bigint(20) NOT NULL DEFAULT '0' COMMENT '护甲上限',
  `护甲倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '护甲倍率',
  `固定精准` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '固定精准',
  `精准上限` int(11) unsigned NOT NULL DEFAULT '200' COMMENT '精准上限',
  `精准倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '精准倍率',
  `固定血量` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定血量',
  `血量下限` bigint(20) NOT NULL DEFAULT '0' COMMENT '血量下限',
  `血量上限` bigint(20) NOT NULL DEFAULT '0' COMMENT '血量上限',
  `血量倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '血量倍率',
  `固定急速` float unsigned NOT NULL DEFAULT '0' COMMENT '固定急速',
  `急速上限` float unsigned NOT NULL DEFAULT '35' COMMENT '急速上限',
  `急速倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '急速倍率',
  `固定伤害` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定伤害',
  `伤害上限` bigint(20) NOT NULL DEFAULT '0' COMMENT '伤害上限',
  `PVE物理伤害倍率` float unsigned NOT NULL DEFAULT '1' COMMENT 'PVE物理伤害倍率',
  `PVE技能伤害倍率` float unsigned NOT NULL DEFAULT '1' COMMENT 'PVE技能伤害倍率',
  `PVP物理伤害倍率` float unsigned NOT NULL DEFAULT '1' COMMENT 'PVP物理伤害倍率',
  `PVP技能伤害倍率` float unsigned NOT NULL DEFAULT '1' COMMENT 'PVP技能伤害倍率',
  `PVE免伤率` float unsigned NOT NULL DEFAULT '0' COMMENT 'PVE免伤率',
  `PVP免伤率` float unsigned NOT NULL DEFAULT '0' COMMENT 'PVP免伤率',
  `固定治疗` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定治疗',
  `治疗上限` bigint(20) NOT NULL DEFAULT '0' COMMENT '治疗上限',
  `治疗倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '治疗倍率',
  `固定蓝量` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定蓝量',
  `蓝量下限` bigint(20) NOT NULL DEFAULT '0' COMMENT '蓝量下限',
  `蓝量上限` bigint(20) NOT NULL DEFAULT '0' COMMENT '蓝量上限',
  `蓝量倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '蓝量倍率',
  `固定力量` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定力量',
  `力量下限` bigint(20) NOT NULL DEFAULT '0' COMMENT '力量下限',
  `固定敏捷` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定敏捷',
  `敏捷下限` bigint(20) NOT NULL DEFAULT '0' COMMENT '敏捷下限',
  `固定耐力` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定耐力',
  `耐力下限` bigint(20) NOT NULL DEFAULT '0' COMMENT '耐力下限',
  `固定智力` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定智力',
  `智力下限` bigint(20) NOT NULL DEFAULT '0' COMMENT '智力下限',
  `固定精神` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定精神',
  `精神下限` bigint(20) NOT NULL DEFAULT '0' COMMENT '精神下限',
  `固定武器最小伤害` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定武器最小伤害',
  `固定武器最大伤害` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定武器最大伤害',
  `固定攻击强度` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定攻击强度',
  `固定法术强度` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定法术强度',
  `固定治疗效果` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定治疗效果',
  `智力加法术强度倍率` float unsigned NOT NULL DEFAULT '0' COMMENT '0表示不加 0.1表示加10% 以此类推',
  `智力加治疗效果倍率` float unsigned NOT NULL DEFAULT '0' COMMENT '0表示不加 0.1表示加10% 以此类推',
  `精神加法术强度倍率` float unsigned NOT NULL DEFAULT '0' COMMENT '0表示不加 0.1表示加10% 以此类推',
  `精神加治疗效果倍率` float unsigned NOT NULL DEFAULT '0' COMMENT '0表示不加 0.1表示加10% 以此类推',
  `PVEDOT伤害倍率` float NOT NULL DEFAULT '1' COMMENT 'PVEDOT伤害倍率',
  `PVPDOT伤害倍率` float NOT NULL DEFAULT '1' COMMENT 'PVPDOT伤害倍率',
  PRIMARY KEY (`模版编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `__模版生物属性表` (
  `模版编号` int(11) NOT NULL DEFAULT '0',
  `备注` varchar(255) NOT NULL DEFAULT '',
  `生物等级` int(11) NOT NULL DEFAULT '80',
  `生物等级范围最小` int(11) NOT NULL DEFAULT '-1' COMMENT '-1=使用生物等级字段',
  `生物等级范围最大` int(11) NOT NULL DEFAULT '-1' COMMENT '-1=使用生物等级字段',
  `血条数量` int(11) NOT NULL DEFAULT '1' COMMENT '1=原始 以此类推',
  `固定生命值` decimal(65,0) NOT NULL DEFAULT '0',
  `血量倍率` double unsigned NOT NULL DEFAULT '1',
  `法力倍率` double unsigned NOT NULL DEFAULT '1',
  `法力值` decimal(65,0) unsigned NOT NULL DEFAULT '0',
  `受到的伤害固定值` decimal(65,0) NOT NULL DEFAULT '0',
  `伤害减免百分比` bigint(20) NOT NULL DEFAULT '0',
  `切割比例` float NOT NULL DEFAULT '0',
  `造成的伤害固定值` decimal(65,0) NOT NULL DEFAULT '0',
  `真实伤害值` decimal(65,0) unsigned NOT NULL DEFAULT '0',
  `物理伤害Min` decimal(65,0) NOT NULL DEFAULT '0',
  `物理伤害Max` decimal(65,0) NOT NULL DEFAULT '0',
  `最小伤害倍率` double unsigned NOT NULL DEFAULT '1',
  `最大伤害倍率` double unsigned NOT NULL DEFAULT '1',
  `技能法术伤害Min` decimal(65,0) NOT NULL DEFAULT '0',
  `技能法术伤害Max` decimal(65,0) NOT NULL DEFAULT '0',
  `技能法术比例` float NOT NULL DEFAULT '1',
  `攻击强度倍率` double unsigned NOT NULL DEFAULT '1',
  `攻击强度值` decimal(65,0) unsigned NOT NULL DEFAULT '0',
  `法术强度倍率` double unsigned NOT NULL DEFAULT '1',
  `法术强度值` decimal(65,0) unsigned NOT NULL DEFAULT '0',
  `护甲` decimal(65,0) NOT NULL DEFAULT '0',
  `护甲倍率` double unsigned NOT NULL DEFAULT '1',
  `神圣抗性` int(11) NOT NULL DEFAULT '0',
  `火焰抗性` int(11) NOT NULL DEFAULT '0',
  `火抗倍率` double unsigned NOT NULL DEFAULT '1',
  `自然抗性` int(11) NOT NULL DEFAULT '0',
  `自然抗倍率` double unsigned NOT NULL DEFAULT '1',
  `冰霜抗性` int(11) NOT NULL DEFAULT '0',
  `冰霜抗倍率` double unsigned NOT NULL DEFAULT '1',
  `暗影抗性` int(11) NOT NULL DEFAULT '0',
  `暗影抗倍率` double unsigned NOT NULL DEFAULT '1',
  `奥术抗性` int(11) NOT NULL DEFAULT '0',
  `奥术抗倍率` double unsigned NOT NULL DEFAULT '1',
  `攻击速度倍率` float(10,2) unsigned NOT NULL DEFAULT '1.00',
  `攻击速度值` int(10) unsigned NOT NULL DEFAULT '0',
  `施法速度倍率` float(10,2) unsigned NOT NULL DEFAULT '1.00',
  `施法速度值` int(10) unsigned NOT NULL DEFAULT '0',
  `移动速度倍率` float(10,2) unsigned NOT NULL DEFAULT '1.00',
  `移动速度值` int(10) unsigned NOT NULL DEFAULT '0',
  `命中率加成` float(10,2) NOT NULL DEFAULT '0.00',
  `护甲穿透值` int(10) unsigned NOT NULL DEFAULT '0',
  `护甲穿透百分比` float(10,2) unsigned NOT NULL DEFAULT '0.00',
  `格挡穿透值` int(10) unsigned NOT NULL DEFAULT '0',
  `格挡穿透百分比` float(10,2) unsigned NOT NULL DEFAULT '0.00',
  `生物光环组` int(11) NOT NULL DEFAULT '0' COMMENT '__模版技能光环',
  `生物光环几率` float NOT NULL DEFAULT '100',
  `生物光环数量` int(11) NOT NULL DEFAULT '0',
  `生物固定光环` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `官方掉落` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `额外掉落编号` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `超级掉落组号` int(11) NOT NULL DEFAULT '0' COMMENT '对应 __生物超级掉落组表',
  `超级掉落几率` float NOT NULL DEFAULT '100' COMMENT '这个几率决定是否掉落',
  `超级掉落抽取数量Min` int(11) NOT NULL DEFAULT '1' COMMENT '组里面按组那边的几率出来的多少个物品',
  `超级掉落抽取数量Max` int(11) NOT NULL DEFAULT '1' COMMENT '组里面按组那边的几率出来的多少个物品',
  `装备属性倍率` float(10,2) unsigned NOT NULL DEFAULT '1.00',
  `装备属性值` int(10) unsigned NOT NULL DEFAULT '0',
  `装备属性倍率模式` tinyint(4) NOT NULL DEFAULT '0',
  `鉴定组` int(10) unsigned NOT NULL DEFAULT '0',
  `鉴定品质` tinyint(3) unsigned NOT NULL DEFAULT '0',
  `鉴定物品等级` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`模版编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `__模版需求总表` (
  `模版编号` int(11) NOT NULL DEFAULT '0' COMMENT '模版编号',
  `消耗材料1` int(11) NOT NULL DEFAULT '0' COMMENT '消耗材料1',
  `消耗数量1` int(11) NOT NULL DEFAULT '0' COMMENT '消耗数量1',
  `消耗材料2` int(11) NOT NULL DEFAULT '0' COMMENT '消耗材料2',
  `消耗数量2` int(11) NOT NULL DEFAULT '0' COMMENT '消耗数量2',
  `消耗材料3` int(11) NOT NULL DEFAULT '0' COMMENT '消耗材料3',
  `消耗数量3` int(11) NOT NULL DEFAULT '0' COMMENT '消耗数量3',
  `消耗材料4` int(11) NOT NULL DEFAULT '0' COMMENT '消耗材料4',
  `消耗数量4` int(11) NOT NULL DEFAULT '0' COMMENT '消耗数量4',
  `消耗材料5` int(11) NOT NULL DEFAULT '0' COMMENT '消耗材料5',
  `消耗数量5` int(11) NOT NULL DEFAULT '0' COMMENT '消耗数量5',
  `消耗材料6` int(11) NOT NULL DEFAULT '0' COMMENT '消耗材料6',
  `消耗数量6` int(11) NOT NULL DEFAULT '0' COMMENT '消耗数量6',
  `消耗材料7` int(11) NOT NULL DEFAULT '0' COMMENT '消耗材料7',
  `消耗数量7` int(11) NOT NULL DEFAULT '0' COMMENT '消耗数量7',
  `消耗材料8` int(11) NOT NULL DEFAULT '0' COMMENT '消耗材料8',
  `消耗数量8` int(11) NOT NULL DEFAULT '0' COMMENT '消耗数量8',
  `需要会员等级` int(11) NOT NULL DEFAULT '0' COMMENT '需要会员等级',
  `消耗积分` bigint(20) NOT NULL DEFAULT '0' COMMENT '消耗积分',
  `达到累计积分` bigint(20) NOT NULL DEFAULT '0' COMMENT '达到累计积分',
  `需要人物等级` int(11) NOT NULL DEFAULT '0' COMMENT '需要人物等级',
  `消耗荣誉点数` int(11) NOT NULL DEFAULT '0' COMMENT '消耗荣誉点数',
  `消耗竞技场点数` int(11) NOT NULL DEFAULT '0' COMMENT '消耗竞技场点数',
  `消耗金币` int(11) NOT NULL DEFAULT '0' COMMENT '消耗金币',
  `消耗魔币` varchar(255) DEFAULT NULL COMMENT '消耗魔币',
  `达到累计魔币` bigint(20) DEFAULT NULL COMMENT '达到累计魔币',
  `消耗泡点` bigint(20) NOT NULL DEFAULT '0' COMMENT '消耗泡点',
  `需要职业掩码` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制 支持掩码',
  `需要种族掩码` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制 支持掩码',
  `需要阵营` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制 1=联盟 2=部落',
  `需要声望编号` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制 对应Item表的RequiredReputationFaction具体是对应DBC',
  `需要声望等级` int(11) NOT NULL DEFAULT '0' COMMENT '0=仇恨 1=敌对 2=冷淡 3=中立\r\
4=友善 5=尊敬 6=崇敬 7=崇拜',
  `需要位于地图` int(11) NOT NULL DEFAULT '-1' COMMENT '需要位于地图',
  `需要位于区域` int(11) NOT NULL DEFAULT '0' COMMENT '需要位于区域',
  `需要达到成就点数` int(11) NOT NULL DEFAULT '0' COMMENT '需要达到成就点数',
  `需要军衔等级` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制',
  `需要斗气等级` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制 支持掩码',
  `需要拥有光环组` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `需要通过成就组` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `需要完成任务组` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `需要主属性类型掩码` int(11) NOT NULL DEFAULT '0' COMMENT '1=力量2=敏捷4=耐力8=智力16=精神',
  `需要主属性达到值` int(11) NOT NULL DEFAULT '0' COMMENT '需要主属性达到值',
  `需要学会技能组` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `需要赞助等级` int(11) NOT NULL DEFAULT '0' COMMENT '需要赞助等级',
  `需要全部装备栏等级` int(11) NOT NULL DEFAULT '0' COMMENT '需要全部装备栏等级',
  `需要转生等级` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '需要转生等级',
  PRIMARY KEY (`模版编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `__生物超级掉落组表` (
  `组号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '技能组',
  `物品编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '物品编号',
  `几率` float(10,0) unsigned NOT NULL DEFAULT '0' COMMENT '生物超级掉落组表的几率',
  `超级模版组号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '超级模版组号',
  `备注` varchar(128) NOT NULL DEFAULT '' COMMENT '备注',
  PRIMARY KEY (`组号`,`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `__聊天敏感词列表` (
  `ID` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT 'ID',
  `敏感词` varchar(100) NOT NULL COMMENT '需要屏蔽的敏感词',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='聊天敏感词列表';

CREATE TABLE IF NOT EXISTS `__转生配置表` (
  `转生等级` int(10) unsigned NOT NULL COMMENT '转生等级',
  `需求模版ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联__模版需求总表的模版编号',
  `奖励模版ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联__模版奖励总表的模版编号',
  `对话文本` varchar(255) NOT NULL DEFAULT '' COMMENT 'NPC对话显示文本',
  PRIMARY KEY (`转生等级`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='转生系统配置表';

CREATE TABLE IF NOT EXISTS `_id幻化表` (
  `物品编号` int(11) unsigned NOT NULL COMMENT '物品ID',
  `需求模板` int(11) NOT NULL DEFAULT '0' COMMENT '需求模板',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '备忘',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_一键回收组` (
  `序号` int(11) NOT NULL COMMENT '一键回收组的序号',
  `菜单文本` varchar(255) NOT NULL DEFAULT '' COMMENT '一键回收组的菜单文本',
  PRIMARY KEY (`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_一键回收表` (
  `物品编号` int(11) NOT NULL COMMENT '物品编号',
  `奖励材料1` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料1',
  `奖励数量1` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量1',
  `奖励材料2` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料2',
  `奖励数量2` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量2',
  `奖励材料3` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料3',
  `奖励数量3` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量3',
  `奖励材料4` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料4',
  `奖励数量4` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量4',
  `奖励材料5` int(11) NOT NULL DEFAULT '0' COMMENT '奖励材料5',
  `奖励数量5` int(11) NOT NULL DEFAULT '0' COMMENT '奖励数量5',
  `积分` int(11) NOT NULL DEFAULT '0' COMMENT '积分',
  `回收组序号` int(11) NOT NULL DEFAULT '0' COMMENT '回收组序号',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_事件` (
  `事件ID` int(11) NOT NULL DEFAULT '70' COMMENT '事件ID',
  `事件名称` varchar(128) DEFAULT NULL COMMENT '事件名称',
  `弹窗提示文本` text COMMENT '弹窗提示文本',
  `菜单提示文本` text COMMENT '菜单提示文本',
  `归属地图ID` int(11) DEFAULT '-1' COMMENT '归属地图ID',
  `区域ID` int(11) DEFAULT '0' COMMENT '.gps命令显示的zone,事件类型为对线时需要填写',
  `事件类型` enum('战场 - 战歌','战场 - 阿拉希','战场 - 风暴之眼','战场 - 远古海滩','战场 - 奥山','战场 - 征服之岛','仅传送','对线','公会') NOT NULL DEFAULT '仅传送' COMMENT '事件类型',
  `组队类型` enum('对线 - 全部阵营1','对线 - 联盟阵营1BL阵营2','对线 - 联盟阵营2BL阵营1','对线 - 联盟部落随机','攻守 - 玩家随机') NOT NULL DEFAULT '对线 - 全部阵营1' COMMENT '事件类型为对线时有效',
  `重新加入类型` enum('重新加入 - 禁止','重新加入- 之前队伍','重新加入 - 系统分配') NOT NULL COMMENT '事件类型为对线时有效',
  `胜利奖励模板ID` int(11) NOT NULL DEFAULT '0' COMMENT '胜利奖励模板ID',
  `失败奖励模板ID` int(11) NOT NULL DEFAULT '0' COMMENT '失败奖励模板ID',
  `获得奖励最小伤害值` bigint(20) NOT NULL DEFAULT '0' COMMENT '获得奖励最小伤害值',
  `获得奖励最小治疗值` bigint(20) NOT NULL DEFAULT '0' COMMENT '获得奖励最小治疗值',
  `获得奖励最小击杀数` int(11) NOT NULL DEFAULT '0' COMMENT '四者满足其一可以获得奖励',
  `获得奖励最小被杀数` int(11) NOT NULL DEFAULT '0' COMMENT '四者满足其一可以获得奖励',
  PRIMARY KEY (`事件ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_事件_世界状态配置` (
  `事件ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '事件状态是指屏幕上方显示的资源',
  `世界状态ID` int(11) unsigned NOT NULL DEFAULT '0' COMMENT 'WorldStateUI.dbc第10列数字部分',
  `世界状态初始值` int(11) NOT NULL DEFAULT '0' COMMENT '世界状态初始值',
  `更新世界状态需要击杀生物或摧毁物体ID1` int(11) NOT NULL DEFAULT '0' COMMENT 'creature或gamobject表中guid,正数为生物，负数为物体',
  `更新世界状态增减值1` int(11) NOT NULL DEFAULT '0' COMMENT '击杀生物或摧毁物体改变的世界状态数值，正数为增加，负数为减少',
  `更新世界状态需要击杀生物或摧毁物体ID2` int(11) NOT NULL DEFAULT '0' COMMENT 'creature或gamobject表中guid,正数为生物，负数为物体',
  `更新世界状态增减值2` int(11) NOT NULL DEFAULT '0' COMMENT '击杀生物或摧毁物体改变的世界状态数值，正数为增加，负数为减少',
  `更新世界状态需要击杀生物或摧毁物体ID3` int(11) NOT NULL DEFAULT '0' COMMENT 'creature或gamobject表中guid,正数为生物，负数为物体',
  `更新世界状态增减值3` int(11) NOT NULL DEFAULT '0' COMMENT '击杀生物或摧毁物体改变的世界状态数值，正数为增加，负数为减少',
  `世界状态达到该值时提示` int(11) NOT NULL DEFAULT '0' COMMENT '世界状态达到该值时提示',
  `提示文本` varchar(128) DEFAULT NULL COMMENT '提示文本',
  PRIMARY KEY (`世界状态ID`,`事件ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_事件_世界状态队伍配置` (
  `事件ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '需配合[_事件_世界状态配置]使用',
  `世界状态ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'WorldStateUI.dbc第10列数字部分',
  `队伍ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '队伍ID',
  `击杀或被杀` tinyint(1) unsigned NOT NULL DEFAULT '1' COMMENT '1击杀敌对阵营玩家，0被敌对阵营玩家击杀时更新世界状态',
  `更新世界状态增减值` int(11) NOT NULL DEFAULT '0' COMMENT '击杀或被杀时改变的世界状态数值，正数为增加，负数为减少',
  `世界状态达到该值时提示` int(11) NOT NULL DEFAULT '0' COMMENT '世界状态达到该值时提示',
  `提示文本` varchar(128) DEFAULT NULL COMMENT '提示文本',
  PRIMARY KEY (`事件ID`,`世界状态ID`,`队伍ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_事件_生物或物体刷新配置` (
  `事件ID` int(10) unsigned DEFAULT '0' COMMENT '事件ID',
  `刷新生物或物体ID` int(11) DEFAULT '0' COMMENT '刷新生物或物体的entry,正数为生物，负数为物体',
  `阶段ID` int(10) unsigned DEFAULT '0' COMMENT '刷新生物或物体的阶段ID',
  `刷新时间` int(10) unsigned DEFAULT '1' COMMENT '刷新生物或物体的时间（单位：秒），从阶段开始时开始计算，比如100秒，则当前阶段开始100秒后，刷新生物或物体',
  `刷新位置坐标ID` int(10) unsigned DEFAULT '0' COMMENT '刷新生物或物体的坐标，对应_postion表中posId',
  `移动到坐标ID` int(10) unsigned DEFAULT '0' COMMENT '刷新后自动移动到的坐标，对应_postion表中posId 0表示不生效',
  `持续时间` int(10) unsigned DEFAULT '0' COMMENT '刷新生物或物体持续的时间，单位：秒。为0则生物不消失',
  `提示时间1` int(10) unsigned DEFAULT '0' COMMENT '即将刷新生物或物体的提示时间（单位：秒），比如90秒，从阶段开始时开始计算90秒后给出提示',
  `提示文本1` varchar(128) DEFAULT NULL COMMENT '提示文本1',
  `提示时间2` int(10) unsigned DEFAULT '0' COMMENT '即将刷新生物或物体的提示时间（单位：秒），比如90秒，从阶段开始时开始计算90秒后给出提示',
  `提示文本2` varchar(128) DEFAULT NULL COMMENT '提示文本2',
  `提示时间3` int(10) unsigned DEFAULT '0' COMMENT '即将刷新生物或物体的提示时间（单位：秒），比如90秒，从阶段开始时开始计算90秒后给出提示',
  `提示文本3` varchar(128) DEFAULT NULL COMMENT '提示文本3'
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_事件_生物或物体激活条件配置` (
  `事件ID` int(11) NOT NULL DEFAULT '0' COMMENT '事件ID',
  `生物或物体GUID` int(11) NOT NULL DEFAULT '0' COMMENT 'creature或gamobject表中guid,正数为生物，负数为物体',
  `生物或物体激活后FLAG` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '一般开始时creature_template或gameobject_template中设置为不可选或不能攻击，激活后设置为0.',
  `激活时提示文本` varchar(128) DEFAULT NULL COMMENT '激活时提示文本',
  `需击杀生物或摧毁物体ID1` int(11) NOT NULL DEFAULT '0' COMMENT '正数为生物，负数为物体',
  `数量1` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '数量1',
  `需击杀生物或摧毁物体ID2` int(11) NOT NULL DEFAULT '0' COMMENT '正数为生物，负数为物体',
  `数量2` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '数量2',
  `需击杀生物或摧毁物体ID3` int(11) NOT NULL DEFAULT '0' COMMENT '正数为生物，负数为物体',
  `数量3` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '数量3',
  `需击杀生物或摧毁物体ID4` int(11) NOT NULL DEFAULT '0' COMMENT '正数为生物，负数为物体',
  `数量4` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '数量4',
  `需击杀生物或摧毁物体ID5` int(11) NOT NULL DEFAULT '0' COMMENT '正数为生物，负数为物体',
  `数量5` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '数量5',
  PRIMARY KEY (`事件ID`,`生物或物体GUID`,`生物或物体激活后FLAG`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_事件_生物或物体激活条件队伍配置` (
  `事件ID` int(11) NOT NULL DEFAULT '0' COMMENT '该表需配合[_事件_生物或物体激活条件配置]使用',
  `生物或物体GUID` int(11) NOT NULL DEFAULT '0' COMMENT 'creature或gamobject表中guid,正数为生物，负数为物体',
  `队伍ID` int(11) NOT NULL DEFAULT '0' COMMENT '队伍ID',
  `击杀玩家数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '击杀玩家数',
  `玩家被杀数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '玩家被杀数',
  PRIMARY KEY (`事件ID`,`生物或物体GUID`,`队伍ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_事件_结束条件配置` (
  `备注` varchar(128) DEFAULT NULL COMMENT '事件_结束条件配置的备注',
  `事件ID` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '事件ID',
  `队伍ID` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '队伍ID',
  `结束类型` enum('击杀生物 - guid','摧毁物体 - guid','击杀生物 - entry','击杀玩家 - 数量') NOT NULL DEFAULT '击杀生物 - guid' COMMENT '结束类型',
  `参数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '事件_结束条件配置的参数',
  `变量` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '事件_结束条件配置的变量',
  `权重值` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '权重值',
  PRIMARY KEY (`事件ID`,`队伍ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_事件_队伍配置` (
  `备注` varchar(255) DEFAULT NULL COMMENT '事件_队伍配置的备注',
  `事件ID` int(11) unsigned NOT NULL COMMENT '事件ID',
  `队伍ID` int(11) unsigned NOT NULL COMMENT '事件类型为仅传送（0为联盟 1为部落）对线（双方分别为1，2），自定义阵营（_faction表自定义阵营ID）',
  `队伍名称` varchar(128) DEFAULT NULL COMMENT '队伍名称',
  `事件开始传送坐标ID` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '_postion',
  `事件结束传送坐标ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '_postion',
  PRIMARY KEY (`事件ID`,`队伍ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_事件_阶段生物或物体配置` (
  `事件ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '需配合[_事件_阶段队伍配置]使用',
  `阶段ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '阶段ID',
  `阶段结束需击杀生物或摧毁物体GUID` int(11) NOT NULL DEFAULT '0' COMMENT 'creature或gamobject表中guid,正数为生物，负数为物体',
  PRIMARY KEY (`事件ID`,`阶段ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_事件_阶段队伍配置` (
  `事件ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '事件ID',
  `阶段ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '阶段ID',
  `队伍ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '队伍ID',
  `墓地灵魂医者ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'creature_template entry 自动召唤',
  `墓地坐标ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '死亡后传送坐标_postion',
  `墓地保护区范围` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '单位码，敌方玩家进入保护区将被传送',
  `进入墓地保护区提示文本` varchar(128) DEFAULT NULL COMMENT '进入墓地保护区提示文本',
  `阶段结束需击杀玩家数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '阶段结束需击杀玩家数',
  `阶段结束需玩家被杀数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '阶段结束需玩家被杀数',
  `阶段结束集体传送坐标ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '阶段结束集体传送坐标ID',
  `阶段结束提示文本` varchar(128) DEFAULT NULL COMMENT '阶段结束提示文本',
  PRIMARY KEY (`事件ID`,`阶段ID`,`队伍ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_会员等级配置表` (
  `会员等级` int(11) NOT NULL COMMENT '会员等级',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `奖励模版` int(11) NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `附加额外属性比例` float NOT NULL DEFAULT '0' COMMENT '附加额外属性比例',
  `增加天赋点数` int(11) NOT NULL DEFAULT '0' COMMENT '奖励的天赋点',
  `附加掉率` float NOT NULL DEFAULT '0' COMMENT '附加掉率',
  `爆率比例` float NOT NULL DEFAULT '0' COMMENT '爆率比例',
  `升级成功率` float NOT NULL DEFAULT '0' COMMENT '加法 暂时仅支持合成升级',
  `附加的光环` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开 附加的光环设计成永久无法移除的',
  `会员玩家模版` int(11) NOT NULL DEFAULT '0' COMMENT '不能重载的',
  `图标字符` varchar(255) NOT NULL DEFAULT '' COMMENT '这是不能重载的',
  `菜单描述` varchar(255) NOT NULL DEFAULT '会员奖励' COMMENT '这是不能重载的',
  `名字前缀` varchar(255) NOT NULL DEFAULT '' COMMENT '这是不能重载的',
  PRIMARY KEY (`会员等级`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_传奇十二生肖激活表` (
  `阶级` int(11) NOT NULL COMMENT '阶级',
  `生肖物品1` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品1',
  `生肖物品2` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品2',
  `生肖物品3` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品3',
  `生肖物品4` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品4',
  `生肖物品5` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品5',
  `生肖物品6` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品6',
  `生肖物品7` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品7',
  `生肖物品8` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品8',
  `生肖物品9` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品9',
  `生肖物品10` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品10',
  `生肖物品11` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品11',
  `生肖物品12` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品12',
  `激活光环` int(11) NOT NULL DEFAULT '0' COMMENT '激活光环',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '传奇十二生肖激活表的备注',
  PRIMARY KEY (`阶级`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_传奇十二生肖配置表` (
  `生肖物品编号` int(11) NOT NULL DEFAULT '0' COMMENT '生肖物品编号',
  `自带光环` int(11) NOT NULL DEFAULT '0' COMMENT '自带光环',
  `归属位置` int(11) NOT NULL DEFAULT '0' COMMENT '归属位置',
  `放入需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '放入需求模版',
  `取出需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '取出需求模版',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '传奇十二生肖配置表的备注',
  PRIMARY KEY (`生肖物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_假人移动坐标表` (
  `序号` int(11) NOT NULL DEFAULT '0' COMMENT '假人移动坐标表的序号',
  `坐标编号` int(11) NOT NULL DEFAULT '0' COMMENT '坐标编号',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '假人移动坐标表的备注',
  PRIMARY KEY (`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_光环控制光环关联层数表` (
  `主光环编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '主光环编号',
  `关联光环编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联光环编号',
  `关联光环层数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联光环层数',
  `触发技能组` varchar(128) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `触发技能后移除层数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '触发技能后移除层数',
  `备注` varchar(128) DEFAULT '' COMMENT '光环控制光环关联层数表的备注',
  PRIMARY KEY (`主光环编号`,`关联光环编号`,`关联光环层数`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_全局随机附魔限制表` (
  `附魔编号` int(11) unsigned NOT NULL COMMENT '附魔编号',
  `限制数量` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制',
  PRIMARY KEY (`附魔编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_军衔` (
  `军衔编号` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '军衔编号',
  `升级需求模版` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '升级需求模版',
  `升级奖励模版` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '升级奖励模版',
  `头衔` varchar(255) DEFAULT '' COMMENT '头衔',
  PRIMARY KEY (`军衔编号`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=31 DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_副本天梯怪物组` (
  `怪物组` int(11) NOT NULL DEFAULT '1' COMMENT '怪物组',
  `生物编号` int(11) NOT NULL DEFAULT '0' COMMENT '怪物编号',
  `数量` int(11) NOT NULL DEFAULT '1' COMMENT '副本天梯怪物组的数量',
  `坐标X` float NOT NULL DEFAULT '0' COMMENT '坐标X',
  `坐标Y` float NOT NULL DEFAULT '0' COMMENT '坐标Y',
  `坐标Z` float NOT NULL DEFAULT '0' COMMENT '坐标Z',
  `坐标O` float NOT NULL DEFAULT '0' COMMENT '坐标O',
  PRIMARY KEY (`怪物组`,`生物编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_副本天梯配置` (
  `层数` int(11) NOT NULL COMMENT '控制总表 配置NPC编号',
  `召唤生物组` int(11) NOT NULL DEFAULT '0' COMMENT '对应 _副本闯关怪物组',
  `奖励物品礼包` int(11) NOT NULL DEFAULT '0' COMMENT '做个礼包方便插件显示',
  `菜单文本` varchar(255) DEFAULT '' COMMENT '副本天梯配置的菜单文本',
  `奖励模版` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  PRIMARY KEY (`层数`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_副本重置` (
  `序号` int(11) NOT NULL COMMENT '副本重置的序号',
  `菜单号` int(11) NOT NULL COMMENT '菜单号',
  `菜单文本` varchar(255) NOT NULL DEFAULT '' COMMENT '副本重置的菜单文本',
  `副本地图编号` int(11) NOT NULL COMMENT '控制总表 配置NPC编号',
  `副本难度` int(11) NOT NULL DEFAULT '-1' COMMENT '0=10人普通 1=5人普通或25人普通 2=5人英雄或10人英雄 3=25人英雄',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `重置次数` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '副本重置的备注',
  PRIMARY KEY (`序号`,`菜单号`,`副本地图编号`,`副本难度`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_副本闯关` (
  `关卡序号` int(11) NOT NULL COMMENT '全局唯一关卡编号，也是角色进度编号',
  `章节编号` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '对应_副本闯关章节',
  `章节内序号` tinyint(3) unsigned NOT NULL DEFAULT '1' COMMENT '每章固定1-5，第5关为首领关',
  `召唤生物组` int(11) NOT NULL COMMENT '对应 _副本闯关怪物组',
  `通关召唤物体编号` int(11) NOT NULL COMMENT '关卡奖励箱等普通物体，0=不召唤',
  `菜单文本` varchar(255) DEFAULT '' COMMENT '副本闯关的菜单文本',
  `关卡坐标编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '对应__模版坐标，必须位于章节地图',
  `通关传送门编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0=使用章节默认传送门；ScriptName必须为StageChallengePortal',
  `首领关` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '章节第5关必须为1，其余关必须为0',
  `通关奖励模版` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '角色首次推进该关时发放',
  PRIMARY KEY (`关卡序号`) USING BTREE,
  UNIQUE KEY `uk_章节关卡` (`章节编号`,`章节内序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_副本闯关怪物组` (
  `怪物组` int(11) NOT NULL DEFAULT '1' COMMENT '怪物组',
  `生物编号` int(11) NOT NULL DEFAULT '0' COMMENT '怪物编号',
  `数量` int(11) NOT NULL DEFAULT '1' COMMENT '副本闯关怪物组的数量',
  `坐标X` float NOT NULL DEFAULT '0' COMMENT '坐标X',
  `坐标Y` float NOT NULL DEFAULT '0' COMMENT '坐标Y',
  `坐标Z` float NOT NULL DEFAULT '0' COMMENT '坐标Z',
  `坐标O` float NOT NULL DEFAULT '0' COMMENT '坐标O',
  PRIMARY KEY (`怪物组`,`生物编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_副本闯关章节` (
  `章节编号` int(10) unsigned NOT NULL COMMENT '章节编号',
  `章节名称` varchar(64) NOT NULL DEFAULT '' COMMENT '章节名称',
  `地图编号` int(10) unsigned NOT NULL COMMENT '必须为副本地图；同一章节的5关使用此地图实例',
  `入口坐标编号` int(10) unsigned NOT NULL COMMENT '对应__模版坐标，地图必须等于地图编号',
  `出口坐标编号` int(10) unsigned NOT NULL COMMENT '对应__模版坐标，可指向副本外地图',
  `章节奖励模版` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '章节奖励模版',
  `下一章节编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0=最后章节',
  `默认传送门物体编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'gameobject_template.ScriptName必须为StageChallengePortal',
  `启用` tinyint(3) unsigned NOT NULL DEFAULT '1' COMMENT '启用',
  PRIMARY KEY (`章节编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_功能传送表` (
  `对象编号` int(11) NOT NULL DEFAULT '0' COMMENT '物品/生物/物体 编号',
  `序号` int(11) NOT NULL DEFAULT '0' COMMENT '菜单',
  `菜单编号` int(11) NOT NULL DEFAULT '0' COMMENT '默认0 = 主菜单 大于0 = 子菜单 负数是内置',
  `标题` varchar(255) NOT NULL DEFAULT '' COMMENT '菜单的描述',
  `菜单项` int(11) NOT NULL DEFAULT '0' COMMENT '执行的菜单项',
  `阵营` int(11) NOT NULL DEFAULT '0' COMMENT '0=不区分 1=联盟 2=部落',
  `显示需求模版` int(10) unsigned DEFAULT '0' COMMENT '显示需求模版ID（只做判断不扣除，满足才显示菜单）',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `坐标编号` int(11) NOT NULL DEFAULT '1' COMMENT '对应模板坐标',
  `GM命令组` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开多个',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '功能传送表的备注',
  PRIMARY KEY (`序号`) USING BTREE,
  KEY `inxpx` (`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_动作触发功能表` (
  `序号` int(11) NOT NULL DEFAULT '0' COMMENT '没有实际意义 仅作为主键',
  `唯一` enum('全服唯一','账号唯一','角色唯一','不限制') CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL DEFAULT '不限制' COMMENT '唯一',
  `动作` enum('说话','收到密语','击杀生物','击杀玩家','完成任务','完成成就','被玩家击杀','被生物击杀','拾取指定物品','等级变化','切换地图','切换区域','离开地图','切换地区','离开区域','释放技能','登陆','登出','第一次登陆','VIP等级变化','复活','装备物品','金币变化','获得经验','获得声望','学习技能','遗忘技能','决斗开始','决斗中止','决斗胜利','决斗失败','释放尸体','学习天赋','进入战斗','结束战斗','重置天赋','获得光环','失去光环','模型改变','表情','采矿','采药','钓鱼','玩家移动','生物移动','使用指定物品','接受任务','放弃任务','玩家死亡','任务奖励物品') CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '说话',
  `内容` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL DEFAULT '' COMMENT '多个的时候可以使用#隔开',
  `触发几率` float NOT NULL DEFAULT '100' COMMENT '触发几率',
  `触发间隔` int(11) NOT NULL DEFAULT '2' COMMENT '单位: 秒',
  `关联事件` int(11) NOT NULL DEFAULT '0' COMMENT '0不限制',
  `关联地图` int(11) NOT NULL DEFAULT '-1' COMMENT '-1不限制',
  `关联区域` int(11) NOT NULL DEFAULT '-1' COMMENT '-1不限制',
  `关联地区` int(11) NOT NULL DEFAULT '-1' COMMENT '-1不限制',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `奖励模版` int(11) NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `公告类型` int(11) NOT NULL DEFAULT '1' COMMENT '1=个人 2=全服普通 3=全服上方',
  `公告内容` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL DEFAULT '' COMMENT '涉及到的格式 %s 具体根据实际有多少个%s来决定',
  `备注` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL DEFAULT '' COMMENT '动作触发功能表的备注',
  PRIMARY KEY (`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_卡牌效果控制表` (
  `分组编号` int(11) NOT NULL COMMENT '分组编号',
  `2件技能效果` int(11) NOT NULL DEFAULT '0' COMMENT '2件技能效果',
  `4件技能效果` int(11) NOT NULL DEFAULT '0' COMMENT '4件技能效果',
  `6件技能效果` int(11) NOT NULL DEFAULT '0' COMMENT '6件技能效果',
  `8件技能效果` int(11) NOT NULL DEFAULT '0' COMMENT '8件技能效果',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '卡牌效果控制表的备注',
  PRIMARY KEY (`分组编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_卡牌物品控制表` (
  `卡牌物品编号` int(11) NOT NULL COMMENT '卡牌物品编号',
  `分组1` int(11) NOT NULL DEFAULT '0' COMMENT '分组1',
  `分组2` int(11) NOT NULL DEFAULT '0' COMMENT '分组2',
  `分组3` int(11) NOT NULL DEFAULT '0' COMMENT '分组3',
  `镶嵌需求模板` int(11) NOT NULL DEFAULT '0' COMMENT '镶嵌需求模板',
  `镶嵌是否绑定` smallint(1) NOT NULL DEFAULT '0' COMMENT '0=不绑定 1=绑定 默认0',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '卡牌物品控制表的备注',
  PRIMARY KEY (`卡牌物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_双甲物品表` (
  `物品编号` int(11) unsigned NOT NULL COMMENT '物品编号',
  `需求模版` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '需求模版',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_吸收盾技能特殊控制表` (
  `技能编号` int(11) NOT NULL COMMENT '盾技能的编号 类似法师牧师的盾',
  `固定吸收量` int(11) NOT NULL DEFAULT '0' COMMENT '默认0表示官方 如果固定 那么其他字段的比例就无效',
  `增加吸收量` int(11) NOT NULL DEFAULT '0' COMMENT '增加的量 与 其他比例加成相加',
  `攻强加成比例` float NOT NULL DEFAULT '0' COMMENT '0=不加成 0.5表示加一半',
  `法强加成比例` float NOT NULL DEFAULT '0' COMMENT '0=不加成 0.5表示加一半',
  `关联的光环` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开表示多个',
  `关联后加成比例` float NOT NULL DEFAULT '1' COMMENT '假如被套盾的目标 拥有关联的光环后给与的加成\r\
例如写入1.2后 可制作诸如\r\
装备: 使你的盾吸收量(寒冰护体)提高20%',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '吸收盾技能特殊控制表的备注',
  PRIMARY KEY (`技能编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_在线商城出售表` (
  `物品编号` int(11) NOT NULL DEFAULT '0' COMMENT '物品编号',
  `归属分类` int(11) NOT NULL DEFAULT '1' COMMENT '分类具体参考 控制总表 1=分类1 以此类推',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_地图区域综合控制表` (
  `地图或区域编号` int(11) NOT NULL COMMENT '正数地图 负数区域Area',
  `设置属性` enum('不作改变','安全地区','全体攻击','组队攻击','公会攻击') NOT NULL DEFAULT '不作改变' COMMENT '设置属性',
  `允许进入` smallint(1) NOT NULL DEFAULT '1' COMMENT '允许进入',
  `允许决斗` smallint(1) NOT NULL DEFAULT '1' COMMENT '允许决斗',
  `允许组队` smallint(1) NOT NULL DEFAULT '1' COMMENT '允许组队',
  `允许陆地坐骑` smallint(1) NOT NULL DEFAULT '1' COMMENT '允许陆地坐骑',
  `允许飞行坐骑` smallint(1) NOT NULL DEFAULT '1' COMMENT '允许飞行坐骑',
  `联盟墓地坐标模版` int(11) NOT NULL DEFAULT '0' COMMENT '联盟墓地坐标模版',
  `部落墓地坐标模版` int(11) NOT NULL DEFAULT '0' COMMENT '部落墓地坐标模版',
  `禁止技能施放` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `强制光环无效` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `进入提示内容` varchar(255) NOT NULL DEFAULT '' COMMENT '进入提示内容',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '地图区域综合控制表的备注',
  PRIMARY KEY (`地图或区域编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_地图或区域墓地复活表` (
  `地图或区域` int(11) NOT NULL DEFAULT '0' COMMENT '正数地图 负数区域',
  `阵营` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '0=不限制 1=联盟 2=部落',
  `墓地坐标区间Min` int(11) unsigned NOT NULL DEFAULT '1' COMMENT '墓地坐标区间Min',
  `墓地坐标区间Max` int(11) unsigned NOT NULL DEFAULT '10' COMMENT '墓地坐标区间Max',
  `复活后血量蓝量比例` float unsigned NOT NULL DEFAULT '1' COMMENT '复活后血量蓝量比例',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '备忘',
  PRIMARY KEY (`地图或区域`,`阵营`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_地图玩家光环表` (
  `地图编号` int(11) NOT NULL DEFAULT '-1' COMMENT '地图编号',
  `阵营` int(11) NOT NULL DEFAULT '0' COMMENT '0=所有 1=联盟 2=部落',
  `职业掩码` int(11) NOT NULL DEFAULT '0' COMMENT '0=所有',
  `光环组` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT '' COMMENT '以#隔开',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT '' COMMENT '地图玩家光环表的备注',
  PRIMARY KEY (`地图编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_大秘境_副本信息` (
  `副本id` int(11) NOT NULL COMMENT '副本id',
  `副本难度` enum('5人普通','5人英雄','10人普通','10人英雄','25人普通','25人英雄') CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '5人普通' COMMENT '副本难度',
  `副本名称` varchar(50) CHARACTER SET utf8 COLLATE utf8_unicode_ci DEFAULT NULL COMMENT '副本名称',
  `秘境名称` varchar(50) CHARACTER SET utf8 COLLATE utf8_unicode_ci DEFAULT NULL COMMENT '秘境名称',
  PRIMARY KEY (`副本id`,`副本难度`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_大秘境_宝箱配置` (
  `宝箱id` int(11) NOT NULL COMMENT '宝箱ID',
  `宝箱名称` varchar(50) NOT NULL COMMENT '宝箱名称',
  `最低分数` int(11) NOT NULL COMMENT '领取该宝箱所需的最低分数',
  `最高分数` int(11) NOT NULL COMMENT '领取该宝箱所需的最高分数',
  `宝箱物品id` int(11) NOT NULL COMMENT '宝箱物品的ItemID',
  `宝箱数量` int(11) DEFAULT '1' COMMENT '发放的宝箱数量',
  `显示顺序` int(11) DEFAULT '0' COMMENT '界面显示顺序（从小到大）',
  `宝箱描述` varchar(255) DEFAULT NULL COMMENT '宝箱描述',
  PRIMARY KEY (`宝箱id`) USING BTREE,
  KEY `idx_score_range` (`最低分数`,`最高分数`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC COMMENT='大秘境宝箱配置表';

CREATE TABLE IF NOT EXISTS `_大秘境_杂项配置` (
  `id` int(11) NOT NULL AUTO_INCREMENT COMMENT '记录ID',
  `值` varchar(300) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '' COMMENT '大秘境_杂项配置的值',
  `字段` varchar(200) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '' COMMENT '字段',
  `备注` varchar(300) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '' COMMENT '大秘境_杂项配置的备注',
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=36 DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_大秘境_系统配置` (
  `配置名称` varchar(50) NOT NULL COMMENT '配置项名称',
  `配置值` varchar(255) NOT NULL COMMENT '配置项值',
  `配置说明` varchar(255) DEFAULT NULL COMMENT '配置说明',
  PRIMARY KEY (`配置名称`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC COMMENT='大秘境系统配置表';

CREATE TABLE IF NOT EXISTS `_大秘境_钥匙配置` (
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci DEFAULT NULL COMMENT '大秘境_钥匙配置的备注',
  `秘境钥匙id` int(11) NOT NULL COMMENT '秘境钥匙id',
  `秘境层数` int(11) DEFAULT NULL COMMENT '秘境层数',
  `副本mapid` int(11) DEFAULT NULL COMMENT '副本mapid',
  `召唤的npc` int(11) DEFAULT NULL COMMENT '召唤的npc',
  `挑战限时(秒)` int(11) DEFAULT NULL COMMENT '挑战限时(秒)',
  `允许死亡次数` int(11) DEFAULT NULL COMMENT '允许死亡次数',
  `需要击杀的boss(多个用#隔开)` varchar(300) CHARACTER SET utf8 COLLATE utf8_unicode_ci DEFAULT NULL COMMENT '最后一个代表最终首领',
  `死亡后复活的坐标` varchar(300) CHARACTER SET utf8 COLLATE utf8_unicode_ci DEFAULT NULL COMMENT '死亡后复活的坐标',
  `层数说明` varchar(300) CHARACTER SET utf8 COLLATE utf8_unicode_ci DEFAULT NULL COMMENT '层数说明',
  `副本难度` enum('5人普通','5人英雄','10人普通','10人英雄','25人普通','25人英雄') CHARACTER SET utf8 COLLATE utf8_unicode_ci DEFAULT NULL COMMENT '副本难度',
  `掉落物品` varchar(200) CHARACTER SET utf8 COLLATE utf8_unicode_ci DEFAULT NULL COMMENT '掉落物品',
  `允许组队` enum('Y','N') CHARACTER SET utf8 COLLATE utf8_unicode_ci DEFAULT 'Y' COMMENT '允许组队',
  `小怪进度值` decimal(5,2) DEFAULT '0.50' COMMENT '击杀普通小怪获得的进度值(0.01-100.00)',
  `进度模式` enum('BOSS模式','进度条模式') CHARACTER SET utf8 COLLATE utf8_unicode_ci DEFAULT '进度条模式' COMMENT '通关模式：BOSS模式(击杀所有BOSS)或进度条模式(进度100%+最终首领)',
  `最终首领ID` int(11) DEFAULT '0' COMMENT '最终首领的Entry ID，0表示无最终首领',
  PRIMARY KEY (`秘境钥匙id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_天赋技能需求表` (
  `天赋技能编号` int(11) unsigned NOT NULL COMMENT '天赋技能编号',
  `需求模版` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '需求模版',
  `需要当前系点数` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '需要当前系点数',
  `需要全部点数` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '需要全部点数',
  `互斥组` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开 在组内的会跟天赋技能编号互斥',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '天赋技能需求表的备注',
  PRIMARY KEY (`天赋技能编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_守卫雅典娜功能表` (
  `阶段` int(11) unsigned NOT NULL COMMENT '阶段',
  `生物编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '生物编号',
  `移动速度` float unsigned NOT NULL DEFAULT '1.14' COMMENT '移动速度',
  `刷新波数` int(11) unsigned NOT NULL DEFAULT '1' COMMENT '这一个阶段刷新的批次',
  `刷新间隔` int(11) unsigned NOT NULL DEFAULT '1' COMMENT '以秒为单位',
  PRIMARY KEY (`阶段`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_守卫雅典娜配置表` (
  `序号` int(11) NOT NULL COMMENT '守卫雅典娜配置表的序号',
  `配置说明` varchar(255) NOT NULL DEFAULT '' COMMENT '守卫雅典娜配置表的配置说明',
  `配置值` varchar(255) NOT NULL DEFAULT '' COMMENT '守卫雅典娜配置表的配置值',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '守卫雅典娜配置表的备注',
  PRIMARY KEY (`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_宝石不可拆除表` (
  `宝石物品编号` int(11) unsigned NOT NULL COMMENT '宝石物品编号',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '宝石不可拆除表的备注',
  PRIMARY KEY (`宝石物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_幸运抽奖表` (
  `物品编号` int(11) unsigned NOT NULL COMMENT '物品ID',
  `物品数量` int(11) NOT NULL COMMENT '物品数量',
  `几率` float unsigned NOT NULL DEFAULT '10' COMMENT '幸运抽奖表的几率',
  `是否公告提示` int(11) NOT NULL DEFAULT '1' COMMENT '是否公告提示',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '备忘',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_战场控制表` (
  `战场编号` int(11) unsigned NOT NULL COMMENT '战场ID',
  `需要伤害量` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '需要伤害量',
  `需要治疗量` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '需要治疗量',
  `胜利奖励模版` int(11) NOT NULL DEFAULT '0' COMMENT '胜利奖励模版',
  `失败奖励模版` int(11) NOT NULL DEFAULT '0' COMMENT '失败奖励模版',
  `胜利人头数` int(11) NOT NULL DEFAULT '0' COMMENT '需要某一方击杀多少人则赢',
  `旗帜或资源数量` int(11) NOT NULL DEFAULT '0' COMMENT '某些战场不可用 0=官方',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '备忘',
  PRIMARY KEY (`战场编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_战宠基础配置表` (
  `生物编号` int(11) unsigned NOT NULL COMMENT '生物编号',
  `宠物阶级` int(11) NOT NULL DEFAULT '1' COMMENT '宠物阶级',
  `所需经验` int(11) NOT NULL DEFAULT '0' COMMENT '所需经验',
  `资质评分` bigint(20) NOT NULL DEFAULT '888' COMMENT '资质评分',
  `属性模版` int(11) NOT NULL DEFAULT '0' COMMENT '属性模版',
  `力量` int(11) NOT NULL DEFAULT '100' COMMENT '力量',
  `敏捷` int(11) NOT NULL DEFAULT '100' COMMENT '敏捷',
  `智力` int(11) NOT NULL DEFAULT '100' COMMENT '智力',
  `精神` int(11) NOT NULL DEFAULT '100' COMMENT '精神',
  `技能1` int(11) NOT NULL DEFAULT '0' COMMENT '技能1',
  `技能2` int(11) NOT NULL DEFAULT '0' COMMENT '技能2',
  `技能3` int(11) NOT NULL DEFAULT '0' COMMENT '技能3',
  `技能4` int(11) NOT NULL DEFAULT '0' COMMENT '技能4',
  `技能5` int(11) NOT NULL DEFAULT '0' COMMENT '技能5',
  `技能6` int(11) NOT NULL DEFAULT '0' COMMENT '技能6',
  `技能7` int(11) NOT NULL DEFAULT '0' COMMENT '技能7',
  `技能8` int(11) NOT NULL DEFAULT '0' COMMENT '技能8',
  `合体光环集合` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `合体加成比例` float NOT NULL DEFAULT '1' COMMENT '合体加成比例',
  PRIMARY KEY (`生物编号`,`宠物阶级`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_战宠经验奖励表` (
  `怪物或物品编号` int(11) NOT NULL COMMENT '怪物或物品编号',
  `直达阶级` int(11) NOT NULL DEFAULT '0' COMMENT '直达阶级',
  `奖励经验` int(11) NOT NULL DEFAULT '0' COMMENT '奖励经验',
  `启用加1级` smallint(1) NOT NULL DEFAULT '0' COMMENT '0=不启用 1=启用 (启用后表示在当前等级上加1级)',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '战宠经验奖励表的备注',
  PRIMARY KEY (`怪物或物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_战宠蛋使用配置表` (
  `使用物品编号` int(11) NOT NULL COMMENT '使用物品编号',
  `宠物编号` int(11) NOT NULL DEFAULT '0' COMMENT '宠物编号',
  `阶级组` int(11) NOT NULL DEFAULT '0' COMMENT '从组内概率出什么阶级的 默认0',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '战宠蛋使用配置表的备注',
  PRIMARY KEY (`使用物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_战宠蛋阶级组表` (
  `阶级组号` int(11) NOT NULL DEFAULT '1' COMMENT '阶级组号',
  `直接阶级` int(11) NOT NULL DEFAULT '1' COMMENT '直接阶级',
  `概率` float NOT NULL DEFAULT '30' COMMENT '概率',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '战宠蛋阶级组表的备注',
  PRIMARY KEY (`阶级组号`,`直接阶级`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_技能` (
  `备注` varchar(128) DEFAULT NULL COMMENT '技能的备注',
  `技能ID` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '技能ID',
  `释放技能需求模板ID` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '释放技能需求模板ID',
  `伤害倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '伤害倍率',
  `治疗倍率` float unsigned NOT NULL DEFAULT '1' COMMENT '治疗倍率',
  `固定伤害` bigint(20) NOT NULL DEFAULT '0' COMMENT '固定伤害',
  `是否禁用` tinyint(1) NOT NULL DEFAULT '0' COMMENT '是否禁用',
  `施法时间` int(10) NOT NULL DEFAULT '-1' COMMENT '-1=官方',
  `持续时间` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '持续时间',
  `冷却时间` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '冷却时间',
  `触发机率` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '触发机率',
  `周期时间1` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '周期时间1',
  `周期时间2` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '周期时间2',
  `周期时间3` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '周期时间3',
  PRIMARY KEY (`技能ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_技能_主属性转换` (
  `备注` varchar(128) DEFAULT NULL COMMENT '用于制作智力转法伤这种光环',
  `光环技能ID` int(11) NOT NULL DEFAULT '0' COMMENT '光环技能ID',
  `光环类型1` enum('治疗效果','法术强度','近战攻击强度','远程攻击强度','空') DEFAULT '空' COMMENT '光环类型1',
  `属性类型1` enum('力量','敏捷','耐力','智力','精神','空') NOT NULL DEFAULT '空' COMMENT '属性类型1',
  `转换百分比1` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '转换百分比1',
  `光环类型2` enum('近战攻击强度','法术强度','治疗效果','空','远程攻击强度') DEFAULT '空' COMMENT '光环类型2',
  `属性类型2` enum('力量','敏捷','耐力','智力','精神','空') NOT NULL DEFAULT '空' COMMENT '属性类型2',
  `转换百分比2` int(11) NOT NULL DEFAULT '0' COMMENT '转换百分比2',
  `光环类型3` enum('法术强度','治疗效果','施法速度','空','近战攻击强度','远程攻击强度') DEFAULT '空' COMMENT '光环类型3',
  `属性类型3` enum('力量','敏捷','耐力','智力','精神','空') NOT NULL DEFAULT '空' COMMENT '属性类型3',
  `转换百分比3` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '转换百分比3',
  PRIMARY KEY (`光环技能ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_技能_光环叠加` (
  `备注` varchar(128) DEFAULT NULL COMMENT '技能_光环叠加的备注',
  `光环技能ID` int(10) unsigned NOT NULL COMMENT '光环技能ID',
  `叠加层数` int(10) unsigned NOT NULL COMMENT '叠加层数',
  `触发技能组` varchar(128) NOT NULL COMMENT '触发技能组',
  `触发技能后移除层数` int(10) unsigned NOT NULL COMMENT '触发技能后移除层数',
  PRIMARY KEY (`光环技能ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_技能_光环死亡触发` (
  `备注` varchar(128) DEFAULT NULL COMMENT '技能_光环死亡触发的备注',
  `光环技能ID` int(11) unsigned NOT NULL COMMENT '光环技能ID',
  `冷却时间` int(11) unsigned NOT NULL DEFAULT '1000' COMMENT '冷却时间',
  `是否阻止死亡` tinyint(1) unsigned NOT NULL DEFAULT '1' COMMENT '是否阻止死亡',
  `对攻击者施放技能ID` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '对攻击者施放技能ID',
  `对攻击者施放技能几率` float unsigned NOT NULL DEFAULT '0' COMMENT '对攻击者施放技能几率',
  `对自身施放技能ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '对自身施放技能ID',
  `对自身施放技能几率` float unsigned NOT NULL DEFAULT '0' COMMENT '对自身施放技能几率',
  PRIMARY KEY (`光环技能ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_技能_光环百分比效果` (
  `备注` varchar(128) DEFAULT NULL COMMENT '技能_光环百分比效果的备注',
  `光环技能ID` int(11) NOT NULL DEFAULT '0' COMMENT '光环技能ID',
  `效果类型1` enum('空','生命值','力量','敏捷','耐力','智力','精神','全属性','近战攻击强度','远程攻击强度','法术强度','治疗效果','施法速度','近战攻击速度','远程攻击速度','造成奥术伤害','造成火焰伤害','造成冰霜伤害','造成神圣伤害','造成暗影伤害','造成自然伤害','造成法术伤害','造成物理伤害','受到所有伤害','造成暴击伤害','造成所有伤害') NOT NULL DEFAULT '空' COMMENT '效果类型1',
  `提升类型1` enum('百分比','固定值') NOT NULL DEFAULT '百分比' COMMENT '提升类型1',
  `提升数值1` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '提升数值1',
  `效果类型2` enum('空','生命值','力量','敏捷','耐力','智力','精神','全属性','近战攻击强度','远程攻击强度','法术强度','治疗效果','施法速度','近战攻击速度','远程攻击速度','造成奥术伤害','造成火焰伤害','造成冰霜伤害','造成神圣伤害','造成暗影伤害','造成自然伤害','造成法术伤害','造成物理伤害','受到所有伤害','造成暴击伤害','造成所有伤害') NOT NULL DEFAULT '空' COMMENT '效果类型2',
  `提升类型2` enum('百分比','固定值') NOT NULL DEFAULT '百分比' COMMENT '提升类型2',
  `提升数值2` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '提升数值2',
  `效果类型3` enum('空','生命值','力量','敏捷','耐力','智力','精神','全属性','近战攻击强度','远程攻击强度','法术强度','治疗效果','施法速度','近战攻击速度','远程攻击速度','造成奥术伤害','造成火焰伤害','造成冰霜伤害','造成神圣伤害','造成暗影伤害','造成自然伤害','造成法术伤害','造成物理伤害','受到所有伤害','造成暴击伤害','造成所有伤害') NOT NULL DEFAULT '空' COMMENT '效果类型3',
  `提升类型3` enum('百分比','固定值') NOT NULL DEFAULT '百分比' COMMENT '提升类型3',
  `提升数值3` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '提升数值3',
  PRIMARY KEY (`光环技能ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_技能_光环触发` (
  `备注` varchar(128) NOT NULL DEFAULT '' COMMENT '技能_光环触发的备注',
  `光环ID` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '光环ID，有此光环时，才会触发下面技能',
  `链接技能组` varchar(255) DEFAULT '' COMMENT '链接的技能组，为空时所有技能都可能触发目标技能。不为空时，组里技能触发目标技能',
  `触发技能组` varchar(255) DEFAULT NULL COMMENT '触发技能组',
  `目标最大数量` int(11) NOT NULL DEFAULT '1' COMMENT '触发的技能攻击的目标个数',
  `目标范围` float NOT NULL DEFAULT '100' COMMENT '触发的技能攻击的范围',
  `忽略目标类型` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '忽略目标的掩码，1 - 目标为BOSS时不触发 2 - 在副本内不触发  4 - 在野外不触发 8 - 目标为玩家时不触发 16 - 目标为生物时不触发',
  `触发类型` int(11) NOT NULL DEFAULT '20' COMMENT '触发的条件，意义和spell.dbc第35列procFlags一致，列举下常用的 20 - 物理攻击 320 - 远程攻击 81920 - 法术伤害 664232 - 受到伤害',
  `触发几率` int(11) unsigned NOT NULL DEFAULT '100' COMMENT '触发的几率，例如80 则触发几率为80%',
  `冷却时间` int(10) unsigned NOT NULL DEFAULT '1000' COMMENT '冷却时间，单位毫秒，例如1000，则1秒内只能触发一次',
  `攻强百分比增加基础值1` float NOT NULL DEFAULT '0' COMMENT '动态增加的攻强的百分比，动态修改spell.dbc 81 - 83基础值，最终数值等于原值加攻强的百分比',
  `攻强百分比增加基础值2` float NOT NULL DEFAULT '0' COMMENT '动态增加的攻强的百分比，动态修改spell.dbc 81 - 83基础值，最终数值等于原值加攻强的百分比',
  `攻强百分比增加基础值3` float NOT NULL DEFAULT '0' COMMENT '动态增加的攻强的百分比，动态修改spell.dbc 81 - 83基础值，最终数值等于原值加攻强的百分比',
  `法伤百分比增加基础值1` float NOT NULL DEFAULT '0' COMMENT '动态增加的法伤的百分比，动态修改spell.dbc 81 - 83基础值，最终数值等于原值加攻强的百分比',
  `法伤百分比增加基础值2` float NOT NULL DEFAULT '0' COMMENT '动态增加的法伤的百分比，动态修改spell.dbc 81 - 83基础值，最终数值等于原值加攻强的百分比',
  `法伤百分比增加基础值3` float NOT NULL DEFAULT '0' COMMENT '动态增加的法伤的百分比，动态修改spell.dbc 81 - 83基础值，最终数值等于原值加攻强的百分比',
  PRIMARY KEY (`光环ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_技能_数据库修改` (
  `Comment` varchar(255) NOT NULL DEFAULT '' COMMENT '备注说明',
  `spellid` int(11) NOT NULL DEFAULT '0' COMMENT '法术ID',
  `Effect1` tinyint(3) NOT NULL DEFAULT '0' COMMENT '法术效果1类型',
  `Effect2` tinyint(3) NOT NULL DEFAULT '0' COMMENT '法术效果2类型',
  `Effect3` tinyint(3) NOT NULL DEFAULT '0' COMMENT '法术效果3类型',
  `AuraName1` int(11) NOT NULL DEFAULT '0' COMMENT '光环1名称',
  `AuraName2` int(11) NOT NULL DEFAULT '0' COMMENT '光环2名称',
  `AuraName3` int(11) NOT NULL DEFAULT '0' COMMENT '光环3名称',
  `MiscValue1` tinyint(11) NOT NULL DEFAULT '0' COMMENT '法术效果1附加参数',
  `MiscValue2` tinyint(11) NOT NULL DEFAULT '0' COMMENT '法术效果2附加参数',
  `MiscValue3` tinyint(11) NOT NULL DEFAULT '0' COMMENT '法术效果3附加参数',
  `BasePoints1` int(11) NOT NULL DEFAULT '0' COMMENT '法术效果1基础数值',
  `BasePoints2` int(11) NOT NULL DEFAULT '0' COMMENT '法术效果2基础数值',
  `BasePoints3` int(11) NOT NULL DEFAULT '0' COMMENT '法术效果3基础数值',
  `SpellMaskA1` int(11) NOT NULL DEFAULT '0' COMMENT '法术族掩码A1',
  `SpellMaskA2` int(11) NOT NULL DEFAULT '0' COMMENT '法术族掩码A2',
  `SpellMaskA3` int(11) NOT NULL DEFAULT '0' COMMENT '法术族掩码A3',
  `SpellMaskB1` int(11) NOT NULL DEFAULT '0' COMMENT '法术族掩码B1',
  `SpellMaskB2` int(11) NOT NULL DEFAULT '0' COMMENT '法术族掩码B2',
  `SpellMaskB3` int(11) NOT NULL DEFAULT '0' COMMENT '法术族掩码B3',
  `SpellMaskC1` int(11) NOT NULL DEFAULT '0' COMMENT '法术族掩码C1',
  `SpellMaskC2` int(11) NOT NULL DEFAULT '0' COMMENT '法术族掩码C2',
  `SpellMaskC3` int(11) NOT NULL DEFAULT '0' COMMENT '法术族掩码C3',
  `EffectTarget1` tinyint(3) NOT NULL DEFAULT '1' COMMENT '法术效果1目标类型',
  `EffectTarget2` tinyint(3) NOT NULL DEFAULT '1' COMMENT '法术效果2目标类型',
  `EffectTarget3` tinyint(3) NOT NULL DEFAULT '1' COMMENT '法术效果3目标类型',
  `targetspell1` int(11) NOT NULL DEFAULT '0' COMMENT '目标法术1ID',
  `targetspell2` int(11) NOT NULL DEFAULT '0' COMMENT '目标法术2ID',
  `targetspell3` int(11) NOT NULL DEFAULT '0' COMMENT '目标法术3ID',
  `cooldown` int(11) NOT NULL DEFAULT '0' COMMENT '冷却时间',
  `targetflag` int(11) NOT NULL DEFAULT '0' COMMENT '目标标记',
  `chance` int(11) NOT NULL DEFAULT '0' COMMENT '触发几率',
  `charges` int(11) NOT NULL DEFAULT '0' COMMENT '可触发次数',
  `SpellFamilyName` tinyint(3) NOT NULL DEFAULT '0' COMMENT '法术族名称',
  PRIMARY KEY (`spellid`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_技能_目标限制` (
  `comment` varchar(128) DEFAULT NULL COMMENT '备注说明',
  `技能ID` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '技能ID',
  `目标掩码` int(11) unsigned NOT NULL DEFAULT '0' COMMENT 'BOSS-1 小怪-2 玩家-4。例如限制技能目标为玩家，则填4。限制技能目标为小怪和玩家，则填2+4 = 6',
  PRIMARY KEY (`技能ID`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_技能光环控制表` (
  `技能光环编号` int(11) NOT NULL COMMENT '技能光环编号',
  `吸血倍率` float NOT NULL DEFAULT '1' COMMENT '1=打出100伤害 就回100',
  `PVE增伤倍率之前` float NOT NULL DEFAULT '0' COMMENT 'PVE增伤倍率之前',
  `PVE增伤倍率` float NOT NULL DEFAULT '0' COMMENT 'PVE增伤倍率',
  `PVE减伤倍率` float NOT NULL DEFAULT '0' COMMENT 'PVE减伤倍率',
  `PVP增伤倍率` float NOT NULL DEFAULT '0' COMMENT 'PVP增伤倍率',
  `PVP减伤倍率` float NOT NULL DEFAULT '0' COMMENT 'PVP减伤倍率',
  `增加物品爆率` float NOT NULL DEFAULT '0' COMMENT '增加物品爆率',
  `物品爆率比例` float NOT NULL DEFAULT '0' COMMENT '物品爆率比例',
  `多倍掉落几率` float NOT NULL DEFAULT '100' COMMENT '此项表示触发多倍掉落的几率',
  `多倍掉落` float NOT NULL DEFAULT '0' COMMENT '多倍掉落',
  `护盾吸收攻强比` float NOT NULL DEFAULT '0' COMMENT '护盾吸收攻强比',
  `护盾吸收法强比` float NOT NULL DEFAULT '0' COMMENT '护盾吸收法强比',
  `斗气经验比例` float NOT NULL DEFAULT '0' COMMENT '0.1=10% 以此类推',
  `PVP物理暴击伤害比例` float NOT NULL DEFAULT '0' COMMENT 'PVP物理暴击伤害比例',
  `PVP技能暴击伤害比例` float NOT NULL DEFAULT '0' COMMENT 'PVP技能暴击伤害比例',
  `PVE物理暴击伤害比例` float NOT NULL DEFAULT '0' COMMENT 'PVE物理暴击伤害比例',
  `PVE技能暴击伤害比例` float NOT NULL DEFAULT '0' COMMENT 'PVE技能暴击伤害比例',
  `附带真实伤害值` bigint(20) NOT NULL DEFAULT '0' COMMENT '附带真实伤害值',
  `额外掉落几率` float NOT NULL DEFAULT '0' COMMENT '额外掉落几率',
  `额外掉落编号` int(11) NOT NULL DEFAULT '0' COMMENT '额外掉落编号',
  `掉落数量比例` float NOT NULL DEFAULT '1' COMMENT '默认1=原始 1.5=1.5倍2=2倍',
  `掉落数量加值` int(11) NOT NULL DEFAULT '0' COMMENT '在原有的数量上 加法',
  PRIMARY KEY (`技能光环编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_技能加强关联控制表` (
  `技能光环编号` int(11) NOT NULL COMMENT '技能光环编号',
  `加强技能编号` int(11) NOT NULL DEFAULT '0' COMMENT '加强技能编号',
  `PVE增伤倍率` float NOT NULL DEFAULT '0' COMMENT 'PVE增伤倍率',
  `PVE治疗倍率` float NOT NULL DEFAULT '0' COMMENT 'PVE治疗倍率',
  `PVP增伤倍率` float NOT NULL DEFAULT '0' COMMENT 'PVP增伤倍率',
  `PVP治疗倍率` float NOT NULL DEFAULT '0' COMMENT 'PVP治疗倍率',
  `全局伤害加值` bigint(20) NOT NULL DEFAULT '0' COMMENT '全局伤害加值',
  `属性掩码` int(11) NOT NULL DEFAULT '0' COMMENT '0=无效\r\
1=力量\r\
2=敏捷\r\
4=耐力\r\
8=智力\r\
16=精神\r\
32=急速等级\r\
1+2=3表示包含力量和敏捷 以此类推',
  `属性强化伤害率` float NOT NULL DEFAULT '0' COMMENT '0=无效 表示根据计算得到的比值 属性掩码的总和',
  `属性转换伤害率` float NOT NULL DEFAULT '0' COMMENT '0=无效 0.1表示+10% 算法根据属性掩码的总和',
  `附加法术强度比例伤害` float NOT NULL DEFAULT '0' COMMENT '这是附加的 0.1就表示 +10%',
  `附加攻击强度比例伤害` float NOT NULL DEFAULT '0' COMMENT '这是附加的 0.1就表示 +10%',
  PRIMARY KEY (`技能光环编号`,`加强技能编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_技能施法完成gm命令表` (
  `技能编号` int(11) NOT NULL COMMENT '技能编号',
  `GM命令组` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '以#隔开',
  PRIMARY KEY (`技能编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=gbk ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_技能特殊吸血控制表` (
  `技能光环编号` int(11) NOT NULL COMMENT '技能光环编号',
  `施行技能编号` int(11) NOT NULL DEFAULT '0' COMMENT '施行技能编号',
  `PVE吸血倍率` float NOT NULL DEFAULT '1' COMMENT 'PVE吸血倍率',
  `PVP吸血倍率` float NOT NULL DEFAULT '1' COMMENT 'PVP吸血倍率',
  `PVE固定吸血` bigint(20) NOT NULL DEFAULT '0' COMMENT 'PVE固定吸血',
  `PVP固定吸血` bigint(20) NOT NULL DEFAULT '0' COMMENT 'PVP固定吸血',
  PRIMARY KEY (`技能光环编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_挑战副本配置表` (
  `副本地图编号` int(11) NOT NULL COMMENT '副本地图编号',
  `难度` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '如果不是副本默认0\r\
如果是副本 0=普通1=英雄2=25人普通3=25英雄',
  `生物类型` smallint(1) NOT NULL DEFAULT '0' COMMENT '0=小怪 1=BOSS',
  `生物属性模版` int(11) NOT NULL DEFAULT '0' COMMENT '生物属性模版',
  `菜单文字` varchar(255) NOT NULL DEFAULT '' COMMENT '菜单文字',
  `进入该难度需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '进入该难度需求模版',
  `重置的物品` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '对应 类似重置卷轴',
  `出本重置` tinyint(1) NOT NULL DEFAULT '0' COMMENT '1=出本自动重置，0=不自动重置',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '挑战副本配置表的备注',
  PRIMARY KEY (`副本地图编号`,`难度`,`生物类型`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_文本提示表` (
  `编号` int(11) NOT NULL COMMENT '序号(不能改动)',
  `提示内容` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '功能的名称',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '文本提示表的备注',
  PRIMARY KEY (`编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=gbk ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_斗气奖励表` (
  `怪物或物品编号` int(11) NOT NULL COMMENT '怪物或物品编号',
  `直达等级` int(11) NOT NULL DEFAULT '0' COMMENT '直达等级',
  `奖励经验` bigint(20) NOT NULL DEFAULT '0' COMMENT '奖励经验',
  `启用加1级` smallint(1) NOT NULL DEFAULT '0' COMMENT '0=不启用 1=启用 (启用后表示在当前等级上加1级)',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '斗气奖励表的备注',
  PRIMARY KEY (`怪物或物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_斗气等级经验表` (
  `斗气等级` int(11) NOT NULL COMMENT '斗气等级',
  `需要经验` bigint(20) NOT NULL DEFAULT '0' COMMENT '需要经验',
  `奖励斗气点数` bigint(20) NOT NULL DEFAULT '1' COMMENT '奖励斗气点数',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '斗气等级经验表的备注',
  PRIMARY KEY (`斗气等级`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_斗气配置表` (
  `序号` int(11) NOT NULL COMMENT '斗气配置表的序号',
  `说明` varchar(255) NOT NULL DEFAULT '' COMMENT '说明',
  `功能值` bigint(20) NOT NULL DEFAULT '0' COMMENT '功能值',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '斗气配置表的备注',
  PRIMARY KEY (`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_新物品附魔提取转移表` (
  `装备编号` int(11) NOT NULL COMMENT '装备编号',
  `附魔位置` int(11) NOT NULL DEFAULT '1' COMMENT '附魔位置',
  `转移到物品编号` int(11) NOT NULL DEFAULT '0' COMMENT '转移到物品编号',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT '' COMMENT '新物品附魔提取转移表的备注',
  PRIMARY KEY (`装备编号`,`附魔位置`,`转移到物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_模板_地图控制` (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT '',
  `id` int(10) unsigned NOT NULL DEFAULT '0' COMMENT 'Global Unique Identifier',
  `副本难度` enum('全部','普通5或10人','普通25人','英雄5或10人','英雄25人') CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '全部' COMMENT '副本难度，如果是野外，则无效',
  `秘境难度` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '秘境等级(.传送 坐标 秘境等级)',
  `队伍人数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '动态副本难度_队伍人数',
  `map` int(11) NOT NULL DEFAULT '-1' COMMENT '地图id（指定map难度）优先级map>zone>area',
  `zone` int(11) NOT NULL DEFAULT '-1' COMMENT 'zone id（指定zone难度）',
  `area` int(11) NOT NULL DEFAULT '-1' COMMENT 'area id（指定area难度）',
  `条件需求` int(11) NOT NULL DEFAULT '0' COMMENT '不满足条件时，传出地图',
  `区域属性` enum('安全区','自由PVP','自定义阵营','公会PVP','绝对安全区','正常区域') CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '正常区域' COMMENT '''安全区'',''自由PVP'',''自定义阵营'',''公会PVP'',''绝对安全区'',''正常区域''',
  `进入自动满血` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0否 1是',
  `是否允许组队` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '是否允许组队（0允许1不允许）',
  `是否允许飞行` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '是否允许飞行（0允许1不允许）',
  `禁用技能_多个逗号隔开` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci,
  `禁用物品_多个逗号隔开` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT '',
  `获得光环_多个逗号隔开` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT '',
  `允许坐骑id_多个逗号隔开` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci COMMENT '地图取消坐骑使用限制',
  `可视距离` float unsigned NOT NULL DEFAULT '0' COMMENT '默认 500',
  `__采集_坐标_id_联盟死亡复活点_多个随机逗号隔开` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci COMMENT '传送_死亡_复活的坐标',
  `__采集_坐标_id_部落死亡复活点_多个随机逗号隔开` text CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci COMMENT '传送_死亡_复活的坐标',
  `地图进度标题` varchar(1000) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT '',
  `地图死亡次数限制x` int(10) unsigned DEFAULT '0' COMMENT '达到次数，传到出生地',
  `地图挑战时间秒x` int(10) unsigned DEFAULT '0' COMMENT '超出时间，传回出生地',
  `地图进度值x` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT '' COMMENT '格式:(Rank 值)比如 0 1,3 2（普通怪 获得1进度值,Boss怪 获得2进度值）',
  `地图进度值奖励x` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT '' COMMENT '格式:100 1,300 2（达到100进度值，奖励1，200进度值，奖励2）',
  `地图进度值奖励对象x` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT '' COMMENT '格式:100 0,300 1（达到100进度值，奖励队伍所有人，200进度值，奖励随机一个人）',
  `地图完成进度公告x` int(10) unsigned DEFAULT '0' COMMENT '达到进度奖励时触发公告',
  `地图进度是否循环` int(10) unsigned DEFAULT '0' COMMENT '进度值完成后从0开始循环，0不循环 1循环',
  `进入地图公告` int(10) unsigned DEFAULT '0' COMMENT '进入地图的公告',
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC COMMENT='Item System';

CREATE TABLE IF NOT EXISTS `_模板_地图生物控制` (
  `配置ID` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '配置ID',
  `生物ID` int(11) NOT NULL DEFAULT '-1' COMMENT '生物模板ID(-1=适用所有生物)',
  `生物类型` int(11) NOT NULL DEFAULT '-1' COMMENT '生物类型(-1=全部,0=非Boss,1=普通,2=精英,3=Boss)',
  `地图ID` int(11) NOT NULL DEFAULT '-1' COMMENT '地图ID(-1=适用所有地图)',
  `区域ID` int(11) NOT NULL DEFAULT '-1' COMMENT '区域ID(-1=适用所有区域)',
  `难度` int(11) NOT NULL DEFAULT '-1' COMMENT '副本难度(-1=所有难度,0=普通/10人普通,1=英雄/10人英雄,2=25人普通,3=25人英雄)',
  `秘境难度` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '0' COMMENT '秘境等级(0/空/全部=所有秘境等级，多个等级用逗号隔开)',
  `生物控制组` int(10) unsigned NOT NULL DEFAULT '0',
  `提示方式` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '0=不提示,1=聊天,2=聊天和屏幕,3=特殊提示',
  `刷新公告` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '[提示]: %s 刷新了' COMMENT '%s 会替换为生物名称',
  `刷新时间` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '覆盖尸体移除和重生时间，单位秒；0=使用官方时间',
  `动态难度开关` tinyint(3) unsigned NOT NULL DEFAULT '1' COMMENT '0=关闭战力门槛和动态倍率,1=开启',
  `战力门槛` decimal(65,0) unsigned NOT NULL DEFAULT '0' COMMENT '0=不限制,>0=最低战力并作为动态难度计算基准',
  `难度倍率` double unsigned NOT NULL DEFAULT '1' COMMENT '最终动态倍率=(当前战力/战力门槛)*本值；门槛为0时直接使用本值',
  PRIMARY KEY (`配置ID`),
  KEY `生物模板ID` (`生物ID`),
  KEY `地图ID` (`地图ID`),
  KEY `地图区域难度` (`地图ID`,`区域ID`,`难度`)
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='地图生物属性控制表';

CREATE TABLE IF NOT EXISTS `_模板_物品套装` (
  `套装ID` int(10) unsigned NOT NULL COMMENT '套装唯一标识',
  `组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '套装组别，用于分类管理',
  `几率` float DEFAULT '0' COMMENT '随机分配到该组的几率(0-100)',
  `套装名称` varchar(255) NOT NULL COMMENT '套装的名称',
  `套装描述` text COMMENT '套装的详细描述',
  `需求系统ID` int(10) unsigned DEFAULT '0' COMMENT '旧核心__模版需求总表序号',
  `套装品质` tinyint(3) unsigned DEFAULT '0' COMMENT '套装的品质等级',
  `物品数量` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '激活效果需要的物品数量',
  `效果类型` tinyint(3) unsigned DEFAULT '1' COMMENT '效果类型(固定为1=属性类型)',
  `套装属性` text COMMENT '套装属性效果，格式: "属性类型 数值,属性类型 数值" 如: "4 20,7 30,31 15"',
  `效果ID` int(10) unsigned DEFAULT '0' COMMENT '技能/光环ID，如果有值则自动判断类型：主动技能做强化，被动光环应用到玩家',
  `效果数值` float DEFAULT '0' COMMENT '效果增强百分比(1=1%, 10=10%, 用于增强技能或光环效果)',
  `效果描述` varchar(255) DEFAULT NULL COMMENT '效果的文字描述',
  PRIMARY KEY (`套装ID`,`物品数量`),
  KEY `需求系统ID` (`需求系统ID`),
  KEY `套装ID` (`套装ID`),
  KEY `组` (`组`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='物品套装基本信息表';

CREATE TABLE IF NOT EXISTS `_模板_物品属性` (
  `id` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '属性ID，唯一标识符',
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT '' COMMENT '属性描述，仅供开发者参考',
  `客户端显示` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL COMMENT '客户端显示的属性名称',
  `组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '属性组，用于分类属性',
  `属性类型` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '属性类型，决定属性的效果',
  `获取几率` int(10) unsigned NOT NULL DEFAULT '100' COMMENT '随机生成时的获取几率，1-100',
  `属性最小百分比` bigint(20) NOT NULL DEFAULT '100' COMMENT '属性值的最小百分比',
  `属性最大百分比` bigint(20) NOT NULL DEFAULT '100' COMMENT '属性值的最大百分比',
  `属性计算方式` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0=常规值 1=乘以物品等级 2=固定值',
  `技能模板_组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '用于技能相关属性',
  `属性品质要求` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '物品最低品质要求，0=全部',
  `属性等级要求` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '物品最低等级要求',
  `属性职业要求` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '物品职业要求，0=全部',
  `属性颜色` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT 'FFFFFF' COMMENT '属性文本颜色，格式为RRGGBB',
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC COMMENT='物品属性模板系统';

CREATE TABLE IF NOT EXISTS `_模板_物品强化` (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL,
  `id` int(11) NOT NULL,
  `组` int(10) unsigned NOT NULL DEFAULT '0',
  `等级` int(10) unsigned NOT NULL DEFAULT '0',
  `几率` float unsigned NOT NULL DEFAULT '0',
  `物品属性_模板_组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联物品属性_模板',
  `平均随机分配` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '属性分配方式（0平均分配 1随机分配）',
  `属性值1` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最小值',
  `属性值2` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最大值',
  `属性百分比1` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最小百分比',
  `属性百分比2` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最大百分比',
  `达到该强化等级后的奖励属性` bigint(20) unsigned NOT NULL DEFAULT '0',
  `达到该强化等级后的奖励属性百分比` bigint(20) unsigned NOT NULL DEFAULT '0',
  `物品技能_模板_id_多个逗号隔开` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL,
  `强化需求` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联旧核心__模版需求总表的序号',
  `失败处理` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '失败后处理（0不变，1等级清0，2摧毁，3失败降级）',
  `失败降级等级` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '仅失败处理=3时生效；0、1、2时忽略',
  `保底失败次数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0关闭；连续失败达到该次数后本次必定成功',
  `每次失败增加成功率` float unsigned NOT NULL DEFAULT '0' COMMENT '按当前连续失败次数增加成功率',
  `幸运物品` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '背包存在时自动消耗并增加本次成功率',
  `幸运物品数量` int(10) unsigned NOT NULL DEFAULT '0',
  `幸运增加成功率` float unsigned NOT NULL DEFAULT '0',
  `保护物品` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '失败将清零、降级或摧毁时自动消耗并保护装备',
  `保护物品数量` int(10) unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci COMMENT='物品强化模板表';

CREATE TABLE IF NOT EXISTS `_模板_物品成长` (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL,
  `id` int(11) NOT NULL,
  `需求模板` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联需求模板的ID，0表示无需求限制，大于0时需要满足对应需求模板才能随机到该组',
  `组` int(10) unsigned NOT NULL DEFAULT '1',
  `等级` int(10) unsigned NOT NULL DEFAULT '0',
  `几率` int(10) unsigned NOT NULL DEFAULT '100' COMMENT '该组的随机几率权重，数值越大被随机到的概率越高',
  `成长经验` int(10) unsigned NOT NULL DEFAULT '0',
  `物品属性_模板_组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联mod-item-attributes模块的属性组ID，0表示不关联任何属性组，大于0时会在物品升级时自动应用对应属性组的随机属性',
  `平均随机分配` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '属性分配方式：0=平均分配（所有属性都加固定值），1=随机分配（在最小值和最大值之间随机分配）',
  `属性值1` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最小值',
  `属性值2` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最大值',
  `属性百分比1` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最小百分比',
  `属性百分比2` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '随机属性最大百分比',
  `达到该成长等级后的奖励属性` bigint(20) unsigned NOT NULL DEFAULT '0',
  `达到该成长等级后的奖励属性百分比` bigint(20) unsigned NOT NULL DEFAULT '0',
  `物品技能_模板id_多个逗号隔开` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL COMMENT '【已弃用】保留字段，不再使用',
  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE KEY `uk_组_等级` (`组`,`等级`) COMMENT '【审计修复】确保每个组内等级唯一，避免重复配置导致的不确定行为'
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC COMMENT='物品成长模板';

CREATE TABLE IF NOT EXISTS `_模板_物品技能` (
  `id` int(11) NOT NULL AUTO_INCREMENT COMMENT '技能模板唯一ID，主键',
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL COMMENT '技能的中文描述，用于管理员查看',
  `客户端显示` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT NULL COMMENT '客户端显示的技能描述，会在物品提示中显示',
  `组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '技能组ID，相同组的技能属于同一系列（如火系、冰系），可用于升级或随机选择',
  `等级` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '技能在组内的等级，1级为基础，数字越大等级越高',
  `技能id` int(11) NOT NULL DEFAULT '0' COMMENT '游戏中实际的技能ID，对应spell.dbc中的ID',
  `几率` float NOT NULL DEFAULT '100' COMMENT '选择权重：同组内有多个技能时，此值越高被选中概率越大。计算方式：该技能几率/同组所有技能几率之和',
  `技能触发类型` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '技能触发类型：0=使用触发，1=装备触发，2=击中触发，100=学习技能',
  `触发几率` float NOT NULL DEFAULT '100' COMMENT '技能触发几率：0-100之间的数值，表示技能触发的百分比概率。例如：50表示50%几率触发',
  `技能使用次数` smallint(6) NOT NULL DEFAULT '0' COMMENT '技能使用次数：0=无限使用，负数=用完删除物品，正数=用完不删除物品',
  `技能每分钟触发次数` float NOT NULL DEFAULT '0' COMMENT '技能每分钟触发次数（PPM），仅当触发类型为2(击中触发)时有效',
  `技能冷却时间` tinyint(4) NOT NULL DEFAULT '0' COMMENT '技能冷却时间模式：0=官方冷却，1=无冷却，2=自定义冷却值',
  `技能冷却值` int(11) NOT NULL DEFAULT '0' COMMENT '技能冷却值，单位毫秒（1000=1秒），仅当技能冷却时间=2时生效',
  `技能消耗` tinyint(4) NOT NULL DEFAULT '0' COMMENT '技能消耗模式：0=官方消耗（法力/能量等），1=无消耗',
  `技能施法` tinyint(4) NOT NULL DEFAULT '0' COMMENT '技能施法时间模式：0=官方施法时间+官方动画，1=无施法时间（瞬发）+有动画，2=无施法时间+无动画（静默模式）',
  `技能强化` tinyint(4) NOT NULL DEFAULT '0' COMMENT '技能伤害/治疗强化模式：0=官方，1=固定值增幅，2=百分比增幅',
  `技能伤害` int(11) NOT NULL DEFAULT '0' COMMENT '技能伤害/治疗增幅值：当强化=1时为固定附加值，当强化=2时为百分比（10=110%）',
  `技能类型` tinyint(4) NOT NULL DEFAULT '0' COMMENT '技能类型：0=不修改，1=buff（修改持续时间）',
  `技能持续时间` int(11) NOT NULL DEFAULT '0' COMMENT '技能持续时间，单位分钟，仅当技能类型=1时生效（1=1分钟，100=100分钟）',
  `启用状态` tinyint(4) NOT NULL DEFAULT '1' COMMENT '是否启用：1=启用，0=禁用（禁用的技能不会被分配）',
  `优先级` int(11) NOT NULL DEFAULT '0' COMMENT '技能优先级，数值越大优先级越高，用于解决技能冲突',
  PRIMARY KEY (`id`) USING BTREE,
  KEY `idx_group_level` (`组`,`等级`) COMMENT '组和等级组合索引，优化按组查询',
  KEY `idx_triggertype` (`技能触发类型`) COMMENT '触发类型索引，优化按类型查询',
  KEY `idx_status` (`启用状态`) COMMENT '启用状态索引，快速过滤禁用技能'
) ENGINE=InnoDB AUTO_INCREMENT=123 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC COMMENT='物品技能系统 - 技能模板库（通用技能模板，可添加到任意物品）';

CREATE TABLE IF NOT EXISTS `_模板_物品洗炼` (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '备注描述',
  `物品id` int(10) unsigned NOT NULL COMMENT '允许洗炼的物品模板ID',
  `_物品_鉴定_组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联 _模板_物品鉴定.组；仅配置类目4并使用_物品随机附魔控制表时允许填0',
  `洗炼需求` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联 __模版需求总表.序号',
  `洗炼类目限制` varchar(1000) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '1基础属性,2追加属性,3追加技能,4附魔；类目4优先使用ItemTargetEnch同源的官方附魔配置',
  PRIMARY KEY (`物品id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='物品洗炼配置';

CREATE TABLE IF NOT EXISTS `_模板_物品符文` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '符文记录ID(自增)',
  `物品ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '符文物品ID (item_template.entry)',
  `组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '符文组别，用于分类管理',
  `名称` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '符文名称',
  `符文槽位` tinyint(3) unsigned NOT NULL DEFAULT '1' COMMENT '符文槽位 (1-20，表示槽位1到槽位20)',
  `符文品质` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '符文品质 (0=普通, 1=优秀, 2=精良, 3=史诗, 4=传说)',
  `法术ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联法术ID',
  `冷却时间` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '冷却时间(毫秒)',
  `触发几率` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '触发几率(%)',
  `效果类型` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '效果类型 (0=无加成, 1=属性加成, 2=技能加成, 3=百分比属性加成, 4=百分比技能加成)',
  `效果值` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT '效果值 (格式: 属性类型 值,属性类型 值 | 技能加成用 99 值, 如: 3 10,4 15=耐力+10力量+15 | 99 10=技能+10)',
  `效果描述` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci COMMENT '效果的详细描述',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=120 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='符文系统 - 符文信息表';

CREATE TABLE IF NOT EXISTS `_模板_物品鉴定` (
  `注释` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci DEFAULT '',
  `id` int(11) NOT NULL,
  `组` int(10) unsigned NOT NULL DEFAULT '1',
  `等级` int(10) unsigned NOT NULL DEFAULT '0',
  `随机几率` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '公式：当前几率除以一个组的几率之和',
  `物品成长_系统` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT '' COMMENT '关联mod-item-growth模块的组字段，多个组用逗号隔开',
  `成长属性最小数量` int(11) NOT NULL DEFAULT '0' COMMENT '成长属性最小条目数量',
  `成长属性最大数量` int(11) NOT NULL DEFAULT '0' COMMENT '成长属性最大条目数量',
  `成长属性最小属性值` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '成长属性单条最小属性值',
  `成长属性最大属性值` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '成长属性单条最大属性值',
  `物品强化_系统` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT '' COMMENT '关联mod-item-enhancement模块的组字段，多个组用逗号隔开',
  `强化属性最小数量` int(11) NOT NULL DEFAULT '0' COMMENT '强化属性最小条目数量',
  `强化属性最大数量` int(11) NOT NULL DEFAULT '0' COMMENT '强化属性最大条目数量',
  `强化属性最小属性值` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '强化属性单条最小属性值',
  `强化属性最大属性值` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '强化属性单条最大属性值',
  `物品属性_模板` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT '' COMMENT '关联mod-item-attributes模块的组字段，多个组用逗号隔开',
  `基础属性最小数量` int(11) NOT NULL DEFAULT '0',
  `基础属性最大数量` int(11) NOT NULL DEFAULT '0',
  `基础最小属性值` bigint(20) unsigned NOT NULL DEFAULT '0',
  `基础最大属性值` bigint(20) unsigned NOT NULL DEFAULT '0',
  `基础属性允许重复` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0可以重复获取；1不可重复获取',
  `物品属性_模板_组` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '关联mod-item-attributes模块的组字段，多个组用逗号隔开，按顺序每个组取一个',
  `追加属性最小数量` int(11) NOT NULL DEFAULT '0',
  `追加属性最大数量` int(11) NOT NULL DEFAULT '0',
  `追加属性最小值` bigint(20) unsigned NOT NULL DEFAULT '0',
  `追加属性最大值` bigint(20) unsigned NOT NULL DEFAULT '0',
  `追加属性允许重复` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0可以重复获取；1不可重复获取',
  `物品技能_模板_组` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '关联mod-item-skills模块的组字段，多个组用逗号隔开，按顺序每个组取一个',
  `追加技能最小数量` int(11) NOT NULL DEFAULT '0',
  `追加技能最大数量` int(11) NOT NULL DEFAULT '0',
  `追加技能允许重复` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0可以重复获取；1不可重复获取',
  `需求_模板` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联旧核心__模版需求总表的序号',
  `符文系统_符文` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '关联mod-rune-system模块的组字段，多个组用逗号隔开',
  `符文凹槽最小数量` int(10) unsigned NOT NULL DEFAULT '0',
  `符文凹槽最大数量` int(10) unsigned NOT NULL DEFAULT '0',
  `技能模板_套装_组` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '关联mod-item-sets模块套装系统表的组字段，多个组用逗号隔开',
  `套装最小数量` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '随机候选套装组最小数量',
  `套装最大数量` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '随机候选套装组最大数量',
  `套装允许重复` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0可以重复获取；1不可重复获取',
  `公告模板` int(10) unsigned DEFAULT '0' COMMENT '成功后公告',
  `品质颜色` varchar(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '前缀+物品名+后缀 整体默认颜色，支持|cAARRGGBB、AARRGGBB或RRGGBB',
  `物品名字前缀` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '显示在物品名称前面的前缀',
  `物品名字后缀` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '显示在物品名称后面的后缀',
  `物品名字颜色_多个逗号隔开` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '按完整显示名称逐字着色，多个颜色用逗号分隔；只有1个颜色则整串同色，支持|cAARRGGBB、AARRGGBB或RRGGBB',
  `物品底部描述` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT '显示在提示底部的额外描述',
  `回收奖励模板` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联__模版奖励总表，主动回收该实例时发放',
  `售卖奖励模板` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联__模版奖励总表，成功售给商人后发放',
  `随机附魔组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联_装备超级随机附魔组表.组号',
  `随机附魔条数` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '鉴定时最多抽取的附魔数量，最大5',
  `装备位置限制` varchar(255) COLLATE utf8mb4_general_ci NOT NULL DEFAULT '' COMMENT 'item_template.InventoryType，多个用逗号或#分隔，空值为全部',
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci ROW_FORMAT=DYNAMIC COMMENT='物品鉴定模板';

CREATE TABLE IF NOT EXISTS `_每日活跃度奖励表` (
  `达到活跃值` int(11) NOT NULL COMMENT '达到的活跃值就有奖励 每日清空',
  `奖励模版` int(11) NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '每日活跃度奖励表的备注',
  PRIMARY KEY (`达到活跃值`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_每日活跃度配置表` (
  `序号` int(11) NOT NULL COMMENT '跟显示的排序有关 总项目不要超过240',
  `活跃类型` enum('击杀怪物','收集物品','完成任务','击杀玩家','赢得战场') NOT NULL DEFAULT '击杀怪物' COMMENT '击杀玩家/赢得战场 变量1=0 变量2=数量',
  `变量1` int(11) NOT NULL DEFAULT '0' COMMENT '变量1',
  `变量2` int(11) NOT NULL DEFAULT '1' COMMENT '变量2',
  `奖励活跃值` int(11) NOT NULL DEFAULT '1' COMMENT '奖励活跃值',
  `提示信息` varchar(255) NOT NULL DEFAULT '无提示' COMMENT '用于玩家参考怎么达成这个活跃度的提示',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '每日活跃度配置表的备注',
  PRIMARY KEY (`序号`,`活跃类型`,`变量1`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_比武大会奖励` (
  `名次` tinyint(11) NOT NULL DEFAULT '0' COMMENT '名次',
  `礼包物品` int(8) NOT NULL DEFAULT '0' COMMENT '礼包物品',
  `附加光环` int(8) NOT NULL DEFAULT '0' COMMENT '附加光环',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '比武大会奖励的备注',
  PRIMARY KEY (`名次`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_比武大会配置` (
  `序号` int(11) NOT NULL COMMENT '比武大会配置的序号',
  `配置说明` varchar(255) NOT NULL DEFAULT '' COMMENT '比武大会配置的配置说明',
  `配置值` varchar(255) NOT NULL DEFAULT '' COMMENT '比武大会配置的配置值',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '比武大会配置的备注',
  PRIMARY KEY (`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_游戏模式` (
  `ID` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '记录ID',
  `模式类型` varchar(50) NOT NULL COMMENT '模式名称',
  `物品ID` varchar(255) DEFAULT '' COMMENT '奖励ID，多个用#分隔',
  `物品数量` varchar(255) DEFAULT '' COMMENT '奖励数量，多个用#分隔',
  `圆圈类型` varchar(255) DEFAULT '' COMMENT '圆圈类型。2=绿色,3=蓝色,4=紫色,5=橙色,6=金色,7=银色',
  `奖励类型` varchar(255) DEFAULT '' COMMENT '奖励类型。1=物品,2=法术!多个用#分隔',
  `开启消耗` int(10) unsigned DEFAULT '0' COMMENT '开启模式消耗金币（单位：金）',
  `关闭消耗` int(10) unsigned DEFAULT '0' COMMENT '关闭模式消耗金币（单位：金）',
  `启用状态` tinyint(3) unsigned DEFAULT '1' COMMENT '启用状态',
  `备注` varchar(255) DEFAULT NULL COMMENT '游戏模式的备注',
  PRIMARY KEY (`ID`) USING BTREE,
  UNIQUE KEY `idx_mode_type` (`模式类型`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_游戏模式轮回配置表` (
  `ID` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '记录ID',
  `光环ID` int(10) unsigned NOT NULL COMMENT '会员光环ID，0表示无会员',
  `轮回次数` int(10) unsigned DEFAULT '3' COMMENT '允许的轮回次数',
  `启用状态` tinyint(3) unsigned DEFAULT '1' COMMENT '0=禁用，1=启用',
  PRIMARY KEY (`ID`) USING BTREE,
  UNIQUE KEY `idx_aura` (`光环ID`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC COMMENT='游戏模式轮回次数配置表';

CREATE TABLE IF NOT EXISTS `_物品使用命令表` (
  `物品编号` int(11) unsigned NOT NULL COMMENT '物品ID',
  `需求模版` int(11) NOT NULL COMMENT '需求模版',
  `执行命令组` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开 可多组',
  `随机命令组` varchar(255) DEFAULT NULL COMMENT '以#隔开 随机里面的一个',
  `是否消耗` int(11) unsigned NOT NULL DEFAULT '1' COMMENT '是否消耗',
  `需求模板` int(11) NOT NULL DEFAULT '0' COMMENT '需求模板',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品升级选择线表` (
  `原物品编号` int(11) NOT NULL COMMENT '原物品编号',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `是否保留附魔宝石` int(11) NOT NULL DEFAULT '0' COMMENT '是否保留附魔宝石',
  `新物品编号1` int(11) NOT NULL DEFAULT '0' COMMENT '新物品编号1',
  `成功率1` float NOT NULL DEFAULT '5' COMMENT '成功率1',
  `新物品编号2` int(11) NOT NULL DEFAULT '0' COMMENT '新物品编号2',
  `成功率2` float NOT NULL DEFAULT '5' COMMENT '成功率2',
  `新物品编号3` int(11) NOT NULL DEFAULT '0' COMMENT '新物品编号3',
  `成功率3` float NOT NULL DEFAULT '5' COMMENT '成功率3',
  `幸运宝石` int(11) NOT NULL DEFAULT '0' COMMENT '幸运宝石',
  `幸运几率` float NOT NULL DEFAULT '1' COMMENT '每个幸运宝石增加的几率',
  `幸运数量` int(11) NOT NULL DEFAULT '1' COMMENT '幸运数量',
  `防爆宝石` int(11) NOT NULL DEFAULT '0' COMMENT '防爆宝石',
  `是否碎裂` int(11) NOT NULL DEFAULT '0' COMMENT '0=不碎裂 1=碎裂',
  PRIMARY KEY (`原物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品升级随机线表` (
  `原物品编号` int(11) NOT NULL COMMENT '原物品编号',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `是否保留附魔宝石` int(11) NOT NULL DEFAULT '0' COMMENT '是否保留附魔宝石',
  `新物品编号1` int(11) NOT NULL DEFAULT '0' COMMENT '新物品编号1',
  `成功率1` float NOT NULL DEFAULT '5' COMMENT '成功率1',
  `新物品编号2` int(11) NOT NULL DEFAULT '0' COMMENT '新物品编号2',
  `成功率2` float NOT NULL DEFAULT '5' COMMENT '成功率2',
  `新物品编号3` int(11) NOT NULL DEFAULT '0' COMMENT '新物品编号3',
  `成功率3` float NOT NULL DEFAULT '5' COMMENT '成功率3',
  `幸运宝石` int(11) NOT NULL DEFAULT '0' COMMENT '幸运宝石',
  `幸运几率` float NOT NULL DEFAULT '1' COMMENT '每个幸运宝石增加的几率',
  `幸运数量` int(11) NOT NULL DEFAULT '1' COMMENT '幸运数量',
  PRIMARY KEY (`原物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品召唤表` (
  `物品编号` int(11) unsigned NOT NULL COMMENT '物品编号',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `召唤编号` int(11) NOT NULL DEFAULT '0' COMMENT '正数生物 负数物体',
  `提示语` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '提示语',
  `持续时间` int(11) NOT NULL DEFAULT '0' COMMENT '大于0 = 秒',
  `是否消耗` int(11) unsigned NOT NULL DEFAULT '1' COMMENT '是否消耗',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品名字额外描述表` (
  `物品编号` int(11) NOT NULL COMMENT '物品编号',
  `文本描述` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '文本描述',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=gbk ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品属性奖励表` (
  `物品编号` int(11) NOT NULL COMMENT '使用类型的',
  `是否消耗` smallint(1) NOT NULL DEFAULT '1' COMMENT '0=不消耗',
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
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品普通升级合成表` (
  `原物品编号` int(11) NOT NULL COMMENT '原物品编号',
  `新物品编号` int(11) NOT NULL DEFAULT '0' COMMENT '新物品编号',
  `合成模式` int(11) NOT NULL DEFAULT '0' COMMENT '0=普通 1=1合1 2=2合1 以此类推',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `是否保留附魔宝石` smallint(11) NOT NULL DEFAULT '0' COMMENT '是否保留附魔宝石',
  `是否保留超级武器伤害` smallint(11) NOT NULL DEFAULT '1' COMMENT '是否保留超级武器伤害',
  `是否保留超级属性` smallint(11) NOT NULL DEFAULT '1' COMMENT '是否保留超级属性',
  `是否保留超级技能` smallint(11) NOT NULL DEFAULT '1' COMMENT '是否保留超级技能',
  `成功率` float NOT NULL DEFAULT '5' COMMENT '成功率1',
  `幸运宝石` int(11) NOT NULL DEFAULT '0' COMMENT '幸运宝石',
  `幸运几率` float NOT NULL DEFAULT '1' COMMENT '每个幸运宝石增加的几率',
  `幸运数量` int(11) NOT NULL DEFAULT '1' COMMENT '幸运数量',
  `防爆宝石` int(11) NOT NULL DEFAULT '0' COMMENT '防爆宝石',
  `是否碎裂` smallint(11) NOT NULL DEFAULT '0' COMMENT '0=不碎裂 1=碎裂',
  PRIMARY KEY (`原物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品玩家使用次数表` (
  `物品编号` int(11) NOT NULL COMMENT '物品编号',
  `每日次数` int(11) NOT NULL DEFAULT '0' COMMENT '每日次数',
  `永久次数` int(11) NOT NULL DEFAULT '0' COMMENT '永久次数',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品背包银行获得属性表` (
  `物品编号` int(11) unsigned NOT NULL COMMENT '物品ID',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品脚本消耗表` (
  `物品编号` int(11) NOT NULL COMMENT '物品编号',
  `是否消耗` smallint(1) NOT NULL DEFAULT '1' COMMENT '0=不消耗',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT '' COMMENT '物品脚本消耗表的备注',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品获取限制表` (
  `物品编号` int(11) NOT NULL COMMENT '物品编号',
  `角色每日上限` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制',
  `角色永久上限` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制',
  `账号每日上限` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制',
  `账号永久上限` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品装备时需求表` (
  `物品编号` int(11) unsigned NOT NULL COMMENT '物品ID',
  `需求模板` int(11) NOT NULL DEFAULT '0' COMMENT '需求模板',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '备忘',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品解绑表` (
  `物品编号` int(11) NOT NULL COMMENT '物品编号',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT '' COMMENT '物品解绑表的备注',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品购买或出售表` (
  `物品编号` int(11) NOT NULL DEFAULT '0' COMMENT '正数=购买 负数=出售',
  `商人编号` int(11) NOT NULL DEFAULT '0' COMMENT '0=忽略 对应商人编号',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '正数=购买 负数=出售',
  `奖励模版` int(11) NOT NULL DEFAULT '0' COMMENT '奖励模版',
  PRIMARY KEY (`物品编号`,`商人编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品附魔提取转移表` (
  `物品编号` int(11) NOT NULL COMMENT '物品编号',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT '' COMMENT '物品附魔提取转移表的备注',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品随机附魔指定表` (
  `物品编号` int(11) NOT NULL COMMENT '用于类似卷轴选中的物品',
  `装备编号` int(11) NOT NULL DEFAULT '0' COMMENT '装备编号',
  `附魔1几率` float NOT NULL DEFAULT '100' COMMENT '附魔1几率',
  `附魔1组编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '附魔1组编号',
  `附魔2几率` float NOT NULL DEFAULT '100' COMMENT '附魔2几率',
  `附魔2组编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '附魔2组编号',
  `附魔3几率` float NOT NULL DEFAULT '100' COMMENT '附魔3几率',
  `附魔3组编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '附魔3组编号',
  `附魔4几率` float NOT NULL DEFAULT '100' COMMENT '附魔4几率',
  `附魔4组编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '附魔4组编号',
  `附魔5几率` float NOT NULL DEFAULT '100' COMMENT '附魔5几率',
  `附魔5组编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '附魔5组编号',
  PRIMARY KEY (`物品编号`,`装备编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品随机附魔控制表` (
  `物品编号` int(11) unsigned NOT NULL COMMENT '物品编号',
  `附魔1几率` float NOT NULL DEFAULT '100' COMMENT '附魔1几率',
  `附魔1组编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '附魔1组编号',
  `附魔2几率` float NOT NULL DEFAULT '100' COMMENT '附魔2几率',
  `附魔2组编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '附魔2组编号',
  `附魔3几率` float NOT NULL DEFAULT '100' COMMENT '附魔3几率',
  `附魔3组编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '附魔3组编号',
  `附魔4几率` float NOT NULL DEFAULT '100' COMMENT '附魔4几率',
  `附魔4组编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '附魔4组编号',
  `附魔5几率` float NOT NULL DEFAULT '100' COMMENT '附魔5几率',
  `附魔5组编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '附魔5组编号',
  `洗练次数` int(11) NOT NULL DEFAULT '0' COMMENT '洗练次数',
  `洗练需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '洗练需求模版',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_物品随机附魔组表` (
  `组编号` int(11) unsigned NOT NULL COMMENT '组编号',
  `附魔编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '附魔编号',
  `几率` float NOT NULL DEFAULT '100' COMMENT '物品随机附魔组表的几率',
  PRIMARY KEY (`组编号`,`附魔编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_玩家光环杀怪掉落表` (
  `光环编号` int(11) NOT NULL COMMENT '光环编号',
  `掉落物品1` float NOT NULL DEFAULT '0' COMMENT '掉落物品1',
  `掉落数量1` float NOT NULL DEFAULT '0' COMMENT '掉落数量1',
  `掉落物品2` float NOT NULL DEFAULT '0' COMMENT '掉落物品2',
  `掉落数量2` float NOT NULL DEFAULT '0' COMMENT '掉落数量2',
  `掉落几率` float NOT NULL DEFAULT '0' COMMENT '掉落几率',
  PRIMARY KEY (`光环编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_玩家击杀玩家奖励表` (
  `绑定事件编号` int(11) NOT NULL DEFAULT '0' COMMENT '事件=0则表示 不绑定事件也触发',
  `地图或区域` int(11) NOT NULL DEFAULT '-1' COMMENT '正数地图 负数区域 -1 = 所有',
  `击杀者奖励几率` float NOT NULL DEFAULT '100' COMMENT '击杀者奖励几率',
  `击杀者奖励模版` int(11) NOT NULL DEFAULT '0' COMMENT '击杀者奖励模版',
  `被杀者奖励几率` float NOT NULL DEFAULT '100' COMMENT '被杀者奖励几率',
  `被杀者奖励模版` int(11) NOT NULL DEFAULT '0' COMMENT '被杀者奖励模版',
  `同IP判定` smallint(1) NOT NULL DEFAULT '1' COMMENT '0=不判定 用于方便单机测试',
  PRIMARY KEY (`绑定事件编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_玩家法宝技能表` (
  `物品编号` int(11) NOT NULL COMMENT '物品编号',
  `法宝类型` enum('金','木','水','火','土','天','地','玄') NOT NULL DEFAULT '金' COMMENT '法宝类型',
  `技能编号` int(11) NOT NULL DEFAULT '0' COMMENT '技能编号',
  `是否主动被动` smallint(1) NOT NULL DEFAULT '0' COMMENT '0=上光环 1=学会技能(在会出现在动作条里的主动或被动)',
  `是否绑定` smallint(1) NOT NULL DEFAULT '1' COMMENT '=1 表示取出会绑定',
  `全局限制数量` int(11) NOT NULL DEFAULT '1' COMMENT '0=不限制',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '玩家法宝技能表的备注',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_玩家法宝类型激活表` (
  `法宝类型` enum('金','木','水','火','土','天','地','玄') NOT NULL DEFAULT '金' COMMENT '法宝类型',
  `激活光环` int(11) NOT NULL DEFAULT '0' COMMENT '激活光环',
  `是否主动被动` smallint(1) NOT NULL DEFAULT '0' COMMENT '0=上光环 1=学会技能(在会出现在动作条里的主动或被动)',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '玩家法宝类型激活表的备注',
  PRIMARY KEY (`法宝类型`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_玩家职业属性调整表` (
  `职业编号` int(11) unsigned NOT NULL COMMENT '职业ID',
  `属性模版` int(11) NOT NULL DEFAULT '0' COMMENT '属性模版',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '玩家职业属性调整表的备注',
  PRIMARY KEY (`职业编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_玩家被玩家击杀掉落表` (
  `物品编号` int(11) NOT NULL DEFAULT '0' COMMENT '物品编号',
  `概率` float NOT NULL DEFAULT '100' COMMENT '概率',
  `最小数量` int(11) NOT NULL DEFAULT '1' COMMENT '最小数量',
  `最大数量` int(11) NOT NULL DEFAULT '1' COMMENT '最大数量',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_生物伤害排行榜表` (
  `生物编号` int(11) unsigned NOT NULL COMMENT '生物编号',
  `第1名` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `第2名` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `第3名` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `第4名` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `第5名` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `第6名` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `第7名` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `第8名` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `第9名` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `第10名` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `参与` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `最低伤害要求` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '最低伤害要求',
  `IP地址数量限制` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '必须不能超过此才能奖励',
  PRIMARY KEY (`生物编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_生物击杀特殊配置表` (
  `生物编号` int(11) unsigned NOT NULL COMMENT '生物编号',
  `固定命令组` varchar(255) NOT NULL DEFAULT '' COMMENT '多个的话以#隔开',
  `随机命令组` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开 随机里面的一个',
  `随机召唤对象组` varchar(255) NOT NULL DEFAULT '' COMMENT '1#0|-2#1|\r\
以#号区分生物物体和持续时间 以|号区分另一组\r\
#前面表示 正数生物 负数物体\r\
#后面表示持续时间 0表示死亡才消失\r\
随机以 | 隔开的组',
  `奖励模版` int(11) NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `奖励几率` float NOT NULL DEFAULT '100' COMMENT '奖励几率',
  `随机召唤生物坐标组` int(11) NOT NULL DEFAULT '0' COMMENT '随机召唤生物坐标组',
  `随机召唤数量` int(11) NOT NULL DEFAULT '1' COMMENT '表示从坐标组里面抽取的数量',
  `随机召唤几率` float NOT NULL DEFAULT '100' COMMENT '随机召唤几率',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '备注',
  PRIMARY KEY (`生物编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_生物击杀特殊随机坐标组表` (
  `坐标组编号` int(11) unsigned NOT NULL COMMENT '坐标组编号',
  `生物编号` int(11) NOT NULL DEFAULT '0' COMMENT '生物编号',
  `模版坐标编号` int(11) NOT NULL DEFAULT '0' COMMENT '模版坐标编号',
  `持续时间` int(11) NOT NULL DEFAULT '30' COMMENT '0=永久 以秒为单位',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '备注',
  PRIMARY KEY (`坐标组编号`,`生物编号`,`模版坐标编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_生物控制组表` (
  `组号` int(11) unsigned NOT NULL COMMENT '组号',
  `属性模版编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '0=官方',
  `模型编号` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '0=官方',
  `模型大小` float NOT NULL DEFAULT '0' COMMENT '0=官方',
  `武器1` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '0=官方',
  `武器2` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '0=官方',
  `武器3` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '0=官方',
  `几率` float NOT NULL DEFAULT '0' COMMENT '生物控制组表的几率',
  PRIMARY KEY (`组号`,`属性模版编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_生物自定义售卖表` (
  `生物编号` mediumint(8) unsigned NOT NULL DEFAULT '0' COMMENT '生物编号（对应 creature_template.entry）',
  `排序位置` smallint(6) NOT NULL DEFAULT '0' COMMENT '排序位置',
  `物品编号` mediumint(9) NOT NULL DEFAULT '0' COMMENT '物品编号（对应 item_template.entry）',
  `最大库存数量` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '最大库存数量，0为无限',
  `刷新时间` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '刷新时间（秒）',
  `扩展消耗编号` mediumint(8) unsigned NOT NULL DEFAULT '0' COMMENT '扩展消耗编号（对应 ItemExtendedCostEntry）',
  `需求模版` mediumint(8) unsigned NOT NULL DEFAULT '0' COMMENT '关联 __模版需求总表.模版编号，0表示无额外需求',
  `是否回购` tinyint(1) unsigned NOT NULL DEFAULT '1' COMMENT '是否允许回购，1=允许，0=禁止',
  PRIMARY KEY (`生物编号`,`排序位置`,`物品编号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='生物自定义售卖表，可在线热加载';

CREATE TABLE IF NOT EXISTS `_生物自定义掉落表` (
  `序号` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '_生物自定义掉落表的序号',
  `类型` enum('生物','物品','物体') NOT NULL COMMENT '掉落来源类型',
  `来源编号` int(10) unsigned NOT NULL COMMENT '生物、物品或物体编号',
  `物品编号` int(10) unsigned NOT NULL COMMENT '掉落的物品编号',
  `几率` float NOT NULL DEFAULT '100' COMMENT '掉落几率0-100',
  `最小数量` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '最小数量',
  `最大数量` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '最大数量',
  `允许多倍` enum('关闭','开启') NOT NULL DEFAULT '关闭' COMMENT '允许多倍',
  PRIMARY KEY (`序号`),
  KEY `来源索引` (`类型`,`来源编号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='生物自定义掉落';

CREATE TABLE IF NOT EXISTS `_生物自定义智能表` (
  `生物编号` int(11) NOT NULL DEFAULT '0' COMMENT '生物编号',
  `事件类型` enum('进入战斗','杀死玩家','怪物死亡','血量低于90%','血量低于75%','血量低于50%','血量低于25%','血量低于10%','重置AI') CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '进入战斗' COMMENT '事件类型',
  `事件阶段` int(11) NOT NULL DEFAULT '0' COMMENT '事件阶段',
  `延迟时间` int(11) NOT NULL DEFAULT '0' COMMENT '延迟时间',
  `最小重复时间` int(11) NOT NULL DEFAULT '0' COMMENT '最小重复时间',
  `最大重复时间` int(11) NOT NULL DEFAULT '0' COMMENT '最大重复时间',
  `动作类型` enum('激活或使失活','设定阶段','释放技能','移动范围','召唤','说话') CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '释放技能' COMMENT '动作类型',
  `动作参数1` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '动作参数1',
  `动作参数2` int(11) NOT NULL DEFAULT '0' COMMENT '动作参数2',
  `动作参数3` int(11) DEFAULT '0' COMMENT '动作参数3',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '生物自定义智能表的备注',
  PRIMARY KEY (`生物编号`,`事件类型`,`事件阶段`,`延迟时间`,`最小重复时间`,`最大重复时间`,`动作类型`,`动作参数1`,`动作参数2`) USING BTREE,
  UNIQUE KEY `entry` (`生物编号`,`事件类型`,`事件阶段`,`延迟时间`,`最小重复时间`,`最大重复时间`,`动作类型`,`动作参数1`,`动作参数2`,`备注`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_神兽翻牌品质配置` (
  `卡牌编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0为全局默认，1-72为单卡覆盖',
  `品质等级` tinyint(3) unsigned NOT NULL COMMENT '0D灰 1C白 2B绿 3A蓝 4S紫 5SS橙 6SSR暗金 7SSS红 8SR粉 9UR彩色',
  `是否启用` tinyint(3) unsigned NOT NULL DEFAULT '1' COMMENT '0关闭该品质升级，1开启',
  `升级需求模板ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0沿用卡牌翻牌需求模板',
  `属性倍率` smallint(5) unsigned NOT NULL DEFAULT '100' COMMENT '相对基础激活属性的百分比倍率',
  `技能ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '效果类型2/4的目标技能；0表示全部技能',
  `效果类型` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '0无 1固定属性 2技能固定 3百分比属性 4技能百分比',
  `效果值` varchar(640) NOT NULL DEFAULT '' COMMENT '多属性格式4 50,7 100；技能格式99 10',
  `效果描述` varchar(640) NOT NULL DEFAULT '' COMMENT '客户端品质效果区域显示；空值时服务端自动生成',
  PRIMARY KEY (`卡牌编号`,`品质等级`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='神兽翻牌品质升级配置';

CREATE TABLE IF NOT EXISTS `_神兽翻牌羁绊配置` (
  `羁绊编号` int(10) unsigned NOT NULL COMMENT '羁绊唯一编号',
  `羁绊名称` varchar(64) NOT NULL DEFAULT '' COMMENT '客户端显示名称',
  `卡牌组合` varchar(640) NOT NULL DEFAULT '' COMMENT '卡牌编号以#分隔，例如1#2#3',
  `激活数量` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '组合中需要翻开的卡牌数量，0表示全部',
  `技能ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '效果类型2/4的目标技能；0表示全部技能',
  `效果类型` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '0无 1固定属性 2技能固定 3百分比属性 4技能百分比',
  `效果值` varchar(640) NOT NULL DEFAULT '' COMMENT '多属性格式4 50,7 100；技能格式99 10',
  `效果描述` varchar(640) NOT NULL DEFAULT '' COMMENT '客户端羁绊面板显示；空值时服务端自动生成',
  `是否启用` tinyint(3) unsigned NOT NULL DEFAULT '1' COMMENT '0关闭，1开启',
  `排序` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '客户端显示顺序',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '神兽翻牌羁绊配置的备注',
  PRIMARY KEY (`羁绊编号`),
  KEY `idx_启用排序` (`是否启用`,`排序`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='神兽翻牌羁绊配置';

CREATE TABLE IF NOT EXISTS `_神兽翻牌配置` (
  `卡牌编号` int(10) unsigned NOT NULL COMMENT '客户端素材编号，范围 1-72',
  `卡牌名称` varchar(64) NOT NULL DEFAULT '' COMMENT '卡牌名称',
  `卡牌介绍` varchar(640) NOT NULL DEFAULT '' COMMENT '卡牌介绍',
  `是否启用` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '0关闭，1开启',
  `需求模板ID` int(11) NOT NULL DEFAULT '-1' COMMENT '大于0启用需求条件；-1或0表示不使用需求条件',
  `奖励模板ID` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联 __模版奖励总表.模版编号，必须有效',
  `击杀生物编号` int(11) NOT NULL DEFAULT '-1' COMMENT '大于0且需要击杀数量大于0时启用击杀条件；-1或0表示不使用击杀条件',
  `需要击杀数量` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '需要击杀数量',
  `激活后奖励属性编号` smallint(5) unsigned NOT NULL DEFAULT '1' COMMENT '激活后奖励属性编号',
  `激活后奖励属性数值` int(11) NOT NULL DEFAULT '0' COMMENT '激活后奖励属性数值',
  `属性模式` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '0固定值，1百分比',
  PRIMARY KEY (`卡牌编号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='神兽翻牌配置';

CREATE TABLE IF NOT EXISTS `_签到奖励表` (
  `日期` int(11) NOT NULL COMMENT '表示 1-31号',
  `物品1` int(11) NOT NULL DEFAULT '0' COMMENT '物品1',
  `数量1` int(11) NOT NULL DEFAULT '0' COMMENT '数量1',
  `物品2` int(11) NOT NULL DEFAULT '0' COMMENT '物品2',
  `数量2` int(11) NOT NULL DEFAULT '0' COMMENT '数量2',
  `物品3` int(11) NOT NULL DEFAULT '0' COMMENT '物品3',
  `数量3` int(11) NOT NULL DEFAULT '0' COMMENT '数量3',
  `物品4` int(11) NOT NULL DEFAULT '0' COMMENT '物品4',
  `数量4` int(11) NOT NULL DEFAULT '0' COMMENT '数量4',
  `物品5` int(11) NOT NULL DEFAULT '0' COMMENT '物品5',
  `数量5` int(11) NOT NULL DEFAULT '0' COMMENT '数量5',
  `物品6` int(11) NOT NULL DEFAULT '0' COMMENT '物品6',
  `数量6` int(11) NOT NULL DEFAULT '0' COMMENT '数量6',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '签到奖励表的备注',
  PRIMARY KEY (`日期`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_综合商人表` (
  `生物编号` int(11) NOT NULL COMMENT '生物编号',
  `商人编号` int(11) NOT NULL COMMENT '商人编号',
  `排序` int(11) NOT NULL DEFAULT '0' COMMENT '排序',
  `显示需求模版` int(10) unsigned DEFAULT '0' COMMENT '显示需求模版ID（只做判断不扣除，满足才显示菜单）',
  `显示的菜单图标` varchar(255) NOT NULL DEFAULT '' COMMENT '显示的菜单图标',
  `阵营` int(11) NOT NULL DEFAULT '0' COMMENT '1=联盟 2=部落',
  `职业掩码` int(11) NOT NULL DEFAULT '0' COMMENT '0=不限制 参考1=战士2=骑士4=猎人8=盗贼16=牧师32=死骑64=萨满128=法师256=术士1024=德鲁伊',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '综合商人表的备注',
  PRIMARY KEY (`生物编号`,`商人编号`,`排序`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_虚拟名字列表` (
  `名字` varchar(255) NOT NULL DEFAULT '' COMMENT '名字',
  `公会` varchar(255) NOT NULL DEFAULT '' COMMENT '公会',
  `等级` int(11) NOT NULL DEFAULT '0' COMMENT '虚拟名字列表的等级',
  `职业` int(11) NOT NULL DEFAULT '0' COMMENT '职业',
  `种族` int(11) NOT NULL DEFAULT '0' COMMENT '种族',
  `地区` int(11) NOT NULL DEFAULT '0' COMMENT 'ZoneId',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '虚拟名字列表的备注',
  PRIMARY KEY (`名字`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备属性倍率调整` (
  `装备编号` int(11) NOT NULL COMMENT '装备编号',
  `目标装备编号` int(11) NOT NULL DEFAULT '0' COMMENT '目标装备编号',
  `属性倍率` float NOT NULL DEFAULT '0' COMMENT '属性倍率',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '装备属性倍率调整的备注',
  PRIMARY KEY (`装备编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备栏强化` (
  `序号` int(11) NOT NULL COMMENT '装备栏强化的序号',
  `等级` int(11) NOT NULL DEFAULT '1' COMMENT '装备栏强化的等级',
  `增加属性百分比` float NOT NULL DEFAULT '1' COMMENT '增加属性百分比',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT '' COMMENT '装备栏强化的备注',
  PRIMARY KEY (`序号`,`等级`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备栏拓展` (
  `全身等级` int(11) NOT NULL COMMENT '17件b不同部位以上的全部等级满足',
  `套装光环` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT '' COMMENT '以#隔开',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT '' COMMENT '装备栏拓展的备注',
  PRIMARY KEY (`全身等级`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备特殊成长使用经验卷轴表` (
  `物品成长卷轴编号` int(11) NOT NULL COMMENT '物品成长卷轴编号',
  `目标装备编号` int(11) NOT NULL DEFAULT '0' COMMENT '0表示所有',
  `卷轴经验值Min` int(11) NOT NULL DEFAULT '1' COMMENT '经验值',
  `卷轴经验值Max` int(11) NOT NULL DEFAULT '1' COMMENT '经验值',
  `备注` varchar(11) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '备注',
  PRIMARY KEY (`物品成长卷轴编号`,`目标装备编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备特殊成长经验配置表` (
  `装备编号` int(11) NOT NULL COMMENT '装备编号',
  `成长等级` int(11) NOT NULL DEFAULT '1' COMMENT '装备的成长等级 和 装备编号 属于同主键',
  `需要成长经验` int(11) NOT NULL DEFAULT '50' COMMENT '提升到该等级需要的成长经验',
  `满足经验后物品编号` int(11) NOT NULL DEFAULT '0' COMMENT '经验达成后的物品编号 如果=0则指向 _装备超级随机模板总表',
  `满足经验后超级模版` int(11) NOT NULL DEFAULT '0' COMMENT '指向 _装备超级随机模板总表',
  `地图类型` int(11) NOT NULL DEFAULT '1' COMMENT '1=地图编号 2=ZoneId 3=AreaId -1=所有',
  `地图类型编号` int(11) NOT NULL DEFAULT '-1' COMMENT '根据地图类型的编号',
  `击杀普通怪物经验值Min` int(11) NOT NULL DEFAULT '1' COMMENT '普通怪物的经验值(普通就是除了BOSS以外)',
  `击杀普通怪物经验值Max` int(11) NOT NULL DEFAULT '1' COMMENT '普通怪物的经验值(普通就是除了BOSS以外)',
  `击杀BOSS经验值Min` int(11) NOT NULL DEFAULT '1' COMMENT 'BOSS的经验值',
  `击杀BOSS经验值Max` int(11) NOT NULL DEFAULT '1' COMMENT 'BOSS的经验值',
  `指定怪物编号` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '#隔开',
  `指定怪物经验值Min` int(11) NOT NULL DEFAULT '1' COMMENT '经验值',
  `指定怪物经验值Max` int(11) NOT NULL DEFAULT '1' COMMENT '经验值',
  `成长设置选项` int(11) NOT NULL DEFAULT '2' COMMENT '=1为背包内获取经验 =2 穿戴才能获取经验 =3为背包和穿戴都能获取',
  `备注` varchar(11) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '备注',
  PRIMARY KEY (`装备编号`,`成长等级`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备符文宝石表` (
  `宝石编号` int(11) NOT NULL COMMENT '宝石编号',
  `自带技能` int(11) NOT NULL DEFAULT '0' COMMENT '自带技能',
  `主动或被动` smallint(1) NOT NULL DEFAULT '0' COMMENT '0=被动 1=主动',
  `插入该宝石需求模板` int(11) NOT NULL DEFAULT '0' COMMENT '插入该宝石需求模板',
  `拆除该宝石需求模板` int(11) NOT NULL DEFAULT '0' COMMENT '拆除该宝石需求模板',
  `插入后是否绑定` smallint(1) NOT NULL DEFAULT '1' COMMENT '1=绑定 0=不绑定',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '装备符文宝石表的备注',
  PRIMARY KEY (`宝石编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备符文开槽表` (
  `装备编号` int(11) NOT NULL COMMENT '装备编号',
  `需求模板` int(11) NOT NULL DEFAULT '0' COMMENT '需求模板',
  `开槽成功率` float NOT NULL DEFAULT '100' COMMENT '开槽成功率',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '装备符文开槽表的备注',
  PRIMARY KEY (`装备编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备符文技能组合表` (
  `组合编号` int(11) NOT NULL COMMENT '组合编号',
  `序号` int(11) NOT NULL DEFAULT '1' COMMENT '作为键区别 顺序填写即可',
  `组合效果名称` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '组合效果名称',
  `符文宝石1` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石1',
  `符文宝石2` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石2',
  `符文宝石3` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石3',
  `符文宝石4` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石4',
  `符文宝石5` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石5',
  `符文宝石6` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石6',
  `符文宝石7` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石7',
  `符文宝石8` int(11) NOT NULL DEFAULT '0' COMMENT '符文宝石8',
  `技能列表` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '以#隔开',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '装备符文技能组合表的备注',
  PRIMARY KEY (`组合编号`,`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备符文配置表` (
  `装备编号` int(11) NOT NULL COMMENT '装备编号',
  `符文插槽最少数量` int(11) NOT NULL DEFAULT '1' COMMENT '最多8',
  `符文插槽最多数量` int(11) NOT NULL DEFAULT '8' COMMENT '最多8',
  `符文组合编号` int(11) NOT NULL DEFAULT '0' COMMENT '符文组合编号',
  `最多可开槽` int(11) NOT NULL DEFAULT '1' COMMENT '最多8',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '装备符文配置表的备注',
  PRIMARY KEY (`装备编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备超级强化售卖奖励表` (
  `装备编号` int(11) NOT NULL COMMENT '装备编号',
  `等级` int(11) NOT NULL DEFAULT '0' COMMENT '装备超级强化售卖奖励表的等级',
  `奖励模版` int(11) NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `备注` varchar(11) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '备注',
  PRIMARY KEY (`装备编号`,`等级`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备超级随机属性倍率` (
  `序号` int(10) unsigned NOT NULL AUTO_INCREMENT COMMENT '自增主键',
  `属性倍率组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '属性倍率组号，对应生成总表的属性倍率组',
  `属性类型` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '属性类型：3=力量,4=敏捷,5=耐力,6=智力,7=精神,38=攻击强度,45=法术强度',
  `最小倍率` bigint(20) NOT NULL DEFAULT '1' COMMENT '最小倍率',
  `最大倍率` bigint(20) NOT NULL DEFAULT '1' COMMENT '最大倍率',
  `概率` float NOT NULL DEFAULT '0' COMMENT '此属性出现的概率（0-100）',
  PRIMARY KEY (`序号`) USING BTREE,
  KEY `idx_属性倍率组` (`属性倍率组`) USING BTREE,
  KEY `idx_属性类型` (`属性类型`) USING BTREE
) ENGINE=InnoDB AUTO_INCREMENT=11 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC COMMENT='装备超级随机属性倍率配置表';

CREATE TABLE IF NOT EXISTS `_装备超级随机属性组表` (
  `组号` int(11) NOT NULL COMMENT '组号',
  `属性类型` int(11) NOT NULL DEFAULT '-1' COMMENT '同官方用法',
  `最小值` bigint(20) NOT NULL DEFAULT '0' COMMENT '最小值',
  `最大值` bigint(20) NOT NULL DEFAULT '0' COMMENT '最大值',
  `生效几率` float NOT NULL DEFAULT '100' COMMENT '生效几率',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '装备超级随机属性组表的备注',
  PRIMARY KEY (`组号`,`属性类型`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备超级随机强化总表` (
  `装备编号` int(11) NOT NULL COMMENT '装备编号',
  `等级` int(11) NOT NULL DEFAULT '0' COMMENT '装备超级随机强化总表的等级',
  `直接强化几率` float NOT NULL DEFAULT '0' COMMENT '生成时/获取时直接到达等级的几率',
  `强化百分比` float NOT NULL DEFAULT '0.1' COMMENT '0.1=10% 以此类推',
  `技能组` int(11) NOT NULL DEFAULT '0' COMMENT '默认0则不改变原来',
  `技能条数` int(11) NOT NULL DEFAULT '0' COMMENT '默认0则不改变原来',
  `需求模版` int(11) NOT NULL DEFAULT '0' COMMENT '需求模版',
  `是否保留随机附魔` int(11) NOT NULL DEFAULT '1' COMMENT '是否保留随机附魔',
  `是否保留超级技能` int(11) NOT NULL DEFAULT '0' COMMENT '若保留 则 技能组和技能条数无效 继承上一级的',
  `成功率` float NOT NULL DEFAULT '100' COMMENT '成功率',
  `失败后降级的几率` float NOT NULL DEFAULT '0' COMMENT '0则不会被降级 降级的前提是在失败后才会降级 如果你的成功率是100% 那将永远不会被降级',
  `被降低的等级` int(11) NOT NULL DEFAULT '0' COMMENT '如果原来等级是5 配置2 降级后就是3 必须注意原来的等级够降级',
  `强化后新的编号` int(11) NOT NULL DEFAULT '0' COMMENT '如果大于0 则旧的会消失 然后给新的编号 只跟成功率有关联',
  `强化名称` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '在装备名字后面的 可留空',
  `幸运宝石` int(11) NOT NULL DEFAULT '0' COMMENT '幸运宝石',
  `幸运几率` float NOT NULL DEFAULT '1' COMMENT '每个幸运宝石增加的几率',
  `幸运数量` int(11) NOT NULL DEFAULT '1' COMMENT '幸运数量',
  `防爆宝石` int(11) NOT NULL DEFAULT '0' COMMENT '防爆宝石',
  `是否碎裂` smallint(11) NOT NULL DEFAULT '0' COMMENT '0=不碎裂 1=碎裂',
  `备注` varchar(11) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '备注',
  PRIMARY KEY (`装备编号`,`等级`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备超级随机强化秒升表` (
  `使用物品编号` int(11) NOT NULL COMMENT '使用物品编号',
  `目标装备编号` int(11) NOT NULL DEFAULT '0' COMMENT '此项是 秒升的目标装备',
  `秒升等级` int(11) NOT NULL DEFAULT '1' COMMENT '秒升等级',
  `几率` float NOT NULL DEFAULT '100' COMMENT '装备超级随机强化秒升表的几率',
  `备注` varchar(11) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '备注',
  PRIMARY KEY (`使用物品编号`,`目标装备编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备超级随机技能组表` (
  `组号` int(11) NOT NULL COMMENT '组号',
  `技能编号` int(11) NOT NULL DEFAULT '0' COMMENT '技能编号',
  `生效几率` float NOT NULL DEFAULT '100' COMMENT '生效几率',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '装备超级随机技能组表的备注',
  PRIMARY KEY (`组号`,`技能编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备超级随机模板总表` (
  `模板组号` int(11) NOT NULL COMMENT '模板组号',
  `序号` float NOT NULL DEFAULT '1' COMMENT '1起顺序',
  `几率` float NOT NULL DEFAULT '100' COMMENT '装备超级随机模板总表的几率',
  `前缀名称` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '在装备名字前面的 可留空',
  `后缀名称` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '在装备名字后面的 可留空',
  `武器最小伤害Min` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '武器最小伤害Min',
  `武器最小伤害Max` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '武器最小伤害Max',
  `武器最大伤害Min` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '武器最大伤害Min',
  `武器最大伤害Max` bigint(20) unsigned NOT NULL DEFAULT '0' COMMENT '武器最大伤害Max',
  `属性组` int(11) NOT NULL DEFAULT '0' COMMENT '对应 _装备超级随机属性组表',
  `属性条数` int(11) NOT NULL DEFAULT '16' COMMENT '最大16条',
  `技能组` int(11) NOT NULL DEFAULT '0' COMMENT '对应 _装备超级随机技能组表',
  `技能条数` int(11) NOT NULL DEFAULT '8' COMMENT '最大8条',
  `随机附魔组` int(11) NOT NULL DEFAULT '0' COMMENT '对应 _装备超级随机附魔组表',
  `随机附魔条数` int(11) NOT NULL DEFAULT '5' COMMENT '最大5条',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '装备超级随机模板总表的备注',
  PRIMARY KEY (`模板组号`,`序号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备超级随机生成总表` (
  `装备编号` int(11) NOT NULL COMMENT '装备编号',
  `超级模板组号` int(11) NOT NULL DEFAULT '0' COMMENT ' 对应 _装备超级随机模板总表',
  `需求模板` int(11) NOT NULL DEFAULT '0' COMMENT ' 对应 需求模板 表示满足才能出现 需求模板不配置消耗的方式即可',
  `是否覆盖` int(11) NOT NULL DEFAULT '0' COMMENT '若覆盖 则原白绿属性 技能都无效',
  `概率` float NOT NULL DEFAULT '100' COMMENT '生成时/获取时 这个装备生效超级随机功能的概率',
  `属性倍率组` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '关联到属性倍率表的组号，0表示不使用',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '装备超级随机生成总表的备注',
  PRIMARY KEY (`装备编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备超级随机重置总表` (
  `使用物品编号` int(11) NOT NULL COMMENT '使用物品编号',
  `目标装备编号` int(11) NOT NULL DEFAULT '0' COMMENT '0=所有 大于0指向目标装备编号',
  `超级模板组号` int(11) NOT NULL DEFAULT '0' COMMENT '对应超级强化模板',
  `需求模板` int(11) NOT NULL DEFAULT '0' COMMENT '需求模板',
  `是否保留随机附魔` int(11) NOT NULL DEFAULT '1' COMMENT '是否保留随机附魔',
  `是否保留超级技能` int(11) NOT NULL DEFAULT '0' COMMENT '若保留 则 技能组和技能条数无效 继承上一级的',
  `概率` float NOT NULL DEFAULT '100' COMMENT '概率',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '装备超级随机重置总表的备注',
  PRIMARY KEY (`使用物品编号`,`目标装备编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_装备超级随机附魔组表` (
  `组号` int(11) NOT NULL COMMENT '组号',
  `附魔编号` int(11) NOT NULL DEFAULT '0' COMMENT '附魔编号',
  `生效几率` int(11) NOT NULL DEFAULT '100' COMMENT '生效几率',
  `备注` varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '' COMMENT '装备超级随机附魔组表的备注',
  PRIMARY KEY (`组号`,`附魔编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_进入副本控制表` (
  `副本地图编号` int(11) unsigned NOT NULL COMMENT '副本地图编号',
  `难度` int(11) NOT NULL DEFAULT '0' COMMENT '难度',
  `通行证` varchar(255) NOT NULL DEFAULT '' COMMENT '以#隔开',
  `需求模版` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '需求模版',
  `需要装等` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '最低装备等级总和',
  `排除类型` varchar(255) NOT NULL DEFAULT '' COMMENT '按这个位置 物品表 InventoryType',
  PRIMARY KEY (`副本地图编号`,`难度`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_进入地图或区域表` (
  `地图或区域` int(11) NOT NULL DEFAULT '0' COMMENT '正数地图 负数区域',
  `职业` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '职业',
  `属性模板` int(11) unsigned NOT NULL DEFAULT '0' COMMENT '属性模板',
  `备注` varchar(255) NOT NULL DEFAULT '' COMMENT '备忘',
  PRIMARY KEY (`地图或区域`,`职业`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_通告_击杀玩家表` (
  `地图编号` int(11) NOT NULL DEFAULT '-1' COMMENT '-1表示所有',
  `通告内容` varchar(255) NOT NULL DEFAULT '玩家 %s 击杀 玩家 %s' COMMENT '通告内容',
  PRIMARY KEY (`地图编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_通告_击杀生物表` (
  `生物编号` int(11) unsigned NOT NULL COMMENT '生物编号',
  `通告内容` varchar(255) NOT NULL DEFAULT '玩家 %s 击杀了 %s' COMMENT '通告内容',
  PRIMARY KEY (`生物编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_通告_物品掉落表` (
  `物品编号` int(11) unsigned NOT NULL COMMENT '物品编号',
  `通告内容` varchar(255) NOT NULL DEFAULT '[物品掉落通告]: 玩家 %s 人品爆发 拾取了 %s X %u' COMMENT '通告内容',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `_通告_物品获得表` (
  `物品编号` int(11) unsigned NOT NULL COMMENT '物品编号',
  `通告内容` varchar(255) NOT NULL DEFAULT '[物品掉落通告]: 玩家 %s 人品爆发 拾取了 %s X %u' COMMENT '通告内容',
  PRIMARY KEY (`物品编号`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8 ROW_FORMAT=DYNAMIC;

CREATE TABLE IF NOT EXISTS `zs_图鉴_物品_设置羁绊` (
  `羁绊编号` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '羁绊编号',
  `激活件数` int(10) NOT NULL DEFAULT '0' COMMENT '激活件数',
  `羁绊效果` varchar(640) NOT NULL DEFAULT '' COMMENT '多个以#隔开',
  `效果说明` varchar(640) NOT NULL DEFAULT '' COMMENT '效果说明',
  `备注` varchar(640) NOT NULL DEFAULT '' COMMENT '备忘',
  PRIMARY KEY (`羁绊编号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `zs_图鉴_物品_配置项目` (
  `序号` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '用来显示排序',
  `子项名称` varchar(640) NOT NULL DEFAULT '' COMMENT '显示的名称',
  `子项图标` varchar(640) NOT NULL DEFAULT '' COMMENT '子项菜单显示的图标 Interface\\Icons',
  `关联的页面编号` int(10) NOT NULL DEFAULT '0' COMMENT '对应 zs_图鉴_物品_页面关联',
  `羁绊编号` int(10) NOT NULL DEFAULT '0' COMMENT '对应 zs_图鉴_物品_设置羁绊',
  `备注` varchar(640) NOT NULL DEFAULT '' COMMENT '备忘',
  PRIMARY KEY (`序号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `zs_图鉴_物品_页面关联` (
  `页面编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '页面编号',
  `物品编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '物品编号',
  `奖励模版` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `激活是否生效属性` smallint(1) unsigned NOT NULL DEFAULT '1' COMMENT '激活是否生效属性',
  `激活是否生效技能` smallint(1) unsigned NOT NULL DEFAULT '1' COMMENT '激活是否生效技能',
  `是否扣除` smallint(1) unsigned NOT NULL DEFAULT '1' COMMENT '是否扣除',
  `排序` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '排序',
  `备注` varchar(640) NOT NULL DEFAULT '' COMMENT '备忘',
  PRIMARY KEY (`页面编号`,`物品编号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `zs_图鉴_生物_设置羁绊` (
  `羁绊编号` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '羁绊编号',
  `激活件数` int(10) NOT NULL DEFAULT '0' COMMENT '激活件数',
  `羁绊效果` varchar(640) NOT NULL DEFAULT '' COMMENT '多个以#隔开',
  `效果说明` varchar(640) NOT NULL DEFAULT '' COMMENT '效果说明',
  `备注` varchar(640) NOT NULL DEFAULT '' COMMENT '备忘',
  PRIMARY KEY (`羁绊编号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `zs_图鉴_生物_配置项目` (
  `序号` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '用来显示排序',
  `子项名称` varchar(640) NOT NULL DEFAULT '' COMMENT '显示的名称',
  `子项图标` varchar(640) NOT NULL DEFAULT '' COMMENT '子项菜单显示的图标 Interface\\Icons',
  `关联的页面编号` int(10) NOT NULL DEFAULT '0' COMMENT '对应 zs_图鉴_物品_页面关联',
  `羁绊编号` int(10) NOT NULL DEFAULT '0' COMMENT '对应 zs_图鉴_物品_设置羁绊',
  `备注` varchar(640) NOT NULL DEFAULT '' COMMENT '备忘',
  PRIMARY KEY (`序号`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE IF NOT EXISTS `zs_图鉴_生物_页面关联` (
  `页面编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '页面编号',
  `生物编号` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '生物编号',
  `难度` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '0普通，1英雄，2为25人普通，3为25人英雄',
  `等级` int(10) unsigned NOT NULL DEFAULT '1' COMMENT '同一生物的成长等级，从1开始',
  `需要击杀数量` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '当前等级单独需要的击杀数量，每个等级配置一行',
  `奖励模版` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '奖励模版',
  `激活后奖励属性编号` smallint(5) unsigned NOT NULL DEFAULT '1' COMMENT '当前等级激活的属性编号',
  `激活后奖励属性数值` int(11) NOT NULL DEFAULT '1' COMMENT '当前等级激活的属性数值',
  `可以叠加属性` tinyint(1) unsigned NOT NULL DEFAULT '0' COMMENT '1叠加全部已激活等级，0只生效最高等级',
  `排序` int(10) unsigned NOT NULL DEFAULT '0' COMMENT '控制生物在图鉴页面中的排列，各等级可以填写相同排序',
  `备注` varchar(640) NOT NULL DEFAULT '' COMMENT '备忘',
  PRIMARY KEY (`页面编号`,`生物编号`,`难度`,`等级`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='生物图鉴多行分级配置';

CREATE TABLE IF NOT EXISTS `生物击杀参与宝箱` (
  `生物ID` int(10) unsigned NOT NULL COMMENT '生物的entry，生物死亡时查找此表',
  `宝箱ID` int(10) unsigned NOT NULL COMMENT '宝箱GameObject的entry，对应gameobject_template.entry',
  `宝箱掉落ID` int(10) unsigned NOT NULL COMMENT '引用 gameobject_loot_template 的 entry 字段',
  `宝箱存在时长` int(10) unsigned NOT NULL DEFAULT '300' COMMENT '宝箱存在时间，单位秒',
  `最低伤害要求` decimal(10,2) unsigned NOT NULL DEFAULT '0.00' COMMENT '玩家对生物造成的累计伤害需达到此值才能开启宝箱',
  `IP数量限制` tinyint(3) unsigned NOT NULL DEFAULT '0' COMMENT '允许拾取的不同IP数量，0表示无限制',
  `IP限制白名单账号` varchar(255) NOT NULL DEFAULT '' COMMENT 'IP限制的白名单账号，多个用#分隔，留空表示不启用白名单',
  PRIMARY KEY (`生物ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='生物击杀宝箱配置表';

SET FOREIGN_KEY_CHECKS = 1;
