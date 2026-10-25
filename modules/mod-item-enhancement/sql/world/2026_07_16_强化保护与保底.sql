DROP PROCEDURE IF EXISTS `upgrade_item_enhancement_protection`;
DELIMITER $$
CREATE PROCEDURE `upgrade_item_enhancement_protection`()
BEGIN
  ALTER TABLE `_模板_物品强化`
    MODIFY COLUMN `失败处理` tinyint UNSIGNED NOT NULL DEFAULT 0 COMMENT '失败后处理（0不变，1等级清0，2摧毁，3失败降级）';

  IF NOT EXISTS (SELECT 1 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='_模板_物品强化' AND COLUMN_NAME='失败降级等级') THEN
    ALTER TABLE `_模板_物品强化`
      ADD COLUMN `失败降级等级` int UNSIGNED NOT NULL DEFAULT 1 COMMENT '仅失败处理=3时生效；0、1、2时忽略' AFTER `失败处理`,
      ADD COLUMN `保底失败次数` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '0关闭；连续失败达到该次数后本次必定成功' AFTER `失败降级等级`,
      ADD COLUMN `每次失败增加成功率` float UNSIGNED NOT NULL DEFAULT 0 COMMENT '按当前连续失败次数增加成功率' AFTER `保底失败次数`,
      ADD COLUMN `幸运物品` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '背包存在时自动消耗并增加本次成功率' AFTER `每次失败增加成功率`,
      ADD COLUMN `幸运物品数量` int UNSIGNED NOT NULL DEFAULT 0 AFTER `幸运物品`,
      ADD COLUMN `幸运增加成功率` float UNSIGNED NOT NULL DEFAULT 0 AFTER `幸运物品数量`,
      ADD COLUMN `保护物品` int UNSIGNED NOT NULL DEFAULT 0 COMMENT '失败将清零、降级或摧毁时自动消耗并保护装备' AFTER `幸运增加成功率`,
      ADD COLUMN `保护物品数量` int UNSIGNED NOT NULL DEFAULT 0 AFTER `保护物品`;
  ELSE
    ALTER TABLE `_模板_物品强化`
      MODIFY COLUMN `失败降级等级` int UNSIGNED NOT NULL DEFAULT 1 COMMENT '仅失败处理=3时生效；0、1、2时忽略';
  END IF;
END$$
DELIMITER ;
CALL `upgrade_item_enhancement_protection`();
DROP PROCEDURE `upgrade_item_enhancement_protection`;
