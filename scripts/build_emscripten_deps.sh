#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
BUILD="$REPO_ROOT/external/emscripten"
PREFIX="$BUILD/prefix"

VERSIONS="$REPO_ROOT/cmake/Versions.cmake"

command -v emcmake >/dev/null 2>&1 || { echo "[error] emcmake not found (source emsdk_env.sh)"; exit 1; }
command -v cmake   >/dev/null 2>&1 || { echo "[error] cmake not found"; exit 1; }
command -v ninja   >/dev/null 2>&1 || { echo "[error] ninja not found"; exit 1; }

cmake_var() {
  local name="$1"
  sed -nE "s/^[[:space:]]*set\\($name[[:space:]]+([^[:space:])#]+).*\\).*/\\1/p" "$VERSIONS" | head -n 1
}

SDL_VERSION="$(cmake_var SDL_VERSION)"
SDL_IMAGE_VERSION="$(cmake_var SDL_IMAGE_VERSION)"
SDL_TTF_VERSION="$(cmake_var SDL_TTF_VERSION)"
SDL_MIXER_VERSION="$(cmake_var SDL_MIXER_VERSION)"

mkdir -p "$PREFIX" "$BUILD"

build_install() {
  local name="$1"
  local url="$2"
  local src="$BUILD/src-$name"
  local bld="$BUILD/build-$name"

  if [[ ! -d "$src" ]]; then
    mkdir -p "$src"
    echo "[info] Fetching $name"
    curl -L --fail "$url" -o "$BUILD/$name.zip"
    rm -rf "$src"
    unzip -q "$BUILD/$name.zip" -d "$BUILD"
    rm -f "$BUILD/$name.zip"
    local extracted
    extracted="$(find "$BUILD" -maxdepth 1 -type d -name "${name}-*" | head -n 1)"
    mv "$extracted" "$src"
  fi

  rm -rf "$bld"
  mkdir -p "$bld"

  echo "[info] Configuring $name"
  emcmake cmake -S "$src" -B "$bld" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DSDL3_DIR="$BUILD/build-SDL" \
    -DSDL3_image_DIR="$BUILD/build-SDL_image" \
    -DSDL3_ttf_DIR="$BUILD/build-SDL_ttf" \
    -DSDL3_mixer_DIR="$BUILD/build-SDL_mixer" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DBUILD_SHARED_LIBS=OFF \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF \
    -DSDLIMAGE_SHARED=OFF -DSDLIMAGE_STATIC=ON -DSDLIMAGE_SAMPLES=OFF -DSDLIMAGE_TESTS=OFF \
    -DSDLTTF_SHARED=OFF -DSDLTTF_STATIC=ON -DSDLTTF_SAMPLES=OFF -DSDLTTF_TESTS=OFF \
    -DSDL3MIXER_SHARED=OFF -DSDL3MIXER_STATIC=ON -DSDL3MIXER_SAMPLES=OFF -DSDL3MIXER_TESTS=OFF

  echo "[info] Building $name"
  cmake --build "$bld"

  echo "[info] Installing $name -> $PREFIX"
  cmake --install "$bld"
}

build_install "SDL"       "https://github.com/libsdl-org/SDL/archive/refs/tags/preview-${SDL_VERSION}.zip"
build_install "SDL_image" "https://github.com/libsdl-org/SDL_image/archive/refs/tags/release-${SDL_IMAGE_VERSION}.zip"
build_install "SDL_ttf"   "https://github.com/libsdl-org/SDL_ttf/archive/refs/tags/release-${SDL_TTF_VERSION}.zip"
build_install "SDL_mixer"   "https://github.com/martinstarkov/SDL_mixer/archive/refs/heads/main.zip"
# build_install "SDL_mixer" "https://github.com/libsdl-org/SDL_mixer/archive/refs/tags/release-${SDL_MIXER_VERSION}.zip"

echo "[info] Done. Prefix populated at: $PREFIX"
