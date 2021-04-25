cmake_minimum_required(VERSION 3.12)
project(test_app)

# Set C++ language requirement.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add include directories to project.

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Find all cpp files in the src directory of the project.

file(GLOB_RECURSE FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# Create executable from all source files.

add_executable(
    ${PROJECT_NAME}
    ${FILES}
)

# Copy resource files over to binary directory.

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/" DESTINATION ${PROJECT_BINARY_DIR}/resources)

# Add protegon engine to the target executable.
# IMPORTANT NOTE: If using this file as a template for your project, 
# make sure add_subdirectory(<protegon_root_dir>) has been called before.

add_protegon_to(${PROJECT_NAME})