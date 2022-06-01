set(PROTEGON_DIR        "${CMAKE_CURRENT_SOURCE_DIR}")
set(PROTEGON_SRC_DIR    "${PROTEGON_DIR}/src")
set(PROTEGON_VENDOR_DIR "${PROTEGON_DIR}/vendor")

list(APPEND CMAKE_MODULE_PATH "${PROTEGON_CMAKE_DIR}/cmake")

file(GLOB_RECURSE PROTEGON_FILES CONFIGURE_DEPENDS LIST_DIRECTORIES false 
     "${PROTEGON_SRC_DIR}/*.h"
	 "${PROTEGON_SRC_DIR}/*.cpp")

if (WIN32)
	set(PROTEGON_SETUP_SCRIPT "${PROTEGON_DIR}/scripts/Setup.bat")
	execute_process(COMMAND cmd /C ${PROTEGON_SETUP_SCRIPT})
	set(SDL2_PATH       "${PROTEGON_VENDOR_DIR}/SDL2-2.0.16"      CACHE BOOL "" FORCE)
	set(SDL2_IMAGE_PATH "${PROTEGON_VENDOR_DIR}/SDL2_image-2.0.5" CACHE BOOL "" FORCE)
	set(SDL2_MIXER_PATH "${PROTEGON_VENDOR_DIR}/SDL2_mixer-2.0.4" CACHE BOOL "" FORCE)
	set(SDL2_TTF_PATH   "${PROTEGON_VENDOR_DIR}/SDL2_ttf-2.0.15"  CACHE BOOL "" FORCE)
	foreach(_source IN ITEMS ${PROTEGON_FILES})
		get_filename_component(_source_path "${_source}" PATH)
		file(RELATIVE_PATH _source_path_rel "${PROTEGON_SRC_DIR}" "${_source_path}")
		string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
		source_group("${_group_path}" FILES "${_source}")
	endforeach()
endif()

set(SDL2MAIN_LIBRARY FALSE)

find_package(SDL2       REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf   REQUIRED)
find_package(SDL2_mixer REQUIRED)

add_library(protegon STATIC ${PROTEGON_FILES})

target_link_libraries(protegon PRIVATE SDL2::Main SDL2::Image SDL2::TTF SDL2::Mixer)

target_include_directories(protegon INTERFACE ${PROTEGON_SRC_DIR})

target_include_directories(protegon PRIVATE ${PROTEGON_SRC_DIR}
										    ${SDL2_INCLUDE_DIRS}
											${SDL2_IMAGE_INCLUDE_DIRS}
											${SDL2_MIXER_INCLUDE_DIRS}
											${SDL2_TTF_INCLUDE_DIRS})

get_filename_component(SDL2_LIB_DIR ${SDL2_LIBRARIES} DIRECTORY)
get_filename_component(SDL2_IMAGE_LIB_DIR ${SDL2_IMAGE_LIBRARIES} DIRECTORY)
get_filename_component(SDL2_TTF_LIB_DIR ${SDL2_TTF_LIBRARIES} DIRECTORY)
get_filename_component(SDL2_MIXER_LIB_DIR ${SDL2_MIXER_LIBRARIES} DIRECTORY)
		
file(GLOB_RECURSE DLLS CONFIGURE_DEPENDS
	 "${SDL2_LIB_DIR}/*.dll"
	 "${SDL2_IMAGE_LIB_DIR}/*.dll"
	 "${SDL2_MIXER_LIB_DIR}/*.dll"
	 "${SDL2_TTF_LIB_DIR}/*.dll")

set_property(GLOBAL PROPERTY PROTEGON_DLLS ${DLLS})

function(add_protegon_to TARGET)
    set_target_properties(${TARGET} PROPERTIES LINKER_LANGUAGE CXX)
	set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 17)
    target_link_libraries(${TARGET} protegon)
    # Commands for copying dlls to executable directory on Windows.
    if(WIN32)
		get_property(DLLS GLOBAL PROPERTY PROTEGON_DLLS)
        foreach(item IN LISTS DLLS)
            add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND "${CMAKE_COMMAND}"
                               -E copy_if_different "${item}" $<TARGET_FILE_DIR:${TARGET}>)
        endforeach()
        message(STATUS "Added post build commands to copy SDL dlls for ${TARGET}")
    endif()
endfunction()

mark_as_advanced(PROTEGON_DIR
	             PROTEGON_SRC_DIR
				 PROTEGON_VENDOR_DIR
				 PROTEGON_FILES
				 DLLS
				 protegon)

message(STATUS "Found protegon")