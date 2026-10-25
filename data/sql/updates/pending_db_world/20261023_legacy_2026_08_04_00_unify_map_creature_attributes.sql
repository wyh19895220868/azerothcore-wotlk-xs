-- 将地图生物控制改为：地图路由 -> 生物控制组 -> 生物属性模板。
-- 迁移完成后仅删除已被替代的 _生物属性控制表；其他旧表保留。

DELIMITER $$

DROP PROCEDURE IF EXISTS `unify_map_creature_attributes`$$
CREATE PROCEDURE `unify_map_creature_attributes`()
BEGIN
    IF EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表'
    ) AND NOT EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '生物等级范围最小'
    ) THEN
        ALTER TABLE `__模版生物属性表`
            ADD COLUMN `生物等级范围最小` int NOT NULL DEFAULT -1 AFTER `超级掉落抽取数量Max`,
            ADD COLUMN `生物等级范围最大` int NOT NULL DEFAULT -1 AFTER `生物等级范围最小`,
            ADD COLUMN `血量倍率` double unsigned NOT NULL DEFAULT 1 AFTER `生物等级范围最大`,
            ADD COLUMN `法力倍率` double unsigned NOT NULL DEFAULT 1 AFTER `血量倍率`,
            ADD COLUMN `法力值` decimal(65,0) unsigned NOT NULL DEFAULT 0 AFTER `法力倍率`,
            ADD COLUMN `最小伤害倍率` double unsigned NOT NULL DEFAULT 1 AFTER `法力值`,
            ADD COLUMN `最大伤害倍率` double unsigned NOT NULL DEFAULT 1 AFTER `最小伤害倍率`,
            ADD COLUMN `真实伤害值` decimal(65,0) unsigned NOT NULL DEFAULT 0 AFTER `最大伤害倍率`,
            ADD COLUMN `护甲倍率` double unsigned NOT NULL DEFAULT 1 AFTER `真实伤害值`,
            ADD COLUMN `火抗倍率` double unsigned NOT NULL DEFAULT 1 AFTER `护甲倍率`,
            ADD COLUMN `自然抗倍率` double unsigned NOT NULL DEFAULT 1 AFTER `火抗倍率`,
            ADD COLUMN `冰霜抗倍率` double unsigned NOT NULL DEFAULT 1 AFTER `自然抗倍率`,
            ADD COLUMN `暗影抗倍率` double unsigned NOT NULL DEFAULT 1 AFTER `冰霜抗倍率`,
            ADD COLUMN `奥术抗倍率` double unsigned NOT NULL DEFAULT 1 AFTER `暗影抗倍率`,
            ADD COLUMN `攻击强度倍率` double unsigned NOT NULL DEFAULT 1 AFTER `奥术抗倍率`,
            ADD COLUMN `攻击强度值` decimal(65,0) unsigned NOT NULL DEFAULT 0 AFTER `攻击强度倍率`,
            ADD COLUMN `法术强度倍率` double unsigned NOT NULL DEFAULT 1 AFTER `攻击强度值`,
            ADD COLUMN `法术强度值` decimal(65,0) unsigned NOT NULL DEFAULT 0 AFTER `法术强度倍率`,
            ADD COLUMN `移动速度倍率` float(10,2) unsigned NOT NULL DEFAULT 1 AFTER `法术强度值`,
            ADD COLUMN `移动速度值` int unsigned NOT NULL DEFAULT 0 AFTER `移动速度倍率`,
            ADD COLUMN `攻击速度倍率` float(10,2) unsigned NOT NULL DEFAULT 1 AFTER `移动速度值`,
            ADD COLUMN `攻击速度值` int unsigned NOT NULL DEFAULT 0 AFTER `攻击速度倍率`,
            ADD COLUMN `施法速度倍率` float(10,2) unsigned NOT NULL DEFAULT 1 AFTER `攻击速度值`,
            ADD COLUMN `施法速度值` int unsigned NOT NULL DEFAULT 0 AFTER `施法速度倍率`,
            ADD COLUMN `命中率加成` float(10,2) NOT NULL DEFAULT 0 AFTER `施法速度值`,
            ADD COLUMN `护甲穿透值` int unsigned NOT NULL DEFAULT 0 AFTER `命中率加成`,
            ADD COLUMN `护甲穿透百分比` float(10,2) unsigned NOT NULL DEFAULT 0 AFTER `护甲穿透值`,
            ADD COLUMN `格挡穿透值` int unsigned NOT NULL DEFAULT 0 AFTER `护甲穿透百分比`,
            ADD COLUMN `格挡穿透百分比` float(10,2) unsigned NOT NULL DEFAULT 0 AFTER `格挡穿透值`,
            ADD COLUMN `伤害减免百分比` bigint NOT NULL DEFAULT 0 AFTER `格挡穿透百分比`,
            ADD COLUMN `官方掉落` tinyint unsigned NOT NULL DEFAULT 1 AFTER `伤害减免百分比`,
            ADD COLUMN `装备属性倍率` float(10,2) unsigned NOT NULL DEFAULT 1 AFTER `官方掉落`,
            ADD COLUMN `装备属性值` int unsigned NOT NULL DEFAULT 0 AFTER `装备属性倍率`,
            ADD COLUMN `装备属性倍率模式` tinyint NOT NULL DEFAULT 0 AFTER `装备属性值`,
            ADD COLUMN `鉴定组` int unsigned NOT NULL DEFAULT 0 AFTER `装备属性倍率模式`,
            ADD COLUMN `鉴定品质` tinyint unsigned NOT NULL DEFAULT 0 AFTER `鉴定组`,
            ADD COLUMN `鉴定物品等级` int unsigned NOT NULL DEFAULT 0 AFTER `鉴定品质`;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '掉落id1'
    ) THEN
        ALTER TABLE `__模版生物属性表` DROP COLUMN `掉落id1`;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '掉落id2'
    ) THEN
        ALTER TABLE `__模版生物属性表` DROP COLUMN `掉落id2`;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '掉落id3'
    ) THEN
        ALTER TABLE `__模版生物属性表` DROP COLUMN `掉落id3`;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '掉落id4'
    ) THEN
        ALTER TABLE `__模版生物属性表` DROP COLUMN `掉落id4`;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '血量比例'
    ) AND EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表' AND COLUMN_NAME = '血量倍率'
    ) THEN
        UPDATE `__模版生物属性表`
        SET `血量倍率` = IF(`血量倍率` = 1 AND `血量比例` <> 1, `血量比例`, `血量倍率`),
            `法力倍率` = IF(`法力倍率` = 1 AND `蓝量比例` <> 1, `蓝量比例`, `法力倍率`),
            `最小伤害倍率` = IF(`最小伤害倍率` = 1 AND `物理伤害比例` <> 1, `物理伤害比例`, `最小伤害倍率`),
            `最大伤害倍率` = IF(`最大伤害倍率` = 1 AND `物理伤害比例` <> 1, `物理伤害比例`, `最大伤害倍率`),
            `伤害减免百分比` = IF(`伤害减免百分比` = 0 AND `免伤比例` > 0, ROUND(LEAST(`免伤比例`, 1) * 100), `伤害减免百分比`),
            `攻击速度值` = IF(`攻击速度值` = 0, GREATEST(`攻速`, 0), `攻击速度值`),
            `官方掉落` = IF(`是否覆盖原掉落` <> 0, 0, `官方掉落`);
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_地图生物控制'
    ) AND NOT EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_地图生物控制' AND COLUMN_NAME = '生物控制组'
    ) THEN
        ALTER TABLE `_模板_地图生物控制`
            ADD COLUMN `生物控制组` int unsigned NOT NULL DEFAULT 0 AFTER `秘境难度`;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_地图生物控制'
    ) AND NOT EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_地图生物控制' AND COLUMN_NAME = '提示方式'
    ) THEN
        ALTER TABLE `_模板_地图生物控制`
            ADD COLUMN `提示方式` tinyint unsigned NOT NULL DEFAULT 0 AFTER `生物控制组`;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_地图生物控制'
    ) AND NOT EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_地图生物控制' AND COLUMN_NAME = '刷新公告'
    ) THEN
        ALTER TABLE `_模板_地图生物控制`
            ADD COLUMN `刷新公告` varchar(255) NOT NULL DEFAULT '[提示]: %s 刷新了' AFTER `提示方式`;
    END IF;

    IF EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_地图生物控制'
    ) AND NOT EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_地图生物控制' AND COLUMN_NAME = '刷新时间'
    ) THEN
        ALTER TABLE `_模板_地图生物控制`
            ADD COLUMN `刷新时间` int unsigned NOT NULL DEFAULT 0 AFTER `刷新公告`;
    END IF;

    -- 将宽地图表中的属性逐条搬到新模板，并生成一对一控制组。
    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_地图生物控制' AND COLUMN_NAME = '血量倍率'
    ) AND EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '__模版生物属性表'
    ) AND EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_生物控制组表'
    ) THEN
        DROP TEMPORARY TABLE IF EXISTS `tmp_map_creature_attribute_migration`;
        CREATE TEMPORARY TABLE `tmp_map_creature_attribute_migration` (
            `序号` int unsigned NOT NULL AUTO_INCREMENT,
            `配置ID` int unsigned NOT NULL,
            PRIMARY KEY (`序号`),
            UNIQUE KEY (`配置ID`)
        ) ENGINE=MEMORY;

        INSERT INTO `tmp_map_creature_attribute_migration` (`配置ID`)
        SELECT `配置ID` FROM `_模板_地图生物控制`
        WHERE `生物控制组` = 0 ORDER BY `配置ID`;

        SET @attribute_template_base = (SELECT COALESCE(MAX(`模版编号`), 0) FROM `__模版生物属性表`);
        SET @creature_group_base = (SELECT COALESCE(MAX(`组号`), 0) FROM `_生物控制组表`);

        INSERT INTO `__模版生物属性表`
            (`模版编号`,`生物等级`,`血条数量`,`固定生命值`,`物理伤害Min`,`物理伤害Max`,`护甲`,`火焰抗性`,`自然抗性`,`冰霜抗性`,`暗影抗性`,`奥术抗性`,
             `生物等级范围最小`,`生物等级范围最大`,`血量倍率`,`法力倍率`,`法力值`,`最小伤害倍率`,`最大伤害倍率`,`真实伤害值`,`护甲倍率`,`火抗倍率`,`自然抗倍率`,`冰霜抗倍率`,`暗影抗倍率`,`奥术抗倍率`,
             `攻击强度倍率`,`攻击强度值`,`法术强度倍率`,`法术强度值`,`移动速度倍率`,`移动速度值`,`攻击速度倍率`,`攻击速度值`,`施法速度倍率`,`施法速度值`,`命中率加成`,`护甲穿透值`,`护甲穿透百分比`,
             `格挡穿透值`,`格挡穿透百分比`,`伤害减免百分比`,`官方掉落`,`装备属性倍率`,`装备属性值`,`装备属性倍率模式`,`鉴定组`,`鉴定品质`,`鉴定物品等级`,`备注`)
        SELECT @attribute_template_base + m.`序号`,
               IF(c.`生物等级范围最小` >= 0, c.`生物等级范围最小`, 0), c.`血条数量`, c.`血量值`, c.`最小伤害值`, c.`最大伤害值`, c.`护甲值`,
               LEAST(c.`火抗值`, 2147483647), LEAST(c.`自然抗值`, 2147483647), LEAST(c.`冰霜抗值`, 2147483647), LEAST(c.`暗影抗值`, 2147483647), LEAST(c.`奥术抗值`, 2147483647),
               c.`生物等级范围最小`, c.`生物等级范围最大`, c.`血量倍率`, c.`法力倍率`, c.`法力值`, c.`最小伤害倍率`, c.`最大伤害倍率`, c.`真实伤害值`, c.`护甲倍率`, c.`火抗倍率`, c.`自然抗倍率`, c.`冰霜抗倍率`, c.`暗影抗倍率`, c.`奥术抗倍率`,
               c.`攻击强度倍率`, c.`攻击强度值`, c.`法术强度倍率`, c.`法术强度值`, c.`移动速度倍率`, c.`移动速度值`, c.`攻击速度倍率`, c.`攻击速度值`, c.`施法速度倍率`, c.`施法速度值`, c.`命中率加成`, c.`护甲穿透值`, c.`护甲穿透百分比`,
               c.`格挡穿透值`, c.`格挡穿透百分比`, c.`伤害减免百分比`, c.`官方掉落`, c.`装备属性倍率`, c.`装备属性值`, c.`装备属性倍率模式`, c.`鉴定组`, c.`鉴定品质`, c.`鉴定物品等级`, CONCAT('由地图配置 ', c.`配置ID`, ' 自动迁移')
        FROM `tmp_map_creature_attribute_migration` m
        JOIN `_模板_地图生物控制` c ON c.`配置ID` = m.`配置ID`;

        INSERT INTO `_生物控制组表` (`组号`,`属性模版编号`,`模型编号`,`模型大小`,`武器1`,`武器2`,`武器3`,`几率`)
        SELECT @creature_group_base + `序号`, @attribute_template_base + `序号`, 0, 0, 0, 0, 0, 100
        FROM `tmp_map_creature_attribute_migration`;

        UPDATE `_模板_地图生物控制` c
        JOIN `tmp_map_creature_attribute_migration` m ON m.`配置ID` = c.`配置ID`
        SET c.`生物控制组` = @creature_group_base + m.`序号`;

        DROP TEMPORARY TABLE `tmp_map_creature_attribute_migration`;
    END IF;

    -- 旧的按生物编号控制配置迁入新地图表，保留公告和刷新时间。
    IF EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_生物属性控制表'
    ) AND EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_地图生物控制'
    ) THEN
        INSERT INTO `_模板_地图生物控制`
            (`生物ID`,`生物类型`,`地图ID`,`区域ID`,`难度`,`秘境难度`,`生物控制组`,`提示方式`,`刷新公告`,`刷新时间`,`动态难度开关`,`战力门槛`,`难度倍率`)
        SELECT old.`生物编号`, -1, -1, -1, old.`难度`, '0', GREATEST(old.`控制组号`, 0), GREATEST(old.`提示方式`, 0), old.`刷新公告`, GREATEST(old.`刷新时间`, 0), 0, 0, 1
        FROM `_生物属性控制表` old
        WHERE NOT EXISTS (
            SELECT 1 FROM `_模板_地图生物控制` current
            WHERE current.`生物ID` = old.`生物编号`
              AND current.`难度` = old.`难度`
              AND current.`生物控制组` = GREATEST(old.`控制组号`, 0)
        );

        DROP TABLE `_生物属性控制表`;
    END IF;

    -- 保留 _地图区域生物属性表，但将其中配置复制到新路由，避免升级后失效。
    IF EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_地图区域生物属性表'
    ) AND EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_地图生物控制'
    ) AND EXISTS (
        SELECT 1 FROM information_schema.TABLES
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_生物控制组表'
    ) THEN
        DROP TEMPORARY TABLE IF EXISTS `tmp_legacy_map_creature_migration`;
        CREATE TEMPORARY TABLE `tmp_legacy_map_creature_migration` (
            `序号` int unsigned NOT NULL AUTO_INCREMENT,
            `地图或区域编号` int NOT NULL,
            `难度` int unsigned NOT NULL,
            `生物类型` tinyint unsigned NOT NULL,
            `生物属性模版` int unsigned NOT NULL,
            PRIMARY KEY (`序号`)
        ) ENGINE=MEMORY;

        INSERT INTO `tmp_legacy_map_creature_migration` (`地图或区域编号`,`难度`,`生物类型`,`生物属性模版`)
        SELECT `地图或区域编号`,`难度`,`生物类型`,`生物属性模版`
        FROM `_地图区域生物属性表`
        WHERE `生物属性模版` > 0;

        SET @legacy_group_base = (SELECT COALESCE(MAX(`组号`), 0) FROM `_生物控制组表`);

        INSERT INTO `_生物控制组表` (`组号`,`属性模版编号`,`模型编号`,`模型大小`,`武器1`,`武器2`,`武器3`,`几率`)
        SELECT @legacy_group_base + `序号`, `生物属性模版`, 0, 0, 0, 0, 0, 100
        FROM `tmp_legacy_map_creature_migration`;

        INSERT INTO `_模板_地图生物控制`
            (`生物ID`,`生物类型`,`地图ID`,`区域ID`,`难度`,`秘境难度`,`生物控制组`,`提示方式`,`刷新公告`,`刷新时间`,`动态难度开关`,`战力门槛`,`难度倍率`)
        SELECT -1,
               IF(m.`生物类型` = 1, 3, 0),
               IF(m.`地图或区域编号` >= 0, m.`地图或区域编号`, -1),
               IF(m.`地图或区域编号` < 0, -m.`地图或区域编号`, -1),
               m.`难度`, '0', @legacy_group_base + m.`序号`, 0, '[提示]: %s 刷新了', 0, 0, 0, 1
        FROM `tmp_legacy_map_creature_migration` m;

        DROP TEMPORARY TABLE `tmp_legacy_map_creature_migration`;
    END IF;

    -- 所有宽字段已迁入属性模板，地图表只留下路由字段。
    IF EXISTS (
        SELECT 1 FROM information_schema.COLUMNS
        WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = '_模板_地图生物控制' AND COLUMN_NAME = '血量倍率'
    ) THEN
        ALTER TABLE `_模板_地图生物控制`
            DROP COLUMN `生物等级范围最小`, DROP COLUMN `生物等级范围最大`,
            DROP COLUMN `血量倍率`, DROP COLUMN `血量值`, DROP COLUMN `法力倍率`, DROP COLUMN `法力值`,
            DROP COLUMN `最小伤害倍率`, DROP COLUMN `最小伤害值`, DROP COLUMN `最大伤害倍率`, DROP COLUMN `最大伤害值`, DROP COLUMN `真实伤害值`,
            DROP COLUMN `护甲倍率`, DROP COLUMN `护甲值`, DROP COLUMN `火抗倍率`, DROP COLUMN `火抗值`, DROP COLUMN `自然抗倍率`, DROP COLUMN `自然抗值`,
            DROP COLUMN `冰霜抗倍率`, DROP COLUMN `冰霜抗值`, DROP COLUMN `暗影抗倍率`, DROP COLUMN `暗影抗值`, DROP COLUMN `奥术抗倍率`, DROP COLUMN `奥术抗值`,
            DROP COLUMN `攻击强度倍率`, DROP COLUMN `攻击强度值`, DROP COLUMN `法术强度倍率`, DROP COLUMN `法术强度值`,
            DROP COLUMN `移动速度倍率`, DROP COLUMN `移动速度值`, DROP COLUMN `攻击速度倍率`, DROP COLUMN `攻击速度值`, DROP COLUMN `施法速度倍率`, DROP COLUMN `施法速度值`,
            DROP COLUMN `血条数量`, DROP COLUMN `命中率加成`, DROP COLUMN `护甲穿透值`, DROP COLUMN `护甲穿透百分比`, DROP COLUMN `格挡穿透值`, DROP COLUMN `格挡穿透百分比`, DROP COLUMN `伤害减免百分比`,
            DROP COLUMN `官方掉落`, DROP COLUMN `装备属性倍率`, DROP COLUMN `装备属性值`, DROP COLUMN `装备属性倍率模式`,
            DROP COLUMN `掉落id1`, DROP COLUMN `掉落id2`, DROP COLUMN `掉落id3`, DROP COLUMN `掉落id4`, DROP COLUMN `鉴定组`, DROP COLUMN `鉴定品质`, DROP COLUMN `鉴定物品等级`;
    END IF;
END$$

CALL `unify_map_creature_attributes`()$$
DROP PROCEDURE `unify_map_creature_attributes`$$

DELIMITER ;
