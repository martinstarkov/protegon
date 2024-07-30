cmake_minimum_required(VERSION 3.20)

project(tests)

add_executable(${PROJECT_NAME} ${PROTEGON_TEST_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE "${PROTEGON_ROOT_DIR}/src")

add_protegon_to(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES}
          SDL2::SDL2main
          SDL2::SDL2
          SDL2_image::SDL2_image
          SDL2_ttf::SDL2_ttf
          SDL2_mixer::SDL2_mixer)

IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    create_resource_symlink(${PROJECT_NAME} "resources")
ENDIF()