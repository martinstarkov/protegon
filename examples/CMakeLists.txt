# examples/CMakeLists.txt

# Selection:
# -DPROTEGON_EXAMPLES=ALL
# -DPROTEGON_EXAMPLES="basic/hello_world;audio/mixer_demo"
set(PROTEGON_EXAMPLES "ALL" CACHE STRING
  "Examples to build (ALL or list like basic/hello_world;audio/mixer_demo)")

# Find all examples by main.cpp convention
file(GLOB_RECURSE _example_mains
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*/*/main.cpp"
)

if(NOT _example_mains)
  message(STATUS "No examples found under ${CMAKE_CURRENT_SOURCE_DIR}")
  return()
endif()

set(_built_examples "")

foreach(_main IN LISTS _example_mains)
  get_filename_component(_example_dir "${_main}" DIRECTORY) # examples/<cat>/<ex>
  file(RELATIVE_PATH _rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_example_dir}") # <cat>/<ex>

  # selection
  if(NOT PROTEGON_EXAMPLES STREQUAL "ALL")
    list(FIND PROTEGON_EXAMPLES "${_rel}" _idx)
    if(_idx EQUAL -1)
      message(STATUS "Skipping example '${_rel}' (not selected)")
      continue()
    endif()
  endif()

  # target name: example_basic__hello_world
  string(REPLACE "/" "__" _suffix "${_rel}")
  set(_t "example_${_suffix}")

  add_executable(${_t} "${_main}")

  # optional: auto-add other .cpp files in that example folder
  file(GLOB _more CONFIGURE_DEPENDS "${_example_dir}/*.cpp")
  list(REMOVE_ITEM _more "${_main}")
  if(_more)
    target_sources(${_t} PRIVATE ${_more})
  endif()

  # apply engine + shared assets + emscripten/windows behavior
  protegon_apply_to_exe(${_t}
    ASSETS_DIR "${CMAKE_SOURCE_DIR}/examples/assets"
  )

  # useful define for logs/UI
  target_compile_definitions(${_t} PRIVATE PROTEGON_EXAMPLE_ID="${_rel}")

  list(APPEND _built_examples "${_rel}")
endforeach()

message(STATUS "Built examples: ${_built_examples}")

if(EMSCRIPTEN)
  set(PROTEGON_DIST_DIR "${CMAKE_BINARY_DIR}/dist" CACHE PATH "Web output folder")
  file(MAKE_DIRECTORY "${PROTEGON_DIST_DIR}")

  # Build JS array items (one per example)
  set(PROTEGON_EXAMPLE_ITEMS "")
  foreach(_rel IN LISTS _built_examples)
    string(REPLACE "/" "__" _suffix "${_rel}")
    set(_file "example_${_suffix}.html")
    string(APPEND PROTEGON_EXAMPLE_ITEMS
      "      { label: \"${_rel}\", file: \"${_file}\" },\n"
    )
  endforeach()

  # Generate dist/index.html from template
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html.in"
    "${PROTEGON_DIST_DIR}/index.html"
    @ONLY
  )

  # Copy each built example output to dist after build (keep your existing logic)
  foreach(_rel IN LISTS _built_examples)
    string(REPLACE "/" "__" _suffix "${_rel}")
    set(_t "example_${_suffix}")

    add_custom_command(TARGET ${_t} POST_BUILD
      COMMAND ${CMAKE_COMMAND}
        -DINPUT_HTML=$<TARGET_FILE:${_t}>
        -DDEST_DIR=${PROTEGON_DIST_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/CopyEmscriptenOutput.cmake
    )
  endforeach()
endif()
