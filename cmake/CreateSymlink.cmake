# cmake/CreateSymlink.cmake
#
# Provides:
#   create_symlink(<target> <src_path> <dest_root>)
#
# Also acts as a build-time script when invoked via:
#   cmake -DPTGN_SRC=... -DPTGN_DST=... -P cmake/CreateSymlink.cmake

# -----------------------------
# Build-time script entrypoint
# -----------------------------
function(_ptgn_create_link_impl)
  if(NOT DEFINED PTGN_SRC OR NOT DEFINED PTGN_DST)
    message(FATAL_ERROR "CreateSymlink.cmake requires -DPTGN_SRC and -DPTGN_DST when run with -P")
  endif()

  # Strip accidental surrounding quotes
  string(REPLACE "\"" "" PTGN_SRC "${PTGN_SRC}")
  string(REPLACE "\"" "" PTGN_DST "${PTGN_DST}")

  file(TO_CMAKE_PATH "${PTGN_SRC}" _src)
  file(TO_CMAKE_PATH "${PTGN_DST}" _dst)

  if(NOT EXISTS "${_src}")
    message(FATAL_ERROR "Symlink source does not exist: '${_src}'")
  endif()

  # If destination already exists, do nothing
  if(EXISTS "${_dst}")
    message(STATUS "Symlink/junction exists, skipping: '${_dst}'")
    return()
  endif()

  # Ensure parent directory exists
  get_filename_component(_dst_parent "${_dst}" DIRECTORY)
  if(NOT EXISTS "${_dst_parent}")
    file(MAKE_DIRECTORY "${_dst_parent}")
  endif()

  # Windows: prefer junctions (no admin/dev-mode required)
  if(WIN32)
    file(TO_NATIVE_PATH "${_src}" _src_native)
    file(TO_NATIVE_PATH "${_dst}" _dst_native)

    execute_process(
      COMMAND cmd.exe /c mklink /J "${_dst_native}" "${_src_native}"
      RESULT_VARIABLE _rv
      OUTPUT_VARIABLE _out
      ERROR_VARIABLE  _err
    )

    if(NOT _rv EQUAL 0)
      message(FATAL_ERROR
        "Failed to create junction:\n"
        "  dst: ${_dst_native}\n"
        "  src: ${_src_native}\n"
        "  exit: ${_rv}\n"
        "  out: ${_out}\n"
        "  err: ${_err}\n")
    endif()

    message(STATUS "Created junction: '${_dst}' -> '${_src}'")
  else()
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E create_symlink "${_src}" "${_dst}"
      RESULT_VARIABLE _rv
      OUTPUT_VARIABLE _out
      ERROR_VARIABLE  _err
    )

    if(NOT _rv EQUAL 0)
      message(FATAL_ERROR
        "Failed to create symlink:\n"
        "  dst: ${_dst}\n"
        "  src: ${_src}\n"
        "  exit: ${_rv}\n"
        "  out: ${_out}\n"
        "  err: ${_err}\n")
    endif()

    message(STATUS "Created symlink: '${_dst}' -> '${_src}'")
  endif()
endfunction()

# If we're being run as a script (-P), do the work and stop.
if(CMAKE_SCRIPT_MODE_FILE)
  # Only run if the caller passed PTGN_SRC/PTGN_DST; otherwise it's being included.
  if(DEFINED PTGN_SRC OR DEFINED PTGN_DST)
    _ptgn_create_link_impl()
  endif()
  return()
endif()

# -----------------------------
# Configure-time function
# -----------------------------
function(create_symlink TARGET SRC_PATH DEST_ROOT)
  if(NOT TARGET "${TARGET}")
    message(AUTHOR_WARNING "${TARGET} is not a target, thus no symlink was added.")
    return()
  endif()

  get_filename_component(_name "${SRC_PATH}" NAME)
  if(_name STREQUAL "" OR _name STREQUAL "." OR _name STREQUAL "..")
    message(FATAL_ERROR "create_symlink: Could not infer name from SRC_PATH='${SRC_PATH}'")
  endif()

  # Always link into DEST_ROOT/<name>.
  # (DEST_ROOT can be a generator expression like $<TARGET_FILE_DIR:...>.)
  set(_dst "${DEST_ROOT}/${_name}")

  add_custom_command(
    TARGET "${TARGET}"
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      -DPTGN_SRC:PATH=${SRC_PATH}
      -DPTGN_DST:PATH=${_dst}
      -P "${PTGN_ROOT_DIR}/cmake/CreateSymlink.cmake"
    VERBATIM
  )
endfunction()
