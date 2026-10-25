-- Native client protocol v1: deploy with the matching 64-bit-money client and server.
ALTER TABLE `characters`
    MODIFY `money` BIGINT UNSIGNED NOT NULL DEFAULT '0';
ALTER TABLE `mail`
    MODIFY `money` BIGINT UNSIGNED NOT NULL DEFAULT '0',
    MODIFY `cod` BIGINT UNSIGNED NOT NULL DEFAULT '0';
ALTER TABLE `auctionhouse`
    MODIFY `buyoutprice` BIGINT UNSIGNED NOT NULL DEFAULT '0',
    MODIFY `lastbid` BIGINT UNSIGNED NOT NULL DEFAULT '0',
    MODIFY `startbid` BIGINT UNSIGNED NOT NULL DEFAULT '0',
    MODIFY `deposit` BIGINT UNSIGNED NOT NULL DEFAULT '0';
ALTER TABLE `guild_rank`
    MODIFY `BankMoneyPerDay` BIGINT UNSIGNED NOT NULL DEFAULT '0';
ALTER TABLE `guild_member_withdraw`
    MODIFY `money` BIGINT UNSIGNED NOT NULL DEFAULT '0';
ALTER TABLE `guild_bank_eventlog`
    MODIFY `ItemOrMoney` BIGINT UNSIGNED NOT NULL DEFAULT '0';
ALTER TABLE `item_refund_instance`
    MODIFY `paidMoney` BIGINT UNSIGNED NOT NULL DEFAULT '0';
ALTER TABLE `item_loot_storage`
    MODIFY `count` BIGINT UNSIGNED NOT NULL;
UPDATE `guild_rank` SET `BankMoneyPerDay` = 18446744073709551615 WHERE `rid` = 0;
