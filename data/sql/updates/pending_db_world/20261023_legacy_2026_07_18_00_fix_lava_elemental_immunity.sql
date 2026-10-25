-- Lava elementals are fire immune (4), not nature immune (8).
-- Preserve any unrelated school-immunity bits.
UPDATE `creature_template`
SET `spell_school_immune_mask` = (`spell_school_immune_mask` & ~8) | 4
WHERE `entry` IN (5855, 11321)
  AND (`spell_school_immune_mask` & 8) <> 0;
