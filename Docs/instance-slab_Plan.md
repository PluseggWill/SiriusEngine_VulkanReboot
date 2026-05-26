# Plan: instance-slab

**Sprint:** S1 — CPU draw stream  
**Status:** Done (2026-05-26)  
**SprintPlan:** Per-frame instance slab (ring UBO); no per-object heap allocs on hot path.

## Problem

`RecordScenePass` reads `Gfx_SceneSoA::GetTransform(myEntityIndex)` — violates render-backend boundary (§4.3). Extract already emits `Gfx_DrawInstance.myInstanceDataOffset`, but it used raw `slot * sizeof(mat4)` without UBO alignment and nothing backs the offset on GPU.

`Vk_FrameData::myObjectBuffer` / `myObjectDescriptor` exist as stubs; Set 2 `UNIFORM_BUFFER_DYNAMIC` is a **separate** follow-up task.

## Goals

1. Per in-flight frame: one **CPU-mapped instance ring UBO** with stride `PadUniformBufferSize(sizeof(GpuObjectData))`.
2. After extract → cull → sort: **`FillInstanceSlab`** writes each visible draw’s `model` (demo spin composed here) and assigns **`myInstanceDataOffset`** sequentially — no `new`/`malloc` on hot path.
3. **`RecordScenePass`** reads `mat4 model` from slab via `myInstanceDataOffset` only (no SoA).
4. Log once: slab capacity, stride, draw count.

## Non-goals

- Set 2 descriptor layout, `vkCmdBindDescriptorSets` dynamic offsets, shader UBO for model (next task: verify descriptor policy Set 2).
- Batch record, Set 1 per material.
- Moving demo spin out of `Vk_Core` (gameplay/sim owns transforms later).

## Design

| Item | Choice |
|------|--------|
| Struct | `GpuObjectData { alignas(16) glm::mat4 model; }` — std140, matches future Set 2 |
| Capacity | `kMaxInstanceSlabEntries = 256` in `Vk_DescriptorPolicy.h` |
| Buffer | `VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT`, `VMA_MEMORY_USAGE_CPU_ONLY`, persistently mapped |
| Offset assignment | `FillInstanceSlab` after sort: `offset = index * stride` |
| Record | `reinterpret_cast<const GpuObjectData*>(slabPtr + offset)->model` → push constant (unchanged shader) |

## Files

| File | Change |
|------|--------|
| `VulkanDesktop/RenderCore/Vk_DescriptorPolicy.h` | `kMaxInstanceSlabEntries` |
| `VulkanDesktop/RenderCore/Vk_Camera.h` | `GpuObjectData` |
| `VulkanDesktop/RenderCore/Vk_FrameData.h` | `myInstanceSlabMapped`, comments |
| `VulkanDesktop/RenderCore/Vk_Core.h` | `CreateInstanceSlabs`, `FillInstanceSlab`, `InstanceSlabStride` |
| `VulkanDesktop/RenderCore/Vk_Core.cpp` | create/fill/record |
| `VulkanDesktop/Gfx/Gfx_DrawExtract.cpp` | offset assigned in fill, not extract |
| `Docs/EngineArchitecture.md` | §4.2/§3.1 instance slab done |
| `Docs/SprintPlan.md` | archive line + S1 notes |

## Steps

- [ ] P1 — `GpuObjectData`, capacity constant, `Vk_FrameData` mapped pointer
- [ ] P2 — `CreateInstanceSlabs` in init; deletion queue cleanup
- [ ] P3 — `FillInstanceSlab` + call from `DrawFrame` after sort
- [ ] P4 — `RecordScenePass` reads slab; remove SoA in record
- [ ] P5 — Extract: `myInstanceDataOffset = 0` until fill
- [ ] P6 — Build Debug\|x64 + smoke-run + log check; archive sprint item

## Build / smoke-run

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
& "<MSBuild>" VulkanDesktop.sln /p:Configuration=Debug /p:Platform=x64 /v:m
Set-Location x64\Debug; $p = Start-Process -FilePath ".\VulkanDesktop.exe" -PassThru -WindowStyle Minimized; Start-Sleep 4; if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
```

Expect: build 0; `[EXTRACT]`/`[CULL]` unchanged; no `[ERROR]` during init; two meshes visible with spin.

## Risks

- Draw count > 256: clamp + log error once (demo has 2 entities).
- Persistent map: ensure no use-after-free on shutdown (deletion queue destroys buffer).
