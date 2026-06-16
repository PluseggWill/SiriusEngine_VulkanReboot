# Progress: contact-soft-ao

## Closeout — 2026-06-16

- **Outcome:** ShadowAoSoft pass (pack AO+shadow RG8, bilateral blur). SSAO outputs linear AO; deferred applies power once. Fallback textures + shadow depth layout barriers fix AO-off validation. Shadow map barrier includes COMPUTE stage for pack pass.
- **Verification:** `Verify-CI.ps1` exit 0; GfxTests pass; `--validation --smoke-frames 120 --smoke-seconds 6` on stress.json → 0 validation ERROR lines.
- **Deviations:** Merged into same commit as modular `Vk_AoPass` + HBAO+ v0 (user-requested follow-on in same session).
