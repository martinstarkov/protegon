set(PTGN_EDITOR_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PTGN_EDITOR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

file(GLOB_RECURSE PTGN_EDITOR_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false "${PTGN_EDITOR_SRC_DIR}/*.cpp")
file(GLOB_RECURSE PTGN_EDITOR_HEADERS CONFIGURE_DEPENDS LIST_DIRECTORIES false "${PTGN_EDITOR_SRC_DIR}/*.h" "${PTGN_EDITOR_INCLUDE_DIR}/*.h")

list(
  APPEND
  PTGN_EDITOR_SOURCES
  "${PTGN_COMPONENT_REGISTRATION_SOURCES}"
)

set(PTGN_EDITOR_FILES ${PTGN_EDITOR_SOURCES} ${PTGN_EDITOR_HEADERS})

add_library(protegon_editor STATIC ${PTGN_EDITOR_FILES})

add_library(protegon::editor ALIAS protegon_editor)

target_include_directories(protegon_editor
  PUBLIC
  ${PTGN_EDITOR_INCLUDE_DIR}
  ${PTGN_EDITOR_SRC_DIR}
)
target_compile_features(protegon_editor PUBLIC cxx_std_23)
target_link_libraries(protegon_editor PUBLIC protegon)
target_compile_definitions(protegon_editor PUBLIC PTGN_EDITOR=1)
if(EMSCRIPTEN AND (CMAKE_BUILD_TYPE STREQUAL "Debug"))
  target_compile_definitions(protegon_editor PRIVATE JSON_DIAGNOSTICS=1)
endif()
target_precompile_headers(protegon_editor PRIVATE "${PTGN_EDITOR_SRC_DIR}/ptgn_editor_pch.h")

if(NOT EMSCRIPTEN)
  set_project_warnings(protegon_editor "${PTGN_WARNINGS_AS_ERRORS}")
  set_compiler_settings(protegon_editor "${CMAKE_CURRENT_SOURCE_DIR}" ${PTGN_EDITOR_FILES})
endif()