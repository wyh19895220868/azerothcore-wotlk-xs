-- Restore all custom tables and data from E:/xs-wow/数据库备份/最新纯净库/A库.sql.
-- Existing rows in the new database win; this migration fills missing keys.
SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- _account_vip: 2 legacy rows.
DELETE FROM `_account_vip` WHERE 1 = 0;
INSERT IGNORE INTO `_account_vip` VALUES
(1, 0, 23976, 0, 1103, 0, 0),
(2, 0, 3996, 0, 0, 0, 0);

SET FOREIGN_KEY_CHECKS = 1;
