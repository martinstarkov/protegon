cmake_minimum_required(VERSION 3.28)

project(protegon LANGUAGES C CXX)

if(APPLE)
  enable_language(OBJC)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(PTGN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(PTGN_EDITOR "Build editor support" ON)

set(
  PTGN_EXAMPLES
  "ALL"
  CACHE STRING
  "Examples to build: ALL, an empty value to disable, or a semicolon-separated list such as basic/hello_world;audio/mixer_demo"
)

if(NOT EMSCRIPTEN)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerSettings.cmake)
endif()

set(
  PTGN_REGISTRATION_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/engine/src/runtime/scripting/script_registry.cpp"
)

add_subdirectory(engine)

if(PTGN_EDITOR)
  add_subdirectory(editor)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/AddProtegon.cmake)

if(PTGN_EXAMPLES)
  add_subdirectory(examples)
endif()