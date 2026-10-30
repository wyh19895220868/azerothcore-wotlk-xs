-- Tooltip LIMIT section fixture: one daily cap on item 2092 so the client-side
-- item tooltip can be tested. Remove before release.
DELETE FROM `_物品获取限制表` WHERE `物品编号` = 2092;
INSERT INTO `_物品获取限制表` (`物品编号`, `角色每日上限`, `角色永久上限`, `账号每日上限`, `账号永久上限`) VALUES (2092, 5, 0, 0, 0);
