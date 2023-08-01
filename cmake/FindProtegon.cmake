
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
endif()

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

set(MODULES_DIR          ${CMAKE_CURRENT_SOURCE_DIR}/modules)
set(EXTERNAL_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/external)
set(PROTEGON_SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(PROTEGON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

function(download_and_extract url folder_name plain_name)
	if (NOT EXISTS ${EXTERNAL_DIR}/${folder_name})
		get_filename_component(zip_name ${url} NAME)
		get_filename_component(extension ${url} LAST_EXT)
		message(STATUS "Downloading ${url}...")
		set(zip_location ${EXTERNAL_DIR}/${zip_name})
		file(DOWNLOAD ${url} ${zip_location} INACTIVITY_TIMEOUT 60) # SHOW_PROGRESS
		string(TOLOWER "${extension}" lower_extension)
		if ("${lower_extension}" STREQUAL ".zip")
			file(ARCHIVE_EXTRACT INPUT ${zip_location} DESTINATION ${EXTERNAL_DIR})
		elseif("${lower_extension}" STREQUAL ".dmg")
			message(STATUS "Extracting ${folder_name}...")
			execute_process(COMMAND hdiutil attach ${zip_location} -quiet
			                COMMAND cp -r /Volumes/${plain_name}/${folder_name} ${EXTERNAL_DIR}
			                COMMAND hdiutil detach /Volumes/${plain_name} -quiet 
											WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
		endif()
		file(REMOVE ${zip_location})
		message(STATUS "Successfully downloaded and extracted ${plain_name}")
	endif()
endfunction()

# Currently handled by git submodules
#download_and_extract(https://github.com/martinstarkov/ecs/archive/refs/heads/main.zip ecs-main ecs)
#download_and_extract(https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent/archive/refs/tags/v3.11.2.zip nlohmann_json_cmake_fetchcontent-3.11.2 json)

set(ECS_INCLUDE_DIR  ${MODULES_DIR}/ecs/include)
set(JSON_INCLUDE_DIR ${MODULES_DIR}/json/single_include)

if(AUTOMATIC_SDL)
  set(SDL2_VERSION       2.28.1)
  set(SDL2_IMAGE_VERSION 2.6.3)
  set(SDL2_TTF_VERSION   2.20.2)
  set(SDL2_MIXER_VERSION 2.6.3)
	# TODO: Change this to detect between VC and MinGW
	if (WIN32)
    download_and_extract(https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-VC.zip SDL2-${SDL2_VERSION} SDL2)
    download_and_extract(https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/SDL2_image-devel-${SDL2_IMAGE_VERSION}-VC.zip SDL2_image-${SDL2_IMAGE_VERSION} SDL2_image)
    download_and_extract(https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/SDL2_ttf-devel-${SDL2_TTF_VERSION}-VC.zip SDL2_ttf-${SDL2_TTF_VERSION} SDL2_ttf)
    download_and_extract(https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL2_MIXER_VERSION}/SDL2_mixer-devel-${SDL2_MIXER_VERSION}-VC.zip SDL2_mixer-${SDL2_MIXER_VERSION} SDL2_mixer)
    set(SDL2_DIR       ${EXTERNAL_DIR}/SDL2-${SDL2_VERSION}/cmake             CACHE BOOL "" FORCE)
    set(SDL2_image_DIR ${EXTERNAL_DIR}/SDL2_image-${SDL2_IMAGE_VERSION}/cmake CACHE BOOL "" FORCE)
    set(SDL2_ttf_DIR   ${EXTERNAL_DIR}/SDL2_ttf-${SDL2_TTF_VERSION}/cmake     CACHE BOOL "" FORCE)
    set(SDL2_mixer_DIR ${EXTERNAL_DIR}/SDL2_mixer-${SDL2_MIXER_VERSION}/cmake CACHE BOOL "" FORCE)
  elseif(MACOSX)
		download_and_extract(https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.dmg SDL2.framework SDL2)
    download_and_extract(https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/SDL2_image-${SDL2_IMAGE_VERSION}.dmg SDL2_image.framework SDL2_image)
    download_and_extract(https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/SDL2_ttf-${SDL2_TTF_VERSION}.dmg SDL2_ttf.framework SDL2_ttf)
    download_and_extract(https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL2_MIXER_VERSION}/SDL2_mixer-${SDL2_MIXER_VERSION}.dmg SDL2_mixer.framework SDL2_mixer)
    set(SDL2_DIR       ${EXTERNAL_DIR}/SDL2.framework/Resources/CMake       CACHE BOOL "" FORCE)
    set(SDL2_image_DIR ${EXTERNAL_DIR}/SDL2_image.framework/Resources/CMake CACHE BOOL "" FORCE)
    set(SDL2_ttf_DIR   ${EXTERNAL_DIR}/SDL2_ttf.framework/Resources/CMake   CACHE BOOL "" FORCE)
    set(SDL2_mixer_DIR ${EXTERNAL_DIR}/SDL2_mixer.framework/Resources/CMake CACHE BOOL "" FORCE)
  endif()
    list(APPEND CMAKE_MODULE_PATH ${SDL2_DIR})
    list(APPEND CMAKE_MODULE_PATH ${SDL2_image_DIR})
    list(APPEND CMAKE_MODULE_PATH ${SDL2_ttf_DIR})
    list(APPEND CMAKE_MODULE_PATH ${SDL2_mixer_DIR})
endif()

file(GLOB_RECURSE PROTEGON_FILES CONFIGURE_DEPENDS LIST_DIRECTORIES false 
  ${PROTEGON_SRC_DIR}/*.h
	${PROTEGON_SRC_DIR}/*.cpp
	${PROTEGON_INCLUDE_DIR}/*.h)

set(SDL2MAIN_LIBRARY FALSE)

if (WIN32)
	find_package(SDL2       REQUIRED)
	find_package(SDL2_image REQUIRED)
	find_package(SDL2_ttf   REQUIRED)
	find_package(SDL2_mixer REQUIRED)
elseif(MACOSX)
	set(CMAKE_SKIP_BUILD_RPATH FALSE)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH YES)
	set(CMAKE_INSTALL_RPATH ${EXTERNAL_DIR})
	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	find_package(SDL2       REQUIRED NAMES SDL2 PATHS       ${EXTERNAL_DIR}/SDL2.framework/Versions/A/       NO_DEFAULT_PATH)
	find_package(SDL2_image REQUIRED NAMES SDL2_image PATHS ${EXTERNAL_DIR}/SDL2_image.framework/Versions/A/ NO_DEFAULT_PATH)
	find_package(SDL2_ttf   REQUIRED NAMES SDL2_ttf PATHS   ${EXTERNAL_DIR}/SDL2_ttf.framework/Versions/A/   NO_DEFAULT_PATH)
	find_package(SDL2_mixer REQUIRED NAMES SDL2_mixer PATHS ${EXTERNAL_DIR}/SDL2_mixer.framework/Versions/A/ NO_DEFAULT_PATH)
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
	get_filename_component(_source_path ${_source} PATH)
	file(RELATIVE_PATH _source_path_rel ${PROTEGON_SRC_DIR} ${_source_path})
	string(REPLACE "/" "\\" _group_path ${_source_path_rel})
	source_group(${_group_path} FILES ${_source})
	endforeach()
endif()

add_library(protegon STATIC ${PROTEGON_FILES})

target_compile_features(protegon PUBLIC cxx_std_17)
set_target_properties(protegon PROPERTIES CXX_EXTENSIONS OFF)

target_link_libraries(protegon
	PRIVATE
	SDL2::SDL2
	SDL2_image::SDL2_image
	SDL2_ttf::SDL2_ttf
	SDL2_mixer::SDL2_mixer)

target_include_directories(protegon
	INTERFACE
	${PROTEGON_INCLUDE_DIR}
	${ECS_INCLUDE_DIR}
	${JSON_INCLUDE_DIR}
	PRIVATE
	${PROTEGON_INCLUDE_DIR}
	${PROTEGON_SRC_DIR}
	${ECS_INCLUDE_DIR}
	${JSON_INCLUDE_DIR}
	${SDL2_INCLUDE_DIRS}
	${SDL2_IMAGE_INCLUDE_DIRS}
	${SDL2_MIXER_INCLUDE_DIRS}
	${SDL2_TTF_INCLUDE_DIRS})

if (WIN32)
	get_target_property(SDL2_DLL       SDL2::SDL2 	          IMPORTED_LOCATION)
	get_target_property(SDL2_IMAGE_DLL SDL2_image::SDL2_image IMPORTED_LOCATION)
	get_target_property(SDL2_TTF_DLL   SDL2_ttf::SDL2_ttf     IMPORTED_LOCATION)
	get_target_property(SDL2_MIXER_DLL SDL2_mixer::SDL2_mixer IMPORTED_LOCATION)

	get_filename_component(SDL2_LIB_DIR       ${SDL2_DLL}       DIRECTORY)
	get_filename_component(SDL2_IMAGE_LIB_DIR ${SDL2_IMAGE_DLL} DIRECTORY)
	get_filename_component(SDL2_TTF_LIB_DIR   ${SDL2_TTF_DLL}   DIRECTORY)
	get_filename_component(SDL2_MIXER_LIB_DIR ${SDL2_MIXER_DLL} DIRECTORY)

	file(GLOB_RECURSE DLLS CONFIGURE_DEPENDS
		${SDL2_LIBDIR}/*.dll
		${SDL2_IMAGE_LIB_DIR}/*.dll
		${SDL2_TTF_LIB_DIR}/*.dll
		${SDL2_MIXER_LIB_DIR}/*.dll)

	set_property(GLOBAL PROPERTY PROTEGON_DLLS ${DLLS})
endif()

function(add_protegon_to TARGET)
    set_target_properties(${TARGET} PROPERTIES LINKER_LANGUAGE CXX)
	set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 17)
    target_link_libraries(${TARGET} protegon)
	target_include_directories(${TARGET} PRIVATE ${ECS_INCLUDE_DIR})
	target_include_directories(${TARGET} PRIVATE ${JSON_INCLUDE_DIR})
	if(MACOSX)
		set_target_properties(${TARGET} PROPERTIES
			XCODE_GENERATE_SCHEME TRUE
			XCODE_SCHEME_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
	endif()
    # Commands for copying dlls to executable directory on Windows.
    if(WIN32)
		get_property(DLLS GLOBAL PROPERTY PROTEGON_DLLS)
        foreach(item IN LISTS DLLS)
            add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND}
                               -E copy_if_different ${item} $<TARGET_FILE_DIR:${TARGET}>)
        endforeach()
        #message(STATUS "Added post build commands to copy SDL dlls for ${TARGET}")
		mark_as_advanced(DLLS)
    endif()
endfunction()

function(create_protegon_link TARGET_NAME DIR_NAME)
	set(SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME})
	set(DESTINATION_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${DIR_NAME})
	if (NOT EXISTS ${DESTINATION_DIRECTORY})
		if(WIN32)
			file(TO_NATIVE_PATH ${DESTINATION_DIRECTORY} _dst_dir)
			file(TO_NATIVE_PATH ${SOURCE_DIRECTORY} _src_dir)
			execute_process(COMMAND cmd.exe /c mklink /J ${_dst_dir} ${_src_dir})
		elseif(MACOSX)
			#message(STATUS "Creating Symlink from ${SOURCE_DIRECTORY} to ${DESTINATION_DIRECTORY}")
			execute_process(COMMAND ln -s ${SOURCE_DIRECTORY} ${DESTINATION_DIRECTORY})
		elseif(UNIX)
			execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${SOURCE_DIRECTORY} ${DESTINATION_DIRECTORY})
		endif()
	endif()
endfunction()

mark_as_advanced(
	PROTEGON_SRC_DIR
	PROTEGON_INCLUDE_DIR
	EXTERNAL_DIR
	PROTEGON_FILES
	protegon)

message(STATUS "Found protegon")