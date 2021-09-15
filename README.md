# Protegon

2D game engine built on top of SDL2, SDL2_ttf, SDL2_mixer, and SDL2_image. Requires at least C++17.

# Usage in CMake

Clone this repository into a directory.

Add the following lines in the ```CMakeLists.txt``` file after creating a target:

```cmake
add_subdirectory(<protegon_repository_directory> binary_dir)
add_protegon_to(<target_name>)
```
- ```<protegon_repository_directory>``` directory where this repository resides (cloned or submodule).
- ```<target_name>``` name of your desired cmake target.
