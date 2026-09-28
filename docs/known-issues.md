# Known issues and tech debt

Found while reviewing the code on 2026-09-29. Apart from the two Linux-port fixes listed in
[build.md](build.md), none of these have been fixed in Glint_vk. The point is to avoid repeating them in
`glint`, not to patch this repo.
Severity: 🔴 wrong output, crash or spec violation · 🟠 misleading or fragile · ⚪ cleanup.
Line numbers are for commit `8183e97` plus the Linux port changes.

## Frame synchronisation

| | Where | Issue |
|---|---|---|
| 🔴 | `samples/sample_main.cpp:101` → `renderer.cpp:115` | `SampleManager::update()` writes `UBO[currentFrame]` **before** `drawFrame` waits on the fence for that frame slot, so the CPU can overwrite a uniform buffer the GPU is still reading from two frames ago. Fix: wait on the frame's fence first, then update, then record. |
| 🔴 | `renderer.cpp:176`, `synchronization_manager.cpp:42` | `renderFinished` semaphores are indexed by frame-in-flight, but they're consumed by `vkQueuePresentKHR`, which has no fence. With 2 slots and 3+ images, a semaphore can be re-signalled while a previous present still waits on it. Newer validation layers flag this (`VUID-vkQueueSubmit-pSignalSemaphores-00067`). Fix: one present semaphore **per swapchain image**. |
| 🟠 | `renderer.cpp:115,181` | Fences are per image, but the wait uses the image this *slot* used last time, while the reset and submit use the *newly acquired* image's fence. That's only safe if the new image's previous submission is finished, which acquire order usually gives you but doesn't guarantee. The same applies to re-recording `cmd[imageIndex]`. |
| 🟠 | `renderer.cpp:56,74`, `swapchain.cpp` recreate | Command buffers, fences and `m_CommandBufferRecorded` are sized to the image count once. If recreation returns more images, `imageIndex` indexes out of range (it's only guarded by `assert`). |
| 🟠 | `renderer.cpp:96` + `:123` | On `VK_ERROR_OUT_OF_DATE_KHR` from acquire, `handleResize()` returns early unless the window's `resized` flag is set, so an out-of-date swapchain that wasn't caused by a resize (e.g. a monitor change) is never recreated, and every frame bails out. |
| 🟠 | `samples/sample_manager.cpp:111` | Switching samples happens inside command buffer recording: `waitIdle`, destroy the sample and pipeline, create new ones, then keep recording. It works by accident of ordering. |
| 🟠 | dynamic UBO sample (`dynamic_uniform_buffer.cpp:74,192`) | One view UBO and one dynamic UBO are shared by both frames in flight, so they're written while in use. |
| 🟠 | command buffer caching (`renderer.cpp:133`) | Breaks ImGui (known). It also bakes the descriptor set of the recording frame slot into a per-image buffer, so later frames read a stale UBO while the other one is being written. See [architecture.md](architecture.md#command-buffer-caching---enable_command_buffer_caching). |

## Rendering correctness

| | Where | Issue |
|---|---|---|
| 🔴 | `src/minimal/cube/main.cpp:200` | `ubo.proj[1][1] *= -1` on a matrix that `Camera::getProjectionMatrix()` already flipped (`camera.cpp:106`), so the minimal cube app renders **upside down**, and back-face culling would be inverted if it were enabled. |
| 🟠 | `texture.cpp:84` | Every texture is uploaded as `R8G8B8A8_SRGB`. That's correct for albedo, and wrong for normal, specular or data maps. |
| 🟠 | `texture.cpp:144` | If the format doesn't support linear blits, it logs "falling back to generating mipmaps on CPU", transitions the whole image to `SHADER_READ_ONLY`, and then **runs the blit loop anyway** with the wrong old layouts. |
| 🟠 | `vk_context.cpp:200` + `pipeline.cpp:96` | MSAA is always at the device's maximum sample count (for colour, depth, every pipeline and ImGui). It isn't configurable, and on big windows it costs a lot of memory and bandwidth. |
| 🟠 | `swapchain.cpp:237` | MAILBOX is preferred, so frames are uncapped (100 % GPU, high FPS readout) with no vsync option; FPS numbers aren't comparable to Glint_gl's vsynced loop. |
| 🟠 | `swapchain.cpp:257` | When `currentExtent` is undefined, the extent comes from the window size in screen coordinates rather than `glfwGetFramebufferSize`, which is wrong on HiDPI. (`dynamic_cast` there is also unnecessary.) |
| 🟠 | `samples/sample.cpp:58` | Camera input reads ImGui mouse state without checking `io.WantCaptureMouse`, so dragging ImGui windows also orbits the camera. The wheel delta is truncated to `int`, so trackpad scrolling is lost. |
| 🟠 | `rotating_sample.cpp:76`, `textured_quad.cpp` | Each sample builds its own projection and flips Y itself, while `CubeSample` gets a pre-flipped matrix from `Camera`. There are two conventions in one codebase. |
| ⚪ | swapchain is `B8G8R8A8_SRGB` | ImGui's colours are authored in sRGB and get gamma-encoded again, so the UI looks washed out. |

## Robustness and lifetime

| | Where | Issue |
|---|---|---|
| 🔴 | `renderer.cpp:127` | `"failed to acquire swap chain image!" + __LINE__` is pointer arithmetic on a string literal, not concatenation. The exception message is garbage (or reads out of bounds). |
| 🔴 | `mesh.h:46` | `Mesh` stores `const std::vector<Vertex>&` and `const std::vector<uint32_t>&` members that bind to the constructor arguments, which are temporaries or locals in every caller (and the `indices = {}` default). They dangle once construction ends. It's only safe because nothing reads them after the constructor. |
| 🟠 | `vk_tools.h:45` | `VK_CHECK_RESULT` `assert`s, so in Release (`NDEBUG`) every failed Vulkan call is printed and **execution continues**. |
| 🟠 | `vk_context.cpp:107,194,231` | API version 1.0. The first "suitable" device wins (no discrete/integrated preference; llvmpipe would be accepted if listed first). `sampleRateShading` is enabled without checking support. |
| 🟠 | `swapchain.cpp:140` | `oldSwapchain` is always `VK_NULL_HANDLE`, and recreation waits for the device to go idle and destroys everything first. It's correct but causes a stall and a flash on resize. |
| 🟠 | `vk_utils.h:52`, `vk_context.h` `setCommandPool` | Global static context, plus a command pool borrowed from `CommandManager` via `VkContext`. It hides dependencies and forces the "command manager before swapchain" ordering (`renderer.cpp` TODO). |
| 🟠 | `vk_utils.cpp:65`, `mesh.cpp:158` | Every upload is a separate submit + `vkQueueWaitIdle`, and `Mesh::copyBuffer` duplicates `VkUtils::copyBuffer`. That's fine for learning, but it makes loading anything larger (e.g. glTF) slow. |
| 🟠 | `logger.h:138` | `OneTimeLogger::loggedFunctions` has to be defined in every executable's `main.cpp`, or it fails to link. `inline static` would fix it. |
| 🟠 | `logger.h:29` | The log file opens (and truncates `log.txt` in the CWD) on first use, even when logging is disabled. `log.txt` is tracked in git, so every run dirties the tree. |
| 🟠 | `samples/CMakeLists.txt:35` vs `minimal/CMakeLists.txt` | `GLM_FORCE_DEPTH_ZERO_TO_ONE` / `GLM_FORCE_RADIANS` are set per target (`PRIVATE` on `glint_core`, on `glint_samples`, not on the minimal apps). GLM's inline functions are therefore compiled with different settings in different TUs (an ODR hazard). They should be `PUBLIC` on `glint_core`. |
| ⚪ | `logger.h:143` | `LOGCALL(x)` expands to two statements; inside `if (...)` it only works because of C++17's if-with-initialiser. |
| ⚪ | `imgui_manager.cpp:55,61,77` | Font upload is wrapped in an empty one-shot command buffer (ImGui 1.92 uploads fonts itself). The scroll callback replaces ImGui's own (which was installed with `install_callbacks = true`) instead of chaining it. `ImGui::EndFrame()` after `Render()` is redundant. |
| ⚪ | `cube_sample.cpp:96` etc. | `static auto t` in `update()` is shared across sample instances, so the wobble phase carries over between switches. |
| ⚪ | registration | Sample order in the combo depends on static initialisation / link order. |

## Dead or unfinished code

| | Where | State |
|---|---|---|
| ⚪ | `src/glint_core/vks/VulkanDevice.*` | Entirely commented out. |
| ⚪ | `src/glint_core/vks/vk_gltf_model.*` | Doesn't compile; excluded from the build. |
| ⚪ | `samples/specialization_constants.*` | Copy of `DynamicUniformBuffer`; spec constants never implemented; not built. |
| ⚪ | `shaders/uber.vert/.frag` | Not in the shader build list. |
| ⚪ | `core/input_.h`, `renderer/material_.h`, `shader_.h`, `scene_graph_.h` | Declarations only. |
| ⚪ | `Mesh::loadModel`, `res/viking_room.*`, `ext/ktx`, `ext/tinygltf` | Built or present but unused. |
