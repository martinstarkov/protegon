cmake_minimum_required(VERSION 3.12)

project(protegon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(protegon REQUIRED)

option(PROTEGON_TEST_APP "PROTEGON_TEST_APP" OFF)

if(PROTEGON_TEST_APP)
    add_subdirectory(test_app)
endif()