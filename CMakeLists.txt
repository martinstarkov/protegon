cmake_minimum_required(VERSION 3.16)

project(protegon)

option(TESTS "Build protegon tests" OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SetupProtegon.cmake)

if(TESTS)
  add_subdirectory(tests)
endif()