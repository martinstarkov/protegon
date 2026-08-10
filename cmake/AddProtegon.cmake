
function(_ptgn_add_build_info target assets_dir)
	get_target_property(
		_ptgn_example_id
		${target}
		PTGN_EXAMPLE_ID
	)

	if(
		NOT _ptgn_example_id
		OR _ptgn_example_id MATCHES "-NOTFOUND$"
	)
		set(_ptgn_example_id "")
	endif()

	get_filename_component(
		_ptgn_engine_source_directory
		"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.."
		ABSOLUTE
	)

	if(assets_dir)
		get_filename_component(
			_ptgn_asset_source_directory
			"${assets_dir}"
			ABSOLUTE
		)

		get_filename_component(
			_ptgn_asset_directory_name
			"${_ptgn_asset_source_directory}"
			NAME
		)

		get_filename_component(
			_ptgn_desktop_runtime_root
			"${_ptgn_asset_source_directory}"
			DIRECTORY
		)
	else()
		set(
			_ptgn_asset_source_directory
			""
		)

		set(
			_ptgn_asset_directory_name
			""
		)

		set(
			_ptgn_desktop_runtime_root
			"${CMAKE_SOURCE_DIR}"
		)
	endif()

	if(EMSCRIPTEN)
		set(
			_ptgn_runtime_root
			"/"
		)

		if(_ptgn_asset_directory_name)
			set(
				_ptgn_runtime_asset_directory
				"/${_ptgn_asset_directory_name}"
			)
		else()
			set(
				_ptgn_runtime_asset_directory
				""
			)
		endif()

		set(PTGN_INFO_WEB true)
	else()
		set(
			_ptgn_runtime_root
			"${_ptgn_desktop_runtime_root}"
		)

		set(
			_ptgn_runtime_asset_directory
			"${_ptgn_asset_source_directory}"
		)

		set(PTGN_INFO_WEB false)
	endif()

	set(
		PTGN_INFO_SOURCE_DIRECTORY
		"${CMAKE_SOURCE_DIR}"
	)

	set(
		PTGN_INFO_BINARY_DIRECTORY
		"${CMAKE_BINARY_DIR}"
	)

	set(
		PTGN_INFO_ENGINE_DIRECTORY
		"${_ptgn_engine_source_directory}"
	)

	set(
		PTGN_INFO_RUNTIME_ROOT
		"${_ptgn_runtime_root}"
	)

	set(
		PTGN_INFO_ASSET_SOURCE_DIRECTORY
		"${_ptgn_asset_source_directory}"
	)

	set(
		PTGN_INFO_ASSET_DIRECTORY
		"${_ptgn_runtime_asset_directory}"
	)

	set(
		PTGN_INFO_TARGET
		"${target}"
	)

	set(
		PTGN_INFO_GENERATOR
		"${CMAKE_GENERATOR}"
	)

	set(
		PTGN_INFO_EXAMPLE_ID
		"${_ptgn_example_id}"
	)

	set(
		_ptgn_generated_directory
		"${CMAKE_CURRENT_BINARY_DIR}/ptgn_generated"
	)

	file(
		MAKE_DIRECTORY
		"${_ptgn_generated_directory}"
	)

	set(
		_ptgn_generated_build_info
		"${_ptgn_generated_directory}/${target}_build_info.cpp"
	)

	configure_file(
		"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/BuildInfo.cpp.in"
		"${_ptgn_generated_build_info}"
		@ONLY
	)

	target_sources(
		${target}
		PRIVATE
		"${_ptgn_generated_build_info}"
	)
endfunction()

function(add_protegon_to target)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "add_protegon_to: target '${target}' does not exist")
  endif()

  set(options)
  set(oneValueArgs ASSETS_DIR SHELL_HTML)
  cmake_parse_arguments(PTGN "${options}" "${oneValueArgs}" "" ${ARGN})

  if(PTGN_EDITOR)
    if(NOT TARGET protegon_editor)
      message(FATAL_ERROR
        "add_protegon_to(${target}): PTGN_EDITOR is ON but protegon_editor target does not exist"
      )
    endif()

    target_link_libraries(${target} PRIVATE protegon_editor)
  else()
    target_link_libraries(${target} PRIVATE protegon)
  endif()

  if(EMSCRIPTEN)
    if(NOT PTGN_ASSETS_DIR)
      message(FATAL_ERROR
        "add_protegon_to(${target}): ASSETS_DIR is required when building with Emscripten"
      )
    endif()

    if(NOT PTGN_SHELL_HTML)
      set(PTGN_SHELL_HTML "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../platform/emscripten/shell.html")
    endif()

    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/dist")

    set_target_properties(${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
      OUTPUT_NAME "index"
      SUFFIX ".html"
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_compile_options(${target} PRIVATE -O0)
    else()
      target_compile_options(${target} PRIVATE -O3)
    endif()

    get_filename_component(
      _ptgn_asset_mount_name
      "${PTGN_ASSETS_DIR}"
      NAME
    )

    target_link_options(${target} PRIVATE
      "--shell-file=${PTGN_SHELL_HTML}"
      "--preload-file=${PTGN_ASSETS_DIR}@/${_ptgn_asset_mount_name}"
      "-sALLOW_MEMORY_GROWTH=1"
      "-sFULL_ES3=1"
      "-sWARN_ON_UNDEFINED_SYMBOLS=1"
      "-sNO_EXIT_RUNTIME=1"
      "-sUSE_ZLIB=1"
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_compile_options(${target} PRIVATE
        -O0
        -g3
        -fexceptions
        "-sDISABLE_EXCEPTION_CATCHING=0"
      )

      target_link_options(${target} PRIVATE
        -O0
        -g3
        -fexceptions
        "-sDISABLE_EXCEPTION_CATCHING=0"
        "-sASSERTIONS=2"
        "-sSTACK_OVERFLOW_CHECK=2"
        "-sSAFE_HEAP=1"
      )

      target_compile_definitions(${target} PRIVATE
        JSON_DIAGNOSTICS=1
      )
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
      target_compile_options(${target} PRIVATE
        -O2
        -g3
      )

      target_link_options(${target} PRIVATE
        -O2
        -g3
        "-sASSERTIONS=1"
      )

    else()
      target_compile_options(${target} PRIVATE
        -O3
      )

      target_link_options(${target} PRIVATE
        -O3
        "-sASSERTIONS=1"
      )
    endif()
  endif()

  _ptgn_add_build_info(
    ${target}
    "${PTGN_ASSETS_DIR}"
  )
endfunction()

function(add_protegon_to_example target example_id)
	if(NOT TARGET ${target})
		message(FATAL_ERROR
			"add_protegon_to_example: target '${target}' does not exist"
		)
	endif()

	if("${example_id}" STREQUAL "")
		message(FATAL_ERROR
			"add_protegon_to_example(${target}): example ID cannot be empty"
		)
	endif()

	set_property(
		TARGET ${target}
		PROPERTY
		PTGN_EXAMPLE_ID "${example_id}"
	)

	add_protegon_to(
		${target}
		${ARGN}
	)
endfunction()