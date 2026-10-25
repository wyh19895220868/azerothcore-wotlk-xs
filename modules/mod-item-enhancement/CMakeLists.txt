# 设置模块名称
set(MODULE_NAME "mod-item-enhancement")

# 添加包含路径
include_directories(
    ${CMAKE_SOURCE_DIR}/modules
    ${CMAKE_SOURCE_DIR}/modules/interfaces
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/_Helper
)

# 如果存在物品鉴定系统模块，则启用集成
if(EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-item-identification-system")
    add_definitions(-DMODULE_ITEM_IDENTIFICATION_SYSTEM)
endif()

# 添加源文件
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemEnhancement.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemEnhancementMgr.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemEnhancementCommands.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemEnhancementScript.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemEnhancementPlayerScript.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemInspectGuidHandler.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemEnhancementUIScript.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemEnhancementAddonHandler.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/ItemEnhancementAddonUtil.cpp")
AC_ADD_SCRIPT_LOADER("ItemEnhancement" "${CMAKE_CURRENT_LIST_DIR}/src/loader.h")

# 添加配置文件
AC_ADD_CONFIG_FILE("${CMAKE_CURRENT_LIST_DIR}/conf/ItemEnhancement.conf.dist")

# 添加简单的编译选项
if(MSVC)
    # 禁用特定警告
    add_compile_options(/wd4996) # 禁用"被声明为已弃用"的警告
endif()
