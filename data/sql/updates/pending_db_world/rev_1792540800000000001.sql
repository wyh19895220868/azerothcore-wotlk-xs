-- Keep vendor prices in copper, without narrowing them through signed 32-bit storage.
ALTER TABLE `item_template`
    MODIFY `BuyPrice` BIGINT UNSIGNED NOT NULL DEFAULT '0',
    MODIFY `SellPrice` BIGINT UNSIGNED NOT NULL DEFAULT '0';
