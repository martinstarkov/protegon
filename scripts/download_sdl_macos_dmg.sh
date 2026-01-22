#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

VERSIONS_CMAKE="$REPO_ROOT/cmake/SDLVersions.cmake"
OUT_ROOT="$REPO_ROOT/external/macos/dmg"

command -v curl   >/dev/null 2>&1 || { echo "[error] curl not found"; exit 1; }
command -v hdiutil >/dev/null 2>&1 || { echo "[error] hdiutil not found"; exit 1; }
command -v ditto  >/dev/null 2>&1 || { echo "[error] ditto not found"; exit 1; }
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

[[ -n "$SDL_VERSION" ]]       || { echo "[error] SDL_VERSION missing"; exit 1; }
[[ -n "$SDL_IMAGE_VERSION" ]] || { echo "[error] SDL_IMAGE_VERSION missing"; exit 1; }
[[ -n "$SDL_TTF_VERSION" ]]   || { echo "[error] SDL_TTF_VERSION missing"; exit 1; }
[[ -n "$SDL_MIXER_VERSION" ]] || { echo "[error] SDL_MIXER_VERSION missing"; exit 1; }

download_mount_copy() {
  local name="$1"        # e.g. SDL3, SDL3_image
  local repo="$2"        # e.g. SDL, SDL_image
  local tag_prefix="$3"  # e.g. release, preview
  local version="$4"     # e.g. 3.4.0
  local dmg_name="$5"    # e.g. SDL3-3.4.0.dmg

  local dest_dir="$OUT_ROOT/${name}-${version}"
  [[ -d "$dest_dir" ]] && { echo "[info] ${name}-${version} already present; skipping"; return 0; }

  local url="https://github.com/libsdl-org/${repo}/releases/download/${tag_prefix}-${version}/${dmg_name}"

  local tmp_dir dmg mount_point=""
  tmp_dir="$(mktemp -d)"
  dmg="$tmp_dir/$dmg_name"

  cleanup() {
    if [[ -n "$mount_point" ]]; then
      hdiutil detach "$mount_point" -quiet >/dev/null 2>&1 || true
    fi
    rm -rf "$tmp_dir"
  }
  trap cleanup RETURN

  echo "[info] Downloading $url"
  curl -L --fail -o "$dmg" "$url"

  echo "[info] Mounting $dmg_name"
  mount_point="$(hdiutil attach "$dmg" -nobrowse -readonly | awk 'END{print $NF}')"
  [[ -d "$mount_point" ]] || { echo "[error] mount failed for $dmg_name"; return 1; }

  echo "[info] Copying to $dest_dir"
  mkdir -p "$dest_dir"
  ditto "$mount_point" "$dest_dir"
}

download_mount_copy "SDL3"       "SDL"       "release" "$SDL_VERSION"       "SDL3-${SDL_VERSION}.dmg"
download_mount_copy "SDL3_image" "SDL_image" "release" "$SDL_IMAGE_VERSION" "SDL3_image-${SDL_IMAGE_VERSION}.dmg"
download_mount_copy "SDL3_ttf"   "SDL_ttf"   "release" "$SDL_TTF_VERSION"   "SDL3_ttf-${SDL_TTF_VERSION}.dmg"
download_mount_copy "SDL3_mixer" "SDL_mixer" "prerelease" "$SDL_MIXER_VERSION" "SDL3_mixer-${SDL_MIXER_VERSION}.dmg"

echo "[info] Done"
