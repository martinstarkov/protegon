# protegon

protegon is a C++23 2D game engine with an optional ImGui editor. It uses OpenGL for desktop rendering, WebGL2 for web builds, and GLFW for windowing and input.

## Features

- **Renderer**: Batched 2D rendering, render targets, custom shaders, cameras, layers, blending, and screen effects.
- **Editor**: Dockable ImGui editor with scene management, hierarchy, inspector, viewport, content browser, settings, console, undo/redo, and play mode.
- **Scenes**: Multiple active scenes, scene transitions, registered custom scene types, and project scene files.
- **ECS**: Entity/component system with component registration and editor reflection.
- **Assets**: Textures, fonts, audio, shaders, prefabs, and project asset management.
- **Audio**: Audio playback through miniaudio.
- **Input**: Keyboard and mouse input through GLFW.
- **Physics & Collision**: 2D rigid bodies, colliders, collision handling, and scene bounds.
- **Scripting**: Built-in scripts, sequences, triggers, actions, timers, tweens, and animation utilities.
- **UI**: Buttons, toggle groups, dropdowns, dialogue, and interaction helpers.
- **Math**: Vectors, matrices, geometry, transforms, interpolation, RNG, and noise.
- **Platforms**: Windows, Linux, macOS, and WebAssembly/Emscripten.

## Requirements

- C++23 compiler
- [CMake 3.28+](https://cmake.org/download/)
- A supported build system such as Ninja, Make, or Visual Studio
- OpenGL 3.3 capable GPU for desktop builds

For web builds:

- [Emscripten SDK](https://emscripten.org/)
- Ninja

## Adding protegon to a CMake project

Clone the repository directly or add it as a submodule:

```bash
git clone https://github.com/martinstarkov/protegon.git

# or

git submodule add https://github.com/martinstarkov/protegon.git
```

Then add protegon to your `CMakeLists.txt`:

```cmake
add_subdirectory(<repository_directory> binary_dir)

add_protegon_to(<target_name>)
```

If needed, assets can be linked into the runtime directory with:

```cmake
create_symlink(
    <target_name>
    <source_parent_dir>
    <destination_parent_dir>
)
```

## Building

A typical native build using Ninja:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Or with Unix Makefiles:

```bash
cmake -S . -B build -G "Unix Makefiles"
cmake --build build
```

Visual Studio:

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
```

The editor is controlled by the CMake option:

```cmake
-DPTGN_EDITOR=ON
```

Examples can be selected with:

```cmake
-DPTGN_EXAMPLES=ALL
```

## Editor

protegon includes an optional editor built with ImGui.

The editor provides:

- Scene creation, deletion, duplication, renaming, and ordering
- Scene hierarchy and entity selection
- Component and script inspection
- Scene and project settings
- Content browser and asset importing
- Game viewport and editor camera
- Play, pause, and stop controls
- Undo/redo history
- Console and debug tools
- Desktop and web export

Editor scenes are stored as part of a protegon project. Changes can also be made in web builds, but browser-side project changes are not written permanently back to the source project files.

## Web Builds

Activate the Emscripten environment first:

```bash
source ~/emsdk/emsdk_env.sh
```

Verify it is available:

```bash
emcc --version
emcmake --version
```

### Engine examples

From `scripts/`:

```bash
./build_web_dependencies.sh
./build_web_examples.sh
./run_web_examples.sh
```

Or build and run together:

```bash
./build_run_web_examples.sh
```

`build_web_dependencies.sh` only needs to be rerun when the web dependencies change.

Projects can be exported for the web directly from the editor using the Export window.

## Editor Project

A protegon editor project uses a `.ptgnproj` manifest and typically contains an asset directory with folders such as:

```text
assets/
├── audio/
├── data/
├── fonts/
├── prefabs/
├── scenes/
├── shaders/
└── textures/
```

Scenes are stored as `.ptgnscene` files and referenced by a unique scene key in the project manifest.

## Troubleshooting

### Windows symlink permissions

If Windows reports:

```text
A required privilege is not held by the client
```

when creating asset symlinks, enable [Developer Mode](https://learn.microsoft.com/en-us/windows/apps/get-started/enable-your-device-for-development).

### Emscripten commands not found

If `emcc` or `emcmake` is not available, activate the SDK environment:

```bash
source ~/emsdk/emsdk_env.sh
```

To load it automatically when logging in:

```bash
echo 'source "$HOME/emsdk/emsdk_env.sh" >/dev/null' >> ~/.profile
```

## License

[MIT License](LICENSE)
