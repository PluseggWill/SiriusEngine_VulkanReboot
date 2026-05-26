# draw-extract — Extract phase + DrawInstance (S1)

## Problem statement and goals

Implement the S1 **Extract** boundary from `EngineArchitecture.md` §4.3 / §5.1 step 0: convert scene entities into flat **`Gfx_DrawInstance`** records and **visible entity indices** without any Vulkan calls.

**Goals**

- Add `Gfx_DrawInstance` with sort key, mesh id, material id, instance data offset, pipeline permutation id.
- Add `Gfx_ExtractDrawInstances` in `Gfx/` (no Vulkan includes).
- Wire `Vk_Core::DrawFrame` to build a minimal demo entity list, run extract each frame, and log counts (record path unchanged until batch/cull tasks land).

## Non-goals

- SoA columns, slot maps, LOD, transparency split, frustum cull (separate S1 submission task).
- Set 1/2 descriptors, push constants, multi-mesh record.
- Deleting `DrawObjects` (optional hygiene; not required for extract acceptance).

## Design decisions

| Topic | Choice | Alternatives |
|-------|--------|--------------|
| Module home | `Gfx/Gfx_DrawExtract.h` + `.cpp` | `Vk_Core` — rejected; extract must not include Vulkan |
| Entity input v0 | `Gfx_SceneEntity` plain struct (index + generation, mesh/material ids, `glm::mat4`) | Full SoA — deferred |
| Visibility v0 | All entities visible (no frustum test) | Inline frustum — deferred to cull task |
| Sort key | Packed `uint64_t` — perm (16) \| material (16) \| mesh (16) \| depth bucket (16) | Separate fields only — worse for sort |
| Depth bucket | Eye-space Z quantized to 16 bits (farther = larger bucket for opaque front-to-back tie) | Float sort — deferred |
| Output | `Gfx_ExtractResult`: `visibleEntityIndices` + `drawInstances` | Single list — split transparent later |

## File touch list

| Path | Action |
|------|--------|
| `VulkanDesktop/Gfx/Gfx_DrawExtract.h` | Add types + extract API |
| `VulkanDesktop/Gfx/Gfx_DrawExtract.cpp` | Implement extract + sort key pack |
| `VulkanDesktop/RenderCore/Vk_Core.h` | Scene entities + extract result members; remove `DrawObjects` decl |
| `VulkanDesktop/RenderCore/Vk_Core.cpp` | Init demo entities; extract in `DrawFrame`; delete `DrawObjects` |
| `VulkanDesktop/VulkanDesktop.vcxproj` | Compile/include new Gfx files |
| `VulkanDesktop/VulkanDesktop.vcxproj.filters` | Filter entries |
| `Docs/draw-extract_Progress.md` | Progress log |
| `Docs/SprintPlan.md` | Archive completed extract line |
| `Docs/EngineArchitecture.md` | §9 incremental alignment note |

## Implementation plan

- [x] 1. Add `Gfx_DrawExtract.h` — `Gfx_DrawInstance`, `Gfx_ExtractResult`, `Gfx_PackOpaqueSortKey`, `Gfx_ExtractDrawInstances` (view params: later unified as **`Gfx_CullViewParams`** in draw-cull-sort).
- [x] 2. Add `Gfx_DrawExtract.cpp` — extract loop, eye-space depth bucket, emit draw instances.
- [x] 3. Update `Vk_Core` — demo entities after mesh init; call extract before `RecordScenePass`; periodic `[EXTRACT]` log.
- [x] 4. Remove dead `DrawObjects` stub; document `RecordScenePass` as live Vulkan path until batch record.
- [x] 5. Update `.vcxproj` / `.filters`.
- [x] 6. Build Debug\|x64 + smoke-run; tail runtime log for `[EXTRACT]`.

## Build / smoke-run

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
& "<MSBuild>" VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m
& "x64\Debug\VulkanDesktop.exe" --help
# minimized 4s window run from x64\Debug
Get-Content Logs\engine_runtime_log.txt -Tail 40
```

**Success:** build exit 0; smoke-run no crash; log shows `[EXTRACT] entities=N draws=M` with N≥2, M≥2 on demo scene.

## Risks

- None blocking; record path still single-mesh until submission tasks land.
