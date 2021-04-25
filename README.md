# protegon
2D game engine built on top of SDL2.

# Usage in CMake

Add the following lines in your ```CMakeLists.txt``` after creating your target:

```cmake
add_subdirectory(<protegon_repository_directory>)
add_protegon_to(<target_name>)
```
- ```<protegon_repository_directory>``` corresponds to the directory where this repository resides (cloned or submodule).
- ```<target_name>``` corresponds to the name of your desired cmake target.

That's it!

Enjoy the features the engine has to offer!


PS. Might add feature documentation at some point in the future.