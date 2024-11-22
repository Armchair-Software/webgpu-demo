# WebGPU Emscripten demo for the Armchair Engine

This is a simple proof of concept, a minimal 3D engine written in C++, compiled to WASM with Emscripten.  Running in the browser, rendering with WebGPU.

[![image](https://github.com/user-attachments/assets/c3aab0f2-d9b3-41cb-b333-b37f74710215)](https://armchair-software.github.io/webgpu-demo/)

For the next demo (adding GUI with [dear imgui](https://github.com/ocornut/imgui) and gamepad support), see https://github.com/Armchair-Software/webgpu-demo2.

## Live demo
Live demo: https://armchair-software.github.io/webgpu-demo/

This requires Firefox Nightly, or a recent version of Chrome or Chromium, with webgpu and Vulkan support explicitly enabled.

## Dependencies
- [Emscripten](https://emscripten.org/)
- CMake
- [VectorStorm](https://github.com/Armchair-Software/vectorstorm) (included)
- [LogStorm](https://github.com/VoxelStorm-Ltd/logstorm) (included)
- [magic_enum](https://github.com/Neargye/magic_enum) (included)

## Building
The easiest way to assemble everything (including in-tree shader resource assembly) is to use the included build script:
```sh
./build.sh
```

To launch a local server and bring up a browser:
```sh
./run.sh
```

For manual builds with CMake, and to adjust how the example is run locally, inspect the `build.sh` and `run.sh` scripts.
