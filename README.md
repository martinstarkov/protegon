# Protegon

2D game engine built on top of SDL2, SDL2_ttf, and SDL2_image.

# Usage in CMake

Add the following lines in your ```CMakeLists.txt``` after creating your target:

```cmake
add_subdirectory(<protegon_repository_directory> binary_dir)
add_protegon_to(<target_name>)
```
- ```<protegon_repository_directory>``` corresponds to the directory where this repository resides (cloned or submodule).
- ```<target_name>``` corresponds to the name of your desired cmake target.

That's it!

Enjoy the features the engine has to offer!


PS. Might add feature documentation here at some point in the future.