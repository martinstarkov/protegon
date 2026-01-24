function(set_compiler_settings project_name)
  # Optional second argument: list of source files for MSVC source_group()
  set(_files "${ARGN}")

  target_compile_options(
    ${project_name}
    PUBLIC
      $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/Zc:preprocessor>
      $<$<CXX_COMPILER_ID:MSVC>:/MP>
      # $<$<AND:$<CXX_COMPILER_ID:GNU>,$<PLATFORM_ID:Windows>>:-static-libstdc++>
    PUBLIC
      $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
  )

  set(CMAKE_BUILD_PARALLEL_LEVEL 8)

  if(NOT TARGET ${project_name})
    message(AUTHOR_WARNING
      "${project_name} is not a target, thus no compiler settings were added."
    )
    return()
  endif()

  if(MSVC AND _files)
    # Group files for MSVC project tree
    foreach(_source IN LISTS _files)
      get_filename_component(_source_path "${_source}" PATH)
      file(RELATIVE_PATH _source_path_rel "${PTGN_ROOT_DIR}/src" "${_source_path}")
      string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
      source_group("${_group_path}" FILES "${_source}")
    endforeach()
  endif()
endfunction()
