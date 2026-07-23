# Progress — Gfx/Rhi ownership completion

**Plan:** [`gfx-rhi-ownership-completion_Plan.md`](gfx-rhi-ownership-completion_Plan.md)  
**Started:** 2026-07-23

## 2026-07-23 — O3 DepthPyramid Init + O4 Record peel + graphics create
- **DepthPyramid Init in Gfx:** `Gfx_DepthPyramidPass::{CreatePipeline,CreateOrRecreateImage,Destroy*}`; RC thin SPIR-V load + Vk mirrors for SSR/deferred.
- **Deleted Record facades:** DepthPyramid, ClusterBuild, AO, Soft, SSR — FG/pass TUs call Gfx via `Vk_FrameCmd`.
- **Rhi graphics create:** `DeviceCreateRenderPass` / `Framebuffer` / `GraphicsPipeline`; Post hybrid RP/FB + tonemap PSO use them.
- **Still pending:** Post/Shadow/Deferred Record peel; more Init moves (O3/O5).
- **Verification:** `Verify-CI` PASSED; `Verify-Smoke` PASSED; sponza `--validation` exit 0 (known benign noise only if present).

## 2026-07-23 — kickoff
- **Files:** Progress created; README Active now → this WIP
- **Next:** O1 FG Begin/End peel; O2 Rhi create surface; O3/O4 pilot then fan-out
- **Verification:** pending

## 2026-07-23 — Hi-Z correctness fix (not occlusion cull)
- **What it is:** Min-depth pyramid for **SSR** + debug; **S16** is GPU occlusion cull (future).
- **Bugs fixed:** (1) mip0 only wrote half-res into full-res image → garbage UV sampling; now mip0 = full-res depth copy. (2) Sample view was single-mip → `textureLod`/`Hi-Z` debug broken; now full mip-chain view. (3) Debug remaps ZO depth for visibility; ImGui tooltip clarifies role.
- **Files:** `DepthPyramid.comp`, `Gfx_DepthPyramidPass.cpp`, `Vk_DepthPyramidPass.cpp`, `DeferredLighting.frag`, `Util_RenderDebugPanel.cpp`, regenerated `.spv`
- **Verification:** build OK; `Verify-Smoke.ps1` PASSED; sponza `--validation` exit 0 (known `enabledLayerCount` only). `Verify-CI` will pass once regenerated `.spv` are committed.