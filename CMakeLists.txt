cmake_minimum_required(VERSION 3.24)

project(protegon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PROTEGON_DEMOS "Build protegon demos" ON)
option(PROTEGON_WARNINGS_AS_ERRORS "Treat compiler warnings as errors." OFF)
option(PROTEGON_DOWNLOAD_SDL "Download prebuilt windows SDL libraries" ON)

include(cmake/SetupProtegon.cmake)

set(PROTEGON_ROOT_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}"
  CACHE BOOL "")

if(PROTEGON_DEMOS)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/demos")
endif()
