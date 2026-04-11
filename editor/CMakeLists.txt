set(PTGN_EDITOR_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PTGN_EDITOR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/protegon_editor")

add_library(protegon_editor STATIC
  "${PTGN_EDITOR_SRC_DIR}/layer.cpp"
  "${PTGN_EDITOR_SRC_DIR}/panels.cpp"
  "${PTGN_EDITOR_INCLUDE_DIR}/layer.h"
  "${PTGN_EDITOR_INCLUDE_DIR}/panels.h"
)

add_library(protegon::editor ALIAS protegon_editor)

target_include_directories(protegon_editor
  PUBLIC
    ${PTGN_EDITOR_INCLUDE_DIR}
  PRIVATE
    ${PTGN_EDITOR_SRC_DIR}
)

target_compile_features(protegon_editor PUBLIC cxx_std_23)

target_link_libraries(protegon_editor PUBLIC protegon)