#!/bin/bash

package_name="$1"

if brew list | grep -q "^$package_name\$"; then
  exit 0  # Package found
else
  exit 1  # Package not found
fi
