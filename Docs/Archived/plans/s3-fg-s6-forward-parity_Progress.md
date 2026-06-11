# Progress: s3-fg-s6-forward-parity

**Plan:** [`s3-fg-s6-forward-parity_Plan.md`](s3-fg-s6-forward-parity_Plan.md)  
**Branch:** `S3`

## Closeout — 2026-06-11

- **Outcome:** DeferredLighting samples G-buffer depth, reconstructs world position, adds Blinn-Phong specular (fogDistances.xy) per sun light — opaque hybrid closer to ForwardLit.
- **Verification:** `Verify-CI.ps1` + `Verify-Smoke.ps1` exit 0.
- **Deviations:** visual parity checklist / perf A-B deferred to hybrid epic §D; not full PBR.
