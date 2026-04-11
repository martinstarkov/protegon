# examples/CMakeLists.txt
# No project() call here.

# Enable IDE folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

set(_select_all TRUE)
set(_select_list "")

if(EMSCRIPTEN)
  set(_sel_raw "${PTGN_WEB_EXAMPLES}")
  string(STRIP "${_sel_raw}" _sel_raw)
  string(TOUPPER "${_sel_raw}" _sel_upper)

  if(_sel_raw STREQUAL "")
    message(STATUS "Web examples disabled (PTGN_WEB_EXAMPLES is empty)")
    return()
  endif()

  if(_sel_upper STREQUAL "ALL")
    set(_select_all TRUE)
  else()
    set(_select_all FALSE)
    set(_select_list ${_sel_raw}) # semicolon-separated list
  endif()
endif()

set(_examples_dir "${PTGN_ROOT_DIR}/examples")
set(_web_dir "${_examples_dir}/web")
set(_default_assets_dir "${_examples_dir}/assets")

# Discover examples by single .cpp files
file(GLOB_RECURSE _example_sources CONFIGURE_DEPENDS "${_examples_dir}/*.cpp")

if(NOT _example_sources)
  message(STATUS "No examples found under ${_examples_dir}")
  return()
endif()

# Validate requested web examples early
if(EMSCRIPTEN AND NOT _select_all)
  foreach(_req IN LISTS _select_list)
    string(STRIP "${_req}" _req)
    if(_req STREQUAL "")
      continue()
    endif()

    if(_req MATCHES "\\.\\.")
      message(FATAL_ERROR "PTGN_WEB_EXAMPLES entry must not contain '..': '${_req}'")
    endif()

    if(NOT EXISTS "${_examples_dir}/${_req}.cpp")
      message(FATAL_ERROR
        "Requested web example '${_req}' not found. Expected examples/${_req}.cpp")
    endif()
  endforeach()
endif()

if(EMSCRIPTEN)
  set(_shell_html "${_web_dir}/shell.html")
  if(NOT EXISTS "${_shell_html}")
    message(FATAL_ERROR "Missing Emscripten shell file: ${_shell_html}")
  endif()
endif()

# Track target-name collisions if leaf names repeat
set(_used_target_names "")
set(_built_examples "")
set(_built_targets "")

foreach(_src IN LISTS _example_sources)
  file(RELATIVE_PATH _rel "${_examples_dir}" "${_src}") # e.g. audio/basic.cpp

  # Skip web support sources if present under examples/web
  if(_rel MATCHES "^web/")
    continue()
  endif()

  get_filename_component(_example_dir "${_src}" DIRECTORY)
  get_filename_component(_rel_no_ext "${_rel}" NAME_WE)
  get_filename_component(_rel_dir "${_rel}" DIRECTORY)

  if(_rel_dir STREQUAL "")
    set(_rel_key "${_rel_no_ext}")
  else()
    set(_rel_key "${_rel_dir}/${_rel_no_ext}")
  endif()

  # Selection only applies to Emscripten builds
  if(EMSCRIPTEN AND NOT _select_all)
    list(FIND _select_list "${_rel_key}" _idx)
    if(_idx EQUAL -1)
      continue()
    endif()
  endif()

  # Target name defaults to leaf filename
  get_filename_component(_leaf "${_src}" NAME_WE)
  set(_t "${_leaf}")

  # Disambiguate duplicate leaf names
  list(FIND _used_target_names "${_t}" _taken)
  if(NOT _taken EQUAL -1)
    string(REPLACE "/" "_" _suffix "${_rel_key}")
    set(_t "${_suffix}")

    list(FIND _used_target_names "${_t}" _taken2)
    if(NOT _taken2 EQUAL -1)
      message(FATAL_ERROR
        "Target name collision even after disambiguation for '${_rel_key}'.")
    endif()
  endif()
  list(APPEND _used_target_names "${_t}")

  add_executable(${_t} "${_src}")

  # Choose assets directory before wiring Protegon
  set(_assets_src "${_default_assets_dir}")
  if(EXISTS "${_example_dir}/assets")
    set(_assets_src "${_example_dir}/assets")
  endif()

  if(EMSCRIPTEN)
    add_protegon_to(${_t}
      ASSETS_DIR "${_assets_src}"
      SHELL_HTML "${_shell_html}"
    )
  else()
    add_protegon_to(${_t})
  endif()

  set_target_properties(${_t} PROPERTIES OUTPUT_NAME "${_t}")
  target_compile_definitions(${_t} PRIVATE PTGN_EXAMPLE_ID="${_rel}")

  if(MSVC)
    set_target_properties(${_t} PROPERTIES
      VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")
  endif()

  # IDE folder grouping
  get_filename_component(_parent_path "${_rel}" DIRECTORY)
  if(_parent_path STREQUAL "." OR _parent_path STREQUAL "")
    set(_folder "examples")
  else()
    set(_folder "examples/${_parent_path}")
  endif()
  set_target_properties(${_t} PROPERTIES FOLDER "${_folder}")

  list(APPEND _built_examples "${_rel_key}")
  list(APPEND _built_targets "${_t}")
endforeach()

if(_built_examples)
  message(STATUS "Built examples: ${_built_examples}")
else()
  if(EMSCRIPTEN AND _select_all)
    message(WARNING "Selection was ALL but no examples were built (unexpected).")
  elseif(EMSCRIPTEN)
    message(STATUS "No web examples matched selection: ${PTGN_WEB_EXAMPLES}")
  else()
    message(WARNING "No examples were built (unexpected).")
  endif()
endif()

# ---- Emscripten: generate dist/index and copy outputs for built examples ----
if(EMSCRIPTEN AND _built_examples)
  set(PTGN_DIST_DIR "${CMAKE_BINARY_DIR}/dist" CACHE PATH "Web output folder")
  file(MAKE_DIRECTORY "${PTGN_DIST_DIR}")

  set(PTGN_EXAMPLE_ITEMS "")

  list(LENGTH _built_examples _n_examples)
  math(EXPR _last_index "${_n_examples} - 1")

  foreach(_i RANGE ${_last_index})
    list(GET _built_examples ${_i} _rel_key)
    list(GET _built_targets  ${_i} _t)

    string(APPEND PTGN_EXAMPLE_ITEMS
      "      { label: \"${_rel_key}\", file: \"${_t}.html\" },\n")
  endforeach()

  configure_file(
    "${_web_dir}/index.html.in"
    "${PTGN_DIST_DIR}/index.html"
    @ONLY
  )

  configure_file(
    "${_web_dir}/app.js.in"
    "${PTGN_DIST_DIR}/app.js"
    @ONLY
  )

  configure_file(
    "${_web_dir}/style.css"
    "${PTGN_DIST_DIR}/style.css"
    COPYONLY
  )

  foreach(_t IN LISTS _built_targets)
    add_custom_command(
      TARGET ${_t}
      POST_BUILD
      COMMAND
        ${CMAKE_COMMAND}
          -DINPUT_HTML=$<TARGET_FILE:${_t}>
          -DDEST_DIR=${PTGN_DIST_DIR}
          -P ${PTGN_ROOT_DIR}/cmake/CopyEmscriptenOutput.cmake
    )
  endforeach()
endif()