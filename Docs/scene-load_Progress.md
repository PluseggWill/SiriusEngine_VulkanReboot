# Progress: scene-load

## 2026-05-22 — Plan + SprintPlan tasks

- **Plan ref:** Design doc + SprintPlan linkage
- **Files:** `Docs/scene-load_Plan.md`, `Docs/SprintPlan.md`
- **What changed:** Documented migration from `Util_DemoAssets` / hard-coded `Vk_Core` loads to scene-driven `AssetManifest`, lifecycle order, and phased implementation (A–D). Added S2 tasks referencing this plan.
- **Verification:** N/A (documentation only)

## 2026-05-26 — Phase A: scene JSON parse + dependency closure

- **Plan ref:** Phase A (A1–A4)
- **Files:** `Data/Scenes/demo.json`, `VulkanDesktop/Gfx/Gfx_SceneDesc.h`, `Gfx_SceneLoader.{h,cpp}`, `Util/Util_AssetManifest.{h,cpp}`, `Util/Util_AssetConfig.{h,cpp}`, `VulkanDesktop/VulkanDesktop.cpp`, `VulkanDesktop.vcxproj`, `VulkanDesktop.vcxproj.filters`, `lib/nlohmann/json.hpp`, `Docs/scene-load_Plan.md`, `Docs/SprintPlan.md`, `Docs/EngineArchitecture.md`
- **What changed:**
  - Added v1 scene JSON (`version`, shaders/meshes/textures/materials/entities with column-major `transform`, `logicalMesh`, optional `renderFlags`).
  - `Gfx_LoadSceneDesc` parses via vendored nlohmann/json (CPU-only, no Vulkan).
  - `Util_CollectDependencies` deduplicates repo-relative paths (16 entries for demo scene).
  - CLI `--scene` (default `Data/Scenes/demo.json`); startup loads/parses scene before existing `UtilStartupChecks` (Phase B will switch verify to manifest).
  - GPU / SoA demo path unchanged (`Vk_Core::InitDemoSceneEntities` still hard-coded).
- **Verification:**
  - Build: `MSBuild VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64` — exit 0
  - Smoke: `--help` shows `--scene`; default run logs `[CONFIG] scene=…`, `[SCENE] Parsed scene v1 … entities=9`, `[SCENE] Collected 16 unique asset path(s)`; app init + one frame OK
  - Log: `Logs/engine_runtime_log.txt`

## 2026-05-26 — Phase B: manifest-driven startup verify

- **Plan ref:** Phase B (B1–B4)
- **Files:** `Util/Util_AssetManifest.{h,cpp}`, `Util/Util_DemoAssets.h`, `VulkanDesktop/VulkanDesktop.cpp`, removed `Util_StartupChecks.{h,cpp}`, vcxproj/filters, docs
- **What changed:**
  - `Util_VerifyManifest` — same `[STARTUP] OK/ERROR` semantics, driven by scene manifest paths.
  - `VulkanDesktop.cpp`: `LoadSceneDesc` → `CollectDependencies` → `VerifyManifest` → `Run()`.
  - Removed `UtilDemoAssets::kRequiredFiles` and `Util_StartupChecks` module.
- **Verification:**
  - Build Debug|x64 — exit 0
  - Smoke-run: `[STARTUP] Verifying scene asset manifest (16 path(s))`, all OK, app init OK
  - Grep: no `kRequiredFiles` / `VerifyRequiredAssets` under `VulkanDesktop/`

## 2026-05-27 — Phase C: scene-driven SoA, LOD, and resource tables

- **Plan ref:** Phase C (C1–C4)
- **Files:** `Gfx_SceneApply.{h,cpp}`, `Gfx_SceneDesc.h`, `Gfx_SceneLoader.cpp`, `Data/Scenes/demo.json` (`logicalMeshes`), `Vk_Core.{h,cpp}`, `VulkanDesktop.cpp`, vcxproj/filters, docs
- **What changed:**
  - `SetLoadedScene` + lifecycle: main loads/verifies scene → passes desc to `Vk_Core` before `Run()`.
  - `Gfx_BuildResourceManifestFromSceneDesc` / `Gfx_PopulateSceneSoAFromSceneDesc` / `Gfx_BuildLodTableFromSceneDesc`.
  - `Vk_Core::InitVulkan` uses scene shaders (`lit`), scene manifest for `LoadFromManifest`; removed `InitDemoSceneEntities` and demo manifest builder.
  - `demo.json` adds `logicalMeshes` (tree LOD chain + distances).
- **Verification:**
  - Build Debug|x64 — exit 0
  - Smoke-run: `[SCENE] Populated SoA … entities=9`, `[RESOURCE-TABLE] meshes=8 materials=7`, `[EXTRACT] entities=9 draws=9`
  - Grep: no `UtilDemoAssets` / `Data/Models` in `Vk_Core.cpp`

## 2026-05-27 — Review note: legacy manifest builder

- **Plan ref:** Phase C close-out / `scene-load_Plan.md` → Legacy retained
- **What:** Documented why `Gfx_BuildDemoResourceManifest` stays in tree (reference ids + manifest shape; not called from `Vk_Core`). Header comments in `Gfx_ResourceManifest.*`, `Util_DemoAssets.h`.
- **Verification:** N/A (docs/comments only)

## 2026-05-27 — Session pause: handoff + SceneJSON guide (no Phase D code)

- **Plan ref:** `scene-load_Plan.md` → Handoff — 2026-05-27 pause; Phase D deferred
- **Files:** `Docs/scene-load_Plan.md`, `Docs/scene-load_Progress.md`, `Docs/SprintPlan.md`, `Docs/SceneJSON.md`, `Data/Scenes/README.md`, `Docs/README.md`
- **What changed:**
  - Recorded **current runtime flow**, completed vs open gaps, **mermaid dependency graph**, and explicit note that SprintPlan **Application lifecycle** ≠ Phase C C1.
  - **Decision:** Do **not** start Phase D until S2 Application lifecycle (`LoadSceneResources` out of `InitVulkan`, `UnloadScene` API).
  - Added **[`SceneJSON.md`](SceneJSON.md)** — v1 schema, transform 约定, id 下标规则, 自检清单, demo 对照表.
- **Next session (recommended order):**
  1. S2 Application lifecycle plan/implement
  2. Thin scheduler (optional same PR)
  3. scene-load Phase D (D1 → D3)
- **Verification:** N/A (documentation only)

## 2026-05-27 — SceneJSON.en.md + README links

- **Plan ref:** Handoff doc index
- **Files:** `Docs/SceneJSON.en.md`, `Docs/SceneJSON.md`, `README.md`, `Docs/README.md`, `Data/Scenes/README.md`, `Docs/scene-load_Plan.md`, `Docs/SprintPlan.md`
- **What changed:** Full English scene JSON authoring guide (mirror of Chinese doc); cross-links EN/中文; root README points to both.
- **Verification:** N/A (documentation only)

## 2026-05-29 — Phase D: GPU unload, asset verify policy, smoke scene

- **Plan ref:** Phase D (D1–D3)
- **Files:** `RenderCore/Vk_Core.{h,cpp}`, `Vk_ResourceTables.{h,cpp}`, `Vk_DescriptorSystem.cpp`, `Vk_GfxPipelineCache.{h,cpp}`, `Vk_SwapchainHost.cpp`, `Util/Util_AssetManifest.{h,cpp}`, `Util/Util_EngineConfig.{h,cpp}`, `App/Application.cpp`, `Config/engine.json`, `Data/Scenes/smoke.json`, `Data/Scenes/README.md`, docs
- **What changed:**
  - **D1:** `mySceneDeletionQueue` for scene GPU resources; `UnloadScene` waits idle, shuts down ImGui, destroys scene pipelines, flushes scene queue, clears tables and descriptor handles; `LoadSceneResources` fail-closed if scene already loaded; descriptor pool only created at scene load (removed duplicate pool from `InitRenderDevice`); removed duplicate `CreateDescriptorSetLayout` from swapchain init.
  - **D2:** `Util_AssetVerifyPolicy` + `Util_VerifyManifest(manifest, policy)`; `engine.json` `"assetVerify": "strict"|"warn"`.
  - **D3:** `Data/Scenes/smoke.json` (1 entity); README updated.
  - **Dev:** `--smoke-frames N` for graceful lifecycle smoke; fixed instance-slab VMA unmap before destroy on shutdown.
- **Verification:** MSBuild Debug|x64 exit 0; `VulkanDesktop.exe --smoke-frames 2 --no-validation --scene Data/Scenes/smoke.json` exit 0; log contains `UnloadScene: GPU scene resources released`, `[RESOURCE-TABLE] meshes=1`, no crash through `Shutdown`.

## 2026-05-29 — CLI reference + ImGui Scene panel

- **Plan ref:** User request (CLI doc + runtime scene switch)
- **Files:** `Docs/CLI.md`, `Docs/bootstrap.md`, `Docs/README.md`, `Util/Util_ScenePanel.{h,cpp}`, `App/Application.{h,cpp}`, `RenderCore/Vk_Core.{h,cpp}`, `Util/Util_EngineConfig.cpp`, vcxproj/filters
- **What changed:** Added full CLI/config doc; ImGui **Scene** panel lists `Data/Scenes/*.json` and **Load selected scene** triggers `UnloadScene` → verify → `LoadSceneResources`; reload failure restores previous scene; `--help` points to `Docs/CLI.md`.
- **Verification:** MSBuild Debug|x64 exit 0.

## 2026-05-29 — Task closeout (scene-load + CLI + smoke-test rule)

- **Plan ref:** scene-load Phases A–D closed; SprintPlan archived `[S2]` Phase D + ImGui reload lines
- **Files:** all session deltas; `Docs/scene-load_Plan.md` handoff/closeout, `Docs/README.md`, `SceneJSON*.md`, `EngineArchitecture.md`, `.cursor/rules/vulkan-smoke-test.mdc`, vibe-coding skill/entry
- **Verification:** `VulkanDesktop.exe --no-validation --smoke-frames 2 --scene Data/Scenes/smoke.json` exit 0; log `UnloadScene: GPU scene resources released`, `Engine exited run loop normally`
