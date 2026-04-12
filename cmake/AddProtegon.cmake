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

  if(NOT EMSCRIPTEN)
    if(NOT PTGN_ASSETS_DIR)
      target_compile_definitions(protegon PRIVATE PTGN_ASSET_ROOT="${CMAKE_SOURCE_DIR}")
    else()
      target_compile_definitions(protegon PRIVATE PTGN_ASSET_ROOT="${PTGN_ASSETS_DIR}/..")
    endif()
  else()
    if(NOT PTGN_ASSETS_DIR)
      message(FATAL_ERROR
        "add_protegon_to(${target}): ASSETS_DIR is required when building with Emscripten"
      )
    endif()

    if(NOT PTGN_SHELL_HTML)
      set(PTGN_SHELL_HTML "${CMAKE_CURRENT_SOURCE_DIR}/../platform/emscripten/shell.html")
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
    
    target_compile_definitions(protegon PRIVATE PTGN_ASSET_ROOT="/assets/..")

    target_link_options(${target} PRIVATE
      "--shell-file=${PTGN_SHELL_HTML}"
      "--preload-file=${PTGN_ASSETS_DIR}@/assets"
      "-sALLOW_MEMORY_GROWTH=1"
      "-sFULL_ES3=1"
      "-sWARN_ON_UNDEFINED_SYMBOLS=1"
      "-sNO_EXIT_RUNTIME=1"
      "-sUSE_ZLIB=1"
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_compile_options(${target} PRIVATE
        -O0
        -g3
      )

      target_link_options(${target} PRIVATE
        -O0
        -g3
        "-sASSERTIONS=2"
        "-sSTACK_OVERFLOW_CHECK=2"
        "-sSAFE_HEAP=1"
      )

    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
      target_compile_options(${target} PRIVATE
        -O2
        -g3
      )

      target_link_options(${target} PRIVATE
        -O2
        -g3
        "-sASSERTIONS=1"
      )

    else()
      target_compile_options(${target} PRIVATE
        -O3
      )

      target_link_options(${target} PRIVATE
        -O3
        "-sASSERTIONS=1"
      )
    endif()
  endif()
endfunction()