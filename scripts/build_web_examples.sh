#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="${REPO_ROOT}/build-web"

command -v emcmake >/dev/null 2>&1 || { echo "[error] emcmake not found (source emsdk_env.sh first)"; exit 1; }
command -v emcc    >/dev/null 2>&1 || { echo "[error] emcc not found (source emsdk_env.sh first)"; exit 1; }
command -v ninja   >/dev/null 2>&1 || { echo "[error] ninja not found"; exit 1; }

# Optional arguments:
#   ./scripts/build_web.sh
#       -> Release build, ALL web examples
#
#   ./scripts/build_web.sh ALL
#       -> Release build, ALL web examples
#
#   ./scripts/build_web.sh "a/b;c/d"
#       -> Release build, selected web examples
#
#   ./scripts/build_web.sh ALL Debug
#       -> Debug build, ALL web examples
#
#   ./scripts/build_web.sh "a/b;c/d" RelWithDebInfo
#       -> RelWithDebInfo build, selected web examples
WEB_EXAMPLES_ARG="${1:-ALL}"
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
  -DPTGN_EXAMPLES=ON
  "-DPTGN_WEB_EXAMPLES=${WEB_EXAMPLES_ARG}"
)

echo "[info] CMAKE_BUILD_TYPE=${BUILD_TYPE}"
echo "[info] PTGN_WEB_EXAMPLES=${WEB_EXAMPLES_ARG}"

emcmake cmake "${CMAKE_ARGS[@]}"

cmake --build "$BUILD_DIR"

OUT_INDEX="${BUILD_DIR}/dist/index.html"
OUT_HTML="${BUILD_DIR}/protegon.html"

if [[ -f "$OUT_INDEX" ]]; then
  echo "[info] Built: $OUT_INDEX"
elif [[ -f "$OUT_HTML" ]]; then
  echo "[info] Built: $OUT_HTML"
else
  echo "[error] Expected output not found."
  echo "[error] Looked for:"
  echo "        - $OUT_INDEX"
  echo "        - $OUT_HTML"
  echo "[error] Build outputs:"
  ls -la "$BUILD_DIR" | sed -n '1,160p'
  exit 1
fi
