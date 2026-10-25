-- Restore the legacy quest reward field still read by the NAz compatibility loader.

SET @naz_reward_bonus_money_sql := IF(
    EXISTS (
        SELECT 1
        FROM information_schema.columns
        WHERE table_schema = DATABASE()
          AND table_name = 'quest_template'
          AND column_name = 'RewardBonusMoney'
    ),
    'SELECT 1',
    'ALTER TABLE `quest_template` ADD COLUMN `RewardBonusMoney` int unsigned NOT NULL DEFAULT 0 AFTER `RewardMoneyDifficulty`'
);
PREPARE naz_reward_bonus_money_stmt FROM @naz_reward_bonus_money_sql;
EXECUTE naz_reward_bonus_money_stmt;
DEALLOCATE PREPARE naz_reward_bonus_money_stmt;
