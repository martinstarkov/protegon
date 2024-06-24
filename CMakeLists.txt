cmake_minimum_required(VERSION 3.16)

project(protegon)

option(PROTEGON_TESTS "Build protegon tests" OFF)

if (WIN32)
  option(DOWNLOAD_SDL "Download prebuilt windows SDL libraries" OFF)
  # TODO: Figure out how to disable console for macOS and enable for linux
  option(ENABLE_CONSOLE "Enable console executable" ON)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SetupProtegon.cmake)

if(PROTEGON_TESTS)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()