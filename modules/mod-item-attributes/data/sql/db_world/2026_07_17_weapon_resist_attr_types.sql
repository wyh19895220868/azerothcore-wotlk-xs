-- 仅补充缺少的类型，不覆盖运营库中已有配置。
INSERT INTO `_模板_物品属性`
(`id`, `注释`, `客户端显示`, `组`, `属性类型`, `获取几率`, `属性最小百分比`, `属性最大百分比`, `属性计算方式`, `技能模板_组`, `属性品质要求`, `属性等级要求`, `属性职业要求`, `属性颜色`)
SELECT 100000 + v.attribute_type, v.comment_text, v.display_text, 0, v.attribute_type, 0, 1, 1, 2, 0, 0, 0, 0,
       IF(v.attribute_type >= 150, 'FF00FF', '00FFFF')
FROM (
    SELECT '武器速度（毫秒）' comment_text, '|cFF00FFFF武器速度 +%s毫秒|r' display_text, 120 attribute_type UNION ALL
    SELECT '武器伤害', '|cFF00FFFF武器伤害 +%s|r', 121 UNION ALL
    SELECT '每秒伤害', '|cFF00FFFF每秒伤害 +%s|r', 122 UNION ALL
    SELECT '神圣最小伤害', '|cFF00FFFF神圣最小伤害 +%s|r', 123 UNION ALL
    SELECT '神圣最大伤害', '|cFF00FFFF神圣最大伤害 +%s|r', 124 UNION ALL
    SELECT '火焰最小伤害', '|cFF00FFFF火焰最小伤害 +%s|r', 125 UNION ALL
    SELECT '火焰最大伤害', '|cFF00FFFF火焰最大伤害 +%s|r', 126 UNION ALL
    SELECT '自然最小伤害', '|cFF00FFFF自然最小伤害 +%s|r', 127 UNION ALL
    SELECT '自然最大伤害', '|cFF00FFFF自然最大伤害 +%s|r', 128 UNION ALL
    SELECT '冰霜最小伤害', '|cFF00FFFF冰霜最小伤害 +%s|r', 129 UNION ALL
    SELECT '冰霜最大伤害', '|cFF00FFFF冰霜最大伤害 +%s|r', 130 UNION ALL
    SELECT '暗影最小伤害', '|cFF00FFFF暗影最小伤害 +%s|r', 131 UNION ALL
    SELECT '暗影最大伤害', '|cFF00FFFF暗影最大伤害 +%s|r', 132 UNION ALL
    SELECT '奥术最小伤害', '|cFF00FFFF奥术最小伤害 +%s|r', 133 UNION ALL
    SELECT '奥术最大伤害', '|cFF00FFFF奥术最大伤害 +%s|r', 134 UNION ALL
    SELECT '护甲', '|cFF00FFFF护甲 +%s|r', 135 UNION ALL
    SELECT '格挡值', '|cFF00FFFF格挡值 +%s|r', 136 UNION ALL
    SELECT '物品等级', '|cFF00FFFF物品等级 +%s|r', 137 UNION ALL
    SELECT '神圣抗性', '|cFFFF00FF神圣抗性 +%s|r', 150 UNION ALL
    SELECT '火焰抗性', '|cFFFF00FF火焰抗性 +%s|r', 151 UNION ALL
    SELECT '自然抗性', '|cFFFF00FF自然抗性 +%s|r', 152 UNION ALL
    SELECT '冰霜抗性', '|cFFFF00FF冰霜抗性 +%s|r', 153 UNION ALL
    SELECT '暗影抗性', '|cFFFF00FF暗影抗性 +%s|r', 154 UNION ALL
    SELECT '奥术抗性', '|cFFFF00FF奥术抗性 +%s|r', 155
) v
WHERE NOT EXISTS (
    SELECT 1 FROM `_模板_物品属性` t WHERE t.`属性类型` = v.attribute_type
);
