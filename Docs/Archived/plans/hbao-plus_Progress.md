# Progress: hbao-plus

## Closeout — 2026-06-16

- **Outcome:** Renamed `Vk_SsaoPass` → `Vk_AoPass` with `Gfx_AoMethod` (Classic SSAO, HBAO+). Shared `AoCommon.glsl`. HBAO+ half-res horizon compute + depth-aware upsample. ImGui method combo. Default method: HBAO+.
- **Verification:** `Verify-CI.ps1` exit 0; `--validation --smoke-frames 120` → 0 validation ERROR lines.
- **Deviations:** none
