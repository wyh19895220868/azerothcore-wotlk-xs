-- 为现有三件模板装备开放第 4 类洗炼：附魔。
-- FIND_IN_SET 条件保证该脚本可重复执行，不会重复追加 #4。
UPDATE `_模板_物品洗炼`
SET `洗炼类目限制` = CASE
    WHEN `洗炼类目限制` = '' THEN '4'
    ELSE CONCAT(`洗炼类目限制`, '#4')
END
WHERE `物品id` IN (14145, 14148, 14149)
  AND FIND_IN_SET('4', REPLACE(`洗炼类目限制`, '#', ',')) = 0;
