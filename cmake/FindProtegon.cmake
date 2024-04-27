cmake_minimum_required(VERSION 3.20)

option(SHARED_SDL2_LIBS "Statically Link SDL2 libs to protegon" OFF)

if (WIN32)
  option(ENABLE_CONSOLE "Enable console executable" OFF)
endif()

set(BUILD_SHARED_LIBS ${SHARED_SDL2_LIBS} CACHE BOOL "" FORCE)

set(PROTEGON_DIR         ${PROJECT_SOURCE_DIR} CACHE BOOL "" FORCE)
set(EXTERNAL_DIR         ${PROTEGON_DIR}/external)
set(PROTEGON_SRC_DIR     ${PROTEGON_DIR}/src)
set(PROTEGON_INCLUDE_DIR ${PROTEGON_DIR}/include)
set(MODULES_DIR          ${PROTEGON_DIR}/modules)
set(ECS_INCLUDE_DIR      ${MODULES_DIR}/ecs/include)
set(JSON_INCLUDE_DIR     ${MODULES_DIR}/json/single_include)

set(SDL2_VERSION       2.30.0)
set(SDL2_IMAGE_VERSION 2.8.2)
set(SDL2_MIXER_VERSION 2.8.0)
set(SDL2_TTF_VERSION   2.22.0)

file(GLOB_RECURSE PROTEGON_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false 
	${PROTEGON_SRC_DIR}/*.cpp)
file(GLOB_RECURSE PROTEGON_HEADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false 
  ${PROTEGON_SRC_DIR}/*.h
  ${PROTEGON_INCLUDE_DIR}/*.h)
set(PROTEGON_FILES ${PROTEGON_SOURCES} ${PROTEGON_HEADERS})
  
# Group files for MSVC project tree
if (MSVC)
  foreach(_source IN ITEMS ${PROTEGON_FILES})
    get_filename_component(_source_path ${_source} PATH)
    file(RELATIVE_PATH _source_path_rel ${PROTEGON_SRC_DIR} ${_source_path})
    string(REPLACE "/" "\\" _group_path ${_source_path_rel})
    source_group(${_group_path} FILES ${_source})
  endforeach()
endif()

add_library(protegon STATIC ${PROTEGON_SOURCES})
target_compile_features(protegon PUBLIC cxx_std_17)
set_target_properties(protegon PROPERTIES CXX_EXTENSIONS OFF)

function(download_and_extract url archive_name plain_name)
  if (NOT EXISTS ${EXTERNAL_DIR}/${archive_name})
    get_filename_component(zip_name ${url} NAME)
    get_filename_component(extension ${url} LAST_EXT)
    message(STATUS "Downloading ${url}...")
    set(zip_location ${EXTERNAL_DIR}/${zip_name})
    file(DOWNLOAD ${url} ${zip_location} INACTIVITY_TIMEOUT 60) # SHOW_PROGRESS
    string(TOLOWER "${extension}" lower_extension)
    if ("${lower_extension}" STREQUAL ".zip")
      file(ARCHIVE_EXTRACT INPUT ${zip_location} DESTINATION ${EXTERNAL_DIR})
    elseif("${lower_extension}" STREQUAL ".dmg" AND NOT plain_name STREQUAL "")
      message(STATUS "Extracting ${archive_name}...")
      execute_process(COMMAND hdiutil attach ${zip_location} -quiet
                      COMMAND cp -r /Volumes/${plain_name}/${archive_name} ${EXTERNAL_DIR}
                      COMMAND hdiutil detach /Volumes/${plain_name} -quiet 
                      WORKING_DIRECTORY ${PROTEGON_DIR})
    endif()
    file(REMOVE ${zip_location})
    message(STATUS "Successfully downloaded and extracted ${archive_name}")
  endif()
endfunction()

set(SDL2MAIN_LIBRARY FALSE)

set(SDL_LOCATION ${EXTERNAL_DIR})

if(WIN32)
  if(MSVC)
    if (ENABLE_CONSOLE)
      target_link_options(protegon PUBLIC "/SUBSYSTEM:CONSOLE")
    else()
      target_link_options(protegon PUBLIC "/SUBSYSTEM:WINDOWS")
      target_compile_definitions(protegon INTERFACE main=WinMain)
    endif()
    set(WIN_COMPILER_POSTFIX "VC")
    # Updates CMAKE_PREFIX_PATH paths to include library architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(CMAKE_LIBRARY_ARCHITECTURE "x64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(CMAKE_LIBRARY_ARCHITECTURE "x86")
    endif()
    if (NOT BUILD_SHARED_LIBS)
      set(SDL_LOCATION ${EXTERNAL_DIR}/SDL2-static-libs-main)
    endif()
  elseif(MINGW)
    if (ENABLE_CONSOLE)
      target_link_options(protegon PUBLIC "-mconsole")
    else()
      target_link_options(protegon PUBLIC "-mwindows")
    endif()
    set(WIN_COMPILER_POSTFIX "mingw")
  endif()
endif()

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

if (WIN32)
  if (MSVC AND NOT BUILD_SHARED_LIBS) # Static MSVC

    set(SDL2_URL "https://github.com/martinstarkov/SDL2-static-libs/archive/refs/heads/main.zip")
    download_and_extract(${SDL2_URL} SDL2-static-libs-main "")

  else() # MinGW and shared MSVC

    set(SDL2_URL       "https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-${WIN_COMPILER_POSTFIX}.zip")
    set(SDL2_IMAGE_URL "https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/SDL2_image-devel-${SDL2_IMAGE_VERSION}-${WIN_COMPILER_POSTFIX}.zip")
    set(SDL2_MIXER_URL "https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL2_MIXER_VERSION}/SDL2_mixer-devel-${SDL2_MIXER_VERSION}-${WIN_COMPILER_POSTFIX}.zip")
    set(SDL2_TTF_URL   "https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/SDL2_ttf-devel-${SDL2_TTF_VERSION}-${WIN_COMPILER_POSTFIX}.zip")
    
    download_and_extract(${SDL2_URL} SDL2-${SDL2_VERSION} SDL2)
    download_and_extract(${SDL2_IMAGE_URL} SDL2_image-${SDL2_IMAGE_VERSION} SDL2_image)
    download_and_extract(${SDL2_MIXER_URL} SDL2_mixer-${SDL2_MIXER_VERSION} SDL2_mixer)
    download_and_extract(${SDL2_TTF_URL} SDL2_ttf-${SDL2_TTF_VERSION} SDL2_ttf)

  endif()

  set(SDL2_DIR       ${SDL_LOCATION}/SDL2-${SDL2_VERSION}/cmake             CACHE BOOL "" FORCE)
  set(SDL2_image_DIR ${SDL_LOCATION}/SDL2_image-${SDL2_IMAGE_VERSION}/cmake CACHE BOOL "" FORCE)
  set(SDL2_ttf_DIR   ${SDL_LOCATION}/SDL2_ttf-${SDL2_TTF_VERSION}/cmake     CACHE BOOL "" FORCE)
  set(SDL2_mixer_DIR ${SDL_LOCATION}/SDL2_mixer-${SDL2_MIXER_VERSION}/cmake CACHE BOOL "" FORCE)

elseif(APPLE)

  set(SDL2_URL       "https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.dmg")
  set(SDL2_IMAGE_URL "https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/SDL2_image-${SDL2_IMAGE_VERSION}.dmg")
  set(SDL2_MIXER_URL "https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL2_MIXER_VERSION}/SDL2_mixer-${SDL2_MIXER_VERSION}.dmg")
  set(SDL2_TTF_URL   "https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/SDL2_ttf-${SDL2_TTF_VERSION}.dmg")

  download_and_extract(${SDL2_URL} SDL2.framework SDL2)
  download_and_extract(${SDL2_IMAGE_URL} SDL2_image.framework SDL2_image)
  download_and_extract(${SDL2_MIXER_URL} SDL2_ttf.framework SDL2_mixer)
  download_and_extract(${SDL2_TTF_URL} SDL2_mixer.framework SDL2_ttf)

  set(SDL2_DIR       ${SDL_LOCATION}/SDL2.framework/Resources/CMake       CACHE BOOL "" FORCE)
  set(SDL2_image_DIR ${SDL_LOCATION}/SDL2_image.framework/Resources/CMake CACHE BOOL "" FORCE)
  set(SDL2_ttf_DIR   ${SDL_LOCATION}/SDL2_ttf.framework/Resources/CMake   CACHE BOOL "" FORCE)
  set(SDL2_mixer_DIR ${SDL_LOCATION}/SDL2_mixer.framework/Resources/CMake CACHE BOOL "" FORCE)

elseif(UNIX)

  execute_process(COMMAND sudo apt-get install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev)

endif()

if (WIN32 OR APPLE)
  list(APPEND CMAKE_MODULE_PATH ${SDL2_DIR})
  list(APPEND CMAKE_MODULE_PATH ${SDL2_image_DIR})
  list(APPEND CMAKE_MODULE_PATH ${SDL2_ttf_DIR})
  list(APPEND CMAKE_MODULE_PATH ${SDL2_mixer_DIR})
endif()
    
if(NOT APPLE)

  find_package(SDL2       REQUIRED)
  find_package(SDL2_image REQUIRED)
  find_package(SDL2_ttf   REQUIRED)
  find_package(SDL2_mixer REQUIRED)

else()

  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH YES)
  set(CMAKE_INSTALL_RPATH ${SDL_LOCATION})
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

  find_package(SDL2       REQUIRED NAMES SDL2       PATHS ${SDL_LOCATION}/SDL2.framework/Versions/A/       NO_DEFAULT_PATH)
  find_package(SDL2_image REQUIRED NAMES SDL2_image PATHS ${SDL_LOCATION}/SDL2_image.framework/Versions/A/ NO_DEFAULT_PATH)
  find_package(SDL2_ttf   REQUIRED NAMES SDL2_ttf   PATHS ${SDL_LOCATION}/SDL2_ttf.framework/Versions/A/   NO_DEFAULT_PATH)
  find_package(SDL2_mixer REQUIRED NAMES SDL2_mixer PATHS ${SDL_LOCATION}/SDL2_mixer.framework/Versions/A/ NO_DEFAULT_PATH)

endif()

set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)

set(FREETYPE_LIB "")

set(SDL_TARGET_FILES "$<TARGET_FILE:SDL2::SDL2>"
                     "$<TARGET_FILE:SDL2_image::SDL2_image>"
                     "$<TARGET_FILE:SDL2_ttf::SDL2_ttf>"
                     "$<TARGET_FILE:SDL2_mixer::SDL2_mixer>")

if(NOT BUILD_SHARED_LIBS)
  if(MSVC)
    set(FREETYPE_LIB "${SDL_LOCATION}/SDL2_ttf-${SDL2_TTF_VERSION}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/freetype.lib")
    list(APPEND SDL_TARGET_FILES ${FREETYPE_LIB})
    add_custom_command(TARGET protegon POST_BUILD
      COMMAND ${CMAKE_AR} /NOLOGO /OUT:"$<TARGET_FILE:protegon>"
      "$<TARGET_FILE:protegon>" ${TARGET_FILES} COMMAND_EXPAND_LISTS)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|GNU)$") # TODO: Check if works with Clang "^(Clang|GNU)$")
    list(APPEND SDL_TARGET_FILES freetype)
    if (MINGW)
    	target_link_options(protegon PUBLIC "-static")
    endif()
    add_custom_command(
      TARGET protegon
      POST_BUILD
      COMMAND ${CMAKE_AR}
      VERBATIM
      r
      "$<TARGET_FILE:protegon>"
      ${TARGET_FILES}
      COMMAND_EXPAND_LISTS)
  endif()
endif()

if(NOT BUILD_SHARED_LIBS AND MSVC)
  # target_link_libraries(SDL2 PRIVATE "-nodefaultlib:MSVCRT")
  target_link_options(protegon PUBLIC $<IF:$<CONFIG:Debug>,/NODEFAULTLIB:MSVCRT,>)
endif()

if(MINGW AND NOT BUILD_SHARED_LIBS)
  target_link_libraries(protegon PRIVATE
    SDL2::SDL2-static
    SDL2_image::SDL2_image-static
    SDL2_mixer::SDL2_mixer-static
    SDL2_ttf::SDL2_ttf-static
    ${FREETYPE_LIB})
else()
  target_link_libraries(protegon PRIVATE
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
    SDL2_ttf::SDL2_ttf
    ${FREETYPE_LIB})
endif()

if(MINGW)
  target_link_libraries(protegon PUBLIC shlwapi)
endif()

target_include_directories(protegon PUBLIC
	${PROTEGON_INCLUDE_DIR}
	${ECS_INCLUDE_DIR}
	${JSON_INCLUDE_DIR}
  PRIVATE
	${PROTEGON_SRC_DIR})

# Add d to debug static lib files to differentiate them from release
set_target_properties(protegon PROPERTIES DEBUG_POSTFIX d)

target_include_directories(protegon PUBLIC
  $<BUILD_INTERFACE:${PROTEGON_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>)

# Copy SDL dlls to parent exe directory
if(WIN32 AND BUILD_SHARED_LIBS AND NOT CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  get_target_property(SDL2_DLL       SDL2::SDL2 	          IMPORTED_LOCATION)
  get_target_property(SDL2_IMAGE_DLL SDL2_image::SDL2_image IMPORTED_LOCATION)
  get_target_property(SDL2_TTF_DLL   SDL2_ttf::SDL2_ttf     IMPORTED_LOCATION)
  get_target_property(SDL2_MIXER_DLL SDL2_mixer::SDL2_mixer IMPORTED_LOCATION)

  get_filename_component(SDL2_LIB_DIR       ${SDL2_DLL}       DIRECTORY)
  get_filename_component(SDL2_IMAGE_LIB_DIR ${SDL2_IMAGE_DLL} DIRECTORY)
  get_filename_component(SDL2_TTF_LIB_DIR   ${SDL2_TTF_DLL}   DIRECTORY)
  get_filename_component(SDL2_MIXER_LIB_DIR ${SDL2_MIXER_DLL} DIRECTORY)

  file(GLOB_RECURSE DLLS CONFIGURE_DEPENDS
    ${SDL2_LIB_DIR}/*.dll
    ${SDL2_IMAGE_LIB_DIR}/*.dll
    ${SDL2_TTF_LIB_DIR}/*.dll
    ${SDL2_MIXER_LIB_DIR}/*.dll)

  set_property(GLOBAL PROPERTY PROTEGON_DLLS ${DLLS})
endif()

function(add_protegon_to TARGET)
  target_link_libraries(${TARGET} PRIVATE protegon)
  if(MACOSX)
    set_target_properties(${TARGET} PROPERTIES
      XCODE_GENERATE_SCHEME TRUE
      XCODE_SCHEME_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  endif()
  # Commands for copying dlls to executable directory on Windows.
  if(WIN32 AND BUILD_SHARED_LIBS)
    get_property(DLLS GLOBAL PROPERTY PROTEGON_DLLS)
    add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND}
                        -E copy_if_different ${DLLS} $<TARGET_FILE_DIR:${TARGET}>
                        COMMAND_EXPAND_LISTS)
    mark_as_advanced(DLLS)
  endif()
endfunction()

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)

  include(GNUInstallDirs)

  if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/install CACHE PATH "" FORCE)
  endif()

  install(TARGETS protegon
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

  install(
    FILES 
    ${PROTEGON_HEADERS}
    DESTINATION
    ${CMAKE_INSTALL_INCLUDEDIR}/protegon)

  if (WIN32 AND BUILD_SHARED_LIBS)
    # Copy SDL dlls to executable directory
    install(
      FILES
      ${TARGET_FILES}
      DESTINATION
      ${CMAKE_INSTALL_BINDIR})
  endif()

  if(MSVC)
    add_custom_command(TARGET protegon POST_BUILD
      COMMAND ${CMAKE_COMMAND} --install . --config $<CONFIG>)
  endif()

  configure_file(protegon.pc.in protegon.pc @ONLY)

  install(FILES ${CMAKE_BINARY_DIR}/protegon.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

endif()

function(create_resource_symlink TARGET DIR_NAME)
	set(SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME})
	set(DESTINATION_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${DIR_NAME})
  file(TO_NATIVE_PATH ${SOURCE_DIRECTORY} _src_dir)
  if (MSVC)
	  set(EXE_DEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${DIR_NAME})
    file(TO_NATIVE_PATH ${EXE_DEST_DIR} _exe_dir)
    # This is for distributing the binaries
    add_custom_command(TARGET ${TARGET}
      COMMAND ${PROTEGON_DIR}/scripts/create_link.sh
      ARGS "${_exe_dir}" "${_src_dir}")
  endif()
	if (NOT EXISTS ${DESTINATION_DIRECTORY})
		if (WIN32)
      file(TO_NATIVE_PATH ${DESTINATION_DIRECTORY} _dst_dir)
      # This is for MSVC IDE
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
  PROTEGON_DIR
	PROTEGON_SRC_DIR
	PROTEGON_INCLUDE_DIR
	EXTERNAL_DIR
	PROTEGON_HEADERS
	PROTEGON_SOURCES
	PROTEGON_FILES
	protegon)

message(STATUS "Found protegon")