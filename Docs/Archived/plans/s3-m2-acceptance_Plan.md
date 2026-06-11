# Plan: s3-m2-acceptance — M2 sign-off (GPU indirect path)

**Status:** Closed (2026-06-11)  
**Parent:** [`Active-Plan.md`](../../Active-Plan.md) § S3 (first line)  
**Depends on:** P2–P3 M2 geometry (archived), **G1** met *(2026-06-10)*  
**Next after close:** [`s3-fg-v0_Plan.md`](../../s3-fg-v0_Plan.md) step 1

## Problem

P3 delivered GPU frustum cull → slot-indexed indirect record (`--gpu-cull`) and GfxTests CPU/GPU parity (**G1**). The **S3 M2 acceptance** line in Active-Plan is still open: we need automated dogfood evidence and a documented sign-off against [`SprintOutcomeValidation.md`](../../SprintOutcomeValidation.md) §S3 before FG v0 work.

## Goal

1. **`--gpu-cull` dogfood:** stress smoke runs the GPU cull path and asserts expected log tokens (not manual-only).
2. **G1 in CI:** confirm `Verify-CI.ps1` / GfxTests parity cases remain required (no regression).
3. **No per-object direct draw on gpu path:** gpu-cull record uses slot `vkCmdDrawIndexedIndirect` only — no `vkCmdDrawIndexed` when `--gpu-cull` and not `--legacy-direct-draw` (P3 outcome; add grep/audit guard).
4. **Sign-off:** update §S3 validation with commands + log evidence; archive Active-Plan line.

## Non-goals

- FG v0 / `HybridDeferred` preset ([`s3-fg-v0_Plan.md`](../../s3-fg-v0_Plan.md))
- GPU compaction, multi-draw-indirect batching (Wishlist)
- `engine.json` `features.gpuCull` unless dogfood needs config (CLI extension to smoke is enough for v0)
- `EngineArchitecture.md` policy change (P3 already documents gpu path; sync rule)

## Touch list

| Area | Files (expected) |
|------|------------------|
| Smoke | `Scripts/Verify-Smoke.ps1`, `Scripts/Assert-SmokeLog.ps1` (or sibling assert) |
| CI | `.github/workflows/vulkan-desktop.yml` *(only if smoke contract changes)* |
| Audit | `VulkanDesktop/RenderCore/Vk_ScenePasses.cpp` (confirm gpu path) |
| GfxTests | `GfxTests_Main.cpp` *(optional static guard test)* |
| Docs | `SprintOutcomeValidation.md` §S3, `CLI.md` *(gpu-cull smoke note)* |
| Close | `Active-Plan.md`, `Archived-Plan.md`, `README.md` |

## Ordered steps

1. **Baseline audit:** grep gpu path — `gpuCullRecord` → `vkCmdDrawIndexedIndirect` only; document current log tokens (`gpuCull=true`, `gpu-deferred`, `GPU cull dispatch`, `GPU-filled slot indirect`).
2. **Smoke dogfood:** extend `Verify-Smoke.ps1` with a second pass (or `-GpuCull` switch) that appends `--gpu-cull` to the stress run; keep default single-pass behavior optional for fast local runs if we add a switch — **CI must run gpu-cull pass**.
3. **Log contract:** extend `Assert-SmokeLog.ps1` (or `-RequiredTokens` param) to require gpu-cull tokens when that pass runs.
4. **G1 check:** verify `Verify-CI.ps1` still runs GfxTests parity (`cpu/gpu cull parity` cases); no code change expected.
5. **§S3 evidence:** add runbook block to `SprintOutcomeValidation.md` (commands, required log lines, link to this plan closeout).
6. **Close:** Progress closeout → archive Plan+Progress; move Active-Plan M2 line → `Archived-Plan.md`; clear README **Active now** WIP pointer (or point at FG v0 kickoff if user starts next task).

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0 (GfxTests G1 parity)
- `powershell -File Scripts/Verify-Smoke.ps1` exit 0 **including** gpu-cull dogfood pass
- Log (gpu-cull pass): `gpuCull=true`, `CULL … (gpu-deferred)`, `GPU cull dispatch`, `Scene record using GPU-filled slot indirect`
- Manual once (optional): RenderDoc capture with `--gpu-cull` — indirect buffer driven draws visible

## Risks

- **Stress scene + gpu cull:** ~108 entities; smoke time may grow slightly — acceptable for acceptance.
- **CI duration:** second smoke invocation adds ~6s; document in closeout.
- **LOD + gpu cull:** entity-record LOD uses primary-view eye (P3 close note) — stress `lodEnabled: true` is the right dogfood scene.

## References

- Validation criteria: [`SprintOutcomeValidation.md`](../../SprintOutcomeValidation.md#validation-s3)
- P3 implementation: [`render-m2-p3-c_Plan.md`](render-m2-p3-c_Plan.md), [`render-m2-p3-g1_Plan.md`](render-m2-p3-g1_Plan.md)
- Architecture diagram: [`EngineArchitecture.md`](../../EngineArchitecture.md) §4 gpu indirect branch
