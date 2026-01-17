#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="${REPO_ROOT}/build-wasm"

DEPS_ROOT="${REPO_ROOT}/external/emscripten"
PREFIX="${DEPS_ROOT}/prefix"

command -v emcmake >/dev/null 2>&1 || { echo "[error] emcmake not found (source emsdk_env.sh first)"; exit 1; }
command -v emcc    >/dev/null 2>&1 || { echo "[error] emcc not found (source emsdk_env.sh first)"; exit 1; }
command -v ninja   >/dev/null 2>&1 || { echo "[error] ninja not found"; exit 1; }

[[ -d "$PREFIX" ]] || { echo "[error] Missing deps prefix: $PREFIX (run scripts/build_emscripten_deps.sh)"; exit 1; }

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

emcmake cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PREFIX"

cmake --build "$BUILD_DIR"

OUT_HTML="${BUILD_DIR}/protegon.html"
if [[ -f "$OUT_HTML" ]]; then
  echo "[info] Built: $OUT_HTML"
else
  echo "[error] Expected HTML not found: $OUT_HTML"
  echo "[error] Build outputs:"
  ls -la "$BUILD_DIR" | sed -n '1,160p'
  exit 1
fi
