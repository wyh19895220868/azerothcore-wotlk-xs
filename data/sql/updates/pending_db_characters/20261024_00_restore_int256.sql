-- Store exact 256-bit decimal values; preserve existing defaults and column comments.

ALTER TABLE `characters`
    MODIFY COLUMN `health` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `power1` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `power2` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `power3` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `power4` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `power5` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `power6` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `power7` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `character_pet`
    MODIFY COLUMN `curhealth` varchar(80) NOT NULL DEFAULT '1',
    MODIFY COLUMN `curmana` varchar(80) NOT NULL DEFAULT '0';

ALTER TABLE `character_stats`
    MODIFY COLUMN `maxhealth` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `maxpower1` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `maxpower2` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `maxpower3` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `maxpower4` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `maxpower5` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `maxpower6` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `maxpower7` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `strength` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `agility` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `stamina` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `intellect` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `spirit` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `armor` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `attackPower` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `rangedAttackPower` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `spellPower` varchar(80) NOT NULL DEFAULT '0',
    MODIFY COLUMN `resilience` varchar(80) NOT NULL DEFAULT '0';
