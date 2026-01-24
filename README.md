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
- **[CMake 3.28+](https://cmake.org/download/)**
- A C++ compiler + build system (Visual Studio, Ninja, Make, etc.)

---

## 📦 Adding protegon to your CMake project

Clone or add protegon as a submodule:

```bash
git clone https://github.com/martinstarkov/protegon.git  
# or  
git submodule add https://github.com/martinstarkov/protegon.git  
```

Then, protegon can be used as a CMake subproject via `add_subdirectory()`.

```cmake
add_subdirectory(<path/to/protegon_repo_root> binary_dir)

# Link Protegon + configure assets (optional)
add_protegon_to(<target> ASSETS_DIR "<path/to/your/assets_dir>")

# Create asset directory symlink (optional)
if (NOT EMSCRIPTEN AND EXISTS "<path/to/your/assets_dir>")
  create_symlink(<target> "<path/to/your/assets_dir>" "$<TARGET_FILE_DIR:<target>>")
endif()
```

### Web build scripts (Emscripten)

Helper scripts for external projects are available under:

```
<protegon>/scripts/external/
```

* `build_web.sh` — configure + build the web version
* `run_web.sh` — run/serve the output using `emrun`
* `zip_web.sh` — package the built output for upload (e.g. itch.io)
* `build_run_web.sh` — build then run
* `build_zip_web.sh` — build then zip

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

## ❗ Troubleshooting

### Windows: Symlink Error

If you see:
- If you get the error `A required privilege is not held by the client` when creating a symlink using `create_resource_symlink` on Windows, [turn on Developer mode](https://learn.microsoft.com/en-us/windows/apps/get-started/enable-your-device-for-development).


## 📚 License

[MIT License](LICENSE)