# Progress: s3-fg-s5-bindless-hybrid

**Plan:** [`s3-fg-s5-bindless-hybrid_Plan.md`](s3-fg-s5-bindless-hybrid_Plan.md)  
**Branch:** `S3`

## Closeout — 2026-06-11

- **Outcome:** `GBufferFrag_Bindless.frag` + `myGBufferPipelineBindless`; hybrid chain runs on default bindless path (no ForwardLit fallback). `RecordOpaque/TransparentPacketDraws` keyed by `Vk_RenderMaterialPath`.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0. Manual `--render-preset HybridDeferred` + `materialPath=Bindless` → hybrid chain log, no fallback.
- **Deviations:** none.
