# Module reference

One entry per file. "Port:" says where the piece goes in [`glint`](../../glint/docs/PLAN.md). Remember
glint's rule: **no RHI**, and a `vk.cpp` shows raw Vulkan calls first, with helpers extracted only after a
technique has done the raw version once. Much of `glint_core` is exactly the kind of wrapper glint
postpones, so most entries say "reference for the raw code" rather than "copy".

## `src/glint_core/core/`

### `window.h/.cpp` — `Window`
GLFW wrapper with `GLFW_NO_API`. Stores title, size and a `resized` flag, set by a framebuffer-size
callback that also calls an optional `resizeCallback`. Provides `createSurface(instance)`,
`getRequiredInstanceExtensions()`, `waitIfMinimized()` (blocks in `glfwWaitEvents` while the framebuffer is
0×0) and `isKeyPressed`. The destructor calls `glfwTerminate` unconditionally.
**Port:** `core/window` with `GraphicsApi::Vulkan`. The surface creation moves into `vk/context`
(`core/` must not include Vulkan).

### `config.h/.cpp` — `Config`
Singleton holding logging and validation flags, shader and resource paths, and a map of generic CLI options.
Parse order: defaults (with `_DEBUG` → logging and validation on) → env vars → CLI. See the top-level
`CLAUDE.md` for the flag list.
**Port:** `core/config` (paths, window size, vsync, validation on/off). Keep the generic `--option` map;
it's handy for per-technique toggles.

### `camera.h/.cpp` — `Camera`
Arcball orbit camera around `arcball_camera.h` (right-drag rotates, middle-drag pans, the wheel zooms).
Keeps eye/target/up as float arrays and the view matrix as `float[16]`. The projection is `glm::perspective`
with **`[1][1] *= -1`** (the Vulkan Y flip is baked into the camera). `processMouseMovement` ignores its
`x0,y0` arguments and tracks the previous position itself.
**Port:** `core/camera`, with the Y flip removed. Projection takes a `ClipDepth` flag, and the VK backend
flips via negative viewport height (glint PLAN Phase 2 / `gl-vs-vk.md`). Merge with Glint_gl's
`CameraController`; they wrap the same library.

### `logger.h` — `Logger`, `FunctionLogger`, `OneTimeLogger`, macros
Header-only call-trace logger (see [architecture.md](architecture.md#logging-and-error-handling)).
**Port:** don't. glint uses `glint::log::info/warn/error` on fmt. The call-trace idea (`LOGFN`) could be
kept as a debug-only scope tracer, but it's noisy per frame.

### `input_.h` (sketch, not built)
Static `Input` class declaration with GLFW callbacks and polling. There is no implementation.
**Port:** the idea goes to `core/input` (glint Phase 2).

## `src/glint_core/renderer/`

### `renderer.h/.cpp` — `Renderer`
Owns context, swapchain, render pass, pipeline, command manager and sync manager (in that init order;
the command manager must exist before the swapchain because the depth image transition uses its pool).
`drawFrame(recordFn)` is the whole frame: fence wait → acquire → record through the callback → submit →
present → resize handling. `createPipeline(config)` waits for the device to go idle and replaces the single
pipeline. Also implements optional command buffer caching.
**Port:** split between `vk/frame` (per-frame sync + command buffers) and each app's `main.cpp` (loop and
present). There's no `Renderer` class in glint. Don't carry over the single global pipeline:
techniques own their pipelines.

### `vk_context.h/.cpp` — `VkContext`
Instance (API **1.0**, GLFW extensions, plus debug utils when validation is on), debug messenger (also
chained into instance creation), surface, physical device pick (**first** device that has graphics +
present queues, swapchain extension, a non-empty format/mode list and anisotropy), logical device with
`samplerAnisotropy` + `sampleRateShading`, graphics and present queues. Also `findMemoryType`,
`getMaxUsableSampleCount`, and a borrowed command pool (`setCommandPool`) used for one-shot commands.
`--list_gpus` (with logging) prints every device.
**Port:** `vk/context`. Upgrade to 1.3 and enable `dynamicRendering` + `synchronization2`. Pick devices by
score (discrete > integrated, skip `CPU` type such as llvmpipe). Query features before enabling them.

### `swapchain.h/.cpp` — `SwapChain`
Swapchain (prefers `B8G8R8A8_SRGB` + SRGB_NONLINEAR, MAILBOX > FIFO, `minImageCount + 1`, concurrent
sharing if graphics ≠ present), image views, **one** depth image and **one** MSAA colour image (at the
max sample count), framebuffers for the render pass. `recreateSwapchain` waits idle, destroys everything
and rebuilds it, without `oldSwapchain`. Debug names are set on images.
**Port:** `vk/swapchain`. Keep recreation on `OUT_OF_DATE`/`SUBOPTIMAL`; pass `oldSwapchain`; no
framebuffers (dynamic rendering); depth and MSAA targets move to the techniques that need them.

### `render_pass.h/.cpp` — `RenderPass`
The one and only `VkRenderPass`: MSAA colour + depth + resolve, one subpass, one external dependency.
`begin(cmd, imageIndex, clearColor)` / `end(cmd)`.
**Port:** replaced by `vkCmdBeginRendering`. This file is the source for the single `NOTES.md` entry
explaining the legacy render-pass model (attachments, subpass, dependency, framebuffer), per glint's
decision log.

### `pipeline.h/.cpp` — `PipelineConfig`, `Pipeline`
Loads two SPIR-V files, builds a graphics pipeline with dynamic viewport/scissor against the global render
pass. The layout has at most one descriptor set layout and no push constants. `readFile` throws on a
missing file. Shader modules are destroyed after pipeline creation.
**Port:** raw `VkGraphicsPipelineCreateInfo` in `01_triangle/vk.cpp` first (with
`VkPipelineRenderingCreateInfo`), then a `vk/pipeline` helper. That helper should cover push constants,
blending and multiple set layouts, which `PipelineConfig` lacks. `arch.md` sketches a builder API for it.

### `descriptor.h/.cpp` — `DescriptorSetLayout(::Builder)`, `DescriptorPool`, `Descriptor`, `UniformBuffer`
- `DescriptorSetLayout::Builder` → `addBinding / addUniformBuffer / addTextureSampler` → `build()`.
- `DescriptorPool` sized from a layout's bindings × `maxSets`. (The older `(context, maxSets)` constructor
  only supports UBOs.)
- `Descriptor` allocates `count` sets from a pool and provides `updateUniformBuffer`,
  `updateTextureSampler` and `bind` (always set 0, optional dynamic offsets).
- `UniformBuffer` is a persistently mapped buffer with `update(const void*)` (memcpy of the full size) and a
  `VkDescriptorBufferInfo`.

**Port:** `02_cube/vk.cpp` writes these raw first (layout, pool, allocate, write), then `vk/descriptors`.
This builder is a reasonable model for that helper.

### `command_manager.h/.cpp` — `CommandManager`
One resettable command pool on the graphics family, plus N primary buffers (N = swapchain image count).
`beginSingleTimeCommands(i)` / `endSingleTimeCommands(i)` actually mean begin/end of buffer *i*, despite
the name. The pool is also lent to `VkContext` for `VkUtils`' one-shot commands.
**Port:** folds into `vk/frame` (per-frame pool + buffer) and `vk/upload` (a dedicated transient pool for
uploads).

### `synchronization_manager.h/.cpp` — `SynchronizationManager`
Creates the `imageAvailable` and `renderFinished` semaphores ×framesInFlight, and fences ×imageCount
(created signalled). `waitForFence` / `resetFence` use a 100 s timeout. See the
[mapping table](architecture.md#frames-in-flight-vs-swapchain-images).
**Port:** `vk/frame`. Use fences per frame, acquire semaphores per frame, and present semaphores **per
image**.

### `vk_utils.h/.cpp` — `VkUtils` (static)
Global-context helpers: `setObjectName`, `createBuffer` (one allocation per buffer),
`copyBuffer`, `createImage`, `transitionImageLayout` (3 hard-coded layout pairs, sync1),
`copyBufferToImage`, `createImageView`, and `begin/endSingleTimeCommands` (allocate from the context's
pool, submit, `vkQueueWaitIdle`, free).
**Port:** `vk/buffer`, `vk/image`, `vk/upload`, `vk/debug`, as free functions that take the context
explicitly. `transitionImage()` uses `vkCmdPipelineBarrier2` with explicit stage/access masks instead of a
lookup table.

### `vk_tools.h/.cpp`, `initializers.h` (Sascha Willems, MIT)
`VK_CHECK_RESULT`, `errorString(VkResult)`, `physicalDeviceTypeString`, and `initializers::*` (structure
builders with `sType` filled in). Most of `vk_tools.h` is commented out. On Windows it includes `<windows.h>`.
**Port:** glint's `VK_CHECK` aborts in every build type and uses `string_VkResult` from
`vk_enum_string_helper.h`. Initializers are optional; the point of glint is to see the structs.

### `texture.h/.cpp` — `Texture`
`stbi_load` (forced RGBA) → staging → `R8G8B8A8_SRGB` image with a full mip chain generated with
`vkCmdBlitImage` → view → sampler (linear, repeat, max anisotropy, `maxLod = mipLevels`). Throws if the file
can't be loaded.
**Port:** `03_textured_cube/vk.cpp` (raw), then `vk/image`. Take the format as a parameter: normal and
specular maps (Glint_gl scene 6) must be UNORM, not sRGB. Pixel loading moves to `core/assets`
(`ImageData`).

### `mesh.h/.cpp` — `Mesh`
Vertex buffer + optional index buffer (uint32) through staging. `bind` / `draw` (indexed or not).
`Mesh::loadModel(path)` loads an OBJ with tinyobjloader, deduplicates vertices with a hash map, flips V
(`1 - v`), and sets colour to white. It has **no normals**. `copyBuffer` duplicates `VkUtils::copyBuffer`.
**Port:** the OBJ loading and dedup go to `core/assets` (`MeshData`), merged with Glint_gl's
`VBOIndex`, and they need to keep normals. The GPU half becomes `vk/buffer` upload code.

### `mesh_factory.h/.cpp` — `MeshFactory`
`createTriangle`, `createQuad(textured)`, `createCube` (8 shared verts, coloured) and `createTexturedCube`
(24 verts, per-face UVs).
**Port:** `core/assets` `makeTriangle/makeQuad/makeCube()` returning `MeshData`, with normals added.

### `vertex.h/.cpp` — `Vertex`, `VertexAttributeFlags`
See the [vertex format](architecture.md#vertex-format).
**Port:** each backend describes vertex input from `MeshData`'s layout. Pick one location convention
shared with GL.

### `material_.h`, `shader_.h`, `scene_graph_.h` (sketches, not built)
Declarations only: `Material` (pipeline + texture + descriptor set), `Shader`/`ShaderLibrary`, and
`SceneNode` (TRS hierarchy).
**Port:** not in glint's scope before Phase 8.

## `src/glint_core/vks/` (Sascha Willems, MIT)

### `VulkanDevice.h/.cpp`
Every line is commented out; it compiles to nothing.
**Port:** retire (glint PLAN Phase 6 already says so).

### `vk_gltf_model.h/.cpp` — `vkglTF::Model`
Sascha's glTF 2.0 loader (nodes, meshes, materials, skins, animations, textures via KTX/stb) half-adapted
to `glint::VkContext`. It **does not compile** (references to `vks::`, `VK_CHECK_RESULT` without the
include, `device` members that don't exist) and is excluded from the build.
**Port:** glint Phase 6 `11_gltf`. Rewrite the loading into `core/assets` (tinygltf → `MeshData` +
materials) and use this file only as a reference for glTF semantics.

## `src/glint_ui/`

### `imgui_manager.cpp`, `ui/imgui_manager.h` — `ImGuiManager`
Creates its own descriptor pool (100 sets; sampler, combined image sampler and UBO types), initialises the
GLFW backend (installing callbacks) and the Vulkan backend against the renderer's **render pass**
(subpass 0, MSAA samples = context max), uploads fonts, and **overrides the scroll callback** to feed
`io.MouseWheel`. `newFrame()` and `render(cmd)` are static. `render` calls `ImGui::Render`,
`RenderDrawData`, then `EndFrame`.
**Port:** backend init goes into `app_vk/main.cpp` with `UseDynamicRendering = true`, and ImGui is drawn in
its own single-sample pass straight onto the swapchain image (this also fixes the caching problem). Frame
logic and panels go to `core/ui`.

## `src/samples/`

### `sample.h/.cpp` — `Sample`, `BasicSample`, `TriangleSample`, `QuadSample`
`Sample` provides `init` (stores window/renderer, calls `initSample`), `setupDefaultVieportAndScissor`
(full swapchain extent), and optional camera helpers (`initCamera`, `processCameraInput` from ImGui IO,
`updateCamera`). `BasicSample` draws an untransformed mesh with `base.vert/frag` and no descriptors.
**Port:** becomes `vk::Technique { init(vk::Context&); update(dt, Frame&); record(cmd, FrameInfo); ui(); }`.

### `sample_manager.h/.cpp` — `SampleManager`, `REGISTER_SAMPLE`
Singleton registry (name → factory, plus an ordered name list starting with `"None"`), active sample,
`setActiveSample` (wait idle → cleanup → construct → init), `update`, and `render`. `render` draws the
"Sample Selector" combo, begins the render pass, renders the sample and ImGui, and ends the pass.
**Port:** `core` `Registry<T>` instantiated per backend (glint Phase 2). Registration is explicit in each
app (`GLINT_REGISTER_VK`) or static; if static, register into a function-local static like this one does
to avoid init-order bugs. Move the render pass begin/end and the ImGui draw out of the registry.

### `sample_main.cpp` — `App`, `main`
Window 800×600, 2 frames in flight, starts on `CubeSample`, draws a "Glint Stats" window (FPS, frame time,
active sample), and ESC quits. Defines `OneTimeLogger::loggedFunctions`.
**Port:** `app_vk/main.cpp`.

The individual samples are covered in [samples.md](samples.md).
