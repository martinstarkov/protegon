cmake_minimum_required(VERSION 3.20)

# Protegon variables

set(PROTEGON_DIR         ${PROJECT_SOURCE_DIR}              CACHE BOOL "" FORCE)
set(SCRIPT_DIR           ${PROTEGON_DIR}/scripts            CACHE BOOL "" FORCE)
set(EXTERNAL_DIR         ${PROTEGON_DIR}/external           CACHE BOOL "" FORCE)
set(SDL_DIR              ${EXTERNAL_DIR}/sdl2               CACHE BOOL "" FORCE)

# SDL variables

# Versions not enforced on Linux (instead using brew latest version)
set(SDL2_VERSION       2.30.0)
set(SDL2_IMAGE_VERSION 2.8.2)
set(SDL2_MIXER_VERSION 2.8.0)
set(SDL2_TTF_VERSION   2.22.0)
set(SDL2MAIN_LIBRARY FALSE)

set(SDL2_NAME       SDL2-${SDL2_VERSION})
set(SDL2_IMAGE_NAME SDL2_image-${SDL2_IMAGE_VERSION})
set(SDL2_MIXER_NAME SDL2_mixer-${SDL2_MIXER_VERSION})
set(SDL2_TTF_NAME   SDL2_ttf-${SDL2_TTF_VERSION})

function(download_and_extract url output_dir flags)
  execute_process(COMMAND sh ${SCRIPT_DIR}/download_extract_file.sh "${url}" "${output_dir}" "${flags}")
endfunction()

function(install_with_homebrew library archive_name library_location library_path)
  execute_process(COMMAND sh ${SCRIPT_DIR}/check_brew_package.sh ${library} OUTPUT_VARIABLE brew_package_status OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE brew_package_result)
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
    if (BUILD_SHARED_LIBS)
      set(OUTPUT_DIR ${SDL_DIR}-msvc-shared)
    else()
      set(OUTPUT_DIR ${SDL_DIR}-msvc-static)
    endif()
  elseif(MINGW)
    set(WIN_COMPILER_POSTFIX "-mingw")
    set(OUTPUT_DIR ${SDL_DIR}-mingw)
  endif()
  
  set(EXTENSION "zip")
  set(WIN_COMPILER_PREFIX "-devel")
  set(SDL_CMAKE_PATH cmake)
elseif(APPLE)
  set(OUTPUT_DIR ${SDL_DIR}-mac)

  set(SDL2_NAME       SDL2.framework)
  set(SDL2_IMAGE_NAME SDL2_image.framework)
  set(SDL2_MIXER_NAME SDL2_mixer.framework)
  set(SDL2_TTF_NAME   SDL2_ttf.framework)

  set(EXTENSION "dmg")
  set(WIN_COMPILER_PREFIX "")
  set(WIN_COMPILER_POSTFIX "")
  set(SDL_CMAKE_PATH Versions/A/Resources/CMake)
endif()

if ((WIN32 OR APPLE) AND NOT EXISTS ${OUTPUT_DIR})
  if (APPLE OR MINGW OR MSVC AND BUILD_SHARED_LIBS)
    message(STATUS "${OUTPUT_DIR} DOES NOT EXIST")
    download_and_extract(https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2${WIN_COMPILER_PREFIX}-${SDL2_VERSION}${WIN_COMPILER_POSTFIX}.${EXTENSION}                         ${OUTPUT_DIR} "")
    download_and_extract(https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/SDL2_image${WIN_COMPILER_PREFIX}-${SDL2_IMAGE_VERSION}${WIN_COMPILER_POSTFIX}.${EXTENSION} ${OUTPUT_DIR} "")
    download_and_extract(https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL2_MIXER_VERSION}/SDL2_mixer${WIN_COMPILER_PREFIX}-${SDL2_MIXER_VERSION}${WIN_COMPILER_POSTFIX}.${EXTENSION} ${OUTPUT_DIR} "")
    download_and_extract(https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/SDL2_ttf${WIN_COMPILER_PREFIX}-${SDL2_TTF_VERSION}${WIN_COMPILER_POSTFIX}.${EXTENSION}         ${OUTPUT_DIR} "")
  elseif(MSVC AND NOT BUILD_SHARED_LIBS)
    download_and_extract(https://github.com/martinstarkov/SDL2-static-libs/archive/refs/heads/main.zip ${OUTPUT_DIR} "-m")
  endif()
elseif(UNIX AND NOT APPLE)
  execute_process(COMMAND sh ${SCRIPT_DIR}/check_brew.sh
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

  install_with_homebrew(sdl2       SDL2-${SDL2_VERSION}             ${OUTPUT_DIR}/${SDL2_NAME}       https://raw.githubusercontent.com/Homebrew/homebrew-core/5f2b3ba5bb5c4bef36c1e4be278f43601394b729/Formula/s/sdl2.rb)
  install_with_homebrew(sdl2_image SDL2_image-${SDL2_IMAGE_VERSION} ${OUTPUT_DIR}/${SDL2_IMAGE_NAME} https://raw.githubusercontent.com/Homebrew/homebrew-core/6c917a52f9fc1de0dfe5eb962da23288b478423d/Formula/s/sdl2_image.rb)
  install_with_homebrew(sdl2_ttf   SDL2_ttf-${SDL2_TTF_VERSION}     ${OUTPUT_DIR}/${SDL2_TTF_NAME}   https://raw.githubusercontent.com/Homebrew/homebrew-core/254685622723c6603b528e1456b7827e4390a860/Formula/s/sdl2_ttf.rb)
  install_with_homebrew(sdl2_mixer SDL2_mixer-${SDL2_MIXER_VERSION} ${OUTPUT_DIR}/${SDL2_MIXER_NAME} https://raw.githubusercontent.com/Homebrew/homebrew-core/6f7a42bf1a9375d1a2786d90476b32ce366f232d/Formula/s/sdl2_mixer.rb)
  
  set(SDL_CMAKE_PATH lib/cmake)
endif()

set(SDL2_DIR       ${OUTPUT_DIR}/${SDL2_NAME}/${SDL_CMAKE_PATH}       CACHE BOOL "" FORCE)
set(SDL2_image_DIR ${OUTPUT_DIR}/${SDL2_IMAGE_NAME}/${SDL_CMAKE_PATH} CACHE BOOL "" FORCE)
set(SDL2_ttf_DIR   ${OUTPUT_DIR}/${SDL2_TTF_NAME}/${SDL_CMAKE_PATH}   CACHE BOOL "" FORCE)
set(SDL2_mixer_DIR ${OUTPUT_DIR}/${SDL2_MIXER_NAME}/${SDL_CMAKE_PATH} CACHE BOOL "" FORCE)

list(APPEND CMAKE_MODULE_PATH ${SDL2_DIR})
list(APPEND CMAKE_MODULE_PATH ${SDL2_image_DIR})
list(APPEND CMAKE_MODULE_PATH ${SDL2_ttf_DIR})
list(APPEND CMAKE_MODULE_PATH ${SDL2_mixer_DIR})