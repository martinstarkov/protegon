cmake_minimum_required(VERSION 3.28)

project(protegon LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PTGN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(PTGN_EDITOR "Build editor support" ON)
option(PTGN_EXAMPLES "Build examples" ON)

set(PTGN_WEB_EXAMPLES "ALL" CACHE STRING
  "Web examples to build (Emscripten only): ALL=build all examples, or list like basic/hello_world;audio/mixer_demo")

if(NOT EMSCRIPTEN)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerSettings.cmake)
endif()

add_subdirectory(engine)

if(PTGN_EDITOR)
  add_subdirectory(editor)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/AddProtegon.cmake)

if(PTGN_EXAMPLES)
  add_subdirectory(examples)
endif()