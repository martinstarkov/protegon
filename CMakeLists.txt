cmake_minimum_required(VERSION 3.24)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(protegon)

option(PROTEGON_DEMOS "Build protegon demos" ON)
option(protegon_WARNINGS_AS_ERRORS "Treat compiler warnings as errors." OFF)

if(WIN32)
  option(DOWNLOAD_SDL "Download prebuilt windows SDL libraries" ON)
  option(LINK_STATIC_SDL "Link SDL2 statically into the protegon library" OFF)

  # TODO: Figure out how to disable console for macOS and enable for linux
  # option(ENABLE_CONSOLE "Enable console executable" ON)
endif()

set(PROTEGON_ROOT_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}"
  CACHE BOOL "")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SetupProtegon.cmake")

if(PROTEGON_DEMOS)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/demos")
endif()
