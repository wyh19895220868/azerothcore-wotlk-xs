-- 更新角色存档字段说明；tinyint原本已支持0-9，不需要改动数据类型。
ALTER TABLE `_神兽翻牌存档`
  MODIFY COLUMN `品质等级` tinyint unsigned NOT NULL DEFAULT '0'
  COMMENT '0D灰 1C白 2B绿 3A蓝 4S紫 5SS橙 6SSR暗金 7SSS红 8SR粉 9UR彩色';
