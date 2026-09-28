# Building

## Linux (tested on Pop!_OS 22.04, GCC 11.4, CMake 3.22, Ninja)

```bash
./build.sh [Debug|Release] [extra cmake args]   # configure build/<Type> with Ninja, then build
./run.sh   [Debug|Release] [app args]           # build + run glint_samples
APP=cube ./run.sh                               # run a minimal app instead (triangle | cube)
```

Outputs: `build/<Type>/bin/{glint_samples,triangle,cube}` and `build/<Type>/bin/shaders/*.spv`.
`compile_commands.json` is exported into the build dir.

Packages (Debian/Ubuntu):

| Package | Why | Required |
|---|---|---|
| `libvulkan-dev` | loader + headers | yes |
| `libglfw3-dev` | GLFW ≥ 3.3 (`find_package(glfw3)`) | yes |
| `glslang-tools` | `glslangValidator`, used when `glslc` isn't found | yes, unless the LunarG SDK is set up |
| `vulkan-validationlayers` | `VK_LAYER_KHRONOS_validation` (on by default in Debug) | no; the app warns and runs without |
| `libglm-dev` | GLM | no; FetchContent downloads GLM 1.0.1 if it's missing |

Ubuntu 22.04's validation layers are 1.3.204. For newer layers and `glslc`, use the LunarG SDK tarball
and `source <sdk>/setup-env.sh` before configuring (the same setup `glint` uses).

A specific compiler binary can be passed with `-DGLSLC=/path/to/glslc` or
`-DGLSLANG_VALIDATOR=/path/to/glslangValidator`.

## Windows (MSVC, Visual Studio generator)

```bat
configure.bat    :: cmake -Bbuild
build.bat        :: cmake --build build --config Debug
run.bat [args]   :: build\bin\Debug\glint_samples.exe
```

Needs the Vulkan SDK (`VULKAN_SDK` set; provides glslc and GLM) and the prebuilt GLFW in
`ext/glfw-3.4.bin.WIN64` (gitignored, so it has to be present locally).

## What the Linux port changed (2026-09-29)

| File | Change |
|---|---|
| `CMakeLists.txt` | minimum 3.25 → 3.20; default build type Debug; export compile commands; define `_DEBUG` in Debug for non-MSVC |
| `ext/CMakeLists.txt` | GLFW: prebuilt on `WIN32`, `find_package(glfw3)` elsewhere. New `glm_headers` target: empty on Windows (SDK include), system `glm::glm` or FetchContent elsewhere |
| `src/shaders/CMakeLists.txt` | fall back to `glslangValidator -V` when `glslc` isn't found; also look in `$VULKAN_SDK/bin` |
| `src/glint_core/CMakeLists.txt` | link `glm_headers`; new `SHADER_DIR` define (`<build>/bin/shaders`) so a per-config build dir works; `vks/vk_gltf_model.cpp` commented out (doesn't compile on any platform) |
| `src/glint_core/core/config.cpp` | default shader path uses `SHADER_DIR` |
| `src/glint_core/core/window.h` | `Window()` overload instead of `= WindowProps()` default argument (GCC bug 88165) |
| `src/glint_core/renderer/vk_context.*` | missing validation layers → warning instead of exception; `cleanup()` skips null handles (it used to abort in the loader when init failed part way) |
| `build.sh`, `run.sh` | new |

Line endings: source files are committed with **CRLF**. Keep them that way when editing. `*.sh` are LF.

## Verified

- Builds with GCC 11.4, no errors.
- `glint_samples` runs on AMD Radeon (RADV RENOIR, Mesa 25.1), and picks it over llvmpipe because it comes
  first in the device list, not because of any scoring.
- Validation was not yet run clean on Linux (the layers weren't installed at the time of writing).
