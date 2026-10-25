#
# Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
#

# 设置模块名称
set(MODULE_NAME "mod-item-sets")

# 添加包含路径
include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/_Helper
)

# Add scripts - 使用整合后的文件
AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/ItemSets.cpp")
AC_ADD_SCRIPT_LOADER("ItemSets" "${CMAKE_CURRENT_SOURCE_DIR}/src/ItemSets.h")

# Add config file
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_SOURCE_DIR}/conf/mod_item_sets.conf.dist")
