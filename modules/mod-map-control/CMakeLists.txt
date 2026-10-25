# 地图控制模块

include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/server/game
    ${CMAKE_SOURCE_DIR}/src/server/shared
    ${CMAKE_SOURCE_DIR}/src/server/database
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/modules
)

add_definitions(-DMODULE_MAP_CONTROL)

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/MapControlSystem.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/MapControlSystem.h")
AC_ADD_SCRIPT_LOADER("MapControlSystem" "${CMAKE_CURRENT_LIST_DIR}/src/MapControlSystem_loader.h")
