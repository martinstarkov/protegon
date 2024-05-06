cmake_minimum_required(VERSION 3.20)

file(TO_CMAKE_PATH "./CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
    message(FATAL_ERROR "You cannot build in a source directory (or any directory with a CMakeLists.txt file). Please make a build subdirectory.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(protegon)

option(TESTS "Build protegon tests" OFF)

find_package(protegon REQUIRED)

if(TESTS)
  add_subdirectory(tests)
endif()