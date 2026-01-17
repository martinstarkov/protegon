include_guard(GLOBAL)

function(_req_var name)
  if(NOT DEFINED ${name} OR "${${name}}" STREQUAL "")
    message(FATAL_ERROR "Missing required version variable: ${name}")
  endif()
endfunction()

_req_var(SDL_VERSION)
_req_var(SDL_IMAGE_VERSION)
_req_var(SDL_TTF_VERSION)
_req_var(SDL_MIXER_VERSION)

if(EMSCRIPTEN)
  set(_EM_PREFIX "${CMAKE_SOURCE_DIR}/external/emscripten/prefix")

  if(NOT EXISTS "${_EM_PREFIX}")
    message(FATAL_ERROR
      "Missing Emscripten SDL prefix: ${_EM_PREFIX}\n"
      "Run your deps script to build+install SDL3/SDL3_image/SDL3_ttf/SDL3_mixer into that prefix."
    )
  endif()

  list(PREPEND CMAKE_PREFIX_PATH "${_EM_PREFIX}")

  find_package(SDL3       ${SDL_VERSION}       CONFIG REQUIRED)
  find_package(SDL3_image ${SDL_IMAGE_VERSION} CONFIG REQUIRED)
  find_package(SDL3_ttf   ${SDL_TTF_VERSION}   CONFIG REQUIRED)
  find_package(SDL3_mixer ${SDL_MIXER_VERSION} CONFIG REQUIRED)

  return()
endif()

# --- native logic unchanged below ---
set(_WIN_MSVC_ROOT  "${CMAKE_SOURCE_DIR}/external/windows/msvc")
set(_WIN_MINGW_ROOT "${CMAKE_SOURCE_DIR}/external/windows/mingw")
set(_MAC_DMG_ROOT   "${CMAKE_SOURCE_DIR}/external/macos/dmg")

set(_prefixes "")

if(WIN32)
  if(MSVC AND EXISTS "${_WIN_MSVC_ROOT}")
    list(APPEND _prefixes "${_WIN_MSVC_ROOT}")
  elseif(MINGW AND EXISTS "${_WIN_MINGW_ROOT}")
    list(APPEND _prefixes "${_WIN_MINGW_ROOT}")
  else()
    if(EXISTS "${_WIN_MSVC_ROOT}")
      list(APPEND _prefixes "${_WIN_MSVC_ROOT}")
    endif()
    if(EXISTS "${_WIN_MINGW_ROOT}")
      list(APPEND _prefixes "${_WIN_MINGW_ROOT}")
    endif()
  endif()

elseif(APPLE)
  if(EXISTS "${_MAC_DMG_ROOT}")
    list(APPEND _prefixes
      "${_MAC_DMG_ROOT}/SDL3-${SDL_VERSION}"
      "${_MAC_DMG_ROOT}/SDL3_image-${SDL_IMAGE_VERSION}"
      "${_MAC_DMG_ROOT}/SDL3_ttf-${SDL_TTF_VERSION}"
      "${_MAC_DMG_ROOT}/SDL3_mixer-${SDL_MIXER_VERSION}"
    )
  endif()

  execute_process(
    COMMAND brew --prefix
    OUTPUT_VARIABLE _BREW_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _BREW_OK
  )
  if(_BREW_OK EQUAL 0 AND EXISTS "${_BREW_PREFIX}")
    list(APPEND _prefixes "${_BREW_PREFIX}")
  endif()
endif()

list(APPEND CMAKE_PREFIX_PATH ${_prefixes})

find_package(SDL3       ${SDL_VERSION}       CONFIG REQUIRED)
find_package(SDL3_image ${SDL_IMAGE_VERSION} CONFIG REQUIRED)
find_package(SDL3_ttf   ${SDL_TTF_VERSION}   CONFIG REQUIRED)
find_package(SDL3_mixer ${SDL_MIXER_VERSION} CONFIG REQUIRED)
