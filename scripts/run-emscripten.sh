#!/bin/bash

if [[ -z $(which emrun) ]]; then
  echo "No emrun detected.  Is Emscripten installed and in your PATH?"
  echo "Remember to follow the Emscripten SDK instructions."
  exit 1
fi

builddir=../build-wasm/tests/protegon/

if [ ! -e $builddir/index.html ]; then
  echo "Could not find built index.html file. Run build-emscripten.sh first."
  exit 1
fi

cd $builddir && emrun index.html