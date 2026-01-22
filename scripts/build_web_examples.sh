#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="${REPO_ROOT}/build-web"

DEPS_ROOT="${REPO_ROOT}/external/emscripten"
PREFIX="${DEPS_ROOT}/prefix"

command -v emcmake >/dev/null 2>&1 || { echo "[error] emcmake not found (source emsdk_env.sh first)"; exit 1; }
command -v emcc    >/dev/null 2>&1 || { echo "[error] emcc not found (source emsdk_env.sh first)"; exit 1; }
command -v ninja   >/dev/null 2>&1 || { echo "[error] ninja not found"; exit 1; }

[[ -d "$PREFIX" ]] || { echo "[error] Missing web dependencies: run scripts/build_web_dependencies.sh"; exit 1; }

# Optional argument:
#   ./scripts/build_web.sh                      -> builds ALL examples
#   ./scripts/build_web.sh ALL                  -> builds ALL examples
#   ./scripts/build_web.sh "a/b;c/d"            -> builds selected examples
EXAMPLES_ARG="${1:-ALL}"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

CMAKE_ARGS=(
  -S "$REPO_ROOT"
  -B "$BUILD_DIR"
  -G Ninja
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_PREFIX_PATH="$PREFIX"
  "-DPTGN_EXAMPLES=${EXAMPLES_ARG}"
)

echo "[info] PTGN_EXAMPLES=${EXAMPLES_ARG}"

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
