# protegon

protegon is a fast, modular 2D game engine written in C++23, using OpenGL for rendering and SDL3 for input, audio, and font support. Designed for flexibility and performance, it gives you full control over your game while providing helpful abstractions to get you started quickly.

---

## Features at a Glance

- **Custom Batch Renderer**: Efficient rendering with minimal draw calls.
- **Shader Support**: Easily integrate and utilize GLSL shaders.
- **Scene & Camera System**: Organize your game logic into manageable scenes with camera control.
- **Entity Component System**: Lightweight ECS for scalable data handling.
- **Audio Support**: Audio playback using SDL_mixer.
- **Input Handling**: Mouse and keyboard input built in.
- **Math Library**: Vectors, matrices, quaternions, and more.
- **Collision Detection**: Built-in physics utilities for 2D collisions.
- **UI Elements**: Basic UI widgets like buttons, dropdowns, dialogue boxes.
- **Timers & Tweens**: Animations and pre-built tween effects.
- **Randomness & Noise**: Fractal noise and RNG utilities.
- **Debug Tools**: Function profiling and performance logging.

---

## Getting Started

### Prerequisites

- C++23 or newer
- [CMake 3.28+](https://cmake.org/download/)
- A C++ compiler + build system (Visual Studio, Ninja, Make, etc.)

---

## Adding protegon to your CMake project

Clone or add protegon as a submodule:

```bash
git clone https://github.com/martinstarkov/protegon.git  
# or  
git submodule add https://github.com/martinstarkov/protegon.git  
```

Then, in your CMakeLists.txt:

```cmake
add_subdirectory(<repository_directory> binary_dir)  
add_protegon_to(<target_name>)  

# (optional) create symlink to asset folder  
create_symlink(<target_name> <source_parent_dir> <destination_parent_dir>) 
```

---

## Building protegon

1. Clone the repository:  
    `git clone https://github.com/martinstarkov/protegon.git && cd protegon`

2. Create and enter a build directory:  
```bash
   mkdir build  
   cd build  
```

### Visual Studio

```bash
cmake .. -G "Visual Studio 17 2022"  
```

Open the generated `.sln`, set your project as the startup project, then **Build & Run**.

### Ninja

```bash
cmake .. -G Ninja  
ninja  
./your_project_name.exe  
```

### macOS

```bash
cmake .. -G Xcode  
make  
./your_project_name.exe  
```

### Linux

```bash
cmake .. -G "Unix Makefiles"  
make  
./your_project_name.exe  
```

> *On Linux, you may need to install `Homebrew` or development packages for SDL3.*

## Web (Emscripten + WebGL)

### Requirements

- [Emscripten SDK](https://emscripten.org/)
- [Ninja](https://ninja-build.org/) or [MinGW](https://www.mingw-w64.org/)

Verify setup:  
```bash
emcc --version  
ninja --version  # or gcc --version  
```

## Build Scripts (run from `scripts/`)

### Web (Engine-Level)

> Run `./build_web_dependencies.sh` at least once before building web examples.

- `./build_web_dependencies.sh` – Build required web dependencies  
- `./build_web_examples.sh` – Build all web examples (HTML + WASM)  
- `./run_web_examples.sh` – Serve examples locally  
- `./build_run_web_examples.sh` – Build and run examples  

---

### Web (Specific Project)

Located in `scripts/project/`:

- `./build_web.sh` – Build the project (HTML + WASM)  
- `./run_web.sh` – Serve the project locally  
- `./build_run_web.sh` – Build and run the project  
- `./zip_web.sh` – Create distributable `.zip`  
- `./build_zip_web.sh` – Build and zip in one step  

---

### SDL Setup (Native Builds)

> Required only if the correct SDL development binaries are not already installed.

- `./download_sdl_windows_msvc.sh` – Windows (MSVC) SDL dev binaries  
- `./download_sdl_macos_brew.sh` – macOS (Homebrew install)  
- `./download_sdl_macos_dmg.sh` – macOS (manual DMG download)  

## Troubleshooting

### Windows: Symlink Error

If you see:
- If you get the error `A required privilege is not held by the client` when creating a symlink using `create_symlink` on Windows, [turn on Developer mode](https://learn.microsoft.com/en-us/windows/apps/get-started/enable-your-device-for-development).


## License

[MIT License](LICENSE)