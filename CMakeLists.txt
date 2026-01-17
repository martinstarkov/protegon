cmake_minimum_required(VERSION 3.28)

project(protegon LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Versions.cmake)
include(cmake/FindSDL.cmake)

add_executable(protegon
  engine/src/main.cpp
)

target_include_directories(
  protegon
  PUBLIC include/protegon
         modules/ecs/include
  # Keeping this public for testing purposes.
  PUBLIC engine/src)

# Add d to debug static lib files to differentiate them from release
set_target_properties(protegon PROPERTIES DEBUG_POSTFIX d)

target_compile_features(protegon PRIVATE cxx_std_23)

if(EMSCRIPTEN)
  set_target_properties(protegon PROPERTIES SUFFIX ".html")

  target_compile_options(protegon PRIVATE
    -O2
  )

  target_link_options(protegon PRIVATE
    "--shell-file=${CMAKE_SOURCE_DIR}/platform/emscripten/shell.html"
    "--preload-file=${CMAKE_SOURCE_DIR}/assets@/assets"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sFULL_ES3=1"
    "-sWARN_ON_UNDEFINED_SYMBOLS=1"
    "-sNO_EXIT_RUNTIME=1"
    "-sAGGRESSIVE_VARIABLE_ELIMINATION=1"
    "-sUSE_ZLIB=1"
    "-sASSERTIONS=1"
  )
else()
  find_package(OpenGL REQUIRED)
  target_link_libraries(protegon PRIVATE ${OPENGL_LIBRARIES})
endif()

target_link_libraries(protegon PRIVATE
  SDL3_image::SDL3_image
  SDL3_ttf::SDL3_ttf
  SDL3_mixer::SDL3_mixer
  SDL3::SDL3
)

if(WIN32)
  # Copy runtime DLLs next to the exe on Windows (no-op elsewhere)
  add_custom_command(TARGET protegon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:SDL3::SDL3>
      $<TARGET_FILE:SDL3_image::SDL3_image>
      $<TARGET_FILE:SDL3_ttf::SDL3_ttf>
      $<TARGET_FILE:SDL3_mixer::SDL3_mixer>
      $<TARGET_FILE_DIR:protegon>
    COMMAND_EXPAND_LISTS
  )

  if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT protegon)
  endif()
endif()
