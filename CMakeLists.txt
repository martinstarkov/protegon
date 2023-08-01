cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(protegon)

option(AUTOMATIC_SDL "Automatically download SDL zips into external directory" ON)
option(DEMOS "Build protegon demos" ON)
option(TESTS "Build protegon tests" ON)

find_package(protegon REQUIRED)

if(DEMOS)
  add_subdirectory(demos)
endif()

if(TESTS)
  add_subdirectory(tests)
endif()