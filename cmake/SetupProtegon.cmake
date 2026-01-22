include(cmake/SDLVersions.cmake)
include(cmake/FindSDL.cmake)

function(add_protegon_to target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "protegon_apply_to_exe: target '${target}' does not exist")
  endif()

  set(options)
  set(oneValueArgs ASSETS_DIR SHELL_HTML)
  cmake_parse_arguments(P "${options}" "${oneValueArgs}" "" ${ARGN})

  if(NOT P_ASSETS_DIR)
    set(P_ASSETS_DIR "${CMAKE_SOURCE_DIR}/examples/assets")
  endif()

  if(NOT P_SHELL_HTML)
    set(P_SHELL_HTML "${CMAKE_SOURCE_DIR}/platform/emscripten/shell.html")
  endif()

  target_link_libraries(${target} PRIVATE protegon)
  set_target_properties(${target} PROPERTIES DEBUG_POSTFIX d)

  if(EMSCRIPTEN)
    set_target_properties(${target} PROPERTIES SUFFIX ".html")
    target_compile_options(${target} PRIVATE -O2)

    # Shared assets for ALL examples
    if(EXISTS "${P_ASSETS_DIR}")
      target_link_options(${target} PRIVATE
        "--preload-file=${P_ASSETS_DIR}@/assets"
      )
    endif()

    target_link_options(${target} PRIVATE
      "--shell-file=${P_SHELL_HTML}"
      "-sALLOW_MEMORY_GROWTH=1"
      "-sFULL_ES3=1"
      "-sWARN_ON_UNDEFINED_SYMBOLS=1"
      "-sNO_EXIT_RUNTIME=1"
      "-sAGGRESSIVE_VARIABLE_ELIMINATION=1"
      "-sAGGRESSIVE_VARIABLE_ELIMINATION=1"
      "-sUSE_ZLIB=1"
      "-sASSERTIONS=1"
    )
  else()
    if(WIN32)
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:SDL3::SDL3>
          $<TARGET_FILE:SDL3_image::SDL3_image>
          $<TARGET_FILE:SDL3_ttf::SDL3_ttf>
          $<TARGET_FILE:SDL3_mixer::SDL3_mixer>
          $<TARGET_FILE_DIR:${target}>
        COMMAND_EXPAND_LISTS
      )
    endif()
  endif()
endfunction()

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