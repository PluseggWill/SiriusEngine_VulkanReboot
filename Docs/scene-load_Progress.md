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
