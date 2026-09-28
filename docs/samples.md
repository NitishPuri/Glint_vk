# Samples

Registered with `REGISTER_SAMPLE` and listed in the "Sample Selector" combo (registration order depends on
link order; `"None"` is always first). The app starts on `CubeSample`. All samples render inside the
renderer's single MSAA render pass, use the interleaved `Vertex` (pos/colour/uv), and use the dynamic
viewport and scissor for the full swapchain extent.

The "glint" line maps each sample to the technique list in `glint/docs/PLAN.md`, and says what it's
worth taking.

| Registry name | Class / file | Technique | Descriptors | Shaders | Depth | Camera |
|---|---|---|---|---|---|---|
| `TriangleSample` | `BasicSample` / `sample.cpp` | hello triangle, vertex colours, no transform | none | `base.vert/.frag` | off | — |
| `QuadSample` | `sample.cpp` | indexed quad, vertex colours | none | `base.vert/.frag` | off | — |
| `RotatingSample` | `rotating_sample.*` | triangle rotating about Y, MVP UBO | UBO ×2 (per frame) | `baseMVP.vert`, `base.frag` | off | fixed `lookAt` |
| `TexturedRotatingSample` | `textured_quad.*` | textured quad rotating about Z | UBO + combined sampler ×2 | `basic_tex.vert/.frag` | off | fixed `lookAt` |
| `CubeSample` (default) | `cube_sample.*` | textured cube, wobbling translate + axis blend, orbit camera | UBO + combined sampler ×2 | `basic_tex.vert/.frag` | on | arcball |
| `DynamicUniformBuffer` | `dynamic_uniform_buffer.*` | 125 cubes, one dynamic UBO slice each | UBO + dynamic UBO ×1 | `dynamic_uniform_buffer.vert`, `base.frag` | on | arcball |

Not registered or not built:

| Name | Where | State |
|---|---|---|
| `SpecializationConstants` | `specialization_constants.*` | Commented out of CMake. It's a copy of `DynamicUniformBuffer` (same shaders, same code); the specialization-constant part was never written. `uber.vert/.frag` (Sascha's Phong/toon/textured shader with `constant_id` 0/1) exist but aren't in the shader build list. |
| `triangle` | `src/minimal/triangle/main.cpp` | Separate executable. Same as `TriangleSample`, without ImGui. |
| `cube` | `src/minimal/cube/main.cpp` | Separate executable. Textured cube with ImGui stats and camera; renders **upside down** (double Y flip, see known issues). |

> The minimal apps are **not** raw Vulkan: they sit on `glint_core`'s `Renderer`, `Pipeline`, `Mesh` and so
> on. There is no raw single-file Vulkan program in this repo. For glint's raw `01_triangle/vk.cpp`, use
> vulkan-tutorial.com (which this code follows closely) or `/mnt/e/tree/graphics/vulkan/` as the reference.

---

## TriangleSample / QuadSample — `sample.cpp`
- **Resources:** `MeshFactory::createTriangle()` (3 verts, indexed) or `createQuad()` (4 verts, 6 indices).
  No descriptors.
- **Pipeline:** `POSITION_COLOR`, no depth, back-face culling, CCW.
- **Frame:** bind pipeline → bind mesh → viewport/scissor → draw.
- **glint:** `01_triangle`. The VK side in glint starts even simpler (vertices from `gl_VertexIndex`, no
  vertex buffer). Glint_gl's `QuadScene` covers the quad.

## RotatingSample — `rotating_sample.*`
- **Resources:** triangle mesh; set layout {0: UBO, vertex}; pool and 2 sets; 2 `UniformBuffer`s holding
  `{model, view, proj}`.
- **Init:** fixed view (eye at z = 3) and perspective (45°, aspect at init time, Y flipped); written to both UBOs.
- **Update:** `model = rotate(angle, Y)` → `UBO[currentFrame]`.
- **Frame:** pipeline → set[currentFrame] → mesh → draw. No depth, no culling.
- **Note:** the aspect ratio is captured once and not updated on resize.
- **glint:** folds into `01_triangle` or `02_cube` as the first UBO. In glint, prefer a push constant for
  a single matrix: it's the smallest step up from GL's `glUniformMatrix4fv`.

## TexturedRotatingSample — `textured_quad.*`
- **Resources:** textured quad; `res/texture.jpg` → `Texture` (sRGB, mipmapped, anisotropic); layout {0: UBO
  (vertex), 1: combined image sampler (fragment)}; 2 UBOs and 2 sets, each pointing at the same texture.
- **Update:** rotation about Z. View and projection are rebuilt every frame from the swapchain extent.
- **Shader:** `outColor = vertexColour * texture(uv).rgb`, so the texture is tinted by vertex colour.
- **glint:** `03_textured_cube` (image upload, layout transitions, sampler, mip blits).

## CubeSample — `cube_sample.*` (default)
- **Resources:** `MeshFactory::createTexturedCube()` (24 verts); `texture.jpg`; same descriptor layout as
  the textured quad; 2 UBOs.
- **Pipeline:** depth test and write on, `LESS`, **no culling**.
- **Update:** rotation angle += 45°/s; translate `x = sin(t)/2`; rotation axis blends Z → X by `sin(t)`;
  camera input from ImGui (right-drag orbits, middle-drag pans, wheel zooms). Then `UBO[currentFrame]`
  gets `{model, camera.view, camera.proj}`.
- **Commented-out alternative:** load `res/viking_room.obj` + `viking_room.png` (vulkan-tutorial's model).
- **glint:** `02_cube` + `03_textured_cube`. This sample is the best place to start comparing with Glint_gl's
  `UVCubeScene`.

## DynamicUniformBuffer — `dynamic_uniform_buffer.*`
Port of Sascha Willems' `dynamicuniformbuffer` example.
- **Resources:** coloured cube (8 shared verts); **one** view UBO `{projection, view}` (coherent); **one**
  dynamic UBO of `125 × dynamicAlignment` bytes (host-visible, **not** coherent), where
  `dynamicAlignment = align(sizeof(mat4), minUniformBufferOffsetAlignment)`; the CPU copy is allocated with
  `posix_memalign` / `_aligned_malloc`.
- **Descriptors:** layout {0: UBO, 1: `UNIFORM_BUFFER_DYNAMIC`}, one set; the dynamic binding's range is one
  `dynamicAlignment` slice.
- **Update:** camera → view UBO; at most 60 Hz, 5×5×5 grid of model matrices with random rotation speeds →
  memcpy → `vkFlushMappedMemoryRanges`.
- **Frame:** 125 × (`vkCmdBindDescriptorSets` with dynamic offset `j × dynamicAlignment` → `drawIndexed`).
- **Note:** a single copy of each UBO is shared by both frames in flight, so the CPU writes while the GPU may
  still be reading the previous frame.
- **glint:** Phase 6 `09_dynamic_uniform_buffer`. GL twin: one UBO + `glBindBufferRange` at
  `GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT`. Worth a `NOTES.md` comparison with push constants and instancing,
  which are the more common ways to do per-object data.

## Shaders (`src/shaders/`)

| File | Inputs | Resources | Used by |
|---|---|---|---|
| `base.vert` | pos, colour | — | Triangle, Quad |
| `baseMVP.vert` | pos, colour | b0 UBO {model, view, proj} | Rotating |
| `basic_tex.vert` | pos, colour, uv | b0 UBO {model, view, proj} | TexturedRotating, Cube, minimal cube |
| `dynamic_uniform_buffer.vert` | pos, colour | b0 UBO {projection, view}, b1 dynamic UBO {model} | DynamicUniformBuffer |
| `base.frag` | colour | — | several |
| `basic_tex.frag` | colour, uv | b1 `sampler2D` | TexturedRotating, Cube |
| `uber.vert/.frag` | pos, normal, uv, colour | b0 UBO, b1–2 samplers, spec constants 0–1 | nothing (not compiled) |

All shaders are `#version 450`. There's no `#include` or shared header, and no push constants.

## Assets (`res/`)

| File | Used by |
|---|---|
| `texture.jpg` | TexturedRotating, Cube, minimal cube |
| `viking_room.obj`, `viking_room.png` | nothing (commented out in `CubeSample`); vulkan-tutorial's model |
