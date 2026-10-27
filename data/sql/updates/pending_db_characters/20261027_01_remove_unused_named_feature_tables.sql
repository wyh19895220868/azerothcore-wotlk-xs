-- Remove character-database tables for retired features only when they hold no current data.
DROP TABLE IF EXISTS
    `生物采集_日志`,
    `生物采集_玩家数据`,
    `生物采集_种植记录`,
    `生物采集_采集冷却`,
    `生物采集_野生生物记录`,
    `_装备特殊成长经验存档`;
