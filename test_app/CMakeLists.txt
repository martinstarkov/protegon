cmake_minimum_required(VERSION 3.12)

project(test_app)

set(TEST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

include_directories(TEST_SRC_DIR)

file(GLOB_RECURSE TEST_SRC_FILES CONFIGURE_DEPENDS LIST_DIRECTORIES false 
    "${TEST_SRC_DIR}/*.h" "${TEST_SRC_DIR}/*.cpp")

add_executable(${PROJECT_NAME} ${TEST_SRC_FILES})

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/" DESTINATION "${PROJECT_BINARY_DIR}/resources")

add_protegon_to(${PROJECT_NAME})