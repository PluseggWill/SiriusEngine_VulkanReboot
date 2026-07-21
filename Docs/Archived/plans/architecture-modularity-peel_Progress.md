# Progress: architecture-modularity-peel

**Plan:** [`architecture-modularity-peel_Plan.md`](architecture-modularity-peel_Plan.md)  
**Kickoff:** 2026-07-21 · Decisions: defaults (full MOD order; accept Util Logger; delete Gfx_* aliases after A4)

## 2026-07-21 — Kickoff
- **Files:** README Active now; this Progress; Plan Status → In Progress
- **Verification:** N/A (docs)

## 2026-07-21 — MOD-A1
- **Files:** `Gfx_FrameDebugToggles.h` (+gpuCull/legacy/hybrid); `Vk_RenderFeatures.h`; App builds toggles from config; FrameGraph/ScenePasses/GBuffer/GpuCull/Post/GfxPipelineCache/DescriptorSystem/ShaderEffectMeta use DTOs; Architecture §1 App↔RenderCore note
- **Verification:** `Verify-CI.ps1` exit 0; `Verify-Smoke.ps1` exit 0; grep: no RenderCore `EngineConfig().GetGpuCull|GetLegacyDirectDraw`
- **Gate:** record-path policy via `Gfx_FrameDebugToggles` / init via `Vk_RenderFeatures`

## 2026-07-21 — MOD-A2
- **Files:** `Vk_TextureLoader.{h,cpp}`; slim `Util_Loader` to ResolvePath/ReadFile (no Vulkan); ResourceTables + IblResources call sites; Architecture Util must-not
- **Verification:** `Verify-CI` exit 0; `Verify-Smoke` exit 0; G0-validation stress exit 0 (no new `[VULKAN-VALIDATION]` ERROR)

## 2026-07-21 — MOD-A3
- **Files:** `Gpu_ClusterPush.h`, `Gpu_CullPush.h`, `Gpu_DrawTemplate.h`, `Gpu_EntityRecord.h`; Gfx keeps cluster helpers + fill; call sites renamed
- **Verification:** `Verify-CI` exit 0; `Verify-Smoke` exit 0

## 2026-07-21 — MOD-A4
- **Files:** RenderCore call sites → `Vk_TextureResource` / `Vk_MeshResource` / `Vk_MaterialResource`; deleted `using Gfx_*` aliases; Architecture resource ownership note
- **Verification:** `Verify-CI` exit 0; `Verify-Smoke` exit 0

## Track A — complete

## 2026-07-21 — MOD-B2
- **Files:** `Util_ImGuiLayer` → `Vk_ImGuiLayer`; PlatformContext; layout + Architecture Util must-not
- **Verification:** `Verify-CI` exit 0; `Verify-Smoke` exit 0

## 2026-07-21 — MOD-B3
- **Files:** `Pf_FrameHooks.h`; PlatformHost BeginFrame/ImGui/InitWindow use hooks; Application wires renderer; Architecture Platform boundary
- **Verification:** `Verify-CI` exit 0; `Verify-Smoke` exit 0

## 2026-07-21 — MOD-B1
- **Files:** App owns `Gfx_RenderCamera`; `Vk_PrimaryCameraState` sync; removed Renderer fly-camera ownership; DebugOverlay/SceneCpuLoad updated; Architecture App↔RenderCore note
- **Verification:** `Verify-CI` exit 0; `Verify-Smoke` exit 0

## 2026-07-21 — MOD-B7
- **Files:** TuningPrefs/Panel DTO Capture/Apply/Reset; Post/Temporal Actions; Lighting/RenderDebug → `Gpu_EnvironmentData.h`; App DebugOverlay applies TAA/temporal side effects; `App_ApplyDefaultEnvironmentData`
- **Verification:** `Verify-CI` exit 0; `Verify-Smoke` exit 0; Util panels: no `Vk_Renderer` / RenderCore includes
- **Gate:** panel headers compile without Vulkan / `Vk_Renderer`

## 2026-07-21 — MOD-B6
- **Files:** `Gfx_ShaderPermutation::Initialize(resolvedPath)` + App `SetActiveByName`; `Gfx_LoadSceneDesc(assetRoot, path)`; `UtilResolvePath`/`UtilLoader` asset-root overloads; Architecture Gfx↔Util locked note
- **Verification:** `Verify-CI` exit 0; `Verify-Smoke` exit 0; Gfx public APIs: no `Util_EngineConfig` type
- **Deferred to C4:** UtilLogger still used in Gfx load/draw-stream paths

## 2026-07-21 — MOD-B5
- **Files:** `Util_EngineConfig.cpp` → core getters/load; `Util_EngineConfigCli.cpp`; `Util_EngineConfigJson.cpp`; `Util_EngineConfigInternal.h`; vcxproj(+GfxTests)
- **Verification:** `Verify-CI` exit 0
- **Note:** Public aggregate API unchanged (`Util_EngineConfig` still owned by Application)

## 2026-07-21 — MOD-B4
- **Files:** `Vk_DeferredLightingPass_Record.cpp` (Create/Record peel); `Vk_PostProcessPassLayouts.cpp` + `Vk_PostProcessPassInternal.h` (CPU layout trackers)
- **Verification:** `Verify-CI` exit 0; `Verify-Smoke` exit 0
- **Deviation:** `Vk_AoPass` Create/Record + PostProcess Create/Record body split deferred (layout coupling); follow-up outside this epic if needed

## 2026-07-21 — MOD-C*
- **C1:** skipped (optional `App_FramePipeline`)
- **C2:** skipped (umbrella naming drive-by)
- **C3:** `Gfx_DebugView.h` — moved enum out of `Gfx_MaterialTypes.h`
- **C4:** accept Util Logger / VulkanResult as shared infra (Architecture locked)
- **C5:** `Vk_Enum.h` remains binding SoT + reflection verify (Architecture locked)
- **Verification:** `Verify-CI` exit 0

## 2026-07-21 — Third-party decode wraps (drive-by)
- **Files:** `Util_ImageDecode` (stb); `Util_ObjLoad` (tinyobj); `Gfx_MeshCpu` / `Vk_TextureLoader` call wrappers only
- **Accepted as-is:** GLM math; VMA in RenderCore; ImGui in Util panels + `Vk_ImGuiLayer`; GLFW in Platform/WSI; nlohmann JSON at parse sites
- **Verification:** `Verify-CI` + `Verify-Smoke` exit 0

## Closeout — 2026-07-21
- **Outcome:** Tracks A–B complete (B4 partial on Ao/Post Record); Track C policy/hygiene landed; layering greps + Architecture locked notes updated
- **Verification:** `Verify-CI` exit 0; `Verify-Smoke` exit 0
- **Deviations:** B4 Ao + PostProcess Create/Record body split deferred; C1/C2 skipped as optional
