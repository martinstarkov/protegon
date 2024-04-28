#!/bin/bash

parent_dir="$1"

cd "$parent_dir" || exit 1

# Loop through all entries in the directory
for entry in *; do
  # Skip anything that's not a directory or "."/.."
  if [[ -d "$entry" ]] && [[ "$entry" != "." ]] && [[ "$entry" != ".." ]]; then
    mv "$parent_dir/$entry" "$parent_dir/.."
  fi
done

rm -rf "$parent_dir"
