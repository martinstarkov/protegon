cmake_minimum_required(VERSION 3.16)

# SDL variables

set(SDL_DIR "${PROTEGON_DIR}/external/sdl2" CACHE BOOL "" FORCE)
set(SDL2MAIN_LIBRARY FALSE)

# Versions not enforced on Linux (instead using brew latest version)
set(SDL2_VERSION       2.30.0)
set(SDL2_IMAGE_VERSION 2.8.2)
set(SDL2_MIXER_VERSION 2.8.0)
set(SDL2_TTF_VERSION   2.22.0)

set(SDL2_NAME       SDL2-${SDL2_VERSION})
set(SDL2_IMAGE_NAME SDL2_image-${SDL2_IMAGE_VERSION})
set(SDL2_MIXER_NAME SDL2_mixer-${SDL2_MIXER_VERSION})
set(SDL2_TTF_NAME   SDL2_ttf-${SDL2_TTF_VERSION})

if (${DOWNLOAD_SDL2})

  function(download_and_extract url output_dir flags)
    execute_process(COMMAND sh "${SCRIPT_DIR}/download_extract_file.sh" "${url}" "${output_dir}" "${flags}")
  endfunction()

  function(install_with_homebrew library archive_name library_location library_path)
    execute_process(COMMAND sh "${SCRIPT_DIR}/check_brew_package.sh" ${library}
                    OUTPUT_VARIABLE brew_package_status
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE brew_package_result)
    if(brew_package_result EQUAL 1)
      message(STATUS "Package '${library}' found in Homebrew.")
    else()
      message(STATUS "Package '${library}' not found in Homebrew. Installing...")
      execute_process(COMMAND brew install ${library})
      # DOWNLOAD OLDER VERSION OF BREW LIBRARY
      # set(OUTPUT_DIR ${EXTERNAL_DIR}/sdl2-linux/${archive_name})
      # execute_process(COMMAND brew unlink ${library})
      # execute_process(COMMAND wget ${library_path} -P ${OUTPUT_DIR})
      # execute_process(COMMAND brew install --HEAD -s ${OUTPUT_DIR}/${library}.rb
      #                 WORKING_DIRECTORY ${OUTPUT_DIR})
    endif()

    execute_process(COMMAND brew --prefix "${library}" OUTPUT_VARIABLE location_prefix)
    set(${library_location} "${location_prefix}" PARENT_SCOPE)
  endfunction()

  if (WIN32)
    if(MSVC)
      set(WIN_COMPILER_POSTFIX "-VC")
      if (SHARED_SDL2_LIBS)
        set(OUTPUT_DIR "${SDL_DIR}-msvc-shared")
      else()
        set(OUTPUT_DIR "${SDL_DIR}-msvc-static")
      endif()
    elseif(MINGW)
      set(WIN_COMPILER_POSTFIX "-mingw")
      set(OUTPUT_DIR "${SDL_DIR}-mingw")
    endif()
    
    set(EXTENSION "zip")
    set(WIN_COMPILER_PREFIX "-devel")
    set(SDL_CMAKE_PATH cmake)
  elseif(APPLE)
    set(OUTPUT_DIR "${SDL_DIR}-mac")

    set(SDL2_NAME       SDL2.framework)
    set(SDL2_IMAGE_NAME SDL2_image.framework)
    set(SDL2_MIXER_NAME SDL2_mixer.framework)
    set(SDL2_TTF_NAME   SDL2_ttf.framework)

    set(EXTENSION "dmg")
    set(WIN_COMPILER_PREFIX "")
    set(WIN_COMPILER_POSTFIX "")
    set(SDL_CMAKE_PATH "Versions/A/Resources/CMake")
  endif()

  if ((WIN32 OR APPLE) AND NOT EXISTS ${OUTPUT_DIR})
    if (APPLE OR MINGW OR MSVC AND SHARED_SDL2_LIBS)
      download_and_extract(https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2${WIN_COMPILER_PREFIX}-${SDL2_VERSION}${WIN_COMPILER_POSTFIX}.${EXTENSION}                         "${OUTPUT_DIR}" "")
      download_and_extract(https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/SDL2_image${WIN_COMPILER_PREFIX}-${SDL2_IMAGE_VERSION}${WIN_COMPILER_POSTFIX}.${EXTENSION} "${OUTPUT_DIR}" "")
      download_and_extract(https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL2_MIXER_VERSION}/SDL2_mixer${WIN_COMPILER_PREFIX}-${SDL2_MIXER_VERSION}${WIN_COMPILER_POSTFIX}.${EXTENSION} "${OUTPUT_DIR}" "")
      download_and_extract(https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/SDL2_ttf${WIN_COMPILER_PREFIX}-${SDL2_TTF_VERSION}${WIN_COMPILER_POSTFIX}.${EXTENSION}         "${OUTPUT_DIR}" "")
    elseif(MSVC AND NOT SHARED_SDL2_LIBS)
      download_and_extract(https://github.com/martinstarkov/SDL2-static-libs/archive/refs/heads/main.zip "${OUTPUT_DIR}" "-m")
    endif()
  elseif(UNIX AND NOT APPLE)
    execute_process(COMMAND sh "${SCRIPT_DIR}/check_brew.sh"
                    OUTPUT_VARIABLE brew_installed
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE brew_installed_result)
    if(brew_installed_result EQUAL 1)
      message(STATUS "Homebrew found.")
      set(ENV{HOMEBREW_NO_INSTALL_CLEANUP} TRUE)
      set(ENV{HOMEBREW_NO_ENV_HINTS} TRUE)
    else()
      message(FATAL_ERROR "Homebrew not found. Please install it before continuing.")
    endif()

    install_with_homebrew(sdl2       SDL2-${SDL2_VERSION}             "${OUTPUT_DIR}/${SDL2_NAME}"       https://raw.githubusercontent.com/Homebrew/homebrew-core/5f2b3ba5bb5c4bef36c1e4be278f43601394b729/Formula/s/sdl2.rb)
    install_with_homebrew(sdl2_image SDL2_image-${SDL2_IMAGE_VERSION} "${OUTPUT_DIR}/${SDL2_IMAGE_NAME}" https://raw.githubusercontent.com/Homebrew/homebrew-core/6c917a52f9fc1de0dfe5eb962da23288b478423d/Formula/s/sdl2_image.rb)
    install_with_homebrew(sdl2_ttf   SDL2_ttf-${SDL2_TTF_VERSION}     "${OUTPUT_DIR}/${SDL2_TTF_NAME}"   https://raw.githubusercontent.com/Homebrew/homebrew-core/254685622723c6603b528e1456b7827e4390a860/Formula/s/sdl2_ttf.rb)
    install_with_homebrew(sdl2_mixer SDL2_mixer-${SDL2_MIXER_VERSION} "${OUTPUT_DIR}/${SDL2_MIXER_NAME}" https://raw.githubusercontent.com/Homebrew/homebrew-core/6f7a42bf1a9375d1a2786d90476b32ce366f232d/Formula/s/sdl2_mixer.rb)
    
    set(SDL_CMAKE_PATH lib/cmake)
  endif()

  set(SDL2_DIR       "${OUTPUT_DIR}/${SDL2_NAME}/${SDL_CMAKE_PATH}"       CACHE BOOL "" FORCE)
  set(SDL2_image_DIR "${OUTPUT_DIR}/${SDL2_IMAGE_NAME}/${SDL_CMAKE_PATH}" CACHE BOOL "" FORCE)
  set(SDL2_ttf_DIR   "${OUTPUT_DIR}/${SDL2_TTF_NAME}/${SDL_CMAKE_PATH}"   CACHE BOOL "" FORCE)
  set(SDL2_mixer_DIR "${OUTPUT_DIR}/${SDL2_MIXER_NAME}/${SDL_CMAKE_PATH}" CACHE BOOL "" FORCE)

  list(APPEND CMAKE_MODULE_PATH "${SDL2_DIR}")
  list(APPEND CMAKE_MODULE_PATH "${SDL2_image_DIR}")
  list(APPEND CMAKE_MODULE_PATH "${SDL2_ttf_DIR}")
  list(APPEND CMAKE_MODULE_PATH "${SDL2_mixer_DIR}")

endif()

if (MSVC)
  # Console related flags
  if (ENABLE_CONSOLE)
    target_link_options(protegon PUBLIC "/SUBSYSTEM:CONSOLE")
  else()
    target_link_options(protegon PUBLIC "/SUBSYSTEM:WINDOWS")
    target_compile_definitions(protegon INTERFACE main=WinMain)
  endif()
  # Updates CMAKE_PREFIX_PATH paths to include library architecture
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CMAKE_LIBRARY_ARCHITECTURE "x64")
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CMAKE_LIBRARY_ARCHITECTURE "x86")
  endif()
elseif(MINGW)
  if (ENABLE_CONSOLE)
    target_link_options(protegon PUBLIC "-mconsole")
  else()
    target_link_options(protegon PUBLIC "-mwindows")
  endif()
endif()

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
find_package(SDL2       REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf   REQUIRED)
find_package(SDL2_mixer REQUIRED)
set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)

if (MINGW AND NOT SHARED_SDL2_LIBS)
  set(STATIC_POSTFIX -static)
else()
  set(STATIC_POSTFIX)
endif()

set(SDL_TARGETS SDL2::SDL2${STATIC_POSTFIX}
                SDL2_image::SDL2_image${STATIC_POSTFIX}
                SDL2_ttf::SDL2_ttf${STATIC_POSTFIX}
                SDL2_mixer::SDL2_mixer${STATIC_POSTFIX})

target_link_libraries(protegon PRIVATE ${SDL_TARGETS})

if(WIN32)
  if (MINGW)
    target_link_libraries(protegon PUBLIC shlwapi)
    if (NOT BUILD_SHARED_LIBS)
      target_link_options(protegon PUBLIC "-static")
    endif()
  elseif (MSVC)
    if (NOT SHARED_SDL2_LIBS)
      set(FREETYPE_LIB "${OUTPUT_DIR}/${SDL2_TTF_NAME}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/freetype.lib")
      target_link_libraries(protegon PRIVATE ${FREETYPE_LIB})
    endif()
    target_link_options(protegon PUBLIC $<IF:$<CONFIG:Debug>,/NODEFAULTLIB:MSVCRT,>)
  endif()
  if(NOT SHARED_SDL2_LIBS AND ${DOWNLOAD_SDL2} AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    function(get_target_file_paths TARGETS OUTPUT_VAR)
      set(TARGET_FILE_PATHS)
      foreach(TARGET IN LISTS ${TARGETS})
        list(APPEND TARGET_FILE_PATHS $<TARGET_FILE:${TARGET}>)
      endforeach(TARGET)
      set(${OUTPUT_VAR} ${TARGET_FILE_PATHS} PARENT_SCOPE)
    endfunction(get_target_file_paths)

    get_target_file_paths(SDL_TARGETS SDL_TARGET_FILES)
    if(MSVC)
      list(APPEND SDL_TARGET_FILES ${FREETYPE_LIB})
      add_custom_command(TARGET protegon POST_BUILD
                        COMMAND ${CMAKE_AR} /NOLOGO /OUT:"$<TARGET_FILE:protegon>" "$<TARGET_FILE:protegon>" "${SDL_TARGET_FILES}"
                        COMMAND_EXPAND_LISTS)
    elseif(MINGW)
      add_custom_command(TARGET protegon POST_BUILD
                        COMMAND ${CMAKE_AR}
                        VERBATIM r "$<TARGET_FILE:protegon>" "${SDL_TARGET_FILES}"
                        COMMAND_EXPAND_LISTS)
    endif()
  elseif(SHARED_SDL2_LIBS AND NOT CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    # Copy SDL dlls to parent exe directory
    get_target_property(SDL2_DLL       SDL2::SDL2 	          IMPORTED_LOCATION)
    get_target_property(SDL2_IMAGE_DLL SDL2_image::SDL2_image IMPORTED_LOCATION)
    get_target_property(SDL2_TTF_DLL   SDL2_ttf::SDL2_ttf     IMPORTED_LOCATION)
    get_target_property(SDL2_MIXER_DLL SDL2_mixer::SDL2_mixer IMPORTED_LOCATION)

    get_filename_component(SDL2_LIB_DIR       ${SDL2_DLL}       DIRECTORY)
    get_filename_component(SDL2_IMAGE_LIB_DIR ${SDL2_IMAGE_DLL} DIRECTORY)
    get_filename_component(SDL2_TTF_LIB_DIR   ${SDL2_TTF_DLL}   DIRECTORY)
    get_filename_component(SDL2_MIXER_LIB_DIR ${SDL2_MIXER_DLL} DIRECTORY)

    file(GLOB_RECURSE DLLS CONFIGURE_DEPENDS
      ${SDL2_LIB_DIR}/*.dll
      ${SDL2_IMAGE_LIB_DIR}/*.dll
      ${SDL2_TTF_LIB_DIR}/*.dll
      ${SDL2_MIXER_LIB_DIR}/*.dll)

    set_property(GLOBAL PROPERTY PROTEGON_DLLS ${DLLS})
  endif()
endif()
