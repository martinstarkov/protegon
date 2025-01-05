function(set_compiler_settings project_name)   
    if(MSVC)
        target_compile_options(${project_name} PRIVATE /MP /bigobj)
    endif()

    set(CMAKE_BUILD_PARALLEL_LEVEL 8)

    if(NOT TARGET ${project_name})
        message(AUTHOR_WARNING "${project_name} is not a target, thus no compiler settings were added.")
    endif()
endfunction()
