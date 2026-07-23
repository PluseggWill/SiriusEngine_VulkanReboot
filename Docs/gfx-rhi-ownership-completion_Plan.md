# Plan — Gfx/Rhi ownership completion (retire facades + Init)

**Status:** Open (roadmap) — **queue #1** (2026-07-22)  
**Related:** Closed [`gfx-rhi-pass-migration_Plan.md`](Archived/plans/gfx-rhi-pass-migration_Plan.md) (E0–E5 Records) · [`EngineArchitecture.md`](EngineArchitecture.md) · Wishlist S21 · [`Active-Plan.md`](Active-Plan.md)

## Problem

E0–E5 moved HybridDeferred **Records** into `Gfx_*Pass` via `Rhi_CommandList`. RenderCore still owns:

- Thin `Vk_*_Record.cpp` adopt / descriptor / DTO facades
- `Vk_*Pass.cpp` **Init** (pipelines, images, RP/FB, descriptor layouts)
- `Vk_FrameGraph` GBuffer/hybrid **Begin/End** (former E4.6f)

Goal: finish ownership so Gfx records **and** creates through Rhi; delete per-pass `Vk_*_Record` / pass Init TUs (RenderCore keeps `Vk_RhiBackend` + WSI/submit only).

## Non-goals

- Second RHI backend (D3D12/Metal)
- Bindless layout codegen / lab presets / WSI-D (stay S21 lab)
- Deleting `Vk_Renderer` frame submit / swapchain host
- Rewriting Forward path topology in the same slices as HybridDeferred peel

## Ordered phases

| Phase | Focus | Exit |
|-------|--------|------|
| **O1** | Peel FG GBuffer/hybrid Begin/End into Gfx plan executor (former E4.6f) | Smoke + G0-validation; FG no longer owns those Begin/End — **Done 2026-07-23** |
| **O2** | Expand Rhi **create** surface: compute + graphics RP/FB/PSO + buffer descriptor update/map | GfxTests + Verify-CI — **Done 2026-07-23** |
| **O3** | Move pass **Init** into `Gfx_*Pass` (Rhi-only); RC keeps backend map | DepthPyramid **full**; ClusterBuild **full**; Soft/SSR **pipeline**; AO/Post/Shadow/Deferred Init pending |
| **O4** | Delete `Vk_*_Record.cpp` facades; FG/executor calls Gfx with Rhi handles + plain DTOs | **Done 2026-07-23** — all Record TUs deleted (Post/Shadow/Deferred+DDGI included) |
| **O5** | Retire empty/`Init`-only `Vk_*Pass.cpp` shells; state lives on Gfx or RC resource tables as designed | Pending (RC still hosts image create + descriptor writes for Soft/SSR/AO/…) |

**Pilot order (suggested):** O1 first (unblocks clean Record scope) → O2 foundation → O3/O4 on one compute pass (e.g. DepthPyramid or AO) → fan out → O5.

## Touch list (expected)

- `Rhi/` + `Vk_RhiBackend` (create/update APIs)
- `Gfx_*Pass` Init + Record
- `Vk_FrameGraph` / plan executor
- `Vk_*_Record.cpp` → delete in O4
- `Vk_*Pass.cpp` → shrink then delete in O3/O5
- `EngineArchitecture.md` migration note on O5 (locked boundary if Init ownership moves)

## Verification

| Phase | Bar |
|-------|-----|
| O1–O5 | `Scripts/Verify-CI.ps1` |
| O1, O3–O4 (pass/barrier/RP) | `Verify-Smoke.ps1` + G0-validation on sponza when GPU available |
| O2 | GfxTests for new Rhi create surface |

## Risks

- Descriptor update ownership vs bindless policy — consult archived shader-bindless-policy before changing set layouts
- Swapchain tonemap FB index must stay correct when facade disappears
- Dual Forward + HybridDeferred paths: do not break Forward while peeling HybridDeferred FG

## Kickoff

On vibe start: add `_Progress.md`, set README **Active now**, implement **O1** first unless replan.
