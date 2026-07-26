#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="${REPO_ROOT}/build-web"

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[error] Required command not found: $1"
    exit 1
  fi
}

require_command emcmake
require_command emcc
require_command cmake
require_command ninja

# Usage:
#   ./scripts/build_web_examples.sh
#       Builds all examples in Release.
#
#   ./scripts/build_web_examples.sh ALL Debug
#       Builds all examples in Debug.
#
#   ./scripts/build_web_examples.sh "audio/basic;graphics/sprite" RelWithDebInfo
#       Builds selected examples in RelWithDebInfo.
EXAMPLES_ARG="${1:-ALL}"
BUILD_TYPE="${2:-Release}"

case "$BUILD_TYPE" in
  Debug|Release|RelWithDebInfo|MinSizeRel)
    ;;
  *)
    echo "[error] Invalid build type: $BUILD_TYPE"
    echo "[error] Expected one of: Debug, Release, RelWithDebInfo, MinSizeRel"
    exit 1
    ;;
esac

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

CMAKE_ARGS=(
  -S "$REPO_ROOT"
  -B "$BUILD_DIR"
  -G Ninja
  "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
  "-DPTGN_EXAMPLES=${EXAMPLES_ARG}"
)

echo "[info] CMAKE_BUILD_TYPE=${BUILD_TYPE}"
echo "[info] PTGN_EXAMPLES=${EXAMPLES_ARG}"

emcmake cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR"

OUT_INDEX="${BUILD_DIR}/dist/index.html"

if [[ ! -f "$OUT_INDEX" ]]; then
  echo "[error] Expected output not found: $OUT_INDEX"
  echo "[error] Build directory contents:"
  find "$BUILD_DIR" -maxdepth 2 -type f | sort | sed -n '1,160p'
  exit 1
fi

echo "[info] Built: $OUT_INDEX"