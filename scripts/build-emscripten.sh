#!/bin/bash

if [[ -z $(which emcmake) ]]; then
  echo "No emcmake detected.  Is Emscripten installed and in your PATH?"
  echo "Remember to follow the Emscripten SDK instructions."
  exit 1
fi

# deleting index.html so it is refreshed from emscripten/shell.html
cd ../ && mkdir -p build-wasm && cd build-wasm && rm -rf tests/protegon/index.html && emcmake cmake .. && cmake --build .