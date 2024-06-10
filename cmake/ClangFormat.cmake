cmake_minimum_required(VERSION 3.16)

function(add_clang_tidy)
  if (MSVC)
    find_program(CLANG_TIDY_COMMAND NAMES clang-tidy NO_CACHE)
    if (CLANG_TIDY_COMMAND)
      set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_COMMAND} -extra-arg=-Wno-unknown-warning-option)
    endif()
  else()
    find_program(CLANGTIDY clang-tidy)
    if(CLANGTIDY)
      set(CMAKE_CXX_CLANG_TIDY ${TIDY_COMMAND} -extra-arg=-Wno-unknown-warning-option)
    endif()
  endif()

  if (CMAKE_CXX_CLANG_TIDY)
    message(STATUS "Clang-Tidy finished setting up.")
  else()
    message(SEND_ERROR "Clang-Tidy requested but executable not found.")
  endif()
endfunction()

function(add_clang_format_target sources exe_sources headers)
    if(MSVC)
      add_custom_target(clang-format
        COMMAND clang-format
        -i ${sources} ${headers}
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
    else()
      if(NOT ${PROJECT_NAME}_CLANG_FORMAT_BINARY)
        find_program(${PROJECT_NAME}_CLANG_FORMAT_BINARY clang-format)
      endif()
      if(${PROJECT_NAME}_CLANG_FORMAT_BINARY)
        add_custom_target(clang-format
            COMMAND ${${PROJECT_NAME}_CLANG_FORMAT_BINARY}
            -i ${sources} ${headers}
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
      endif()
    endif()

    message(STATUS "Format the project using the `clang-format` target (i.e: cmake --build build --target clang-format).\n")
endfunction()