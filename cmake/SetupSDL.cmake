set(SDL_VERSION "3.3.2")
set(SDL_IMAGE_VERSION "3.2.4")
set(SDL_TTF_VERSION "3.2.2")
set(SDL_MIXER_VERSION "3.1.0")

# We need VIDEO (window + keyboard/mouse events) and AUDIO (SDL_mixer output).
# Everything else can go.
set(SDL_AUDIO ON) # required for SDL_mixer to talk to the audio device
set(SDL_VIDEO ON)
set(SDL_RENDER OFF) # we have our own renderer
set(SDL_GPU OFF)
set(SDL_CAMERA OFF)
set(SDL_JOYSTICK OFF) # set ON if you actually use controllers
set(SDL_HAPTIC OFF)
set(SDL_HIDAPI OFF)
set(SDL_POWER OFF)
set(SDL_SENSOR OFF)
set(SDL_DIALOG OFF)
set(SDL_TRAY OFF)

# If you don't want SDL_ttf, then remove this section. SDL_ttf – we just want
# TTF rendering, no extra samples/install/emoji/etc.
set(SDLTTF_VENDORED ON) # bundling freetype/harfbuzz/plutosvg if used

# Optional features — disable for minimum footprint
set(SDLTTF_HARFBUZZ OFF) # OFF if you don't need complex text shaping
set(SDLTTF_PLUTOSVG OFF) # OFF if you don't need color emoji
set(SDLTTF_SAMPLES OFF)
set(SDLTTF_INSTALL OFF)
set(SDLTTF_STRICT OFF)
# SDL_mixer – we only want: WAV (built-in), OGG, MP3
set(SDLMIXER_VENDORED ON)

# Enable decoders we care about
set(SDLMIXER_MP3_DRMP3 ON) # MP3 via dr_mp3 (no external mpg123)
set(SDLMIXER_VORBIS_STB ON) # OGG Vorbis via stb_vorbis

# WAV is built-in; there's no separate toggle.

# Disable everything else
set(SDLMIXER_MP3_MPG123 OFF)
set(SDLMIXER_FLAC_DRFLAC OFF)
set(SDLMIXER_OPUS OFF)
set(SDLMIXER_VORBIS_VORBISFILE OFF)
set(SDLMIXER_GME OFF)
set(SDLMIXER_WAVPACK OFF)
set(SDLMIXER_MOD OFF)
set(SDLMIXER_MOD_XMP OFF)

set(SDLMIXER_MIDI_NATIVE OFF)
set(SDLMIXER_MIDI_FLUIDSYNTH OFF)
set(SDLMIXER_MIDI_TIMIDITY OFF)

# If your SDL_mixer version has it:
set(SDLMIXER_STRICT OFF)
# SDL_image (used for loading various image formats) SDL_image (only PNG + JPEG)
set(SDLIMAGE_VENDORED ON)

# Backends: stb is enough in most cases (no extra system libs), keep the others
# off
set(SDLIMAGE_BACKEND_STB ON)
set(SDLIMAGE_BACKEND_WIC OFF) # Windows imaging; turn ON only if you want it
set(SDLIMAGE_BACKEND_IMAGEIO OFF) # On Apple, you *can* leave this ON if you
                                  # like

# Format support: ON only for PNG + JPEG, OFF for everything else
set(SDLIMAGE_PNG ON)
set(SDLIMAGE_JPG ON) # Some versions use SDLIMAGE_JPG...
set(SDLIMAGE_JPEG ON) # ... others use SDLIMAGE_JPEG; set both just in case.

set(SDLIMAGE_AVIF OFF)
set(SDLIMAGE_BMP OFF)
set(SDLIMAGE_GIF OFF)
set(SDLIMAGE_TIF OFF)
set(SDLIMAGE_TIFF OFF) # (depending on exact option name)
set(SDLIMAGE_WEBP OFF)
set(SDLIMAGE_JXL OFF)
set(SDLIMAGE_QOI OFF)
set(SDLIMAGE_SVG OFF)
set(SDLIMAGE_XCF OFF)
set(SDLIMAGE_XPM OFF)
set(SDLIMAGE_LBM OFF)
set(SDLIMAGE_PCX OFF)
set(SDLIMAGE_PNM OFF)
set(SDLIMAGE_TGA OFF)
set(SDLIMAGE_XV OFF)

# If present in your version, keep strict/off so missing libs just disable
# formats, instead of killing the configure step.
set(SDLIMAGE_STRICT OFF)

# Optional: if SDL tries to build extra tools/tests when used as a subproject,
# these keep things lean. They are usually off by default in subdirs, but
# harmless.
set(SDL_TEST OFF)
set(SDL_INSTALL OFF)

if((APPLE AND NOT CMAKE_SYSTEM_NAME MATCHES "Darwin") OR EMSCRIPTEN)
  set(BUILD_SHARED_LIBS
      OFF
      CACHE INTERNAL "") # Disable shared builds on platforms where it does not
                         # make sense to use them
  set(SDL_SHARED OFF)
else()
  set(SDL_SHARED ON)
endif()

function(download_and_extract url output_dir flags)
  execute_process(
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/scripts/download_extract_file.sh"
            "${url}" "${output_dir}" "${flags}")
endfunction()

set(SDL_DOWNLOAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")

if(WIN32 AND PROTEGON_DOWNLOAD_SDL)
  if(MSVC)
    include(cmake/DownloadSDLMSVC.cmake)
  elseif(MINGW)
    include(cmake/DownloadSDLMinGW.cmake)
  endif()
elseif(APPLE)
  include(cmake/DownloadSDLApple.cmake)
endif()

find_package(SDL3 ${SDL_VERSION} REQUIRED)
find_package(SDL3_image ${SDL_IMAGE_VERSION} REQUIRED)
find_package(SDL3_ttf ${SDL_TTF_VERSION} REQUIRED)
find_package(SDL3_mixer ${SDL_MIXER_VERSION} REQUIRED)

if(WIN32)
  function(add_sdl_dll_copy target)
    add_custom_command(TARGET "${target}" POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE:SDL3_image::SDL3_image>
        $<TARGET_FILE:SDL3_ttf::SDL3_ttf>
        $<TARGET_FILE:SDL3_mixer::SDL3_mixer>
        $<TARGET_FILE_DIR:${target}>
      COMMAND_EXPAND_LISTS
    )
  endfunction()
endif()
