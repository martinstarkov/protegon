set(SDL_SAVE_DIR "${SDL_DOWNLOAD_DIR}/sdl2-mingw")

download_and_extract(https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-mingw.zip                         "${SDL_SAVE_DIR}" "")
download_and_extract(https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/SDL2_image-devel-${SDL2_IMAGE_VERSION}-mingw.zip "${SDL_SAVE_DIR}" "")
download_and_extract(https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL2_MIXER_VERSION}/SDL2_mixer-devel-${SDL2_MIXER_VERSION}-mingw.zip "${SDL_SAVE_DIR}" "")
download_and_extract(https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/SDL2_ttf-devel-${SDL2_TTF_VERSION}-mingw.zip         "${SDL_SAVE_DIR}" "")

list(APPEND CMAKE_PREFIX_PATH "${SDL_SAVE_DIR}/SDL2-${SDL2_VERSION}/")
list(APPEND CMAKE_PREFIX_PATH "${SDL_SAVE_DIR}/SDL2_image-${SDL2_IMAGE_VERSION}/")
list(APPEND CMAKE_PREFIX_PATH "${SDL_SAVE_DIR}/SDL2_ttf-${SDL2_TTF_VERSION}/")
list(APPEND CMAKE_PREFIX_PATH "${SDL_SAVE_DIR}/SDL2_mixer-${SDL2_MIXER_VERSION}/")