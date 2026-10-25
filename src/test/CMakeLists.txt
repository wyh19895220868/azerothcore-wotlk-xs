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
CollectSourceFiles(
        ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE_SOURCES
)

include_directories(
        "${CMAKE_CURRENT_SOURCE_DIR}/mocks"
)

# An empty filter keeps the full upstream test suite. Custom forks can build a
# focused set while unrelated tests still reference APIs they have not ported.
set(ACORE_TEST_SOURCE_FILTER "" CACHE STRING "Regex selecting test paths relative to src/test")
if(ACORE_TEST_SOURCE_FILTER)
    set(SELECTED_TEST_SOURCES)
    foreach(TEST_SOURCE IN LISTS PRIVATE_SOURCES)
        file(RELATIVE_PATH TEST_RELATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${TEST_SOURCE}")
        if(TEST_RELATIVE_PATH MATCHES "^mocks/" OR TEST_RELATIVE_PATH MATCHES "${ACORE_TEST_SOURCE_FILTER}")
            list(APPEND SELECTED_TEST_SOURCES "${TEST_SOURCE}")
        endif()
    endforeach()
    set(PRIVATE_SOURCES ${SELECTED_TEST_SOURCES})
    message(STATUS "Core test source filter: ${ACORE_TEST_SOURCE_FILTER}")
endif()

add_executable(
        unit_tests
        ${PRIVATE_SOURCES}
)

target_link_libraries(
        unit_tests
        acore-core-interface
        game
        gtest_main
        gmock_main
        game-interface
)

# Add module test sources if any modules registered tests
get_property(MODULE_TEST_SOURCES GLOBAL PROPERTY ACORE_MODULE_TEST_SOURCES)
get_property(MODULE_TEST_INCLUDES GLOBAL PROPERTY ACORE_MODULE_TEST_INCLUDES)

if(MODULE_TEST_SOURCES AND NOT ACORE_TEST_SOURCE_FILTER)
    target_sources(unit_tests PRIVATE ${MODULE_TEST_SOURCES})
    message(STATUS "Added module tests to unit_tests target")
endif()

if(MODULE_TEST_INCLUDES)
    list(REMOVE_DUPLICATES MODULE_TEST_INCLUDES)
    target_include_directories(unit_tests PRIVATE ${MODULE_TEST_INCLUDES})
    message(STATUS "Added module test includes to unit_tests target")
endif()

# This custom core calls Custom/ implementations from game and modules. Tests
# must resolve those implementations through the same script library as worldserver.
if(TARGET scripts)
    target_link_libraries(unit_tests scripts)
endif()

# Link modules library to tests so module code is available
if(TARGET modules)
    target_link_libraries(unit_tests modules)
endif()

add_test(
        NAME
        unit
        COMMAND
        $<TARGET_FILE:unit_tests>
)
