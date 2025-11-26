function(protegon_add_demo TARGET_NAME SRC_ROOT BIN_ROOT)
  set(SRC_DIR "${SRC_ROOT}/src")
  file(
    GLOB_RECURSE
    SRC_FILES
    CONFIGURE_DEPENDS
    "${SRC_DIR}/*.h"
    "${SRC_DIR}/*.hpp"
    "${SRC_DIR}/*.c"
    "${SRC_DIR}/*.cc"
    "${SRC_DIR}/*.cxx"
    "${SRC_DIR}/*.cpp"
    "${SRC_DIR}/*.mm")

  if(TARGET "${TARGET_NAME}")
    message(
      FATAL_ERROR
        "Target '${TARGET_NAME}' already exists. Generated names must be unique."
    )
  endif()

  add_executable("${TARGET_NAME}" ${SRC_FILES})
  target_include_directories("${TARGET_NAME}" PRIVATE "${PROTEGON_ROOT_DIR}/src"
                                                      "${SRC_DIR}")
  add_protegon_to("${TARGET_NAME}")

  # Per-demo bin dir
  set_target_properties(
    "${TARGET_NAME}"
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BIN_ROOT}"
               LIBRARY_OUTPUT_DIRECTORY "${BIN_ROOT}"
               ARCHIVE_OUTPUT_DIRECTORY "${BIN_ROOT}")

  # Nice executable name = leaf of the demo path
  get_filename_component(_leaf "${SRC_ROOT}" NAME) # e.g., component_hooks
  if(NOT EMSCRIPTEN)
    set_target_properties("${TARGET_NAME}" PROPERTIES OUTPUT_NAME "${_leaf}")
  else()
    set_target_properties("${TARGET_NAME}" PROPERTIES OUTPUT_NAME "index")
  endif()

  if(MSVC)
    set_target_properties(
      "${TARGET_NAME}" PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY
                                  "${BIN_ROOT}/$<CONFIG>")
  endif()

  if(EXISTS "${SRC_ROOT}/resources")
    create_resource_symlink("${TARGET_NAME}" "${SRC_ROOT}" "${BIN_ROOT}"
                            "resources")
  endif()

  if(EMSCRIPTEN)
    # Build flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(EM_OPT "-O0")
    else()
      set(EM_OPT "-O3")
    endif()

    set(EM_PORTS
        "--use-port=sdl2" "--use-port=sdl2_image:formats=bmp,png,xpm,jpg"
        "--use-port=sdl2_mixer" "--use-port=sdl2_ttf")

    target_compile_features("${TARGET_NAME}" PRIVATE cxx_std_20)
    target_compile_options("${TARGET_NAME}" PRIVATE ${EM_OPT})
    target_link_options(
      "${TARGET_NAME}"
      PRIVATE
      ${EM_OPT}
      ${EM_PORTS}
      "--shell-file"
      "${PROTEGON_ROOT_DIR}/emscripten/shell.html"
      "--preload-file"
      "${ASSETS_DIRECTORY}"
      "-s"
      "FULL_ES3=1"
      "-s"
      "ALLOW_MEMORY_GROWTH=1"
      "-s"
      "WARN_ON_UNDEFINED_SYMBOLS=1"
      "-s"
      "NO_EXIT_RUNTIME=1"
      "-s"
      "AGGRESSIVE_VARIABLE_ELIMINATION=1")
    set_target_properties("${TARGET_NAME}" PROPERTIES OUTPUT_NAME "index")
  endif()
endfunction()
