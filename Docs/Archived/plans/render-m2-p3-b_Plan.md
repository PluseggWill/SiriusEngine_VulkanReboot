# Plan: render-m2-p3-b — compute cull → indirect buffer

**Status:** Closed (2026-06-10)  
**Parent:** [`Active-Plan.md`](Active-Plan.md) **P3** (line 2)  
**Depends on:** [`Archived/plans/render-m2-p3-a_Plan.md`](Archived/plans/render-m2-p3-a_Plan.md) (entity-record SSBO)

## Goal

Add a **compute frustum-cull** pass that reads `Gfx_EntityGpuRecord[]` and writes **per-slot** `VkDrawIndexedIndirectCommand` (instanceCount = 0 when culled). CPU extract/cull/record path stays default; `--gpu-cull` enables dispatch for dogfood / future parity (G1).

## Non-goals

- Switch record to GPU-filled compact list (next P3 line)
- Automated CPU vs GPU parity (G1 task)
- Compaction pass
- Bindless / graphics descriptor changes

## Touch list

| Area | Files |
|------|--------|
| Shader | `Shader/EntityCull.comp`, `CompileShader_Glslc.bat` |
| Gfx contract | `Gfx_GpuCull.h` (push constants, frustum parity note) |
| RenderCore | `Vk_GpuCull.h/.cpp`, `Vk_FrameData.h`, `Vk_Core.cpp`, `Vk_ScenePasses.cpp` |
| Config | `Util_EngineConfig.*`, `Config/engine.json` |
| Build | `VulkanDesktop.vcxproj` (+ filters) |

## Steps

1. `EntityCull.comp` — frustum + layer mask; mirror `Gfx_BuildFrustumFromViewProj` / `Gfx_IsBoundsVisible`.
2. `Vk_GpuCull` — compute pipeline, per-frame SSBO bindings (entity in, slot-indirect out, atomic visible count).
3. `myGpuCullIndirectBuffer` per frame; partition by active view count (same policy as draw buffers).
4. `PrepareFrameCpu` stash viewProj + layer mask per view; `DrawFrameGpu` dispatch when `gpuCullEnabled`.
5. CLI `--gpu-cull` / `--no-gpu-cull`; default **off** (CI unchanged).

## Verification

- `Verify-CI.ps1` exit 0 (incl. shader drift for `EntityCull.spv`)
- `Verify-Smoke.ps1` exit 0 (gpu cull off)
- Manual: `--gpu-cull` → log `GPU cull dispatch` + `visibleSlots=`

## Risks

- Multi-view partition must match draw-buffer policy — document in `Gfx_GpuCull.h`.
