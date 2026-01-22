#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
VERSIONS_CMAKE="$REPO_ROOT/cmake/SDLVersions.cmake"

command -v brew >/dev/null 2>&1 || { echo "[error] Homebrew (brew) not found"; exit 1; }
[[ -f "$VERSIONS_CMAKE" ]] || { echo "[error] Missing $VERSIONS_CMAKE"; exit 1; }

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

install_or_upgrade() {
  local formula="$1"
  if brew list --formula "$formula" >/dev/null 2>&1; then
    brew upgrade "$formula" >/dev/null 2>&1 || true
  else
    brew install "$formula"
  fi
}

installed_version() {
  local formula="$1"
  # output format: "formula 3.4.0"
  brew list --versions "$formula" 2>/dev/null | awk '{print $2}' | head -n 1
}

assert_version() {
  local formula="$1"
  local expected="$2"
  local got
  got="$(installed_version "$formula" || true)"

  if [[ -z "$got" ]]; then
    echo "[error] $formula not installed"
    exit 1
  fi

  # Homebrew versions can include suffixes like "3.4.0_1"; compare the prefix.
  if [[ "$got" != "$expected"* ]]; then
    echo "[error] $formula version mismatch: expected $expected, got $got"
    echo "        Homebrew usually installs the latest version."
    echo "        If you need an exact version, use a versioned formula (if it exists) or pin via a tap:"
    echo "          brew search $formula"
    echo "          brew extract --version=$expected $formula <your/tap>"
    exit 2
  fi
}

brew update >/dev/null

install_or_upgrade sdl3
install_or_upgrade sdl3_image
install_or_upgrade sdl3_ttf
install_or_upgrade sdl3_mixer

assert_version sdl3       "$SDL_VERSION"
assert_version sdl3_image "$SDL_IMAGE_VERSION"
assert_version sdl3_ttf   "$SDL_TTF_VERSION"
assert_version sdl3_mixer "$SDL_MIXER_VERSION"

echo "[info] Done"
