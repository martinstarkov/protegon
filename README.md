# protegon

2D game engine built on top of SDL2, SDL2_ttf, SDL2_mixer, and SDL2_image. Requires C++17 or newer.

# Build Instructions using CMake

# Ninja

`mkdir build`
`cd build`
`cmake .. -G Ninja`
`ninja`

# Other

Before starting the build, ensure that you have [CMake 3.20+](https://cmake.org/download/). You will also need a valid C++ compiler. Tested on Windows 10 MSVC 2022 and MacOS Xcode 14.3.1.

1. [Clone the repository](https://docs.github.com/en/repositories/creating-and-managing-repositories/cloning-a-repository) to your local machine.
2. Navigate into the cloned repository by typing `cd <replace_with_repository_directory>` into a terminal such as [Git Bash](https://git-scm.com/downloads).
3. `mkdir build` to create a build directory.
4. `cd build` to enter the created build directory.
5. `cmake ..` to build with CMake (add `-DDEMOS=OFF` to build without demos) (add `-G Xcode` when using Xcode).
6. All done! Open/access the generated project files in the build directory.

# Adding to CMake project

Clone this repository into a directory.

Add the following lines in your `CMakeLists.txt` file after creating the target:

```cmake
add_subdirectory(<repository_directory> binary_dir)
add_protegon_to(<target_name>)
```

- `<repository_directory>` directory where this repository resides (cloned or submodule).
- `<target_name>` name of your desired cmake target.
