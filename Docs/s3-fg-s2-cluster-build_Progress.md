# Progress: s3-fg-s2-cluster-build

**Plan:** [`s3-fg-s2-cluster-build_Plan.md`](s3-fg-s2-cluster-build_Plan.md)  
**Branch:** `S3`

## 2026-06-11 — Kickoff

- **Scope:** Slice 2 — `ClusterBuild.comp` stub + `Vk_ClusterBuildPass`; record between G-buffer and composite; sun-only light list.
- **Next:** Steps 1–5 implementation.

## 2026-06-11 — Steps 1–5 implementation

- **Files:** `Gfx_ClusterLighting.h`, `Vk_ClusterBuildPass.*`, `ClusterBuild.comp` + `.spv`, `Vk_GBufferPass.cpp`, `Vk_Core.*`, `Vk_SwapchainHost.cpp`, GfxTests `TestClusterGridCount`.
- **Record:** `GBufferOpaque → ClusterBuild → CompositeAlbedo`; `[FG]` + `[CLUSTER]` once logs.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0 (ForwardLit default).

## 2026-06-11 — Review polish

- **Lifecycle:** resize waits idle before cluster-list buffer recreate; descriptor sets kept across extent rebuild.
- **Layout:** `static_assert` on Gfx/GPU struct sizes; shader `kMaxLightsPerCluster` named constant.
- **Cleanup:** removed unused `IsActive`, duplicate sun-light write, redundant lazy Init in `RecordFrame`.
