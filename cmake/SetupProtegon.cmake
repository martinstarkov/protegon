set(PTGN_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")
set(PTGN_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL "")

include(cmake/SDLVersions.cmake)
include(cmake/FindSDL.cmake)
include(cmake/SourcesAndHeaders.cmake)
include(cmake/CreateSymlink.cmake)
include(cmake/CMakeRC.cmake)

function(add_protegon_to target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "add_protegon_to: target '${target}' does not exist")
  endif()

  set(options)
  set(oneValueArgs ASSETS_DIR SHELL_HTML)
  cmake_parse_arguments(P "${options}" "${oneValueArgs}" "" ${ARGN})

  if(NOT P_ASSETS_DIR)
    set(P_ASSETS_DIR "${PTGN_ROOT_DIR}/examples/assets")
  endif()

  if(NOT P_SHELL_HTML)
    set(P_SHELL_HTML "${PTGN_ROOT_DIR}/platform/emscripten/shell.html")
  endif()

  target_link_libraries(${target} PRIVATE protegon)

  if(EMSCRIPTEN)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/dist")
    set_target_properties(${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
    )

    set_target_properties(${target} PROPERTIES OUTPUT_NAME "index")
    set_target_properties(${target} PROPERTIES SUFFIX ".html")
    
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
      target_compile_options(${target} PRIVATE -O0)
    else()
      target_compile_options(${target} PRIVATE -O3)
    endif()
    

    # Shared assets for ALL examples
    if(EXISTS "${P_ASSETS_DIR}")
      target_link_options(${target} PRIVATE
        "--preload-file=${P_ASSETS_DIR}@/assets"
      )
    endif()

    target_link_options(${target} PRIVATE
      "--shell-file=${P_SHELL_HTML}"
      "-sALLOW_MEMORY_GROWTH=1"
      "-sFULL_ES3=1"
      "-sWARN_ON_UNDEFINED_SYMBOLS=1"
      "-sNO_EXIT_RUNTIME=1"
      "-sAGGRESSIVE_VARIABLE_ELIMINATION=1"
      "-sUSE_ZLIB=1"
      "-sASSERTIONS=1"
    )
  else()
    if(WIN32)
      add_sdl_dll_copy(${target})
    endif()
  endif()
endfunction()

include(FetchContent)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

cmrc_add_resource_library(resources-shader ALIAS rc::shader NAMESPACE shader WHENCE "${PTGN_SHADER_DIR}" ${PTGN_SHADERS} "${PTGN_SHADER_DIR}/manifest.json")

add_library(protegon STATIC ${PTGN_FILES})

target_include_directories(protegon
  PUBLIC "${PTGN_ROOT_DIR}/include"
         "${PTGN_ROOT_DIR}/modules/ecs/include"
         "${PTGN_ROOT_DIR}/engine/assets"
  PUBLIC "${PTGN_ROOT_DIR}/engine/src"
)

target_compile_features(protegon PUBLIC cxx_std_23)

# Link third-party deps ON THE LIBRARY
target_link_libraries(protegon
  PUBLIC
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
    SDL3_mixer::SDL3_mixer
    SDL3::SDL3
    rc::shader
    nlohmann_json::nlohmann_json
)

set_target_properties(protegon PROPERTIES DEBUG_POSTFIX d)

if(NOT EMSCRIPTEN)
  include(cmake/CompilerWarnings.cmake)
  include(cmake/CompilerSettings.cmake)
  
  set_project_warnings(protegon "${PTGN_WARNINGS_AS_ERRORS}")
  set_compiler_settings(protegon ${PTGN_FILES})

  find_package(OpenGL REQUIRED)
  target_link_libraries(protegon PUBLIC ${OPENGL_LIBRARIES})
endif()