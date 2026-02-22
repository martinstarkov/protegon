function(set_compiler_settings project_name)
  target_compile_options(
    ${project_name}
    PUBLIC
      $<$<CXX_COMPILER_ID:MSVC>:/MP>
      # $<$<AND:$<CXX_COMPILER_ID:GNU>,$<PLATFORM_ID:Windows>>:-static-libstdc++>
    PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)

  set(CMAKE_BUILD_PARALLEL_LEVEL 8)

  if(NOT TARGET ${project_name})
    message(
      AUTHOR_WARNING
        "${project_name} is not a target, thus no compiler settings were added."
    )
  endif()
endfunction()
