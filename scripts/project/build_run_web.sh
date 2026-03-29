#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
"$DIR/build_web.sh"
"$DIR/run_web.sh"
