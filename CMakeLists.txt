cmake_minimum_required(VERSION 3.28)
project(protegon LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Versions.cmake)
include(cmake/FindSDL.cmake)
include(cmake/Protegon.cmake) # we’ll create this

add_library(protegon STATIC
  engine/src/test.h
  engine/src/test.cpp
)

target_include_directories(protegon
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include/protegon
    ${CMAKE_SOURCE_DIR}/modules/ecs/include
  PUBLIC
    # you had this public for testing; keep if desired
    ${CMAKE_SOURCE_DIR}/engine/src
)

target_compile_features(protegon PUBLIC cxx_std_23)

# Link third-party deps ON THE LIBRARY
target_link_libraries(protegon
  PUBLIC
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    SDL3_mixer::SDL3_mixer
)

# Platform deps (OpenGL for native)
if(NOT EMSCRIPTEN)
  find_package(OpenGL REQUIRED)
  target_link_libraries(protegon PUBLIC ${OPENGL_LIBRARIES})
endif()

option(PROTEGON_BUILD_EXAMPLES "Build examples" ON)
set(PROTEGON_EXAMPLES "ALL" CACHE STRING "Examples to build (ALL or list, semicolon-separated)")

if(PROTEGON_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
