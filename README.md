# protegon

**protegon** is a fast, modular **2D game engine** written in C++23, using **OpenGL** for rendering and **SDL2** for input, audio, and font support. Designed for flexibility and performance, it gives you full control over your game while providing helpful abstractions to get you started quickly.

---

## 🚀 Features at a Glance

- 🔺 **Custom Batch Renderer** — Efficient rendering with minimal draw calls.
- ✨ **Shader Support** — Easily integrate and manage GLSL shaders.
- 🎬 **Scene & Camera System** — Organize your game logic into manageable scenes with camera control.
- 🧱 **Entity Component System** — Lightweight ECS for scalable gameplay architecture.
- 🔊 **Audio Support** — Music and sound playback using SDL_mixer.
- 🖱️ **Input Handling** — Mouse and keyboard input built in.
- 🧮 **Math Library** — Vectors, matrices, quaternions, and more.
- 🧲 **Collision Detection** — Built-in physics utilities for 2D collisions.
- 🎛️ **UI Elements** — Basic UI widgets like buttons.
- 🕒 **Timers & Tweens** — Time-based animations and smooth transitions.
- 🎲 **Randomness & Noise** — Fractal noise and RNG utilities.
- 📦 **Resource Managers** — Manage textures, fonts, shaders, and more.
- 🧪 **Debug Tools** — Function profiling, performance logging, and development helpers.

---

## 🛠️ Getting Started

### Prerequisites

- **C++23 or newer**
- **[CMake 3.20+](https://cmake.org/download/)**
- A C++ compiler + build system (Visual Studio, Ninja, Make, etc.)

---

## 📦 Adding protegon to your CMake project

Clone or add protegon as a submodule:

```bash
git clone https://github.com/yourusername/protegon.git  
# or  
git submodule add https://github.com/yourusername/protegon.git  
```

Then, in your CMakeLists.txt:

```cmake
add_subdirectory(<repository_directory> binary_dir)  
add_protegon_to(<target_name>)  

# (optional) create symlink to resources folder  
create_resource_symlink(<target_name> <source_parent_dir> <destination_parent_dir> <directory_name>)  
```

---

## 🧱 Building protegon

1. Clone the repository:  
    `git clone https://github.com/yourusername/protegon.git && cd protegon`

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

> 💡 *Tip: On Linux, you may need to install `Homebrew` or development packages for SDL2 and OpenGL.*

## 🌐 Web (Emscripten + WebGL)

### Requirements

- [Emscripten SDK](https://emscripten.org/)
- [Ninja](https://ninja-build.org/) or [MinGW](https://www.mingw-w64.org/)

Verify setup:  
```bash
emcc --version  
ninja --version  # or gcc --version  
```

```bash
emcmake cmake -S . -B build-web -DPROTEGON_BUILD_EXAMPLES=ON -DPROTEGON_EXAMPLES="audio/test_audio;audio/test_visuals"
emcmake cmake -S . -B build-web -DPROTEGON_BUILD_EXAMPLES=ON -DPROTEGON_EXAMPLES=ALL
cmake --build build-web
python3 -m http.server 8000 --directory build-web/dist
```

### Build Scripts (Run from `scripts/`)

| Script | Description |
|--------|-------------|
| `./build-emscripten.sh` | Builds WebGL version (HTML, WASM) |
| `./run-emscripten.sh` | Serves locally via `emrun` |
| `./build-run-emscripten.sh` | Combines build and run |
| `./zip-for-itch.sh` | Creates `.zip` for Itch.io uploads |
| `./build-itch.sh` | Builds and zips for Itch.io |

## ❗ Troubleshooting

### Windows: Symlink Error

If you see:
- If you get the error `A required privilege is not held by the client` when creating a symlink using `create_resource_symlink` on Windows, [turn on Developer mode](https://learn.microsoft.com/en-us/windows/apps/get-started/enable-your-device-for-development).


## 📚 License

[MIT License](LICENSE)