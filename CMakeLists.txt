cmake_minimum_required(VERSION 3.28)

project(protegon LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/SetupProtegon.cmake)

# Single variable controlling examples. Empty => don't even parse examples/ (fast).
set(PTGN_EXAMPLES "" CACHE STRING
  "Examples to build: empty=none, ALL=all, or list like basic/hello_world;audio/mixer_demo")

# Only add examples if the user requested them
string(STRIP "${PTGN_EXAMPLES}" _ex_raw)
string(TOUPPER "${_ex_raw}" _ex_upper)

if(NOT (_ex_raw STREQUAL "" OR _ex_upper STREQUAL "OFF" OR _ex_upper STREQUAL "FALSE" OR _ex_upper STREQUAL "0"))
  add_subdirectory(examples)
endif()