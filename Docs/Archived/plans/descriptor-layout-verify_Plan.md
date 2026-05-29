# Plan: descriptor-layout-verify

**Sprint:** S2 — Engine layering & hygiene  
**Status:** Closed (2026-05-29)  
**Active-Plan:** Verify `VkPipelineLayout` Set 0/1/2 vs `Vk_DescriptorPolicy.h`; document material rebuild path.

## Problem

Set indices and bindings are spread across `Vk_DescriptorPolicy.h`, `Vk_Enum.h`, `Vk_DescriptorSystem.cpp`, shaders, and two pipeline layouts (batch vs bindless). Stale comments (e.g. Set 1 “placeholder”) and no single rebuild contract block scene-load / swapchain work.

## Goals

1. **Audit:** Confirm batch and bindless `VkPipelineLayout` use sets `0/1/2` in policy order with bindings matching `TriangleVertex.vert`, `TriangleFrag_Lit.frag`, `TriangleFrag_Lit_Bindless.frag`.
2. **Runtime signal:** One `[DESCRIPTOR]` log per run summarizing layout contract (path, set count, key bindings).
3. **Document rebuild:** When material descriptors/pipelines are created, refreshed, or require full scene reload.
4. **Hygiene:** Fix outdated `Vk_Enum.h` comment; bindless record uses `myBindlessPipelineLayout` for Set 0 (layout handle consistency).

## Non-goals

- Shader reflection / auto layout codegen (`shader-reflection` task).
- Hot-reload material sets without `UnloadScene`.
- Changing descriptor types or adding Set 3.

## Design

| Path | `pSetLayouts` order | Set 1 layout |
|------|---------------------|--------------|
| Batch | `global`, `material`, `object` | Per-material sets (`COMBINED_IMAGE_SAMPLER` + `UNIFORM_BUFFER` alpha) |
| Bindless | `global`, `bindlessMaterial`, `object` | One set: texture array + material SSBO |

**Material rebuild contract (document only):**

| Event | Action |
|-------|--------|
| Device init | `InitDeviceLayouts` → create set layouts (+ bindless layout if enabled) |
| `LoadSceneResources` | `InitSceneDescriptors` → pool, frame/object sets, `CreateMaterialDescriptorSets` or bindless writes |
| Swapchain recreate | `RefreshMaterialPipelinesAfterSwapchainRecreate` → `Gfx_Material` pipeline handles only |
| Material count / texture / bindless table change | `UnloadScene` → `LoadSceneResources` (no partial descriptor rebuild today) |

## Touch list

| File | Change |
|------|--------|
| `VulkanDesktop/RenderCore/Vk_DescriptorPolicy.h` | Rebuild contract comment block |
| `VulkanDesktop/RenderCore/Vk_Enum.h` | Fix Set 1 comment |
| `VulkanDesktop/RenderCore/Vk_DescriptorSystem.{h,cpp}` | `LogLayoutContract()` after layouts |
| `VulkanDesktop/RenderCore/Vk_ScenePasses.cpp` | Set 0 bind uses correct pipeline layout on bindless path |
| `Docs/EngineArchitecture.md` | §5.3 material rebuild bullet |

## Steps

- [x] P1 — Code audit table in Progress; fix stale comments
- [x] P2 — `LogLayoutContract` + call from `InitDeviceLayouts`
- [x] P3 — Rebuild contract in policy + Architecture §5.3
- [x] P4 — Bindless Set 0 layout handle fix in `RecordScene`
- [x] P5 — Build + smoke (`--no-validation --smoke-frames 2`)
- [x] P6 — Closeout: archive plan, move Active-Plan line

## Build / smoke-run

```powershell
& "<MSBuild>" VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m
Set-Location x64\Debug
.\VulkanDesktop.exe --no-validation --smoke-frames 2
```

**Success:** exit 0; log contains `[DESCRIPTOR] layout contract:` and `[SCENE] UnloadScene`.
