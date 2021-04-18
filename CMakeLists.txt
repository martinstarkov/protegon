cmake_minimum_required(VERSION 3.12)
project(protegon)

# Enforce C++17 version requirement.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add package find files to cmake module path.

set(SDL2_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2")
set(PROTEGON_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/protegon")
list(APPEND CMAKE_MODULE_PATH ${SDL2_CMAKE_PATH})
list(APPEND CMAKE_MODULE_PATH ${PROTEGON_CMAKE_PATH})

add_subdirectory(protegon)

# Add option to include test application.

option(INCLUDE_PROTEGON_TEST_APP "INCLUDE_PROTEGON_TEST_APP" OFF)

if(INCLUDE_PROTEGON_TEST_APP)
    add_subdirectory(test_app)
endif()