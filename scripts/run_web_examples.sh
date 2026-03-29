#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

DIST_DIR="${REPO_ROOT}/build-web/dist"
PORT="${PORT:-8000}"
URL="http://localhost:${PORT}/"
NO_BROWSER="${NO_BROWSER:-0}" # default: open browser (set to 1 to disable)

if [[ ! -d "$DIST_DIR" ]]; then
  echo "[error] Dist directory not found: $DIST_DIR"
  echo "[error] Build the web examples first so build-web/dist exists."
  exit 1
fi

if ! command -v emrun >/dev/null 2>&1; then
  echo "[error] emrun not found on PATH."
  echo "[hint]  Install/activate Emscripten and ensure emrun is available (e.g. 'source /path/to/emsdk_env.sh')."
  exit 1
fi

# Pick an entry HTML file to launch
ENTRY_HTML="${DIST_DIR}/index.html"
if [[ ! -f "$ENTRY_HTML" ]]; then
  mapfile -t HTMLS < <(find "$DIST_DIR" -maxdepth 1 -type f -name "*.html" | sort)
  if [[ "${#HTMLS[@]}" -eq 0 ]]; then
    echo "[error] No .html files found in: $DIST_DIR"
    exit 1
  fi
  ENTRY_HTML="${HTMLS[0]}"
fi

echo "[info] Serving (emrun): $DIST_DIR"
echo "[info] URL:            $URL"
echo "[info] Entry:          $(basename "$ENTRY_HTML")"

EMRUN_ARGS=(--port "$PORT" --serve_root "$DIST_DIR")
if [[ "$NO_BROWSER" == "1" ]]; then
  EMRUN_ARGS+=(--no_browser)
fi

emrun "${EMRUN_ARGS[@]}" "$ENTRY_HTML" \
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


