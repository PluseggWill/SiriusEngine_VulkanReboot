# Progress: temporal

## 2026-07-13 — S9.1 temporal skeleton and jitter
- **Files:** `Gfx/Gfx_TemporalJitter.h`, `RenderCore/Vk_Temporal*`, `Util/Util_TemporalPanel.*`, `Application.cpp`, `Vk_SwapchainHost.cpp`, `Vk_Renderer.cpp`, `DebugOverlay.cpp`, `GfxTests_Main.cpp`
- **Verification:** `powershell -File Scripts/Verify-CI.ps1` exit 0; smoke pass 1 (default) exit 0; pass 2 gpu-cull leg still fails pre-existing `(gpu-deferred)` log token (unchanged by S9.1)
- **Notes:** Halton(2,3) jitter on primary view projection; shared history reset on resize/scene swap/camera cut/manual; Engine Debug → Temporal tab.

## 2026-07-13 — S9.2 motion vectors baseline
- **Files:** `RenderCore/Vk_GBufferPass.*`, `RenderCore/Vk_DeferredLightingPass.cpp`, `RenderCore/Vk_FrameUniformUploader.cpp`, `RenderCore/Vk_Camera.h`, `Shader/GBuffer.vert`, `Shader/GBuffer.frag`, `Shader/DeferredLighting.frag`, `Util/Util_RenderDebugPanel.cpp`, `Gfx/Gfx_MaterialTypes.h`
- **Verification:** `powershell -File Scripts/Verify-Smoke.ps1 -SkipGpuCull` exit 0; manual `--validation` smoke exit 0 (no `[VULKAN-VALIDATION]` in `Logs/engine_runtime_log.txt`)
- **Notes:** Added G-buffer motion-vector attachment (RG16F) with current-vs-previous reprojection, wired deferred debug view `Motion vectors`, and uploaded temporal camera matrices via `GpuCameraData`.

## 2026-07-13 — S9.3 TAA v0.5 sharpen pass
- **Files:** `Shader/TaaResolve.comp`, `RenderContract/GpuPostSettings.h`, `RenderCore/Vk_PostProcessPass.cpp`, `Util/Util_PostProcessPanel.cpp`
- **Verification:** Debug+Release rebuild; `Verify-Smoke.ps1 -SkipGpuCull` exit 0
- **Notes:** Catmull-Rom history, tonemapped YCoCg variance clip (γ), velocity-adaptive blend, unsharp; ImGui: history blend / variance gamma / sharpen. Defaults: blend=0.875, γ=1.25, sharpen=0.35.

## 2026-07-13 — TAA distant jitter fix
- **Files:** `Shader/GBuffer.vert`, `Shader/GBuffer.frag`, `Shader/GBufferFrag_Bindless.frag`, `Shader/TaaResolve.comp`
- **Verification:** Debug+Release rebuild; `Verify-Smoke.ps1 -SkipGpuCull` exit 0
- **Notes:** Perspective-correct MV via clip varyings; velocity-adaptive deadzone 1.5px (ignore Halton); reduce sharpen when static.

## 2026-07-13 — Pre-commit review polish
- **Files:** `Shader/TaaResolve.comp`, `Util/Util_TuningPrefs.cpp`, `Shader/TriangleVertex.vert`
- **Notes:** Single MV sample in TAA resolve; persist TAA tunables in prefs; align `TriangleVertex` CameraData with `GpuCameraData`. S9.1–S9.3 ready to land; S9.4/S9.5 still open.
