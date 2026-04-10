# Asteroids Prototype

This directory contains a small C++/Vulkan prototype for a faithful Asteroids-style ship slice:

- wireframe ship
- rotate left/right
- forward thrust with inertia
- screen-edge wraparound

## Dependencies

You need:

- a C++20 compiler
- CMake 3.20+
- Vulkan SDK or system Vulkan development packages
- GLFW 3 development package
- `glslc` or `glslangValidator` for shader compilation

Examples:

- Fedora: `sudo dnf install gcc-c++ cmake vulkan-loader-devel vulkan-headers glfw-devel shaderc`
- Ubuntu: `sudo apt install g++ cmake libvulkan-dev libglfw3-dev glslang-tools`

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/asteroids
```

Controls:

- `Left` / `Right`: rotate
- `Up`: thrust
- `Esc`: quit
