project(test_app)

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Create executable from all source files.

file(GLOB_RECURSE FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(
    ${PROJECT_NAME}
    ${FILES}
)

# Copy resource files over to binary directory.

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/" DESTINATION ${PROJECT_BINARY_DIR}/resources)

# Add protegon engine to the executable.

find_package(PROTEGON REQUIRED)
add_protegon_to(${PROJECT_NAME})