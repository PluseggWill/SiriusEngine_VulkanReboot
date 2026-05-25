# resource-tables — GPU resource tables (S1)

**Status:** Done (2026-05-26). Follow-ups: instance slab + Set 1 batching (S1); `scene-load` Phase C manifest (S2). Per-draw `model` push constant documented in `EngineArchitecture.md` §5.3 and `resource-tables_Progress.md`.

## Problem statement and goals

SprintPlan S1 data-plane: **mesh/material → GPU resource indices**; **`Gfx_DrawInstance`** already stores table ids only. Populate tables from a **manifest** (scene JSON in `scene-load` Phase C; **demo manifest v0** mirrors `UtilDemoAssets` until then).

**Goals**

- `Gfx_ResourceManifest` — CPU-only path list (no Vulkan); `Gfx_BuildDemoResourceManifest`.
- `Vk_ResourceTables` — dense tables: mesh buffers, material → pipeline + texture id, texture image views.
- `Vk_Core` loads via manifest; removes ad-hoc `myMeshMap` / path globals.
- `CreateDescriptorSets` resolves texture through material → texture table.
- `RecordScenePass` resolves mesh/material from `myExtractResult` draw ids (multi-mesh draw, same pipeline v0).

## Non-goals

- Scene JSON parser / `LoadSceneDesc` (scene-load Phase A–C).
- Set 1 per-material descriptor sets, instance slab, sort/batch (separate S1 tasks).
- Moving tables out of `Vk_Core` into a standalone module (incremental peel stays S2).

## Design decisions

| Topic | Choice |
|-------|--------|
| Manifest home | `Gfx/Gfx_ResourceManifest.h` — Gfx module, no Vulkan |
| GPU tables home | `RenderCore/Vk_ResourceTables.h/.cpp` |
| Indexing | Dense `uint32_t` id = vector index; manifest entries carry explicit id |
| Descriptor v0 | Frame Set 0 texture binding uses `materialId → textureId → Gfx_Texture` |
| Demo content | Same paths as `UtilDemoAssets`; SoA entities keep mesh 0/1, material 0 |

## File touch list

| Path | Action |
|------|--------|
| `VulkanDesktop/Gfx/Gfx_ResourceManifest.h` | Manifest types + demo builder |
| `VulkanDesktop/RenderCore/Vk_ResourceTables.h` | Table API + entries |
| `VulkanDesktop/RenderCore/Vk_ResourceTables.cpp` | Load + resolve |
| `VulkanDesktop/RenderCore/Vk_Core.h` / `.cpp` | Use tables; peel maps |
| `VulkanDesktop/VulkanDesktop.vcxproj` / `.filters` | New compile entries |
| `Docs/resource-tables_Progress.md` | Progress log |
| `Docs/SprintPlan.md` | Archive completed line |
| `Docs/EngineArchitecture.md` | §4.2 incremental note |

## Implementation plan

1. Add `Gfx_ResourceManifest` + `Gfx_BuildDemoResourceManifest`.
2. Add `Vk_ResourceTables` (load from manifest, GetMesh/Material/Texture).
3. Refactor `Vk_Core::InitVulkan` — manifest → tables; remove path globals and maps.
4. Wire descriptors + `RecordScenePass` through table resolve.
5. vcxproj + docs archive + Architecture sync.
6. Build Debug\|x64 + smoke-run; log `[RESOURCE-TABLE]` and `[EXTRACT]`.

## Build / smoke-run

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
& "<MSBuild>" VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m
& "x64\Debug\VulkanDesktop.exe" --help
# minimized 4s run from x64\Debug
Get-Content Logs\engine_runtime_log.txt -Tail 50
```

**Success:** build exit 0; smoke-run no crash; `[RESOURCE-TABLE] meshes=2 materials=1 textures=1`; `[EXTRACT] draws=2`; `[RESOURCE]` load lines for both meshes.

## Risks

- `Gfx_Mesh::BuildBuffers` still uses `Vk_Core::GetInstance()` — unchanged this task.
- Full scene manifest swap is Phase C — demo builder must stay aligned with `UtilDemoAssets`.
