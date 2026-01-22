# examples/CMakeLists.txt
# No project() call here.

# Selection:
#   -DPTGN_EXAMPLES=ALL
#   -DPTGN_EXAMPLES="basic/hello_world;audio/mixer_demo"
#   -DPTGN_EXAMPLES=   (empty => none)
set(PTGN_EXAMPLES "" CACHE STRING
  "Examples to build: empty=none, ALL=all, or list like basic/hello_world;audio/mixer_demo")

# Enable IDE folders (harmless on generators that don't use it)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

# ---- Parse selection (inside examples/, as requested) ----
set(_sel_raw "${PTGN_EXAMPLES}")
string(STRIP "${_sel_raw}" _sel_raw)
string(TOUPPER "${_sel_raw}" _sel_upper)

if(_sel_raw STREQUAL "" OR _sel_upper STREQUAL "OFF" OR _sel_upper STREQUAL "FALSE" OR _sel_upper STREQUAL "0")
  message(STATUS "Examples disabled (PTGN_EXAMPLES is empty/OFF/FALSE/0)")
  return()
endif()

set(_select_all FALSE)
set(_select_list "")

if(_sel_upper STREQUAL "ALL")
  set(_select_all TRUE)
else()
  # Treat as semicolon list
  set(_select_list ${_sel_raw})
endif()

# ---- Discover examples by main.cpp convention ----
file(GLOB_RECURSE _example_mains
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*/*/main.cpp"
)

if(NOT _example_mains)
  message(STATUS "No examples found under ${CMAKE_CURRENT_SOURCE_DIR}")
  return()
endif()

# Optional: validate requested examples exist early
if(NOT _select_all)
  foreach(_req IN LISTS _select_list)
    string(STRIP "${_req}" _req)
    if(_req STREQUAL "")
      continue()
    endif()
    if(_req MATCHES "\\.\\.")
      message(FATAL_ERROR "PTGN_EXAMPLES entry must not contain '..': '${_req}'")
    endif()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_req}/main.cpp")
      message(FATAL_ERROR
        "Requested example '${_req}' not found. Expected examples/${_req}/main.cpp")
    endif()
  endforeach()
endif()

# Track target-name collisions if leaf names repeat
set(_used_target_names "")

set(_built_examples "")

foreach(_main IN LISTS _example_mains)
  get_filename_component(_example_dir "${_main}" DIRECTORY) # examples/<cat>/<ex>
  file(RELATIVE_PATH _rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_example_dir}") # <cat>/<ex>

  # selection
  if(NOT _select_all)
    list(FIND _select_list "${_rel}" _idx)
    if(_idx EQUAL -1)
      continue()
    endif()
  endif()

  # ---- Target name: use leaf folder name (no prefix) ----
  # Example: basic/hello_world -> target "hello_world"
  get_filename_component(_leaf "${_rel}" NAME)
  set(_t "${_leaf}")

  # If the leaf name is duplicated across categories, disambiguate
  list(FIND _used_target_names "${_t}" _taken)
  if(NOT _taken EQUAL -1)
    # fall back to full relative path with '_' separator
    string(REPLACE "/" "_" _suffix "${_rel}")
    set(_t "${_suffix}")
    list(FIND _used_target_names "${_t}" _taken2)
    if(NOT _taken2 EQUAL -1)
      message(FATAL_ERROR "Target name collision even after disambiguation for '${_rel}'.")
    endif()
  endif()
  list(APPEND _used_target_names "${_t}")

  add_executable(${_t} "${_main}")

  # optional: auto-add other .cpp files in that example folder
  file(GLOB _more CONFIGURE_DEPENDS "${_example_dir}/*.cpp")
  list(REMOVE_ITEM _more "${_main}")
  if(_more)
    target_sources(${_t} PRIVATE ${_more})
  endif()

  # apply engine + shared assets + emscripten/windows behavior
  add_protegon_to(${_t}
    ASSETS_DIR "${CMAKE_SOURCE_DIR}/examples/assets"
  )

  # useful define for logs/UI
  target_compile_definitions(${_t} PRIVATE PTGN_EXAMPLE_ID="${_rel}")

  # ---- IDE folder grouping (nested solution folders) ----
  # Put target under examples/<all parent categories>
  get_filename_component(_parent_path "${_rel}" DIRECTORY)  # e.g., "basic", "audio", "physics/collisions"
  if(_parent_path STREQUAL "." OR _parent_path STREQUAL "")
    set(_folder "examples")
  else()
    set(_folder "examples/${_parent_path}")
  endif()
  set_target_properties(${_t} PROPERTIES FOLDER "${_folder}")

  list(APPEND _built_examples "${_rel}")
endforeach()

if(_built_examples)
  message(STATUS "Built examples: ${_built_examples}")
else()
  if(_select_all)
    message(WARNING "Selection was ALL but no examples were built (unexpected).")
  else()
    message(STATUS "No examples matched selection: ${PTGN_EXAMPLES}")
  endif()
endif()

# ---- Emscripten: generate dist/index and copy outputs for built examples ----
if(EMSCRIPTEN AND _built_examples)
  set(PTGN_DIST_DIR "${CMAKE_BINARY_DIR}/dist" CACHE PATH "Web output folder")
  file(MAKE_DIRECTORY "${PTGN_DIST_DIR}")

  # Build JS array items (one per example)
  set(PTGN_EXAMPLE_ITEMS "")
  foreach(_rel IN LISTS _built_examples)
    # IMPORTANT: if your CopyEmscriptenOutput.cmake expects target-file naming,
    # consider using target name -> file mapping instead of "example_<suffix>.html"
    get_filename_component(_leaf "${_rel}" NAME)
    set(_t "${_leaf}")
    # If leaf might collide, mirror the disambiguation logic or build a map.
    # (Keeping it simple here: assumes no collisions for emscripten usage.)

    set(_file "${_t}.html")
    string(APPEND PTGN_EXAMPLE_ITEMS
      "      { label: \"${_rel}\", file: \"${_file}\" },\n"
    )
  endforeach()

  # Generate dist/index.html from template
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html.in"
    "${PTGN_DIST_DIR}/index.html"
    @ONLY
  )

  # Generate dist/app.js from template
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/web/app.js.in"
    "${PTGN_DIST_DIR}/app.js"
    @ONLY
  )

  # Copy css as-is
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/web/style.css"
    "${PTGN_DIST_DIR}/style.css"
    COPYONLY
  )


  foreach(_rel IN LISTS _built_examples)
    get_filename_component(_leaf "${_rel}" NAME)
    set(_t "${_leaf}")

    add_custom_command(TARGET ${_t} POST_BUILD
      COMMAND ${CMAKE_COMMAND}
        -DINPUT_HTML=$<TARGET_FILE:${_t}>
        -DDEST_DIR=${PTGN_DIST_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/CopyEmscriptenOutput.cmake
    )
  endforeach()
endif()
