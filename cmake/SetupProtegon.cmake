include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SourcesAndHeaders.cmake")

if(EMSCRIPTEN)
  add_library(protegon STATIC ${PROTEGON_SOURCES} ${PROTEGON_HEADERS}
                              ${PROTEGON_ES_SHADERS})
else()
  add_library(protegon STATIC ${PROTEGON_SOURCES} ${PROTEGON_HEADERS}
                              ${PROTEGON_CORE_SHADERS})
endif()

target_compile_features(protegon PUBLIC cxx_std_20)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CreateSymlink.cmake")

if(NOT EMSCRIPTEN)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SetupSDL2.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerWarnings.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerSettings.cmake")
  set_project_warnings(protegon)
  set_compiler_settings(protegon)
endif()

if(MSVC)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MSVCSetup.cmake")
endif()

include(FetchContent)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

target_link_libraries(protegon PUBLIC nlohmann_json::nlohmann_json)

if(NOT EMSCRIPTEN)
  find_package(OpenGL REQUIRED)

  target_link_libraries(
    protegon PRIVATE ${OPENGL_LIBRARIES} SDL2::SDL2 SDL2_image::SDL2_image
                     SDL2_ttf::SDL2_ttf SDL2_mixer::SDL2_mixer)
else()
  if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(ECXXFLAGS "-O0")
  else()
    set(ECXXFLAGS "-O3")
  endif()
  set(ECXXFLAGS
      "${ECXXFLAGS} -std=c++20 --use-port=sdl2 --use-port=sdl2_image:formats=bmp,png,xpm,jpg --use-port=sdl2_mixer --use-port=sdl2_ttf"
  )
  set_target_properties(
    protegon
    PROPERTIES
      LINK_FLAGS
      "${ECXXFLAGS} -s FULL_ES3=1 -s ALLOW_MEMORY_GROWTH=1 -s WARN_ON_UNDEFINED_SYMBOLS=1 -s NO_EXIT_RUNTIME=1 -s AGGRESSIVE_VARIABLE_ELIMINATION=1"
  )
  set_target_properties(protegon PROPERTIES COMPILE_FLAGS "${ECXXFLAGS}")
endif()

target_include_directories(
  protegon
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
         "${CMAKE_CURRENT_SOURCE_DIR}/modules/ecs/include"
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Add d to debug static lib files to differentiate them from release
set_target_properties(protegon PROPERTIES DEBUG_POSTFIX d)

function(add_protegon_to TARGET)

  target_link_libraries(${TARGET} PRIVATE protegon)
  # if(XCODE) set_target_properties(${TARGET} PROPERTIES XCODE_GENERATE_SCHEME
  # TRUE XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}") endif()
  # Commands for copying dlls to executable directory on Windows.
  if(WIN32
     AND NOT LINK_STATIC_SDL
     AND NOT EMSCRIPTEN)
    add_sdl_dll_copy(${TARGET})
  endif()

endfunction()

message(STATUS "Found protegon")

# Add d to debug static lib files to differentiate them from release
# set_target_properties(protegon PROPERTIES DEBUG_POSTFIX d)

# target_include_directories(protegon PUBLIC
# $<BUILD_INTERFACE:${PROTEGON_DIR}/include>
# $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include> $<INSTALL_INTERFACE:include>)

# if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)

# include(GNUInstallDirs)

# if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT) set(CMAKE_INSTALL_PREFIX
# "${PROJECT_SOURCE_DIR}/install" CACHE PATH "" FORCE) endif()

# install(TARGETS protegon RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" LIBRARY
# DESTINATION "${CMAKE_INSTALL_LIBDIR}" ARCHIVE DESTINATION
# "${CMAKE_INSTALL_LIBDIR}")

# install(FILES ${PROTEGON_HEADERS} DESTINATION
# "${CMAKE_INSTALL_INCLUDEDIR}/protegon")

# if (WIN32 AND SHARED_SDL2_LIBS) # Copy SDL dlls to executable directory
# install(FILES ${SDL_TARGET_FILES} DESTINATION "${CMAKE_INSTALL_BINDIR}")
# endif()

# endif()
