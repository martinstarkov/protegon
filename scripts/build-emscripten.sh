#!/bin/bash

default_build_type="Release"

if [[ -z "$1" ]]; then
  build_type=$default_build_type
else
  if [[ "$1" == "Release" || "$1" == "Debug" ]]; then 
    build_type="$1"
  else
    echo "Unrecognized build script argument"
    exit 1
  fi
fi

build_command="cmake --build . --config $build_type"

if [[ -z $(which emcmake) ]]; then
  echo "No emcmake detected.  Is Emscripten installed and in your PATH?"
  echo "Remember to follow the Emscripten SDK instructions."
  exit 1
fi

index_html_location=tests/protegon/index.html

# deleting index.html so it is refreshed from emscripten/shell.html
cd .. && mkdir -p build-wasm && cd build-wasm && rm -rf $index_html_location

echo "CMake Build Type: $build_type"

if [[ "$build_type" == "Release" ]]; then 
  emcmake cmake .. -DCMAKE_BUILD_TYPE=Release
  cmake --build . --config Release -j8
  exit 0
fi

if [[ "$build_type" == "Debug" ]]; then
  emcmake cmake .. -DCMAKE_BUILD_TYPE=Debug
  cmake --build . --config Debug -j8
  exit 0
fi

exit 1