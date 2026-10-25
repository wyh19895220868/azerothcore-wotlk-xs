ALTER TABLE `_模板_地图生物控制`
    MODIFY COLUMN `动态难度开关` tinyint unsigned NOT NULL DEFAULT 1 COMMENT '0=关闭战力门槛和动态倍率,1=开启',
    MODIFY COLUMN `战力门槛` decimal(65,0) unsigned NOT NULL DEFAULT 0 COMMENT '0=不限制,>0=最低战力并作为动态难度计算基准',
    MODIFY COLUMN `难度倍率` double unsigned NOT NULL DEFAULT 1 COMMENT '最终动态倍率=(当前战力/战力门槛)*本值；门槛为0时直接使用本值';
