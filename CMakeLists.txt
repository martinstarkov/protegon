cmake_minimum_required(VERSION 3.20)

project(protegon)

option(DEMOS "Build protegon demos" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(protegon REQUIRED)

if(DEMOS)
  add_subdirectory(demos)
endif()