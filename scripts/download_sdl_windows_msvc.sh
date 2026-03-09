#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

VERSIONS_CMAKE="$REPO_ROOT/cmake/SDLVersions.cmake"
OUT_ROOT="$REPO_ROOT/external/windows/msvc"

command -v curl  >/dev/null 2>&1 || { echo "[error] curl not found"; exit 1; }
command -v unzip >/dev/null 2>&1 || { echo "[error] unzip not found"; exit 1; }

[[ -f "$VERSIONS_CMAKE" ]] || { echo "[error] Missing $VERSIONS_CMAKE"; exit 1; }
mkdir -p "$OUT_ROOT"

cmake_var() {
  local name="$1"
  sed -nE "s/^[[:space:]]*set\\($name[[:space:]]+([^[:space:])#]+).*\\).*/\\1/p" \
    "$VERSIONS_CMAKE" | head -n 1
}

SDL_VERSION="$(cmake_var SDL_VERSION)"
SDL_IMAGE_VERSION="$(cmake_var SDL_IMAGE_VERSION)"
SDL_TTF_VERSION="$(cmake_var SDL_TTF_VERSION)"
SDL_MIXER_VERSION="$(cmake_var SDL_MIXER_VERSION)"

[[ -n "$SDL_VERSION" ]] || { echo "[error] SDL_VERSION missing"; exit 1; }
[[ -n "$SDL_IMAGE_VERSION" ]] || { echo "[error] SDL_IMAGE_VERSION missing"; exit 1; }
[[ -n "$SDL_TTF_VERSION" ]] || { echo "[error] SDL_TTF_VERSION missing"; exit 1; }
[[ -n "$SDL_MIXER_VERSION" ]] || { echo "[error] SDL_MIXER_VERSION missing"; exit 1; }

download_and_extract() {
  local url_template="$1"
  local version="$2"

  local url="${url_template//\{VERSION\}/$version}"
  local zip_name="${url##*/}"
  local zip_path="$OUT_ROOT/$zip_name"

  local expected_dir="$OUT_ROOT/${zip_name%.zip}"
  [[ -d "$expected_dir" ]] && { echo "[info] ${zip_name%.zip} already present; skipping"; return 0; }

  echo "[info] Downloading ${zip_name%.zip}"
  curl -L --fail -o "$zip_path" "$url"
  unzip -q "$zip_path" -d "$OUT_ROOT"
  rm -f "$zip_path"
}

download_and_extract "https://github.com/libsdl-org/SDL/releases/download/release-{VERSION}/SDL3-devel-{VERSION}-VC.zip"             "$SDL_VERSION"
download_and_extract "https://github.com/libsdl-org/SDL_image/releases/download/release-{VERSION}/SDL3_image-devel-{VERSION}-VC.zip" "$SDL_IMAGE_VERSION"
download_and_extract "https://github.com/libsdl-org/SDL_ttf/releases/download/release-{VERSION}/SDL3_ttf-devel-{VERSION}-VC.zip"     "$SDL_TTF_VERSION"
download_and_extract "https://github.com/libsdl-org/SDL_mixer/releases/download/release-{VERSION}/SDL3_mixer-devel-{VERSION}-VC.zip" "$SDL_MIXER_VERSION"

echo "[info] Done"
