#
# This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#

# 设置模块名称
set(MOD_RUNE_SYSTEM "mod-rune-system")

# 添加包含路径
include_directories(
  ${CMAKE_SOURCE_DIR}/modules
)

# 添加源文件
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneSystem.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneSystem.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneManager.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneManager.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneSystemScripts.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneSystemLoader.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RunePlayerScripts.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneCombatScripts.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneCommands.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneUIScript.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneTypes.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneUtils.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneApplyManager.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneEffectHandler.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneEquipHandler.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/RuneApplyManager.cpp")
AC_ADD_SCRIPT_LOADER("mod_rune_system" "${CMAKE_CURRENT_LIST_DIR}/src/MP_loader.h")

# 添加配置文件
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/RuneSystem.conf.dist")

# 定义模块预处理器宏，用于条件编译
add_definitions(-DMODULE_RUNE_SYSTEM)

# 消息输出
message("-- 符文系统模块已加载（已启用 MODULE_RUNE_SYSTEM 宏）")
