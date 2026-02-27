#!/usr/bin/env bash
set -euo pipefail

# Simple Emscripten build for an external Protegon-based project.

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build-web"
BUILD_TYPE="${BUILD_TYPE:-Release}"   # optional: Debug/Release
CLEAN="${CLEAN:-1}"                  # optional: set to 0 for incremental

command -v emcmake >/dev/null 2>&1 || { echo "[error] emcmake not found (source emsdk_env.sh first)"; exit 1; }
command -v emcc    >/dev/null 2>&1 || { echo "[error] emcc not found (source emsdk_env.sh first)"; exit 1; }
command -v ninja   >/dev/null 2>&1 || { echo "[error] ninja not found"; exit 1; }

if [[ "$CLEAN" == "1" ]]; then
  rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

echo "[info] Building web"
echo "[info] ROOT      = $ROOT"
echo "[info] BUILD_DIR = $BUILD_DIR"
echo "[info] TYPE      = $BUILD_TYPE"

emcmake cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR"

# Common places projects emit an entry HTML
CANDIDATES=(
  "$BUILD_DIR/dist/index.html"
  "$BUILD_DIR/index.html"
  "$BUILD_DIR/${PWD##*/}.html"
)

ENTRY=""
for f in "${CANDIDATES[@]}"; do
  if [[ -f "$f" ]]; then
    ENTRY="$f"
    break
  fi
done

if [[ -z "$ENTRY" ]]; then
  # Fall back to any html we can find (top-level or dist)
  ENTRY="$(find "$BUILD_DIR" -maxdepth 2 -type f -name "*.html" | sort | head -n 1 || true)"
fi

if [[ -z "$ENTRY" || ! -f "$ENTRY" ]]; then
  echo "[error] Build finished, but no .html output found in: $BUILD_DIR"
  echo "[hint]  Listing build dir:"
  ls -la "$BUILD_DIR" | sed -n '1,160p'
  exit 1
fi

echo "[info] Built entry: $ENTRY"
