# protegon

2D game engine built on top of SDL2, SDL2_ttf, SDL2_mixer, and SDL2_image. Requires C++17 or newer.

# Adding to CMake project

Clone this repository into a directory.

Add the following lines in your ```CMakeLists.txt``` file after creating the target:

```cmake
add_subdirectory(<repository_directory> binary_dir)
add_protegon_to(<target_name>)
```
- ```<repository_directory>``` directory where this repository resides (cloned or submodule).
- ```<target_name>``` name of your desired cmake target.
