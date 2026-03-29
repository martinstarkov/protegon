cmake_minimum_required(VERSION 3.28)

project(protegon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/SetupProtegon.cmake)

option(PTGN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(PTGN_EXAMPLES "Build protegon examples" ON)

set(PTGN_WEB_EXAMPLES "ALL" CACHE STRING
  "Web examples to build (Emscripten only): ALL=build all examples, or list like basic/hello_world;audio/mixer_demo")

if(PTGN_EXAMPLES)
  add_subdirectory(examples)
endif()