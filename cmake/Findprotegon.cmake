cmake_minimum_required(VERSION 3.20)

# Misc options

option(SHARED_SDL2_LIBS "Statically Link SDL2 libs to protegon" OFF)
option(DOWNLOAD_SDL2 "Download prebuilt SDL2 libs" ON)

# TODO: Figure out how to disable console for macOS
if (WIN32)
  option(ENABLE_CONSOLE "Enable console executable" ON)
endif()

set(BUILD_SHARED_LIBS ${SHARED_SDL2_LIBS})

# Protegon variables

set(PROTEGON_DIR         ${PROJECT_SOURCE_DIR}              CACHE BOOL "" FORCE)
set(CMAKE_DIR            ${PROTEGON_DIR}/cmake              CACHE BOOL "" FORCE)
set(SCRIPT_DIR           ${PROTEGON_DIR}/scripts            CACHE BOOL "" FORCE)
set(PROTEGON_SRC_DIR     ${PROTEGON_DIR}/src                CACHE BOOL "" FORCE)
set(PROTEGON_INCLUDE_DIR ${PROTEGON_DIR}/include            CACHE BOOL "" FORCE)
set(MODULES_DIR          ${PROTEGON_DIR}/modules            CACHE BOOL "" FORCE)
set(ECS_INCLUDE_DIR      ${MODULES_DIR}/ecs/include         CACHE BOOL "" FORCE)
set(JSON_INCLUDE_DIR     ${MODULES_DIR}/json/single_include CACHE BOOL "" FORCE)

if (${DOWNLOAD_SDL2})
  include(${CMAKE_DIR}/InstallSDL.cmake)
endif()

# Search for protegon source and header files
file(GLOB_RECURSE PROTEGON_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false 
	${PROTEGON_SRC_DIR}/*.cpp)
file(GLOB_RECURSE PROTEGON_HEADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false 
  ${PROTEGON_SRC_DIR}/*.h
  ${PROTEGON_INCLUDE_DIR}/*.h)

set(PROTEGON_FILES ${PROTEGON_SOURCES} ${PROTEGON_HEADERS})

# Create static library
add_library(protegon STATIC ${PROTEGON_SOURCES})
target_compile_features(protegon PUBLIC cxx_std_17)
set_target_properties(protegon PROPERTIES CXX_EXTENSIONS OFF)

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

function(get_target_file_paths TARGETS OUTPUT_VAR)
  set(TARGET_FILE_PATHS)
  foreach(TARGET IN LISTS ${TARGETS})
  list(APPEND TARGET_FILE_PATHS $<TARGET_FILE:${TARGET}>)
  endforeach(TARGET)
  set(${OUTPUT_VAR} ${TARGET_FILE_PATHS} PARENT_SCOPE)
endfunction(get_target_file_paths)

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
elseif(MINGW)
  if (ENABLE_CONSOLE)
    target_link_options(protegon PUBLIC "-mconsole")
  else()
    target_link_options(protegon PUBLIC "-mwindows")
  endif()
endif()

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
find_package(SDL2       REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf   REQUIRED)
find_package(SDL2_mixer REQUIRED)
set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)

# IF MINGW and NOT SHARED_SDL2_LIBS add '-static' to the targets 
set(STATIC_POSTFIX $<IF:$<AND:$<BOOL:${MINGW}>,$<NOT:$<BOOL:${SHARED_SDL2_LIBS}>>>,-static,>)
set(SDL_TARGETS SDL2::SDL2${STATIC_POSTFIX}
                SDL2_image::SDL2_image${STATIC_POSTFIX}
                SDL2_ttf::SDL2_ttf${STATIC_POSTFIX}
                SDL2_mixer::SDL2_mixer${STATIC_POSTFIX})

# Combine freetype lib into protegon lib.
set(FREETYPE_LIB "")
if(NOT SHARED_SDL2_LIBS AND ${DOWNLOAD_SDL2})
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

# Install

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
