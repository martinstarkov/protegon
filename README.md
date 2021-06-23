# Protegon

2D game engine built on top of SDL2, SDL2_ttf, and SDL2_image. Requires at least C++17.

# Usage in CMake

Add the following lines in your ```CMakeLists.txt``` after creating your target:

```cmake
add_subdirectory(<protegon_repository_directory> binary_dir)
add_protegon_to(<target_name>)
```
- ```<protegon_repository_directory>``` directory where this repository resides (cloned or submodule).
- ```<target_name>``` name of your desired cmake target.

That's it!


PS. Might add feature documentation here at some point in the future.