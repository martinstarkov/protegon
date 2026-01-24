#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"

BUILD_SCRIPT="${SCRIPT_DIR}/build_web_examples.sh"
SERVE_SCRIPT="${SCRIPT_DIR}/run_web_examples.sh"

# Optional:
#   ./scripts/build_and_run_web_examples.sh
#   ./scripts/build_and_run_web_examples.sh ALL
#   ./scripts/build_and_run_web_examples.sh "audio/test_audio;audio/test_visuals"
EXAMPLES_ARG="${1:-ALL}"

[[ -f "$BUILD_SCRIPT" ]] || { echo "[error] Missing: $BUILD_SCRIPT"; exit 1; }
[[ -f "$SERVE_SCRIPT" ]] || { echo "[error] Missing: $SERVE_SCRIPT"; exit 1; }

echo "[info] Building web examples (PTGN_WEB_EXAMPLES=${EXAMPLES_ARG})..."
"$BUILD_SCRIPT" "$EXAMPLES_ARG"

echo "[info] Launching web server..."
"$SERVE_SCRIPT"
