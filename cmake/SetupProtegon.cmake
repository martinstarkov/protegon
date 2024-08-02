include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SourcesAndHeaders.cmake")

add_library(protegon STATIC ${PROTEGON_SOURCES} ${PROTEGON_HEADERS})

target_compile_features(protegon PUBLIC cxx_std_17)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerWarnings.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerSettings.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SetupSDL2.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CreateSymlink.cmake")
if (MSVC)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MSVCSetup.cmake")
endif()

set_project_warnings(protegon)
set_compiler_settings(protegon)

find_package(OpenGL REQUIRED)

target_link_libraries(protegon
  PRIVATE ${OPENGL_LIBRARIES}
          SDL2::SDL2
          SDL2_image::SDL2_image
          SDL2_ttf::SDL2_ttf
          SDL2_mixer::SDL2_mixer)

target_include_directories(protegon
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
         "${CMAKE_CURRENT_SOURCE_DIR}/modules/ecs/include"
         "${CMAKE_CURRENT_SOURCE_DIR}/modules/json/single_include"
         "${CMAKE_CURRENT_SOURCE_DIR}/modules/luple/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Add d to debug static lib files to differentiate them from release
set_target_properties(protegon PROPERTIES DEBUG_POSTFIX d)

function(add_protegon_to TARGET)

  target_link_libraries(${TARGET} PRIVATE protegon)
  # if(XCODE)
  #   set_target_properties(${TARGET} PROPERTIES
  #     XCODE_GENERATE_SCHEME TRUE
  #     XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  # endif()
  # Commands for copying dlls to executable directory on Windows.
  if(WIN32 AND NOT LINK_STATIC_SDL)
    add_sdl_dll_copy(${TARGET})
  endif()

endfunction()

message(STATUS "Found protegon")

# Add d to debug static lib files to differentiate them from release
# set_target_properties(protegon PROPERTIES DEBUG_POSTFIX d)

# target_include_directories(protegon PUBLIC
#   $<BUILD_INTERFACE:${PROTEGON_DIR}/include>
#   $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
#   $<INSTALL_INTERFACE:include>)

# if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)

#   include(GNUInstallDirs)

#   if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
#     set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE PATH "" FORCE)
#   endif()

#   install(TARGETS protegon
#     RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
#     LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
#     ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

#   install(FILES ${PROTEGON_HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/protegon")

#   if (WIN32 AND SHARED_SDL2_LIBS)
#     # Copy SDL dlls to executable directory
#     install(FILES ${SDL_TARGET_FILES} DESTINATION "${CMAKE_INSTALL_BINDIR}")
#   endif()

# endif()
