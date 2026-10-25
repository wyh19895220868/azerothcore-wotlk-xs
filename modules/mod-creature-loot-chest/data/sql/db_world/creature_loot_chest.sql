-- 生物击杀参与宝箱表
-- 描述：当生物死亡时，在死亡坐标处生成一个宝箱。
-- 只有对生物累计伤害达到最低要求的玩家才能开启宝箱。
-- 每个玩家独立拾取（首次开启后不能再开）。
-- 宝箱不写入DB，重启后消失，存在时长到达后自动删除。

DROP TABLE IF EXISTS `生物击杀参与宝箱`;
CREATE TABLE `生物击杀参与宝箱` (
    `生物ID` INT UNSIGNED NOT NULL COMMENT '生物的entry，生物死亡时查找此表',
    `宝箱ID` INT UNSIGNED NOT NULL COMMENT '宝箱GameObject的entry，对应gameobject_template.entry',
    `宝箱掉落ID` INT UNSIGNED NOT NULL COMMENT '引用 gameobject_loot_template 的 entry 字段',
    `宝箱存在时长` INT UNSIGNED NOT NULL DEFAULT 300 COMMENT '宝箱存在时间，单位秒',
    `最低伤害要求` DECIMAL(10,2) UNSIGNED NOT NULL DEFAULT 0.00 COMMENT '玩家对生物造成的累计伤害需达到此值才能开启宝箱',
    `IP数量限制` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '允许拾取的不同IP数量，0表示无限制',
    `IP限制白名单账号` VARCHAR(255) NOT NULL DEFAULT '' COMMENT 'IP限制的白名单账号，多个用#分隔，留空表示不启用白名单',
    PRIMARY KEY (`生物ID`)
) ENGINE=INNODB DEFAULT CHARSET=utf8mb4 COMMENT='生物击杀宝箱配置表';
