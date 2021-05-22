cmake_minimum_required(VERSION 3.12)
project(protegon)

# Enforce C++17 version requirement.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add package find files to cmake module path.

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(protegon)

# Macro for easily adding protegon library to targets.

macro(add_protegon_to TARGET_NAME)
  target_link_libraries(${PROJECT_NAME} protegon)
  # Commands for copying dlls to executable directory on Windows.
  if(WIN32)
      get_property(PROTEGON_EXTERNAL_DLLS_TEMP GLOBAL PROPERTY PROTEGON_EXTERNAL_DLLS)
      foreach(item IN LISTS PROTEGON_EXTERNAL_DLLS_TEMP)
          add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
              COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${item}" $<TARGET_FILE_DIR:${TARGET_NAME}>
          )
      endforeach()
      message(STATUS "Added post build commands to copy SDL dlls to ${TARGET_NAME}")
  endif()
  message(STATUS "Added protegon library to ${TARGET_NAME}")
endmacro()

# Add option to include test application.

option(TEST_APP "TEST_APP" OFF)

if(TEST_APP)
    add_subdirectory(test_app)
endif()
