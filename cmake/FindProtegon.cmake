if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
endif()

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

set(PROTEGON_DIR         "${CMAKE_CURRENT_SOURCE_DIR}")
set(PROTEGON_SRC_DIR     "${PROTEGON_DIR}/src")
set(PROTEGON_INCLUDE_DIR "${PROTEGON_DIR}/include")
set(PROTEGON_VENDOR_DIR  "${PROTEGON_DIR}/external")

# Ensure that these versions are consistent with those found in the protegon/scripts/setup.py file.
set(SDL2_RELEASE_VERSION       "2.26.5")
set(SDL2_IMAGE_RELEASE_VERSION "2.6.3")
set(SDL2_TTF_RELEASE_VERSION   "2.20.2")
set(SDL2_MIXER_RELEASE_VERSION "2.6.3")

include(FetchContent)

FetchContent_Declare(ecs 
					 GIT_REPOSITORY https://github.com/martinstarkov/ecs.git
					 GIT_TAG main)
FetchContent_MakeAvailable(ecs)

FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
  GIT_PROGRESS TRUE
  GIT_SHALLOW TRUE
  GIT_TAG v3.11.2)
FetchContent_MakeAvailable(nlohmann_json)

set(ECS_INCLUDE_DIR "${ecs_SOURCE_DIR}/include")
set(JSON_INCLUDE_DIR "${nlohmann_json_SOURCE_DIR}/single_include")

file(GLOB_RECURSE PROTEGON_FILES CONFIGURE_DEPENDS LIST_DIRECTORIES false 
     "${PROTEGON_SRC_DIR}/*.h"
	 "${PROTEGON_SRC_DIR}/*.cpp"
	 "${PROTEGON_INCLUDE_DIR}/*.h")

if (AUTOMATIC_SDL)
	if (WIN32)
		# TODO: Figure out if this is needed or can it be run directly like in Mac case.
		set(PROTEGON_SETUP_SCRIPT "${PROTEGON_DIR}/scripts/setup.bat")
		execute_process(COMMAND cmd /C ${PROTEGON_SETUP_SCRIPT})
		set(SDL2_PATH       "${PROTEGON_VENDOR_DIR}/SDL2-${SDL2_RELEASE_VERSION}"             CACHE BOOL "" FORCE)
		set(SDL2_IMAGE_PATH "${PROTEGON_VENDOR_DIR}/SDL2_image-${SDL2_IMAGE_RELEASE_VERSION}" CACHE BOOL "" FORCE)
		set(SDL2_TTF_PATH   "${PROTEGON_VENDOR_DIR}/SDL2_ttf-${SDL2_TTF_RELEASE_VERSION}"     CACHE BOOL "" FORCE)
		set(SDL2_MIXER_PATH "${PROTEGON_VENDOR_DIR}/SDL2_mixer-${SDL2_MIXER_RELEASE_VERSION}" CACHE BOOL "" FORCE)
	elseif(MACOSX)
		set(PROTEGON_SETUP_SCRIPT "${PROTEGON_DIR}/scripts/setup.py")
		execute_process(COMMAND python3 ${PROTEGON_SETUP_SCRIPT})
		list(APPEND CMAKE_MODULE_PATH "${PROTEGON_VENDOR_DIR}/SDL2.framework/Resources/CMake")
		list(APPEND CMAKE_MODULE_PATH "${PROTEGON_VENDOR_DIR}/SDL2_image.framework/Resources/CMake")
		list(APPEND CMAKE_MODULE_PATH "${PROTEGON_VENDOR_DIR}/SDL2_ttf.framework/Resources/CMake")
		list(APPEND CMAKE_MODULE_PATH "${PROTEGON_VENDOR_DIR}/SDL2_mixer.framework/Resources/CMake")
		set(SDL2_DIR "${PROTEGON_VENDOR_DIR}/SDL2.framework/Resources/CMake" CACHE BOOL "" FORCE)
		set(SDL2_image_DIR "${PROTEGON_VENDOR_DIR}/SDL2_image.framework/Resources/CMake" CACHE BOOL "" FORCE)
		set(SDL2_ttf_DIR "${PROTEGON_VENDOR_DIR}/SDL2_ttf.framework/Resources/CMake" CACHE BOOL "" FORCE)
		set(SDL2_mixer_DIR "${PROTEGON_VENDOR_DIR}/SDL2_mixer.framework/Resources/CMake" CACHE BOOL "" FORCE)
	endif()
endif()

set(SDL2MAIN_LIBRARY FALSE)

if (WIN32)
	find_package(SDL2       REQUIRED)
	find_package(SDL2_image REQUIRED)
	find_package(SDL2_ttf   REQUIRED)
	find_package(SDL2_mixer REQUIRED)
elseif(MACOSX)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH YES)
	set(CMAKE_INSTALL_RPATH "${PROTEGON_VENDOR_DIR}")
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	find_package(SDL2       REQUIRED NAMES SDL2 PATHS "${PROTEGON_VENDOR_DIR}/SDL2.framework/Versions/A/" NO_DEFAULT_PATH)
	find_package(SDL2_image REQUIRED NAMES SDL2_image PATHS "${PROTEGON_VENDOR_DIR}/SDL2_image.framework/Versions/A/" NO_DEFAULT_PATH)
	find_package(SDL2_ttf   REQUIRED NAMES SDL2_ttf PATHS "${PROTEGON_VENDOR_DIR}/SDL2_ttf.framework/Versions/A/" NO_DEFAULT_PATH)
	find_package(SDL2_mixer REQUIRED NAMES SDL2_mixer PATHS "${PROTEGON_VENDOR_DIR}/SDL2_mixer.framework/Versions/A/" NO_DEFAULT_PATH)
endif()

MACRO(SUBDIRLIST result curdir)
 FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
 SET(dirlist "")
 FOREACH(child ${children})
  IF(IS_DIRECTORY ${curdir}/${child})
   LIST(APPEND dirlist ${child})
  ENDIF()
 ENDFOREACH()
 SET(${result} ${dirlist})
ENDMACRO()

if (WIN32)
	foreach(_source IN ITEMS ${PROTEGON_FILES})
	get_filename_component(_source_path "${_source}" PATH)
	file(RELATIVE_PATH _source_path_rel "${PROTEGON_SRC_DIR}" "${_source_path}")
	string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
	source_group("${_group_path}" FILES "${_source}")
	endforeach()
endif()

add_library(protegon STATIC ${PROTEGON_FILES})

if (WIN32)
target_link_libraries(protegon PRIVATE SDL2::Main 
                                       SDL2::Image 
									   SDL2::TTF 
									   SDL2::Mixer)
elseif(MACOSX)
target_link_libraries(protegon PRIVATE SDL2::SDL2
                                       SDL2_image::SDL2_image
									   SDL2_ttf::SDL2_ttf
									   SDL2_mixer::SDL2_mixer)							   
endif()

target_include_directories(protegon INTERFACE ${PROTEGON_INCLUDE_DIR}
											  ${ECS_INCLUDE_DIR}
											  ${JSON_INCLUDE_DIR})

target_include_directories(protegon PRIVATE ${PROTEGON_INCLUDE_DIR}
											${PROTEGON_SRC_DIR}
											${ECS_INCLUDE_DIR}
											${JSON_INCLUDE_DIR}
										    ${SDL2_INCLUDE_DIRS}
											${SDL2_IMAGE_INCLUDE_DIRS}
											${SDL2_MIXER_INCLUDE_DIRS}
											${SDL2_TTF_INCLUDE_DIRS})

if (WIN32)
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
endif()

function(add_protegon_to TARGET)
    set_target_properties(${TARGET} PROPERTIES LINKER_LANGUAGE CXX)
	set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 17)
    target_link_libraries(${TARGET} protegon)
	target_include_directories(${TARGET} PRIVATE ${ECS_INCLUDE_DIR})
	target_include_directories(${TARGET} PRIVATE ${JSON_INCLUDE_DIR})
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

function(create_protegon_link TARGET_NAME DIR_NAME)
	if(WIN32)
		get_filename_component(real_path "${DIR_NAME}" REALPATH)
		string(REPLACE "/" "\\" target_path "${real_path}")
		execute_process(
			COMMAND cmd /C mklink /J ${DIR_NAME} "${target_path}"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
	else()
		if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${DIR_NAME})
			message(STATUS "Creating Symlink from ${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME} to ${CMAKE_CURRENT_BINARY_DIR}/${DIR_NAME}")
			execute_process(COMMAND ln -s ${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME} ${CMAKE_CURRENT_BINARY_DIR}/${DIR_NAME})
		endif()
		set_target_properties(${TARGET_NAME} PROPERTIES 
        XCODE_GENERATE_SCHEME TRUE
        XCODE_SCHEME_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
	endif()
endfunction()

if (WIN32)
	mark_as_advanced(DLLS)
endif()

mark_as_advanced(PROTEGON_DIR
	             PROTEGON_SRC_DIR
				 PROTEGON_VENDOR_DIR
				 PROTEGON_FILES
				 protegon)

message(STATUS "Found protegon")