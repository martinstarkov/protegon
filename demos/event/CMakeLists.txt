cmake_minimum_required(VERSION 3.20)

project(event)

set(TEST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

file(GLOB_RECURSE TEST_SRC_FILES CONFIGURE_DEPENDS LIST_DIRECTORIES false 
    "${TEST_SRC_DIR}/*.h" "${TEST_SRC_DIR}/*.cpp")

add_executable(${PROJECT_NAME} ${TEST_SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${TEST_SRC_DIR})

#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}../../protegon binary_dir)

add_protegon_to(${PROJECT_NAME})

IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/" DESTINATION "${PROJECT_BINARY_DIR}/resources")
ENDIF()