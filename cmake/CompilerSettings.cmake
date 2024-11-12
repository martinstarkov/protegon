function(set_compiler_settings project_name)   
    if(MSVC)
        add_compile_options(/MP /bigobj)
    endif()

    set(CMAKE_BUILD_PARALLEL_LEVEL 8)

    if(NOT TARGET ${project_name})
        message(AUTHOR_WARNING "${project_name} is not a target, thus no compiler settings were added.")
    endif()
endfunction()
