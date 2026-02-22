#!/bin/bash

# Function to create the symbolic link
create_link() {
  local dest_dir="$1"
  local source_dir="$2"

  # Check if destination already exists (using Windows-specific method)
  if [[ -e "$dest_dir" ]]; then
    echo "Skipping symbolic link creation: '$dest_dir' already exists."
  else
    cmd <<< "mklink /J \"${dest_dir%/}\" \"${source_dir%/}\""
    if [[ $? -eq 0 ]]; then
      echo "Successfully created symbolic link '$dest_dir' pointing to '$source_dir'."
    else
      echo "Error creating symbolic link: exit code '$?'"
    fi
  fi
}

# Check if required arguments are passed
if [[ $# -ne 2 ]]; then
  echo "Error: Please provide destination and source directory paths as arguments."
  exit 1
fi

# Call the create_link function with arguments
create_link "$1" "$2"