set(PTGN_EDITOR_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PTGN_EDITOR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(PTGN_EDITOR_FILES 
  "${PTGN_EDITOR_SRC_DIR}/layer.cpp"
  "${PTGN_EDITOR_SRC_DIR}/panels.cpp"
  "${PTGN_EDITOR_INCLUDE_DIR}/protegon_editor/layer.h"
  "${PTGN_EDITOR_INCLUDE_DIR}/protegon_editor/panels.h")

add_library(protegon_editor STATIC ${PTGN_EDITOR_FILES})

add_library(protegon::editor ALIAS protegon_editor)

target_include_directories(protegon_editor
  PUBLIC
    ${PTGN_EDITOR_INCLUDE_DIR}
  PRIVATE
    ${PTGN_EDITOR_SRC_DIR}
)

target_compile_features(protegon_editor PUBLIC cxx_std_23)

target_link_libraries(protegon_editor PUBLIC protegon)

if(NOT EMSCRIPTEN)
  set_project_warnings(protegon_editor "${PTGN_WARNINGS_AS_ERRORS}")
  set_compiler_settings(protegon_editor "${CMAKE_CURRENT_SOURCE_DIR}" ${PTGN_EDITOR_FILES})
endif()