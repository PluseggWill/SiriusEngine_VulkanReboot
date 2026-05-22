# Plan: scene-load

**Sprint:** S2 — Engine layering & hygiene (scene + lifecycle); ties to S1 resource tables  
**Status:** Planned (design doc; implementation not started)  
**Related:** [`EngineArchitecture.md`](EngineArchitecture.md) §3–4, [`asset-root_Plan.md`](asset-root_Plan.md), [`startup-checks_Plan.md`](startup-checks_Plan.md)

## Problem

Today the demo path is **hard-coded**: `Util_DemoAssets.h` lists SPIR-V / mesh / texture paths; `UtilStartupChecks` verifies that fixed list before Vulkan init; `Vk_Core::InitVulkan` calls `CreateMesh` / `CreateTexture` with global strings. That was intentional **S0 bootstrap** but does not scale to multiple scenes, editor content, or a reproducible vertical slice.

**Goal:** Scene file on disk becomes the **single source of truth** for which assets exist; startup checks and GPU upload both derive from a **dependency manifest** collected from the scene—not from a C++ constant array.

## Goals

1. **Scene description on disk** (`Data/Scenes/…`) — meshes, textures, shader pairs, entities (transform + mesh + material refs).
2. **Parse without Vulkan** — scene load is a CPU-only phase in application lifecycle.
3. **`AssetManifest`** — deduplicated repo-relative paths from the scene closure (shaders + referenced content).
4. **Startup checks from manifest** — replace `UtilDemoAssets::kRequiredFiles` with `VerifyManifest(CollectDependencies(scene))`; still run **after** `UtilAssetConfig::Initialize`, **before** `vkCreateInstance`.
5. **Resource tables** — load paths once into `MeshTable` / `TextureTable` / material → pipeline; hot path uses integer ids only (aligns with S1 draw stream).
6. **CLI** — `--scene <logical-path>` (default e.g. `Data/Scenes/demo.json` during transition).

## Non-goals (this plan)

- Full gameplay / objective loop (parallel vertical slice).
- Async IO / background mesh decode (v1 may stay synchronous; reserve interfaces).
- Scene graph hierarchy (document flat matrices first; parent indices later).
- Bindless / GPU-driven path changes (S3–S6); scene format should not block those.
- Validating SPIR-V magic or OBJ topology at manifest stage (optional later; load-time fail-fast is enough for v1).

## Target architecture

Dependency direction matches `EngineArchitecture.md`:

```text
磁盘场景文件 (Data/Scenes/…)
    ↓ 解析（无 Vulkan）
SceneDesc / World（SoA + 资源引用：meshId, materialId, textureId…）
    ↓ CollectDependencies
AssetManifest（去重逻辑路径）
    ↓ VerifyManifest（存在性，仍在 Vulkan 之前）
    ↓ LoadSceneResources（同步 v1）
ResourceTables → GPU 句柄 / 描述符索引
    ↓ Extract（S1，无 Vulkan）
Draw stream → Vk_Core 只录制/提交
```

**Rule:** `Vk_Core` must not own scene JSON parsing or repo-relative path strings long-term.

## Migration from today

| Today (S0) | Target (scene-load) |
|------------|---------------------|
| `Util_DemoAssets.h` five constants | Paths in `Data/Scenes/demo.json` (or scene-specific file) |
| `UtilStartupChecks::VerifyRequiredAssets()` | `VerifyManifest(CollectDependencies(sceneDesc))` |
| Check in `VulkanDesktop.cpp` before `Run()` | After `LoadSceneDesc`, before `InitVulkan` |
| `Vk_Core` globals `vertShaderPath`, `CreateMesh(…)` in `InitVulkan` | `SceneResourceLoader` in lifecycle **load resources**; `Vk_Core` consumes tables |
| `UtilLoader::ResolvePath` | **Unchanged** — scene stores repo-relative paths under `assetRoot` |

Keep `Util_DemoAssets` only until the first scene file drives the same demo; then delete or reduce to a one-line default scene path constant.

## Design decisions

| Item | Decision |
|------|----------|
| Scene format (v1) | **JSON** under `Data/Scenes/` (minimal parser or small dep; match `engine.json` style if hand-rolled) |
| Path strings | Repo-relative (`Data/…`, `VulkanDesktop/Shader_Generated/…`); resolve via `UtilLoader::ResolvePath` |
| Manifest | `CollectDependencies(SceneDesc)` returns `std::vector<std::string>` or small `AssetManifest` struct (paths + optional kind enum) |
| Startup policy | **Strict** at boot for manifest (missing file → throw, log `[STARTUP]`); runtime optional assets → warn + placeholder (parallel slice; config later) |
| Default scene | `Data/Scenes/demo.json` equivalent to current demo content |
| Lifecycle order | `Init` → `LoadSceneDesc` → `VerifyManifest` → `InitWindow` / `InitVulkan` → `LoadSceneResources` → loop → `UnloadScene` / shutdown |
| Modules (proposed) | `Gfx_SceneDesc` or `Util_SceneLoader` (parse); `Util_AssetManifest` (collect + verify); `Application` or thin `VulkanDesktop` lifecycle coordinator |

### Example scene sketch (v1)

```json
{
  "shaders": {
    "lit": {
      "vert": "VulkanDesktop/Shader_Generated/TriangleVert.spv",
      "frag": "VulkanDesktop/Shader_Generated/TrianglePix.spv"
    }
  },
  "meshes": [
    { "id": "viking_room", "path": "Data/Models/viking_room.obj" },
    { "id": "monkey", "path": "Data/Models/monkey_smooth.obj" }
  ],
  "textures": [
    { "id": "viking_albedo", "path": "Data/Textures/viking_room.png" }
  ],
  "materials": [
    { "id": "lit_viking", "shader": "lit", "texture": "viking_albedo" }
  ],
  "entities": [
    { "mesh": "viking_room", "material": "lit_viking", "transform": [ ... ] },
    { "mesh": "monkey", "material": "lit_viking", "transform": [ ... ] }
  ]
}
```

Exact schema is an implementation detail; plan steps below lock order, not every field.

## Implementation phases

### Phase A — Scene description (no GPU behavior change)

- [ ] **A1** — Add `Data/Scenes/demo.json` mirroring current `Util_DemoAssets` set.
- [ ] **A2** — `LoadSceneDesc(path)` → in-memory `SceneDesc` (paths + entity refs only).
- [ ] **A3** — `CollectDependencies(SceneDesc)` → `AssetManifest`.
- [ ] **A4** — Wire `--scene`; default `Data/Scenes/demo.json`.

### Phase B — Manifest-driven startup (replaces hard-coded checks)

- [ ] **B1** — `VerifyManifest(manifest)` (same semantics as current `[STARTUP] OK/ERROR`).
- [ ] **B2** — `VulkanDesktop`: `LoadSceneDesc` → `VerifyManifest` → `Run()` / Vulkan.
- [ ] **B3** — Remove `Util_DemoAssets::kRequiredFiles`; keep demo paths only inside JSON until `Vk_Core` migrated.
- [ ] **B4** — Document in `startup-checks_Plan.md` archived note: superseded by manifest path.

### Phase C — Lifecycle + resource load (S2 + S1)

- [ ] **C1** — Application lifecycle: init → load scene desc → verify → Vulkan bootstrap → **load resources** → update/render → shutdown.
- [ ] **C2** — `LoadSceneResources`: populate mesh/texture tables from manifest; one load per unique path.
- [ ] **C3** — Remove `CreateMesh` / `CreateTexture` hard-coded paths from `Vk_Core::InitVulkan`.
- [ ] **C4** — Entities reference table ids; draw path uses indices (coordinate with S1 draw-stream tasks).

### Phase D — Scene change & policy (later)

- [ ] **D1** — `UnloadScene` + GPU lifetime rules (`DeletionQueue`, descriptor rebuild policy).
- [ ] **D2** — `engine.json` or scene flag: strict vs warn for missing optional assets.
- [ ] **D3** — Second tiny scene for load smoke tests (`Data/Scenes/smoke.json`).

## Files (expected touch list)

| Area | Paths |
|------|--------|
| Data | `Data/Scenes/demo.json`, optional `smoke.json` |
| Util / Gfx | `Util_SceneLoader.{h,cpp}` or `Gfx_SceneDesc.{h,cpp}`, `Util_AssetManifest.{h,cpp}` |
| App | `VulkanDesktop.cpp`, future `Application.{h,cpp}` |
| RenderCore | `Vk_Core.cpp` (peel init hacks), resource table headers |
| Docs | `Docs/scene-load_Progress.md`, `Docs/SprintPlan.md`, `README.md` (run with `--scene`) |
| Deprecate | `Util_DemoAssets.h` after Phase B/C |

## Anti-patterns

- Parsing scene JSON inside `Vk_Core`.
- Per-frame `LoadMesh(path)` — paths only at load time; hot path uses ids.
- Global hard-coded asset list in C++ for multi-scene builds.
- Skipping manifest verify before `vkCreateInstance`.

## Verification

1. Build Debug\|x64 after each phase that changes compile surface.
2. Run with default scene: same visual as today; log shows manifest paths then `[VULKAN]`.
3. Run with `--scene Data/Scenes/demo.json` from `x64\Debug` and from repo root via `--asset-root`.
4. Delete one manifest entry file: fail before Vulkan instance; clear `[STARTUP]` error with logical + resolved path.
5. Grep: no `UtilDemoAssets::kRequiredFiles` after Phase B; no demo mesh paths in `InitVulkan` after Phase C.

## Risks

- Hand-rolled JSON parser limits schema evolution — document fields or adopt a tiny library when schema grows.
- Descriptor/pipeline rebuild when materials change — must align with `Vk_DescriptorPolicy.h` (S2 layout task).
- Transition period: duplicate paths in JSON and `Vk_Core` until Phase C complete — keep phases short.

## Rollback

- Revert to `Util_DemoAssets` + `VerifyRequiredAssets` if scene loader blocks S1; manifest API can remain while demo JSON is optional.
