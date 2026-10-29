#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

EXAMPLES_ARG="${1:-ALL}"
BUILD_TYPE="${2:-Release}"

"${SCRIPT_DIR}/build_web_examples.sh" "$EXAMPLES_ARG" "$BUILD_TYPE"
exec "${SCRIPT_DIR}/run_web_examples.sh"