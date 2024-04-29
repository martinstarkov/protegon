cmake_minimum_required(VERSION 3.20)

# Misc options

option(SHARED_SDL2_LIBS "Statically Link SDL2 libs to protegon" ON)

# TODO: Figure out how to disable console for macOS
if (WIN32)
  option(ENABLE_CONSOLE "Enable console executable" OFF)
endif()

set(BUILD_SHARED_LIBS ${SHARED_SDL2_LIBS})

# Protegon variables

set(PROTEGON_DIR         ${PROJECT_SOURCE_DIR}              CACHE BOOL "" FORCE)
set(SCRIPT_DIR           ${PROTEGON_DIR}/scripts            CACHE BOOL "" FORCE)
set(EXTERNAL_DIR         ${PROTEGON_DIR}/external           CACHE BOOL "" FORCE)
set(SDL_DIR              ${EXTERNAL_DIR}/sdl2               CACHE BOOL "" FORCE)
set(PROTEGON_SRC_DIR     ${PROTEGON_DIR}/src                CACHE BOOL "" FORCE)
set(PROTEGON_INCLUDE_DIR ${PROTEGON_DIR}/include            CACHE BOOL "" FORCE)
set(MODULES_DIR          ${PROTEGON_DIR}/modules            CACHE BOOL "" FORCE)
set(ECS_INCLUDE_DIR      ${MODULES_DIR}/ecs/include         CACHE BOOL "" FORCE)
set(JSON_INCLUDE_DIR     ${MODULES_DIR}/json/single_include CACHE BOOL "" FORCE)

# Search for protegon source and header files
file(GLOB_RECURSE PROTEGON_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false 
	${PROTEGON_SRC_DIR}/*.cpp)
file(GLOB_RECURSE PROTEGON_HEADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false 
  ${PROTEGON_SRC_DIR}/*.h
  ${PROTEGON_INCLUDE_DIR}/*.h)

set(PROTEGON_FILES ${PROTEGON_SOURCES} ${PROTEGON_HEADERS})

# SDL variables

# Versions not enforced on Linux (instead using brew latest version)
set(SDL2_VERSION       2.30.0)
set(SDL2_IMAGE_VERSION 2.8.2)
set(SDL2_MIXER_VERSION 2.8.0)
set(SDL2_TTF_VERSION   2.22.0)
set(SDL2MAIN_LIBRARY FALSE)

# Create static library
add_library(protegon STATIC ${PROTEGON_SOURCES})
target_compile_features(protegon PUBLIC cxx_std_17)
set_target_properties(protegon PROPERTIES CXX_EXTENSIONS OFF)

# Function for downloading and extracting SDL libraries
function(download_and_extract dir exists_dir url archive_name plain_name downloaded)
  if (NOT EXISTS ${exists_dir})
    file(MAKE_DIRECTORY ${dir})
    get_filename_component(zip_name ${url} NAME)
    get_filename_component(extension ${url} LAST_EXT)
    message(STATUS "Downloading ${url}...")
    set(zip_location ${dir}/${zip_name})
    file(DOWNLOAD ${url} ${zip_location} INACTIVITY_TIMEOUT 60) # SHOW_PROGRESS
    string(TOLOWER "${extension}" lower_extension)
    if ("${lower_extension}" STREQUAL ".zip")
      file(ARCHIVE_EXTRACT INPUT ${zip_location} DESTINATION ${dir})
    elseif("${lower_extension}" STREQUAL ".dmg" AND NOT plain_name STREQUAL "")
      message(STATUS "Extracting ${archive_name}...")
      execute_process(COMMAND hdiutil attach ${zip_location} -quiet
                      COMMAND cp -r /Volumes/${plain_name}/${archive_name} ${dir}/${archive_name}
                      COMMAND hdiutil detach /Volumes/${plain_name} -quiet)
    endif()
    file(REMOVE ${zip_location})
    message(STATUS "Successfully downloaded and extracted ${archive_name}")
    set(${downloaded} TRUE PARENT_SCOPE)
  else()
    set(${downloaded} FALSE PARENT_SCOPE)
  endif()
endfunction()

function(install_with_homebrew library archive_name library_location library_path)
  execute_process(
    COMMAND sh ${SCRIPT_DIR}/check_brew_package.sh ${library}
    OUTPUT_VARIABLE brew_package_status
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE brew_package_result)
  if(brew_package_result EQUAL 1)
    message(STATUS "Package '${library}' found in Homebrew.")
  else()
    message(STATUS "Package '${library}' not found in Homebrew. Installing...")
    execute_process(COMMAND brew install ${library})
    # DOWNLOAD OLDER VERSION OF BREW LIBRARY
    # set(OUTPUT_DIR ${EXTERNAL_DIR}/sdl2-linux/${archive_name})
    # execute_process(COMMAND brew unlink ${library})
    # execute_process(COMMAND wget ${library_path} -P ${OUTPUT_DIR})
    # execute_process(COMMAND brew install --HEAD -s ${OUTPUT_DIR}/${library}.rb
    #                 WORKING_DIRECTORY ${OUTPUT_DIR})
  endif()

  execute_process(COMMAND brew --prefix "${library}" OUTPUT_VARIABLE location_prefix)
  set(${library_location} "${location_prefix}" PARENT_SCOPE)
endfunction()

function(add_protegon_to TARGET)
  target_link_libraries(${TARGET} PRIVATE protegon)
  if(APPLE)
    set_target_properties(${TARGET} PROPERTIES
      XCODE_GENERATE_SCHEME TRUE
      XCODE_SCHEME_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  endif()
  # Commands for copying dlls to executable directory on Windows.
  if(WIN32 AND SHARED_SDL2_LIBS)
    get_property(DLLS GLOBAL PROPERTY PROTEGON_DLLS)
    add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND}
                        -E copy_if_different ${DLLS} $<TARGET_FILE_DIR:${TARGET}>
                        COMMAND_EXPAND_LISTS)
    mark_as_advanced(DLLS)
  endif()
endfunction()

function(create_resource_symlink TARGET DIR_NAME)
	set(SOURCE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME})
	set(DESTINATION_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${DIR_NAME})
  file(TO_NATIVE_PATH ${SOURCE_DIRECTORY} _src_dir)
  if (MSVC OR XCODE)
    set(EXE_DEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${DIR_NAME})
    file(TO_NATIVE_PATH ${EXE_DEST_DIR} _exe_dir)
    if (MSVC)
      add_custom_command(TARGET ${TARGET} COMMAND ${SCRIPT_DIR}/create_link_win.sh "${_exe_dir}" "${_src_dir}")
    elseif(XCODE)
      add_custom_command(TARGET ${TARGET} COMMAND ln -sf ${SOURCE_DIRECTORY} ${EXE_DEST_DIR})
    endif()
      # This is for distributing the binaries
      #add_custom_command(TARGET ${TARGET} COMMAND ${SYMLINK_COMMAND})
  endif()
	if (NOT EXISTS ${DESTINATION_DIRECTORY})
		if (WIN32)
      file(TO_NATIVE_PATH ${DESTINATION_DIRECTORY} _dst_dir)
      # This is for MSVC IDE
      execute_process(COMMAND cmd.exe /c mklink /J ${_dst_dir} ${_src_dir})
		elseif(APPLE)
			#message(STATUS "Creating Symlink from ${SOURCE_DIRECTORY} to ${DESTINATION_DIRECTORY}")
			execute_process(COMMAND ln -s ${SOURCE_DIRECTORY} ${DESTINATION_DIRECTORY})
		elseif(UNIX AND NOT APPLE)
			execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${SOURCE_DIRECTORY} ${DESTINATION_DIRECTORY})
		endif()
	endif()
endfunction()

if (MSVC)
  # Group files for MSVC project tree
  foreach(_source IN ITEMS ${PROTEGON_FILES})
    get_filename_component(_source_path ${_source} PATH)
    file(RELATIVE_PATH _source_path_rel ${PROTEGON_SRC_DIR} ${_source_path})
    string(REPLACE "/" "\\" _group_path ${_source_path_rel})
    source_group(${_group_path} FILES ${_source})
  endforeach()
  # Console related flags
  if (ENABLE_CONSOLE)
    target_link_options(protegon PUBLIC "/SUBSYSTEM:CONSOLE")
  else()
    target_link_options(protegon PUBLIC "/SUBSYSTEM:WINDOWS")
    target_compile_definitions(protegon INTERFACE main=WinMain)
  endif()
  # Updates CMAKE_PREFIX_PATH paths to include library architecture
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_LIBRARY_ARCHITECTURE "x64")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CMAKE_LIBRARY_ARCHITECTURE "x86")
  endif()
endif()

set(SDL2_NAME       SDL2-${SDL2_VERSION})
set(SDL2_IMAGE_NAME SDL2_image-${SDL2_IMAGE_VERSION})
set(SDL2_MIXER_NAME SDL2_mixer-${SDL2_MIXER_VERSION})
set(SDL2_TTF_NAME   SDL2_ttf-${SDL2_TTF_VERSION})

if (WIN32)
  if(MSVC)
    set(WIN_COMPILER_POSTFIX "-VC")
    if (SHARED_SDL2_LIBS)
      set(OUTPUT_DIR ${SDL_DIR}-msvc-shared)
    else()
      set(OUTPUT_DIR ${SDL_DIR}-msvc-static)
    endif()
  elseif(MINGW)
    # Console related flags
    if (ENABLE_CONSOLE)
      target_link_options(protegon PUBLIC "-mconsole")
    else()
      target_link_options(protegon PUBLIC "-mwindows")
    endif()
    set(WIN_COMPILER_POSTFIX "-mingw")
    set(OUTPUT_DIR ${SDL_DIR}-mingw)
  endif()
  
  set(EXTENSION "zip")
  set(WIN_COMPILER_PREFIX "-devel")

  set(SDL2_LOCATION       ${OUTPUT_DIR}/${SDL2_NAME})
  set(SDL2_IMAGE_LOCATION ${OUTPUT_DIR}/${SDL2_IMAGE_NAME})
  set(SDL2_MIXER_LOCATION ${OUTPUT_DIR}/${SDL2_MIXER_NAME})
  set(SDL2_TTF_LOCATION   ${OUTPUT_DIR}/${SDL2_TTF_NAME})

  set(SDL_CMAKE_PATH cmake)

elseif(APPLE)
  set(OUTPUT_DIR ${SDL_DIR}-mac)

  set(SDL2_NAME       SDL2.framework)
  set(SDL2_IMAGE_NAME SDL2_image.framework)
  set(SDL2_MIXER_NAME SDL2_mixer.framework)
  set(SDL2_TTF_NAME   SDL2_ttf.framework)

  set(SDL2_LOCATION       ${OUTPUT_DIR}/${SDL2_NAME})
  set(SDL2_IMAGE_LOCATION ${OUTPUT_DIR}/${SDL2_IMAGE_NAME})
  set(SDL2_MIXER_LOCATION ${OUTPUT_DIR}/${SDL2_MIXER_NAME})
  set(SDL2_TTF_LOCATION   ${OUTPUT_DIR}/${SDL2_TTF_NAME})

  set(EXTENSION "dmg")
  set(WIN_COMPILER_PREFIX "")
  set(WIN_COMPILER_POSTFIX "")

  set(SDL_CMAKE_PATH Versions/A/Resources/CMake)
elseif(UNIX AND NOT APPLE)
  
  execute_process(COMMAND sh ${SCRIPT_DIR}/check_brew.sh
                  OUTPUT_VARIABLE brew_installed
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  RESULT_VARIABLE brew_installed_result)
  if(brew_installed_result EQUAL 1)
    message(STATUS "Homebrew found.")
    set(ENV{HOMEBREW_NO_INSTALL_CLEANUP} TRUE)
    set(ENV{HOMEBREW_NO_ENV_HINTS} TRUE)
  else()
    message(FATAL_ERROR "Homebrew not found. Please install it before continuing.")
  endif()

  install_with_homebrew(sdl2       SDL2-${SDL2_VERSION}             SDL2_LOCATION       https://raw.githubusercontent.com/Homebrew/homebrew-core/5f2b3ba5bb5c4bef36c1e4be278f43601394b729/Formula/s/sdl2.rb)
  install_with_homebrew(sdl2_image SDL2_image-${SDL2_IMAGE_VERSION} SDL2_IMAGE_LOCATION https://github.com/Homebrew/homebrew-core/raw/6c917a52f9fc1de0dfe5eb962da23288b478423d/Formula/s/sdl2_image.rb)
  install_with_homebrew(sdl2_ttf   SDL2_ttf-${SDL2_TTF_VERSION}     SDL2_TTF_LOCATION   https://github.com/Homebrew/homebrew-core/raw/254685622723c6603b528e1456b7827e4390a860/Formula/s/sdl2_ttf.rb)
  install_with_homebrew(sdl2_mixer SDL2_mixer-${SDL2_MIXER_VERSION} SDL2_MIXER_LOCATION https://github.com/Homebrew/homebrew-core/raw/6f7a42bf1a9375d1a2786d90476b32ce366f232d/Formula/s/sdl2_mixer.rb)
  
  set(SDL_CMAKE_PATH lib/cmake)
endif()

if (APPLE OR MINGW OR MSVC AND SHARED_SDL2_LIBS)
  download_and_extract(${OUTPUT_DIR} ${SDL2_LOCATION}       https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2${WIN_COMPILER_PREFIX}-${SDL2_VERSION}${WIN_COMPILER_POSTFIX}.${EXTENSION}                         ${SDL2_NAME}       SDL2       DOWNLOADED)
  download_and_extract(${OUTPUT_DIR} ${SDL2_IMAGE_LOCATION} https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/SDL2_image${WIN_COMPILER_PREFIX}-${SDL2_IMAGE_VERSION}${WIN_COMPILER_POSTFIX}.${EXTENSION} ${SDL2_IMAGE_NAME} SDL2_image DOWNLOADED)
  download_and_extract(${OUTPUT_DIR} ${SDL2_MIXER_LOCATION} https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL2_MIXER_VERSION}/SDL2_mixer${WIN_COMPILER_PREFIX}-${SDL2_MIXER_VERSION}${WIN_COMPILER_POSTFIX}.${EXTENSION} ${SDL2_MIXER_NAME} SDL2_mixer DOWNLOADED)
  download_and_extract(${OUTPUT_DIR} ${SDL2_TTF_LOCATION}   https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/SDL2_ttf${WIN_COMPILER_PREFIX}-${SDL2_TTF_VERSION}${WIN_COMPILER_POSTFIX}.${EXTENSION}         ${SDL2_TTF_NAME}   SDL2_ttf   DOWNLOADED)
elseif(MSVC AND NOT SHARED_SDL2_LIBS)
  download_and_extract(${OUTPUT_DIR} ${OUTPUT_DIR} https://github.com/martinstarkov/SDL2-static-libs/archive/refs/heads/main.zip SDL2-static-libs-main "" DOWNLOADED)
  if (${DOWNLOADED})
    execute_process(COMMAND sh ${SCRIPT_DIR}/move_subdirs.sh "${OUTPUT_DIR}/SDL2-static-libs-main")
  endif()
endif()

set(SDL2_DIR       ${SDL2_LOCATION}/${SDL_CMAKE_PATH}       CACHE BOOL "" FORCE)
set(SDL2_image_DIR ${SDL2_IMAGE_LOCATION}/${SDL_CMAKE_PATH} CACHE BOOL "" FORCE)
set(SDL2_mixer_DIR ${SDL2_MIXER_LOCATION}/${SDL_CMAKE_PATH} CACHE BOOL "" FORCE)
set(SDL2_ttf_DIR   ${SDL2_TTF_LOCATION}/${SDL_CMAKE_PATH}   CACHE BOOL "" FORCE)

list(APPEND CMAKE_MODULE_PATH ${SDL2_DIR})
list(APPEND CMAKE_MODULE_PATH ${SDL2_image_DIR})
list(APPEND CMAKE_MODULE_PATH ${SDL2_ttf_DIR})
list(APPEND CMAKE_MODULE_PATH ${SDL2_mixer_DIR})

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
find_package(SDL2       REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf   REQUIRED)
find_package(SDL2_mixer REQUIRED)
set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)

set(STATIC_POSTFIX $<IF:$<AND:$<BOOL:${MINGW}>,$<NOT:$<BOOL:${SHARED_SDL2_LIBS}>>>,-static,>)
set(SDL_TARGETS SDL2::SDL2${STATIC_POSTFIX}
                SDL2_image::SDL2_image${STATIC_POSTFIX}
                SDL2_ttf::SDL2_ttf${STATIC_POSTFIX}
                SDL2_mixer::SDL2_mixer${STATIC_POSTFIX})

function(get_target_file_paths TARGETS OUTPUT_VAR)
set(TARGET_FILE_PATHS)
foreach(TARGET IN LISTS ${TARGETS})
list(APPEND TARGET_FILE_PATHS $<TARGET_FILE:${TARGET}>)
endforeach(TARGET)
set(${OUTPUT_VAR} ${TARGET_FILE_PATHS} PARENT_SCOPE)
endfunction(get_target_file_paths)

# Combine freetype lib into protegon lib.
set(FREETYPE_LIB "")
if(NOT SHARED_SDL2_LIBS)
  if(MSVC)
    set(FREETYPE_LIB "${OUTPUT_DIR}/SDL2_ttf-${SDL2_TTF_VERSION}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/freetype.lib")
    get_target_file_paths(SDL_TARGETS SDL_TARGET_FILES)
    list(APPEND SDL_TARGET_FILES ${FREETYPE_LIB})
    add_custom_command(TARGET protegon POST_BUILD
                       COMMAND ${CMAKE_AR} /NOLOGO /OUT:"$<TARGET_FILE:protegon>" "$<TARGET_FILE:protegon>" "${SDL_TARGET_FILES}"
                       COMMAND_EXPAND_LISTS)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|GNU)$") # TODO: Check if works with Clang "^(Clang|GNU)$")
    if (MINGW)
    	target_link_options(protegon PUBLIC "-static")
    else()
      set(FREETYPE_LIB freetype)
    endif()
    get_target_file_paths(SDL_TARGETS SDL_TARGET_FILES)
    if (UNIX AND NOT APPLE)

    else()
      list(APPEND SDL_TARGET_FILES ${FREETYPE_LIB})
    endif()
    add_custom_command(TARGET protegon POST_BUILD
                       COMMAND ${CMAKE_AR}
                       VERBATIM r "$<TARGET_FILE:protegon>" "${SDL_TARGET_FILES}"
                       COMMAND_EXPAND_LISTS)
  endif()
endif()

#file(GENERATE OUTPUT protegon_log_output_file CONTENT "Targets: ${SDL_TARGETS}..............TargetFiles: ${SDL_TARGET_FILES}")

if(NOT SHARED_SDL2_LIBS AND MSVC)
  # target_link_libraries(SDL2 PRIVATE "-nodefaultlib:MSVCRT")
  target_link_options(protegon PUBLIC $<IF:$<CONFIG:Debug>,/NODEFAULTLIB:MSVCRT,>)
endif()

target_link_libraries(protegon PRIVATE ${SDL_TARGETS} ${FREETYPE_LIB})

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
if(WIN32 AND SHARED_SDL2_LIBS AND NOT CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
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

  if (WIN32 AND SHARED_SDL2_LIBS)
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
