# 幻境系统模块
include_directories(
  ${CMAKE_SOURCE_DIR}/modules
  ${CMAKE_SOURCE_DIR}/modules/mod-item-growth/src
  ${CMAKE_SOURCE_DIR}/modules/mod-item-identification-system/src
)

add_definitions(-DMODULE_HUANJING_SYSTEM)
add_definitions(-DMODULE_ITEM_GROWTH)
add_definitions(-DMODULE_ITEM_IDENTIFICATION)

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/HuanJingSystem.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/HuanJingSystem.h")
