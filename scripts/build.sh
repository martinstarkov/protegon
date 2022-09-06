#!/usr/bin/env bash

# Build protegon on any system with a bash shell
cd ..
rm -rf build
mkdir build
cd build
cmake ..
cmake --build .
