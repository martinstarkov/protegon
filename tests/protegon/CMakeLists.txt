cmake_minimum_required(VERSION 3.20)

project(tests)

add_executable(${PROJECT_NAME} ${PROTEGON_TEST_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${PROTEGON_TEST_FILES})

add_protegon_to(${PROJECT_NAME})

IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    create_resource_symlink(${PROJECT_NAME} "resources")
ENDIF()