set(PROTEGON_SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/scripts" CACHE BOOL "")

function(create_resource_symlink TARGET DIR_NAME)
	set(SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}")
	set(DESTINATION_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${DIR_NAME}")
  file(TO_NATIVE_PATH ${SOURCE_DIRECTORY} _src_dir)
  if (MSVC OR XCODE)
    set(EXE_DEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${DIR_NAME}")
    file(TO_NATIVE_PATH ${EXE_DEST_DIR} _exe_dir)
    if (MSVC)
      add_custom_command(TARGET ${TARGET} COMMAND "${PROTEGON_SCRIPT_DIR}/create_link_win.sh" "${_exe_dir}" "${_src_dir}")
    elseif(XCODE)
      message(STATUS "Creating Symlink from ${SOURCE_DIRECTORY} to ${EXE_DEST_DIR}")
      add_custom_command(TARGET ${TARGET} COMMAND ln -sf "${SOURCE_DIRECTORY}" "${EXE_DEST_DIR}")
    endif()
      # This is for distributing the binaries
      #add_custom_command(TARGET ${TARGET} COMMAND ${SYMLINK_COMMAND})
  endif()
	if (NOT EXISTS ${DESTINATION_DIRECTORY})
		if (WIN32)
      file(TO_NATIVE_PATH "${DESTINATION_DIRECTORY}" _dst_dir)
      # This is for MSVC IDE
      execute_process(COMMAND cmd.exe /c mklink /J "${_dst_dir}" "${_src_dir}")
		elseif(APPLE)
			message(STATUS "Creating Symlink from ${SOURCE_DIRECTORY} to ${DESTINATION_DIRECTORY}")
			execute_process(COMMAND ln -sf "${SOURCE_DIRECTORY}" "${DESTINATION_DIRECTORY}")
		elseif(UNIX AND NOT APPLE)
			execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${SOURCE_DIRECTORY}" "${DESTINATION_DIRECTORY}")
		endif()
	endif()
endfunction()