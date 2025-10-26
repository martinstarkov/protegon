# No project() call here. The root CMakeLists should have the single project().

include(${PROTEGON_ROOT_DIR}/cmake/ProtegonDemo.cmake)

message(STATUS "Adding demos")

# Find all 'src' dirs under demos
file(GLOB_RECURSE DEMO_SRC_DIRS
     LIST_DIRECTORIES true
     RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
     "*/src")
list(REMOVE_DUPLICATES DEMO_SRC_DIRS)

# --- map to parent demo dirs and filter out any under 'resources' ---
set(DEMO_DIRS "")
foreach(src_rel IN LISTS DEMO_SRC_DIRS)
  # parent directory of the src/ folder (e.g., "core/script_sequence")
  get_filename_component(parent_rel "${src_rel}" DIRECTORY)

  # normalize + lowercase to filter 'resources' reliably
  string(REPLACE "\\" "/" parent_norm "${parent_rel}")
  string(TOLOWER "${parent_norm}" parent_lc)
  if (parent_lc MATCHES "(^|/)resources(/|$)")
    continue()
  endif()

  list(APPEND DEMO_DIRS "${parent_rel}")
endforeach()
list(REMOVE_DUPLICATES DEMO_DIRS)

# (optional) skip demos whose src/ has no compile-able sources
set(FILTERED_DEMO_DIRS "")
foreach(demo_rel IN LISTS DEMO_DIRS)
  file(GLOB_RECURSE _has_sources
       "${CMAKE_CURRENT_SOURCE_DIR}/${demo_rel}/src/*.c"
       "${CMAKE_CURRENT_SOURCE_DIR}/${demo_rel}/src/*.cc"
       "${CMAKE_CURRENT_SOURCE_DIR}/${demo_rel}/src/*.cxx"
       "${CMAKE_CURRENT_SOURCE_DIR}/${demo_rel}/src/*.cpp"
       "${CMAKE_CURRENT_SOURCE_DIR}/${demo_rel}/src/*.mm")
  if (_has_sources)
    list(APPEND FILTERED_DEMO_DIRS "${demo_rel}")
  endif()
endforeach()

# --- create one target per *parent* demo dir (not the src/ dir) ---
foreach(demo_rel IN LISTS FILTERED_DEMO_DIRS)
  # Target name: prefix to avoid clashes; '/' -> '_'
  string(REPLACE "/" "_" target_base "${demo_rel}")
  set(TARGET_NAME "${target_base}")

  set(SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/${demo_rel}")
  set(BIN_ROOT "${CMAKE_CURRENT_BINARY_DIR}/${demo_rel}")

  #message(STATUS "Adding demo: ${demo_rel}, src: ${SRC_ROOT}, bin: ${BIN_ROOT}")

  protegon_add_demo("${TARGET_NAME}" "${SRC_ROOT}" "${BIN_ROOT}")

  if(MSVC)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
    
# demo_rel is the relative demo path like "core/script_sequence" or "physics/collisions/advanced"
# Put the target under demos/<all-parent-categories>
get_filename_component(_parent_path "${demo_rel}" DIRECTORY)  # e.g., ".", "core", "physics/collisions"

if(_parent_path STREQUAL "." OR _parent_path STREQUAL "")
  set(_folder "demos")
else()
  # VS accepts '/' as separators; this gives nested solution folders
  set(_folder "demos/${_parent_path}")
endif()

set_target_properties("${TARGET_NAME}" PROPERTIES FOLDER "${_folder}")
  endif()
endforeach()
