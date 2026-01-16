set(SDL_SAVE_DIR "${SDL_DOWNLOAD_DIR}/sdl-msvc")

set(SDL_LOCATION       "${SDL_SAVE_DIR}/SDL3-${SDL_VERSION}")
set(SDL_IMAGE_LOCATION "${SDL_SAVE_DIR}/SDL3_image-${SDL_IMAGE_VERSION}")
set(SDL_TTF_LOCATION   "${SDL_SAVE_DIR}/SDL3_ttf-${SDL_TTF_VERSION}")
set(SDL_MIXER_LOCATION "${SDL_SAVE_DIR}/SDL3_mixer-${SDL_MIXER_VERSION}")

if(NOT EXISTS ${SDL_LOCATION})
# TODO: Replace preview with release once SDL3_mixer is released.
download_and_extract(https://github.com/libsdl-org/SDL/releases/download/preview-${SDL_VERSION}/SDL3-devel-${SDL_VERSION}-VC.zip                         "${SDL_SAVE_DIR}" "")
endif()  
if(NOT EXISTS ${SDL_IMAGE_LOCATION})
download_and_extract(https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL_IMAGE_VERSION}/SDL3_image-devel-${SDL_IMAGE_VERSION}-VC.zip "${SDL_SAVE_DIR}" "")
endif()
if(NOT EXISTS ${SDL_TTF_LOCATION})
download_and_extract(https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL_TTF_VERSION}/SDL3_ttf-devel-${SDL_TTF_VERSION}-VC.zip         "${SDL_SAVE_DIR}" "")
endif()
if(NOT EXISTS ${SDL_MIXER_LOCATION})
# TODO: Fix with SDL3_mixer once it is released:
# download_and_extract(https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL_MIXER_VERSION}/SDL3_mixer-devel-${SDL_MIXER_VERSION}-VC.zip "${SDL_SAVE_DIR}" "")
download_and_extract(https://github.com/martinstarkov/SDL_mixer/releases/download/${SDL_MIXER_VERSION}/SDL3_mixer-devel-${SDL_MIXER_VERSION}-VC.zip "${SDL_SAVE_DIR}" "")
endif()

list(APPEND CMAKE_PREFIX_PATH "${SDL_LOCATION}/cmake")
list(APPEND CMAKE_PREFIX_PATH "${SDL_IMAGE_LOCATION}/cmake")
list(APPEND CMAKE_PREFIX_PATH "${SDL_TTF_LOCATION}/cmake")
list(APPEND CMAKE_PREFIX_PATH "${SDL_MIXER_LOCATION}/cmake")