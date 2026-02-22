set(SDL_SAVE_DIR "${SDL_DOWNLOAD_DIR}/sdl2-mac")

download_and_extract(https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.dmg                         "${SDL_SAVE_DIR}" "")
download_and_extract(https://github.com/libsdl-org/SDL_image/releases/download/release-${SDL2_IMAGE_VERSION}/SDL2_image-${SDL2_IMAGE_VERSION}.dmg "${SDL_SAVE_DIR}" "")
download_and_extract(https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL2_MIXER_VERSION}/SDL2_mixer-${SDL2_MIXER_VERSION}.dmg "${SDL_SAVE_DIR}" "")
download_and_extract(https://github.com/libsdl-org/SDL_ttf/releases/download/release-${SDL2_TTF_VERSION}/SDL2_ttf-${SDL2_TTF_VERSION}.dmg         "${SDL_SAVE_DIR}" "")

list(APPEND CMAKE_PREFIX_PATH "${SDL_SAVE_DIR}/SDL2.framework/Versions/A/Resources/CMake")
list(APPEND CMAKE_PREFIX_PATH "${SDL_SAVE_DIR}/SDL2_image.framework/Versions/A/Resources/CMake")
list(APPEND CMAKE_PREFIX_PATH "${SDL_SAVE_DIR}/SDL2_ttf.framework/Versions/A/Resources/CMake")
list(APPEND CMAKE_PREFIX_PATH "${SDL_SAVE_DIR}/SDL2_mixer.framework/Versions/A/Resources/CMake")