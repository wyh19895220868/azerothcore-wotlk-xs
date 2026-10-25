-- 五人副本传送等级门槛与升级路线。
-- 目标：只更新标题含“【五人】副本名”的现有传送项，
--       不修改坐标编号、GM 命令、对象编号或序号。
-- 需求模版编号从当前最大值之后自动分配；重复执行时会复用本脚本已写入的模版。
-- 执行完成后，在游戏内以管理员执行：.dore 2

DROP TEMPORARY TABLE IF EXISTS `tmp_五人副本等级路线`;

CREATE TEMPORARY TABLE `tmp_五人副本等级路线` (
    `排序` INT NOT NULL,
    `副本名` VARCHAR(32) NOT NULL,
    `进入等级` INT NOT NULL,
    `推荐区间` VARCHAR(16) NOT NULL,
    `匹配行数` INT NOT NULL DEFAULT 0,
    `模版编号` INT NULL,
    PRIMARY KEY (`副本名`)
) ENGINE=MEMORY DEFAULT CHARSET=utf8;

INSERT INTO `tmp_五人副本等级路线` (`排序`, `副本名`, `进入等级`, `推荐区间`) VALUES
    ( 1, '哀嚎洞穴',   15, '17-20'),
    ( 2, '死亡矿井',   15, '17-20'),
    ( 3, '影牙城堡',   16, '18-21'),
    ( 4, '黑暗深渊',   19, '21-24'),
    ( 5, '暴风城监狱', 20, '22-25'),
    ( 6, '剃刀沼泽',   22, '24-27'),
    ( 7, '诺莫瑞根',   23, '25-28'),
    ( 8, '血色修道院', 27, '29-40'),
    ( 9, '奥达曼',     35, '37-40'),
    (10, '祖尔法拉克', 41, '43-46'),
    (11, '玛拉顿',     41, '43-46'),
    (12, '沉没的神庙', 45, '47-50'),
    (13, '厄运之槌',   53, '55-60'),
    (14, '黑石塔',     55, '57-60'),
    (15, '通灵学院',   55, '60'),
    (16, '斯坦索姆',   55, '60'),
    (17, '奴隶围栏',   60, '62-64'),
    (18, '幽暗沼泽',   61, '63-65'),
    (19, '法力陵墓',   62, '64-66'),
    (20, '奥金尼',     63, '65-67'),
    (21, '塞泰克大厅', 65, '67-68'),
    (22, '蒸汽地窖',   67, '70'),
    (23, '暗影迷宫',   67, '70'),
    (24, '生态船',     67, '70'),
    (25, '能源舰',     67, '70'),
    (26, '禁魔监狱',   68, '70'),
    (27, '艾卓',       70, '72-74'),
    (28, '岩石大厅',   75, '77-79'),
    (29, '魔环',       77, '79-80');

START TRANSACTION;

-- 为首次执行分配全新模版编号；已由本脚本格式化过的菜单项会继续使用原模版。
SET @five_man_template_base := GREATEST(
    COALESCE((SELECT MAX(`模版编号`) FROM `__模版需求总表`), 0),
    1000000
) + 1;

UPDATE `tmp_五人副本等级路线` AS d
LEFT JOIN (
    SELECT d0.`副本名`,
           COUNT(f.`菜单编号`) AS `匹配行数`,
           MAX(CASE
               WHEN f.`标题` LIKE '%|r 进入%（推荐%级）'
                    AND f.`需求模版` > 0
               THEN f.`需求模版`
           END) AS `已有模版编号`
    FROM `tmp_五人副本等级路线` AS d0
    LEFT JOIN `_功能传送表` AS f
        ON f.`标题` LIKE CONCAT('%【五人】', d0.`副本名`, '%')
    GROUP BY d0.`副本名`
) AS m ON m.`副本名` = d.`副本名`
SET d.`匹配行数` = m.`匹配行数`,
    d.`模版编号` = COALESCE(m.`已有模版编号`, @five_man_template_base + d.`排序`);

SET @five_man_wlk_parent_count := (
    SELECT COUNT(*)
    FROM `_功能传送表`
    WHERE `标题` LIKE '%[WLK]5人副本%'
);

SET @five_man_route_ready := (
    SELECT CASE
        WHEN COUNT(*) = 29
         AND SUM(CASE WHEN `匹配行数` = 1 THEN 1 ELSE 0 END) = 29
         AND @five_man_wlk_parent_count = 1
        THEN 1 ELSE 0
    END
    FROM `tmp_五人副本等级路线`
);

-- 预检查：正常应返回 29 行、每行匹配行数为 1，且可执行标记为 1。
-- 若任一项不符合，下面的写入语句都会跳过，不会创建模版或修改菜单。
SELECT `副本名`, `进入等级`, `推荐区间`, `匹配行数`, `模版编号`
FROM `tmp_五人副本等级路线`
ORDER BY `排序`;

SELECT @five_man_wlk_parent_count AS `WLK父菜单匹配数`,
       @five_man_route_ready AS `可执行标记`;

INSERT INTO `__模版需求总表` (`模版编号`, `需要人物等级`, `需要位于地图`)
SELECT `模版编号`, `进入等级`, -1
FROM `tmp_五人副本等级路线`
WHERE `匹配行数` > 0
  AND @five_man_route_ready = 1
ON DUPLICATE KEY UPDATE
    `需要人物等级` = VALUES(`需要人物等级`),
    `需要位于地图` = VALUES(`需要位于地图`);

-- 显示需求负责隐藏未到等级的菜单项；需求模版负责在点击传送时再次硬性拦截。
UPDATE `_功能传送表` AS f
JOIN `tmp_五人副本等级路线` AS d
    ON f.`标题` LIKE CONCAT('%【五人】', d.`副本名`, '%')
   AND @five_man_route_ready = 1
SET f.`标题` = CONCAT(
        IF(INSTR(f.`标题`, '→') > 0, CONCAT(SUBSTRING_INDEX(f.`标题`, '→', 1), '→'), ''),
        '【五人】', d.`副本名`, '|r 进入', d.`进入等级`,
        '级（推荐', d.`推荐区间`, '级）'
    ),
    f.`显示需求模版` = d.`模版编号`,
    f.`需求模版` = d.`模版编号`;

SELECT ROW_COUNT() AS `已更新五人副本菜单项数`;

-- [WLK] 父菜单仅负责显示解锁条件；首个已配置副本“艾卓”在 70 级可进入。
UPDATE `_功能传送表` AS f
JOIN `tmp_五人副本等级路线` AS d
    ON d.`副本名` = '艾卓'
   AND d.`匹配行数` > 0
   AND @five_man_route_ready = 1
SET f.`标题` = CONCAT(
        IF(INSTR(f.`标题`, '→') > 0, CONCAT(SUBSTRING_INDEX(f.`标题`, '→', 1), '→'), ''),
        '[WLK]5人副本|r 70-80级'
    ),
    f.`显示需求模版` = d.`模版编号`,
    f.`需求模版` = 0
WHERE f.`标题` LIKE '%[WLK]5人副本%';

SELECT ROW_COUNT() AS `已更新WLK父菜单数`;

COMMIT;

DROP TEMPORARY TABLE `tmp_五人副本等级路线`;
