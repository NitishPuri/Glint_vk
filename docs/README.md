# Glint_vk — Documentation

Architecture notes for Glint_vk, a small Vulkan learning renderer built around a sample browser.
Written 2026-09-29 while porting to Linux and preparing the migration into
[`glint`](../../glint) (OpenGL + Vulkan side by side). The OpenGL counterpart is documented in
[`Glint_gl/docs`](../../Glint_gl/docs/README.md).

| Doc | What's in it |
|---|---|
| [architecture.md](architecture.md) | Big picture: layers, startup, frame loop, frames-in-flight vs swapchain images, command buffer caching, sample lifecycle, ownership, resource uploads, paths, logging |
| [modules.md](modules.md) | Reference for each class and file in `src/glint_core`, `src/glint_ui` and `src/samples`, with a **Port:** target in glint |
| [samples.md](samples.md) | The 6 registered samples, 2 minimal apps, and the unbuilt ones: technique, resources, descriptors, shaders, what's worth porting |
| [build.md](build.md) | Building on Linux and Windows, dependencies, what the Linux port changed |
| [known-issues.md](known-issues.md) | Bugs and tech debt found during review, with file:line references |

## One-paragraph summary

`samples/sample_main.cpp` creates a GLFW window (no GL context), a `Renderer`, an `ImGuiManager` and the
`SampleManager` singleton. The `Renderer` owns the whole Vulkan backbone: `VkContext` (instance, device,
queues), `SwapChain` (plus one shared MSAA colour image and depth image), a single `VkRenderPass`
(MSAA colour + depth + resolve-to-swapchain), **one** `Pipeline` at a time, a `CommandManager` and a
`SynchronizationManager`. Each technique is a `Sample` subclass that registers itself through a static
initialiser (`REGISTER_SAMPLE`). Choosing one in the ImGui combo **constructs a new instance** and calls
`initSample()`, which builds its descriptors, uniform buffers, mesh and texture, and asks the renderer to
replace the global pipeline. Every frame, `update(dt)` writes that frame's uniform buffer, then
`Renderer::drawFrame` records a command buffer through a callback: the `SampleManager` begins the render
pass, the sample binds and draws, ImGui draws on top, and the pass ends. Command buffers are re-recorded
every frame unless `--enable_command_buffer_caching` is passed, which reuses them per swapchain image
(and breaks ImGui).

## How this differs from Glint_gl (at a glance)

| | Glint_gl | Glint_vk |
|---|---|---|
| Unit of technique | `SceneBase` (header-only, 8 scenes) | `Sample` (.h/.cpp, 6 registered) |
| Registration | explicit list in `app.cpp` | static-init self-registration, `REGISTER_SAMPLE(T)` |
| Engine layer | thin GL object wrappers, scenes call GL directly | `glint_core` library wraps most Vulkan objects; samples use the wrappers |
| Pipeline state | global GL state machine | one `Pipeline` owned by `Renderer`, replaced on sample switch |
| Shaders | GLSL loaded from source tree at runtime | GLSL compiled to SPIR-V at build time, loaded from the build dir |
| Camera | `CameraController` (arcball) | `Camera` (same `arcball_camera.h`), Y flipped in the projection matrix |
| Techniques covered | quad → shadow mapping (opengl-tutorial) | triangle, quad, textured, cube, dynamic UBO (vulkan-tutorial + Sascha Willems) |
