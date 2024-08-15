if (MSVC)
  # Group files for MSVC project tree
  set(PROTEGON_FILES ${PROTEGON_SOURCES} ${PROTEGON_HEADERS} ${PROTEGON_SHADERS})
  foreach(_source IN ITEMS ${PROTEGON_FILES})
    get_filename_component(_source_path "${_source}" PATH)
    file(RELATIVE_PATH _source_path_rel "${CMAKE_CURRENT_SOURCE_DIR}/src" ${_source_path})
    string(REPLACE "/" "\\" _group_path ${_source_path_rel})
    source_group(${_group_path} FILES "${_source}")
  endforeach()
endif()