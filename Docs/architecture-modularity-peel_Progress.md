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

## Remaining (not yet landed)
- **MOD-B7** Panels without `Vk_Renderer`
- **MOD-B6** Gfx ⟂ Util config/IO
- **MOD-B5** Split `Util_EngineConfig`
- **MOD-B4** Split fat passes
- **MOD-C*** Hygiene

## 2026-07-21 — Third-party decode wraps (drive-by)
- **Files:** `Util_ImageDecode` (stb); `Util_ObjLoad` (tinyobj); `Gfx_MeshCpu` / `Vk_TextureLoader` call wrappers only
- **Accepted as-is:** GLM math; VMA in RenderCore; ImGui in Util panels + `Vk_ImGuiLayer`; GLFW in Platform/WSI; nlohmann JSON at parse sites
- **Verification:** `Verify-CI` + `Verify-Smoke` exit 0
