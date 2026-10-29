#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

DIST_DIR="${REPO_ROOT}/build-web/dist"
PORT="${PORT:-8000}"
NO_BROWSER="${NO_BROWSER:-0}"

if [[ ! -d "$DIST_DIR" ]]; then
  echo "[error] Dist directory not found: $DIST_DIR"
  echo "[error] Build the web examples first."
  exit 1
fi

if ! command -v emrun >/dev/null 2>&1; then
  echo "[error] emrun not found on PATH."
  echo "[hint] Install Emscripten or activate your emsdk environment."
  exit 1
fi

ENTRY_HTML="${DIST_DIR}/index.html"

if [[ ! -f "$ENTRY_HTML" ]]; then
  ENTRY_HTML=""

  # Avoid mapfile because Apple's bundled Bash does not provide it.
  # Shell glob expansion is already sorted lexicographically.
  for candidate in "$DIST_DIR"/*.html; do
    if [[ -f "$candidate" ]]; then
      ENTRY_HTML="$candidate"
      break
    fi
  done

  if [[ -z "$ENTRY_HTML" ]]; then
    echo "[error] No .html files found in: $DIST_DIR"
    exit 1
  fi
fi

ENTRY_NAME="$(basename "$ENTRY_HTML")"
URL="http://localhost:${PORT}/${ENTRY_NAME}"

echo "[info] Serving: $DIST_DIR"
echo "[info] URL:     $URL"
echo "[info] Entry:   $ENTRY_NAME"

EMRUN_ARGS=(
  --port "$PORT"
  --serve_root "$DIST_DIR"
)

if [[ "$NO_BROWSER" == "1" ]]; then
  EMRUN_ARGS+=(--no_browser)
fi

exec emrun "${EMRUN_ARGS[@]}" "$ENTRY_HTML"