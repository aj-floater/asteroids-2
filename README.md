# Starshard 05

Starshard 05 is a small C++/Vulkan retro arcade space shooter prototype focused on:

- wireframe ship movement
- rotational aiming
- forward thrust with inertia
- screen-edge wraparound
- escalating asteroid-field survival

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
./build/starshard-05
```

Controls:

- `Left` / `Right`: rotate
- `Up`: thrust
- `Esc`: quit

## Install

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
cmake --install build --prefix /tmp/starshard-install
```

This installs the binary, compiled shaders, desktop file, metainfo, icon, and license payload using standard Linux directories.

## Flatpak

The repo includes a Flathub-oriented manifest at [io.github.aj_floater.starshard_05.yml](/home/arjames/Coding/asteroids-2/io.github.aj_floater.starshard_05.yml).

It assumes the upstream GitHub repo has been renamed to `starshard-05` and tagged `v0.1.0` before submission.

For local development from the current checkout, use [io.github.aj_floater.starshard_05.local.yml](/home/arjames/Coding/asteroids-2/io.github.aj_floater.starshard_05.local.yml) instead.
