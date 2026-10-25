-- Keep the legacy NAz entry-point route string alongside the current core taxi nodes.
ALTER TABLE `character_entry_point`
    ADD COLUMN `taxiPath` text NOT NULL AFTER `taxiPath1`;

UPDATE `character_entry_point`
SET `taxiPath` = TRIM(CONCAT_WS(' ', NULLIF(`taxiPath0`, 0), NULLIF(`taxiPath1`, 0)))
WHERE `taxiPath0` <> 0 OR `taxiPath1` <> 0;
