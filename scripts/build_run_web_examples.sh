#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

# Optional:
#   ./scripts/build_and_run_web_examples.sh
#       -> ALL examples, Release
#
#   ./scripts/build_and_run_web_examples.sh ALL
#       -> ALL examples, Release
#
#   ./scripts/build_and_run_web_examples.sh ALL Debug
#       -> ALL examples, Debug
#
#   ./scripts/build_and_run_web_examples.sh "a/b;c/d" RelWithDebInfo
#       -> selected examples, RelWithDebInfo
EXAMPLES_ARG="${1:-ALL}"
BUILD_TYPE="${2:-Release}"

"${SCRIPT_DIR}/build_web_examples.sh" "$EXAMPLES_ARG" "$BUILD_TYPE"
"${SCRIPT_DIR}/run_web_examples.sh"