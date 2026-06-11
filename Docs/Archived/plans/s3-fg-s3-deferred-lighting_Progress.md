# Progress: s3-fg-s3-deferred-lighting

**Plan:** [`s3-fg-s3-deferred-lighting_Plan.md`](s3-fg-s3-deferred-lighting_Plan.md)  
**Branch:** `S3`

## Closeout — 2026-06-11

- **Outcome:** `Vk_DeferredLightingPass` replaces `CompositeAlbedo` on hybrid hot path; fullscreen resolve reads G-buffer + cluster SSBO; diffuse + ambient (sun stub, depth slice 0).
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0 (ForwardLit). GfxTests `TestClusterIndexFromTile`.
- **Deviations:** no specular; bindless/multiview/transparent limitations unchanged from slice 1.
