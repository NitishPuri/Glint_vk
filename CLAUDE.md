# CLAUDE.md

Glint (Vulkan) is a personal C++20 Vulkan renderer and learning playground: a small engine library (`glint_core`) plus a sample browser that switches between rendering samples at runtime. It is the Vulkan counterpart of `../Glint_gl`. The roadmap lives in `README.md`, and `arch.md` sketches the builder-style APIs the project is heading toward.

## Build & run

The repo lives on a shared NTFS drive (`/mnt/e`) and is built from both Windows (MSVC / Visual Studio) and Linux (GCC + Ninja). Keep both working. Source files are committed with **CRLF** line endings, so preserve CRLF when you edit existing files. Shell scripts (`*.sh`) must stay LF.

**Linux**
```bash
./build.sh [Debug|Release] [extra cmake args]   # configures build/<Type> with Ninja, then builds
./run.sh   [Debug|Release] [app args]           # build + run glint_samples
APP=cube ./run.sh                               # run a minimal app instead (triangle | cube)
```
Binaries go to `build/<Type>/bin/`, and compiled SPIR-V goes to `build/<Type>/bin/shaders/`. `compile_commands.json` is exported into the build dir.

System packages (Debian/Ubuntu): `libvulkan-dev libglfw3-dev glslang-tools vulkan-validationlayers`, plus `libglm-dev` (optional: if it's missing, CMake fetches GLM 1.0.1 via FetchContent). Shader compilation uses `glslc` if found, otherwise `glslangValidator -V`. You can point CMake at a specific binary with `-DGLSLC=...` or `-DGLSLANG_VALIDATOR=...`.

**Windows**: `configure.bat` → `build.bat` → `run.bat` (VS generator into `build/`, output in `build/bin/Debug/`). GLFW comes from the prebuilt `ext/glfw-3.4.bin.WIN64` (gitignored, so it must be present locally). GLM and glslc come from the Vulkan SDK (`VULKAN_SDK`).

There are no tests yet.

## Runtime config (`core/config.*`)

- CLI: `-l/-L` enables/disables logging, `-v/-V` enables/disables validation layers, `-s <shader dir>`, `-r <resource dir>`. Any other `--name value` or bare flag goes into a generic option map, which you query with `Config::isOptionSet("name")`. For example, `--enable_command_buffer_caching` (see below).
- Env: `GLINT_ENABLE_LOGGING`, `GLINT_ENABLE_VALIDATION`, `GLINT_SHADER_PATH`, `GLINT_RESOURCE_PATH`.
- Debug builds (`_DEBUG`, which CMake also defines for non-MSVC) turn logging and validation on by default. **If `VK_LAYER_KHRONOS_validation` isn't installed, instance creation throws.** Install the layers or pass `-V`.
- Default paths are baked in at compile time: `SHADER_DIR` = `<build dir>/bin/shaders` and `BASE_DIR` = the source root (resources come from `res/`).
- The logger writes an indented call trace (`LOGFN`, `LOG(...)`) to stdout and to `log.txt` in the CWD. `log.txt` is tracked in git, so don't commit the churn from running the app.

## Architecture

```
ext/                 third-party: imgui (submodule), stb, tinyobjloader, tinygltf, ktx (C sources, built static),
                     arcball_camera; ext/CMakeLists.txt defines INTERFACE/STATIC targets for each (+ glfw, glm_headers)
src/shaders/         GLSL → SPIR-V; the list of shaders to compile is explicit in src/shaders/CMakeLists.txt
src/glint_core/      static lib, namespace glint
  core/              Window (GLFW), Config (singleton), Camera, Logger
  renderer/          VkContext (instance/device/surface), SwapChain, RenderPass, Pipeline(+PipelineConfig),
                     CommandManager, SynchronizationManager, Descriptor*, Mesh/MeshFactory, Texture, Vertex,
                     vk_utils/vk_tools/initializers (helpers, partly borrowed from Sascha Willems' samples)
  vks/               WIP port of Sascha Willems' glTF loader. vk_gltf_model.cpp is excluded from the build (doesn't compile)
src/glint_ui/        ImGuiManager (imgui GLFW + Vulkan backends)
src/samples/         glint_samples executable: the sample browser
src/minimal/         triangle, cube: standalone minimal apps (add via add_glint_app() in its CMakeLists)
res/                 textures/models (viking_room.obj etc.)
```
Files with a trailing underscore (`input_.h`, `material_.h`, `shader_.h`, `scene_graph_.h`) are unbuilt design sketches, and `*_.h` is gitignored.

**Frame flow.** `Renderer::drawFrame(fn)` acquires a swapchain image and calls `fn(VkCommandBuffer, imageIndex)` to record commands. The sample browser passes `SampleManager::render`, which renders the active sample and then ImGui. Command buffers and in-flight fences map one-to-one with swapchain images, not with frames in flight. With `--enable_command_buffer_caching`, buffers are recorded once and reused until `markCommandBuffersDirty()`. That breaks ImGui (it needs re-recording every frame), and the known fix is to give ImGui its own per-frame pass.

**Adding a sample.** Subclass `glint::Sample` (`initSample`/`update`/`render`/`cleanup`, with an optional camera via `initCamera`), put `REGISTER_SAMPLE(MySample);` in the .cpp (static-init self-registration), and add the files to `GLINT_SAMPLE_SOURCES` in `src/samples/CMakeLists.txt`. Add any new shaders to the list in `src/shaders/CMakeLists.txt`. Shaders load at runtime as `<shader dir>/<name>.spv`. `specialization_constants.*` is currently commented out of the build.

GLM is used with `GLM_FORCE_RADIANS` and `GLM_FORCE_DEPTH_ZERO_TO_ONE`. Style: `.clang-format` (Google-based, 2-space indent, 120 columns). Members use `m_PascalCase`.

## Portability notes

- GCC is stricter than MSVC. For example, a nested struct with default member initializers can't be used as a default argument inside its enclosing class (GCC bug 88165), which is why `Window` has a separate `Window()` overload.
- GCC 11 (Ubuntu 22.04) has no `<format>`. Avoid `std::format`, or add a fallback the way `../Glint_gl` does.
