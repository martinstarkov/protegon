include(cmake/SourcesAndHeaders.cmake)

if(EMSCRIPTEN)
  add_library(protegon STATIC ${PROTEGON_SOURCES} ${PROTEGON_HEADERS}
                              ${PROTEGON_ES_SHADERS})
else()
  add_library(protegon STATIC ${PROTEGON_SOURCES} ${PROTEGON_HEADERS}
                              ${PROTEGON_CORE_SHADERS})
endif()

target_compile_features(protegon PUBLIC cxx_std_20)
set_property(TARGET protegon PROPERTY CXX_EXTENSIONS OFF)

include(cmake/CreateSymlink.cmake)

if(NOT EMSCRIPTEN)
  include(cmake/SetupSDL.cmake)
  include(cmake/CompilerWarnings.cmake)
  include(cmake/CompilerSettings.cmake)
  set_project_warnings(protegon)
  set_compiler_settings(protegon)
endif()

if(MSVC)
  include(cmake/MSVCSetup.cmake)
endif()

include(cmake/ShaderSetup.cmake)

include(FetchContent)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

target_link_libraries(protegon PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(protegon PUBLIC rc::shader)

if(NOT EMSCRIPTEN)
  find_package(OpenGL REQUIRED)

  target_link_libraries(
    protegon PRIVATE ${OPENGL_LIBRARIES} SDL3::SDL3 SDL3_image::SDL3_image
                     SDL3_ttf::SDL3_ttf SDL3_mixer::SDL3_mixer)
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
         "${CMAKE_CURRENT_SOURCE_DIR}/engine/assets"
  # Keeping this public for testing purposes.
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/engine/src")

# Add d to debug static lib files to differentiate them from release
set_target_properties(protegon PROPERTIES DEBUG_POSTFIX d)

function(add_protegon_to TARGET)

  target_link_libraries(${TARGET} PRIVATE protegon)
  # Copying dlls to executable directory on Windows.
  if(WIN32 AND NOT EMSCRIPTEN)
    add_sdl_dll_copy(${TARGET})
  endif()

endfunction()

message(STATUS "Found protegon")