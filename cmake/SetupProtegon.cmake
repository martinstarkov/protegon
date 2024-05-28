cmake_minimum_required(VERSION 3.16)

# Misc options

option(DOWNLOAD_SDL2 "Download prebuilt SDL2 libs" ON)

if (WIN32)
  option(SHARED_SDL2_LIBS "Link SDL2 libs to protegon as dlls" ON)
  # TODO: Figure out how to disable console for macOS and enable for linux
  option(ENABLE_CONSOLE "Enable console executable" ON)
else()
  set(SHARED_SDL2_LIBS ON CACHE BOOL "" FORCE)
endif()

# Protegon variables

set(PROTEGON_DIR         "${PROJECT_SOURCE_DIR}"              CACHE BOOL "" FORCE)
set(CMAKE_DIR            "${PROTEGON_DIR}/cmake"              CACHE BOOL "" FORCE)
set(SCRIPT_DIR           "${PROTEGON_DIR}/scripts"            CACHE BOOL "" FORCE)
set(PROTEGON_SRC_DIR     "${PROTEGON_DIR}/src"                CACHE BOOL "" FORCE)
set(PROTEGON_INCLUDE_DIR "${PROTEGON_DIR}/include"            CACHE BOOL "" FORCE)
set(MODULES_DIR          "${PROTEGON_DIR}/modules"            CACHE BOOL "" FORCE)
set(ECS_INCLUDE_DIR      "${MODULES_DIR}/ecs/include"         CACHE BOOL "" FORCE)
set(JSON_INCLUDE_DIR     "${MODULES_DIR}/json/single_include" CACHE BOOL "" FORCE)

# Search for protegon source and header files
file(GLOB_RECURSE PROTEGON_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false 
	"${PROTEGON_SRC_DIR}/*.cpp")
file(GLOB_RECURSE PROTEGON_HEADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false 
  "${PROTEGON_SRC_DIR}/*.h" "${PROTEGON_INCLUDE_DIR}/*.h")

set(PROTEGON_FILES ${PROTEGON_SOURCES} ${PROTEGON_HEADERS})

# Create static library
add_library(protegon STATIC ${PROTEGON_FILES})
target_compile_features(protegon PUBLIC cxx_std_17)
set_target_properties(protegon PROPERTIES CXX_EXTENSIONS OFF)

include("${CMAKE_DIR}/SetupSDL2.cmake")

function(add_protegon_to TARGET)
  target_link_libraries(${TARGET} PRIVATE protegon)
  if(XCODE)
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

# When using AppleClang, for some reason the working directory for the executable is set to $HOME instead of the executable directory.
# Therefore, the C++ code corrects the working directory using std::filesystem so that relative paths work properly.
if (APPLE AND NOT XCODE)
  target_compile_definitions(protegon PRIVATE USING_APPLE_CLANG=1)
endif()

if (MSVC)
  # Group files for MSVC project tree
  foreach(_source IN ITEMS ${PROTEGON_FILES})
    get_filename_component(_source_path ${_source} PATH)
    file(RELATIVE_PATH _source_path_rel "${PROTEGON_SRC_DIR}" ${_source_path})
    string(REPLACE "/" "\\" _group_path ${_source_path_rel})
    source_group(${_group_path} FILES ${_source})
  endforeach()
endif()

function(create_resource_symlink TARGET DIR_NAME)
	set(SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}")
	set(DESTINATION_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${DIR_NAME}")
  file(TO_NATIVE_PATH ${SOURCE_DIRECTORY} _src_dir)
  if (MSVC OR XCODE)
    set(EXE_DEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${DIR_NAME}")
    file(TO_NATIVE_PATH ${EXE_DEST_DIR} _exe_dir)
    if (MSVC)
      add_custom_command(TARGET ${TARGET} COMMAND "${SCRIPT_DIR}/create_link_win.sh" "${_exe_dir}" "${_src_dir}")
    elseif(XCODE)
      message(STATUS "Creating Symlink from ${SOURCE_DIRECTORY} to ${EXE_DEST_DIR}")
      add_custom_command(TARGET ${TARGET} COMMAND ln -sf "${SOURCE_DIRECTORY}" "${EXE_DEST_DIR}")
    endif()
      # This is for distributing the binaries
      #add_custom_command(TARGET ${TARGET} COMMAND ${SYMLINK_COMMAND})
  endif()
	if (NOT EXISTS ${DESTINATION_DIRECTORY})
		if (WIN32)
      file(TO_NATIVE_PATH "${DESTINATION_DIRECTORY}" _dst_dir)
      # This is for MSVC IDE
      execute_process(COMMAND cmd.exe /c mklink /J "${_dst_dir}" "${_src_dir}")
		elseif(APPLE)
			message(STATUS "Creating Symlink from ${SOURCE_DIRECTORY} to ${DESTINATION_DIRECTORY}")
			execute_process(COMMAND ln -sf "${SOURCE_DIRECTORY}" "${DESTINATION_DIRECTORY}")
		elseif(UNIX AND NOT APPLE)
			execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${SOURCE_DIRECTORY}" "${DESTINATION_DIRECTORY}")
		endif()
	endif()
endfunction()

find_package(OpenGL REQUIRED)

target_link_libraries(protegon PRIVATE ${OPENGL_LIBRARIES})

target_include_directories(protegon PUBLIC
	"${PROTEGON_INCLUDE_DIR}"
	"${ECS_INCLUDE_DIR}"
	"${JSON_INCLUDE_DIR}"
  PRIVATE
  "${OPENGL_INCLUDE_DIRS}"
	"${PROTEGON_SRC_DIR}")

# Add d to debug static lib files to differentiate them from release
set_target_properties(protegon PROPERTIES DEBUG_POSTFIX d)

target_include_directories(protegon PUBLIC
  $<BUILD_INTERFACE:${PROTEGON_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)

  include(GNUInstallDirs)

  if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE PATH "" FORCE)
  endif()

  install(TARGETS protegon
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

  install(FILES ${PROTEGON_HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/protegon")

  if (WIN32 AND SHARED_SDL2_LIBS)
    # Copy SDL dlls to executable directory
    install(FILES ${SDL_TARGET_FILES} DESTINATION "${CMAKE_INSTALL_BINDIR}")
  endif()

  # Optional: Call cmake install after MSVC build completes.
  # if(MSVC)
  #   add_custom_command(TARGET protegon POST_BUILD COMMAND ${CMAKE_COMMAND} --install . --config $<CONFIG>)
  # endif()

endif()

message(STATUS "Found protegon")
