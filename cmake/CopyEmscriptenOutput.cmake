# cmake/CopyEmscriptenOutput.cmake
# Inputs:
#  -DINPUT_HTML=/path/to/example_x.html
#  -DDEST_DIR=/path/to/dist

if(NOT DEFINED INPUT_HTML OR NOT DEFINED DEST_DIR)
  message(FATAL_ERROR "CopyEmscriptenOutput.cmake requires INPUT_HTML and DEST_DIR")
endif()

get_filename_component(_dir  "${INPUT_HTML}" DIRECTORY)
get_filename_component(_name "${INPUT_HTML}" NAME)         # example_x.html
get_filename_component(_stem "${INPUT_HTML}" NAME_WE)      # example_x

file(MAKE_DIRECTORY "${DEST_DIR}")

# Copy the html
file(COPY_FILE "${INPUT_HTML}" "${DEST_DIR}/${_name}")

# Copy common sibling outputs if present
foreach(ext js wasm data)
  set(src "${_dir}/${_stem}.${ext}")
  if(EXISTS "${src}")
    file(COPY_FILE "${src}" "${DEST_DIR}/${_stem}.${ext}")
  endif()
endforeach()
