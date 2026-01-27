# examples/CMakeLists.txt
# No project() call here.

# Enable IDE folders (harmless on generators that don't use it)
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
    set(_select_list ${_sel_raw}) # treat as semicolon list
  endif()
endif()

set(_examples_dir "${PTGN_ROOT_DIR}/examples")

# ---- Discover examples by main.cpp convention ----
file(GLOB_RECURSE _example_mains
  CONFIGURE_DEPENDS
  "${_examples_dir}/*/*/main.cpp"
)

if(NOT _example_mains)
  message(STATUS "No examples found under ${_examples_dir}")
  return()
endif()

# Optional: validate requested web examples exist early
if(EMSCRIPTEN AND NOT _select_all)
  foreach(_req IN LISTS _select_list)
    string(STRIP "${_req}" _req)
    if(_req STREQUAL "")
      continue()
    endif()
    if(_req MATCHES "\\.\\.")
      message(FATAL_ERROR "PTGN_WEB_EXAMPLES entry must not contain '..': '${_req}'")
    endif()
    if(NOT EXISTS "${_examples_dir}/${_req}/main.cpp")
      message(FATAL_ERROR
        "Requested web example '${_req}' not found. Expected examples/${_req}/main.cpp")
    endif()
  endforeach()
endif()

# Track target-name collisions if leaf names repeat
set(_used_target_names "")
set(_built_examples "")

foreach(_main IN LISTS _example_mains)
  get_filename_component(_example_dir "${_main}" DIRECTORY) # examples/<cat>/<ex>
  file(RELATIVE_PATH _rel "${_examples_dir}" "${_example_dir}") # <cat>/<ex>

  # selection (only applies to emscripten)
  if(EMSCRIPTEN AND NOT _select_all)
    list(FIND _select_list "${_rel}" _idx)
    if(_idx EQUAL -1)
      continue()
    endif()
  endif()

  # ---- Target name: use leaf folder name ----
  get_filename_component(_leaf "${_rel}" NAME)
  set(_t "${_leaf}")

  # If the leaf name is duplicated across categories, disambiguate
  list(FIND _used_target_names "${_t}" _taken)
  if(NOT _taken EQUAL -1)
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
  
  set(_assets_src "${_examples_dir}/assets")

  add_protegon_to(${_t} ASSETS_DIR "${_assets_src}")

  set_target_properties(${_t} PROPERTIES OUTPUT_NAME "${_t}")
  target_compile_definitions(${_t} PRIVATE PTGN_EXAMPLE_ID="${_rel}")

  if(EXISTS "${_example_dir}/assets")
    set(_assets_src "${_example_dir}/assets")
  endif()

  if(EXISTS "${_assets_src}")
    create_symlink(${_t} "${_assets_src}" "$<TARGET_FILE_DIR:${_t}>")
  endif()
  
  if(MSVC)
    set_target_properties(${_t} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")
  endif()

  # IDE folder grouping
  get_filename_component(_parent_path "${_rel}" DIRECTORY)
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
  foreach(_rel IN LISTS _built_examples)
    get_filename_component(_leaf "${_rel}" NAME)
    set(_t "${_leaf}") # NOTE: if collisions exist, mirror disambiguation for web filenames too
    set(_file "${_t}.html")
    string(APPEND PTGN_EXAMPLE_ITEMS
      "      { label: \"${_rel}\", file: \"${_file}\" },\n"
    )
  endforeach()

  configure_file(
    "${_examples_dir}/web/index.html.in"
    "${PTGN_DIST_DIR}/index.html"
    @ONLY
  )
  configure_file(
    "${_examples_dir}/web/app.js.in"
    "${PTGN_DIST_DIR}/app.js"
    @ONLY
  )
  configure_file(
    "${_examples_dir}/web/style.css"
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
        -P ${PTGN_ROOT_DIR}/cmake/CopyEmscriptenOutput.cmake
    )
  endforeach()
endif()
