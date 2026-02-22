#!/bin/bash

# Function to exit the script with an error message
error_exit() {
  echo "Error: $1" >&2
  exit 1
}

# Get download URL, output directory, and move script flag (optional)
download_url="$1"
output_dir="$2"
move_subdirs="$3"

# Informative message for missing arguments (less strict than error_exit)
if [[ -z "$download_url" || -z "$output_dir" ]]; then
  echo "Missing arguments. Usage: $0 <download_url> <output_dir> [-m]"
  exit 1
fi

# Validate move script flag if provided (optional check)
if [[ ! -z "$move_subdirs" && "$move_subdirs" != "-m" ]]; then
  echo "Warning: Invalid move script flag '$move_subdirs'. Use '-m' (optional)."
fi

if [[ ! -d "$output_dir" ]]; then
  mkdir -p "$output_dir" || error_exit "Failed to create output directory: $output_dir"
fi

# Get the downloaded file path
downloaded_file_name="$(basename "$download_url")"
downloaded_file="$output_dir/$downloaded_file_name"
plain_name="${downloaded_file_name%%-*}"

# Check file extension and extract accordingly
file_extension="${downloaded_file_name##*.}"

case "$file_extension" in
  "dmg")
    archive_name="$plain_name.framework"
    output_name="$output_dir/$archive_name"
    if [ -d "$output_name" ]; then
      exit 1
    fi
    ;;
esac

# Download the file using curl
echo "Downloading $plain_name from: $download_url"
curl -L -o "$downloaded_file" "$download_url" || error_exit "Download failed: $download_url"
      
echo "Successfully downloaded $plain_name"

echo "Extracting $plain_name from: $downloaded_file"

case "$file_extension" in
  "zip")
    # Check if PowerShell can be executed (more reliable than just checking if it exists)
    if ! powershell -ExecutionPolicy Bypass -Command Exit &>/dev/null; then
      echo "Warning: PowerShell execution failed. Skipping extraction."
    else
      powershell -ExecutionPolicy Bypass -Command Expand-Archive -Path "$downloaded_file" -DestinationPath "$output_dir" -Force || error_exit "Failed to extract $downloaded_file using Expand-Archive"
      extracted_folder=$(powershell -ExecutionPolicy Bypass -Command "Get-ChildItem -Path '$output_dir' -Directory | Select-Object -ExpandProperty Name")
      extracted_folder_path="$output_dir/$extracted_folder"
    fi
    ;;
  "dmg")
    # Check for hdiutil
    if ! command -v hdiutil >/dev/null 2>&1; then
      echo "Warning: hdiutil not found. Skipping extraction."
    else
      hdiutil attach "$downloaded_file" -quiet || error_exit "Failed to mount $downloaded_file"
      cp -r "/Volumes/$plain_name/$archive_name" "$output_dir/$archive_name"
      hdiutil detach "/Volumes/$plain_name" -quiet || error_exit "Failed to unmount $downloaded_file"
    fi
    ;;
  "tar.gz" | "tgz")
    # Use tar for .tar.gz and .tgz
    tar -xf "$downloaded_file" -C "$output_dir" || error_exit "Failed to extract $downloaded_file using tar"
    ;;
  *)
    echo "Warning: Unsupported file extension: $file_extension. Skipping extraction."
    ;;
esac

rm -rf $downloaded_file

# Move subdirectories out and remove empty parent directory
move_extracted_subdirs() {
  target_dir="$1"
  for subdir in "$target_dir/"*; do
    if [[ -d "$subdir" ]]; then
      mv "$subdir" "$subdir/../.."  # Move subdir to its parent directory
    fi
  done
  echo "$1"
  rmdir "$1"
}

# Run move script if move_subdirs is '-m' (optional)
if [[ "$move_subdirs" == "-m" ]]; then
  echo "$extracted_folder_path"
  move_extracted_subdirs "$extracted_folder_path" || error_exit "Failed to move subdirectories"
fi
            
echo "Successfully extracted $plain_name"
