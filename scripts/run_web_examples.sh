#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

DIST_DIR="${REPO_ROOT}/build-web/dist"
PORT="8000"
URL="http://localhost:${PORT}/"

if command -v python3 >/dev/null 2>&1; then
  PYTHON="python3"
elif command -v python >/dev/null 2>&1; then
  PYTHON="python"
else
  echo "[error] Python not found. Install Python 3 and ensure it's on PATH."
  exit 1
fi

if [[ ! -d "$DIST_DIR" ]]; then
  echo "[error] Dist directory not found: $DIST_DIR"
  echo "[error] Build the web examples first so build-web/dist exists."
  exit 1
fi

echo "[info] Serving: $DIST_DIR"
echo "[info] URL:     $URL"

exec "$PYTHON" -m http.server "$PORT" --directory "$DIST_DIR"
