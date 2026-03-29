#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
OUT_ZIP="${1:-}"  # optional: custom output path

"$DIR/build_web.sh"
if [[ -n "$OUT_ZIP" ]]; then
  "$DIR/zip_web.sh" "$OUT_ZIP"
else
  "$DIR/zip_web.sh"
fi
