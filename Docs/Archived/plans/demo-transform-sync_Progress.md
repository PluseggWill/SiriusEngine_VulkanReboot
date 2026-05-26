# Progress: demo-transform-sync

## 2026-05-26 — P1–P4

- **Plan ref:** P1–P4
- **Files:** `Vk_Core.h`, `Vk_Core.cpp`
- **What changed:** `myDemoBaseTransforms` at init; `ApplyDemoTransformAnimation` before extract; `FillInstanceSlab` uses SoA transform only.
- **Verification:** MSBuild Debug|x64; smoke 4s; two meshes + spin OK.
