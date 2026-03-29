#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build-web"
PORT="${PORT:-8000}"
NO_BROWSER="${NO_BROWSER:-0}"   # set 1 to disable
ENTRY_HTML="${ENTRY_HTML:-}"    # optional: override path

command -v emrun >/dev/null 2>&1 || {
  echo "[error] emrun not found (source emsdk_env.sh first)"
  exit 1
}

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "[error] Build dir not found: $BUILD_DIR"
  echo "[hint]  Run: ./scripts/build_web.sh"
  exit 1
fi

# Choose entry html
if [[ -n "$ENTRY_HTML" ]]; then
  # relative -> relative to build dir
  if [[ "$ENTRY_HTML" != /* ]]; then
    ENTRY_HTML="$BUILD_DIR/$ENTRY_HTML"
  fi
else
  # prefer dist/index.html then any html
  if [[ -f "$BUILD_DIR/dist/index.html" ]]; then
    ENTRY_HTML="$BUILD_DIR/dist/index.html"
  else
    ENTRY_HTML="$(find "$BUILD_DIR" -maxdepth 2 -type f -name "*.html" | sort | head -n 1 || true)"
  fi
fi

if [[ -z "$ENTRY_HTML" || ! -f "$ENTRY_HTML" ]]; then
  echo "[error] No entry html found. Build first: ./scripts/build_web.sh"
  exit 1
fi

SERVE_ROOT="$(dirname "$ENTRY_HTML")"
# If it’s inside dist/, serve from dist/. Otherwise serve from build dir.
if [[ "$(basename "$SERVE_ROOT")" == "dist" ]]; then
  SERVE_ROOT="$SERVE_ROOT"
else
  SERVE_ROOT="$BUILD_DIR"
fi

echo "[info] Serving:   $SERVE_ROOT"
echo "[info] Entry:    $ENTRY_HTML"
echo "[info] URL:      http://localhost:${PORT}/"
echo "[info] Options:  PORT=#### NO_BROWSER=1 ENTRY_HTML=relative/or/absolute"

ARGS=(--port "$PORT" --serve_root "$SERVE_ROOT")
if [[ "$NO_BROWSER" == "1" ]]; then
  ARGS+=(--no_browser)
fi

emrun "${ARGS[@]}" "$ENTRY_HTML" \
  > >(grep -v '^Now listening at ') \
  2> >(grep -v '^Now listening at ' >&2) &
PID=$!

cleanup() {
  # kill the process group if possible, fall back to the pid
  kill -- -"$PID" 2>/dev/null || kill "$PID" 2>/dev/null || true
}
trap cleanup INT TERM EXIT

wait "$PID"
trap - INT TERM EXIT

