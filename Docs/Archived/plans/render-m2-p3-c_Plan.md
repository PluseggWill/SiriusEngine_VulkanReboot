# Plan: render-m2-p3-c — GPU indirect record path

**Status:** Closed (2026-06-10)  
**Parent:** [`Active-Plan.md`](Active-Plan.md) **P3** (line 1: GPU indirect record)  
**Depends on:** [`Archived/plans/render-m2-p3-b_Plan.md`](Archived/plans/render-m2-p3-b_Plan.md) (compute cull → slot indirect)

## Goal

When `--gpu-cull` is on, **scene record** consumes `myGpuCullIndirectBuffer` indexed by **entity SoA slot** (view partition matches `outputBaseSlot`). Skip CPU frustum cull so GPU `instanceCount=0` drives visibility. Default path (`gpuCull` off) unchanged.

## Non-goals

- Compaction / multi-draw indirect (optional P3 line)
- G1 automated parity test
- LOD GPU subset parity
- `EngineArchitecture.md` policy change

## Touch list

| Area | Files |
|------|--------|
| Gfx stream | `Gfx_FrameDrawStream.h/.cpp`, `Gfx_RenderPacket.h` |
| Frame prep | `Vk_FrameDrawPrep.h`, `Vk_Core.cpp` |
| Record | `Vk_ScenePasses.cpp` |
| Tests | `GfxTests_Main.cpp` (slot helper) |

## Steps

1. `Gfx_ComputeEntityIndirectSlot(viewBase, entitySlot)` — same partition as `outputBaseSlot + slot`.
2. `Gfx_BuildFrameDrawStream` — skip `Gfx_CullDrawInstancesInPlace` when `myGpuCullEnabled`.
3. `PrepareFrameCpu` — pass `GetGpuCullEnabled()` into draw prep.
4. `RecordScene` — when gpu cull + indirect: bind `myGpuCullIndirectBuffer`, offset by entity slot; log once.
5. GfxTests: entity indirect slot matches cull push `outputBaseSlot + entity`.

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0
- `powershell -File Scripts/Verify-Smoke.ps1` exit 0 (gpu cull off)
- Manual: `--gpu-cull` → log `GPU indirect record` + prior `GPU cull dispatch`

## Risks

- LOD + gpu cull mesh mismatch deferred to LOD parity line.
- Per-draw `vkCmdDrawIndexedIndirect` remains (compaction later).
