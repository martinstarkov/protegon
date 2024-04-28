#!/bin/bash

which -s brew
if [[ $? != 0 ]]; then
  exit 0 # Brew not found
else
  exit 1 # Brew found
fi