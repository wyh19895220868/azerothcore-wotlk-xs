INSERT INTO `_模板_物品属性`
(`id`, `注释`, `客户端显示`, `组`, `属性类型`, `获取几率`, `属性最小百分比`, `属性最大百分比`, `属性计算方式`, `技能模板_组`, `属性品质要求`, `属性等级要求`, `属性职业要求`, `属性颜色`)
SELECT 100000 + v.t, CONCAT(v.n, '属性'), v.n, 0, v.t, 100, 90, 110, 1, 0, 0, 1, 0, v.c
FROM (
    SELECT 0 t, '法力值' n, '0000FF' c UNION ALL SELECT 1, '生命值', 'FF0000' UNION ALL SELECT 2, '护甲', 'FFFFFF' UNION ALL
    SELECT 3, '敏捷', '00FF00' UNION ALL SELECT 4, '力量', 'FF0000' UNION ALL SELECT 5, '智力', 'FF00FF' UNION ALL
    SELECT 6, '精神', 'FFFF00' UNION ALL SELECT 7, '耐力', '0000FF' UNION ALL SELECT 12, '防御等级', 'FFFF00' UNION ALL
    SELECT 13, '躲闪等级', '00FF00' UNION ALL SELECT 14, '招架等级', '00FFFF' UNION ALL SELECT 15, '格挡等级', 'FF8800' UNION ALL
    SELECT 16, '近战命中等级', 'FFFFFF' UNION ALL SELECT 17, '远程命中等级', 'FFFFFF' UNION ALL SELECT 18, '法术命中等级', 'FFFFFF' UNION ALL
    SELECT 19, '近战暴击等级', 'FF0000' UNION ALL SELECT 20, '远程暴击等级', 'FF0000' UNION ALL SELECT 21, '法术暴击等级', 'FF0000' UNION ALL
    SELECT 22, '近战命中回避等级', '00FF00' UNION ALL SELECT 23, '远程命中回避等级', '00FF00' UNION ALL SELECT 24, '法术命中回避等级', '00FF00' UNION ALL
    SELECT 25, '近战致命一击回避等级', 'FF00FF' UNION ALL SELECT 26, '远程致命一击回避等级', 'FF00FF' UNION ALL SELECT 27, '法术致命一击回避等级', 'FF00FF' UNION ALL
    SELECT 28, '近战急速等级', '00FFFF' UNION ALL SELECT 29, '远程急速等级', '00FFFF' UNION ALL SELECT 30, '法术急速等级', '00FFFF' UNION ALL
    SELECT 31, '命中等级', 'FFFFFF' UNION ALL SELECT 32, '暴击等级', 'FF0000' UNION ALL SELECT 33, '命中回避等级', '00FF00' UNION ALL
    SELECT 34, '致命一击回避等级', 'FF00FF' UNION ALL SELECT 35, '韧性等级', 'FF00FF' UNION ALL SELECT 36, '急速等级', '00FFFF' UNION ALL
    SELECT 37, '精准等级', 'FFAA00' UNION ALL SELECT 38, '攻击强度', 'FF0000' UNION ALL SELECT 39, '远程攻击强度', 'FF0000' UNION ALL
    SELECT 40, '形态攻击强度', 'FF0000' UNION ALL SELECT 41, '法术治疗', '00FF00' UNION ALL SELECT 42, '法术伤害', 'FF00FF' UNION ALL
    SELECT 43, '法力恢复', '0000FF' UNION ALL SELECT 44, '护甲穿透等级', 'FF0000' UNION ALL SELECT 45, '法术强度', 'FF00FF' UNION ALL
    SELECT 46, '生命恢复', 'FF0000' UNION ALL SELECT 47, '法术穿透', 'FF00FF' UNION ALL SELECT 48, '格挡值', 'FF8800'
) v
LEFT JOIN `_模板_物品属性` e ON e.`属性类型` = v.t
WHERE e.`id` IS NULL;
