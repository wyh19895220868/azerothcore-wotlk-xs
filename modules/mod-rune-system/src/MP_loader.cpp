/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

// 声明外部函数
void AddRuneSystemScripts();
void AddSC_RuneSystemLoader();
void AddSC_rune_commands();
void AddSC_rune_player_scripts();
void AddSC_rune_combat_scripts();
void AddSC_rune_ui_script();

// 添加所有脚本
// cf. the naming convention https://github.com/azerothcore/azerothcore-wotlk/blob/master/doc/changelog/master.md#how-to-upgrade-4
// additionally replace all '-' in the module folder name with '_' here
void Addmod_rune_systemScripts()
{
    AddSC_RuneSystemLoader();  // 必须最先调用，初始化符文系统
    AddRuneSystemScripts();
    AddSC_rune_commands();
    AddSC_rune_player_scripts();
    AddSC_rune_combat_scripts();
    AddSC_rune_ui_script();
}
