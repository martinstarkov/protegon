cmake_minimum_required(VERSION 3.20)

project(player_platforming)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

file(
  GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  LIST_DIRECTORIES false
  "${SRC_DIR}/*.h" "${SRC_DIR}/*.cpp")

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE "${PROTEGON_ROOT_DIR}/src")
target_include_directories(${PROJECT_NAME} PRIVATE ${SRC_DIR})

add_protegon_to(${PROJECT_NAME})

if(EMSCRIPTEN)
  if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(ECXXFLAGS "-O0")
  else()
    set(ECXXFLAGS "-O3")
  endif()
  set(ASSETS_DIRECTORY "resources")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${ASSETS_DIRECTORY}")
    set(DEST_SYMLINK ${CMAKE_CURRENT_BINARY_DIR})
    message(STATUS "Creating resources symlink to ${DEST_SYMLINK}")
    create_resource_symlink(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}
                            ${DEST_SYMLINK} ${ASSETS_DIRECTORY})
  else()
    message(
      STATUS
        "Failed to create resources symlink to ${CMAKE_CURRENT_SOURCE_DIR}/${ASSETS_DIRECTORY}"
    )
  endif()
  set(SHELL_HTML_FILE "${PROTEGON_ROOT_DIR}/emscripten/shell.html")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  # Check if sdl is needed here.
  set(ECXXFLAGS
      "${ECXXFLAGS} -std=c++20 --use-port=sdl2 --use-port=sdl2_image:formats=bmp,png,xpm,jpg --use-port=sdl2_mixer --use-port=sdl2_ttf"
  )
  set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
      LINK_FLAGS
      "${ECXXFLAGS} --shell-file ${SHELL_HTML_FILE} --preload-file ${ASSETS_DIRECTORY} -s FULL_ES3=1 -s ALLOW_MEMORY_GROWTH=1 -s WARN_ON_UNDEFINED_SYMBOLS=1 -s NO_EXIT_RUNTIME=1 -s AGGRESSIVE_VARIABLE_ELIMINATION=1"
  )
  set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "${ECXXFLAGS}")
  set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "index")
else()
  target_link_libraries(${PROJECT_NAME})

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    create_resource_symlink(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}
                            ${CMAKE_CURRENT_BINARY_DIR} "resources")
  endif()
endif()
