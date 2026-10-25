# 添加包含目录
include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_SOURCE_DIR}/modules
    ${CMAKE_SOURCE_DIR}/modules/interfaces
    ${CMAKE_SOURCE_DIR}/src/server/scripts
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/_Helper
)

# 如果存在物品鉴定系统模块，则启用集成（用于避免成长升级时与鉴定属性重复叠加）
if(EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-item-identification-system")
    add_definitions(-DMODULE_ITEM_IDENTIFICATION_SYSTEM)
endif()

# 源文件
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemGrowth.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemGrowthCommands.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemGrowthMgr.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemGrowthCore.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemGrowthUIScript.cpp")

# 头文件
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemGrowth.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemGrowthCommands.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemGrowthMgr.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemGrowthTypes.h")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemGrowthLoader.h")

# 加载器
AC_ADD_SCRIPT_LOADER("ItemGrowth" "${CMAKE_CURRENT_LIST_DIR}/src/ItemGrowthLoader.h")

# 配置文件
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/ItemGrowth.conf.dist")
