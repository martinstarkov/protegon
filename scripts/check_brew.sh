#!/bin/bash

if brew --version > /dev/null; then
  exit 1 # Homebrew found.
else
  exit 0 # Homebrew not found.
fi