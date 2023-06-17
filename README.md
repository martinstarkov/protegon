# protegon

2D game engine built on top of SDL2, SDL2_ttf, SDL2_mixer, and SDL2_image. Requires C++17 or newer.

# Build Instructions using CMake

Before starting the build, ensure that you have [CMake 3.20+](https://cmake.org/download/) and [Python 3.3+](https://www.python.org/downloads/) installed. You will also need a valid C++ compiler.

1. [Clone the repository](https://docs.github.com/en/repositories/creating-and-managing-repositories/cloning-a-repository) to your local machine.
2. Navigate into the cloned repository by typing `cd <replace_with_repository_directory>` into a terminal such as [Git Bash](https://git-scm.com/downloads).
3. `mkdir build` to create a build directory.
4. `cd build` to enter the created build directory.
5. `cmake ..` to build protegon using CMake with demos (add ` -DDEMOS=OFF` (note the repeated D) to the command to build without demos).
6. If prompted, enter `Y` to confirm the download of the `requests` python package. 
7. Enter `Y` to each of the SDL2 module download confirmation prompts. 
8. All done! Open/access the generated project files in the build directory.

# Adding to CMake project

Clone this repository into a directory.

Add the following lines in your ```CMakeLists.txt``` file after creating the target:

```cmake
add_subdirectory(<repository_directory> binary_dir)
add_protegon_to(<target_name>)
```
- ```<repository_directory>``` directory where this repository resides (cloned or submodule).
- ```<target_name>``` name of your desired cmake target.
