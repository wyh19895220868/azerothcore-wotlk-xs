# 物品属性模块

# 添加脚本
CU_ADD_HOOK(AFTER_LOAD_CMAKE_MODULES "${CMAKE_CURRENT_LIST_DIR}/cmake/after_load_cmake_modules.cmake")
CU_ADD_HOOK(AFTER_LOAD_CONF "${CMAKE_CURRENT_LIST_DIR}/cmake/after_load_conf.cmake")

AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/mod_item_attributes.conf.dist")

# 添加源文件
set(SRC_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# 添加include目录到编译器搜索路径
include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/server/game
    ${CMAKE_SOURCE_DIR}/src/server/shared
    ${CMAKE_SOURCE_DIR}/src/server/database
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/modules
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemAttributesDisplay.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemAttributesEffects.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemAttributesEvents.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemAttributesGenerator.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemAttributesLoader.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemAttributesScript.cpp")

AC_ADD_SCRIPT_LOADER("ItemAttributes" "${CMAKE_CURRENT_LIST_DIR}/src/loader.h")
