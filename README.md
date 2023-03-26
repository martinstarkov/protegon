# protegon

2D game engine built on top of SDL2, SDL2_ttf, SDL2_mixer, and SDL2_image. Requires C++17 or newer.

# Build Instructions

1. Clone the repository to your local machine.
2. Navigate into the cloned repository in a terminal such as Git Bash.
2. `mkdir build` to create a build directory.
3. `cd build` to enter the build directory.
4. `cmake .. -DDEMOS=ON` to build protegon with demos using CMake.
5. Open the project files in the build directory.

# Adding to CMake project

Clone this repository into a directory.

Add the following lines in your ```CMakeLists.txt``` file after creating the target:

```cmake
add_subdirectory(<repository_directory> binary_dir)
add_protegon_to(<target_name>)
```
- ```<repository_directory>``` directory where this repository resides (cloned or submodule).
- ```<target_name>``` name of your desired cmake target.
