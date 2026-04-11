set(PTGN_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")
set(PTGN_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL "")

function(add_protegon_to target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "add_protegon_to: target '${target}' does not exist")
  endif()

  set(options)
  set(oneValueArgs ASSETS_DIR SHELL_HTML)
  cmake_parse_arguments(PTGN "${options}" "${oneValueArgs}" "" ${ARGN})

  target_link_libraries(${target} PRIVATE protegon)

  if(PTGN_EDITOR)
    if(NOT TARGET protegon_editor)
      message(FATAL_ERROR
        "add_protegon_to(${target}): PTGN_EDITOR is ON but protegon_editor target does not exist"
      )
    endif()

    target_link_libraries(${target} PRIVATE protegon_editor)
    target_compile_definitions(${target} PRIVATE PTGN_EDITOR=1)
  endif()

  if(EMSCRIPTEN)
    if(NOT PTGN_ASSETS_DIR)
      message(FATAL_ERROR
        "add_protegon_to(${target}): ASSETS_DIR is required when building with Emscripten"
      )
    endif()

    if(NOT PTGN_SHELL_HTML)
      message(FATAL_ERROR
        "add_protegon_to(${target}): SHELL_HTML is required when building with Emscripten"
      )
    endif()

    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/dist")

    set_target_properties(${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
      OUTPUT_NAME "index"
      SUFFIX ".html"
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_compile_options(${target} PRIVATE -O0)
    else()
      target_compile_options(${target} PRIVATE -O3)
    endif()

    if(EXISTS "${PTGN_ASSETS_DIR}")
      target_link_options(${target} PRIVATE
        "--preload-file=${PTGN_ASSETS_DIR}@/assets"
      )
    else()
      message(FATAL_ERROR
        "add_protegon_to(${target}): ASSETS_DIR does not exist: ${PTGN_ASSETS_DIR}"
      )
    endif()

    if(NOT EXISTS "${PTGN_SHELL_HTML}")
      message(FATAL_ERROR
        "add_protegon_to(${target}): SHELL_HTML does not exist: ${PTGN_SHELL_HTML}"
      )
    endif()

    target_link_options(${target} PRIVATE
      "--shell-file=${PTGN_SHELL_HTML}"
      "-sALLOW_MEMORY_GROWTH=1"
      "-sFULL_ES3=1"
      "-sWARN_ON_UNDEFINED_SYMBOLS=1"
      "-sNO_EXIT_RUNTIME=1"
      "-sAGGRESSIVE_VARIABLE_ELIMINATION=1"
      "-sUSE_ZLIB=1"
      "-sASSERTIONS=1"
    )
  endif()
endfunction()