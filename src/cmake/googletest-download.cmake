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

# code copied from https://crascit.com/2015/07/25/cmake-gtest/
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(googletest-download NONE)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

include(ExternalProject)

ExternalProject_Add(
        googletest
        SOURCE_DIR "@GOOGLETEST_DOWNLOAD_ROOT@/googletest-src"
        BINARY_DIR "@GOOGLETEST_DOWNLOAD_ROOT@/googletest-build"
        # This post-v1.17.0 pin includes the Clang 21 printer fix from upstream fa8438ae.
        # Use an archive of the same pinned commit to avoid cloning the complete history.
        URL https://codeload.github.com/google/googletest/zip/3940de91897160fea4815998e08d0fa3c2fb077e
        URL_HASH SHA256=a2e975f6191baa5fe052f0b034ee4df942ae780102c91ce5a742421269307828
        TIMEOUT 120
        INACTIVITY_TIMEOUT 30
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        TEST_COMMAND ""
)
