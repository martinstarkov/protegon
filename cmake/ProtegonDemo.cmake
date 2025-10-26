function(protegon_link_resources TARGET SRC_ROOT BIN_ROOT ASSETS_DIR)
  set(src "${SRC_ROOT}/${ASSETS_DIR}")
  set(dst "${BIN_ROOT}/${ASSETS_DIR}")
  if (NOT EXISTS "${src}")
    return()
  endif()

  # Do it after the target builds so BIN_ROOT exists.
  add_custom_command(TARGET "${TARGET}" POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BIN_ROOT}"
    # clean any old link/copy
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${dst}"
    # try to create a link first
    COMMAND ${CMAKE_COMMAND} -E create_symlink "${src}" "${dst}"
    VERBATIM
  )

  # On Windows, symlinks may need admin/Dev Mode; if create_symlink fails,
  # the build would spew. Add a second, best-effort fallback to COPY.
  # We can do this by wrapping a small script so failure of the link doesn’t fail the build.
  set(script "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_reslink.cmake")
  file(WRITE "${script}" "
    file(MAKE_DIRECTORY \"${BIN_ROOT}\")
    if(EXISTS \"${src}\")
      # Try symbolic link first
      file(CREATE_LINK \"${src}\" \"${dst}\" SYMBOLIC RESULT _res)
      if(NOT _res EQUAL 0)
        # Fallback: copy (quiet)
        file(REMOVE_RECURSE \"${dst}\")
        file(COPY \"${src}\" DESTINATION \"${BIN_ROOT}\")
      endif()
    endif()
  ")

  add_custom_command(TARGET "${TARGET}" POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -P "${script}"
    VERBATIM
  )
endfunction()

function(protegon_add_demo TARGET_NAME SRC_ROOT BIN_ROOT)
  set(SRC_DIR "${SRC_ROOT}/src")
  file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
       "${SRC_DIR}/*.h" "${SRC_DIR}/*.hpp" "${SRC_DIR}/*.c" "${SRC_DIR}/*.cc" "${SRC_DIR}/*.cxx" "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.mm")

  if (TARGET "${TARGET_NAME}")
    message(FATAL_ERROR "Target '${TARGET_NAME}' already exists. Generated names must be unique.")
  endif()

  add_executable("${TARGET_NAME}" ${SRC_FILES})
  target_include_directories("${TARGET_NAME}" PRIVATE "${PROTEGON_ROOT_DIR}/src" "${SRC_DIR}")
  add_protegon_to("${TARGET_NAME}")

  # Per-demo bin dir
  set_target_properties("${TARGET_NAME}" PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BIN_ROOT}"
    LIBRARY_OUTPUT_DIRECTORY "${BIN_ROOT}"
    ARCHIVE_OUTPUT_DIRECTORY "${BIN_ROOT}"
  )

  # Nice executable name = leaf of the demo path
  get_filename_component(_leaf "${SRC_ROOT}" NAME)      # e.g., component_hooks
  if (NOT EMSCRIPTEN)
    set_target_properties("${TARGET_NAME}" PROPERTIES OUTPUT_NAME "${_leaf}")
  else()
    set_target_properties("${TARGET_NAME}" PROPERTIES OUTPUT_NAME "index")
  endif()

  # Resources (robust)
  protegon_link_resources("${TARGET_NAME}" "${SRC_ROOT}" "${BIN_ROOT}" "resources")

  if (EMSCRIPTEN)
    # Build flags
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(EM_OPT "-O0")
    else()
      set(EM_OPT "-O3")
    endif()

    set(EM_PORTS
      "--use-port=sdl2"
      "--use-port=sdl2_image:formats=bmp,png,xpm,jpg"
      "--use-port=sdl2_mixer"
      "--use-port=sdl2_ttf"
    )

    target_compile_features("${TARGET_NAME}" PRIVATE cxx_std_20)
    target_compile_options("${TARGET_NAME}" PRIVATE ${EM_OPT})
    target_link_options("${TARGET_NAME}" PRIVATE
      ${EM_OPT}
      ${EM_PORTS}
      "--shell-file" "${PROTEGON_ROOT_DIR}/emscripten/shell.html"
      "--preload-file" "${ASSETS_DIRECTORY}"
      "-s" "FULL_ES3=1"
      "-s" "ALLOW_MEMORY_GROWTH=1"
      "-s" "WARN_ON_UNDEFINED_SYMBOLS=1"
      "-s" "NO_EXIT_RUNTIME=1"
      "-s" "AGGRESSIVE_VARIABLE_ELIMINATION=1"
    )
    set_target_properties("${TARGET_NAME}" PROPERTIES OUTPUT_NAME "index")
  endif()
endfunction()
