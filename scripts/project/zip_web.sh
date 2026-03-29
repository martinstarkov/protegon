#!/usr/bin/env bash
set -euo pipefail

# Zips the built web output using the "git archive" hack.
# Requires: git
#
# Default input:  ../build-web/dist
# Default output: ../build-web/web.zip
#
# Usage:
#   ./scripts/zip_web.sh
#   ./scripts/zip_web.sh ../build-web/itch.zip
#   DIST_DIR=../build-web/dist ./scripts/zip_web.sh

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

DIST_DIR="${DIST_DIR:-$SCRIPT_DIR/../build-web/dist}"
OUT_ZIP="${1:-$SCRIPT_DIR/../build-web/web.zip}"

if ! command -v git >/dev/null 2>&1; then
  echo "[error] git not found on PATH."
  exit 1
fi

if [[ ! -d "$DIST_DIR" ]]; then
  echo "[error] Dist directory not found: $DIST_DIR"
  echo "[hint]  Build first (build_web.sh) so dist exists."
  exit 1
fi

# Optional sanity check
if [[ ! -f "$DIST_DIR/index.html" ]]; then
  # Not fatal, but usually expected
  echo "[warn] index.html not found in: $DIST_DIR"
  echo "[warn] Zipping anyway."
fi

# Create a temp dir (portable across Git Bash / MSYS)
workdir=""
cleanup() {
  [[ -n "${workdir:-}" && -d "${workdir:-}" ]] && rm -rf "$workdir"
}
trap cleanup EXIT

# mktemp portability: Git Bash usually supports -d -t
workdir="$(mktemp -d -t gitzip.XXXXXX 2>/dev/null || true)"
if [[ -z "$workdir" || ! -d "$workdir" ]]; then
  # fallback: try without -t
  workdir="$(mktemp -d 2>/dev/null || true)"
fi
if [[ -z "$workdir" || ! -d "$workdir" ]]; then
  echo "[error] mktemp failed to create temp directory."
  exit 1
fi

# Copy dist contents into temp working dir
cp -a "$DIST_DIR/." "$workdir/"

# Create an ephemeral git repo and archive it to zip
(
  cd "$workdir"
  git init -q
  git config --local user.email "zip@example.com"
  git config --local user.name "zip"

  # Add all files (including in subdirs)
  git add -A
  git commit -qm "commit for zip"
)

mkdir -p "$(dirname "$OUT_ZIP")"
rm -f "$OUT_ZIP"

# Use --remote trick (same as your old script)
git archive --format=zip -o "$OUT_ZIP" --remote="$workdir" HEAD

echo "[info] Wrote zip: $OUT_ZIP"
