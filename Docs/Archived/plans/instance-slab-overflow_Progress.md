# Progress: instance-slab-overflow

## 2026-05-26 — P1–P3

- **Plan ref:** P1–P3
- **Files:** `Vk_Core.h`, `Vk_Core.cpp`
- **What changed:** `FillInstanceSlab` returns `false` on overflow (no partial writes). `DrawFrame` skips `RecordScenePass` when fill fails; one-time `Warn` log.
- **Verification:** MSBuild Debug|x64; smoke 4s; demo renders normally.
