function(add_clang_tidy)
  #set(CMAKE_CXX_CLANG_TIDY clang-tidy -extra-arg=-Wno-unknown-warning-option)
endfunction()

function(add_clang_format_target sources exe_sources headers)
    # if(MSVC)
    #   add_custom_target(clang-format
    #     COMMAND clang-format
    #     -i ${sources} ${headers}
    #     WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
    # else()
    #   if(NOT ${PROJECT_NAME}_CLANG_FORMAT_BINARY)
    #     find_program(${PROJECT_NAME}_CLANG_FORMAT_BINARY clang-format)
    #   endif()
    #   if(${PROJECT_NAME}_CLANG_FORMAT_BINARY)
    #     add_custom_target(clang-format
    #         COMMAND ${${PROJECT_NAME}_CLANG_FORMAT_BINARY}
    #         -i ${sources} ${headers}
    #         WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
    #   endif()
    # endif()

    # message(STATUS "Format project using clang-format target")
endfunction()