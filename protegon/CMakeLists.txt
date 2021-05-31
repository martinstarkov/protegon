project(protegon)

# Obtain access to SDL2 libraries and includes.

if (WIN32)
    set(SDL2_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vendor/SDL2" CACHE BOOL "" FORCE)
    set(SDL2_IMAGE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vendor/SDL2_image" CACHE BOOL "" FORCE)
    set(SDL2_TTF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vendor/SDL2_ttf" CACHE BOOL "" FORCE)
endif()

set(SDL2MAIN_LIBRARY FALSE)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)

if ("${SDL2_INCLUDE_DIRS}" STREQUAL "")
    message(FATAL_ERROR "Could not find SDL2 library")
endif()

if ("${SDL2_IMAGE_INCLUDE_DIRS}" STREQUAL "")
    message(FATAL_ERROR "Could not find SDL2_image library")
endif()

if ("${SDL2_TTF_INCLUDE_DIRS}" STREQUAL "")
    message(FATAL_ERROR "Could not find SDL2_ttf library")
endif()

# Find all protegon source and header files.

set(SRC_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src")

file(
    GLOB_RECURSE
    PROTEGON_FILES
    "${SRC_DIRECTORY}/*.h"
    "${SRC_DIRECTORY}/*.cpp"
)


include_directories(
    ${SRC_DIRECTORY}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
)

# Create static library.

add_library(
    ${PROJECT_NAME}
    STATIC
    ${PROTEGON_FILES}
)

if (WIN32)

    # Keep original filter structure in MSVC.

    file(
        GLOB_RECURSE _source_list 
        LIST_DIRECTORIES false
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "${SRC_DIRECTORY}/*.cpp"
        "${SRC_DIRECTORY}/*.h"
    )

    foreach(_source IN ITEMS ${_source_list})
        get_filename_component(_source_path "${_source}" PATH)
        string(REPLACE "/" "\\" _source_path_msvc "${_source_path}")
        source_group("${_source_path_msvc}" FILES "${_source}")
    endforeach()

    # Find all SDL dlls required for engine to run.

    get_filename_component(SDL2_DLL_DIRECTORY ${SDL2_LIBRARIES} DIRECTORY)
    get_filename_component(SDL2_IMAGE_DLL_DIRECTORY ${SDL2_IMAGE_LIBRARIES} DIRECTORY)
    get_filename_component(SDL2_TTF_DLL_DIRECTORY ${SDL2_TTF_LIBRARIES} DIRECTORY)
    
    file(
        GLOB_RECURSE
        EXTERNAL_DLLS
        "${SDL2_DLL_DIRECTORY}/*.dll"
        "${SDL2_IMAGE_DLL_DIRECTORY}/*.dll"
        "${SDL2_TTF_DLL_DIRECTORY}/*.dll"
    )

    # Set global variable for external SDL dll copying.

    set_property(GLOBAL PROPERTY PROTEGON_EXTERNAL_DLLS "${EXTERNAL_DLLS}")

endif()

# Define target includes.

target_include_directories(
    ${PROJECT_NAME}
    INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/"
)

# Define target linking.

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
)