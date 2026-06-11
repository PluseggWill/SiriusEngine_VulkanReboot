# Plan: s3-fg-v0 — Frame graph v0 (Lighting Stage 2 entry)

**Status:** Planned *(roadmap — kickoff adds `{name}_Progress.md`)*  
**Parent:** [`Active-Plan.md`](Active-Plan.md) § S3 · epic [`hybrid-deferred-epic_Plan.md`](hybrid-deferred-epic_Plan.md) §A  
**Depends on:** P2–P3 M2 geometry (archived), S2 permutation/cache (archived), **G1** met

## Goal

Spike **minimal hybrid-deferred topology** without full S7 frame-graph infra:

`GBufferOpaque -> ClusterBuild -> DeferredLighting` on the **opaque** path; keep `ForwardLit` preset as default fallback.

## Non-goals

- Full `FrameGraphBuilder` / transient RT pool (S7)
- Transparent deferred; `ForwardTransparent` wiring (S7)
- Full PBR / IBL parity (hybrid epic §C)
- DDGI (Stage 3)
- `EngineArchitecture.md` policy change unless descriptor/pass contract shifts (sync rule)

## Touch list

| Area | Files (expected) |
|------|------------------|
| Pass topology | `Vk_ScenePasses.cpp`, new `Vk_*Pass` modules or FG v0 builder |
| Shaders | `Shader/GBuffer*.vert/frag`, `ClusterBuild.comp`, `DeferredLighting*.frag` |
| Preset | `Util_EngineConfig.*`, `Config/engine.json`, permutation registry |
| Gfx contract | opaque draw stream unchanged as geometry source |
| Docs | `hybrid-deferred-epic_Plan.md` cross-links only |

## Ordered steps

1. **Preset hook:** `HybridDeferred` preset opt-in; default remains `ForwardLit`.
2. **GBufferOpaque:** raster opaque from existing `Gfx_FrameRenderPacket` / indirect path; lock format policy (document in plan Progress, not Architecture unless policy lock).
3. **ClusterBuild:** compute pass stub — cluster grid + light index lists (minimal light count).
4. **DeferredLighting:** fullscreen/cluster resolve → swapchain or HDR intermediate; bind G-buffer + cluster SSBO.
5. **Record order:** insert chain before present; `ForwardLit` path unchanged when preset off.
6. **Smoke:** `Verify-Smoke.ps1` still green on `ForwardLit`; manual `HybridDeferred` log once.

## Verification

- `powershell -File Scripts/Verify-CI.ps1` exit 0 (`ForwardLit` default)
- `powershell -File Scripts/Verify-Smoke.ps1` exit 0
- Manual: `--render-preset HybridDeferred` (or equivalent) → log pass chain once; validation-clean frame

## Risks

- Bindless Set 1 contract — follow [`shader-bindless-policy_Plan.md`](Archived/plans/shader-bindless-policy_Plan.md) §Maintenance contract
- GPU cull (`--gpu-cull`) optional; FG v0 v1 may use CPU indirect path first
- Resize / swapchain: reuse RHI-B2 recreate; no maintenance1 (Wishlist S7)

## References

- Validation: [`SprintOutcomeValidation.md`](SprintOutcomeValidation.md) §S3 (M2) + future Stage 2 gate in S7
- Architecture pass diagram: [`EngineArchitecture.md`](EngineArchitecture.md) §7
