function(set_compiler_settings project_name root_dir)
  if(NOT TARGET ${project_name})
    message(AUTHOR_WARNING
      "${project_name} is not a target, thus no compiler settings were added."
    )
    return()
  endif()

  if(MSVC)
    target_compile_options(
      ${project_name}
      PUBLIC
        /Zc:preprocessor
      PRIVATE
        /JMC
        /MP
        /bigobj
    )
  endif()

  if(MSVC AND ARGN)
    foreach(_source IN LISTS ARGN)
      get_filename_component(_source_path "${_source}" PATH)
      file(RELATIVE_PATH _source_path_rel "${root_dir}" "${_source_path}")

      if(_source_path_rel STREQUAL "" OR _source_path_rel STREQUAL ".")
        set(_group_path "\\")
      else()
        string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
      endif()

      source_group("${_group_path}" FILES "${_source}")
    endforeach()
  endif()
endfunction()