# protegon

2D game engine using SDL2, SDL2_ttf, SDL2_mixer, and SDL2_image.

# Prerequisites

This project requires C++17 or newer and [CMake 3.20+](https://cmake.org/download/).
You will also need a build system and a C++ compiler (see CMake Build Instructions).

# Adding to a CMake project

1. [Clone the repository](https://docs.github.com/en/repositories/creating-and-managing-repositories/cloning-a-repository) to a directory on your local machine (referred to as `<repository_directory>`). Alternatively, you can add it as a git submodule.

2. Add the following lines in your `CMakeLists.txt` file after creating your target (referred to as `<target_name>`):

```cmake
add_subdirectory(<repository_directory> binary_dir)
add_protegon_to(<target_name>)
create_resource_symlink(<target_name> "resources") // (optional for creating a symlink for the resources folder)
```

# CMake Build Instructions

1. `cd <replace_with_repository_directory>` to navigate into the cloned repository.
2. `mkdir build` to create a build directory.
3. `cd build` to enter the created build directory.
4. Depending on your build system, follow the appropriate next steps:

# Microsoft Visual Studio

5. `cmake .. -G "Visual Studio 17 2022"` to generate build files.
6. Open the generated `<your_project_name>.sln` file.
7. Set `<your_project_name>` as the startup project.
8. Build and run.

# Ninja

5. `cmake .. -G Ninja` to generate build files.
6. `ninja` to build the project.
7. `./your_project_name.exe` to run your executable.

# macOS

5. `/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"` (Installs [Homebrew](https://brew.sh/) for Linux).
6. (required if brew not in PATH) `(echo; echo 'eval "$(/home/linuxbrew/.linuxbrew/bin/brew shellenv)"') >> /home/<your_username>/.bashrc`
7. (required if brew not in PATH) `eval "$(/home/linuxbrew/.linuxbrew/bin/brew shellenv)"`
8. `cmake .. -G "Unix Makefiles"` to generate build files.
9. `make` to build the project.
10. `./your_project_name.exe` to run your executable.

# Linux

8. `cmake .. -G XCode` to generate build files.
9. `make` to build the project.
10. `./your_project_name.exe` to run your executable.
