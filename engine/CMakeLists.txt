add_subdirectory(vendor)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/CMakeRC.cmake)

set(PTGN_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(PTGN_SHADER_DIR "${PTGN_ASSETS_DIR}/shader")
set(PTGN_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PTGN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

file(GLOB_RECURSE PTGN_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false "${PTGN_SRC_DIR}/*.cpp")
file(GLOB_RECURSE PTGN_HEADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false "${PTGN_SRC_DIR}/*.h" "${PTGN_INCLUDE_DIR}/*.h")
file(GLOB_RECURSE PTGN_SHADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false "${PTGN_SHADER_DIR}/*.glsl")
  
set(PTGN_FILES ${PTGN_SOURCES} ${PTGN_HEADERS} ${PTGN_SHADERS})

add_library(protegon STATIC ${PTGN_FILES})

add_library(protegon::protegon ALIAS protegon)

cmrc_add_resource_library(assets_shader ALIAS assets::shader NAMESPACE shader WHENCE "${PTGN_SHADER_DIR}" ${PTGN_SHADERS} "${PTGN_SHADER_DIR}/manifest.json")

target_include_directories(protegon
  PUBLIC
    "${PTGN_INCLUDE_DIR}"
    "${PTGN_SRC_DIR}"
  PRIVATE
    "${PTGN_ASSETS_DIR}"
)

target_compile_features(protegon PUBLIC cxx_std_23)

target_link_libraries(protegon
  PUBLIC
    assets::shader
    ptgn_ecs
    nlohmann_json::nlohmann_json
    ptgn_imgui
    magic_enum::magic_enum
  PRIVATE
    ptgn_glad
    glfw
    ptgn_miniaudio
    ptgn_stb
    Freetype::Freetype
    msdf-atlas-gen::msdf-atlas-gen
)

if (NOT EMSCRIPTEN)
  target_link_libraries(protegon PRIVATE nfd)
endif()

set_target_properties(protegon PROPERTIES DEBUG_POSTFIX d)

if(NOT EMSCRIPTEN)
  set_project_warnings(protegon "${PTGN_WARNINGS_AS_ERRORS}")
  set_compiler_settings(protegon "${CMAKE_CURRENT_SOURCE_DIR}" ${PTGN_FILES})

  find_package(OpenGL REQUIRED)
  target_link_libraries(protegon PRIVATE OpenGL::GL)
endif()