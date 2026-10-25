set(MODULE_NAME "mod-item-refinement")

include_directories(
    ${CMAKE_SOURCE_DIR}/modules
    ${CMAKE_SOURCE_DIR}/modules/mod-item-identification-system/src
    ${CMAKE_SOURCE_DIR}/src/server/scripts/Custom/_Helper
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

AC_ADD_SCRIPT("${CMAKE_CURRENT_SOURCE_DIR}/src/ItemRefinement.cpp")
AC_ADD_SCRIPT_LOADER("ItemRefinement" "${CMAKE_CURRENT_SOURCE_DIR}/src/loader.h")
